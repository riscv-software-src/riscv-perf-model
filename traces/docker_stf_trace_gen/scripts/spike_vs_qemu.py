#!/usr/bin/env python3
"""
Comprehensive Spike vs QEMU Benchmarking Tool

This script provides a unified benchmarking framework to compare Spike and QEMU
performance across different workloads, configurations, and measurements.

Features:
- Build and run benchmarks on both Spike and QEMU
- Support for different execution modes (normal, BBV generation, trace generation)
- Interval size analysis for BBV generation
- Performance comparison and visualization
- Reproducible results with detailed logging
- Clean output format with optional visualizations

Author: Generated for RISC-V Performance Modeling
"""

import argparse
import subprocess
import time
import json
import csv
import logging
from pathlib import Path
from dataclasses import dataclass, asdict
from typing import List, Optional, Tuple
import sys

# Optional imports for visualization (only loaded if --visualize is used)
try:
    import matplotlib.pyplot as plt
    import numpy as np
    import pandas as pd
    VISUALIZATION_AVAILABLE = True
except ImportError:
    VISUALIZATION_AVAILABLE = False

# Workload configurations
WORKLOAD_CONFIG = {
    "embench-iot": {
        "path": "/workloads/embench-iot",
        "arch": "rv32",
        "benchmarks": ["aha-mont64", "crc32", "cubic", "edn", "huffbench", "matmult-int", 
                      "md5sum", "minver", "nbody", "nettle-aes", "nettle-sha256", 
                      "nsichneu", "picojpeg", "qrduino", "sglib-combined", 
                      "slre", "st", "statemate", "ud", "wikisort"]
    },
    "riscv-tests": {
        "path": "/workloads/riscv-tests", 
        "arch": "rv64",
        "benchmarks": ["dhrystone", "median", "mm", "mt-matmul", "mt-memcpy", 
                      "mt-vvadd", "multiply", "qsort", "rsort", "spmv", 
                      "towers", "vvadd", "vec-memcpy", "vec-daxpy", 
                      "vec-sgemm", "vec-strcmp"]
    }
}

@dataclass
class BenchmarkResult:
    """Data class to store benchmark results"""
    workload_suite: str  # "embench-iot" or "riscv-tests"
    benchmark: str       # individual benchmark name
    platform: str        # "spike" or "qemu"
    execution_mode: str   # "normal", "bbv", "trace"
    arch: str
    board: str
    run_time: float
    interval_size: Optional[int] = None
    bbv_generated: bool = False
    trace_generated: bool = False
    status: str = "success"
    error_message: Optional[str] = None

class BenchmarkRunner:
    """Main benchmarking class"""
    
    def __init__(self, output_dir: str = "benchmark_results", verbose: bool = False):
        self.output_dir = Path(output_dir)
        self.output_dir.mkdir(exist_ok=True)
        
        # Setup logging
        log_level = logging.DEBUG if verbose else logging.INFO
        logging.basicConfig(
            level=log_level,
            format='%(asctime)s - %(levelname)s - %(message)s',
            handlers=[
                logging.FileHandler(self.output_dir / "benchmark.log"),
                logging.StreamHandler(sys.stdout)
            ]
        )
        self.logger = logging.getLogger(__name__)
        
        # Build script paths
        self.build_script = Path("flow/build_workload.py")
        self.run_script = Path("flow/run_workload.py")
        
        # Validate scripts exist
        if not self.build_script.exists():
            raise FileNotFoundError(f"Build script not found: {self.build_script}")
        if not self.run_script.exists():
            raise FileNotFoundError(f"Run script not found: {self.run_script}")
        
        self.logger.info(f"Benchmarking framework initialized. Output dir: {self.output_dir}")

    def parse_benchmark_spec(self, spec: str) -> List[Tuple[str, str, str]]:
        """Parse benchmark specification and return list of (workload_suite, benchmark, arch)"""
        results = []
        
        if spec in WORKLOAD_CONFIG:
            # Full workload suite specified
            config = WORKLOAD_CONFIG[spec]
            for benchmark in config["benchmarks"]:
                results.append((spec, benchmark, config["arch"]))
        elif ":" in spec:
            # Specific benchmark within suite: "embench-iot:md5sum"
            suite, benchmark = spec.split(":", 1)
            if suite in WORKLOAD_CONFIG and benchmark in WORKLOAD_CONFIG[suite]["benchmarks"]:
                results.append((suite, benchmark, WORKLOAD_CONFIG[suite]["arch"]))
            else:
                self.logger.error(f"Invalid benchmark specification: {spec}")
        else:
            # Try to find benchmark in any suite
            found = False
            for suite, config in WORKLOAD_CONFIG.items():
                if spec in config["benchmarks"]:
                    results.append((suite, spec, config["arch"]))
                    found = True
                    break
            if not found:
                self.logger.error(f"Benchmark '{spec}' not found in any workload suite")
        
        return results

    def list_available_benchmarks(self):
        """Print available benchmarks"""
        print("\nAvailable Workload Suites and Benchmarks:")
        print("=" * 50)
        for suite, config in WORKLOAD_CONFIG.items():
            print(f"\n{suite} ({config['arch']}):")
            for i, benchmark in enumerate(config["benchmarks"], 1):
                print(f"  {i:2d}. {benchmark}")
        
        print("\nUsage examples:")
        print("  --workloads embench-iot              # All embench-iot benchmarks")
        print("  --workloads embench-iot:md5sum       # Just md5sum from embench-iot") 
        print("  --workloads md5sum,dhrystone         # md5sum and dhrystone")
        print("  --workloads riscv-tests:vec-memcpy   # Just vec-memcpy from riscv-tests")

    def run_command(self, cmd: List[str], timeout: int = 300) -> Tuple[bool, str, float]:
        """Run a command and return success status, output, and execution time"""
        self.logger.debug(f"Executing: {' '.join(cmd)}")
        
        start_time = time.time()
        try:
            result = subprocess.run(
                cmd, 
                capture_output=True, 
                text=True, 
                timeout=timeout,
                check=False
            )
            execution_time = time.time() - start_time
            
            if result.returncode == 0:
                self.logger.debug(f"Command succeeded in {execution_time:.2f}s")
                return True, result.stdout, execution_time
            else:
                self.logger.warning(f"Command failed with code {result.returncode}")
                return False, result.stderr, execution_time
                
        except subprocess.TimeoutExpired:
            execution_time = time.time() - start_time
            self.logger.error(f"Command timed out after {timeout}s")
            return False, f"Command timed out after {timeout}s", execution_time
        except Exception as e:
            execution_time = time.time() - start_time
            self.logger.error(f"Command failed with exception: {e}")
            return False, str(e), execution_time


    def build_workload(self, workload_suite: str, benchmark: str, arch: str = "rv64", 
                      platform: str = "baremetal", board: str = "spike", 
                      bbv: bool = False, trace: bool = False) -> bool:
        """Build a workload and return success status"""
        self.logger.info(f"Building {workload_suite}:{benchmark} for {board} (arch: {arch}, bbv: {bbv}, trace: {trace})")
        
        cmd = [
            "python3", str(self.build_script),
            "--workload", workload_suite,
            "--benchmark", benchmark,  # Specify individual benchmark
            "--arch", arch,
            "--platform", platform,
            "--emulator", board
        ]
        
        if bbv:
            cmd.append("--bbv")
        if trace:
            cmd.append("--trace")
        
        success, output, build_time = self.run_command(cmd, timeout=180)
        
        if not success:
            self.logger.error(f"Build failed for {workload_suite}:{benchmark}: {output}")
        
        return success

    def run_workload(self, benchmark: str, board: str = "spike", 
                    interval_size: Optional[int] = None, bbv=False,trace=False) -> Tuple[bool, float]:
        """Run a workload and return success status and run time"""
        self.logger.info(f"Running {benchmark} on {board}")
        
        # The run_workload.py script expects --emulator, --workload filter, --interval-size
        cmd = [
            "python3", str(self.run_script),
            "--emulator", board,
            "--workload", benchmark  # Filter to specific benchmark
        ]
        if bbv:
            cmd.append("--bbv")
        if trace:
            cmd.append("--trace")
        
        if interval_size is not None:
            cmd.extend(["--interval-size", str(interval_size)])
        
        success, output, run_time = self.run_command(cmd, timeout=300)
        
        if not success:
            self.logger.error(f"Run failed for {benchmark}: {output}")
        
        return success, run_time

    def check_outputs_generated(self, workload: str, board: str, bbv: bool, trace: bool) -> Tuple[bool, bool]:
        """Check if BBV and trace outputs were generated"""
        bbv_generated = False
        trace_generated = False
        
        if bbv:
            # Check for BBV files (different formats for spike vs qemu)
            output_base = Path(f"/outputs/{board}_output/bbv")
            if board == "spike":
                # Spike may produce .bbv or .bbv_cpu0
                candidates = [output_base / f"{workload}.bbv", output_base / f"{workload}.bbv_cpu0"]
            else:  # qemu: outfile prefix <name>.bbv -> files like <name>.bbv.0.bb
                candidates = [output_base / f"{workload}.bbv.0.bb", output_base / f"{workload}_bbv.0.bb"]
            bbv_file = next((p for p in candidates if p.exists()), output_base / f"{workload}.bbv")
            
            bbv_generated = bbv_file.exists() and bbv_file.stat().st_size > 0
            self.logger.debug(f"BBV file {bbv_file}: {'found' if bbv_generated else 'not found'}")
        
        if trace:
            # Check for STF trace files generated into /outputs/<board>_output/traces
            trace_file = Path(f"/outputs/{board}_output/traces/{workload}.zstf")
            trace_generated = trace_file.exists() and trace_file.stat().st_size > 0
            self.logger.debug(f"Trace file {trace_file}: {'found' if trace_generated else 'not found'}")
        
        return bbv_generated, trace_generated

    def benchmark_single(self, workload_suite: str, benchmark: str, arch: str, board: str, 
                        execution_mode: str = "normal", interval_size: Optional[int] = None) -> BenchmarkResult:
        """Run a single benchmark and return results"""
        
        bbv = execution_mode in ["bbv", "trace"]
#        trace = execution_mode == "trace"
        trace = execution_mode in "trace"
        
        self.logger.info(f"Benchmarking {workload_suite}:{benchmark} on {board} (mode: {execution_mode})")
        
        # Build phase
        build_success = self.build_workload(
            workload_suite=workload_suite,
            benchmark=benchmark,
            arch=arch,
            board=board,
            bbv=bbv,
            trace=trace
        )
        
        if not build_success:
            return BenchmarkResult(
                workload_suite=workload_suite,
                benchmark=benchmark,
                platform=board,
                execution_mode=execution_mode,
                arch=arch,
                board=board,
                run_time=0.0,
                interval_size=interval_size,
                status="build_failed",
                error_message="Build failed"
            )
        
        # Run phase
        run_success, run_time = self.run_workload(
            benchmark=benchmark,
            board=board,
            interval_size=interval_size,
            bbv=bbv,
            trace=trace
        )
        
        if not run_success:
            return BenchmarkResult(
                workload_suite=workload_suite,
                benchmark=benchmark,
                platform=board,
                execution_mode=execution_mode,
                arch=arch,
                board=board,
                run_time=run_time,
                interval_size=interval_size,
                status="run_failed",
                error_message="Run failed"
            )
        
        # Check outputs
        bbv_generated, trace_generated = self.check_outputs_generated(benchmark, board, bbv, trace)
        
        return BenchmarkResult(
            workload_suite=workload_suite,
            benchmark=benchmark,
            platform=board,
            execution_mode=execution_mode,
            arch=arch,
            board=board,
            run_time=run_time,
            interval_size=interval_size,
            bbv_generated=bbv_generated,
            trace_generated=trace_generated,
            status="success"
        )

    def benchmark_comparison(self, workloads: List[str],
                           execution_modes: List[str] = ["normal"]) -> List[BenchmarkResult]:
        """Run comparative benchmarks between Spike and QEMU"""
        
        results = []
        
        # Parse all benchmark specifications
        all_benchmarks = []
        for workload_spec in workloads:
            parsed = self.parse_benchmark_spec(workload_spec)
            all_benchmarks.extend(parsed)
        
        if not all_benchmarks:
            self.logger.error("No valid benchmarks found to run")
            return results
        
        total_tests = len(all_benchmarks) * len(execution_modes) * 2  # 2 platforms
        current_test = 0
        
        self.logger.info(f"Starting comparative benchmark: {len(all_benchmarks)} benchmarks, "
                        f"{len(execution_modes)} modes, 2 platforms = {total_tests} total tests")
        
        for workload_suite, benchmark, arch in all_benchmarks:
            for mode in execution_modes:
                for board in ["spike", "qemu"]:
                    current_test += 1
                    self.logger.info(f"Progress: {current_test}/{total_tests} - {workload_suite}:{benchmark} on {board} ({mode})")
                    
                    result = self.benchmark_single(
                        workload_suite=workload_suite,
                        benchmark=benchmark,
                        arch=arch,  # Use the correct arch for this benchmark
                        board=board,
                        execution_mode=mode
                    )
                    results.append(result)
                    
                    # Brief pause between tests
                    #time.sleep(1)
        
        return results

    def interval_analysis(self, benchmark_spec: str, 
                         interval_sizes: Optional[List[int]] = None) -> List[BenchmarkResult]:
        """Run interval size analysis for BBV generation"""
        
        if interval_sizes is None:
            # Generate logarithmic interval sizes from 1K to 10M
            try:
                import numpy as np
                interval_sizes = [int(10**x) for x in np.linspace(3, 7, 20)]  # 1K to 10M
            except ImportError:
                # Fallback if numpy not available
                interval_sizes = [1000, 5000, 10000, 50000, 100000, 500000, 1000000, 5000000, 10000000]
        
        # Parse benchmark specification
        parsed = self.parse_benchmark_spec(benchmark_spec)
        if not parsed:
            self.logger.error(f"Invalid benchmark specification: {benchmark_spec}")
            return []
        
        workload_suite, benchmark, arch = parsed[0]  # Take first match
        
        self.logger.info(f"Starting interval analysis for {workload_suite}:{benchmark} with {len(interval_sizes)} intervals")
        
        results = []
        for i, interval_size in enumerate(interval_sizes):
            self.logger.info(f"Interval analysis progress: {i+1}/{len(interval_sizes)} - size: {interval_size:,}")
            
            for board in ["spike", "qemu"]:
                result = self.benchmark_single(
                    workload_suite=workload_suite,
                    benchmark=benchmark,
                    arch=arch,
                    board=board,
                    execution_mode="bbv",
                    interval_size=interval_size
                )
                results.append(result)
        
        return results

    def save_results(self, results: List[BenchmarkResult], filename: str = "benchmark_results.json"):
        """Save results to JSON and CSV files"""
        
        # Save as JSON
        json_file = self.output_dir / filename
        with open(json_file, 'w') as f:
            json.dump([asdict(r) for r in results], f, indent=2)
        self.logger.info(f"Results saved to {json_file}")
        
        # Save as CSV
        csv_file = self.output_dir / filename.replace('.json', '.csv')
        if results:
            fieldnames = list(asdict(results[0]).keys())
            with open(csv_file, 'w', newline='') as f:
                writer = csv.DictWriter(f, fieldnames=fieldnames)
                writer.writeheader()
                for result in results:
                    writer.writerow(asdict(result))
            self.logger.info(f"Results saved to {csv_file}")

    def generate_comparison_report(self, results: List[BenchmarkResult]) -> str:
        """Generate a text summary report"""
        
        report = ["SPIKE vs QEMU Benchmark Comparison Report", "=" * 50, ""]
        
        # Group results by workload suite:benchmark and mode
        workload_groups = {}
        for result in results:
            key = (f"{result.workload_suite}:{result.benchmark}", result.execution_mode)
            if key not in workload_groups:
                workload_groups[key] = {}
            workload_groups[key][result.platform] = result
        
        # Generate comparison table
        report.append(f"{'Benchmark':<25} {'Mode':<10} {'Spike (s)':<12} {'QEMU (s)':<12} {'Speedup':<10} {'Status'}")
        report.append("-" * 80)
        
        total_spike_time = 0
        total_qemu_time = 0
        
        for (benchmark, mode), platforms in workload_groups.items():
            spike_result = platforms.get('spike')
            qemu_result = platforms.get('qemu')
            
            if spike_result and qemu_result:
                spike_time = spike_result.run_time
                qemu_time = qemu_result.run_time
                
                if spike_time > 0:
                    speedup = qemu_time / spike_time
                    speedup_str = f"{speedup:.2f}x"
                else:
                    speedup_str = "N/A"
                
                status = "✓" if (spike_result.status == "success" and qemu_result.status == "success") else "✗"
                
                report.append(f"{benchmark:<25} {mode:<10} {spike_time:<12.3f} {qemu_time:<12.3f} {speedup_str:<10} {status}")
                
                if spike_result.status == "success":
                    total_spike_time += spike_time
                if qemu_result.status == "success":
                    total_qemu_time += qemu_time
        
        # Summary statistics
        report.extend(["", "Summary:", "-" * 20])
        if total_spike_time > 0:
            overall_speedup = total_qemu_time / total_spike_time
            report.append(f"Total Spike runtime: {total_spike_time:.3f}s")
            report.append(f"Total QEMU runtime: {total_qemu_time:.3f}s")
            report.append(f"Overall speedup: {overall_speedup:.2f}x")
            
            if overall_speedup > 1:
                report.append(f"Spike is {overall_speedup:.2f}x faster overall")
            else:
                report.append(f"QEMU is {1/overall_speedup:.2f}x faster overall")
        
        return "\n".join(report)

    def visualize_results(self, results: List[BenchmarkResult], show_plots: bool = True):
        """Generate visualizations of benchmark results"""
        
        if not VISUALIZATION_AVAILABLE:
            self.logger.warning("Visualization libraries not available. Install matplotlib, pandas, numpy.")
            return
        
        # Convert to DataFrame for easier manipulation
        df = pd.DataFrame([asdict(r) for r in results])
        
        # 1. Execution time comparison
        plt.figure(figsize=(12, 8))
        
        # Group by workload suite:benchmark and execution mode
        comparison_data = []
        df['benchmark_full'] = df['workload_suite'] + ':' + df['benchmark']
        for (benchmark_full, mode), group in df.groupby(['benchmark_full', 'execution_mode']):
            spike_data = group[group['platform'] == 'spike']
            qemu_data = group[group['platform'] == 'qemu']
            
            if not spike_data.empty and not qemu_data.empty:
                comparison_data.append({
                    'benchmark': benchmark_full,
                    'mode': mode,
                    'spike_time': spike_data['run_time'].iloc[0],
                    'qemu_time': qemu_data['run_time'].iloc[0]
                })
        
        if comparison_data:
            comp_df = pd.DataFrame(comparison_data)
            
            # Bar chart comparison
            x = np.arange(len(comp_df))
            width = 0.35
            
            plt.bar(x - width/2, comp_df['spike_time'], width, label='Spike', alpha=0.8)
            plt.bar(x + width/2, comp_df['qemu_time'], width, label='QEMU', alpha=0.8)
            
            plt.xlabel('Workload/Mode')
            plt.ylabel('Runtime (seconds)')
            plt.title('Spike vs QEMU Runtime Performance Comparison')
            plt.xticks(x, [f"{row['benchmark']}\n({row['mode']})" for _, row in comp_df.iterrows()], rotation=45)
            plt.legend()
            plt.grid(True, alpha=0.3)
            plt.tight_layout()
            
            # Save plot
            plot_file = self.output_dir / "performance_comparison.png"
            plt.savefig(plot_file, dpi=300, bbox_inches='tight')
            self.logger.info(f"Performance comparison plot saved to {plot_file}")
            
            if show_plots:
                plt.show()
        
        # 2. Interval size analysis (if applicable)
        interval_results = df[df['interval_size'].notna()]
        if not interval_results.empty:
            plt.figure(figsize=(12, 6))
            
            for platform in ['spike', 'qemu']:
                platform_data = interval_results[interval_results['platform'] == platform]
                if not platform_data.empty:
                    plt.loglog(platform_data['interval_size'], platform_data['run_time'], 
                              'o-', label=f'{platform.title()}', linewidth=2, markersize=6)
            
            plt.xlabel('Interval Size')
            plt.ylabel('Runtime (seconds)')
            plt.title('Runtime Performance vs BBV Interval Size')
            plt.grid(True, alpha=0.3)
            plt.legend()
            plt.tight_layout()
            
            # Save interval plot
            interval_plot_file = self.output_dir / "interval_analysis.png"
            plt.savefig(interval_plot_file, dpi=300, bbox_inches='tight')
            self.logger.info(f"Interval analysis plot saved to {interval_plot_file}")
            
            if show_plots:
                plt.show()


def parse_interval_list(interval_str: str) -> List[int]:
    """Parse comma-separated interval sizes"""
    try:
        return [int(x.strip()) for x in interval_str.split(',')]
    except ValueError as e:
        raise argparse.ArgumentTypeError(f"Invalid interval size list: {e}")


def main():
    """Main entry point"""
    
    parser = argparse.ArgumentParser(
        description="Comprehensive Spike vs QEMU Benchmarking Tool",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # List available benchmarks
  python3 spike_vs_qemu_benchmark.py --list-benchmarks

  # Basic comparison - specific benchmarks
  python3 spike_vs_qemu_benchmark.py --workloads riscv-tests:dhrystone,embench-iot:md5sum --modes normal,bbv

  # Full workload suite
  python3 spike_vs_qemu_benchmark.py --workloads embench-iot --modes normal

  # Interval size analysis
  python3 spike_vs_qemu_benchmark.py --interval-analysis riscv-tests:dhrystone --intervals 1000,10000,100000

  # Full analysis with visualization
  python3 spike_vs_qemu_benchmark.py --workloads riscv-tests:dhrystone --modes normal,bbv,trace --visualize

  # Custom output directory
  python3 spike_vs_qemu_benchmark.py --workloads embench-iot:md5sum --output results_20240101
        """
    )
    
    # Main operation modes
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument('--workloads', type=str,
                      help='Comma-separated list of workload specifications to benchmark')
    group.add_argument('--interval-analysis', type=str,
                      help='Benchmark specification for interval size analysis')
    group.add_argument('--list-benchmarks', action='store_true',
                      help='List available workload suites and benchmarks')
    
    # Common options
    # Note: arch is now determined automatically based on workload suite
    parser.add_argument('--modes', default='normal',
                       help='Comma-separated execution modes: normal,bbv,trace (default: normal)')
    parser.add_argument('--output', default='benchmark_results',
                       help='Output directory (default: benchmark_results)')
    parser.add_argument('--verbose', '-v', action='store_true',
                       help='Verbose logging')
    
    # Interval analysis options
    parser.add_argument('--intervals', type=parse_interval_list,
                       help='Comma-separated interval sizes for analysis (default: auto-generated)')
    
    # Visualization options
    parser.add_argument('--visualize', action='store_true',
                       help='Generate visualizations (requires matplotlib)')
    parser.add_argument('--show-plots', action='store_true',
                       help='Show plots interactively (implies --visualize)')
    
    # Output options
    parser.add_argument('--report-only', action='store_true',
                       help='Only generate report from existing results')
    
    args = parser.parse_args()
    
    # Validate dependencies
    if (args.visualize or args.show_plots) and not VISUALIZATION_AVAILABLE:
        print("Error: Visualization requires matplotlib, pandas, and numpy")
        print("Install with: pip install matplotlib pandas numpy")
        sys.exit(1)
    
    if args.show_plots:
        args.visualize = True
    
    # Handle list benchmarks mode
    if args.list_benchmarks:
        # Create temporary runner just for listing
        try:
            runner = BenchmarkRunner(output_dir=args.output, verbose=args.verbose)
            runner.list_available_benchmarks()
            sys.exit(0)
        except FileNotFoundError as e:
            print(f"Error: {e}")
            sys.exit(1)
    
    # Initialize benchmark runner
    try:
        runner = BenchmarkRunner(output_dir=args.output, verbose=args.verbose)
    except FileNotFoundError as e:
        print(f"Error: {e}")
        sys.exit(1)
    
    # Parse modes
    modes = [mode.strip() for mode in args.modes.split(',')]
    valid_modes = ['normal', 'bbv', 'trace']
    for mode in modes:
        if mode not in valid_modes:
            print(f"Error: Invalid mode '{mode}'. Valid modes: {', '.join(valid_modes)}")
            sys.exit(1)
    
    # Run benchmarks based on mode
    results = []
    
    if args.workloads:
        # Regular comparison benchmarking
        workloads = [w.strip() for w in args.workloads.split(',')]
        runner.logger.info(f"Running comparative benchmarks for workloads: {workloads}")
        runner.logger.info(f"Execution modes: {modes}")
        
        results = runner.benchmark_comparison(
            workloads=workloads,
            execution_modes=modes
        )
    
    elif args.interval_analysis:
        # Interval size analysis
        benchmark_spec = args.interval_analysis
        runner.logger.info(f"Running interval analysis for benchmark: {benchmark_spec}")
        
        results = runner.interval_analysis(
            benchmark_spec=benchmark_spec,
            interval_sizes=args.intervals
        )
    
    # Save results
    if results:
        if args.interval_analysis:
            filename = f"interval_analysis_{args.interval_analysis}.json"
        else:
            filename = "benchmark_comparison.json"
        
        runner.save_results(results, filename)
        
        # Generate report
        report = runner.generate_comparison_report(results)
        
        # Save report
        report_file = runner.output_dir / "benchmark_report.txt"
        with open(report_file, 'w') as f:
            f.write(report)
        
        # Print report
        print("\n" + report)
        
        # Generate visualizations
        if args.visualize:
            runner.visualize_results(results, show_plots=args.show_plots)
        
        runner.logger.info("Benchmarking completed successfully!")
        print(f"\nResults saved in: {runner.output_dir}")
        
    else:
        runner.logger.error("No results generated")
        sys.exit(1)


if __name__ == "__main__":
    main()

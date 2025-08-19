#!/usr/bin/env python3
"""
SimPoint Analysis Script for RISC-V Workload Build & Run System

This script performs SimPoint analysis on BBV (Basic Block Vector) files generated
by Spike or QEMU emulators. It identifies representative simulation points for
long-running workloads to enable simulation acceleration.

Features:
- BBV file processing for both Spike and QEMU
- SimPoint clustering analysis
- Workload filtering and selection
- Configurable interval sizes and clustering parameters
- Integration with existing build/run workflow
- Support for trace slicing based on SimPoint results

Author: Generated for RISC-V Performance Modeling
"""

import argparse
import os
import sys
import subprocess
import time
import logging
from pathlib import Path
from typing import List, Optional, Tuple, Dict
import json

def setup_logging(verbose: bool = False) -> logging.Logger:
    """Setup logging configuration"""
    log_level = logging.DEBUG if verbose else logging.INFO
    logging.basicConfig(
        level=log_level,
        format='%(asctime)s - %(levelname)s - %(message)s',
        handlers=[
            logging.StreamHandler(sys.stdout)
        ]
    )
    return logging.getLogger(__name__)

def log(level: str, msg: str, logger: Optional[logging.Logger] = None):
    """Print colored log messages"""
    color = {"INFO": "\033[32m", "ERROR": "\033[31m", "DEBUG": "\033[34m", "WARN": "\033[33m"}.get(level, "")
    ts = time.strftime('%H:%M:%S')
    formatted_msg = f"[{ts}] {color}{level}:\033[0m {msg}"
    
    if logger:
        if level == "ERROR":
            logger.error(msg)
        elif level == "WARN":
            logger.warning(msg)
        elif level == "DEBUG":
            logger.debug(msg)
        else:
            logger.info(msg)
    
    print(formatted_msg, file=sys.stderr if level == "ERROR" else sys.stdout)
    if level == "ERROR": 
        sys.exit(1)

def run_cmd(cmd: List[str], cwd: Optional[str] = None, timeout: int = 300) -> Tuple[bool, str, str]:
    """Run a command and return success status, stdout, stderr"""
    cmd_str = ' '.join(cmd)
    print(f"Running: {cmd_str}")
    
    try:
        result = subprocess.run(
            cmd, 
            cwd=cwd,
            capture_output=True, 
            text=True, 
            timeout=timeout,
            check=False
        )
        
        success = result.returncode == 0
        if not success:
            print(f"Command failed with return code {result.returncode}")
            print(f"stderr: {result.stderr}")
        
        return success, result.stdout, result.stderr
        
    except subprocess.TimeoutExpired:
        print(f"Command timed out after {timeout} seconds")
        return False, "", f"Timeout after {timeout}s"
    except Exception as e:
        print(f"Command failed with exception: {e}")
        return False, "", str(e)

def validate_simpoint_binary() -> bool:
    """Check if SimPoint binary is available"""
    try:
        success, stdout, stderr = run_cmd(["simpoint", "-h"], timeout=10)
        return success
    except:
        return False

def read_binary_list(emulator: str, workload_type: Optional[str] = None) -> List[Path]:
    """Read binaries from board-specific binary list and optionally filter by workload type"""
    binary_list_file = Path(f"/workloads/binary_list_{emulator}.txt")
    
    if not binary_list_file.exists():
        log("ERROR", f"Binary list not found: {binary_list_file}. Please run build_workload.py first.")
    
    log("INFO", f"Reading binary list from {binary_list_file}")
    
    # Read all binaries
    all_binaries = []
    for line in binary_list_file.read_text().strip().split('\n'):
        if line.strip():
            binary_path = Path(line.strip())
            if binary_path.exists():
                all_binaries.append(binary_path)
            else:
                log("WARN", f"Binary not found: {binary_path}")
    
    # Filter by workload type if specified
    if workload_type:
        filtered_binaries = []
        
        if workload_type == "dhrystone":
            filtered_binaries = [b for b in all_binaries if "dhrystone" in b.name]
        elif workload_type == "embench":
            # Look for embench patterns in path or name
            filtered_binaries = [b for b in all_binaries if 
                               "embench" in str(b) or "src" in str(b.parent)]
        else:
            # Generic workload type - filter by name
            filtered_binaries = [b for b in all_binaries if workload_type in b.name or workload_type in str(b)]
        
        if not filtered_binaries:
            log("ERROR", f"No binaries found for workload type: {workload_type}")
        
        log("INFO", f"Filtered to {len(filtered_binaries)} binaries matching '{workload_type}'")
        return filtered_binaries
    
    log("INFO", f"Processing all {len(all_binaries)} available binaries")
    return all_binaries

def find_bbv_files(emulator: str, binaries: List[Path]) -> Dict[str, Path]:
    """Find BBV files for given binaries"""
    bbv_files = {}
    output_dir = Path(f"/output/{emulator}_output/bbv")
    
    if not output_dir.exists():
        log("ERROR", f"BBV output directory not found: {output_dir}. Run with --bbv first.")
    
    for binary in binaries:
        benchmark_name = binary.stem
        
        if emulator == "spike":
            # Spike BBV files: benchmark.bbv
            bbv_file = output_dir / f"{benchmark_name}.bbv"
        else:  # qemu
            # QEMU BBV files: benchmark.bbv.0.bb (plugin generates this format)
            bbv_file = output_dir / f"{benchmark_name}.bbv.0.bb"
            # Also check alternative format
            if not bbv_file.exists():
                bbv_file = output_dir / f"{benchmark_name}_bbv.0.bb"
        
        if bbv_file.exists() and bbv_file.stat().st_size > 0:
            bbv_files[benchmark_name] = bbv_file
            log("DEBUG", f"Found BBV file for {benchmark_name}: {bbv_file}")
        else:
            log("WARN", f"BBV file not found or empty for {benchmark_name}: {bbv_file}")
    
    if not bbv_files:
        log("ERROR", "No BBV files found. Ensure workloads were run with --bbv flag.")
    
    log("INFO", f"Found BBV files for {len(bbv_files)} benchmarks")
    return bbv_files

def run_simpoint_analysis(bbv_file: Path, benchmark_name: str, max_k: int = 30, 
                         output_dir: Path = None) -> Tuple[bool, Path, Path]:
    """Run SimPoint analysis on a single BBV file"""
    if output_dir is None:
        output_dir = Path("/output/simpoint_analysis")
    
    output_dir.mkdir(parents=True, exist_ok=True)
    
    # SimPoint output files
    simpoints_file = output_dir / f"{benchmark_name}.simpoints"
    weights_file = output_dir / f"{benchmark_name}.weights"
    
    log("INFO", f"Running SimPoint analysis for {benchmark_name}")
    log("DEBUG", f"  Input BBV: {bbv_file}")
    log("DEBUG", f"  Max K: {max_k}")
    log("DEBUG", f"  Output dir: {output_dir}")
    
    # SimPoint command
    cmd = [
        "simpoint",
        "-loadFVFile", str(bbv_file),
        "-maxK", str(max_k),
        "-saveSimpoints", str(simpoints_file),
        "-saveSimpointWeights", str(weights_file)
    ]
    
    success, stdout, stderr = run_cmd(cmd, timeout=300)
    
    if success and simpoints_file.exists() and weights_file.exists():
        log("INFO", f"SimPoint analysis completed for {benchmark_name}")
        log("INFO", f"  SimPoints: {simpoints_file}")
        log("INFO", f"  Weights: {weights_file}")
        return True, simpoints_file, weights_file
    else:
        log("ERROR", f"SimPoint analysis failed for {benchmark_name}")
        log("ERROR", f"stdout: {stdout}")
        log("ERROR", f"stderr: {stderr}")
        return False, simpoints_file, weights_file

def generate_bbv_workloads(emulator: str, binaries: List[Path], interval_size: int = None) -> Dict[str, Path]:
    """Generate BBV files for workloads if they don't exist"""
    log("INFO", "Generating BBV files for workloads...")
    
    # Set default interval sizes
    if interval_size is None:
        interval_size = 100000000 if emulator == "spike" else 10000000
    
    bbv_files = {}
    
    for binary in binaries:
        benchmark_name = binary.stem
        log("INFO", f"Generating BBV for {benchmark_name}...")
        
        if emulator == "spike":
            # Run Spike with BBV generation
            cmd = [
                "spike", 
                "--isa=rv32gc",
                "--en_bbv",
                f"--bb_file=/output/spike_output/bbv/{benchmark_name}.bbv",
                f"--simpoint_size={interval_size}",
                str(binary)
            ]
            bbv_file = Path(f"/output/spike_output/bbv/{benchmark_name}.bbv")
            
        else:  # qemu
            # Check for BBV plugin
            bbv_plugin = "/qemu/build/contrib/plugins/libbbv.so"
            if not Path(bbv_plugin).exists():
                log("ERROR", f"QEMU BBV plugin not found: {bbv_plugin}")
                continue
            
            # Create BBV output directory
            Path("/output/qemu_output/bbv").mkdir(parents=True, exist_ok=True)
            
            # Run QEMU with BBV plugin
            cmd = [
                "qemu-system-riscv32",
                "-nographic", "-machine", "virt", "-bios", "none",
                "-kernel", str(binary),
                "-plugin", f"{bbv_plugin},interval={interval_size},outfile=/output/qemu_output/bbv/{benchmark_name}_bbv"
            ]
            bbv_file = Path(f"/output/qemu_output/bbv/{benchmark_name}_bbv.0.bb")
        
        # Run the command
        success, stdout, stderr = run_cmd(cmd, timeout=600)
        
        if success and bbv_file.exists() and bbv_file.stat().st_size > 0:
            bbv_files[benchmark_name] = bbv_file
            log("INFO", f"BBV generation completed for {benchmark_name}")
        else:
            log("WARN", f"BBV generation failed for {benchmark_name}")
    
    return bbv_files

def parse_simpoint_results(simpoints_file: Path, weights_file: Path) -> List[Tuple[int, float]]:
    """Parse SimPoint results to get (interval, weight) pairs"""
    simpoints = []
    weights = []
    
    # Read simpoints
    if simpoints_file.exists():
        with open(simpoints_file, 'r') as f:
            for line in f:
                parts = line.strip().split()
                if len(parts) >= 2:
                    simpoints.append(int(parts[0]))  # interval number
    
    # Read weights  
    if weights_file.exists():
        with open(weights_file, 'r') as f:
            for line in f:
                parts = line.strip().split()
                if len(parts) >= 2:
                    weights.append(float(parts[1]))  # weight value
    
    # Combine simpoints and weights
    if len(simpoints) == len(weights):
        return list(zip(simpoints, weights))
    else:
        log("WARN", f"Simpoints ({len(simpoints)}) and weights ({len(weights)}) count mismatch")
        return []

def generate_summary_report(results: Dict[str, Dict], output_file: Path):
    """Generate a summary report of SimPoint analysis"""
    report_lines = [
        "SimPoint Analysis Summary Report",
        "=" * 40,
        "",
        f"Analysis Date: {time.strftime('%Y-%m-%d %H:%M:%S')}",
        f"Total Benchmarks: {len(results)}",
        "",
        "Results:",
        "-" * 20
    ]
    
    successful = 0
    total_simpoints = 0
    
    for benchmark, result in results.items():
        if result['success']:
            successful += 1
            simpoints_count = result.get('simpoints_count', 0)
            total_simpoints += simpoints_count
            
            report_lines.append(f"{benchmark}:")
            report_lines.append(f"  Status: SUCCESS")
            report_lines.append(f"  SimPoints: {simpoints_count}")
            report_lines.append(f"  Coverage: {result.get('coverage', 'N/A')}")
            
            # Show top representative intervals
            if result.get('intervals'):
                top_intervals = sorted(result['intervals'], key=lambda x: x[1], reverse=True)[:3]
                report_lines.append(f"  Top intervals: {', '.join([f'{i}({w:.3f})' for i, w in top_intervals])}")
        else:
            report_lines.append(f"{benchmark}: FAILED")
        
        report_lines.append("")
    
    # Summary statistics
    report_lines.extend([
        "Summary:",
        f"  Successful analyses: {successful}/{len(results)}",
        f"  Average SimPoints per benchmark: {total_simpoints/max(successful,1):.1f}",
        f"  Results directory: /output/simpoint_analysis/",
        ""
    ])
    
    # Write report
    with open(output_file, 'w') as f:
        f.write('\n'.join(report_lines))
    
    # Print report
    print('\n'.join(report_lines))

def main():
    """Main entry point"""
    parser = argparse.ArgumentParser(
        description="Run SimPoint analysis on BBV files",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Run SimPoint analysis on all available BBV files for Spike
  python3 run_simpoint.py --emulator spike

  # Analyze specific workload type with QEMU
  python3 run_simpoint.py --emulator qemu --workload-type embench

  # Run with custom parameters
  python3 run_simpoint.py --emulator spike --workload-type dhrystone --max-k 20 --interval-size 50000000

  # Generate BBV and then analyze
  python3 run_simpoint.py --emulator qemu --generate-bbv --interval-size 5000000
        """
    )
    
    # Required arguments
    parser.add_argument("--emulator", required=True, choices=["spike", "qemu"],
                       help="Emulator that generated the BBV files")
    
    # Optional arguments
    parser.add_argument("--workload-type", type=str,
                       help="Filter by workload type (embench, dhrystone, riscv-tests, or custom name)")
    parser.add_argument("--interval-size", type=int,
                       help="BBV interval size (default: 100M for Spike, 10M for QEMU)")
    parser.add_argument("--max-k", type=int, default=30,
                       help="Maximum number of clusters for SimPoint (default: 30)")
    parser.add_argument("--output-dir", type=str, default="/output/simpoint_analysis",
                       help="Output directory for SimPoint results")
    parser.add_argument("--generate-bbv", action="store_true",
                       help="Generate BBV files if they don't exist")
    parser.add_argument("--verbose", "-v", action="store_true",
                       help="Verbose output")
    
    args = parser.parse_args()
    
    # Setup logging
    logger = setup_logging(args.verbose)
    
    # Validate SimPoint binary
    if not validate_simpoint_binary():
        log("ERROR", "SimPoint binary not found. Please install SimPoint and ensure it's in PATH.")
    
    # Create output directory
    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    
    log("INFO", "Starting SimPoint analysis")
    log("INFO", f"Emulator: {args.emulator}")
    log("INFO", f"Workload type: {args.workload_type or 'all'}")
    log("INFO", f"Max K: {args.max_k}")
    log("INFO", f"Output directory: {output_dir}")
    
    # Read binary list
    binaries = read_binary_list(args.emulator, args.workload_type)
    log("INFO", f"Processing {len(binaries)} binaries")
    
    # Find or generate BBV files
    if args.generate_bbv:
        bbv_files = generate_bbv_workloads(args.emulator, binaries, args.interval_size)
    else:
        bbv_files = find_bbv_files(args.emulator, binaries)
    
    if not bbv_files:
        log("ERROR", "No BBV files available for analysis")
    
    # Run SimPoint analysis on each BBV file
    results = {}
    
    for benchmark_name, bbv_file in bbv_files.items():
        log("INFO", f"Analyzing {benchmark_name}...")
        
        success, simpoints_file, weights_file = run_simpoint_analysis(
            bbv_file, benchmark_name, args.max_k, output_dir
        )
        
        result = {
            'success': success,
            'bbv_file': str(bbv_file),
            'simpoints_file': str(simpoints_file),
            'weights_file': str(weights_file)
        }
        
        if success:
            # Parse results
            intervals = parse_simpoint_results(simpoints_file, weights_file)
            result['intervals'] = intervals
            result['simpoints_count'] = len(intervals)
            
            if intervals:
                total_weight = sum(weight for _, weight in intervals)
                result['coverage'] = f"{total_weight:.3f}"
                
                log("INFO", f"  Found {len(intervals)} SimPoints")
                log("INFO", f"  Coverage: {total_weight:.3f}")
        
        results[benchmark_name] = result
    
    # Generate summary report
    summary_file = output_dir / "simpoint_summary.txt"
    generate_summary_report(results, summary_file)
    
    # Save detailed results as JSON
    json_file = output_dir / "simpoint_results.json"
    with open(json_file, 'w') as f:
        json.dump(results, f, indent=2)
    
    log("INFO", f"SimPoint analysis completed!")
    log("INFO", f"Summary report: {summary_file}")
    log("INFO", f"Detailed results: {json_file}")
    log("INFO", f"SimPoint files: {output_dir}")
    
    # Success summary
    successful = sum(1 for r in results.values() if r['success'])
    log("INFO", f"Successfully analyzed {successful}/{len(results)} benchmarks")

if __name__ == "__main__":
    main()
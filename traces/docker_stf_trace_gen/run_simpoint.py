#!/usr/bin/env python3
"""Performs SimPoint analysis on BBV files."""
import argparse
import json
import time
from pathlib import Path
from typing import Dict, List, Tuple
from utils.util import log, LogLevel, run_cmd, ensure_dir, validate_tool, read_file_lines, file_exists

def find_bbv_files(emulator: str, binaries: List[Path]) -> Dict[str, Path]:
    """Find BBV files for binaries."""
    output_dir = Path(f"/outputs/{emulator}_output/bbv")
    if not output_dir.exists():
        log(LogLevel.ERROR, f"BBV directory not found: {output_dir}")
    
    bbv_files = {}
    for binary in binaries:
        name = binary.stem
        bbv_file = output_dir / (f"{name}.bbv_cpu0" if emulator == "spike" else f"{name}.bbv.0.bb")
        if not bbv_file.exists() and emulator == "qemu":
            bbv_file = output_dir / f"{name}_bbv.0.bb"
        if bbv_file.exists() and bbv_file.stat().st_size > 0:
            bbv_files[name] = bbv_file
    if not bbv_files:
        log(LogLevel.ERROR, "No valid BBV files found")
    log(LogLevel.INFO, f"Found {len(bbv_files)} BBV files")
    return bbv_files

def run_simpoint_analysis(bbv_file: Path, benchmark: str, max_k: int, output_dir: Path) -> Tuple[bool, Path, Path]:
    """Run SimPoint analysis on a BBV file."""
    ensure_dir(output_dir)
    simpoints = output_dir / f"{benchmark}.simpoints"
    weights = output_dir / f"{benchmark}.weights"
    cmd = ["simpoint", "-loadFVFile", str(bbv_file), "-maxK", str(max_k), "-saveSimpoints", str(simpoints), "-saveSimpointWeights", str(weights)]
    success, _, _ = run_cmd(cmd, timeout=300)
    return success and simpoints.exists() and weights.exists(), simpoints, weights

def parse_simpoint_results(simpoints_file: Path, weights_file: Path) -> List[Tuple[int, float]]:
    """Parse SimPoint intervals and weights."""
    simpoints = [int(line.split()[0]) for line in read_file_lines(simpoints_file) if line.split()[0].isdigit()] if simpoints_file.exists() else []
    weights = [float(line.split()[1]) for line in read_file_lines(weights_file) if len(line.split()) > 1] if weights_file.exists() else []
    return list(zip(simpoints, weights)) if len(simpoints) == len(weights) else []

def generate_summary(results: Dict[str, Dict], output_file: Path):
    """Generate SimPoint analysis report."""
    successful = sum(1 for r in results.values() if r['success'])
    total_simpoints = sum(r.get('simpoints_count', 0) for r in results.values() if r['success'])
    lines = [
        f"SimPoint Analysis Summary - {time.strftime('%Y-%m-%d %H:%M:%S')}",
        f"Total Benchmarks: {len(results)}",
        f"Successful: {successful}/{len(results)}",
        f"Average SimPoints: {total_simpoints/max(successful,1):.1f}",
        "\nResults:"
    ]
    for bench, res in results.items():
        lines.append(f"{bench}: {'SUCCESS' if res['success'] else 'FAILED'}")
        if res['success']:
            lines.append(f"  SimPoints: {res['simpoints_count']}")
            lines.append(f"  Coverage: {res.get('coverage', 'N/A')}")
            if res.get('intervals'):
                top = sorted(res['intervals'], key=lambda x: x[1], reverse=True)[:3]
                lines.append(f"  Top intervals: {', '.join(f'{i}({w:.3f})' for i, w in top)}")
    
    with output_file.open('w') as f:
        f.write('\n'.join(lines))
    log(LogLevel.INFO, '\n'.join(lines))

def main():
    """Main entry point."""
    parser = argparse.ArgumentParser(description="Run SimPoint analysis on BBV files")
    parser.add_argument("--emulator", required=True, choices=["spike", "qemu"])
    parser.add_argument("--workload-type", help="Filter by workload type")
    parser.add_argument("--max-k", type=int, default=30, help="Max clusters for SimPoint")
    parser.add_argument("--output-dir", default="/outputs/simpoint_analysis")
    args = parser.parse_args()

    validate_tool("simpoint")
    
    output_dir = ensure_dir(Path(args.output_dir))
    log(LogLevel.INFO, f"Starting SimPoint analysis for {args.emulator}")
    
    binary_list = Path(f"/workloads/binary_list_{args.emulator}.txt")
    binaries = [Path(line) for line in read_file_lines(binary_list) if file_exists(line)]
    if args.workload_type:
        binaries = [b for b in binaries if args.workload_type in b.name]
    
    bbv_files = find_bbv_files(args.emulator, binaries)
    results = {}
    for bench, bbv_file in bbv_files.items():
        log(LogLevel.INFO, f"Analyzing {bench}")
        success, simpoints, weights = run_simpoint_analysis(bbv_file, bench, args.max_k, output_dir)
        result = {'success': success, 'bbv_file': str(bbv_file), 'simpoints_file': str(simpoints), 'weights_file': str(weights)}
        if success:
            intervals = parse_simpoint_results(simpoints, weights)
            result.update({'intervals': intervals, 'simpoints_count': len(intervals), 'coverage': f"{sum(w for _, w in intervals):.3f}"})
        results[bench] = result
    
    summary_file = output_dir / "simpoint_summary.txt"
    generate_summary(results, summary_file)
    with (output_dir / "simpoint_results.json").open('w') as f:
        json.dump(results, f, indent=2)


    if True: #reduce: 
        #parse the weights and simpoitn files to get the top 3 intervals and their weights
        # and then slice the traces accodinlgy for that interval from the trace
        # make sure to tweak around with diff intervl_sizes (btw ) give optionf or that 
        #then save the trace corresponding to the top 3 intervals in seperate files 
        # in the format required by the perf model and traace-archive.py tool
        pass

    
    log(LogLevel.INFO, f"Analysis completed. Summary: {summary_file}")

if __name__ == "__main__":
    main()
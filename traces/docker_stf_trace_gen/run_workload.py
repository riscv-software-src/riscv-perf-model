#!/usr/bin/env python3
"""Runs RISC-V workloads on Spike or QEMU."""
import argparse
from pathlib import Path
from typing import Dict
from utils.util import log, LogLevel, run_cmd, get_time, validate_tool, clean_dir, ensure_dir

def validate_environment(emulator: str, platform: str, arch: str):
    """Validate emulator tools."""
    if emulator == "spike":
        validate_tool("spike")
    elif emulator == "qemu":
        validate_tool(f"qemu-{'system-' if platform == 'baremetal' else ''}riscv{32 if arch == 'rv32' else 64}")

def setup_output_dirs(emulator: str) -> Dict[str, Path]:
    """Setup output directories."""
    base = clean_dir(Path(f"/outputs/{emulator}_output"))
    return {
        'base': base, 'logs': base / "logs", 'bbv': base / "bbv",
        'traces': base / "traces", 'results': base / "results.txt"
    }

def run_emulator(binary: Path, dirs: Dict[str, Path], emulator: str, bbv: bool, trace: bool, platform: str, arch: str, interval_size: int, enable_stf_tools: bool) -> float:
    """Run workload on emulator."""
    name = binary.stem
    log(LogLevel.INFO, f"Running {name} on {emulator.upper()} ({platform}/{arch})")
    if emulator == "qemu" and trace: 
        print("QEMU Cannot generate STF traces, Please use Spike")
        trace = False
    
    configs = {
        "spike": {
            "cmd": ["spike", f"--isa=rv{arch[2:]}imafdc"],
            "bbv": lambda: ["--en_bbv", f"--bb_file={dirs['bbv'] / f'{name}.bbv'}", f"--simpoint_size={interval_size}"],
            "trace": lambda: ["--stf_macro_tracing", "--stf_trace_memory_records", f"--stf_trace={dirs['traces'] / f'{name}.zstf'}"],
            "bin": [str(binary)]
        },
        "qemu": {
            "cmd": (["qemu-system-riscv32" if arch == "rv32" else "qemu-system-riscv64", "-nographic", "-machine", "virt", "-bios", "none", "-kernel", str(binary)]
                    if platform == "baremetal" else
                    [f"qemu-riscv{32 if arch == 'rv32' else 64}", str(binary)]),
            "bbv": lambda: ["-plugin", f"/qemu/build/contrib/plugins/libbbv.so,interval={interval_size},outfile={dirs['bbv'] / f'{name}.bbv'}"],
            #disable trace for qemu
        }
    }
    
    cfg = configs[emulator]
    cmd = cfg["cmd"] + (cfg["bbv"]() if bbv else []) + (cfg["trace"]() if trace else []) + cfg.get("bin", [])
    # build the logs file 
    start = get_time() 
    run_cmd(cmd)
    end = get_time()
    
    if emulator == "spike" and trace and enable_stf_tools:
        trace_file = dirs['traces'] / f"{name}.zstf"
        if trace_file.exists():
            stf_dump = Path("/riscv/stf_tools/release/tools/stf_dump/stf_dump")
            if stf_dump.exists():
                run_cmd([str(stf_dump), str(trace_file)])
    
    return end - start

def run_workloads(emulator: str, platform: str, arch: str, bbv: bool, trace: bool, workload: str = None, interval_size: int = 10**7, enable_stf_tools: bool = False):
    """Run all workloads from binary list."""
    validate_environment(emulator, platform, arch)
    dirs = setup_output_dirs(emulator)
    for d in [dirs['logs'], dirs['bbv'], dirs['traces']]:
        ensure_dir(d)
    
    # Fetch binaries in /workloads/bin/<emulator>/ 
    binary_dir = Path(f"/workloads/bin/{emulator}")
    # Get all files in the emulator's directory, excluding .o files
    binaries = [f for f in binary_dir.glob("*") if f.is_file() and f.suffix != ".o"]

    # This is for a specific benchmark/workload ?
    if workload:
        binaries = [b for b in binaries if workload in b.name]
        if not binaries:
            log(LogLevel.ERROR, f"No workloads matching: {workload}")
    
    log(LogLevel.INFO, f"Running {len(binaries)} workloads")
    results = []
    total_time = 0
    with dirs['results'].open('w') as f:
        f.write("Workload RunTime(s) CodeSize(bytes)\n")
        for binary in binaries:
            try:
                run_time = run_emulator(binary, dirs, emulator, bbv, trace, platform, arch, interval_size, enable_stf_tools)
                results.append((binary.stem, run_time, binary.stat().st_size))
                f.write(f"{binary.stem} {run_time:.3f} {binary.stat().st_size}\n")
                total_time += run_time
            except RuntimeError:
                continue
    
    log(LogLevel.INFO, f"Summary: {len(results)} workloads, {total_time:.2f}s, results in {dirs['results']}")

def main():
    """Main entry point."""
    parser = argparse.ArgumentParser(description="Run RISC-V workloads")
    parser.add_argument("--emulator", required=True, choices=["spike", "qemu"], help="Emulator to use")
    parser.add_argument("--platform", default="baremetal", choices=["baremetal", "linux"], help="Platform type")
    parser.add_argument("--arch", default="rv32", choices=["rv32", "rv64"], help="Architecture")
    parser.add_argument("--bbv", action="store_true", help="Enable BBV generation")
    parser.add_argument("--trace", action="store_true", help="Generate STF Traces (Spike only)")
    parser.add_argument("--workload", help="Filter specific workload")
    parser.add_argument("--interval-size", type=int, default=10**7, help="BBV Interval size")
    parser.add_argument("--enable-stf-tools", action="store_true")
    args = parser.parse_args()
    
    run_workloads(args.emulator, args.platform, args.arch, args.bbv, args.trace, 
                  args.workload, args.interval_size, args.enable_stf_tools)

if __name__ == "__main__":
    main()
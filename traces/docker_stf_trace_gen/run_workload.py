#!/usr/bin/env python3

import argparse
import subprocess
from pathlib import Path

from util import log, run_cmd, get_time, validate_tool, clean_dir, read_file_lines

def validate_environment(emulator, platform, arch="rv32"):
    """Validate required tools are available"""
    if emulator == "spike":
        validate_tool("spike")
    elif emulator == "qemu":
        if platform == "baremetal":
            tool = "qemu-system-riscv32" if arch == "rv32" else "qemu-system-riscv64"
        else:
            tool = "qemu-riscv32" if arch == "rv32" else "qemu-riscv64"
        validate_tool(tool)
    else:
        log("ERROR", f"Unknown emulator: {emulator}")

def setup_output_dirs(emulator):
    """Create organized output directories"""
    base_dir = Path(f"/outputs/{emulator}_output")
    dirs = {
        'base': base_dir,
        'logs': base_dir / "logs",
        'bbv': base_dir / "bbv", 
        'traces': base_dir / "traces",
        'results': base_dir / "results.txt"
    }
    
    # Clean and create directories
    clean_dir(dirs['base'])
    for dir_path in [dirs['logs'], dirs['bbv'], dirs['traces']]:
        dir_path.mkdir(parents=True, exist_ok=True)
    
    return dirs

def read_binary_list(emulator):
    """Read binaries from board-specific binary_list.txt"""
    binary_list_file = Path(f"/workloads/binary_list_{emulator}.txt")
    if not binary_list_file.exists():
        log("ERROR", f"binary_list_{emulator}.txt not found. Run build_workload.py for {emulator} first.")
    
    binaries = []
    for line_path in read_file_lines(binary_list_file):
        binary_path = Path(line_path)
        if binary_path.exists():
            binaries.append(binary_path)
        else:
            log("ERROR", f"Binary not found: {binary_path}")
    
    return binaries

def run_spike(binary, dirs, bbv=False, trace=False, arch="rv32", interval_size=100000000, enable_stf_tools=False):
    """Run workload on Spike"""
    workload_name = binary.stem
    log("INFO", f"Running {workload_name} on Spike")
    
    # Base spike command
    isa = f"rv{arch[2:]}imafdc"
    cmd = ["spike", f"--isa={isa}"]
    
    # Add BBV support
    if bbv:
        bbv_file = dirs['bbv'] / f"{workload_name}.bbv"
        cmd.extend([
            "--en_bbv",
            f"--bb_file={bbv_file}",
            f"--simpoint_size={interval_size}"
        ])
        log("INFO", f"Generating BBV: {bbv_file}")
    
    # Add trace support  
    trace_file = None
    if trace:
        trace_file = dirs['traces'] / f"{workload_name}.zstf"
        cmd.extend([
            "--stf_macro_tracing",
            "--stf_trace_memory_records", 
            f"--stf_trace={trace_file}"
        ])
        log("INFO", f"Generating STF trace: {trace_file}")
    
    # Add binary
    cmd.append(str(binary))
    
    # Run and capture output
    output_file = dirs['logs'] / f"{workload_name}_output.log"
    start_time = get_time()
    run_cmd(cmd, f"Spike failed for {workload_name}", output_file)
    end_time = get_time()
    
    # Generate STF dump if enabled and trace was generated
    if enable_stf_tools and trace and trace_file:
        stf_dump_path = Path("/riscv/stf_tools/release/tools/stf_dump/stf_dump")
        if stf_dump_path.exists():
            dump_file = f"{trace_file}.dump"
            log("INFO", f"Generating STF dump: {dump_file}")
            dump_cmd = [str(stf_dump_path), str(trace_file)]
            run_cmd(dump_cmd, f"STF dump failed for {workload_name}", dump_file)
        else:
            log("ERROR", f"STF dump tool not found: {stf_dump_path}")
    
    run_time = end_time - start_time
    log("INFO", f"Completed {workload_name} in {run_time:.2f}s")
    
    return run_time

def run_qemu(binary, dirs, bbv=False, trace=False, platform="baremetal", arch="rv32", interval_size=10000000):
    """Run workload on QEMU"""
    workload_name = binary.stem
    log("INFO", f"Running {workload_name} on QEMU ({platform}/{arch})")
    
    # Base QEMU command
    if platform == "baremetal":
        # System emulation
        if arch == "rv32":
            cmd = ["qemu-system-riscv32", "-nographic", "-machine", "virt", "-bios", "none"]
        else:
            cmd = ["qemu-system-riscv64", "-nographic", "-machine", "virt", "-bios", "none"]
        cmd.extend(["-kernel", str(binary)])
        
        # Add BBV support (plugin for system mode)
        if bbv:
            bbv_plugin = "/qemu/build/contrib/plugins/libbbv.so"
            if Path(bbv_plugin).exists():
                bbv_file = dirs['bbv'] / f"{workload_name}.bbv"
                cmd.extend([
                    "-plugin", 
                    f"{bbv_plugin},interval={interval_size},outfile={bbv_file}"
                ])
                log("INFO", f"Generating BBV: {bbv_file}")
            else:
                log("ERROR", f"BBV plugin not found: {bbv_plugin}")
    else:
        # User-mode emulation for Linux
        if arch == "rv32":
            cmd = ["qemu-riscv32", str(binary)]
        else:
            cmd = ["qemu-riscv64", str(binary)]
        
        # BBV for user-mode is more complex - would need different plugin
        if bbv:
            log("WARNING", "BBV generation not fully supported in QEMU user-mode")
    
    # Add trace support
    if trace:
        trace_file = dirs['traces'] / f"{workload_name}_trace.log"
        cmd.extend(["-d", "in_asm", "-D", str(trace_file)])
        log("INFO", f"Generating trace: {trace_file}")
    
    # Run and capture output
    output_file = dirs['logs'] / f"{workload_name}_output.log"
    start_time = get_time()
    run_cmd(cmd, f"QEMU failed for {workload_name}", output_file)
    end_time = get_time()
    
    run_time = end_time - start_time
    log("INFO", f"Completed {workload_name} in {run_time:.2f}s")
    
    return run_time

def run_workloads(emulator, platform, arch, bbv, trace, workload_filter=None, interval_size=10**6, enable_stf_tools=False):
    """Run all workloads from board-specific binary_list.txt"""
    validate_environment(emulator, platform, arch)
    dirs = setup_output_dirs(emulator)
    binaries = read_binary_list(emulator)
    
    # Filter workloads if specified
    if workload_filter:
        binaries = [b for b in binaries if workload_filter in b.name]
        if not binaries:
            log("ERROR", f"No workloads found matching: {workload_filter}")
    
    log("INFO", f"Running {len(binaries)} workloads on {emulator} ({platform}/{arch})")
    
    # Results tracking
    results = []
    total_time = 0
    
    with open(dirs['results'], 'w') as f:
        f.write("Workload RunTime(s) CodeSize(bytes)\n")
        
        for binary in binaries:
            workload_name = binary.stem
            code_size = binary.stat().st_size
            
            try:
                if emulator == "spike":
                    run_time = run_spike(binary, dirs, bbv, trace, arch, interval_size, enable_stf_tools)
                else:  # qemu
                    run_time = run_qemu(binary, dirs, bbv, trace, platform, arch, interval_size)
                
                results.append((workload_name, run_time, code_size))
                total_time += run_time
                f.write(f"{workload_name} {run_time:.3f} {code_size}\n")
                f.flush()
                
            except Exception as e:
                log("ERROR", f"Failed to run {workload_name}: {e}")
                continue
    
    # Summary
    log("INFO", "=== Execution Summary ===")
    print(f"Emulator:         {emulator}")
    print(f"Platform:         {platform}")
    print(f"Architecture:     {arch}")
    print(f"BBV Generation:   {bbv}")
    print(f"Trace Generation: {trace}")
    print(f"STF Tools:        {enable_stf_tools}")
    print(f"Workloads:        {len(results)}")
    print(f"Total Time:       {total_time:.2f}s")
    print(f"Outputs:          {dirs['base']}")
    print(f"Results:          {dirs['results']}")
    
    log("INFO", "All workloads completed successfully!")

def main():
    parser = argparse.ArgumentParser(description="Run RISC-V workloads on Spike or QEMU")
    parser.add_argument("--emulator", required=True, choices=["spike", "qemu"], 
                       help="Emulator to use")
    parser.add_argument("--platform", default="baremetal", choices=["baremetal", "linux"],
                       help="Platform type")
    parser.add_argument("--arch", default="rv32", choices=["rv32", "rv64"],
                       help="Architecture")
    parser.add_argument("--bbv", action="store_true", help="Generate BBV")
    parser.add_argument("--trace", action="store_true", help="Generate instruction traces")
    parser.add_argument("--workload", help="Filter specific workload")
    parser.add_argument("--interval-size", type=int, default=10000000,
                       help="BBV interval size (default: 100M for Spike, 10M for QEMU)")
    parser.add_argument("--enable-stf-tools", action="store_true", 
                       help="Enable STF dump generation (Spike only)")
    
    args = parser.parse_args()
    
    # Adjust default interval size based on emulator if using default
    if args.interval_size == 10000000:  # Using default value
        if args.emulator == "spike":
            args.interval_size = 100000000  # 100M for Spike
        # else keep 10M for QEMU
    
    run_workloads(args.emulator, args.platform, args.arch, args.bbv, args.trace, 
                  args.workload, args.interval_size, args.enable_stf_tools)

if __name__ == "__main__":
    main()
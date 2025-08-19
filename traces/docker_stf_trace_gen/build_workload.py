#!/usr/bin/env python3

import argparse
import sys
from pathlib import Path

from util import log, run_cmd, ensure_dir, clean_dir, file_exists
from config import BoardConfig

DEFAULT_WORKLOADS = {
    "embench-iot": "/workloads/embench-iot", 
    "riscv-tests": "/workloads/riscv-tests"
}

class WorkloadBuilder:
    """Modular workload builder using configuration files"""
    
    def __init__(self, board, arch, platform, bbv=False, trace=False):
        self.board = board
        self.arch = arch
        self.platform = platform
        self.bbv = bbv
        self.trace = trace
        self.config = BoardConfig(board)
        self.executables = []
        
        # Set up directories
        self.bin_dir = Path(f"/workloads/bin/{board}")
        self.env_dir = Path(f"/workloads/environment/{board}")
        
    def clean_build_dirs(self):
        """Clean previous build artifacts"""
        log("INFO", f"Cleaning build directories for {self.board}")
        clean_dir(self.bin_dir)
        
        # Clean environment object files
        for obj_file in self.env_dir.glob("*.o"):
            obj_file.unlink()
    
    def get_compiler_info(self, workload=None, benchmark_name=None):
        """Get compiler information from config"""
        build_config = self.config.get_build_config(
            self.arch, self.platform, workload, self.bbv, self.trace, benchmark_name
        )
        
        cc = build_config.get('cc')
        base_cflags = build_config.get('base_cflags', [])
        base_ldflags = build_config.get('base_ldflags', [])
        
        return cc, base_cflags, base_ldflags, build_config
    
    def build_environment(self, workload=None):
        """Build environment runtime files"""
        if self.config.should_skip_environment(self.platform, workload):
            log("INFO", f"Skipping environment build for {self.platform}")
            return
            
        log("INFO", f"Building environment at {self.env_dir}")
        
        cc, base_cflags, base_ldflags, build_config = self.get_compiler_info(workload)
        env_files = self.config.get_environment_files(workload)
        
        # Add board-specific defines
        cflags = base_cflags.copy()
        for define in build_config.get('defines', []):
            cflags.append(f"-D{define}")
        
        cflags.extend([f"-I{self.env_dir}"])
        
        for src in env_files:
            src_file = self.env_dir / src
            obj_file = self.env_dir / f"{Path(src).stem}.o"
            
            if src_file.exists():
                compile_cmd = [cc, "-c"] + cflags + ["-o", str(obj_file), str(src_file)]
                run_cmd(compile_cmd, f"Failed to compile {src}")
    
    def build_common_files(self, workload_path, workload_type):
        """Build common files for riscv-tests"""
        if workload_type != "riscv-tests":
            return []
            
        common_dir = Path(workload_path) / "benchmarks" / "common"
        if not common_dir.exists():
            return []
        
        cc, base_cflags, base_ldflags, build_config = self.get_compiler_info("riscv-tests")
        
        # Get workload-specific flags
        cflags = base_cflags.copy()
        if 'cflags' in build_config:
            cflags.extend(build_config['cflags'])
        
        # Add includes
        includes = self.config.get_workload_includes(workload_path, "riscv-tests")
        for include in includes:
            cflags.append(f"-I{include}")
        cflags.append(f"-I{workload_path}/env")
        
        # Add defines
        for define in build_config.get('defines', []):
            cflags.append(f"-D{define}")
        
        skip_files = self.config.get_skip_common_files(self.platform, "riscv-tests")
        obj_files = []
        
        for c_file in common_dir.glob("*.c"):
            if c_file.name in skip_files:
                log("INFO", f"Skipping {c_file.name} for {self.platform} build")
                continue
                
            obj_file = self.bin_dir / f"{c_file.stem}.o"
            obj_files.append(str(obj_file))
            
            compile_cmd = [cc, "-c"] + cflags + ["-o", str(obj_file), str(c_file)]
            run_cmd(compile_cmd, f"Failed to compile {c_file}")
        
        return obj_files
    
    def build_benchmark(self, bench_name, workload_path, workload_type, common_objs=None):
        """Build a single benchmark"""
        log("INFO", f"Building {bench_name}")
        
        if workload_type == "embench-iot":
            return self._build_embench_benchmark(bench_name, workload_path, common_objs)
        elif workload_type == "riscv-tests":
            return self._build_riscv_benchmark(bench_name, workload_path, common_objs)
        else:
            log("ERROR", f"Unsupported workload type: {workload_type}")
    
    def _build_embench_benchmark(self, bench_name, workload_path, common_objs):
        """Build embench-iot benchmark"""
        bench_dir = Path(workload_path) / "src" / bench_name
        if not bench_dir.exists():
            log("ERROR", f"Benchmark directory not found: {bench_dir}")
        
        c_files = list(bench_dir.glob("*.c"))
        if not c_files:
            log("ERROR", f"No .c files found for {bench_name}")
        
        cc, base_cflags, base_ldflags, build_config = self.get_compiler_info("embench-iot")
        
        # Build compilation flags
        cflags = base_cflags.copy()
        if 'cflags' in build_config:
            cflags.extend(build_config['cflags'])
        
        # Add board-specific defines and includes
        for define in build_config.get('defines', []):
            cflags.append(f"-D{define}")
        
        if self.platform == "baremetal":
            cflags.extend([f"-I{self.env_dir}"])
        
        # Compile benchmark sources
        obj_files = []
        for c_file in c_files:
            obj_file = self.bin_dir / f"{c_file.stem}.o"
            obj_files.append(str(obj_file))
            
            compile_cmd = [cc, "-c"] + cflags + ["-o", str(obj_file), str(c_file)]
            run_cmd(compile_cmd, f"Failed to compile {c_file}")
        
        # Compile additional workload sources (e.g., BEEBS support files)
        workload_sources = build_config.get('workload_sources', [])
        for source_file in workload_sources:
            source_path = Path(source_file)
            if source_path.exists():
                obj_file = self.bin_dir / f"{source_path.stem}_support.o"
                obj_files.append(str(obj_file))
                
                compile_cmd = [cc, "-c"] + cflags + ["-o", str(obj_file), str(source_path)]
                run_cmd(compile_cmd, f"Failed to compile {source_file}")
            else:
                log("WARNING", f"Workload source file not found: {source_file}")
        
        # Link
        return self._link_executable(bench_name, obj_files, build_config, common_objs, "embench-iot")
    
    def _build_riscv_benchmark(self, bench_name, workload_path, common_objs):
        """Build riscv-tests benchmark"""
        bench_dir = Path(workload_path) / "benchmarks" / bench_name
        if not bench_dir.exists():
            log("ERROR", f"Benchmark directory not found: {bench_dir}")
        
        # Find source files
        c_files = list(bench_dir.glob("*.c"))
        s_files = list(bench_dir.glob("*.S"))
        source_files = c_files + s_files
        if not source_files:
            log("ERROR", f"No .c or .S files found for {bench_name}")
        
        cc, base_cflags, base_ldflags, build_config = self.get_compiler_info("riscv-tests", bench_name)
        
        # Build compilation flags
        cflags = base_cflags.copy()
        if 'cflags' in build_config:
            cflags.extend(build_config['cflags'])
        
        # Add includes
        includes = self.config.get_workload_includes(workload_path, "riscv-tests")
        for include in includes:
            cflags.append(f"-I{include}")
        cflags.append(f"-I{workload_path}/env")
        
        if self.platform == "baremetal":
            cflags.extend([f"-I{self.env_dir}"])
        
        # Add defines
        for define in build_config.get('defines', []):
            cflags.append(f"-D{define}")
        
        # Compile sources
        obj_files = []
        for source_file in source_files:
            obj_file = self.bin_dir / f"{source_file.stem}.o"
            obj_files.append(str(obj_file))
            
            compile_cmd = [cc, "-c"] + cflags + ["-o", str(obj_file), str(source_file)]
            run_cmd(compile_cmd, f"Failed to compile {source_file}")
        
        # Add common object files
        if common_objs:
            obj_files.extend(common_objs)
        
        # Link
        return self._link_executable(bench_name, obj_files, build_config, None, "riscv-tests")
    
    def _link_executable(self, bench_name, obj_files, build_config, common_objs, workload=None):
        """Link executable"""
        exe_path = str(self.bin_dir / bench_name)
        
        cc = build_config.get('cc')
        ldflags = build_config.get('base_ldflags', [])
        libs = build_config.get('libs', [])
        
        if self.platform == "baremetal":
            # Add environment objects
            env_files = self.config.get_environment_files(workload)
            env_objs = [str(self.env_dir / f"{Path(f).stem}.o") for f in env_files]
            
            # Add linker script
            linker_script = build_config.get('linker_script', 'link.ld')
            ldflags = [f"-T{self.env_dir / linker_script}"] + ldflags
            
            link_cmd = [cc] + ldflags + ["-o", exe_path] + obj_files + env_objs + libs
        else:
            # Linux linking
            link_cmd = [cc] + ldflags + ["-o", exe_path] + obj_files + libs
        
        run_cmd(link_cmd, f"Failed to link {bench_name}")
        self.executables.append(exe_path)
        return exe_path
    
    def list_benchmarks(self, workload_path, workload_type):
        """List available benchmarks for a workload"""
        if workload_type == "embench-iot":
            src_dir = Path(workload_path) / "src"
            return [d.name for d in src_dir.iterdir() if d.is_dir()] if src_dir.exists() else []
        elif workload_type == "riscv-tests":
            bench_dir = Path(workload_path) / "benchmarks"
            return [d.name for d in bench_dir.iterdir() 
                   if d.is_dir() and d.name != "common"] if bench_dir.exists() else []
        return []
    
    def build_workload(self, workload, benchmark=None, custom_path=None):
        """Main build function"""
        # Determine workload path and type
        if workload in DEFAULT_WORKLOADS:
            workload_path, workload_type = DEFAULT_WORKLOADS[workload], workload
        elif workload == "dhrystone":
            workload_path, workload_type = DEFAULT_WORKLOADS["riscv-tests"], "riscv-tests"
        elif custom_path:
            workload_path, workload_type = custom_path, "custom"
        else:
            log("ERROR", f"Unknown workload: {workload}. Use --list to see available workloads.")
        
        if not file_exists(workload_path):
            log("ERROR", f"Workload path does not exist: {workload_path}")
        
        log("INFO", f"Building {workload} for {self.arch} on {self.platform}/{self.board}")
        
        self.clean_build_dirs()
        self.build_environment(workload_type)
        
        # Build common files if needed
        common_objs = self.build_common_files(workload_path, workload_type)
        
        # Determine benchmarks to build
        available = self.list_benchmarks(workload_path, workload_type)
        if workload == "dhrystone":
            benchmarks = ["dhrystone"]
        else:
            benchmarks = [benchmark] if benchmark else available
        
        if benchmark and benchmark not in available:
            log("ERROR", f"Benchmark {benchmark} not found. Available: {', '.join(available)}")
        
        # Build each benchmark
        for bench in benchmarks:
            self.build_benchmark(bench, workload_path, workload_type, common_objs)
        
        # Create binary list
        if self.executables:
            binary_list = Path(f"/workloads/binary_list_{self.board}.txt")
            binary_list.write_text("\n".join(self.executables) + "\n")
            log("INFO", f"Built {len(self.executables)} executables in {self.bin_dir}/")
            print(binary_list.read_text())
        else:
            log("ERROR", "No executables were built")

def main():
    """Main entry point"""
    parser = argparse.ArgumentParser(description="Build RISC-V workloads with config-driven approach")
    parser.add_argument("--workload", help="Workload name")
    parser.add_argument("--arch", default="rv32", choices=["rv32", "rv64"])
    parser.add_argument("--platform", default="baremetal", choices=["baremetal", "linux"])
    parser.add_argument("--board", default="spike", choices=["spike", "qemu"])
    parser.add_argument("--benchmark", help="Specific benchmark")
    parser.add_argument("--custom-path", help="Custom workload path")
    parser.add_argument("--bbv", action="store_true", help="Enable BBV support")
    parser.add_argument("--trace", action="store_true", help="Enable Tracing")
    parser.add_argument("--list", action="store_true", help="List available workloads")
    
    args = parser.parse_args()
    
    if args.list:
        print("Available workloads:")
        for name, path in DEFAULT_WORKLOADS.items():
            print(f"  {name}: {path}")
            if file_exists(path):
                builder = WorkloadBuilder(args.board, args.arch, args.platform)
                benchmarks = builder.list_benchmarks(path, name)
                if benchmarks:
                    print(f"    Benchmarks: {', '.join(benchmarks[:10])}{'...' if len(benchmarks) > 10 else ''}")
        sys.exit(0)
    
    if not args.workload:
        log("ERROR", "Workload required. Use --list to see available workloads.")
    
    builder = WorkloadBuilder(args.board, args.arch, args.platform, args.bbv, args.trace)
    builder.build_workload(args.workload, args.benchmark, args.custom_path)
    log("INFO", "Build completed successfully!")

if __name__ == "__main__":
    main()
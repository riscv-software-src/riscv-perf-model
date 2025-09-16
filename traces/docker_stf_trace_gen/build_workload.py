#!/usr/bin/env python3
"""Builds RISC-V workloads using configuration-driven approach."""
import argparse
from pathlib import Path
from typing import List
from utils.util import Util, LogLevel
from utils.config import BoardConfig

DEFAULT_WORKLOADS = {
    "embench-iot": "/workloads/embench-iot",
    "riscv-tests": "/workloads/riscv-tests",
    "dhrystone": "/workloads/riscv-tests"
}

class WorkloadBuilder:
    """Manages building of RISC-V workloads."""
    def __init__(self, board: str, arch: str, platform: str, bbv: bool, trace: bool):
        self.board = board
        self.arch = arch
        self.platform = platform
        self.bbv = bbv
        self.trace = trace
        self.config = BoardConfig(board)
        self.bin_dir = Util.clean_dir(Path(f"/workloads/bin/{board}"))
        self.env_dir = Path(f"/workloads/environment/{board}")
        self.executables = []

    def _get_flags(self, config: dict, workload_path: Path, workload_type: str, benchmark: str = None) -> tuple:
        """Get compiler and linker flags."""
        build_config = self.config.get_build_config(self.arch, self.platform, workload_type, self.bbv, self.trace, benchmark)
        cc = build_config.get('cc')
        cflags = build_config.get('base_cflags', []) + build_config.get('cflags', [])
        cflags.extend(f"-D{define}" for define in build_config.get('defines', []))
        cflags.extend(f"-I{inc}" for inc in self.config.get_workload_includes(workload_path, workload_type))
        cflags.append(f"-I{workload_path}/env")
        if self.platform == "baremetal":
            cflags.append(f"-I{self.env_dir}")
        ldflags = build_config.get('base_ldflags', [])
        return cc, cflags, ldflags, build_config

    def build_environment(self, workload: str):
        """Compile environment runtime files."""
        if self.config.should_skip_environment(self.platform, workload):
            Util.log(LogLevel.INFO, f"Skipping environment build for {self.platform}")
            return
        cc, cflags, _, _ = self._get_flags({}, workload, workload)
        for src in self.config.get_environment_files(workload):
            src_file = self.env_dir / src
            if src_file.exists():
                obj = self.env_dir / f"{Path(src).stem}.o"
                Util.run_cmd([cc, "-c", *cflags, "-o", str(obj), str(src_file)])

    def build_common_files(self, workload_path: Path, workload_type: str) -> List[str]:
        """Compile common files for riscv-tests."""
        if workload_type != "riscv-tests" or not (common_dir := workload_path / "benchmarks" / "common").exists():
            return []
        cc, cflags, _, _ = self._get_flags({}, workload_path, workload_type)
        skip = self.config.get_skip_common_files(self.platform, workload_type)
        obj_files = []
        for c_file in common_dir.glob("*.c"):
            if c_file.name in skip:
                continue
            obj = self.bin_dir / f"{c_file.stem}.o"
            if Util.run_cmd([cc, "-c", *cflags, "-o", str(obj), str(c_file)]):
                obj_files.append(str(obj))
        return obj_files

    def build_benchmark(self, bench: str, workload_path: Path, workload_type: str, common_objs: List[str]):
        """Compile and link a single benchmark."""
        Util.log(LogLevel.INFO, f"Building {bench}")
        bench_dir = workload_path / ("src" if workload_type == "embench-iot" else "benchmarks") / bench
        if not bench_dir.exists():
            Util.log(LogLevel.ERROR, f"Benchmark directory not found: {bench_dir}")
        
        # Find source files
        source_exts = ['.c'] if workload_type == "embench-iot" else ['.c', '.S']
        sources = [f for ext in source_exts for f in bench_dir.glob(f"*{ext}")]
        if not sources:
            Util.log(LogLevel.ERROR, f"No sources found for {bench}")
        
        # Compile sources
        cc, cflags, ldflags, config = self._get_flags({}, workload_path, workload_type, bench)
        obj_files = []
        for src in sources:
            obj = self.bin_dir / f"{src.stem}.o"
            if Util.run_cmd([cc, "-c", *cflags, "-o", str(obj), str(src)]):
                obj_files.append(str(obj))
        
        # Compile additional sources for embench-iot
        if workload_type == "embench-iot":
            for src in config.get('workload_sources', []):
                src_path = Path(src)
                if src_path.exists():
                    obj = self.bin_dir / f"{src_path.stem}_support.o"
                    if Util.run_cmd([cc, "-c", *cflags, "-o", str(obj), str(src_path)]):
                        obj_files.append(str(obj))
        
        # Link executable
        if common_objs:
            obj_files.extend(common_objs)
        exe = self.bin_dir / bench
        link_cmd = [cc, *ldflags, "-o", str(exe), *obj_files]
        if self.platform == "baremetal":
            link_cmd.extend([f"-T{self.env_dir / config.get('linker_script', 'link.ld')}",
                             *[str(self.env_dir / f"{Path(f).stem}.o") for f in self.config.get_environment_files(workload_type)]])
        link_cmd.extend(config.get('libs', []))
        if Util.run_cmd(link_cmd):
            self.executables.append(str(exe))

    def list_benchmarks(self, workload_path: Path, workload_type: str) -> List[str]:
        """List available benchmarks for a workload."""
        dir_path = workload_path / ("src" if workload_type == "embench-iot" else "benchmarks")
        if not dir_path.exists():
            return []
        return [d.name for d in dir_path.iterdir() if d.is_dir() and (workload_type != "riscv-tests" or d.name != "common")]

    def build_workload(self, workload: str, benchmark: str = None, custom_path: str = None):
        """Build specified workload or benchmark."""
        workload_path = Path(custom_path or DEFAULT_WORKLOADS.get(workload, DEFAULT_WORKLOADS["riscv-tests"]))
        workload_type = workload if workload in DEFAULT_WORKLOADS else "custom"
        if not Util.file_exists(workload_path):
            Util.log(LogLevel.ERROR, f"Workload path not found: {workload_path}")
        
        Util.log(LogLevel.INFO, f"Building {workload} for {self.arch}/{self.platform}/{self.board}")
        self.build_environment(workload_type)
        common_objs = self.build_common_files(workload_path, workload_type)
        benchmarks = [benchmark] if benchmark else (["dhrystone"] if workload == "dhrystone" else self.list_benchmarks(workload_path, workload_type))
        
        for bench in benchmarks:
            self.build_benchmark(bench, workload_path, workload_type, common_objs)
        
        Util.log(LogLevel.INFO, f"Built {len(self.executables)} executables in {self.bin_dir}")

def main():
    """Main entry point for building workloads."""
    parser = argparse.ArgumentParser(description="Build RISC-V workloads")
    parser.add_argument("--workload", help="Workload name")
    parser.add_argument("--arch", default="rv32", choices=["rv32", "rv64"])
    parser.add_argument("--platform", default="baremetal", choices=["baremetal", "linux"])
    parser.add_argument("--board", default="spike", choices=["spike", "qemu"])
    parser.add_argument("--benchmark", help="Specific benchmark")
    parser.add_argument("--custom-path", help="Custom workload path")
    parser.add_argument("--bbv", action="store_true", help="Enable BBV support")
    parser.add_argument("--trace", action="store_true", help="Enable tracing")
    parser.add_argument("--list", action="store_true", help="List available workloads")
    args = parser.parse_args()

    if args.list:
        for name, path in DEFAULT_WORKLOADS.items():
            if Util.file_exists(path):
                Util.log(LogLevel.INFO, f"{name}: {path}")
                builder = WorkloadBuilder(args.board, args.arch, args.platform, args.bbv, args.trace)
                benchmarks = builder.list_benchmarks(Path(path), name)
                if benchmarks:
                    Util.log(LogLevel.INFO, f"  Benchmarks: {', '.join(benchmarks[:10])}{'...' if len(benchmarks) > 10 else ''}")
        return

    if not args.workload:
        Util.log(LogLevel.ERROR, "Workload required. Use --list to see available workloads")
    builder = WorkloadBuilder(args.board, args.arch, args.platform, args.bbv, args.trace)
    builder.build_workload(args.workload, args.benchmark, args.custom_path)

if __name__ == "__main__":
    main()

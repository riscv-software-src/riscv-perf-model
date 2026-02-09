#!/usr/bin/env python3
"""Run RISC-V workloads on Spike or QEMU and capture BBV/trace artefacts."""
from __future__ import annotations

import argparse
import json
import shlex
import shutil
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Dict, List, Optional
import sys

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from data.consts import Const
from flow.utils.paths import BenchmarkPaths, binaries_root
from flow.utils.util import CommandError, CommandResult, Util


DEFAULT_ISA = {"rv32": "rv32imafdc", "rv64": "rv64imafdc"}


@dataclass
class RunTarget:
    workload: str
    benchmark: str
    binary: Path
    build_meta: Dict[str, Any]

    @property
    def isa(self) -> Optional[str]:
        return self.build_meta.get("isa") if self.build_meta else None

    @property
    def features(self) -> Dict[str, bool]:
        if not self.build_meta:
            return {}
        return self.build_meta.get("features", {})


# -----------------------------------------------------------------------------
# Discovery helpers
# -----------------------------------------------------------------------------

def _load_build_meta(binary_dir: Path) -> Dict[str, Any]:
    meta_path = binary_dir / "build_meta.json"
    if meta_path.exists():
        try:
            return Util.read_json(meta_path)
        except json.JSONDecodeError as err:
            Util.warn(f"Failed to parse build metadata {meta_path}: {err}")
    return {}


def _discover_from_outputs(args: argparse.Namespace) -> List[RunTarget]:
    root = binaries_root(args.emulator)
    if not root.exists():
        raise SystemExit(f"No binaries found. Expected directory {root}")

    targets: List[RunTarget] = []
    for workload_dir in sorted(root.iterdir()):
        if not workload_dir.is_dir() or workload_dir.name in {"env", "wrapper"}:
            continue
        if args.workload and workload_dir.name != args.workload:
            continue
        for bench_dir in sorted(workload_dir.iterdir()):
            if not bench_dir.is_dir():
                continue
            bench_name = bench_dir.name
            if args.benchmark and bench_name != args.benchmark:
                continue
            binary = bench_dir / bench_name
            if not binary.exists():
                continue
            meta = _load_build_meta(bench_dir)
            targets.append(RunTarget(workload_dir.name, bench_name, binary, meta))
    if not targets:
        raise SystemExit("No matching binaries found. Did you run build_workload.py?")
    return targets


def _target_from_binary(args: argparse.Namespace) -> RunTarget:
    binary = Path(args.binary).resolve()
    if not binary.exists():
        raise SystemExit(f"Binary not found: {binary}")
    if not args.workload or not args.benchmark:
        raise SystemExit("--binary requires both --workload and --benchmark to locate outputs")
    meta = {}
    bench_dir = binary.parent
    meta_path = bench_dir / "build_meta.json"
    if meta_path.exists():
        meta = _load_build_meta(bench_dir)
    return RunTarget(args.workload, args.benchmark, binary, meta)


# -----------------------------------------------------------------------------
# Command assembly
# -----------------------------------------------------------------------------

def _spike_command(
    target: RunTarget,
    paths: BenchmarkPaths,
    *,
    isa: str,
    bbv: bool,
    trace: bool,
    interval_size: int,
    platform: str,
) -> Dict[str, Any]:
    cmd: List[str] = ["spike", f"--isa={isa}"]
    bbv_file: Optional[Path] = None
    trace_file: Optional[Path] = None

    if bbv:
        paths.bbv_dir.mkdir(parents=True, exist_ok=True)
        bbv_file = paths.bbv_dir / f"{target.benchmark}.bbv"
        cmd.extend([
            "--en_bbv",
            f"--bb_file={bbv_file}",
            f"--simpoint_size={interval_size}",
        ])

    if trace:
        paths.trace_dir.mkdir(parents=True, exist_ok=True)
        trace_file = paths.trace_dir / f"{target.benchmark}.full.zstf"
        cmd.extend([
            "--stf_trace_memory_records",
            "--stf_macro_tracing",
            f"--stf_trace={trace_file}",
        ])

    binary_cmd: List[str]
    if platform == "linux":
        binary_cmd = [Const.SPIKE_PK, str(target.binary)]
    else:
        binary_cmd = [str(target.binary)]

    return {
        "argv": cmd + binary_cmd,
        "bbv_file": bbv_file,
        "trace_file": trace_file,
    }


def _qemu_command(
    target: RunTarget,
    paths: BenchmarkPaths,
    *,
    arch: str,
    platform: str,
    bbv: bool,
    trace: bool,
    interval_size: int,
) -> Dict[str, Any]:
    bits = 32 if arch == "rv32" else 64
    cmd: List[str]
    bbv_file: Optional[Path] = None
    trace_file: Optional[Path] = None

    if platform == "baremetal":
        cmd = [f"qemu-system-riscv{bits}", "-nographic", "-machine", "virt", "-bios", "none", "-kernel", str(target.binary)]
    else:
        cmd = [f"qemu-riscv{bits}", str(target.binary)]

    if bbv:
        plugin = "/usr/lib/libbbv.so"
        if not Path(plugin).exists():
            Util.warn("BBV requested for QEMU but libbbv.so not found; skipping")
        else:
            paths.bbv_dir.mkdir(parents=True, exist_ok=True)
            bbv_file = paths.bbv_dir / f"{target.benchmark}.bbv"
            cmd.extend([
                "-plugin",
                f"{plugin},interval={interval_size},outfile={bbv_file}",
            ])

    if trace:
        plugin = Const.LIBSTFMEM
        if not Path(plugin).exists():
            Util.warn("Trace requested for QEMU but libstfmem.so not found; skipping")
        else:
            paths.trace_dir.mkdir(parents=True, exist_ok=True)
            trace_file = paths.trace_dir / f"{target.benchmark}.full.zstf"
            cmd.extend([
                "-plugin",
                f"{plugin},mode=dyn_insn_count,start_dyn_insn=0,num_instructions=18446744073709551615,outfile={trace_file}",
                "-d",
                "plugin",
            ])
    return {
        "argv": cmd,
        "bbv_file": bbv_file,
        "trace_file": trace_file,
    }


# -----------------------------------------------------------------------------
# Execution
# -----------------------------------------------------------------------------

def _write_logs(paths: BenchmarkPaths, target: RunTarget, result: CommandResult) -> None:
    paths.logs_dir.mkdir(parents=True, exist_ok=True)
    log_path = paths.logs_dir / f"{target.benchmark}.log"
    log_path.write_text(result.stdout + ("\n" + result.stderr if result.stderr else ""))


def _clean_outputs(paths: BenchmarkPaths) -> None:
    for directory in (paths.bbv_dir, paths.trace_dir, paths.logs_dir):
        if directory.exists():
            shutil.rmtree(directory)


def _write_run_metadata(
    paths: BenchmarkPaths,
    target: RunTarget,
    *,
    args: argparse.Namespace,
    isa: str,
    bbv: bool,
    trace: bool,
    bbv_file: Optional[Path],
    trace_file: Optional[Path],
    command: List[str],
    elapsed: float,
) -> None:
    metadata = {
        "timestamp": Util.now_iso(),
        "emulator": args.emulator,
        "arch": args.arch,
        "platform": args.platform,
        "workload": target.workload,
        "benchmark": target.benchmark,
        "binary": str(target.binary),
        "isa": isa,
        "interval_size": args.interval_size,
        "bbv_enabled": bbv,
        "trace_enabled": trace,
        "bbv_file": str(bbv_file) if bbv_file else None,
        "trace_file": str(trace_file) if trace_file else None,
        "command": command,
        "elapsed_seconds": elapsed,
        "build_meta": target.build_meta,
    }
    Util.write_json(paths.run_meta_path, metadata)


# -----------------------------------------------------------------------------
# CLI handling
# -----------------------------------------------------------------------------

def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run workloads on Spike or QEMU")
    parser.add_argument("--emulator", required=True, choices=["spike", "qemu"])
    parser.add_argument("--platform", default="baremetal", choices=["baremetal", "linux"])
    parser.add_argument("--arch", default="rv32", choices=["rv32", "rv64"])
    parser.add_argument("--workload", help="Filter to a specific workload")
    parser.add_argument("--benchmark", help="Filter to a single benchmark")
    parser.add_argument("--binary", help="Run a specific binary instead of discovered outputs")
    parser.add_argument("--interval-size", type=int, default=10_000_000)
    parser.add_argument("--stf-dump", action="store_true", help="Run stf_dump on generated traces")
    parser.add_argument("--clean", action="store_true", help="Clean bbv/trace/log directories before running")
    parser.add_argument("--list", action="store_true", help="List discovered binaries and exit")
    parser.set_defaults(bbv=None, trace=None)
    parser.add_argument("--bbv", dest="bbv", action="store_true", help="Enable BBV generation during run")
    parser.add_argument("--no-bbv", dest="bbv", action="store_false", help="Disable BBV regardless of build metadata")
    parser.add_argument("--trace", dest="trace", action="store_true", help="Generate full STF traces during run")
    parser.add_argument("--no-trace", dest="trace", action="store_false", help="Disable trace generation")
    parser.add_argument("--isa", help="Override ISA passed to Spike (defaults to build metadata or arch mapping)")
    return parser.parse_args()


def _list_targets(targets: List[RunTarget]) -> None:
    Util.info("Discovered binaries:")
    for target in targets:
        Util.info(f"  {target.workload}/{target.benchmark}: {target.binary}")


def _effective_flag(value: Optional[bool], default: bool) -> bool:
    return default if value is None else value


def run_targets(args: argparse.Namespace, targets: List[RunTarget]) -> None:
    Util.info(f"Running {len(targets)} benchmark(s) on {args.emulator}")

    for target in targets:
        paths = BenchmarkPaths(args.emulator, target.workload, target.benchmark)
        paths.resolve()
        if args.clean:
            _clean_outputs(paths)

        meta_features = target.features
        bbv = _effective_flag(args.bbv, meta_features.get("bbv", False))
        trace = _effective_flag(args.trace, meta_features.get("trace", False))

        if args.emulator == "spike":
            isa = args.isa or target.isa or DEFAULT_ISA.get(args.arch, args.arch)
            command_info = _spike_command(target, paths, isa=isa, bbv=bbv, trace=trace, interval_size=args.interval_size, platform=args.platform)
        else:
            isa = args.isa or target.isa or DEFAULT_ISA.get(args.arch, args.arch)
            command_info = _qemu_command(target, paths, arch=args.arch, platform=args.platform, bbv=bbv, trace=trace, interval_size=args.interval_size)

        argv = command_info["argv"]
        bbv_file = command_info.get("bbv_file")
        trace_file = command_info.get("trace_file")

        Util.info(f"Running {target.workload}/{target.benchmark}")
        Util.info("Command: " + " ".join(shlex.quote(arg) for arg in argv))

        start = time.time()
        try:
            result = Util.run_cmd(argv)
        except CommandError as err:
            raise SystemExit(str(err))
        elapsed = time.time() - start

        # Spike appends suffixes like _cpu0 to BBV files; capture the file that actually exists.
        if bbv and bbv_file and not bbv_file.exists():
            candidates = sorted(paths.bbv_dir.glob(f"{target.benchmark}.bbv*"))
            if candidates:
                bbv_file = candidates[0]
        if trace and trace_file and not trace_file.exists():
            candidates = sorted(paths.trace_dir.glob(f"{target.benchmark}*.zstf"))
            if candidates:
                trace_file = candidates[0]

        _write_logs(paths, target, result)
        _write_run_metadata(
            paths,
            target,
            args=args,
            isa=isa,
            bbv=bbv,
            trace=trace,
            bbv_file=bbv_file,
            trace_file=trace_file,
            command=list(argv),
            elapsed=elapsed,
        )

        Util.info(f"Completed {target.benchmark} in {elapsed:.2f}s")

        if trace and args.stf_dump and trace_file and trace_file.exists():
            dump_tool = Path(Const.STF_TOOLS) / "stf_dump" / "stf_dump"
            if dump_tool.exists():
                Util.info(f"Dumping trace with {dump_tool}")
                try:
                    Util.run_cmd([str(dump_tool), str(trace_file)], capture_output=False)
                except CommandError as err:
                    Util.warn(f"stf_dump failed: {err}")


# -----------------------------------------------------------------------------
# Main entry point
# -----------------------------------------------------------------------------

def main() -> None:
    args = parse_args()

    if args.emulator == "spike":
        tool_to_check = "spike"
    else:
        bits = 32 if args.arch == "rv32" else 64
        tool_to_check = f"qemu-{'system-' if args.platform == 'baremetal' else ''}riscv{bits}"

    if not Util.validate_tool(tool_to_check):
        raise SystemExit(1)

    if args.binary:
        targets = [_target_from_binary(args)]
    else:
        targets = _discover_from_outputs(args)

    if args.list:
        _list_targets(targets)
        return

    run_targets(args, targets)


if __name__ == "__main__":
    main()

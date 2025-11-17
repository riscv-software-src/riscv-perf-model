#!/usr/bin/env python3
"""Generate STF traces either as single windows or SimPoint-sliced sets."""
from __future__ import annotations

import argparse
import json
import re
import shutil
from dataclasses import asdict
from pathlib import Path
from typing import Dict, List, Optional
import sys

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from data.consts import Const
from factories.metadata_factory import MetadataFactory
from flow.utils.paths import BenchmarkPaths, simpoint_analysis_root
from flow.utils.util import CommandError, Util


def resolve_output(binary: Path, output: Optional[str]) -> Path:
    if output is None:
        return binary.with_suffix(".zstf")
    candidate = Path(output)
    if candidate.exists() and candidate.is_dir():
        return candidate / (binary.stem + ".zstf")
    if candidate.suffix in {".zstf", ".stf"}:
        return candidate
    if not candidate.exists() and not candidate.suffix:
        return candidate / (binary.stem + ".zstf")
    raise SystemExit("--output must point to a directory or .zstf/.stf file")


def write_metadata(trace_path: Path, metadata) -> Path:
    metadata_path = trace_path.with_suffix(".metadata.json")
    metadata_path.write_text(json.dumps(asdict(metadata), indent=2) + "\n")
    return metadata_path


def run_stf_tool(tool: str, trace_path: Path) -> Optional[str]:
    tool_binary = Path(Const.STF_TOOLS) / tool / tool
    if not tool_binary.exists():
        Util.warn(f"{tool} tool not available; skipping")
        return None
    try:
        result = Util.run_cmd([str(tool_binary), str(trace_path)])
    except CommandError as err:
        Util.warn(f"{tool} failed: {err}")
        return None
    return result.stdout


def dump_trace(trace_path: Path) -> None:
    dump_stdout = run_stf_tool("stf_dump", trace_path)
    if dump_stdout is None:
        return
    trace_path.with_suffix(".dump").write_text(dump_stdout)


def verify_trace(trace_path: Path, expected: int) -> Dict[str, object]:
    output = run_stf_tool("stf_count", trace_path)
    if output is None:
        return {"verified": None, "counted": None}
    numbers = [int(x) for x in re.findall(r"\d+", output)]
    counted = numbers[-1] if numbers else None
    verified = counted is not None and abs(counted - expected) <= 1
    return {"verified": verified, "counted": counted}


class TraceGenerator:
    """Encapsulates single-trace generation for reuse across scripts."""

    def __init__(self, metadata_factory: Optional[MetadataFactory] = None) -> None:
        self.metadata_factory = metadata_factory or MetadataFactory()

    def run(self, args: argparse.Namespace) -> Path:
        binary = Path(args.binary).resolve()
        if not binary.exists():
            raise SystemExit(f"Binary not found: {binary}")

        output_path = resolve_output(binary, args.output).resolve()
        output_path.parent.mkdir(parents=True, exist_ok=True)

        self._run_trace(args, binary, output_path)
        metadata = self._build_metadata(args, binary, output_path)
        write_metadata(output_path, metadata)

        if getattr(args, "dump", False):
            dump_trace(output_path)

        Util.info(f"Trace generated at {output_path}")
        return output_path

    def _run_trace(self, args: argparse.Namespace, binary: Path, output_path: Path) -> None:
        if args.emulator == "spike":
            cmd = self._spike_command(args, binary, output_path)
        elif args.emulator == "qemu":
            cmd = self._qemu_command(args, binary, output_path)
        else:
            raise SystemExit(f"Unsupported emulator: {args.emulator}")

        Util.info("Generating trace: " + " ".join(str(part) for part in cmd))
        try:
            Util.run_cmd(cmd)
        except CommandError as err:
            raise SystemExit(str(err))

    def _spike_command(self, args: argparse.Namespace, binary: Path, output_path: Path) -> List[str]:
        cmd: List[str] = ["spike"]
        if args.isa:
            cmd.append(f"--isa={args.isa}")
        cmd.extend(["--stf_trace_memory_records", f"--stf_trace={output_path}"])

        if args.mode == "macro":
            cmd.append("--stf_macro_tracing")
        elif args.mode == "insn_count":
            if args.num_instructions is None:
                raise SystemExit("--num-instructions is required for insn_count mode")
            cmd.extend(
                [
                    "--stf_insn_num_tracing",
                    "--stf_insn_start",
                    str(args.start_instruction or 0),
                    "--stf_insn_count",
                    str(args.num_instructions),
                ]
            )
        else:
            raise SystemExit("Spike does not support pc_count mode")

        if args.pk:
            cmd.append(Const.SPIKE_PK)
        cmd.append(str(binary))
        return cmd

    def _qemu_command(self, args: argparse.Namespace, binary: Path, output_path: Path) -> List[str]:
        bits = 32 if args.arch == "rv32" else 64
        cmd: List[str] = [f"qemu-riscv{bits}", str(binary)]
        plugin = Path(Const.LIBSTFMEM)
        if not plugin.exists():
            raise SystemExit(f"STF plugin not found: {plugin}")

        if args.mode == "insn_count":
            if args.num_instructions is None:
                raise SystemExit("--num-instructions is required for insn_count mode")
            start_dyn = (args.start_instruction or 0) + 1
            plugin_cfg = (
                f"{plugin},mode=dyn_insn_count,start_dyn_insn={start_dyn},"
                f"num_instructions={args.num_instructions},outfile={output_path}"
            )
        elif args.mode == "pc_count":
            if args.num_instructions is None or args.start_pc is None:
                raise SystemExit("pc_count requires --num-instructions and --start-pc")
            plugin_cfg = (
                f"{plugin},mode=ip,start_ip={args.start_pc},ip_hit_threshold={args.pc_threshold},"
                f"num_instructions={args.num_instructions},outfile={output_path}"
            )
        else:
            raise SystemExit("Macro tracing is not available via QEMU")

        cmd.extend(["-plugin", plugin_cfg, "-d", "plugin"])
        return cmd

    def _build_metadata(self, args: argparse.Namespace, binary: Path, output_path: Path):
        interval_mode = {
            "macro": "macro",
            "insn_count": "instructionCount",
            "pc_count": "ip",
        }[args.mode]
        return self.metadata_factory.create(
            workload_path=str(binary),
            trace_path=str(output_path),
            trace_interval_mode=interval_mode,
            start_instruction=getattr(args, "start_instruction", None),
            num_instructions=getattr(args, "num_instructions", None),
            start_pc=getattr(args, "start_pc", None),
            pc_threshold=getattr(args, "pc_threshold", None),
        )


def run_single(args: argparse.Namespace) -> None:
    TraceGenerator().run(args)


def parse_simpoints(simpoints: Path, weights: Path) -> List[Dict[str, float]]:
    if not simpoints.exists() or not weights.exists():
        raise SystemExit("SimPoint files missing")

    intervals: Dict[int, int] = {}
    for line in simpoints.read_text().splitlines():
        if not line.strip():
            continue
        idx, cluster = line.split()
        intervals[int(cluster)] = int(idx)

    weights_map: Dict[int, float] = {}
    for line in weights.read_text().splitlines():
        if not line.strip():
            continue
        weight, cluster = line.split()
        weights_map[int(cluster)] = float(weight)

    entries: List[Dict[str, float]] = []
    for cluster, interval in intervals.items():
        if cluster not in weights_map:
            continue
        entries.append({"cluster": cluster, "interval": interval, "weight": weights_map[cluster]})
    entries.sort(key=lambda item: item["interval"])
    return entries


def run_sliced(args: argparse.Namespace) -> None:
    paths = BenchmarkPaths(args.emulator, args.workload, args.benchmark)
    run_meta_path = paths.run_meta_path
    if not run_meta_path.exists():
        raise SystemExit(f"run_meta.json not found. Run run_workload.py first: {run_meta_path}")

    run_meta = Util.read_json(run_meta_path)
    interval_size = args.interval_size or run_meta.get("interval_size")
    if not interval_size:
        raise SystemExit("Interval size missing; pass --interval-size")
    isa = run_meta.get("isa")
    if not isa:
        raise SystemExit("ISA missing from run metadata; rebuild/run with updated tooling")
    binary = Path(run_meta.get("binary", ""))
    if not binary.exists():
        raise SystemExit(f"Binary not found: {binary}")
    platform = run_meta.get("platform", "baremetal")

    simpoints = Path(args.simpoints) if args.simpoints else simpoint_analysis_root() / f"{args.benchmark}.simpoints"
    weights = Path(args.weights) if args.weights else simpoint_analysis_root() / f"{args.benchmark}.weights"
    entries = parse_simpoints(simpoints, weights)
    if not entries:
        raise SystemExit("No SimPoint entries detected")

    output_dir = paths.simpoint_dir
    if args.clean and output_dir.exists():
        shutil.rmtree(output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    Util.info(f"Generating {len(entries)} sliced traces for {args.workload}/{args.benchmark}")

    manifest_entries: List[Dict[str, object]] = []
    metadata_factory = MetadataFactory()

    for entry in entries:
        interval_idx = entry["interval"]
        weight = entry["weight"]
        start = interval_idx * interval_size
        trace_path = output_dir / f"{args.benchmark}.sp_{interval_idx}.zstf"

        cmd = [
            "spike",
            f"--isa={isa}",
            "--stf_trace_memory_records",
            "--stf_insn_num_tracing",
            "--stf_insn_start",
            str(start),
            "--stf_insn_count",
            str(interval_size),
            f"--stf_trace={trace_path}",
        ]
        if platform == "linux":
            cmd.append(Const.SPIKE_PK)
        cmd.append(str(binary))

        Util.info(f"Slice interval {interval_idx} (weight={weight:.6f})")
        try:
            Util.run_cmd(cmd)
        except CommandError as err:
            raise SystemExit(str(err))

        verification = {"verified": None, "counted": None}
        if args.verify:
            verification = verify_trace(trace_path, interval_size)

        if args.dump:
            dump_trace(trace_path)

        metadata = metadata_factory.create(
            workload_path=str(binary),
            trace_path=str(trace_path),
            trace_interval_mode="instructionCount",
            start_instruction=start,
            num_instructions=interval_size,
        )
        metadata_path = write_metadata(trace_path, metadata)

        manifest_entries.append(
            {
                "interval_index": interval_idx,
                "weight": weight,
                "start_instruction": start,
                "num_instructions": interval_size,
                "trace": str(trace_path),
                "metadata": str(metadata_path),
                "verified": verification.get("verified"),
                "counted_instructions": verification.get("counted"),
            }
        )

    manifest = {
        "workload": args.workload,
        "benchmark": args.benchmark,
        "emulator": args.emulator,
        "interval_size": interval_size,
        "generated_at": Util.now_iso(),
        "simpoints": str(simpoints),
        "weights": str(weights),
        "slices": manifest_entries,
        "total_weight": sum(item["weight"] for item in entries),
    }
    manifest_path = output_dir / "slices.json"
    Util.write_json(manifest_path, manifest)
    Util.info(f"SimPoint slices stored under {output_dir}")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Trace generation toolkit")
    sub = parser.add_subparsers(dest="command", required=True)

    single = sub.add_parser("single", help="Generate a single STF trace window")
    single_modes = single.add_subparsers(dest="mode", required=True, metavar="{macro,insn_count,pc_count}")

    single_common = argparse.ArgumentParser(add_help=False)
    single_common.add_argument("--emulator", required=True, choices=["spike", "qemu"])
    single_common.add_argument("--arch", choices=["rv32", "rv64"], default="rv64", help="Used for QEMU target selection")
    single_common.add_argument("--isa", help="ISA string for Spike")
    single_common.add_argument("--pk", action="store_true", help="Launch Spike with proxy kernel")
    single_common.add_argument("-o", "--output", help="Output file or directory")
    single_common.add_argument("--dump", action="store_true", help="Emit stf_dump alongside trace")

    macro = single_modes.add_parser(
        "macro",
        parents=[single_common],
        help="Trace using START_TRACE/STOP_TRACE markers (Spike only)",
    )
    macro.add_argument("binary", help="Path to workload binary")
    macro.set_defaults(num_instructions=None, start_instruction=None, start_pc=None, pc_threshold=None)

    insn_count = single_modes.add_parser(
        "insn_count",
        parents=[single_common],
        help="Trace a fixed instruction window",
    )
    insn_count.add_argument("binary", help="Path to workload binary")
    insn_count.add_argument("--num-instructions", required=True, type=int, help="Number of instructions to trace")
    insn_count.add_argument("--start-instruction", type=int, default=0, help="Number of instructions to skip before tracing")
    insn_count.set_defaults(start_pc=None, pc_threshold=None)

    pc_count = single_modes.add_parser(
        "pc_count",
        parents=[single_common],
        help="Trace starting from a PC hit threshold (QEMU only)",
    )
    pc_count.add_argument("binary", help="Path to workload binary")
    pc_count.add_argument("--num-instructions", required=True, type=int, help="Number of instructions to trace")
    pc_count.add_argument("--start-pc", required=True, type=lambda x: int(x, 0), help="PC value that triggers tracing")
    pc_count.add_argument("--pc-threshold", type=int, default=1, help="Number of PC hits before tracing starts")
    pc_count.set_defaults(start_instruction=None)

    sliced = sub.add_parser("sliced", help="Generate SimPoint-sliced traces")
    sliced.add_argument("--emulator", required=True, choices=["spike"], help="Spike is required for slicing")
    sliced.add_argument("--workload", required=True)
    sliced.add_argument("--benchmark", required=True)
    sliced.add_argument("--interval-size", type=int, help="Override interval size (defaults to run metadata)")
    sliced.add_argument("--simpoints", help="Path to .simpoints file")
    sliced.add_argument("--weights", help="Path to .weights file")
    sliced.add_argument("--verify", action="store_true", help="Validate traces via stf_count")
    sliced.add_argument("--dump", action="store_true", help="Emit stf_dump per slice")
    sliced.add_argument("--clean", action="store_true", help="Remove existing slices before generation")

    return parser.parse_args()


def main() -> None:
    args = parse_args()
    if args.command == "single":
        run_single(args)
    else:
        run_sliced(args)


if __name__ == "__main__":
    main()

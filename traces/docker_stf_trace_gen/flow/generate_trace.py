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


def _resolve_output(binary: Path, output: Optional[str]) -> Path:
    if output is None:
        return binary.with_suffix(".zstf")
    candidate = Path(output)
    if candidate.is_dir():
        return candidate / (binary.stem + ".zstf")
    if candidate.suffix in {".zstf", ".stf"}:
        return candidate
    raise SystemExit("--output must point to a directory or .zstf/.stf file")


def _run_single(args: argparse.Namespace) -> None:
    binary = Path(args.binary).resolve()
    if not binary.exists():
        raise SystemExit(f"Binary not found: {binary}")

    output_path = _resolve_output(binary, args.output).resolve()
    output_path.parent.mkdir(parents=True, exist_ok=True)

    if args.emulator == "spike":
        cmd = ["spike"]
        if args.isa:
            cmd.append(f"--isa={args.isa}")
        cmd.extend(["--stf_trace_memory_records", f"--stf_trace={output_path}"])

        if args.mode == "macro":
            cmd.append("--stf_macro_tracing")
        elif args.mode == "insn_count":
            if args.num_instructions is None:
                raise SystemExit("--num-instructions is required for insn_count mode")
            cmd.extend([
                "--stf_insn_num_tracing",
                "--stf_insn_start",
                str(args.start_instruction or 0),
                "--stf_insn_count",
                str(args.num_instructions),
            ])
        else:
            raise SystemExit("Spike does not support pc_count mode")

        if args.pk:
            cmd.append(Const.SPIKE_PK)
        cmd.append(str(binary))

    else:  # QEMU
        bits = 32 if args.arch == "rv32" else 64
        cmd = [f"qemu-riscv{bits}", str(binary)]
        plugin = Const.LIBSTFMEM
        if not Path(plugin).exists():
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

    Util.info("Generating trace: " + " ".join(cmd))
    try:
        Util.run_cmd(cmd)
    except CommandError as err:
        raise SystemExit(str(err))

    metadata = MetadataFactory().create(
        workload_path=str(binary),
        trace_path=str(output_path),
        trace_interval_mode={
            "macro": "macro",
            "insn_count": "instructionCount",
            "pc_count": "ip",
        }[args.mode],
        start_instruction=args.start_instruction,
        num_instructions=args.num_instructions,
        start_pc=args.start_pc,
        pc_threshold=args.pc_threshold,
    )
    metadata_path = output_path.with_suffix(".metadata.json")
    metadata_path.write_text(json.dumps(asdict(metadata), indent=2) + "\n")

    if args.dump:
        dump_tool = Path(Const.STF_TOOLS) / "stf_dump" / "stf_dump"
        if dump_tool.exists():
            dump_result = Util.run_cmd([str(dump_tool), str(output_path)])
            output_path.with_suffix(".dump").write_text(dump_result.stdout)
        else:
            Util.warn("stf_dump tool not available; skipping dump")

    Util.info(f"Trace generated at {output_path}")


def _parse_simpoints(simpoints: Path, weights: Path) -> List[Dict[str, float]]:
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


def _verify_trace(trace_path: Path, expected: int) -> Dict[str, object]:
    count_tool = Path(Const.STF_TOOLS) / "stf_count" / "stf_count"
    if not count_tool.exists():
        Util.warn("stf_count not available; skipping verification")
        return {"verified": None, "counted": None}
    try:
        result = Util.run_cmd([str(count_tool), str(trace_path)])
    except CommandError as err:
        Util.warn(f"stf_count failed: {err}")
        return {"verified": None, "counted": None}
    numbers = [int(x) for x in re.findall(r"\d+", result.stdout)]
    counted = numbers[-1] if numbers else None
    verified = counted is not None and abs(counted - expected) <= 1
    return {"verified": verified, "counted": counted}


def _dump_trace(trace_path: Path) -> None:
    dump_tool = Path(Const.STF_TOOLS) / "stf_dump" / "stf_dump"
    if not dump_tool.exists():
        Util.warn("stf_dump not available; skipping dump")
        return
    try:
        dump_result = Util.run_cmd([str(dump_tool), str(trace_path)])
    except CommandError as err:
        Util.warn(f"stf_dump failed: {err}")
        return
    trace_path.with_suffix(".dump").write_text(dump_result.stdout)


def _run_sliced(args: argparse.Namespace) -> None:
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
    entries = _parse_simpoints(simpoints, weights)
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
            verification = _verify_trace(trace_path, interval_size)

        if args.dump:
            _dump_trace(trace_path)

        metadata = metadata_factory.create(
            workload_path=str(binary),
            trace_path=str(trace_path),
            trace_interval_mode="instructionCount",
            start_instruction=start,
            num_instructions=interval_size,
        )
        metadata_path = trace_path.with_suffix(".metadata.json")
        metadata_path.write_text(json.dumps(asdict(metadata), indent=2) + "\n")

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
    single.add_argument("binary", help="Path to workload binary")
    single.add_argument("--emulator", required=True, choices=["spike", "qemu"])
    single.add_argument("--mode", choices=["macro", "insn_count", "pc_count"], default="macro")
    single.add_argument("--arch", choices=["rv32", "rv64"], default="rv64", help="Used for QEMU target selection")
    single.add_argument("--isa", help="ISA string for Spike")
    single.add_argument("--pk", action="store_true", help="Launch Spike with proxy kernel")
    single.add_argument("--num-instructions", type=int, help="Number of instructions to trace (instruction-count modes)")
    single.add_argument("--start-instruction", type=int, default=0)
    single.add_argument("--start-pc", type=lambda x: int(x, 0))
    single.add_argument("--pc-threshold", type=int, default=1)
    single.add_argument("-o", "--output", help="Output file or directory")
    single.add_argument("--dump", action="store_true", help="Emit stf_dump alongside trace")

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
        _run_single(args)
    else:
        _run_sliced(args)


if __name__ == "__main__":
    main()

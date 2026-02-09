#!/usr/bin/env python3
"""Run SimPoint analysis on BBV traces produced by run_workload.py."""
from __future__ import annotations

import argparse
import json
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List, Optional
import sys

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from flow.utils.paths import BenchmarkPaths, outputs_root, simpoint_analysis_root
from flow.utils.util import CommandError, Util


@dataclass
class BBVTarget:
    workload: str
    benchmark: str
    bbv_file: Path
    run_meta: Path

    @property
    def run_meta_data(self) -> Dict:
        return Util.read_json(self.run_meta)


def _discover_targets(args: argparse.Namespace) -> List[BBVTarget]:
    run_root = outputs_root() / args.emulator
    if not run_root.exists():
        raise SystemExit(f"No run outputs present: {run_root}")

    targets: List[BBVTarget] = []
    for workload_dir in sorted(run_root.iterdir()):
        if workload_dir.name in {"bin", "simpointed"}:
            continue
        if args.workload and workload_dir.name != args.workload:
            continue
        for bench_dir in sorted(workload_dir.iterdir()):
            if not bench_dir.is_dir():
                continue
            if args.benchmark and bench_dir.name != args.benchmark:
                continue
            run_meta = bench_dir / "run_meta.json"
            if not run_meta.exists():
                continue
            data = Util.read_json(run_meta)
            bbv_path = data.get("bbv_file")
            if not bbv_path:
                continue
            bbv_file = Path(bbv_path)
            if not bbv_file.exists() or bbv_file.stat().st_size == 0:
                continue
            targets.append(BBVTarget(workload_dir.name, bench_dir.name, bbv_file, run_meta))
    if not targets:
        raise SystemExit("No BBV files discovered. Did you run with --bbv?")
    return targets


def _run_simpoint(bbv_file: Path, benchmark: str, max_k: int, output_dir: Path) -> Dict:
    output_dir.mkdir(parents=True, exist_ok=True)
    simpoints = output_dir / f"{benchmark}.simpoints"
    weights = output_dir / f"{benchmark}.weights"

    cmd = [
        "simpoint",
        "-loadFVFile",
        str(bbv_file),
        "-maxK",
        str(max_k),
        "-saveSimpoints",
        str(simpoints),
        "-saveSimpointWeights",
        str(weights),
    ]
    try:
        result = Util.run_cmd(cmd)
    except CommandError as err:
        raise SystemExit(str(err))

    if not simpoints.exists() or not weights.exists():
        raise SystemExit(
            "SimPoint completed without producing output files. "
            "Check that 'simpoint' is installed and the BBV file is valid."
        )

    intervals = _join_simpoint_outputs(simpoints, weights)
    coverage = sum(item["weight"] for item in intervals)
    return {
        "simpoints_file": str(simpoints),
        "weights_file": str(weights),
        "intervals": intervals,
        "coverage": coverage,
    }


def _join_simpoint_outputs(simpoints_path: Path, weights_path: Path) -> List[Dict[str, float]]:
    intervals_by_cluster: Dict[int, int] = {}
    for line in simpoints_path.read_text().splitlines():
        if not line.strip():
            continue
        parts = line.split()
        if len(parts) != 2:
            continue
        interval, cluster = parts
        intervals_by_cluster[int(cluster)] = int(interval)

    weights_by_cluster: Dict[int, float] = {}
    for line in weights_path.read_text().splitlines():
        if not line.strip():
            continue
        parts = line.split()
        if len(parts) != 2:
            continue
        weight, cluster = parts
        weights_by_cluster[int(cluster)] = float(weight)

    entries: List[Dict[str, float]] = []
    for cluster, interval in intervals_by_cluster.items():
        weight = weights_by_cluster.get(cluster)
        if weight is None:
            continue
        entries.append({"cluster": cluster, "interval": interval, "weight": weight})
    entries.sort(key=lambda item: item["interval"])
    return entries


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run SimPoint analysis on BBV vectors")
    parser.add_argument("--emulator", required=True, choices=["spike", "qemu"])
    parser.add_argument("--workload", help="Filter workload")
    parser.add_argument("--benchmark", help="Filter benchmark")
    parser.add_argument("--max-k", type=int, default=30)
    parser.add_argument("--output-dir", default=None, help="Override output directory (defaults to /outputs/simpoint_analysis)")
    return parser.parse_args()


def main() -> None:
    args = parse_args()

    if not Util.validate_tool("simpoint"):
        raise SystemExit(1)

    targets = _discover_targets(args)
    output_dir = Path(args.output_dir) if args.output_dir else simpoint_analysis_root()
    summary: Dict[str, Dict] = {}

    for target in targets:
        Util.info(f"Running SimPoint for {target.workload}/{target.benchmark}")
        result = _run_simpoint(target.bbv_file, target.benchmark, args.max_k, output_dir)
        result.update(
            {
                "workload": target.workload,
                "benchmark": target.benchmark,
                "bbv_file": str(target.bbv_file),
            }
        )
        summary[f"{target.workload}:{target.benchmark}"] = result

    summary_path = output_dir / "simpoint_summary.json"
    Util.write_json(summary_path, summary)
    Util.info(f"Wrote SimPoint summary to {summary_path}")


if __name__ == "__main__":
    main()

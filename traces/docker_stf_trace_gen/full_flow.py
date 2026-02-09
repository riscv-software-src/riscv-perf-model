#!/usr/bin/env python3
"""Host-side orchestrator that runs the full workload→SimPoint→slicing flow."""
from __future__ import annotations

import argparse

from data.consts import Const
from flow.utils.docker_orchestrator import DockerOrchestrator
from flow.utils.util import CommandError, Util


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="End-to-end RISC-V trace pipeline")
    parser.add_argument("--workload", required=True, help="Workload name from board.yaml")
    parser.add_argument("--benchmark", help="Specific benchmark (required for slicing)")
    parser.add_argument("--emulator", choices=["spike", "qemu"], default="spike")
    parser.add_argument("--arch", choices=["rv32", "rv64"], default="rv32")
    parser.add_argument("--platform", choices=["baremetal", "linux"], default="baremetal")

    parser.add_argument("--bbv", action="store_true", help="Compile/run with BBV support")
    parser.add_argument("--trace", action="store_true", help="Compile/run with trace macros")
    parser.add_argument("--interval-size", type=int, default=10_000_000, help="BBV interval size for run phase")

    parser.add_argument("--skip-build", action="store_true")
    parser.add_argument("--skip-run", action="store_true")

    parser.add_argument("--simpoint", action="store_true", help="Execute run_simpoint.py after workload run")
    parser.add_argument("--simpoint-max-k", type=int, default=30)

    parser.add_argument("--slice", action="store_true", help="Generate SimPoint-sliced traces")
    parser.add_argument("--slice-verify", action="store_true", help="Verify sliced traces via stf_count")
    parser.add_argument("--slice-dump", action="store_true", help="Emit stf_dump for each slice")
    parser.add_argument("--slice-clean", action="store_true", help="Clean slice directory before regeneration")
    parser.add_argument("--slice-interval-size", type=int, help="Override interval size during slicing")

    parser.add_argument("--image-name", default=Const.DOCKER_IMAGE_NAME)
    parser.add_argument("--list-benchmarks", action="store_true", help="List available workloads/benchmarks inside container")
    return parser.parse_args()


def run_command(orchestrator: DockerOrchestrator, argv: list[str]) -> None:
    Util.info("→ " + " ".join(argv))
    try:
        orchestrator.run(argv)
    except CommandError as err:
        raise SystemExit(str(err))


def main() -> None:
    args = parse_args()
    orchestrator = DockerOrchestrator(args.image_name)

    if args.list_benchmarks:
        run_command(orchestrator, ["python3", "flow/build_workload.py", "--list", "--emulator", args.emulator])
        return

    if not args.skip_build:
        build_cmd = [
            "python3",
            "flow/build_workload.py",
            "--workload",
            args.workload,
            "--arch",
            args.arch,
            "--platform",
            args.platform,
            "--emulator",
            args.emulator,
        ]
        if args.benchmark:
            build_cmd.extend(["--benchmark", args.benchmark])
        if args.bbv:
            build_cmd.append("--bbv")
        if args.trace:
            build_cmd.append("--trace")
        run_command(orchestrator, build_cmd)

    if not args.skip_run:
        run_cmd = [
            "python3",
            "flow/run_workload.py",
            "--emulator",
            args.emulator,
            "--platform",
            args.platform,
            "--arch",
            args.arch,
            "--interval-size",
            str(args.interval_size),
            "--workload",
            args.workload,
        ]
        if args.benchmark:
            run_cmd.extend(["--benchmark", args.benchmark])
        if args.bbv:
            run_cmd.append("--bbv")
        if args.trace:
            run_cmd.append("--trace")
        run_command(orchestrator, run_cmd)

    if args.simpoint:
        sim_cmd = [
            "python3",
            "flow/run_simpoint.py",
            "--emulator",
            args.emulator,
            "--workload",
            args.workload,
            "--max-k",
            str(args.simpoint_max_k),
        ]
        if args.benchmark:
            sim_cmd.extend(["--benchmark", args.benchmark])
        run_command(orchestrator, sim_cmd)

    if args.slice:
        if not args.benchmark:
            raise SystemExit("--slice requires --benchmark")
        slice_cmd = [
            "python3",
            "flow/generate_trace.py",
            "sliced",
            "--emulator",
            args.emulator,
            "--workload",
            args.workload,
            "--benchmark",
            args.benchmark,
        ]
        if args.slice_interval_size:
            slice_cmd.extend(["--interval-size", str(args.slice_interval_size)])
        if args.slice_verify:
            slice_cmd.append("--verify")
        if args.slice_dump:
            slice_cmd.append("--dump")
        if args.slice_clean:
            slice_cmd.append("--clean")
        run_command(orchestrator, slice_cmd)

    Util.info("Workflow completed")


if __name__ == "__main__":
    main()

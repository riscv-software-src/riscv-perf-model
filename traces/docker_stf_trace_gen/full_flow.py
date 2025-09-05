#!/usr/bin/env python3
"""Orchestrates RISC-V workload analysis with Docker."""
import argparse
import sys
import json
import time
from pathlib import Path
from typing import List, Tuple, Dict
from utils.util import Util, LogLevel
from utils.config import BoardConfig


# do we need these functions in util.py? they seem generic enough
def discover_workloads() -> Dict[str, str]:
    """Discover available workloads."""
    base_dir = Path.cwd().parent / "workloads" if (Path.cwd().parent / "workloads").exists() else Path("/workloads")
    workloads = {
        "embench-iot": str(base_dir / "embench-iot"),
        "riscv-tests": str(base_dir / "riscv-tests"),
        "dhrystone": str(base_dir / "riscv-tests")
    }
    return {k: v for k, v in workloads.items() if Util.file_exists(v)}

def get_benchmarks(workload: str, board: str = 'spike') -> List[str]:
    """Get benchmarks for a workload."""
    workloads = discover_workloads()
    workload_path = Path(workloads.get(workload, workloads.get("riscv-tests", "")))
    if workload == "embench-iot":
        src_dir = workload_path / "src"
        return [d.name for d in src_dir.iterdir() if d.is_dir()] if src_dir.exists() else []
    elif workload in ["riscv-tests", "dhrystone"]:
        bench_dir = workload_path / "benchmarks"
        return [d.name for d in bench_dir.iterdir() if d.is_dir() and d.name != "common"] if bench_dir.exists() else []
    return []

def get_board_config(board: str) -> Dict:
    """Get board configuration."""
    try:
        config = BoardConfig(board)
        sample = config.get_build_config('rv32', 'baremetal')
        return {
            'cc': sample.get('cc', 'unknown'),
            'supported_archs': ['rv32', 'rv64'],
            'supported_platforms': ['baremetal', 'linux'],
            'features': ['bbv', 'trace'] if board == 'spike' else ['bbv', 'trace']
        }
    except Exception as e:
        Util.log(LogLevel.WARN, f"Could not load board config: {e}")
        return {'cc': 'unknown', 'supported_archs': ['rv32', 'rv64'], 'supported_platforms': ['baremetal'], 'features': []}

class DockerOrchestrator:
    """Manages Docker container operations."""
    def __init__(self, container_name: str, image_name: str, host_output_dir: str):
        self.container_name = container_name
        self.image_name = image_name
        self.host_output_dir = Util.ensure_dir(Path(host_output_dir).resolve())
        self.host_bin_dir = Util.ensure_dir(self.host_output_dir / "workloads_bin")
        self.host_meta_dir = Util.ensure_dir(self.host_output_dir / "workloads_meta")
        self.container_output_dir = "/outputs"
        self.container_code_dir = "/flow"
    
    def check_docker(self) -> bool:
        """Check if Docker is available."""
        success, out, _ = Util.run_cmd(["docker", "--version"], show=False)
        if success:
            Util.log(LogLevel.INFO, f"Docker available: {out.strip()}")
            return True
        Util.log(LogLevel.ERROR, "Docker not found")
        return False

    
    def check_image(self) -> bool:
        """Check if Docker image exists."""
        success, out, _ = Util.run_cmd(["docker", "images", "-q", self.image_name], show=False)
        if out.strip():
            Util.log(LogLevel.INFO, f"Image found: {self.image_name}")
            return True
        Util.log(LogLevel.WARN, f"Image not found: {self.image_name}")
        return False

    def build_image(self) -> bool:
        """Build Docker image."""
        Util.log(LogLevel.INFO, "Building Docker image...")
        if not Util.file_exists("Dockerfile"):
            Util.log(LogLevel.ERROR, "Dockerfile not found")
            return False
        cmd = ["docker", "build", "-t", self.image_name, "."]
        success, _, _ = Util.run_cmd(cmd)
        if success:
            Util.log(LogLevel.INFO, "Image built successfully")
            return True
        Util.log(LogLevel.ERROR, "Failed to build image")
        return False

    def run_command(self, command: List[str], interactive: bool = False) -> Tuple[bool, str, str]:
        """Run command in Docker container."""
        mounts = [
            f"-v {self.host_output_dir}:{self.container_output_dir}",
            f"-v {Path.cwd()}:{self.container_code_dir}",
            f"-v {Path.cwd() / 'environment'}:/workloads/environment",
        ]
        if (workloads_dir := Path.cwd().parent / "workloads").exists():
            mounts.append(f"-v {workloads_dir}:/workloads")
        mounts.extend([
            f"-v {self.host_bin_dir}:/workloads/bin",
            f"-v {self.host_meta_dir}:/workloads/meta",
        ])
        for board in ['spike', 'qemu']:
            binary_list = self.host_meta_dir / f"binary_list_{board}.txt"
            binary_list.touch(exist_ok=True)
            mounts.append(f"-v {binary_list}:/workloads/binary_list_{board}.txt")
        
        docker_cmd = ["docker", "run", "--rm"] + mounts + (["-it"] if interactive else []) + \
                     [self.image_name, "bash", "-c", f"cd {self.container_code_dir} && {' '.join(command)}"]
        return Util.run_cmd(docker_cmd, interactive=interactive)

class WorkflowManager:
    """Manages RISC-V analysis workflow."""
    def __init__(self, orchestrator: DockerOrchestrator):
        self.orchestrator = orchestrator
        self.config = {
            'workload_suite': None, 'benchmarks': [], 'emulator': None,
            'architecture': None, 'platform': 'baremetal',
            'enable_bbv': False, 'enable_trace': False, 'enable_simpoint': False
        }

    def get_input(self, prompt: str, choices: List[str] = None, default: str = None, multi: bool = False) -> str:
        """Get validated user input."""
        while True:
            if choices:
                Util.log(LogLevel.INFO, f"\n{prompt}")
                for i, c in enumerate(choices, 1):
                    Util.log(LogLevel.INFO, f"  {i}. {c}{' (default)' if c == default else ''}")
                if multi:
                    Util.log(LogLevel.INFO, "  Enter comma-separated numbers")
                try:
                    resp = input(f"Select [1-{len(choices)}]: ").strip()
                    if not resp and default:
                        return default
                    if multi and ',' in resp:
                        indices = [int(x.strip()) - 1 for x in resp.split(',')]
                        return ','.join([choices[i] for i in indices if 0 <= i < len(choices)])
                    idx = int(resp) - 1
                    if 0 <= idx < len(choices):
                        return choices[idx]
                except (ValueError, IndexError):
                    Util.log(LogLevel.ERROR, "Invalid selection")
            else:
                resp = input(f"{prompt}: ").strip()
                return resp or default

    def configure_interactive(self):
        """Configure workflow interactively."""
        Util.log(LogLevel.HEADER, "RISC-V Workload Analysis Configuration")
        
        # Workload selection
        workloads = discover_workloads()
        self.config['workload_suite'] = self.get_input(
            "Select workload suite", list(workloads.keys()), "embench-iot")
        self.config['architecture'] = "rv32" if self.config['workload_suite'] == "embench-iot" else "rv64"
        Util.log(LogLevel.INFO, f"Selected: {self.config['workload_suite']} ({workloads[self.config['workload_suite']]})")

        # Benchmark selection
        benchmarks = get_benchmarks(self.config['workload_suite'])
        if benchmarks:
            Util.log(LogLevel.INFO, f"Found {len(benchmarks)} benchmarks: {', '.join(benchmarks[:10])}{'...' if len(benchmarks) > 10 else ''}")
            if self.get_input("Use all benchmarks? [Y/n]", ["y", "n"], "y") == "y":
                self.config['benchmarks'] = ['all']
            else:
                selected = self.get_input("Enter benchmarks (comma-separated)", benchmarks, multi=True)
                self.config['benchmarks'] = [b.strip() for b in selected.split(',') if b.strip() in benchmarks] or ['all']
        
        # Emulator selection
        boards = ['spike', 'qemu']
        self.config['emulator'] = self.get_input("Select emulator", boards, "spike")
        board_config = get_board_config(self.config['emulator'])
        Util.log(LogLevel.INFO, f"Emulator: {self.config['emulator']} (Features: {', '.join(board_config['features'])})")

        # Architecture and platform
        archs = board_config['supported_archs']
        plats = board_config['supported_platforms']
        self.config['architecture'] = self.get_input(f"Select architecture (current: {self.config['architecture']})", archs, self.config['architecture'])
        self.config['platform'] = self.get_input("Select platform", plats, "baremetal") if len(plats) > 1 else plats[0]

        # Analysis features
        features = board_config['features']
        if 'bbv' in features:
            self.config['enable_bbv'] = self.get_input("Enable BBV? [y/n]", ["y", "n"], "y") == "y"
        if self.config['enable_bbv'] and 'trace' in features:
            self.config['enable_trace'] = self.get_input("Enable tracing? [y/n]", ["y", "n"], "n") == "y"
        if self.config['enable_bbv']:
            self.config['enable_simpoint'] = self.get_input("Enable SimPoint? [y/n]", ["y", "n"], "y") == "y"

        # Confirm
        Util.log(LogLevel.HEADER, "Configuration Summary")
        for k, v in self.config.items():
            Util.log(LogLevel.INFO, f"  {k.replace('_', ' ').title():20}: {v}")
        return self.get_input("Proceed? [y/n]", ["y", "n"], "y") == "y"

    def _generate_cmd(self, script: str, workload_specific: bool = False) -> List[str]:
        """Generate command for build/run/simpoint."""
        cmd = ["python3", script]
        if script != "run_simpoint.py":
            cmd.extend(["--arch", self.config['architecture'], "--platform", self.config['platform']])
            if self.config['enable_bbv']:
                cmd.append("--bbv")
            if self.config['enable_trace']:
                cmd.append("--trace")
        if script == "build_workload.py":
            cmd.extend(["--workload", self.config['workload_suite'], "--board", self.config['emulator']])
            if self.config['benchmarks'] != ['all']:
                cmd.extend(["--benchmark", self.config['benchmarks'][0]])
        elif script == "run_workload.py":
            cmd.extend(["--emulator", self.config['emulator']])
            if workload_specific and self.config['benchmarks'] != ['all']:
                cmd.extend(["--workload", self.config['benchmarks'][0]])
        elif script == "run_simpoint.py":
            cmd.extend(["--emulator", self.config['emulator'], "--workload-type", self.config['workload_suite'], "--verbose"])
        return cmd

    def run_step(self, step: str, script: str, workload_specific: bool = False) -> bool:
        """Run a workflow step."""
        Util.log(LogLevel.INFO, f"Executing {step}")
        cmd = self._generate_cmd(script, workload_specific)
        success, stdout, stderr = self.orchestrator.run_command(cmd)
        if success:
            Util.log(LogLevel.INFO, f"{step} completed")
            if stdout:
                Util.log(LogLevel.DEBUG, stdout[-1000:])
        else:
            Util.log(LogLevel.WARN, f"{step} failed")
            if stderr:
                Util.log(LogLevel.DEBUG, stderr[-1000:])
        return success

    def collect_results(self):
        """Collect and summarize results."""
        Util.log(LogLevel.HEADER, "Collecting Results")
        output_files = [
            p for p in self.orchestrator.host_output_dir.rglob("*")
            if p.is_file() and any(s in str(p) for s in ["results.txt", "bbv/", "traces/", "simpoint_analysis/"])
        ]
        if output_files:
            Util.log(LogLevel.INFO, "Results found:")
            for f in output_files:
                Util.log(LogLevel.INFO, f"  • {f.relative_to(self.orchestrator.host_output_dir)}")
        else:
            Util.log(LogLevel.WARN, "No results found")

        summary = {
            "timestamp": time.strftime("%Y-%m-%d %H:%M:%S"),
            "config": self.config,
            "output_dir": str(self.orchestrator.host_output_dir),
            "output_files": [str(f) for f in output_files]
        }
        summary_file = self.orchestrator.host_output_dir / "analysis_summary.json"
        with summary_file.open('w') as f:
            json.dump(summary, f, indent=2)
        Util.log(LogLevel.INFO, f"Summary saved: {summary_file}")

def main():
    """Main entry point for RISC-V analysis."""
    parser = argparse.ArgumentParser(description="RISC-V Workload Analysis Orchestrator")
    parser.add_argument("--container-name", default="riscv-analysis")
    parser.add_argument("--image-name", default="riscv-perf-model:latest")
    parser.add_argument("--output-dir", default="./outputs")
    parser.add_argument("--workload", help="Workload suite")
    parser.add_argument("--benchmark", help="Specific benchmark")
    parser.add_argument("--emulator", choices=["spike", "qemu"])
    parser.add_argument("--arch", choices=["rv32", "rv64"])
    parser.add_argument("--platform", choices=["baremetal", "linux"], default="baremetal")
    parser.add_argument("--bbv", action="store_true")
    parser.add_argument("--trace", action="store_true")
    parser.add_argument("--simpoint", action="store_true")
    parser.add_argument("--build-image", action="store_true")
    parser.add_argument("--skip-build", action="store_true")
    parser.add_argument("--skip-run", action="store_true")
    args = parser.parse_args()

    Util.log(LogLevel.HEADER, f"RISC-V Analysis (Output: {Path(args.output_dir).resolve()})")
    orchestrator = DockerOrchestrator(args.container_name, args.image_name, args.output_dir)
    
    if not orchestrator.check_docker():
        sys.exit(1)
    if args.build_image or not orchestrator.check_image():
        if not orchestrator.build_image():
            sys.exit(1)

    workflow = WorkflowManager(orchestrator)
    if args.workload:
        workloads = discover_workloads()
        if args.workload not in workloads:
            Util.log(LogLevel.ERROR, f"Unknown workload: {args.workload}. Available: {', '.join(workloads.keys())}")
        workflow.config.update({
            'workload_suite': args.workload,
            'benchmarks': [args.benchmark] if args.benchmark else ['all'],
            'emulator': args.emulator or 'spike',
            'architecture': args.arch or ('rv32' if args.workload == 'embench-iot' else 'rv64'),
            # need a better way to set/fetch default platform/arch based on workload/emulator (yaml)
            'platform': args.platform,
            'enable_bbv': args.bbv,
            'enable_trace': args.trace,
            'enable_simpoint': args.simpoint
        })
    else:
        if not workflow.configure_interactive():
            Util.log(LogLevel.WARN, "Cancelled by user")
            sys.exit(0)

    success = True
    if not args.skip_build:
        success = workflow.run_step("Build", "build_workload.py")
    if success and not args.skip_run:
        success = workflow.run_step("Execution", "run_workload.py", workload_specific=True)
    if success and workflow.config['enable_simpoint']:
        workflow.run_step("SimPoint Analysis", "run_simpoint.py")
    workflow.collect_results()

    Util.log(LogLevel.HEADER, "Analysis " + ("Completed" if success else "Failed"))
    Util.log(LogLevel.INFO, f"Results in: {orchestrator.host_output_dir}")

if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        Util.log(LogLevel.WARN, "Interrupted by user")
        sys.exit(1)
    except Exception as e:
        Util.log(LogLevel.ERROR, f"Unexpected error: {e}")
        sys.exit(1)
#!/usr/bin/env python3
"""
Full Flow Orchestrator for RISC-V Workload Analysis - Refactored Version

This script provides an interactive interface to orchestrate the complete 
RISC-V workload analysis workflow. It runs on the host machine and executes
individual analysis steps inside the Docker container.

Features:
- Config-driven workload discovery (no hardcoding)
- Automated Docker container management with persistent outputs
- Volume mounting for persistent output to /outputs
- Step-by-step workflow with progress tracking
- Support for build, run, BBV, tracing, and SimPoint analysis
- Detailed command logging with flags visibility
- Modular and extensible design

Author: Generated for RISC-V Performance Modeling
"""

import argparse
import os
import sys
import subprocess
import time
import json
from pathlib import Path
from typing import List, Dict, Optional, Tuple
import configparser

# Import our shared utilities (conditional import for host compatibility)
try:
    from util import file_exists
    from config import BoardConfig
except ImportError:
    # Fallback implementations for when running on host
    def file_exists(path):
        return Path(path).exists()
    
    class BoardConfig:
        def __init__(self, board):
            self.board = board
        
        def get_build_config(self, arch, platform, workload=None):
            return {'cc': f'riscv{arch[2:]}-unknown-elf-gcc'}

# ANSI color codes for better output
class Colors:
    HEADER = '\033[95m'
    OKBLUE = '\033[94m'
    OKCYAN = '\033[96m'
    OKGREEN = '\033[92m'
    WARNING = '\033[93m'
    FAIL = '\033[91m'
    ENDC = '\033[0m'
    BOLD = '\033[1m'
    UNDERLINE = '\033[4m'

def print_header(text: str):
    """Print a formatted header"""
    print(f"\n{Colors.HEADER}{Colors.BOLD}{'='*70}")
    print(f"  {text}")
    print(f"{'='*70}{Colors.ENDC}")

def print_step(text: str):
    """Print a formatted step"""
    print(f"\n{Colors.OKBLUE}{Colors.BOLD}[STEP] {text}{Colors.ENDC}")

def print_success(text: str):
    """Print a success message"""
    print(f"{Colors.OKGREEN}✓ {text}{Colors.ENDC}")

def print_warning(text: str):
    """Print a warning message"""
    print(f"{Colors.WARNING}⚠ {text}{Colors.ENDC}")

def print_error(text: str):
    """Print an error message"""
    print(f"{Colors.FAIL}✗ {text}{Colors.ENDC}")

def print_info(text: str):
    """Print an info message"""
    print(f"{Colors.OKCYAN}ℹ {text}{Colors.ENDC}")

def print_command(text: str):
    """Print a command being executed"""
    print(f"{Colors.BOLD}🔧 Command: {text}{Colors.ENDC}")

class ConfigDiscovery:
    """Discovers available workloads and configurations from config files"""
    
    def __init__(self):
        self.available_boards = ['spike', 'qemu']
        # Use relative paths that work on both host and container
        self.default_workloads = {
            "embench-iot": "embench-iot",
            "riscv-tests": "riscv-tests"
        }
        self.workloads_base_dir = Path.cwd().parent / "workloads" if (Path.cwd().parent / "workloads").exists() else Path("/workloads")
    
    def get_available_workloads(self) -> Dict[str, str]:
        """Get available workloads from default list and config discovery"""
        workloads = {}
        
        # Convert to absolute paths based on current environment
        for name, relative_path in self.default_workloads.items():
            full_path = self.workloads_base_dir / relative_path
            workloads[name] = str(full_path)
        
        # Add dhrystone as special case
        workloads["dhrystone"] = workloads.get("riscv-tests", str(self.workloads_base_dir / "riscv-tests"))
        
        return workloads
    
    def get_workload_benchmarks(self, workload: str, board: str = 'spike') -> List[str]:
        """Get available benchmarks for a workload by querying build system"""
        try:
            workload_paths = self.get_available_workloads()
            
            if workload == "embench-iot" and workload in workload_paths:
                workload_path = Path(workload_paths[workload])
                if workload_path.exists():
                    src_dir = workload_path / "src"
                    if src_dir.exists():
                        return [d.name for d in src_dir.iterdir() if d.is_dir()]
            elif workload in ["riscv-tests", "dhrystone"] and "riscv-tests" in workload_paths:
                workload_path = Path(workload_paths["riscv-tests"])
                if workload_path.exists():
                    bench_dir = workload_path / "benchmarks"
                    if bench_dir.exists():
                        return [d.name for d in bench_dir.iterdir() 
                               if d.is_dir() and d.name != "common"]
        except Exception as e:
            print_warning(f"Could not discover benchmarks for {workload}: {e}")
        
        return []
    
    def get_board_configs(self, board: str) -> Dict:
        """Get board configuration details"""
        try:
            config = BoardConfig(board)
            # Get a sample configuration to understand capabilities
            sample_config = config.get_build_config('rv32', 'baremetal', 'embench-iot')
            return {
                'cc': sample_config.get('cc', 'unknown'),
                'supported_archs': ['rv32', 'rv64'],
                'supported_platforms': ['baremetal', 'linux'],
                'features': ['bbv', 'trace'] if board == 'spike' else ['bbv', 'trace']
            }
        except Exception as e:
            print_warning(f"Could not load config for {board}: {e}")
            return {'cc': 'unknown', 'supported_archs': ['rv32', 'rv64'], 
                   'supported_platforms': ['baremetal'], 'features': []}

class DockerOrchestrator:
    """Docker container orchestration for RISC-V analysis workflow"""
    
    def __init__(self, container_name: str = "riscv-analysis", 
                 image_name: str = "perf:latest",
                 host_output_dir: str = "./outputs"):
        self.container_name = container_name
        self.image_name = image_name
        self.host_output_dir = Path(host_output_dir).resolve()
        
        # Ensure output directory exists
        self.host_output_dir.mkdir(parents=True, exist_ok=True)
        
        # Create persistent directories for build artifacts
        self.host_workloads_bin_dir = self.host_output_dir / "workloads_bin"
        self.host_workloads_meta_dir = self.host_output_dir / "workloads_meta"
        self.host_workloads_bin_dir.mkdir(parents=True, exist_ok=True)
        self.host_workloads_meta_dir.mkdir(parents=True, exist_ok=True)
        
        # Container paths
        self.container_workloads_dir = "/workloads"
        self.container_output_dir = "/outputs"  # Changed to match host
        self.container_code_dir = "/flow"
    
    def check_docker(self) -> bool:
        """Check if Docker is available and running"""
        try:
            result = subprocess.run(["docker", "--version"], capture_output=True, text=True)
            if result.returncode != 0:
                print_error("Docker is not installed or not in PATH")
                return False
            print_success(f"Docker available: {result.stdout.strip()}")
            return True
        except FileNotFoundError:
            print_error("Docker command not found")
            return False
    
    def check_image(self) -> bool:
        """Check if the Docker image exists"""
        try:
            result = subprocess.run(
                ["docker", "images", "-q", self.image_name], 
                capture_output=True, text=True
            )
            if result.stdout.strip():
                print_success(f"Docker image found: {self.image_name}")
                return True
            else:
                print_warning(f"Docker image not found: {self.image_name}")
                return False
        except Exception as e:
            print_error(f"Failed to check Docker image: {e}")
            return False
    
    def build_image(self) -> bool:
        """Build the Docker image"""
        print_step("Building Docker image...")
        dockerfile_path = Path("./Dockerfile")
        
        if not dockerfile_path.exists():
            print_error("Dockerfile not found in current directory")
            return False
        
        try:
            cmd = ["docker", "build", "-t", self.image_name, "."]
            print_command(f"docker build -t {self.image_name} .")
            result = subprocess.run(cmd, text=True)
            
            if result.returncode == 0:
                print_success("Docker image built successfully")
                return True
            else:
                print_error("Failed to build Docker image")
                return False
        except Exception as e:
            print_error(f"Failed to build Docker image: {e}")
            return False
    
    def run_command(self, command: List[str], interactive: bool = False, 
                   show_command: bool = True) -> Tuple[bool, str, str]:
        """Run a command inside the Docker container"""
        
        # Prepare Docker run command
        docker_cmd = ["docker", "run", "--rm"]
        
        # Mount output directory for persistent results
        docker_cmd.extend(["-v", f"{self.host_output_dir}:{self.container_output_dir}"])
        
        # Mount current directory (flow scripts) to /flow in container
        current_dir = Path.cwd()
        docker_cmd.extend(["-v", f"{current_dir}:{self.container_code_dir}"])
        
        # Mount environment directory
        env_dir = current_dir / "environment"
        docker_cmd.extend(["-v", f"{env_dir}:/workloads/environment"])
        
        # Mount workloads directory if it exists
        workloads_dir = current_dir.parent / "workloads"
        if workloads_dir.exists():
            docker_cmd.extend(["-v", f"{workloads_dir}:/workloads"])
        
        # CRITICAL: Mount build artifacts directories for persistence between containers
        # This ensures build step artifacts are available to run step
        docker_cmd.extend(["-v", f"{self.host_workloads_bin_dir}:/workloads/bin"])
        docker_cmd.extend(["-v", f"{self.host_workloads_meta_dir}:/workloads/meta"])
        
        # Mount individual binary list files (they're created at /workloads/ root)
        for board in ['spike', 'qemu']:
            binary_list_file = self.host_workloads_meta_dir / f"binary_list_{board}.txt"
            binary_list_file.touch()  # Create empty file if it doesn't exist
            docker_cmd.extend(["-v", f"{binary_list_file}:/workloads/binary_list_{board}.txt"])
        
        # Add interactive flags if needed
        if interactive:
            docker_cmd.extend(["-it"])
        
        # Add image and command
        full_command = f"cd {self.container_code_dir} && {' '.join(command)}"
        docker_cmd.extend([self.image_name, "bash", "-c", full_command])
        
        if show_command:
            workloads_mount = f" -v {workloads_dir}:/workloads" if workloads_dir.exists() else ""
            build_mounts = f" -v {self.host_workloads_bin_dir}:/workloads/bin -v {self.host_workloads_meta_dir}:/workloads/meta"
            print_command(f"docker run --rm -v {self.host_output_dir}:{self.container_output_dir} -v {current_dir}:{self.container_code_dir} -v {env_dir}:/workloads/environment{workloads_mount}{build_mounts} [+binary_list_mounts] {self.image_name} bash -c \"{full_command}\"")
        
        try:
            if interactive:
                result = subprocess.run(docker_cmd)
                return result.returncode == 0, "", ""
            else:
                result = subprocess.run(docker_cmd, capture_output=True, text=True, timeout=1800)
                return result.returncode == 0, result.stdout, result.stderr
        except subprocess.TimeoutExpired:
            print_error("Command timed out (30 minutes)")
            return False, "", "Timeout"
        except Exception as e:
            print_error(f"Failed to run command: {e}")
            return False, "", str(e)

class WorkflowManager:
    """Manages the interactive workflow for RISC-V analysis"""
    
    def __init__(self, orchestrator: DockerOrchestrator):
        self.orchestrator = orchestrator
        self.discovery = ConfigDiscovery()
        self.config = {
            'workload_suite': None,
            'benchmarks': [],
            'emulator': None,
            'architecture': None,
            'platform': 'baremetal',
            'enable_bbv': False,
            'enable_trace': False,
            'enable_simpoint': False,
            'custom_options': {}
        }
    
    def get_user_input(self, prompt: str, choices: List[str] = None, 
                      default: str = None, allow_multiple: bool = False) -> str:
        """Get user input with validation"""
        while True:
            if choices:
                print(f"\n{prompt}")
                for i, choice in enumerate(choices, 1):
                    marker = " (default)" if choice == default else ""
                    print(f"  {i}. {choice}{marker}")
                
                if allow_multiple:
                    print("  Enter comma-separated numbers for multiple selections")
                
                try:
                    response = input(f"\nSelect [1-{len(choices)}]: ").strip()
                    if not response and default:
                        return default
                    
                    if allow_multiple and ',' in response:
                        indices = [int(x.strip()) - 1 for x in response.split(',')]
                        selected = [choices[i] for i in indices if 0 <= i < len(choices)]
                        return ','.join(selected)
                    else:
                        index = int(response) - 1
                        if 0 <= index < len(choices):
                            return choices[index]
                        else:
                            print_error("Invalid selection")
                            continue
                except (ValueError, IndexError):
                    print_error("Please enter a valid number")
                    continue
            else:
                response = input(f"{prompt}: ").strip()
                if response or default is None:
                    return response
                return default
    
    def get_yes_no(self, prompt: str, default: bool = False) -> bool:
        """Get yes/no input from user"""
        default_str = "Y/n" if default else "y/N"
        response = input(f"{prompt} [{default_str}]: ").strip().lower()
        
        if not response:
            return default
        return response in ['y', 'yes']
    
    def select_workload_suite(self):
        """Interactive workload suite selection using config discovery"""
        print_step("Workload Suite Selection")
        
        available_workloads = self.discovery.get_available_workloads()
        workload_names = list(available_workloads.keys())
        
        # Add descriptions based on discovery
        descriptions = []
        for workload in workload_names:
            if workload == "embench-iot":
                benchmarks = self.discovery.get_workload_benchmarks(workload)
                desc = f"Embench IoT benchmark suite ({len(benchmarks)} benchmarks, embedded workloads)"
            elif workload == "riscv-tests":
                benchmarks = self.discovery.get_workload_benchmarks(workload)
                desc = f"RISC-V official test suite ({len(benchmarks)} benchmarks, comprehensive tests)"
            elif workload == "dhrystone":
                desc = "Classic Dhrystone benchmark (single benchmark)"
            else:
                desc = "Custom workload"
            descriptions.append(desc)
        
        print("Available workload suites:")
        for i, (workload, desc) in enumerate(zip(workload_names, descriptions), 1):
            print(f"  {i}. {Colors.BOLD}{workload}{Colors.ENDC}: {desc}")
        
        choice = self.get_user_input("Select workload suite", workload_names, "embench-iot")
        self.config['workload_suite'] = choice
        
        # Set default architecture based on suite
        if choice == "embench-iot":
            self.config['architecture'] = "rv32"
        elif choice in ["riscv-tests", "dhrystone"]:
            self.config['architecture'] = "rv64"
        
        print_success(f"Selected: {choice}")
        print_info(f"Workload path: {available_workloads[choice]}")
    
    def select_specific_benchmarks(self):
        """Select specific benchmarks within a suite using config discovery"""
        print_step("Benchmark Selection")
        
        workload = self.config['workload_suite']
        available_benchmarks = self.discovery.get_workload_benchmarks(workload)
        
        if not available_benchmarks:
            print_warning("Could not discover benchmarks, using all by default")
            self.config['benchmarks'] = ['all']
            return
        
        print(f"\nAvailable benchmarks for {workload}:")
        print(f"  Found {len(available_benchmarks)} benchmarks")
        
        # Show sample benchmarks
        sample_count = min(10, len(available_benchmarks))
        for i, benchmark in enumerate(available_benchmarks[:sample_count]):
            print(f"    • {benchmark}")
        if len(available_benchmarks) > sample_count:
            print(f"    ... and {len(available_benchmarks) - sample_count} more")
        
        use_all = self.get_yes_no("Use all available benchmarks?", default=True)
        if use_all:
            self.config['benchmarks'] = ['all']
            print_success(f"Will build all {len(available_benchmarks)} benchmarks")
        else:
            print("\nEnter specific benchmark names (comma-separated):")
            benchmark_input = input("Benchmarks: ").strip()
            if benchmark_input:
                selected = [b.strip() for b in benchmark_input.split(',')]
                valid_benchmarks = [b for b in selected if b in available_benchmarks]
                if valid_benchmarks:
                    self.config['benchmarks'] = valid_benchmarks
                    print_success(f"Selected: {', '.join(valid_benchmarks)}")
                else:
                    print_warning("No valid benchmarks selected, using all")
                    self.config['benchmarks'] = ['all']
            else:
                self.config['benchmarks'] = ['all']
    
    def select_emulator(self):
        """Interactive emulator selection with config-driven features"""
        print_step("Emulator Selection")
        
        available_boards = self.discovery.available_boards
        
        print("Available emulators:")
        for board in available_boards:
            config_info = self.discovery.get_board_configs(board)
            features = ', '.join(config_info.get('features', []))
            cc = config_info.get('cc', 'unknown')
            
            if board == 'spike':
                desc = f"Spike: Functional ISA simulator (accurate, detailed analysis)"
            else:
                desc = f"QEMU: Binary translation emulator (fast, high-throughput)"
            
            print(f"  • {Colors.BOLD}{board}{Colors.ENDC}: {desc}")
            print(f"    Compiler: {cc}, Features: {features}")
        
        choice = self.get_user_input("Select emulator", available_boards, "spike")
        self.config['emulator'] = choice
        
        print_success(f"Selected: {choice}")
        
        # Show configuration details
        config_info = self.discovery.get_board_configs(choice)
        print_info(f"Board configuration loaded from environment/{choice}/board.cfg")
    
    def configure_architecture_and_platform(self):
        """Configure target architecture and platform"""
        print_step("Architecture & Platform Configuration")
        
        current_arch = self.config.get('architecture', 'rv32')
        board_config = self.discovery.get_board_configs(self.config['emulator'])
        supported_archs = board_config.get('supported_archs', ['rv32', 'rv64'])
        supported_platforms = board_config.get('supported_platforms', ['baremetal'])
        
        print(f"Current architecture: {Colors.BOLD}{current_arch}{Colors.ENDC}")
        print(f"Supported architectures: {', '.join(supported_archs)}")
        
        # Architecture selection
        if self.get_yes_no(f"Keep architecture as {current_arch}?", default=True):
            pass
        else:
            choice = self.get_user_input("Select architecture", supported_archs, current_arch)
            self.config['architecture'] = choice
        
        # Platform selection
        print(f"\nSupported platforms: {', '.join(supported_platforms)}")
        if len(supported_platforms) > 1:
            platform_choice = self.get_user_input("Select platform", supported_platforms, "baremetal")
            self.config['platform'] = platform_choice
        else:
            self.config['platform'] = supported_platforms[0]
        
        print_success(f"Architecture: {self.config['architecture']}, Platform: {self.config['platform']}")
    
    def configure_analysis_features(self):
        """Configure BBV, tracing, and SimPoint options"""
        print_step("Analysis Features Configuration")
        
        board_features = self.discovery.get_board_configs(self.config['emulator']).get('features', [])
        
        print("Available analysis features:")
        if 'bbv' in board_features:
            print(f"  • {Colors.BOLD}BBV{Colors.ENDC}: Basic Block Vector generation for SimPoint analysis")
        if 'trace' in board_features:
            print(f"  • {Colors.BOLD}Tracing{Colors.ENDC}: Instruction-level execution traces")
        print(f"  • {Colors.BOLD}SimPoint{Colors.ENDC}: Representative simulation point analysis")
        
        if 'bbv' in board_features:
            self.config['enable_bbv'] = self.get_yes_no("Enable BBV generation?", default=True)
        
        if self.config['enable_bbv'] and 'trace' in board_features:
            self.config['enable_trace'] = self.get_yes_no("Enable instruction tracing?", default=False)
        
        if self.config['enable_bbv']:
            self.config['enable_simpoint'] = self.get_yes_no("Enable SimPoint analysis?", default=True)
        
        # Display selections
        print("\nSelected features:")
        features = []
        if self.config['enable_bbv']:
            features.append("BBV")
        if self.config['enable_trace']:
            features.append("Tracing")
        if self.config['enable_simpoint']:
            features.append("SimPoint")
        
        if features:
            print_success(f"Enabled: {', '.join(features)}")
        else:
            print_warning("No analysis features enabled")
    
    def display_configuration(self):
        """Display the complete configuration with command preview"""
        print_step("Configuration Summary")
        
        config_items = [
            ("Workload Suite", self.config['workload_suite']),
            ("Benchmarks", ', '.join(self.config['benchmarks']) if self.config['benchmarks'] else 'all'),
            ("Emulator", self.config['emulator']),
            ("Architecture", self.config['architecture']),
            ("Platform", self.config['platform']),
            ("BBV Generation", "Yes" if self.config['enable_bbv'] else "No"),
            ("Instruction Tracing", "Yes" if self.config['enable_trace'] else "No"),
            ("SimPoint Analysis", "Yes" if self.config['enable_simpoint'] else "No"),
        ]
        
        print("\nConfiguration:")
        for key, value in config_items:
            print(f"  {Colors.BOLD}{key:20}{Colors.ENDC}: {value}")
        
        # Show command preview
        print(f"\n{Colors.BOLD}Command Preview:{Colors.ENDC}")
        build_cmd = self._generate_build_command()
        run_cmd = self._generate_run_command()
        
        print(f"  Build: {' '.join(build_cmd)}")
        print(f"  Run:   {' '.join(run_cmd)}")
        
        return self.get_yes_no("\nProceed with this configuration?", default=True)
    
    def _generate_build_command(self) -> List[str]:
        """Generate build command based on configuration"""
        cmd = ["python3", "build_workload.py"]
        cmd.extend(["--workload", self.config['workload_suite']])
        cmd.extend(["--arch", self.config['architecture']])
        cmd.extend(["--platform", self.config['platform']])
        cmd.extend(["--board", self.config['emulator']])
        
        if self.config['benchmarks'] and self.config['benchmarks'][0] != 'all':
            cmd.extend(["--benchmark", self.config['benchmarks'][0]])
        
        if self.config['enable_bbv']:
            cmd.append("--bbv")
        if self.config['enable_trace']:
            cmd.append("--trace")
        
        return cmd
    
    def _generate_run_command(self) -> List[str]:
        """Generate run command based on configuration"""
        cmd = ["python3", "run_workload.py"]
        cmd.extend(["--emulator", self.config['emulator']])
        cmd.extend(["--arch", self.config['architecture']])
        cmd.extend(["--platform", self.config['platform']])
        
        if self.config['enable_bbv']:
            cmd.append("--bbv")
        if self.config['enable_trace']:
            cmd.append("--trace")
        
        if self.config['benchmarks'] and self.config['benchmarks'][0] != 'all':
            cmd.extend(["--workload", self.config['benchmarks'][0]])
        
        return cmd
    
    def run_build_step(self):
        """Execute the build step"""
        print_step("Building Workloads")
        
        cmd = self._generate_build_command()
        print_info(f"Build command: {' '.join(cmd)}")
        
        success, stdout, stderr = self.orchestrator.run_command(cmd, show_command=True)
        
        if success:
            print_success("Build completed successfully")
            if stdout:
                print("\n" + stdout[-1500:])  # Show last part of output
            return True
        else:
            print_error("Build failed")
            if stderr:
                print("\n" + stderr[-1000:])
            return False
    
    def run_execution_step(self):
        """Execute the workload running step"""
        print_step("Running Workloads")
        
        cmd = self._generate_run_command()
        print_info(f"Run command: {' '.join(cmd)}")
        
        success, stdout, stderr = self.orchestrator.run_command(cmd, show_command=True)
        
        if success:
            print_success("Execution completed successfully")
            if stdout:
                print("\n" + stdout[-1500:])
            return True
        else:
            print_error("Execution failed")
            if stderr:
                print("\n" + stderr[-1000:])
            return False
    
    def run_simpoint_step(self):
        """Execute SimPoint analysis"""
        if not self.config['enable_simpoint']:
            return True
        
        print_step("Running SimPoint Analysis")
        
        cmd = ["python3", "run_simpoint.py"]
        cmd.extend(["--emulator", self.config['emulator']])
        cmd.extend(["--workload-type", self.config['workload_suite']])
        cmd.append("--verbose")
        
        print_info(f"SimPoint command: {' '.join(cmd)}")
        
        success, stdout, stderr = self.orchestrator.run_command(cmd, show_command=True)
        
        if success:
            print_success("SimPoint analysis completed successfully")
            if stdout:
                print("\n" + stdout[-1000:])
            return True
        else:
            print_warning("SimPoint analysis failed or no suitable BBV files found")
            if stderr:
                print("\n" + stderr[-500:])
            return False
    
    def collect_results(self):
        """Collect and organize results"""
        print_step("Collecting Results")
        
        output_files = []
        
        # Check for various output files in the mounted output directory
        potential_patterns = [
            f"{self.config['emulator']}_output/results.txt",
            f"{self.config['emulator']}_output/logs/",
            f"{self.config['emulator']}_output/bbv/",
            f"{self.config['emulator']}_output/traces/",
            "simpoint_analysis/",
            "workloads_bin/",
            "workloads_meta/"
        ]
        
        for pattern in potential_patterns:
            file_path = self.orchestrator.host_output_dir / pattern
            if file_path.exists():
                output_files.append(file_path)
        
        if output_files:
            print_success("Results collected in host directory:")
            for file_path in output_files:
                rel_path = file_path.relative_to(self.orchestrator.host_output_dir)
                print(f"  • {self.orchestrator.host_output_dir}/{rel_path}")
        else:
            print_warning("No output files found")
        
        # Create comprehensive summary
        summary_file = self.orchestrator.host_output_dir / "analysis_summary.json"
        summary = {
            "timestamp": time.strftime("%Y-%m-%d %H:%M:%S"),
            "configuration": self.config,
            "commands": {
                "build": self._generate_build_command(),
                "run": self._generate_run_command()
            },
            "output_directory": str(self.orchestrator.host_output_dir),
            "output_files": [str(f) for f in output_files],
            "docker_image": self.orchestrator.image_name
        }
        
        with open(summary_file, 'w') as f:
            json.dump(summary, f, indent=2)
        
        print_success(f"Analysis summary saved: {summary_file}")
        return True

def main():
    """Main entry point"""
    parser = argparse.ArgumentParser(
        description="Config-driven RISC-V Workload Analysis Orchestrator",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
This script provides an interactive interface to run the complete RISC-V 
workload analysis workflow using Docker containers with config-driven discovery.

Features:
- Automatic workload discovery from config files  
- Persistent output directory mounting to /outputs
- Detailed command logging with visible flags
- Modular and extensible design

Examples:
  # Interactive mode (recommended)
  python3 full_flow_orchestrator.py

  # Custom output directory (persists after container exit)
  python3 full_flow_orchestrator.py --output-dir ./my_analysis_results
        """
    )
    
    # Docker configuration
    parser.add_argument("--container-name", default="riscv-analysis",
                       help="Docker container name")
    parser.add_argument("--image-name", default="perf:latest", 
                       help="Docker image name")
    parser.add_argument("--output-dir", default="./outputs",
                       help="Host output directory (mounted to /outputs in container)")
    
    # Non-interactive options (for future automation)
    parser.add_argument("--workload", help="Workload suite (non-interactive)")
    parser.add_argument("--benchmark", help="Specific benchmark name")
    parser.add_argument("--emulator", choices=["spike", "qemu"],
                       help="Emulator choice (non-interactive)")
    parser.add_argument("--arch", choices=["rv32", "rv64"],
                       help="Architecture choice")
    parser.add_argument("--platform", choices=["baremetal", "linux"],
                       help="Platform choice")
    parser.add_argument("--bbv", action="store_true",
                       help="Enable BBV generation")
    parser.add_argument("--trace", action="store_true", 
                       help="Enable tracing")
    parser.add_argument("--simpoint", action="store_true",
                       help="Enable SimPoint analysis")
    
    # Operational flags
    parser.add_argument("--build-image", action="store_true",
                       help="Build Docker image before running")
    parser.add_argument("--skip-build", action="store_true", 
                       help="Skip the build step")
    parser.add_argument("--skip-run", action="store_true",
                       help="Skip the execution step")
    
    args = parser.parse_args()
    
    # Welcome message
    print_header("RISC-V Workload Analysis Orchestrator - Refactored")
    print("This tool uses config-driven discovery to guide you through the complete")
    print("RISC-V workload analysis workflow with persistent output mounting.")
    print(f"\n{Colors.BOLD}Output Directory:{Colors.ENDC} {Path(args.output_dir).resolve()}")
    print("(This directory will be mounted to /outputs in the container)")
    
    # Initialize orchestrator
    orchestrator = DockerOrchestrator(
        container_name=args.container_name,
        image_name=args.image_name,
        host_output_dir=args.output_dir
    )
    
    # Check Docker availability
    if not orchestrator.check_docker():
        print_error("Docker is required but not available")
        sys.exit(1)
    
    # Check/build image
    if args.build_image or not orchestrator.check_image():
        if not orchestrator.build_image():
            print_error("Failed to build Docker image")
            sys.exit(1)
    
    # Initialize workflow manager
    workflow = WorkflowManager(orchestrator)
    
    # Configure workflow (currently interactive only, non-interactive mode ready for future)
    if args.workload:  # Non-interactive mode (basic implementation)
        discovery = ConfigDiscovery()
        available_workloads = discovery.get_available_workloads()
        
        if args.workload not in available_workloads:
            print_error(f"Unknown workload: {args.workload}")
            print_info(f"Available: {', '.join(available_workloads.keys())}")
            sys.exit(1)
        
        workflow.config.update({
            'workload_suite': args.workload,
            'benchmarks': [args.benchmark] if args.benchmark else ['all'],
            'emulator': args.emulator or 'spike',
            'architecture': args.arch or ('rv32' if args.workload == 'embench-iot' else 'rv64'),
            'platform': args.platform or 'baremetal',
            'enable_bbv': args.bbv,
            'enable_trace': args.trace,
            'enable_simpoint': args.simpoint
        })
        
        print_info("Using non-interactive configuration:")
        workflow.display_configuration()
    else:  # Interactive mode
        workflow.select_workload_suite()
        workflow.select_specific_benchmarks()
        workflow.select_emulator()
        workflow.configure_architecture_and_platform()
        workflow.configure_analysis_features()
        
        if not workflow.display_configuration():
            print_warning("Analysis cancelled by user")
            sys.exit(0)
    
    # Execute workflow steps
    success = True
    
    try:
        if not args.skip_build:
            success = workflow.run_build_step()
            
        if success and not args.skip_run:
            success = workflow.run_execution_step()
            
        if success:
            workflow.run_simpoint_step()  # Don't fail on SimPoint issues
            
        workflow.collect_results()
        
        if success:
            print_header("Analysis Completed Successfully!")
            print_success(f"Results persisted in: {orchestrator.host_output_dir}")
            print_info("Results will remain available after container exit")
        else:
            print_header("Analysis Completed with Errors")
            print_error("Some steps failed, but results may still be available")
        
    except KeyboardInterrupt:
        print_warning("\nAnalysis interrupted by user")
        sys.exit(1)
    except Exception as e:
        print_error(f"Unexpected error: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()

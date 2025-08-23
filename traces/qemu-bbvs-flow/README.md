# QEMU SimPoint Unified Analysis

A unified, containerized workflow for running various workloads on QEMU with SimPoint analysis for program phase detection and basic block vector generation.

## Table of Contents
- [Overview](#overview)
- [Quick Start](#quick-start)
- [Prerequisites](#prerequisites)
- [Installation](#installation)
- [Usage](#usage)
- [Workload Types](#workload-types)
- [Configuration](#configuration)
- [Results](#results)
- [VS Code Setup](#vs-code-setup)
- [Troubleshooting](#troubleshooting)
- [Contributing](#contributing)

## Overview

This project provides a unified Docker-based environment for:
- **Multiple Workloads**: Dhrystone, Embench, and custom workloads
- **BBV Generation**: Basic Block Vector creation using QEMU plugins
- **SimPoint Analysis**: Program phase detection and clustering
- **Flexible Configuration**: Configurable compiler flags and QEMU architectures

## Quick Start

### 1. Clone Repository
```bash
git clone https://github.com/yourusername/qemu-bbvs-flow.git
cd qemu-bbvs-flow
```

### 2. Run Dhrystone Analysis
```bash
./build_and_run.sh dhrystone 
# OR

./build_docker.sh 
./run_workload.sh dhrystone
```

### 3. Run Embench Analysis
```bash
#Embench gets compiled during the build itself
./build_docker.sh 

# Run specific Embench workload with BBV 
docker run --rm -v $(pwd)/simpoint_output:/output qemu-simpoint-unified /run_embench_simple.sh [workload_name] --bbv
#[For example] – 
docker run --rm -v $(pwd)/simpoint_output:/output qemu-simpoint-unified /run_embench_simple.sh crc32 --bbv

```

### 4. Run Custom Workload
```bash
./build_and_run.sh custom /path/to/your/source "-O2 -static" riscv64
```
### 5. Individual Component Execution
```bash
./build_docker.sh
./run_workload.sh [workload_type] [parameters]
```

## Prerequisites

### System Requirements
- **OS**: Windows 10/11 (with WSL 2), macOS, or Linux
- **RAM**: Minimum 8GB (16GB recommended)
- **Storage**: 20GB available disk space
- **Network**: Internet connection for Docker image building

### Software Dependencies
- Docker Desktop (Windows/Mac) or Docker Engine (Linux)
- Git
- WSL 2 (Windows only)

## Installation

### Windows Setup
1. **Install WSL 2**:
   ```powershell
   # Run in PowerShell as Administrator
   wsl --install
   # Restart computer
   ```

2. **Install Docker Desktop**:
   - Download from [Docker Desktop](https://www.docker.com/products/docker-desktop)
   - Enable WSL 2 integration in settings

3. **Open WSL 2 Terminal** and clone repository:
   ```bash
   git clone https://github.com/yourusername/qemu-bbvs-flow.git
   cd qemu-bbvs-flow
   chmod +x *.sh
   ```

### Linux/macOS Setup
1. **Install Docker**:
   ```bash
   # Ubuntu/Debian
   sudo apt update && sudo apt install docker.io
   sudo systemctl start docker
   sudo usermod -aG docker $USER
   # Log out and log back in
   
   # macOS (using Homebrew)
   brew install --cask docker
   ```

2. **Clone and Setup**:
   ```bash
   git clone https://github.com/yourusername/qemu-bbvs-flow.git
   cd qemu-bbvs-flow
   chmod +x *.sh
   ```

## Usage

### Basic Commands

#### Complete Workflow (Recommended)
```bash
# Run with default settings
./build_and_run.sh [workload_type] [workload_source] [compiler_flags] [qemu_arch] [interval] [max_k]
```

#### Step-by-Step Execution
```bash
# 1. Build Docker image
./build_docker.sh

# 2. Run specific workload
./run_workload.sh [workload_type] [workload_source] [compiler_flags] [qemu_arch] [interval] [max_k]
```

### Examples

#### Dhrystone with Default Settings
```bash
./build_and_run.sh dhrystone
```

#### Embench with Custom Settings
```bash
#Embench gets compiled during the build itself
./build_docker.sh
```

#### Custom Workload
```bash
./build_and_run.sh custom /path/to/source "-O3 -static" riscv64 200 25
```

## Workload Types

### 1. Dhrystone
- **Description**: Integer arithmetic benchmark
- **Source**: Automatically cloned from SiFive repository
- **Default Architecture**: riscv64
- **Iterations**: Modified to 100,000 for better analysis

### 2. Embench
- **Description**: IoT benchmark suite
- **Source**: Automatically cloned from Embench repository
- **Default Architecture**: riscv32
- **Components**: Multiple small benchmarks

### 3. Custom
- **Description**: User-provided workload
- **Requirements**: Source directory with Makefile or build.sh
- **Flexibility**: Full control over compilation and execution

## Technical Details

### Architecture

```
┌─────────────────┐    ┌──────────────────┐    ┌─────────────────┐
│   Benchmark     │    │      QEMU        │    │    SimPoint     │
│   (Dhrystone/   │───▶│   + BBV Plugin   │───▶│   Analysis      │
│    Embench)     │    │                  │    │                 │
└─────────────────┘    └──────────────────┘    └─────────────────┘
        │                        │                        │
        ▼                        ▼                        ▼
   RISC-V Binary           BBV Files (.bb)         SimPoints + Weights
```


## Configuration

### Parameters

| Parameter | Description | Default | Examples |
|-----------|-------------|---------|----------|
| `workload_type` | Type of workload | dhrystone | dhrystone, embench, custom |
| `workload_source` | Source directory path | "" | /path/to/source |
| `compiler_flags` | Compilation flags | "-static" | "-O2 -static", "-march=rv32i" |
| `qemu_arch` | QEMU architecture | riscv64 | riscv32, riscv64 |
| `interval` | BBV generation interval | 100 | 50, 100, 200 |
| `max_k` | Maximum clusters | 30 | 10, 20, 30 |

### Configuration File
See `workload_config.json` for predefined configurations:

```json
{
  "workloads": {
    "dhrystone": {
      "compiler_flags": "-static",
      "qemu_arch": "riscv64",
      "interval": 100,
      "max_k": 30
    }
  }
}
```

## Results

### Generated Files
Results are saved in `simpoint_output/` directory:

- **BBV Files** (`*_bbv.0.bb`): Basic block vectors in binary format
- **SimPoint Files** (`*.simpoints`): Representative simulation points
- **Weight Files** (`*.weights`): Cluster weights

### File Formats

#### SimPoint Files
```
# Format: SimPoint_ID Cluster_ID
0 0
1 1
2 0
```

#### Weight Files
```
# Format: Cluster_ID Weight
0 0.45
1 0.32
2 0.23
```

## VS Code Setup

### 1. Install Extensions
- **Remote - WSL** (Windows only)
- **Docker**
- **Remote - Containers**

### 2. Open Project
1. Open VS Code
2. For Windows: Press `Ctrl+Shift+P`, select "Remote-WSL: Open Folder in WSL"
3. Navigate to `qemu-bbvs-flow` directory
4. Open folder

### 3. Container Development
1. Press `Ctrl+Shift+P`
2. Select "Remote-Containers: Open Folder in Container"
3. VS Code will build and open the project in the container

### 4. Running Commands
Use the integrated terminal in VS Code:
```bash
# Build and run
./build_and_run.sh dhrystone

# View results
ls simpoint_output/
```

### 5. Debugging
1. Set breakpoints in shell scripts
2. Use "Debug Console" for container inspection
3. Access container logs through Docker extension

## Troubleshooting

### Common Issues

#### Docker Build Failures
```bash
# Check Docker is running
docker --version

# Check available space
df -h

# Clean Docker cache
docker system prune -a
```

#### Permission Errors
```bash
# Add user to docker group (Linux)
sudo usermod -aG docker $USER
# Log out and log back in

# Make scripts executable
chmod +x *.sh
```

#### Memory Issues
- Increase Docker memory limit in Docker Desktop settings
- Reduce analysis parameters:
  ```bash
  ./build_and_run.sh dhrystone "" "-static" riscv64 1000 10
  ```

#### Build Time Issues
- Expected build time: 30-90 minutes
- Ensure stable internet connection
- Use SSD storage for better performance
- Close unnecessary applications to free up resources

#### Custom Workload Issues
```bash
# Ensure your custom workload has proper build system
ls /path/to/source/  # Should contain Makefile or build.sh

# Check build script permissions
chmod +x /path/to/source/build.sh

# Test compilation manually
cd /path/to/source
make CC=riscv64-linux-gnu-gcc CFLAGS="-static"
```

### Verification Steps

#### Check Docker Installation
```bash
docker run hello-world
```

#### Check WSL 2 (Windows)
```bash
wsl --list --verbose
```

#### Test Build Process
```bash
# Test with minimal build
./build_docker.sh
docker images | grep qemu-simpoint-unified
```

### Performance Optimization

#### Reduce Build Time
- Use Docker layer caching
- Build during off-peak hours
- Ensure sufficient RAM allocation

#### Optimize Analysis
- Increase BBV interval for faster execution
- Reduce max clusters for quicker analysis
- Use parallel processing where available

## Advanced Usage

### Custom Compiler Flags
```bash
# Optimization levels
./build_and_run.sh dhrystone "" "-O3 -static" riscv64

# Architecture-specific flags
./build_and_run.sh embench "" "-march=rv32i -mabi=ilp32 -static" riscv32

# Debug flags
./build_and_run.sh custom /path/to/source "-g -static" riscv64
```

### Batch Processing
```bash
# Create batch script
cat > batch_analysis.sh << 'EOF'
#!/bin/bash
./build_docker.sh  # Build once

# Run multiple configurations
./run_workload.sh dhrystone "" "-static" riscv64 100 30
./run_workload.sh dhrystone "" "-O2 -static" riscv64 100 30
./run_workload.sh dhrystone "" "-O3 -static" riscv64 100 30
EOF

chmod +x batch_analysis.sh
./batch_analysis.sh
```

### Result Analysis
```bash
# View generated files
ls -la simpoint_output/

# Analyze SimPoint results
head simpoint_output/*.simpoints
head simpoint_output/*.weights

# Check BBV file sizes
ls -lh simpoint_output/*.bb
```

## Project Structure

```
qemu-bbvs-flow/
├── Dockerfile                   # Unified Docker image
├── setup_workload.sh           # Workload setup script
├── run_analysis.sh             # Analysis execution script
├── run_embench_simple.sh       # embench compilation script
├── build_docker.sh             # Docker image build script
├── run_workload.sh             # Workload execution script
├── build_and_run.sh            # Complete workflow script
├── workload_config.json        # Workload configuration
├── README.md                   # User documentation
├── documentation/
│   └── technical_details.md    # This file
└── simpoint_output/            # Generated results (after execution)
```

## Contributing

### Development Setup
1. Fork the repository
2. Create a feature branch
3. Test changes with multiple workloads
4. Submit pull request

### Adding New Workloads
1. Update `setup_workload.sh` with new workload case
2. Add configuration to `workload_config.json`
3. Test with various compiler flags
4. Update documentation

### Reporting Issues
Include in your issue report:
- Operating system and version
- Docker version
- Complete error messages
- Steps to reproduce
- Expected vs actual behavior


## Acknowledgments

- [QEMU Project](https://gitlab.com/qemu-project/qemu) for the emulation framework
- [SimPoint](https://github.com/hanhwi/SimPoint) for program phase analysis
- [SiFive](https://github.com/sifive/benchmark-dhrystone) for the Dhrystone benchmark
- [Embench](https://github.com/embench/embench-iot) for the IoT benchmark suite
- [Original inspiration](https://github.com/vinicius-r-silva/olympia-mentorship) for the workflow design
- Docker for containerization support

## Support

For support and questions:
1. Check this README and technical documentation
2. Search existing issues in the repository
3. Create a new issue with detailed information
4. Join our community discussions

## Changelog

### Version 1.0.0
- Initial unified Docker implementation
- Support for Dhrystone, Embench, and custom workloads
- Configurable compiler flags and QEMU architectures
- Automated build and run scripts
- Comprehensive documentation
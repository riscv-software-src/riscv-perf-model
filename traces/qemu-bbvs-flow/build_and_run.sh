#!/bin/bash
set -e

# Configuration
BUILD_SCRIPT="./build_docker.sh"
RUN_SCRIPT="./run_workload.sh"

# Parse command line arguments
WORKLOAD_TYPE=${1:-dhrystone}
WORKLOAD_SOURCE=${2:-""}
COMPILER_FLAGS=${3:-"-static"}
QEMU_ARCH=${4:-"riscv64"}
INTERVAL=${5:-100}
MAX_K=${6:-30}

echo "=========================================="
echo "QEMU SimPoint Unified Analysis"
echo "=========================================="

echo "[STEP 1] Building Docker image..."
bash "$BUILD_SCRIPT"

echo ""
echo "[STEP 2] Running workload analysis..."
bash "$RUN_SCRIPT" "$WORKLOAD_TYPE" "$WORKLOAD_SOURCE" "$COMPILER_FLAGS" "$QEMU_ARCH" "$INTERVAL" "$MAX_K"

echo ""
echo "=========================================="
echo "Complete workflow finished successfully!"
echo "Check the simpoint_output directory for results"
echo "=========================================="

#!/bin/bash
set -e

# Configuration
IMAGE_NAME="qemu-simpoint-unified"
WORKLOAD_TYPE=${1:-dhrystone}
WORKLOAD_SOURCE=${2:-""}
COMPILER_FLAGS=${3:-"-static"}
QEMU_ARCH=${4:-"riscv64"}
INTERVAL=${5:-100}
MAX_K=${6:-30}

# Create output directory
OUTPUT_DIR="$(pwd)/simpoint_output"
mkdir -p "$OUTPUT_DIR"

echo "=========================================="
echo "Running QEMU SimPoint Analysis"
echo "=========================================="
echo "Workload type: $WORKLOAD_TYPE"
echo "Workload source: $WORKLOAD_SOURCE"
echo "Compiler flags: $COMPILER_FLAGS"
echo "QEMU architecture: $QEMU_ARCH"
echo "BBV interval: $INTERVAL"
echo "Max K clusters: $MAX_K"
echo "Output directory: $OUTPUT_DIR"
echo "=========================================="

# Prepare volume mounts
VOLUME_MOUNTS="-v $OUTPUT_DIR:/output"

# Add workload source volume if provided and not using pre-built workloads
if [ -n "$WORKLOAD_SOURCE" ] && [ -d "$WORKLOAD_SOURCE" ] && [ "$WORKLOAD_TYPE" != "embench" ]; then
    VOLUME_MOUNTS="$VOLUME_MOUNTS -v $WORKLOAD_SOURCE:/workload_source"
fi

# Special handling for Embench
if [ "$WORKLOAD_TYPE" = "embench" ]; then
    echo "Using pre-built Embench workloads from container"
    # Set appropriate architecture for Embench
    QEMU_ARCH="riscv32"
    if [ -z "$COMPILER_FLAGS" ] || [ "$COMPILER_FLAGS" = "-static" ]; then
        COMPILER_FLAGS="-c -O2 -ffunction-sections -march=rv32imfdc -mabi=ilp32d"
    fi
fi

# Run the workflow inside the container
docker run --rm \
  $VOLUME_MOUNTS \
  "$IMAGE_NAME" \
  bash -c "
    echo 'Setting up workload...'
    /setup_workload.sh '$WORKLOAD_TYPE' '$WORKLOAD_SOURCE' '$COMPILER_FLAGS' '$QEMU_ARCH'
    
    echo 'Starting analysis...'
    /run_analysis.sh '$WORKLOAD_TYPE' '$QEMU_ARCH' '$INTERVAL' '$MAX_K'
    
    echo 'Workflow completed successfully!'
  "

echo "=========================================="
echo "Experiment completed successfully!"
echo "Results are available in: $OUTPUT_DIR"
echo "=========================================="

# Display summary of generated files
if [ -d "$OUTPUT_DIR" ] && [ "$(ls -A $OUTPUT_DIR)" ]; then
    echo ""
    echo "Generated files:"
    ls -la "$OUTPUT_DIR"
    
    # Count different file types
    bbv_files=$(find "$OUTPUT_DIR" -name "*.bb" 2>/dev/null | wc -l)
    simpoints_files=$(find "$OUTPUT_DIR" -name "*.simpoints" 2>/dev/null | wc -l)
    weights_files=$(find "$OUTPUT_DIR" -name "*.weights" 2>/dev/null | wc -l)
    
    echo ""
    echo "File Summary:"
    echo "- BBV files: $bbv_files"
    echo "- SimPoints files: $simpoints_files"
    echo "- Weights files: $weights_files"
fi
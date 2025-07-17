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

# Add workload source volume if provided
if [ -n "$WORKLOAD_SOURCE" ] && [ -d "$WORKLOAD_SOURCE" ]; then
    VOLUME_MOUNTS="$VOLUME_MOUNTS -v $WORKLOAD_SOURCE:/workload_source"
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

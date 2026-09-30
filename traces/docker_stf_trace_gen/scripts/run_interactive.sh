#!/bin/bash
# Interactive Docker container with all necessary mounts
# This script provides the same mounting setup as full_flow_orchestrator.py

# Get current directory
CURRENT_DIR=$(pwd)
OUTPUT_DIR="${CURRENT_DIR}/outputs"
WORKLOADS_DIR="${CURRENT_DIR}/../workloads"

# Create outputs directory if it doesn't exist
mkdir -p "${OUTPUT_DIR}"

# Print mounting information
echo "🐳 Starting interactive Docker container with mounts:"
echo "  📁 Flow scripts:  ${CURRENT_DIR} -> /flow"
echo "  📁 Environment:   ${CURRENT_DIR}/environment -> /default/environment"
echo "  📁 Outputs:       ${OUTPUT_DIR} -> /outputs"

# Check if workloads directory exists
if [ -d "${WORKLOADS_DIR}" ]; then
    echo "  📁 Workloads:     ${WORKLOADS_DIR} -> /workloads"
    WORKLOADS_MOUNT="-v ${WORKLOADS_DIR}:/workloads"
else
    echo "  ⚠️  Workloads directory not found: ${WORKLOADS_DIR}"
    WORKLOADS_MOUNT=""
fi
echo ""

# Run Docker with all mounts
docker run --rm -it \
    -u $(id -u):$(id -g) \
    -v "${OUTPUT_DIR}:/outputs" \
    -v "${CURRENT_DIR}:/flow" \
    -v "${CURRENT_DIR}/environment:/default/environment" \
    ${WORKLOADS_MOUNT} \
    -w /flow \
    riscv-perf-model:olympia \
    bash

echo "🏁 Container exited. Outputs preserved in: ${OUTPUT_DIR}"

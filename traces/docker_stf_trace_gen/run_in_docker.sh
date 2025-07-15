#!/bin/bash

set -e

IMAGE_NAME="spike-stf"
OUTPUT_DIR="$(pwd)/trace_output"

# The binary to run on spike (bare-metal)
BINARY_NAME="dhrystone/bin/dhrystone_opt1.1000.gcc.bare.riscv" 
TRACEOUT_NAME="trace_output.zstf"

mkdir -p "$OUTPUT_DIR"

docker run --rm \
  -v "$OUTPUT_DIR":/riscv/condor.riscv-isa-sim/trace_out \
  "$IMAGE_NAME" \
  bash -c "
     bash scripts/run-spike-stf.sh $BINARY_NAME $TRACEOUT_NAME
  "

#!/bin/bash

set -e

IMAGE_NAME="spike-stf"
OUTPUT_DIR="$(pwd)/trace_output"

mkdir -p "$OUTPUT_DIR"

docker run --rm \
  -v "$OUTPUT_DIR":/output \
  "$IMAGE_NAME" \
  bash -c "
    echo '<REPLACE THIS ECHO COMMAND WITH SPIKE STF TRACE GENERATION COMMAND>' >> /output/EXAMPLE.zstf
  "
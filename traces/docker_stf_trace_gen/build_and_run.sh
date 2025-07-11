#!/bin/bash

set -e

# === Configuration ===
BUILD_SCRIPT="./build_docker.sh"
RUN_SCRIPT="./run_in_docker.sh"

echo "[STEP 1] Building Docker image..."
bash "$BUILD_SCRIPT"

echo "[STEP 2] Running BBV generation and SimPoint analysis..."
bash "$RUN_SCRIPT"
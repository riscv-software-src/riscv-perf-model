#!/bin/bash

set -e

# === Configuration ===
BUILD_SCRIPT="./build_docker.sh"
RUN_SCRIPT="./full_flow.py"

echo "[STEP 1] Building Docker image..."
bash "$BUILD_SCRIPT"

echo "[STEP 2] Running the Full Flow in Docker..."
bash "$RUN_SCRIPT"
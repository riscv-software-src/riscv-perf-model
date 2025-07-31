#!/bin/bash
set -e

# Configuration
IMAGE_NAME="qemu-simpoint-unified"

echo "=========================================="
echo "Building Docker image: $IMAGE_NAME"
echo "=========================================="

# Build the Docker image
docker build -t "$IMAGE_NAME" .

echo "=========================================="
echo "Docker image built successfully!"
echo "Image name: $IMAGE_NAME"
echo "=========================================="

#!/bin/bash

set -e

IMAGE_NAME="riscv-perf-model"
docker build -t "$IMAGE_NAME" .

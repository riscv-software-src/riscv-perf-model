#!/bin/bash

set -e

IMAGE_NAME="spike-stf"
docker build -t "$IMAGE_NAME" .

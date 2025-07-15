# Spike-STF trace generation

> This workflow is still under development. For the most up-to-date instructions, refer to the [Generating Input for Olympia](https://github.com/riscv-software-src/riscv-perf-model/blob/master/traces/README.md) README

## Table of Contents

1. [Introduction](#introduction)
1. [Dependencies](#dependencies)
1. [Project Structure](#project-structure)
1. [Usage](#usage)
   1. [Build the Docker Image](#build-the-docker-image)
   1. [Run STF Trace Generation](#run-stf-trace-generation)

## Introduction

This setup provides a containerized workflow to generate STF traces using Spike-STF. It sets up a complete environment with the RISC-V GNU toolchain, pk (proxy kernel), and Spike-STF.

## Dependencies

- **Docker** – See the [official installation guide](https://docs.docker.com/engine/install).  
  This workflow was tested with Docker version 28.3.0, build 38b7060, but it should also work with the latest available version.

## Project Structure

```bash
qemu-simpoint-dhrystone/
├── Dockerfile               # Creates a Docker image with the RISC-V GNU toolchain, pk, stf_tools and Spike-STF
├── run_in_docker.sh         # Runs an STF trace generation example inside the container
├── build_docker.sh          # Builds the Docker image
├── build_and_run.sh         # Executes both build_docker and run_in_docker scripts
```

## Usage

This workflow can be split into two sequential tasks:

- Build the Docker image
- Generate STF traces using the Docker container

Alternatively, you may run the [`build_and_run.sh`](./build_and_run.sh) script to see an example of the trace generation process.

### Build the Docker Image

```bash
docker build -t spike-stf .
```

### Run STF Trace Generation

```bash
mkdir -p trace_output

docker run --rm \
  -v "$OUTPUT_DIR":/riscv/condor.riscv-isa-sim/trace_out \
  "$IMAGE_NAME" \
  bash -c "
     bash scripts/run-spike-stf.sh $BINARY_NAME $TRACEOUT_NAME
  "
```

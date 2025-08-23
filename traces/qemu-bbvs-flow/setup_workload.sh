#!/bin/bash

# Setup workload based on workload type
WORKLOAD_TYPE=$1
WORKLOAD_SOURCE=$2
COMPILER_FLAGS=$3
QEMU_ARCH=$4

echo "Setting up workload: $WORKLOAD_TYPE"
echo "Source: $WORKLOAD_SOURCE"
echo "Compiler flags: $COMPILER_FLAGS"
echo "QEMU architecture: $QEMU_ARCH"

# Create workload directory
mkdir -p /workloads

# Generic workload handling (minimum 3 characters)
if [ -z "$WORKLOAD_TYPE" ] || [ ${#WORKLOAD_TYPE} -lt 3 ]; then
    echo "Error: Workload type must be specified and at least 3 characters long"
    exit 1
fi

case $WORKLOAD_TYPE in
    "dhrystone")
        echo "Setting up Dhrystone workload..."
        git clone https://github.com/sifive/benchmark-dhrystone.git /workloads/dhrystone
        cd /workloads/dhrystone

        # Modify dhrystone to run more iterations for better analysis
        sed -i 's/^#define DHRY_ITERS 2000$/#define DHRY_ITERS 100000/' dhry_1.c

        # Build with provided compiler flags
        make CC=riscv64-linux-gnu-gcc CFLAGS="$COMPILER_FLAGS"

        # Create binary list for dhrystone only
        echo "/workloads/dhrystone/dhrystone" > /workloads/binary_list.txt
        ;;

    "embench")
        echo "Setting up Embench workload..."
        # Use pre-built embench from /workspace/embench-iot
        if [ -d "/workspace/embench-iot" ]; then
            echo "Using pre-built Embench workloads..."
            cd /workspace/embench-iot
            
            # Create binary list for embench workloads only
            find bd/src -name "*.elf" -type f 2>/dev/null > /tmp/embench_binaries.txt || \
            find bd/src -type f -executable -not -name "*.sh" > /tmp/embench_binaries.txt
            
            # Convert to absolute paths and filter for embench only
            sed 's|^|/workspace/embench-iot/|' /tmp/embench_binaries.txt > /workloads/binary_list.txt
        else
            echo "Pre-built Embench not found, building fresh..."
            git clone --recursive https://github.com/embench/embench-iot.git /workloads/embench
            cd /workloads/embench

            # Fix the wikisort bool typedef issue
            sed -i '36s/^typedef uint8_t bool;/\/\/ &/' /workloads/embench/src/wikisort/libwikisort.c

            # Build Embench with RISC-V toolchain
            python3 ./build_all.py \
                --arch riscv32 \
                --chip generic \
                --board ri5cyverilator \
                --cc riscv64-unknown-elf-gcc \
                --cflags="$COMPILER_FLAGS -march=rv32imfdc -mabi=ilp32d" \
                --ldflags="$COMPILER_FLAGS -march=rv32imfdc -mabi=ilp32d" \
                --user-libs="-lm"

            # Create binary list for embench workloads only
            find bd/src -type f -executable -not -name "*.sh" | sed 's|^|/workloads/embench/|' > /workloads/binary_list.txt
        fi
        ;;

    *)
        # Generic workload handling (replacing hardcoded "custom")
        echo "Setting up generic workload: $WORKLOAD_TYPE"
        
        if [ -z "$WORKLOAD_SOURCE" ] || [ ! -d "$WORKLOAD_SOURCE" ]; then
            echo "Error: Workload source directory not provided or doesn't exist: $WORKLOAD_SOURCE"
            exit 1
        fi
        
        # Copy source files from mounted volume
        WORKLOAD_DIR="/workloads/$WORKLOAD_TYPE"
        mkdir -p "$WORKLOAD_DIR"
        cp -r "$WORKLOAD_SOURCE"/* "$WORKLOAD_DIR/"
        cd "$WORKLOAD_DIR"

        # Build with provided compiler flags
        if [ -f Makefile ]; then
            make CC=riscv64-linux-gnu-gcc CFLAGS="$COMPILER_FLAGS"
        elif [ -f build.sh ]; then
            chmod +x build.sh
            ./build.sh "$COMPILER_FLAGS"
        else
            echo "No Makefile or build.sh found in workload: $WORKLOAD_TYPE"
            exit 1
        fi

        # Find all executables and create binary list for this workload only
        find . -type f -executable -not -name "*.sh" | sed "s|^|$WORKLOAD_DIR/|" > /workloads/binary_list.txt
        ;;
esac

echo "Workload setup completed!"
echo "Binary list for $WORKLOAD_TYPE:"
cat /workloads/binary_list.txt
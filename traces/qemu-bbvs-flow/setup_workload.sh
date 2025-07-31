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

case $WORKLOAD_TYPE in
    "dhrystone")
        echo "Setting up Dhrystone workload..."
        git clone https://github.com/sifive/benchmark-dhrystone.git /workloads/dhrystone
        cd /workloads/dhrystone
        
        # Modify dhrystone to run more iterations for better analysis
        sed -i 's/^#define DHRY_ITERS 2000$/#define DHRY_ITERS 100000/' dhry_1.c
        
        # Build with provided compiler flags
        make CC=riscv64-linux-gnu-gcc CFLAGS="$COMPILER_FLAGS"
        
        # Create binary list
        echo "/workloads/dhrystone/dhrystone" > /workloads/binary_list.txt
        ;;
        
    "embench")
        echo "Setting up Embench workload..."
        git clone https://github.com/embench/embench-iot.git /workloads/embench
        cd /workloads/embench
        
        # Setup Python virtual environment
        python3 -m venv venv
        source venv/bin/activate
        
        # Install requirements if they exist
        if [ -f requirements.txt ]; then
            pip install -r requirements.txt
        fi
        
        # Build Embench with provided compiler flags
        python3 build_all.py --arch riscv32 --chip generic --board ri5cyverilator --cc riscv64-linux-gnu-gcc --cflags="$COMPILER_FLAGS" --ldflags="$COMPILER_FLAGS"
        
        # Create binary list
        find bd -name "*.elf" -type f | sed 's|^|/workloads/embench/|' > /workloads/binary_list.txt
        ;;
        
    "custom")
        echo "Setting up custom workload..."
        # Copy source files from mounted volume
        cp -r /workload_source/* /workloads/custom/
        cd /workloads/custom
        
        # Build with provided compiler flags
        if [ -f Makefile ]; then
            make CC=riscv64-linux-gnu-gcc CFLAGS="$COMPILER_FLAGS"
        elif [ -f build.sh ]; then
            chmod +x build.sh
            ./build.sh "$COMPILER_FLAGS"
        else
            echo "No Makefile or build.sh found in custom workload"
            exit 1
        fi
        
        # Find all executables and create binary list
        find . -type f -executable -not -name "*.sh" | sed 's|^|/workloads/custom/|' > /workloads/binary_list.txt
        ;;
        
    *)
        echo "Unknown workload type: $WORKLOAD_TYPE"
        echo "Supported types: dhrystone, embench, custom"
        exit 1
        ;;
esac

echo "Workload setup completed!"
echo "Binary list:"
cat /workloads/binary_list.txt

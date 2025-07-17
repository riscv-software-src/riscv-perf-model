#!/bin/bash

# Run BBV generation and SimPoint analysis
WORKLOAD_TYPE=$1
QEMU_ARCH=$2
INTERVAL=${3:-100}
MAX_K=${4:-30}

echo "Running analysis for workload: $WORKLOAD_TYPE"
echo "QEMU architecture: $QEMU_ARCH"
echo "BBV interval: $INTERVAL"
echo "Max K clusters: $MAX_K"

# Check if binary list exists
if [ ! -f /workloads/binary_list.txt ]; then
    echo "Error: Binary list not found. Please run setup_workload.sh first."
    exit 1
fi

# Process each binary
while IFS= read -r binary; do
    if [ ! -f "$binary" ]; then
        echo "Warning: Binary not found: $binary"
        continue
    fi
    
    benchmark_name=$(basename "$binary" | sed 's/\.[^.]*$//')
    echo "Processing benchmark: $benchmark_name"
    
    # Run with BBV generation
    echo "Generating BBV for $benchmark_name..."
    qemu-$QEMU_ARCH -plugin $QEMU_PLUGINS/libbbv.so,interval=$INTERVAL,outfile=/output/${benchmark_name}_bbv "$binary"
    
    # Check if BBV file was generated
    if [ -f "/output/${benchmark_name}_bbv.0.bb" ]; then
        echo "Running SimPoint analysis for $benchmark_name..."
        simpoint \
            -loadFVFile /output/${benchmark_name}_bbv.0.bb \
            -maxK $MAX_K \
            -saveSimpoints /output/${benchmark_name}.simpoints \
            -saveSimpointWeights /output/${benchmark_name}.weights
        
        echo "Completed: $benchmark_name"
    else
        echo "Warning: BBV file not generated for $benchmark_name"
    fi
    
done < /workloads/binary_list.txt

echo "Analysis completed!"
echo "Results saved to /output directory:"
ls -la /output/

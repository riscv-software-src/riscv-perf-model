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

# Filter binaries based on workload type if specified
if [ -n "$WORKLOAD_TYPE" ]; then
    echo "Filtering binaries for workload type: $WORKLOAD_TYPE"
    # Create filtered binary list for specific workload type
    case $WORKLOAD_TYPE in
        "dhrystone")
            grep "dhrystone" /workloads/binary_list.txt > /tmp/filtered_binaries.txt || {
                echo "No dhrystone binaries found in binary list"
                exit 1
            }
            ;;
        "embench")
            grep -E "(embench|bd/src)" /workloads/binary_list.txt > /tmp/filtered_binaries.txt || {
                echo "No embench binaries found in binary list"
                exit 1
            }
            ;;
        *)
            # For generic workloads, filter by workload directory path
            grep "$WORKLOAD_TYPE" /workloads/binary_list.txt > /tmp/filtered_binaries.txt || {
                echo "No binaries found for workload type: $WORKLOAD_TYPE"
                exit 1
            }
            ;;
    esac
    BINARY_LIST="/tmp/filtered_binaries.txt"
else
    echo "Processing all available binaries"
    BINARY_LIST="/workloads/binary_list.txt"
fi

echo "Processing $(wc -l < $BINARY_LIST) binary(ies)"

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

done < "$BINARY_LIST"

echo "Analysis completed for workload type: ${WORKLOAD_TYPE:-all}"
echo "Results saved to /output directory:"
ls -la /output/
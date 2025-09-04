#!/bin/bash
# run_embench_simple.sh - Simple script to run Embench workloads
set -e

EMBENCH_DIR="/workspace/embench-iot"

usage() {
    echo "Usage: $0 [workload_name] [options]"
    echo "Options:"
    echo "  -l, --list          List all available workloads"
    echo "  -a, --all           Run all workloads"
    echo "  -t, --time          Run with timing information"
    echo "  -b, --bbv           Generate BBV traces"
    echo "  -h, --help          Show this help message"
    echo ""
    echo "Examples:"
    echo "  $0 crc32            # Run crc32 workload"
    echo "  $0 crc32 --time     # Run crc32 with timing"
    echo "  $0 crc32 --bbv      # Run crc32 with BBV generation"
    echo "  $0 --all            # Run all workloads"
    echo "  $0 --list           # List available workloads"
}

list_workloads() {
    echo "Available Embench workloads:"
    echo "================================"
    local count=0
    for workload_dir in "${EMBENCH_DIR}/bd/src"/*; do
        if [ -d "$workload_dir" ]; then
            workload=$(basename "$workload_dir")
            executable="${workload_dir}/${workload}"
            if [ -f "$executable" ]; then
                count=$((count + 1))
                printf "%2d. %s\n" $count "$workload"
            fi
        fi
    done
    echo "================================"
    echo "Total: $count workloads"
}

run_workload() {
    local workload=$1
    local with_timing=$2
    local with_bbv=$3
    local workload_path="${EMBENCH_DIR}/bd/src/${workload}"
    local executable="${workload_path}/${workload}"
    
    if [ ! -f "$executable" ]; then
        echo "Error: Workload '$workload' not found at $executable"
        echo "Available workloads:"
        list_workloads
        return 1
    fi
    
    echo "Running workload: $workload"
    echo "Executable: $executable"
    echo "Architecture: RISC-V 32-bit"
    echo ""
    
    if [ "$with_bbv" = "true" ]; then
        echo "Running with BBV generation..."
        echo "----------------------------------------"
        if [ "$with_timing" = "true" ]; then
            time qemu-riscv32 -plugin $QEMU_PLUGINS/libbbv.so,interval=100,outfile=/output/${workload}_bbv "$executable"
        else
            qemu-riscv32 -plugin $QEMU_PLUGINS/libbbv.so,interval=100,outfile=/output/${workload}_bbv "$executable"
        fi
        echo "BBV trace saved to /output/${workload}_bbv.0.bb"
        echo "----------------------------------------"
    elif [ "$with_timing" = "true" ]; then
        echo "Running with timing information..."
        echo "----------------------------------------"
        time qemu-riscv32 "$executable"
        echo "----------------------------------------"
    else
        echo "Execution output:"
        echo "----------------------------------------"
        qemu-riscv32 "$executable"
        echo "----------------------------------------"
    fi
    
    echo "Successfully ran $workload"
    echo ""
}

run_all_workloads() {
    local with_timing=$1
    local with_bbv=$2
    
    echo "Running all Embench workloads..."
    echo ""
    
    local success_count=0
    local total_count=0
    local failed_workloads=()
    
    for workload_dir in "${EMBENCH_DIR}/bd/src"/*; do
        if [ -d "$workload_dir" ]; then
            workload=$(basename "$workload_dir")
            executable="${workload_dir}/${workload}"
            if [ -f "$executable" ]; then
                total_count=$((total_count + 1))
                echo "[$total_count] Running $workload..."
                
                if run_workload "$workload" "$with_timing" "$with_bbv"; then
                    success_count=$((success_count + 1))
                else
                    failed_workloads+=("$workload")
                fi
            fi
        fi
    done
    
    echo "Execution Summary:"
    echo "================================"
    echo "Successful: $success_count/$total_count workloads"
    echo "Failed: $((total_count - success_count))/$total_count workloads"
    
    if [ ${#failed_workloads[@]} -gt 0 ]; then
        echo "Failed workloads: ${failed_workloads[*]}"
    fi
    echo "================================"
}

# Parse arguments
WITH_TIMING=false
WITH_BBV=false
ACTION=""
WORKLOAD=""

while [[ $# -gt 0 ]]; do
    case $1 in
        -l|--list)
            ACTION="list"
            shift
            ;;
        -a|--all)
            ACTION="all"
            shift
            ;;
        -t|--time)
            WITH_TIMING=true
            shift
            ;;
        -b|--bbv)
            WITH_BBV=true
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        -*)
            echo "Unknown option: $1"
            usage
            exit 1
            ;;
        *)
            WORKLOAD="$1"
            shift
            ;;
    esac
done

# Execute based on action
case "$ACTION" in
    list)
        list_workloads
        ;;
    all)
        run_all_workloads "$WITH_TIMING" "$WITH_BBV"
        ;;
    *)
        if [ -n "$WORKLOAD" ]; then
            run_workload "$WORKLOAD" "$WITH_TIMING" "$WITH_BBV"
        else
            echo "Please specify a workload or use --list to see available options"
            echo ""
            usage
        fi
        ;;
esac
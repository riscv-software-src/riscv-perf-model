# BBV & Trace Integration Guide

This guide explains how to use Basic Block Vector (BBV) generation and instruction tracing in the RISC-V Workload Build & Run System. These features enable detailed performance analysis and SimPoint-based simulation acceleration.

## Overview

### BBV (Basic Block Vectors)
- **Purpose**: Generate execution profiles for SimPoint analysis
- **Output**: `.bbv` files containing basic block execution counts
- **Use Case**: Identify representative simulation points in long-running workloads
- **Supported**: Spike (CSR-based), QEMU (plugin-based)

### Instruction Tracing  
- **Purpose**: Record detailed execution traces for analysis
- **Output**: STF traces (Spike), assembly traces (QEMU)
- **Use Case**: Detailed performance analysis, debugging, verification
- **Supported**: Spike (STF format), QEMU (text format)

## Quick Start

### Enable BBV and Tracing

```bash
# Build with BBV and trace support
python3 flow/build_workload.py --workload embench-iot --benchmark md5sum --emulator spike --bbv --trace

# Run with BBV and tracing enabled
python3 flow/run_workload.py --emulator spike --bbv --trace --workload md5sum
```

### Check Output

```bash
# BBV files
ls /outputs/spike_output/bbv/md5sum.bbv

# Trace files  
ls /outputs/spike_output/traces/md5sum.zstf

# Use stf_tools based stf_dump to convert STF trace files to human readable dump
```

## Region of Interest (ROI) Markers

### Automatic Marker Placement

The smart runtime environment **automatically** places markers around your benchmark execution:

```c
// In environment/main.c - env_main() function
int env_main(int argc, char *argv[]) {
    initialise_board();
    initialise_benchmark();
    warm_caches(WARMUP_HEAT);
    
    // Automatic marker placement
#ifdef BBV
    START_BBV;    // 🔥 Automatically placed before benchmark
#endif
#ifdef TRACE
    START_TRACE;  // 🔥 Automatically placed before benchmark
#endif 

    result = benchmark();  // Your workload runs here

#ifdef TRACE
    STOP_TRACE;   // 🔥 Automatically placed after benchmark
#endif
#ifdef BBV
    STOP_BBV;     // 🔥 Automatically placed after benchmark  
#endif

    correct = verify_benchmark(result);
    return (!correct);
}
```

**Key Benefit**: No source modification required! The system automatically instruments your workload.

### Manual Marker Placement (Advanced)

For fine-grained control, manually place markers in your workload source:

```c
#include "support.h"  // Provides marker definitions

int benchmark(void) {
    // Setup phase (not measured)
    setup_data_structures();
    allocate_memory();
    
    // Start measuring region of interest
    START_BBV; 
    START_TRACE;
    
    // Core computation (measured region)
    for (int i = 0; i < iterations; i++) {
        process_iteration(i);
    }
    
    // Stop measuring
    STOP_TRACE;
    STOP_BBV;
    
    // Cleanup phase (not measured)
    deallocate_memory();
    validate_results();
    
    return result;
}
```

## Marker Definitions

### Cross-Platform Markers

The markers are designed for cross-platform compatibility:

```c
// BBV markers (Spike-specific)
#define START_BBV  __asm__ volatile ("csrsi 0x8c2, 1");  // Set CSR bit
#define STOP_BBV   __asm__ volatile ("csrci 0x8c2, 1");  // Clear CSR bit

// Trace markers (cross-platform)  
#ifdef __riscv
#define START_TRACE  asm volatile ("xor x0, x0, x0");  // NOP variant 1
#define STOP_TRACE   asm volatile ("xor x0, x1, x1");  // NOP variant 2
#else
// Disabled for non-RISC-V builds (x86 testing)
#define START_TRACE
#define STOP_TRACE
#endif
```

### Opcode Recognition

For simulator recognition, the markers correspond to specific opcodes:

```c
// 32-bit instruction opcodes
#define START_TRACE_OPC 0x00004033  // xor x0, x0, x0
#define STOP_TRACE_OPC  0x0010c033  // xor x0, x1, x1
```

## BBV Generation

### Spike BBV (CSR-based)

Spike uses Control and Status Register (CSR) accesses to mark BBV regions:

**Build Requirements:**
```bash
# Must use --bbv flag during build to enable CSR markers
python3 flow/build_workload.py --workload embench-iot --benchmark aha-mont64 --emulator spike --bbv
```

**Runtime Behavior:**
```c
// Compiled into binary when --bbv is used
START_BBV;  // csrsi 0x8c2, 1 - Start BBV collection
// ... benchmark execution ...
STOP_BBV;   // csrci 0x8c2, 1 - Stop BBV collection
```

### QEMU BBV (Plugin-based)

QEMU uses a plugin architecture for BBV generation:

**Build Requirements:**
```bash
# --bbv flag adds -DBBV but QEMU doesn't need source markers
python3 flow/build_workload.py --workload embench-iot --benchmark aha-mont64 --emulator qemu --bbv
```

**Runtime Behavior:**
```bash
# QEMU with BBV plugin
qemu-riscv32 -plugin /qemu/build/contrib/plugins/libbbv.so,interval=10000000,outfile=workload_bbv workload

# Plugin parameters:
# - interval: BBV interval size (instructions between samples)  
# - outfile: Output file prefix
```

### BBV Intervals

BBV generation uses intervals to sample execution:

**Choosing Intervals:**
```bash
# Fine-grained analysis (more data)
python3 flow/run_workload.py --emulator qemu --bbv --interval-size 1000000    # 1M instructions

# Coarse-grained analysis (less data)  
python3 flow/run_workload.py --emulator spike --bbv --interval-size 1000000000 # 1B instructions
```

## Instruction Tracing

### Spike Tracing (STF Format)

Spike generates System Trace Format (STF) traces:

**Build Requirements:**
```bash
# Use --trace flag to enable trace markers
python3 flow/build_workload.py --workload embench-iot --benchmark md5sum --emulator spike --trace
```

**Runtime Command:**
```bash
# Spike with tracing enabled
spike --isa=rv32gc --stf_macro_tracing --stf_trace_memory_records --stf_trace=./trace.zstf pk <workload_bin>
# Creates: trace.zstf
```


### QEMU Tracing (Assembly Format)

QEMU generates human-readable assembly traces:

**Build Requirements:**
```bash
# --trace flag adds -DTRACE for potential source-level control
python3 flow/build_workload.py --workload embench-iot --benchmark md5sum --emulator qemu --trace  
```

**Runtime Command:**
```bash
# QEMU with instruction tracing
qemu-riscv32 -d in_asm,cpu -D trace.log workload
# Creates: trace.log with assembly instructions

# Options:
# -d in_asm: Log assembly instructions
# -d cpu: Log CPU state changes  
# -D file: Output to file instead of stderr
```

**QEMU Output Example:**
```
# QEMU trace format (text)
----------------
IN: 
0x00010000:  00008137  lui      x2,0x8
0x00010004:  87010113  addi     x2,x2,-1936
0x00010008:  00000517  auipc    x10,0x0
...
```

## Integration with Build System

### Build-Time Flags

The build system automatically adds required compiler flags:

```python
# In build_workload.py
def build_workload(..., bbv=False, trace=False):
    cflags = base_cflags.split()
    
    if bbv:
        cflags.append("-DBBV")     # Enables BBV markers in source
    if trace:
        cflags.append("-DTRACE")   # Enables TRACE markers in source
        
    # Environment also gets flags for automatic marker placement
    if platform == "baremetal":
        env_cflags = base_cflags
        if bbv and board == "spike":
            env_cflags += " -DBBV"    # Enable in environment/main.c
        if trace and board == "spike": 
            env_cflags += " -DTRACE"  # Enable in environment/main.c
```

### Conditional Compilation

Markers are only included when appropriate flags are set:

```c
// In environment/main.c
#ifdef BBV
    START_BBV;    // Only compiled if -DBBV was used
#endif
#ifdef TRACE  
    START_TRACE;  // Only compiled if -DTRACE was used
#endif

result = benchmark();

#ifdef TRACE
    STOP_TRACE;   // Only compiled if -DTRACE was used
#endif
#ifdef BBV
    STOP_BBV;     // Only compiled if -DBBV was used  
#endif
```

## Output Organization

### Directory Structure

BBV and trace files are organized by emulator:

```
/outputs/
├── spike_output/
│   ├── bbv/                    # Spike BBV files
│   │   ├── md5sum.bbv
│   │   ├── aha-mont64.bbv
│   │   └── slre.bbv
│   ├── traces/                 # Spike STF traces
│   │   ├── md5sum.zstf
│   │   ├── aha-mont64.zstf  
│   │   └── slre.zstf
│   └── logs/                   # Execution logs
│       ├── md5sum.log
│       └── ...
└── qemu_output/
    ├── bbv/                    # QEMU BBV files  
    │   ├── md5sum_bbv.0.bb
    │   ├── aha-mont64_bbv.0.bb
    │   └── slre_bbv.0.bb
    ├── traces/                 # QEMU assembly traces
    │   ├── md5sum_trace.log
    │   ├── aha-mont64_trace.log
    │   └── slre_trace.log
    └── logs/                   # Execution logs
        ├── md5sum.log
        └── ...
```

### File Naming Conventions

**Spike:**
- BBV: `{workload}.bbv_cpu0`
- Trace: `{workload}.zstf` (compressed) or `{workload}.stf`
- Log: `{workload}.log`

**QEMU:**
- BBV: `{workload}_bbv.0.bb` (plugin generates `.0.bb` suffix)
- Trace: `{workload}_trace.log`  
- Log: `{workload}.log`

## Advanced Usage

### Multi-Phase Workloads

For workloads with multiple phases, use multiple marker pairs:

```c
int benchmark(void) {
    // Phase 1: Initialization
    START_BBV; START_TRACE;
    initialize_phase();
    STOP_TRACE; STOP_BBV;
    
    // Phase 2: Core computation  
    START_BBV; START_TRACE;
    compute_phase();
    STOP_TRACE; STOP_BBV;
    
    // Phase 3: Finalization
    START_BBV; START_TRACE; 
    finalize_phase();
    STOP_TRACE; STOP_BBV;
    
    return result;
}
```

### Nested Measurements

Avoid nested markers (not supported):

```c
// ❌ DON'T DO THIS - Nested markers not supported
START_BBV;
    START_TRACE;  // Wrong - nested inside BBV
        compute();
    STOP_TRACE;
STOP_BBV;

// ✅ DO THIS - Parallel markers
START_BBV; START_TRACE;
    compute();
STOP_TRACE; STOP_BBV;
```

### Workload-Specific Markers

Override automatic markers for custom control:

```c
// Define your own markers to override automatic ones
#ifdef CUSTOM_ROI
#undef START_BBV
#undef STOP_BBV
#define START_BBV custom_start_measurement()
#define STOP_BBV  custom_stop_measurement()
#endif

int benchmark(void) {
    setup();
    
    START_BBV; START_TRACE;
    // Only measure the core loop
    for (int i = 0; i < ITERATIONS; i++) {
        core_computation(i);
    }
    STOP_TRACE; STOP_BBV;
    
    cleanup();
    return result;
}
```


## Integration with SimPoint Analysis

BBV files are designed for SimPoint analysis:

```bash
# Generate BBV files
python3 flow/run_workload.py --emulator spike --bbv --workload embench-iot

# Run SimPoint analysis (using generated BBV files)
python3 flow/run_simpoint.py --workload embench-iot --emulator spike --max-k 30

# Results: .simpoints and .weights files for representative intervals
```

See [SimPoint Analysis Guide](simpoint.md) for detailed SimPoint integration. (to do)

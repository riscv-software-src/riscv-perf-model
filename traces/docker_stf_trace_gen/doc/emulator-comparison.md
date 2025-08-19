# Emulator Comparison: Spike vs QEMU

This guide compares Spike and QEMU emulators within the RISC-V Workload Build & Run System, covering performance characteristics, feature differences, and use case recommendations.

## Overview

The system supports two primary RISC-V emulators QEMU and SPIKE (With STF Generation and BBV Generation)

## Performance Characteristics

### Execution Speed Analysis

**Key Findings from Recent Benchmarking:**

Our analysis of 20 Embench-IoT benchmarks reveals different performance characteristics depending on the analysis mode:
(The script used for comparison can be found at [spive_vs_qemu.py](../spike_vs_qemu.py))

#### 1. **Normal Execution** - QEMU Performance Advantage
```
Total Spike time: 1.186s
Total QEMU time: 0.954s
Overall speedup: 0.80x (QEMU 1.24x faster)
```

**Finding**: For basic execution without instrumentation, QEMU shows a clear performance advantage.

#### 2. **BBV Generation** - Significant QEMU Advantage  
```
Total Spike time: 2.699s
Total QEMU time: 0.998s
Overall speedup: 0.37x (QEMU 2.70x faster)
```

**Analysis**: BBV generation shows significant overhead on Spike compared to QEMU's plugin approach, with QEMU maintaining much better performance.

#### 3. **Trace Generation** - Different Trace Formats
**Important Note**: QEMU and Spike use completely different trace formats:
- **Spike**: Generates detailed STF (System Trace Format) traces with instruction-level detail
- **QEMU**: Generates simple assembly traces using `-d in_asm` output

QEMU cannot generate STF traces - only basic assembly instruction logs.

### Performance Recommendations

| Use Case | Normal Mode | BBV Mode | Trace Mode | Recommended Emulator |
|----------|-------------|----------|------------|---------------------|
| **Development/Testing** | **QEMU** (1.24x faster) | **QEMU** (2.70x faster) | **QEMU** (assembly traces) | QEMU |
| **Large-scale Analysis** | **QEMU** | **QEMU** (much faster) | **QEMU** (much faster) | QEMU |
| **Detailed Research** | **QEMU** (unless accuracy critical) | **QEMU** | **Spike** (STF format) | Context-dependent |
| **Production Workflow** | **QEMU** | **QEMU** | **QEMU** | QEMU |

### Key Performance Insights

Based on recent benchmark analysis, QEMU generally offers better performance for large-scale workload execution. The choice between emulators should consider:

- **Speed requirements**: QEMU is consistently faster
- **Trace format needs**: Spike for STF traces, QEMU for assembly traces  
- **Analysis depth**: Spike for detailed architectural research, QEMU for throughput and bbv generation along with slicing and simpointing.

## BBV Generation Comparison

### Spike BBV (CSR-based)

**Mechanism:**
- Uses Control Status Register (CSR) accesses
- Automatic detection of ROI markers
- Built-in BBV generation

**Implementation:**
```c
// Markers compiled into binary with --bbv flag
START_BBV;  // csrsi 0x8c2, 1 - Start BBV collection
// benchmark execution
STOP_BBV;   // csrci 0x8c2, 1 - Stop BBV collection
```

**Command:**
```bash
# Automatic BBV generation when markers detected
spike --isa=rv32gc  --en_bbv --bb_file=./output.bbv --simpoint_size=100000  pk <workload_bin>
# Output: output.bbv
```


### QEMU BBV (Plugin-based)

**Mechanism:**
- External plugin for BBV collection
- Plugin-based instrumentation  
- Configurable interval sizes

**Implementation:**
```bash
# Plugin-based BBV generation
qemu-riscv32 -plugin /qemu/build/contrib/plugins/libbbv.so,interval=10000000,outfile=workload_bbv workload

# For Baremetal 
qemu-system-riscv32 -nographic -machine virt -bios none -kernel <workload_bi> -plugin /qemu/build/contrib/plugins/libbbv.so,interval=100000,outfile=./output.bbv
# Output: output.bbv.0.bb
```

**Plugin Parameters:**
- `interval=N`: Instructions per BBV interval
- `outfile=prefix`: Output file prefix


## Instruction Tracing Comparison

### Spike Tracing (STF Format)

**Features:**
- System Trace Format (STF) output
- Detailed instruction-level traces
- Memory access recording
- Register state tracking

**Command:**
```bash
# STF trace generation
spike --isa=rv32gc --trace-file=trace.stf pk workload
# Output: trace.stf (or compressed trace.zstf)
```

**STF Capabilities:**
- **Instructions**: Every executed instruction with PC
- **Memory Access**: Load/store addresses and values
- **Register Updates**: Register file changes
- **Control Flow**: Branch targets and conditions
- **Compression**: zstd compression (10x size reduction)

### QEMU Tracing (Text Format)

**Features:**
- Human-readable assembly traces
- Basic instruction logging
- CPU state information
- Simple text format

**Command:**
```bash  
# Assembly instruction tracing
qemu-riscv32 -d in_asm,cpu -D trace.log workload
# Output: trace.log
```

**QEMU Capabilities:**
- **Instructions**: Assembly instructions with addresses
- **CPU State**: Register and flag states
- **Basic Blocks**: Block-level execution flow
- **Text Format**: Easy parsing and analysis

**Example QEMU Content:**
```
# Text format
IN:
0x00010000: 00008137  lui      x2,0x8
0x00010004: 87010113  addi     x2,x2,-1936  
0x00010008: 00000517  auipc    x10,0x0
```


## Use Case Recommendations

QEMU for BBV Generation then Simpoint and then reduction of workload.
Then we use spike for Generating STF Traces based on the simpoint output.
## Benchmarking Framework

### Benchmarking Results

Recent benchmarking shows consistent QEMU performance advantages:

**Sample Results (Embench-IoT md5sum):**
```
Benchmark                 Mode       Spike (s)    QEMU (s)     Speedup    Status
--------------------------------------------------------------------------------
embench-iot:md5sum        normal     0.040        0.042        1.04x      ✓
embench-iot:md5sum        bbv        0.093        0.047        0.50x      ✓
```

**Key Finding:** QEMU maintains faster execution across different analysis modes, particularly beneficial for BBV generation with large workloads.

## Performance Optimization
### Accuracy Concerns

**Spike vs QEMU Result Differences:**
- Check for architectural differences (vector spec versions)
- Verify floating-point rounding modes
- Compare instruction counts for validation

**BBV Compatibility:**
- Both formats work with standard SimPoint tools
- Block numbering may differ (expected)
- Defined Regions of Interest (To do: in QEMU)

### Analysis Integration

1. **Unified Workflows**: Use provided scripts for consistent analysis
2. **Cross-Validation**: Compare results between emulators when critical
3. **Documentation**: Record emulator versions and configurations
4. **Reproducibility**: Save complete build and run configurations

The choice between Spike and QEMU depends on your analysis goals, time constraints, and accuracy requirements. The provided benchmarking framework helps quantify these trade-offs for your specific workloads.
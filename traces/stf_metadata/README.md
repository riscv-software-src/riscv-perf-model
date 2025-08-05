# STF (Simulation Trace Format) Metadata Specification

> This specification is still under development.

## Table of Contents

1. [Introduction](#introduction)
1. [Metadata Schema](#metadata-schema)
   1. [Example: dhrystone.zstf.metadata.yaml](#example-dhrystonzstfmetadatayaml)
1. [File Naming Convention](#file-naming-convention)
   1. [Example:](#example)

## Introduction

The STF Metadata Specification defines a standardized YAML format for describing metadata associated with simulation trace files. The goal for this metadata format is to accompany the STF file, providing information about the trace generator, workload, and compiler.

## Metadata Schema

Below is the schema for the STF metadata file:

```yaml
trace_id: "Automatically generated trace ID, used for trace archive solutions"
description: "Optional description, use this to describe scenario, input, etc."

author:
  name: "Full name of the person or entity generating the trace"
  company: "Organization or affiliation"
  e-mail: "Valid contact email"

workload:
  filename: "Binary filename used as workload"
  SHA256: "SHA256 hash of binary file"
  execution_command: "command line, including arguments, used to execute the workload"
  elf_sections:
    comment: "compiler comment (e.g. GCC: (GNU) 13.3.0)"
    riscv.attributes: "Architecture/ABI info"
    GCC.command.line: "GCC command-line flags used for compilation extracted with -frecord-gcc-switches"
  source_code:
    repository_url: "URL to source code repository"
    commit_hash: "Last commit hash used during workload compilation"

stf:
  timestamp: "ISO 8601 date-time of metadata creation"
  stf_trace_info:
    VERSION: "libstf version used by stf_tools"
    GENERATOR: "Trace generator name"
    GEN_VERSION: "Trace generator version"
    GEN_COMMENT: "Trace generator comment (e.g., git commit SHA for spike-stf)"
    STF_FEATURES: "list of STF features in the STF file"
  trace_interval:
    instruction_pc: "Program counter (PC) value at the start of the trace"
    pc_count: "Program counter execution count value present at the start of the trace"
    interval_lenght: "Number of instructions executed"
    start_instruction_index: "Instruction index at the beginning of the trace"
    end_instruction_index: "Instruction index at the end of the trace"
```

### Example: [dhrystone.zstf.metadata.yaml](./example/dhrystone.zstf.metadata.yaml)

```yaml
trace_id: dhrystone_v0_part0_rev0
description: null
author:
  name: Jane Doe
  company: RISCV
  email: jane.doe@riscv.org
workload:
  filename: dhrystone
  SHA256: 5c35ccfe1d5b81b2e37366b011107eec40e39aa2b930447edc1f83ceaf877066
  execution_command: ./dhrystone
  elf_sections:
    comment: "GCC: (riscv-embecosm-embedded-ubuntu2204-20250309) 15.0.1 20250308
      (experimental)"
    riscv.attributes: riscv rv64i2p1_m2p0_a2p1_f2p2_d2p2_c2p0_b1p0_zicsr2p0_zifencei2p0_zmmul1p0_zaamo1p0_zalrsc1p0_zca1p0_zcd1p0_zba1p0_zbb1p0_zbc1p0_zbs1p0
    GCC.command.line:
      GNU C99 15.0.1 20250308 (experimental) -mabi=lp64d -mcmodel=medany
      -misa-spec=20191213 -march=rv64imafdcb_zicsr_zifencei_zmmul_zaamo_zalrsc_zca_zcd_zba_zbb_zbc_zbs
      -O2 -std=gnu99 -fno-inline -ffast-math -funsafe-math-optimizations -finline-functions
      -fno-common -fno-builtin-printf
  source_code:
    repository_url: https://github.com/sifive/benchmark-dhrystone
    commit_hash: 0ddff533cc9052c524990d5ace4560372053314b
stf:
  timestamp: "2025-07-20T21:05:32.840053+00:00"
  stf_trace_info:
    VERSION: "1.5"
    GENERATOR: Spike
    GEN_VERSION: 2.0.0
    GEN_COMMENT: SPIKE SHA:32fa34983b853da98b8a17797275d52e0acda940
    STF_FEATURES:
      - STF_CONTAIN_PHYSICAL_ADDRESS
      - STF_CONTAIN_RV64
      - STF_CONTAIN_EVENT64
  trace_interval:
    instruction_pc: 0
    pc_count: 0
    interval_lenght: 100
    start_instruction_index: 0
    end_instruction_index: 100
```

## File Naming Convention

The metadata file must accompany the corresponding trace file, sharing the same base filename.

### Example

```
Trace file: dhrystone.zstf
Metadata file: dhrystone.zstf.metadata.yaml
```

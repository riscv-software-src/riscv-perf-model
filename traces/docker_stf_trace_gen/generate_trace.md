# Trace Generation Tool

This tool generates execution traces for RISC-V workloads using either **Spike** or **QEMU** emulators.  

There are **three different modes** for generating traces:
- **Macro** → uses `START_TRACE` and `STOP_TRACE` macros embedded in the workload  
- **Instruction Count (`insn_count`)** → traces a fixed number of instructions after skipping some  
- **Program Counter (`pc_count`)** → traces after a specific program counter (PC) is reached  

---

## Table of Contents

1. [Quickstart](#quickstart)
2. [Usage](#usage)
3. [Global Options](#global-options)
4. [Modes](#modes)  
   - [Macro](#macro)  
   - [Instruction Count](#insn_count)  
   - [Program Counter](#pc_count)  
5. [Mode Restrictions](#mode-restrictions)
6. [Summary Table](#summary-table)
7. [Help and More Info](#help-and-more-info)

---

## Quickstart

1. **Macro mode with Spike**  
   Trace using `START_TRACE` / `STOP_TRACE` markers inside the workload:  
    ```bash
   python generate_trace.py --emulator spike macro workload.elf
    ```

2. **Instruction Count mode**
   Skip 1000 instructions, then trace 5000 instructions:

   ```bash
   python generate_trace.py --emulator qemu insn_count \
       --num-instructions 5000 --start-instruction 1000 workload.elf
   ```

3. **Program Counter mode (QEMU only)**
   Start tracing after PC `0x80000000` is hit 5 times, trace 2000 instructions:

   ```bash
   python generate_trace.py --emulator qemu pc_count \
       --num-instructions 2000 --start-pc 0x80000000 --pc-threshold 5 workload.elf
   ```

---

## Usage

```bash
python generate_trace.py [OPTIONS] MODE WORKLOAD_PATH
```

Example with help:

```bash
python generate_trace.py macro --help
```

---

## Global Options

These options apply to all modes:

* **`--emulator {spike,qemu}`** *(required)*
  Select which emulator to use.

* **`--isa ISA`** *(optional)*
  Instruction set architecture (e.g., `rv64imafdc`).

* **`--dump`** *(flag)*
  Create a trace file dump.

* **`--pk`** *(flag)*
  Run Spike with **pk (proxy kernel)**.

* **`--image-name IMAGE_NAME`** *(default: `Const.DOCKER_IMAGE_NAME`)*
  Use a custom Docker image instead of the default.

* **`-o, --output OUTPUT`** *(optional)*
  Output folder or file path.

* **`workload`** *(positional, required)*
  Path to workload binary.

---

## Modes

### `macro`

Trace mode using `START_TRACE` and `STOP_TRACE` macros in the workload binary.

* **Only works with Spike**
* No additional arguments required beyond the workload path.

**Example:**

```bash
python generate_trace.py --emulator spike macro workload.elf
```

---

### `insn_count`

Trace a fixed number of instructions after skipping a given number.

**Arguments:**

* **`--num-instructions`** *(required, int)* → number of instructions to trace.
* **`--start-instruction`** *(required, int, default=0)* → instructions to skip before tracing starts.

**Example:**

```bash
python generate_trace.py --emulator qemu insn_count \
    --num-instructions 5000 --start-instruction 1000 workload.elf
```

---

### `pc_count`

Trace a fixed number of instructions after reaching a given PC value a certain number of times.

* **Only works with QEMU**

**Arguments:**

* **`--num-instructions`** *(required, int)* → number of instructions to trace.
* **`--start-pc`** *(required, int)* → starting program counter (hex or decimal).
* **`--pc-threshold`** *(required, int, default=1)* → number of times the PC must be hit before tracing begins.

**Example:**

```bash
python generate_trace.py --emulator qemu pc_count \
    --num-instructions 2000 --start-pc 0x80000000 --pc-threshold 5 workload.elf
```

---

## Mode Restrictions

* `macro` mode **cannot** be used with `qemu`.
* `pc_count` mode **cannot** be used with `spike`.
* Each mode has its own required arguments.

---

## Summary Table

| Mode         | Emulator   | Required Arguments                                   |
| ------------ | ---------- | ---------------------------------------------------- |
| `macro`      | spike      | workload                                             |
| `insn_count` | spike/qemu | `--num-instructions`, `--start-instruction`          |
| `pc_count`   | qemu       | `--num-instructions`, `--start-pc`, `--pc-threshold` |

---
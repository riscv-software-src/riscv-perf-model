# Trace Generation Tool

This utility emits STF traces for RISC-V workloads using Spike or QEMU. It supports two workflows:

1. **`single`** – produce one trace window from a workload binary (macro markers, instruction-count window or PC-count window)
2. **`sliced`** – replay SimPoint-selected windows and emit a manifest of per-interval traces

---

## Quickstart

1. **Spike macro markers**  
   ```bash
   python3 flow/generate_trace.py single macro --emulator spike build/aha-mont64.elf
   ```

2. **QEMU instruction-count window**  
   ```bash
   python3 flow/generate_trace.py single insn_count --emulator qemu --arch rv64 \
       --num-instructions 5000 --start-instruction 1000 build/aha-mont64.elf
   ```

3. **SimPoint slicing**  
   ```bash
   python3 flow/generate_trace.py sliced --emulator spike \
       --workload embench-iot --benchmark aha-mont64 --verify
   ```

---

## Usage

```bash
python3 flow/generate_trace.py <single|sliced> [OPTIONS]
```

To inspect per-command options:

```bash
python3 flow/generate_trace.py single --help
python3 flow/generate_trace.py sliced --help
```

---

## `single` command

Generates a single STF trace from a workload binary.

### Required arguments

- `macro | insn_count | pc_count` – mode-specific sub-command
- `binary` – path to the ELF to execute
- `--emulator {spike,qemu}` – execution engine (required for every mode)

### Optional arguments

- `--arch {rv32,rv64}` – QEMU target width (default `rv64`)
- `--isa ISA` – override the ISA passed to Spike (defaults to build metadata)
- `--num-instructions N` – number of instructions to trace (for `insn_count`/`pc_count`)
- `--start-instruction N` – instructions to skip before tracing (`insn_count`)
- `--start-pc PC` – program counter that triggers tracing (`pc_count`, accepts hex)
- `--pc-threshold N` – number of hits on `start-pc` before tracing (`pc_count`)
- `--pk` – launch Spike with the proxy kernel
- `--dump` – emit `stf_dump` output next to the trace
- `-o, --output PATH` – custom output file or directory (defaults to `<binary>.zstf`)

### Mode notes

- `macro` is Spike-only and relies on `START_TRACE` / `STOP_TRACE` macros compiled into the workload
- `insn_count` is available on Spike and QEMU
- `pc_count` is QEMU-only and uses the STF plugin in IP mode

---

## `sliced` command

Replays SimPoint intervals and produces a directory of per-interval STF traces.

### Required arguments

- `--workload NAME` – workload suite used during build/run
- `--benchmark NAME` – benchmark within the workload
- `--emulator spike` – slicing currently relies on Spike’s instruction-count tracing

### Optional arguments

- `--interval-size N` – override the interval size recorded in `run_meta.json`
- `--simpoints PATH` / `--weights PATH` – override the default SimPoint outputs under `/outputs/simpoint_analysis`
- `--verify` – run `stf_count` on each slice and record the measured instruction count
- `--dump` – emit `stf_dump` output for each slice
- `--clean` – remove the existing slice directory before regenerating

### Outputs

For each benchmark the tool writes:

- One `.zstf` trace per SimPoint interval under `/outputs/simpointed/<emulator>/<workload>/<benchmark>/`
- Matching `.metadata.json` files with trace provenance
- An aggregate `slices.json` manifest detailing weights, interval indices, verification status, and trace paths

---

## Workflow dependencies

Before running `sliced`, ensure the following steps have completed for the target benchmark:

1. `build_workload.py` (compiled with `--bbv` if SimPoint will be used)
2. `run_workload.py --bbv [--trace]` (produces BBV vectors and run metadata)
3. `run_simpoint.py --emulator spike --workload <suite> [--benchmark <bench>]`

---

## Mode restrictions

| Mode            | Emulator | Notes                                       |
|-----------------|----------|---------------------------------------------|
| `single/macro`  | spike    | Requires instrumentation macros             |
| `single/insn_count` | spike/qemu | Instruction window tracing             |
| `single/pc_count`  | qemu    | Uses STF plugin IP mode                    |
| `sliced`        | spike    | Requires SimPoint artefacts and run metadata |

Use `--dump` and `--verify` to generate additional artefacts for debugging or validation.

---

## Further help

Invoke the sub-command help flags for the most accurate, up-to-date argument list.

```bash
python3 flow/generate_trace.py single --help
python3 flow/generate_trace.py sliced --help
```

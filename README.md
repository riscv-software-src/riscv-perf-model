[![Regress Olympia on Ubuntu](https://github.com/riscv-software-src/riscv-perf-model/actions/workflows/ubuntu-build.yml/badge.svg)](https://github.com/riscv-software-src/riscv-perf-model/actions/workflows/ubuntu-build.yml)

# olympia

Olympia is a Performance Model written in C++ for the RISC-V community as an
_example_ of an Out-of-Order RISC-V CPU Performance Model based on the
[Sparta Modeling
Framework](https://github.com/sparcians/map/tree/master/sparta).

Olympia's intent is to provide a starting point for RISC-V CPU performance modeling development
enabling the community to build upon Olympia by extending its
functionality in areas like branch prediction, prefetching/caching
concepts, application profiling, middle-core design, etc.

Currently, Olympia is a _trace-driven_ simulator running instructions
streams provided in either [JSON
format](https://github.com/riscv-software-src/riscv-perf-model/tree/master/traces#json-inputs)
or [STF](https://github.com/sparcians/stf_spec).  However, extending
Olympia with a functional back-end to run applications natively is
under development.

## Building

1. Set up a clean working `conda` environment by following the directions [here](https://github.com/riscv-software-src/riscv-perf-model/tree/master/conda)
1. Download and build Sparta and checkout branch [map_v2](https://github.com/sparcians/map/tree/map_v2).  Follow the directions on the [Sparta README](https://github.com/sparcians/map/tree/map_v2#building-map) to build _and install_ Sparta
1. Make sure you have the [required libraries](https://github.com/sparcians/stf_lib#required-packages) for the STF toolsuite installed
1. Clone olympia
   ```
   git clone --recursive git@github.com:riscv-software-src/riscv-perf-model.git
   ```
1. Build Olympia in the new `conda` environment created

```

################################################################################
# Enable conda environment (suggested)
conda activate sparta

################################################################################
# Optimized, no symbols

# A release build
mkdir release; cd release

# Assumes sparta was installed in the conda environment.
# If not, use -DSPARTA_SEARCH_DIR=/path/to/sparta/install
cmake .. -DCMAKE_BUILD_TYPE=Release

# Just builds the simulator
make olympia

################################################################################
# Fast Debug, optimized (not LTO) with debug symbols

# A FastDebug build
mkdir fastdebug; cd fastdebug

# Assumes sparta was installed in the conda environment.
# If not, use -DSPARTA_SEARCH_DIR=/path/to/sparta/install
cmake .. -DCMAKE_BUILD_TYPE=fastdebug

# Just builds the simulator
make olympia

################################################################################
# Debug

# A debug build
mkdir debug; cd debug

# Assumes sparta was installed in the conda environment.
# If not, use -DSPARTA_SEARCH_DIR=/path/to/sparta/install
cmake .. -DCMAKE_BUILD_TYPE=Debug

# Just builds the simulator
make olympia

################################################################################
# Regression
make regress

```

## Developing

Developing on Olympia is encouraged!  Please check out the
[Issue](https://github.com/riscv-software-src/riscv-perf-model/issues)
section for areas of needed contributions.  If there is no Assignee,
the work isn't being done!

When developing on Olympia, please adhere to the documented [Coding
Style Guidelines](./CodingStyle.md).

## Example Usage

### Get Help Messages
```
./olympia --help                  # Full help
./olympia --help-brief            # Brief help
./olympia --help-topic topics     # Topics to get detailed help on
./olympia --help-topic parameters # Help on parameters
```

### Get Simulation Layout
```
./olympia --show-tree       --no-run # Show the full tree; do not run the simulator
./olympia --show-parameters --no-run # Show the parameter tree; do not run the simulator
./olympia --show-loggers    --no-run # Show the loggers; do not run the simulator
# ... more --show options; see help
```

### Running

```
# Run a given JSON "trace" file
./olympia ../traces/example_json.json

# Run a given STF trace file
./olympia ../traces/dhry_riscv.zstf

# Run a given STF trace file only 100K instructions
./olympia -i100K ../traces/dhry_riscv.zstf

# Run a given STF trace file and generate a
# generic full simulation report
./olympia ../traces/dhry_riscv.zstf --report-all dhry_report.out
```

### Generate and Consume Configuration Files

```
# Generate a baseline config
./olympia --write-final-config baseline.yaml --no-run

# Generate a config with a parameter change
./olympia -p top.cpu.core0.lsu.params.tlb_always_hit true --write-final-config always_hit_DL1.yaml --no-run
dyff between baseline.yaml always_hit_DL1.yaml

# Use the configuration file generated
./olympia -c always_hit_DL1.yaml -i1M ../traces/dhry_riscv.zstf
```

### Generate Logs
```
# Log of all messages, different outputs
./olympia -i1K --auto-summary off ../traces/dhry_riscv.zstf \
   -l top info all_messages.log.basic   \
   -l top info all_messages.log.verbose \
   -l top info all_messages.log.raw

# Different logs, some shared
./olympia -i1K --auto-summary off ../traces/dhry_riscv.zstf \
   -l top.*.*.decode info decode.log \
   -l top.*.*.rob    info rob.log    \
   -l top.*.*.decode info decode_rob.log \
   -l top.*.*.rob    info decode_rob.log
```
### Generate Reports
```
# Run with 1M instructions, generate a report from the top of the tree
# with stats that are not hidden; turn off the auto reporting
cat reports/core_stats.yaml
./olympia -i1M ../traces/dhry_riscv.zstf --auto-summary off  --report "top" reports/core_stats.yaml my_full_report.txt text

# Generate a report only for decode in text form
./olympia -i1M ../traces/dhry_riscv.zstf --auto-summary off  --report "top.cpu.core0.decode" reports/core_stats.yaml my_decode_report.txt text

# Generate a report in JSON format
./olympia -i1M ../traces/dhry_riscv.zstf --auto-summary off  --report "top" reports/core_stats.yaml my_json_report.json json

# Generate a report in CSV format
./olympia -i1M ../traces/dhry_riscv.zstf --auto-summary off  --report "top" reports/core_stats.yaml my_csv_report.csv csv

# Generate a report in HTML format
./olympia -i1M ../traces/dhry_riscv.zstf --auto-summary off  --report "top" reports/core_stats.yaml my_html_report.html html
```

### Generate More Complex Reports
```
# Using a report definition file, program the report collection to
# start after 500K instructions
cat reports/core_report.def
./olympia -i1M ../traces/dhry_riscv.zstf --auto-summary off    \
   --report reports/core_report.def  \
   --report-search reports           \
   --report-yaml-replacements        \
       OUT_BASE my_report            \
       OUT_FORMAT text               \
       INST_START 500K

# Generate a time-series report -- capture all stats every 10K instructions
cat reports/core_timeseries.def
./olympia -i1M ../traces/dhry_riscv.zstf --auto-summary off       \
   --report reports/core_timeseries.def \
   --report-search reports              \
   --report-yaml-replacements           \
       OUT_BASE my_report               \
       TS_PERIOD 10K
python3 ./reports/plot_ts.y my_report_time_series_all.csv
```

### Experimenting with Architectures
```
# By default, olympia uses the small_core architecture
./olympia -i1M  ../traces/dhry_riscv.zstf --auto-summary off --report-all report_small.out

# Use the medium sized core
cat arches/medium_core.yaml  # Example the medium core
./olympia -i1M  ../traces/dhry_riscv.zstf --arch medium_core --auto-summary off --report-all report_medium.out
diff -y -W 150 report_small.out report_medium.out

# Use the big core
cat arches/big_core.yaml  # Example the medium core
./olympia -i1M  ../traces/dhry_riscv.zstf --arch big_core --auto-summary off --report-all report_big.out
diff -y -W 150 report_medium.out report_big.out

```

### Generate and View a Pipeout
```
./olympia -i1M ../traces/dhry_riscv.zstf --debug-on-icount 100K -i 101K -z pipeout_1K --auto-summary off

# Launch the viewer
# *** MacOS use pythonw
python $MAP_BASE/helios/pipeViewer/pipe_view/argos.py -d pipeout_1K -l ../layouts/small_core.alf
```

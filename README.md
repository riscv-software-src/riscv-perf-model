# olympia

An extraction of the Map/Spara Example Performance Model based on the
Sparta Modeling Framework, Olympia is a fully-featured OoO RISC-V CPU
performance model for the RISC-V community.

Olympia is a _trace-driven_ simulator running instructions streams
defined in either JSON format or
[STF](https://github.com/sparcians/stf_spec).

# Build Directions

1. Download and build Sparta.  Follow the directions on the [Sparta README](https://github.com/sparcians/map/tree/master/sparta)
1. Clone olympia
   ```
   git clone --recursive git@github.com:riscv-software-src/riscv-perf-model.git
   ```
1. Build Olympia

```

################################################################################
# Optimized

# A release build
mkdir release; cd release

# Assumes a build of sparta at /path/to/map/sparta/release
cmake .. -DCMAKE_BUILD_TYPE=Release -DSPARTA_BASE=/path/to/map/sparta

# Just builds the simulator
make olympia

################################################################################
# Debug

# A release build
mkdir debug; cd debug

# Assumes a build of sparta at /path/to/map/sparta/debug
cmake .. -DCMAKE_BUILD_TYPE=Debug -DSPARTA_BASE=/path/to/map/sparta

# Just builds the simulator
make olympia

################################################################################
# Regression
make regress

```

# Limitations

1. Rename doesn't actually rename.  In fact, there are no operand
dependencies supported... yet.  (This work to be done)[https://github.com/riscv-software-src/riscv-perf-model/issues/2]
1. The model's topology is "fixed" meaning that a user cannot change
fundamental attributes like number of ALU units, FPU, LS, etc (This work to be done)[https://github.com/riscv-software-src/riscv-perf-model/issues/5]


# Example Usage

## Get help messages
```
./olympia --help                  # Full help
./olympia --help-brief            # Brief help
./olympia --help-topic topics     # Topics to get detailed help on
./olympia --help-topic parameters # Help on parameters
```

## Get simulation layout
```
./olympia --show-tree       --no-run # Show the full tree; do not run the simulator
./olympia --show-parameters --no-run # Show the parameter tree; do not run the simulator
./olympia --show-loggers    --no-run # Show the loggers; do not run the simulator
# ... more --show options; see help
```

## Running

```
# Run a given JSON "trace" file
./olympia ../traces/example_json.json

# Run a given STF trace file
./olympia ../traces/dhrystone.zstf

# Run a given STF trace file only 100K instructions
./olympia -i100K ../traces/dhrystone.zstf

# Run a given STF trace file and generate a
# generic full simulation report
./olympia ../traces/dhrystone.zstf --report-all dhry_report.out
```

## Generate and consume Configuration Files

```
# Generate a baseline config
./olympia --write-final-config baseline.yaml --no-run

# Generate a config with a parameter change
./olympia -p top.cpu.core0.lsu.params.tlb_always_hit true --write-final-config always_hit_DL1.yaml --no-run
dyff between baseline.yaml always_hit_DL1.yaml

# Use the configuration file generated
./olympia -c always_hit_DL1.yaml -i1M ../traces/dhrystone.zstf
```

## Generate logs
```
# Log of all messages, different outputs
./olympia -i1K --auto-summary off ../traces/dhrystone.zstf \
   -l top info all_messages.log.basic   \
   -l top info all_messages.log.verbose \
   -l top info all_messages.log.raw

# Different logs, some shared
./olympia -i1K --auto-summary off ../traces/dhrystone.zstf \
   -l top.*.*.decode info decode.log \
   -l top.*.*.rob    info rob.log    \
   -l top.*.*.decode info decode_rob.log \
   -l top.*.*.rob    info decode_rob.log
```
## Generate reports
```
# Run with 1M instructions, generate a report from the top of the tree
# with stats that are not hidden; turn off the auto reporting
cat reports/core_stats.yaml
./olympia -i1M ../traces/dhrystone.zstf --auto-summary off  --report "top" reports/core_stats.yaml my_full_report.txt text

# Generate a report only for decode in text form
./olympia -i1M ../traces/dhrystone.zstf --auto-summary off  --report "top.cpu.core0.decode" reports/core_stats.yaml my_decode_report.txt text

# Generate a report in JSON format
./olympia -i1M ../traces/dhrystone.zstf --auto-summary off  --report "top" reports/core_stats.yaml my_json_report.json json

# Generate a report in CSV format
./olympia -i1M ../traces/dhrystone.zstf --auto-summary off  --report "top" reports/core_stats.yaml my_csv_report.csv csv

# Generate a report in HTML format
./olympia -i1M ../traces/dhrystone.zstf --auto-summary off  --report "top" reports/core_stats.yaml my_html_report.html html
```

## Generate more complex reports
```
# Using a report definition file, program the report collection to
# start after 500K instructions
cat reports/core_report.def
./olympia -i1M ../traces/dhrystone.zstf --auto-summary off    \
   --report reports/core_report.def  \
   --report-search reports           \
   --report-yaml-replacements        \
       OUT_BASE my_report            \
       OUT_FORMAT text               \
       INST_START 500K

# Generate a time-series report -- capture all stats every 10K instructions
cat reports/core_timeseries.def
./olympia -i1M ../traces/dhrystone.zstf --auto-summary off       \
   --report reports/core_timeseries.def \
   --report-search reports              \
   --report-yaml-replacements           \
       OUT_BASE my_report               \
       TS_PERIOD 10K
python3 ./reports/plot_ts.y my_report_time_series_all.csv
```

## Generate and view a pipeout
```
./olympia -i1M ../traces/dhrystone.zstf --debug-on-icount 100K -i 101K -z pipeout_1K --auto-summary off

# Launch the viewer
# *** MacOS use pythonw
python  $MAP_BASE/helios/pipeViewer/pipe_view/argos.py -d pipeout_1K -l ../layouts/core.alf
```

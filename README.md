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

1. Clone olympia, making sure this is done recursively.
   ```
   git clone --recursive git@github.com:riscv-software-src/riscv-perf-model.git
   ```
1. Before building, make sure the following packages are installed:

   * Sparta v2: https://github.com/sparcians/map?tab=readme-ov-file#simple
   * (cmake) cmake v3.22
   * (libboost-all-dev) boost 1.78.0
   * (yaml-cpp-dev) YAML CPP 0.7.0
   * (rapidjson-dev) RapidJSON CPP 1.1.0
   * (libsqlite3-dev) SQLite3 3.37.2
   * (libhdf5-dev) HDF5 1.10.7
   * (clang++) Clang, Version: 14.0.0 OR (g++) v13.0.0 or greater

1. Build Olympia in one of three modes (see below):
   * `release`: Highly optimized, no debug w/ LTO (if supported)
   * `fastdebug`: Optimized, no LTO, with debug information
   * `debug`: All optimizations are disabled

If sparta was NOT installed in the standand locations, the user can
specify the path to sparta by setting
`-DSPARTA_SEARCH_DIR=/path/to/sparta/install/` to `cmake`

```
################################################################################
# Optimized, no symbols

# A release build
mkdir release; cd release

# Assumes sparta was installed on the local machine
# If not, use -DSPARTA_SEARCH_DIR=/path/to/sparta/install
cmake .. -DCMAKE_BUILD_TYPE=release

# Just builds the simulator
make olympia

################################################################################
# Fast Debug, optimized (not LTO) with debug symbols

# A FastDebug build
mkdir fastdebug; cd fastdebug

# Assumes sparta was installed on the local machine
# If not, use -DSPARTA_SEARCH_DIR=/path/to/sparta/install
cmake .. -DCMAKE_BUILD_TYPE=fastdebug

# Just builds the simulator
make olympia

################################################################################
# Debug

# A debug build
mkdir debug; cd debug

# Assumes sparta was installed on the local machine
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
### Generate PEvents (for Correlation)

PEvents or Performance Events are part of the Sparta Modeling
Framework typically used to correlate a performance model with RTL.
Unlike pipeout collection Name/Value Definition Pairs (see an example
in
[Inst.hpp](https://github.com/riscv-software-src/riscv-perf-model/blob/db6c2f1878381389966544d7844c7d10c557083e/core/Inst.hpp#L330)),
PEvent Name/Value Definitions are typically more compact.  Below the
surface, Sparta uses the logging infrastructure to collect the data.

Olympia has instrumented a few PEvents as an example.  The following
commands are useful in listing/using this functionality.

```
# Dump the list of supported PEvents
./olympia --help-pevents --no-run

# Generate RETIRE only pevents for the first 100 instructions of Dhrystone
./olympia traces/dhry_riscv.zstf --pevents retire.out RETIRE -i100

# Generate COMPLETE only pevents ...
./olympia traces/dhry_riscv.zstf --pevents complete.out COMPLETE -i100

# Generate COMPLETE pevents into complete.out and RETIRE pevents into retire.out ...
./olympia traces/dhry_riscv.zstf --pevents retire.out RETIRE --pevents complete.out COMPLETE -i100

# Generate RETIRE and COMPLETE pevents to the same file
./olympia traces/dhry_riscv.zstf --pevents complete_retire.out RETIRE,COMPLETE -i100

# Generate all pevents
./olympia traces/dhry_riscv.zstf --pevents complete_retire.out all -i100
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
cat arches/medium_core.yaml  # Example of the medium core
./olympia -i1M  ../traces/dhry_riscv.zstf --arch medium_core --auto-summary off --report-all report_medium.out
diff -y -W 150 report_small.out report_medium.out

# Use the big core
cat arches/big_core.yaml  # Example of the big core
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

### Issue Queue Modeling
Olympia has the ability to define issue queue to execution pipe mapping, as well as what pipe targets are available per execution unit. Also, with the implemenation of issue queue, Olympia now has a generic execution unit for all types, so one doesn't have to define `alu0` or `fpu0`, it is purely based off of the pipe targets, instead of unit types as before.
In the example below:
```
top.cpu.core0.extension.core_extensions:
  # this sets the pipe targets for each execution unit
  # you can set a multiple or just one:
  # ["int", "div"] would mean this execution pipe can accept
  # targets of: "int" and "div"
  pipelines:
  [
    ["int"], # exe0
    ["int", "div"], # exe1
    ["int", "mul"], # exe2
    ["int", "mul", "i2f", "cmov"], # exe3
    ["int"], # exe4
    ["int"], # exe5
    ["float", "faddsub", "fmac"], # exe6
    ["float", "f2i"], # exe7
    ["br"], # exe8
    ["br"] # exe9
  ]
  # this is used to set how many units per queue
  # ["0", "3"] means iq0 has exe0, exe1, exe2, and exe3, so it's inclusive
  # if you want just one execution unit to issue queue you can do:
  # ["0"] which would result in iq0 -> exe0
  # *note if you change the number of issue queues,
  # you need to add it to latency matrix below

  issue_queue_to_pipe_map:
  [
    ["0", "1"], # iq0 -> exe0, exe1
    ["2", "3"], # iq1 -> exe2, exe3
    ["4", "5"], # iq2 -> exe4, exe5
    ["6", "7"], # iq3 -> exe6, exe7
    ["8", "9"]  # iq4 -> exe8, exe9
  ]
```

The `pipelines` section defines for each execution unit, what are it's pipe targets. For example, the first row that has `["int"]` defines the first execution unit `exe0` that only handles instructions with pipe targets of `int`. Additionally, the second row defines an execution unit that handles instructions of `int` and `div` and so on.

The `issue_queue_to_pipe_map` defines which execution units map to which issue queues, with the position being the issue queue number. So in the above `["0", "1"]` in the first row is the first issue queue that connects to `exe0` and `exe1`, so do note it's inclusive of the end value. If one wanted to have a one execution unit to issue queue mapping, the above would turn into:
```
issue_queue_to_pipe_map:
  [
    ["0"], # iq0 -> exe0
    ["1"], # iq1 -> exe1
    ["2"], # iq2 -> exe2
    ["3"], # iq3 -> exe3
    ["4"], # iq4 -> exe4
    ["5"], # iq5 -> exe5
    ["6"], # iq6 -> exe6
    ["7"], # iq7 -> exe7
    ["8"], # iq8 -> exe8
    ["9"], # iq9 -> exe9
  ]
```

Additionally, one can rename the issue queues and execution units to more descriptive names of their use such as:
```
exe_pipe_rename:
  [
    ["exe0", "alu0"],
    ["exe1", "alu1"],
    ["exe2", "alu2"],
    ["exe3", "alu3"],
    ["exe4", "alu4"],
    ["exe5", "alu5"],
    ["exe6", "fpu0"],
    ["exe7", "fpu1"],
    ["exe8", "br0"],
    ["exe9", "br1"],
  ]

  # optional if you want to rename each iq* unit
  issue_queue_rename:
  [
    ["iq0", "iq0_alu"],
    ["iq1", "iq1_alu"],
    ["iq2", "iq2_alu"],
    ["iq3", "iq3_fpu"],
    ["iq4", "iq4_br"],
  ]
```
The above shows a 1 to 1 mapping of the renaming the execution units and issue queues. Do keep in mind that the order does matter, so you have to rename it exe0, exe1 and in order. Additionally, you have to either rename all execution units or all issue queue units, you cannot do partial. You can rename only the execution units but not the issue queues.

Finally, if you do rename the issue queue names, you will need to update their definition in the scoreboard as so:
```
top.cpu.core0.rename.scoreboards:
  # From
  # |
  # V
  integer.params.latency_matrix: |
      [["",         "lsu",     "iq0_alu", "iq1_alu", "iq2_alu", "iq3_fpu", "iq4_br"],
      ["lsu",       1,         1,         1,          1,        1,         1],
      ["iq0_alu",   1,         1,         1,          1,        1,         1],
      ["iq1_alu",   1,         1,         1,          1,        1,         1],
      ["iq2_alu",   1,         1,         1,          1,        1,         1],
      ["iq3_fpu",   1,         1,         1,          1,        1,         1],
      ["iq4_br",    1,         1,         1,          1,        1,         1]]
  float.params.latency_matrix: |
      [["",         "lsu",     "iq0_alu", "iq1_alu", "iq2_alu", "iq3_fpu", "iq4_br"],
      ["lsu",       1,         1,         1,          1,        1,         1],
      ["iq0_alu",   1,         1,         1,          1,        1,         1],
      ["iq1_alu",   1,         1,         1,          1,        1,         1],
      ["iq2_alu",   1,         1,         1,          1,        1,         1],
      ["iq3_fpu",   1,         1,         1,          1,        1,         1],
      ["iq4_br",    1,         1,         1,          1,        1,         1]]
```

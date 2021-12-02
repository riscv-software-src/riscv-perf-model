# olympia
aA RISC-V CPU Trace-Drirven Performance Model based on the Sparta Modeling Framework.

# Build Directions

1. Download and build Sparta.  Follow the directions on the [Sparta README](https://github.com/sparcians/map/tree/master/sparta)
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
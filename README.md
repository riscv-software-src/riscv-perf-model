# olympia
A RISC-V CPU Trace-Drirven Performance Model based on the Sparta Modeling Framework.

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

# Limitaions

1. The model is not trace-driven nor execution-driven ... yet.  Will be working on adding support for a true decoder and STF trace reading ([STF](https://github.com/sparcians/stf_lib))
2. Rename doesn't actually rename.  In fact, there are no operand dependencies supported... yet.  Again, need a decoder and a real trace

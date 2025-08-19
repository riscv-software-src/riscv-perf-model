# Adding New Workloads

This guide explains how to integrate custom benchmark suites into the RISC-V Workload Build & Run System. Our workload-agnostic design means most benchmarks can be added without source modifications.

## Overview

The system supports workloads through:
- **Smart Runtime Environment**: Provides compatible entry points via weak symbols
- **Flexible Build System**: Handles different source layouts and compilation requirements
- **Automatic Integration**: BBV and trace markers are added automatically

## Quick Example

To add a new workload called "my-benchmark":

```python
# 1. Add to DEFAULT_WORKLOADS in build_workload.py
DEFAULT_WORKLOADS = {
    "embench-iot": "/workloads/embench-iot",
    "riscv-tests": "/workloads/riscv-tests",
    "my-benchmark": "/workloads/my-benchmark"  # Add this line
}

# 2. Add build logic for your workload type
elif workload_type == "my-benchmark":
    # Your build logic here
    benchmarks = get_my_benchmark_list()
    for bench in benchmarks:
        exe_path = build_my_benchmark(bench, workload_path, cc, base_cflags, 
                                    platform, board, bbv, trace)
        executables.append(exe_path)
```

## Workload Requirements

### Mandatory Requirements

Your workload MUST provide either:

1. **`benchmark()` function** (Embench-style):
   ```c
   int benchmark(void) {
       // Your benchmark implementation
       return result_value;
   }
   ```

2. **`main()` function** (RISC-V Tests style):
   ```c
   int main(void) {
       // Your benchmark implementation  
       return result_value;
   }
   ```

### Optional Functions

Implement these for advanced features:

```c
// Called before benchmark execution
void initialise_benchmark(void) {
    // Setup code
}

// Called after benchmark for validation
int verify_benchmark(int result) {
    // Return 1 for success, 0 for failure
    return (result == expected_value) ? 1 : 0;
}

// Called for cache warming (if needed)
void warm_caches(int heat) {
    // Cache warming code
}
```

## Runtime Integration

### Smart Entry Point

The runtime automatically calls your workload:

```c
// env_main() in environment/main.c
int env_main(int argc, char *argv[]) {
    initialise_board();
    initialise_benchmark();
    warm_caches(WARMUP_HEAT);
    
    START_BBV; START_TRACE;
    result = benchmark();  // Calls your implementation
    STOP_TRACE; STOP_BBV;
    
    correct = verify_benchmark(result);
    return (!correct);
}
```

### Execution Flow by Workload Type

**Embench-style (provides `benchmark()`):**
```
env_main() → benchmark() [your code] → benchmark_body() [internal]
```

**RISC-V Tests style (provides `main()`):**
```
env_main() → benchmark() [weak fallback] → main() [your code]
```

## Directory Structure

Organize your workload following these patterns:

### Option 1: Embench-style Layout
```
/workloads/my-benchmark/
├── src/
│   ├── benchmark1/
│   │   ├── benchmark1.c        # Contains benchmark() function
│   │   └── other_files.c
│   ├── benchmark2/
│   │   └── benchmark2.c
│   └── ...
└── README.md
```

### Option 2: RISC-V Tests Layout  
```
/workloads/my-benchmark/
├── benchmarks/
│   ├── common/                 # Shared utilities
│   │   ├── util.c
│   │   └── util.h
│   ├── benchmark1/
│   │   └── benchmark1.c        # Contains main() function
│   ├── benchmark2/
│   │   └── benchmark2.c
│   └── ...
└── README.md
```

### Option 3: Custom Layout
```
/workloads/my-benchmark/
├── custom_dir/
│   └── sources...
└── build_config.json          # Custom build configuration
```

## Implementation Steps

### Step 1: Add Workload Definition

Edit `build_workload.py` to add your workload:

```python
DEFAULT_WORKLOADS = {
    "embench-iot": "/workloads/embench-iot",
    "riscv-tests": "/workloads/riscv-tests", 
    "my-benchmark": "/workloads/my-benchmark",
    "another-suite": "/workloads/another-suite"
}
```

### Step 2: Implement Benchmark Discovery

Add a function to find benchmarks in your workload:

```python
def list_benchmarks(workload_path, workload_type):
    """List available benchmarks for a workload"""
    if workload_type == "embench-iot":
        src_dir = Path(workload_path) / "src"
        return [d.name for d in src_dir.iterdir() if d.is_dir()]
    elif workload_type == "riscv-tests":
        bench_dir = Path(workload_path) / "benchmarks"
        return [d.name for d in bench_dir.iterdir() 
               if d.is_dir() and d.name != "common"]
    elif workload_type == "my-benchmark":  # Add this
        # Implement your benchmark discovery logic
        bench_dir = Path(workload_path) / "benchmarks"
        return [f.stem for f in bench_dir.glob("*.c")]
    return []
```

### Step 3: Implement Build Function

Create a build function for your workload:

```python
def build_my_benchmark(bench_name, workload_path, cc, base_cflags, platform, board, 
                      bbv=False, trace=False):
    """Build a single my-benchmark benchmark"""
    bench_file = Path(workload_path) / "benchmarks" / f"{bench_name}.c"
    if not bench_file.exists():
        log("ERROR", f"Benchmark file not found: {bench_file}")
    
    # Build compilation flags
    cflags = base_cflags.split()
    
    if platform == "baremetal":
        env_base = f"/workloads/environment/{board}"
        cflags.extend([f"-D{board.upper()}", "-mstrict-align", f"-I{env_base}", 
                      "-DCPU_MHZ=1", "-DWARMUP_HEAT=1"])
    
    if bbv:
        cflags.append("-DBBV")
    if trace:
        cflags.append("-DTRACE")
    
    # Compile
    obj_file = f"/workloads/bin/{board}/{bench_name}.o"
    compile_cmd = [cc, "-c"] + cflags + ["-o", obj_file, str(bench_file)]
    run_cmd(compile_cmd, f"Failed to compile {bench_name}")
    
    # Link
    exe_path = f"/workloads/bin/{board}/{bench_name}"
    if platform == "baremetal":
        env_base = f"/workloads/environment/{board}"
        env_objs = [f"{env_base}/{f}.o" for f in ["crt0", "main", "beebsc", "board", "chip", "stub", "util", "dummy-libc"]]
        link_cmd = [cc] + base_cflags.split() + [f"-T{env_base}/link.ld", "-nostartfiles", 
                   "-Wl,--no-warn-rwx-segments", "-o", exe_path, obj_file] + env_objs + ["-lc", "-lm"]
    else:
        link_cmd = [cc] + base_cflags.split() + ["-o", exe_path, obj_file] + ["-lm"]
    
    run_cmd(link_cmd, f"Failed to link {bench_name}")
    return exe_path
```

### Step 4: Add Build Logic

Integrate your build function into the main build logic:

```python
def build_workload(workload, arch, platform, board, benchmark=None, custom_path=None, 
                  bbv=False, trace=False):
    # ... existing code ...
    
    elif workload_type == "my-benchmark":  # Add this section
        available = list_benchmarks(workload_path, "my-benchmark")
        benchmarks = [benchmark] if benchmark else available
        
        if benchmark and benchmark not in available:
            log("ERROR", f"Benchmark {benchmark} not found. Available: {', '.join(available)}")
        
        # Build each benchmark
        for bench in benchmarks:
            log("INFO", f"Building {bench}")
            exe_path = build_my_benchmark(bench, workload_path, cc, base_cflags, 
                                        platform, board, bbv, trace)
            executables.append(exe_path)
```

## BBV & Trace Integration

### Automatic Markers

BBV and trace markers are automatically added by the runtime environment. No source modification needed:

```c
// In environment/main.c
#ifdef BBV
    START_BBV;  // Automatically inserted before benchmark()
#endif
#ifdef TRACE
    START_TRACE;
#endif 
    result = benchmark();
#ifdef TRACE
    STOP_TRACE;  // Automatically inserted after benchmark()
#endif
#ifdef BBV
    STOP_BBV;
#endif
```

### Custom Region Marking

For fine-grained control, manually add markers in your workload:

```c
#include "support.h"  // Provides marker definitions

int benchmark(void) {
    // Setup code (not measured)
    setup_data();
    
    START_BBV;  START_TRACE;
    // Region of Interest
    compute_result();
    STOP_TRACE; STOP_BBV;
    
    // Cleanup code (not measured) 
    cleanup_data();
    return result;
}
```

### Marker Definitions

The markers are defined for cross-platform compatibility:

```c
// For Spike BBV
#define START_BBV  __asm__ volatile ("csrsi 0x8c2, 1");
#define STOP_BBV   __asm__ volatile ("csrci 0x8c2, 1");

// For cross-platform tracing
#define START_TRACE asm volatile ("xor x0, x0, x0");
#define STOP_TRACE  asm volatile ("xor x0, x1, x1");
```

## Build System Integration

### Compiler Flags

The system automatically handles:
- **Architecture**: `-march=rv32gc` or `-march=rv64gc`
- **ABI**: `-mabi=ilp32d` or `-mabi=lp64d`
- **Platform**: Baremetal vs Linux linking
- **Board-specific**: `-DSPIKE` or `-DQEMU` defines

### Linking

**Baremetal linking:**
```bash
riscv64-unknown-elf-gcc -Tlink.ld -nostartfiles \
    -Wl,--no-warn-rwx-segments \
    benchmark.o crt0.o main.o board.o chip.o util.o \
    -lc -lm -o benchmark
```

**Linux linking:**
```bash
riscv64-linux-gnu-gcc -static -O2 \
    benchmark.o -lm -o benchmark
```

## Testing Your Workload

### Build Test

```bash
# Build specific benchmark
./build_workload.py --workload my-benchmark --benchmark test1 --board spike

# Build all benchmarks  
./build_workload.py --workload my-benchmark --board qemu --arch rv64

# Build with BBV support
./build_workload.py --workload my-benchmark --bbv --board spike
```

### Run Test

```bash
# Run built workloads
./run_workload.py --emulator spike --workload test1

# Run with analysis features
./run_workload.py --emulator qemu --bbv --trace
```

### Verify Output

Check that your workload produces expected results:

```bash
# Check binary was created
ls -la /workloads/bin/spike/test1

# Check execution logs
cat /output/spike_output/logs/test1.log

# Check BBV generation (if enabled)
ls -la /output/spike_output/bbv/test1.bbv
```

## Advanced Features

### Architecture-Specific Builds

Handle different architectural requirements:

```python
def build_my_benchmark(bench_name, workload_path, cc, base_cflags, platform, board, 
                      bbv=False, trace=False):
    # Check architecture from base_cflags
    is_rv64 = "rv64" in base_cflags
    arch_bits = "64" if is_rv64 else "32"
    
    # Architecture-specific flags
    if bench_name.startswith("64bit-") and not is_rv64:
        log("ERROR", f"{bench_name} requires RV64 architecture")
    
    if bench_name.startswith("vector-"):
        # Enable vector extension
        modified_cflags = []
        for flag in base_cflags.split():
            if flag.startswith("-march="):
                modified_cflags.append(f"-march=rv{arch_bits}gcv")  # Add vector
            else:
                modified_cflags.append(flag)
        base_cflags = " ".join(modified_cflags)
```

### Custom Compilation

Handle special compilation requirements:

```python
def build_my_benchmark(bench_name, workload_path, cc, base_cflags, platform, board,
                      bbv=False, trace=False):
    # Special handling for specific benchmarks
    if bench_name == "floating-point-intensive":
        cflags.extend(["-mhard-float", "-D__STDC_IEC_559__"])
    elif bench_name == "memory-intensive":
        cflags.extend(["-DLARGE_DATASET", "-mcmodel=medany"])
    elif bench_name.startswith("crypto-"):
        cflags.extend(["-mbmi2", "-DCRYPTO_OPTIMIZATIONS"])
```

### Multiple Source Files

Handle benchmarks with multiple source files:

```python
def build_my_benchmark(bench_name, workload_path, cc, base_cflags, platform, board,
                      bbv=False, trace=False):
    bench_dir = Path(workload_path) / "src" / bench_name
    
    # Find all source files
    c_files = list(bench_dir.glob("*.c"))
    s_files = list(bench_dir.glob("*.S"))
    cpp_files = list(bench_dir.glob("*.cpp"))
    
    obj_files = []
    
    # Compile each source file
    for source_file in c_files + s_files + cpp_files:
        obj_file = f"/workloads/bin/{board}/{source_file.stem}.o"
        obj_files.append(obj_file)
        
        # Use g++ for C++ files
        compiler = cc.replace("gcc", "g++") if source_file.suffix == ".cpp" else cc
        compile_cmd = [compiler, "-c"] + cflags + ["-o", obj_file, str(source_file)]
        run_cmd(compile_cmd, f"Failed to compile {source_file}")
    
    # Link all objects
    link_cmd = [cc] + base_cflags.split() + ["-o", exe_path] + obj_files + env_objs + ["-lc", "-lm"]
    run_cmd(link_cmd, f"Failed to link {bench_name}")
```

## Example: Adding CoreMark

(To be done, Work in Progress)
**Why CoreMark?** Based on our analysis, Embench-IoT benchmarks are too short (0.27-0.51s) for meaningful performance differentiation

- **Longer execution** (5-30 seconds) for better analysis
- **Industry standard** CPU benchmark
- **Better SimPoint viability** with more execution intervals

(WIP, add more comprehensive workloads)

---


With this framework, adding new workloads should be straightforward while maintaining compatibility with the existing analysis infrastructure.
Along with the .cfg files for each workload.
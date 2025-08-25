/* Unified main.c for all benchmark types with integrated board support */
#include <stdio.h>
#include <stdint.h>

#ifndef WARMUP_HEAT
#define WARMUP_HEAT 1
#endif

// Timing and board support
static unsigned long long start_time;

#define CORETIMETYPE unsigned long long
#define read_csr(reg) ({ unsigned long __tmp; \
    asm volatile ("csrr %0, " #reg : "=r"(__tmp)); \
    __tmp; })

// BBV and trace macros (QEMU uses different approach)
#ifdef BBV
// For QEMU, BBV is handled via plugin, no inline markers needed
#define START_BBV
#define STOP_BBV   asm volatile ("csrci 0x8c2, 1");  // Stop marker for consistency
#else
#define START_BBV
#define STOP_BBV
#endif

#ifdef TRACE
// QEMU tracing is handled via -d in_asm, no inline markers needed
#define START_TRACE
#define STOP_TRACE
#else
#define START_TRACE
#define STOP_TRACE
#endif

// Weak fallbacks - workload will override as needed
int __attribute__((weak)) main(void) { 
    printf("ERROR: Default main() called - workload should provide main() or benchmark()\n");
    return -1; 
}

int __attribute__((weak)) benchmark(void) { 
    return main(); 
}

void __attribute__((weak)) warm_caches(int x) { }
void __attribute__((weak)) initialise_benchmark(void) { }
int __attribute__((weak)) verify_benchmark(int x) { return 1; }

// Board support functions (formerly in boardsupport.c)
void __attribute__((noinline)) __attribute__((externally_visible))
start_trigger(void)
{
    unsigned long hi = read_csr(mcycleh);
    unsigned long lo = read_csr(mcycle);
    start_time = (unsigned long long)(((CORETIMETYPE)hi) << 32) | lo;
}

void __attribute__((noinline)) __attribute__((externally_visible))
stop_trigger(void)
{
    unsigned long hi = read_csr(mcycleh);
    unsigned long lo = read_csr(mcycle);
    unsigned long long end_time = (unsigned long long)(((CORETIMETYPE)hi) << 32) | lo;
    // Optional: printf("Cycle count: %llu\n", end_time - start_time);
}

void __attribute__((noinline))
initialise_board(void)
{
    // Board-specific initialization if needed
}

// Main entry point
int __attribute__((used))
env_main(int argc __attribute__((unused)), char *argv[] __attribute__((unused)))
{
    int result, correct;
    
    initialise_board();
    initialise_benchmark();
    warm_caches(WARMUP_HEAT);
    
    // Qemu does not support defining ROI.

    
    result = benchmark();

    
    correct = verify_benchmark(result);
    
    return (!correct);
}
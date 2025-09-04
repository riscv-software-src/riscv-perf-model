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

// BBV and trace macros
#ifdef BBV
#define START_BBV  asm volatile ("csrsi 0x8c2, 1");
#define STOP_BBV   asm volatile ("csrci 0x8c2, 1");
#else
#define START_BBV
#define STOP_BBV
#endif

#ifdef TRACE
#define START_TRACE   asm volatile ("xor x0, x0, x0");
#define STOP_TRACE    asm volatile ("xor x0, x1, x1");
#else
#define START_TRACE
#define STOP_TRACE
#endif

// Weak fallbacks - workload will override as needed
int __attribute__((weak)) main(void) { 
    printf("ERROR: Default main() called - workload should provide main() or benchmark()\n");
    return -1; 
}

int __attribute__((weak)) benchmark(void) {  //  Coremark main()
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
    
    START_BBV;
    START_TRACE;
    
    result = benchmark();
    
    STOP_TRACE;
    STOP_BBV;
    
    correct = verify_benchmark(result);
    
    return (!correct);
}
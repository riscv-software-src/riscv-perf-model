/* QEMU-specific util.c - No HTIF dependencies */
#include <stdint.h>
#include <stddef.h>
#include <string.h>

// QEMU test device for clean exits
#define QEMU_TEST_DEVICE_BASE 0x100000
volatile uint32_t *test_device = (volatile uint32_t *)QEMU_TEST_DEVICE_BASE;

// No tohost/fromhost for QEMU - use test device instead
void tohost_exit(uintptr_t code) {
    if (code == 0) {
        test_device[0] = 0x5555;  // Success
    } else {
        test_device[1] = (uint32_t)code;
        test_device[0] = 0x3333;  // Failure
    }
    while (1);  // Wait for QEMU to shut down
}

// Simple syscall implementation without HTIF polling
uintptr_t htif_syscall(uintptr_t arg) {
    uintptr_t magic_mem[8];
    magic_mem[0] = arg;
    
    long num = magic_mem[0] & 0xff;
    
    if (num == 64) {        // SYS_write`
        return magic_mem[3]; // Return length
    } else if (num == 93) { // SYS_exit
        tohost_exit(magic_mem[1]);
    }
    return 0;
}

// Override syscall to avoid HTIF
uintptr_t syscall(uintptr_t n, uintptr_t a0, uintptr_t a1, uintptr_t a2,
                  uintptr_t a3, uintptr_t a4, uintptr_t a5, uintptr_t a6) {
    if (n == 64) {      // SYS_write
        return a2;      // Return "bytes written"
    } else if (n == 93) { // SYS_exit
        tohost_exit(a0);
    }
    return 0;
}

// Provide minimal printf support without HTIF
void print_char(char c) {
    // For QEMU, we can either ignore or use semihosting
    // This avoids the HTIF polling loop
}

// Weak implementations of required functions
void __attribute__((weak)) initialise_benchmark(void) {}
void __attribute__((weak)) warm_caches(int heat) {}
int __attribute__((weak)) verify_benchmark(int result) { return 1; }
void __attribute__((weak)) setStats(int enable) {}
void __attribute__((weak)) start_trigger(void) {}
void __attribute__((weak)) stop_trigger(void) {}

// Standard library functions
/* why do we need this ? 
void *memset(void *s, int c, size_t n) {
    char *p = s;
    while (n--) *p++ = c;
    return s;
}

void *memcpy(void *dest, const void *src, size_t n) {
    char *d = dest;
    const char *s = src;
    while (n--) *d++ = *s++;
    return dest;
}

int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++; s2++;
    }
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

unsigned int strlen(const char *s) {
    unsigned int len = 0;
    while (*s++) len++;
    return len;
}


*/
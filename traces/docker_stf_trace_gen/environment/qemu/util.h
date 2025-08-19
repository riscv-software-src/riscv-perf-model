/* Copyright (C) lowRISC contributors

   This file is part of Embench.

   SPDX-License-Identifier: GPL-3.0-or-later OR Apache-2.0 */

#ifndef UTIL_H
#define UTIL_H

#include <stdint.h>
#include <stddef.h>

#define SYS_exit 93
#define SYS_write 64

// HTIF functions
uintptr_t syscall(uintptr_t n, uintptr_t a0, uintptr_t a1, uintptr_t a2,
                  uintptr_t a3, uintptr_t a4, uintptr_t a5, uintptr_t a6);
void shutdown(int code);
void print(const char *s);
void printn(const char *s, int len);
void tohost_exit(uintptr_t code);

// String functions
unsigned int strlen(const char *s);

#endif
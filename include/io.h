/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 KekkoTech Softwares Open Source (Matteo Checcacci)
 *
 * io.h — x86 port I/O access.
 *
 * On x86 the legacy peripherals are not memory mapped: they live in a
 * separate address space reachable only through the `in` and `out`
 * instructions. Getting at them from C takes inline assembly, declared
 * static inline so the compiler expands it in place instead of emitting a
 * function call.
 */

#ifndef IO_H
#define IO_H

#include <stdint.h>

static inline void outb(uint16_t port, uint8_t value)
{
    /* "a" puts the value in al, "Nd" the port in dx (or encodes it as an
     * immediate when it is a constant below 256). */
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port)
{
    uint8_t value;
    __asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

#endif /* IO_H */

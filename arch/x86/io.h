/* SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 KekkoTech Softwares (Matteo Checcacci)
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

/* An `out` retires as soon as the bus has taken the byte, while the device
 * may still be busy with it: old peripherals need the bus to idle in between
 * two accesses or they drop the second one. The 8259 interrupt controller is
 * the classic victim, which is why its init sequence is written one control
 * word at a time with this in the middle. Port 0x80 is the POST diagnostic
 * latch, unused once the BIOS has handed over, so writing to it is a harmless
 * way to spend one bus cycle doing nothing. */
static inline void io_wait(void)
{
    outb(0x80, 0);
}

#endif /* IO_H */

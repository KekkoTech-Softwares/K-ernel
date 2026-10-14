/* SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 KekkoTech Softwares (Matteo Checcacci)
 *
 * pit.h — the 8253/8254 programmable interval timer, channel 0.
 *
 * PRIVATE to arch/x86, like io.h, gdt.h, pic.h and idt.h: a divisor and a
 * port number mean nothing on another machine. The rest of the kernel asks
 * for a heartbeat through arch.h (arch_timer_init, arch_timer_ticks), and
 * pit.c is where that promise is kept on x86.
 */

#ifndef PIT_H
#define PIT_H

#include <stdint.h>

//IRQ 0 on the PC/AT: channel 0's output pin is wired straight to the first
//input of the master 8259. Channel 0 is the only one of the three that is
//ours — channel 1 refreshed DRAM and channel 2 drives the speaker.
#define PIT_IRQ 0

/* The oscillator all three channels count down from. The PC/AT feeds the
 * 8254 its 14.31818 MHz crystal divided by 12 — 1193181.66 Hz, rounded here
 * to the nearest integer. The odd crystal is the NTSC colour burst frequency
 * times four: the original IBM PC drove the timer and the video card from one
 * part, and every x86 since has kept the number for compatibility. */
#define PIT_BASE_HZ 1193182u

//The range arch.h promises its callers; arch_timer_init() panics outside it.
#define PIT_MIN_HZ 1
#define PIT_MAX_HZ 1000

/* The slowest rate this chip can actually produce. The divisor is 16 bits, so
 * the floor is 1193182/65535 = 18.2 Hz and no hz below 19 can be programmed
 * on a single channel. arch.h promises a wider range than the hardware has:
 * see the clamp in pit_divisor() and the warning in arch_timer_init() for
 * what a request below the floor really gets. */
#define PIT_HW_MIN_HZ 19

/* The value to load into channel 0 for a given rate, clamped to what the chip
 * can hold. Exposed because it is the only part of this driver a self test
 * can reach: the counter itself is driven by a clock the tests cannot see,
 * and a bad divisor is a bug that reads as "the clock is wrong", months
 * later. hz must already be inside [PIT_MIN_HZ, PIT_MAX_HZ] — zero divides by
 * zero, and a hz above the base frequency truncates to the 0 the chip reads
 * as 65536. */
uint16_t pit_divisor(unsigned hz);

//Self test suite for this module. Implemented in pit_test.c and handed to
//ktest by arch_init(), like the GDT, PIC and IDT ones.
void test_pit(void);

#endif //PIT_H

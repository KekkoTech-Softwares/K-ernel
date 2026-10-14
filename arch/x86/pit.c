/* SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 KekkoTech Softwares (Matteo Checcacci)
 *
 * pit.c — the kernel's heartbeat: channel 0 of the 8254, counted in ticks.
 *
 * This is the timer half of arch.h. Everything above it only ever asks "how
 * many ticks since you started", which is what makes sleeping, timeouts and
 * one day a preemptive scheduler possible.
 */

#include <stdint.h>

#include "arch.h"
#include "io.h"
#include "kprintf.h"
#include "panic.h"
#include "pit.h"

/* Port addresses from the PC/AT board, not from the chip: channel 0's counter
 * is decoded at 0x40 and the mode/command register, shared by all three
 * channels, at 0x43. Nothing here ever touches 0x41 or 0x42 — those belong to
 * the other two channels, and channel 2 is wired to the speaker. */
#define PIT_CH0_DATA 0x40
#define PIT_CMD      0x43

/* Control word 0x36, bit by bit (Intel 8254 datasheet, "Control Word
 * Format"): 00 selects counter 0, 11 says the 16-bit count arrives as low
 * byte then high byte, 011 is mode 3, and 0 means a binary count rather than
 * BCD.
 *
 * Mode 3 — square wave — rather than mode 2: both reload the counter on their
 * own, so the interrupt repeats without the kernel reprogramming anything,
 * and both give a period of exactly `divisor` input clocks. Mode 3 is what
 * every PC firmware leaves channel 0 in, so it is the path QEMU and real
 * chipsets are best tested on. */
#define PIT_CMD_CH0_MODE3 0x36

/* The tick count, as two halves instead of one uint64_t, both volatile.
 *
 * On a 32-bit machine a 64-bit increment is two instructions with a window
 * between them, and the writer here is an interrupt handler: a reader that
 * landed in that window would see a number that was never true — the low half
 * already wrapped to zero, the high half not yet carried. So the halves are
 * separate and arch_timer_ticks() re-reads the high one to notice the carry.
 *
 * volatile is not decoration: without it -O2 treats the second read of
 * ticks_hi as the same expression as the first, deletes it, and the retry
 * loop below quietly becomes straight-line code. */
static volatile uint32_t ticks_lo;
static volatile uint32_t ticks_hi;

/* One tick. Short on purpose: it runs with interrupts disabled, so whatever
 * it does is time nothing else on the machine gets. No printing from here —
 * at 100 Hz a single kprintf would bury the console and spend the whole tick
 * budget inside the serial port's polling loop. */
static void pit_tick(void * ctx) {
    (void)ctx; //the driver has no per-line state to carry

    //the carry costs one branch per tick and happens once every 497 days at
    //100 Hz, which is also why it will never be seen in a test.
    if (++ticks_lo == 0)
        ticks_hi++;
}

uint16_t pit_divisor(unsigned hz) {
    uint32_t div = PIT_BASE_HZ / hz;

    /* The chip latches 16 bits, so a slower rate than PIT_HW_MIN_HZ simply
     * cannot be expressed: truncating 1193182 to 16 bits would program 13534
     * — 88 Hz for a caller that asked for 1 Hz — and a divisor that happened
     * to truncate to 0 would be read by the 8254 as 65536. Clamping is the
     * honest answer: the slowest tick this hardware has, and arch_timer_init()
     * says so out loud. */
    if (div > 0xFFFF)
        return 0xFFFF;

    return (uint16_t)div;
}

void arch_timer_init(unsigned hz) {
    /* Boot only, once. The tick count has one writer, pit_tick(), and the
     * reset further down is the single exception — sound only because it
     * happens before the line has ever been opened. A second call with
     * interrupts enabled is the most natural thing in the world (change the
     * tick rate) and the most damaging: it would zero the count under the
     * handler's feet and under every deadline already computed from it, so
     * time would run backwards. Re-arming is not forbidden forever, it just
     * cannot be free — whoever needs it has to decide what becomes of the
     * ticks already counted, and this panic is what makes them decide. */
    static int armed;
    uint16_t div;

    if (armed)
        kpanic("arch_timer_init: already running, %u hz asked for a second time", hz);

    /* Out of range is a programming error, so it stops here rather than
     * reaching the chip. Zero is the one that matters: PIT_BASE_HZ / 0 is a
     * #DE on the spot, and a caller that computed its hz from something else
     * and got it wrong would otherwise end up with the divisor 0 the chip
     * reads as 65536 — a timer that looks installed and ticks eighteen times
     * a second. */
    if (hz < PIT_MIN_HZ || hz > PIT_MAX_HZ)
        kpanic("arch_timer_init: %u hz is outside [%u, %u]",
               hz, (unsigned)PIT_MIN_HZ, (unsigned)PIT_MAX_HZ);

    div = pit_divisor(hz);

    /* A rate below the hardware floor is legal per arch.h and impossible per
     * the datasheet, so it is rounded *up* — arch.h promises never slower
     * than asked — and said out loud. The divisor goes in the message with
     * the rate: 1193182/65535 is 18.2 and the integer division prints 18, so
     * the number alone would not add up for whoever recomputes it. */
    if (hz < PIT_HW_MIN_HZ)
        kwarn("pit: %u hz is slower than the 8254 can tick, using divisor %u (~%u hz)\n",
              hz, div, PIT_BASE_HZ / div);

    /* Command word first, then the count: the write order is not a style
     * choice, the control word is what tells the chip how many bytes to
     * expect and in which order. Low byte then high byte, because that is
     * what bits 5-4 of the word above selected; the new count takes effect
     * when the high byte lands, so writing them the other way round loads a
     * counter built from two unrelated halves.
     *
     * io_wait() in between for the same reason pic.c has it: these are three
     * back-to-back accesses to one ISA-era part, which needs the bus to idle
     * between two of them or it drops the second. QEMU does not care, a real
     * chipset does — and a dropped high byte is a counter off by a factor,
     * discovered as "the clock runs fast" on hardware nobody has to hand. */
    outb(PIT_CMD, PIT_CMD_CH0_MODE3);
    io_wait();
    outb(PIT_CH0_DATA, (uint8_t)(div & 0xFF));
    io_wait();
    outb(PIT_CH0_DATA, (uint8_t)(div >> 8));

    /* arch.h promises ticks counted "since arch_timer_init()", so the counter
     * restarts here — and it restarts *before* the line is opened, never
     * after, or the first tick to arrive would be thrown away by the reset. */
    ticks_lo = 0;
    ticks_hi = 0;

    //registering also unmasks IRQ 0, and that is the last thing to happen:
    //from this call on, the only thing between the chip and pit_tick() is the
    //sti the kernel has not run yet.
    arch_irq_register(PIT_IRQ, pit_tick, NULL);

    armed = 1;
}

uint64_t arch_timer_ticks(void) {
    uint32_t hi, lo;

    /* Read the high half, the low half, then the high half again: if the
     * carry happened in between, the two do not agree and the whole read is
     * taken again. No cli/sti around it on purpose — that would hand
     * interrupts back to a caller who had disabled them, and arch.h forbids
     * exactly that.
     *
     * What makes this enough, rather than merely likely: the writer is an
     * interrupt handler on a single CPU, entered through a gate that clears
     * IF, so it always runs to completion between two instructions of this
     * loop. It is never caught half way. The day handlers nest, or a second
     * CPU writes these, this needs rethinking — a sequence counter, or the
     * 64-bit atomic that a 32-bit x86 only has as cmpxchg8b. */
    do {
        hi = ticks_hi;
        lo = ticks_lo;
    } while (hi != ticks_hi);

    return ((uint64_t)hi << 32) | lo;
}

/* SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 KekkoTech Softwares (Matteo Checcacci)
 *
 * pit_test.c — self tests for the 8254 timer.
 *
 * Where this suite sits in the boot, because two checks below depend on it:
 * kernel_main calls arch_timer_init() *before* ktest_run() and
 * arch_irq_enable() *after* it. So the timer is fully set up here — the
 * divisor is in the chip and IRQ 0 is open — and still completely silent.
 * Move arch_timer_init() after ktest_run() and the mask check below turns
 * red, while the one on the tick count keeps passing for the wrong reason.
 *
 * Two things this suite cannot test, and where they are tested instead:
 *
 *   - the rate. ktest_run() runs with interrupts still disabled — nothing
 *     calls arch_irq_enable() before it, and idt_test.c depends on that too —
 *     so not one tick can arrive while these checks run. Whether the heart
 *     beats a hundred times a second is decided against a real clock, in
 *     kernel_main, after the sti.
 *   - the panic on a hz outside the range. A panic stops the machine for
 *     good, so a test can only cover the arithmetic behind the guard, which
 *     is what pit_divisor() is exposed for.
 *
 * What is left is that arithmetic, and it is where the expensive bug lives: a
 * divisor of 0 is read by the 8254 as 65536, its slowest rate, so a divisor
 * computed wrong does not fail — it ticks eighteen times a second and lets
 * every timeout in the kernel be five times too long.
 *
 * The suite changes no hardware state, so unlike pic_test.c it has nothing to
 * put back.
 */

#include <stdint.h>

#include "arch.h"
#include "ktest.h"
#include "pic.h"
#include "pit.h"

void test_pit(void) {
    int nonzero_ok = 1;

    /* The rate kernel_main asks for, worked out by hand: 1193182/100 =
     * 11931.82, truncated to 11931, which is 100.0068 Hz — 68 ppm fast, six
     * seconds of drift a day. A base frequency off by a factor (the 14.31818
     * MHz crystal instead of the divided-by-12 clock is the classic slip)
     * would land nowhere near this number. */
    KCHECK(pit_divisor(100) == 11931);
    KCHECK(PIT_BASE_HZ / pit_divisor(100) == 100);

    //The top of the range arch.h promises is plain division, and has to stay
    //well inside the 16 bits the chip latches.
    KCHECK(pit_divisor(PIT_MAX_HZ) == 1193);

    /* The bottom is the one that bites. 1193182 does not fit in 16 bits, so
     * 1 Hz is promised by arch.h and unreachable on this chip: pit_divisor()
     * must clamp to the slowest it can do instead of truncating — truncation
     * would program 13534, that is 88 Hz, for a caller that asked for one
     * tick a second. */
    KCHECK(pit_divisor(PIT_MIN_HZ) == 0xFFFF);

    //...and the floor is where it is claimed to be: one hz above it fits, one
    //below it does not. Both sides, or the constant could be off by any
    //amount and still look right.
    KCHECK(pit_divisor(PIT_HW_MIN_HZ) < 0xFFFF);
    KCHECK(pit_divisor(PIT_HW_MIN_HZ - 1) == 0xFFFF);

    /* Nothing in between may come out as the 0 the chip reads as 65536. The
     * whole promised range rather than a handful of samples: the division is
     * cheap, the bug it guards against is not — and a sample would miss
     * whichever hz a future rounding change happens to break. */
    for (unsigned hz = PIT_MIN_HZ; hz <= PIT_MAX_HZ; hz++)
        if (pit_divisor(hz) == 0)
            nonzero_ok = 0;

    KCHECK(nonzero_ok);

    /* The counter is back to zero, which is all this boot can honestly say
     * about it: nothing has incremented it yet, so a non-zero here means
     * arch_timer_init() did not reset it — ticks counted "since init" would
     * then start from whatever the previous run left behind.
     *
     * Worth writing down what this does *not* cover, because it looks like it
     * does: with both halves at zero, any way of assembling them gives zero,
     * so neither the shift nor the retry loop in arch_timer_ticks() is
     * falsifiable here — two reads with IF clear agree even if the function
     * is wrong in every other respect. The only real check of both is the
     * live counter after the sti, in kernel_main. */
    KCHECK(arch_timer_ticks() == 0);

    /* IRQ 0 is open, and that bit is also the proof the handler was
     * registered: pic_remap() closes all sixteen lines and the only thing
     * that reopens one is arch_irq_register() with a non-NULL handler — a
     * NULL one masks the line again. A set bit here means the chip was
     * programmed and then never connected to anything: the ticks would stay
     * at zero for the rest of the boot, and every wait in the kernel would
     * hang forever. */
    KCHECK((pic_get_mask() & (1u << PIT_IRQ)) == 0);
}

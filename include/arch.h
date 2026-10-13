/* SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 KekkoTech Softwares (Matteo Checcacci)
 *
 * arch.h — the machine-dependent entry point.
 *
 * Every architecture implements arch_init() in its own arch/<arch>/arch.c.
 * What it does inside is nobody else's business: on x86 it is the GDT, on
 * another target it will be something else entirely.
 */

#ifndef ARCH_H
#define ARCH_H

#include <stdint.h>

void arch_init(void);

/* --- Interrupts -----------------------------------------------------------
 *
 * An interrupt is a hardware signal that stops the CPU to make it run a
 * function. Every architecture gets there its own way: x86 has the IDT and
 * the 8259 PIC, ARM an exception vector and the GIC, RISC-V traps and a PLIC.
 * None of that belongs up here — generic code only ever says "run this when
 * that line fires", "not now", "done".
 *
 * The irq number is a logical line whose numbering is **defined by the
 * architecture**: generic code passes along a number it was given, never a
 * literal. (On x86 line 0 is the timer and 1 the keyboard — and that is an
 * x86 detail, not a promise this interface makes.)
 *
 * A handler runs with interrupts disabled, so it must not block, must not
 * wait for another interrupt, and must be short. It does not see the
 * interrupted frame: changing where the CPU resumes is a different job, and
 * it has its own interface. For the same reason the phase 5 scheduler will
 * NOT hook the timer through arch_irq_register(): the architecture's own
 * timer handler will call into it, and switching context stays with
 * arch_context_switch().
 */

void arch_irq_enable(void);   /* lets interrupts through again */
void arch_irq_disable(void);  /* blocks them: a critical section */

/* Installs handler for one line and unmasks it. ctx is handed back to the
 * handler untouched, so a driver can pass its own state instead of reaching
 * for a global; it is stored before the line is unmasked, so the handler can
 * never run against half-initialised state. A NULL handler masks the line
 * again. An irq the machine does not have is a programming error, not a
 * runtime condition: it panics rather than writing to a random controller. */
void arch_irq_register(unsigned irq, void (*handler)(void *), void *ctx);

/* "I am done with this line", towards the interrupt controller. The
 * architecture already sends it once the handler returns, so calling this is
 * redundant today — it exists for the handler that will want to acknowledge
 * early and keep working. Until handlers can nest, do not call it from one:
 * two acknowledgements for one interrupt are harmless only as long as no
 * other line is in service. */
void arch_irq_eoi(unsigned irq);

/* --- Periodic timer -------------------------------------------------------
 *
 * The heartbeat every kernel needs: it is what makes sleeping, timeouts and
 * one day preemptive scheduling possible.
 */

/* Starts ticking hz times per second. hz must be in [1, 1000]: zero has no
 * meaning and the hardware divisor has a floor, so a value outside the range
 * is a programming error, same as a bad irq. */
void arch_timer_init(unsigned hz);

/* Ticks since arch_timer_init(). Safe to call with interrupts enabled: the
 * implementation must not hand back a value torn between the low and the high
 * half, and must not silently re-enable interrupts for a caller that had
 * disabled them. */
uint64_t arch_timer_ticks(void);

/* --- Stopping -------------------------------------------------------------
 *
 * Stops the CPU until the next interrupt arrives. An empty loop would burn a
 * core for nothing; on a laptop it would also burn the battery. With
 * interrupts disabled it never comes back, which is exactly what a panic
 * wants. */
void arch_halt(void);

#endif //ARCH_H

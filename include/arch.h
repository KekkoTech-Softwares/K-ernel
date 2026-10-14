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

/* Starts ticking hz times per second. Requires arch_init(): it opens an
 * interrupt line, which before then would mean a controller nobody has
 * programmed and no table to dispatch through — the first tick would be a
 * triple fault. Call it once, at boot: changing the rate afterwards is not
 * supported today, and a second call panics rather than let time appear to
 * jump backwards for whoever already read the counter.
 *
 * [1, 1000] is what a caller may *ask for*; outside it is a programming
 * error, same as a bad irq, and panics. Inside it, what the caller gets is
 * **the closest rate that is not slower** than the one asked for: the
 * direction is the whole point, because rounding up makes every timeout built
 * on the tick expire early rather than late, and nothing waits forever.
 *
 * Only a rate the machine cannot produce *at all* — below its own floor —
 * reaches the log. The rounding inside the range does not: at 100 Hz the
 * x86 divisor gives 100.0068 Hz, an error of 68 parts per million, and a
 * warning on every boot about that would train everyone to ignore warnings.
 *
 * Why not promise the exact number: on x86 the 8254 takes a 16-bit divisor,
 * so it cannot tick slower than 18.2 times a second — 1 Hz is honoured as
 * 18.2 Hz. That is one chip's limit, not an interface's: ARM and RISC-V
 * timers really do 1 Hz, and carving a 1981 part's floor into a portable
 * header would be the wrong trade.
 *
 * The day a caller computes a deadline from the hz it asked for, the
 * difference has to become visible in *code* and not only in the log — a
 * return value with the real rate, or an arch_timer_hz(). Not today: nothing
 * reads it, and an interface with no caller is a guess about the future. */
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

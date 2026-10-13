/* SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 KekkoTech Softwares (Matteo Checcacci)
 *
 * pic.h — the pair of 8259A programmable interrupt controllers.
 *
 * PRIVATE to arch/x86, like io.h and gdt.h: generic code must never include
 * this. How a device interrupt reaches the CPU is a machine detail, and the
 * bridge for the rest of the kernel stays arch.h.
 *
 * Why remapping is not optional: out of reset the master 8259 raises vectors
 * 8-15 and the slave 0x70-0x77, but the CPU has already reserved 0-31 for its
 * own exceptions (Intel SDM Vol.3, "Exception and Interrupt Vectors") —
 * vector 8 is #DF, 13 is #GP. Without a remap a timer tick and a double
 * fault arrive at the same handler, and there is no way to tell them apart.
 */

#ifndef PIC_H
#define PIC_H

#include <stdint.h>

//First vector the IRQs land on after the remap: 32 is the lowest one the CPU
//does not reserve. IRQ n becomes vector PIC_VECTOR_BASE + n.
#define PIC_VECTOR_BASE 32
#define PIC_IRQ_COUNT   16

//The master pin the slave is cascaded on, fixed by the PC/AT wiring. Both
//chips are told about it in ICW3, and the master has to keep it unmasked for
//any slave line to be heard — so pic_unmask() opens it on its own.
#define PIC_CASCADE_IRQ 2

/* The lowest priority line of each chip. When a chip asserts its pin and then
 * finds nothing in service by the time the CPU acknowledges — a noise pulse,
 * or a line that dropped too early — it has to answer with some vector, and
 * answers with this one. Hence "spurious IRQ 7" and "spurious IRQ 15". */
#define PIC_SPURIOUS_MASTER 7
#define PIC_SPURIOUS_SLAVE  15

void pic_remap(void);
void pic_mask(unsigned irq);
void pic_unmask(unsigned irq);

/* Acknowledges the interrupt being serviced, which the chip needs before it
 * will deliver that line (or any lower priority one) again. Knows the
 * spurious rule on its own: on a spurious IRQ 7 it sends nothing, on a
 * spurious IRQ 15 only the master is told, because only the master really
 * latched the cascade. The dispatcher may call this for every IRQ it took,
 * spurious included, and must not second guess it. */
void pic_eoi(unsigned irq);

/* True when this IRQ was never actually raised by a device: the chip reports
 * its lowest priority line but leaves the matching in-service bit clear.
 * Only IRQ 7 and 15 can be spurious; anything else is always false.
 *
 * Today the rule lives inside pic_eoi() alone, which is why nothing calls
 * this yet: pic_eoi() decides what to *acknowledge*. Deciding whether to run
 * the handler at all is the dispatcher's job and it is still open (K-012):
 * until some handler re-enables interrupts, servicing a line that never
 * fired costs nothing but a wasted call. */
int pic_is_spurious(unsigned irq);

/* The in-service register of both chips as one word: master in the low byte,
 * slave in the high one, a set bit meaning "this line was dispatched and has
 * not been acknowledged yet". Read by selecting it through OCW3.
 *
 * Two uses: telling a spurious interrupt from a real one (see above), and
 * catching a handler that returned without an EOI — the bit stays set, and
 * that line is silently dead from then on. */
uint16_t pic_read_isr(void);

//The 16 interrupt masks as one word: master in the low byte, slave in the
//high one, a set bit meaning the line is disabled. For the self tests, which
//have to read what the chip holds instead of trusting what we wrote into it.
uint16_t pic_get_mask(void);

//Self test suite for this module. Implemented in pic_test.c and handed to
//ktest by arch_init(), like the GDT one.
void test_pic(void);

#endif //PIC_H

/* SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 KekkoTech Softwares (Matteo Checcacci)
 *
 * idt.h — Interrupt Descriptor Table: where the CPU goes when something
 *         interrupts it, be it a fault of its own or a device.
 *
 * PRIVATE to arch/x86, like io.h, gdt.h and pic.h: generic code never sees a
 * register frame of a particular machine. What it does see is arch.h, whose
 * interrupt half is implemented in idt.c.
 */

#ifndef IDT_H
#define IDT_H

#include <stdint.h>

//The table is always 256 entries: the vector number is a byte, and the CPU
//indexes with it without ever consulting a count of its own.
#define IDT_ENTRIES 256

//The vectors that have a stub behind them: the 32 the CPU reserves for its
//exceptions, plus the 16 the PIC was remapped onto. Everything above is left
//not present, so an int 0x80 today faults instead of jumping into nothing.
#define IDT_STUB_COUNT 48

/* Gate type and attribute byte, 0x8E: present, DPL 0, 32-bit interrupt gate
 * (Intel SDM Vol.3, "IDT Descriptors").
 *
 * DPL 0 on every gate, including the trap-like ones. The DPL of a gate is
 * only checked when software executes `int n`, never when the CPU itself
 * dispatches — so ring 0 can still run `int3` from a self test, while ring 3
 * cannot hand-invoke a page fault handler and lie about the frame.
 *
 * Interrupt gate rather than trap gate: entering through it clears IF, so a
 * handler starts with interrupts off instead of having to race to disable
 * them. */
#define IDT_GATE_INT32 0x8E

//one 8 byte gate. The handler address is split in two halves with the
//selector and the flags wedged in between, same 80286 heritage as the GDT.
struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  zero;
    uint8_t  flags;
    uint16_t offset_high;
} __attribute__((packed));

//what lidt wants, exactly like gdt_ptr: (size of table) - 1, then its
//address. packed for the same reason — without it base would align to 4 and
//sidt would write it where C does not read it.
struct idt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

/* The frame the stubs in isr.s build, read from the lowest address up: four
 * segment registers, then what pusha left, then the two words the stub and
 * the CPU put there, then the CPU's own return frame.
 *
 * The segment fields are uint32_t and not uint16_t: `push ds` in 32-bit mode
 * moves esp by four, but the CPU only writes the low half, so the top 16 bits
 * are whatever was on the stack before. Mask with 0xFFFF before printing or
 * comparing one.
 *
 * esp_dummy is the esp pusha recorded for itself, not the esp of the
 * interrupted code — it is useless and only exists because popa expects the
 * slot. user_esp and ss are different: the CPU pushes them *only* when the
 * interrupt crossed a privilege boundary, so reading them without checking
 * (cs & 3) first means reading past the frame. */
struct regs {
    uint32_t gs, fs, es, ds;
    uint32_t edi, esi, ebp, esp_dummy, ebx, edx, ecx, eax;
    uint32_t vector, err_code;
    uint32_t eip, cs, eflags;
    uint32_t user_esp, ss;
};

void idt_init(void);

/* Takes over one of the 32 exception vectors. Needed by the self tests, and
 * from phase 4 on by the page fault handler, which is not a failure at all
 * but the entry point of demand paging. NULL puts the default back, that is
 * the register dump followed by a panic. A vector that is not an exception
 * is a programming error and panics. */
void idt_set_exception_handler(unsigned vector, void (*fn)(struct regs *));

//What the CPU really holds, as opposed to what idt.c wrote into memory. Same
//reasoning as gdt_read_gdtr(): a table can be built perfectly and never
//reach the hardware.
void idt_read_idtr(struct idt_ptr * out);

//The stub addresses, built by isr.s next to the stubs themselves. The self
//tests read it to check that each gate really points at its own stub.
extern void * const isr_stub_table[];

//Self test suite for this module. Implemented in idt_test.c and handed to
//ktest by arch_init(), like the GDT and PIC ones.
void test_idt(void);

#endif //IDT_H

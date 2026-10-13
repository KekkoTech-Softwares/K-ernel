/* SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 KekkoTech Softwares (Matteo Checcacci)
 *
 * idt_test.c — self tests for the IDT and the exception stubs.
 *
 * The frame built by isr.s is the most fragile structure of this phase: every
 * mistake in it is silent. A gate with the two halves of its offset swapped,
 * a struct whose fields do not line up with the pushes, one forgotten `add
 * esp` on the way out — none of that produces a message, it produces a jump
 * to a random address, a double fault with no handler, and a reset. Exactly
 * the shape of the 108 byte TSS bug, so it is checked the same way: against
 * what the hardware holds, not against what the code meant to write.
 *
 * The suite runs with interrupts still disabled (nothing calls
 * arch_irq_enable() before ktest_run(), and that must stay true): the only
 * interrupt it takes is the int3 it fires itself.
 */

#include <stddef.h>
#include <stdint.h>

#include "gdt.h"
#include "idt.h"
#include "kprintf.h"
#include "ktest.h"

/* Same as KCHECK, and it also remembers whether everything so far has passed.
 * The int3 at the end is the one test that can take the machine down with it,
 * so it must not run on an IDT that is already known to be wrong. Calls
 * ktest_check() directly rather than KCHECK so that a failure still prints
 * the condition as it was written here. */
#define IDT_CHECK(cond)                                         \
    do {                                                        \
        int ok_ = (cond) ? 1 : 0;                               \
        all_ok &= ok_;                                          \
        ktest_check(ok_, __FILE__, __LINE__, #cond);            \
    } while (0)

/* What the breakpoint handler saw. volatile because the compiler has no idea
 * that an `int3` in an asm statement can reach this file: without it -O2 is
 * free to keep bp_hits in a register across the asm, and the check below
 * would read a stale zero and fail for no reason. */
static volatile int bp_hits;
static volatile uint32_t bp_vector, bp_err, bp_cs, bp_eip;

static void bp_handler(struct regs * r) {
    bp_hits++;

    bp_vector = r->vector;
    bp_err    = r->err_code;
    bp_cs     = r->cs;
    bp_eip    = r->eip;
}

void test_idt(void) {
    struct idt_ptr idtr;
    const struct idt_entry * gates;
    int all_ok = 1;
    int present_ok = 1, selector_ok = 1, target_ok = 1, absent_ok = 1;

    /* --- layout ---------------------------------------------------------
     *
     * struct regs mirrors the pushes in isr.s, so its size is arithmetic that
     * has to come out exactly: 4 segment registers (16) + pusha (32) + vector
     * and error code (8) + eip, cs, eflags (12) + the user esp and ss the CPU
     * only pushes on a privilege change (8) = 76. err_code therefore sits at
     * 16 + 32 + 4 = 52, and the CPU's own frame starts right after it. */
    IDT_CHECK(sizeof(struct idt_entry) == 8);
    IDT_CHECK(sizeof(struct idt_ptr) == 6);
    IDT_CHECK(sizeof(struct regs) == 76);
    IDT_CHECK(offsetof(struct regs, err_code) == 52);
    IDT_CHECK(offsetof(struct regs, eip) == 56);

    /* --- what the CPU loaded --------------------------------------------
     *
     * Read through the IDTR and not through the static array: a table can be
     * built perfectly and never reach the hardware, and then every check
     * below would be testing memory nobody uses.
     */
    idt_read_idtr(&idtr);

    IDT_CHECK(idtr.limit == IDT_ENTRIES * sizeof(struct idt_entry) - 1);

    //with a limit that wrong, base is not to be trusted either: reading 2 KiB
    //through it would be reading somebody else's memory, or MMIO.
    if (!all_ok)
        return;

    gates = (const struct idt_entry *)idtr.base;

    for (unsigned v = 0; v < IDT_STUB_COUNT; v++) {
        uint32_t target = gates[v].offset_low | ((uint32_t)gates[v].offset_high << 16);

        //present, DPL 0, 32-bit interrupt gate — all three in one byte
        if (gates[v].flags != IDT_GATE_INT32)
            present_ok = 0;

        //a handler reached through the user code segment would run at ring 3
        if (gates[v].selector != GDT_KERNEL_CODE)
            selector_ok = 0;

        /* Each gate must point at *its own* stub: this is what catches the
         * two halves of the offset being written the wrong way round, which
         * otherwise looks like a perfectly valid gate to a wrong address. */
        if (target != (uint32_t)isr_stub_table[v])
            target_ok = 0;
    }

    IDT_CHECK(present_ok);
    IDT_CHECK(selector_ok);
    IDT_CHECK(target_ok);

    /* The vectors with no stub must stay absent. A present bit left set there
     * would send an `int 0x80` into whatever address the zeroed offset points
     * at instead of raising #GP. */
    for (unsigned v = IDT_STUB_COUNT; v < IDT_ENTRIES; v++)
        if (gates[v].flags & 0x80)
            absent_ok = 0;

    IDT_CHECK(absent_ok);

    //vector 3 on its own, because it is the one fired below
    IDT_CHECK(gates[3].flags == IDT_GATE_INT32 &&
              gates[3].selector == GDT_KERNEL_CODE);

    /* --- the live round trip --------------------------------------------
     *
     * Everything above reads tables. This is the only check that proves the
     * whole path works: gate, stub, dispatcher, and the return through iret.
     * If the exit sequence in isr.s is unbalanced the machine does not fail
     * this check, it reboots — which is why it goes last and only runs on an
     * IDT that already checked out.
     */
    if (!all_ok) {
        kwarn("\nidt: table is wrong, skipping the int3 round trip\n");
        return;
    }

    idt_set_exception_handler(3, bp_handler);

    //"memory" so the compiler keeps bp_hits in memory across the trap
    __asm__ volatile("int3" ::: "memory");

    idt_set_exception_handler(3, NULL);

    //exactly once: a stub that jumped back to its own entry point, or an iret
    //returning to the int3 instead of past it, would hit this twice or loop
    KCHECK(bp_hits == 1);

    KCHECK(bp_vector == 3);

    //int3 is one of the vectors the CPU pushes nothing for, so this zero is
    //the one the stub invented: it proves the two frames really are uniform
    KCHECK(bp_err == 0);

    KCHECK((bp_cs & 0xFFFF) == GDT_KERNEL_CODE);

    //the address the handler would return to; zero would mean the frame was
    //read at the wrong offset
    KCHECK(bp_eip != 0);
}

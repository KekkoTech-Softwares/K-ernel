/* SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 KekkoTech Softwares (Matteo Checcacci)
 *
 * arch.c — x86 implementation of the generic arch_init().
 */

#include "arch.h"
#include "gdt.h"
#include "idt.h"
#include "ktest.h"
#include "pic.h"
#include "pit.h"

void arch_init(void) {
    gdt_init();

    /* The PIC before the IDT, and not the other way round: by the time a gate
     * exists, every line that could reach it is already masked and already
     * remapped. Nothing can fire in between — Multiboot hands over with
     * interrupts disabled and nobody has run sti yet — but out of reset the
     * controller raises vectors 8-15, which are exception vectors, so the
     * order is the difference between a wrong gate and no gate. */
    pic_remap();
    idt_init();

    //ktest_run() runs later, in kernel_main: registering the suite here is
    //enough, and keeps the machine specific tests out of kernel/ktest.c.
    ktest_register("gdt", test_gdt);
    ktest_register("pic", test_pic);
    ktest_register("idt", test_idt);
    ktest_register("pit", test_pit);
}

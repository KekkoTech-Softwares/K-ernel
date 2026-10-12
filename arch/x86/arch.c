/* SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 KekkoTech Softwares Open Source Project (Matteo Checcacci)
 *
 * arch.c — x86 implementation of the generic arch_init().
 */

#include "arch.h"
#include "gdt.h"
#include "ktest.h"

void arch_init(void) {
    gdt_init();

    //ktest_run() runs later, in kernel_main: registering the suite here is
    //enough, and keeps the machine specific tests out of kernel/ktest.c.
    ktest_register("gdt", test_gdt);
}
/* SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 KekkoTech Softwares Open Source Project (Matteo Checcacci)
 *
 * arch.c — x86 implementation of the generic arch_init().
 */

#include "arch.h"
#include "gdt.h"

void arch_init(void) {
    gdt_init();
}
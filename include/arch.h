/* SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 KekkoTech Softwares Open Source Project (Matteo Checcacci)
 *
 * arch.h — the machine-dependent entry point.
 *
 * Every architecture implements arch_init() in its own arch/<arch>/arch.c.
 * What it does inside is nobody else's business: on x86 it is the GDT, on
 * another target it will be something else entirely.
 */

#ifndef ARCH_H
#define ARCH_H

void arch_init(void);

#endif //ARCH_H
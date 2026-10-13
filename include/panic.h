/* SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 KekkoTech Softwares (Matteo Checcacci)
 *
 * panic.h — the kernel's last words.
 *
 * In user space a fatal error kills one process and the system shrugs. Here
 * there is nothing underneath: when an invariant is broken, continuing means
 * corrupting whatever still works. So the kernel says why, as loudly as it
 * can, and stops.
 */

#ifndef PANIC_H
#define PANIC_H

/* Prints the message on every output channel and stops the machine for good.
 * Marked noreturn, so the caller needs no fallback path after it — and the
 * compiler knows the code below is unreachable.
 *
 * Machine-specific details (a register dump, the faulting address) are printed
 * by the architecture *before* calling this: eax does not exist on ARM. */
void kpanic(const char * fmt, ...) __attribute__((noreturn));

#endif //PANIC_H

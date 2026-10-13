/* SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 KekkoTech Softwares (Matteo Checcacci)
 *
 * panic.c — print why, then stop.
 */

#include <stdarg.h>

#include "panic.h"
#include "arch.h"
#include "kprintf.h"
#include "vga.h"

/* A fault while printing the panic would re-enter here, and the second pass
 * would fault again: stack growing, no double fault handler, triple fault and
 * a silent reset — precisely what a panic message exists to prevent. The
 * second entry says nothing and stops. */
static int in_panic;

void kpanic(const char * fmt, ...) {
    va_list args;

    /* Interrupts first: a pending IRQ arriving in the middle of the message
     * would interleave its own output, and there is nothing left worth
     * servicing anyway. */
    arch_irq_disable();

    if (in_panic)
        for (;;)
            arch_halt();

    in_panic = 1;

    /* The last words go everywhere, whatever the caller had selected — and
     * not into a ksnprintf buffer, which is where they would end up if the
     * panic happened inside one. */
    kout_detach_sink();
    kout_set(KOUT_ALL);

    kset_color(VGA_LIGHT_RED, VGA_BLACK);
    kputs("\nKERNEL PANIC: ");

    va_start(args, fmt);
    kvprintf(fmt, args);
    va_end(args);

    kputchar('\n');

    /* hlt parks the CPU instead of spinning. The loop is for the one thing
     * that can still wake it: a non-maskable interrupt. */
    for (;;)
        arch_halt();
}

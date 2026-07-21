/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 KekkoTech Softwares Open Source (Matteo Checcacci)
 *
 * kprintf.c — formatted kernel output.
 * All the kernel's output passes from kputchar, only point who knows 
 * which channel is active.
 */

#include <stddef.h>

#include "kprintf.h"
#include "serial.h"
#include "vga.h"

void kputchar(char c) {
    vga_putchar(c);
    serial_putchar(c);
}

void kputs(const char *str) {
    for(size_t i = 0; str[i]; i++)
        kputchar(str[i]);
}
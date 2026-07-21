/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 KekkoTech Softwares Open Source (Matteo Checcacci)
 *
 * kernel.c — the kernel's C entry point.
 *
 * Reached from `_start` (boot.s) once GRUB has loaded the kernel at 1 MiB and
 * switched the CPU to protected mode. C here is "freestanding": no libc, no
 * printf, no malloc, no operating system underneath. Anything used has to be
 * written first.
 */

#include <stdint.h>

#include "kprintf.h"
#include "serial.h"
#include "version.h"
#include "vga.h"

/* The value GRUB leaves in eax to signal "I loaded you, through Multiboot".
 * Note this differs from the header magic (0x1BADB002) that GRUB looks for
 * inside the kernel binary. */
#define MULTIBOOT_BOOTLOADER_MAGIC 0x2BADB002

/* First piece of the future mini-library: prints a uint32 in hexadecimal.
 * Needed already to show the magic number handed over by GRUB. */
static void kput_hex(uint32_t value)
{
    static const char digits[] = "0123456789ABCDEF";
    char buf[11];

    buf[0] = '0';
    buf[1] = 'x';
    for (int i = 0; i < 8; i++)
        buf[2 + i] = digits[(value >> ((7 - i) * 4)) & 0xF];
    buf[10] = '\0';

    kputs(buf);
}

void kernel_main(uint32_t magic, uint32_t *mb_info)
{
    (void)mb_info; /* I'll need this memory map during phase 4 (PMM) */

    serial_init();
    vga_init();
    kputs("kputchar working pretty well\n");

    /* Every piece here is a string literal, so the compiler concatenates
     * them into a single constant: no formatting needed at runtime. */
    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    kputs(KERNEL_NAME " v" KERNEL_VERSION_STRING "\n");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    kputs(KERNEL_DESCRIPTION "\n");
    kputs("Phase 1: booted through GRUB/Multiboot.\n\n");

    kputs("Multiboot magic: ");
    kput_hex(magic);

    if (magic == MULTIBOOT_BOOTLOADER_MAGIC) {
        vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
        kputs("  [OK]\n");
    } else {
        /* A mismatching magic means the kernel was not loaded by a Multiboot
         * bootloader, so mb_info is not trustworthy and must not be used. */
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        kputs("  [EXPECTED 0x2BADB002]\n");
    }

    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    kputs("\nNext step: GDT (phase 2).\n");

    /* Nothing left to schedule, so halt the CPU. `hlt` parks it until the
     * next interrupt instead of burning a core on an empty loop. */
    for (;;)
        __asm__ volatile("hlt");
}

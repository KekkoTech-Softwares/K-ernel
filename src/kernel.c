/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 KekkoTech Softwares Open Source Project (Matteo Checcacci)
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
#include "ktest.h"
#include "string.h"
#include "serial.h"
#include "version.h"
#include "vga.h"

/* The value GRUB leaves in eax to signal "I loaded you, through Multiboot".
 * Note this differs from the header magic (0x1BADB002) that GRUB looks for
 * inside the kernel binary. */
#define MULTIBOOT_BOOTLOADER_MAGIC 0x2BADB002

void kernel_main(uint32_t magic, uint32_t *mb_info)
{
    (void)mb_info; /* I'll need this memory map during phase 4 (PMM) */
    serial_init();
    vga_init();

    kinfo("K-ernel is starting...\n");

    kprintf("\n");
    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    kprintf(KERNEL_NAME " v" KERNEL_VERSION_STRING "\n");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    kprintf(KERNEL_DESCRIPTION "\n");
    kprintf("\n\n");

    // --- Ker Test ---
    kinfo("K-ernel self-test is starting...\n");
    unsigned int saved = kout_get(); //save status
    kout_set(KOUT_SERIAL); //switch to serial
    ktest_run();
    kout_set(saved); //back as it was (saved status)


    kprintf("\n\n");

    kprintf("Booted through GRUB/Multiboot.\n\n");

    kprintf("Multiboot magic: 0x%08X", magic);

    if (magic == MULTIBOOT_BOOTLOADER_MAGIC) {
        vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
        kprintf("  [OK]\n");
    } else {
        /* A mismatching magic means the kernel was not loaded by a Multiboot
         * bootloader, so mb_info is not trustworthy and must not be used. */
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        kprintf("  [EXPECTED 0x2BADB002]\n");
    }

    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);

    /* Nothing left to schedule, so halt the CPU. `hlt` parks it until the
     * next interrupt instead of burning a core on an empty loop. */
    for (;;)
        __asm__ volatile("hlt");
}

/* SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 KekkoTech Softwares (Matteo Checcacci)
 *
 * kernel.c — the kernel's C entry point.
 *
 * Reached from `_start` (boot.s) once GRUB has loaded the kernel at 1 MiB and
 * switched the CPU to protected mode. C here is "freestanding": no libc, no
 * printf, no malloc, no operating system underneath. Anything used has to be
 * written first.
 */

#include <stdint.h>

#include "arch.h"
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

/* The heartbeat: 100 ticks a second, so one tick is 10 ms. Fine enough for
 * timeouts and for the scheduler slice of phase 5, coarse enough that the
 * handler costs nothing measurable. The number lives here and not in the
 * driver because it is a policy, not a property of the chip. */
#define TIMER_HZ 100

void kernel_main(uint32_t magic, uint32_t *mb_info)
{
    (void)mb_info; /* I'll need this memory map during phase 4 (PMM) */
    serial_init();
    vga_init();
    arch_init();

    /* The timer is programmed here, with interrupts still disabled: the chip
     * starts counting and IRQ 0 is unmasked, but nothing is delivered until
     * the sti much further down. That is deliberate — the self tests can then
     * check how the timer was set up without any suite having to survive a
     * tick arriving in the middle of it. */
    arch_timer_init(TIMER_HZ);

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

    /* Interrupts on, at last. Everything that can take one is installed: the
     * gates, the controller, and the one handler that has an owner. */
    kprintf("\n");
    arch_irq_enable();
    kinfo("interrupts enabled, timer at %u Hz\n", TIMER_HZ);

    /* The heartbeat has to be seen beating once, or "the timer is installed"
     * stays a claim nobody checked: the chip can be programmed perfectly and
     * the tick still never arrive — a line left masked, a gate pointing
     * elsewhere, a handler registered on another irq. One tick is 10 ms, so
     * the proof costs less than the line that prints it, and when it fails it
     * fails where it can be read: the boot stops here, under the message
     * above, instead of going on as if the kernel had a clock.
     *
     * The *rate* is a different question and is not asked here. Measuring it
     * needs a clock outside the machine, so it was measured once by hand
     * against the host's — roughly 3900 ticks in 39 s, so about 100 Hz, which
     * is as much precision as timing a serial line through an emulator can
     * honestly claim — and that is not something every boot has to redo.
     *
     * The first number printed below is rarely 0, and that is not an
     * off-by-one in the counter: the line above spends more than one tick
     * period polling the serial port at 38400 baud, so a tick usually lands
     * before start is even read. What is being proved is that the value
     * *changes*, not where it starts from. */
    uint64_t start = arch_timer_ticks();

    while (arch_timer_ticks() == start)
        arch_halt();

    kinfo("timer is ticking: %u -> %u\n",
          (unsigned)start, (unsigned)arch_timer_ticks());

    /* Nothing left to schedule, so halt the CPU: arch_halt() parks it until
     * the next interrupt instead of burning a core on an empty loop. How it
     * does that is the architecture's business — this was the last line of
     * assembly left in generic code. */
    for (;;)
        arch_halt();
}

/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 KekkoTech Softwares Open Source (Matteo Checcacci)
 *
 * linker.ld — where each part of the kernel ends up in memory.
 *
 * The kernel is loaded at 1 MiB. Below that mark low memory is crowded with
 * legacy structures (BIOS interrupt vector table, BIOS data area, video
 * buffers, option ROMs), so 1 MiB is the first genuinely free address and
 * the usual convention on x86.
 */

ENTRY(_start)

SECTIONS
{
    . = 1M;

    /* kernel_start and kernel_end will let the Physical Memory Manager
     * (phase 4) mark the frames occupied by the kernel itself as used. */
    kernel_start = .;

    /* .multiboot comes first: the header must sit within the first 8 KiB. */
    .text BLOCK(4K) : ALIGN(4K)
    {
        *(.multiboot)
        *(.text)
    }

    .rodata BLOCK(4K) : ALIGN(4K)
    {
        *(.rodata)
    }

    .data BLOCK(4K) : ALIGN(4K)
    {
        *(.data)
    }

    /* .bss takes no space in the ELF file: it is memory zeroed at runtime,
     * and it is where the stack defined in boot.s lives. */
    .bss BLOCK(4K) : ALIGN(4K)
    {
        *(COMMON)
        *(.bss)
    }

    kernel_end = .;
}

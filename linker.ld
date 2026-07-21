/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 KekkoTech Softwares Open Source (Matteo Checcacci)
 *
 * linker.ld — dove finisce ogni pezzo del kernel in memoria.
 *
 * Il kernel viene caricato a 1 MiB: sotto quella soglia la memoria bassa e'
 * piena di roba legacy (IVT del BIOS, area BIOS, buffer video, ROM), quindi
 * 1 MiB e' il primo indirizzo davvero libero ed e' la convenzione su x86.
 */

ENTRY(_start)

SECTIONS
{
    . = 1M;

    /* kernel_start / kernel_end serviranno al Physical Memory Manager (Fase 4)
     * per marcare come occupati i frame in cui vive il kernel stesso. */
    kernel_start = .;

    /* .multiboot per prima: l'header deve stare nei primi 8 KiB del binario. */
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

    /* .bss non occupa spazio nel file ELF: e' solo memoria azzerata a runtime
     * (qui dentro ci finisce lo stack definito in boot.s). */
    .bss BLOCK(4K) : ALIGN(4K)
    {
        *(COMMON)
        *(.bss)
    }

    kernel_end = .;
}

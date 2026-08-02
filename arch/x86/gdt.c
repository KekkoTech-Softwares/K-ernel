/* SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 KekkoTech Softwares Open Source Project (Matteo Checcacci)
 *
 * gdt.c — builds the segment descriptors and hands them to the CPU.
 */

#include <stdint.h>

#include "gdt.h"
#include "string.h"

//defined in gdt_flush.s
extern void gdt_flush(uint32_t gdtr_address);
extern void tss_flush(void);

//top of the boot stack, from boot.s. Delcared as an array so thath the symbol's address is the value:
//ad 'extern uint32_t stack_top' would make the compiler read the memory there instead
extern uint8_t stack_top[];

static struct gdt_entry     gdt[GDT_ENTRIES];
static struct gdt_ptr       gdtr;
static struct tss_entry     tss;

static void gdt_set_entry(int index, uint32_t base, uint32_t limit, uint8_t access, uint8_t flags) {
    gdt[index].base_low     =   base & 0xFFFF;
    gdt[index].base_middle  =   (base >> 16) & 0xFF;
    gdt[index].base_high    =   (base >> 24) & 0xFF;

    gdt[index].limit_low    =   limit & 0xFFFF;
    gdt[index].granularity  =   ((limit >> 16) & 0x0F) | (flags & 0xF0);

    gdt[index].access = access;
}

static void tss_init(uint32_t kernel_stack) {
    memset(&tss, 0, sizeof tss);

    tss.ss0     =   GDT_KERNEL_DATA;
    tss.esp0    =   kernel_stack;

    //no I/O perm bitmap: pointing past the end of the TSS denies ring 3 every port.
    tss.iomap_base = sizeof tss;

    gdt_set_entry(5, (uint32_t)&tss, sizeof tss - 1, 0x89, 0x00);
}

void tss_set_stack(uint32_t esp0) {
    tss.esp0 = esp0;
}

void gdt_init(void) {
    gdt_set_entry(0, 0, 0, 0, 0);               //NULL descriptor
    gdt_set_entry(1, 0, 0xFFFFF, 0x9A, 0xC0);   //kernel code, ring 0
    gdt_set_entry(2, 0, 0xFFFFF, 0x92, 0xC0);   //kernel data, ring 0
    gdt_set_entry(3, 0, 0xFFFFF, 0xFA, 0xC0);   //user code, ring 3
    gdt_set_entry(4, 0, 0xFFFFF, 0xF2, 0xC0);   //user data, ring 3

    tss_init((uint32_t)stack_top);

    gdtr.limit = sizeof gdt - 1;
    gdtr.base = (uint32_t)&gdt;

    gdt_flush((uint32_t)&gdtr);
    tss_flush();
}
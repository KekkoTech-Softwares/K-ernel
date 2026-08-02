/* SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 KekkoTech Softwares Open Source Project (Matteo Checcacci)
 *
 * gdt.h — Global Descriptor Table and Task State Segment.
 *
 * PRIVATE to arch/x86, like io.h: generic code must never include this.
 * The entry point for the rest of the kernel is arch_init(), in arch.h.
 */

#ifndef GDT_H
#define GDT_H

#include <stdint.h>

#define GDT_ENTRIES 6

//Segment selectors index << 3, plus the requested privilege level in the low two bits. 
//The kernel ones run at RPL 0, so they are just offset

#define GDT_KERNEL_CODE 0x08
#define GDT_KERNEL_DATA 0x10
#define GDT_USER_CODE   0x1B    //0x18 | 3
#define GDT_USER_DATA   0x23    //0x20 | 3
#define GDT_TSS         0x2B    //0x28 | 3

//__attribute((packed))__ is necessary. Without this gcc allign byite (?) as it wants.
//if struct gdt_entry is not 8 bytes in the right order, lgdt load trash and the machine restars (?)

//one 8 byte descriptor. The base and the limit are split across non-adjacent fields for backwards compatibility 
//with 80286
struct gdt_entry {
    uint16_t    limit_low;
    uint16_t    base_low;
    uint8_t     base_middle;
    uint8_t     access;
    uint8_t     granularity;
    uint8_t     base_high;
} __attribute__((packed));

//what lgdt wants: (size of tables) - 1, then it's address
struct gdt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

//the i386 task state stagement: 104 bytes with a fixed layout. Onòy ss0,
//esp0 and iomap_base matter here, the resti is written by the CPU durint hwd task switching.
struct tss_entry {
    uint32_t prev_tss;
    uint32_t esp0;
    uint32_t ss0;
    uint32_t esp1, ss1;
    uint32_t esp2, ss2;
    uint32_t cr3;
    uint32_t eip;
    uint32_t eflags;
    uint32_t eax, ecx, edx, ebx;
    uint32_t esp, ebp, esi, edi;
    uint32_t es, cs, ss, ds, fs, gs;
    uin t32_t ldt;
    uint32_t trap;
    uint32_t iomap_base;
} __attribute__((packed));

void gdt_init(void);
void tss_set_stack(uint32_t esp0);

#endif //GDT_H
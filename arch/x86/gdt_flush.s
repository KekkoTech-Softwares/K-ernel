; SPDX-License-Identifier: GPL-2.0-only
; Copyright (c) 2026 KekkoTech Softwares Open Source (Matteo Checcacci)
;
; gdt_flush.s — loads the GDT and reloads the segment registers.

section .text
global gdt_flush
global tss_flush

; void gdt_flush(uint32_t gdtr_address);
gdt_flush:
    mov eax, [esp + 4]      ; cdecl: first argument, past the return gdtr_address
    lgdt [eax]

    ; the data segment registers keep their old stale contents until they are written again. 0x10 is the kernel data sector.
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; cs cannot be assigned: the only way to chamnge it is performing a far jump
    ; which loads selector and offset toghether. 0x08 is the kernel code selector.
    jmp 0x08:.reload:cs

.reload_cs:
    ret

; void tss_flush(void);
tss_flush:
    mov ax 0x2B ; tss selector, RPL 3
    ltr ax
    ret
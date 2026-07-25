; SPDX-License-Identifier: MIT
; Copyright (c) 2026 KekkoTech Softwares Open Source (Matteo Checcacci)
;
; boot.s — Multiboot header and kernel entry point.
;
; GRUB looks for the Multiboot header within the first 8 KiB of the binary;
; the linker script places the .multiboot section at the very start of .text
; to guarantee that. By the time GRUB hands over control the CPU is already
; in 32-bit protected mode, with interrupts disabled and paging off.

MBALIGN  equ 1 << 0                 ; align loaded modules on 4 KiB boundaries
MEMINFO  equ 1 << 1                 ; ask GRUB for the memory map
FLAGS    equ MBALIGN | MEMINFO
MAGIC    equ 0x1BADB002             ; Multiboot 1 magic number
CHECKSUM equ -(MAGIC + FLAGS)       ; MAGIC + FLAGS + CHECKSUM must total 0

section .multiboot
align 4
    dd MAGIC
    dd FLAGS
    dd CHECKSUM

; GRUB does not guarantee a usable stack, so the kernel provides its own.
; It lives in .bss, which takes up no space in the binary, and grows
; downwards from stack_top to stack_bottom.
section .bss
align 16
stack_bottom:
    resb 16384                      ; 16 KiB
stack_top:

section .text
global _start
extern kernel_main

_start:
    mov esp, stack_top              ; C code cannot run without a stack

    ; System V i386 ABI: esp must be 16-byte aligned at the point of the
    ; `call`. stack_top is 16-byte aligned and the two pushes below subtract
    ; 8, so another 8 bytes restore the alignment.
    sub esp, 8
    push ebx                        ; second argument: Multiboot info pointer
    push eax                        ; first argument: magic number (0x2BADB002)

    call kernel_main

    ; kernel_main is not expected to return. Should it ever do so, halt the
    ; CPU: cli disables interrupts, hlt parks the CPU until one arrives (and
    ; none will), and the jmp covers an NMI waking the CPU back up.
    cli
.hang:
    hlt
    jmp .hang

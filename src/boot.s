; SPDX-License-Identifier: MIT
; Copyright (c) 2026 KekkoTech Softwares Open Source (Matteo Checcacci)
;
; boot.s — header Multiboot ed entry point del kernel.
;
; GRUB cerca l'header Multiboot nei primi 8 KiB del binario: il linker script
; mette la sezione .multiboot in testa a .text proprio per garantirlo.
; Quando GRUB ci passa il controllo siamo gia' in protected mode a 32 bit,
; con interrupt disabilitati e paging spento.

MBALIGN  equ 1 << 0                 ; allinea i moduli caricati a 4 KiB
MEMINFO  equ 1 << 1                 ; chiedi a GRUB la memory map
FLAGS    equ MBALIGN | MEMINFO
MAGIC    equ 0x1BADB002             ; numero magico Multiboot 1
CHECKSUM equ -(MAGIC + FLAGS)       ; MAGIC + FLAGS + CHECKSUM deve fare 0

section .multiboot
align 4
    dd MAGIC
    dd FLAGS
    dd CHECKSUM

; Lo stack non esiste finche' non ce lo creiamo noi: GRUB non ne garantisce
; uno utilizzabile. Lo mettiamo in .bss (non occupa spazio nel binario) e
; cresce verso il basso, da stack_top a stack_bottom.
section .bss
align 16
stack_bottom:
    resb 16384                      ; 16 KiB
stack_top:

section .text
global _start
extern kernel_main

_start:
    mov esp, stack_top              ; il C ha bisogno di uno stack

    ; ABI System V i386: esp deve essere allineato a 16 byte al momento della
    ; `call`. stack_top e' allineato a 16; i due push sottraggono 8, quindi
    ; ne togliamo altri 8 per tornare in bolla.
    sub esp, 8
    push ebx                        ; secondo argomento: puntatore info Multiboot
    push eax                        ; primo argomento: magic number (0x2BADB002)

    call kernel_main

    ; kernel_main non dovrebbe mai tornare. Se succede, blocchiamo la CPU:
    ; cli spegne gli interrupt, hlt la ferma finche' non ne arriva uno (e non
    ; ne arrivano piu'), il jmp copre il caso di un NMI che risveglia la CPU.
    cli
.hang:
    hlt
    jmp .hang

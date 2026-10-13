; SPDX-License-Identifier: GPL-2.0-only
; Copyright (c) 2026 KekkoTech Softwares (Matteo Checcacci)
;
; isr.s — the 48 entry points the IDT gates point at, and the common path
;         that turns an interrupt frame into a `struct regs *` for C.

section .text
extern isr_dispatch

; Two macros, one per calling convention the CPU uses.
;
; On a few exceptions the CPU pushes an error code of its own, on every other
; vector it pushes nothing: C would then see two different frames for the same
; struct. The no-error-code stub pushes a zero in its place, so isr_dispatch
; always finds the same layout — and the exit path can always drop the same
; eight bytes. The fake zero goes first, because the stack grows down and the
; error code sits *above* the vector number in memory.
;
; The vector number is not available any other way: by the time the handler
; runs the CPU has forgotten which gate it came through.

%macro ISR_NOERR 1
isr%1:
    push dword 0            ; fake error code, so every frame looks alike
    push dword %1           ; vector number
    jmp isr_common
%endmacro

%macro ISR_ERR 1
isr%1:
    ; no fake push here: the CPU already left its own error code on the stack
    push dword %1
    jmp isr_common
%endmacro

; The 32 vectors the CPU reserves for itself (Intel SDM Vol.3, "Exception and
; Interrupt Vectors"). Which of them carry an error code is a hardware fact
; written out one line at a time on purpose: deducing it wrong misaligns the
; stack by four bytes, and the iret at the bottom then returns into nowhere —
; a triple fault with no message.
ISR_NOERR 0                 ; #DE divide by zero
ISR_NOERR 1                 ; #DB debug
ISR_NOERR 2                 ; NMI
ISR_NOERR 3                 ; #BP breakpoint (int3)
ISR_NOERR 4                 ; #OF overflow
ISR_NOERR 5                 ; #BR bound range exceeded
ISR_NOERR 6                 ; #UD invalid opcode
ISR_NOERR 7                 ; #NM device not available
ISR_ERR   8                 ; #DF double fault — always zero, but pushed
ISR_NOERR 9                 ; coprocessor segment overrun (none since the 486)
ISR_ERR   10                ; #TS invalid TSS
ISR_ERR   11                ; #NP segment not present
ISR_ERR   12                ; #SS stack segment fault
ISR_ERR   13                ; #GP general protection
ISR_ERR   14                ; #PF page fault
ISR_NOERR 15                ; reserved
ISR_NOERR 16                ; #MF x87 floating point
ISR_ERR   17                ; #AC alignment check
ISR_NOERR 18                ; #MC machine check
ISR_NOERR 19                ; #XM SIMD floating point
ISR_NOERR 20                ; #VE virtualisation
ISR_NOERR 21                ; #CP control protection — pushes an error code on
                            ; the CPUs that have it, and i686 has not: in the
                            ; no-error-code group it can only be wrong on a
                            ; machine where the vector never fires
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28                ; #HV hypervisor injection
ISR_NOERR 29                ; #VC VMM communication
ISR_NOERR 30                ; #SX security exception
ISR_NOERR 31                ; reserved

; The 16 PIC lines, remapped onto vectors 32-47 by pic_remap(). The chip never
; pushes an error code, so they all take the padded path.
ISR_NOERR 32                ; IRQ 0  — PIT
ISR_NOERR 33                ; IRQ 1  — keyboard
ISR_NOERR 34                ; IRQ 2  — cascade, never raised by a device
ISR_NOERR 35                ; IRQ 3
ISR_NOERR 36                ; IRQ 4
ISR_NOERR 37                ; IRQ 5
ISR_NOERR 38                ; IRQ 6
ISR_NOERR 39                ; IRQ 7
ISR_NOERR 40                ; IRQ 8
ISR_NOERR 41                ; IRQ 9
ISR_NOERR 42                ; IRQ 10
ISR_NOERR 43                ; IRQ 11
ISR_NOERR 44                ; IRQ 12
ISR_NOERR 45                ; IRQ 13
ISR_NOERR 46                ; IRQ 14
ISR_NOERR 47                ; IRQ 15

; What every stub falls into. On entry the stack already holds, from the top
; down: vector, error code, then the eip/cs/eflags the CPU pushed (plus esp
; and ss if the interrupt crossed a privilege boundary).
isr_common:
    pusha                   ; eax ecx edx ebx esp ebp esi edi, in that order,
                            ; which puts edi lowest in memory
    push ds
    push es
    push fs
    push gs

    ; An interrupt can arrive while ring 3 code is running, with the data
    ; segments holding user selectors: the kernel has to put its own back
    ; before touching any memory through them. All four, not just ds —
    ; whatever is left in es, fs or gs is what string instructions and any
    ; future per-CPU access would use.
    mov ax, 0x10            ; GDT_KERNEL_DATA (see gdt.h)
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    ; Never `mov ss, ax` here: on an interrupt the CPU has already loaded the
    ; right ss, and writing it opens a window where ss and esp disagree.
    cld                     ; the i386 ABI lets a caller leave DF set, and gcc
                            ; can expand an inline struct copy as `rep movs`:
                            ; entering C with DF=1 would walk it backwards.

    push esp                ; the argument: esp *is* the struct regs *
    call isr_dispatch
    add esp, 4              ; cdecl leaves the argument to the caller

    ; The exit mirrors the entry, and the three leftovers have to go by hand:
    ; the argument above, then the four segments and pusha, then the eight
    ; bytes the stubs and the CPU pushed. iret wants esp pointing exactly at
    ; eip, so one forgotten add is a return to a garbage address.
    pop gs
    pop fs
    pop es
    pop ds
    popa
    add esp, 8              ; vector + error code, real or fake
    iret

section .rodata

; One array instead of 48 extern declarations in idt.c — and the only symbol
; this file has to export, since the table is built where the stubs are.
global isr_stub_table
isr_stub_table:
%assign vec 0
%rep 48                     ; keep in step with IDT_STUB_COUNT in idt.h: the
                            ; loop in idt_init() trusts this count, and the
                            ; #if there only guards the C side
    dd isr %+ vec
%assign vec vec+1
%endrep

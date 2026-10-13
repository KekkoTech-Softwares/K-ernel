/* SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 KekkoTech Softwares (Matteo Checcacci)
 *
 * idt.c — builds the 256 gates, and dispatches whatever comes through them.
 *
 * This is also where the interrupt half of arch.h lands: enabling, disabling,
 * registering a device handler and halting are all the same subject seen from
 * the generic side.
 */

#include <stdint.h>

#include "arch.h"
#include "gdt.h"
#include "idt.h"
#include "kprintf.h"
#include "panic.h"
#include "pic.h"
#include "string.h"

//isr.s has one stub per vector it knows about. If the PIC ever grows a
//different number of lines, the stubs and this table stop agreeing and the
//IRQ gates would point at the wrong entry points.
#if IDT_STUB_COUNT != 32 + PIC_IRQ_COUNT
#error "isr.s and pic.h disagree on how many vectors need a stub"
#endif

//The vectors the CPU owns. Everything above 31 is ours to assign.
#define EXCEPTION_COUNT 32

static struct idt_entry idt[IDT_ENTRIES];
static struct idt_ptr   idtr;

//Exceptions the kernel wants to handle itself instead of dying on: the page
//fault, from phase 4 on, and the breakpoint in the self tests. NULL means
//"print the frame and stop".
static void (*exception_handlers[EXCEPTION_COUNT])(struct regs *);

//Device handlers. The ctx travels with the function so a driver can hand its
//own state over instead of reaching for a global.
static struct {
    void (*fn)(void *);
    void * ctx;
} irq_handlers[PIC_IRQ_COUNT];

/* Interrupts taken on a line nobody claimed. Not a panic, not even a warning
 * printed from the handler: one noise pulse on a floating ISA line would then
 * be enough to stop the machine, or to flood the console from inside an
 * interrupt. Counted and dropped. */
static unsigned irq_nohandler;

/* The names the CPU's own manual uses (Intel SDM Vol.3, "Exception and
 * Interrupt Vectors"). Worth the 32 pointers: "exception 13" means nothing at
 * three in the morning, "#GP general protection" says where to look. */
static const char * const exception_names[EXCEPTION_COUNT] = {
    "#DE divide by zero",
    "#DB debug",
    "NMI non-maskable interrupt",
    "#BP breakpoint",
    "#OF overflow",
    "#BR bound range exceeded",
    "#UD invalid opcode",
    "#NM device not available",
    "#DF double fault",
    "coprocessor segment overrun",
    "#TS invalid TSS",
    "#NP segment not present",
    "#SS stack segment fault",
    "#GP general protection",
    "#PF page fault",
    "reserved (15)",
    "#MF x87 floating point",
    "#AC alignment check",
    "#MC machine check",
    "#XM SIMD floating point",
    "#VE virtualisation",
    "#CP control protection",
    "reserved (22)",
    "reserved (23)",
    "reserved (24)",
    "reserved (25)",
    "reserved (26)",
    "reserved (27)",
    "#HV hypervisor injection",
    "#VC VMM communication",
    "#SX security exception",
    "reserved (31)",
};

//Every gate the kernel installs is the same kind: a present, DPL 0, 32-bit
//interrupt gate. The day a vector needs different attributes — a DPL 3 gate
//for a software syscall — the flags become an argument.
static void idt_set_gate(unsigned vector, void * handler) {
    uint32_t offset = (uint32_t)handler;

    idt[vector].offset_low  = offset & 0xFFFF;
    idt[vector].offset_high = (offset >> 16) & 0xFFFF;

    //the handler runs in the kernel's own code segment, whatever the
    //interrupted code was running in.
    idt[vector].selector    = GDT_KERNEL_CODE;
    idt[vector].zero        = 0;
    idt[vector].flags       = IDT_GATE_INT32;
}

void idt_init(void) {
    /* Zeroed first, and not just left to .bss: a gate with the present bit
     * clear is what makes vectors 48-255 fault instead of jumping to address
     * zero, so it has to be a decision taken here rather than a property of
     * where the array happened to be linked. */
    memset(idt, 0, sizeof idt);

    for (unsigned v = 0; v < IDT_STUB_COUNT; v++)
        idt_set_gate(v, isr_stub_table[v]);

    idtr.limit = sizeof idt - 1;
    idtr.base  = (uint32_t)&idt;

    //lidt has no register form, so the operand has to be memory — "m" on the
    //struct itself, exactly like lgdt in gdt_flush.s.
    __asm__ volatile("lidt %0" : : "m"(idtr));
}

void idt_read_idtr(struct idt_ptr * out) {
    //sidt writes 6 bytes and, like sgdt, only to memory.
    __asm__ volatile("sidt %0" : "=m"(*out));
}

void idt_set_exception_handler(unsigned vector, void (*fn)(struct regs *)) {
    //an IRQ or a software vector asked for here is a typo in the caller, not
    //a condition to handle: device lines go through arch_irq_register().
    if (vector >= EXCEPTION_COUNT)
        kpanic("idt_set_exception_handler: vector %u is not an exception", vector);

    exception_handlers[vector] = fn;
}

/* --- the fatal path -------------------------------------------------------
 *
 * Everything the CPU knew when it stopped, printed before kpanic() is called:
 * arch.h promises the machine prints its own registers, because eax means
 * nothing to generic code and nothing at all on another architecture.
 */

static void dump_regs(const struct regs * r) {
    /* The esp the interrupted code was using. Not esp_dummy, which is the esp
     * pusha saw inside the handler, and not user_esp either unless the
     * interrupt crossed a privilege boundary: without one the CPU pushed no
     * stack pointer, and the one it had is simply where the frame ends. */
    uint32_t esp = (r->cs & 3) ? r->user_esp : (uint32_t)&r->user_esp;

    kprintf("eax=0x%08x ebx=0x%08x ecx=0x%08x edx=0x%08x\n",
            r->eax, r->ebx, r->ecx, r->edx);
    kprintf("esi=0x%08x edi=0x%08x ebp=0x%08x esp=0x%08x\n",
            r->esi, r->edi, r->ebp, esp);
    kprintf("eip=0x%08x eflags=0x%08x err=0x%08x\n",
            r->eip, r->eflags, r->err_code);

    //the segment registers are 16 bit values sitting in 32 bit slots: the top
    //half is stack litter and printing it would look like corruption.
    kprintf("cs=0x%04x ds=0x%04x es=0x%04x fs=0x%04x gs=0x%04x\n",
            r->cs & 0xFFFF, r->ds & 0xFFFF, r->es & 0xFFFF,
            r->fs & 0xFFFF, r->gs & 0xFFFF);
}

static void exception_fatal(const struct regs * r) __attribute__((noreturn));

static void exception_fatal(const struct regs * r) {
    /* kpanic() has a guard of its own, but it only runs after the dump: a
     * fault inside dump_regs would come back through here and recurse down a
     * 16 KiB stack whose bottom is the GDT and the TSS. Unreachable today
     * (no paging, IF clear), reachable at the first #PF of phase 4. */
    static int in_fatal;
    if (in_fatal)
        kpanic("exception %u while reporting another one", r->vector);
    in_fatal = 1;

    /* The dump has to reach a device, and right now it might be aimed at a
     * ksnprintf buffer or at the serial port alone — the whole self test
     * suite runs that way. kpanic() does the same for its own message, but it
     * runs after this. */
    kout_detach_sink();
    kout_set(KOUT_ALL);

    kset_color(VGA_LIGHT_RED, VGA_BLACK);
    kprintf("\n--- EXCEPTION %u: %s ---\n",
            r->vector, exception_names[r->vector]);

    dump_regs(r);

    kpanic("unhandled exception %u (%s) at eip 0x%08x",
           r->vector, exception_names[r->vector], r->eip);
}

/* --- the dispatcher -------------------------------------------------------
 *
 * Called by the common tail of every stub in isr.s, and by nothing else: the
 * frame it reads only exists while that stub is running.
 */
void isr_dispatch(struct regs * r);

void isr_dispatch(struct regs * r) {
    if (r->vector < EXCEPTION_COUNT) {
        if (exception_handlers[r->vector]) {
            exception_handlers[r->vector](r);
            return;
        }

        exception_fatal(r);  //never comes back
    }

    if (r->vector >= PIC_VECTOR_BASE &&
        r->vector < PIC_VECTOR_BASE + PIC_IRQ_COUNT) {
        //validated above, so the subtraction cannot wrap into the array
        unsigned irq = r->vector - PIC_VECTOR_BASE;

        if (irq_handlers[irq].fn)
            irq_handlers[irq].fn(irq_handlers[irq].ctx);
        else
            irq_nohandler++;

        /* Always acknowledged, handler or not: a line left in service blocks
         * itself and every lower priority one from then on. pic_eoi() knows
         * what to do about a spurious interrupt, so there is nothing to
         * decide here — until a handler starts re-enabling interrupts, at
         * which point one acknowledgement too many closes the wrong line and
         * the spurious check has to move up here. */
        pic_eoi(irq);
        return;
    }

    /* No gate above 47 is present, so this is unreachable by hardware. If it
     * ever happens the IDT does not say what the code says, and guessing what
     * the frame means would be worse than stopping. */
    kpanic("interrupt on vector %u, which has no stub", r->vector);
}

/* --- arch.h, interrupt half ---------------------------------------------- */

/* The "memory" clobber is what stops the compiler from moving a load or a
 * store across the barrier. Redundant while these are out-of-line functions
 * in a translation unit of their own, and not redundant at all the day
 * somebody turns on -flto or makes them static inline in a header — two
 * instructions are a standing invitation. */
void arch_irq_enable(void) {
    __asm__ volatile("sti" ::: "memory");
}

void arch_irq_disable(void) {
    __asm__ volatile("cli" ::: "memory");
}

void arch_irq_register(unsigned irq, void (*handler)(void *), void * ctx) {
    if (irq >= PIC_IRQ_COUNT)
        kpanic("arch_irq_register: irq %u does not exist", irq);

    if (!handler) {
        //masked first, then forgotten: the other order would leave an open
        //line with a NULL handler, which is the one case the dispatcher
        //counts as unclaimed instead of servicing.
        pic_mask(irq);
        irq_handlers[irq].fn  = NULL;
        irq_handlers[irq].ctx = NULL;
        return;
    }

    /* Stored before the line is opened, never after. A device that already
     * has a request pending fires the moment the mask drops, and with the
     * fields written in the other order that first interrupt would call the
     * handler with somebody else's ctx — or a NULL one. */
    irq_handlers[irq].fn  = handler;
    irq_handlers[irq].ctx = ctx;

    pic_unmask(irq);
}

void arch_irq_eoi(unsigned irq) {
    //same policy as arch_irq_register on the same public boundary: the pic_*
    //functions tolerate a bad line in silence because they are internal,
    //arch_* does not, or a typo in a driver becomes invisible.
    if (irq >= PIC_IRQ_COUNT)
        kpanic("arch_irq_eoi: irq %u does not exist", irq);

    pic_eoi(irq);
}

//hlt belongs here and not in some generic place because it is an instruction:
//it parks the CPU until an interrupt arrives, which with interrupts disabled
//means for good — which is exactly what a panic wants.
void arch_halt(void) {
    __asm__ volatile("hlt");
}

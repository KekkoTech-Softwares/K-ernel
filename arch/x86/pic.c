/* SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 KekkoTech Softwares (Matteo Checcacci)
 *
 * pic.c — remaps the 8259A pair and owns their masks and acknowledgements.
 */

#include <stdint.h>

#include "io.h"
#include "pic.h"

/* The port addresses come from the PC/AT board, not from the chip: the master
 * is decoded at 0x20-0x21 and the slave at 0xA0-0xA1. The even port is the
 * command side (ICW1 during init, OCW2/OCW3 afterwards), the odd one the data
 * side (ICW2-ICW4 during init, then the interrupt mask register). */
#define PIC1_CMD    0x20
#define PIC1_DATA   0x21
#define PIC2_CMD    0xA0
#define PIC2_DATA   0xA1

/* Initialisation control words, from the 8259A datasheet.
 *
 * ICW1 0x11: bit 4 is the fixed marker that starts the sequence, bit 0 says a
 *      fourth control word will follow. The chip then expects ICW2, ICW3 and
 *      ICW4 on the data port, in that order and with nothing in between.
 * ICW3 differs between the two: on the master it is a bitmap of which input
 *      pins have a slave hanging off them, on the slave it is the master pin
 *      it answers from, written as a plain number. The PC/AT wires the slave
 *      to IRQ 2, hence 1 << 2 = 0x04 and 0x02.
 * ICW4 0x01: 8086/88 mode. Without it the chip answers an interrupt
 *      acknowledge with an 8080 CALL instruction, which is not what an x86
 *      CPU is asking for — it wants a single vector number. */
#define ICW1_INIT   0x11
#define ICW3_MASTER 0x04
#define ICW3_SLAVE  0x02
#define ICW4_8086   0x01

//OCW2 with bit 5 set: "end of interrupt", clear the highest priority level
//currently in service. Same number as PIC1_CMD by accident, different thing.
#define PIC_EOI     0x20

/* OCW3 0x0B: bit 3 is the marker that makes the chip read this byte as OCW3,
 * and bits 1-0 = 11 point the command port at the in-service register (10
 * would pick the request register instead). The selection is sticky, which is
 * harmless here because pic_read_isr() is the only code that ever reads back
 * from a command port. */
#define OCW3_READ_ISR   0x0B

//Every line disabled: OCW1 is inverted, a 1 bit means "do not deliver".
#define PIC_MASK_ALL    0xFF

void pic_remap(void) {
    /* The whole sequence writes the same two ports five times in a row. The
     * 8259 is an ISA-era part with no handshake on the data side: it needs
     * the bus to idle between accesses, and on a CPU fast enough to issue
     * them back to back a control word simply gets dropped — leaving the chip
     * stuck mid-initialisation. io_wait() buys that pause. */
    outb(PIC1_CMD, ICW1_INIT);
    io_wait();
    outb(PIC2_CMD, ICW1_INIT);
    io_wait();

    //ICW2: the base vector each chip adds its own line number to.
    outb(PIC1_DATA, PIC_VECTOR_BASE);
    io_wait();
    outb(PIC2_DATA, PIC_VECTOR_BASE + 8);
    io_wait();

    outb(PIC1_DATA, ICW3_MASTER);
    io_wait();
    outb(PIC2_DATA, ICW3_SLAVE);
    io_wait();

    outb(PIC1_DATA, ICW4_8086);
    io_wait();
    outb(PIC2_DATA, ICW4_8086);
    io_wait();

    /* Masked on the way out, all sixteen. The controller is programmed before
     * any driver exists, and the PIT is already ticking out of reset, so an
     * open line plus the first sti means an interrupt into a handler nobody
     * has written yet. Lines are opened one at a time, by whoever can
     * actually service them. */
    outb(PIC1_DATA, PIC_MASK_ALL);
    outb(PIC2_DATA, PIC_MASK_ALL);
}

/* The chip remembers which line it dispatched in its in-service register, and
 * until it is told the handler is finished it blocks that line and every
 * lower priority one. A slave line was latched twice: by the slave, where it
 * originated, and by the master, whose cascade pin the slave pulled. So the
 * master always has to be acknowledged — even for IRQ 8-15, where forgetting
 * it leaves IRQ 2 in service and silences the whole slave for good. The slave
 * goes first, so that it is already free when the master reopens the
 * cascade. */
void pic_eoi(unsigned irq) {
    if (irq >= PIC_IRQ_COUNT)
        return;

    /* A spurious interrupt was never put in service, so there is nothing to
     * end: an EOI here would clear whatever real line is in service at that
     * priority, and that handler would then never be heard from again. The
     * slave's own spurious pulse is the one exception that still needs a
     * word, and only to the master: the cascade on IRQ 2 did go in service
     * there, and leaving it set kills IRQ 3-15 for good. */
    if (pic_is_spurious(irq)) {
        if (irq == PIC_SPURIOUS_SLAVE)
            outb(PIC1_CMD, PIC_EOI);

        return;
    }

    if (irq >= 8)
        outb(PIC2_CMD, PIC_EOI);

    outb(PIC1_CMD, PIC_EOI);
}

/* Once initialised, the data port is the interrupt mask register, and there
 * is no way to touch a single bit: the byte goes in whole, so the current
 * value has to be read back, edited and written again. */
void pic_mask(unsigned irq) {
    uint16_t port;

    if (irq >= PIC_IRQ_COUNT)
        return;

    port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
    outb(port, inb(port) | (uint8_t)(1u << (irq & 7)));
}

void pic_unmask(unsigned irq) {
    uint16_t port;

    if (irq >= PIC_IRQ_COUNT)
        return;

    port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
    outb(port, inb(port) & (uint8_t)~(1u << (irq & 7)));

    /* A slave line needs one more bit open. The slave has no connection to
     * the CPU: it signals by raising the master's cascade pin, so with IRQ 2
     * still masked all of IRQ 8-15 stay silent while their own bits read as
     * enabled. It is left masked by pic_remap() like everything else, and
     * nothing else would ever ask for it — IRQ 2 has no device of its own.
     *
     * Not symmetric on purpose: pic_mask() does not close IRQ 2 back when the
     * last slave line goes away. The cascade pin only ever fires because a
     * slave line did, and those are masked individually, so leaving it open
     * costs nothing and avoids tracking a count. */
    if (irq >= 8)
        pic_unmask(PIC_CASCADE_IRQ);
}

uint16_t pic_get_mask(void) {
    return (uint16_t)(inb(PIC1_DATA) | (inb(PIC2_DATA) << 8));
}

uint16_t pic_read_isr(void) {
    //OCW3 goes to the command port, and the command port is then what reads
    //the register back — the data port keeps meaning the mask.
    outb(PIC1_CMD, OCW3_READ_ISR);
    outb(PIC2_CMD, OCW3_READ_ISR);

    return (uint16_t)(inb(PIC1_CMD) | (inb(PIC2_CMD) << 8));
}

int pic_is_spurious(unsigned irq) {
    //Every other line is wired to a device that holds it until acknowledged,
    //so for those the question does not even make sense.
    if (irq != PIC_SPURIOUS_MASTER && irq != PIC_SPURIOUS_SLAVE)
        return 0;

    //The chip answered with this line but never marked it in service: nothing
    //raised it, the CPU is holding an interrupt that does not exist.
    return (pic_read_isr() & (uint16_t)(1u << irq)) == 0;
}

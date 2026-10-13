/* SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 KekkoTech Softwares (Matteo Checcacci)
 *
 * pic_test.c — self tests for the 8259A pair.
 *
 * What cannot be tested here: the remap itself. ICW2 is write only, so no
 * read will ever tell which vector the chips were given — the only proof is
 * an IRQ 0 arriving at vector 32, and that belongs to the timer's tests.
 * These checks cover the part that can be read back: the masks, the in-service
 * register, and the cascade rule hidden inside pic_unmask().
 *
 * The suite runs after the drivers have opened their own lines, so it must
 * never assert the masks are all closed: that would be a check with an expiry
 * date. It asserts invariants instead, and puts back everything it changes.
 */

#include <stdint.h>

#include "ktest.h"
#include "pic.h"

/* Two lines the PC/AT leaves free, one per chip: nothing is wired to them, so
 * opening one for a few instructions cannot produce an interrupt. If a driver
 * ever claims IRQ 5 or IRQ 10, pick another free line here — do not drop the
 * check, the point is that unused lines stay shut. */
#define FREE_MASTER_IRQ 5
#define FREE_SLAVE_IRQ  10

//Puts the masks back exactly as they were found. The cascade goes last on
//purpose: pic_unmask() reopens it as a side effect of opening a slave line,
//so restoring it earlier would be undone by the loop.
static void restore_mask(uint16_t mask) {
    for (unsigned irq = 0; irq < PIC_IRQ_COUNT; irq++) {
        if (irq == PIC_CASCADE_IRQ)
            continue;

        if (mask & (1u << irq))
            pic_mask(irq);
        else
            pic_unmask(irq);
    }

    if (mask & (1u << PIC_CASCADE_IRQ))
        pic_mask(PIC_CASCADE_IRQ);
    else
        pic_unmask(PIC_CASCADE_IRQ);
}

void test_pic(void) {
    uint16_t saved = pic_get_mask();
    uint16_t mask;

    /* Nothing can be in service: this code runs from kernel_main, not from a
     * handler. A bit stuck here means some handler returned without calling
     * pic_eoi(), and that line will never be delivered again — the failure
     * mode is a device that works exactly once. */
    KCHECK(pic_read_isr() == 0);

    /* With the in-service register empty, an IRQ 7 arriving right now could
     * only be spurious — so the rule must say so. A false here means the OCW3
     * readback is wrong (wrong control word, or the two bytes swapped), and
     * pic_eoi() would then send a master EOI for a line that never fired,
     * clearing whatever really is in service. */
    KCHECK(pic_is_spurious(PIC_SPURIOUS_MASTER));

    //...and a line with a device behind it is never spurious, whatever the
    //ISR says: only 7 and 15 can be.
    KCHECK(!pic_is_spurious(0));

    /* Lines nobody has asked for are still masked. pic_remap() closes all
     * sixteen and only a driver reopens one, so a zero bit here means some
     * line was left open without an owner: the first sti would then take an
     * interrupt into a handler that does not exist. */
    KCHECK(saved & (1u << FREE_MASTER_IRQ));
    KCHECK(saved & (1u << FREE_SLAVE_IRQ));

    /* Out of range must change nothing. The mask functions pick their port
     * from the IRQ number, so without the range guard an IRQ 16 would write
     * the master's mask with bit 0 — silently enabling or killing the timer
     * because a caller passed a bad number. */
    pic_mask(PIC_IRQ_COUNT);
    pic_unmask(PIC_IRQ_COUNT + 7);
    KCHECK(pic_get_mask() == saved);

    /* The masks are read-modify-write on a whole byte: the obvious bug is
     * touching the other seven lines along with the intended one. Checking
     * the full word, not just the bit, is what would catch it. */
    pic_unmask(FREE_MASTER_IRQ);
    mask = pic_get_mask();
    KCHECK((mask & (1u << FREE_MASTER_IRQ)) == 0);
    KCHECK(mask == (uint16_t)(saved & ~(1u << FREE_MASTER_IRQ)));

    pic_mask(FREE_MASTER_IRQ);
    KCHECK(pic_get_mask() == saved);

    /* Opening a slave line must open the cascade too: the slave has no path
     * to the CPU of its own, it raises the master's IRQ 2. Without this the
     * line looks enabled in the mask and stays mute — the bug reads as "the
     * device is dead" and is invisible from the mask register. */
    pic_unmask(FREE_SLAVE_IRQ);
    mask = pic_get_mask();
    KCHECK((mask & (1u << FREE_SLAVE_IRQ)) == 0);
    KCHECK((mask & (1u << PIC_CASCADE_IRQ)) == 0);

    restore_mask(saved);

    /* And the same invariant seen from the other side, on the state the test
     * was handed: if any slave line is open, the cascade has to be open as
     * well. Vacuously true while the whole slave is masked, and the real
     * check once an IRQ 8-15 driver exists. */
    mask = pic_get_mask();
    if ((mask >> 8) != 0xFF)
        KCHECK((mask & (1u << PIC_CASCADE_IRQ)) == 0);

    //The suite must hand the chip back untouched: a test that leaves a line
    //open is a test that breaks the machine it was meant to protect.
    KCHECK(mask == saved);
}

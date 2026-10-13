/* SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 KekkoTech Softwares (Matteo Checcacci)
 *
 * gdt_test.c — self tests for the GDT and the TSS.
 *
 * These values used to be checked by hand, reading `info registers` in the
 * QEMU monitor. They run at every boot now because the one real bug this
 * module had so far (a 108 byte TSS) compiled without a warning, booted
 * without a crash, and printed nothing wrong.
 *
 * The suite lives in arch/x86/ and not in kernel/ktest.c for the same reason
 * gdt.h is private: segments are a machine detail. arch_init() hands it to
 * ktest with ktest_register().
 */

#include <stddef.h>

#include "gdt.h"
#include "ktest.h"

void test_gdt(void) {
    struct gdt_ptr gdtr;

    //cs is only ever written by the far jump at the end of gdt_flush: a wrong
    //value here means that jump did not take, so the kernel is still running
    //on whatever code segment GRUB left behind.
    KCHECK(gdt_read_cs() == GDT_KERNEL_CODE);

    //the data segment registers keep their stale contents until gdt_flush
    //rewrites them: a wrong ds means the segment reload was skipped.
    KCHECK(gdt_read_ds() == GDT_KERNEL_DATA);

    //a null tr means ltr never ran: without a TSS there is no ring 3 -> ring 0
    //stack switch, so the first privilege change would triple fault.
    KCHECK(gdt_read_tr() == GDT_TSS);

    //the limit the CPU holds is the one lgdt actually took. If it disagrees
    //with the table built in gdt.c, the GDTR was loaded from the wrong place
    //or with the wrong size, and the descriptors beyond the limit are invalid.
    gdt_read_gdtr(&gdtr);
    KCHECK(gdtr.limit == sizeof(struct gdt_entry) * GDT_ENTRIES - 1);

    //the check above compares an expression with itself: it proves lgdt took
    //the right value, not that the structs have the size the CPU expects.
    //gdt_entry survives even without packed (two 16 bit fields then four 8 bit
    //ones already pack to 8), but gdt_ptr does not: base would align to 4 and
    //the struct would grow to 8 bytes, so sgdt would write base at offset 2
    //while C reads it at offset 4 — and lgdt would load a garbage address.
    KCHECK(sizeof(struct gdt_entry) == 8);
    KCHECK(sizeof(struct gdt_ptr) == 6);

    /* Regression guard on a bug that really happened (3 Aug 2026).
     *
     * trap and iomap_base were declared uint32_t instead of uint16_t: the
     * struct grew to 108 bytes and iomap_base moved away from offset 102,
     * where the CPU reads it unconditionally. It then found 0, meaning "the
     * I/O permission bitmap starts at the top of the TSS" — so instead of
     * denying ring 3 every port, the TSS header itself was read as permission
     * bits. Nothing warned, nothing crashed; the only visible trace was the
     * TSS limit reading 0x6b instead of 0x67 in the QEMU monitor.
     *
     * The two checks below are that manual inspection, automated. They are
     * about the layout the hardware mandates, so they must not be "fixed" by
     * updating the numbers if a field is ever added. */
    KCHECK(sizeof(struct tss_entry) == 104);
    KCHECK(offsetof(struct tss_entry, iomap_base) == 102);
}

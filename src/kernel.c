/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 KekkoTech Softwares Open Source (Matteo Checcacci)
 *
 * kernel.c — punto di ingresso in C del kernel.
 *
 * Ci arriva `_start` (boot.s) dopo che GRUB ci ha caricati a 1 MiB e messi in
 * protected mode. Qui il C e' "freestanding": niente libc, niente printf,
 * niente malloc, nessun sistema operativo sotto di noi. Tutto quello che
 * usiamo, prima ce lo scriviamo.
 */

#include <stdint.h>

#include "serial.h"
#include "vga.h"

/* Valore che GRUB lascia in eax per dire "ti ho caricato io, via Multiboot".
 * Nota: e' diverso dal magic dell'header (0x1BADB002) che GRUB cerca in noi. */
#define MULTIBOOT_BOOTLOADER_MAGIC 0x2BADB002

/* Scrive sia sullo schermo sia sulla seriale: lo schermo per il colpo d'occhio,
 * la seriale perche' e' scrollabile e resta nel terminale dell'host. */
static void kputs(const char *str)
{
    vga_puts(str);
    serial_puts(str);
}

/* Primo pezzo della futura mini-libreria: stampa un uint32 in esadecimale.
 * Serve gia' adesso per far vedere il magic number che GRUB ci ha passato. */
static void kput_hex(uint32_t value)
{
    static const char digits[] = "0123456789ABCDEF";
    char buf[11];

    buf[0] = '0';
    buf[1] = 'x';
    for (int i = 0; i < 8; i++)
        buf[2 + i] = digits[(value >> ((7 - i) * 4)) & 0xF];
    buf[10] = '\0';

    kputs(buf);
}

void kernel_main(uint32_t magic, uint32_t *mb_info)
{
    (void)mb_info; /* la memory map ci servira' in Fase 4 (PMM) */

    serial_init();
    vga_init();

    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    kputs("K-ernel\n");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    kputs("Fase 1: boot via GRUB/Multiboot completato.\n\n");

    kputs("Multiboot magic: ");
    kput_hex(magic);

    if (magic == MULTIBOOT_BOOTLOADER_MAGIC) {
        vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
        kputs("  [OK]\n");
    } else {
        /* Se il magic non torna, non siamo stati caricati da un bootloader
         * Multiboot: mb_info non e' affidabile e non va usato. */
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        kputs("  [ATTESO 0x2BADB002]\n");
    }

    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    kputs("\nProssimo passo: GDT (Fase 2).\n");

    /* Non abbiamo nulla da schedulare: fermiamo la CPU. `hlt` la mette in
     * pausa fino al prossimo interrupt, cosi' non brucia un core a vuoto. */
    for (;;)
        __asm__ volatile("hlt");
}

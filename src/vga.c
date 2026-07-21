/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 KekkoTech Softwares Open Source (Matteo Checcacci)
 *
 * vga.c — terminale testuale sul VGA text buffer.
 *
 * A 0xB8000 c'e' una matrice 80x25 di celle da 2 byte: il byte basso e' il
 * codice ASCII, quello alto il colore (4 bit foreground + 4 bit background).
 * Scrivere in quella memoria significa scrivere sullo schermo, senza
 * bisogno di driver: e' il modo piu' diretto per avere output da un kernel.
 */

#include "vga.h"
#include "io.h"

#define VGA_WIDTH  80
#define VGA_HEIGHT 25

/* volatile: il compilatore non deve ottimizzare via scritture che "sembrano"
 * inutili, perche' hanno un effetto collaterale visibile (lo schermo). */
static volatile uint16_t *const vga_buffer = (volatile uint16_t *)0xB8000;

static size_t vga_row;
static size_t vga_column;
static uint8_t vga_attr;

static inline uint16_t vga_entry(char c, uint8_t attr)
{
    return (uint16_t)(uint8_t)c | ((uint16_t)attr << 8);
}

/* Il cursore hardware si pilota tramite due porte: 0x3D4 seleziona il
 * registro interno del controller VGA, 0x3D5 ne legge/scrive il valore.
 * I registri 14 e 15 contengono la posizione del cursore su 16 bit. */
static void vga_update_cursor(void)
{
    uint16_t pos = (uint16_t)(vga_row * VGA_WIDTH + vga_column);

    outb(0x3D4, 14);
    outb(0x3D5, (uint8_t)(pos >> 8));
    outb(0x3D4, 15);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
}

static void vga_clear(void)
{
    for (size_t y = 0; y < VGA_HEIGHT; y++)
        for (size_t x = 0; x < VGA_WIDTH; x++)
            vga_buffer[y * VGA_WIDTH + x] = vga_entry(' ', vga_attr);
}

/* Scorre lo schermo di una riga: copia tutto in su e svuota l'ultima riga. */
static void vga_scroll(void)
{
    for (size_t y = 1; y < VGA_HEIGHT; y++)
        for (size_t x = 0; x < VGA_WIDTH; x++)
            vga_buffer[(y - 1) * VGA_WIDTH + x] = vga_buffer[y * VGA_WIDTH + x];

    for (size_t x = 0; x < VGA_WIDTH; x++)
        vga_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = vga_entry(' ', vga_attr);

    vga_row = VGA_HEIGHT - 1;
}

static void vga_newline(void)
{
    vga_column = 0;
    if (++vga_row == VGA_HEIGHT)
        vga_scroll();
}

void vga_set_color(enum vga_color fg, enum vga_color bg)
{
    vga_attr = (uint8_t)fg | (uint8_t)(bg << 4);
}

void vga_init(void)
{
    vga_row = 0;
    vga_column = 0;
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_clear();
    vga_update_cursor();
}

void vga_putchar(char c)
{
    switch (c) {
    case '\n':
        vga_newline();
        break;
    case '\r':
        vga_column = 0;
        break;
    case '\t':
        do {
            vga_putchar(' ');
        } while (vga_column % 4 != 0);
        break;
    default:
        vga_buffer[vga_row * VGA_WIDTH + vga_column] = vga_entry(c, vga_attr);
        if (++vga_column == VGA_WIDTH)
            vga_newline();
        break;
    }

    vga_update_cursor();
}

void vga_write(const char *data, size_t size)
{
    for (size_t i = 0; i < size; i++)
        vga_putchar(data[i]);
}

void vga_puts(const char *str)
{
    for (size_t i = 0; str[i]; i++)
        vga_putchar(str[i]);
}

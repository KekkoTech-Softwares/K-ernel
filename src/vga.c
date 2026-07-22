/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 KekkoTech Softwares Open Source (Matteo Checcacci)
 *
 * vga.c — text terminal on the VGA text buffer.
 *
 * At 0xB8000 sits an 80x25 grid of 2-byte cells: the low byte holds the
 * ASCII code, the high byte the color (4 bits foreground, 4 bits
 * background). Writing to that memory writes to the screen, with no driver
 * involved — the most direct way to get output out of a kernel.
 */

#include "vga.h"
#include "io.h"

#define VGA_WIDTH  80
#define VGA_HEIGHT 25

/* volatile keeps the compiler from optimising away stores that look useless
 * to it: their side effect is visible on screen, not in the program state. */
static volatile uint16_t *const vga_buffer = (volatile uint16_t *)0xB8000;

static size_t vga_row;
static size_t vga_column;
static uint8_t vga_attr;

static inline uint16_t vga_entry(char c, uint8_t attr)
{
    return (uint16_t)(uint8_t)c | ((uint16_t)attr << 8);
}

/* The hardware cursor is driven through two ports: 0x3D4 selects an internal
 * register of the VGA controller and 0x3D5 reads or writes its value.
 * Registers 14 and 15 hold the 16-bit cursor position. */
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

/* Scrolls the screen by one line: everything moves up and the last line is
 * cleared. */
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

uint8_t vga_get_color(void) {
    return vga_attr;
}

void vga_set_attr(uint8_t attr) {
    vga_attr = attr;
}

void vga_init(void) {
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

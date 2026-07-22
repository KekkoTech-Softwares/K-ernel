/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 KekkoTech Softwares Open Source Project (Matteo Checcacci)
 *
 * vga.h — text terminal on the VGA text buffer (0xB8000, 80x25).
 */

#ifndef VGA_H
#define VGA_H

#include <stddef.h>
#include <stdint.h>

enum vga_color {
    VGA_BLACK = 0,
    VGA_BLUE = 1,
    VGA_GREEN = 2,
    VGA_CYAN = 3,
    VGA_RED = 4,
    VGA_MAGENTA = 5,
    VGA_BROWN = 6,
    VGA_LIGHT_GREY = 7,
    VGA_DARK_GREY = 8,
    VGA_LIGHT_BLUE = 9,
    VGA_LIGHT_GREEN = 10,
    VGA_LIGHT_CYAN = 11,
    VGA_LIGHT_RED = 12,
    VGA_LIGHT_MAGENTA = 13,
    VGA_LIGHT_BROWN = 14,
    VGA_WHITE = 15,
};

void vga_init(void);
void vga_set_color(enum vga_color fg, enum vga_color bg);
void vga_putchar(char c);
void vga_write(const char *data, size_t size);
void vga_puts(const char *str);
uint8_t vga_get_color(void);
void vga_set_attr(uint8_t attr);

#endif /* VGA_H */

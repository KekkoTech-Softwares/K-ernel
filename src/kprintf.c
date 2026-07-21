/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 KekkoTech Softwares Open Source (Matteo Checcacci)
 *
 * kprintf.c — formatted kernel output.
 * All the kernel's output passes from kputchar, only point who knows 
 * which channel is active.
 */

#include <stddef.h>

#include "kprintf.h"
#include "serial.h"
#include "vga.h"

/*PRINT-UINT: print an integer whithout sign in the indicated base (f.ex.: 10, 2, 16 ecc..)
width is the minimum lenght. If the number is shorter it is filled on the left with the pad character. 
width = 0 means "no filling"*/
static void print_uint(unsigned int value, unsigned int base, int width, char pad) {
    
    static const char digits[] = "0123456789ABCDEF";
    char buf[33]; //this is the worst case->32 digits base 2 plus padding.
    int len = 0;

    if(value == 0) {
        buf[len++] = '0';
    }
    else {
        while(value > 0) {
            buf[len++] = digits[value % base];
            value /= base;
        }
    }

    for(int i = len; i < width; i++)
        kputchar(pad);

    //digits are not in the right order. Must be printed from the bottom
    while(len > 0){
        kputchar(buf[--len]);
    }
}

//PRINT_INT: print integer base 10 with sign
static void print_int(int value) {
    unsigned int u = (unsigned int)value;

    if(value < 0) {
        kputchar('-');
        u = -u;
    }

    print_uint(u, 10, 0, ' ');
}


void kputchar(char c) {
    vga_putchar(c);
    serial_putchar(c);
}

void kputs(const char *str) {
    for(size_t i = 0; str[i]; i++)
        kputchar(str[i]);
}
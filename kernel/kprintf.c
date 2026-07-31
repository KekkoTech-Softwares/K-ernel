/* SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 KekkoTech Softwares Open Source Project (Matteo Checcacci)
 *
 * kprintf.c — formatted kernel output.
 * All the kernel's output passes from kputchar, only point who knows 
 * which channel is active.
 */

#include <stddef.h>
#include <stdarg.h>
#include <stdint.h> //uint8_t to save VGA attribute

#include "kprintf.h"
#include "serial.h"
#include "vga.h"

static unsigned int kout_channels = KOUT_ALL;

//VGA index -> ansi. (conversion with a table because of different order)
static const unsigned char ansi_fg[16] = {
    30, 34, 32, 36, 31, 35, 33, 37,
    90, 94, 92, 96, 91, 95, 93, 97,
};

struct klog_style {
    const char * tag;
    enum vga_color fg;
};

static const struct klog_style klog_styles[] = {
    [KLOG_DEBUG] = {"[ DBG  ] ", VGA_DARK_GREY},
    [KLOG_INFO]  = {"[ INFO ] ", VGA_LIGHT_GREY},
    [KLOG_WARN]  = {"[ WARN ] ", VGA_LIGHT_BROWN},
    [KLOG_ERR]   = {"[ ERR  ] ", VGA_LIGHT_RED},
};

static enum klog_level klog_min = KLOG_DEBUG;

/*PRINT-UINT: print an integer whithout sign in the indicated base (f.ex.: 10, 2, 16 ecc..)
width is the minimum lenght. If the number is shorter it is filled on the left with the pad character. 
width = 0 means "no filling"*/
static void print_uint(unsigned int value, unsigned int base, int width, char pad, int upper) {
    static const char digits_upper[] = "0123456789ABCDEF";
    static const char digits_lower[] = "0123456789abcdef";
    const char * digits = upper ? digits_upper : digits_lower;
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

//number of digits of value in the given base
static int uint_len(unsigned int value, unsigned int base) {
    int len = 1;

    while(value >= base) {
        value /= base;
        len++;
    }
    return len;
}

//PRINT_INT: print integer base 10 with sign
static void print_int(int value, int width, char pad) {
    unsigned int u = (unsigned int)value;
    int negative = (value < 0);
    int len;

    if(negative)
        u = -u;

    len = uint_len(u, 10) + (negative ? 1 : 0);

    if(pad == '0') {
            if(negative)
                kputchar('-');
            for(int i = len; i < width; i++)
                kputchar('0');
    }
    else {
        for(int i = len; i < width; i++)
            kputchar(' ');
        if(negative)
            kputchar('-');
    }

    print_uint(u, 10, 0, ' ', 0);
}


void kputchar(char c) {
    if(kout_channels & KOUT_VGA)
        vga_putchar(c);
    if(kout_channels & KOUT_SERIAL)
        serial_putchar(c);
}

void kputs(const char *str) {
    for(size_t i = 0; str[i]; i++)
        kputchar(str[i]);
}

//prints the low bits of value, most significant first, inserting "_" every group bits. group = 0 disables the separator
static void print_bits(unsigned int value, int bits, int group) {
    for(int i = bits - 1; i >= 0; i--) {
        kputchar((value >> i) & 1u ? '1' : '0');
        if(group > 0 && i > 0 && i % group == 0)
            kputchar('_');
    }
}

static void serial_ansi_color(enum vga_color fg) {
    unsigned char code = ansi_fg[fg];

    serial_putchar('\x1b');
    serial_putchar('[');
    serial_putchar((char)('0' + code / 10)); //always 2 digits 
    serial_putchar((char)('0' + code % 10));
    serial_putchar('m');
}

static void serial_ansi_reset(void) {
    serial_putchar('\x1b');
    serial_putchar('[');
    serial_putchar('0');
    serial_putchar('m');
}

void kvprintf(const char *fmt, va_list args) {

    for(size_t i = 0; fmt[i]; i++) {
        //norma text print
        if(fmt[i] != '%') {
            kputchar(fmt[i]);
            continue;
        }

        i++; //reading of the char after '%'

        //if the string ends with '%' i dunnow
        if(fmt[i] == '\0')
            break;
        
        //padding with '0' f.ex.: %08x
        char pad = ' ';
        int width = 0;

        if (fmt[i] == '0') {
            pad = '0';
            i++;
        }
        while(fmt[i] >= '0' && fmt[i] <= '9') {
            width = width * 10 + fmt[i] - '0';
            i++;
        }

        switch(fmt[i]) {
            case 'd':
            case 'i':
                print_int(va_arg(args, int), width, pad);
                break;
            case 'u':
                print_uint(va_arg(args, unsigned int), 10, width, pad, 0);
                break;
            case 'x':
                print_uint(va_arg(args, unsigned int), 16, width, pad, 0);
                break;
            case 'X':
                print_uint(va_arg(args, unsigned int), 16, width, pad, 1);
                break;
            case 'b':
                print_uint(va_arg(args, unsigned int), 2, width, pad, 0);
                break;
            case 'B':
                print_bits(va_arg(args, unsigned int), width > 0 ? width : 32, 8);
                break;
            case 'p':
                kputs("0x");
                print_uint((unsigned int)va_arg(args, void *), 16, 8, '0', 1);
                break;
            case 'c':
                kputchar((char)va_arg(args, int));
                break;
            case 's':
                kputs(va_arg(args, const char *));
                break;
            case '%':
                kputchar('%');
                break;
            default: //unkown idetificator. show it as it is.
                kputchar('%');
                kputchar(fmt[i]);
                break;
        }
    }
}

void kprintf(const char * fmt, ...) {
    va_list args;

    va_start(args, fmt);
    kvprintf(fmt, args);
    va_end(args);
}

void kset_color(enum vga_color fg, enum vga_color bg) {
    vga_set_color(fg, bg);

    if(kout_channels & KOUT_SERIAL)
        serial_ansi_color(fg);
}

void kprintf_color(enum vga_color fg, enum vga_color bg, const char * fmt, ...) {
    uint8_t saved = vga_get_color();
    va_list args;

    kset_color(fg, bg);

    va_start(args, fmt);
    kvprintf(fmt, args);
    va_end(args);

    vga_set_attr(saved);

    if(kout_channels & KOUT_SERIAL)
        serial_ansi_reset();
}

unsigned int kout_get(void) {
    return kout_channels;
}

void kout_set(unsigned int channels) {
    kout_channels = channels;
}

void klog_set_level(enum klog_level min) {
    klog_min = min;
}

void klog(enum klog_level level, const char * fmt, ...) {
    if(level < klog_min)
        return;

    const struct klog_style * style = &klog_styles[level];
    uint8_t saved = vga_get_color();
    va_list args;

    kset_color(style->fg, VGA_BLACK);
    kputs(style->tag);
    vga_set_attr(saved);
    serial_ansi_reset();

    va_start(args, fmt);
    kvprintf(fmt, args);
    va_end(args);
}
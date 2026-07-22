/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 KekkoTech Softwares Open Source Project (Matteo Checcacci)
 *
 * kprintf.h — formatted kernel output.
 */

 //Include guard. 
#ifndef KPRINTF_H
#define KPRINTF_H

#include <stdarg.h>

//Output channels: one bit each, so they can be combined with |.
#define KOUT_VGA (1u << 0)
#define KOUT_SERIAL (1u << 1)
#define KOUT_ALL (KOUT_VGA | KOUT_SERIAL)
#define kdebug(...) klog(KLOG_DEBUG,    __VA_ARGS__)
#define kinfo(...)  klog(KLOG_INFO,     __VA_ARGS__)
#define kwarn(...)  klog(KLOG_WARN,     __VA_ARGS__)
#define kerr(...)   klog(KLOG_ERR,      __VA_ARGS__)

enum klog_level {
    KLOG_DEBUG  = 0,
    KLOG_INFO,
    KLOG_WARN,
    KLOG_ERR,
};

void kputchar(char c);
void kputs(const char *str);
void kvprintf(const char *fmt, va_list args);
void kprintf(const char *fmt, ...);

unsigned int kout_get(void);
void kout_set(unsigned int channels);

void klog(enum klog_level level, const char * fmt, ...);
void klog_set_level(enum klog_level min);


#endif //KPRINTF_H
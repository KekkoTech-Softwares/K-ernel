/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 KekkoTech Softwares Open Source (Matteo Checcacci)
 *
 * kprintf.h — formatted kernel output.
 */

 //Include guard. 
#ifndef KPRINTF_H
#define KPRINTF_H
#define KOUT_VGA (1u << 0)
#define KOUT_SERIAL (1u << 1)
#define KOUT_ALL (KOUT_VGA | KOUT_SERIAL)


#include <stdarg.h>

void kputchar(char c);
void kputs(const char *str);
void kvprintf(const char *fmt, va_list args);
void kprintf(const char *fmt, ...);
unsigned int kout_get(void);
void kout_set(unsigned int channels);


#endif //KPRINTF_H
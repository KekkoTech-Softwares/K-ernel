/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 KekkoTech Softwares Open Source (Matteo Checcacci)
 *
 * kprintf.h — formatted kernel output.
 */

 //Include guard. 
 #ifndef KPRINTF_H
 #define KPRINTF_H

 void kputchar(char c);
 void kputs(const char *str);
 void kprintf(const char *fmt, ...);

 #endif //KPRINTF_H
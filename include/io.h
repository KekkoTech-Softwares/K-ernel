/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 KekkoTech Softwares Open Source (Matteo Checcacci)
 *
 * io.h — accesso alle porte I/O x86.
 *
 * Su x86 le periferiche legacy non sono mappate in memoria ma in uno spazio
 * di indirizzamento separato, raggiungibile solo con le istruzioni `in`/`out`.
 * Da C serve inline assembly: static inline cosi' il compilatore le espande
 * senza chiamata di funzione.
 */

#ifndef IO_H
#define IO_H

#include <stdint.h>

static inline void outb(uint16_t port, uint8_t value)
{
    /* "a" = valore in al, "Nd" = porta in dx (o immediato se costante < 256) */
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port)
{
    uint8_t value;
    __asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

#endif /* IO_H */

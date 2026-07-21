/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 KekkoTech Softwares Open Source (Matteo Checcacci)
 *
 * serial.c — driver minimale per la porta seriale COM1 (0x3F8).
 *
 * Perche' la seriale: il VGA text buffer e' comodo ma effimero (80x25, niente
 * storico). QEMU invece puo' redirigere COM1 sul terminale dell'host
 * (`-serial stdio`), quindi la seriale diventa il log del kernel: persistente,
 * scrollabile, e utilizzabile anche quando lo schermo e' occupato da altro.
 */

#include "serial.h"
#include "io.h"

#define COM1 0x3F8

/* Offset dei registri rispetto alla base della porta. */
#define REG_DATA        0   /* dati (e divisore basso quando DLAB=1)   */
#define REG_INT_ENABLE  1   /* interrupt enable (e divisore alto)      */
#define REG_FIFO_CTRL   2
#define REG_LINE_CTRL   3
#define REG_MODEM_CTRL  4
#define REG_LINE_STATUS 5

void serial_init(void)
{
    outb(COM1 + REG_INT_ENABLE, 0x00); /* niente interrupt: per ora polling  */
    outb(COM1 + REG_LINE_CTRL, 0x80);  /* DLAB=1: i primi due registri       */
                                       /* diventano il divisore di baud rate */
    outb(COM1 + REG_DATA, 0x03);       /* divisore = 3 -> 115200/3 = 38400   */
    outb(COM1 + REG_INT_ENABLE, 0x00);
    outb(COM1 + REG_LINE_CTRL, 0x03);  /* DLAB=0, 8 bit, no parita', 1 stop  */
    outb(COM1 + REG_FIFO_CTRL, 0xC7);  /* abilita FIFO e la svuota           */
    outb(COM1 + REG_MODEM_CTRL, 0x0B); /* DTR + RTS + OUT2                   */
}

/* Bit 5 del line status = "transmit holding register empty": finche' e' 0
 * il chip sta ancora spedendo il byte precedente e dobbiamo aspettare. */
static int serial_can_transmit(void)
{
    return inb(COM1 + REG_LINE_STATUS) & 0x20;
}

void serial_putchar(char c)
{
    /* Il terminale si aspetta CRLF, non il solo LF. */
    if (c == '\n')
        serial_putchar('\r');

    while (!serial_can_transmit())
        ;

    outb(COM1 + REG_DATA, (uint8_t)c);
}

void serial_write(const char *data, size_t size)
{
    for (size_t i = 0; i < size; i++)
        serial_putchar(data[i]);
}

void serial_puts(const char *str)
{
    for (size_t i = 0; str[i]; i++)
        serial_putchar(str[i]);
}

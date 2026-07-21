/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 KekkoTech Softwares Open Source (Matteo Checcacci)
 *
 * serial.c — minimal driver for the COM1 serial port (0x3F8).
 *
 * Why the serial port: the VGA text buffer is convenient but ephemeral —
 * 80x25 characters and no history. QEMU can redirect COM1 to the host
 * terminal (`-serial stdio`), which turns the serial port into the kernel
 * log: persistent, scrollable, and still usable when the screen is busy
 * showing something else.
 */

#include "serial.h"
#include "io.h"

#define COM1 0x3F8

/* Register offsets from the port base address. */
#define REG_DATA        0   /* data (and divisor low byte when DLAB=1)  */
#define REG_INT_ENABLE  1   /* interrupt enable (and divisor high byte) */
#define REG_FIFO_CTRL   2
#define REG_LINE_CTRL   3
#define REG_MODEM_CTRL  4
#define REG_LINE_STATUS 5

void serial_init(void)
{
    outb(COM1 + REG_INT_ENABLE, 0x00); /* no interrupts: polling for now     */
    outb(COM1 + REG_LINE_CTRL, 0x80);  /* DLAB=1 turns the first two         */
                                       /* registers into the baud divisor    */
    outb(COM1 + REG_DATA, 0x03);       /* divisor 3 -> 115200/3 = 38400 baud */
    outb(COM1 + REG_INT_ENABLE, 0x00);
    outb(COM1 + REG_LINE_CTRL, 0x03);  /* DLAB=0, 8 bits, no parity, 1 stop  */
    outb(COM1 + REG_FIFO_CTRL, 0xC7);  /* enable the FIFO and clear it       */
    outb(COM1 + REG_MODEM_CTRL, 0x0B); /* DTR + RTS + OUT2                   */
}

/* Bit 5 of the line status register is "transmit holding register empty":
 * while it reads 0 the chip is still sending the previous byte. */
static int serial_can_transmit(void)
{
    return inb(COM1 + REG_LINE_STATUS) & 0x20;
}

void serial_putchar(char c)
{
    /* Terminals expect CRLF, not a bare LF. */
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

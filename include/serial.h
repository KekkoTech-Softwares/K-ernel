/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 KekkoTech Softwares Open Source (Matteo Checcacci)
 *
 * serial.h — COM1 serial port output, the kernel logging channel.
 */

#ifndef SERIAL_H
#define SERIAL_H

#include <stddef.h>

void serial_init(void);
void serial_putchar(char c);
void serial_write(const char *data, size_t size);
void serial_puts(const char *str);

#endif /* SERIAL_H */

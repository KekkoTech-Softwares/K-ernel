# SPDX-License-Identifier: MIT
# Copyright (c) 2026 KekkoTech Softwares Open Source (Matteo Checcacci)
#
# Makefile — builds the kernel, creates the ISO and runs it under QEMU.
#
# Run this INSIDE the toolchain container (see docker/run.sh): i686-elf-gcc
# and grub-mkrescue are not available on the host.

TARGET  := i686-elf
CC      := $(TARGET)-gcc
AS      := nasm

BUILD   := build
SRC     := src
ISODIR  := $(BUILD)/isodir

KERNEL  := $(BUILD)/kernel.bin
ISO     := $(BUILD)/k-ernel.iso

# -ffreestanding: no libc, no assumptions about an underlying OS.
# -fno-stack-protector: the canary needs runtime support that does not exist.
CFLAGS  := -std=gnu99 -ffreestanding -O2 -Wall -Wextra -Iinclude \
           -fno-stack-protector -fno-builtin
ASFLAGS := -f elf32
# -lgcc: compiler support routines, such as 64-bit division.
LDFLAGS := -T linker.ld -ffreestanding -O2 -nostdlib

C_SOURCES   := $(wildcard $(SRC)/*.c)
ASM_SOURCES := $(wildcard $(SRC)/*.s)
OBJS        := $(patsubst $(SRC)/%.c,$(BUILD)/%.o,$(C_SOURCES)) \
               $(patsubst $(SRC)/%.s,$(BUILD)/%.o,$(ASM_SOURCES))
DEPS        := $(OBJS:.o=.d)

.PHONY: all iso run run-vga debug clean check

all: $(KERNEL)

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/%.o: $(SRC)/%.c | $(BUILD)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/%.o: $(SRC)/%.s | $(BUILD)
	$(AS) $(ASFLAGS) $< -o $@

$(KERNEL): $(OBJS) linker.ld
	$(CC) $(LDFLAGS) -o $@ $(OBJS) -lgcc
	@grub-file --is-x86-multiboot $@ \
		&& echo "OK: valid Multiboot header" \
		|| (echo "ERROR: missing Multiboot header" && false)

# Explicit check, handy after touching boot.s or the linker script.
check: $(KERNEL)
	grub-file --is-x86-multiboot $(KERNEL) && echo "Multiboot: OK"

iso: $(ISO)

$(ISO): $(KERNEL) grub.cfg
	mkdir -p $(ISODIR)/boot/grub
	cp $(KERNEL) $(ISODIR)/boot/kernel.bin
	cp grub.cfg $(ISODIR)/boot/grub/grub.cfg
	grub-mkrescue -o $@ $(ISODIR) 2>/dev/null

# Normal run: no graphical window (the container has no display), so the
# serial output lands straight in this terminal.
# To quit QEMU: Ctrl-C. The Ctrl-A X escape does not apply here: it only
# works when the monitor and the serial port share one channel, which is not
# the case with a plain -serial stdio.
run: $(ISO)
	qemu-system-i386 -cdrom $(ISO) -display none -serial stdio

# Renders the actual VGA screen inside the terminal (needs an interactive
# terminal). To quit: Ctrl-C.
run-vga: $(ISO)
	qemu-system-i386 -cdrom $(ISO) -display curses -serial file:$(BUILD)/serial.log

# Starts QEMU halted, waiting for GDB on port 1234.
# From a second shell inside the container:
#   gdb build/kernel.bin -ex 'target remote :1234' -ex 'break kernel_main'
debug: $(ISO)
	qemu-system-i386 -cdrom $(ISO) -display none -serial stdio -s -S

clean:
	rm -rf $(BUILD)

-include $(DEPS)

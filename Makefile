# SPDX-License-Identifier: GPL-2.0-only
# Copyright (c) 2026 KekkoTech Softwares Open Source (Matteo Checcacci)
#
# Makefile — builds the kernel, creates the ISO and runs it under QEMU.
#
# Run this INSIDE the toolchain container (see docker/run.sh): i686-elf-gcc
# and grub-mkrescue are not available on the host.

TARGET  := i686-elf
CC      := $(TARGET)-gcc
AS      := nasm

# The architecture we build for. Selects arch/$(ARCH): its sources, its
# private headers and its linker script. Portability (x86-64/ARM/RISC-V)
# starts here — the rest of the build is architecture-agnostic.
ARCH    := x86

BUILD   := build
ISODIR  := $(BUILD)/isodir

# Where sources live. "kernel/" is generic, architecture-independent code;
# "arch/$(ARCH)/" is the machine-specific part (boot, VGA, serial, port I/O).
KERNEL_DIR := kernel
ARCH_DIR   := arch/$(ARCH)

LINKER  := $(ARCH_DIR)/linker.ld

KERNEL  := $(BUILD)/kernel.bin
ISO     := $(BUILD)/k-ernel.iso

# -Iinclude holds the generic public headers only. Architecture-private
# headers (io.h) live in arch/$(ARCH) and are added, further down, *only*
# to the arch objects — so generic code cannot include io.h by accident.
# -ffreestanding: no libc, no assumptions about an underlying OS.
# -fno-stack-protector: the canary needs runtime support that does not exist.
CFLAGS  := -std=gnu99 -ffreestanding -O2 -Wall -Wextra -Iinclude \
           -fno-stack-protector -fno-builtin
ASFLAGS := -f elf32
# -lgcc: compiler support routines, such as 64-bit division.
LDFLAGS := -T $(LINKER) -ffreestanding -O2 -nostdlib

C_SOURCES   := $(wildcard $(KERNEL_DIR)/*.c) $(wildcard $(ARCH_DIR)/*.c)
ASM_SOURCES := $(wildcard $(ARCH_DIR)/*.s)
# Object files mirror the source tree under build/ (e.g. build/arch/x86/vga.o),
# so sources from different directories never collide.
OBJS        := $(patsubst %.c,$(BUILD)/%.o,$(C_SOURCES)) \
               $(patsubst %.s,$(BUILD)/%.o,$(ASM_SOURCES))
DEPS        := $(OBJS:.o=.d)

# Architecture-private include path: granted only to arch objects, so the
# machine-independent code in kernel/ physically cannot reach io.h.
$(BUILD)/$(ARCH_DIR)/%.o: CFLAGS += -I$(ARCH_DIR)

.PHONY: all iso run run-vga debug clean check

all: $(KERNEL)

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/%.o: %.s
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< -o $@

$(KERNEL): $(OBJS) $(LINKER)
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

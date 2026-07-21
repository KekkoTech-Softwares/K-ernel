# SPDX-License-Identifier: MIT
# Copyright (c) 2026 KekkoTech Softwares Open Source (Matteo Checcacci)
#
# Makefile — build del kernel, creazione ISO e avvio in QEMU.
#
# Va eseguito DENTRO il container della toolchain (vedi docker/run.sh):
# sul Mac i686-elf-gcc e grub-mkrescue non esistono.

TARGET  := i686-elf
CC      := $(TARGET)-gcc
AS      := nasm

BUILD   := build
SRC     := src
ISODIR  := $(BUILD)/isodir

KERNEL  := $(BUILD)/kernel.bin
ISO     := $(BUILD)/k-ernel.iso

# -ffreestanding: nessuna libc, nessuna assunzione su un OS sottostante.
# -fno-stack-protector: il canary richiederebbe supporto a runtime che non c'e'.
CFLAGS  := -std=gnu99 -ffreestanding -O2 -Wall -Wextra -Iinclude \
           -fno-stack-protector -fno-builtin
ASFLAGS := -f elf32
# -lgcc: routine di supporto del compilatore (es. divisioni a 64 bit).
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
		&& echo "OK: header Multiboot valido" \
		|| (echo "ERRORE: header Multiboot mancante" && false)

# Verifica esplicita, utile quando si tocca boot.s o il linker script.
check: $(KERNEL)
	grub-file --is-x86-multiboot $(KERNEL) && echo "Multiboot: OK"

iso: $(ISO)

$(ISO): $(KERNEL) grub.cfg
	mkdir -p $(ISODIR)/boot/grub
	cp $(KERNEL) $(ISODIR)/boot/kernel.bin
	cp grub.cfg $(ISODIR)/boot/grub/grub.cfg
	grub-mkrescue -o $@ $(ISODIR) 2>/dev/null

# Avvio normale: nessuna finestra grafica (nel container non c'e' un display),
# l'output della seriale finisce direttamente in questo terminale.
# Per uscire da QEMU: Ctrl-A poi X.
run: $(ISO)
	qemu-system-i386 -cdrom $(ISO) -display none -serial stdio

# Mostra il vero schermo VGA renderizzato nel terminale (richiede un
# terminale interattivo). Per uscire: Ctrl-A poi X.
run-vga: $(ISO)
	qemu-system-i386 -cdrom $(ISO) -display curses -serial file:$(BUILD)/serial.log

# Avvia QEMU fermo in attesa di GDB sulla porta 1234.
# In un secondo terminale dentro il container:
#   gdb build/kernel.bin -ex 'target remote :1234' -ex 'break kernel_main'
debug: $(ISO)
	qemu-system-i386 -cdrom $(ISO) -display none -serial stdio -s -S

clean:
	rm -rf $(BUILD)

-include $(DEPS)

# K-ernel

A 32-bit x86 (i686) kernel written from scratch.
Un kernel x86 a 32 bit (i686) scritto da zero.

**[English](#english) · [Italiano](#italiano)**

---

## English

K-ernel is a hobby operating-system kernel built from scratch to learn how a
system really works underneath: boot, segmentation, interrupts, memory
management, multitasking.

It uses no standard library: the code runs *freestanding*, with no operating
system beneath it. Booting goes through GRUB and the Multiboot standard, so we
start already in 32-bit protected mode without writing a bootloader by hand.

The tree is split between architecture-independent code (`kernel/`) and
machine-specific code (`arch/<arch>/`) from the start, so that adding x86-64,
ARM or RISC-V later does not mean rewriting everything.

### Status

The kernel boots, prints to the screen through the VGA text buffer and logs to
the serial port.

- [x] **Phase 0** — development environment (cross-compiled toolchain)
- [x] **Phase 1** — boot via GRUB/Multiboot, VGA and serial output
- [ ] **Phase 2** — GDT (segmentation)
- [ ] **Phase 3** — IDT, exceptions and hardware interrupts (timer, keyboard)
- [ ] **Phase 4** — memory management (PMM, paging, heap)
- [ ] **Phase 5** — multitasking

### Requirements

You need a **cross-compiled** toolchain for the `i686-elf` target: the system
compiler produces binaries for the OS it runs on, whereas we need a compiler
that assumes no target operating system.

To avoid installing it by hand, the project ships a Docker image with
everything needed. The only requirement on the host machine is therefore
**Docker**.

The image includes: `i686-elf-gcc` 13.2.0, GNU binutils 2.42, NASM, GNU Make,
GDB, QEMU and GRUB with `xorriso` for building bootable ISOs.

> **Note for Apple Silicon (and other ARM machines)**
> The image is pinned to `linux/amd64` because `grub-pc-bin`, which holds the
> x86 BIOS boot images, does not exist for ARM. Docker runs it under emulation.
> The first build compiles GCC and binutils from source and can take several
> hours; later builds reuse the layer cache and are instant.

### Build

All the work happens **inside the container**. The script opens a shell with
the project mounted at `/kernel`, building the image on first run:

```sh
./docker/run.sh
```

From there, inside the container:

```sh
make            # builds the kernel to build/kernel.bin
make iso        # generates the bootable image build/k-ernel.iso
make run        # boots the kernel in QEMU
```

Edits made by the editor on the host are visible in the container immediately:
no need to rebuild the image or reopen the shell.

### Running the kernel

| Command        | What it does                                                   |
| -------------- | -------------------------------------------------------------- |
| `make run`     | Boots QEMU with no graphical window; serial output arrives straight in the terminal. |
| `make run-vga` | Shows the real VGA screen rendered in the terminal; serial goes to `build/serial.log`. |
| `make debug`   | Boots QEMU halted, waiting for GDB on port 1234.               |
| `make check`   | Verifies the binary carries a valid Multiboot header.          |
| `make clean`   | Removes all build artifacts.                                   |

To quit QEMU: `Ctrl-C`.

The `Ctrl-A` `X` sequence documented in many guides does **not** work here: it
is QEMU's multiplexer escape, active only when the monitor and the serial port
share one channel (as with `-nographic`). With `-serial stdio` the serial port
has the terminal to itself and `Ctrl-C` reaches QEMU as a plain SIGINT.

#### Debugging with GDB

Run `make debug` in one terminal, then open a second shell in the container
(`docker exec -it <container> bash`) and attach:

```sh
gdb build/kernel.bin -ex 'target remote :1234' -ex 'break kernel_main' -ex continue
```

### Project layout

```
arch/x86/boot.s      Multiboot header, initial stack, entry point (_start)
arch/x86/vga.c       80x25 terminal on the VGA text buffer at 0xB8000
arch/x86/serial.c    COM1 serial port driver, used for logs
arch/x86/io.h        Port I/O (inb/outb) — x86-private; generic code never includes it
arch/x86/linker.ld   Memory layout: the kernel is loaded at 1 MiB
kernel/kernel.c      kernel_main: first C code that runs
kernel/kprintf.c     Formatted output, log levels, output channels
kernel/string.c      Freestanding string/memory helpers
kernel/ktest.c       Boot-time self-test suite
include/             Generic public headers (kprintf.h, string.h, vga.h, ...)
grub.cfg             GRUB menu entry to boot the kernel
Makefile             Build, ISO generation and QEMU launch
docker/              Cross-compilation toolchain image
```

`kernel/` is machine-independent; `arch/x86/` is everything x86-specific. The
build grants the `arch/x86` include path **only** to arch objects, so generic
code cannot include `io.h` (or any other machine header) by accident. This is
the seam that keeps future ports (x86-64, ARM, RISC-V) tractable.

### Why these choices

**GRUB instead of a custom bootloader.** Writing a bootloader is a project of
its own, full of legacy detail (real mode, BIOS, the A20 gate) that would eat
weeks before touching the kernel. GRUB loads the kernel, switches to protected
mode and hands over the memory map. A custom bootloader stays a possible
separate project.

**The kernel at 1 MiB.** Below that mark memory is taken by legacy structures:
BIOS interrupt vectors, the BIOS data area, video buffers, ROMs. 1 MiB is the
first genuinely free address.

**The serial port as a log channel.** The VGA text buffer is handy but
ephemeral: 80x25 characters and no history. QEMU can redirect the serial port
to the terminal, giving a scrollable, persistent log — indispensable once
interrupt debugging starts.

### License

Distributed under the **GNU General Public License, version 2** (GPL-2.0-only):
you may use, study, modify and redistribute the code, but derivative works must
be released under the same license and with source available. The full text is
in the [LICENSE](LICENSE) file.

The GPL-2.0 choice is deliberate and forward-looking: a long-term goal of the
project is to reuse existing Linux drivers (which are GPLv2) through a
compatibility layer. A GPLv2-compatible core avoids a license conflict at that
point.

Every source file carries an [SPDX](https://spdx.dev/) identifier at the top,
so the license stays attached to each file even when extracted from the
project.

Created and maintained by **KekkoTech Softwares Open Source**.
Copyright © 2026 KekkoTech Softwares Open Source (Matteo Checcacci).

---

## Italiano

K-ernel è un kernel di sistema operativo scritto da zero, per imparare come
funziona davvero un sistema sotto il cofano: boot, segmentazione, interrupt,
gestione della memoria, multitasking.

Non usa alcuna libreria standard: il codice gira *freestanding*, senza nessun
sistema operativo sotto. Il boot avviene tramite GRUB e lo standard Multiboot,
così da partire già in protected mode a 32 bit senza dover scrivere un
bootloader da zero.

Il codice è diviso fin da subito tra la parte indipendente dall'architettura
(`kernel/`) e quella specifica della macchina (`arch/<arch>/`), così che
aggiungere in futuro x86-64, ARM o RISC-V non significhi riscrivere tutto.

### Stato

Il kernel si avvia, scrive sullo schermo tramite il VGA text buffer e registra
i log sulla porta seriale.

- [x] **Fase 0** — ambiente di sviluppo (toolchain cross-compilata)
- [x] **Fase 1** — boot via GRUB/Multiboot, output VGA e seriale
- [ ] **Fase 2** — GDT (segmentazione)
- [ ] **Fase 3** — IDT, eccezioni e interrupt hardware (timer, tastiera)
- [ ] **Fase 4** — gestione della memoria (PMM, paging, heap)
- [ ] **Fase 5** — multitasking

### Requisiti

Serve una toolchain **cross-compilata** per il target `i686-elf`: il
compilatore di sistema produce binari per il sistema operativo su cui gira,
mentre a noi serve un compilatore che non assuma alcun sistema operativo di
destinazione.

Per evitare di doverla installare a mano, il progetto include un'immagine
Docker che contiene tutto il necessario. L'unico requisito sulla macchina host
è quindi **Docker**.

L'immagine include: `i686-elf-gcc` 13.2.0, GNU binutils 2.42, NASM, GNU Make,
GDB, QEMU e GRUB con `xorriso` per generare le ISO avviabili.

> **Nota per Apple Silicon (e altre macchine ARM)**
> L'immagine è forzata a `linux/amd64` perché `grub-pc-bin`, che contiene le
> immagini di boot per BIOS x86, non esiste per ARM. Docker la esegue in
> emulazione. La prima build compila GCC e binutils da sorgente e può
> richiedere diverse ore; le build successive riusano la cache dei layer e sono
> immediate.

### Build

Tutto il lavoro avviene **dentro il container**. Lo script apre una shell con
il progetto montato in `/kernel`, costruendo l'immagine al primo avvio:

```sh
./docker/run.sh
```

Da lì in poi, dentro il container:

```sh
make            # compila il kernel in build/kernel.bin
make iso        # genera l'immagine avviabile build/k-ernel.iso
make run        # avvia il kernel in QEMU
```

Le modifiche ai file fatte dall'editor sulla macchina host sono visibili subito
nel container: non serve ricostruire l'immagine né riaprire la shell.

### Eseguire il kernel

| Comando        | Cosa fa                                                        |
| -------------- | -------------------------------------------------------------- |
| `make run`     | Avvia QEMU senza finestra grafica; l'output della porta seriale arriva direttamente nel terminale. |
| `make run-vga` | Mostra il vero schermo VGA renderizzato nel terminale; la seriale finisce in `build/serial.log`. |
| `make debug`   | Avvia QEMU in pausa, in attesa di GDB sulla porta 1234.        |
| `make check`   | Verifica che il binario contenga un header Multiboot valido.   |
| `make clean`   | Rimuove tutti gli artefatti di build.                          |

Per uscire da QEMU: `Ctrl-C`.

La sequenza `Ctrl-A` `X` documentata in molte guide qui **non** funziona: è
l'escape del multiplexer di QEMU, attivo solo quando il monitor e la porta
seriale condividono lo stesso canale (come con `-nographic`). Con
`-serial stdio` la seriale ha il terminale per sé e `Ctrl-C` arriva a QEMU come
un normale SIGINT.

#### Debug con GDB

Avvia `make debug` in un terminale, poi apri una seconda shell nel container
(`docker exec -it <container> bash`) e collegati:

```sh
gdb build/kernel.bin -ex 'target remote :1234' -ex 'break kernel_main' -ex continue
```

### Struttura del progetto

```
arch/x86/boot.s      Header Multiboot, stack iniziale, entry point (_start)
arch/x86/vga.c       Terminale 80x25 sul VGA text buffer a 0xB8000
arch/x86/serial.c    Driver della porta seriale COM1, usata per i log
arch/x86/io.h        Port I/O (inb/outb) — privato x86; il codice generico non lo include mai
arch/x86/linker.ld   Layout di memoria: il kernel viene caricato a 1 MiB
kernel/kernel.c      kernel_main: primo codice C eseguito
kernel/kprintf.c     Output formattato, livelli di log, canali di output
kernel/string.c      Helper freestanding per stringhe/memoria
kernel/ktest.c       Suite di self test al boot
include/             Header pubblici generici (kprintf.h, string.h, vga.h, ...)
grub.cfg             Voce di menu di GRUB per avviare il kernel
Makefile             Build, generazione della ISO e avvio di QEMU
docker/              Immagine con la toolchain di cross-compilazione
```

`kernel/` è indipendente dalla macchina; `arch/x86/` è tutto ciò che è
specifico dell'x86. La build concede il percorso di include `arch/x86`
**solo** agli oggetti arch, così il codice generico non può includere `io.h`
(né altri header di macchina) per sbaglio. È questa la cucitura che tiene
gestibili i port futuri (x86-64, ARM, RISC-V).

### Perché queste scelte

**GRUB invece di un bootloader custom.** Scrivere un bootloader è un progetto a
sé, pieno di dettagli legacy (real mode, BIOS, gate A20) che ruberebbero
settimane prima ancora di toccare il kernel. GRUB carica il kernel, lo mette in
protected mode e gli passa la memory map. Un bootloader custom resta un
possibile progetto separato.

**Il kernel a 1 MiB.** Sotto quella soglia la memoria è occupata da strutture
legacy: vettori di interrupt del BIOS, area dati BIOS, buffer video, ROM. 1 MiB
è il primo indirizzo davvero libero.

**La porta seriale come canale di log.** Il VGA text buffer è comodo ma
effimero: 80x25 caratteri e nessuno storico. QEMU può redirigere la seriale sul
terminale, ottenendo un log scrollabile e persistente — indispensabile quando
si passa al debug degli interrupt.

### Licenza

Distribuito sotto **GNU General Public License, versione 2** (GPL-2.0-only):
puoi usare, studiare, modificare e ridistribuire il codice, ma le opere
derivate devono essere rilasciate sotto la stessa licenza e con il sorgente
disponibile. Il testo completo è nel file [LICENSE](LICENSE).

La scelta della GPL-2.0 è deliberata e guarda avanti: un obiettivo di lungo
termine del progetto è riusare i driver Linux esistenti (che sono GPLv2)
tramite un layer di compatibilità. Un core compatibile con la GPLv2 evita un
conflitto di licenza in quel momento.

Ogni file sorgente riporta in testa un identificatore [SPDX](https://spdx.dev/),
così la licenza resta associata al singolo file anche se viene estratto dal
progetto.

Creato e mantenuto da **KekkoTech Softwares Open Source**.
Copyright © 2026 KekkoTech Softwares Open Source (Matteo Checcacci).

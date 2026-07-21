# K-ernel

Un kernel x86 a 32 bit (i686) scritto da zero, per imparare come funziona
davvero un sistema operativo sotto il cofano: boot, segmentazione,
interrupt, gestione della memoria, multitasking.

Non usa alcuna libreria standard: il codice gira *freestanding*, senza
nessun sistema operativo sotto. Il boot avviene tramite GRUB e lo standard
Multiboot, così da partire già in protected mode a 32 bit senza dover
scrivere un bootloader da zero.

## Stato

Il kernel si avvia, scrive sullo schermo tramite il VGA text buffer e
registra i log sulla porta seriale.

- [x] **Fase 0** — ambiente di sviluppo (toolchain cross-compilata)
- [x] **Fase 1** — boot via GRUB/Multiboot, output VGA e seriale
- [ ] **Fase 2** — GDT (segmentazione)
- [ ] **Fase 3** — IDT, eccezioni e interrupt hardware (timer, tastiera)
- [ ] **Fase 4** — gestione della memoria (PMM, paging, heap)
- [ ] **Fase 5** — multitasking

## Requisiti

Serve una toolchain **cross-compilata** per il target `i686-elf`: il
compilatore di sistema produce binari per il sistema operativo su cui gira,
mentre a noi serve un compilatore che non assuma alcun sistema operativo di
destinazione.

Per evitare di doverla installare a mano, il progetto include un'immagine
Docker che contiene tutto il necessario. L'unico requisito sulla macchina
host è quindi **Docker**.

L'immagine include: `i686-elf-gcc` 13.2.0, GNU binutils 2.42, NASM, GNU
Make, GDB, QEMU e GRUB con `xorriso` per generare le ISO avviabili.

> **Nota per Apple Silicon (e altre macchine ARM)**
> L'immagine è forzata a `linux/amd64` perché `grub-pc-bin`, che contiene le
> immagini di boot per BIOS x86, non esiste per ARM. Docker la esegue in
> emulazione. La prima build compila GCC e binutils da sorgente e può
> richiedere diverse ore; le build successive riusano la cache dei layer e
> sono immediate.

## Build

Tutto il lavoro avviene **dentro il container**. Lo script apre una shell
con il progetto montato in `/kernel`, costruendo l'immagine al primo avvio:

```sh
./docker/run.sh
```

Da lì in poi, dentro il container:

```sh
make            # compila il kernel in build/kernel.bin
make iso        # genera l'immagine avviabile build/k-ernel.iso
make run        # avvia il kernel in QEMU
```

Le modifiche ai file fatte dall'editor sulla macchina host sono visibili
subito nel container: non serve ricostruire l'immagine né riaprire la shell.

## Eseguire il kernel

| Comando        | Cosa fa                                                        |
| -------------- | -------------------------------------------------------------- |
| `make run`     | Avvia QEMU senza finestra grafica; l'output della porta seriale arriva direttamente nel terminale. |
| `make run-vga` | Mostra il vero schermo VGA renderizzato nel terminale; la seriale finisce in `build/serial.log`. |
| `make debug`   | Avvia QEMU in pausa, in attesa di GDB sulla porta 1234.          |
| `make check`   | Verifica che il binario contenga un header Multiboot valido.     |
| `make clean`   | Rimuove tutti gli artefatti di build.                            |

Per uscire da QEMU: `Ctrl-A` seguito da `X`.

### Debug con GDB

Avvia `make debug` in un terminale, poi apri una seconda shell nel container
(`docker exec -it <container> bash`) e collegati:

```sh
gdb build/kernel.bin -ex 'target remote :1234' -ex 'break kernel_main' -ex continue
```

## Struttura del progetto

```
src/boot.s      Header Multiboot, stack iniziale, entry point (_start)
src/kernel.c    kernel_main: primo codice C eseguito
src/vga.c       Terminale 80x25 sul VGA text buffer a 0xB8000
src/serial.c    Driver della porta seriale COM1, usata per i log
include/        Header pubblici (vga.h, serial.h, io.h)
linker.ld       Layout di memoria: il kernel viene caricato a 1 MiB
grub.cfg        Voce di menu di GRUB per avviare il kernel
Makefile        Build, generazione della ISO e avvio di QEMU
docker/         Immagine con la toolchain di cross-compilazione
```

## Perché queste scelte

**GRUB invece di un bootloader custom.** Scrivere un bootloader è un
progetto a sé, pieno di dettagli legacy (real mode, BIOS, gate A20) che
ruberebbero settimane prima ancora di toccare il kernel. GRUB carica il
kernel, lo mette in protected mode e gli passa la memory map. Un bootloader
custom resta un possibile progetto separato.

**Il kernel a 1 MiB.** Sotto quella soglia la memoria è occupata da
strutture legacy: vettori di interrupt del BIOS, area dati BIOS, buffer
video, ROM. 1 MiB è il primo indirizzo davvero libero.

**La porta seriale come canale di log.** Il VGA text buffer è comodo ma
effimero: 80x25 caratteri e nessuno storico. QEMU può redirigere la seriale
sul terminale, ottenendo un log scrollabile e persistente — indispensabile
quando si passa al debug degli interrupt.

## Licenza

Distribuito sotto licenza **MIT**: puoi usare, modificare e ridistribuire il
codice, anche in progetti commerciali, a patto di mantenere la nota di
copyright. Il testo completo è nel file [LICENSE](LICENSE).

Ogni file sorgente riporta in testa un identificatore
[SPDX](https://spdx.dev/), così la licenza resta associata al singolo file
anche se viene estratto dal progetto.

Creato e mantenuto da **KekkoTech Softwares Open Source**.
Copyright © 2026 KekkoTech Softwares Open Source (Matteo Checcacci).

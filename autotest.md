# Autotest del kernel — guida completa

Documento di lavoro: aggiungere una suite di test che gira all'avvio del
kernel e dice, in una riga, se `string.c` e `kprintf` funzionano davvero.

Prerequisiti: `string.c` e `kprintf` già scritti e funzionanti (Step 2–5).

---

## Perché

Il kernel non ha un debugger comodo né messaggi d'errore gentili: un bug si
manifesta con un riavvio silenzioso, spesso a chilometri di distanza dal punto
in cui l'hai introdotto. Le funzioni di `string.c` sono usate ovunque —
`memcpy` verrà chiamata dal Physical Memory Manager, `memset` dall'allocatore —
quindi un errore lì dentro si propaga in modo imprevedibile.

Testarle costa poco perché **restituiscono valori confrontabili**: non devi
guardare lo schermo e fidarti dell'occhio, puoi chiedere al codice stesso se il
risultato è quello atteso.

C'è anche un motivo più sottile. Il test che avevi in `kernel_main`:

```c
kputs(buf);
kputs("MEMORY&STRINGS TEST: if you saw 5 x it is working.");
```

stampava il messaggio di successo **anche quando `memset` era vuota**. Una
verifica che annuncia successo a prescindere dal risultato non sta verificando
niente. Gli autotest risolvono esattamente questo: il verdetto lo dà il codice,
non chi guarda.

---

## Step 1 — `include/ktest.h`

Crea il file:

```c
/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 KekkoTech Softwares Open Source (Matteo Checcacci)
 *
 * ktest.h — self tests executed at boot.
 */

#ifndef KTEST_H
#define KTEST_H

void ktest_run(void);

#endif /* KTEST_H */
```

Una sola dichiarazione. Tutto il resto — la macro `CHECK`, le singole funzioni
di test, i contatori — resta `static` dentro il `.c`, perché sono dettagli
interni che nessun altro modulo deve conoscere. È lo stesso principio per cui
`print_uint` non sta in `kprintf.h`.

---

## Step 2 — `src/ktest.c`

```c
/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 KekkoTech Softwares Open Source (Matteo Checcacci)
 *
 * ktest.c — self tests executed at boot.
 */

#include "ktest.h"
#include "kprintf.h"
#include "string.h"

static int checks;
static int failed;

/* CHECK valuta la condizione e tiene il conto. In caso di fallimento stampa
 * il numero di riga e il testo esatto della condizione, senza che serva
 * scrivere una descrizione a mano. */
#define CHECK(cond)                                            \
    do {                                                       \
        checks++;                                              \
        if (cond) {                                            \
            kputchar('.');                                     \
        } else {                                               \
            failed++;                                          \
            kprintf("\n  FAIL line %d: %s\n", __LINE__, #cond); \
        }                                                      \
    } while (0)

static void test_string(void)
{
    char b[16];

    memset(b, 'x', 5);
    b[5] = '\0';
    CHECK(strlen(b) == 5);
    CHECK(b[0] == 'x' && b[4] == 'x');

    /* memset non deve scrivere oltre gli n byte richiesti */
    memset(b, 0, sizeof b);
    memset(b, 'A', 3);
    CHECK(b[3] == 0);

    CHECK(strlen("") == 0);
    CHECK(strcmp("abc", "abc") == 0);
    CHECK(strcmp("abc", "abd") < 0);
    CHECK(strcmp("abd", "abc") > 0);

    CHECK(memcmp("abc", "abc", 3) == 0);
    CHECK(memcmp("abc", "abd", 3) != 0);
    CHECK(memcmp("abc", "abd", 2) == 0);   /* si ferma a n byte */

    memcpy(b, "hello", 6);
    CHECK(strcmp(b, "hello") == 0);
}

static void test_memmove(void)
{
    char b[8];

    /* dest > src: copiando in avanti si otterrebbe "ABABABA" */
    memcpy(b, "ABCDE\0\0", 7);
    memmove(b + 2, b, 5);
    CHECK(memcmp(b, "ABABCDE", 7) == 0);

    /* dest < src: qui la copia in avanti e' corretta */
    memcpy(b, "ABCDE\0\0", 7);
    memmove(b, b + 2, 5);
    CHECK(strcmp(b, "CDE") == 0);
}

void ktest_run(void)
{
    checks = 0;
    failed = 0;

    kprintf("self tests: ");
    test_string();
    test_memmove();
    kprintf("\nself tests: %d/%d passed\n\n", checks - failed, checks);
}
```

---

## Step 3 — Chiamarla all'avvio

In `src/kernel.c`, aggiungi l'include insieme agli altri:

```c
#include "ktest.h"
```

e la chiamata dentro `kernel_main`:

```c
    serial_init();
    vga_init();

    ktest_run();
```

**Deve stare dopo `serial_init()` e `vga_init()`.** I test stampano tramite
`kprintf`, che passa da `kputchar`, che scrive su VGA e seriale: se giri prima
che quei due canali siano inizializzati, non vedi niente e non capisci perché.

Puoi anche togliere da `kernel_main` il vecchio blocco `MEMORY&STRINGS TEST`:
ora quel controllo lo fa `test_string`, in modo più rigoroso.

---

## Step 4 — Compilare e provare

Il `Makefile` trova i sorgenti con `$(wildcard src/*.c)`: `src/ktest.c` viene
raccolto da solo, non c'è nulla da modificare.

```sh
make run
```

Output atteso:

```
self tests: .............
self tests: 13/13 passed
```

Tredici punti, uno per ogni `CHECK` superato.

Se qualcosa fallisce vedrai qualcosa del genere — questo è l'output reale
ottenuto rompendo `memmove` di proposito, facendole copiare sempre in avanti:

```
self tests: ...........
  FAIL line 41: memcmp(b, "ABABCDE", 7) == 0
.
self tests: 12/13 passed
```

Riga e condizione esatta, senza aver scritto nessun messaggio d'errore a mano.

---

## Come funziona la macro `CHECK`

Tre meccanismi del preprocessore, tutti già incontrati altrove nel progetto.

**`#cond` — lo stringify.** Trasforma la condizione così com'è scritta nel
codice nella sua stringa. `CHECK(strlen(b) == 5)` produce il testo
`"strlen(b) == 5"`. È lo stesso `#` che in `version.h` trasforma il numero di
versione in stringa; qui evita di dover scrivere due volte la stessa cosa, una
per il codice e una per il messaggio.

**`__LINE__` — il numero di riga.** Macro predefinita, la riempie il
preprocessore. Insieme a `#cond` rende il messaggio autosufficiente.

**`do { ... } while (0)` — l'involucro.** Sembra inutile e non lo è. Serve a
rendere la macro **una singola istruzione**, così che

```c
if (x) CHECK(y); else z();
```

continui a funzionare. Senza l'involucro, l'espansione conterrebbe più
istruzioni fra graffe seguite da un `;`, e l'`else` resterebbe orfano con un
errore di sintassi difficile da leggere. È l'idioma standard per le macro
multi-istruzione in C: lo troverai in qualunque codebase seria.

---

## Perché proprio questi test

Non sono controlli a caso: ognuno copre un modo specifico di sbagliare.

**`CHECK(b[3] == 0)` dopo `memset(b, 'A', 3)`** verifica che `memset` **si
fermi**. Un ciclo con `<=` invece di `<` scrive un byte di troppo: funziona in
apparenza, e corrompe la memoria adiacente. Nel kernel quel byte può essere
qualunque cosa.

**`CHECK(memcmp("abc", "abd", 2) == 0)`** verifica lo stesso principio al
contrario: `memcmp` deve confrontare esattamente `n` byte e non proseguire fino
al terminatore. I due caratteri finali sono diversi apposta.

**I due test di `memmove` sono i più importanti del file.** `memmove` esiste
*solo* per gestire le aree sovrapposte: se non testi quello, stai testando una
`memcpy` con un altro nome. Il primo caso è costruito perché un'implementazione
sbagliata dia `ABABABA` invece di `ABABCDE` — la differenza si vede subito.

**`strcmp` è testata in tre direzioni** (uguale, minore, maggiore) perché un
errore tipico è restituire il segno invertito, e con il solo test di uguaglianza
non te ne accorgeresti mai.

---

## Passo successivo: testare `kprintf` stessa

I test qui sopra usano `kprintf` per stampare, ma non ne verificano l'output.
Per farlo serve poter catturare ciò che scrive in un buffer, invece che a
schermo.

La cosa è semplice proprio grazie alla scelta fatta allo Step 1 del `kprintf`:
**tutto l'output passa da `kputchar`**. Basta insegnargli a deviare:

```c
/* in kprintf.c */
static char *capture;
static size_t capture_len, capture_max;

void kprintf_capture_begin(char *buf, size_t size)
{
    capture = buf;
    capture_max = size;
    capture_len = 0;
    buf[0] = '\0';
}

void kprintf_capture_end(void)
{
    capture = NULL;
}

void kputchar(char c)
{
    if (capture) {
        if (capture_len + 1 < capture_max)
            capture[capture_len++] = c;
        capture[capture_len] = '\0';
        return;
    }
    vga_putchar(c);
    serial_putchar(c);
}
```

A quel punto i test diventano confronti esatti:

```c
static void test_kprintf(void)
{
    char out[64];

    kprintf_capture_begin(out, sizeof out);
    kprintf("%d|%u|%x|%08x|%c|%s|%%", -42, 7u, 0xAB, 0x1234, 'K', "ok");
    kprintf_capture_end();
    CHECK(strcmp(out, "-42|7|AB|00001234|K|ok|%") == 0);

    kprintf_capture_begin(out, sizeof out);
    kprintf("%d", -2147483647 - 1);
    kprintf_capture_end();
    CHECK(strcmp(out, "-2147483648") == 0);
}
```

Attenzione a una cosa: `CHECK` stampa, quindi non può essere chiamata **mentre**
la cattura è attiva. Prima si chiude la cattura, poi si confronta.

Se un giorno l'output del kernel dovrà andare anche altrove — un buffer
circolare per il log, una seconda seriale — il posto da modificare resterà uno
solo. È il motivo per cui `kputchar` è stato progettato così.

---

## Più avanti: fallire davvero

Oggi i test dicono come è andata, ma il kernel prosegue comunque. QEMU offre un
dispositivo `isa-debug-exit` che permette al codice guest di **terminare QEMU
con un codice di uscita**: da lì un target `make test` può fallire per davvero
quando i test falliscono, e la cosa diventa agganciabile a una CI su GitHub che
compila e verifica il kernel a ogni push.

Non serve adesso, ma è la naturale evoluzione quando il progetto crescerà.

---

## Se vuoi poterli disattivare

Avvolgi la chiamata in `kernel.c`:

```c
#ifdef KERNEL_TESTS
    ktest_run();
#endif
```

e aggiungi al `Makefile` un target che li accende:

```make
test: CFLAGS += -DKERNEL_TESTS
test: run
```

Finché il kernel non fa altro, però, tenerli sempre attivi è più utile: sono
istantanei e ti avvisano subito se una modifica ha rotto qualcosa.

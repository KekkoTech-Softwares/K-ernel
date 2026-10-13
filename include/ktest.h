/* SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 KekkoTech Softwares (Matteo Checcacci)
 *
 * ktest.h — self tests executed at boot.
 */

#ifndef KTEST_H
#define KTEST_H

//runs every suite. returns the number of failed checks, plus any suite that
//could not be registered: zero means "everything ran and everything passed".
int ktest_run(void);

/* Registers a suite that lives outside kernel/ktest.c — for instance in
 * arch/x86/, where the tests of a machine-specific module belong. Must be
 * called before ktest_run(): arch_init() does it. */
void ktest_register(const char * name, void (*fn)(void));

/* The counters stay private in kernel/ktest.c, so a check coming from another
 * file has to pass through a function. KCHECK is the only thing a suite uses:
 * __FILE__ and __LINE__ are expanded where the check is written, not here. */
void ktest_check(int ok, const char * file, int line, const char * expr);

#define KCHECK(cond) ktest_check((cond) ? 1 : 0, __FILE__, __LINE__, #cond)

#endif //KTEST_H
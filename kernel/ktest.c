/* SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 KekkoTech Softwares (Matteo Checcacci)
 *
 * ktest.c — self tests executed at boot.
 */

#include <stddef.h>

#include "ktest.h"
#include "kprintf.h"
#include "string.h"
#include "version.h"

//totals for the whole run. resets before each
static int checks;
static int failed;
static int suite_checks;
static int suite_failed;

//One check. KCHECK (in ktest.h) is the macro that calls it with file and line
//of the check itself, so this works just as well for a suite written in
//arch/x86/ as for the ones below.
void ktest_check(int ok, const char * file, int line, const char * expr) {
    checks++;
    suite_checks++;

    if (ok) {
        kputchar('.');
        return;
    }

    failed++;
    suite_failed++;
    kerr("\n%s:%d  %s\n", file, line, expr);
}

//Prints s followed by enough spaces to fill width columns, so the suite names lineup. 
//(%-10s would to it but i'll do it later. If i'll remind it)
static void print_padded(const char * s, int width) {
    int n = 0;

    kputs(s);

    while(s[n])
        n++;

    for(; n < width; n++)
        kputchar(' ');
}

static void test_kprintf(void) {
    char b[64];

    KCHECK(ksnprintf(b, sizeof b, "%d", 0) == 1 && strcmp(b, "0") == 0);
    KCHECK(ksnprintf(b, sizeof b, "%d", -42) == 3 && strcmp(b, "-42") == 0);

    ksnprintf(b, sizeof b, "%d", -2147483647 - 1);
    KCHECK(strcmp(b, "-2147483648") == 0);

    ksnprintf(b, sizeof b, "%u", 4294967295u);
    KCHECK(strcmp(b, "4294967295") == 0);

    ksnprintf(b, sizeof b, "%x|%X|%08x", 0xdeadbeefu, 0xdeadbeefu, 0x1234u);
    KCHECK(strcmp(b, "deadbeef|DEADBEEF|00001234") == 0);

    ksnprintf(b, sizeof b, "%b|%08b|%8b", 5u, 5u, 5u);
    KCHECK(strcmp(b, "101|00000101|     101") == 0);

    ksnprintf(b, sizeof b, "[%s][%c]%%", "ciao", 'K');
    KCHECK(strcmp(b, "[ciao][K]%") == 0);

    ksnprintf(b, sizeof b, "%p", (void *)0xB8000);
    KCHECK(strcmp(b, "0x000B8000") == 0);

    ksnprintf(b, sizeof b, "[%5d][%05d]", -42, -42);
    KCHECK(strcmp(b, "[  -42][-0042]") == 0);

    // Truncation: the return value is what wuold have been written. 
    KCHECK(ksnprintf(b, 4, "abcdef") == 6 && strcmp(b, "abc") == 0);

    // A lone trailing '%' must not read past the string. 
    ksnprintf(b, sizeof b, "fine%");
    KCHECK(strcmp(b, "fine") == 0);
}

static void test_string(void) {
    char b[16];

    memset(b, 'x', 5);
    b[5] = '\0';
    KCHECK(strlen(b) == 5);
    KCHECK(b[0] == 'x' && b[4] == 'x');

    //memset should not write over n-bytes requested
    memset(b, 0, sizeof b);
    memset(b, 'A', 3);
    KCHECK(b[3] == 0);

    KCHECK(strlen("") == 0);
    KCHECK(strcmp("abc", "abc") == 0);
    KCHECK(strcmp("abc", "abd") < 0);
    KCHECK(strcmp("abd", "abc") > 0);

    KCHECK(memcmp("abc", "abc", 3) == 0);
    KCHECK(memcmp("abc", "abd", 3) != 0);
    KCHECK(memcmp("abc", "abd", 2) == 0); //stops at n byte

    memcpy(b, "hello", 6);
    KCHECK(strcmp(b, "hello") == 0);
}

static void test_memmove(void) {
    char b[8];

    //dest > src by copying forward we'll obtain "ABABAB"
    memcpy(b, "ABCDE\0\0", 7);
    memmove(b + 2, b, 5);
    KCHECK(memcmp(b, "ABABCDE", 7) == 0);

    //dest < src in this case the copy forward is correct
    memcpy(b, "ABCDE\0\0", 7);
    memmove(b,b + 2, 5);
    KCHECK(strcmp(b, "CDE") == 0);
}

struct ktest_suite {
    const char * name;
    void (*fn)(void);
};

static const struct ktest_suite suites[] = {
    {
        "kprintf", test_kprintf
    },
    {
        "string", test_string
    },
    {
        "memmove", test_memmove
    },
};

#define KTEST_SUITE_COUNT (sizeof suites / sizeof suites[0])

//suites[] above is static const — closed inside this file. The ones coming
//from elsewhere go in a second, writable table: it is the smallest change
//that lets arch/x86/ have its own tests, and the generic ones stay read-only.
#define KTEST_MAX_EXTRA 16

static struct ktest_suite extra[KTEST_MAX_EXTRA];
static size_t extra_count;
static int dropped; //suites that did not fit, counted for the final verdict

void ktest_register(const char * name, void (*fn)(void)) {
    if (extra_count >= KTEST_MAX_EXTRA) {
        kerr("ktest: no room for suite '%s'\n", name);
        dropped++;
        return;
    }

    extra[extra_count].name = name;
    extra[extra_count].fn   = fn;
    extra_count++;
}

//runs one suite and prints its line. Extracted from ktest_run so that both
//tables are walked by the same code.
static void run_one(const struct ktest_suite * s) {
    suite_checks = 0;
    suite_failed = 0;

    print_padded(s->name, 10);
    s->fn(); //prints a dot per check

    kprintf("  %d/%d\n", suite_checks - suite_failed, suite_checks);
}

int ktest_run(void) {
    checks = 0;
    failed = 0;

    kprintf("\n---- " KERNEL_NAME " self tests ----\n");
    for(size_t i = 0; i < KTEST_SUITE_COUNT; i++)
        run_one(&suites[i]);
    for(size_t i = 0; i < extra_count; i++)
        run_one(&extra[i]);

    //The vertict goes everywhere, even if the caller sent the details to dhe serial only.
    unsigned int saved = kout_get();

    kout_set(KOUT_ALL);

    if (failed == 0 && dropped == 0)
        kinfo("all %d checks passed ;-)\n", checks);
    else if (failed)
        kerr("%d of %d checks FAILED :-(\n", failed, checks);

    //a suite that never ran cannot fail: without this line a boot with a
    //module left untested would still print "all checks passed".
    if (dropped)
        kerr("%d suites were dropped: KTEST_MAX_EXTRA is too small\n", dropped);

    kout_set(saved);

    return failed + dropped;
}
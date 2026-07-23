/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 KekkoTech Softwares Open Source Project (Matteo Checcacci)
 *
 * ktest.h — self tests executed at boot.
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

#define CHECK(cond)                                            \
    do {                                                       \
        checks++;                                              \
        suite_checks++;                                        \
        if (cond) {                                            \
            kputchar('.');                                     \
        } else {                                               \
            failed++;                                          \
            suite_failed++;                                     \
            kerr("\n%s%d  %s\n", __FILE__, __LINE__, #cond);    \
        }                                                      \
    } while (0)

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

static void test_string(void) {
    char b[16];

    memset(b, 'x', 5);
    b[5] = '\0';
    CHECK(strlen(b) == 5);
    CHECK(b[0] == 'x' && b[4] == 'x');

    //memset should not write over n-bytes requested
    memset(b, 0, sizeof b);
    memset(b, 'A', 3);
    CHECK(b[3] == 0);

    CHECK(strlen("") == 0);
    CHECK(strcmp("abc", "abc") == 0);
    CHECK(strcmp("abc", "abd") < 0);
    CHECK(strcmp("abd", "abc") > 0);

    CHECK(memcmp("abc", "abc", 3) == 0);
    CHECK(memcmp("abc", "abd", 3) != 0);
    CHECK(memcmp("abc", "abd", 2) == 0); //stops at n byte

    memcpy(b, "hello", 6);
    CHECK(strcmp(b, "hello") == 0);
}

static void test_memmove(void) {
    char b[8];

    //dest > src by copying forward we'll obtain "ABABAB"
    memcpy(b, "ABCDE\0\0", 7);
    memmove(b + 2, b, 5);
    CHECK(memcmp(b, "ABABCDE", 7) == 0);

    //dest < src in this case the copy forward is correct
    memcpy(b, "ABCDE\0\0", 7);
    memmove(b,b + 2, 5);
    CHECK(strcmp(b, "CDE") == 0);
}

struct ktest_suite {
    const char * name;
    void (*fn)(void);
};

static const struct ktest_suite suites[] = {
    {
        "string", test_string
    },
    {
        "memmove", test_memmove
    },
};

#define KTEST_SUITE_COUNT (sizeof suites / sizeof suites[0])

void ktest_run(void) {
    checks = 0;
    failed = 0;

    kprintf("\n---- " KERNEL_NAME " self tests ----\n");
    for(size_t i = 0; i < KTEST_SUITE_COUNT; i++) {
        suite_checks = 0;
        suite_failed = 0;
        print_padded(suites[i].name, 10);
        suites[i].fn(); //print dot

        kprintf("  %d/%d\n", suite_checks - suite_failed, suite_checks);
    }
    //The vertict goes everywhere, even if the caller sent the details to dhe serial only.
    unsigned int saved = kout_get();

    kout_set(KOUT_ALL);

    if (failed == 0)
        kinfo("all %d checks passed ;-)\n", checks);
    else 
        kerr("%d of %d checks FAILED :-(", failed, checks);
    
    kout_set(saved);
    
    return failed;
}
/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 KekkoTech Softwares Open Source (Matteo Checcacci)
 *
 * ktest.h — self tests executed at boot.
 */

#include "ktest.h"
#include "kprintf.h"
#include "string.h"

static int checks;
static int failed;

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

void ktest_run(void) {
    checks = 0;
    failed = 0;

    kprintf("---- K-ERNEL SELF TESTS ----");
    test_string();
    test_memmove();
    kprintf("\nself tests: %d/%d passed\n\n", checks - failed, checks);
}
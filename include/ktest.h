/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 KekkoTech Softwares Open Source (Matteo Checcacci)
 *
 * ktest.h — self tests executed at boot.
 */

#ifndef KTEST_H
#define KTEST_H

//runs every suite. returns number of failed checks.
void ktest_run(void);

#endif //KTEST_H
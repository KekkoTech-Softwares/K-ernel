/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 KekkoTech Softwares Open Source (Matteo Checcacci)
 *
 * version.h — kernel version information.
 *
 * Single source of truth for the version number.
 */

#ifndef VERSION_H
#define VERSION_H

#define KERNEL_NAME "K-ernel"
#define KERNEL_DESCRIPTION "Copyright (C) 2026 KekkoTech Softwares Open Source Project. All rights reserved."

/* Semantic versioning. */
#define KERNEL_VERSION_MAJOR 0
#define KERNEL_VERSION_MINOR 1
#define KERNEL_VERSION_PATCH 2
#define KERNEL_VERSION_NAME "Piola | Milestone 1 | pre-alpha"

#define KERNEL_STRINGIFY_(x) #x
#define KERNEL_STRINGIFY(x) KERNEL_STRINGIFY_(x)

#define KERNEL_VERSION_STRING          \
    KERNEL_STRINGIFY(KERNEL_VERSION_MAJOR) "." \
    KERNEL_STRINGIFY(KERNEL_VERSION_MINOR) "." \
    KERNEL_STRINGIFY(KERNEL_VERSION_PATCH) " - " \
    KERNEL_VERSION_NAME

/* __DATE__ and __TIME__ are filled in by the preprocessor at compile time,
 * so the build timestamp needs no support from the build system. */
#define KERNEL_BUILD_DATE __DATE__ " " __TIME__

#endif // VERSION_H

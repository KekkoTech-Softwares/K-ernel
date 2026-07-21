# SPDX-License-Identifier: MIT
# Copyright (c) 2026 KekkoTech Softwares Open Source (Matteo Checcacci)
#
# K-ernel toolchain: i686-elf cross-compiler + NASM + QEMU + GRUB + xorriso + GDB.
# See guide.md, phase 0.

# amd64 is forced because grub-pc-bin (the x86 BIOS boot images) has no arm64
# build. On Apple Silicon, Docker Desktop emulates amd64 transparently.
FROM --platform=linux/amd64 debian:bookworm-slim

ARG BINUTILS_VERSION=2.42
ARG GCC_VERSION=13.2.0
ARG TARGET=i686-elf
ARG PREFIX=/opt/cross

ENV PATH="${PREFIX}/bin:${PATH}"

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    bison \
    flex \
    libgmp3-dev \
    libmpc-dev \
    libmpfr-dev \
    libisl-dev \
    texinfo \
    wget \
    ca-certificates \
    nasm \
    make \
    gdb \
    qemu-system-x86 \
    grub-pc-bin \
    grub-common \
    xorriso \
    mtools \
    && rm -rf /var/lib/apt/lists/*

# --- Cross-compiler: binutils ---
RUN mkdir -p /usr/src/cross && cd /usr/src/cross \
    && wget -q https://ftp.gnu.org/gnu/binutils/binutils-${BINUTILS_VERSION}.tar.gz \
    && tar xf binutils-${BINUTILS_VERSION}.tar.gz \
    && mkdir build-binutils && cd build-binutils \
    && ../binutils-${BINUTILS_VERSION}/configure \
        --target=${TARGET} --prefix=${PREFIX} \
        --with-sysroot --disable-nls --disable-werror \
    && make -j"$(nproc)" \
    && make install \
    && cd / && rm -rf /usr/src/cross/binutils-${BINUTILS_VERSION} /usr/src/cross/build-binutils /usr/src/cross/binutils-${BINUTILS_VERSION}.tar.gz

# --- Cross-compiler: gcc (no headers, no libc: a "freestanding" compiler) ---
RUN cd /usr/src/cross \
    && wget -q https://ftp.gnu.org/gnu/gcc/gcc-${GCC_VERSION}/gcc-${GCC_VERSION}.tar.gz \
    && tar xf gcc-${GCC_VERSION}.tar.gz \
    && mkdir build-gcc && cd build-gcc \
    && ../gcc-${GCC_VERSION}/configure \
        --target=${TARGET} --prefix=${PREFIX} \
        --disable-nls --enable-languages=c,c++ --without-headers \
    && make -j"$(nproc)" all-gcc \
    && make -j"$(nproc)" all-target-libgcc \
    && make install-gcc \
    && make install-target-libgcc \
    && cd / && rm -rf /usr/src/cross

WORKDIR /kernel

CMD ["/bin/bash"]

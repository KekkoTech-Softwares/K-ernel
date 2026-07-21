#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 KekkoTech Softwares Open Source (Matteo Checcacci)
#
# Builda (se serve) e apre una shell nel container con la toolchain di K-ernel.
# Il progetto viene montato in /kernel dentro il container, quindi le modifiche
# ai file fatte da fuori (es. col tuo editor) sono visibili subito dentro.
set -euo pipefail

IMAGE_NAME="k-ernel-toolchain"
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

docker build --platform linux/amd64 -t "${IMAGE_NAME}" "${PROJECT_ROOT}/docker"
docker run --rm -it \
    --platform linux/amd64 \
    -v "${PROJECT_ROOT}:/kernel" \
    "${IMAGE_NAME}"

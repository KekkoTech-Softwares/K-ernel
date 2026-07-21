#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 KekkoTech Softwares Open Source (Matteo Checcacci)
#
# Builds the image if needed and opens a shell in the K-ernel toolchain
# container. The project is mounted at /kernel inside the container, so edits
# made from the host show up immediately in the build.
set -euo pipefail

IMAGE_NAME="k-ernel-toolchain"
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

docker build --platform linux/amd64 -t "${IMAGE_NAME}" "${PROJECT_ROOT}/docker"
docker run --rm -it \
    --platform linux/amd64 \
    -v "${PROJECT_ROOT}:/kernel" \
    "${IMAGE_NAME}"

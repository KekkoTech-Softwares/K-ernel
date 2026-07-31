#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-only
# Copyright (c) 2026 KekkoTech Softwares Open Source (Matteo Checcacci)
#
# Builds the image if needed and opens a shell in the K-ernel toolchain
# container. The project is mounted at /kernel inside the container, so edits
# made from the host show up immediately in the build.
set -euo pipefail

IMAGE_NAME="k-ernel-toolchain"
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DOCKER_WAIT_SECONDS=90

# The Docker CLI is useless without a running daemon: on macOS that means
# Docker Desktop must be up. Start it if needed and wait for the socket.
if ! docker info >/dev/null 2>&1; then
    if [ "$(uname -s)" != "Darwin" ]; then
        echo "The Docker daemon is not running - start it and retry." >&2
        exit 1
    fi

    echo "Docker daemon not running: starting Docker Desktop..."
    open -a Docker

    for _ in $(seq "${DOCKER_WAIT_SECONDS}"); do
        if docker info >/dev/null 2>&1; then
            break
        fi
        sleep 1
    done

    if ! docker info >/dev/null 2>&1; then
        echo "Docker Desktop did not become ready within ${DOCKER_WAIT_SECONDS}s." >&2
        exit 1
    fi
    echo "Docker is ready."
fi

docker build --platform linux/amd64 -t "${IMAGE_NAME}" "${PROJECT_ROOT}/docker"
docker run --rm -it \
    --platform linux/amd64 \
    -v "${PROJECT_ROOT}:/kernel" \
    "${IMAGE_NAME}"

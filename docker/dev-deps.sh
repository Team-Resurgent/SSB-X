#!/bin/sh
# Host-side packages the build needs: a C compiler for the host tools
# (bin2c, genromfs, gfxdis, the hosttests), make, and Python.
#
# Two package managers because the image has two bases: Debian by default,
# Alpine when built on KallistiOS's prebuilt musl toolchain image.
set -e

if command -v apk >/dev/null 2>&1; then
    # build-base drags in the gmp/mpfr/mpc/isl runtimes the cross-compiler
    # is linked against; ncurses is for sh-elf-gdb; img4dc's console.c
    # includes <linux/limits.h>, which musl leaves to linux-headers.
    apk add --no-cache bash build-base make python3 python3-dev py3-pip \
        ca-certificates ncurses-libs linux-headers
else
    export DEBIAN_FRONTEND=noninteractive
    apt-get update
    apt-get install -y --no-install-recommends \
        build-essential make python3 python3-dev python3-venv \
        ca-certificates
    rm -rf /var/lib/apt/lists/*
fi

#!/bin/sh
# The AICA's ARM7 compiler: kos-chain's
# `platform=aica` build -- arm-eabi binutils and GCC 8.5.0, libgcc built
# for -mcpu=arm7di. The ARM7DI is ARMv3 (no halfword loads, no Thumb),
# and 8.5 is the last GCC that targets it, which is why kos-chain pins it.
#
# Run in a stage FROM ${DEV_BASE}, so the compiler is linked against the
# same C library as the image it is copied into: glibc on the Debian
# path, musl on the --prebuilt Alpine one.
set -eu

if command -v apk >/dev/null 2>&1; then
    apk add --no-cache bash build-base texinfo bison flex gawk patch curl \
        git ca-certificates gmp-dev mpfr-dev mpc1-dev isl-dev zlib-dev \
        python3 xz linux-headers coreutils
else
    export DEBIAN_FRONTEND=noninteractive
    apt-get update
    apt-get install -y --no-install-recommends build-essential texinfo \
        bison flex gawk patch curl git ca-certificates libgmp-dev \
        libmpfr-dev libmpc-dev libisl-dev zlib1g-dev python3 xz-utils
    rm -rf /var/lib/apt/lists/*
fi

cd /kos/utils/kos-chain
make platform=aica toolchain_profile=stable \
    toolchain_path=/opt/toolchains/dc/arm-eabi verbose=0 \
    makejobs="$(nproc)" build

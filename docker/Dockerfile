# ssb64-dc build image. Holds everything the build needs: the
# sh-elf cross-compiler, KallistiOS built against it, the decomp the asset
# tools parse, gfxdis, and Python with the exporters' dependencies.
#
# Built by scripts/ctr.sh (`./run.sh image`) from the revisions in
# docker/pins.env. The repo is bind-mounted at /work when the image runs;
# only dc_env.sh, build_gfxdis.sh and docker/ are baked in (see
# .dockerignore), and the baserom never enters the image at all.

# Declared here so every stage can see them and no build arg goes unused.
ARG DEV_BASE=debian:bookworm-slim
ARG TOOLCHAIN_FROM=toolchain-build
ARG TOOLCHAIN_PROFILE=legacy
ARG KOS_URL
ARG KOS_REV
ARG DECOMP_URL
ARG DECOMP_REV
ARG N64_URL
ARG N64_REV
ARG SH4ZAM_URL
ARG SH4ZAM_REV

# --- the upstream trees, pinned ---------------------------------------------
FROM alpine:3.22 AS src
RUN apk add --no-cache git
COPY docker/fetch-src.sh /fetch-src.sh
ARG KOS_URL KOS_REV DECOMP_URL DECOMP_REV N64_URL N64_REV \
    SH4ZAM_URL SH4ZAM_REV
RUN /fetch-src.sh "$KOS_URL" "$KOS_REV" /src/kos
RUN /fetch-src.sh "$DECOMP_URL" "$DECOMP_REV" /src/ssb-decomp-re
RUN /fetch-src.sh "$N64_URL" "$N64_REV" /src/n64
RUN /fetch-src.sh "$SH4ZAM_URL" "$SH4ZAM_REV" /src/sh4zam
# The handful of decomp edits the port cannot express from outside the tree
# -- struct layouts a shim header cannot re-declare. One patch per reason,
# each guarded so the decomp's own N64 build is unchanged; see
# docker/patches/README.md. Applied here, at the end of the stage, so the
# layers the other trees live in stay cached when a patch changes.
COPY docker/apply-patches.sh /apply-patches.sh
COPY docker/patches /patches
RUN /apply-patches.sh /src/ssb-decomp-re /patches

# img4dc, for cdi4dc -- the ISO->CDI wrapper the disc build ends with, and the
# one step nothing in KallistiOS does. Declared and fetched here at the END of
# the stage, not in the ARG block at the top, so adding it does not move an
# instruction ahead of the clones above and cost their cache.
ARG IMG4DC_URL
ARG IMG4DC_REV
RUN /fetch-src.sh "$IMG4DC_URL" "$IMG4DC_REV" /src/img4dc

# --- the sh-elf cross-compiler ----------------------------------------------
# An hour of GCC bootstrap, so it is its own stage and almost never reruns.
# Skipped entirely when TOOLCHAIN_FROM names a prebuilt image instead.
FROM debian:bookworm-slim AS toolchain-build
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential texinfo bison flex gawk patch curl git ca-certificates \
        libgmp-dev libmpfr-dev libmpc-dev libisl-dev libelf-dev \
        libncurses-dev libexpat1-dev zlib1g-dev python3 xz-utils \
    && rm -rf /var/lib/apt/lists/*
# The whole tree, not just utils/kos-chain: the newlib fixup copies
# KOS's own sys/lock.h in from ../../include.
COPY --from=src /src/kos /kos
# No Makefile.cfg: with platform= on the command line kos-chain falls back to
# its own Makefile.dreamcast.cfg, so the configure lines match the toolchain
# this port was developed with. Only the profile and install path are ours.
ARG TOOLCHAIN_PROFILE
ARG WITH_GDB=1
# makejobs is a bare count; kos-chain adds the -j itself.
ENV KC="platform=dreamcast toolchain_path=/opt/toolchains/dc/sh-elf verbose=0"
# Compiler and debugger are separate layers so a failure in the short one
# does not throw away the hour spent on the long one.
RUN cd /kos/utils/kos-chain \
    && make $KC toolchain_profile=$TOOLCHAIN_PROFILE makejobs=$(nproc) build
RUN if [ "$WITH_GDB" = 1 ]; then \
        cd /kos/utils/kos-chain \
        && make $KC toolchain_profile=$TOOLCHAIN_PROFILE makejobs=$(nproc) gdb; \
    fi
RUN rm -rf /kos /root/.cache

# --- the AICA's ARM7 compiler -----------------------------------------------
# For the port's own sound firmware. Built on
# DEV_BASE, not Debian, so it runs in whichever dev image it lands in; a
# few minutes against the sh-elf stage's hour, and that stage stays cached.
FROM ${DEV_BASE} AS arm-build
COPY --from=src /src/kos /kos
COPY docker/arm-chain.sh /tmp/arm-chain.sh
RUN /tmp/arm-chain.sh && rm -rf /kos /tmp/arm-chain.sh /root/.cache

# --- the image everything runs in -------------------------------------------
FROM ${TOOLCHAIN_FROM} AS toolchain

FROM ${DEV_BASE} AS dev
COPY docker/dev-deps.sh /tmp/dev-deps.sh
RUN /tmp/dev-deps.sh && rm -f /tmp/dev-deps.sh

COPY --from=toolchain /opt/toolchains/dc/sh-elf /opt/toolchains/dc/sh-elf
COPY --from=src /src/kos /opt/toolchains/dc/kos
COPY scripts/dc_env.sh /opt/ssb64-dc/scripts/dc_env.sh

ENV DC_TOOLCHAIN_ROOT=/opt/toolchains/dc \
    KOS_BASE=/opt/toolchains/dc/kos \
    KOS_CC_BASE=/opt/toolchains/dc/sh-elf \
    SSB_DECOMP_DIR=/opt/ssb-decomp-re \
    SH4ZAM_DIR=/opt/sh4zam \
    SSB_GFXDIS=/opt/ssb64-dc/bin/gfxdis.f3dex2 \
    SSB64_CONTAINER=1 \
    HOME=/tmp/home \
    PYTHONDONTWRITEBYTECODE=1 \
    PATH=/opt/venv/bin:$PATH

# KallistiOS, built with the repo's own environment so the targets in the
# container see the same flags they see on a host build. The two host tools
# the build actually calls are bin2c and genromfs; the rest of utils/ needs
# libjpeg and nothing here uses it.
RUN . /opt/ssb64-dc/scripts/dc_env.sh \
    && make -C "$KOS_BASE/utils/bin2c" \
    && make -C "$KOS_BASE/utils/genromfs" \
    && make -C "$KOS_BASE/kernel" -j"$(nproc)" \
    && make -C "$KOS_BASE/addons" -j"$(nproc)" \
    && find "$KOS_BASE/kernel" "$KOS_BASE/addons" -name '*.o' -delete

# The ARM compiler comes in after KallistiOS is built, so KOS keeps its
# prebuilt stream.drv rather than rebuilding it with this compiler; only the
# port's own firmware (src/dc/aica/) is compiled with it.
COPY --from=arm-build /opt/toolchains/dc/arm-eabi /opt/toolchains/dc/arm-eabi

# gfxdis: the reference display-list disassembler tools/check/verify_dl.py
# checks our decode against. Built by the repo's own script, from the pinned
# tree.
ARG WITH_GFXDIS=1
COPY --from=src /src/n64 /tmp/n64
COPY tools/build_gfxdis.sh /tmp/build_gfxdis.sh
RUN if [ "$WITH_GFXDIS" = 1 ]; then \
        N64_DIR=/tmp/n64 OUT_DIR=/opt/ssb64-dc/bin CONF_DIR=/tmp/gfxdis-inc \
            /tmp/build_gfxdis.sh; \
    fi \
    && rm -rf /tmp/n64 /tmp/gfxdis-inc /tmp/build_gfxdis.sh

# The decomp: source text the asset exporters parse, plus the two Python
# modules they import (audio_codec, cseq_to_mid).
COPY --from=src /src/ssb-decomp-re /opt/ssb-decomp-re

# SH4ZAM. The Dreamcast build is header-only (everything it calls is
# SHZ_INLINE), so nothing links libsh4zam. source/ comes along anyway for
# the host test: off-target SH4ZAM falls back to a software XMTRX whose
# state lives in source/sw/shz_xmtrx_sw.c, since on the SH4 that state is
# the FP back-bank and needs no storage.
COPY --from=src /src/sh4zam/include /opt/sh4zam/include
COPY --from=src /src/sh4zam/source /opt/sh4zam/source

# A venv so `python3` is the interpreter with numpy and pygfxd on it.
COPY docker/requirements.txt /tmp/requirements.txt
RUN python3 -m venv /opt/venv \
    && /opt/venv/bin/pip install --no-cache-dir --upgrade pip \
    && /opt/venv/bin/pip install --no-cache-dir -r /tmp/requirements.txt \
    && rm -f /tmp/requirements.txt

# Commands run as the invoking user (scripts/ctr.sh maps the uid), who has no
# passwd entry and no home; point HOME somewhere writable so pip and gdb are
# happy.
RUN mkdir -p /tmp/home && chmod 1777 /tmp/home

# --- the disc build's tools -------------------------------------------------
# What scripts/make_cdi.sh needs beyond the cross-compiler. Deliberately the
# last layers in this stage: everything above is an hour of GCC and several
# minutes of KallistiOS, and a change down here must not throw that away.
#
# An ISO builder with sort weights: genisoimage's mkisofs takes -sort <file>,
# which is the lever the disc layout is built on.
# cdrkit is the same program on Alpine. libpng and zlib come with it because
# makeip below links them unconditionally -- its -l flag draws a PNG into the
# IP.BIN's MR logo, which this build does not use and cannot compile out.
RUN if command -v apk >/dev/null 2>&1; then \
        apk add --no-cache cdrkit libpng-dev zlib-dev; \
    else \
        export DEBIAN_FRONTEND=noninteractive; \
        apt-get update; \
        apt-get install -y --no-install-recommends genisoimage libpng-dev zlib1g-dev; \
        rm -rf /var/lib/apt/lists/*; \
    fi

# scramble (the CD bootstrap descrambles 1ST_READ.BIN as it reads it, so it
# is stored scrambled) and makeip (IP.BIN, the 32 KB bootstrap in the ISO's
# system area) ship with KallistiOS as source, and are built the way bin2c
# and genromfs are above -- after the packages, because makeip links them.
RUN . /opt/ssb64-dc/scripts/dc_env.sh \
    && make -C "$KOS_BASE/utils/scramble" \
    && make -C "$KOS_BASE/utils/makeip"

# cdi4dc, from the pinned img4dc tree. Upstream builds it with CMake; it is
# five files in three directories and adding CMake to this image to compile
# them would be the tail wagging the dog. -std=c99 (upstream's) would hide
# realpath() behind glibc's feature-test macros, so those are set here.
COPY --from=src /src/img4dc /tmp/img4dc
RUN mkdir -p /opt/ssb64-dc/bin \
    && gcc -O2 -w -D_XOPEN_SOURCE=700 -D_DEFAULT_SOURCE \
        -I/tmp/img4dc/cdi4dc/inc -I/tmp/img4dc/edc/inc -I/tmp/img4dc/common/inc \
        -o /opt/ssb64-dc/bin/cdi4dc \
        /tmp/img4dc/cdi4dc/src/*.c /tmp/img4dc/edc/src/*.c \
        /tmp/img4dc/common/src/*.c \
    && rm -rf /tmp/img4dc
ENV SSB_CDI4DC=/opt/ssb64-dc/bin/cdi4dc

# The intro-crowd baker (`make hosttest BAKER=1`) is the port's own
# draw path built for the host as a 32-bit program: the SH-4's pointers are 32
# bits and the console's video RAM and registers are mapped at their real
# addresses, both of which a 64-bit process cannot do. On x86 the host
# compiler needs its 32-bit libc (gcc -m32). Elsewhere (arm64) there is no
# multilib, so the Makefile cross-compiles an i386 binary with
# gcc-i686-linux-gnu and runs it under qemu-i386. Debian only -- musl has no
# multilib.
RUN if command -v apt-get >/dev/null 2>&1; then \
        export DEBIAN_FRONTEND=noninteractive; \
        apt-get update; \
        case "$(dpkg --print-architecture)" in \
        amd64) apt-get install -y --no-install-recommends gcc-multilib ;; \
        *) apt-get install -y --no-install-recommends \
               gcc-i686-linux-gnu libc6-dev-i386-cross qemu-user ;; \
        esac; \
        rm -rf /var/lib/apt/lists/*; \
    fi

COPY docker/entrypoint.sh /opt/ssb64-dc/entrypoint.sh
WORKDIR /work
ENTRYPOINT ["/opt/ssb64-dc/entrypoint.sh"]
CMD ["bash"]

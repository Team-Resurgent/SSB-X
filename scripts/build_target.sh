#!/bin/bash
# ssb64-dc: build one target in its directory. Runs inside the build image
# (scripts/ctr.sh); `./run.sh build` runs it.
#
# Usage: scripts/build_target.sh <target>
#
# A target is a directory under src/game/ (the game is ssb64);
# scripts/target.sh resolves the name to a directory. The Makefile bakes
# its own assets from the baserom.
#
# MTX_BACKEND=SH4ZAM in the environment builds the render core against the
# SH4's XMTRX/FTRV instead of scalar C (src/dc/mtx.h). EXTRA_CFLAGS in the
# environment reaches the compiler the same way (the debug knobs in
# src/dc/db.h); the Makefile rebuilds what either changes. EXTRA_LDFLAGS
# reaches the link line (the -Wl,--wrap=... db.h's heap probes need).
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$REPO_ROOT"

. "$SCRIPT_DIR/target.sh"
TARGET="${1:?usage: build_target.sh <target>}"
TARGET_DIR="$(target_dir "$TARGET")"

ROMDISK_ARGS=()
[ -d "$TARGET_DIR/romdisk" ] && ROMDISK_ARGS=(ROMDISK="$TARGET_DIR/romdisk")
BACKEND_ARGS=()
[ -n "${MTX_BACKEND:-}" ] && BACKEND_ARGS=(MTX_BACKEND="$MTX_BACKEND")
[ -n "${EXTRA_CFLAGS:-}" ] && BACKEND_ARGS+=(EXTRA_CFLAGS="$EXTRA_CFLAGS")
[ -n "${EXTRA_LDFLAGS:-}" ] && BACKEND_ARGS+=(EXTRA_LDFLAGS="$EXTRA_LDFLAGS")
# -j: every rule in the Makefile has one output and names its own
# prerequisites. The edge that used to be true only by its order in
# `all` -- rm-elf before the link -- is written down next to `all`; the
# comment there is where the reasoning lives. SSB64_BUILD_JOBS
# overrides the count, and 1 puts it back to one compile at a time.
JOBS="${SSB64_BUILD_JOBS:-$(nproc 2>/dev/null || echo 4)}"
make -C "$TARGET_DIR" -j"$JOBS" TARGET="$TARGET.elf" "${ROMDISK_ARGS[@]}" \
    "${BACKEND_ARGS[@]}"

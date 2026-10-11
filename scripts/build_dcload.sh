#!/bin/bash
# ssb64-dc: build one target for the dcload dev-cable path into build-dc/.
# Runs inside the build image (scripts/ctr.sh); scripts/run_hw.sh calls it
# before pushing the result with dc-tool-ser/dc-tool-ip.
#
# Usage: scripts/build_dcload.sh <target>
#
# The DISC=1 sibling of this script is scripts/make_cdi.sh; this is the
# DCLOAD=1 one -- same reason to skip romdisk.o (src/game/ssb64/Makefile),
# different reason to want an ELF of code only: not a CD's data track, but
# a /pc host-filesystem passthrough dc-tool serves at upload time (there is
# no compile-time host path -- scripts/run_hw.sh's -c is what points it at
# this target's romdisk/). No scramble/objcopy/ISO step: dc-tool -x wants
# a plain ELF, and unstripped, so DB_GDB has symbols to work with.
#
# UNVERIFIED on real hardware (no cable in hand yet): this only proves the
# build links and produces a populated romdisk/ tree.
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$REPO_ROOT"

. "$SCRIPT_DIR/target.sh"
TARGET="${1:?usage: build_dcload.sh <target>}"
TARGET_DIR="$(target_dir "$TARGET")"
[ -d "$TARGET_DIR/romdisk" ] || {
    echo "$TARGET has no romdisk/ to serve over /pc" >&2; exit 1; }

mkdir -p build-dc

# Its own ELF name, so a dcload build, a disc build and a side-loaded
# romdisk build can all exist at once without overwriting each other.
ELF="$TARGET_DIR/$TARGET-dcload.elf"

# -j, EXTRA_CFLAGS, EXTRA_LDFLAGS, MTX_BACKEND as scripts/build_target.sh and
# scripts/make_cdi.sh take them (src/dc/db.h's knobs; run_hw.sh's --gdb
# is EXTRA_CFLAGS=-DDB_GDB, its --ip is EXTRA_CFLAGS=-DDB_DCLOAD_NET).
# EXTRA_LDFLAGS carries the -Wl,--wrap=... the heap probes need
# (src/dc/db.h's DB_HEAP_GUARD).
FLAG_ARGS=()
[ -n "${MTX_BACKEND:-}" ] && FLAG_ARGS+=(MTX_BACKEND="$MTX_BACKEND")
[ -n "${EXTRA_CFLAGS:-}" ] && FLAG_ARGS+=(EXTRA_CFLAGS="$EXTRA_CFLAGS")
[ -n "${EXTRA_LDFLAGS:-}" ] && FLAG_ARGS+=(EXTRA_LDFLAGS="$EXTRA_LDFLAGS")
make -C "$TARGET_DIR" -j"${SSB64_BUILD_JOBS:-$(nproc 2>/dev/null || echo 4)}" \
    TARGET="$TARGET-dcload.elf" DCLOAD=1 "${FLAG_ARGS[@]}"

echo "built $ELF" >&2

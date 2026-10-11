#!/bin/bash
# ssb64-dc: attach sh-elf-gdb to real hardware over dc-tool's -g relay.
#
# Usage: scripts/gdb_hw.sh <target> [elf]   (./run.sh gdb-hw [elf])
#
# The other end is dc-tool-ser -g (or dc-tool-ip -g), run on the host, whose
# GDB relay listens on localhost:2159. On the guest that is KOS's gdb stub,
# brought up by src/dc/db.h's DB_GDB: the target must be built with
# -DDB_GDB or gdb_init() never runs and nothing answers the relay. This
# script runs inside the build image on the host network (scripts/ctr.sh
# run --net-host), so localhost:2159 is dc-tool-ser's relay, not the
# container's.
#
# Debug the unstripped ELF dc-tool-ser was handed. That is the src one by
# default; pass it when it is not, e.g.
#   ./run.sh gdb-hw build-dc/ssb64-dcload-soak-gdb.elf
#
# Start order:
#   1. on the host:  sudo ./run.sh run-hw --serial /dev/ttyUSB0 --gdb
#      (a DB_GDB build stops at its first instruction until this attach)
#   2. here:         ./run.sh gdb-hw [elf]
#
# KOS's stub forwards faults into GDB, so a crash lands here stopped at the
# fault with live memory. It has software breakpoints and single-step, not
# data watchpoints (external/kos/kernel/arch/dreamcast/gdb/gdb.c), so it can
# inspect a fault but cannot trap a wild writer. The port's hang watchdog
# (src/dc/db.c db_hang_watch) will report a false hang while every thread is
# stopped at a breakpoint -- expected, not a regression.
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$(cd "$SCRIPT_DIR/.." && pwd)"

. "$SCRIPT_DIR/target.sh"
TARGET="${1:?usage: gdb_hw.sh <target> [elf]}"
ELF="${2:-$(target_dir "$TARGET")/$TARGET-dcload.elf}"
[ -f "$ELF" ] || {
    echo "no $ELF -- build a DB_GDB dcload ELF first:" >&2
    echo "  EXTRA_CFLAGS=-DDB_GDB ./run.sh run-hw --serial <dev> --gdb" >&2
    exit 1; }

if ! command -v sh-elf-gdb >/dev/null 2>&1; then
    echo "no sh-elf-gdb in this image." >&2
    echo "  The prebuilt toolchain ships without it; rebuild with: ./run.sh image" >&2
    exit 1
fi

exec sh-elf-gdb "$ELF" -ex 'target remote localhost:2159'

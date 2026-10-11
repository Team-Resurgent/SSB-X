#!/bin/bash
# ssb64-dc: build a target for the dcload dev-cable path (DCLOAD=1) and
# push it to real Dreamcast hardware with dc-tool-ser or dc-tool-ip.
#
# UNVERIFIED (no coder's cable or BBA-capable console in hand yet): this
# has only ever been build-tested (scripts/build_dcload.sh, the container
# step below). The dc-tool invocation, its flags, and the whole /pc and
# GDB path on the other end of a real cable have never been run. Written
# to fail loudly -- a missing tool or a missing flag is an error, not a
# guess -- rather than pretend any of this has been proven to work.
#
# Usage:
#   scripts/run_hw.sh <target> --serial <dev> [--baud N] [--gdb]
#   scripts/run_hw.sh <target> --ip <addr> [--gdb]
#
#   target    the game (ssb64); scripts/target.sh resolves the name
#   --serial  the coder's cable device, e.g. /dev/ttyUSB0 (dc-tool-ser -t)
#   --baud    dc-tool-ser's -b, if your cable/adapter wants something
#             other than its default
#   --ip      the console's address on the BBA/LAN adapter's network
#             (dc-tool-ip -t) -- needs EXTRA_CFLAGS=-DDB_DCLOAD_NET on
#             the build (src/dc/db.h), or the console never brings the
#             network-backed dcload transport up at all
#   --gdb     also pass dc-tool's -g, opening its GDB relay on local port
#             2159. Only does anything if the build also has
#             EXTRA_CFLAGS=-DDB_GDB (src/dc/db.h) -- without it the guest
#             never calls gdb_init() and there is nothing on the other
#             end to relay. Attach with: sh-elf-gdb <elf> -ex 'target
#             remote localhost:2159' (KOS's kos-gdb wrapper does the same).
#
# Exactly one of --serial/--ip is required -- no device or address is
# guessed, since none is universally sane. EXTRA_CFLAGS and MTX_BACKEND in
# the environment reach the build the same way they do for every other
# target (src/dc/db.h, scripts/build_dcload.sh).
#
# dc-tool-ser/dc-tool-ip are not part of this repo or its image -- KOS's
# own doc/environ.sh.sample names DC_TOOLS_BASE as where they're expected
# (commonly /opt/toolchains/dc/bin); this script looks on PATH and there.
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$REPO_ROOT"

[ -f dc_env.local.sh ] && . ./dc_env.local.sh

usage() {
    echo "usage: scripts/run_hw.sh <target> --serial <dev> [--baud N] [--gdb]" >&2
    echo "   or: scripts/run_hw.sh <target> --ip <addr> [--gdb]" >&2
    exit 2
}

[ $# -ge 1 ] || usage
. "$SCRIPT_DIR/target.sh"
TARGET="$1"; shift
target_dir "$TARGET" >/dev/null

SERIAL_DEV=""
IP_ADDR=""
BAUD=""
GDB=0
while [ $# -gt 0 ]; do
    case "$1" in
        --serial) shift; SERIAL_DEV="${1:?--serial wants a device}" ;;
        --serial=*) SERIAL_DEV="${1#--serial=}" ;;
        --ip)     shift; IP_ADDR="${1:?--ip wants an address}" ;;
        --ip=*)   IP_ADDR="${1#--ip=}" ;;
        --baud)   shift; BAUD="${1:?--baud wants a number}" ;;
        --baud=*) BAUD="${1#--baud=}" ;;
        --gdb)    GDB=1 ;;
        *) echo "unknown arg: $1" >&2; usage ;;
    esac
    shift
done

if [ -n "$SERIAL_DEV" ] && [ -n "$IP_ADDR" ]; then
    echo "pass one of --serial or --ip, not both" >&2; exit 2
fi
if [ -z "$SERIAL_DEV" ] && [ -z "$IP_ADDR" ]; then
    echo "need one of --serial <dev> or --ip <addr>" >&2; usage
fi
if [ -n "$BAUD" ] && [ -z "$SERIAL_DEV" ]; then
    echo "--baud only applies to --serial" >&2; exit 2
fi

DC_TOOLS_BASE="${DC_TOOLS_BASE:-/opt/toolchains/dc/bin}"
find_tool() {
    local name="$1"
    if command -v "$name" >/dev/null 2>&1; then
        command -v "$name"; return 0
    fi
    if [ -x "$DC_TOOLS_BASE/$name" ]; then
        echo "$DC_TOOLS_BASE/$name"; return 0
    fi
    echo "no $name on PATH or in DC_TOOLS_BASE ($DC_TOOLS_BASE)" >&2
    echo "  install dcload-serial/dcload-ip's host tools, or set" >&2
    echo "  DC_TOOLS_BASE (see external/kos/doc/environ.sh.sample)" >&2
    return 1
}

if [ -n "$SERIAL_DEV" ]; then
    DC_TOOL="$(find_tool dc-tool-ser)"
else
    DC_TOOL="$(find_tool dc-tool-ip)"
fi

"$SCRIPT_DIR/ctr.sh" run ./scripts/build_dcload.sh "$TARGET"

TARGET_DIR="$(target_dir "$TARGET")"
ELF="$TARGET_DIR/$TARGET-dcload.elf"
[ -f "$ELF" ] || { echo "no $ELF after the build -- see the output above" >&2; exit 1; }

DC_TOOL_ARGS=(-x "$ELF" -c "$TARGET_DIR/romdisk")
if [ -n "$SERIAL_DEV" ]; then
    DC_TOOL_ARGS+=(-t "$SERIAL_DEV")
    [ -n "$BAUD" ] && DC_TOOL_ARGS+=(-b "$BAUD")
else
    DC_TOOL_ARGS+=(-t "$IP_ADDR")
fi
[ "$GDB" = 1 ] && DC_TOOL_ARGS+=(-g)

echo "run_hw: $DC_TOOL ${DC_TOOL_ARGS[*]}" >&2
echo "run_hw: UNVERIFIED -- see this script's header" >&2
exec "$DC_TOOL" "${DC_TOOL_ARGS[@]}"

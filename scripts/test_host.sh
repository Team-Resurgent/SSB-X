#!/bin/bash
# ssb64-dc: the headless cross-tests -- port behaviour checked against the
# decompiled N64 code, compiled for the build host. No console, no baserom.
#
#   src/dc/test  src/dc/mtx.h's algebra, scalar and sh4zam backends;
#                src/dc/input.c's controller semantics vs sys/controller.c
#   ssb64        ftCommon physics and action states vs ft/ftphysics.c et al
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$(cd "$SCRIPT_DIR/.." && pwd)"

make -C src/dc/test hosttest
make -C src/game/ssb64 hosttest

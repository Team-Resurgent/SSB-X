#!/bin/bash
# ssb64-dc: drop every build product -- objects, baked assets,
# romdisks and the disc images. Runs in the build image.
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$(cd "$SCRIPT_DIR/.." && pwd)"

for mk in src/game/*/Makefile; do
    make -C "$(dirname "$mk")" clean || true
done
rm -rf build-dc build/reloc

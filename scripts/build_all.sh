#!/bin/bash
# ssb64-dc: build every target under src/game/. Runs inside the build image.
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$REPO_ROOT"

. "$SCRIPT_DIR/target.sh"

for d in $(target_dirs); do
    n="$(basename "$d")"
    echo "=== $n"
    "$SCRIPT_DIR/build_target.sh" "$n"
done

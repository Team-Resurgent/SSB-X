#!/bin/sh
# Apply docker/patches/*.patch to a fetched upstream tree.
#
#   docker/apply-patches.sh <tree> <patchdir>
#
# A patch that no longer applies fails the image build. That is the point:
# the alternative is an image whose decomp is silently not the one the port
# was written against. (fetch-src.sh drops the .git directory before this
# runs; git apply is happy to work on a plain tree.)
set -e

tree="$1"; dir="$2"
[ -n "$tree" ] && [ -n "$dir" ] || {
    echo "usage: apply-patches.sh <tree> <patchdir>" >&2; exit 2; }
[ -d "$dir" ] || { echo "apply-patches: no $dir, nothing to do"; exit 0; }
dir=$(cd "$dir" && pwd)

for p in "$dir"/*.patch; do
    [ -e "$p" ] || { echo "apply-patches: no patches in $dir"; exit 0; }
    echo "apply-patches: $(basename "$p")"
    (cd "$tree" && git apply --verbose "$p")
done

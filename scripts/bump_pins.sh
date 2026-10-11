#!/bin/bash
# ssb64-dc: move docker/pins.env to each upstream project's current HEAD.
#
# Usage: scripts/bump_pins.sh        (scripts/bump_pins.sh)
#
# Prints what changed and stops there. Rebuild and test before trusting it:
#
#   ./run.sh image && ./run.sh build && ./run.sh test
#
# The asset exporters parse the decomp's source text -- comment formats,
# initializer layout, relocData filenames -- so a decomp bump can change
# baked output without breaking a build. That is what the tests are for.
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$REPO_ROOT"

. docker/pins.env

bump() {
    local var="$1" url="$2" old="$3" new
    new="$(git ls-remote "$url" HEAD | cut -f1)"
    [ -n "$new" ] || { echo "could not reach $url" >&2; exit 1; }
    if [ "$new" = "$old" ]; then
        echo "  $var unchanged (${old:0:7})"
    else
        sed -i "s|^$var=.*|$var=$new|" docker/pins.env
        echo "  $var ${old:0:7} -> ${new:0:7}"
    fi
}

echo "resolving upstream HEADs..."
bump KOS_REV "$KOS_URL" "$KOS_REV"
bump DECOMP_REV "$DECOMP_URL" "$DECOMP_REV"
bump N64_REV "$N64_URL" "$N64_REV"
bump SH4ZAM_REV "$SH4ZAM_URL" "$SH4ZAM_REV"
bump IMG4DC_REV "$IMG4DC_URL" "$IMG4DC_REV"

# The published toolchain tag floats; re-resolve the digest it points at now.
if command -v skopeo >/dev/null 2>&1; then
    d="$(skopeo inspect "docker://${TOOLCHAIN_IMAGE}:${TOOLCHAIN_PROFILE}" \
         2>/dev/null | sed -n 's/.*"Digest": "\(sha256:[0-9a-f]*\)".*/\1/p' | head -1)"
    if [ -n "$d" ] && [ "$d" != "$TOOLCHAIN_IMAGE_DIGEST" ]; then
        sed -i "s|^TOOLCHAIN_IMAGE_DIGEST=.*|TOOLCHAIN_IMAGE_DIGEST=$d|" docker/pins.env
        echo "  TOOLCHAIN_IMAGE_DIGEST -> ${d:0:19}"
    fi
fi

echo
echo "now: ./run.sh image && ./run.sh build && ./run.sh test"

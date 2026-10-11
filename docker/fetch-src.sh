#!/bin/sh
# Fetch one upstream tree at an exact revision, without its history.
#
#   docker/fetch-src.sh <url> <rev> <dest>
#
# Fetch-by-SHA works on GitHub and costs one commit; the full-clone fallback
# is there for a mirror that refuses it. The .git directory goes: nothing in
# the build reads it, and KallistiOS stamps a git revision into its boot
# banner when it finds one.
set -e

url="$1"; rev="$2"; dest="$3"
[ -n "$url" ] && [ -n "$rev" ] && [ -n "$dest" ] || {
    echo "usage: fetch-src.sh <url> <rev> <dest>" >&2; exit 2; }

mkdir -p "$dest"
cd "$dest"
git init -q
git remote add origin "$url"
if git fetch -q --depth 1 origin "$rev" 2>/dev/null; then
    git checkout -q FETCH_HEAD
else
    echo "fetch-src: $url refused a fetch by revision; cloning history" >&2
    git fetch -q origin
    git checkout -q "$rev"
fi
rm -rf .git

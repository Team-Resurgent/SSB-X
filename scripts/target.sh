# ssb64-dc: where a build target's directory is. Sourced, not run.
#
# A target is a directory under src/game/ with a Makefile; the game is
# src/game/ssb64 -- one main.c over src/dc/ and the decomp. Every script
# that takes a target name resolves it through this.
#
#   target_dir ssb64     ->  src/game/ssb64
target_dir() {
    local n="${1:?target_dir wants a target name}"
    if [ -f "src/game/$n/Makefile" ]; then echo "src/game/$n"; return 0; fi
    echo "no target named '$n' (looked in src/game/)" >&2
    return 1
}

# Every target, for the commands that build or clean them all.
target_dirs() {
    local d
    for d in src/game/*/Makefile; do
        [ -f "$d" ] && dirname "$d"
    done
}

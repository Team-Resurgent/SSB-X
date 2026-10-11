#!/usr/bin/env bash
# ssb64-dc: command entrypoint. Everything builds inside the container image
# (docker/Dockerfile), so a clone plus `./run.sh setup` is the whole
# environment -- no toolchain, no SDK, no submodules to check out.
#
# `./run.sh` (or `./run.sh help`) shows this menu. Commands are thin; the work is
# in scripts/. Each command is a cmd_<name> function below (a `-` in the
# name is `_` in the function); the `##` lines above it are its help text.
# Commands that run others stop at the first one that fails.
set -euo pipefail

cd "$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")"

CTR_SH=./scripts/ctr.sh

need() {
    # need <count> <usage> "$@": refuse a command missing a required argument
    local n=$1 usage=$2
    shift 2
    if [ "$#" -lt "$n" ]; then
        echo "usage: ./run.sh $usage" >&2
        exit 2
    fi
}

## Build the image: cross-compiler, KallistiOS, the decomp, gfxdis, Python.
## `./run.sh image --prebuilt` uses KallistiOS's published toolchain (amd64
## only, and without gdb) instead of compiling GCC, which takes about an hour.
cmd_image() { "$CTR_SH" build "$@"; }

## The image, the commit hook, and a first build. Run this after cloning.
cmd_setup() { cmd_image; cmd_hooks; cmd_build; }

## Refuse a commit over a red `./run.sh test`. See .githooks/pre-commit.
cmd_hooks() {
    git config core.hooksPath .githooks
    echo "hooks: core.hooksPath -> .githooks"
}

## Build the game's ELF and its assets in src/game/ssb64/: what the checks
## read. The ELF carries the romdisk and is too big to boot; `disc` boots.
## Parallel: SSB64_BUILD_JOBS=1 for one compile at a time.
cmd_build() { "$CTR_SH" run ./scripts/build_target.sh "${1:-ssb64}"; }

## The development disc: build-dc/ssb64.cdi, assets on the disc and code in
## the ELF. Debug knobs (src/dc/db.h) go in EXTRA_CFLAGS:
##   EXTRA_CFLAGS=-DDB_BOOT_SCENE=nSCKindVSBattle ./run.sh disc
## `EXTRA_CFLAGS= ./run.sh disc` is a clean build with none.
cmd_disc() { "$CTR_SH" run ./scripts/make_cdi.sh "${1:-ssb64}"; }

## The release disc: build-dc/ssb64-release.cdi. No debug facility, no log
## output, no knobs -- the build a player gets. Takes no flags (EXTRA_CFLAGS
## is refused); it has its own image, so `./run.sh disc` is not overwritten.
cmd_release() {
    RELEASE=1 EXTRA_CFLAGS= EXTRA_LDFLAGS= "$CTR_SH" run ./scripts/make_cdi.sh ssb64
}

## The soak disc: four random CPU fighters on a random stage, the results
## screen for ten seconds, then a new random match, forever -- for long
## unattended runs on a console. Rosters log as "soak: match N ..." on
## serial; DB_SOAK_SEED pins the rolls:
##   EXTRA_CFLAGS=-DDB_SOAK_SEED=1234 ./run.sh soak
cmd_soak() {
    EXTRA_CFLAGS="-DDB_SOAK ${EXTRA_CFLAGS:-}" "$CTR_SH" run ./scripts/make_cdi.sh ssb64
}

## Build a dev-cable build (DCLOAD=1) and push it to a console over
## dc-tool-ser or dc-tool-ip (scripts/run_hw.sh says what of it has run).
## `./run.sh run-hw --serial /dev/ttyUSB0` or `./run.sh run-hw --ip 192.168.1.2 --gdb`
cmd_run_hw() { ./scripts/run_hw.sh ssb64 "$@"; }

## Attach sh-elf-gdb to a DB_GDB build on a console over dc-tool's -g relay
## (localhost:2159). Start `dc-tool-ser -g` on the host first -- see
## scripts/gdb_hw.sh. Build with `EXTRA_CFLAGS=-DDB_GDB` or nothing answers.
## An argument names the ELF when it is not the default one.
cmd_gdb_hw() { "$CTR_SH" run --net-host ./scripts/gdb_hw.sh ssb64 "$@"; }

## Drop every build product
cmd_clean() { "$CTR_SH" run ./scripts/clean.sh; }

# The checks `./run.sh test` runs, in order. Each is a check_<name> function;
# the `#:` line above it is what `./run.sh test --list` prints.

#: host cross-tests: the port's C against the decomp's, headless (needs the ROM)
check_host() { "$CTR_SH" run ./scripts/test_host.sh; }
#: oracles: the exporters against gfxdis and the decomp on ROM data (SSB64_TEST_JOBS=1 for one at a time)
check_oracle() { "$CTR_SH" run ./scripts/test_oracle.sh; }
#: no fsca/fsrra where the floats must match the N64, no fmac anywhere (needs a build)
check_purity() { "$CTR_SH" run ./scripts/test_purity.sh; }
#: every scene's overlay reload clears every static it keeps (needs a build)
check_overlay() { "$CTR_SH" run python3 tools/check/overlay_check.py; }
#: the AICA reverb program equals the N64 reverb's network and decay
check_reverb() { "$CTR_SH" run python3 tools/check/reverb_check.py; }
#: src/dc/decomp/reloc_data.us.h is what the decomp's generator emits
check_reloc() { "$CTR_SH" run python3 tools/check/reloc_check.py; }
#: src/dc/itemoffsets.h agrees with the decomp's item data table
check_itoffset() { "$CTR_SH" run python3 tools/check/itoffset_check.py; }
#: the weapon-attribute offsets match the reloc header, no zeroed stand-ins
check_wpattr() { "$CTR_SH" run python3 tools/check/wpattr_check.py; }
#: no stage pack is bigger than fighter_init accepts (needs a build)
check_stgtex() { "$CTR_SH" run python3 tools/check/stgtex_check.py; }
#: every stage's ground-actor table against ef/efground.c (needs a build)
check_gractor() { "$CTR_SH" run python3 tools/check/gractor_check.py; }
#: every PVR poly header is compiled before it is sent
check_polyhdr() { "$CTR_SH" run python3 tools/check/polyhdr_check.py; }
#: every FGM id is in a sound set, and every scene's worst case fits sound RAM
check_sndsets() { "$CTR_SH" run python3 tools/export/ssb_sndsets.py --check; }
#: no baked batch draws texture alpha in the opaque list
check_packalpha() { "$CTR_SH" run python3 tools/check/packalpha_check.py; }
#: every MObj's batches follow its picture run and palette bank (needs a build)
check_mobjrun() { "$CTR_SH" run python3 tools/check/mobjrun_check.py; }
#: no verbatim line hands the renderer N64 bytes or an unbound reloc offset
check_standin() { "$CTR_SH" run python3 tools/check/standin_check.py; }
#: every VRAM write waits for the renderer first
check_vramfence() { "$CTR_SH" run python3 tools/check/vramfence_check.py; }
#: every file call goes through the one I/O lock
check_io() { "$CTR_SH" run python3 tools/check/io_check.py; }
#: models.bnd matches its manifest and the loose models, byte for byte
check_bundle() {
    "$CTR_SH" run sh -c 'python3 tools/export/ssb_bundle.py --out /tmp/models.bnd --check /tmp/models.bnd --src src/game/ssb64/romdisk --src src/game/ssb64/disc && { [ ! -f src/game/ssb64/disc/models.bnd ] || python3 tools/export/ssb_bundle.py --check src/game/ssb64/disc/models.bnd --src src/game/ssb64/romdisk --src src/game/ssb64/disc; }'
}
#: no baked texture is combed by a wrong row stride (needs a build)
check_comb() { "$CTR_SH" run python3 tools/check/comb_check.py; }
#: no FILLCOLOR camera ahead of 3D, no battle camera without a camera_mask
check_camera() { "$CTR_SH" run python3 tools/check/camera_check.py; }
#: every fighter status table row is the game's, or a listed work item
check_status() { "$CTR_SH" run python3 tools/check/status_check.py --check; }
#: each fighter's .anm tiers hold what the menus play (needs a build)
check_anmtier() { "$CTR_SH" run python3 tools/check/anm_tier_check.py --check; }
#: the VMU image tool against KOS's card layout and the game's save
check_vmu() { "$CTR_SH" run python3 tools/check/vmuimg.py selftest; }

# What CI runs on every push: no ROM, no build. The reloc header
# is generated first; reloc reads it. standin needs the ROM-built
# gusinf.c, so it is local-only.
CI_CHECKS="io polyhdr vramfence camera vmu status reloc reverb"

## Run the checks that need no console: all of them, or the ones named.
## `./run.sh test`, `./run.sh test io camera`, `./run.sh test --list`.
## `./run.sh test --ci` is the set CI runs: no ROM, no build.
cmd_test() {
    local all c
    all=$(awk '/^check_[a-z]+\(\)/ { n = $1; sub(/^check_/, "", n); sub(/\(\).*/, "", n); print n }' "${BASH_SOURCE[0]}")
    case ${1:-} in
        --list)
            awk '/^#: / { doc = substr($0, 4); next }
                 /^check_[a-z]+\(\)/ { n = $1; sub(/^check_/, "", n); sub(/\(\).*/, "", n)
                                       printf "  %-10s %s\n", n, doc }' "${BASH_SOURCE[0]}"
            return ;;
        --ci)
            "$CTR_SH" run make -C src/game/ssb64 ../../dc/decomp/reloc_data.us.h
            python3 tools/check/rom_leak_check.py --no-rom
            set -- $CI_CHECKS ;;
        "") set -- $all ;;
    esac
    for c in "$@"; do
        if ! declare -F "check_$c" >/dev/null; then
            echo "run.sh: no check '$c' -- ./run.sh test --list names them" >&2
            exit 2
        fi
    done
    for c in "$@"; do
        "check_$c"
    done
}

## This menu
cmd_help() {
    echo "usage: ./run.sh <command> [args...]"
    echo
    # each command's name, then its help text indented beneath it
    awk '
        /^## /  { doc[n++] = substr($0, 4); next }
        /^##$/  { doc[n++] = ""; next }
        /^cmd_[a-z_]+\(\)/ {
            name = $1; sub(/^cmd_/, "", name); sub(/\(\).*/, "", name)
            gsub(/_/, "-", name)
            print name
            for (i = 0; i < n; i++) print "    " doc[i]
            n = 0; next
        }
        { n = 0 }
    ' "${BASH_SOURCE[0]}"
}

main() {
    local name=${1:-help}
    [ "$#" -gt 0 ] && shift
    case $name in
        -h|--help|--list|-l) name=help ;;
    esac
    local fn=cmd_${name//-/_}
    if [[ ! $name =~ ^[a-z][a-z-]*$ ]] || ! declare -F "$fn" >/dev/null; then
        echo "run.sh: no command '$name' -- ./run.sh help lists them" >&2
        exit 2
    fi
    "$fn" "$@"
}

main "$@"

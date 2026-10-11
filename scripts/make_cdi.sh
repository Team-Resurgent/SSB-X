#!/bin/bash
# ssb64-dc: package a target as a bootable Dreamcast disc image (.cdi).
#
# Usage: scripts/make_cdi.sh [target]         (./run.sh disc [target])
#   target    the game (ssb64, the default)
#
# Runs inside the build image (scripts/ctr.sh). Output: build-dc/<target>.cdi
#
# Why a disc at all: the romdisk is linked into the ELF, 13 MB of a 14 MB image
# against 16 MB of RAM, and the disc is where those bytes belong. So this
# builds the target with DISC=1 -- no romdisk.o, an ELF of code only -- and
# writes the same romdisk/ directory into the disc's data track, where the
# loaders find it at /cd (src/dc/assetroot.h) instead of /rd.
#
# The pipeline is the one every self-booting Dreamcast disc uses, and the
# reference for it here is ~/code/garbage-bus/scripts/make_cdi.sh:
#
#   .elf -> objcopy -O binary   drop the ELF wrapper; the SH4 is handed a
#                               flat image loaded at 0x8c010000
#        -> scramble            the CD bootstrap descrambles 1ST_READ.BIN as
#                               it reads it, so it is stored scrambled
#        -> makeip              IP.BIN: the 32 KB bootstrap and metadata that
#                               live in the ISO's system area -- the region
#                               and the peripherals among them (the ip_check
#                               below)
#        -> mkisofs             ISO9660, with every LBA offset for a second
#                               session (the -C below), files laid out in the
#                               order the game reads them (the -sort below)
#        -> cdi4dc              wrap it in a Discjuggler container
#        -> disc_check.py       read the ISO back and prove the layout took
#
# -C 0,11702 is not arbitrary: a self-booting CD is a MIL-CD, an audio track
# followed by a data session that starts at LBA 11702, and every LBA recorded
# inside the ISO has to be written as if the image lived there or the
# bootstrap follows the directory records into empty space. It has to agree
# with cdi4dc's mode: no flag means "Audio/Data from a MSINFO 11702 ISO".
#
# -l (31-character names) is load-bearing, for mnitemswitch.spr. Joliet and
# Rock Ridge are not: nothing in the port ever calls readdir -- every path is
# explicit -- and KOS's fncompare (fs_iso9660.c) is case-insensitive and
# strips the ";1", so the port's lowercase names resolve against a plain
# ISO9660 volume.
#
# -sort is. Without it mkisofs writes the tree alphabetically,
# which interleaves every scene's files with every other's; tools/
# disc_layout.py emits the weights from its ordered table and
# tools/check/disc_check.py reads the finished image back and asserts the
# extents really ascend in that order -- because -sort is a hint to a tool we do
# not control, and a layout that quietly stopped applying would look exactly
# like one that worked.
#
# The pad (disc_layout.PAD) is a file of zeros sorted in front of all of
# that, sized so the image ends 20 seconds short of an 80-minute disc's
# last lead-out. The GD-ROM drive reads a CD at constant angular velocity,
# so the game's bytes stream about twice as fast at the outer edge as
# at the inner edge, where an unpadded image leaves them. The image is
# ~680 MB and the .cdi ~780 MB either way. SSB_DISC_PAD=0
# leaves the pad off, for a quicker build when only the bytes matter.
# NOT YET TESTED ON REAL HARDWARE (2026-09-24).
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$REPO_ROOT"

. "$SCRIPT_DIR/target.sh"
TARGET="${1:-ssb64}"
TARGET_DIR="$(target_dir "$TARGET")"
[ -d "$TARGET_DIR/romdisk" ] || {
    echo "$TARGET has no romdisk/ to put on a disc" >&2; exit 1; }

SCRAMBLE="$KOS_BASE/utils/scramble/scramble"
MAKEIP="$KOS_BASE/utils/makeip/makeip"
CDI4DC="${SSB_CDI4DC:-/opt/ssb64-dc/bin/cdi4dc}"
for tool in "$SCRAMBLE" "$MAKEIP" "$CDI4DC"; do
    [ -x "$tool" ] || { echo "missing $tool -- rebuild the image (./run.sh image)" >&2
                        exit 1; }
done
command -v mkisofs >/dev/null || {
    echo "mkisofs not in the image -- rebuild it (./run.sh image)" >&2; exit 1; }

# Its own ELF name, so a disc build and the romdisk build (`./run.sh build`,
# what the checks read) can both exist: they differ only in whether
# romdisk.o is linked.
ELF="$TARGET_DIR/$TARGET-disc.elf"
# RELEASE=1 (./run.sh release): the player's build -- see the Makefile. Its own
# image and staging directory, so it never overwrites the development disc.
SUFFIX=""
if [ "${RELEASE:-0}" = "1" ]; then
    [ -z "${EXTRA_CFLAGS:-}${EXTRA_LDFLAGS:-}" ] || {
        echo "RELEASE=1 takes no EXTRA_CFLAGS or EXTRA_LDFLAGS" >&2; exit 1; }
    ELF="$TARGET_DIR/$TARGET-release.elf"
    SUFFIX="-release"
fi
# -j for the same reason scripts/build_target.sh passes it, and with the
# same override.
# EXTRA_CFLAGS, EXTRA_LDFLAGS and MTX_BACKEND as scripts/build_target.sh
# takes them: the disc is the one build with the memory for the menus, the
# results screen and a battle of more than one fighter, so it is the one
# the debug knobs (src/dc/db.h) most often want to steer.
FLAG_ARGS=()
[ -n "${MTX_BACKEND:-}" ] && FLAG_ARGS+=(MTX_BACKEND="$MTX_BACKEND")
[ -n "${EXTRA_CFLAGS:-}" ] && FLAG_ARGS+=(EXTRA_CFLAGS="$EXTRA_CFLAGS")
[ -n "${EXTRA_LDFLAGS:-}" ] && FLAG_ARGS+=(EXTRA_LDFLAGS="$EXTRA_LDFLAGS")
[ "${RELEASE:-0}" = "1" ] && FLAG_ARGS+=(RELEASE=1)
make -C "$TARGET_DIR" -j"${SSB64_BUILD_JOBS:-$(nproc 2>/dev/null || echo 4)}" \
    TARGET="$(basename "$ELF")" DISC=1 "${FLAG_ARGS[@]}"

STAGE="build-dc/disc/$TARGET$SUFFIX"
CDI="build-dc/$TARGET$SUFFIX.cdi"
rm -rf "$STAGE"
mkdir -p "$STAGE/iso" build-dc

# -R .stack is what KOS's own elf2bin does: the section is NOBITS reserve and
# emitting it would pad the binary by the whole stack size.
sh-elf-objcopy -R .stack -O binary "$ELF" "$STAGE/game.bin"
"$SCRAMBLE" "$STAGE/game.bin" "$STAGE/iso/1ST_READ.BIN"

# The asset tree the exporters already write, verbatim.
cp -a "$TARGET_DIR/romdisk/." "$STAGE/iso/"
# disc/ holds what is built for the disc and deliberately kept out of the
# ELF's romdisk (efslash.mdl; the Makefile's DISC_ONLY). The disc is the
# one place it can be read from.
[ -d "$TARGET_DIR/disc" ] && cp -a "$TARGET_DIR/disc/." "$STAGE/iso/"

# models.bnd (tools/export/ssb_bundle.py): the battle's 124 small models as one
# file, which src/dc/assetroot.c mounts at boot and serves by name. With
# it on the disc the loose copies are dead weight -- 124 extents and most
# of a sector of padding each -- so they are left off. NOT YET TESTED ON
# REAL HARDWARE (2026-09-22): SSB_DISC_NO_BUNDLE=1 builds the loose-file
# disc instead, for a hardware round-trip that tests one thing at a time.
if [ -f "$STAGE/iso/models.bnd" ]; then
    if [ "${SSB_DISC_NO_BUNDLE:-0}" = "1" ]; then
        rm -f "$STAGE/iso/models.bnd"
    else
        python3 tools/export/ssb_bundle.py --names | while read -r n; do
            rm -f "$STAGE/iso/$n"
        done
    fi
fi

# The file src/dc/assetroot.c probes for, and the line the boot log prints:
# it is how a serial log says which medium the run read from. Never written
# into the target's romdisk/ -- that is exactly the ambiguity it settles.
STAMP="$TARGET$SUFFIX $(date -u '+%Y-%m-%dT%H:%MZ')"
if command -v git >/dev/null 2>&1 && git rev-parse --short HEAD >/dev/null 2>&1
then
    STAMP="$STAMP $(git rev-parse --short HEAD)"
fi
echo "$STAMP" > "$STAGE/iso/disc.id"

# Empty for now: disc_layout needs it in the tree to sort it first, and
# its size is only known once mkisofs has measured everything else.
PAD="$STAGE/iso/$(python3 -c 'import sys; sys.path.insert(0, "tools/export"); import disc_layout; print(disc_layout.PAD)')"
[ "${SSB_DISC_PAD:-1}" = "0" ] || : > "$PAD"

# Every file the disc will carry now exists, so the layout can be computed:
# one "<path> <weight>" line each, highest weight nearest the start of the
# data track. A file in the tree that the manifest does not name is an
# error there, which is what keeps the table and the image together.
python3 tools/export/disc_layout.py "$STAGE/iso" -o "$STAGE/sort.txt"

# What the disc declares about itself: the regions it is playable in and
# the hardware it needs. Both are fields a console reads before it reads a
# byte of the game, and makeip's defaults for both are wrong for this port
# -- "supports a gun, a keyboard and a mouse, requires no controller
# feature at all". tools/check/ip_check.py holds the truth as named bits, prints
# makeip's flags from them here, and reads the finished image back below to
# assert they took, for tools/check/disc_check.py's reason: makeip is a tool we
# do not control.
mapfile -t IP_ARGS < <(python3 tools/check/ip_check.py --makeip-args)
"$MAKEIP" -f "${IP_ARGS[@]}" "$STAGE/IP.BIN" >/dev/null

MKISOFS_ARGS=(-C 0,11702 -V "SSB64DC" -G "$STAGE/IP.BIN" -l
              -sort "$STAGE/sort.txt")

# The pad's size: the image's length with it empty, measured, and the
# rest of the disc made up in zeros. truncate makes the file sparse, so
# only mkisofs's copy of the zeros is ever written.
if [ -f "$PAD" ]; then
    SECTORS=$(mkisofs "${MKISOFS_ARGS[@]}" -print-size "$STAGE/iso" 2>/dev/null)
    truncate -s "$(python3 tools/export/disc_layout.py --pad-bytes "$SECTORS")" "$PAD"
fi

mkisofs "${MKISOFS_ARGS[@]}" -o "$STAGE/data.iso" "$STAGE/iso" 2>/dev/null

python3 tools/check/disc_check.py "$STAGE/data.iso"
# The same image, read for its first 256 bytes: mkisofs -G put IP.BIN at the
# front of the volume's system area, so this is the header the console reads.
python3 tools/check/ip_check.py --quiet "$STAGE/data.iso"

"$CDI4DC" "$STAGE/data.iso" "$CDI" >/dev/null

# Both numbers, because the split between them is the point of the exercise:
# the ELF should hold code and nothing else, and every asset byte should be
# on the disc.
echo
echo "disc: $CDI ($(du -h "$CDI" | cut -f1))"
echo "  1ST_READ.BIN: $(stat -c %s "$STAGE/iso/1ST_READ.BIN") bytes"
echo "  data track:   $(du -sb --exclude="$(basename "$PAD")" "$STAGE/iso" | cut -f1) bytes in $(ls -1 "$STAGE/iso" | grep -vxc "$(basename "$PAD")") files"
[ -f "$PAD" ] && echo "  pad:          $(stat -c %s "$PAD") bytes in front of them"
echo "  stamp:        $STAMP"
sh-elf-size "$ELF" | sed 's/^/  /'

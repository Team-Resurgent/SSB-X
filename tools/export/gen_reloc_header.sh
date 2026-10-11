#!/bin/sh
# Generate src/dc/decomp/reloc_data.us.h with the decompilation's OWN
# generator, rather than a hand-kept list.
#
# Why this exists
# ---------------
# Every ROM file, and every block inside one, has a symbol in the game's
# link: llMarioMainFileID, llGRCastleMapMapHeader, llNessMainMotion...
# There are 5,048 of them at the current pin, and the count is not this
# comment's to keep true. The decomp does not check them in either --
# tools/genRelocSymbols.py derives them from tools/relocFileDescriptions.us.txt
# and the per-file manifests, so they stay true as blocks are converted,
# and ssb-decomp-re/Makefile:510 runs exactly this command.
#
# Before this, src/dc/decomp/reloc_data.h declared 82 of them by hand (the
# stage map ids mp/mpcollision.c's file table needs) and nothing else. That
# one gap is why 151 of the 209 decomp sources that would not compile
# against the port did not compile: not a portability problem, a
# declaration that was never written down. With the generated header on the
# include path, 97 of them compile clean -- 27 sc/scsubsys, 20 it/itcommon,
# 13 it/itmonster, 14 of the 15 wp/ weapon files, 6 more common statuses,
# and three of the 47 missing common statuses (attacks4, capturecaptain,
# capturekirby) that no other blocker holds back.
#
# What the declarations mean, and what they do NOT do
# ---------------------------------------------------
# These are absolute linker symbols: the *address* of llGRCastleMapFileID
# is the file id (0x103), not its contents, which is why the game says
# `&llGRCastleMapFileID` everywhere. This script emits only the C
# declarations. The values live in the companion linker script, which the
# decomp passes to ld as a plain input file (its Makefile:157). The port
# does not link it yet: nothing it compiles today references a reloc symbol
# except mpcollision.c's file table, whose 82 entries src/dc/mpshim.c
# defines as zeroed ints and which the port never indexes (stage geometry
# comes from the packs). Adopting a decomp file that really reads one is
# what makes the linker script necessary.
#
# --converted-files "": the decomp passes $(RELOC_C_FILES) so that offsets
# inside converted files are recomputed from their manifests. That only
# moves values, and the port takes no values from here -- so the cheap arm
# is the honest one, and it does not depend on the decomp's build tree
# existing.
set -eu

OUT=${1:?usage: gen_reloc_header.sh <output-header>}
SSB_DECOMP_DIR=${SSB_DECOMP_DIR:-/opt/ssb-decomp-re}

GEN=$SSB_DECOMP_DIR/tools/genRelocSymbols.py
DESC=$SSB_DECOMP_DIR/tools/relocFileDescriptions.us.txt
[ -f "$GEN" ]  || { echo "gen_reloc_header: no $GEN" >&2; exit 1; }
[ -f "$DESC" ] || { echo "gen_reloc_header: no $DESC" >&2; exit 1; }

TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

# Run from a scratch directory: the generator's import chain creates
# relative output directories, and ssb-decomp-re must stay untouched.
(cd "$TMP" && python3 "$GEN" "$DESC" "$TMP/reloc_data.us.h" \
    "$TMP/reloc_data_symbols.us.txt" --converted-files "" >/dev/null)

# The linker script is not installed -- see the note above -- but generating
# it is not optional (the tool writes both), and its line count is the
# check that the header is complete.
H=$(grep -c '^extern' "$TMP/reloc_data.us.h")
L=$(grep -c '^ll' "$TMP/reloc_data_symbols.us.txt")
[ "$H" -eq "$L" ] || { echo "gen_reloc_header: $H decls vs $L symbols" >&2; exit 1; }
[ "$H" -gt 5000 ] || { echo "gen_reloc_header: only $H declarations" >&2; exit 1; }

# Write only on change, so make does not rebuild the world for a no-op.
if [ -f "$OUT" ] && cmp -s "$TMP/reloc_data.us.h" "$OUT"; then
    exit 0
fi
mkdir -p "$(dirname "$OUT")"
cp "$TMP/reloc_data.us.h" "$OUT"
echo "gen_reloc_header: $OUT ($H declarations)"

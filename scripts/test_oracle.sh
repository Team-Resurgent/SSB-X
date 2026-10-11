#!/bin/bash
# ssb64-dc: the oracle checks -- our converters against independent
# implementations, on real ROM data.
#
#   figatree_check.py  the decomp's animation interpreter, as the port
#                      builds it, vs a Python reading of the same scripts
#   verify_dl.py       our F3DEX2 decode vs glankk's gfxdis
#   sprite_check.py    our TMEM row unshuffle vs the decomp's
#   texfmt_check.py    our N64 texel decode vs the decomp's
#   texshuf_check.py   which extracted images are stored row-shuffled and
#                      which are not: SP_TEXSHUF on every stage wallpaper,
#                      and the pixels themselves on the model tiles, which
#                      carry no flag to read
#   rendermode_check.py our RDP render-mode read -- which PVR list every
#                      stage batch lands in -- and our texture-filter read
#                      vs gfxdis
#   bgm_check.py       our compressed-MIDI reader and sequence player
#                      vs the decomp's
#   fgm_check.py       our gmFGMVoiceID numbering vs a C compiler's,
#                      and what the AICA transcode costs
#   vadpcm_check.py    our VADPCM decode vs the decomp's, and the one
#                      line where it deliberately differs
#   transition_check.py the eleven screen wipes' UVs against the bands
#                      their own display lists load
#   emblem_check.py    the ten series emblems' material animation: the
#                      sliced, rebased scripts replayed against the
#                      originals still in the ROM
#   spotlight_check.py the character select's spotlight mask: the packed,
#                      whitened, twiddled texels walked back to the
#                      intensity bytes in the ROM
#   slash_check.py     the damage slash's materials: its MatAnimJoints
#                      replayed out of the pack against the ROM's, and
#                      the eight and five frames of its two sprite arrays
#                      checked to be eight and five different pictures
#   spark_check.py     the damage sparks' material: the same replay, plus
#                      the flags-zero default gcDrawMObjForDObj reads as
#                      TEXTURE|TILESIZE|ALPHA, and the seven frames of its
#                      sprite array matched to the ROM's by how many TLUT
#                      colours each one uses
#   mdust_check.py     the metal dust's material: the sparks' checks again
#                      over four blocks of its own (both run through
#                      tools/check/flipbook_check.py), with each frame's texel
#                      values matched exactly against the IA16 sprite the
#                      ROM has it at
#   dexp_check.py      the dead explosion's model: that the two reloc
#                      symbols really are named the wrong way round, that
#                      every one of its three shards lerps between an
#                      environment and a primitive colour, that its four
#                      MatAnimJoints -- one per player, and nothing else
#                      in the game has more than one -- replay out of the
#                      pack as they do out of the ROM, and how far the
#                      PVR's base colour clamps where PRIM is the darker
#   backup_check.py    the save data's defaults: 879 fields of hand-copied
#                      initializer against the ROM's own .data, the three
#                      struct sizes against the symbols that span them,
#                      and the game's checksum over the ROM's bytes
#   pack_anim_check.py every exported AnimJoint/MatAnimJoint script (stage
#                      map/layer/ground/bonus/boss blocks, effect, item and
#                      weapon packs) decoded event by event and replayed
#                      against the script it was sliced from in the ROM
#   effect_model_check.py every effect/weapon/scene-model pack ssb_effectexport
#                      writes (79): DObjDesc tree, MObjSub fields, batch and
#                      joint to MObj maps, geometry, textures and palettes
#                      against a fresh MeshBaker bake of the ROM, and which
#                      script each joint / MObj got
#   model_pack_check.py the fighter (27), stage (38 + the bonus platforms)
#                      and item/weapon/alt (61) model packs against a second
#                      F3DEX2 interpreter that shares no code with the baker
#                      (raw words, relocation-chain pointers, its own
#                      gcDrawMObjForDObj port, gfxdis as a third reader of
#                      the vertex loads and triangles): per-triangle
#                      position, owner joint, normal or colour, alpha, UV in
#                      texels, texturedness, list/cull/filter bits, shaded,
#                      PRIM/light/ENV colours, texture identity, and every
#                      texel through a TMEM replay of its own; --selftest
#                      mutates a Mario pack and demands every change is caught
#   camanim_check.py   the thirteen .cam banks' camera scripts (55 in all)
#                      decoded and replayed against the ROM scripts they
#                      were cut from, with their link labels and extents
#   anm_body_check.py  the fighter .anm files the real exporter ships (27
#                      packs, every tier prefix as the loader cuts it):
#                      slot tables, words, every script's event stream and
#                      a frame replay against the figatree / AnimJoint it
#                      was cut from in the ROM
#   shade_check.py     the fighter core's lighting: PRIM x SHADE in the
#                      RSP's and RDP's order over every fighter batch, and
#                      the light direction a stage's two angles become
#                      against ftDisplayLightsDrawReflect's bytes
#   particle_check.py  the nine particle banks: eighteen exported, swapped
#                      byte ranges against all eighteen of the decomp's
#                      own src/particles sources compiled for the host
#   particletex_check.py the same nine banks' 246 images, converted for
#                      the PVR: every texel against the decomp's own
#                      extractParticleTextures.py, and every frame of the
#                      packs untwiddled back to it
#   joint_check.py     where every joint id a fighter's data names points:
#                      hurtboxes, effect joints, feet, hidden parts and
#                      the motion table's hidden-part bits, over all
#                      twelve fighters, under the game's base
#                      joints[4 + k] -- and what the port's joints[1 + k]
#                      makes of the same ids; then each
#                      fighter's pack built and read back, its setup mask
#                      and hidden-part rows against the source's and
#                      every animation's raw slot table against the
#                      figatree header in the ROM
#   sintable_check.py  gSYSinTable's address, derived from the decomp's
#                      own symbol and segment, and its shape -- which is
#                      not the sine function's, and is why it is cut from
#                      the ROM rather than computed
#   lbparticle_check.py the particle interpreter: src/dc/lbparticle.c
#                      against the lb/lbparticle.c it was copied from,
#                      line by line and then run for run over all 119
#                      scripts of the efcommon bank; then the renderer
#                      driven over the same scene in both builds, the
#                      decomp's F3DEX2 decoded back into the port's own
#                      draw records field by field; and then the one
#                      thing a pair of little-endian processes cannot
#                      see about each other, lbParticleReadFloatBigEnd
#                      against Python's own big-endian float, at every
#                      offset of all nine script banks
#   efmanager_check.py the effect manager: src/dc/efmanager.c against the
#                      ef/efmanager.c it was copied from, run by run line
#                      by line with nothing but comments between them,
#                      and the two functions it edited named rather than
#                      described; then every particle script id the copy
#                      asks for, read back out of the copy, against the
#                      exported efcommon bank. Twenty-three runs and two
#                      named divergences: the model path, and the four
#                      lines of the screen quake's maker that read an
#                      AnimJoint table out of a file the port does not
#                      load
#   fpu_check.py       the fused multiply-add: every code range the
#                      decomp's segment list names, scanned for one, so
#                      that "the N64 could not do this" is a fact about
#                      the ROM and not about the manual
#   trig_check.py      libultra's __sinf and __cosf: the copies against
#                      libultra/gu's own source line by line, their
#                      constant tables against the ROM's .rodata word by
#                      word, and both functions swept against the C
#                      library's to say how far apart they are
#
# All thirty-one read the baserom, so they are skipped where it is absent.
#
# They run concurrently, and their output is held in files and replayed in
# the order listed above, so a parallel run's log reads exactly like a
# serial one's. There is nothing between them to serialize: each is a
# separate process that reads the ROM, builds whatever C it needs in a
# tempdir of its own, and writes nothing into the tree. The one visible
# difference is that a red run now names every check that failed instead of
# stopping at the first. SSB64_TEST_JOBS=1 puts this level back to one check
# at a time -- figatree_check.py keeps its own pool, and takes --jobs 1 to
# give that up as well.
#
# figatree_check.py is most of the suite's work by itself and parallelises
# internally as well (--jobs), so what is left here is the other twenty-four
# riding alongside it rather than after it.
set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$(cd "$SCRIPT_DIR/.." && pwd)"

ROM="baserom.z64"
if [ ! -f "$ROM" ]; then
    echo "no baserom at $ROM -- skipping the oracle checks"
    exit 0
fi

CHECKS="figatree_check
verify_dl
sprite_check
texfmt_check
texshuf_check
rendermode_check
bgm_check
fgm_check
vadpcm_check
transition_check
emblem_check
spotlight_check
slash_check
spark_check
mdust_check
dexp_check
backup_check
shade_check
particle_check
particletex_check
joint_check
sintable_check
lbparticle_check
efmanager_check
trig_check
fpu_check
pack_anim_check
effect_model_check
model_pack_check
camanim_check
anm_body_check"

JOBS="${SSB64_TEST_JOBS:-$(nproc 2>/dev/null || echo 4)}"
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT
export ORACLE_OUT="$OUT"

# -n 1 hands each name to bash as $0. xargs exits 123 when any child does;
# the per-check codes collected below are what this script actually reports.
echo "$CHECKS" | xargs -P "$JOBS" -n 1 bash -c \
    'python3 "tools/check/$0.py" >"$ORACLE_OUT/$0.log" 2>&1; echo $? >"$ORACLE_OUT/$0.rc"' \
    || true

fail=0
for c in $CHECKS; do
    cat "$OUT/$c.log" 2>/dev/null
    rc="$(cat "$OUT/$c.rc" 2>/dev/null || echo 127)"
    if [ "$rc" != 0 ]; then
        echo "test_oracle: $c.py failed (exit $rc)" >&2
        fail=1
    fi
done
exit "$fail"

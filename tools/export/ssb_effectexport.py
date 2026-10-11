#!/usr/bin/env python3
"""ssb64-dc: the rebirth halo of ef/efmanager.c, packed.

`efManagerRebirthHaloMakeEffect` (efmanager.c:5994) is the first effect
in the port that has a *model*. Every one before it -- the
eight -- made a particle and nothing else; this one makes a GObj out of
`dEFManagerRebirthHaloEffectDesc`, which names a DObjDesc tree and an
AnimJoint at fixed offsets into relocData file 85 (EFCommonEffects3).

The tree is three nodes:

    0  the stand, no geometry.        Matrix kind 0x50: not a transform
                                     at all but a function --
                                     lbcommon.c's func_ovl0_800C99CC,
                                     which puts the DObj at the world
                                     position of whatever DObj its
                                     user_data points at. The halo's
                                     points at the fighter's TopN, which
                                     is how it hangs over him.
    1  the halo itself, at y = -60.  Two display lists, one queued into
                                     DL head 0 and one into head 1 --
                                     the opaque ring and the translucent
                                     glow. `gcDrawDObjTreeDLLinksForGObj`
                                     is the EFDesc's render proc, so the
                                     payload is a DObjDLLink array and
                                     not a display list.
    2  a second opaque list.         Driven by the block's one AnimJoint
                                     script.

The pack is baked the way the off-screen arrows' is
(tools/export/ssb_arrowexport.py) -- the DObjDesc walked into joints, the
display lists baked into world-space batches, the AnimJoint carried
whole and rebased so src/dc/fighter.c's reloc walk makes its pointers
addresses again. What differs is the payload shape (DObjDLLink, as
tools/export/ssb_spotexport.py reads it) and that the bucket each batch lands
in is the DL head the game queued it into rather than one seeded here.

The three offsets are not written down twice: they are read out of
src/dc/decomp/reloc_data.us.h, which is the decomp's own generator's
output (tools/export/gen_reloc_header.sh) and the same header the port compiles
against.

The second model this exports is not a model at all. The screen quake
(`efManagerQuakeMakeEffect`, efmanager.c:3812) makes one DObj with a
Translate XObj, no display list, and drives it with one of four AnimJoint
scripts in relocData file 83; its update reads that DObj's translate
every frame and hands it to `gmCameraSetVelAt`. So `efquake.mdl` is a
pack with one joint, no vertices, no triangles, no batches and no
textures -- four animations and nothing else. The pack format already
allows that; nothing had asked for it before.

Usage: python3 tools/export/ssb_effectexport.py
           --what halo|quake|orbs|slash|spark|mdust|deadexplode|shield|
                  yoshiegg|shock|firespark|reflectbreak|thundershock
                  (and the EFENTRY rows)
           --out <file.mdl> [--rom <rom.z64>]

The two shield models are not in an effects file
at all: the bubble every fighter but Yoshi raises is a DObjDesc in the
common fighter file (gFTManagerCommonFile, llFTManagerCommonShieldDObjDesc)
and Yoshi's egg is a display list in his own model file
(gFTDataYoshiModel, llYoshiModelShieldDObjDesc -- a Gfx* despite the
name, as an EFDesc without flag 0x4 takes one). Both are one textured
quad the game draws as a billboard (matrix kind 0x2C).
"""
import math
import os
import re
import struct
import sys

sys.path[:0] = [os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), d)
                for d in ("lib", "export", "check")]
import ssb_extract           # noqa: E402
import ssb_assets as A       # noqa: E402
import ssb_meshexport as M   # noqa: E402
import ssb_packexport as K   # noqa: E402

MAGIC = b"SSBPACKA"
ANIM_NAME_LEN = K.ANIM_NAME_LEN

# The name the pack carries; FPackHeader.name holds eight characters.
NAME = "EFHalo"

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
RELOC_HEADER = os.path.join(ROOT, "src", "dc", "decomp", "reloc_data.us.h")

# What the tree must be for the offsets to be the right ones: three
# DObjDesc entries whose ids are 0, 1, 2 -- a stand and two limbs off it
# -- and then the array's terminator.
HALO_JOINT_IDS = (0, 1, 2)
# The DL heads the game queues the three lists into. Head 0 is the
# opaque pass and head 1 the translucent one (ef/efdisplay.c's two
# render-mode GObj pairs set them), so this is also what says the halo
# is one solid ring and one glow over it.
HALO_DL_LINKS = ((), (0, 1), (0,))


def reloc_offsets(want):
    """Named symbols' values, out of the generated reloc header."""
    try:
        src = open(RELOC_HEADER).read()
    except OSError as e:
        sys.exit("effects: %s" % e)
    found = {m.group(1): int(m.group(2), 16) for m in re.finditer(
        r"extern int (ll\w+); // (0x[0-9a-fA-F]+)", src)}
    out = []
    for name in want:
        if name not in found:
            sys.exit("effects: %s declares no %s" % (RELOC_HEADER, name))
        out.append(found[name])
    return out


def animjoint_block(f, reloc, anim_off, njoints, who, patch=None,
                    direct=False):
    """One AnimJoint block as (entries, words, relocs).

    The table at `anim_off` is one AObjEvent32* per joint; the scripts sit
    after it. The block runs from the table to the last word any of those
    scripts reaches, and each pointer in that range is rebased onto it, so
    src/dc/fighter.c's reloc walk makes an address of it at load the way
    lb/lbreloc.c does for the game.

    Where the block ends matters: an effect's AnimJoint is one of many in
    its file, and taking everything after it would carry the effects that
    follow into the pack -- 20 KB of other people's data for the orbs'
    two commands. The walk already knows every word each script reads, so
    the end is the highest of them. The one thing the walk does not
    follow is SetTranslateInterp's spline table, which sits outside the
    script; if any pointer inside the block leaves it, the block keeps
    the rest of the file rather than guessing how much of it to take."""
    table = [reloc.get(anim_off + 4 * k) for k in range(njoints)]
    if direct:
        # the symbol is the one joint's script itself (gcAddDObjAnimJoint),
        # not a table of them: the block starts at the script
        table = [anim_off]
    # `patch` is {joint: offset} for the entries a maker overwrites after
    # the array is applied. What the game plays is the array with these
    # written over it, so that is what the block carries.
    for k, off in (patch or {}).items():
        if not 0 <= k < njoints:
            sys.exit("%s: the animation patch names joint %d of %d"
                     % (who, k, njoints))
        table[k] = off
    body_off = anim_off + (0 if direct else 4 * njoints)
    if len(f) % 4:
        sys.exit("%s: file is %d bytes, not a whole number of words"
                 % (who, len(f)))
    entries = [-1] * njoints
    floats = set()
    reached = set()
    for k, t in enumerate(table):
        if t is None:
            continue
        if not anim_off <= t < len(f):
            sys.exit("%s: joint %d's script at 0x%04X is outside the "
                     "animation at 0x%04X" % (who, k, t, anim_off))
        # Walk it once before it ships: the game's parser has no default
        # case, so an opcode it does not know is an infinite loop on the
        # target rather than an error.
        seen, floats = A.animjoint_walk(f, reloc, t, floats)
        reached |= seen
        entries[k] = (t - anim_off) // 4
    if not any(e >= 0 for e in entries):
        sys.exit("%s: the AnimJoint table at 0x%04X drives no joint"
                 % (who, anim_off))
    end = max(reached | floats) + 4
    if any(anim_off <= loc < end and target >= end
           for loc, target in reloc.items()):
        end = len(f)
    words = list(struct.unpack(">%dI" % ((end - anim_off) // 4),
                               f[anim_off:end]))
    relocs = []
    for loc, target in sorted(reloc.items()):
        if loc < body_off or loc >= end:
            continue
        if target < anim_off:
            sys.exit("%s: pointer at 0x%04X leaves the animation"
                     % (who, loc))
        words[(loc - anim_off) // 4] = (target - anim_off) // 4
        relocs.append((loc - anim_off) // 4)
    return entries, words, relocs


def build_pack(name, njoints, secs, counts, blocks, center, radius,
               mobjs=None):
    """One .mdl out of baked sections and a list of animation blocks.

    The four packs this file writes have the same shape and differ only
    in what is in it, so the layout lives here once: `counts` is (verts,
    tris, batches, textures, palettes) and `blocks` is [(animation name,
    entries, words, relocs)] -- one block for the halo, four for the
    quake's magnitudes, one for the orbs, one for the slash.

    `mobjs`, when given, is the FPackMObjs section of a pack whose
    materials are objects rather than baked-in colours: a dict of the
    five blobs plus the MatAnimJoint words and their reloc list, and an
    optional "alt" saying how many whole MatAnimJoints the entry blob
    holds end to end (one, unless the game swaps between them: the dead
    explosion has four, one per player). The slash, the sparks, the metal
    dust and the dead explosion have one here; tools/export/ssb_emblemexport.py
    and tools/export/ssb_spotexport.py lay the same section out for their own
    packs.

    An optional "dpals" (raw FPackDPal frames, tools/lib/ssb_assets.py
    read_palette's own shape) and "dpal_count" ride beside them for a
    MObj whose MatAnimJoint steps palette_id through more than one value
    (Run's crash and Clash's wallpaper quadrants) -- empty
    for every other caller of this function, which is still all of them.
    """
    nverts, ntris, nbatches, ntexs, npals = counts

    def align(b):
        return b + b"\0" * (-len(b) % 4)

    header_size = 128
    mobjhdr_size = 48
    off = header_size
    offsets = {}
    sections = [("joints", secs["joints"]), ("verts", secs["verts"]),
                ("tris", secs["tris"]), ("batches", secs["batches"]),
                ("texs", secs["texs"]), ("pals", secs["pals"]),
                ("texdata", secs["texdata"]),
                ("anims", b"\0" * (len(blocks) * (ANIM_NAME_LEN + 24)))]
    if mobjs is not None:
        sections += [("mobjhdr", b"\0" * mobjhdr_size),
                     ("subs", mobjs["subs"]), ("mjoint", mobjs["joint"]),
                     ("mbatch", mobjs["batch"]), ("mentry", mobjs["entry"]),
                     ("mwords", mobjs["words"]), ("mreloc", mobjs["reloc"]),
                     ("mdpal", mobjs.get("dpals", b""))]
    for k, (_, entries, words, relocs) in enumerate(blocks):
        sections.append(("entries%d" % k,
                         struct.pack("<%di" % njoints, *entries)
                         + struct.pack("<%dI" % len(relocs), *relocs)))
        sections.append(("words%d" % k,
                         struct.pack("<%dI" % len(words), *words)))
    for key, sec in sections:
        offsets[key] = off
        off += len(align(sec))

    mobjhdr = b""
    if mobjs is not None:
        mobjhdr = struct.pack("<12I", mobjs["count"], offsets["subs"],
                              offsets["mjoint"], offsets["mbatch"],
                              offsets["mentry"], offsets["mwords"],
                              len(mobjs["words"]) // 4, offsets["mreloc"],
                              len(mobjs["reloc"]) // 4, mobjs.get("alt", 1),
                              offsets["mdpal"], mobjs.get("dpal_count", 0))
        assert len(mobjhdr) == mobjhdr_size, len(mobjhdr)

    directory = b""
    for k, (anim_name, entries, words, relocs) in enumerate(blocks):
        directory += struct.pack("<%dsIIIIII" % ANIM_NAME_LEN,
                                 anim_name.encode()[:ANIM_NAME_LEN - 1],
                                 offsets["words%d" % k], len(words),
                                 offsets["entries%d" % k],
                                 1,      # FPACK_ANIM_ANIMJOINT
                                 offsets["entries%d" % k] + njoints * 4,
                                 len(relocs))
    body = b""
    for key, sec in sections:
        body += align({"anims": directory, "mobjhdr": mobjhdr}.get(key, sec))

    header = struct.pack("<8s8I8I3ff8s8I", MAGIC,
                         njoints, nverts, ntris, nbatches, ntexs, npals,
                         len(blocks), len(secs["texdata"]),
                         offsets["joints"], offsets["verts"],
                         offsets["tris"], offsets["batches"],
                         offsets["texs"], offsets["pals"],
                         offsets["texdata"], offsets["anims"],
                         center[0], center[1], center[2], radius,
                         name.encode()[:8],
                         0, 0, 0, 0, 0, 0, 0,
                         0 if mobjs is None else offsets["mobjhdr"])
    assert len(header) == header_size, len(header)
    return header + body


def check_tree(nodes):
    ids = tuple(n.joint_id for n in nodes)
    if ids != HALO_JOINT_IDS:
        sys.exit("halo: DObjDesc reads joint ids %s, wanted %s"
                 % (ids, HALO_JOINT_IDS))
    links = tuple(tuple(b for b, _ in n.dl_links) for n in nodes)
    if links != HALO_DL_LINKS:
        sys.exit("halo: DObjDLLink buckets read %s, wanted %s"
                 % (links, HALO_DL_LINKS))


def pack_halo(rom):
    import pygfxd

    fid, dobj_off, anim_off = reloc_offsets(
        ("llEFCommonEffects3FileID",
         "llEFCommonEffects3RebirthHaloDObjDesc",
         "llEFCommonEffects3RebirthHaloAnimJoint"))

    f = A.get_file(rom, fid, ssb_extract)
    entry = ssb_extract.read_entry(rom, ssb_extract.RELOC_SEG, fid)
    reloc = A.walk_reloc(f, entry["reloc_intern"])

    root, nodes = A.read_dobj_tree(f, reloc, dobj_off, dl_links=True)
    check_tree(nodes)

    baker = A.MeshBaker(f, pygfxd, [[] for _ in nodes])
    verts, tris, joints = baker.bake(root)

    joint_of = {}
    for j in joints:
        for b in range(j["batch_first"], j["batch_first"] + j["batch_count"]):
            joint_of[b] = j["node"].index
    secs = K.model_sections(
        [(n.parent.index, n.translate, n.rotate, n.scale)
         for n in baker.nodes],
        verts, tris,
        [(b["tri_first"], b["tri_count"], b["mat"], joint_of[i],
          b.get("bucket", 0)) for i, b in enumerate(baker.batches)],
        baker.textures, baker.palettes)

    # -- the one animation ---------------------------------------------
    njoints = len(baker.nodes)
    entries, words, relocs = animjoint_block(f, reloc, anim_off, njoints,
                                             "halo")

    world = baker.world_verts()
    lo, hi = baker.bounds()
    cx, cy, cz = ((lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2,
                  (lo[2] + hi[2]) / 2)
    radius = max(math.sqrt((p[0] - cx) ** 2 + (p[1] - cy) ** 2 +
                           (p[2] - cz) ** 2) for p in world)

    blob = build_pack(NAME, njoints, secs,
                      (len(verts), len(tris), len(baker.batches),
                       len(baker.textures), len(baker.palettes)),
                      [(NAME, entries, words, relocs)], (cx, cy, cz), radius)
    print("halo: file %d, DObjDesc 0x%04X, AnimJoint 0x%04X: %d joints, "
          "%d verts, %d tris, %d batches, %d textures, %d driven joints, "
          "%d anim words, %d pointers"
          % (fid, dobj_off, anim_off, njoints, len(verts), len(tris),
             len(baker.batches), len(baker.textures),
             sum(1 for e in entries if e >= 0), len(words), len(relocs)))
    return blob


# efmanager.c:3838-3856, the four AnimJoint blocks efManagerQuakeMakeEffect
# picks between. Magnitude 0 to 3; ft/ftcommon/ftcommondead.c asks for 2.
QUAKE_ANIMS = ["llEFCommonEffects1QuakeMag%dAnimJoint" % k for k in range(4)]
QUAKE_NAME = "EFQuake"
# One joint: efManagerQuakeMakeEffect makes exactly one DObj
# (gcAddDObjForGObj with a NULL display list) and gives it a Translate
# XObj. There is nothing to draw.
QUAKE_JOINTS = 1


def pack_quake(rom):
    vals = reloc_offsets(["llEFCommonEffects1FileID"] + QUAKE_ANIMS)
    fid, offs = vals[0], vals[1:]

    f = A.get_file(rom, fid, ssb_extract)
    entry = ssb_extract.read_entry(rom, ssb_extract.RELOC_SEG, fid)
    reloc = A.walk_reloc(f, entry["reloc_intern"])

    blocks = [animjoint_block(f, reloc, o, QUAKE_JOINTS,
                              "quake mag %d" % k)
              for k, o in enumerate(offs)]

    # The one joint, at the origin with no parent: the DObj the game makes
    # by hand. Its translate is what the script drives and what
    # efManagerQuakeProcUpdate reads back out.
    secs = K.model_sections([(-1, (0.0, 0.0, 0.0), (0.0, 0.0, 0.0),
                              (1.0, 1.0, 1.0))],
                            [], [], [], [], [])

    blob = build_pack(QUAKE_NAME, QUAKE_JOINTS, secs, (0, 0, 0, 0, 0),
                      [("Mag%d" % k,) + b for k, b in enumerate(blocks)],
                      (0.0, 0.0, 0.0), 0.0)
    print("quake: file %d, %d AnimJoint blocks at %s: %s anim words, "
          "%s pointers, no geometry"
          % (fid, len(blocks), ", ".join("0x%04X" % o for o in offs),
             "/".join(str(len(w)) for _, w, _ in blocks),
             "/".join(str(len(r)) for _, _, r in blocks)))
    return blob


# efmanager.c:141-168 dEFManagerDamageFlyOrbsEffectDesc. Its flags are
# EFFECT_FLAG_USERDATA | 0x1 and *not* 0x4, so efManagerMakeEffect reads
# `o_dobjsetup` as a display list and not as a DObjDesc array
# (efmanager.c:1997): the tree it builds is two DObjs by hand -- a stand
# with no geometry carrying transform_types1, and one child carrying the
# display list and transform_types2. The reloc symbol is named
# ...FlyOrbsDObjDesc all the same, and the bytes at it are F3DEX2: a
# pipe sync, a combiner, a 7-colour TLUT, an 8x8 CI4 tile and one quad.
# So the pack has the two joints the game makes, and the geometry hangs
# off the second exactly as the game's display list does.
ORBS_NAME = "EFOrbs"
ORBS_JOINTS = 2
# lb/lbcommon.c:2024 lbCommonDrawDObjScaleX, which is the EFDesc's render
# proc (lbCommonDObjScaleXProcDisplay), queues the display list into
# gSYTaskmanDLHeads[1] -- the translucent head, whose render mode
# ef/efdisplay.c's XLU display GObj set. The DL sets no render mode of
# its own, so the head is what decides the bucket, and saying so here is
# the same fact the halo's DObjDLLink array carries in the file.
ORBS_HEAD = 1


def pack_orbs(rom):
    import pygfxd

    fid, dl_off, anim_off = reloc_offsets(
        ("llEFCommonEffects1FileID",
         "llEFCommonEffects1FlyOrbsDObjDesc",
         "llEFCommonEffects1FlyOrbsAnimJoint"))

    f = A.get_file(rom, fid, ssb_extract)
    entry = ssb_extract.read_entry(rom, ssb_extract.RELOC_SEG, fid)
    reloc = A.walk_reloc(f, entry["reloc_intern"])

    zero, one = (0.0, 0.0, 0.0), (1.0, 1.0, 1.0)
    root = A.DObjNode(-1, -1, None, zero, zero, one)
    stand = A.DObjNode(0, 0, None, zero, zero, one)
    orb = A.DObjNode(1, 1, None, zero, zero, one)
    orb.dl_links = [(ORBS_HEAD, dl_off)]
    stand.parent, root.children = root, [stand]
    orb.parent, stand.children = stand, [orb]

    baker = A.MeshBaker(f, pygfxd, [[] for _ in range(ORBS_JOINTS)])
    verts, tris, joints = baker.bake(root)

    if len(baker.batches) != 1:
        sys.exit("orbs: the display list bakes to %d batches, wanted one"
                 % len(baker.batches))
    if (baker.batches[0]["bucket"] & A.FPACK_LIST_MASK) != A.FPACK_LIST_TR:
        sys.exit("orbs: the quad landed in PVR list %d, wanted the "
                 "translucent one" % (baker.batches[0]["bucket"] &
                                      A.FPACK_LIST_MASK))
    if len(tris) != 2:
        sys.exit("orbs: %d triangles, wanted the two of one quad" % len(tris))

    joint_of = {}
    for j in joints:
        for b in range(j["batch_first"], j["batch_first"] + j["batch_count"]):
            joint_of[b] = j["node"].index
    secs = K.model_sections(
        [(n.parent.index, n.translate, n.rotate, n.scale)
         for n in baker.nodes],
        verts, tris,
        [(b["tri_first"], b["tri_count"], b["mat"], joint_of[i],
          b.get("bucket", 0)) for i, b in enumerate(baker.batches)],
        baker.textures, baker.palettes)

    # The AnimJoint table has one entry and it drives the *child*: the
    # game hands lbCommonAddTreeDObjsAnimAll the DObj it has just walked
    # down to (efmanager.c:1999), not the stand. So the pack's table is
    # the stand's -1 followed by the game's one script.
    entries, words, relocs = animjoint_block(f, reloc, anim_off, 1, "orbs")
    entries = [-1] + entries

    world = baker.world_verts()
    lo, hi = baker.bounds()
    cx, cy, cz = ((lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2,
                  (lo[2] + hi[2]) / 2)
    radius = max(math.sqrt((p[0] - cx) ** 2 + (p[1] - cy) ** 2 +
                           (p[2] - cz) ** 2) for p in world)

    blob = build_pack(ORBS_NAME, ORBS_JOINTS, secs,
                      (len(verts), len(tris), len(baker.batches),
                       len(baker.textures), len(baker.palettes)),
                      [(ORBS_NAME, entries, words, relocs)],
                      (cx, cy, cz), radius)
    print("orbs: file %d, display list 0x%04X into head %d, AnimJoint "
          "0x%04X: %d joints, %d verts, %d tris, %d batches, %d textures, "
          "%d anim words, %d pointers"
          % (fid, dl_off, ORBS_HEAD, anim_off, ORBS_JOINTS, len(verts),
             len(tris), len(baker.batches), len(baker.textures),
             len(words), len(relocs)))
    return blob


# efmanager.c:2486 efManagerDamageSlashMakeEffect, the white streak a hit
# leaves. dEFManagerDamageSlashEffectDesc (efmanager.c:81) is the first
# descriptor the port reads that names all four of an EFDesc's offsets:
# a DObjDesc tree, an MObjSub table, an AnimJoint and a MatAnimJoint.
#
# Three joints: a stand, the streak (rotated a quarter turn about Z) and
# the flash over it, each with one DObjDLLink into list 1. Each of the
# two drawn joints has one MObj, and each of those MObjs owns a *sprite
# array* -- eight 16x48 CI4 frames for the streak, five 32x32 for the
# flash -- which its MatAnimJoint steps through with
# nGCAnimTrackTextureIDCurrent while it fades the primitive colour out.
# That is the first animated texture in the port: every model before this
# one had exactly the picture its display list loaded.
SLASH_NAME = "EFSlash"
SLASH_JOINT_IDS = (0, 1, 2)
# gcDrawDObjTreeDLLinksForGObj is the descriptor's render proc, so each
# joint's payload is a DObjDLLink array rather than a display list; both
# drawn joints queue into head 1, the translucent one.
SLASH_DL_LINKS = ((), (1,), (1,))
# The MObjSub.flags a slash material may set. ALPHA is what makes
# gcDrawMObjForDObj take the picture out of the sprite array
# (ssb_assets.py _apply_mobj), and the other three are the colours
# src/dc/objdisplay.c already carries. Anything else -- a palette out of
# the MObj, a tile size of its own -- is refused here rather than drawn
# wrongly.
SLASH_MOBJ_OK = (A.MOBJ_FLAG_ALPHA | A.MOBJ_FLAG_PRIMCOLOR |
                 A.MOBJ_FLAG_LIGHT1 | A.MOBJ_FLAG_LIGHT2)


def matanim_block(f, reloc, scripts, who):
    """The MatAnimJoint scripts of one effect as (entries, words, relocs).

    Same shape as animjoint_block above and for the same reasons, with
    two differences. The block starts at the lowest script rather than at
    a table: the game's `AObjEvent32 ***` indirection is per DObj and per
    MObj, and src/dc/objmodel.c dc_model_add_mobjs rebuilds both arrays
    out of FPackMObjs.off_entry, so carrying them would be carrying two
    arrays nothing reads. And the walk is the material parser's
    (animjoint_walk mat=True), because these scripts are read by
    objanim.c gcParseMObjMatAnimJoint and not by its joint parser.
    """
    base = min(s for s in scripts if s is not None)
    reached, vals = set(), set()
    for s in scripts:
        if s is None:
            continue
        seen, vals = A.animjoint_walk(f, reloc, s, vals, mat=True)
        reached |= seen
    end = max(reached | vals) + 4
    if base % 4 or end % 4:
        sys.exit("%s: MatAnimJoint block 0x%04X..0x%04X is not word aligned"
                 % (who, base, end))
    words = list(struct.unpack(">%dI" % ((end - base) // 4), f[base:end]))
    relocs = []
    for loc, target in sorted(reloc.items()):
        if not base <= loc < end:
            continue
        if not base <= target < end:
            sys.exit("%s: pointer at 0x%04X leaves the MatAnimJoint block"
                     % (who, loc))
        words[(loc - base) // 4] = (target - base) // 4
        relocs.append((loc - base) // 4)
    for s in scripts:
        if s is not None and not base <= s < end:
            sys.exit("%s: script at 0x%04X is outside the block at 0x%04X"
                     % (who, s, base))
    return [-1 if s is None else (s - base) // 4 for s in scripts], \
        words, relocs


def matanim_texture_ids(f, reloc, script):
    """Every value one MatAnimJoint script gives texture_id_curr.

    ssb_assets.matanim_frame_ids is the walk itself, shared with
    ssb_stageexport, which asks the same question of palette_id for
    Brinstar's acid. This name stays because it is what this file's six
    callers read as, and because "texture ids" is what they want.
    """
    return A.matanim_frame_ids(f, reloc, script, A.MATANIM_BIT_TEXID)


# efmanager.c:201 dEFManagerImpactWaveEffectDesc, the green shockwave a
# fighter throws when it slams into a wall, ceiling or floor
# (ft/ftcommon/ftcommonwalldamage.c efManagerImpactWaveMakeEffect). Flags
# are EFFECT_FLAG_USERDATA (0x2) alone -- no 0x1, no 0x4 -- so
# efManagerMakeEffect takes its simplest branch: one DObj with the raw DL
# at o_dobjsetup bound straight to it (gcAddDObjForGObj(gobj, DL)), the way
# the magnify handle is, plus one MObj, one AnimJoint that scales and spins
# the ring open over eleven frames, and one MatAnimJoint that scrolls its
# UV. The DObjDesc offset is therefore a display list, not a DObjDesc array
# (read_dobj_tree does not apply); the geometry is an 18-vertex fan the
# baker lifts out of the DL. The MatAnimJoint never touches texture_id_curr,
# so unlike the slash there is one picture and no sprite array.
IMPACT_NAME = "EFImpact"
FIREBALL_NAME = "WPFireba"
BLASTER_NAME = "WPBlast"
ARWINGLASER_NAME = "WPArLsr"
# The entry vehicles a fighter arrives on, one row each: the pack name,
# the file and the two symbols its EFDesc names, and how many joints the
# tree is expected to have. All of these have the rebirth halo's shape --
# a DObjDesc tree and one AnimJoint, no MObjSub and no MatAnimJoint --
# and makers that do nothing but make the effect and set its position.
# The ones that are not here are not here because of their MAKERS: Kirby's
# swaps the descriptor's AnimJoint between an L and an R block, Link's and
# Yoshi's carry MObjs, and Fox's and Captain Falcon's walk the tree by
# hand to hang animations on an Arwing's wing and a car's wheels.
EFENTRY = {
    # Mario's pipe
    "dokan": ("Mario's (and Luigi's) warp pipe", "llMarioSpecial2FileID",
              "llMarioSpecial2EntryDokanDObjDesc",
              "llMarioSpecial2EntryDokanAnimJoint", "EFDokan", 3, False),
    # Donkey Kong's barrel
    "taru": ("Donkey Kong's barrel", "llDonkeySpecial2FileID",
             "llDonkeySpecial2EntryTaruDObjDesc",
             "llDonkeySpecial2EntryTaruAnimJoint", "EFTaru", 2, False),
    # Samus's is the first vehicle whose geometry hangs off DObjDLLink
    # lists, and the first model in this port with MORE THAN ONE link on
    # a node: two, ids 0 and 1. The game draws BOTH -- gcDrawDObjDLLinks
    # walks the array to its terminator and queues each list into
    # gSYTaskmanDLHeads[list_id] (objdisplay.c:1561) -- so a link id
    # picks a DL HEAD, not one list out of several, and baking every one
    # of them is what the game does.
    "point": ("Samus's arrival point", "llSamusSpecial2FileID",
              "llSamusSpecial2EntryPointDObjDesc",
              "llSamusSpecial2EntryPointAnimJoint", "EFPoint", 2, True),
    # Two AnimJoints rather than one: the star sweeps in from
    # the right or the left, and efManagerKirbyEntryStarMakeEffect picks
    # by writing the block offset into the shared descriptor before making
    # the effect -- the same thing efManagerDeadExplodeMakeEffect does to
    # o_matanim_joint. Both come across as animations of the one pack, R
    # first, and src/dc/efmanager.c picks by index.
    "star": ("Kirby's entry star", "llKirbySpecial2FileID",
             "llKirbySpecial2EntryStarDObjDesc",
             ("llKirbySpecial2EntryStarRAnimJoint",
              "llKirbySpecial2EntryStarLAnimJoint"), "EFStar", 2, True),
    # Kirby's star, over a swallowed fighter and the one he
    # loses his copy power as (efmanager.c:1588 and :1618). Its o_dobjsetup
    # is ITCommonObject's display list at 0x5458 -- the Star Rod's and
    # Yoshi's star -- and the descs' flags are 0x1 with no 0x4, so the game
    # builds a bare DObj and hangs the list on a child of it
    # (EFENTRY_WRAPPED); both makers and both updates work on dobj->child.
    "kirbystar": ("Kirby's star", "llITCommonObjectFileID",
                  "llITCommonDataKirbyStarDObjDesc", (), "EFKStar", 2,
                  False),
}

# The vehicles that also carry an MObj and a MatAnimJoint: same row plus
# the two extra symbols. Their MObjs hold NO sprite array -- the material
# animation moves a colour or a texture offset, not a picture -- which is
# what makes these simpler than the ground Thunder Jolt's six.
EFENTRY_MOBJ = {
    # Link's wave
    "wave": ("Link's entry wave", "llLinkSpecial2FileID",
             "llLinkSpecial2EntryWaveDObjDesc",
             "llLinkSpecial2EntryWaveAnimJoint", "EFWave", 2, True,
             "llLinkSpecial2EntryWaveMObjSub",
             "llLinkSpecial2EntryWaveMatAnimJoint"),
    "beam": ("Link's entry beam", "llLinkSpecial2FileID",
             "llLinkSpecial2EntryBeamDObjDesc",
             "llLinkSpecial2EntryBeamAnimJoint", "EFBeam", 2, True,
             "llLinkSpecial2EntryBeamMObjSub",
             "llLinkSpecial2EntryBeamMatAnimJoint"),
    # The only vehicle whose EFDesc flags LACK 0x4, which is
    # the bit that says o_dobjsetup is a DObjDesc tree
    # (efmanager.c:1992-2004). Without it the field is a plain display
    # list bound to one DObj -- so the generated header's name for it,
    # llYoshiSpecial2EntryEggDObjDesc, says the opposite of what it is.
    # That is the same trap as llLinkSpecial3BoomerangDLDisplayList at
    # in reverse.
    # Link's Spin Attack's glow, out of the same Special2
    # his entry wave and beam came from. Its EFDesc lacks
    # 0x1, so no stand and the maker's tree walk is the port's own
    # (rule).
    "spin": ("Link's Spin Attack glow", "llLinkSpecial2FileID",
             "llLinkSpecial2SpinAttackDObjDesc",
             "llLinkSpecial2SpinAttackAnimJoint", "EFSpin", 2, True,
             "llLinkSpecial2SpinAttackMObjSub",
             "llLinkSpecial2SpinAttackMatAnimJoint"),
    "egg": ("Yoshi's entry egg", "llYoshiSpecial2FileID",
            "llYoshiSpecial2EntryEggDObjDesc",
            "llYoshiSpecial2EntryEggAnimJoint", "EFEgg", 1, False,
            "llYoshiSpecial2EntryEggMObjSub",
            "llYoshiSpecial2EntryEggMatAnimJoint"),
    # the Poké Ball's opening rays, and the FIRST row here
    # whose file is not a fighter's -- EFCommonEffects3 is the item
    # manager's own bank (relocData 0x55), the one the halo, the quake,
    # the orbs and the slash also come from. efManagerMBallRaysMakeEffect
    # makes it when a Poké Ball opens (it/itcommon/itmball.c's
    # itMBallOpenInitVars) and holds the GObj for the ball's whole open
    # animation, so unlike every other effect here it is not a one-shot.
    "mballrays": ("The Poké Ball's opening rays", "llEFCommonEffects3FileID",
                  "llEFCommonEffects3MBallRaysDObjDesc",
                  "llEFCommonEffects3MBallRaysAnimJoint", "EFMBall", 4,
                  True,
                  "llEFCommonEffects3MBallRaysMObjSub",
                  "llEFCommonEffects3MBallRaysMatAnimJoint"),
    # The item-use step: the swirl that spins up around a fighter who has
    # just picked an item up (it/itmain.c:462
    # efManagerItemGetSwirlProcUpdate, called from itMainSetFighterHold).
    # The rays' row again out of the same EFCommonEffects3 bank, seven
    # joints instead of four -- relocData/85_EFCommonEffects3.c:1110
    # "DObjDesc: ItemGetSwirl @ 0x3170 (7 entries)" -- six joints and the
    # array terminator, which is how the rays count four out of five.
    "itemgetswirl": ("The item pickup swirl", "llEFCommonEffects3FileID",
                     "llEFCommonEffects3ItemGetSwirlDObjDesc",
                     "llEFCommonEffects3ItemGetSwirlAnimJoint", "EFSwirl", 6,
                     True,
                     "llEFCommonEffects3ItemGetSwirlMObjSub",
                     "llEFCommonEffects3ItemGetSwirlMatAnimJoint"),
    # PSI Magnet's bubble (efmanager.c:1011
    # dEFManagerNessPsychicMagnetEffectDesc), relocData 352 NessSpecial2.
    # The wave's shape one joint smaller -- flags 0x4 | USERDATA, no 0x1,
    # so the tree is built straight on the GObj -- a stand and one link at
    # head 1, whose one MObj flips between two sprites on the
    # MatAnimJoint while the AnimJoint spins it and pulses its scale.
    "psimagnet": ("Ness's PSI Magnet bubble", "llNessSpecial2FileID",
                  "llNessSpecial2PsychicMagnetDObjDesc",
                  "llNessSpecial2PsychicMagnetAnimJoint", "EFPsiMag", 2,
                  True,
                  "llNessSpecial2PsychicMagnetMObjSub",
                  "llNessSpecial2PsychicMagnetMatAnimJoint"),
    # the wave PK Thunder's hold makes at Ness's hand
    # (efmanager.c:1089 dEFManagerNessPKThunderWaveEffectDesc), out of
    # NessModel (relocData 335) -- the file his own body is in. Three
    # joints, the last with one link and one MObj of two sprites.
    "pkwave": ("Ness's PK Thunder wave", "llNessModelFileID",
               "llNessModelPKThunderWaveDObjDesc",
               "llNessModelPKThunderWaveAnimJoint", "EFPKWave", 3, True,
               "llNessModelPKThunderWaveMObjSub",
               "llNessModelPKThunderWaveMatAnimJoint"),
    # a reflector shattering (efmanager.c:550
    # dEFManagerReflectBreakEffectDesc, made by ftcommonshieldbreakfly.c's
    # ftCommonShieldBreakFlyReflectorSetStatus), out of EFCommonEffects2.
    # The generated header's two names are the wrong way round --
    # llEFCommonEffects2ReflectBreakMObjSub (0x3398) is the six-entry
    # DObjDesc and ...DObjDesc (0x2F78) the MObjSub wrapper, which
    # relocData/84_EFCommonEffects2.c:977 and :1152 say out loud -- and the
    # EFDesc names them in the slots that match what they really are, so
    # this row does too.
    "reflectbreak": ("A reflector shattering", "llEFCommonEffects2FileID",
                     "llEFCommonEffects2ReflectBreakMObjSub",
                     "llEFCommonEffects2ReflectBreakAnimJoint", "EFRefBrk",
                     5, True,
                     "llEFCommonEffects2ReflectBreakDObjDesc",
                     "llEFCommonEffects2ReflectBreakMatAnimJoint"),
    # the sparks Pikachu's forward smash throws off
    # (efmanager.c:610 dEFManagerPikachuThunderShockEffectDesc), relocData
    # 347 PikachuSpecial2. Four joints -- a stand, the offset joint the
    # maker places, and two DL links under it, the second with the one
    # MObj -- and three animations, each an AnimJoint with its own
    # MatAnimJoint: efManagerPikachuThunderShockMakeEffect plays the
    # desc's (0) or gcAddAnimAll's 1 or 2 by the status's gfx_id.
    "thundershock": ("Pikachu's forward-smash sparks",
                     "llPikachuSpecial2FileID",
                     "llPikachuSpecial2ThunderShockDObjDesc",
                     ("llPikachuSpecial2ThunderShock0AnimJoint",
                      "llPikachuSpecial2ThunderShock1AnimJoint",
                      "llPikachuSpecial2ThunderShock2AnimJoint"),
                     "EFTShock", 4, True,
                     "llPikachuSpecial2ThunderShockMObjSub",
                     ("llPikachuSpecial2ThunderShock0MatAnimJoint",
                      "llPikachuSpecial2ThunderShock1MatAnimJoint",
                      "llPikachuSpecial2ThunderShock2MatAnimJoint")),
    # the swirl where a grab pulls its catch in (efmanager.c:
    # 515 dEFManagerCatchSwirlEffectDesc, made by ftcommoncatch2.c's
    # ftCommonCatchPullProcCatch), out of EFCommonEffects2. Its two names
    # are crossed the same way -- ...CatchSwirlMObjSub (0x2760) is the
    # DObjDesc and ...DObjDesc (0x22B8) the four-MObj wrapper, as
    # relocData/84_EFCommonEffects2.c:501 and :714 say.
    "catchswirl": ("The grab swirl", "llEFCommonEffects2FileID",
                   "llEFCommonEffects2CatchSwirlMObjSub",
                   "llEFCommonEffects2CatchSwirlAnimJoint", "EFCatch",
                   6, True,
                   "llEFCommonEffects2CatchSwirlDObjDesc",
                   "llEFCommonEffects2CatchSwirlMatAnimJoint"),
    # Samus's grapple beam glow (efmanager.c:730
    # dEFManagerSamusGrappleBeamEffectDesc), relocData SamusSpecial2, made
    # by her grab and her throws and hung off joint 23 by kind 0x4F.
    "grapplebeam": ("Samus's grapple beam glow", "llSamusSpecial2FileID",
                    "llSamusSpecial2GrappleBeamDObjDesc",
                    "llSamusSpecial2GrappleBeamAnimJoint", "EFGrpGlo",
                    None, True,
                    "llSamusSpecial2GrappleBeamMObjSub",
                    "llSamusSpecial2GrappleBeamMatAnimJoint"),
    # the Poke Ball thrown in at Pikachu's and Jigglypuff's
    # entry (efmanager.c:1282 dEFManagerMBallThrownEffectDesc), out of
    # ITCommonObject -- the maker finds its file through the MBall item's
    # ITAttributes in ITCommonData (llITCommonDataMBallThrownFileHead), and
    # the symbols are offsets into the file that pointer lands in. Two
    # sweeps, one per facing, each an AnimJoint and its MatAnimJoint: R
    # first, so efManagerMBallThrownMakeEffect picks 0 for lr +1, as
    # Kirby's entry star does.
    "mballthrown": ("The entry Poke Ball", "llITCommonObjectFileID",
                    "llITCommonDataMBallThrownDObjDesc",
                    ("llITCommonDataMBallThrownRAnimJoint",
                     "llITCommonDataMBallThrownLAnimJoint"),
                    "EFMBThrw", None, False,
                    "llITCommonDataMBallThrownMObjSub",
                    ("llITCommonDataMBallThrownRMatAnimJoint",
                     "llITCommonDataMBallThrownLMatAnimJoint")),
    # the spark the ground Thunder Jolt throws off
    # (efmanager.c:669 dEFManagerThunderJoltEffectDesc), relocData
    # PikachuSpecial3, made by wpPikachuThunderJoltGroundProcUpdate.
    "joltspark": ("The Thunder Jolt spark", "llPikachuSpecial3FileID",
                  "llPikachuSpecial3ThunderJoltDObjDesc",
                  "llPikachuSpecial3ThunderJoltAnimJoint", "EFJoltSp",
                  None, False,
                  "llPikachuSpecial3ThunderJoltMObjSub",
                  "llPikachuSpecial3ThunderJoltMatAnimJoint"),
    # the spark Pikachu's Thunder leaves in its wake
    # (efmanager.c:639 dEFManagerPikachuThunderTrailEffectDesc), relocData
    # PikachuModel. No tree -- a DObjDLLink array on one DObj, as Ness's
    # trail end is -- and one MObj over four sprites with no MatAnimJoint:
    # the maker and efManagerPikachuThunderTrailProcUpdate set
    # texture_id_curr themselves (EFENTRY_CODEFRAMES).
    "thundertrail": ("Pikachu's Thunder trail spark", "llPikachuModelFileID",
                     "llPikachuModelThunderTrailDObjDesc", (), "EFPTrail",
                     1, True, "llPikachuModelThunderTrailMObjSub", ()),
}
# Fox's Arwing. Its geometry is in Fox's Special3 and its
# two AnimJoint arrays are in his Special2 -- the first vehicle whose
# animation does not live in the same file as its tree, which is visible
# in the maker: the tree comes from gFTDataFoxSpecial3 and
# lbCommonAddDObjAnimJointAll reads gFTDataFoxSpecial2. Everything else
# about the row is the star's, R first.
EFENTRY["arwing"] = ("Fox's Arwing", "llFoxSpecial3FileID",
                     "llFoxSpecial3EntryArwingDObjDesc",
                     ("llFoxSpecial2EntryArwingRAnimJoint",
                      "llFoxSpecial2EntryArwingLAnimJoint"),
                     "EFArwing", 12, True)
# Captain Falcon's Blue Falcon, the ninth and last entry
# vehicle with an arm of its own. Its EFDesc's flags are 0x4 |
# EFFECT_FLAG_USERDATA and, alone among the vehicles, LACK 0x1 --
# efmanager.c:2017-2043 is the branch that reads, and it calls
# gcSetupCustomDObjs to build the tree straight on the GObj with no empty
# DObj above it. Which is exactly the shape this packer produces, so the
# Blue Falcon's maker ports verbatim where the Arwing's needed a level
# taken off.
EFENTRY["car"] = ("Captain Falcon's Blue Falcon", "llCaptainSpecial2FileID",
                  "llCaptainSpecial2EntryCarDObjDesc",
                  "llCaptainSpecial2_6200_AnimJoint", "EFCar", 12, True)

# the egg Yoshi's Egg Lay puts a caught fighter inside.
# Not a vehicle -- it is in this table because it is the same shape as
# one: a DObjDesc tree in a fighter's own file with AnimJoints over it,
# and no MObj. THREE animations, which is a first here: the throw (the
# descriptor's own o_anim_joint), the wait and the break, and the game
# swaps between the last two by writing a block pointer where the port
# writes an index. Baked throw first so index 0 is the one
# efManagerAddModel applies.
EFENTRY["egglay"] = ("Yoshi's Egg Lay egg", "llYoshiSpecial3FileID",
                     "llYoshiSpecial3EggLayDObjDesc",
                     ("llYoshiSpecial3EggLayThrowAnimJoint",
                      "llYoshiSpecial3EggLayWaitAnimJoint",
                      "llYoshiSpecial3EggLayBreakAnimJoint"),
                     "EFELay", 2, False)

# Kirby's Final Cutter -- the blade he draws, the two arcs
# of the swing and the trail behind it. Four EFDescs, all four out of his
# Special2 (relocData 348, the file his entry star came from), and all
# four with flags 0x4 | EFFECT_FLAG_USERDATA and NO 0x1, so the game
# builds their trees straight on the GObj and the port's makers need no
# level taken off (rule).
#
# The draw is the only entry in this table with NO AnimJoint at all: its
# descriptor's o_anim_joint is zero and nothing animates it -- it is the
# blade, drawn where Kirby's joint 17 is and rotated by his facing.
EFENTRY["cutterup"] = ("Kirby's Final Cutter up-arc", "llKirbySpecial2FileID",
                       "llKirbySpecial2CutterUpDObjDesc",
                       "llKirbySpecial2CutterUpAnimJoint",
                       "EFCutUp", 5, False)
EFENTRY["cutterdown"] = ("Kirby's Final Cutter down-arc",
                         "llKirbySpecial2FileID",
                         "llKirbySpecial2CutterDownDObjDesc",
                         "llKirbySpecial2CutterDownAnimJoint",
                         "EFCutDn", 6, False)
EFENTRY["cutterdraw"] = ("Kirby's Final Cutter blade", "llKirbySpecial2FileID",
                         "llKirbySpecial2CutterDrawDObjDesc",
                         (), "EFCutDr", 2, False)
EFENTRY["cuttertrail"] = ("Kirby's Final Cutter trail",
                          "llKirbySpecial2FileID",
                          "llKirbySpecial2CutterTrailDObjDesc",
                          "llKirbySpecial2CutterTrailAnimJoint",
                          "EFCutTr", 3, True)

# the sparks Kirby's rapid jab throws, out of the same
# Special2 his entry star and Final Cutter came from. A tree with DL
# links and NO AnimJoint at all -- the second pack here with none, after
# the Cutter's blade -- and its EFDesc carries 0x1, so the game hangs its
# tree under a stand and this port does not (rule). The
# maker only writes the root's translate and rotate, which is the case
# that difference does not reach.
EFENTRY["vulcanjab"] = ("Kirby's Vulcan Jab spark", "llKirbySpecial2FileID",
                        "llKirbySpecial2VulcanJabDObjDesc",
                        (), "EFVulcan", None, True)

# the last segment of PK Thunder's trail, and its reflected
# twin's (efmanager.c:1037 and 1063). Both EFDescs name the same
# llNessModelPKThunderTrailDObjDesc and both LACK 0x4, so the game binds
# it to one DObj with gcAddDObjForGObj -- and their ProcDisplay is
# gcDrawDObjDLLinksForGObj, so what that DObj's union holds is a
# DObjDLLink array, not a display list: lesson for
# Pikachu's weapon trail, here on an effect. One link at head 1, one
# quad whose list sets its own texture image -- no MObj and no
# animation, so it is not the weapon trail's pack (WPLINK
# "pkthundertrail", which steps three sprites through an MObj).
EFENTRY["pktrail"] = ("Ness's PK Thunder trail end", "llNessModelFileID",
                      "llNessModelPKThunderTrailDObjDesc",
                      (), "EFPKTrl", 1, True)

# the four effects whose makers were ported with no pack.
# Fox's Reflector (efmanager.c:420 dEFManagerFoxReflectorEffectDesc),
# relocData 346 FoxSpecial2: a stand and the hexagon, no MObj, and four
# AnimJoints in dEFManagerFoxReflectorAnimJointOffsets' order -- Start,
# Loop, Hit, End -- which efManagerFoxReflectorSetAnimID picks by index.
EFENTRY["reflector"] = ("Fox's Reflector", "llFoxSpecial2FileID",
                        "llFoxSpecial2ReflectorDObjDesc",
                        ("llFoxSpecial2ReflectorStartAnimJoint",
                         "llFoxSpecial2ReflectorLoopAnimJoint",
                         "llFoxSpecial2ReflectorHitAnimJoint",
                         "llFoxSpecial2ReflectorEndAnimJoint"),
                        "EFRefl", 2, False)
EFENTRY_MOBJ.update({
    # Falcon Kick's trail (efmanager.c:760), relocData 350 CaptainSpecial2:
    # a stand and one quad with an MObj, drawn by gcDrawDObjTreeForGObj.
    "falconkick": ("Falcon Kick's trail", "llCaptainSpecial2FileID",
                   "llCaptainSpecial2FalconKickDObjDesc",
                   "llCaptainSpecial2FalconKickAnimJoint", "EFFKick", 2,
                   False,
                   "llCaptainSpecial2FalconKickMObjSub",
                   "llCaptainSpecial2FalconKickMatAnimJoint"),
    # Sing's notes (efmanager.c:820), relocData 351 PurinSpecial2: six
    # joints over four DL links, drawn by gcDrawDObjTreeDLLinksForGObj.
    "sing": ("Jigglypuff's Sing notes", "llPurinSpecial2FileID",
             "llPurinSpecial2SingDObjDesc",
             "llPurinSpecial2SingAnimJoint", "EFSing", 6, True,
             "llPurinSpecial2SingMObjSub",
             "llPurinSpecial2SingMatAnimJoint"),
    # Falcon Punch's flame (efmanager.c:790), relocData 333
    # CaptainSpecial3. Flags USERDATA alone: no 0x4, so the "DObjDesc" is
    # the display list at 0x760 bound to the effect's one DObj (the entry
    # egg's shape), and no AnimJoint -- its MatAnimJoint steps three
    # sprites.
    "falconpunch": ("Falcon Punch's flame", "llCaptainSpecial3FileID",
                    "llCaptainSpecial3FalconPunchDObjDesc", (), "EFFPunch",
                    1, False,
                    "llCaptainSpecial3FalconPunchMObjSub",
                    "llCaptainSpecial3FalconPunchMatAnimJoint"),
})
EFENTRY.update({k: v[:7] for k, v in EFENTRY_MOBJ.items()})

# Which vehicles keep their AnimJoint arrays in a file of their own, and
# which file that is.
EFENTRY_ANIMFILE = {"arwing": "llFoxSpecial2FileID"}

# Nodes whose AnimJoint the MAKER sets after the array, and the symbol it
# sets them from. gcAddDObjAnimJoint replaces rather than appends
# (sys/objanim.c:137-149), so the array's entry for such a node never
# runs and what the game ends up playing is the array with these written
# over it -- which is what the pack carries as its one animation.
#
# The Blue Falcon's maker walks eight siblings from dobj->child->child->
# child in four passes of two, giving the odd ones the wheel script at
# 0x6518 (and matrix kind 44 with it, which the port's maker still does
# by hand because it is not animation) and the even ones the script at
# 0x6598. The array itself drives only nodes 1, 2 and 11.
#
# This is the opposite of the Arwing's extra AnimJoint, which the array
# overwrote and which is therefore dead: here the maker writes last, and
# the writes are what plays.
EFENTRY_ANIMPATCH = {
    "car": {
        3: "llCaptainSpecial2_6518_AnimJoint",
        4: "llCaptainSpecial2_6598_AnimJoint",
        5: "llCaptainSpecial2_6518_AnimJoint",
        6: "llCaptainSpecial2_6598_AnimJoint",
        7: "llCaptainSpecial2_6518_AnimJoint",
        8: "llCaptainSpecial2_6598_AnimJoint",
        9: "llCaptainSpecial2_6518_AnimJoint",
        10: "llCaptainSpecial2_6598_AnimJoint",
    },
}

# the two models of mvopeningstandoff.c (relocData 69), which
# a scene builds itself rather than through an EFDesc, but which have the
# shapes the rows above already read. The ground is one display list bound
# to one DObj (gcAddDObjForGObj), the lightning a DObjDLLink tree with
# MObjs and both animation kinds -- 13 joints and the array terminator.
EFENTRY_MOBJ["standofflight"] = (
    "Standoff's lightning", "llMVOpeningStandoffFileID",
    "llMVOpeningStandoffLightningDObjDesc",
    "llMVOpeningStandoffLightningAnimJoint", "SOLight", 13, True,
    "llMVOpeningStandoffLightningMObjSub",
    "llMVOpeningStandoffLightningMatAnimJoint")
# mvopeningclash.c's four wallpaper quadrants (relocData 66),
# each one display list on one DObj with an MObj, a MatAnimJoint and an
# AnimJoint (gcAddDObjForGObj + gcAddMObjAll + gcAddMatAnimJointAll +
# gcAddAnimJointAll).
for _q in ("LL", "LR", "UL", "UR"):
    EFENTRY_MOBJ["clashwall" + _q.lower()] = (
        "Clash's wallpaper " + _q, "llMVOpeningClashWallpaperFileID",
        "llMVOpeningClashWallpaper%sDisplayList" % _q,
        "llMVOpeningClashWallpaper%sAnimJoint" % _q, "CW" + _q, 1, False,
        "llMVOpeningClashWallpaper%sMObjSub" % _q,
        "llMVOpeningClashWallpaper%sMatAnimJoint" % _q)
# mvopeningnewcomers.c's four silhouettes (relocData 61 and
# 62), each a Show and a Hidden display list bound to one DObj with its
# own AnimJoint; the scene picks by whether the fighter is unlocked, so
# both are packs (`--what newcomerspurin` / `newcomerspurinhide` ...).
for _w, _f, _n in (("purin", 1, "Purin"), ("captain", 2, "Captain"),
                   ("luigi", 1, "Luigi"), ("ness", 2, "Ness")):
    for _h in ("Show", "Hidden"):
        EFENTRY["newcomers" + _w + ("hide" if _h == "Hidden" else "")] = (
            "Newcomers' %s (%s)" % (_n, _h.lower()),
            "llMVOpeningNewcomers%dFileID" % _f,
            "llMVOpeningNewcomers%d%s%sDisplayList" % (_f, _n, _h),
            "llMVOpeningNewcomers%d%sAnimJoint" % (_f, _n),
            "NC" + _n[:3] + ("H" if _h == "Hidden" else "S"), 1, False)
# mvopeningrun.c's crash (relocData 75): a five-joint
# DObjDesc tree with an MObj per drawn joint and a MatAnimJoint, no
# AnimJoint, drawn by gcDrawDObjTreeForGObj.
EFENTRY_MOBJ["runcrash"] = (
    "Run's crash", "llMVOpeningRunCrashFileID",
    "llMVOpeningRunCrashDObjDesc", (), "RunCrash", 5, False,
    "llMVOpeningRunCrashMObjSub", "llMVOpeningRunCrashMatAnimJoint")
EFENTRY["standoffground"] = ("Standoff's ground", "llMVOpeningStandoffFileID",
                             "llMVOpeningStandoffGroundDisplayList", (),
                             "SOGround", 1, False)
# mn/mncommon/mntitle.c's slash (relocData 167), the streak
# that cuts across the logo after the opening movie -- mnTitleMakeSlash's
# gcSetupCustomDObjsWithMObj + gcAddAnimJointAll + gcAddMatAnimJointAll, a
# five-joint DObjDLLink tree whose two drawn joints each carry an MObj
# stepping the same seven IA8 frames.
EFENTRY_MOBJ["titleslash"] = (
    "the title's slash", "llMNTitleFileID", "llMNTitleSlashDObjDesc",
    "llMNTitleSlashAnimJoint", "TSlash", 5, True,
    "llMNTitleSlashMObjSub", "llMNTitleSlashMatAnimJoint")

EFENTRY.update({k: v[:7] for k, v in EFENTRY_MOBJ.items()})

# The vehicles whose o_dobjsetup is a display list rather than a tree,
# by the absence of 0x4 in their EFDesc flags.
EFENTRY_FLATDL = ("egg", "kirbystar", "falconpunch", "standoffground",
                  "newcomerspurin", "newcomerspurinhide", "newcomerscaptain",
                  "newcomerscaptainhide", "newcomersluigi", "newcomersluigihide",
                  "newcomersness", "newcomersnesshide",
                  "clashwallll", "clashwalllr", "clashwallul", "clashwallur")
# ...and of those, the ones that keep the game's bare wrapper DObj (EFDesc
# flags 0x1): the list goes on a child of an empty root, so the root takes
# transform_types1 and the list's DObj transform_types2, as
# efManagerMakeEffect's 0x1 arm builds them (efmanager.c:1984-2003).
EFENTRY_WRAPPED = ("kirbystar",)
# ...and the ones whose o_dobjsetup is a DObjDLLink array bound to one
# DObj: no 0x4, and a DL-links ProcDisplay.
EFENTRY_LINKDL = ("pktrail", "thundertrail")
# ...and the ones whose MObj pictures are picked by code rather than by a
# MatAnimJoint, so every sprite is a frame: Pikachu's trail spark takes
# 0..2 at random each tic and 3 on its last.
EFENTRY_CODEFRAMES = ("thundertrail",)
# a sprite array with a NULL slot in it, which
# ssb_assets._ptr_list stops at. The lightning's five-frame arrays are
# {5140, NULL, 4138, 3130, 2128} and its MatAnimJoints step 0, 2, 3, 4 and
# never 1, so the hole is filled with the frame before it (and the array
# cut at its five slots, the ones the scripts can reach).
EFENTRY_HOLES = {"standofflight": 5}
# The silhouettes' animation symbol is a script, not a table of them
# (mvopeningnewcomers.c calls gcAddDObjAnimJoint on the one DObj).
EFENTRY_DIRECTANIM = tuple("newcomers%s%s" % (w, h) for w in
                           ("purin", "captain", "luigi", "ness")
                           for h in ("", "hide"))
# The clash wallpaper's quadrants: one MObj (its MatAnimJoint scrolls the
# picture) over a display list of five draws, each with its own texture.
EFENTRY_MULTI = ("clashwallll", "clashwalllr", "clashwallul", "clashwallur",
                "runcrash")
# ...and of THOSE, the ones whose MObj actually animates palette_id: baked
# with MeshBaker's paletted=True so their CI4 tiles ship
# as a real swappable PVR bank instead of resolving through their TLUT
# to direct colour (MESH_PALETTED's own comment, tools/lib/ssb_assets.py) --
# every other pack this file bakes, including the rest of EFENTRY_MULTI,
# has no reason to spend a bank on a picture that never changes.
EFENTRY_PALETTED = ("clashwallll", "clashwalllr", "clashwallul",
                    "clashwallur", "runcrash")


def fill_holes(which, mobjsubs):
    n = EFENTRY_HOLES.get(which)
    if n is None:
        return mobjsubs
    for lst in mobjsubs:
        for sub in lst:
            slots = sub["sprite_slots"][:n]
            if len(slots) == n and slots[0] is not None:
                for i in range(1, n):
                    if slots[i] is None:
                        slots[i] = slots[i - 1]
                sub["sprites"] = slots
    return mobjsubs

# the two weapons whose model is one textured quad bound
# straight to a DObj -- WPDesc flags 0x00, WPAttributes.p_mobjsubs NULL,
# which is the fireball's and the blaster's path. Each entry is the
# weapon's name, its WPAttributes' file and offset symbols, the file its
# display list lives in, and the pack name.
WPQUAD = {
    "chargeshot": ("Samus's Charge Shot", "llSamusSpecial1FileID",
                   "llSamusSpecial1ChargeShotWeaponAttributes", "WPChgSht"),
    "thunderjolt": ("Pikachu's airborne Thunder Jolt", "llPikachuSpecial1FileID",
                    "llPikachuSpecial1ThunderJoltAirWeaponAttributes", "WPJolt"),
    # found by the extern reloc walk. Neither display list
    # is in one of Yoshi's own four files -- the stars' is relocData 86
    # and the egg's is 338 -- which is why no rule over the raw word
    # could have named them.
    "yoshistar": ("Yoshi's stars", "llYoshiMainFileID",
                  "llYoshiMainStarWeaponAttributes", "WPYStar"),
    "yoshieggthrow": ("Yoshi's thrown egg", "llYoshiMainFileID",
                      "llYoshiMainEggThrowWeaponAttributes", "WPYEgg"),
    # the last of the five flags-0x00 weapons, and the only
    # one that carries an MObj -- so the only one whose display list
    # branches into segment 0xE and cannot bake without it. Its two
    # palettes come across as two frames of the one MObj, exactly as
    # Mario's and Luigi's do in pack_fireball.
    "samusbomb": ("Samus's Bomb", "llSamusMainFileID",
                  "llSamusMainBombWeaponAttributes", "WPSBomb"),
    # the Ray Gun's shot (it/itcommon/itlgun.c
    # dITLGunAmmoWeaponDesc, flags 0x00): its WPAttributes are in
    # ITCommonData and its display list is wherever the extern chain says.
    # The item model exporter read that pointer as a DObjDesc tree and
    # refused it; it was never a tree.
    "lgunammo": ("The Ray Gun's shot", "llITCommonDataFileID",
                 "llITCommonDataLGunAmmoWeaponAttributes", "WPLGunAm"),
}
# Which of the four take an MObj, and how many palettes it winds through.
WPQUAD_MOBJ = {"samusbomb": 2}

# the weapons whose WPAttributes.data is a DObjDesc TREE
# rather than a display list -- WEAPON_FLAG_DOBJDESC in the WPDesc, which
# is wp/wpmanager.c:262's gcSetupCustomDObjs path. Each entry is the
# weapon's name, its WPAttributes' file and offset symbols, the pack
# name, and how many joints the tree is expected to have.
# the weapons whose WPDesc flags are 0x02 -- no
# WEAPON_FLAG_DOBJDESC, so wp/wpmanager.c takes gcAddDObjForGObj's branch,
# but WEAPON_FLAG_DOBJLINKS, so the renderer is gcDrawDObjDLLinksForGObj
# and what it walks is `dobj->dl_link`. That is the SAME FIELD
# gcAddDObjForGObj filled from attr->data (objman.c:1420, `new_dobj->dv =
# dvar`, a union) -- so for these two `data` is a DObjDLLink ARRAY, not a
# display list. Reading it as a list is what made the mesh baker report a
# gsSPModifyVertex that is not there.
#
# Both share one link array, one display list and one MObj, so one pack
# serves both and src/dc/wpmanager.c names it twice. Their four sprite
# frames have no MatAnimJoint at all: wp/wppikachu/wppikachuthunder.c
# picks among them itself, at random each tic for a trail segment and
# frame 3 outright for the head.
# EVERY WEAPON IS DRAWN TRANSLUCENT, AND ITS DISPLAY LIST DOES NOT SAY SO.
# wp/wpdisplay.c's wpDisplayMain brackets every weapon's draw in
# wpDisplayDrawNormal / wpDisplayDrawZBuffer, and the first of those sets
# G_RM_AA_XLU_SURF with G_ZBUFFER cleared on DL head 1 -- so the mode a
# weapon draws under comes from the CODE, not from the list, exactly as
# the impact ring's and Snorlax's did. MeshBaker.bucket_of can only read
# the list, and where the list sets nothing it falls back to the head it
# was replayed at, which is 0, the opaque one. In the opaque PVR list the
# alpha is ignored, and these are all glows: Pikachu's Thunder, Ness's PK
# Thunder trail, Yoshi's stars (which the Star Rod's star shares) and the
# Ray Gun's shot each drew as a BLACK SQUARE with the sprite inside it.
#
# Seeded on head 0 because that is where these bakes replay. A list that
# sets its own mode still wins -- Spearow's swarm sets G_RM_AA_ZB_TEX_EDGE
# and keeps it -- so this is the floor, not an override.
# AN EFFECT'S MODE COMES FROM THE CODE TOO. ef/efdisplay.c inserts two
# bare mode-setter GObjs into each of DL links 15 and 18
# (efDisplayMakeCLD at priority 3, efDisplayMakeXLU at priority 0), and
# both write their render mode into gSYTaskmanDLHeads[1] -- G_RM_CLD_SURF
# or G_RM_AA_ZB_XLU_SURF, whichever setter the effect's own priority puts
# it after. Either way it is a BLENDED mode on the translucent head, and
# either way the PVR list is the translucent one, which is all a bucket
# records. Seeded for the same reason as the weapons: an effect whose list
# sets no mode fell back to head 0 and drew opaque -- the fire spark and
# Kirby's star each as a black square.
EF_SEED = [A.G_RM_AA_ZB_XLU_SURF, None, None, None]

# ...but ONLY for the effects that need it, by name. Seeding every effect
# whose list is silent would move solid models -- Fox's Arwing (21
# batches), Captain Falcon's Blue Falcon (13), Mario's warp pipe, Kirby's
# Final Cutter arcs, the Barrel -- out of the opaque list, and for a batch
# whose texture is fully opaque the two lists differ only in depth
# handling: the translucent list does not write depth, so a solid model
# put there sorts against itself. The colour bug is only real where the
# texture HAS alpha, which is what build/scratch/packalpha.py measures and
# what this table is drawn from.
EF_PROC_RENDERMODE = {
    # Kirby's star -- the Star Rod's display list on a wrapper DObj, 1440
    # clear and 1990 partial texels of 4096, drawn as a black square.
    "kirbystar": EF_SEED,
}
# the Newcomers silhouettes are drawn by gcDrawDObjDLHead1 --
# head 1, the translucent list -- and their cut-out textures carry alpha
# that the opaque list would draw as black.
EF_PROC_RENDERMODE.update({k: EF_SEED for k in EFENTRY_DIRECTANIM})

WP_RENDERMODE = A.G_RM_AA_XLU_SURF
WP_SEED = [WP_RENDERMODE, None, None, None]
# What wpDisplayDrawNormal leaves for every weapon's draw (wp/
# wpdisplay.c): G_ZBUFFER cleared and no Z compare in the render mode, so
# a weapon bakes FPACK_ZALWAYS unless a list of its own turns the Z
# buffer back on. main() sets it as the baker's default for any wp*.mdl
# it writes; tools/check/effect_model_check.py does the same per weapon.
WP_ZSEED = {"ztrack": True, "zbuffer": False,
            "zcmp": [False, False, False, False]}

WPLINK = {
    "thunder": ("Pikachu's Thunder", "llPikachuMainFileID",
                "llPikachuMainThunderHeadWeaponAttributes", "WPThundr", 4),
    # G04: Ness's PK Thunder trail. Pikachu's shape again -- flags 0x02,
    # one link, one MObj, no MatAnimJoint -- but its own model: the head
    # is a tree (WPTREE "pkthunder" below), so the two do not share a
    # pack the way Pikachu's do. THREE sprites, NULL-terminated, and
    # wpnesspkthunder.c:400 draws from them with
    # syUtilsRandIntRange(WPPKTHUNDER_TEXTURES_NUM - 1): 0..2, all three.
    "pkthundertrail": ("Ness's PK Thunder trail", "llNessMainFileID",
                       "llNessMainPKThunderTrailWeaponAttributes",
                       "WPPKTrl", 3),
}

# G04: the flags-0x00 weapon whose display list draws through MObjs that
# carry no picture at all -- wp/wpmanager.c's gcAddDObjForGObj path, like
# WPQUAD's, but with p_mobjsubs AND p_matanim_joints set and no texture
# anywhere. What the MatAnimJoint moves is the MObjs' light colours, which
# is the whole of how the flame flickers. Each entry is the weapon's name,
# its WPAttributes' file and offset symbols, the pack name, and how many
# MObjs (one batch each) the list is expected to draw through.
WPFLAT = {
    "pkfire": ("Ness's PK Fire", "llNessSpecial1FileID",
               "llNessSpecial1PKFireWeaponAttributes", "WPPKFire", 2),
}

WPTREE = {
    #  name, attrs file sym, attrs sym, pack name, joints, DL links?
    "boomerang": ("Link's boomerang", "llLinkSpecial1FileID",
                  "llLinkSpecial1BoomerangWeaponAttributes", "WPBoomer",
                  3, False),
    # Flags 0x03 rather than 0x01: a tree whose geometry
    # hangs off DObjDLLink lists instead of the node's own dl pointer, so
    # wpManagerMakeWeapon gives it wpDisplayDObjTreeDLLinks. Each of its
    # two nodes carries exactly one link, both at id 1, which is why the
    # pack needs nothing new to say which link a batch belongs to.
    "cutter": ("Kirby's Final Cutter blade", "llKirbyMainFileID",
               "llKirbyMainCutterWeaponAttributes", "WPCutter", 2, True),
    # The same tree-with-links, but the first weapon whose
    # joints carry MObjs -- six of them, each with a sprite array -- and
    # the first with a MatAnimJoint stepping through them. That is
    # pack_slash's shape, and the two share matanim_block and
    # matanim_texture_ids.
    "groundjolt": ("Pikachu's ground Thunder Jolt", "llPikachuSpecial1FileID",
                   "llPikachuSpecial1ThunderJoltGroundWeaponAttributes",
                   "WPGJolt", 8, True),
    # Link's Spin Attack, the thirteenth WPDesc this port
    # compiles. Flags 0x03 again, and its attributes are in his MAIN file
    # rather than a Special -- the only weapon here whose are.
    "spinattack": ("Link's Spin Attack", "llLinkMainFileID",
                   "llLinkMainSpinAttackWeaponAttributes", "WPSpin",
                   2, True),
    # G04: Ness's PK Thunder head, and the head a reflect makes. Flags
    # 0x03, the ground jolt's shape: a two-joint tree with DL links, one
    # MObj on the leaf with a three-frame sprite array its MatAnimJoint
    # steps through, and an AnimJoint on the root that pulses its scale
    # 1.2 -> 1.0 -> 0.755 and loops. In Ness's MAIN file like the Spin
    # Attack's, with the model itself in NessModel (relocData 335).
    "pkthunder": ("Ness's PK Thunder head", "llNessMainFileID",
                  "llNessMainPKThunderWeaponAttributes", "WPPKThun",
                  2, True),
    # Master Hand's finger rocket. Flags 0x01 -- the
    # boomerang's shape and not the ground jolt's: WEAPON_FLAG_DOBJDESC
    # alone (wp/wpdef.h:14 makes that 0x1, and DOBJLINKS 0x2), so each
    # node carries its own dl and wpManagerMakeWeapon gives it
    # func_ovl3_80167618 rather than a DL-link walker.
    #
    # Its attributes are in BossMainMotion, the only weapon in the game
    # whose are in a fighter's MOTION file, and its model is in
    # BossModel -- dBossModel_DObjDescs_0x2CB8, three entries of which
    # one is real. The Normal and the Hard bullet name the same array,
    # so one pack serves both rows the way Pikachu's Thunder head and
    # trail share theirs.
    "bossbullet": ("Master Hand's finger rocket", "llBossMainMotionFileID",
                   "llBossMainMotionBulletNormalWeaponAttributes",
                   "WPBBullt", 2, False),
}


def pack_impactwave(rom):
    import pygfxd

    fid, mobj_off, dl_off, anim_off, mat_off = reloc_offsets((
        "llEFCommonEffects1FileID",
        "llEFCommonEffects1ImpactWaveMObjSub",
        "llEFCommonEffects1ImpactWaveDObjDesc",
        "llEFCommonEffects1ImpactWaveAnimJoint",
        "llEFCommonEffects1ImpactWaveMatAnimJoint"))

    f = A.get_file(rom, fid, ssb_extract)
    entry = ssb_extract.read_entry(rom, ssb_extract.RELOC_SEG, fid)
    reloc = A.walk_reloc(f, entry["reloc_intern"])

    # The one DObj, its display list bound directly (efManagerMakeEffect's
    # else/else branch), exactly as ssb_magnifyexport.py builds the handle.
    root = A.DObjNode(-1, -1, None, (0.0, 0.0, 0.0), (0.0, 0.0, 0.0),
                      (1.0, 1.0, 1.0))
    node = A.DObjNode(0, 0, dl_off, (0.0, 0.0, 0.0), (0.0, 0.0, 0.0),
                      (1.0, 1.0, 1.0))
    node.parent = root
    root.children.append(node)

    mobjsubs = A.read_mobjsubs(f, reloc, mobj_off, 1)
    if [len(l) for l in mobjsubs] != [1]:
        sys.exit("impactwave: MObjSub table reads %s MObjs, wanted one"
                 % [len(l) for l in mobjsubs])
    for lst in mobjsubs:
        for sub in lst:
            if sub["sprites"]:
                sys.exit("impactwave: MObjSub@0x%04X carries a sprite array; "
                         "the wave has one picture" % sub["off"])

    # The ring's display list sets no render mode: efManagerImpactWaveProc-
    # Display sets G_RM_AA_ZB_XLU_SURF on head 0 just before
    # gcDrawDObjDLHead0 queues it (ef/efmanager.c:3287). Without the seed
    # the head decided, head 0 is the opaque one, and the ring drew solid:
    # neither its fading primitive alpha nor its TLUT's clear entries
    # showed.
    baker = A.MeshBaker(f, pygfxd, mobjsubs, mobj_batches=True,
                        rendermode=[A.G_RM_AA_ZB_XLU_SURF, None, None, None])
    verts, tris, joints = baker.bake(root)
    if len(baker.batches) != 1 or len(baker.textures) != 1:
        sys.exit("impactwave: %d batches over %d textures, wanted one of each"
                 % (len(baker.batches), len(baker.textures)))
    if (baker.batches[0]["bucket"] & A.FPACK_LIST_MASK) != A.FPACK_LIST_TR:
        sys.exit("impactwave: the ring bakes into list %d, not the "
                 "translucent one" % baker.batches[0]["bucket"])

    joint_of = {}
    for j in joints:
        for b in range(j["batch_first"], j["batch_first"] + j["batch_count"]):
            joint_of[b] = j["node"].index

    joint_first, mobjs = [], []
    for lst in mobjsubs:
        joint_first.append((len(mobjs), len(lst)))
        mobjs.extend(lst)

    batch_mobj = []
    for i, b in enumerate(baker.batches):
        m = b["mobj"]
        batch_mobj.append(-1 if (m is None or m[0] != joint_of[i])
                          else joint_first[m[0]][0] + m[1])
    if any(m < 0 for m in batch_mobj):
        sys.exit("impactwave: %d batches have no MObj"
                 % sum(1 for m in batch_mobj if m < 0))

    # One picture per MObj: frame 0 is the one the bake interned, and the
    # MatAnimJoint scrolls its UV rather than flipping it, so tex_count is 1.
    tex_first = [0]
    tex_count = [1]

    # -- the two animations --------------------------------------------
    entries, words, relocs = animjoint_block(f, reloc, anim_off, 1,
                                              "impactwave")
    scripts = []
    for j in range(1):
        arr = reloc.get(mat_off + 4 * j)
        for k in range(len(mobjsubs[j])):
            scripts.append(None if arr is None else reloc.get(arr + 4 * k))
    if len(scripts) != len(mobjs):
        sys.exit("impactwave: %d MatAnimJoint scripts for %d MObjs"
                 % (len(scripts), len(mobjs)))
    for k, s in enumerate(scripts):
        if s is not None and matanim_texture_ids(f, reloc, s):
            sys.exit("impactwave: MObj %d's script sets texture_id_curr; "
                     "the wave has one picture" % k)
    m_entries, m_words, m_relocs = matanim_block(f, reloc, scripts,
                                                 "impactwave")

    secs = K.model_sections(
        [(n.parent.index, n.translate, n.rotate, n.scale)
         for n in baker.nodes],
        verts, tris,
        [(b["tri_first"], b["tri_count"],
          (tex_first[batch_mobj[i]],) + tuple(b["mat"][1:]),
          joint_of[i], b.get("bucket", 0))
         for i, b in enumerate(baker.batches)],
        baker.textures, baker.palettes)

    world = baker.world_verts()
    lo, hi = baker.bounds()
    cx, cy, cz = ((lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2,
                  (lo[2] + hi[2]) / 2)
    radius = max(math.sqrt((p[0] - cx) ** 2 + (p[1] - cy) ** 2 +
                           (p[2] - cz) ** 2) for p in world)

    mo = {
        "count": len(mobjs),
        "subs": b"".join(
            A.pack_mobjsub(s, tex_first[k], tex_count[k])
            for k, s in enumerate(mobjs)),
        "joint": struct.pack("<%dh" % (2 * len(mobjsubs)),
                             *[v for pair in joint_first for v in pair]),
        "batch": struct.pack("<%dh" % len(batch_mobj), *batch_mobj),
        "entry": struct.pack("<%di" % len(m_entries), *m_entries),
        "words": struct.pack("<%dI" % len(m_words), *m_words),
        "reloc": struct.pack("<%dI" % len(m_relocs), *m_relocs),
    }
    blob = build_pack(IMPACT_NAME, len(baker.nodes), secs,
                      (len(verts), len(tris), len(baker.batches),
                       len(baker.textures), len(baker.palettes)),
                      [(IMPACT_NAME, entries, words, relocs)],
                      (cx, cy, cz), radius, mobjs=mo)
    print("impactwave: file %d, DL 0x%04X, MObjSub 0x%04X, AnimJoint 0x%04X, "
          "MatAnimJoint 0x%04X: %d joint, %d verts, %d tris, %d batch, "
          "%d MObj, %d texture, %d anim words, %d matanim words"
          % (fid, dl_off, mobj_off, anim_off, mat_off, len(baker.nodes),
             len(verts), len(tris), len(baker.batches), len(mobjs),
             len(baker.textures), len(words), len(m_words)))
    return blob


# Mario's fireball, the weapon's model. Not an effect at all,
# but the same shape as one: wp/wpmanager.c's wpManagerAddModel binds the
# `WPAttributes.data` display list straight to the weapon DObj (the else
# branch, gcAddDObjForGObj), exactly as efManagerMakeEffect binds the impact
# wave's -- so the fireball is `impactwave` with no animation.
#
# The geometry lives in relocData file 297 (MarioSpecial3): a 300x300 quad
# at 0x1A8 and the CI4 16x16 sprite at 0x58 it draws, with one MObjSub at
# 0xE8 (p_mobjsubs at 0xD8) carrying the palette. `data` is not the
# `SuperJumpPunchDL` symbol at 0x1D8 but 0x1A8, its combiner-and-render-mode
# preamble; both Mario's fireball (gFTMarioFileMain row) and Luigi's
# (gFTDataLuigiSpecial1 row) point their WPAttributes.data at the same
# 0x1A8, so the model is shared and this bakes it once.
#
# The MObjSub selects among two palettes -- 0x0030 (orange, Mario) and
# 0x0008 (green, Luigi) -- by MObj.palette_id, the fighter attribute row's
# anim_frame (0 for Mario, 1 for Luigi). The pack bakes both: the same
# two-bake trick ssb_effectexport's own pack_slash uses for the damage
# slash's sprite-array frames (read_tree(k), bake, compare geometry, keep
# the textures), except keyed on palette_id here instead of
# texture_id_curr -- the fireball has one picture and two palettes where
# the slash has one palette (none) and several pictures. Baked as
# MObjSub.tex_count = 2 (frame 0 Mario's orange, frame 1 Luigi's green),
# the same array shape and runtime lookup (src/dc/fighter.c hdr_mobj) the
# slash already exercises; src/dc/objmodel.c's dc_joint_material picks the
# frame from mobj->palette_id instead of mobj->texture_id_curr for a
# PALETTE-flagged MObj, which is what wpMarioFireballMakeWeapon's verbatim
# `dobj->mobj->palette_id = anim_frame` write now actually drives.
def pack_fireball(rom):
    import pygfxd

    fid = reloc_offsets(("llMarioSpecial3FileID",))[0]
    DL_OFF = 0x1A8      # WPAttributes.data: the display list, a Gfx*
    MOBJ_OFF = 0xD8     # WPAttributes.p_mobjsubs: mobjsubs_ptr_0x00D8

    f = A.get_file(rom, fid, ssb_extract)
    entry = ssb_extract.read_entry(rom, ssb_extract.RELOC_SEG, fid)
    reloc = A.walk_reloc(f, entry["reloc_intern"])

    def read_tree(palette_id):
        """The tree and its one MObj, wound to `palette_id`. As pack_slash's
        read_tree, but the axis that varies is which palette the CI4 tile
        bakes with, not which picture; the geometry must come out the same
        every time, which is checked below."""
        root = A.DObjNode(-1, -1, None, (0.0, 0.0, 0.0), (0.0, 0.0, 0.0),
                          (1.0, 1.0, 1.0))
        node = A.DObjNode(0, 0, DL_OFF, (0.0, 0.0, 0.0), (0.0, 0.0, 0.0),
                          (1.0, 1.0, 1.0))
        node.parent = root
        root.children.append(node)

        mobjsubs = A.read_mobjsubs(f, reloc, MOBJ_OFF, 1)
        if [len(l) for l in mobjsubs] != [1]:
            sys.exit("fireball: MObjSub table reads %s MObjs, wanted one"
                     % [len(l) for l in mobjsubs])
        for lst in mobjsubs:
            for sub in lst:
                if sub["sprites"]:
                    sys.exit("fireball: MObjSub@0x%04X carries a sprite "
                             "array; the fireball has one picture"
                             % sub["off"])
                if len(sub["palettes"]) != 2:
                    sys.exit("fireball: MObjSub@0x%04X carries %d palettes, "
                             "wanted 2 (Mario's orange, Luigi's green)"
                             % (sub["off"], len(sub["palettes"])))
                sub["palette_id"] = palette_id

        baker = A.MeshBaker(f, pygfxd, mobjsubs, mobj_batches=True)
        verts, tris, joints = baker.bake(root)
        return baker, mobjsubs, verts, tris, joints

    baker, mobjsubs, verts, tris, joints = read_tree(0)
    if len(baker.batches) != 1 or len(tris) != 2 or len(baker.textures) != 1:
        sys.exit("fireball: %d batches, %d tris, %d textures; wanted 1, 2, 1"
                 % (len(baker.batches), len(tris), len(baker.textures)))
    if len(baker.nodes) != 1:
        sys.exit("fireball: %d joints, wanted one" % len(baker.nodes))

    joint_of = {}
    for j in joints:
        for b in range(j["batch_first"], j["batch_first"] + j["batch_count"]):
            joint_of[b] = j["node"].index

    joint_first, mobjs = [], []
    for lst in mobjsubs:
        joint_first.append((len(mobjs), len(lst)))
        mobjs.extend(lst)

    batch_mobj = []
    for i, b in enumerate(baker.batches):
        m = b["mobj"]
        batch_mobj.append(-1 if (m is None or m[0] != joint_of[i])
                          else joint_first[m[0]][0] + m[1])
    if any(m < 0 for m in batch_mobj):
        sys.exit("fireball: %d batches have no MObj"
                 % sum(1 for m in batch_mobj if m < 0))

    # Luigi's palette: bake again with palette_id 1. The DL and tile state
    # are unchanged, so the geometry must come out identical -- only the
    # CI4 tile's TLUT differs, which lands as a second interned texture
    # (and, if the tile is bank-paletted, a second interned palette).
    baker1, _mobjsubs1, verts1, tris1, joints1 = read_tree(1)
    if (len(verts1), len(tris1), len(baker1.batches), len(baker1.textures)) \
            != (len(verts), len(tris), len(baker.batches), 1):
        sys.exit("fireball: palette 1 bakes to %d verts, %d tris, %d "
                 "batches, %d textures; wanted %d, %d, %d, 1 (only the "
                 "picture is supposed to change)"
                 % (len(verts1), len(tris1), len(baker1.batches),
                    len(baker1.textures), len(verts), len(tris),
                    len(baker.batches)))

    # baker.textures[0] is Mario's (frame 0, the batch's own baked pick);
    # baker1.textures[0] is Luigi's, appended as frame 1. Each texture's
    # "pal" indexes its own baker's palette list, so Luigi's is shifted
    # past every palette Mario's bake already interned.
    pal_shift = len(baker.palettes)
    tex1 = dict(baker1.textures[0])
    if tex1["pal"] >= 0:
        tex1["pal"] += pal_shift
    baker.textures.append(tex1)
    baker.palettes = baker.palettes + baker1.palettes

    # One MObj, two frames of its one picture -- Mario's orange (frame 0,
    # also the batch's own FPackBatch.tex, so the fireball is orange before
    # any palette_id write) and Luigi's green (frame 1), the same shape
    # pack_slash's sprite-array frames take.
    tex_first = [0]
    tex_count = [2]

    secs = K.model_sections(
        [(n.parent.index, n.translate, n.rotate, n.scale)
         for n in baker.nodes],
        verts, tris,
        [(b["tri_first"], b["tri_count"],
          (tex_first[batch_mobj[i]],) + tuple(b["mat"][1:]),
          joint_of[i], b.get("bucket", 0))
         for i, b in enumerate(baker.batches)],
        baker.textures, baker.palettes)

    world = baker.world_verts()
    lo, hi = baker.bounds()
    cx, cy, cz = ((lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2,
                  (lo[2] + hi[2]) / 2)
    radius = max(math.sqrt((p[0] - cx) ** 2 + (p[1] - cy) ** 2 +
                           (p[2] - cz) ** 2) for p in world)

    # No AnimJoint and no MatAnimJoint (WPAttributes.anim_joints and
    # .p_matanim_joints are both NULL): every MObj's script entry is -1 and
    # the animation blocks are empty, as the spotlight's (ssb_spotexport.py).
    mo = {
        "count": len(mobjs),
        "subs": b"".join(
            A.pack_mobjsub(s, tex_first[k], tex_count[k])
            for k, s in enumerate(mobjs)),
        "joint": struct.pack("<%dh" % (2 * len(mobjsubs)),
                             *[v for pair in joint_first for v in pair]),
        "batch": struct.pack("<%dh" % len(batch_mobj), *batch_mobj),
        "entry": struct.pack("<%di" % len(mobjs), *([-1] * len(mobjs))),
        "words": b"",
        "reloc": b"",
    }
    blob = build_pack(FIREBALL_NAME, len(baker.nodes), secs,
                      (len(verts), len(tris), len(baker.batches),
                       len(baker.textures), len(baker.palettes)),
                      [], (cx, cy, cz), radius, mobjs=mo)
    b = baker.batches[0]
    print("fireball: file %d, DL 0x%04X, MObjSub 0x%04X: %d joint, %d verts, "
          "%d tris, bucket 0x%02X, %d MObj, %d palette frames, %dx%d "
          "texture, radius %.1f"
          % (fid, DL_OFF, MOBJ_OFF, len(baker.nodes), len(verts), len(tris),
             b.get("bucket", 0), len(mobjs), tex_count[0],
             baker.textures[0]["w"], baker.textures[0]["h"], radius))
    return blob


def pack_blaster(rom):
    """Fox's Blaster shot: one flat SHADE-coloured quad, no
    texture at all. wp/wpfox/wpfoxblaster.c's WPDesc carries no
    WEAPON_FLAG_DOBJDESC and its WPAttributes.p_mobjsubs is NULL (relocData
    210), so the real N64 binds relocData 316's whole DL
    (dFoxSpecial4_ReflectorDL_DisplayList) directly (wp/wpmanager.c:268)
    rather than through an MObjSub tree -- and that DL sets
    gsDPSetCombineLERP(..., SHADE, ..., 1) with gsSPTexture(... G_OFF): the
    colour cycle reads the vertex, the alpha cycle a constant 1 (opaque),
    no texel anywhere. src/dc/fighter.c's compile_batch already has this
    path -- FPackBatch.tex == -1 selects pvr_poly_cxt_col, the PVR's own
    no-texture context -- the same shaded == 2 ("vertex colour") material
    stage geometry already bakes with (ssb_packexport.py's model_sections);
    this is only the first *weapon* pack that needs it. Confirmed by
    baking the DL directly before writing this function: the baker reads
    uses_shade=True, lit=False (-> shaded 2) and alpha_tex=alpha_shade=
    False (-> alpha_src 0, matching the alpha cycle's constant 1) with no
    hand-tuning."""
    import pygfxd

    fid, dl_off = reloc_offsets(("llFoxSpecial4FileID",
                                 "llFoxSpecial4ReflectorDLDisplayList"))
    f = A.get_file(rom, fid, ssb_extract)

    zero = (0.0, 0.0, 0.0)
    one = (1.0, 1.0, 1.0)
    root = A.DObjNode(-1, -1, None, zero, zero, one)
    node = A.DObjNode(0, 0, dl_off, zero, zero, one)
    node.parent = root
    root.children.append(node)

    baker = A.MeshBaker(f, pygfxd, [[]])
    verts, tris, joints = baker.bake(root)

    if len(baker.batches) != 1:
        sys.exit("blaster: the display list bakes to %d batches, wanted one"
                 % len(baker.batches))
    if len(tris) != 2:
        sys.exit("blaster: %d triangles, wanted the two of one quad"
                 % len(tris))
    if len(baker.textures) != 0:
        sys.exit("blaster: %d textures, wanted none -- the shot is flat "
                 "SHADE colour, no texel" % len(baker.textures))
    if len(baker.nodes) != 1:
        sys.exit("blaster: %d joints, wanted one" % len(baker.nodes))

    joint_of = {}
    for j in joints:
        for b in range(j["batch_first"], j["batch_first"] + j["batch_count"]):
            joint_of[b] = j["node"].index
    secs = K.model_sections(
        [(n.parent.index, n.translate, n.rotate, n.scale)
         for n in baker.nodes],
        verts, tris,
        [(b["tri_first"], b["tri_count"], b["mat"], joint_of[i],
          b.get("bucket", 0)) for i, b in enumerate(baker.batches)],
        baker.textures, baker.palettes)

    world = baker.world_verts()
    lo, hi = baker.bounds()
    cx, cy, cz = ((lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2,
                  (lo[2] + hi[2]) / 2)
    radius = max(math.sqrt((p[0] - cx) ** 2 + (p[1] - cy) ** 2 +
                           (p[2] - cz) ** 2) for p in world)

    blob = build_pack(BLASTER_NAME, 1, secs,
                      (len(verts), len(tris), len(baker.batches),
                       len(baker.textures), len(baker.palettes)),
                      [], (cx, cy, cz), radius)
    b = baker.batches[0]
    print("blaster: file %d, DL 0x%04X: 1 joint, %d verts, %d tris, "
          "bucket 0x%02X, shaded, no texture, radius %.1f"
          % (fid, dl_off, len(verts), len(tris), b.get("bucket", 0), radius))
    return blob


def pack_arwinglaser(rom):
    """Sector Z's laser bolt: a six-vertex, four-quad cross
    -- one long spine (z -564..569, ~1133 units) with a diamond of fins
    around its middle -- textured with one 16x16 CI4 tile, SHADE and
    G_LIGHTING off.

    grsector.c's two WPDescs (Arwing2D/3D) carry render flags 0, so
    wp/wpmanager.c:268 binds `WPAttributes.data` as a bare display list to
    one DObj -- the same shape as Fox's Blaster (`pack_blaster`, above)
    and the four `pack_wpquad` weapons, and this bolt sits between them:
    it needs `pack_wpquad`'s CROSS-FILE resolution (both WPDescs' `data`
    is an EXTERN reloc, not a same-file symbol the generated header names
    -- `reloc_data.us.h` has no entry for it, only for the two
    `WPAttributes` themselves) but `pack_blaster`'s NO-MOBJSUB bake (this
    DL's tile is inline in the DL's own state, wound once, with no
    palette to swap -- neither laser's attributes carry a `p_mobjsubs`).
    Where it differs from both: it is FOUR quads, not one, and it DOES
    carry a texture, where the Blaster has none.

    BOTH WPDescs name the SAME bolt -- confirmed by resolving both
    externs and asserting they land on one (file, offset) -- so this is
    the only bake either of them needs; `src/dc/wpmanager.c`'s `sWPModels`
    gives both a row naming it."""
    import pygfxd

    afid, off2d, off3d = reloc_offsets(("llGRSectorMapFileID",
                                        "llGRSectorMapArwingLaser2DWeaponAttributes",
                                        "llGRSectorMapArwingLaser3DWeaponAttributes"))
    af = A.get_file(rom, afid, ssb_extract)
    ext = A.walk_reloc_extern(rom, ssb_extract, afid, af)

    for off in (off2d, off3d):
        if off not in ext:
            sys.exit("arwinglaser: WPAttributes.data at file %d 0x%04X is "
                     "not an extern relocation" % (afid, off))
    fid, dl_off = ext[off2d]

    if ext[off3d] != (fid, dl_off):
        sys.exit("arwinglaser: the 2D and 3D attributes' data point at "
                 "different places (file %d 0x%04X vs file %d 0x%04X); "
                 "this packer assumes one bolt for both" %
                 (fid, dl_off, ext[off3d][0], ext[off3d][1]))

    f = A.get_file(rom, fid, ssb_extract)

    zero, one = (0.0, 0.0, 0.0), (1.0, 1.0, 1.0)
    root = A.DObjNode(-1, -1, None, zero, zero, one)
    node = A.DObjNode(0, 0, dl_off, zero, zero, one)
    node.parent = root
    root.children.append(node)

    baker = A.MeshBaker(f, pygfxd, [[]])
    verts, tris, joints = baker.bake(root)

    if len(baker.batches) != 1:
        sys.exit("arwinglaser: the display list bakes to %d batches, "
                 "wanted one" % len(baker.batches))
    if len(tris) != 8:
        sys.exit("arwinglaser: %d triangles, wanted the eight of the "
                 "spine-and-fins cross" % len(tris))
    if len(baker.textures) != 1:
        sys.exit("arwinglaser: %d textures, wanted one" %
                 len(baker.textures))
    if len(baker.nodes) != 1:
        sys.exit("arwinglaser: %d joints, wanted one" % len(baker.nodes))
    # tex 0, G_LIGHTING off, alpha from the texel, SHADE read for colour --
    # pack_wpquad's own "Blaster's stage" material shape (its comment
    # calls out SHADE+unlit as one of the two shapes its four weapons
    # take), except THIS one keeps a real texture where the Blaster has
    # none: SHADE tints the tile rather than replacing it.
    mat = baker.batches[0]["mat"]
    if mat[0] != 0 or mat[5] is not False or mat[6] is not True:
        sys.exit("arwinglaser: material key %r is not (tex 0, unlit, "
                 "alpha from the texel)" % (mat,))
    if mat[4] is not True:
        sys.exit("arwinglaser: material key %r does not read SHADE" %
                 (mat,))

    joint_of = {}
    for j in joints:
        for b in range(j["batch_first"], j["batch_first"] + j["batch_count"]):
            joint_of[b] = j["node"].index
    secs = K.model_sections(
        [(n.parent.index, n.translate, n.rotate, n.scale)
         for n in baker.nodes],
        verts, tris,
        [(b["tri_first"], b["tri_count"], b["mat"], joint_of[i],
          b.get("bucket", 0)) for i, b in enumerate(baker.batches)],
        baker.textures, baker.palettes)

    world = baker.world_verts()
    lo, hi = baker.bounds()
    cx, cy, cz = ((lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2,
                  (lo[2] + hi[2]) / 2)
    radius = max(math.sqrt((p[0] - cx) ** 2 + (p[1] - cy) ** 2 +
                           (p[2] - cz) ** 2) for p in world)

    blob = build_pack(ARWINGLASER_NAME, 1, secs,
                      (len(verts), len(tris), len(baker.batches),
                       len(baker.textures), len(baker.palettes)),
                      [], (cx, cy, cz), radius)
    b = baker.batches[0]
    print("arwinglaser: file %d, DL 0x%04X: 1 joint, %d verts, %d tris, "
          "bucket 0x%02X, shaded, 1 texture, radius %.1f"
          % (fid, dl_off, len(verts), len(tris), b.get("bucket", 0), radius))
    return blob


def pack_wpquad(rom, which):
    """A weapon whose whole model is one textured quad.

    Five of the twelve WPDescs this port compiles carry flags 0x00 -- no
    WEAPON_FLAG_DOBJDESC, no WEAPON_FLAG_DOBJLINKS -- which is
    wp/wpmanager.c:268's path: `gcAddDObjForGObj(weapon_gobj, attr->data)`
    binds WPAttributes.data as a display list to one DObj, and nothing
    else. Two of the five are Mario's fireball and Fox's blaster, already
    packed. Two more are these, and they are simpler than either: one
    quad, one CI4 tile with its TLUT inline in the tile state, no MObjSub
    tree at all (so no palette winding, which is the whole of what makes
    pack_fireball long) and no AnimJoint.

    The fifth is Samus's Bomb, whose `data` does not decode to a display
    list in any of her four files, so it is not handled here.

    WPAttributes.data is not a symbol in the generated header, so it is
    read out of the attribute record and put through A.ptr_off, the
    deterministic (ptr & 0xFFFF) * 4 rule. The DL it names lives in the
    fighter's Special3 file, not in the Special1/Main file the attributes
    are in, which is why the geometry file is named separately."""
    import pygfxd

    who, attr_file_sym, attr_sym, pack_name = WPQUAD[which]
    afid, aoff = reloc_offsets((attr_file_sym, attr_sym))

    npal = WPQUAD_MOBJ.get(which, 0)

    af = A.get_file(rom, afid, ssb_extract)
    _data, mobj, _anim, matanim = struct.unpack_from(">IIII", af, aoff)
    if matanim != 0:
        sys.exit("%s: WPAttributes carries p_matanim_joints 0x%08X; no "
                 "weapon this packer takes has a material animation"
                 % (which, matanim))
    if (mobj != 0) != (npal != 0):
        sys.exit("%s: WPAttributes p_mobjsubs is 0x%08X but WPQUAD_MOBJ "
                 "says %d palettes" % (which, mobj, npal))
    # Which file the display list is in, and where, comes out of the
    # EXTERN reloc chain: the record's `data` word is
    # patched at load from another relocData file, and the chain says
    # which one and at what offset. Step 72's two happened to have
    # offsets small enough that the UNRELOCATED (ptr & 0xFFFF) * 4 rule
    # guessed the same answer; Yoshi's two do not, and their lists are
    # not even in one of their fighter's own four files.
    ext = A.walk_reloc_extern(rom, ssb_extract, afid, af)

    if aoff not in ext:
        sys.exit("%s: WPAttributes.data at file %d 0x%04X is not an extern "
                 "relocation; this packer reads the chain, not the raw word"
                 % (which, afid, aoff))
    fid, dl_off = ext[aoff]

    f = A.get_file(rom, fid, ssb_extract)
    if dl_off + 8 > len(f):
        sys.exit("%s: the extern chain points at file %d 0x%04X, past its "
                 "end (0x%X bytes)" % (which, fid, dl_off, len(f)))

    zero, one = (0.0, 0.0, 0.0), (1.0, 1.0, 1.0)

    def read_tree(palette_id):
        """The one-node tree, and its MObj wound to `palette_id` when it
        has one. As pack_fireball's: the geometry must come out the same
        for every palette, which is checked below."""
        root = A.DObjNode(-1, -1, None, zero, zero, one)
        node = A.DObjNode(0, 0, dl_off, zero, zero, one)
        node.parent = root
        root.children.append(node)

        if npal == 0:
            b = A.MeshBaker(f, pygfxd, [[]],
                rendermode=WP_SEED)
            return (b, [[]]) + b.bake(root)

        # p_mobjsubs is an extern link of its own, and for the one weapon
        # that has it, it lands in the same file the display list does.
        if (aoff + 4) not in ext:
            sys.exit("%s: p_mobjsubs is not an extern relocation" % which)
        mfid, moff = ext[aoff + 4]

        if mfid != fid:
            sys.exit("%s: the MObjSub table is in file %d and the display "
                     "list in file %d; this packer reads one file"
                     % (which, mfid, fid))
        mreloc = A.walk_reloc(
            f, ssb_extract.read_entry(rom, ssb_extract.RELOC_SEG,
                                      fid)["reloc_intern"])
        subs = A.read_mobjsubs(f, mreloc, moff, 1)

        if [len(l) for l in subs] != [1]:
            sys.exit("%s: MObjSub table reads %s MObjs, wanted one"
                     % (which, [len(l) for l in subs]))
        for lst in subs:
            for sub in lst:
                if sub["sprites"]:
                    sys.exit("%s: MObjSub@0x%04X carries a sprite array; "
                             "this weapon has one picture"
                             % (which, sub["off"]))
                if len(sub["palettes"]) != npal:
                    sys.exit("%s: MObjSub@0x%04X carries %d palettes, "
                             "wanted %d" % (which, sub["off"],
                                            len(sub["palettes"]), npal))
                sub["palette_id"] = palette_id
        b = A.MeshBaker(f, pygfxd, subs, mobj_batches=True,
            rendermode=WP_SEED)
        return (b, subs) + b.bake(root)

    baker, mobjsubs, verts, tris, joints = read_tree(0)

    if len(baker.batches) != 1:
        sys.exit("%s: the display list bakes to %d batches, wanted one"
                 % (which, len(baker.batches)))
    if len(tris) != 2:
        sys.exit("%s: %d triangles, wanted the two of one quad"
                 % (which, len(tris)))
    if len(baker.textures) != 1:
        sys.exit("%s: %d textures, wanted one" % (which, len(baker.textures)))
    if len(baker.nodes) != 1:
        sys.exit("%s: %d joints, wanted one" % (which, len(baker.nodes)))
    # Two material shapes turn up among these four, and both are stages
    # this port already bakes. Three take SHADE with lighting off, which
    # is `shaded 2`, vertex colour -- the Blaster's stage. Yoshi's stars
    # take neither shade nor lighting but DO lerp ENV and PRIM by the
    # texel, which is `shaded 0` plus FPACK_ENVLERP -- the shield
    # bubble's stage, and the reason the stars come out
    # yellow with no light on them. Everything else has to hold either
    # way, and a quad that reads NEITHER would bake flat and untinted.
    mat = baker.batches[0]["mat"]
    if mat[0] != 0 or mat[5] is not False or mat[6] is not True:
        sys.exit("%s: material key %r is not (tex 0, unlit, alpha from "
                 "the texel)" % (which, mat))
    if mat[4] is not True and mat[8] is None:
        sys.exit("%s: material key %r reads neither SHADE nor an ENV lerp"
                 % (which, mat))

    joint_of = {}
    for j in joints:
        for b in range(j["batch_first"], j["batch_first"] + j["batch_count"]):
            joint_of[b] = j["node"].index

    # A weapon with no MObj bakes its one picture straight into the batch.
    # One with an MObj carries every palette as a frame of it, because the
    # weapon's own code winds palette_id at runtime -- Samus's Bomb blinks
    # between its two as the fuse burns down (wpsamusbomb.c:107), faster
    # near the end, and Mario's fireball picks Luigi's the same way. Each
    # extra bake must give the SAME geometry and differ only in the tile's
    # TLUT, which is what is checked here.
    mo = None
    batch_mat = [b["mat"] for b in baker.batches]

    if npal:
        for pal in range(1, npal):
            bakerN, _s, vertsN, trisN, _j = read_tree(pal)

            if (len(vertsN), len(trisN), len(bakerN.batches),
                    len(bakerN.textures)) != (len(verts), len(tris),
                                              len(baker.batches), 1):
                sys.exit("%s: palette %d bakes to %d verts, %d tris, %d "
                         "batches, %d textures; wanted %d, %d, %d, 1 -- "
                         "only the picture is supposed to change"
                         % (which, pal, len(vertsN), len(trisN),
                            len(bakerN.batches), len(bakerN.textures),
                            len(verts), len(tris), len(baker.batches)))
            texN = dict(bakerN.textures[0])

            if texN["pal"] >= 0:
                texN["pal"] += len(baker.palettes)
            baker.textures.append(texN)
            baker.palettes = baker.palettes + bakerN.palettes

        joint_first, mobjs = [], []
        for lst in mobjsubs:
            joint_first.append((len(mobjs), len(lst)))
            mobjs.extend(lst)

        batch_mobj = []
        for i, b in enumerate(baker.batches):
            m = b["mobj"]
            batch_mobj.append(-1 if (m is None or m[0] != joint_of[i])
                              else joint_first[m[0]][0] + m[1])
        if any(m < 0 for m in batch_mobj):
            sys.exit("%s: %d batches have no MObj"
                     % (which, sum(1 for m in batch_mobj if m < 0)))
        tex_first = [0] * len(mobjs)
        tex_count = [npal] * len(mobjs)
        batch_mat = [(tex_first[batch_mobj[i]],) + tuple(b["mat"][1:])
                     for i, b in enumerate(baker.batches)]
        mo = {
            "count": len(mobjs),
            "subs": b"".join(
                A.pack_mobjsub(sub, tex_first[k], tex_count[k])
                for k, sub in enumerate(mobjs)),
            "joint": struct.pack("<%dh" % (2 * len(mobjsubs)),
                                 *[v for pair in joint_first for v in pair]),
            "batch": struct.pack("<%dh" % len(batch_mobj), *batch_mobj),
            "entry": struct.pack("<%di" % len(mobjs), *([-1] * len(mobjs))),
            "words": b"",
            "reloc": b"",
        }
    secs = K.model_sections(
        [(n.parent.index, n.translate, n.rotate, n.scale)
         for n in baker.nodes],
        verts, tris,
        [(b["tri_first"], b["tri_count"], batch_mat[i], joint_of[i],
          b.get("bucket", 0)) for i, b in enumerate(baker.batches)],
        baker.textures, baker.palettes)

    world = baker.world_verts()
    lo, hi = baker.bounds()
    cx, cy, cz = ((lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2,
                  (lo[2] + hi[2]) / 2)
    radius = max(math.sqrt((p[0] - cx) ** 2 + (p[1] - cy) ** 2 +
                           (p[2] - cz) ** 2) for p in world)

    blob = build_pack(pack_name, 1, secs,
                      (len(verts), len(tris), len(baker.batches),
                       len(baker.textures), len(baker.palettes)),
                      [], (cx, cy, cz), radius, mobjs=mo)
    b = baker.batches[0]
    t = baker.textures[0]
    print("%s: %s -- attrs file %d @0x%04X, extern -> file %d @0x%04X: "
          "1 joint, %d verts, %d tris, bucket 0x%02X, %dx%d texture, "
          "%d palettes, %s, radius %.1f"
          % (which, who, afid, aoff, fid, dl_off, len(verts), len(tris),
             b.get("bucket", 0), t["w"], t["h"], len(baker.palettes),
             "shade" if mat[4] else "env lerp 0x%06X" % mat[8], radius))
    if npal:
        print("%s: one MObj, %d palette frames" % (which, npal))
    return blob


def pack_wptree(rom, which):
    """A weapon whose model is a DObjDesc tree.

    Five of the twelve WPDescs this port compiles set
    WEAPON_FLAG_DOBJDESC, so wp/wpmanager.c:262 builds their model with
    gcSetupCustomDObjs over a DObjDesc tree instead of binding one
    display list to one DObj. That is the last shape of weapon model this
    port could not pack, and Link's boomerang is the first of them.

    Everything that was learned on the flat five still holds: the
    attribute record's `data` is an EXTERN relocation, and
    so is `anim_joints` -- both into the fighter's Special3 file here. A
    weapon with a tree is also the first with an ANIMATION: the boomerang
    turns end over end, and that is an AnimJoint, not anything the weapon
    code does. The block comes across the way the halo's does.
    """
    import pygfxd

    who, attr_file_sym, attr_sym, pack_name, njoints_want, links = \
        WPTREE[which]
    afid, aoff = reloc_offsets((attr_file_sym, attr_sym))

    af = A.get_file(rom, afid, ssb_extract)
    _data, mobj, _anim, matanim = struct.unpack_from(">IIII", af, aoff)
    if (mobj != 0) != (matanim != 0):
        sys.exit("%s: WPAttributes has p_mobjsubs 0x%08X and "
                 "p_matanim_joints 0x%08X; a weapon here has both or "
                 "neither" % (which, mobj, matanim))

    ext = A.walk_reloc_extern(rom, ssb_extract, afid, af)
    if aoff not in ext:
        sys.exit("%s: WPAttributes.data is not an extern relocation" % which)
    fid, dobj_off = ext[aoff]

    # A tree that does not animate itself. Every weapon
    # here until Master Hand's finger rocket had an AnimJoint -- the
    # boomerang turns end over end, the jolt crawls -- but his bullet
    # only flies where wpbossbullet.c pushes it, so BossMainMotion
    # leaves anim_joints NULL and relocates nothing there.
    #
    # NULL and unrelocated, or relocated and non-NULL. A record whose
    # word is set but which nothing relocated is a ROM offset where a
    # pointer belongs -- the exact damage once found in the item
    # pack -- so it still aborts.
    anim_reloc = (aoff + 8) in ext
    if anim_reloc != (_anim != 0):
        sys.exit("%s: WPAttributes.anim_joints is 0x%08X and %s an extern "
                 "relocation" % (which, _anim,
                                 "IS" if anim_reloc else "is NOT"))
    anim_off = None
    if anim_reloc:
        afid2, anim_off = ext[aoff + 8]
        if afid2 != fid:
            sys.exit("%s: the tree is in file %d and its animation in file "
                     "%d; this packer reads one file" % (which, fid, afid2))
    f = A.get_file(rom, fid, ssb_extract)
    reloc = A.walk_reloc(
        f, ssb_extract.read_entry(rom, ssb_extract.RELOC_SEG,
                                  fid)["reloc_intern"])

    mat_off = ext[aoff + 12][1] if (aoff + 12) in ext else None

    def read_tree(frame, spans=None):
        """The tree, and its MObjs wound to `frame`.

        `spans` is how long each MObj's sprite array really is, one entry
        per MObj in tree order. Without it the arrays come back
        NULL-terminated, which is the WRONG rule for a sprite array and
        the reason this weapon was misread once: the six arrays here
        OVERLAP -- node 3's begins at node 2's fourth entry -- so they
        are one shared pool each MObj enters at a different offset, and a
        terminator scan gives six different lengths for one run. What
        bounds an array is the script, which is what matanim_texture_ids
        has said all along.

        A frame whose entry in that span is NULL is one the script never
        selects (checked below), so it is baked as frame 0 -- a filler
        that keeps the run contiguous for the runtime to index and can
        never be reached."""
        r, ns = A.read_dobj_tree(f, reloc, dobj_off, dl_links=links)

        if mat_off is None:
            bk = A.MeshBaker(f, pygfxd, [[]] * len(ns),
                rendermode=WP_SEED)
            return (bk, ns, [[]] * len(ns)) + bk.bake(r)
        sb = A.read_mobjsubs(f, reloc, ext[aoff + 4][1], len(ns))
        k = 0
        for lst in sb:
            for sub in lst:
                arr = (reloc.get(sub["off"] + 0x04)
                       if spans is not None else None)

                # An MObj with no sprite array at all keeps the empty
                # list read_mobjsub gave it: what its MatAnimJoint moves
                # is a colour rather than a picture, which is Link's
                # Spin Attack and neither of the two
                # weapons this closure was written for. The span walk
                # below already allowed for it; this did not.
                if arr is not None:
                    sub["sprites"] = [reloc.get(arr + 4 * i)
                                      for i in range(spans[k])]
                if sub["sprites"]:
                    i = min(frame, len(sub["sprites"]) - 1)
                    sub["texture_id_curr"] = 0 if sub["sprites"][i] is None \
                        else i
                k += 1
        bk = A.MeshBaker(f, pygfxd, sb, mobj_batches=True,
            rendermode=WP_SEED)
        return (bk, ns, sb) + bk.bake(r)

    baker, nodes, mobjsubs, verts, tris, joints = read_tree(0)

    # A node with DL links has its geometry on the links rather than on
    # its own dl pointer, and the game draws ONE link at a time
    # (gcDrawDObjTreeDLLinksForGObj). The port draws every batch a pack
    # carries, so a node with more than one link would need the pack to
    # say which link each batch came from -- and neither weapon here has
    # one. Checked rather than assumed.
    if links:
        for n in nodes:
            if len(n.dl_links) > 1:
                sys.exit("%s: joint %d carries %d DL links; the pack has "
                         "no way to say which one a batch belongs to"
                         % (which, n.index, len(n.dl_links)))

    if len(nodes) != njoints_want:
        sys.exit("%s: the DObjDesc tree reads %d joints, wanted %d"
                 % (which, len(nodes), njoints_want))
    if mat_off is None and len(baker.textures) != 1:
        sys.exit("%s: %d textures, wanted one" % (which, len(baker.textures)))
    if not baker.batches:
        sys.exit("%s: the tree bakes to no geometry at all" % which)
    for b in baker.batches:
        mat = b["mat"]
        if mat[5] is not False:
            sys.exit("%s: a batch is lit; no weapon in this port is"
                     % which)

    joint_of = {}
    for j in joints:
        for b in range(j["batch_first"], j["batch_first"] + j["batch_count"]):
            joint_of[b] = j["node"].index
    # -- the MObjs and their sprite arrays ------------------------------
    mo = None
    batch_mat = [b["mat"] for b in baker.batches]

    if mat_off is not None:
        joint_first, mobjs = [], []
        for lst in mobjsubs:
            joint_first.append((len(mobjs), len(lst)))
            mobjs.extend(lst)

        batch_mobj = []
        for i, b in enumerate(baker.batches):
            m = b["mobj"]
            batch_mobj.append(-1 if (m is None or m[0] != joint_of[i])
                              else joint_first[m[0]][0] + m[1])
        if any(m < 0 for m in batch_mobj):
            sys.exit("%s: %d batches have no MObj"
                     % (which, sum(1 for m in batch_mobj if m < 0)))

        scripts = []
        for j in range(len(nodes)):
            arr = reloc.get(mat_off + 4 * j)
            for k in range(len(mobjsubs[j])):
                scripts.append(None if arr is None
                               else reloc.get(arr + 4 * k))
        if len(scripts) != len(mobjs):
            sys.exit("%s: %d MatAnimJoint scripts for %d MObjs"
                     % (which, len(scripts), len(mobjs)))

        # What indexes a sprite array is the TRUNCATION of
        # texture_id_curr -- `sprites[(s32)id]` in the game,
        # src/dc/objmodel.c:152's assignment to a uint16_t here -- so the
        # ids a script gives are read the way C reads them, toward zero.
        # This weapon's own scripts step through tiny values of both
        # signs (1.1e-08 and -2.4e-08) on their way between whole ones,
        # and every one of those is frame 0.
        ids_of, nframes = [], []
        for k, sub in enumerate(mobjs):
            ids = set() if scripts[k] is None else \
                matanim_texture_ids(f, reloc, scripts[k])
            trunc = set(int(v) for v in ids)

            if min(trunc, default=0) < 0:
                sys.exit("%s: MObj %d's script sets texture_id_curr to "
                         "%s; truncated that indexes off the front of "
                         "the array" % (which, k, sorted(ids)))
            ids_of.append(trunc)
            nframes.append(1 if not trunc else max(trunc) + 1)

        # The run has to be contiguous for the runtime to index it, and
        # this weapon's is SPARSE: one of its six MObjs steps 0 -> 2 and
        # never sets 1, and the entry at 1 in the shared pool is a
        # genuine NULL. A NULL the script never selects is filled with
        # frame 0 and nothing ever draws it; a NULL the script DOES
        # select would be a frame that draws from a null texture image on
        # the N64, which is not something to reproduce -- so it stops the
        # bake instead.
        spans = list(nframes)
        for k, sub in enumerate(mobjs):
            arr = reloc.get(sub["off"] + 0x04)
            if arr is None:
                if nframes[k] > 1:
                    sys.exit("%s: MObj %d has no sprite array but its "
                             "script reaches frame %d"
                             % (which, k, nframes[k] - 1))
                continue
            for i in range(nframes[k]):
                if reloc.get(arr + 4 * i) is None and i in ids_of[k]:
                    sys.exit("%s: MObj %d's script selects frame %d and "
                             "that entry is NULL; the game would draw "
                             "from a null texture image there"
                             % (which, k, i))

        batch_of = []
        for k in range(len(mobjs)):
            bs = [i for i, m in enumerate(batch_mobj) if m == k]
            texs = set(baker.batches[i]["mat"][0] for i in bs)
            if len(bs) != 1 or len(texs) != 1:
                sys.exit("%s: MObj %d draws %d batches over %d textures; "
                         "a sprite array replaces one picture, not several"
                         % (which, k, len(bs), len(texs)))
            batch_of.append(bs[0])

        # Re-read with the real spans, then once per frame after that.
        baker, nodes, mobjsubs, verts, tris, joints = read_tree(0, spans)
        joint_of = {}
        for j in joints:
            for b in range(j["batch_first"],
                           j["batch_first"] + j["batch_count"]):
                joint_of[b] = j["node"].index

        later = [[] for _ in mobjs]
        for frame in range(1, max(nframes)):
            b2, _n2, _s2, v2, t2, _j2 = read_tree(frame, spans)
            if (len(v2), len(t2), len(b2.batches), b2.palettes) != \
                    (len(verts), len(tris), len(baker.batches),
                     baker.palettes):
                sys.exit("%s: frame %d bakes to different geometry; only "
                         "the picture is supposed to change"
                         % (which, frame))
            for k in range(len(mobjs)):
                if frame < nframes[k]:
                    later[k].append(
                        b2.textures[b2.batches[batch_of[k]]["mat"][0]])

        # pack_slash asserts one interned texture per MObj here, because
        # its two draw different pictures. This weapon's six all start on
        # the SAME picture, so the baker interns one and the table below
        # repeats it -- six runs of three, eighteen entries over far fewer
        # distinct images. That costs romdisk bytes and buys the runtime a
        # contiguous run per MObj to index, which is the trade the format
        # already makes.
        for k in range(len(mobjs)):
            own = baker.batches[batch_of[k]]["mat"][0]
            if not 0 <= own < len(baker.textures):
                sys.exit("%s: MObj %d's frame 0 is texture %d of %d"
                         % (which, k, own, len(baker.textures)))
        frames, tex_first, tex_count = [], [], []
        for k in range(len(mobjs)):
            own = baker.batches[batch_of[k]]["mat"][0]
            if own < 0:
                sys.exit("%s: MObj %d draws an untextured batch"
                         % (which, k))
            tex_first.append(len(frames))
            frames.append(baker.textures[own])
            frames.extend(later[k])
            tex_count.append(nframes[k])
        baker.textures = frames
        batch_mat = [(tex_first[batch_mobj[i]],) + tuple(b["mat"][1:])
                     for i, b in enumerate(baker.batches)]
        m_entries, m_words, m_relocs = matanim_block(f, reloc, scripts,
                                                     which)
        mo = {
            "count": len(mobjs),
            "subs": b"".join(
                A.pack_mobjsub(sub, tex_first[k], tex_count[k])
                for k, sub in enumerate(mobjs)),
            "joint": struct.pack("<%dh" % (2 * len(mobjsubs)),
                                 *[v for pair in joint_first for v in pair]),
            "batch": struct.pack("<%dh" % len(batch_mobj), *batch_mobj),
            "entry": struct.pack("<%di" % len(m_entries), *m_entries),
            "words": struct.pack("<%dI" % len(m_words), *m_words),
            "reloc": struct.pack("<%dI" % len(m_relocs), *m_relocs),
        }
    secs = K.model_sections(
        [(n.parent.index, n.translate, n.rotate, n.scale)
         for n in baker.nodes],
        verts, tris,
        [(b["tri_first"], b["tri_count"], batch_mat[i], joint_of[i],
          b.get("bucket", 0)) for i, b in enumerate(baker.batches)],
        baker.textures, baker.palettes)

    if anim_off is None:
        # No script for any joint: every entry -1, no words, no
        # pointers. The loader builds the tree and hangs nothing on it.
        entries, words, relocs = [-1] * len(nodes), [], []
    else:
        entries, words, relocs = animjoint_block(f, reloc, anim_off,
                                                 len(nodes), which)

    world = baker.world_verts()
    lo, hi = baker.bounds()
    cx, cy, cz = ((lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2,
                  (lo[2] + hi[2]) / 2)
    radius = max(math.sqrt((p[0] - cx) ** 2 + (p[1] - cy) ** 2 +
                           (p[2] - cz) ** 2) for p in world)

    blob = build_pack(pack_name, len(nodes), secs,
                      (len(verts), len(tris), len(baker.batches),
                       len(baker.textures), len(baker.palettes)),
                      [(pack_name, entries, words, relocs)],
                      (cx, cy, cz), radius, mobjs=mo)
    print("%s: %s -- attrs file %d @0x%04X, extern -> tree file %d "
          "@0x%04X%s, AnimJoint @0x%04X: %d joints, %d verts, %d tris, "
          "%d batches, %dx%d texture, %d driven joints, %d anim words, "
          "%d pointers, radius %.1f"
          % (which, who, afid, aoff, fid, dobj_off,
             " (DL links)" if links else "",
             anim_off if anim_off is not None else 0, len(nodes),
             len(verts), len(tris), len(baker.batches),
             baker.textures[0]["w"], baker.textures[0]["h"],
             sum(1 for e in entries if e >= 0), len(words), len(relocs),
             radius))
    if mo is not None:
        print("%s: %d MObjs, sprite frames %s, %d MatAnimJoint words"
              % (which, mo["count"], tex_count, len(m_words)))
    return blob


def pack_wplink(rom, which):
    """A weapon whose WPAttributes.data is a DObjDLLink array.

    The last shape of weapon model in the game this port could not pack,
    and the only one whose `data` is not geometry at all: it is the array
    gcDrawDObjDLLinksForGObj walks, `{s32 list_id; Gfx *dl}` terminated
    by a list_id of 4. One link, one display list behind it.

    Its MObj carries a sprite array and NO MatAnimJoint, which is the one
    place the NULL-terminated read of that array is the right rule --
    there is no script to bound it, because the weapon's own code picks
    the frame (wppikachuthunder.c: syUtilsRandIntRange for a trail
    segment, 3 for the head).
    """
    import pygfxd

    who, attr_file_sym, attr_sym, pack_name, nsprites = WPLINK[which]
    afid, aoff = reloc_offsets((attr_file_sym, attr_sym))

    af = A.get_file(rom, afid, ssb_extract)
    _d, mobj, anim, matanim = struct.unpack_from(">IIII", af, aoff)
    if anim != 0 or matanim != 0:
        sys.exit("%s: WPAttributes carries anim_joints 0x%08X and "
                 "p_matanim_joints 0x%08X; this weapon animates itself"
                 % (which, anim, matanim))
    if mobj == 0:
        sys.exit("%s: WPAttributes carries no p_mobjsubs" % which)

    ext = A.walk_reloc_extern(rom, ssb_extract, afid, af)
    for name, off in (("data", aoff), ("p_mobjsubs", aoff + 4)):
        if off not in ext:
            sys.exit("%s: WPAttributes.%s is not an extern relocation"
                     % (which, name))
    fid, link_off = ext[aoff]
    mfid, mobj_off = ext[aoff + 4]

    if mfid != fid:
        sys.exit("%s: the link array is in file %d and the MObjSub table "
                 "in file %d; this packer reads one file"
                 % (which, fid, mfid))
    f = A.get_file(rom, fid, ssb_extract)
    reloc = A.walk_reloc(
        f, ssb_extract.read_entry(rom, ssb_extract.RELOC_SEG,
                                  fid)["reloc_intern"])

    dls = read_dl_links(f, reloc, link_off, which)
    if len(dls) != 1:
        sys.exit("%s: the DObjDLLink array holds %d lists, wanted one"
                 % (which, len(dls)))
    list_id, dl_off = dls[0]

    zero, one = (0.0, 0.0, 0.0), (1.0, 1.0, 1.0)

    def read_tree(frame):
        root = A.DObjNode(-1, -1, None, zero, zero, one)
        node = A.DObjNode(0, 0, dl_off, zero, zero, one)
        node.parent = root
        root.children.append(node)
        subs = A.read_mobjsubs(f, reloc, mobj_off, 1)

        if [len(l) for l in subs] != [1]:
            sys.exit("%s: MObjSub table reads %s MObjs, wanted one"
                     % (which, [len(l) for l in subs]))
        for lst in subs:
            for sub in lst:
                if len(sub["sprites"]) != nsprites:
                    sys.exit("%s: the MObj carries %d sprites, wanted %d"
                             % (which, len(sub["sprites"]), nsprites))
                sub["texture_id_curr"] = frame
        b = A.MeshBaker(f, pygfxd, subs, mobj_batches=True,
            rendermode=WP_SEED)
        return (b, subs) + b.bake(root)

    baker, mobjsubs, verts, tris, joints = read_tree(0)

    if len(baker.batches) != 1 or len(baker.nodes) != 1:
        sys.exit("%s: %d batches over %d joints, wanted one of each"
                 % (which, len(baker.batches), len(baker.nodes)))
    if len(baker.textures) != 1:
        sys.exit("%s: frame 0 interned %d textures, wanted one"
                 % (which, len(baker.textures)))

    joint_of = {}
    for j in joints:
        for b in range(j["batch_first"], j["batch_first"] + j["batch_count"]):
            joint_of[b] = j["node"].index

    frames = [baker.textures[0]]
    for frame in range(1, nsprites):
        b2, _s2, v2, t2, _j2 = read_tree(frame)
        if (len(v2), len(t2), len(b2.batches), b2.palettes) != \
                (len(verts), len(tris), len(baker.batches), baker.palettes):
            sys.exit("%s: frame %d bakes to different geometry; only the "
                     "picture is supposed to change" % (which, frame))
        frames.append(b2.textures[b2.batches[0]["mat"][0]])
    baker.textures = frames

    mobjs = [sub for lst in mobjsubs for sub in lst]
    mo = {
        "count": 1,
        "subs": A.pack_mobjsub(mobjs[0], 0, nsprites),
        "joint": struct.pack("<2h", 0, 1),
        "batch": struct.pack("<h", 0),
        "entry": struct.pack("<i", -1),
        "words": b"",
        "reloc": b"",
    }
    secs = K.model_sections(
        [(n.parent.index, n.translate, n.rotate, n.scale)
         for n in baker.nodes],
        verts, tris,
        [(b["tri_first"], b["tri_count"], (0,) + tuple(b["mat"][1:]),
          joint_of[i], b.get("bucket", 0))
         for i, b in enumerate(baker.batches)],
        baker.textures, baker.palettes)

    world = baker.world_verts()
    lo, hi = baker.bounds()
    cx, cy, cz = ((lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2,
                  (lo[2] + hi[2]) / 2)
    radius = max(math.sqrt((p[0] - cx) ** 2 + (p[1] - cy) ** 2 +
                           (p[2] - cz) ** 2) for p in world)

    blob = build_pack(pack_name, 1, secs,
                      (len(verts), len(tris), len(baker.batches),
                       len(baker.textures), len(baker.palettes)),
                      [], (cx, cy, cz), radius, mobjs=mo)
    print("%s: %s -- attrs file %d @0x%04X, extern -> DObjDLLink array "
          "file %d @0x%04X, link %d -> DL @0x%04X: 1 joint, %d verts, "
          "%d tris, bucket 0x%02X, %dx%d texture, %d sprite frames, "
          "radius %.1f"
          % (which, who, afid, aoff, fid, link_off, list_id, dl_off,
             len(verts), len(tris), baker.batches[0].get("bucket", 0),
             baker.textures[0]["w"], baker.textures[0]["h"], nsprites,
             radius))
    return blob


def pack_wpflat(rom, which):
    """A flags-0x00 weapon drawn through colour-only MObjs (G04).

    wp/wpmanager.c binds WPAttributes.data to one DObj as a display list
    (gcAddDObjForGObj), as for WPQUAD's weapons, then hangs p_mobjsubs off
    it and gcAddAnimAll's p_matanim_joints over those. pack_wpquad refuses
    a MatAnimJoint because none of its five has one; this weapon has
    nothing else. Its list is two single triangles, each after a
    G_DL into segment 0xE -- one per MObj -- untextured, lit, over normals
    that face the camera, so what colours them is the MObjs' two light
    colours, and the MatAnimJoint's only track is those. No sprite array,
    no palette, no texture: every MObj's run is the batch's own tex, -1.
    """
    import pygfxd

    who, attr_file_sym, attr_sym, pack_name, nmobjs = WPFLAT[which]
    afid, aoff = reloc_offsets((attr_file_sym, attr_sym))

    af = A.get_file(rom, afid, ssb_extract)
    _d, mobj, anim, matanim = struct.unpack_from(">IIII", af, aoff)
    if anim != 0:
        sys.exit("%s: WPAttributes carries anim_joints 0x%08X; this packer "
                 "bakes no AnimJoint" % (which, anim))
    if mobj == 0 or matanim == 0:
        sys.exit("%s: WPAttributes has p_mobjsubs 0x%08X and "
                 "p_matanim_joints 0x%08X; this packer wants both"
                 % (which, mobj, matanim))

    ext = A.walk_reloc_extern(rom, ssb_extract, afid, af)
    for name, off in (("data", aoff), ("p_mobjsubs", aoff + 4),
                      ("p_matanim_joints", aoff + 12)):
        if off not in ext:
            sys.exit("%s: WPAttributes.%s is not an extern relocation"
                     % (which, name))
    fid, dl_off = ext[aoff]
    if ext[aoff + 4][0] != fid or ext[aoff + 12][0] != fid:
        sys.exit("%s: the display list, MObjSub table and MatAnimJoint are "
                 "not all in file %d; this packer reads one file"
                 % (which, fid))
    mobj_off = ext[aoff + 4][1]
    mat_off = ext[aoff + 12][1]

    f = A.get_file(rom, fid, ssb_extract)
    reloc = A.walk_reloc(
        f, ssb_extract.read_entry(rom, ssb_extract.RELOC_SEG,
                                  fid)["reloc_intern"])

    zero, one = (0.0, 0.0, 0.0), (1.0, 1.0, 1.0)
    root = A.DObjNode(-1, -1, None, zero, zero, one)
    node = A.DObjNode(0, 0, dl_off, zero, zero, one)
    node.parent = root
    root.children.append(node)

    mobjsubs = A.read_mobjsubs(f, reloc, mobj_off, 1)
    mobjs = [sub for lst in mobjsubs for sub in lst]

    if len(mobjs) != nmobjs:
        sys.exit("%s: MObjSub table reads %d MObjs, wanted %d"
                 % (which, len(mobjs), nmobjs))
    # The runtime moves exactly these four colours off an MObj
    # (src/dc/fighter.c material_of); a flag outside them would be a
    # material the pack silently drops.
    colour = (A.MOBJ_FLAG_PRIMCOLOR | A.MOBJ_FLAG_ENVCOLOR |
              A.MOBJ_FLAG_LIGHT1 | A.MOBJ_FLAG_LIGHT2)
    for sub in mobjs:
        if sub["sprites"] or sub["palettes"]:
            sys.exit("%s: MObjSub@0x%04X carries a picture; this packer's "
                     "MObjs are colours only" % (which, sub["off"]))
        if sub["flags"] & ~colour:
            sys.exit("%s: MObjSub@0x%04X flags 0x%04X ask for more than a "
                     "colour" % (which, sub["off"], sub["flags"]))

    baker = A.MeshBaker(f, pygfxd, mobjsubs, mobj_batches=True,
        rendermode=WP_SEED)
    verts, tris, joints = baker.bake(root)

    if baker.textures or baker.palettes:
        sys.exit("%s: %d textures and %d palettes, wanted none"
                 % (which, len(baker.textures), len(baker.palettes)))
    if len(baker.nodes) != 1 or len(baker.batches) != nmobjs:
        sys.exit("%s: %d batches over %d joints, wanted %d over one"
                 % (which, len(baker.batches), len(baker.nodes), nmobjs))

    joint_of = {}
    for j in joints:
        for b in range(j["batch_first"], j["batch_first"] + j["batch_count"]):
            joint_of[b] = j["node"].index

    batch_mobj = []
    for i, b in enumerate(baker.batches):
        m = b["mobj"]
        batch_mobj.append(-1 if (m is None or m[0] != joint_of[i]) else m[1])
    if sorted(batch_mobj) != list(range(nmobjs)):
        sys.exit("%s: batches draw through MObjs %s; wanted each of %d once"
                 % (which, batch_mobj, nmobjs))

    arr = reloc.get(mat_off)
    if arr is None:
        sys.exit("%s: the MatAnimJoint table names no scripts for joint 0"
                 % which)
    scripts = [reloc.get(arr + 4 * k) for k in range(nmobjs)]
    for k, s in enumerate(scripts):
        if s is not None and matanim_texture_ids(f, reloc, s):
            sys.exit("%s: MObj %d's script sets texture_id_curr, and it has "
                     "no sprite array" % (which, k))
    m_entries, m_words, m_relocs = matanim_block(f, reloc, scripts, which)

    mo = {
        "count": nmobjs,
        "subs": b"".join(A.pack_mobjsub(sub, -1, 1) for sub in mobjs),
        "joint": struct.pack("<2h", 0, nmobjs),
        "batch": struct.pack("<%dh" % len(batch_mobj), *batch_mobj),
        "entry": struct.pack("<%di" % len(m_entries), *m_entries),
        "words": struct.pack("<%dI" % len(m_words), *m_words),
        "reloc": struct.pack("<%dI" % len(m_relocs), *m_relocs),
    }
    secs = K.model_sections(
        [(n.parent.index, n.translate, n.rotate, n.scale)
         for n in baker.nodes],
        verts, tris,
        [(b["tri_first"], b["tri_count"], b["mat"], joint_of[i],
          b.get("bucket", 0)) for i, b in enumerate(baker.batches)],
        baker.textures, baker.palettes)

    world = baker.world_verts()
    lo, hi = baker.bounds()
    cx, cy, cz = ((lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2,
                  (lo[2] + hi[2]) / 2)
    radius = max(math.sqrt((p[0] - cx) ** 2 + (p[1] - cy) ** 2 +
                           (p[2] - cz) ** 2) for p in world)

    blob = build_pack(pack_name, 1, secs,
                      (len(verts), len(tris), len(baker.batches), 0, 0),
                      [], (cx, cy, cz), radius, mobjs=mo)
    print("%s: %s -- attrs file %d @0x%04X, extern -> file %d: DL @0x%04X, "
          "MObjSubs @0x%04X, MatAnimJoint @0x%04X: 1 joint, %d verts, "
          "%d tris, %d batches, %d lit, %d MObjs (flags %s), "
          "%d MatAnimJoint words, radius %.1f"
          % (which, who, afid, aoff, fid, dl_off, mobj_off, mat_off,
             len(verts), len(tris), len(baker.batches),
             sum(1 for b in baker.batches if b["mat"][5]), nmobjs,
             "/".join("0x%04X" % s["flags"] for s in mobjs),
             len(m_words), radius))
    return blob


def pack_efentry(rom, which):
    """An entry vehicle a fighter arrives on.

    The ten vehicles the fighters arrive on have been "a bake job of
    their own" dropped the effect arms of
    ftCommonAppearSetStatus. They are not one job: their makers range
    from Captain Falcon's, which hangs three AnimJoints on the car's
    wheels by walking the tree by hand, to this one, which picks a file
    by fighter kind, makes the effect and sets its position. That is why
    the pipe is first.

    Its descriptor is the rebirth halo's shape -- a DObjDesc tree and one
    AnimJoint, no MObjSub and no MatAnimJoint -- so this is pack_halo
    with three joints instead of three and a different pair of symbols.
    Unlike the halo it does NOT use DL links: reading it that way stops
    on an unterminated array.
    """
    import pygfxd

    who, file_sym, dobj_sym, anim_syms, pack_name, njoints, links = \
        EFENTRY[which]
    if isinstance(anim_syms, str):
        anim_syms = (anim_syms,)
    mobj_sym, mat_sym = (EFENTRY_MOBJ[which][7:] if which in EFENTRY_MOBJ
                         else (None, None))
    offs = reloc_offsets((file_sym, dobj_sym) + tuple(anim_syms))
    fid, dobj_off, anim_offs = offs[0], offs[1], offs[2:]

    f = A.get_file(rom, fid, ssb_extract)
    entry = ssb_extract.read_entry(rom, ssb_extract.RELOC_SEG, fid)
    reloc = A.walk_reloc(f, entry["reloc_intern"])
    # Fox's Arwing keeps its two AnimJoint arrays in a file of its own --
    # the tree is in his Special3 and the arrays are in his Special2 --
    # so the offsets above were resolved against the wrong file and are
    # read again here, along with the file to read them out of. Every
    # other vehicle's animation is in the file its tree is in.
    fa, reloca = f, reloc
    # EF_PROC_RENDERMODE: the mode ef/efdisplay.c's setter GObjs put on
    # the head, for the effects whose own list is silent about it.
    ef_seed = EF_PROC_RENDERMODE.get(which)

    if which in EFENTRY_ANIMFILE:
        aoffs = reloc_offsets((EFENTRY_ANIMFILE[which],) + tuple(anim_syms))
        faid, anim_offs = aoffs[0], aoffs[1:]
        fa = A.get_file(rom, faid, ssb_extract)
        reloca = A.walk_reloc(
            fa, ssb_extract.read_entry(rom, ssb_extract.RELOC_SEG,
                                       faid)["reloc_intern"])

    if which in EFENTRY_FLATDL or which in EFENTRY_LINKDL:
        zero, one = (0.0, 0.0, 0.0), (1.0, 1.0, 1.0)
        root = A.DObjNode(-1, -1, None, zero, zero, one)
        node = A.DObjNode(0, 0, dobj_off, zero, zero, one)
        if which in EFENTRY_LINKDL:
            node.dl_off = None
            node.dl_links = read_dl_links(f, reloc, dobj_off, which)
        if which in EFENTRY_WRAPPED:
            wrap = A.DObjNode(0, 0, None, zero, zero, one)
            wrap.parent = root
            root.children.append(wrap)
            node.index, node.joint_id = 1, 1
            node.parent = wrap
            wrap.children.append(node)
            nodes = [wrap, node]
        else:
            node.parent = root
            root.children.append(node)
            nodes = [node]
    else:
        root, nodes = A.read_dobj_tree(f, reloc, dobj_off, dl_links=links)

    if njoints is not None and len(nodes) != njoints:
        sys.exit("%s: the DObjDesc tree reads %d joints, wanted %d"
                 % (which, len(nodes), njoints))
    mobjsubs = [[] for _ in nodes]

    # A tuple of MatAnimJoints is a set of alternates, end to end in the
    # pack the way deadexplode's are (FPackMObjs.alt_count): Pikachu's
    # Thunder Shock picks one of three with its AnimJoint.
    mat_syms = mat_sym if isinstance(mat_sym, tuple) else (mat_sym,)
    if mobj_sym is not None:
        offs = reloc_offsets((mobj_sym,) + mat_syms)
        mobj_off, mat_offs = offs[0], offs[1:]
        mobjsubs = fill_holes(which, A.read_mobjsubs(f, reloc, mobj_off,
                                                     len(nodes)))

    def read_subs(frame):
        """The MObjs, every sprite array wound to `frame`. Most of these
        vehicles' MObjs carry no sprite array at all -- what their
        material animation moves is a colour or a texture offset -- and
        for them this does nothing and one bake is the whole model.
        Yoshi's egg is the one with a flipbook, two pictures of it."""
        if mobj_sym is None:
            return [[] for _ in nodes]
        sb = fill_holes(which, A.read_mobjsubs(f, reloc, mobj_off,
                                               len(nodes)))
        for lst in sb:
            for sub in lst:
                if sub["sprites"]:
                    sub["texture_id_curr"] = min(frame,
                                                 len(sub["sprites"]) - 1)
        return sb

    # A tile whose clamp mode is set but whose vertices sample past the
    # baked extent has to be re-baked with that tile forced to a clamped
    # extent; bake_retry finds the set by running until nothing overruns.
    # Fox's Arwing is the first pack here that needs it -- its hull
    # samples V 120 of a tile clamped at 32 -- where the fighters' own
    # models have wanted it. The converged set is kept so the
    # per-frame bakes below bake the same tiles the same way.
    baker = A.bake_retry(
        lambda force: A.MeshBaker(f, pygfxd, read_subs(0),
                                  force_extent=force,
                                  rendermode=ef_seed,
                                  mobj_batches=(mobj_sym is not None),
                                  paletted=(which in EFENTRY_PALETTED)),
        lambda b: b.bake(root))
    verts, tris, joints = baker.verts, baker.tris, baker.joints
    forced = baker.force_extent

    if not baker.batches or not tris:
        sys.exit("%s: the tree bakes to no geometry at all" % which)
    # Unlike every weapon this port packs, the pipe is LIT: it is a solid
    # object standing on the stage rather than a billboard, so its batches
    # bake to `shaded 1` (N.L over vertex normals) and want the scene's
    # lights the way a fighter's own model does.
    # How many batches are LIT -- `shaded 1`, N.L over vertex normals,
    # which no weapon model in this port uses and every fighter's does --
    # is REPORTED and not asserted, because two samples suggested a rule
    # that a third refuted. The pipe and the barrel are lit throughout and
    # looked like "a vehicle is a solid object"; Samus's point is mixed,
    # two lit batches and a flat quad for the glow; and Kirby's star is a
    # flat billboard with no lit batch at all. There is no rule here, only
    # a number worth printing.
    nlit = sum(1 for b in baker.batches if b["mat"][5])

    joint_of = {}
    for j in joints:
        for b in range(j["batch_first"], j["batch_first"] + j["batch_count"]):
            joint_of[b] = j["node"].index
    # -- the MObjs, if it has any --------------------------------------
    # One picture each and no sprite array, so every MObj's run is one
    # texture long and the batch's own FPackBatch.tex already names it.
    # What the MObj is FOR is the MatAnimJoint over it.
    mo = None
    batch_mat = [b["mat"] for b in baker.batches]

    if mobj_sym is not None:
        joint_first, mobjs = [], []
        for lst in mobjsubs:
            joint_first.append((len(mobjs), len(lst)))
            mobjs.extend(lst)

        batch_mobj = []
        for i, b in enumerate(baker.batches):
            m = b["mobj"]
            batch_mobj.append(-1 if (m is None or m[0] != joint_of[i])
                              else joint_first[m[0]][0] + m[1])
        # A batch no MObj draws keeps its own picture and is -1 in
        # FPackMObjs.off_batch (the Thunder Shock's first link is a plain
        # textured list beside the MObj'd one).
        alt_scripts = []
        for mat_off in mat_offs:
            scripts = []
            for j in range(len(nodes)):
                arr = reloc.get(mat_off + 4 * j)
                for k in range(len(mobjsubs[j])):
                    scripts.append(None if arr is None
                                   else reloc.get(arr + 4 * k))
            if len(scripts) != len(mobjs):
                sys.exit("%s: %d MatAnimJoint scripts for %d MObjs"
                         % (which, len(scripts), len(mobjs)))
            alt_scripts.append(scripts)
        # How many pictures each MObj shows: one unless its own script
        # steps a sprite array, and then as many as the script reaches.
        # The frames of one MObj are contiguous so the runtime can index
        # them, and the batch's FPackBatch.tex is repointed at its run's
        # frame 0 -- pack_slash's shape, and the ground Thunder Jolt's.
        nframes = []
        for k, sub in enumerate(mobjs):
            ids = set()
            for scripts in alt_scripts:
                if scripts[k] is not None:
                    ids |= matanim_texture_ids(f, reloc, scripts[k])
            trunc = set(int(v) for v in ids)

            if min(trunc, default=0) < 0:
                sys.exit("%s: MObj %d's script sets texture_id_curr to %s"
                         % (which, k, sorted(ids)))
            n = 1 if not trunc else max(trunc) + 1
            if which in EFENTRY_CODEFRAMES and sub["sprites"]:
                n = max(n, len(sub["sprites"]))

            if n > 1 and n > len(sub["sprites"]):
                sys.exit("%s: MObj %d's script reaches sprite %d and its "
                         "array holds %d"
                         % (which, k, n - 1, len(sub["sprites"])))
            nframes.append(n)

        batch_of = []
        for k in range(len(mobjs)):
            bs = [i for i, m in enumerate(batch_mobj) if m == k]
            if len(bs) != 1 and not (which in EFENTRY_MULTI and bs and
                                     nframes[k] == 1):
                sys.exit("%s: MObj %d draws %d batches" % (which, k, len(bs)))
            batch_of.append(bs[0])

        later = [[] for _ in mobjs]
        for frame in range(1, max(nframes)):
            b2 = A.MeshBaker(f, pygfxd, read_subs(frame),
                             force_extent=forced, rendermode=ef_seed,
                             mobj_batches=True,
                             paletted=(which in EFENTRY_PALETTED))
            v2, t2, _j2 = b2.bake(root)

            if (len(v2), len(t2), len(b2.batches)) != \
                    (len(verts), len(tris), len(baker.batches)):
                sys.exit("%s: frame %d bakes to different geometry"
                         % (which, frame))
            for k in range(len(mobjs)):
                if frame < nframes[k]:
                    later[k].append(
                        b2.textures[b2.batches[batch_of[k]]["mat"][0]])

        frames, tex_first, runs = [], [], {}
        for k in range(len(mobjs)):
            own = baker.batches[batch_of[k]]["mat"][0]
            if own < 0:
                sys.exit("%s: MObj %d draws an untextured batch"
                         % (which, k))
            run = [baker.textures[own]] + later[k]
            if which in EFENTRY_HOLES:
                # six MObjs stepping the same five pictures share one run
                # of them rather than carrying six copies (each is 16 KB)
                for r in range(k):
                    if runs[r] == run:
                        tex_first.append(tex_first[r])
                        runs[k] = run
                        break
                else:
                    runs[k] = run
                if tex_first and len(tex_first) > k:
                    continue
            runs[k] = run
            tex_first.append(len(frames))
            frames.extend(run)
        plain = {}
        # a MObj that owns several batches (EFENTRY_MULTI) has no sprite
        # array to replace, so every one of its batches keeps its own
        # picture like a batch no MObj draws does
        multi = [batch_mobj[i] >= 0 and which in EFENTRY_MULTI and
                 batch_of[batch_mobj[i]] != i
                 for i in range(len(baker.batches))]
        for i, b in enumerate(baker.batches):
            if (batch_mobj[i] < 0 or multi[i]) and b["mat"][0] >= 0 and \
                    b["mat"][0] not in plain:
                plain[b["mat"][0]] = len(frames)
                frames.append(baker.textures[b["mat"][0]])
        baker.textures = frames
        batch_mat = [((tex_first[batch_mobj[i]] if batch_mobj[i] >= 0
                       and not multi[i]
                       else plain.get(b["mat"][0], -1)),) +
                     tuple(b["mat"][1:])
                     for i, b in enumerate(baker.batches)]
        m_entries, m_words, m_relocs = [], [], []
        if not alt_scripts:
            # no MatAnimJoint at all: one alternate whose every entry is
            # -1, which gcAddMatAnimJointAll skips per MObj (the
            # spotlight's shape)
            m_entries = [-1] * len(mobjs)
        for scripts in alt_scripts:
            e, w, rl = matanim_block(f, reloc, scripts, which)
            base = len(m_words)
            m_entries += [-1 if x < 0 else x + base for x in e]
            m_words += w
            m_relocs += [x + base for x in rl]

        # Dynamic recolour: a MOBJ_FLAG_PALETTE MObj whose script steps
        # palette_id through more than one value (Run's crash and Clash's
        # wallpaper quadrants) never bakes a second texture
        # the way a sprite array's frames do above -- the pixels never
        # change, only the 16-colour TLUT does, and baking one texture
        # plus one PVR bank per step costs a bank PER STEP
        # (FPACK_PAL_BANKS 64, shared by every pack resident at once);
        # Run's crash alone needs 130 after every dedupe. Instead this
        # names the ONE bank its already-baked (single-frame) texture
        # carries and hands the runtime the raw N-frame colour data to
        # rewrite that one bank's entries from, in place, whenever
        # palette_id changes (src/dc/objmodel.c dc_joint_material) --
        # the real N64's own TLUT swap, at a cost of bytes in main RAM
        # rather than PVR banks.
        dpals, dpal, dpal_claimed = [], [], {}
        for k, sub in enumerate(mobjs):
            first, count, bank = 0, 0, -1
            if sub["flags"] & A.MOBJ_FLAG_PALETTE:
                ids = set()
                for scripts in alt_scripts:
                    if scripts[k] is not None:
                        ids |= A.matanim_frame_ids(
                            f, reloc, scripts[k], A.MATANIM_BIT_PALETTEID)
                trunc = set(int(v) for v in ids)
                if min(trunc, default=0) < 0:
                    sys.exit("%s: MObj %d's script sets palette_id to %s"
                             % (which, k, sorted(ids)))
                n = 1 if not trunc else max(trunc) + 1
                if n > 1:
                    if n > len(sub["palettes"]):
                        sys.exit("%s: MObj %d's script reaches palette %d "
                                 "and its array holds %d"
                                 % (which, k, n - 1, len(sub["palettes"])))
                    # batch_mat, not baker.batches[i]["mat"] -- the plain
                    # /multi pass above already reassigned baker.textures
                    # to the deduped `frames` list, so batch_mat carries
                    # the indices that now actually apply into it.
                    own_tex = batch_mat[batch_of[k]][0]
                    bank = baker.textures[own_tex]["pal"] if own_tex >= 0 \
                        else -1
                    if bank < 0:
                        sys.exit("%s: MObj %d animates a palette but its "
                                 "own texture is not paletted"
                                 % (which, k))
                    if bank in dpal_claimed:
                        # Two different MObjs' representative batches
                        # baked to the SAME bank -- Run's crash's own
                        # case, every joint's frame 0 is byte-identical --
                        # and one live bank cannot hold two joints' CURRENT
                        # frame at once. This joint gets a bank of its
                        # own.
                        bank = len(baker.palettes)
                        baker.palettes.append(baker.palettes[
                            baker.textures[own_tex]["pal"]])
                    # ...and every batch it draws (not only the
                    # representative) looks its colours up there
                    tex = [m[0] for m in batch_mat]
                    A.dpal_bind(baker.textures, tex, batch_mobj, k, bank)
                    batch_mat = [(t,) + tuple(m[1:])
                                 for t, m in zip(tex, batch_mat)]
                    dpal_claimed[bank] = k
                    first, count = len(dpals), n
                    for i in range(n):
                        dpals.append(A.read_palette(f, sub["palettes"][i]))
            dpal.append((first, count, bank))

        mo = {
            "count": len(mobjs),
            "alt": max(1, len(mat_offs)),
            "subs": b"".join(
                A.pack_mobjsub(sub, tex_first[k], nframes[k], dpal=dpal[k])
                for k, sub in enumerate(mobjs)),
            "joint": struct.pack("<%dh" % (2 * len(mobjsubs)),
                                 *[v for pair in joint_first for v in pair]),
            "batch": struct.pack("<%dh" % len(batch_mobj), *batch_mobj),
            "entry": struct.pack("<%di" % len(m_entries), *m_entries),
            "words": struct.pack("<%dI" % len(m_words), *m_words),
            "reloc": struct.pack("<%dI" % len(m_relocs), *m_relocs),
            "dpals": struct.pack("<%dH" % (16 * len(dpals)),
                                 *[c for frame in dpals for c in frame]),
            "dpal_count": len(dpals),
        }
    secs = K.model_sections(
        [(n.parent.index, n.translate, n.rotate, n.scale)
         for n in baker.nodes],
        verts, tris,
        [(b["tri_first"], b["tri_count"], batch_mat[i], joint_of[i],
          b.get("bucket", 0)) for i, b in enumerate(baker.batches)],
        baker.textures, baker.palettes)

    patch = None

    if which in EFENTRY_ANIMPATCH:
        syms = EFENTRY_ANIMPATCH[which]
        keys = sorted(syms)
        vals = reloc_offsets(tuple(syms[k] for k in keys))
        patch = dict(zip(keys, vals))
    blocks = []
    for k, anim_off in enumerate(anim_offs):
        entries, words, relocs = animjoint_block(
            fa, reloca, anim_off, len(nodes), which,
            patch=(patch if k == 0 else None),
            direct=which in EFENTRY_DIRECTANIM)
        blocks.append(("%s%d" % (pack_name[:7], k) if len(anim_offs) > 1
                       else pack_name, entries, words, relocs))

    world = baker.world_verts()
    lo, hi = baker.bounds()
    cx, cy, cz = ((lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2,
                  (lo[2] + hi[2]) / 2)
    radius = max(math.sqrt((p[0] - cx) ** 2 + (p[1] - cy) ** 2 +
                           (p[2] - cz) ** 2) for p in world)

    blob = build_pack(pack_name, len(nodes), secs,
                      (len(verts), len(tris), len(baker.batches),
                       len(baker.textures), len(baker.palettes)),
                      blocks, (cx, cy, cz), radius, mobjs=mo)
    print("%s: %s -- file %d, DObjDesc 0x%04X, AnimJoint%s %s: %d "
          "joints, %d verts, %d tris, %d batches%s, %d textures, "
          "%d animations, %d anim words, radius %.1f"
          % (which, who, fid, dobj_off, "s" if len(anim_offs) > 1 else "",
             " ".join("0x%04X" % a for a in anim_offs),
             len(nodes), len(verts), len(tris), len(baker.batches),
             (" (%d DL links)" % sum(len(n.dl_links) for n in nodes)
              if links else "") + ", %d lit" % nlit,
             len(baker.textures), len(blocks),
             sum(len(b[2]) for b in blocks), radius))
    if mo is not None:
        print("%s: %d MObjs, %d MatAnimJoint words"
              % (which, mo["count"], len(m_words)))
    return blob


def pack_slash(rom):
    import pygfxd

    fid, mobj_off, dobj_off, anim_off, mat_off = reloc_offsets(
        ("llEFCommonEffects1FileID",
         "llEFCommonEffects1DamageSlashMObjSub",
         "llEFCommonEffects1DamageSlashDObjDesc",
         "llEFCommonEffects1DamageSlashAnimJoint",
         "llEFCommonEffects1DamageSlashMatAnimJoint"))

    f = A.get_file(rom, fid, ssb_extract)
    entry = ssb_extract.read_entry(rom, ssb_extract.RELOC_SEG, fid)
    reloc = A.walk_reloc(f, entry["reloc_intern"])

    def read_tree(frame):
        """The tree and its MObjs, with every sprite array wound to
        `frame`. The baker takes the picture from the MObj it finds
        (ssb_assets.py _apply_mobj reads texture_id_curr), so one bake per
        frame is one bake per picture -- and the geometry that comes out
        of it has to be the same every time, which is checked below."""
        r, n = A.read_dobj_tree(f, reloc, dobj_off, dl_links=True)
        subs = A.read_mobjsubs(f, reloc, mobj_off, len(n))
        for lst in subs:
            for sub in lst:
                if sub["sprites"]:
                    sub["texture_id_curr"] = min(frame,
                                                 len(sub["sprites"]) - 1)
        b = A.MeshBaker(f, pygfxd, subs, mobj_batches=True)
        return (b, n, subs) + b.bake(r)

    baker, nodes, mobjsubs, verts, tris, joints = read_tree(0)

    ids = tuple(n.joint_id for n in nodes)
    if ids != SLASH_JOINT_IDS:
        sys.exit("slash: DObjDesc reads joint ids %s, wanted %s"
                 % (ids, SLASH_JOINT_IDS))
    links = tuple(tuple(b for b, _ in n.dl_links) for n in nodes)
    if links != SLASH_DL_LINKS:
        sys.exit("slash: DObjDLLink lists read %s, wanted %s"
                 % (links, SLASH_DL_LINKS))
    for lst in mobjsubs:
        for sub in lst:
            if sub["flags"] & ~SLASH_MOBJ_OK:
                sys.exit("slash: MObjSub@0x%04X has flags 0x%04X, and only "
                         "0x%04X is carried" % (sub["off"], sub["flags"],
                                                SLASH_MOBJ_OK))
            if sub["palettes"]:
                sys.exit("slash: MObjSub@0x%04X selects a palette; only the "
                         "sprite array is carried" % sub["off"])

    joint_of = {}
    for j in joints:
        for b in range(j["batch_first"], j["batch_first"] + j["batch_count"]):
            joint_of[b] = j["node"].index

    # As ssb_emblemexport.py numbers them: joint by joint, in the order
    # gcAddMObjAll hands them to gcAddMObjForDObj.
    joint_first, mobjs = [], []
    for lst in mobjsubs:
        joint_first.append((len(mobjs), len(lst)))
        mobjs.extend(lst)

    batch_mobj = []
    for i, b in enumerate(baker.batches):
        m = b["mobj"]
        batch_mobj.append(-1 if (m is None or m[0] != joint_of[i])
                          else joint_first[m[0]][0] + m[1])
    if any(m < 0 for m in batch_mobj):
        sys.exit("slash: %d batches have no MObj; both of the slash's "
                 "drawn joints branch into one"
                 % sum(1 for m in batch_mobj if m < 0))

    # -- the sprite arrays ---------------------------------------------
    # One texture per MObj per frame, appended to the pack's table in MObj
    # order. Frame 0 is the one the bake above already interned, which is
    # also the one the batch's own FPackBatch.tex names, so the runtime
    # draws the right picture even before a MatAnimJoint has run.
    njoints = len(baker.nodes)
    # `AObjEvent32 ***`, one `AObjEvent32 **` per DObj. gcAddMatAnimJointAll
    # takes one entry per MObj on that DObj and stops -- it is the MObj
    # chain that ends the walk, not a terminator in the array -- so the
    # count comes from the MObjSub table and not from where the NULLs are.
    scripts = []
    for j in range(njoints):
        arr = reloc.get(mat_off + 4 * j)
        for k in range(len(mobjsubs[j])):
            scripts.append(None if arr is None else reloc.get(arr + 4 * k))
    if len(scripts) != len(mobjs):
        sys.exit("slash: %d MatAnimJoint scripts for %d MObjs"
                 % (len(scripts), len(mobjs)))
    nframes = []
    for k, sub in enumerate(mobjs):
        ids = set() if scripts[k] is None else \
            matanim_texture_ids(f, reloc, scripts[k])
        n = 1 if not ids else int(max(ids)) + 1
        if min(ids, default=0.0) < 0 or \
                any(v != int(v) for v in ids):
            sys.exit("slash: MObj %d's script sets texture_id_curr to %s; "
                     "the port indexes the array with it"
                     % (k, sorted(ids)))
        if n > 1 and n > len(sub["sprites"]):
            sys.exit("slash: MObj %d's script reaches sprite %d and its "
                     "array holds %d" % (k, n - 1, len(sub["sprites"])))
        nframes.append(n)

    batch_of = []
    for k in range(len(mobjs)):
        bs = [i for i, m in enumerate(batch_mobj) if m == k]
        texs = set(baker.batches[i]["mat"][0] for i in bs)
        if len(bs) != 1 or len(texs) != 1:
            sys.exit("slash: MObj %d draws %d batches over %d textures; a "
                     "sprite array replaces one picture, not several"
                     % (k, len(bs), len(texs)))
        batch_of.append(bs[0])

    later = [[] for _ in mobjs]

    for frame in range(1, max(nframes)):
        b2, _n2, _s2, v2, t2, _j2 = read_tree(frame)
        if (len(v2), len(t2), len(b2.batches), b2.palettes) != \
                (len(verts), len(tris), len(baker.batches), baker.palettes):
            sys.exit("slash: frame %d bakes to different geometry; only the "
                     "picture is supposed to change" % frame)
        for k in range(len(mobjs)):
            if frame < nframes[k]:
                later[k].append(b2.textures[b2.batches[batch_of[k]]["mat"][0]])

    # The frames of one MObj have to be contiguous for the runtime to
    # index them, so the table is rebuilt as one run per MObj: frame 0,
    # which is the picture the bake above interned for the batch, then
    # the rest. The batch's own FPackBatch.tex is repointed at its run's
    # frame 0 rather than left where the bake put it, so no picture is
    # carried twice -- an ELF-resident romdisk cannot afford a duplicate
    # of every first frame.
    if len(baker.textures) != len(mobjs):
        sys.exit("slash: %d textures for %d MObjs, and the table below "
                 "assumes each one is some MObj's frame 0"
                 % (len(baker.textures), len(mobjs)))
    frames, tex_first, tex_count = [], [], []
    for k in range(len(mobjs)):
        own = baker.batches[batch_of[k]]["mat"][0]
        if own < 0:
            sys.exit("slash: MObj %d draws an untextured batch" % k)
        tex_first.append(len(frames))
        frames.append(baker.textures[own])
        frames.extend(later[k])
        tex_count.append(nframes[k])
    baker.textures = frames

    secs = K.model_sections(
        [(n.parent.index, n.translate, n.rotate, n.scale)
         for n in baker.nodes],
        verts, tris,
        [(b["tri_first"], b["tri_count"],
          (tex_first[batch_mobj[i]],) + tuple(b["mat"][1:]),
          joint_of[i], b.get("bucket", 0))
         for i, b in enumerate(baker.batches)],
        baker.textures, baker.palettes)

    # -- the two animations --------------------------------------------
    entries, words, relocs = animjoint_block(f, reloc, anim_off, njoints,
                                             "slash")
    m_entries, m_words, m_relocs = matanim_block(f, reloc, scripts, "slash")

    world = baker.world_verts()
    lo, hi = baker.bounds()
    cx, cy, cz = ((lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2,
                  (lo[2] + hi[2]) / 2)
    radius = max(math.sqrt((p[0] - cx) ** 2 + (p[1] - cy) ** 2 +
                           (p[2] - cz) ** 2) for p in world)

    mo = {
        "count": len(mobjs),
        "subs": b"".join(
            A.pack_mobjsub(s, tex_first[k], tex_count[k])
            for k, s in enumerate(mobjs)),
        "joint": struct.pack("<%dh" % (2 * len(mobjsubs)),
                             *[v for pair in joint_first for v in pair]),
        "batch": struct.pack("<%dh" % len(batch_mobj), *batch_mobj),
        "entry": struct.pack("<%di" % len(m_entries), *m_entries),
        "words": struct.pack("<%dI" % len(m_words), *m_words),
        "reloc": struct.pack("<%dI" % len(m_relocs), *m_relocs),
    }
    blob = build_pack(SLASH_NAME, njoints, secs,
                      (len(verts), len(tris), len(baker.batches),
                       len(baker.textures), len(baker.palettes)),
                      [(SLASH_NAME, entries, words, relocs)],
                      (cx, cy, cz), radius, mobjs=mo)
    print("slash: file %d, DObjDesc 0x%04X, MObjSub 0x%04X, AnimJoint "
          "0x%04X, MatAnimJoint 0x%04X: %d joints, %d verts, %d tris, "
          "%d batches, %d MObjs over %s frames, %d textures, %d anim "
          "words, %d matanim words"
          % (fid, dobj_off, mobj_off, anim_off, mat_off, njoints,
             len(verts), len(tris), len(baker.batches), len(mobjs),
             "+".join(str(n) for n in nframes), len(baker.textures),
             len(words), len(m_words)))
    return blob


# efmanager.c:3493 efManagerDamageSpawnSparksMakeEffect, the sparks a
# heavy hit throws off. Two EFDescs and one model between them: the
# spawner (efmanager.c:291) has no model at all -- flags
# EFFECT_FLAG_USERDATA, proc_display NULL, all four offsets zero -- and
# every fourth tic of its eight it makes a *flyer*
# (dEFManagerDamageFlySparksEffectDesc, efmanager.c:261) at one of three
# angles. The flyer is what has the model, and its four offsets are the
# CommonSpark blocks -- shared, as the name says, with
# dEFManagerStarRodSparkEffectDesc, which is Kirby's copy of Peach's
# star rod, not a part of this block.
#
# The shape is the orbs' and the picture is the slash's. Flags are
# EFFECT_FLAG_USERDATA | 0x1 and not 0x4, so o_dobjsetup is a display
# list rather than a DObjDesc array (efmanager.c:1995-2000) and the tree
# is built by hand: a stand carrying transform_types1 -- tk1 0x28, the
# camera-facing billboard -- and one child carrying transform_types2 and
# the display list. And the child's one MObj owns a *sprite array* of
# seven 32x32 CI4 frames that its MatAnimJoint steps through, which is
# what the slash does.
#
# The one thing neither of those two had: this MObjSub's flags word is
# *zero*, and gcDrawMObjForDObj (objdisplay.c:1159) reads that as
# `MOBJ_FLAG_TEXTURE | 0x20 | MOBJ_FLAG_ALPHA` rather than as nothing.
# ALPHA is the bit that makes the sprite array live, so a reader that
# took the zero at face value would draw frame 0 for all seven tics.
# ssb_assets.py _apply_mobj has carried that default since it was
# written; what is new is that an effect finally depends on it, and
# tools/check/spark_check.py says so out loud.
SPARK_NAME = "EFSpark"
SPARK_JOINTS = 2
# The same head the orbs' display list goes into, and for the same
# reason: lbCommonDObjScaleXProcDisplay is this EFDesc's render proc too.
SPARK_HEAD = 1
# What an MObjSub of this effect may set. Zero is the whole of it -- see
# above -- and anything else would be a picture the bake below does not
# carry.
SPARK_MOBJ_OK = 0

# --------------------------------------------------------------------
# The metal dust: what a heavy hit strikes off a fighter, and the same
# effect once more.
#
# dEFManagerDamageSpawnMDustEffectDesc (efmanager.c:351) is the sparks'
# spawner again -- eight tics, one flyer every fourth, three angles out
# of a table of its own (efmanager.c:18), no model of any kind -- and
# dEFManagerDamageFlyMDustEffectDesc (efmanager.c:321) is its flyer,
# which even shares efManagerDamageFlySparksProcUpdate. What it does not
# share is the four blocks: llEFCommonEffects1DamageFlyMDust* are its
# own, another seven 32x32 frames and another MatAnimJoint stepping
# through them.
#
# Two things differ from the sparks in the bytes, and both are read
# rather than assumed below:
#
#   the render proc is gcDrawDObjTreeDLLinksForGObj, not
#   lbCommonDObjScaleXProcDisplay, so the pointer flags 0x1 hands to
#   gcAddChildForDObj is a DObjDLLink array {s32 list_id; Gfx *dl}
#   terminated by list_id == 4 (objdisplay.c:2380-2392) rather than the
#   commands themselves. It holds one link, into head 1 -- the same head
#   the sparks reach directly -- and the check below says so out loud.
#
#   transform_types2.tk1 is 0x44 where the sparks' is 0x45. Both are
#   data the port copies into its EFDesc verbatim and neither reaches
#   this file; what reaches this file is the geometry, and that is the
#   same quad.
MDUST_NAME = "EFMDust"
MDUST_JOINTS = 2
MDUST_MOBJ_OK = 0


def read_dl_links(f, reloc, off, what):
    """The DObjDLLink array at `off`, as read_dobj_tree's dl_links leg
    reads one: {s32 list_id; Gfx *dl} until list_id is the DL-head count.
    """
    links = []
    while True:
        list_id = struct.unpack_from(">i", f, off + len(links) * 8)[0]
        if list_id == 4:
            return links
        dl = reloc.get(off + len(links) * 8 + 4)
        if dl is None:
            sys.exit("%s: DObjDLLink[%d] at 0x%04X names head %d and no "
                     "display list" % (what, len(links), off, list_id))
        links.append((list_id, dl))
        if len(links) > 16:
            sys.exit("%s: unterminated DObjDLLink array at 0x%04X"
                     % (what, off))


def pack_spark(rom):
    """The damage sparks, whose o_dobjsetup is the display list itself."""
    return pack_flipbook(
        rom, "spark", SPARK_NAME, SPARK_JOINTS, SPARK_MOBJ_OK,
        ("llEFCommonEffects1FileID",
         "llEFCommonEffects1CommonSparkMObjSub",
         "llEFCommonEffects1CommonSparkDObjDesc",
         "llEFCommonEffects1CommonSparkAnimJoint",
         "llEFCommonEffects1CommonSparkMatAnimJoint"),
        dl_is_links=False)


def pack_mdust(rom):
    """The metal dust, whose o_dobjsetup is a DObjDLLink array."""
    return pack_flipbook(
        rom, "mdust", MDUST_NAME, MDUST_JOINTS, MDUST_MOBJ_OK,
        ("llEFCommonEffects1FileID",
         "llEFCommonEffects1DamageFlyMDustMObjSub",
         "llEFCommonEffects1DamageFlyMDustDObjDesc",
         "llEFCommonEffects1DamageFlyMDustAnimJoint",
         "llEFCommonEffects1DamageFlyMDustMatAnimJoint"),
        dl_is_links=True)


def pack_flipbook(rom, what, pack_name, njoints, mobj_ok, syms,
                  dl_is_links):
    """A stand, a child, one MObj and a sprite array it steps through.

    The sparks and the metal dust are the same effect twice over -- the
    same spawner, the same flyer proc, the same two hand-built DObjs,
    the same one quad, the same seven frames -- out of two different
    sets of blocks, so this is one function with two callers rather than
    one function copied. Everything either of them does differently is a
    parameter, and `what` names the caller in every message so a failure
    still says which effect failed.
    """
    import pygfxd

    fid, mobj_off, dl_off, anim_off, mat_off = reloc_offsets(syms)

    f = A.get_file(rom, fid, ssb_extract)
    entry = ssb_extract.read_entry(rom, ssb_extract.RELOC_SEG, fid)
    reloc = A.walk_reloc(f, entry["reloc_intern"])

    # lbCommonAddMObjForTreeDObjs and lbCommonAddTreeDObjsAnimAll are
    # handed the *child*, so both tables are indexed from it and both
    # have one entry. The pack's are indexed from the stand, which has
    # neither, so each gets a hole at the front -- the same shift the
    # orbs' AnimJoint table takes.
    mobjsubs = [[]] + A.read_mobjsubs(f, reloc, mobj_off, 1)
    subs = mobjsubs[1]
    if len(subs) != 1:
        sys.exit("%s: the MObjSub table holds %d MObjs, wanted one"
                 % (what, len(subs)))
    for sub in subs:
        if sub["flags"] & ~mobj_ok:
            sys.exit("%s: MObjSub@0x%04X has flags 0x%04X, and only "
                     "0x%04X is carried" % (what, sub["off"], sub["flags"],
                                            mobj_ok))
        if sub["palettes"]:
            sys.exit("%s: MObjSub@0x%04X selects a palette; only the "
                     "sprite array is carried" % (what, sub["off"]))

    # Where the child's display list is, and which DL head it goes into.
    if dl_is_links:
        links = read_dl_links(f, reloc, dl_off, what)
        if len(links) != 1:
            sys.exit("%s: the DObjDLLink array at 0x%04X holds %d links, "
                     "wanted one" % (what, dl_off, len(links)))
        head, dl = links[0]
    else:
        head, dl = SPARK_HEAD, dl_off

    arr = reloc.get(mat_off)
    scripts = [None if arr is None else reloc.get(arr)]
    if scripts[0] is None:
        sys.exit("%s: the MatAnimJoint table at 0x%04X has no script for "
                 "the one MObj" % (what, mat_off))
    ids = matanim_texture_ids(f, reloc, scripts[0])
    if min(ids, default=0.0) < 0 or any(v != int(v) for v in ids):
        sys.exit("%s: the script sets texture_id_curr to %s; the port "
                 "indexes the array with it" % (what, sorted(ids)))
    nframes = 1 if not ids else int(max(ids)) + 1
    if nframes > len(subs[0]["sprites"]):
        sys.exit("%s: the script reaches sprite %d and the array holds %d"
                 % (what, nframes - 1, len(subs[0]["sprites"])))

    def read_tree(frame):
        """The hand-built two-DObj tree with the sprite array wound to
        `frame`, baked. As pack_slash's: one bake per picture, and the
        geometry has to come out the same every time."""
        zero, one = (0.0, 0.0, 0.0), (1.0, 1.0, 1.0)
        root = A.DObjNode(-1, -1, None, zero, zero, one)
        stand = A.DObjNode(0, 0, None, zero, zero, one)
        child = A.DObjNode(1, 1, None, zero, zero, one)
        child.dl_links = [(head, dl)]
        stand.parent, root.children = root, [stand]
        child.parent, stand.children = stand, [child]
        for sub in subs:
            sub["texture_id_curr"] = min(frame, len(sub["sprites"]) - 1)
        b = A.MeshBaker(f, pygfxd, mobjsubs, mobj_batches=True)
        return (b,) + b.bake(root)

    baker, verts, tris, joints = read_tree(0)

    if len(baker.batches) != 1:
        sys.exit("%s: the display list bakes to %d batches, wanted one"
                 % (what, len(baker.batches)))
    if (baker.batches[0]["bucket"] & A.FPACK_LIST_MASK) != A.FPACK_LIST_TR:
        sys.exit("%s: the quad landed in PVR list %d, wanted the "
                 "translucent one" % (what, baker.batches[0]["bucket"] &
                                      A.FPACK_LIST_MASK))
    if len(tris) != 2:
        sys.exit("%s: %d triangles, wanted the two of one quad"
                 % (what, len(tris)))
    if len(baker.textures) != 1:
        sys.exit("%s: frame 0 interned %d textures, wanted one"
                 % (what, len(baker.textures)))
    if baker.batches[0]["mobj"] != (1, 0):
        sys.exit("%s: the batch reads MObj %r, wanted the child's first"
                 % (what, baker.batches[0]["mobj"]))

    frames = [baker.textures[baker.batches[0]["mat"][0]]]
    for frame in range(1, nframes):
        b2, v2, t2, _j2 = read_tree(frame)
        if (len(v2), len(t2), len(b2.batches), b2.palettes) != \
                (len(verts), len(tris), 1, baker.palettes):
            sys.exit("%s: frame %d bakes to different geometry; only the "
                     "picture is supposed to change" % (what, frame))
        frames.append(b2.textures[b2.batches[0]["mat"][0]])
    baker.textures = frames

    joint_of = {}
    for j in joints:
        for b in range(j["batch_first"], j["batch_first"] + j["batch_count"]):
            joint_of[b] = j["node"].index
    secs = K.model_sections(
        [(n.parent.index, n.translate, n.rotate, n.scale)
         for n in baker.nodes],
        verts, tris,
        [(b["tri_first"], b["tri_count"], (0,) + tuple(b["mat"][1:]),
          joint_of[i], b.get("bucket", 0))
         for i, b in enumerate(baker.batches)],
        baker.textures, baker.palettes)

    entries, words, relocs = animjoint_block(f, reloc, anim_off, 1, what)
    entries = [-1] + entries
    m_entries, m_words, m_relocs = matanim_block(f, reloc, scripts, what)

    world = baker.world_verts()
    lo, hi = baker.bounds()
    cx, cy, cz = ((lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2,
                  (lo[2] + hi[2]) / 2)
    radius = max(math.sqrt((p[0] - cx) ** 2 + (p[1] - cy) ** 2 +
                           (p[2] - cz) ** 2) for p in world)

    s = subs[0]
    mo = {
        "count": 1,
        "subs": A.pack_mobjsub(s, 0, nframes),
        "joint": struct.pack("<%dh" % (2 * njoints), 0, 0, 0, 1),
        "batch": struct.pack("<h", 0),
        "entry": struct.pack("<%di" % len(m_entries), *m_entries),
        "words": struct.pack("<%dI" % len(m_words), *m_words),
        "reloc": struct.pack("<%dI" % len(m_relocs), *m_relocs),
    }
    blob = build_pack(pack_name, njoints, secs,
                      (len(verts), len(tris), len(baker.batches),
                       len(baker.textures), len(baker.palettes)),
                      [(pack_name, entries, words, relocs)],
                      (cx, cy, cz), radius, mobjs=mo)
    print("%s: file %d, display list 0x%04X into head %d, MObjSub "
          "0x%04X, AnimJoint 0x%04X, MatAnimJoint 0x%04X: %d joints, %d "
          "verts, %d tris, %d batches, 1 MObj over %d frames, %d "
          "textures, %d anim words, %d matanim words"
          % (what, fid, dl, head, mobj_off, anim_off, mat_off,
             njoints, len(verts), len(tris), len(baker.batches),
             nframes, len(baker.textures), len(words), len(m_words)))
    return blob


# --------------------------------------------------------------------
# --------------------------------------------------------------------
# The shield: the bubble a guarding fighter raises, and
# Yoshi's egg.
#
# dEFManagerShieldEffectDesc (efmanager.c:460) has flags 0x4 | USERDATA
# and no file of the effect manager's: its model is the two-entry
# DObjDesc at llFTManagerCommonShieldDObjDesc in the common fighter file,
# a stand and one child carrying a DObjDLLink into head 1. The list is
# one quad over a 32x32 IA16 disc under lerp(ENV, PRIM, TEXEL0) with
# alpha TEXEL0 * PRIM -- the same ENVLERP combiner the dead explosion
# bakes -- and efManagerShieldProcDisplay writes PRIM white and ENV the
# player's colour, both at alpha 0xC0, before drawing it. It sets no
# render mode of its own, only the alpha compare, so the head is what
# buckets it (translucent), which is what ssb_assets.MeshBaker's
# othermode_l_rm flag exists to say.
#
# The root's matrix kind is 0x4F (lb/lbcommon.c func_ovl0_800C994C): the
# whole world matrix of the fighter joint its user_data names -- YRotN,
# whose scale ftCommonGuardUpdateShieldCollision sets from the shield's
# health -- and the child's is 0x2C, the RSP billboard that keeps the
# stack's translation and replaces its rotation with the camera's. The
# bubble is a disc that always faces the camera and grows and shrinks
# with the shield; the sphere the hit pass tests is YRotN's scale and
# needs no model.
#
# dEFManagerYoshiShieldEffectDesc (efmanager.c:490) has USERDATA alone:
# gcAddDObjForGObj with the display list at llYoshiModelShieldDObjDesc,
# one DObj whose triple is {0x50, 0x2C, Null} -- YRotN's world position
# and then the same billboard, scale 1.5 in x and y from the maker. Its
# list sets G_RM_AA_TEX_EDGE without Z compare (the egg is drawn Z-less
# over the fighter) and combines TEXEL0 * (SHADE - ENV), and
# efManagerYoshiShieldProcDisplay darkens ENV as the shield's health
# falls.
SHIELD_NAME = "EFShield"
SHIELD_JOINT_IDS = (0, 1)
SHIELD_DL_LINKS = ((), (1,))
YEGG_NAME = "EFYEgg"
# The head gcDrawDObjDLHead1 queues the egg's list into.
YEGG_HEAD = 1


def pack_quad(rom, what, name, nodes_of, dl_flags, njoints):
    """One quad, no animation, out of `nodes_of(f, reloc)`: (root, nodes).
    `dl_flags` is OR'd into every batch's bucket."""
    import pygfxd

    f, reloc, root, nodes, fid, off = nodes_of(rom)

    baker = A.MeshBaker(f, pygfxd, [[] for _ in range(njoints)])
    verts, tris, joints = baker.bake(root)

    if len(baker.batches) != 1:
        sys.exit("%s: the display list bakes to %d batches, wanted one"
                 % (what, len(baker.batches)))
    if len(tris) != 2:
        sys.exit("%s: %d triangles, wanted the two of one quad"
                 % (what, len(tris)))
    if len(baker.textures) != 1:
        sys.exit("%s: %d textures, wanted one" % (what, len(baker.textures)))
    if len(baker.nodes) != njoints:
        sys.exit("%s: %d joints, wanted %d" % (what, len(baker.nodes),
                                               njoints))

    joint_of = {}
    for j in joints:
        for b in range(j["batch_first"], j["batch_first"] + j["batch_count"]):
            joint_of[b] = j["node"].index
    secs = K.model_sections(
        [(n.parent.index, n.translate, n.rotate, n.scale)
         for n in baker.nodes],
        verts, tris,
        [(b["tri_first"], b["tri_count"], b["mat"], joint_of[i],
          b.get("bucket", 0) | dl_flags) for i, b in enumerate(baker.batches)],
        baker.textures, baker.palettes)

    world = baker.world_verts()
    lo, hi = baker.bounds()
    cx, cy, cz = ((lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2,
                  (lo[2] + hi[2]) / 2)
    radius = max(math.sqrt((p[0] - cx) ** 2 + (p[1] - cy) ** 2 +
                           (p[2] - cz) ** 2) for p in world)

    blob = build_pack(name, njoints, secs,
                      (len(verts), len(tris), len(baker.batches),
                       len(baker.textures), len(baker.palettes)),
                      [], (cx, cy, cz), radius)
    b = baker.batches[0]
    print("%s: file %d, 0x%04X into head %d: %d joints, %d verts, %d tris, "
          "bucket 0x%02X, prim %08X, env %s, shade %s, alpha %s, "
          "%dx%d texture, radius %.1f"
          % (what, fid, off, b["head"], njoints, len(verts), len(tris),
             b.get("bucket", 0) | dl_flags, b["mat"][1] & 0xFFFFFFFF,
             "none" if b["mat"][8] is None else "lerp %06X" % b["mat"][8],
             b["mat"][4], "texel" if b["mat"][6] else "shade" if b["mat"][7]
             else "prim", baker.textures[0]["w"], baker.textures[0]["h"],
             radius))
    return blob


def shield_nodes(rom):
    fid, dobj_off = reloc_offsets(("llFTManagerCommonFileID",
                                   "llFTManagerCommonShieldDObjDesc"))
    f = A.get_file(rom, fid, ssb_extract)
    entry = ssb_extract.read_entry(rom, ssb_extract.RELOC_SEG, fid)
    reloc = A.walk_reloc(f, entry["reloc_intern"])
    root, nodes = A.read_dobj_tree(f, reloc, dobj_off, dl_links=True)
    ids = tuple(n.joint_id for n in nodes)
    if ids != SHIELD_JOINT_IDS:
        sys.exit("shield: DObjDesc reads joint ids %s, wanted %s"
                 % (ids, SHIELD_JOINT_IDS))
    links = tuple(tuple(b for b, _ in n.dl_links) for n in nodes)
    if links != SHIELD_DL_LINKS:
        sys.exit("shield: DObjDLLink buckets read %s, wanted %s"
                 % (links, SHIELD_DL_LINKS))
    return f, reloc, root, nodes, fid, dobj_off


def pack_shield(rom):
    blob = pack_quad(rom, "shield", SHIELD_NAME, shield_nodes, 0, 2)
    return blob


def yegg_nodes(rom):
    fid, dl_off = reloc_offsets(("llYoshiModelFileID",
                                 "llYoshiModelShieldDObjDesc"))
    f = A.get_file(rom, fid, ssb_extract)
    entry = ssb_extract.read_entry(rom, ssb_extract.RELOC_SEG, fid)
    reloc = A.walk_reloc(f, entry["reloc_intern"])
    zero, one = (0.0, 0.0, 0.0), (1.0, 1.0, 1.0)
    root = A.DObjNode(-1, -1, None, zero, zero, one)
    egg = A.DObjNode(0, 0, None, zero, zero, one)
    egg.dl_links = [(YEGG_HEAD, dl_off)]
    egg.parent, root.children = root, [egg]
    return f, reloc, root, [egg], fid, dl_off


def pack_yoshiegg(rom):
    # Z-less: the list's own render mode has no Z_CMP and
    # efDisplayCLDProcDisplay clears G_ZBUFFER ahead of it.
    return pack_quad(rom, "yoshiegg", YEGG_NAME, yegg_nodes, A.FPACK_NOZ, 1)


# The dead explosion: efManagerDeadExplodeMakeEffect
# (efmanager.c:4777), the burst a fighter leaves when he is knocked off
# the top or the side of the screen.
#
# dEFManagerDeadExplodeEffectDesc (efmanager.c:850) is the last EFDesc
# in the port with a model. Its flags are EFFECT_FLAG_SPECIALLINK | 0x4
# and *not* 0x1, so o_dobjsetup is a DObjDesc array read by
# gcSetupCustomDObjs (efmanager.c:2018) -- the halo's branch, not the
# sparks' -- and its render proc is gcDrawDObjTreeDLLinksForGObj, so each
# joint's payload is a DObjDLLink array.
#
# The tree is a stand and *three* children, which is what makes this one
# different: every model this port has baked had one drawn joint or two
# in a chain, and the maker reaches into this one by sibling order --
# `dobj->child` for the first shard and `dobj->child->sib_next->sib_next`
# for the third (efmanager.c:4818-4823). Each shard has a scale of its
# own, one MObj and one display list into head 1.
#
# Two things here have no precedent in the port, and both are why this
# step is more than another pack:
#
#   The colour combiner is lerp(ENV, PRIM, TEXEL0) -- (PRIM - ENV) *
#   TEXEL0 + ENV -- on all three shards. Every batch the port had baked
#   until now reduced to PRIM * TEXEL0, ENV unread; here ENV is half the
#   picture and the maker writes a *player colour* into it
#   (efmanager.c:4825-4835). So MeshBaker reads gsDPSetEnvColor,
#   FPackBatch carries the colour and FPACK_ENVLERP, and src/dc/fighter.c
#   draws it with the PVR's offset colour the way src/dc/lbcommon.c has
#   drawn the same combiner for sprites.
#
#   The descriptor names four MatAnimJoints, one per player, and the
#   game swaps which by writing dEFManagerDeadExplodeMatAnimJoints[player]
#   into the descriptor before it makes the effect (efmanager.c:4808).
#   Nothing else in the game does that to an EFDesc. All four are baked
#   side by side and FPackMObjs.alt_count says how many, which is what
#   src/dc/objmodel.c dc_model_add_mobjs_alt picks between.
#
# One trap, and it is the decomp's own note rather than a discovery
# here: the two reloc symbols are *swapped*. The EFDesc lists
# o_dobjsetup first and o_mobjsub second (ef/eftypes.h:20-21), and what
# it lists first is llEFCommonEffects2DeadExplodeDefaultMObjSub. So the
# DObjDesc array is at the symbol named MObjSub and the MObjSub table at
# the one named DObjDesc, which is exactly what
# ssb-decomp-re/src/relocData/84_EFCommonEffects2.c:1371 and :1549 say
# ("was MIS-TYPED"). The names come from the decomp's own generator
# (tools/export/gen_reloc_header.sh) and are not the port's to correct, so they
# are used as they are and said out loud here.
DEXP_NAME = "EFDExp"
# The tree: a stand and three shards, all three parented on the stand.
DEXP_JOINT_IDS = (0, 1, 1, 1)
# One DObjDLLink each into head 1, the translucent one, and none on the
# stand -- ef/efdisplay.c's XLU display GObj is what set that head's
# render mode.
DEXP_DL_LINKS = ((), (1,), (1,), (1,))
# What an MObjSub of this effect may set. PRIMCOLOR is what the four
# MatAnimJoints drive; ENVCOLOR is not baked here -- the maker sets it
# and the flag at runtime on two of the three -- and anything else would
# be a picture or a tile this bake does not carry.
DEXP_MOBJ_OK = A.MOBJ_FLAG_PRIMCOLOR


def pack_deadexplode(rom):
    import pygfxd

    fid, dobj_off, mobj_off, anim_off = reloc_offsets(
        ("llEFCommonEffects2FileID",
         # swapped, as the block comment above says
         "llEFCommonEffects2DeadExplodeDefaultMObjSub",
         "llEFCommonEffects2DeadExplodeDefaultDObjDesc",
         "llEFCommonEffects2DeadExplodeDefaultAnimJoint"))
    # efmanager.c:883-889 dEFManagerDeadExplodeMatAnimJoints, in the
    # order the maker indexes it: player 0 gets block 1.
    mat_offs = reloc_offsets(
        tuple("llEFCommonEffects2DeadExplode%dMatAnimJoint" % k
              for k in (1, 2, 3, 4)))

    f = A.get_file(rom, fid, ssb_extract)
    entry = ssb_extract.read_entry(rom, ssb_extract.RELOC_SEG, fid)
    reloc = A.walk_reloc(f, entry["reloc_intern"])

    r, nodes = A.read_dobj_tree(f, reloc, dobj_off, dl_links=True)
    ids = tuple(n.joint_id for n in nodes)
    if ids != DEXP_JOINT_IDS:
        sys.exit("deadexplode: DObjDesc reads joint ids %s, wanted %s"
                 % (ids, DEXP_JOINT_IDS))
    links = tuple(tuple(b for b, _ in n.dl_links) for n in nodes)
    if links != DEXP_DL_LINKS:
        sys.exit("deadexplode: DObjDLLink lists read %s, wanted %s"
                 % (links, DEXP_DL_LINKS))

    njoints = len(nodes)
    mobjsubs = A.read_mobjsubs(f, reloc, mobj_off, njoints)
    for lst in mobjsubs:
        for sub in lst:
            if sub["flags"] & ~DEXP_MOBJ_OK:
                sys.exit("deadexplode: MObjSub@0x%04X has flags 0x%04X, "
                         "and only 0x%04X is carried"
                         % (sub["off"], sub["flags"], DEXP_MOBJ_OK))
            if sub["sprites"] or sub["palettes"]:
                sys.exit("deadexplode: MObjSub@0x%04X carries a sprite "
                         "array or a palette; this effect's picture does "
                         "not move" % sub["off"])

    baker = A.MeshBaker(f, pygfxd, mobjsubs, mobj_batches=True)
    verts, tris, joints = baker.bake(r)

    joint_of = {}
    for j in joints:
        for b in range(j["batch_first"], j["batch_first"] + j["batch_count"]):
            joint_of[b] = j["node"].index

    # As ssb_emblemexport.py numbers them: joint by joint, in the order
    # gcAddMObjAll hands them to gcAddMObjForDObj.
    joint_first, mobjs = [], []
    for lst in mobjsubs:
        joint_first.append((len(mobjs), len(lst)))
        mobjs.extend(lst)

    batch_mobj = []
    for i, b in enumerate(baker.batches):
        m = b["mobj"]
        batch_mobj.append(-1 if (m is None or m[0] != joint_of[i])
                          else joint_first[m[0]][0] + m[1])
    if any(m < 0 for m in batch_mobj):
        sys.exit("deadexplode: %d batches have no MObj; each of the three "
                 "shards is one display list under one"
                 % sum(1 for m in batch_mobj if m < 0))
    if len(baker.batches) != len(mobjs):
        sys.exit("deadexplode: %d batches over %d MObjs; the pack's MObj "
                 "table below assumes one each"
                 % (len(baker.batches), len(mobjs)))

    # Every batch must carry the lerp, because the player colour arrives
    # as ENV and a batch without it would draw the same for all four.
    envs = [b["mat"][8] for b in baker.batches]
    if any(e is None for e in envs):
        sys.exit("deadexplode: %d of %d batches do not lerp between ENV "
                 "and PRIM; the player's colour is the ENV"
                 % (sum(1 for e in envs if e is None), len(envs)))

    # Which texture each MObj's batch draws, so the runtime has a
    # tex_first even though nothing steps through an array here.
    tex_first = [baker.batches[batch_mobj.index(k)]["mat"][0]
                 for k in range(len(mobjs))]

    secs = K.model_sections(
        [(n.parent.index, n.translate, n.rotate, n.scale)
         for n in baker.nodes],
        verts, tris,
        [(b["tri_first"], b["tri_count"], b["mat"], joint_of[i],
          b.get("bucket", 0))
         for i, b in enumerate(baker.batches)],
        baker.textures, baker.palettes)

    # -- the animations ------------------------------------------------
    entries, words, relocs = animjoint_block(f, reloc, anim_off, njoints,
                                             "deadexplode")

    # The four MatAnimJoints, end to end in one blob: each block is read
    # on its own -- they are four separate tables with four separate runs
    # of scripts in the file -- and then rebased onto the blob so that
    # FPackMObjs.off_entry is alt_count runs of mobj_count word indices.
    m_entries, m_words, m_relocs = [], [], []
    for a, mat_off in enumerate(mat_offs):
        scripts = []
        for j in range(njoints):
            arr = reloc.get(mat_off + 4 * j)
            for k in range(len(mobjsubs[j])):
                scripts.append(None if arr is None
                               else reloc.get(arr + 4 * k))
        if len(scripts) != len(mobjs):
            sys.exit("deadexplode: MatAnimJoint %d has %d scripts for %d "
                     "MObjs" % (a + 1, len(scripts), len(mobjs)))
        e, w, rl = matanim_block(f, reloc, scripts, "deadexplode")
        base = len(m_words)
        m_entries += [-1 if x < 0 else x + base for x in e]
        m_words += w
        m_relocs += [x + base for x in rl]

    world = baker.world_verts()
    lo, hi = baker.bounds()
    cx, cy, cz = ((lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2,
                  (lo[2] + hi[2]) / 2)
    radius = max(math.sqrt((p[0] - cx) ** 2 + (p[1] - cy) ** 2 +
                           (p[2] - cz) ** 2) for p in world)

    mo = {
        "count": len(mobjs),
        "alt": len(mat_offs),
        "subs": b"".join(
            A.pack_mobjsub(s, tex_first[k], 1)
            for k, s in enumerate(mobjs)),
        "joint": struct.pack("<%dh" % (2 * len(mobjsubs)),
                             *[v for pair in joint_first for v in pair]),
        "batch": struct.pack("<%dh" % len(batch_mobj), *batch_mobj),
        "entry": struct.pack("<%di" % len(m_entries), *m_entries),
        "words": struct.pack("<%dI" % len(m_words), *m_words),
        "reloc": struct.pack("<%dI" % len(m_relocs), *m_relocs),
    }
    blob = build_pack(DEXP_NAME, njoints, secs,
                      (len(verts), len(tris), len(baker.batches),
                       len(baker.textures), len(baker.palettes)),
                      [(DEXP_NAME, entries, words, relocs)],
                      (cx, cy, cz), radius, mobjs=mo)
    print("deadexplode: file %d, DObjDesc 0x%04X, MObjSub 0x%04X, "
          "AnimJoint 0x%04X, MatAnimJoint %s: %d joints, %d verts, %d "
          "tris, %d batches over ENV %s, %d MObjs, %d textures, %d anim "
          "words, %d matanim words in %d alternates"
          % (fid, dobj_off, mobj_off, anim_off,
             "/".join("0x%04X" % m for m in mat_offs), njoints,
             len(verts), len(tris), len(baker.batches),
             "/".join("%06X" % e for e in envs), len(mobjs),
             len(baker.textures), len(words), len(m_words),
             len(mat_offs)))
    return blob


# --------------------------------------------------------------------
# The small shock burst: efManagerShockSmallMakeEffect
# (efmanager.c:2772), one of the two DIVERGES arms `ftParamMakeEffect`
# left open that turned out to want a real, unpacked file rather than
# something the port already had (the other two were closed the same way and
# neither needed a new export).
#
# dEFManagerShockSmallEffectDesc (efmanager.c:111) has flags
# EFFECT_FLAG_USERDATA alone -- no 0x4, no 0x1, the Yoshi shield's own
# shape: one DObj, no stand, and `o_dobjsetup` is the
# display list itself rather than a DObjDesc array. What the shield
# does not have and this does is a MatAnimJoint: `o_anim_joint` is 0 (no
# joint-position track) but `o_matanim_joint` names
# `llEFCommonEffects2ShockSmallMatAnimJoint`, so the one MObj steps
# through a sprite array the way the sparks' and the metal dust's own
# MObjs do (`pack_flipbook` above) -- just without their stand-and-child
# pair, since ShockSmall's flags never build one.
SHOCKSMALL_NAME = "EFShock"
# lbCommonDObjScaleXProcDisplay is this EFDesc's render proc too, the
# same one the sparks' and the shield's own use, so the picture goes
# into the same head as theirs.
SHOCKSMALL_HEAD = 1
SHOCKSMALL_MOBJ_OK = 0


def pack_shocksmall(rom):
    import pygfxd

    fid, mobj_off, dl_off, mat_off = reloc_offsets(
        ("llEFCommonEffects2FileID",
         "llEFCommonEffects2ShockSmallMObjSub",
         "llEFCommonEffects2ShockSmallDObjDesc",
         "llEFCommonEffects2ShockSmallMatAnimJoint"))

    f = A.get_file(rom, fid, ssb_extract)
    entry = ssb_extract.read_entry(rom, ssb_extract.RELOC_SEG, fid)
    reloc = A.walk_reloc(f, entry["reloc_intern"])

    mobjsubs = A.read_mobjsubs(f, reloc, mobj_off, 1)
    subs = mobjsubs[0]
    if len(subs) != 1:
        sys.exit("shock: the MObjSub table holds %d MObjs, wanted one"
                 % len(subs))
    for sub in subs:
        if sub["flags"] & ~SHOCKSMALL_MOBJ_OK:
            sys.exit("shock: MObjSub@0x%04X has flags 0x%04X, and only "
                     "0x%04X is carried" % (sub["off"], sub["flags"],
                                            SHOCKSMALL_MOBJ_OK))
        if sub["palettes"]:
            sys.exit("shock: MObjSub@0x%04X selects a palette; only the "
                     "sprite array is carried" % sub["off"])

    arr = reloc.get(mat_off)
    script = None if arr is None else reloc.get(arr)
    if script is None:
        sys.exit("shock: the MatAnimJoint table at 0x%04X has no script "
                 "for the one MObj" % mat_off)
    ids = matanim_texture_ids(f, reloc, script)
    if min(ids, default=0.0) < 0 or any(v != int(v) for v in ids):
        sys.exit("shock: the script sets texture_id_curr to %s; the "
                 "port indexes the array with it" % sorted(ids))
    nframes = 1 if not ids else int(max(ids)) + 1
    if nframes > len(subs[0]["sprites"]):
        sys.exit("shock: the script reaches sprite %d and the array "
                 "holds %d" % (nframes - 1, len(subs[0]["sprites"])))

    def read_tree(frame):
        zero, one = (0.0, 0.0, 0.0), (1.0, 1.0, 1.0)
        root = A.DObjNode(-1, -1, None, zero, zero, one)
        node = A.DObjNode(0, 0, None, zero, zero, one)
        node.dl_links = [(SHOCKSMALL_HEAD, dl_off)]
        node.parent, root.children = root, [node]
        for sub in subs:
            sub["texture_id_curr"] = min(frame, len(sub["sprites"]) - 1)
        b = A.MeshBaker(f, pygfxd, [subs], mobj_batches=True)
        return (b,) + b.bake(root)

    baker, verts, tris, joints = read_tree(0)

    if len(baker.batches) != 1:
        sys.exit("shock: the display list bakes to %d batches, wanted "
                 "one" % len(baker.batches))
    if (baker.batches[0]["bucket"] & A.FPACK_LIST_MASK) != A.FPACK_LIST_TR:
        sys.exit("shock: the quad landed in PVR list %d, wanted the "
                 "translucent one" % (baker.batches[0]["bucket"] &
                                      A.FPACK_LIST_MASK))
    if len(tris) != 2:
        sys.exit("shock: %d triangles, wanted the two of one quad"
                 % len(tris))
    if len(baker.textures) != 1:
        sys.exit("shock: frame 0 interned %d textures, wanted one"
                 % len(baker.textures))
    if baker.batches[0]["mobj"] != (0, 0):
        sys.exit("shock: the batch reads MObj %r, wanted the one "
                 "joint's first" % (baker.batches[0]["mobj"],))

    frames = [baker.textures[baker.batches[0]["mat"][0]]]
    for frame in range(1, nframes):
        b2, v2, t2, _j2 = read_tree(frame)
        if (len(v2), len(t2), len(b2.batches), b2.palettes) != \
                (len(verts), len(tris), 1, baker.palettes):
            sys.exit("shock: frame %d bakes to different geometry; only "
                     "the picture is supposed to change" % frame)
        frames.append(b2.textures[b2.batches[0]["mat"][0]])
    baker.textures = frames

    joint_of = {}
    for j in joints:
        for b in range(j["batch_first"], j["batch_first"] + j["batch_count"]):
            joint_of[b] = j["node"].index
    secs = K.model_sections(
        [(n.parent.index, n.translate, n.rotate, n.scale)
         for n in baker.nodes],
        verts, tris,
        [(b["tri_first"], b["tri_count"], (0,) + tuple(b["mat"][1:]),
          joint_of[i], b.get("bucket", 0))
         for i, b in enumerate(baker.batches)],
        baker.textures, baker.palettes)

    m_entries, m_words, m_relocs = matanim_block(f, reloc, [script],
                                                 "shock")

    world = baker.world_verts()
    lo, hi = baker.bounds()
    cx, cy, cz = ((lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2,
                  (lo[2] + hi[2]) / 2)
    radius = max(math.sqrt((p[0] - cx) ** 2 + (p[1] - cy) ** 2 +
                           (p[2] - cz) ** 2) for p in world)

    s = subs[0]
    mo = {
        "count": 1,
        "subs": A.pack_mobjsub(s, 0, nframes),
        "joint": struct.pack("<2h", 0, 1),
        "batch": struct.pack("<h", 0),
        "entry": struct.pack("<%di" % len(m_entries), *m_entries),
        "words": struct.pack("<%dI" % len(m_words), *m_words),
        "reloc": struct.pack("<%dI" % len(m_relocs), *m_relocs),
    }
    blob = build_pack(SHOCKSMALL_NAME, 1, secs,
                      (len(verts), len(tris), len(baker.batches),
                       len(baker.textures), len(baker.palettes)),
                      [], (cx, cy, cz), radius, mobjs=mo)
    print("shock: file %d, display list 0x%04X into head %d, MObjSub "
          "0x%04X, MatAnimJoint 0x%04X: %d verts, %d tris, 1 MObj over "
          "%d frames, %d textures, %d matanim words"
          % (fid, dl_off, SHOCKSMALL_HEAD, mobj_off, mat_off, len(verts),
             len(tris), nframes, len(baker.textures), len(m_words)))
    return blob


# --------------------------------------------------------------------
# The fire spark: efManagerFireSparkMakeEffect
# (efmanager.c:3998), the second and last of the two real DIVERGES
# arms. dEFManagerFireSparkEffectDesc (efmanager.c:
# 381) has flags `0x4 | EFFECT_FLAG_USERDATA` -- no 0x1 -- the dead
# explosion's own shape: `o_dobjsetup` is a real DObjDesc array,
# `gcSetupCustomDObjs` walks it straight onto the effect's own DObj,
# no separate stand. What the dead explosion does not have and this
# does is a real per-node position: the tree is a root (no picture)
# and one child at (0, -90, 0) whose own display list -- not a
# DObjDLLink array, this EFDesc's render proc is
# lbCommonDObjScaleXProcDisplay, the sparks' own -- carries one quad
# and a four-frame MatAnimJoint, the same shape the sparks' and the
# slash's own MObjs step through. Read off the real ROM: the child's
# MObjSub flags are 0x1 (MOBJ_FLAG_ALPHA), the bit pack_slash's own
# comment already explains (gcDrawMObjForDObj reads a zero flags word
# as `MOBJ_FLAG_TEXTURE | 0x20 | MOBJ_FLAG_ALPHA`, so ALPHA off is what
# a script-driven sprite array needs).
FIRESPARK_NAME = "EFFireSpk"
FIRESPARK_JOINT_IDS = (0, 1)
FIRESPARK_MOBJ_OK = A.MOBJ_FLAG_ALPHA


def pack_firespark(rom):
    import pygfxd

    fid, dobj_off, mobj_off, anim_off, mat_off = reloc_offsets(
        ("llEFCommonEffects2FileID",
         "llEFCommonEffects2FireSparkDObjDesc",
         "llEFCommonEffects2FireSparkMObjSub",
         "llEFCommonEffects2FireSparkAnimJoint",
         "llEFCommonEffects2FireSparkMatAnimJoint"))

    f = A.get_file(rom, fid, ssb_extract)
    entry = ssb_extract.read_entry(rom, ssb_extract.RELOC_SEG, fid)
    reloc = A.walk_reloc(f, entry["reloc_intern"])

    def read_tree(frame):
        """The real tree -- unlike the sparks' hand-built one, this
        EFDesc's transforms are baked at load, not written by the
        maker, so every node's position has to come off the ROM, not
        out of a synthetic zero one. One bake per frame is one bake
        per picture, and the geometry has to come out the same every
        time, checked below."""
        r, n = A.read_dobj_tree(f, reloc, dobj_off)
        subs = A.read_mobjsubs(f, reloc, mobj_off, len(n))
        for lst in subs:
            for sub in lst:
                if sub["sprites"]:
                    sub["texture_id_curr"] = min(frame,
                                                 len(sub["sprites"]) - 1)
        b = A.MeshBaker(f, pygfxd, subs, mobj_batches=True,
                        rendermode=EF_SEED)
        return (b, n, subs) + b.bake(r)

    baker, nodes, mobjsubs, verts, tris, joints = read_tree(0)

    ids = tuple(n.joint_id for n in nodes)
    if ids != FIRESPARK_JOINT_IDS:
        sys.exit("firespark: DObjDesc reads joint ids %s, wanted %s"
                 % (ids, FIRESPARK_JOINT_IDS))
    for lst in mobjsubs:
        for sub in lst:
            if sub["flags"] & ~FIRESPARK_MOBJ_OK:
                sys.exit("firespark: MObjSub@0x%04X has flags 0x%04X, and "
                         "only 0x%04X is carried" % (sub["off"],
                                                     sub["flags"],
                                                     FIRESPARK_MOBJ_OK))
            if sub["palettes"]:
                sys.exit("firespark: MObjSub@0x%04X selects a palette; "
                         "only the sprite array is carried" % sub["off"])
    if sum(len(lst) for lst in mobjsubs) != 1:
        sys.exit("firespark: %d MObjs over the tree, wanted one"
                 % sum(len(lst) for lst in mobjsubs))
    if len(baker.batches) != 1:
        sys.exit("firespark: the display list bakes to %d batches, "
                 "wanted one" % len(baker.batches))
    if baker.batches[0]["mobj"] != (1, 0):
        sys.exit("firespark: the batch reads MObj %r, wanted the "
                 "child's first" % (baker.batches[0]["mobj"],))
    if len(tris) != 2:
        sys.exit("firespark: %d triangles, wanted the two of one quad"
                 % len(tris))
    if len(baker.textures) != 1:
        sys.exit("firespark: frame 0 interned %d textures, wanted one"
                 % len(baker.textures))

    arr = reloc.get(mat_off + 4)
    script = reloc.get(arr) if arr is not None else None
    if script is None:
        sys.exit("firespark: the MatAnimJoint table at 0x%04X has no "
                 "script for the child's MObj" % mat_off)
    ids2 = matanim_texture_ids(f, reloc, script)
    if min(ids2, default=0.0) < 0 or any(v != int(v) for v in ids2):
        sys.exit("firespark: the script sets texture_id_curr to %s; the "
                 "port indexes the array with it" % sorted(ids2))
    nframes = 1 if not ids2 else int(max(ids2)) + 1
    if nframes > len(mobjsubs[1][0]["sprites"]):
        sys.exit("firespark: the script reaches sprite %d and the array "
                 "holds %d" % (nframes - 1, len(mobjsubs[1][0]["sprites"])))

    frames = [baker.textures[baker.batches[0]["mat"][0]]]
    for frame in range(1, nframes):
        b2, _n2, _s2, v2, t2, _j2 = read_tree(frame)
        if (len(v2), len(t2), len(b2.batches), b2.palettes) != \
                (len(verts), len(tris), 1, baker.palettes):
            sys.exit("firespark: frame %d bakes to different geometry; "
                     "only the picture is supposed to change" % frame)
        frames.append(b2.textures[b2.batches[0]["mat"][0]])
    baker.textures = frames

    joint_of = {}
    for j in joints:
        for b in range(j["batch_first"], j["batch_first"] + j["batch_count"]):
            joint_of[b] = j["node"].index
    secs = K.model_sections(
        [(n.parent.index, n.translate, n.rotate, n.scale)
         for n in baker.nodes],
        verts, tris,
        [(b["tri_first"], b["tri_count"], (0,) + tuple(b["mat"][1:]),
          joint_of[i], b.get("bucket", 0))
         for i, b in enumerate(baker.batches)],
        baker.textures, baker.palettes)

    entries, words, relocs = animjoint_block(f, reloc, anim_off, len(nodes),
                                             "firespark")
    m_entries, m_words, m_relocs = matanim_block(f, reloc, [script],
                                                 "firespark")

    world = baker.world_verts()
    lo, hi = baker.bounds()
    cx, cy, cz = ((lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2,
                  (lo[2] + hi[2]) / 2)
    radius = max(math.sqrt((p[0] - cx) ** 2 + (p[1] - cy) ** 2 +
                           (p[2] - cz) ** 2) for p in world)

    s = mobjsubs[1][0]
    mo = {
        "count": 1,
        "subs": A.pack_mobjsub(s, 0, nframes),
        "joint": struct.pack("<4h", 0, 0, 0, 1),
        "batch": struct.pack("<h", 0),
        "entry": struct.pack("<%di" % len(m_entries), *m_entries),
        "words": struct.pack("<%dI" % len(m_words), *m_words),
        "reloc": struct.pack("<%dI" % len(m_relocs), *m_relocs),
    }
    blob = build_pack(FIRESPARK_NAME, len(nodes), secs,
                      (len(verts), len(tris), len(baker.batches),
                       len(baker.textures), len(baker.palettes)),
                      [(FIRESPARK_NAME, entries, words, relocs)],
                      (cx, cy, cz), radius, mobjs=mo)
    print("firespark: file %d, DObjDesc 0x%04X, MObjSub 0x%04X, AnimJoint "
          "0x%04X, MatAnimJoint 0x%04X: %d joints, %d verts, %d tris, 1 "
          "MObj over %d frames, %d textures, %d anim words, %d matanim "
          "words" % (fid, dobj_off, mobj_off, anim_off, mat_off,
                    len(nodes), len(verts), len(tris), nframes,
                    len(baker.textures), len(words), len(m_words)))
    return blob


def main():
    argv = sys.argv[1:]
    rom_path = M.ROM_DEFAULT
    out = None
    what = "halo"
    if "--rom" in argv:
        rom_path = argv[argv.index("--rom") + 1]
    if "--out" in argv:
        out = argv[argv.index("--out") + 1]
    if "--what" in argv:
        what = argv[argv.index("--what") + 1]
    if not out:
        sys.exit("--out <file.mdl> is required")
    # A weapon model (romdisk/wp*.mdl): wpDisplayMain (wp/wpdisplay.c)
    # runs wpDisplayDrawNormal ahead of every weapon's draw -- WP_ZSEED.
    # ssb_itemmodelexport.py seeds its weapons the same way.
    if os.path.basename(out).startswith("wp"):
        A.MeshBaker.default_seed = dict(WP_ZSEED)
    if what not in ("halo", "quake", "orbs", "slash", "spark", "mdust",
                    "deadexplode", "shield", "yoshiegg", "impactwave",
                    "fireball", "blaster", "shock", "firespark",
                    "arwinglaser") \
            and what not in WPQUAD \
            and what not in EFENTRY \
            and what not in WPTREE and what not in WPLINK \
            and what not in WPFLAT:
        sys.exit("--what halo|quake|orbs|slash|spark|mdust|deadexplode|"
                 "shield|yoshiegg|impactwave|fireball|blaster|shock|"
                 "firespark|arwinglaser|"
                 + "|".join(sorted(list(WPQUAD) + list(WPTREE)
                                   + list(WPLINK) + list(WPFLAT)
                                   + list(EFENTRY))))

    rom = open(rom_path, "rb").read()
    if what in WPQUAD:
        blob = pack_wpquad(rom, what)
    elif what in EFENTRY:
        blob = pack_efentry(rom, what)
    elif what in WPLINK:
        blob = pack_wplink(rom, what)
    elif what in WPFLAT:
        blob = pack_wpflat(rom, what)
    elif what in WPTREE:
        blob = pack_wptree(rom, what)
    else:
        blob = {"halo": pack_halo, "quake": pack_quake,
                "orbs": pack_orbs, "slash": pack_slash,
                "spark": pack_spark, "mdust": pack_mdust,
                "deadexplode": pack_deadexplode,
                "shield": pack_shield, "yoshiegg": pack_yoshiegg,
                "impactwave": pack_impactwave,
                "fireball": pack_fireball,
                "blaster": pack_blaster,
                "shock": pack_shocksmall,
                "firespark": pack_firespark,
                "arwinglaser": pack_arwinglaser}[what](rom)
    d = os.path.dirname(out)
    if d:
        os.makedirs(d, exist_ok=True)
    with open(out, "wb") as fp:
        fp.write(blob)
    print("wrote %s (%d bytes)" % (out, len(blob)))


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""ssb64-dc: the off-screen arrows of if/ifcommon.c, packed.

`ifCommonPlayerArrowsInitInterface` (ifcommon.c:1777) builds three GObjs
on DL link 8 -- one that queues a display list of pure render state, and
two that each instantiate the same model, one at x = -134 and one at
x = +134 rotated 180 degrees. The model is `llIFCommonPlayerArrowsDObjDesc`
in relocData file 166 (IFCommonPlayer): a root and three chevrons, each
one triangle, marching outward at x = 5, 16 and 27. The animation beside
it, `llIFCommonPlayerArrowsAnimJoint`, is three AObjEvent32 scripts that
do nothing but set and clear DOBJ_FLAG_HIDDEN on a loop -- which is how
the chevrons chase each other toward the edge of the screen.

Two things about the model are not in the model, and both come from the
display list the third GObj queues (`dIFCommonPlayerArrowsDisplayList`,
ifcommon.c:231):

  - the colour. The per-chevron lists set the combiner to
    G_CC_PRIMITIVE for both cycles and never set a primitive colour, so
    every pixel of every arrow is whatever gsDPSetPrimColor left: red at
    alpha 0x80. Their own vertices carry alpha 0 and are never read.
  - the render mode, G_RM_AA_XLU_SURF, with the Z-buffer cleared out of
    the geometry mode.

So the pack is baked with both seeded (ssb_assets.MeshBaker's `prim` and
`rendermode`), and the batches come out translucent, Z-less and red --
which is what the RDP draws and what there is no display list here to say
at runtime.

There are no textures and no MObjs in the file's arrow half; the tail of
it -- the magnifying glass's own quad and its 16x16 IA8 frame -- belongs
to the magnifiers and is not read here.

Usage: python3 tools/export/ssb_arrowexport.py --out ifarrows.mdl [--rom <rom.z64>]
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
NAME = "IFArrows"

# ifcommon.c:231 dIFCommonPlayerArrowsDisplayList, the three lines of it
# that reach the geometry: gsDPSetRenderMode(G_RM_AA_XLU_SURF,
# G_RM_AA_XLU_SURF2), gsDPSetPrimColor(0, 0, 0xFF, 0x00, 0x00, 0x80) and
# gsDPSetCombineMode(G_CC_PRIMITIVE, G_CC_PRIMITIVE).
ARROWS_RENDERMODE = A.G_RM_AA_XLU_SURF

# G_CC_PRIMITIVE is (0, 0, 0, PRIMITIVE) in both cycles. The colour
# cycle's four slots each have their own spare encoding for a literal
# zero -- 8..15 in A and B, 16..31 in C, 7 in D -- and the alpha cycle's
# share one, AC_ZERO.
ARROWS_SEED = {
    "combine": (8, 8, 16, A.CC_PRIMITIVE),
    "combine_a": (A.AC_ZERO, A.AC_ZERO, A.AC_ZERO, 3),
    "prim": 0xFF000080,
}

# The file's own two block offsets. Unlike a transition's, this file's
# blocks are not named by offset in relocData -- 166_IFCommonPlayer.c
# names them dIFCommonPlayer_Arrows and dIFCommonPlayer_Arrows_AnimJoint
# and writes the offsets in comments -- so they are quoted here and
# checked against the bytes below.
DOBJDESC_OFF = 0x188
ANIMJOINT_OFF = 0x270

# What the tree must be for those offsets to be the right ones: five
# DObjDesc entries, a root with no display list, three chevrons with one,
# and the terminator (nGCMatrixKindTra == 18 is the array's end marker
# here, as lbCommonSetupTreeDObjs reads it).
DOBJDESC_KINDS = (0, 1, 1, 1, 18)


def find_file():
    """(file id, source text) for relocData's NNN_IFCommonPlayer.c."""
    for fn in sorted(os.listdir(M.RELOC_DIR)):
        m = re.fullmatch(r"(\d+)_IFCommonPlayer\.c", fn)
        if m:
            return int(m.group(1)), M.read_source(
                os.path.join(M.RELOC_DIR, fn))
    sys.exit("no relocData file for IFCommonPlayer")


def check_tree(f, reloc):
    """The five DObjDesc kinds at DOBJDESC_OFF, and the AnimJoint table."""
    kinds = tuple(struct.unpack_from(">I", f, DOBJDESC_OFF + 44 * i)[0]
                  for i in range(len(DOBJDESC_KINDS)))
    if kinds != DOBJDESC_KINDS:
        sys.exit("arrows: DObjDesc at 0x%04X reads %s, wanted %s"
                 % (DOBJDESC_OFF, kinds, DOBJDESC_KINDS))
    # Four table entries: NULL for the root, a script for each chevron.
    if reloc.get(ANIMJOINT_OFF) is not None:
        sys.exit("arrows: the AnimJoint table's root entry is not NULL")
    for k in range(1, 4):
        if reloc.get(ANIMJOINT_OFF + 4 * k) is None:
            sys.exit("arrows: no script for joint %d at 0x%04X"
                     % (k, ANIMJOINT_OFF + 4 * k))


def pack_arrows(rom):
    import pygfxd

    fid, src = find_file()
    if "dIFCommonPlayer_Arrows" not in src:
        sys.exit("arrows: file %d has no dIFCommonPlayer_Arrows" % fid)

    f = A.get_file(rom, fid, ssb_extract)
    entry = ssb_extract.read_entry(rom, ssb_extract.RELOC_SEG, fid)
    reloc = A.walk_reloc(f, entry["reloc_intern"])
    check_tree(f, reloc)

    root, nodes = A.read_dobj_tree(f, reloc, DOBJDESC_OFF)
    baker = A.MeshBaker(f, pygfxd, [[] for _ in nodes],
                        rendermode=[ARROWS_RENDERMODE] * 4,
                        seed=ARROWS_SEED)
    verts, tris, joints = baker.bake(root)
    if baker.textures or baker.palettes:
        sys.exit("arrows: %d textures and %d palettes; the arrows are "
                 "flat primitive colour" % (len(baker.textures),
                                            len(baker.palettes)))
    if len(tris) != 3:
        sys.exit("arrows: %d triangles, wanted the three chevrons"
                 % len(tris))

    joint_of = {}
    for j in joints:
        for b in range(j["batch_first"], j["batch_first"] + j["batch_count"]):
            joint_of[b] = j["node"].index
    # gsSPClearGeometryMode(G_ZBUFFER) in the same display list: the
    # arrows are drawn over whatever is behind them, depth ignored.
    secs = K.model_sections(
        [(n.parent.index, n.translate, n.rotate, n.scale)
         for n in baker.nodes],
        verts, tris,
        [(b["tri_first"], b["tri_count"], b["mat"], joint_of[i],
          b.get("bucket", 0) | A.FPACK_NOZ)
         for i, b in enumerate(baker.batches)],
        baker.textures, baker.palettes)

    # -- the one animation ---------------------------------------------
    # Carried the way tools/export/ssb_transexport.py carries a wipe's: the
    # table and every script after it become the animation's words, and each
    # pointer is rebased onto the block so src/dc/fighter.c's reloc walk
    # makes it an address the way lb/lbreloc.c does for the game.
    njoints = len(baker.nodes)
    table = [reloc.get(ANIMJOINT_OFF + 4 * k) for k in range(njoints)]
    body_off = ANIMJOINT_OFF + 4 * njoints
    if len(f) % 4:
        sys.exit("arrows: file %d is %d bytes, not a whole number of words"
                 % (fid, len(f)))
    words = list(struct.unpack(">%dI" % ((len(f) - ANIMJOINT_OFF) // 4),
                               f[ANIMJOINT_OFF:]))
    entries = [-1] * njoints
    floats = set()
    for k, t in enumerate(table):
        if t is None:
            continue
        if not ANIMJOINT_OFF <= t < len(f):
            sys.exit("arrows: joint %d's script at 0x%04X is outside the "
                     "animation at 0x%04X" % (k, t, ANIMJOINT_OFF))
        # Walk it once before it ships: the game's parser has no default
        # case, so an opcode it does not know is an infinite loop on the
        # target rather than an error (ssb_meshexport.read_anim_ex).
        A.animjoint_walk(f, reloc, t, floats)
        entries[k] = (t - ANIMJOINT_OFF) // 4
    relocs = []
    for loc, target in sorted(reloc.items()):
        if loc < body_off:
            continue
        if target < ANIMJOINT_OFF:
            sys.exit("arrows: pointer at 0x%04X leaves the animation" % loc)
        words[(loc - ANIMJOINT_OFF) // 4] = (target - ANIMJOINT_OFF) // 4
        relocs.append((loc - ANIMJOINT_OFF) // 4)

    world = baker.world_verts()
    lo, hi = baker.bounds()
    cx, cy, cz = ((lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2,
                  (lo[2] + hi[2]) / 2)
    radius = max(math.sqrt((p[0] - cx) ** 2 + (p[1] - cy) ** 2 +
                           (p[2] - cz) ** 2) for p in world)

    words_blob = struct.pack("<%dI" % len(words), *words)
    entries_blob = struct.pack("<%di" % njoints, *entries)
    reloc_blob = struct.pack("<%dI" % len(relocs), *relocs)

    def align(b):
        return b + b"\0" * (-len(b) % 4)

    header_size = 128
    off = header_size
    offsets = {}
    body = b""
    sections = [("joints", secs["joints"]), ("verts", secs["verts"]),
                ("tris", secs["tris"]), ("batches", secs["batches"]),
                ("texs", secs["texs"]), ("pals", secs["pals"]),
                ("texdata", secs["texdata"]),
                ("anims", b"\0" * (ANIM_NAME_LEN + 24)),
                ("entries", entries_blob + reloc_blob),
                ("words", words_blob)]
    for key, sec in sections:
        offsets[key] = off
        off += len(align(sec))
    directory = struct.pack("<%dsIIIIII" % ANIM_NAME_LEN,
                            NAME.encode()[:ANIM_NAME_LEN - 1],
                            offsets["words"], len(words),
                            offsets["entries"], 1,   # FPACK_ANIM_ANIMJOINT
                            offsets["entries"] + len(entries_blob),
                            len(relocs))
    for key, sec in sections:
        body += align(directory if key == "anims" else sec)

    header = struct.pack("<8s8I8I3ff8s8I", MAGIC,
                         njoints, len(verts), len(tris),
                         len(baker.batches), len(baker.textures),
                         len(baker.palettes), 1, len(secs["texdata"]),
                         offsets["joints"], offsets["verts"],
                         offsets["tris"], offsets["batches"],
                         offsets["texs"], offsets["pals"],
                         offsets["texdata"], offsets["anims"],
                         cx, cy, cz, radius, NAME.encode()[:8],
                         0, 0, 0, 0, 0, 0, 0, 0)
    assert len(header) == header_size, len(header)
    print("arrows: file %d, DObjDesc 0x%04X, AnimJoint 0x%04X: %d joints, "
          "%d verts, %d tris, %d batches, %d driven joints, %d anim words, "
          "%d pointers"
          % (fid, DOBJDESC_OFF, ANIMJOINT_OFF, njoints, len(verts),
             len(tris), len(baker.batches),
             sum(1 for e in entries if e >= 0), len(words), len(relocs)))
    return header + body


def main():
    argv = sys.argv[1:]
    rom_path = M.ROM_DEFAULT
    out = None
    if "--rom" in argv:
        rom_path = argv[argv.index("--rom") + 1]
    if "--out" in argv:
        out = argv[argv.index("--out") + 1]
    if not out:
        sys.exit("--out <file.mdl> is required")

    rom = open(rom_path, "rb").read()
    blob = pack_arrows(rom)
    d = os.path.dirname(out)
    if d:
        os.makedirs(d, exist_ok=True)
    with open(out, "wb") as fp:
        fp.write(blob)
    print("wrote %s (%d bytes)" % (out, len(blob)))


if __name__ == "__main__":
    main()

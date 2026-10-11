#!/usr/bin/env python3
"""ssb64-dc: the staff roll's scrolling glyphs and its name plaque.

The original cut both, saying the glyphs needed "a texture-format converter
for the raw glyph blocks" and "a generic reloc pipeline". They needed
neither. Relocdata file 195, measured, shows the
glyphs are 56 `Image` blocks and nothing else -- no DObjDesc, no display
list, no MObjSub, no vertex pool -- because on the N64 the geometry is
built in code: `scStaffrollInitNameAndJobDisplayLists` mallocs four `Vtx`
and twelve `Gfx` per glyph and fills them from the width and height in
`dSCStaffrollNameAndJobSpriteInfo`, which this port already compiles in
verbatim.

That is tools/export/ssb_shadowexport.py's shape -- "the pack carries the
texture and nothing else" -- with one difference that matters: the port
has no runtime quad builder. The shadow's four vertices are built every
frame by C the port ported; nothing in src/dc/ turns a width and a height
into a `DCDisplay` a DObj can carry, and handing `gcAddDObjForGObj` a raw
`Gfx *` instead is the crash src/dc/scstaffroll.h's fifth DIVERGES
records (PC e7000004 on frame 1). So the quads are built HERE, at build
time, off the same two numbers -- one joint per glyph, 4 vertices,
2 triangles, one tile -- and src/dc/scstaffroll.c hands each glyph DObj
that joint's payload (`dc_model_init_payload`) where the game hands it
`sSCStaffrollNameAndJobDisplayLists[i]`.

The vertices are `scStaffrollInitNameAndJobDisplayLists`'s own, term for
term: corners (+-w, +-h, 0) and s10.5 texture coordinates spanning
w x h texels, so the glyph draws at twice its texel size. The tile is
loaded by a `gDPLoadTextureBlock_4b` at `G_IM_FMT_I`, width rounded up
to a multiple of 16, clamped on both axes with mask 5 -- that call's
arguments are fed to ssb_assets.bake_tile unchanged, which is what makes
this a decode of the game's own load rather than a guess at a format.
`alpha_only`: `scStaffrollJob/NameProcDisplay` set the combiner to
PRIMITIVE for colour and TEXEL0 for alpha, so the texel's intensity is
the glyph's *coverage* and its colour comes from the primitive -- which
the two display procs set to two different colours over the one pack
(src/dc/scstaffroll.c, fighter_set_prim_color).

The plaque (`llSCStaffrollDObjDesc`, file offset 0x78C0) is an ordinary
small tree: 2 joints, 3 display lists, 29 commands, 8 vertices, 4
triangles, one IA8 tile -- smaller than the off-screen arrows
tools/export/ssb_arrowexport.py already ships, and baked the same way.

Usage: python3 tools/export/ssb_staffrollexport.py --what glyphs0|glyphs1|plaque
                                            --out scglyphs0.mdl
                                            [--rom <rom.z64>]

The glyphs come in two halves of 28 because fighter.h's own
FIGHTER_MAX_JOINTS is 40; see GLYPH_SPLIT below.
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
import ssb_effectexport as E  # noqa: E402

# scstaffroll.c:2064-2085, the gDPLoadTextureBlock_4b every glyph is
# loaded with: I4, clamped on both axes, mask 5 on both, no LOD shift.
GLYPH_FMT, GLYPH_SIZ = 4, 0     # G_IM_FMT_I, G_IM_SIZ_4b
GLYPH_MASK = 5
GLYPH_CM = A.G_TX_CLAMP

SCENE_SRC = os.path.join(M.RELOC_DIR, "..", "sc", "sccommon", "scstaffroll.c")


def sprite_info():
    """dSCStaffrollNameAndJobSpriteInfo's REGION_US arm, as
    [(width, height, symbol)].

    Read out of the decomp's own source rather than typed here, so a
    table that changes fails loudly. The port compiles the same widths
    and heights in (src/dc/scstaffroll.c), and pack_glyphs checks the
    two agree.
    """
    src = open(SCENE_SRC, encoding="utf-8", errors="replace").read()
    i = src.index("dSCStaffrollNameAndJobSpriteInfo")
    j = src.index("};", i)
    body = src[i:j]
    if "#if defined(REGION_JP)" in body:
        sys.exit("staffroll: the sprite table has a JP arm this reader "
                 "does not split")
    rows = re.findall(r"\{\s*(\d+)\s*,\s*(\d+)\s*,\s*&(ll\w+)\s*\}", body)
    if not rows:
        sys.exit("staffroll: no entries in dSCStaffrollNameAndJobSpriteInfo")
    return [(int(w), int(h), sym) for (w, h, sym) in rows]


def port_table():
    """The same widths and heights as src/dc/scstaffroll.c compiles them,
    for the cross-check. The port's own rows carry a 0 where the decomp
    carries the symbol."""
    path = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                        "..", "..", "src", "dc", "scstaffroll.c")
    src = open(path, encoding="utf-8", errors="replace").read()
    i = src.index("SCStaffrollSprite dSCStaffrollNameAndJobSpriteInfo")
    j = src.index("};", i)
    return [(int(w), int(h)) for (w, h) in
            re.findall(r"\{\s*(\d+)\s*,\s*(\d+)\s*,\s*0\s*\}", src[i:j])]


def glyph_texture(f, off, w, h):
    """One glyph's tile, in the shape MeshBaker._texture interns.

    The key is the game's own load: a block of `roundup16(w)` texels per
    row, `h` rows, clamped at an extent of `roundup16(w)` x `h` -- which
    is what gDPLoadTextureBlock_4b's gsDPSetTile/gsDPSetTileSize pair
    says. bake_tile does the rest: decode, clamp-pad and power-of-two.
    """
    phys_w = ((w + 15) // 16) * 16
    key = A.TexKey(off, 0, phys_w, phys_w, h,
                   GLYPH_MASK, GLYPH_CM, GLYPH_MASK, GLYPH_CM,
                   phys_w, h, -1, (False, False), GLYPH_FMT, GLYPH_SIZ, (),
                   True)     # alpha_only: the colour cycle reads PRIMITIVE
    texels, out_w, out_h, cu, cv = A.bake_tile(f, key)
    texels = [(0xFF, 0xFF, 0xFF, a) for (_r, _g, _b, a) in texels]
    pvrfmt, texels = A.pvr_encoding(texels, False)
    return {
        "key": key, "texels": texels, "w": out_w, "h": out_h,
        "clamp_u": cu, "clamp_v": cv, "pal": -1, "fmt": pvrfmt,
        "img": off, "phys_w": phys_w, "phys_h": h,
    }


def glyph_quad(w, h, tex, joint):
    """scStaffrollInitNameAndJobDisplayLists's four Vtx and two triangles
    for one glyph, in model_sections' plain-data shape.

    j runs 0..3 as the game's loop does: ob = (+-w, +-h, 0) and
    tc = (w or 0, 0 or h) in s10.5, which MeshBaker._finish would have
    divided by the baked tile's size. cn is (0, 0, 0x7F, 0x00) -- a +Z
    normal and alpha 0 -- and neither is read, because the batch's
    combiner takes its colour from PRIMITIVE and its alpha from TEXEL0.
    """
    verts = []
    for j in range(4):
        ox = -float(w) if (j & 2) else float(w)
        oy = float(h) if (j == 0 or j >= 3) else -float(h)
        s = 0.0 if (j & 2) else float(w)
        t = 0.0 if (j == 0 or j >= 3) else float(h)
        verts.append((ox, oy, 0.0, s / tex["w"], t / tex["h"],
                      0.0, 0.0, 1.0, 0, joint))
    # gSP2Triangles(3, 2, 1, 0, 0, 3, 1, 0)
    return verts, [(3, 2, 1), (0, 3, 1)]


# The 56 glyphs do not fit in ONE pack: src/dc/fighter.h's
# FIGHTER_MAX_JOINTS is 40 and fighter_init refuses anything over it
# outright ("blob is not a pack this build handles", with nothing named --
# the same way Saffron City's texture count was refused before that limit
# was raised). Raising it is not this exporter's call to make (fighter.h is
# out of scope here and the constant is sized by the largest FIGHTER), so
# the glyphs ship as two packs of 28 joints, and src/dc/scstaffroll.c's
# own payload table picks pack i // 28, joint i % 28. Nothing about a
# glyph depends on its neighbours -- each is one joint, one quad and one
# tile with no parent -- so the split costs nothing but a second file.
GLYPH_SPLIT = 28


def pack_glyphs(rom, lo=0, hi=None, name="SCGlyphs"):
    table = sprite_info()
    syms = ["llSCStaffrollFileID"] + [sym for (_w, _h, sym) in table]
    vals = E.reloc_offsets(tuple(syms))
    fid, offs = vals[0], vals[1:]

    port = port_table()
    if port != [(w, h) for (w, h, _s) in table]:
        sys.exit("staffroll: src/dc/scstaffroll.c's own "
                 "dSCStaffrollNameAndJobSpriteInfo is %d rows and the "
                 "decomp's is %d, or their sizes differ"
                 % (len(port), len(table)))

    f = A.get_file(rom, fid, ssb_extract)

    if hi is None:
        hi = len(table)
    if hi - lo > 40:
        sys.exit("staffroll: %d glyphs is over fighter.h's own "
                 "FIGHTER_MAX_JOINTS" % (hi - lo))

    nodes, verts, tris, batches, textures = [], [], [], [], []
    for i, ((w, h, _sym), off) in enumerate(zip(table[lo:hi], offs[lo:hi])):
        tex = glyph_texture(f, off, w, h)
        textures.append(tex)
        vs, ts = glyph_quad(w, h, tex, i)
        base = len(verts)
        nodes.append((-1, (0.0, 0.0, 0.0), (0.0, 0.0, 0.0),
                      (1.0, 1.0, 1.0)))
        batches.append((len(tris), len(ts),
                        # (tex, prim, light1, light2, uses_shade, lit,
                        #  alpha_tex, alpha_shade, env): white primitive,
                        # overridden live per credit line
                        # (fighter_set_prim_color), alpha from the texel.
                        (i, 0xFFFFFFFF, 0xFFFFFF00, 0x00000000,
                         False, False, True, False, None),
                        i,
                        # Translucent and Z-less: gDPSetRenderMode
                        # (G_RM_XLU_SURF) and gSPClearGeometryMode
                        # (G_ZBUFFER) in both display procs. No cull flag
                        # -- a glyph is a flat quad the plaque's own
                        # rotation can turn either way.
                        A.FPACK_LIST_TR | A.FPACK_NOZ))
        verts.extend(vs)
        tris.extend((a + base, b + base, c + base) for (a, b, c) in ts)

    secs = K.model_sections(nodes, verts, tris, batches, textures, [])
    radius = max(math.sqrt(v[0] ** 2 + v[1] ** 2) for v in verts)
    blob = E.build_pack(name, len(nodes), secs,
                        (len(verts), len(tris), len(batches),
                         len(textures), 0), [], (0.0, 0.0, 0.0), radius)
    print("glyphs: file %d, glyph %d..%d, %d joints, %d verts, %d tris, "
          "%d tiles, %d bytes of texels"
          % (fid, lo, hi - 1, len(nodes), len(verts), len(tris),
             len(textures), len(secs["texdata"])))
    return blob


def pack_plaque(rom):
    import pygfxd

    fid, dobj_off = E.reloc_offsets(("llSCStaffrollFileID",
                                     "llSCStaffrollDObjDesc"))
    f = A.get_file(rom, fid, ssb_extract)
    entry = ssb_extract.read_entry(rom, ssb_extract.RELOC_SEG, fid)
    reloc = A.walk_reloc(f, entry["reloc_intern"])

    root, nodes = A.read_dobj_tree(f, reloc, dobj_off)
    if len(nodes) != 2:
        sys.exit("plaque: the DObjDesc at 0x%04X reads %d joints, wanted 2"
                 % (dobj_off, len(nodes)))
    baker = A.MeshBaker(f, pygfxd, [[] for _ in nodes])
    verts, tris, joints = baker.bake(root)
    if len(tris) != 4:
        sys.exit("plaque: %d triangles, wanted the two quads" % len(tris))

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

    blob = E.build_pack("SCPlaque", len(baker.nodes), secs,
                        (len(verts), len(tris), len(baker.batches),
                         len(baker.textures), len(baker.palettes)),
                        [], (cx, cy, cz), radius)
    print("plaque: file %d, DObjDesc 0x%04X: %d joints, %d verts, %d tris, "
          "%d batches, %d textures"
          % (fid, dobj_off, len(baker.nodes), len(verts), len(tris),
             len(baker.batches), len(baker.textures)))
    return blob


WHATS = {
    "glyphs0": lambda rom: pack_glyphs(rom, 0, GLYPH_SPLIT, "SCGlyph0"),
    "glyphs1": lambda rom: pack_glyphs(rom, GLYPH_SPLIT, None, "SCGlyph1"),
    "plaque": pack_plaque,
}


def main():
    argv = sys.argv[1:]
    rom_path = M.ROM_DEFAULT
    what = out = None
    if "--rom" in argv:
        rom_path = argv[argv.index("--rom") + 1]
    if "--what" in argv:
        what = argv[argv.index("--what") + 1]
    if "--out" in argv:
        out = argv[argv.index("--out") + 1]
    if not what or not out:
        sys.exit("--what <%s> --out <file.mdl> are required"
                 % "|".join(WHATS))
    if what not in WHATS:
        sys.exit("%s is not one of %s" % (what, ", ".join(WHATS)))

    rom = open(rom_path, "rb").read()
    blob = WHATS[what](rom)
    d = os.path.dirname(out)
    if d:
        os.makedirs(d, exist_ok=True)
    with open(out, "wb") as fp:
        fp.write(blob)
    print("wrote %s (%d bytes)" % (out, len(blob)))


if __name__ == "__main__":
    main()

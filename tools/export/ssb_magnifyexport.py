#!/usr/bin/env python3
"""ssb64-dc: the magnifying glass of if/ifcommon.c, packed.

The glass is three things on the N64 (ifcommon.c:1397-1630), and this
pack carries two of them:

  - the handle: `dIFCommonPlayer_Magnify_DisplayList` at file offset
    0x30 of relocData file 166 (IFCommonPlayer), one flat triangle over
    the three Vtx at offset 0 -- apex (0, 40), base (+-6, 26) -- with
    lighting off and G_CC_PRIMITIVE in both cycles. The game binds it to
    a DObj directly (gcAddDObjForGObj) rather than through a DObjDesc,
    so this builds the one-node tree by hand. Its colour is the player's,
    set by gDPSetPrimColor just before it is drawn
    (ifCommonPlayerMagnifyProcDisplay), and its render mode and Z-less
    geometry mode are set there too; the prim colour is seeded white and
    overridden live on the target (src/dc/fighter.h
    fighter_set_prim_color), the render mode and the Z are baked.

  - the frame: `dIFCommonPlayer_MagnifyFrame_Image` at 0x2C8, a 16x16
    IA8 quadrant the RDP mirrors into a 32x32 disc. The game draws it
    with G_CC_BLENDPEDECALA -- rgb = ENV * (1 - I) + PRIM * I, alpha =
    the texel's -- with ENV the player's colour and PRIM the stage's fog
    colour, both runtime values. The PVR has no combiner that lerps two
    vertex colours by a texel, so the frame ships as two ARGB4444
    textures, (1 - I, A) and (I, A): the first modulated by ENV and
    blended normally, the second modulated by PRIM and added, which is
    the same sum (src/dc/ifcommon.c ifCommonPlayerMagnifyUpdateRender).
    No batch references either; src/dc/fighter.c uploads every texture a
    pack carries whether or not a batch does.

The third thing, the mask the same image writes into the Z-buffer to
round off the fighter inside the glass, is not data: it is the sixteen
clip planes in src/dc/clip.c.

Usage: python3 tools/export/ssb_magnifyexport.py --out ifmagnify.mdl [--rom <rom.z64>]
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
NAME = "IFMagnfy"

# ifcommon.c:1602-1604, the state ifCommonPlayerMagnifyProcDisplay sets
# ahead of the handle: gSPClearGeometryMode(G_ZBUFFER),
# gDPSetRenderMode(G_RM_AA_OPA_SURF, G_RM_AA_OPA_SURF2), and the prim
# colour the seed stands in for.
HANDLE_RENDERMODE = A.G_RM_AA_OPA_SURF
HANDLE_SEED = {"prim": 0xFFFFFFFF}

# The file's own offsets, from 166_IFCommonPlayer.c's comments.
VTX_OFF = 0x0
DL_OFF = 0x30
FRAME_OFF = 0x2C8
FRAME_W = FRAME_H = 16

# The handle's display list, all nine words: sync; lighting and smooth
# shading off; G_CC_PRIMITIVE both cycles; texture off; three vertices
# from segment 3 offset 0; one triangle; sync; lighting and smooth
# shading back on; end.
DL_WORDS = (0xE700000000000000, 0xD9DDFFFF00000000, 0xFCFFFFFFFFFDF8FC,
            0xD700000000000000, 0x0100300600330000, 0x0504020000000000,
            0xE700000000000000, 0xD9FFFFFF00220000, 0xDF00000000000000)


def find_file():
    """(file id, source text) for relocData's NNN_IFCommonPlayer.c."""
    for fn in sorted(os.listdir(M.RELOC_DIR)):
        m = re.fullmatch(r"(\d+)_IFCommonPlayer\.c", fn)
        if m:
            return int(m.group(1)), M.read_source(
                os.path.join(M.RELOC_DIR, fn))
    sys.exit("no relocData file for IFCommonPlayer")


def check_file(f, reloc):
    words = struct.unpack_from(">9Q", f, DL_OFF)
    if words != DL_WORDS:
        sys.exit("magnify: the display list at 0x%04X is not the handle's:\n"
                 "  %s" % (DL_OFF, " ".join("%016x" % w for w in words)))
    # gsSPVertex's address is a relocated pointer to the file's own start
    if reloc.get(DL_OFF + 4 * 8 + 4) not in (VTX_OFF, None):
        sys.exit("magnify: the handle's vertices are not at 0x%04X" % VTX_OFF)
    xyz = [struct.unpack_from(">3h", f, VTX_OFF + 16 * i) for i in range(3)]
    if xyz != [(0, 40, 0), (6, 26, 0), (-6, 26, 0)]:
        sys.exit("magnify: the handle's vertices read %s" % (xyz,))
    if len(f) < FRAME_OFF + FRAME_W * FRAME_H:
        sys.exit("magnify: file is %d bytes, the frame image needs 0x%04X"
                 % (len(f), FRAME_OFF + FRAME_W * FRAME_H))


def frame_textures(f):
    """The two ARGB4444 tiles of the frame image, in the shape
    MeshBaker._texture interns."""
    img = f[FRAME_OFF:FRAME_OFF + FRAME_W * FRAME_H]
    inv, direct = [], []
    for b in img:
        i, a = (b >> 4) * 17, (b & 15) * 17
        inv.append((255 - i, 255 - i, 255 - i, a))
        direct.append((i, i, i, a))
    out = []
    for k, texels in enumerate((inv, direct)):
        out.append({
            "key": ("magnify-frame", k), "w": FRAME_W, "h": FRAME_H,
            "texels": [A._pvr_argb4444(t) for t in texels],
            "clamp_u": False, "clamp_v": False, "pal": -1,
            "fmt": A.PVRTEX_ARGB4444, "img": FRAME_OFF,
            "phys_w": FRAME_W, "phys_h": FRAME_H,
        })
    return out


def pack_magnify(rom):
    import pygfxd

    fid, src = find_file()
    if "dIFCommonPlayer_Magnify_DisplayList" not in src:
        sys.exit("magnify: file %d has no dIFCommonPlayer_Magnify_DisplayList"
                 % fid)

    f = A.get_file(rom, fid, ssb_extract)
    entry = ssb_extract.read_entry(rom, ssb_extract.RELOC_SEG, fid)
    reloc = A.walk_reloc(f, entry["reloc_intern"])
    check_file(f, reloc)

    # The tree the game builds in ifCommonPlayerMagnifyMakeInterface: one
    # DObj carrying the display list, at the identity until the display
    # proc places it each frame.
    root = A.DObjNode(-1, -1, None, (0.0, 0.0, 0.0), (0.0, 0.0, 0.0),
                      (1.0, 1.0, 1.0))
    node = A.DObjNode(0, 0, DL_OFF, (0.0, 0.0, 0.0), (0.0, 0.0, 0.0),
                      (1.0, 1.0, 1.0))
    node.parent = root
    root.children.append(node)

    baker = A.MeshBaker(f, pygfxd, [[]],
                        rendermode=[HANDLE_RENDERMODE] * 4,
                        seed=HANDLE_SEED)
    verts, tris, joints = baker.bake(root)
    if baker.textures or baker.palettes:
        sys.exit("magnify: %d textures and %d palettes; the handle is flat "
                 "primitive colour" % (len(baker.textures),
                                       len(baker.palettes)))
    if len(tris) != 1 or len(baker.batches) != 1:
        sys.exit("magnify: %d triangles in %d batches, wanted one of each"
                 % (len(tris), len(baker.batches)))
    baker.textures.extend(frame_textures(f))

    joint_of = {}
    for j in joints:
        for b in range(j["batch_first"], j["batch_first"] + j["batch_count"]):
            joint_of[b] = j["node"].index
    secs = K.model_sections(
        [(n.parent.index, n.translate, n.rotate, n.scale)
         for n in baker.nodes],
        verts, tris,
        [(b["tri_first"], b["tri_count"], b["mat"], joint_of[i],
          b.get("bucket", 0) | A.FPACK_NOZ)
         for i, b in enumerate(baker.batches)],
        baker.textures, baker.palettes)

    world = baker.world_verts()
    lo, hi = baker.bounds()
    cx, cy, cz = ((lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2,
                  (lo[2] + hi[2]) / 2)
    radius = max(math.sqrt((p[0] - cx) ** 2 + (p[1] - cy) ** 2 +
                           (p[2] - cz) ** 2) for p in world)

    def align(b):
        return b + b"\0" * (-len(b) % 4)

    header_size = 128
    off = header_size
    offsets = {}
    body = b""
    sections = [("joints", secs["joints"]), ("verts", secs["verts"]),
                ("tris", secs["tris"]), ("batches", secs["batches"]),
                ("texs", secs["texs"]), ("pals", secs["pals"]),
                ("texdata", secs["texdata"]), ("anims", b"")]
    for key, sec in sections:
        offsets[key] = off
        off += len(align(sec))
        body += align(sec)

    header = struct.pack("<8s8I8I3ff8s8I", MAGIC,
                         len(baker.nodes), len(verts), len(tris),
                         len(baker.batches), len(baker.textures),
                         len(baker.palettes), 0, len(secs["texdata"]),
                         offsets["joints"], offsets["verts"],
                         offsets["tris"], offsets["batches"],
                         offsets["texs"], offsets["pals"],
                         offsets["texdata"], offsets["anims"],
                         cx, cy, cz, radius, NAME.encode()[:8],
                         0, 0, 0, 0, 0, 0, 0, 0)
    assert len(header) == header_size, len(header)
    print("magnify: file %d, handle DL 0x%04X, frame 0x%04X: %d joint, "
          "%d verts, %d tri, %d batch, %d textures, %d bytes of texels"
          % (fid, DL_OFF, FRAME_OFF, len(baker.nodes), len(verts),
             len(tris), len(baker.batches), len(baker.textures),
             len(secs["texdata"])))
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
    blob = pack_magnify(rom)
    d = os.path.dirname(out)
    if d:
        os.makedirs(d, exist_ok=True)
    with open(out, "wb") as fp:
        fp.write(blob)
    print("wrote %s (%d bytes)" % (out, len(blob)))


if __name__ == "__main__":
    main()

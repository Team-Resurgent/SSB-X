#!/usr/bin/env python3
"""ssb64-dc: the blob under every fighter, one texture.

ft/ftshadow.c:85-91 loads the shadow's picture with a single
gDPLoadTextureBlock_4b: 16x16, G_IM_FMT_I, 4 bits a texel, from
`gEFManagerFiles[1] + llEFCommonEffects2ShadowTextureImage` -- that is
relocData file 84 (EFCommonEffects2), offset 0x3A68, the first 128 bytes
of what the relocData source calls a "multi-frame IA16 shadow pool". The
first frame is not IA16 and the display list is what says so.

Both axes are G_TX_MIRROR with mask 4, so the RDP reads a 32x32 disc out
of the 16x16 quadrant, and gsSPTexture's 0x8000 halves the texture
coordinates, so the vertices' 0..2048 in S10.5 -- 0..64 texels before the
scale, 0..32 after -- is exactly one mirrored period. The port keeps the
same numbers and lets the PVR mirror (PVR_UVFLIP_UV, as the magnifying
glass's frame does): u = tc / 1024, v = tc / 1024, over this 16x16 tile.

The quadrant's intensity climbs from 0 at the outside corner to 9 at what
becomes the disc's centre, so with G_CC_MODULATEIA_PRIM and the game's
black 0xA0 primitive the darkest pixel of a shadow is 9/15 * 160/255.

There is no geometry: ftShadowProcDisplay builds its four, six or eight
vertices every frame out of the floor line under the fighter, so the pack
carries the texture and nothing else. src/dc/fighter.c allows that -- a
pack with no batches compiles no headers.

Usage: python3 tools/export/ssb_shadowexport.py --out ftshadow.mdl [--rom <rom.z64>]
"""
import hashlib
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

# The name the pack carries; FPackHeader.name holds eight characters.
NAME = "FTShadow"

# ft/ftshadow.c:90, the arguments of the one gDPLoadTextureBlock_4b.
IMG_OFF = 0x3A68
IMG_W = IMG_H = 16

# The quadrant's 128 bytes, as a digest so the export is a check as well
# as a copy without the picture itself living here: a different ROM, file
# or offset fails rather than baking some other image as the shadow.
QUADRANT_SHA256 = ("c079f9c394a6df45636f72bd6bf05c38"
                   "7ddc01c37c905394398d30730bf479c0")


def find_file():
    """(file id, source text) for relocData's NNN_EFCommonEffects2.c."""
    for fn in sorted(os.listdir(M.RELOC_DIR)):
        m = re.fullmatch(r"(\d+)_EFCommonEffects2\.c", fn)
        if m:
            return int(m.group(1)), M.read_source(
                os.path.join(M.RELOC_DIR, fn))
    sys.exit("no relocData file for EFCommonEffects2")


def nibbles(f):
    """The 256 intensity nibbles of the 16x16 I4 image at IMG_OFF,
    checked against QUADRANT_SHA256."""
    img = f[IMG_OFF:IMG_OFF + IMG_W * IMG_H // 2]
    if len(img) != IMG_W * IMG_H // 2:
        sys.exit("shadow: file is %d bytes, the image needs 0x%04X"
                 % (len(f), IMG_OFF + IMG_W * IMG_H // 2))
    if hashlib.sha256(img).hexdigest() != QUADRANT_SHA256:
        sys.exit("shadow: the image at 0x%04X is not the blob" % IMG_OFF)
    return [n for b in img for n in (b >> 4, b & 15)]


def shadow_texture(f):
    """The quadrant as one ARGB4444 tile, in the shape MeshBaker._texture
    interns. I4 replicates its intensity into all four channels, which is
    what G_CC_MODULATEIA_PRIM reads: rgb = I * PRIM.rgb, a = I * PRIM.a."""
    texels = [(i * 17, i * 17, i * 17, i * 17) for i in nibbles(f)]
    return {
        "key": ("ftshadow-blob", 0), "w": IMG_W, "h": IMG_H,
        "texels": [A._pvr_argb4444(t) for t in texels],
        "clamp_u": False, "clamp_v": False, "pal": -1,
        "fmt": A.PVRTEX_ARGB4444, "img": IMG_OFF,
        "phys_w": IMG_W, "phys_h": IMG_H,
    }


def pack_shadow(rom):
    fid, src = find_file()
    if "dEFCommonEffects2_Shadow_TextureImage" not in src:
        sys.exit("shadow: file %d has no dEFCommonEffects2_Shadow_TextureImage"
                 % fid)
    # the extern declaration comes first; the definition is the last
    head = src[:src.rindex("u8 dEFCommonEffects2_Shadow_TextureImage")]
    hits = re.findall(r"Raw data from file offset 0x([0-9A-Fa-f]+)", head)
    if not hits or int(hits[-1], 16) != IMG_OFF:
        sys.exit("shadow: the source puts the shadow pool at 0x%s, not "
                 "0x%04X" % (hits[-1] if hits else "?", IMG_OFF))

    f = A.get_file(rom, fid, ssb_extract)
    secs = K.model_sections([], [], [], [], [shadow_texture(f)], [])

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
                         0, 0, 0, 0, 1, 0, 0, len(secs["texdata"]),
                         offsets["joints"], offsets["verts"],
                         offsets["tris"], offsets["batches"],
                         offsets["texs"], offsets["pals"],
                         offsets["texdata"], offsets["anims"],
                         0.0, 0.0, 0.0, 0.0, NAME.encode()[:8],
                         0, 0, 0, 0, 0, 0, 0, 0)
    assert len(header) == header_size, len(header)
    print("shadow: file %d, image 0x%04X: 1 texture %dx%d, %d bytes of "
          "texels" % (fid, IMG_OFF, IMG_W, IMG_H, len(secs["texdata"])))
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
    blob = pack_shadow(rom)
    d = os.path.dirname(out)
    if d:
        os.makedirs(d, exist_ok=True)
    with open(out, "wb") as fp:
        fp.write(blob)
    print("wrote %s (%d bytes)" % (out, len(blob)))


if __name__ == "__main__":
    main()

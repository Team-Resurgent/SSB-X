#!/usr/bin/env python3
"""ssb64-dc: the mesh baker's N64 texel decode against the decomp's own.

The baker used to read CI4 and nothing else, which is what four of the
nine VS stages tripped over. It now reads all nine RDP formats, and it
reads them the way the decomp does: ssb_assets.n64_texel is the per-texel
half of tools/relocSpriteTool.py's n64_to_rgba, copied case for case. This
check is the proof that the copy is faithful, on both synthetic data
covering every format and on every tile the exportable stages actually
bake.

Two deliberate narrowings:

  * The oracle's unswizzle is bypassed. n64_to_rgba begins by undoing the
    TMEM odd-line word swap, because it reads .spritelist blobs, which the
    ROM stores the way they sat in TMEM. A model's texture is stored
    unswapped -- the RDP's load-time swap is undone by the same swap in
    its texel fetch, so the render tile sees the linear image, which is
    the reading the CI4 path has rendered correctly on hardware since H1.
    tools/check/sprite_check.py is where the swap itself is checked, on the data
    that has it. Here the two sides are compared on identical bytes, so
    what is under test is the format decode alone.

  * CI4 is compared as RGBA too, through its TLUT, even though the baker
    keeps CI4 paletted and never calls n64_texel for it. The nibble
    order is the same either way and it costs nothing to cover.

  * An I texel's alpha is compared out, and only its colour checked. The
    two sides disagree there on purpose: the oracle writes PNGs, where an
    intensity image wants an opaque alpha, and the RDP's texture unit
    replicates the intensity into all four channels. The game's own data
    is what settles it rather than either tool -- the character select's
    spotlight (relocData 22) is a 32x32 I8 tile whose display list
    computes alpha as TEXEL0 * PRIMITIVE and colour as SHADE alone, and
    it draws as a round soft glow, which an alpha of 0xFF cannot make.
    Every other format's alpha is still checked, and so is every I
    texel's colour.

Usage: python3 tools/check/texfmt_check.py [--rom <rom.z64>] [--stage Name]
"""
import os
import struct
import sys

sys.path[:0] = [os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), d)
                for d in ("lib", "export", "check")]
import ssb_assets as A           # noqa: E402
import ssb_meshexport as M       # noqa: E402  (ROM_DEFAULT)
import ssb_paths                 # noqa: E402
import ssb_stageexport as S      # noqa: E402

sys.path.insert(0, ssb_paths.DECOMP_TOOLS)
import relocSpriteTool as R      # noqa: E402  (the oracle)

# See the module docstring: both sides read the same linear bytes.
R.unswizzle_n64_texture = lambda data, w, h, fmt, siz: data

# The stages that export today. Dream Land and Saffron City wait on
# gcDrawMObjForDObj's tile-size and texture-scale formulas; Mushroom
# Kingdom is the ninth and exports, but is locked in the save data.
STAGES = ["Castle", "Sector", "Jungle", "Zebes", "Hyrule", "Yoster",
          "Inishie"]

FMT_NAME = {0: "rgba", 2: "ci", 3: "ia", 4: "i"}
SIZ_NAME = {0: "4", 1: "8", 2: "16", 3: "32"}


def name_of(fmt, siz):
    return FMT_NAME[fmt] + SIZ_NAME[siz]


def theirs(data, w, h, fmt, siz, tlut):
    """The decomp's decode of a w*h image, as a flat list of (r,g,b,a)."""
    tlut_bytes = struct.pack(">%dH" % len(tlut), *tlut) if tlut else None
    rows = R.n64_to_rgba(data, w, h, w, fmt, siz, tlut_bytes)
    return [tuple(row[x * 4:x * 4 + 4]) for row in rows for x in range(w)]


def ours(data, w, h, fmt, siz, tlut):
    # `w` as the stride, which is what a row-major image's is. Passing it
    # matters: n64_texel's last argument exists so a format CAN vary its
    # read by row, and this check used to leave it 0, which meant the one
    # arm that did -- an odd-row swap for RGBA32 -- was never compared
    # against the decomp at all. It combed Yoshi's Island's fruit frames
    # for as long as it stood. The arm is gone; passing the stride is what
    # stops another one going in unseen.
    return [A.n64_texel(data, 0, k, fmt, siz, tlut, w) for k in range(w * h)]


def compare(data, w, h, fmt, siz, tlut, where, report):
    a, b = ours(data, w, h, fmt, siz, tlut), theirs(data, w, h, fmt, siz,
                                                    tlut)
    if fmt == 4:
        # I formats: alpha is compared out. See the third narrowing.
        a = [t[:3] for t in a]
        b = [t[:3] for t in b]
    if a == b:
        return 0
    i = next(i for i in range(len(a)) if a[i] != b[i])
    report("MISMATCH %s (%s, %dx%d): texel %d, ours %s theirs %s"
           % (where, name_of(fmt, siz), w, h, i, a[i], b[i]))
    return 1


def palette_equivalence(report):
    """Dropping the palette must not change a single colour.

    A CI4 tile used to ship as indices against a bank of rotate5551(TLUT)
    entries; it now ships as ARGB1555 texels resolved through the same
    TLUT. Both are the RDP's RGBA5551 with the alpha bit moved to the top,
    so they agree on every one of the 65536 words a TLUT entry can hold --
    which is what makes MESH_PALETTED a VRAM decision and not a fidelity
    one (ssb_assets.py).
    """
    bad = 0
    for w in range(0x10000):
        a = A.rotate5551(w)
        b = A._pvr_argb1555(A._rgba5551(w))
        if a != b:
            bad += 1
            if bad == 1:
                report("MISMATCH palette equivalence: 0x%04X -> banked "
                       "0x%04X, direct 0x%04X" % (w, a, b))
    return 0x10000, bad


def synthetic(report):
    """Every format over a byte pattern that exercises all its bit fields."""
    w, h = 16, 8
    tlut = tuple((i * 4099) & 0xFFFF for i in range(256))
    bad = n = 0
    for (fmt, siz) in sorted(A.N64_FORMATS):
        nbytes = w * h * A.BPP[siz] // 8
        data = bytes(((i * 37) ^ (i >> 3) ^ 0x5A) & 0xFF
                     for i in range(nbytes))
        pal = tlut if fmt == 2 else ()
        bad += compare(data, w, h, fmt, siz, pal, "synthetic", report)
        n += 1
    return n, bad


def real(rom, stages, report):
    """Every tile the stage exporter interns, at its physical size."""
    tiles = [0]
    bad = [0]
    per_fmt = {}
    orig = A.MeshBaker._texture

    def hook(self, st, alpha_only=False):
        i = orig(self, st, alpha_only)
        t = self.textures[i]
        k = t["key"]
        bpp = A.BPP[k.siz]
        rows = bytearray()
        for y in range(k.phys_h):
            o = k.img + ((k.origin + y * k.stride) * bpp) // 8
            rows += self.f[o:o + k.phys_w * bpp // 8]
        if len(rows) < k.phys_w * k.phys_h * bpp // 8:
            report("SHORT tile 0x%04X (%s, %dx%d): image runs off the file"
                   % (k.img, name_of(k.fmt, k.siz), k.phys_w, k.phys_h))
            bad[0] += 1
            return i
        tlut = k.tlut
        if k.fmt == 2 and not tlut:
            tlut = A.read_tlut(self.f, st["tlut"], 16)
        tiles[0] += 1
        key = name_of(k.fmt, k.siz)
        per_fmt[key] = per_fmt.get(key, 0) + 1
        bad[0] += compare(bytes(rows), k.phys_w, k.phys_h, k.fmt, k.siz,
                          tlut, "tile 0x%04X" % k.img, report)
        return i

    A.MeshBaker._texture = hook
    try:
        for name in stages:
            # The three lines of ssb_stageexport.main that get to a baker.
            ground = S.read_ground(rom, name)
            fid = next(t[0] for t in ground["layers"] if t)
            f_model, _e, reloc, _s, _i = S.L.file_info(rom, fid)
            S.merge_layers(f_model, reloc, ground["layers"],
                           ground["layer_mask"], ground["mobjsubs"])
    finally:
        A.MeshBaker._texture = orig
    return tiles[0], bad[0], per_fmt


def main():
    argv = sys.argv[1:]
    rom_path = M.ROM_DEFAULT
    stages = STAGES
    if "--rom" in argv:
        rom_path = argv[argv.index("--rom") + 1]
    if "--stage" in argv:
        stages = [argv[argv.index("--stage") + 1]]
    rom = open(rom_path, "rb").read()

    lines = []
    n, bad = palette_equivalence(lines.append)
    print("%d TLUT words: banked and direct CI4 give the same colour" % n)
    m, mbad = synthetic(lines.append)
    bad += mbad
    print("%d formats compared on synthetic data" % m)
    tiles, tbad, per_fmt = real(rom, stages, lines.append)
    print("%d tiles across %d stages compared against the decomp's "
          "relocSpriteTool" % (tiles, len(stages)))
    for key in sorted(per_fmt):
        print("  %-6s %4d tiles" % (key, per_fmt[key]))
    for line in lines:
        print(line)
    if bad or tbad:
        print("FAIL: %d disagree" % (bad + tbad))
        return 1
    print("texfmt_check: the texel decode agrees with the decomp everywhere")
    return 0


if __name__ == "__main__":
    sys.exit(main())

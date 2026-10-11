#!/usr/bin/env python3
"""ssb64-dc: which extracted images are stored row-shuffled, and which are not.

The ROM holds pictures in two storage conventions, and reading one as the
other is silent -- nothing fails, the picture simply draws with a fine comb
over it. Sector Z's background once shipped that way.

  * **A libultra Sprite's bitmap rows are stored shuffled.** On every odd
    row the two 4-byte halves of each 8-byte word are already exchanged,
    which is the swap the RDP applies when it loads a line into TMEM, so
    the sprite library's load puts them back. `Sprite.attr` says so in the
    data: SP_TEXSHUF, bit 9 (PR/sp.h:155). Every one of the US ROM's 1178
    sprites sets it, and so does every stage wallpaper -- attr 0x0240 on
    all nine. RGBA32 is the one size the rule counts differently: its unit
    is 16 bytes with 8-byte halves (ssb_spriteexport.unshuffle_row32).

  * **An image a display list loads is stored linear.** A model's texture,
    a particle bank's: gDPLoadTextureBlock's load-time swap is undone by
    the same swap in the RDP's texel fetch, so what the render tile sees
    is the plain raster that is in ROM. Unshuffling one of these would
    comb it exactly as not unshuffling a sprite does.

So the rule is "is it a Sprite", and the flag in `Sprite.attr` is how the
data itself answers. This check holds both sides of it:

  1. **The flag.** Every stage wallpaper must carry SP_TEXSHUF. The
     exporters unshuffle unconditionally -- the alternative is a silent
     comb -- so one that did not would be wrong, loudly, here rather than
     on the console. tools/check/sprite_check.py does the same for the HUD
     sprites (ssb_spriteexport.check_texshuf) and checks the unshuffle
     itself against the decomp's relocSpriteTool.

  2. **The measurement**, which needs no flag and so covers the model
     tiles, where there is none to read. A picture is vertically
     continuous; row-shuffling it permutes every odd row's texels in
     pairs and wrecks that. Mean |row[y] - row[y+1]| over RGB therefore
     tells the two conventions apart from the pixels alone: every stage
     wallpaper gets smoother when unshuffled (1.13x to 1.60x), and not
     one of the 78 stage model tiles of 32x32 or more does by any margin
     worth the name (worst 1.04x, most of them 2x to 8x the other way).
     MIN_SIDE says what the smaller ones cost and why they are not
     judged.

Usage: python3 tools/check/texshuf_check.py [--rom <rom.z64>] [--stage Name]
"""
import os
import struct
import sys

sys.path[:0] = [os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), d)
                for d in ("lib", "export", "check")]
import ssb_assets as A           # noqa: E402
import ssb_meshexport as M       # noqa: E402
import ssb_spriteexport as SP    # noqa: E402
import ssb_stageexport as S      # noqa: E402

# All nine. texfmt_check still lists seven, on the reading that Dream Land
# and Saffron City "wait on gcDrawMObjForDObj's tile-size formulas" -- which
# are stale: all
# nine export and all nine are in the disc manifest.
MODEL_STAGES = ["Castle", "Sector", "Jungle", "Zebes", "Hyrule", "Yoster",
                "Inishie", "Pupupu", "Yamabuki"]

FMT_NAME = {0: "rgba", 2: "ci", 3: "ia", 4: "i"}
SIZ_NAME = {0: "4", 1: "8", 2: "16", 3: "32"}

# A model tile smaller than this in either axis is decoded and compared but
# not judged. Two reasons, and the second is the one that set the number.
# A four-row strip's "vertical continuity" is three differences, which is
# noise. And the prior itself -- that a picture is smooth down the page --
# is simply false for a HORIZONTALLY STRIPED tile, where every row differs
# from the next by construction and any reshuffle can only look calmer.
# Dream Land has two of those, both gold-on-black stripes at 16x16
# (0x0540 and 0x0490): at MIN_SIDE 16 they score 1.54x and 1.36x and are
# the only two tiles in the game the measurement calls shuffled, and both
# are wrong -- read either way neither is a picture, and the as-is stripes
# are the coherent one. Nothing at 32x32 or larger is a false positive.
MIN_SIDE = 32
# How much smoother the unshuffle has to make a wallpaper before the
# measurement counts as agreeing with the flag. The nine range from 1.13x
# (Inishie, whose background is nearly flat) to 1.60x (Yamabuki).
WALLPAPER_MIN = 1.05
# How much smoother the unshuffle would have to make a model tile before
# this calls it shuffled. The worst of the judged tiles is 1.11x.
MODEL_MAX = 1.25


def name_of(fmt, siz):
    return FMT_NAME[fmt] + SIZ_NAME[siz]


def vrough(px, w, h):
    """Mean vertical neighbour difference over RGB, alpha excluded.

    `px` is a flat list of (r, g, b, a). Alpha is left out on purpose: a
    cutout's alpha is a hard edge everywhere and would drown the colour.
    """
    if h < 2:
        return 0.0
    tot = 0
    for y in range(h - 1):
        a, b = y * w, (y + 1) * w
        for x in range(w):
            p, q = px[a + x], px[b + x]
            tot += abs(p[0] - q[0]) + abs(p[1] - q[1]) + abs(p[2] - q[2])
    return tot / float(w * (h - 1) * 3)


def shuffle_image(data, w, bpp, h):
    """A whole raster put through unshuffle_row, row parity from row 0."""
    stride = (w * bpp + 7) // 8
    return b"".join(SP.unshuffle_row(data[r * stride:(r + 1) * stride], r)
                    for r in range(h))


# ---- 1. the wallpapers: the flag, and the measurement beside it -----------


def wallpaper_sprite(rom, name):
    """The stage's wallpaper Sprite: (attr, imgw, imgh, rows both ways).

    The same walk read_wallpaper makes, kept separate so that this checks
    the ROM rather than the exporter's opinion of it.
    """
    cfg = S.STAGES[name]
    f, _e, _i, sites, ids = S.L.file_info(rom, cfg["map_file"])
    ext = {s: (t, o) for (s, o), t in zip(sites, ids)}
    # Where MPGroundData sits in the map logic file is per-stage since
    # Final Destination, whose file is the header alone -- see
    # ssb_stageexport.ground_off. This walk reads the ROM rather than
    # the exporter's opinion of it, but WHERE to read is still the
    # descriptions file's to say, and that is what ground_off reads.
    site = S.ground_off(cfg["map_file"]) + 0x48
    if site not in ext:
        return None
    sfid, soff = ext[site]
    sf, _, sintern, ssites, sids = S.L.file_info(rom, sfid)
    sext = {s: (t, o) for (s, o), t in zip(ssites, sids)}

    def ptr(o):
        return sext[o][1] if o in sext else sintern[o]

    imgw, imgh = struct.unpack_from(">2h", sf, soff + 4)
    attr = struct.unpack_from(">H", sf, soff + 20)[0]
    nbitmaps, = struct.unpack_from(">h", sf, soff + 40)
    bmheight, = struct.unpack_from(">h", sf, soff + 44)
    bmfmt, bmsiz = sf[soff + 48], sf[soff + 49]
    bitmaps = ptr(soff + 52)

    asis, fixed, nrows = [], [], 0
    for i in range(nbitmaps):
        bo = bitmaps + i * 16
        bwi, = struct.unpack_from(">h", sf, bo + 2)
        buf = ptr(bo + 8)
        stride = bwi * 2
        for r in range(bmheight):
            raw = sf[buf + r * stride:buf + (r + 1) * stride]
            if len(raw) < stride or nrows >= imgh:
                break
            nrows += 1
            # Both sides decoded through the mesh path's own texel reader,
            # so what differs between them is the row order and nothing
            # else -- the same discipline texfmt_check keeps.
            asis.extend(A.n64_texel(raw, 0, k, bmfmt, bmsiz, (), bwi)
                        for k in range(bwi))
            un = SP.unshuffle_row(raw, r)
            fixed.extend(A.n64_texel(un, 0, k, bmfmt, bmsiz, (), bwi)
                         for k in range(bwi))
    return attr, imgw, nrows, asis, fixed


def wallpapers(rom, report):
    n = bad = 0
    for name in sorted(S.STAGES):
        if "map_file" not in S.STAGES[name]:
            continue
        try:
            got = wallpaper_sprite(rom, name)
        except Exception as e:                  # a stage with no sprite file
            report("UNREADABLE wallpaper %s: %s" % (name, e))
            bad += 1
            continue
        if got is None:
            continue
        attr, w, h, a, b = got
        n += 1
        if not (attr & SP.SP_TEXSHUF):
            report("NO SP_TEXSHUF %s: attr 0x%04X -- read_wallpaper "
                   "unshuffles unconditionally and would comb it"
                   % (name, attr))
            bad += 1
        va, vb = vrough(a, w, h), vrough(b, w, h)
        ratio = (va / vb) if vb else 0.0
        if ratio < WALLPAPER_MIN:
            report("NOT SHUFFLED %s: unshuffling leaves it %.2fx as "
                   "smooth (want > %.2f); the rows may be linear after all"
                   % (name, ratio, WALLPAPER_MIN))
            bad += 1
        print("  %-9s attr 0x%04X %3dx%-3d  linear %6.3f  unshuffled "
              "%6.3f  %.2fx smoother" % (name, attr, w, h, va, vb, ratio))
    return n, bad


# ---- 2. the model tiles: the measurement alone ---------------------------


def model_tiles(rom, stages, report):
    """Every tile the stage exporter interns, both ways round."""
    seen = set()
    counts = [0, 0]                  # judged, skipped as too small
    bad = [0]
    worst = [(0.0, "")]
    orig = A.MeshBaker._texture
    cur = [""]

    def hook(self, st, alpha_only=False):
        i = orig(self, st, alpha_only)
        k = self.textures[i]["key"]
        ident = (cur[0], k.img, k.fmt, k.siz, k.phys_w, k.phys_h, k.stride)
        if ident in seen:
            return i
        seen.add(ident)
        w, h, bpp = k.phys_w, k.phys_h, A.BPP[k.siz]
        raw = bytearray()
        for y in range(h):
            o = k.img + ((k.origin + y * k.stride) * bpp) // 8
            raw += self.f[o:o + (w * bpp) // 8]
        if len(raw) < (w * h * bpp) // 8:
            return i                 # texfmt_check reports the short ones
        tlut = k.tlut
        if k.fmt == 2 and not tlut:
            tlut = A.read_tlut(self.f, st["tlut"], 16)
        if w < MIN_SIDE or h < MIN_SIDE:
            counts[1] += 1
            return i
        raw = bytes(raw)
        a = [A.n64_texel(raw, 0, t, k.fmt, k.siz, tlut, w)
             for t in range(w * h)]
        sh = shuffle_image(raw, w, bpp, h)
        b = [A.n64_texel(sh, 0, t, k.fmt, k.siz, tlut, w)
             for t in range(w * h)]
        counts[0] += 1
        va, vb = vrough(a, w, h), vrough(b, w, h)
        ratio = (va / vb) if vb else 0.0
        if ratio > worst[0][0]:
            worst[0] = (ratio, "%s tile 0x%04X (%s, %dx%d)"
                        % (cur[0], k.img, name_of(k.fmt, k.siz), w, h))
        if ratio > MODEL_MAX:
            report("SHUFFLED? %s tile 0x%04X (%s, %dx%d): unshuffling "
                   "leaves it %.2fx as smooth -- a model texture should "
                   "be linear in ROM" % (cur[0], k.img,
                                         name_of(k.fmt, k.siz), w, h, ratio))
            bad[0] += 1
        return i

    A.MeshBaker._texture = hook
    try:
        for name in stages:
            cur[0] = name
            ground = S.read_ground(rom, name)
            fid = next(t[0] for t in ground["layers"] if t)
            f_model, _e, reloc, _s, _i = S.L.file_info(rom, fid)
            # tex_files, the way ssb_stageexport.main bakes it. Without it
            # a stage whose images live in ANOTHER file -- Yoshi's Island,
            # whose four layers are in file 111 and whose every texture is
            # an extern reference into 110 -- is measured on the display
            # lists' own bytes rather than on the tiles it ships.
            S.merge_layers(f_model, reloc, ground["layers"],
                           ground["layer_mask"], ground["mobjsubs"],
                           tex_files=S.texture_files(rom, fid))
    finally:
        A.MeshBaker._texture = orig
    return counts[0], counts[1], bad[0], worst[0]


def main():
    argv = sys.argv[1:]
    rom_path = M.ROM_DEFAULT
    stages = MODEL_STAGES
    if "--rom" in argv:
        rom_path = argv[argv.index("--rom") + 1]
    if "--stage" in argv:
        stages = [argv[argv.index("--stage") + 1]]
    rom = open(rom_path, "rb").read()

    lines = []
    print("stage wallpapers: libultra Sprites, stored shuffled (SP_TEXSHUF)")
    n, bad = wallpapers(rom, lines.append)
    print("%d wallpapers carry SP_TEXSHUF and read smoother unshuffled" % n)

    judged, small, mbad, worst = model_tiles(rom, stages, lines.append)
    bad += mbad
    print("%d stage model tiles of %dx%d or more are linear in ROM "
          "(%d smaller ones decoded, not judged)"
          % (judged, MIN_SIDE, MIN_SIDE, small))
    print("  closest to shuffled: %s at %.2fx" % (worst[1], worst[0]))

    for line in lines:
        print(line)
    if bad:
        print("texshuf_check: FAILED (%d)" % bad)
        return 1
    print("texshuf_check: ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())

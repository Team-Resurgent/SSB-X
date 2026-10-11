#!/usr/bin/env python3
"""ssb64-dc: every texture the disc ships, on contact sheets you can look at.

The exporters are many and each one bakes differently, but they all write
the SAME thing: an FPack (src/dc/fighter.h) whose `FPackTex` table points
into a texdata section of twiddled, PVR-ready texels. So this reads the
PACKS rather than the exporters -- one uniform sweep over the fighters,
the stages, the items, the weapons, the effects, the emblems, the wipes
and everything else in the romdisk -- untwiddles each texture back to a
raster and lays them out in a grid.

That makes "is any texture in the game read wrong?" a thing you answer by
looking, once, instead of by booting to nine stages and twelve fighters.
It is the eyeball half of the texture checks; the machine half is
tools/check/texfmt_check.py (the texel decode against the decomp's),
tools/check/texshuf_check.py (which images are stored row-shuffled),
tools/check/sprite_check.py and tools/check/particletex_check.py.

Two pack kinds are NOT here, because they are not FPacks: the HUD sprite
packs (.spr) and the particle banks (.txp). Both already have a check that
compares them against the decomp's own extraction texel for texel.

Usage:
  python3 tools/check/texsheet.py --all                 every pack in the romdisk
  python3 tools/check/texsheet.py pupupu.stg mario.pack just these
  python3 tools/check/texsheet.py --all --out build/texsheets
"""
import argparse
import os
import struct
import sys
import zlib

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
ROMDISK = os.path.join(REPO, "src", "game", "ssb64", "romdisk")
OUT_DEFAULT = os.path.join(REPO, "build", "texsheets")

# The pack extensions that carry an FPackHeader. Sprite packs and particle
# banks are deliberately absent -- see the module docstring.
PACK_EXT = (".pack", ".mdl", ".stg", ".tra", ".emb", ".mag", ".shd", ".spt")

FPACK_TEX_PAL4, FPACK_TEX_ARGB1555, FPACK_TEX_ARGB4444, FPACK_TEX_EXTERN = \
    0, 1, 2, 3
FMT_NAME = {0: "PAL4", 1: "ARGB1555", 2: "ARGB4444", 3: "EXTERN"}

# Layout of the sheet. A cell holds one texture, magnified by whole texels
# so nothing is resampled, and carries a tick-mark index down its left edge
# (units) and right edge (tens) -- a sheet has no room for a typeface and
# the .txt written beside it carries the full table anyway.
CELL, COLS, PAD = 112, 7, 4

HDR = "<8s16I4f8sI2I2I2II"


sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__))), "lib"))
from ssb_texconv import twidout  # noqa: E402


def untwiddle16(data, w, h):
    """The inverse of ssb_assets.twiddle_16bpp: twiddled bytes -> texels."""
    out = [0] * (w * h)
    minv = min(w, h)
    mask = minv - 1
    for y in range(h):
        for x in range(w):
            idx = twidout(x & mask, y & mask) + \
                (x // minv + y // minv) * minv * minv
            out[y * w + x] = struct.unpack_from("<H", data, idx * 2)[0]
    return out


def untwiddle4(data, w, h):
    """The inverse of ssb_assets.twiddle_4bpp: twiddled bytes -> indices."""
    out = [0] * (w * h)
    minv = min(w, h)
    mask = minv - 1
    for y in range(0, h, 2):
        for x in range(0, w, 2):
            idx = twidout((x & mask) // 2, (y & mask) // 2) + \
                (x // minv + y // minv) * minv * minv // 4
            u16 = struct.unpack_from("<H", data, idx * 2)[0]
            out[y * w + x] = u16 & 15
            out[(y + 1) * w + x] = (u16 >> 4) & 15
            out[y * w + x + 1] = (u16 >> 8) & 15
            out[(y + 1) * w + x + 1] = (u16 >> 12) & 15
    return out


def argb1555(c):
    return ((c >> 10 & 31) * 255 // 31, (c >> 5 & 31) * 255 // 31,
            (c & 31) * 255 // 31, 255 if c & 0x8000 else 0)


def argb4444(c):
    return ((c >> 8 & 15) * 17, (c >> 4 & 15) * 17, (c & 15) * 17,
            (c >> 12 & 15) * 17)


def write_png(path, w, h, rows):
    raw = b"".join(b"\0" + bytes(r) for r in rows)

    def chunk(tag, data):
        c = tag + data
        return struct.pack(">I", len(data)) + c + \
            struct.pack(">I", zlib.crc32(c) & 0xFFFFFFFF)

    with open(path, "wb") as fp:
        fp.write(b"\x89PNG\r\n\x1a\n")
        fp.write(chunk(b"IHDR", struct.pack(">2I5B", w, h, 8, 2, 0, 0, 0)))
        fp.write(chunk(b"IDAT", zlib.compress(raw, 9)))
        fp.write(chunk(b"IEND", b""))


def pack_offsets(blob):
    """Every FPack in a file, by offset.

    A .pack or .mdl IS one and starts at 0. A .stg is an 'SSBSTAG1'
    container whose model pack starts at 0x10 and whose extras section
    carries MORE packs after it -- a stage's map objects and its ground
    actors each ship as their own FPack (Whispy Woods, the Arwings,
    Lakitu). Scanning for the magic finds all of them without this having
    to know any container's own table of contents, which is the point:
    the sweep should not need updating when a new section is added.
    """
    out = []
    at = blob.find(b"SSBPACK")
    while at >= 0:
        if at + struct.calcsize(HDR) <= len(blob):
            out.append(at)
        at = blob.find(b"SSBPACK", at + 8)
    return out


def read_pack(blob, base=0):
    """(header dict, [texture dicts]) for the FPack at `base`, or None if
    it is not one. A texture's `px` is its raster as (r, g, b, a) tuples.
    Every offset in the header is relative to the pack, not the file."""
    if len(blob) < base + struct.calcsize(HDR) or \
            not blob[base:base + 7] == b"SSBPACK":
        return None
    blob = blob[base:]
    f = struct.unpack_from(HDR, blob, 0)
    hd = {
        "magic": f[0].decode("ascii", "replace").rstrip("\0"),
        "tex_count": f[5], "pal_count": f[6],
        "off_texs": f[13], "off_pals": f[14], "off_texdata": f[15],
        "name": f[21].decode("ascii", "replace").rstrip("\0"),
    }
    pals = [struct.unpack_from("<16H", blob, hd["off_pals"] + i * 32)
            for i in range(hd["pal_count"])]
    texs = []
    for i in range(hd["tex_count"]):
        off, size, w, h, pal, clamp, fmt, _pad = struct.unpack_from(
            "<2I2H4B", blob, hd["off_texs"] + i * 16)
        t = {"i": i, "w": w, "h": h, "pal": pal, "clamp": clamp,
             "fmt": fmt, "size": size, "px": None}
        if fmt != FPACK_TEX_EXTERN and w and h:
            data = blob[hd["off_texdata"] + off:
                        hd["off_texdata"] + off + size]
            try:
                if fmt == FPACK_TEX_PAL4:
                    bank = pals[pal] if pal < len(pals) else [0] * 16
                    t["px"] = [argb1555(bank[v])
                               for v in untwiddle4(data, w, h)]
                elif fmt == FPACK_TEX_ARGB1555:
                    t["px"] = [argb1555(c) for c in untwiddle16(data, w, h)]
                elif fmt == FPACK_TEX_ARGB4444:
                    t["px"] = [argb4444(c) for c in untwiddle16(data, w, h)]
            except (struct.error, IndexError):
                t["px"] = None          # a short or malformed texdata entry
        texs.append(t)
    return hd, texs


def noise(t):
    """How far a texture is from being a picture, 0 (flat) to 1 (static).

    A real texture is locally smooth: neighbouring texels mostly agree.
    One read out of the wrong bytes is high-entropy in both axes. So this
    is the mean absolute neighbour difference over RGB, normalised, with
    fully transparent texels left out so a cutout is not punished for its
    holes. It ranks; it does not judge -- some real textures (noise,
    dither, a 16x16 of unrelated icons) score high, which is why the
    answer is a sheet to look at and not a pass/fail.
    """
    w, h, px = t["w"], t["h"], t["px"]
    tot = n = 0
    for y in range(h):
        for x in range(w):
            p = px[y * w + x]
            if p[3] == 0:
                continue
            for (dx, dy) in ((1, 0), (0, 1)):
                if x + dx >= w or y + dy >= h:
                    continue
                q = px[(y + dy) * w + x + dx]
                if q[3] == 0:
                    continue
                tot += abs(p[0] - q[0]) + abs(p[1] - q[1]) + abs(p[2] - q[2])
                n += 3
    return (tot / float(n) / 255.0) if n else 0.0


def sheet(texs, path):
    """Lay the textures out and write the PNG. Returns the cell count."""
    cells = [t for t in texs if t["px"]]
    if not cells:
        return 0
    rows_n = (len(cells) + COLS - 1) // COLS
    W = COLS * (CELL + PAD) + PAD
    H = rows_n * (CELL + PAD) + PAD
    img = [[0x1A] * (W * 3) for _ in range(H)]
    for n, t in enumerate(cells):
        cx = PAD + (n % COLS) * (CELL + PAD)
        cy = PAD + (n // COLS) * (CELL + PAD)
        tw, th, px = t["w"], t["h"], t["px"]
        sc = max(1, min(CELL // tw, CELL // th))
        ow, oh = min(tw * sc, CELL), min(th * sc, CELL)
        ox, oy = cx + (CELL - ow) // 2, cy + (CELL - oh) // 2
        for y in range(oh):
            sy = y // sc
            line = img[oy + y]
            for x in range(ow):
                sx = x // sc
                r, g, b, a = px[sy * tw + sx]
                # a checkerboard behind, so a cutout's holes read as holes
                bg = 0x66 if ((sx >> 2) + (sy >> 2)) & 1 else 0x33
                o = (ox + x) * 3
                line[o] = (r * a + bg * (255 - a)) // 255
                line[o + 1] = (g * a + bg * (255 - a)) // 255
                line[o + 2] = (b * a + bg * (255 - a)) // 255
        i = t["i"]
        for k in range(i % 10 + 1):
            for y in range(2):
                img[cy + k * 3 + y][cx * 3:cx * 3 + 9] = \
                    [0xFF, 0xD0, 0x40] * 3
        for k in range(i // 10):
            o = (cx + CELL - 3) * 3
            for y in range(2):
                img[cy + k * 3 + y][o:o + 9] = [0x40, 0xC0, 0xFF] * 3
    write_png(path, W, H, img)
    return len(cells)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("packs", nargs="*", help="pack filenames in the romdisk")
    ap.add_argument("--all", action="store_true")
    ap.add_argument("--romdisk", default=ROMDISK)
    ap.add_argument("--out", default=OUT_DEFAULT)
    ap.add_argument("--rank", type=int, metavar="N", default=0,
                    help="also write worst.png: the N noisiest textures "
                         "in the whole romdisk, ranked, which is where a "
                         "mis-decoded one surfaces")
    args = ap.parse_args()

    names = args.packs
    if args.all or not names:
        names = sorted(n for n in os.listdir(args.romdisk)
                       if n.endswith(PACK_EXT))
    os.makedirs(args.out, exist_ok=True)

    total = skipped = sheets = 0
    ranked = []
    for name in names:
        path = os.path.join(args.romdisk, name)
        if not os.path.exists(path):
            print("%-22s missing" % name)
            continue
        blob = open(path, "rb").read()
        bases = pack_offsets(blob)
        if not bases:
            skipped += 1
            continue
        stem0 = os.path.splitext(name)[0] + "_" + \
            os.path.splitext(name)[1][1:]
        for k, base in enumerate(bases):
            got = read_pack(blob, base)
            if got is None:
                continue
            hd, texs = got
            stem = stem0 if len(bases) == 1 else "%s_%02d_%s" % (
                stem0, k, hd["name"].lower() or "pack")
            n = sheet(texs, os.path.join(args.out, stem + ".png"))
            with open(os.path.join(args.out, stem + ".txt"), "w") as fp:
                fp.write("%s @0x%X  (%s, %s)  %d textures, %d palettes\n"
                         % (name, base, hd["name"], hd["magic"],
                            hd["tex_count"], hd["pal_count"]))
                for t in texs:
                    fp.write("  %3d  %4dx%-4d  %-9s pal %-3d clamp %d%s\n"
                             % (t["i"], t["w"], t["h"],
                                FMT_NAME.get(t["fmt"], "?%d" % t["fmt"]),
                                t["pal"], t["clamp"],
                                "" if t["px"] or t["fmt"] == FPACK_TEX_EXTERN
                                else "  UNREADABLE"))
            if args.rank:
                for t in texs:
                    if t["px"]:
                        ranked.append((noise(t), name, hd["name"], t["i"],
                                       t))
            total += n
            sheets += 1
            print("%-22s %-9s %3d textures -> %s.png"
                  % (name if k == 0 else "", hd["name"], n, stem))
    print("%d textures on %d sheets in %s (%d files carried no FPack)"
          % (total, sheets, args.out, skipped))
    if args.rank:
        ranked.sort(key=lambda r: -r[0])
        worst = ranked[:args.rank]
        sheet([r[4] for r in worst], os.path.join(args.out, "worst.png"))
        with open(os.path.join(args.out, "worst.txt"), "w") as fp:
            fp.write("the %d noisiest textures in the romdisk, worst "
                     "first -- sheet order is this order\n" % len(worst))
            for (sc, fname, pname, i, t) in worst:
                fp.write("  %.3f  %-22s %-9s tex %-3d %4dx%-4d %s\n"
                         % (sc, fname, pname, i, t["w"], t["h"],
                            FMT_NAME.get(t["fmt"], "?")))
        print("worst %d by noise -> worst.png / worst.txt (top: %s)"
              % (len(worst), ", ".join("%s %s#%d %.2f"
                                       % (r[1], r[2], r[3], r[0])
                                       for r in worst[:3])))
    return 0


if __name__ == "__main__":
    sys.exit(main())

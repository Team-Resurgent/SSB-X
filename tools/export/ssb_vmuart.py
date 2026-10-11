#!/usr/bin/env python3
"""ssb64-dc: the save file's pictures, made from the game's own art at
build time.

A Dreamcast save carries pictures a cartridge's never did: the icon the
BIOS's file manager shows beside the file, a larger "eyecatch" a game
may show on its own load screen, and -- not in the file at all -- the
image a game puts on the VMU's little LCD while it runs. The N64 game has
none of them, so there is no original to export; what there is, is the
game's own logo art, and every picture here is made out of that rather
than drawn by hand. Nothing ROM-derived is committed: this runs in the
build, like every other exporter, and its output lands in the romdisk
tree (gitignored).

The sources, all read with tools/export/ssb_spriteexport.py's readers:

- file 0's SmashLogo, the emblem the menus draw (the circle and cross).
  It is drawn 59x59, but its circle is 64 texels across and fills the
  whole 64-texel stored row, so the menus show it cropped by five columns
  on the right; here it is read at its stored width, as a circle. It is
  an I4 sprite -- a mask the menus tint at runtime -- so
  here it is a mask too: the icon fills it with the fire ramp, the
  eyecatch lays it faintly behind the wordmark, and the LCD thresholds it
  to one bit. (The title's LogoAnimFull is the same emblem larger, but
  the title draws it at x=260 and lets the screen's edge crop it, so its
  art stops short of the circle's right side.)
- file 167's Smash, Super and Bros: the title's wordmark, full colour
  (Smash) and intensity-alpha (Super, Bros.). The icon's fire ramp is
  Smash's own colours, top to bottom.

The pictures, in the formats src/dc/vmucard.c hands KOS's vmu_pkg_build:

- the icon: three 32x32 frames, four bits a pixel, one 16-entry ARGB4444
  palette shared by all three with entry 0 transparent (vmu_pkg.h). The
  emblem in the wordmark's fire colours, with a glint crossing it.
- the eyecatch: 72x56, VMUPKG_EC_16COL -- a 16-entry ARGB4444 palette
  and four bits a pixel, 2048 bytes. 16 colours rather than 256 keeps the
  file at 14 blocks rather than 19 (the plan's D9).
- the LCD image: 48x32, one bit a pixel, row-major from the top-left,
  most significant bit leftmost, set = dark -- vmu_draw_lcd_rotated's
  layout (kernel/arch/dreamcast/hardware/maple/vmu.c: it is
  vmu_draw_lcd's bitmap turned 180 degrees, which is the picture the
  right way up for a player holding the pad).

Four-bit pixels put the left pixel in the high nibble, the VMS format's
order for both the icon and the eyecatch.

The output, little-endian, read by src/dc/vmucard.c vmuart_load:

    0   char magic[4]      "VMUA"
    4   u16  version       1
    6   u16  icon_cnt      3
    8   u16  icon_speed    frames per icon frame, the VMS header's field
    10  u16  eyecatch_type 3 (VMUPKG_EC_16COL)
    12  u16  icon_pal[16]
    44  u8   icon[512 * icon_cnt]
        u8   eyecatch[32 + 2016]   its palette, then its pixels
        u8   lcd[192]

Usage: python3 tools/export/ssb_vmuart.py --out romdisk/vmuart.bin
           [--rom <rom.z64>] [--preview <png>]
"""
import argparse
import math
import os
import struct
import sys

sys.path[:0] = [os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), d)
                for d in ("lib", "export", "check")]
import ssb_meshexport as M       # noqa: E402  (ROM_DEFAULT)
import ssb_logicexport as L      # noqa: E402  (file_info)
import ssb_spriteexport as X     # noqa: E402
import vmuimg                    # noqa: E402  (render_png)

MAGIC = b"VMUA"
VERSION = 1
ICON_W = ICON_H = 32
ICON_FRAMES = 3
# The VMS header's animation speed, in frames of the BIOS's own clock.
ICON_SPEED = 10
EC_W, EC_H = 72, 56
EC_16COL = 3
LCD_W, LCD_H = 48, 32

EMBLEM_FILE = 0           # the menus' common sprites
EMBLEM = "SmashLogo"
TITLE_FILE = 167          # MNTitle
WORDMARK = ("Super", "Smash", "Bros")

HEADER = "<4sHHHH16H"
assert struct.calcsize(HEADER) == 44


# ---- sprites as float RGBA ------------------------------------------------

class Image:
    """w x h RGBA, each channel 0..1, straight (not premultiplied)."""

    def __init__(self, w, h, px=None):
        self.w, self.h = w, h
        self.px = px if px is not None else [(0.0, 0.0, 0.0, 0.0)] * (w * h)

    def get(self, x, y):
        return self.px[y * self.w + x]

    def put(self, x, y, c):
        self.px[y * self.w + x] = c


def word_rgba(w, pvrfmt):
    if pvrfmt == X.PVRFMT_ARGB1555:
        return ((w >> 10 & 31) / 31.0, (w >> 5 & 31) / 31.0,
                (w & 31) / 31.0, 1.0 if w & 0x8000 else 0.0)
    return ((w >> 8 & 15) / 15.0, (w >> 4 & 15) / 15.0, (w & 15) / 15.0,
            (w >> 12 & 15) / 15.0)


def sprite(rom, fid, name, stored=False):
    """One sprite of one file as the exporter decodes it: I and IA come
    back white-or-grey with their coverage in alpha, which is how the
    game's combiner reads them too. `stored`: the whole stored row rather
    than the drawn width (the emblem, below)."""
    f, _e, intern, _sites, _ids = L.file_info(rom, fid)
    offs = dict(X.catalogue(fid))
    if name not in offs:
        sys.exit("ssb_vmuart: file %d has no sprite %s" % (fid, name))
    sp = X.read_sprite(f, intern, offs[name])
    pvrfmt, texw, _texh, imgw, imgh, data, _row0 = X.stitch(f, sp)
    # The drawn size, not the stored one: a row is stored padded to the
    # RDP's load width, and the padding is usually not picture.
    w, h = min(sp["width"], imgw), min(sp["height"], imgh)
    if stored:
        w = imgw
    img = Image(w, h)
    for y in range(h):
        for x in range(w):
            word = struct.unpack_from("<H", data, (y * texw + x) * 2)[0]
            img.put(x, y, word_rgba(word, pvrfmt))
    return img


def resample(src, dw, dh):
    """A box filter over premultiplied colour: every source texel counts
    by the area of it the destination pixel covers. Shrinks and grows."""
    out = Image(dw, dh)
    sx, sy = src.w / dw, src.h / dh
    for y in range(dh):
        y0, y1 = y * sy, (y + 1) * sy
        for x in range(dw):
            x0, x1 = x * sx, (x + 1) * sx
            r = g = b = a = wt = 0.0
            for yy in range(int(y0), min(src.h, math.ceil(y1))):
                wy = min(y1, yy + 1) - max(y0, yy)
                for xx in range(int(x0), min(src.w, math.ceil(x1))):
                    w = (min(x1, xx + 1) - max(x0, xx)) * wy
                    pr, pg, pb, pa = src.get(xx, yy)
                    r += pr * pa * w
                    g += pg * pa * w
                    b += pb * pa * w
                    a += pa * w
                    wt += w
            if a > 0:
                out.put(x, y, (r / a, g / a, b / a, a / wt))
    return out


def over(dst, src, ox, oy, alpha=1.0):
    """src over dst at (ox, oy), src's coverage scaled by alpha."""
    for y in range(src.h):
        for x in range(src.w):
            dx, dy = ox + x, oy + y
            if not (0 <= dx < dst.w and 0 <= dy < dst.h):
                continue
            sr, sg, sb, sa = src.get(x, y)
            sa *= alpha
            if sa <= 0:
                continue
            dr, dg, db, da = dst.get(dx, dy)
            oa = sa + da * (1 - sa)
            dst.put(dx, dy, ((sr * sa + dr * da * (1 - sa)) / oa,
                             (sg * sa + dg * da * (1 - sa)) / oa,
                             (sb * sa + db * da * (1 - sa)) / oa, oa))


# ---- the fire gradient ----------------------------------------------------

def fire_stops(smash, n=6):
    """The Smash wordmark's colour down its height, in n stops: each the
    mean of a band of rows' opaque texels, weighted by brightness so the
    red outline every row also crosses pulls it less than the letters'
    fill. Bands and not rows: row by row the flames in the letters make
    the ramp flicker, which a 32-pixel icon shows as stripes."""
    rows = []
    for y in range(smash.h):
        r = g = b = wt = 0.0
        for x in range(smash.w):
            pr, pg, pb, pa = smash.get(x, y)
            w = pa * (pr + pg + pb)
            r, g, b, wt = r + pr * w, g + pg * w, b + pb * w, wt + w
        if wt > 0:
            rows.append((r, g, b, wt))
    stops = []
    for i in range(n):
        band = rows[i * len(rows) // n:(i + 1) * len(rows) // n]
        wt = sum(c[3] for c in band)
        stops.append(tuple(sum(c[k] for c in band) / wt for k in range(3)))
    return stops


def ramp(stops, t):
    t = min(max(t, 0.0), 1.0) * (len(stops) - 1)
    i = min(int(t), len(stops) - 2)
    f = t - i
    return tuple(stops[i][k] * (1 - f) + stops[i + 1][k] * f
                 for k in range(3))


# ---- quantising -----------------------------------------------------------

def to4444(c):
    r, g, b, a = (min(15, max(0, int(v * 15 + 0.5))) for v in c)
    return (a << 12) | (r << 8) | (g << 4) | b


def median_cut(colours, n):
    """{ARGB4444: count} -> at most n palette words, all opaque. A box is
    split at the weighted median of its widest channel until there are n
    of them, or none can be split."""
    boxes = [list(colours.items())]
    while len(boxes) < n:
        best, axis, span = None, 0, 0
        for i, box in enumerate(boxes):
            if len(box) < 2:
                continue
            for sh in (8, 4, 0):
                vals = [c >> sh & 15 for c, _ in box]
                if max(vals) - min(vals) > span:
                    best, axis, span = i, sh, max(vals) - min(vals)
        if best is None:
            break
        box = sorted(boxes.pop(best), key=lambda cw: cw[0] >> axis & 15)
        total = sum(w for _, w in box)
        acc, cut = 0, 1
        for i, (_, w) in enumerate(box):
            acc += w
            if acc * 2 >= total:
                cut = min(max(i + 1, 1), len(box) - 1)
                break
        boxes += [box[:cut], box[cut:]]
    pal = []
    for box in boxes:
        total = sum(w for _, w in box)
        chans = [sum((c >> sh & 15) * w for c, w in box) / total
                 for sh in (8, 4, 0)]
        r, g, b = (int(v + 0.5) for v in chans)
        pal.append(0xF000 | (r << 8) | (g << 4) | b)
    return pal


def nearest(pal, c, first=0):
    r, g, b = c >> 8 & 15, c >> 4 & 15, c & 15
    best, bi = None, first
    for i in range(first, len(pal)):
        p = pal[i]
        d = ((p >> 8 & 15) - r) ** 2 * 3 + ((p >> 4 & 15) - g) ** 2 * 4 + \
            ((p & 15) - b) ** 2 * 2
        if best is None or d < best:
            best, bi = d, i
    return bi


def pack4(indices):
    out = bytearray()
    for i in range(0, len(indices), 2):
        out.append((indices[i] << 4) | indices[i + 1])
    return bytes(out)


# ---- the pictures ---------------------------------------------------------

def emblem_mask(emblem, w, h):
    return resample(emblem, w, h)


def make_icon(emblem, stops):
    """Three frames of the emblem filled with the fire ramp, top to
    bottom, and outlined; a glint -- a diagonal band toward white --
    crosses it from the top left, one step a frame."""
    # 64x59 into 30x28, centred in the 32x32 frame with room for the
    # outline
    mw, mh = 30, 28
    ox, oy = 1, 2
    mask = emblem_mask(emblem, mw, mh)
    inside = [[False] * ICON_W for _ in range(ICON_H)]
    for y in range(mh):
        for x in range(mw):
            inside[oy + y][ox + x] = mask.get(x, y)[3] >= 0.5
    outline = [[False] * ICON_W for _ in range(ICON_H)]
    for y in range(ICON_H):
        for x in range(ICON_W):
            if inside[y][x]:
                continue
            for dy in (-1, 0, 1):
                for dx in (-1, 0, 1):
                    yy, xx = y + dy, x + dx
                    if 0 <= yy < ICON_H and 0 <= xx < ICON_W and inside[yy][xx]:
                        outline[y][x] = True
    # The outline is the ramp's darkest stop, darker still.
    dark = min(stops, key=sum)
    dark = tuple(v * 0.35 for v in dark)
    # where the glint's centre line x + y = c sits in each frame
    glints = [16, 31, 46]

    frames = []
    for k in range(ICON_FRAMES):
        fr = []
        for y in range(ICON_H):
            for x in range(ICON_W):
                if inside[y][x]:
                    c = ramp(stops, (y - oy) / (mh - 1))
                    g = max(0.0, 1.0 - abs(x + y - glints[k]) / 3.5)
                    c = tuple(v + (1.0 - v) * 0.8 * g for v in c)
                    fr.append(to4444(c + (1.0,)))
                elif outline[y][x]:
                    fr.append(to4444(dark + (1.0,)))
                else:
                    fr.append(None)
        frames.append(fr)

    hist = {}
    for fr in frames:
        for c in fr:
            if c is not None:
                hist[c] = hist.get(c, 0) + 1
    pal = [0x0000] + median_cut(hist, 15)
    pal += [0x0000] * (16 - len(pal))
    data = b""
    for fr in frames:
        data += pack4([0 if c is None else nearest(pal, c, 1) for c in fr])
    return pal, data


def make_eyecatch(emblem, words):
    """72x56, opaque: a deep blue ground, the emblem faint behind, SUPER
    above SMASH above BROS. as the title stacks them."""
    ec = Image(EC_W, EC_H)
    for y in range(EC_H):
        t = y / (EC_H - 1)
        c = (0.05 + 0.10 * t, 0.05 + 0.10 * t, 0.20 + 0.25 * t, 1.0)
        for x in range(EC_W):
            ec.put(x, y, c)
    over(ec, emblem_mask(emblem, 56, 52), (EC_W - 56) // 2, 2, alpha=0.18)
    sup, smash, bros = words
    over(ec, resample(sup, 27, 21), 6, 1)
    over(ec, resample(smash, 70, 25), 1, 17)
    over(ec, resample(bros, 24, 22), 43, 33)

    hist = {}
    cols = [to4444(c) for c in ec.px]
    for c in cols:
        hist[c] = hist.get(c, 0) + 1
    pal = median_cut(hist, 16)
    pal += [0xF000] * (16 - len(pal))
    return pal, pack4([nearest(pal, c) for c in cols])


def make_lcd(emblem):
    mw, mh = 32, 30
    ox, oy = (LCD_W - mw) // 2, (LCD_H - mh) // 2
    mask = emblem_mask(emblem, mw, mh)
    bits = bytearray(LCD_W * LCD_H // 8)
    for y in range(mh):
        for x in range(mw):
            if mask.get(x, y)[3] >= 0.5:
                px, py = ox + x, oy + y
                bits[py * (LCD_W // 8) + px // 8] |= 0x80 >> (px % 8)
    return bytes(bits)


def build(rom):
    emblem = sprite(rom, EMBLEM_FILE, EMBLEM, stored=True)
    words = [sprite(rom, TITLE_FILE, n) for n in WORDMARK]
    stops = fire_stops(words[1])
    icon_pal, icons = make_icon(emblem, stops)
    ec_pal, ec_px = make_eyecatch(emblem, words)
    lcd = make_lcd(emblem)
    blob = struct.pack(HEADER, MAGIC, VERSION, ICON_FRAMES, ICON_SPEED,
                       EC_16COL, *icon_pal)
    blob += icons
    blob += struct.pack("<16H", *ec_pal) + ec_px
    blob += lcd
    return blob


def parse(blob):
    """The inverse of build, for the preview and for anyone checking."""
    fields = struct.unpack_from(HEADER, blob, 0)
    magic, version, cnt, speed, ec_type = fields[:5]
    if magic != MAGIC or version != VERSION or ec_type != EC_16COL:
        raise ValueError("not a vmuart.bin v%d" % VERSION)
    at = struct.calcsize(HEADER)
    icons = blob[at:at + 512 * cnt]
    at += 512 * cnt
    eyecatch = blob[at:at + 32 + EC_W * EC_H // 2]
    at += len(eyecatch)
    lcd = blob[at:at + LCD_W * LCD_H // 8]
    at += len(lcd)
    if at != len(blob):
        raise ValueError("vmuart.bin: %d bytes, expected %d" % (len(blob), at))
    return dict(icon_cnt=cnt, icon_speed=speed, icon_pal=list(fields[5:]),
                icons=icons, eyecatch_type=ec_type, eyecatch=eyecatch,
                lcd=lcd)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--rom", default=M.ROM_DEFAULT)
    ap.add_argument("--out", required=True)
    ap.add_argument("--preview", help="also write a PNG of all three "
                    "pictures (build output only; never commit it)")
    args = ap.parse_args()
    with open(args.rom, "rb") as fp:
        rom = fp.read()
    blob = build(rom)
    with open(args.out, "wb") as fp:
        fp.write(blob)
    a = parse(blob)
    if args.preview:
        vmuimg.render_png(args.preview, a["icon_pal"], a["icons"],
                          a["icon_cnt"], a["eyecatch_type"], a["eyecatch"],
                          a["lcd"])
    print("vmuart: %d bytes -> %s (%d icon frames, eyecatch %d, lcd %dx%d)"
          % (len(blob), args.out, a["icon_cnt"], a["eyecatch_type"],
             LCD_W, LCD_H))


if __name__ == "__main__":
    main()

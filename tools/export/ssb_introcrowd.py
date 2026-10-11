#!/usr/bin/env python3
"""ssb_introcrowd.py -- bake the three 1P team cards' crowds with no console.

The Yoshi, Kirby and Fighting Polygon Team cards draw 18, 8 and 30 posed
models a frame for a picture that stops changing after its first second.
This makes that picture as VQ-compressed ARGB1555 tiles the card
draws instead (src/dc/introcrowd.h), out of nothing but the extracted assets:

  1. src/game/ssb64/bake_host (make hosttest BAKER=1) boots the
     card's own scene on the host with the port's real draw path over a
     recording PVR, and writes the triangles the console would send the TA;
  2. tools/lib/pvrsoft.py draws them, on transparent black, by the PVR's
     rules -- the alpha it ends with is the crowd's coverage;
  3. the crowd's box is cut into TILE_W-wide columns, each a VQ texture with
     a codebook of its own, and packed as an ICRW file, one per card,
     OUTDIR/introcrowd_<card>.bin:

    "ICRW"  u32 version (1)
    u16 x0, y0, w, h      the crowd's box on the 640x480 screen, the union
                          over the card's variants
    u16 tiles             per variant: the box cut into TILE_W-wide columns
    u16 variants
    per tile:    u16 dx, u16 draw_w, u16 tex_w, u16 tex_h
    per variant: u16 key, u16 pad, then per tile u32 offset, u32 size
    the tile data, each blob 32-byte aligned: a VQ texture as the PVR reads
    it -- the 2048-byte codebook, then one index byte per 2x2 texels

The variant a card draws is picked by `key` (src/dc/introcrowd.h).

    tools/export/ssb_introcrowd.py --bake-host src/game/ssb64/bake_host OUTDIR
    tools/export/ssb_introcrowd.py --bake-host ... --dump-dir DIR --png OUTDIR

--png also writes each variant's picture as OUTDIR/<name>.png (needs Pillow);
--dump-dir keeps the recorded streams. Variants can be named on the command
line to bake a few. The bake binary is run from --rom-dir, where the romdisk
it reads lives (default src/game/ssb64).
"""
import argparse
import json
import os
import struct
import subprocess
import sys
import tempfile

import numpy as np

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "lib"))
import pvrsoft  # noqa: E402

# nFTKind*, from ft/ftdef.h
FOX, YOSHI, KIRBY = 1, 6, 8
# the 1P ladder rungs the three cards are (nSC1PGameStage*)
STAGE_YOSHI, STAGE_KIRBY, STAGE_POLY = 1, 8, 12
KIRBY_HATS = [0, 3, 9, 11, 13]       # Kirby, Purin, Captain, Luigi, Ness

# card -> [(name, key, stage, player fkind, costume, copy hat or -1)]
# The key is src/dc/introcrowd.h's: Yoshi 0, or 1 + costume when the player is
# Yoshi; Kirby (hat << 1) | (player is Kirby in costume 0); Polygon 0. A
# player who is Fox stands for "not the card's own fighter".
CARDS = {
    "yoshi": [("yoshi_plain", 0, STAGE_YOSHI, FOX, 0, -1)] +
             [("yoshi_shade%d" % c, 1 + c, STAGE_YOSHI, YOSHI, c, -1)
              for c in range(4)],
    "kirby": [("kirby_h%d_%s" % (h, kind), (h << 1) | (kind == "recolor"),
               STAGE_KIRBY, KIRBY if kind == "recolor" else FOX, 0, h)
              for h in KIRBY_HATS for kind in ("plain", "recolor")],
    "poly": [("poly", 0, STAGE_POLY, FOX, 0, -1)],
}

# What of the picture is ever seen, in framebuffer pixels: the card's panel
# between its banners, and short of the right-hand frame. The crowd runs on
# past all four (its camera's viewport is wider) and the banners are drawn
# over it. From the boxes the baked crowds occupy (120..364 tall, out to 620).
WINDOW = (320, 120, 620, 364)
TIC = 150                             # the card's crowd is complete by ~70
# Each tile is its own VQ texture with its own 256-entry codebook, so narrower
# tiles buy quality for bytes (Yoshi Team's crowd, masked-pixel PSNR: 26.6 dB
# at 256 wide, 30.3 at 64, 34.3 at 32, 38.7 at 16; 18, 25, 33 and 49 KB an
# image). 32 is the knee.
TILE_W = 32
TILES_MAX = 12                        # src/dc/introcrowd.c IC_TILES_MAX
VERSION = 1
ALPHA_CUT = 128
VQ_ITERS = 12
VQ_FINAL = 80
ALPHA_WEIGHT = 24.0                   # a covered texel vs an empty one, in 1555 steps


def pow2_up(n):
    return 1 << (max(int(n), 1) - 1).bit_length()


# ---- 1. the picture ---------------------------------------------------------

# The colour the rasteriser is off from the console's by, as a per-channel gain
# and offset on its 0..255 output, fitted once to captured frames of these
# cards; the build only reads it. A brute-force search over hue, saturation and value found no hue or
# saturation error -- the render is about 2.5% dark, more in red and blue.
CALIB = os.path.join(os.path.dirname(__file__), "..", "lib", "pvrsoft_calib.json")


def load_calib(path=CALIB):
    """(gain[3], offset[3]) from the calibration file; the identity without one."""
    if path and os.path.exists(path):
        c = json.load(open(path))
        return np.array(c["gain"], dtype=np.float64), np.array(c["offset"], dtype=np.float64)
    return np.ones(3), np.zeros(3)


def bake_variant(bake_host, rom_dir, name, stage, fkind, costume, hat, dump,
                 calib=None):
    dump = os.path.abspath(dump)
    subprocess.run([os.path.abspath(bake_host), str(stage), str(fkind),
                    str(costume), str(hat), str(TIC), dump],
                   cwd=rom_dir, check=True, stdout=subprocess.DEVNULL,
                   stderr=subprocess.DEVNULL)
    s = pvrsoft.read_log(dump)
    fb, _ = pvrsoft.render(s)
    x0, y0, x1, y1 = WINDOW
    fb = fb[y0:y1, x0:x1].astype(np.float64)
    a = fb[..., 3]
    cov = a >= ALPHA_CUT
    # the blend left colour over transparent black: undo it where covered
    rgb = np.where(cov[..., None], fb[..., :3] * 255.0 / np.maximum(a, 1)[..., None], 0)
    if calib is not None:
        rgb = np.where(cov[..., None], rgb * calib[0] + calib[1], 0)
    out = np.zeros(fb.shape, dtype=np.uint8)
    out[..., :3] = np.clip(np.rint(rgb), 0, 255)
    out[..., 3] = np.where(cov, 255, 0)
    return out


# ---- 2. ARGB1555 VQ ---------------------------------------------------------

def to_1555(rgba):
    """(h, w, 4) uint8 -> (a, r, g, b) as ints, a in {0,1}, colours 0..31,
    with an empty texel black (the same transparent black pvrtex writes)."""
    a = (rgba[..., 3] >= ALPHA_CUT).astype(np.int64)
    c = (rgba[..., :3].astype(np.int64) * 31 + 127) // 255
    c *= a[..., None]
    return a, c


def lbg(X, w, k):
    """Linde-Buzo-Gray: split every cell in two, settle with Lloyd's, until
    there are k. X (n, d) distinct points, w (n,) how many of each."""
    cent = (X * w[:, None]).sum(0, keepdims=True) / w.sum()
    for level in range(64):
        if len(cent) >= k:
            iters = VQ_FINAL
        else:
            iters = VQ_ITERS
        if len(cent) < k:
            spread = np.sqrt(((X - cent[0]) ** 2).mean(0)) * 0.02 + 1e-3
            cent = np.concatenate([cent + spread, cent - spread])[:2 * len(cent)]
        for _ in range(iters):
            d = ((X[:, None, :] - cent[None, :, :]) ** 2).sum(-1)
            asg = d.argmin(1)
            for j in range(len(cent)):
                m = asg == j
                if m.any():
                    cent[j] = (X[m] * w[m, None]).sum(0) / w[m].sum()
                else:
                    cent[j] = X[(d.min(1) * w).argmax()]
        if iters == VQ_FINAL:
            break
    return cent, None


def vq_tile(rgba, k=256):
    """The bytes a PVR reads for a VQ ARGB1555 texture of this picture: the
    2048-byte codebook and one index per 2x2 texels, twiddled. rgba's sides
    are the texture's (powers of two, 8 or more)."""
    h, w = rgba.shape[:2]
    a, c = to_1555(rgba)
    hh, hw = h // 2, w // 2
    # blocks in the codebook's texel order: (x0,y0) (x0,y1) (x1,y0) (x1,y1)
    order = [(0, 0), (1, 0), (0, 1), (1, 1)]            # (dy, dx)
    A = np.stack([a[dy::2, dx::2] for dy, dx in order], -1).reshape(-1, 4)
    C = np.stack([c[dy::2, dx::2] for dy, dx in order], 2).reshape(-1, 4, 3)
    # features: coverage, then the colour weighted by it, so an empty texel's
    # black is not a colour
    F = np.concatenate([A * ALPHA_WEIGHT, (C * A[..., None]).reshape(-1, 12)], 1).astype(np.float64)
    n = len(F)

    uniq, inv, counts = np.unique(F, axis=0, return_inverse=True, return_counts=True)
    inv = inv.reshape(-1)
    wts = counts.astype(np.float64)
    if len(uniq) <= k:
        cent = uniq
        idx = inv
    else:
        cent, _ = lbg(uniq, wts, k)
        d = ((uniq[:, None, :] - cent[None, :, :]) ** 2).sum(-1)
        idx = d.argmin(1)[inv]

    # centroid -> four 1555 words
    book = np.zeros((256, 4), dtype=np.uint16)
    for j in range(len(cent)):
        ca = cent[j][:4] / ALPHA_WEIGHT
        cc = cent[j][4:].reshape(4, 3)
        for t in range(4):
            if ca[t] >= 0.5:
                rgb = np.clip(np.rint(cc[t] / max(ca[t], 1e-6)), 0, 31).astype(int)
                book[j, t] = 0x8000 | (rgb[0] << 10) | (rgb[1] << 5) | rgb[2]
    tw = pvrsoft.twiddle_index(hw, hh)
    data = np.zeros(hw * hh, dtype=np.uint8)
    data[tw.reshape(-1)] = idx.reshape(hh, hw).reshape(-1).astype(np.uint8)
    return book.astype("<u2").tobytes() + data.tobytes()


def decode_vq(blob, tw, th):
    """What the console shows of vq_tile's bytes (pvrsoft's decoder, the one
    the shipped pvrtex tiles decode correctly through)."""
    s = pvrsoft.Stream()
    s.vram[0] = blob
    usz = {8: 0, 16: 1, 32: 2, 64: 3, 128: 4, 256: 5, 512: 6, 1024: 7}
    return pvrsoft.decode_texture(s, (usz[tw] << 3) | usz[th], 1 << 30)


# ---- 3. the file ------------------------------------------------------------

def pack_card(card, variants):
    """variants: [(name, key, rgba window picture)] -> the ICRW bytes."""
    xs0, ys0 = WINDOW[0], WINDOW[1]
    boxes = []
    for _, _, img in variants:
        ys, xs = np.nonzero(img[..., 3])
        if len(xs) == 0:
            sys.exit("ssb_introcrowd: %s: nothing was drawn" % card)
        boxes.append((xs.min(), ys.min(), xs.max() + 1, ys.max() + 1))
    boxes = [tuple(int(v) for v in b) for b in boxes]
    bx0 = min(b[0] for b in boxes)
    by0 = min(b[1] for b in boxes)
    bx1 = max(b[2] for b in boxes)
    by1 = max(b[3] for b in boxes)
    w, h = bx1 - bx0, by1 - by0
    cols = []
    dx = 0
    while dx < w:
        dw = min(TILE_W, w - dx)
        cols.append((dx, dw, pow2_up(max(dw, 8)) if dw < TILE_W else TILE_W))
        dx += dw
    th = pow2_up(h)
    if th > 1024 or len(cols) > TILES_MAX:
        sys.exit("ssb_introcrowd: %s box %dx%d is not a few tiles" % (card, w, h))
    print("%s: box x %d..%d y %d..%d = %d x %d px, %d tile(s): %s" %
          (card, xs0 + bx0, xs0 + bx1, ys0 + by0, ys0 + by1, w, h, len(cols),
           ", ".join("%dx%d" % (c[2], th) for c in cols)))

    blobs = []
    for name, key, img in variants:
        crop = img[by0:by1, bx0:bx1]
        tiles = []
        for cx, cw, tw in cols:
            canvas = np.zeros((th, tw, 4), dtype=np.uint8)
            canvas[:h, :cw] = crop[:, cx:cx + cw]
            tiles.append(vq_tile(canvas))
        blobs.append((key, tiles))

    head = bytearray(b"ICRW")
    head += struct.pack("<I", VERSION)
    head += struct.pack("<4H", xs0 + bx0, ys0 + by0, w, h)
    head += struct.pack("<2H", len(cols), len(blobs))
    for cx, cw, tw in cols:
        head += struct.pack("<4H", cx, cw, tw, th)
    table_at = len(head)
    head += bytes(len(blobs) * (4 + 8 * len(cols)))
    off = (len(head) + 31) & ~31
    body = bytearray()
    table = bytearray()
    for key, tiles in blobs:
        table += struct.pack("<2H", key, 0)
        for data in tiles:
            table += struct.pack("<2I", off + len(body), len(data))
            body += data
            body += bytes((-len(body)) % 32)
    head[table_at:table_at + len(table)] = table
    blob = bytes(head) + bytes(off - len(head)) + bytes(body)

    # the round trip: what the game reads back is what was packed
    rx0, ry0, rw, rh, nt, nv = struct.unpack_from("<4H2H", blob, 8)
    assert (rx0, ry0, rw, rh, nt, nv) == (xs0 + bx0, ys0 + by0, w, h, len(cols), len(blobs))
    return blob, (bx0, by0, bx1, by1), cols, th, blobs


def psnr_of(card_img, blobs_entry, box, cols, th):
    """Masked PSNR of a variant's tiles, decoded, against its picture."""
    bx0, by0, bx1, by1 = box
    h = by1 - by0
    tiles = blobs_entry
    out = np.zeros((h, bx1 - bx0, 4), dtype=np.uint8)
    for (cx, cw, tw), data in zip(cols, tiles):
        out[:, cx:cx + cw] = decode_vq(data, tw, th)[:h, :cw]
    ref = card_img[by0:by1, bx0:bx1]
    m = ref[..., 3] > 0
    mism = ((out[..., 3] > 0) != m).sum()
    d = (out[..., :3].astype(float) - ref[..., :3].astype(float))[m & (out[..., 3] > 0)]
    mse = (d ** 2).mean()
    return 10 * np.log10(255.0 ** 2 / max(mse, 1e-9)), int(mism), int(m.sum())


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--bake-host", required=True)
    ap.add_argument("--rom-dir", default="src/game/ssb64")
    ap.add_argument("--dump-dir")
    ap.add_argument("--png", action="store_true")
    ap.add_argument("--no-calibrate", action="store_true",
                    help="skip the colour calibration (pvrsoft_calib.json)")
    ap.add_argument("outdir")
    ap.add_argument("names", nargs="*")
    args = ap.parse_args()
    os.makedirs(args.outdir, exist_ok=True)
    calib = None if args.no_calibrate else load_calib()
    work = args.dump_dir or tempfile.mkdtemp(prefix="introcrowd_")
    os.makedirs(work, exist_ok=True)

    for card, vlist in CARDS.items():
        if args.names and not any(v[0] in args.names for v in vlist):
            continue
        pics = []
        for name, key, stage, fkind, costume, hat in vlist:
            img = bake_variant(args.bake_host, args.rom_dir, name, stage, fkind,
                               costume, hat, os.path.join(work, name + ".txt"), calib)
            pics.append((name, key, img))
            if args.png:
                from PIL import Image
                bg = np.full(img.shape[:2] + (3,), 70, np.uint8)
                a = img[..., 3:4] / 255.0
                Image.fromarray((img[..., :3] * a + bg * (1 - a)).astype(np.uint8)).save(
                    os.path.join(args.outdir, name + ".png"))
        blob, box, cols, th, blobs = pack_card(card, pics)
        path = os.path.join(args.outdir, "introcrowd_%s.bin" % card)
        open(path, "wb").write(blob)
        print("  %s: %d variant(s), %d bytes" % (path, len(blobs), len(blob)))
        for (name, key, img), (_, tiles) in zip(pics, blobs):
            p, mism, npx = psnr_of(img, tiles, box, cols, th)
            print("    %-18s key %2d  VQ vs render %.2f dB, %d of %d covered texels differ in coverage"
                  % (name, key, p, mism, npx))
    print("ssb_introcrowd: done")


if __name__ == "__main__":
    main()

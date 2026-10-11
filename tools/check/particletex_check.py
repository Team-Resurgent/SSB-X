#!/usr/bin/env python3
"""The particle texture packs against the decomp's own texture decoder.

tools/export/ssb_particletexexport.py turns the nine banks' 246 N64 images into
PVR textures. Every step of that is tools/lib/ssb_assets.py's mesh path --
n64_texel, pvr_encoding, twiddle_16bpp -- which is checked elsewhere and
has been on screen since H1, but it is checked there against *meshes*, and
a particle bank is a different pile of bytes: nine format/size pairs, four
of which no fighter or stage uses.

So this holds every frame to a decoder that is not ours.
ssb-decomp-re/tools/extractParticleTextures.py is the decomp's own, written
for these exact banks -- its parse_textures is what ssb_particleexport.py's
walk was modelled on -- and its decode_image is a second, independent
implementation of the nine cases.

Three legs:

  parse    the exporter's walk of the swapped .txb against the decomp
           tool's walk of the ROM's own bytes: the same textures, the
           same counts, formats, sizes, dimensions and flags, and the
           same image and palette bytes.

  decode   every frame's texels, ssb_assets.n64_texel against the
           decomp tool's decode_image, RGBA8888 for RGBA8888. The one
           allowed difference is an I4/I8 texel's alpha: the RDP
           replicates intensity into all four channels and the decomp's
           tool writes 0xFF because it is writing a PNG for a person to
           look at. That narrowing is written down in
           tools/check/texfmt_check.py and the game's own data settles it (see
           n64_texel's comment). The other is the 5-bit expansion: the
           RDP replicates a 5-bit channel's top bits into its bottom ones
           and ssb_assets._rgba5551 shifts and stops, which is up to
           7/255 dark. Nothing the port ships holds eight bits of a
           channel, so that one has to vanish at four bits and at five,
           and this check requires it to. Any other difference fails.

  pack     the .txp read back the way src/dc/lbpartex.c reads it, every
           frame untwiddled, against the same texels quantised to the
           format its header claims. The twiddle is KOS's own layout and
           this is the inverse of it, so a frame that survives is one the
           PVR will sample as the ROM drew it.

Usage: python3 tools/check/particletex_check.py [--bank <name>]
"""
import argparse
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path[:0] = [os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), d)
                for d in ("lib", "export", "check")]
import ssb_particleexport as PE                                  # noqa: E402
import ssb_particletexexport as TX                               # noqa: E402
from ssb_assets import n64_texel, _pvr_argb1555, _pvr_argb4444   # noqa: E402
from ssb_texconv import twidout                                  # noqa: E402

DECOMP = os.environ.get("SSB_DECOMP_DIR", "/opt/ssb-decomp-re")
if not os.path.isdir(DECOMP):
    DECOMP = os.path.join(ROOT, "ssb-decomp-re")

FMT_I = 4


def untwiddle_16bpp(buf, w, h):
    """The inverse of ssb_texconv.twiddle_16bpp."""
    out = [0] * (w * h)
    minv = min(w, h)
    mask = minv - 1
    for y in range(h):
        for x in range(w):
            idx = twidout(x & mask, y & mask) + \
                (x // minv + y // minv) * minv * minv
            out[y * w + x] = struct.unpack_from("<H", buf, idx * 2)[0]
    return out


def read_pack(data):
    """src/dc/lbpartex.c's own read of a .txp."""
    magic, ntex, nframe, texels_off = TX.HDR.unpack_from(data, 0)
    # src/dc/lbpartex.c refuses a blob that does not start on a 32-byte
    # boundary: that is the GD-ROM DMA stream's condition, and the whole
    # point of the exporter's padding.
    if texels_off % 32:
        raise SystemExit("particletex_check: texels_off %d is not 32-byte "
                         "aligned" % texels_off)
    if magic != TX.MAGIC:
        raise ValueError(f"not a .txp: {magic!r}")
    recs = [TX.TEX.unpack_from(data, TX.HDR.size + i * TX.TEX.size)
            for i in range(ntex)]
    base = TX.HDR.size + ntex * TX.TEX.size
    offs = list(struct.unpack_from("<%dI" % nframe, data, base))
    return recs, offs, texels_off


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--bank", choices=sorted(PE.BANKS))
    args = ap.parse_args()

    if not os.path.isdir(DECOMP):
        print(f"no decomp checkout at {DECOMP} -- skipping")
        return 0
    sys.path.insert(0, os.path.join(DECOMP, "tools"))
    try:
        import extractParticleTextures as EPT
    except ImportError:
        print("no extractParticleTextures.py in the decomp -- skipping")
        return 0

    rom_path = PE.ROM_DEFAULT
    if not os.path.exists(rom_path):
        print("no baserom -- skipping")
        return 0
    with open(rom_path, "rb") as f:
        rom = f.read()

    banks = [args.bank] if args.bank else sorted(PE.BANKS)
    n_tex = n_frame = n_texel = n_alpha = n_expand = 0
    n_pad = 0
    fmts = {}

    for name in banks:
        lo, hi = PE.BANKS[name]["txb"]
        raw = rom[lo:hi]                        # the ROM's own bytes
        swapped = PE.export(rom, name, "txb")   # what the port ships
        theirs = EPT.parse_textures(raw)
        mine = TX.read_bank(swapped)

        if len(theirs) != len(mine):
            print(f"FAIL parse: {name}: {len(mine)} textures, the decomp's "
                  f"tool finds {len(theirs)}")
            return 1

        pack = TX.export(rom, name)
        recs, offs, texels_off = read_pack(pack)
        if len(recs) != len(mine):
            print(f"FAIL pack: {name}: {len(recs)} records for "
                  f"{len(mine)} textures")
            return 1

        for i, (a, b, rec) in enumerate(zip(mine, theirs, recs)):
            where = f"{name} texture {i}"
            for field, k in (("count", "count"), ("fmt", "fmt"),
                             ("siz", "siz"), ("width", "w"),
                             ("height", "h"), ("flags", "flags")):
                if a[field] != b[k]:
                    print(f"FAIL parse: {where}: {field} is {a[field]}, "
                          f"the decomp's tool says {b[k]}")
                    return 1
            n_tex += 1

            first_frame, frames, fmt, pw, ph, sw, sh = rec
            if (frames, sw, sh) != (a["count"], a["width"], a["height"]):
                print(f"FAIL pack: {where}: record is {frames} frames "
                      f"{sw}x{sh}, the bank says {a['count']} "
                      f"{a['width']}x{a['height']}")
                return 1
            if (pw, ph) != (sw, sh):
                n_pad += 1
            fmts[fmt] = fmts.get(fmt, 0) + frames

            entries = 16 if a["siz"] == 0 else 256
            palettes = a["data"][a["count"]:]

            for f in range(a["count"]):
                # decode: ours against theirs, over the same image bytes
                pal = b""
                if b["palettes"]:
                    pal = b["palettes"][0] if (b["flags"] & 1) \
                        else b["palettes"][f]
                theirs_rgba = EPT.decode_image(b["fmt"], b["siz"],
                                               b["images"][f], pal)
                tlut = ()
                if a["fmt"] == TX.FMT_CI:
                    p = palettes[0] if (a["flags"] & 1) else palettes[f]
                    tlut = TX.tlut_words(swapped, p, entries)
                img = a["data"][f]
                ours = [n64_texel(swapped, img, k, a["fmt"], a["siz"], tlut)
                        for k in range(a["width"] * a["height"])]

                for k, (r, g, bl, al) in enumerate(ours):
                    tr, tg, tb, ta = theirs_rgba[k * 4:k * 4 + 4]
                    if (r, g, bl) != (tr, tg, tb):
                        # The 5-bit expansion: the decomp's tool
                        # replicates the top bits into the bottom ones,
                        # (v << 3) | (v >> 2), and ssb_assets._rgba5551
                        # shifts and stops. Theirs is what the RDP does
                        # and ours is up to 7/255 dark -- but nothing the
                        # port ships holds eight bits of a channel, so
                        # every one of these has to come back the same
                        # texel at both of the widths that do.
                        if any((x >> 3) != (y >> 3) or (x >> 4) != (y >> 4)
                               for x, y in ((r, tr), (g, tg), (bl, tb))):
                            print(f"FAIL decode: {where} frame {f} texel "
                                  f"{k}: ours {(r, g, bl, al)}, the "
                                  f"decomp's tool {(tr, tg, tb, ta)}")
                            return 1
                        n_expand += 1
                    if al != ta:
                        if a["fmt"] != FMT_I or ta != 0xFF or al != r:
                            print(f"FAIL decode: {where} frame {f} texel "
                                  f"{k}: alpha {al} against {ta}, and that "
                                  f"is not the I-format narrowing")
                            return 1
                        n_alpha += 1
                    n_texel += 1

                # pack: the file read back, untwiddled, against the same
                # texels quantised the way its header says they were
                off = texels_off + offs[first_frame + f]
                got = untwiddle_16bpp(pack[off:off + pw * ph * 2], pw, ph)
                padded = [ours[min(y, sh - 1) * sw + min(x, sw - 1)]
                          for y in range(ph) for x in range(pw)]
                enc = _pvr_argb1555 if fmt == 1 else _pvr_argb4444
                want = [enc(t) for t in padded]
                if got != want:
                    k = next(j for j in range(len(want)) if got[j] != want[j])
                    print(f"FAIL pack: {where} frame {f} texel {k}: "
                          f"{got[k]:#06x}, wanted {want[k]:#06x}")
                    return 1
                n_frame += 1

    print(f"parse: {n_tex} textures over {len(banks)} banks, the same walk "
          f"as ssb-decomp-re/tools/extractParticleTextures.py")
    print(f"decode: {n_texel} texels against that tool's decode_image; "
          f"{n_alpha} differ in alpha alone and all of them are I4/I8, "
          f"where the RDP replicates intensity and a PNG writer does not; "
          f"{n_expand} differ by the 5-bit expansion's low bits and every "
          f"one of them quantises to the same ARGB1555 and ARGB4444 texel")
    print(f"pack: {n_frame} frames untwiddle to those texels; "
          f"{fmts.get(1, 0)} ARGB1555, {fmts.get(2, 0)} ARGB4444, "
          f"{n_pad} textures padded up to the PVR's power of two")
    print("particletex_check: the packs are the ROM's images, in the PVR's "
          "own layout")
    return 0


if __name__ == "__main__":
    sys.exit(main())

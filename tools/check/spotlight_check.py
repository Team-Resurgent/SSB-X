#!/usr/bin/env python3
"""ssb64-dc: the character select's spotlight mask, end to end.

The spotlight is one quad, and everything that makes it a spotlight
rather than a white square is one 32x32 I8 tile that the RDP reads in the
*alpha* cycle alone: colour is SHADE, alpha is TEXEL0 * PRIMITIVE
(relocData 22's display list). Three things in tools/lib/ssb_assets.py had to
agree for that to survive baking -- an I texel's alpha is its intensity,
a tile the colour cycle never reads is still interned, and such a tile
ships with white RGB so the PVR's MODULATEALPHA is the identity on
colour -- and none of them is visible in the pack except as texels.

So this check walks the pack the exporter produces back to the intensity
bytes in the ROM. It takes the tile's raw bytes from the extractor's own
annotation in the decomp source (`@tex fmt=I8 dim=32x32` over
dMNPlayersSpotlight_Tex_0x0008), never from the display-list replay that
produced the pack, and untwiddles and unpacks what shipped:

  * every texel's alpha is the I8 byte at the same place;
  * every texel's colour is white;
  * the one batch is translucent, says its alpha reads the texel and not
    the vertex, and names an MObj whose flags are MOBJ_FLAG_PRIMCOLOR --
    read out of the ROM's own MObjSub, not the pack's.

Usage: python3 tools/check/spotlight_check.py [--rom <rom.z64>]
"""
import os
import re
import struct
import sys

sys.path[:0] = [os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), d)
                for d in ("lib", "export", "check")]
import ssb_extract              # noqa: E402
import ssb_assets as A          # noqa: E402
import ssb_meshexport as M      # noqa: E402
import ssb_spotexport as S      # noqa: E402

# fighter.h FPackTex.fmt and FPackBatch's two bit sets.
FPACK_TEX_ARGB1555 = 1
FPACK_TEX_ARGB4444 = 2
FPACK_LIST_TR = 2
FPACK_LIST_MASK = 3
FPACK_ALPHA_TEX = 1
FPACK_ALPHA_SHADE = 2
MOBJ_FLAG_PRIMCOLOR = A.MOBJ_FLAG_PRIMCOLOR

HDR = "<8s8I8I3ff8s8I"
BATCH = "<HHhBBHHIIII"
TEX = "<IIHHBBBB"


def tile_from_source(src, f):
    """The tile's raw I8 bytes, off the extractor's annotation.

    The block is written as `/* @tex fmt=I8 dim=32x32 */` immediately
    above `u8 dMNPlayersSpotlight_Tex_0xNNNN[N]`, and the offset in the
    symbol is where it sits in the file. Nothing here replays a display
    list, which is the point: this is the other way round to the pack.
    """
    m = re.search(r"/\* @tex fmt=(\w+) dim=(\d+)x(\d+) \*/\s*\n"
                  r"u8 dMNPlayersSpotlight_Tex_0x([0-9A-Fa-f]+)\[(\d+)\]",
                  src)
    if m is None:
        sys.exit("spotlight_check: no annotated tile in the source")
    fmt, w, h, off, n = (m.group(1), int(m.group(2)), int(m.group(3)),
                         int(m.group(4), 16), int(m.group(5)))
    if fmt != "I8" or w * h != n:
        sys.exit("spotlight_check: the tile is %s %dx%d in %d bytes"
                 % (fmt, w, h, n))
    return w, h, f[off:off + n]


def _morton(x, y):
    """PVR twiddled order: y in the even bits, x in the odd ones.

    Written out here rather than imported so that the two sides of this
    check do not share the arithmetic they are checking.
    """
    n = 0
    for b in range(16):
        n |= ((y >> b) & 1) << (2 * b)
        n |= ((x >> b) & 1) << (2 * b + 1)
    return n


def untwiddle(blob, w, h):
    """The inverse of ssb_texconv.twiddle_16bpp, as a flat texel list."""
    out = [0] * (w * h)
    minv = min(w, h)
    mask = minv - 1
    for y in range(h):
        for x in range(w):
            idx = _morton(x & mask, y & mask) + \
                (x // minv + y // minv) * minv * minv
            out[y * w + x] = struct.unpack_from("<H", blob, idx * 2)[0]
    return out


def unpack16(word, fmt):
    """One PVR 16-bit texel -> (r, g, b, a), 8 bits a channel."""
    if fmt == FPACK_TEX_ARGB1555:
        a = 0xFF if (word >> 15) else 0
        return (((word >> 10) & 0x1F) << 3, ((word >> 5) & 0x1F) << 3,
                (word & 0x1F) << 3, a)
    if fmt == FPACK_TEX_ARGB4444:
        n = [(word >> s) & 0xF for s in (8, 4, 0, 12)]
        return tuple((v << 4) | v for v in n)
    sys.exit("spotlight_check: pack texture format %d is not 16-bit" % fmt)


def main():
    argv = sys.argv[1:]
    rom_path = M.ROM_DEFAULT
    if "--rom" in argv:
        rom_path = argv[argv.index("--rom") + 1]
    rom = open(rom_path, "rb").read()

    fid, src = S.find_file()
    f = A.get_file(rom, fid, ssb_extract)
    entry = ssb_extract.read_entry(rom, ssb_extract.RELOC_SEG, fid)
    reloc = A.walk_reloc(f, entry["reloc_intern"])
    tile_w, tile_h, raw = tile_from_source(src, f)

    blob = S.pack_spotlight(rom)
    hd = struct.unpack_from(HDR, blob, 0)
    (batch_count, tex_count) = (hd[4], hd[5])
    (off_batches, off_texs, off_texdata) = (hd[12], hd[13], hd[15])
    bad = []

    if tex_count != 1 or batch_count != 1:
        sys.exit("spotlight_check: %d textures and %d batches, expected one "
                 "of each" % (tex_count, batch_count))

    # -- the mask ------------------------------------------------------
    off, size, w, h, _pal, _clamp, fmt, _pad = struct.unpack_from(
        TEX, blob, off_texs)
    if (w, h) != (tile_w, tile_h):
        bad.append("the pack's tile is %dx%d, the ROM's %dx%d"
                   % (w, h, tile_w, tile_h))
    else:
        texels = untwiddle(blob[off_texdata + off:off_texdata + off + size],
                           w, h)
        for k, word in enumerate(texels):
            r, g, b, a = unpack16(word, fmt)
            want = raw[k]
            # ARGB4444 keeps the top four bits of each channel, so the
            # comparison is at that precision -- and it is the whole
            # precision the texel has.
            if (a >> 4) != (want >> 4):
                bad.append("texel %d alpha 0x%02X, intensity 0x%02X"
                           % (k, a, want))
            # White at the format's own precision: 0xFF out of
            # ARGB4444's four bits, 0xF8 out of ARGB1555's five.
            if not (r == g == b and r >= 0xF8):
                bad.append("texel %d colour (%d,%d,%d), not white"
                           % (k, r, g, b))
            if len(bad) > 4:
                break

    # -- the batch -----------------------------------------------------
    (_tf, _tc, tex, _shaded, _joint, bucket,
     alpha_src, _p, _l1, _l2, _env) = struct.unpack_from(BATCH, blob,
                                                          off_batches)
    if tex != 0:
        bad.append("the batch's texture is %d, not the mask" % tex)
    if (bucket & FPACK_LIST_MASK) != FPACK_LIST_TR:
        bad.append("the batch is in PVR list %d, not the translucent one"
                   % (bucket & FPACK_LIST_MASK))
    if not (alpha_src & FPACK_ALPHA_TEX):
        bad.append("the batch does not read the texel for alpha")
    if alpha_src & FPACK_ALPHA_SHADE:
        bad.append("the batch reads the vertex for alpha; the RDP does not")

    # -- the material, out of the ROM ----------------------------------
    subs_off = S.symbol_offsets(src, "MObjSub")[0]
    nodes = A.read_dobj_tree(f, reloc, S.symbol_offsets(src, "DObjDesc")[0],
                             dl_links=True)[1]
    subs = [s for lst in A.read_mobjsubs(f, reloc, subs_off, len(nodes))
            for s in lst]
    if len(subs) != 1:
        bad.append("%d MObjSubs in the ROM, expected one" % len(subs))
    elif subs[0]["flags"] != MOBJ_FLAG_PRIMCOLOR:
        bad.append("the ROM's MObjSub has flags 0x%04X, not PRIMCOLOR alone"
                   % subs[0]["flags"])

    for line in bad:
        print("MISMATCH " + line)
    if bad:
        print("spotlight_check: FAILED")
        return 1
    print("spotlight_check: the %dx%d I8 mask survives the bake -- every "
          "texel's alpha is its intensity and every texel is white, in one "
          "translucent batch whose alpha reads the texel"
          % (tile_w, tile_h))
    return 0


if __name__ == "__main__":
    sys.exit(main())

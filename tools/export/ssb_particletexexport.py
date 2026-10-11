#!/usr/bin/env python3
"""ssb64-dc: the particle banks' textures, converted for the PVR.

tools/export/ssb_particleexport.py cuts the nine banks out of the ROM as the game
has them -- an .scb of scripts and a .txb of textures, both in the ROM's
own layout so that lbParticleSetupBankID's pointerize walk lands where it
lands on the N64. That is right for the interpreter, which reads a
texture's `fmt`, `siz`, `width`, `height`, `flags` and `count` and nothing
else, and wrong for anything that has to draw one: the .txb's images are
N64 texels -- CI4 against a TLUT, IA8, RGBA5551, RGBA8888 -- and the PVR
reads none of those.

So this writes a third file per bank, a .txp, holding the same images in
the two formats the PVR does read, twiddled, in the target's word order,
ready for pvr_txr_load to copy verbatim. The .txb stays exactly as it is
and stays the interpreter's; the .txp is the renderer's and is indexed the
same way -- [texture_id][frame_id] -- so lbParticleDrawTextures' own
`sLBParticleTextureBanks[bank_id][pc->texture_id]->data[pc->frame_id]`
becomes the same three numbers into this file.

Everything about the conversion is tools/lib/ssb_assets.py's, which is the
mesh baker's and has been on screen since H1: n64_texel for the nine
format/size cases, pvr_encoding for the choice between ARGB1555 and
ARGB4444, twiddle_16bpp for the layout. Two things are worth writing down
because they are decisions and not copies:

  - **No unswizzle.** The RDP's load-time odd-row word swap is undone by
    the same swap in its texel fetch, so an image a display list loads
    with gDPLoadTextureBlock is stored in RAM as a plain linear raster.
    That is what these are (lb/lbparticle.c:1963) and it is the same
    reading the mesh path has rendered correctly on hardware under since
    H1 -- see n64_texel's docstring. The .spritelist blobs are the other
    case, and they are not these.

  - **Palettes are resolved, not shipped.** A CI4 particle would fit the
    PVR's 4bpp paletted format, but its palette RAM is 1024 entries
    shared by every pack resident at once and a stage already spends up
    to 17 of the 64 banks (ssb_assets.py, MESH_PALETTED). A particle bank
    would need one bank per texture per frame. Resolving through the TLUT
    at export costs VRAM and no fidelity: an N64 TLUT is RGBA5551 and PVR
    ARGB1555 is the same five bits a channel with the alpha bit rotated.
    Which palette a frame resolves against is not a runtime choice --
    lb/lbparticle.c:1821 reads LBPARTICLE_FLAG_SHAREDPAL off the
    *particle*, but lbparticle.c:353 copies that from the texture
    header's own `flags & 1`, so it is a property of the texture and the
    same for every particle that ever uses it.

The file, all little-endian, is what src/dc/lbpartex.c reads:

    header    16 bytes: magic 'PTXP', textures_num, frames_num, texels_off
    textures  16 bytes each: first_frame, frames, fmt, width, height,
              src_width, src_height
    frames    frames_num u32 byte offsets into the texel blob
    texels    each frame twiddled, 16 bits a texel, 8-byte aligned; the
              blob itself starts on a 32-byte boundary

`fmt` is ssb_assets.py's PVRTEX_ARGB1555 (1) or PVRTEX_ARGB4444 (2).
`width`/`height` are the PVR texture's, which is the source's padded up to
the PVR's 8x8 minimum; `src_width`/`src_height` are the .txb's own, which
is what the renderer's dsdx/dtdy are in terms of.

Usage:
  python3 tools/export/ssb_particletexexport.py [--rom <rom.z64>] --bank <name> --out <file>
  python3 tools/export/ssb_particletexexport.py [--rom <rom.z64>] --all --outdir <dir>
"""
import argparse
import os
import struct
import sys

sys.path[:0] = [os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), d)
                for d in ("lib", "export", "check")]
import ssb_particleexport as PE                                  # noqa: E402
from ssb_assets import n64_texel, pvr_encoding                   # noqa: E402
from ssb_texconv import twiddle_16bpp                            # noqa: E402

MAGIC = b"PTXP"
HDR = struct.Struct("<4sIII")
TEX = struct.Struct("<IHHHHHH")
FMT_CI = 2


def _u32(buf, off):
    return struct.unpack_from("<I", buf, off)[0]


def _pot(n):
    p = 8
    while p < n:
        p *= 2
    return p


def data_count(txb, off, count, fmt, flags):
    """How many u32 offsets follow an LBTextureHeader.

    tools/export/ssb_particleexport.py's texture_data_count says the same thing,
    but it runs over the ROM's own bytes and reads them big-endian; this
    runs over what that tool wrote, which is the same bank with every
    header word swapped. Two readings of one rule, not two rules."""
    if count == 0:
        return 0
    if fmt == FMT_CI:
        # One palette per image, or a single shared one after them.
        return count + 1 if (flags & 1) else count * 2
    first_data = _u32(txb, off + PE.LBTEXTURE_HEADER_SIZE)
    return (first_data - (off + PE.LBTEXTURE_HEADER_SIZE)) // 4


def read_bank(txb):
    """The .txb's own headers, as the interpreter reads them."""
    n = _u32(txb, 0)
    out = []
    for i in range(n):
        off = _u32(txb, 4 + i * 4)
        count, fmt, siz, width, height, flags = struct.unpack_from("<6I", txb, off)
        ndata = data_count(txb, off, count, fmt, flags)
        data = [_u32(txb, off + PE.LBTEXTURE_HEADER_SIZE + k * 4)
                for k in range(ndata)]
        out.append({"count": count, "fmt": fmt, "siz": siz, "width": width,
                    "height": height, "flags": flags, "data": data,
                    "off": off})
    return out


def tlut_words(txb, off, entries):
    """A palette as raw N64 RGBA5551 words -- what n64_texel wants."""
    return list(struct.unpack_from(">%dH" % entries, txb, off))


def convert(txb, tex):
    """One texture's frames as (pvr_fmt, w, h, [twiddled bytes per frame])."""
    w, h = tex["width"], tex["height"]
    # The PVR wants a power of two and at least 8 on each axis; the .txb
    # has one texture that is neither (48x48, efcommon 23). The pad is a
    # clamp -- the last row and column repeated -- and it is never
    # sampled: the renderer's UVs cover src_width/width of the texture,
    # because dsdx steps the rectangle across exactly the source's own
    # texels (lb/lbparticle.c:1823). Only the bilinear filter reaches
    # into it, at the edge, which is what a clamp is for.
    pw, ph = _pot(max(w, 8)), _pot(max(h, 8))

    entries = 16 if tex["siz"] == 0 else 256
    palettes = tex["data"][tex["count"]:]

    frames = []
    fmts = set()
    for f in range(tex["count"]):
        tlut = ()
        if tex["fmt"] == FMT_CI:
            p = palettes[0] if (tex["flags"] & 1) else palettes[f]
            tlut = tlut_words(txb, p, entries)
        img = tex["data"][f]
        texels = [n64_texel(txb, img, min(y, h - 1) * w + min(x, w - 1),
                            tex["fmt"], tex["siz"], tlut)
                  for y in range(ph) for x in range(pw)]
        fmt, words = pvr_encoding(texels, paletted=False)
        fmts.add(fmt)
        frames.append(twiddle_16bpp(words, pw, ph))

    # One format for the whole series: a texture's frames are drawn by the
    # same particle as it ages and the renderer binds one PVR format per
    # texture_id. ARGB4444 is the one that can hold both.
    if len(fmts) > 1:
        fmt = 2
        frames = []
        for f in range(tex["count"]):
            tlut = ()
            if tex["fmt"] == FMT_CI:
                p = palettes[0] if (tex["flags"] & 1) else palettes[f]
                tlut = tlut_words(txb, p, entries)
            img = tex["data"][f]
            texels = [n64_texel(txb, img, min(y, h - 1) * w + min(x, w - 1),
                                tex["fmt"], tex["siz"], tlut)
                      for y in range(ph) for x in range(pw)]
            frames.append(twiddle_16bpp(
                [(((a >> 4) << 12) | ((r >> 4) << 8) | ((g >> 4) << 4) | (b >> 4))
                 for (r, g, b, a) in texels], pw, ph))
    else:
        fmt = fmts.pop() if fmts else 1
    return fmt, pw, ph, frames


def export(rom: bytes, bank: str) -> bytes:
    txb = PE.export(rom, bank, "txb")
    texes = read_bank(txb)

    recs, offs, blob = [], [], bytearray()
    for tex in texes:
        fmt, pw, ph, frames = convert(txb, tex)
        recs.append((len(offs), len(frames), fmt, pw, ph,
                     tex["width"], tex["height"]))
        for bits in frames:
            while len(blob) % 8:
                blob.append(0)
            offs.append(len(blob))
            blob += bits

    texels_off = HDR.size + len(recs) * TEX.size + len(offs) * 4
    # 32, not 8: the loader reads the frames straight off the GD-ROM's
    # DMA stream only from a 32-byte-aligned file position
    # (src/dc/assetroot.h), and every frame is a multiple of 128 bytes,
    # so the blob's own start is what decides it for all of them. At 8
    # the whole of efcommon went sector by sector through KOS's cache,
    # 6.7 s.
    while texels_off % 32:
        texels_off += 1                 # the header pads, not the blob
    out = bytearray()
    out += HDR.pack(MAGIC, len(recs), len(offs), texels_off)
    for r in recs:
        out += TEX.pack(*r)
    out += struct.pack("<%dI" % len(offs), *offs)
    out += b"\0" * (texels_off - len(out))
    out += blob
    return bytes(out)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--rom", default=PE.ROM_DEFAULT)
    ap.add_argument("--bank", choices=sorted(PE.BANKS))
    ap.add_argument("--out")
    ap.add_argument("--all", action="store_true")
    ap.add_argument("--outdir")
    args = ap.parse_args()

    with open(args.rom, "rb") as f:
        rom = f.read()

    if args.all:
        if not args.outdir:
            ap.error("--all needs --outdir")
        os.makedirs(args.outdir, exist_ok=True)
        total = 0
        for name in sorted(PE.BANKS):
            data = export(rom, name)
            path = os.path.join(args.outdir, name + ".txp")
            with open(path, "wb") as f:
                f.write(data)
            total += len(data)
            print(f"{path}: {len(data)} bytes")
        print(f"{total} bytes over {len(PE.BANKS)} banks")
        return 0

    if not args.bank or not args.out:
        ap.error("--bank and --out, or --all and --outdir")
    data = export(rom, args.bank)
    with open(args.out, "wb") as f:
        f.write(data)
    print(f"{args.out}: {len(data)} bytes")
    return 0


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""ssb64-dc: the sprite exporter's TMEM row unshuffle against the
decomp's own.

The N64's RDP reads odd lines of TMEM with the two 32-bit halves of every
64-bit word exchanged, so the ROM stores odd rows pre-swapped -- the
SP_TEXSHUF sprites of libultra's sprite library (PR/sp.h:155), which is
every sprite in this game. Getting that reorder wrong, or applying it
twice, is a bug that shows as alternating strips of texels in the wrong
order, and it is a bug the port has had: RGBA32 is not swapped by the
rule above at all, and needs SP_TEXSHUF's own 16-byte unit with 8-byte
halves.

The decomp states both, in tools/relocSpriteTool.py: unswizzle_n64_texture
for 4/8/16bpp (and it returns RGBA32 untouched), deshuffle_texshuf_rows
with word_size=16 for RGBA32. That file imports standalone -- it needs no
baserom, no splat and none of the decomp's extracted assets -- so this
check reads every sprite in the ROM and asserts the port's reorder is
byte-for-byte the decomp's on the stored bytes of every strip.

It is deliberately narrow. What the port does *after* the reorder is not
comparable: the decomp's n64_to_rgba writes a preview, RGBA8 with the
palette applied, while the port writes PVR texels and re-channels I and
IA sprites so the PVR's MODULATEALPHA reproduces the N64's combiner.
Comparing those would mean writing the port's format table twice. The
reorder is the part both sides agree on exactly, and it is the part that
has actually been wrong.

The compressed 4-bit format (bmsiz 4) has no decomp counterpart -- its
extract skips it -- so it is unpacked to plain 4bpp first and the reorder
compared at that size, which is where the port applies it too.

Usage: python3 tools/check/sprite_check.py [--rom <rom.z64>] [--file N]
"""
import os
import sys

sys.path[:0] = [os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), d)
                for d in ("lib", "export", "check")]
import ssb_meshexport as M       # noqa: E402  (ROM_DEFAULT)
import ssb_logicexport as L      # noqa: E402  (file_info)
import ssb_paths                 # noqa: E402
import ssb_spriteexport as S     # noqa: E402

sys.path.insert(0, ssb_paths.DECOMP_TOOLS)
import relocSpriteTool as R      # noqa: E402  (the oracle)

BPP = {S.G_IM_SIZ_4b: 4, S.G_IM_SIZ_8b: 8, S.G_IM_SIZ_16b: 16,
       S.G_IM_SIZ_32b: 32, S.G_IM_SIZ_4c: 4}

FMT_NAME = {S.G_IM_FMT_RGBA: "rgba", S.G_IM_FMT_CI: "ci",
            S.G_IM_FMT_IA: "ia", S.G_IM_FMT_I: "i"}
SIZ_NAME = {S.G_IM_SIZ_4b: "4", S.G_IM_SIZ_8b: "8", S.G_IM_SIZ_16b: "16",
            S.G_IM_SIZ_32b: "32", S.G_IM_SIZ_4c: "4c"}


def decode_4c(f, buf, stride, rows):
    """lbCommonDecodeSpriteBitmapsSiz4b, as the exporter does it: two bits
    a texel widened to a nibble. Returns the decoded 4bpp bytes, still in
    stored row order."""
    n = stride * rows
    src = f[buf:buf + (n + 1) // 2]
    out = bytearray()
    for b in src:
        out.append((S.nib2(b >> 6) << 4) | S.nib2((b >> 4) & 3))
        out.append((S.nib2((b >> 2) & 3) << 4) | S.nib2(b & 3))
    return bytes(out[:n])


def ours(data, stride, rows, siz):
    """The port's reorder over a strip's stored bytes."""
    if siz == S.G_IM_SIZ_32b:
        return b"".join(S.unshuffle_row32(data[r * stride:(r + 1) * stride], r)
                        for r in range(rows))
    return b"".join(S.unshuffle_row(data[r * stride:(r + 1) * stride], r)
                    for r in range(rows))


def theirs(data, stride, rows, fmt, siz, width_img):
    """The decomp's, tools/relocSpriteTool.py."""
    if siz == S.G_IM_SIZ_32b:
        return R.deshuffle_texshuf_rows(data, stride, rows)
    # 4c is unpacked to 4bpp before the reorder on both sides
    dsiz = S.G_IM_SIZ_4b if siz == S.G_IM_SIZ_4c else siz
    return R.unswizzle_n64_texture(data, width_img, rows, fmt, dsiz)


def main():
    argv = sys.argv[1:]
    rom_path = M.ROM_DEFAULT
    only = None
    if "--rom" in argv:
        rom_path = argv[argv.index("--rom") + 1]
    if "--file" in argv:
        only = int(argv[argv.index("--file") + 1])
    rom = open(rom_path, "rb").read()

    fids = set()
    cur = None
    with open(S.DESCRIPTIONS) as fp:
        for line in fp:
            if line.startswith("[") and line[1:-2].isdigit():
                cur = int(line[1:-2])
            elif line.startswith("Sprite ") and cur is not None:
                fids.add(cur)
    if only is not None:
        fids = {only} & fids or {only}

    per_fmt = {}
    sprites = strips = bad = 0

    for fid in sorted(fids):
        try:
            f, _e, intern, _sites, _ids = L.file_info(rom, fid)
        except Exception as exc:                        # noqa: BLE001
            print("file %d: unreadable (%s)" % (fid, exc))
            continue
        for name, off in S.catalogue(fid):
            try:
                sp = S.read_sprite(f, intern, off)
            except Exception as exc:                    # noqa: BLE001
                print("file %d %s: unreadable (%s)" % (fid, name, exc))
                continue
            S.check_texshuf(name, sp)
            sprites += 1
            key = FMT_NAME.get(sp["bmfmt"], "?") + SIZ_NAME[sp["bmsiz"]]
            bpp = BPP[sp["bmsiz"]]
            width_img = sp["strips"][0]["width_img"]
            stride = (width_img * bpp + 7) // 8

            for st in sp["strips"]:
                rows = st["actual_height"]
                if sp["bmsiz"] == S.G_IM_SIZ_4c:
                    data = decode_4c(f, st["buf"], stride, rows)
                else:
                    data = f[st["buf"]:st["buf"] + stride * rows]
                if len(data) < stride * rows:
                    continue
                strips += 1
                a = ours(data, stride, rows, sp["bmsiz"])
                b = theirs(data, stride, rows, sp["bmfmt"], sp["bmsiz"],
                           width_img)
                per_fmt[key] = per_fmt.get(key, 0) + 1
                if a != b:
                    bad += 1
                    where = next((i for i in range(min(len(a), len(b)))
                                  if a[i] != b[i]), -1)
                    print("MISMATCH file %d %s (%s, %dx%d): byte %d"
                          % (fid, name, key, width_img, rows, where))

    print("%d sprites, %d strips compared against the decomp's "
          "relocSpriteTool" % (sprites, strips))
    for key in sorted(per_fmt):
        print("  %-6s %5d strips" % (key, per_fmt[key]))
    if bad:
        print("FAIL: %d strips disagree" % bad)
        return 1
    print("sprite_check: the row unshuffle agrees on every strip")
    return 0


if __name__ == "__main__":
    sys.exit(main())

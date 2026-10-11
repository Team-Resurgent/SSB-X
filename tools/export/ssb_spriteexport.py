#!/usr/bin/env python3
"""ssb64-dc: one relocData file's sprites -> a sprite bank, converted at
build time.

A sprite in this game is a libultra Sprite (PR/sp.h) whose `bitmap` array
is a column of horizontal strips, each Bitmap a run of texel rows the
RDP can load in one gDPLoadBlock. The game draws one texture rectangle
per strip (lb/lbcommon.c lbCommonPrepSObjDraw -> lbCommonDrawSObjBitmap),
stacking them down the screen: every strip but the last contributes
`bmheight` rows, the last contributes its own `actualHeight`, and the
extra row a strip stores beyond bmheight is the next strip's first row
again, so the RDP's bilinear filter does not show a seam at the join.

The bank stitches each sprite's strips back into the one image they are,
padded to a power of two for the PVR with the edge row and column
replicated (which is what G_TX_CLAMP samples past the edge), converts
the texels to a PVR 16-bit format, and keeps every Sprite and Bitmap
field the game reads -- position, size, scale, attr, colour, bmheight,
bmHreal, format, and per strip its width and the row it starts on -- so
the port's lbCommon sprite code runs the game's arithmetic unchanged and
only the last step, the rectangle, is the PVR's.

Formats, by count across the ROM's 1178 sprites (tools/relocFile
Descriptions.us.txt): I4 495, IA8 313, I4 compressed 108, RGBA32 99,
CI4 67, RGBA16 60, IA4 18, CI8 9, I8 6, IA16 3. RGBA16 and the two CI
formats become ARGB1555 (their palettes are RGBA5551 too); everything
else becomes ARGB4444. I and IA textures carry the intensity in RGB and
the alpha in A, so the PVR's MODULATEALPHA texture environment times a
vertex colour is the game's combiner: PRIM * I for I sprites,
lerp(ENV, PRIM, I) for IA through the offset colour. The compressed 4-bit
format (bmsiz 4, "i4c" in the decomp's names) is what
lbCommonDecodeSpriteBitmapsSiz4b unpacks at load time on the N64: two
bits per texel, each expanded to a nibble through {0, 5, A, F}. It is
unpacked here instead and stored as plain I4.

The strips are stored the way the RDP's texture memory wants them, not
the way a bitmap is usually written: lbCommonDrawSObjBitmap loads each
one with gDPLoadBlock and dxt = 0, which copies the bytes into TMEM
without the odd-line word swap the loader normally applies, and TMEM
reads odd lines with the two 32-bit halves of every 64-bit word
exchanged. So the ROM holds odd rows pre-swapped (the SP_TEXSHUF sprites
of libultra's sprite library, which is every sprite in this game), and
unshuffle_row puts them back before the texels are read. Row parity is
per strip: every strip is loaded at TMEM address 0. RGBA32 is the
exception the rule does not cover -- SP_TEXSHUF's own unit, 16 bytes
with 8-byte halves -- and unshuffle_row32 is that one.

Both reorders are the decomp's, in tools/relocSpriteTool.py
(unswizzle_n64_texture and deshuffle_texshuf_rows); tools/check/sprite_check.py
imports that module and asserts this file agrees with it on every strip
of every sprite in the ROM. What is not shared is what comes after: the
decomp writes an RGBA8 preview with the palette applied, and this writes
PVR texels with I and IA re-channelled so the PVR's MODULATEALPHA is the
N64's combiner.

Sprite offsets within a file come from the decomp's own catalogue,
tools/relocFileDescriptions.us.txt, which lists every `Sprite Name
0xOFFSET` in every file; the offset is the one the game's tables carry
(mn/mncommon/mntitle.c dMNTitleCommonSpriteDescs[].offset is
&llMNTitleSmashSprite, which is that number), so it is also the key the
port's sprite_bank_get looks a sprite up by.

A CI sprite whose palette the game swaps at runtime -- the stock icon,
whose FTSprites.stock_luts names one palette per costume -- is stored
once per palette: the record's `nluts` counts the textures, laid end to
end at tex_off, and the port hands the game an int*[] of them for
Sprite.LUT to point into. On the PVR the palette is the texture.

Usage: python3 tools/export/ssb_spriteexport.py --file 167 --out romdisk/mntitle.spr
                                         [--rom <rom.z64>] [--only A,B,C]
                                         [--luts A=LutX,LutY;B=LutZ]
       python3 tools/export/ssb_spriteexport.py --file 167 --list
       python3 tools/export/ssb_spriteexport.py --fighter Mario --out romdisk/ftmario.spr
           the fighter's Stock and FTEmblem sprites out of its model file,
           the stock icon once per costume palette (its main file's
           stock_luts, read from the decomp's relocData source)
"""
import os
import re
import struct
import sys

sys.path[:0] = [os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), d)
                for d in ("lib", "export", "check")]
import ssb_meshexport as M       # noqa: E402  (ROM_DEFAULT)
import ssb_logicexport as L      # noqa: E402  (file_info)
import ssb_paths                 # noqa: E402

DESCRIPTIONS = os.path.join(ssb_paths.DECOMP_TOOLS,
                            "relocFileDescriptions.us.txt")

# PR/gbi.h image formats and sizes
G_IM_FMT_RGBA, G_IM_FMT_YUV, G_IM_FMT_CI, G_IM_FMT_IA, G_IM_FMT_I = range(5)
G_IM_SIZ_4b, G_IM_SIZ_8b, G_IM_SIZ_16b, G_IM_SIZ_32b, G_IM_SIZ_4c = range(5)

PVRFMT_ARGB1555 = 0
PVRFMT_ARGB4444 = 1

SPRITE_RECORD = "<16sI4h2f2hHh4B2h6h4B4H3I"   # see sprite.h SprRecord
BITMAP_RECORD = "<4hI2h"                       # see sprite.h SprBitmap
SPRITE_RECORD_SIZE = struct.calcsize(SPRITE_RECORD)
BITMAP_RECORD_SIZE = struct.calcsize(BITMAP_RECORD)
assert SPRITE_RECORD_SIZE == 88 and BITMAP_RECORD_SIZE == 16


def catalogue(fid, kind="Sprite"):
    """[(name, offset)] for one relocData file, from the decomp's list:
    its sprites, or with kind="LUT" its palettes."""
    out = []
    cur = None
    with open(DESCRIPTIONS) as fp:
        for line in fp:
            m = re.match(r"\[(\d+)\]", line)
            if m:
                cur = int(m.group(1))
                continue
            m = re.match(kind + r" (\S+) (0x[0-9A-Fa-f]+)", line)
            if m and cur == fid:
                name = m.group(1)
                off = int(m.group(2), 16)
                if name == "-":
                    name = "%s_%05x" % (kind.lower(), off)
                out.append((name, off))
    return out


def lut_frames_of(fid, spec):
    """--luts Sprite=LutA,LutB,...: the palettes, by their names in the
    decomp's list, that the game points a sprite's LUT at
    (mn/mnplayers/mnplayersvs.c mnPlayersVSSetGateLUT: the gate card in
    each player's colour). The sprite's own palette leads the frames
    whether or not it is named.

    The decomp's own description generator spells this two ways -- "LUT"
    for the gate card's file, "Palette" for mn/mnoption/mnbackupclear's
    (Yes/No's highlight and confirm-flash palettes) -- so both are read
    and merged; a name present under one spelling and not the other is
    still found.

    A name spelt <file>:<Lut> is a palette from ANOTHER file, and its
    frame is (file, offset) rather than an offset: the three 1P selects
    draw file 23's red card (82 wide) in the player colours of file 17's
    GateMan palettes (mn/mnplayers/mnplayers1pgame.c
    mnPlayers1PGameSetGateLUT). On the PVR the palette is the texture, so
    that is file 23's card baked in file 17's palette -- file 17's own
    RedCard in the same palette is the VS card, 66 wide."""
    def names_of(f):
        luts = dict(catalogue(f, "LUT"))
        luts.update(catalogue(f, "Palette"))
        return luts
    luts = {fid: names_of(fid)}
    out = {}
    for item in spec.split(";"):
        name, _, names = item.partition("=")
        frames = []
        for n in names.split(","):
            src, sep, lname = n.rpartition(":")
            src = int(src) if sep else fid
            if src not in luts:
                luts[src] = names_of(src)
            if lname not in luts[src]:
                sys.exit("file %d: no LUT named %s" % (src, lname))
            off = luts[src][lname]
            frames.append(off if src == fid else (src, off))
        out[name] = frames
    return out


def rotate5551(t):
    """N64 RGBA5551 (alpha in bit 0) -> PVR ARGB1555 (alpha in bit 15)."""
    return ((t & 1) << 15) | (t >> 1)


def nib2(v):
    """lbCommonGetBitmapDecodeNibble: a 2-bit texel to a 4-bit one."""
    return (0x0, 0x5, 0xA, 0xF)[v]


def unshuffle_row(line, r):
    """TMEM's odd-line swap, undone: on an odd row every 8-byte word has
    its two 4-byte halves exchanged. The decomp's own tooling does the
    same in tools/relocSpriteTool.py unswizzle_n64_texture, and skips it
    for RGBA32 -- see the 32-bit branch of decode_rows."""
    if not (r & 1):
        return line
    out = bytearray(len(line))
    n = len(line) & ~7
    out[0:n:8] = line[4:n:8]
    out[1:n:8] = line[5:n:8]
    out[2:n:8] = line[6:n:8]
    out[3:n:8] = line[7:n:8]
    out[4:n:8] = line[0:n:8]
    out[5:n:8] = line[1:n:8]
    out[6:n:8] = line[2:n:8]
    out[7:n:8] = line[3:n:8]
    out[n:] = line[n:]
    return bytes(out)


def unshuffle_row32(raw, r):
    """The same for RGBA32, whose unit is 16 bytes with 8-byte halves --
    see the 32-bit branch of decode_rows. The decomp's is
    relocSpriteTool.py deshuffle_texshuf_rows (word_size=16)."""
    if not (r & 1):
        return raw
    out = bytearray(raw)
    n = len(raw) & ~15
    for k in range(8):
        out[k:n:16] = raw[8 + k:n:16]
        out[8 + k:n:16] = raw[k:n:16]
    return bytes(out)


def strip_rows(f, buf, stride, rows):
    """A strip's stored rows, each `stride` bytes, unshuffled."""
    return [unshuffle_row(f[buf + r * stride:buf + (r + 1) * stride], r)
            for r in range(rows)]


def decode_rows(f, fmt, siz, buf, width, width_img, rows, lut):
    """One strip's stored rows as lists of PVR 16-bit words, `width_img`
    texels wide -- the whole stored row, which is what the game's
    gDPLoadBlock puts in TMEM (lb/lbcommon.c:2316): a sprite drawn tiled
    (SObj.masks) wraps over the row's padding, and a sprite drawn plain
    bilerps into it at its edge, so the texture keeps it. `width` is the
    sprite's drawn width and is not used here. `lut` is the sprite's
    palette as ARGB1555 words, for CI. Returns (pvrfmt, rows)."""
    del width
    out = []
    if siz == G_IM_SIZ_4c:
        # lbCommonDecodeBitmapSiz4b, forwards: each source byte is four
        # 2-bit texels, most significant first, each widened to a nibble.
        # What it decodes into is the shuffled layout above (the game
        # loads the decoded bytes the same way), so unshuffle after.
        stride = width_img // 2
        n = stride * rows                     # decoded bytes
        src = f[buf:buf + (n + 1) // 2]
        dec = bytearray()
        for b in src:
            dec.append((nib2(b >> 6) << 4) | nib2((b >> 4) & 3))
            dec.append((nib2((b >> 2) & 3) << 4) | nib2(b & 3))
        dec = bytes(dec[:n])
        for r in range(rows):
            line = unshuffle_row(dec[r * stride:(r + 1) * stride], r)
            texels = []
            for b in line:
                texels.append(b >> 4)
                texels.append(b & 15)
            out.append([(i << 12) | 0x0FFF for i in texels[:width_img]])
        return PVRFMT_ARGB4444, out

    if siz == G_IM_SIZ_4b:
        stride = width_img // 2
        for r, line in enumerate(strip_rows(f, buf, stride, rows)):
            texels = []
            for b in line:
                texels.append(b >> 4)
                texels.append(b & 15)
            texels = texels[:width_img]
            if fmt == G_IM_FMT_I:
                out.append([(i << 12) | 0x0FFF for i in texels])
            elif fmt == G_IM_FMT_IA:
                # 3 bits of intensity, 1 of alpha
                out.append([((v & 1) * 0xF000) | (((v >> 1) * 15 // 7) * 0x111)
                            for v in texels])
            elif fmt == G_IM_FMT_CI:
                out.append([lut[i] for i in texels])
            else:
                raise ValueError("4-bit format %d" % fmt)
        return (PVRFMT_ARGB1555 if fmt == G_IM_FMT_CI
                else PVRFMT_ARGB4444), out

    if siz == G_IM_SIZ_8b:
        stride = width_img
        for r, line in enumerate(strip_rows(f, buf, stride, rows)):
            line = line[:width_img]
            if fmt == G_IM_FMT_I:
                out.append([((b >> 4) << 12) | 0x0FFF for b in line])
            elif fmt == G_IM_FMT_IA:
                out.append([((b & 15) << 12) | ((b >> 4) * 0x111)
                            for b in line])
            elif fmt == G_IM_FMT_CI:
                out.append([lut[b] for b in line])
            else:
                raise ValueError("8-bit format %d" % fmt)
        return (PVRFMT_ARGB1555 if fmt == G_IM_FMT_CI
                else PVRFMT_ARGB4444), out

    if siz == G_IM_SIZ_16b:
        stride = width_img * 2
        for r, raw in enumerate(strip_rows(f, buf, stride, rows)):
            line = struct.unpack_from(">%dH" % width_img, raw, 0)
            if fmt == G_IM_FMT_RGBA:
                out.append([rotate5551(t) for t in line])
            elif fmt == G_IM_FMT_IA:
                out.append([(((t & 0xFF) >> 4) << 12) | ((t >> 12) * 0x111)
                            for t in line])
            else:
                raise ValueError("16-bit format %d" % fmt)
        return (PVRFMT_ARGB1555 if fmt == G_IM_FMT_RGBA
                else PVRFMT_ARGB4444), out

    if siz == G_IM_SIZ_32b:
        # RGBA32 is not swapped by the rule above: what shuffles it is
        # SP_TEXSHUF (PR/sp.h:155, Sprite.attr bit 9), the flag saying
        # the rows were pre-shuffled in storage, and its unit is 16
        # bytes with 8-byte halves -- two texels at a time. The decomp's
        # tooling splits it the same way (relocSpriteTool.py:
        # unswizzle_n64_texture returns RGBA32 untouched,
        # deshuffle_texshuf_rows with word_size=16 handles it), and
        # measuring agreed before the flag was read: on
        # 19_MNPlayersPortraits the 4-byte swap combs the portraits and
        # the 8-byte one draws them. check_texshuf below is why this
        # runs unconditionally.
        stride = width_img * 4
        for r in range(rows):
            raw = unshuffle_row32(f[buf + r * stride:buf + (r + 1) * stride], r)
            line = struct.unpack_from(">%dI" % width_img, raw, 0)
            out.append([((t & 0xF0) << 8) | ((t >> 28) << 8) |
                        (((t >> 20) & 15) << 4) | ((t >> 12) & 15)
                        for t in line])
        return PVRFMT_ARGB4444, out

    raise ValueError("bitmap size %d" % siz)


def pot(n):
    p = 8                     # the PVR's smallest texture side
    while p < n:
        p *= 2
    return p


# PR/sp.h:155 SP_TEXSHUF: "the rows of this sprite are stored
# pre-shuffled". Every one of the 1178 sprites in the US ROM sets it, so
# unshuffle_row and the 32-bit branch of decode_rows run on every sprite
# without testing it -- but a sprite that did not would come out combed,
# silently, so say so instead.
SP_TEXSHUF = 0x0200


def check_texshuf(name, sp):
    if not (sp["attr"] & SP_TEXSHUF):
        raise AssertionError(
            "%s: attr 0x%04X has no SP_TEXSHUF; its rows are not "
            "pre-shuffled and decode_rows would unshuffle them anyway"
            % (name, sp["attr"]))


def read_sprite(f, intern, off):
    """The Sprite at `off` and its strips, as the game reads them."""
    (x, y, w, h, scalex, scaley, expx, expy, attr, zdepth,
     r, g, b, a, start_tlut, n_tlut) = struct.unpack_from(">4h2f2hHh4B2h",
                                                          f, off)
    lut_off = intern.get(off + 32)
    (istart, istep, nbitmaps, ndisplist,
     bmheight, bmhreal) = struct.unpack_from(">6h", f, off + 36)
    fmt, siz = f[off + 48], f[off + 49]
    bitmaps_off = intern[off + 52]
    strips = []
    for i in range(nbitmaps):
        bo = bitmaps_off + 16 * i
        bw, bwi, s, t = struct.unpack_from(">4h", f, bo)
        buf = intern[bo + 8]
        ah, lo = struct.unpack_from(">2h", f, bo + 12)
        strips.append(dict(width=bw, width_img=bwi, s=s, t=t, buf=buf,
                           actual_height=ah, lut_offset=lo))
    lut = None
    if fmt == G_IM_FMT_CI:
        lut = read_lut_sized(f, siz, n_tlut, lut_off)
    return dict(x=x, y=y, width=w, height=h, scalex=scalex, scaley=scaley,
                expx=expx, expy=expy, attr=attr, zdepth=zdepth,
                rgba=(r, g, b, a), start_tlut=start_tlut, n_tlut=n_tlut,
                istart=istart, istep=istep, nbitmaps=nbitmaps,
                ndisplist=ndisplist, bmheight=bmheight, bmhreal=bmhreal,
                bmfmt=fmt, bmsiz=siz, strips=strips, lut=lut,
                lut_off=lut_off)


def read_lut_sized(f, siz, n_tlut, lut_off):
    """A CI sprite's palette at `lut_off`, as ARGB1555 words: the full
    table for the texel size, of which the file holds the `n_tlut` the
    game loads (lbCommonDrawSObjBitmap's gDPLoadTLUT) -- a palette at
    the end of a file (17_MNPlayersCommon's BackButton) holds no more
    than that. The rest is 0, transparent black, as untouched TMEM
    would not be but no texel of these sprites reaches."""
    full = 16 if siz == G_IM_SIZ_4b else 256
    count = n_tlut if n_tlut else full
    count = min(count, full, (len(f) - lut_off) // 2)
    lut = [rotate5551(t) for t in
           struct.unpack_from(">%dH" % count, f, lut_off)]
    return lut + [0] * (full - count)


def read_lut(f, sp, lut_off):
    """A CI sprite's palette at `lut_off`, as ARGB1555 words."""
    return read_lut_sized(f, sp["bmsiz"], sp["n_tlut"], lut_off)


def defining_source(sym):
    """The relocData source that defines `sym` (an array or struct
    definition, not an extern), as (file id, path)."""
    ssb_paths.require_decomp()
    pat = re.compile(r"^\w+\s+\*?%s(\[|\s*=)" % re.escape(sym), re.M)
    hits = []
    for fn in sorted(os.listdir(M.RELOC_DIR)):
        if not fn.endswith(".c"):
            continue
        path = os.path.join(M.RELOC_DIR, fn)
        if pat.search(open(path).read()):
            hits.append((int(fn.split("_", 1)[0]), path))
    if len(hits) != 1:
        raise ValueError("expected exactly one definition of %s, found %s"
                         % (sym, hits))
    return hits[0]


# sizeof each type a relocData source lays out in the open, for
# source_layout; a Bitmap (sys/objtypes.h) is four s16, a pointer and two
# s16 -- 16 bytes.
LAYOUT_SIZES = {"u8": 1, "s8": 1, "u16": 2, "s16": 2, "u32": 4, "s32": 4,
                "f32": 4, "int": 4, "Bitmap": 16, "Sprite": 68}


def source_layout(path, want):
    """The file offset of `want` in the relocData source at `path`, by
    walking its top-level definitions in order -- PAD(n), typed arrays,
    Bitmap arrays -- the way the source itself lays the file out. For a
    file whose source carries no `@ 0x` placement comments (DK's icon
    file, 319_DkIcon.c, hand-split from a spritelist). Every array the
    walk crosses has to be of a type in LAYOUT_SIZES with an explicit
    count, and a definition it cannot size before reaching `want` is an
    error rather than a guess."""
    src = M.read_source(path)
    off = 0
    for m in re.finditer(r"^(?:PAD\((\d+)\);|(\w+)\s+(\w+)(?:\[(\d*)\])?\s*=)",
                         src, re.M):
        pad, typ, name, count = m.groups()
        if pad is not None:
            off += int(pad)
            continue
        if name == want:
            return off
        if typ not in LAYOUT_SIZES or count == "":
            raise ValueError("%s: cannot size %s %s%s ahead of %s"
                             % (path, typ, name,
                                "[%s]" % count if count is not None else "",
                                want))
        off += LAYOUT_SIZES[typ] * (int(count) if count else 1)
    raise ValueError("%s: no definition of %s" % (path, want))


def place_symbol(sym):
    """(file id, offset) of a relocData symbol: from its name where the
    decomp put the offset there (palette_0xHHHH, gap_0xBASE_sub_0xOFF),
    else from the `@ 0x` comment above its definition, else by walking
    the defining source's layout (source_layout), checked against the
    decomp's own catalogue of the file's sprites."""
    fid, path = defining_source(sym)
    mm = re.search(r"palette_0x([0-9A-Fa-f]+)$", sym)
    if mm:
        return fid, int(mm.group(1), 16)
    mm = re.search(r"gap_0x([0-9A-Fa-f]+)_sub_0x([0-9A-Fa-f]+)$", sym)
    if mm:
        return fid, int(mm.group(1), 16) + int(mm.group(2), 16)
    src = open(path).read()
    d = re.search(r"@ 0x([0-9A-Fa-f]+)[^\n]*\n[^\n]*\b%s\b" % re.escape(sym),
                  src)
    if d:
        return fid, int(d.group(1), 16)
    # the walk, proved on something the catalogue knows: every sprite the
    # decomp lists for this file that the walk can reach (an unsized
    # array, a Gfx[], ends its reach) must come out where the list says,
    # and at least one must be reachable
    proved = 0
    for name, off in catalogue(fid):
        sm = re.search(r"^Sprite\s+(d\w+_%s)\s*=" % re.escape(name), src, re.M)
        if not sm:
            continue
        try:
            walked = source_layout(path, sm.group(1))
        except ValueError:
            continue
        if walked != off:
            raise AssertionError("%s: the layout walk puts %s at 0x%X, the "
                                 "catalogue at 0x%X"
                                 % (path, sm.group(1), walked, off))
        proved += 1
    if proved == 0:
        raise ValueError("%s: nothing in the catalogue proves the layout "
                         "walk" % path)
    return fid, source_layout(path, sym)


def stock_lut_offsets(fighter):
    """The palettes FTSprites.stock_luts names for one fighter, as offsets
    in the file that holds them (the model file, or DK's icon file), read
    from the decomp's relocData source: the main file's
    `d<Name>Main_stock_luts[N] = { &<sym>, ... }`, plus the one-entry
    `d<Name>Main_data_0x...[1]` rows the decomp split the rest of DK's
    table into, in source order while they keep naming the same icon's
    palettes. Returns (file id, [offsets])."""
    _mid, main_path = M.find_reloc_source(fighter + "Main")
    src = open(main_path).read()
    m = re.search(r"\w+Main_stock_luts\[\d+\] = \{(.*?)\};", src, re.S)
    if not m:
        raise ValueError("no stock_luts in %s" % main_path)
    syms = re.findall(r"&?\b(d\w+)\b", m.group(1))
    base = re.sub(r"\d+$", "", syms[-1])
    for row in re.finditer(r"u32 \w+Main_data_0x[0-9A-Fa-f]+\[1\] = \{ "
                           r"\(u32\)(d\w+) \};", src[m.end():]):
        if re.sub(r"\d+$", "", row.group(1)) != base:
            break
        syms.append(row.group(1))
    out = []
    fids = set()
    for sym in syms:
        fid, off = place_symbol(sym)
        fids.add(fid)
        out.append(off)
    if not out:
        raise ValueError("stock_luts in %s names no palettes" % main_path)
    if len(fids) != 1:
        raise ValueError("stock_luts in %s spans files %s"
                         % (main_path, sorted(fids)))
    return fids.pop(), out


def fighter_sprite_parts(fighter):
    """What --fighter exports: the files and sprites the fighter's own
    FTSprites names (relocData NNN_<Name>Main.c d<Name>Main_sprites --
    stock_sprite, stock_luts, emblem), as export_parts' list. Every
    fighter but DK keeps both sprites in his model file; DK's are in a
    file of their own (319_DkIcon.c), which is why this reads the
    initializer rather than assuming the model."""
    _mid, main_path = M.find_reloc_source(fighter + "Main")
    src = open(main_path).read()
    m = re.search(r"FTSprites d\w+Main_sprites = \{(.*?)\};", src, re.S)
    if not m:
        raise ValueError("no FTSprites initializer in %s" % main_path)
    syms = re.findall(r"\b(d\w+|NULL)\b",
                      re.sub(r"/\*.*?\*/", "", m.group(1), flags=re.S))
    if len(syms) != 3:
        raise ValueError("%s: FTSprites names %s" % (main_path, syms))
    stock, luts_sym, emblem = syms
    if stock == "NULL" or luts_sym == "NULL":
        # The twelve Fighting Polygons: { NULL, NULL,
        # dMasterHandIcon_FTEmblem } (207_NMarioMain.c and siblings). No
        # stock icon of their own -- sc1pgame.c draws theirs from
        # llFTStocksZakoSprite -- and Master Hand's emblem.
        if stock != "NULL" or luts_sym != "NULL" or emblem == "NULL":
            raise ValueError("%s: FTSprites names %s" % (main_path, syms))
        return [(defining_source(emblem)[0], ["FTEmblem"], None)]
    stock_fid, _ = defining_source(stock)
    emblem_fid, _ = defining_source(emblem)
    lut_fid, luts = stock_lut_offsets(fighter)
    if lut_fid != stock_fid:
        raise ValueError("%s: the stock icon is in file %d, its palettes "
                         "in %d" % (fighter, stock_fid, lut_fid))
    parts = [(stock_fid, ["Stock"], {"Stock": luts})]
    if emblem_fid == stock_fid:
        parts[0][1].append("FTEmblem")
    else:
        parts.append((emblem_fid, ["FTEmblem"], None))
    return parts


def stitch(f, sp, lut=None):
    """The sprite's strips as one image: (pvrfmt, texw, texh, imgw, imgh,
    linear little-endian 16bpp bytes, [row0 per strip]). `lut` overrides
    the sprite's own palette."""
    rows = []
    row0 = []
    pvrfmt = None
    n = sp["nbitmaps"]
    for i, st in enumerate(sp["strips"]):
        take = st["actual_height"] if i == n - 1 else sp["bmheight"]
        if take > st["actual_height"]:
            raise AssertionError("strip %d stores %d rows, draws %d"
                                 % (i, st["actual_height"], take))
        fmt_i, r = decode_rows(f, sp["bmfmt"], sp["bmsiz"], st["buf"],
                               st["width"], st["width_img"], take,
                               sp["lut"] if lut is None else lut)
        if pvrfmt is None:
            pvrfmt = fmt_i
        elif pvrfmt != fmt_i:
            raise AssertionError("strips disagree on format")
        row0.append(len(rows))
        rows.extend(r)
    imgw = max(st["width_img"] for st in sp["strips"])
    imgh = len(rows)
    texw, texh = pot(imgw), pot(imgh)
    out = bytearray(texw * texh * 2)
    for y in range(min(imgh + 1, texh)):
        row = list(rows[min(y, imgh - 1)])
        row.extend([row[-1]] * (min(texw, len(row) + 1) - len(row)))
        struct.pack_into("<%dH" % len(row), out, y * texw * 2, *row)
    return pvrfmt, texw, texh, imgw, imgh, bytes(out), row0


def export(rom, fid, only=None, lut_frames=None):
    """lut_frames: {sprite name: [palette offsets]} for the sprites whose
    palette the game swaps; every offset becomes one texture."""
    return export_parts(rom, [(fid, only, lut_frames)])


def export_parts(rom, parts):
    """One bank out of several files: parts is [(fid, only, lut_frames)],
    and the sprites of each file follow the last one's in the records,
    the bitmap block and the texture block. A bank was one file until
    DK's, whose stock icon and emblem are in his icon file rather than
    his model file (fighter_sprite_parts)."""
    sprites = []
    bitmaps = b""
    texdata = b""
    for fid, only, lut_frames in parts:
        sprites, bitmaps, texdata = export_into(rom, fid, only, lut_frames,
                                                sprites, bitmaps, texdata)
    return sprites, bitmaps, texdata


def export_into(rom, fid, only, lut_frames, sprites, bitmaps, texdata):
    f, e, intern, sites, ids = L.file_info(rom, fid)
    lut_frames = lut_frames or {}
    entries = catalogue(fid)
    if not entries:
        sys.exit("file %d: no sprites in %s" % (fid, DESCRIPTIONS))
    if only:
        keep = set(only)
        entries = [(n, o) for n, o in entries if n in keep]
        missing = keep - set(n for n, _ in entries)
        if missing:
            sys.exit("file %d: no sprite named %s" % (fid, sorted(missing)))
    for name, off in entries:
        sp = read_sprite(f, intern, off)
        check_texshuf(name, sp)
        pvrfmt, texw, texh, imgw, imgh, data, row0 = stitch(f, sp)
        frames = lut_frames.get(name)
        nluts = 1
        lut_offs = [sp["lut_off"] or 0]
        if frames:
            if sp["bmfmt"] != G_IM_FMT_CI:
                raise AssertionError("%s: palette frames on a non-CI sprite"
                                     % name)
            if frames[0] != sp["lut_off"]:
                # the sprite's own palette leads (a --fighter list names
                # it first; a --luts list need not)
                frames = [sp["lut_off"]] + [fo for fo in frames
                                            if fo != sp["lut_off"]]
            for fo in frames[1:]:
                if isinstance(fo, tuple):
                    # another file's palette (lut_frames_of)
                    lut = read_lut(L.file_info(rom, fo[0])[0], sp, fo[1])
                else:
                    lut = read_lut(f, sp, fo)
                more = stitch(f, sp, lut)
                if more[:5] != (pvrfmt, texw, texh, imgw, imgh):
                    raise AssertionError("%s: palette frames disagree" % name)
                data += more[5]
            nluts = len(frames)
            # the key sprite_bank_lut finds a frame by: the offset the
            # game's table names, whichever file it is in
            lut_offs = [fo[1] if isinstance(fo, tuple) else fo
                        for fo in frames]
            if len(set(lut_offs)) != len(lut_offs):
                raise AssertionError("%s: two palette frames share an "
                                     "offset" % name)
        if max(st["width"] for st in sp["strips"]) != sp["width"]:
            raise AssertionError("%s: strips are %d wide, sprite says %d"
                                 % (name, max(st["width"] for st in sp["strips"]),
                                    sp["width"]))
        expect_h = (sp["bmheight"] * (sp["nbitmaps"] - 1) +
                    sp["strips"][-1]["actual_height"])
        if imgh != expect_h:
            raise AssertionError("%s: %d rows, want %d" % (name, imgh,
                                                            expect_h))
        bm_off = len(bitmaps)
        for st, t0 in zip(sp["strips"], row0):
            bitmaps += struct.pack(BITMAP_RECORD, st["width"],
                                   st["width_img"], st["s"], t0, 0,
                                   st["actual_height"], st["lut_offset"])
        # the compressed 4-bit format leaves here as plain 4-bit: the
        # decode the N64 does in lbCommonMakeSObjForGObj is done
        bmsiz = G_IM_SIZ_4b if sp["bmsiz"] == G_IM_SIZ_4c else sp["bmsiz"]
        rec = struct.pack(SPRITE_RECORD, name.encode()[:15], off,
                          sp["x"], sp["y"], sp["width"], sp["height"],
                          sp["scalex"], sp["scaley"],
                          sp["expx"], sp["expy"], sp["attr"], sp["zdepth"],
                          *sp["rgba"], sp["start_tlut"], sp["n_tlut"],
                          sp["istart"], sp["istep"], sp["nbitmaps"],
                          sp["ndisplist"], sp["bmheight"], sp["bmhreal"],
                          sp["bmfmt"], bmsiz, pvrfmt, nluts,
                          texw, texh, imgw, imgh,
                          len(texdata), len(data) // nluts, bm_off)
        sprites.append((rec, bm_off, name, imgw, imgh, texw, texh,
                        sp["nbitmaps"], sp["bmfmt"], sp["bmsiz"], nluts,
                        lut_offs))
        texdata += data
    return sprites, bitmaps, texdata


def write_bank(out, fid, sprites, bitmaps, texdata):
    count = len(sprites)
    hdr_size = 32
    recs_size = count * SPRITE_RECORD_SIZE
    bitmaps_off = hdr_size + recs_size
    # after the bitmap records, one u32 per texture in the order the
    # sprites' frames run: the relocData offset of the palette that
    # frame was drawn with (0 for a sprite without one), which is what
    # the game's LUT pointers name (sprite.h sprite_bank_lut)
    luts = b"".join(struct.pack("<%dI" % len(lo), *lo)
                    for *_, lo in sprites)
    luts_off = bitmaps_off + len(bitmaps)
    tex_off = luts_off + len(luts)
    tex_off += -tex_off % 32
    blob = bytearray()
    blob += struct.pack("<8s6I", b"SSBSPR1\0", count, fid, bitmaps_off,
                        tex_off, len(texdata), luts_off)
    for rec, bm_off, *_ in sprites:
        # the record's last field is its bitmap records' offset, packed
        # relative to the bitmap block above; make it absolute
        blob += rec[:-4] + struct.pack("<I", bitmaps_off + bm_off)
    assert len(blob) == hdr_size + count * SPRITE_RECORD_SIZE
    blob += bitmaps
    assert len(blob) == luts_off
    blob += luts
    blob += b"\0" * (tex_off - len(blob))
    blob += texdata
    with open(out, "wb") as fp:
        fp.write(blob)
    return len(blob)


def main():
    argv = sys.argv[1:]
    rom_path = M.ROM_DEFAULT
    out = None
    fid = None
    only = None
    lut_frames = None
    do_list = "--list" in argv
    if "--rom" in argv:
        rom_path = argv[argv.index("--rom") + 1]
    if "--out" in argv:
        out = argv[argv.index("--out") + 1]
    if "--file" in argv:
        fid = int(argv[argv.index("--file") + 1])
    if "--only" in argv:
        only = argv[argv.index("--only") + 1].split(",")
    if "--luts" in argv:
        if fid is None:
            sys.exit("--luts wants --file")
        lut_frames = lut_frames_of(fid, argv[argv.index("--luts") + 1])
    parts = None
    if "--fighter" in argv:
        fighter = argv[argv.index("--fighter") + 1]
        parts = fighter_sprite_parts(fighter)
        fid = parts[0][0]
    if fid is None:
        sys.exit("--file <relocData id> or --fighter <Name> is required")
    if do_list:
        for name, off in catalogue(fid):
            print("%-24s 0x%05x" % (name, off))
        return
    if not out:
        sys.exit("--out is required")
    rom = open(rom_path, "rb").read()
    if parts is None:
        parts = [(fid, only, lut_frames)]
    sprites, bitmaps, texdata = export_parts(rom, parts)
    size = write_bank(out, fid, sprites, bitmaps, texdata)
    fmtname = {0: "rgba", 2: "ci", 3: "ia", 4: "i"}
    sizname = {0: "4", 1: "8", 2: "16", 3: "32", 4: "4c"}
    for _, _, name, imgw, imgh, texw, texh, nbm, fmt, siz, nl, _ in sprites:
        print("  %-16s %3dx%-3d %s%s %2d strip%s -> %dx%d%s"
              % (name, imgw, imgh, fmtname.get(fmt, fmt),
                 sizname.get(siz, siz), nbm, "" if nbm == 1 else "s",
                 texw, texh, (" x%d palettes" % nl) if nl > 1 else ""))
    print("wrote %s (%d bytes: %d sprites, %d bytes of texture)"
          % (out, size, len(sprites), len(texdata)))


if __name__ == "__main__":
    main()

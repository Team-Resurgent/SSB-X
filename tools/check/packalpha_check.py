#!/usr/bin/env python3
"""packalpha_check.py -- a picture with alpha, baked into the opaque list.

The PVR has three lists, and the opaque one ignores a texel's alpha
entirely: whatever RGB sits behind a clear texel is written to the frame
buffer. So a sprite whose background is transparent -- and nearly every
glow, spark, star and flat monster in this game is one -- draws as a
RECTANGLE of that background's colour when its batch lands in that list.
Three separate bugs have now been exactly this pairing:

  * the green impact ring (ab22fc3), whose display PROC sets
    G_RM_AA_ZB_XLU_SURF and whose display LIST says nothing;
  * Snorlax, Clefairy and Spearow's swarm (bcca895), the same shape on
    the item side -- black boxes around three flat sprites;
  * Pikachu's Thunder, Ness's PK Thunder trail, Yoshi's stars (which the
    Star Rod's star shares), the Ray Gun's shot, Kirby's star and the
    fire spark, whose mode comes from wp/wpdisplay.c's wpDisplayDrawNormal
    and ef/efdisplay.c's setter GObjs rather than from any display list.

MeshBaker.bucket_of can only read the list it is replaying; where the
list sets no render mode it falls back to the DL head, and head 0 is the
opaque one. Every one of those bugs was that fallback being wrong, and
none of them is visible in the exporter's output -- only in the texels.

So this walks the PACKS THAT SHIPPED, which is the other way round to the
bake: every batch in the opaque list whose texture carries a clear or
partly clear texel, with no knowledge of how it got there. A hit is a
candidate bug, not proof -- a batch whose alpha the N64 also ignored is
faithful -- so each survivor is listed in KNOWN with the reason.

`./run.sh test packalpha` runs it alone; `./run.sh test` runs it with the rest.
"""
import glob
import os
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
ROMDISK = os.path.join(ROOT, "src", "game", "ssb64", "romdisk")

# src/dc/fighter.h: FPackHeader, FPackBatch, FPackTex.
HDR = "<8s8I8I3ff8s8I"
BATCH = "<HHhBBHHIIII"
TEX = "<IIHHBBBB"
MAGIC = b"SSBPACK"
FPACK_LIST_MASK = 3
FPACK_LIST_OP = 0
FPACK_TEX_ARGB1555 = 1
FPACK_TEX_ARGB4444 = 2

# Every batch that is still opaque with an alpha-carrying texture, and why
# it is not the bug above. Keyed by (pack, batch); the value is the
# reason. An entry that stops matching fails the run, so this list cannot
# outlive the packs.
KNOWN = {
    ("kirby.pack", 37):
        "one triangle on joint 2 over a 32x32 tile with 16 clear texels "
        "in it, 1.5% of the tile. Its list sets no render mode either -- "
        "the head decided, as in every bug above -- but for a FIGHTER "
        "head 0 is right: ft/ftdisplaymain.c:1236 sets G_RM_AA_ZB_OPA_SURF "
        "on that head before the body is drawn, so the N64 ignores those "
        "texels' alpha in exactly the way the opaque PVR list does",
    ("mvopeningroombooks.mdl", 2):
        "the spines of the books on Master Hand's desk: a "
        "32x32 tile with 4 clear texels in it, 0.4% of the tile, in one "
        "corner the geometry's UVs do not reach. Its list sets no render "
        "mode, so the head decided -- and head 0 is right here for the "
        "same reason as Kirby's above. mv/mvopening/mvopeningroom.c's "
        "procs are the only ones that touch head 0 in this scene and "
        "every one of them is a set/restore pair that leaves it back at "
        "G_RM_AA_ZB_OPA_SURF (:543-546, :594-607, :1011-1040), which is "
        "also what sys/rdp.c's reset list starts each frame with. So the "
        "N64 draws these books opaque too",
    ("mvopeningroombooks.mdl", 4):
        "the second book stack's spines, the same tile and the same "
        "reason as batch 2 above -- the two stacks are one geometry on "
        "two joints",
}


def texel_alpha(word, fmt):
    """One PVR 16-bit texel's alpha, 0..255."""
    if fmt == FPACK_TEX_ARGB4444:
        return ((word >> 12) & 0xF) * 17
    return 0xFF if (word >> 15) else 0


# PRIM's two halves (fighter.h FPACK_ALPHA_PRIM and
# FPACK_COLOR_PRIM, the batch's alpha_src). The colour cycle reads PRIM's
# RGB and the alpha cycle its A, separately, and the bake used to keep all
# four bytes or none by the colour cycle's answer alone -- so a list that
# shades with TEXEL0 * SHADE and fades with TEXEL0 * PRIMITIVE baked
# opaque. Two rules hold every pack to the split, and the anchors are the
# opening Room's own, which is where it was seen:
#   * a half the batch's combiner does not read is baked white / 0xFF;
#   * a PRIM alpha the combiner DOES read is wasted in the opaque list.
FPACK_ALPHA_PRIM = 4
FPACK_COLOR_PRIM = 8
PRIM_ANCHORS = {
    # (pack, batch): (prim, the two bits) -- the haze over the desk, and
    # Master Hand's shadow on it
    ("mvopeningroomhaze.mdl", 0): (0xFFFFFF4C, FPACK_ALPHA_PRIM),
    ("mvopeningroombossshadow.mdl", 0): (0xFFFFFF80, FPACK_ALPHA_PRIM),
}


def audit_prim(path):
    """The batches of one pack that break a PRIM rule, as lines."""
    blob = open(path, "rb").read()

    if len(blob) < struct.calcsize(HDR):
        return []
    hd = struct.unpack_from(HDR, blob, 0)

    if not hd[0].startswith(MAGIC):
        return []
    name, out = os.path.basename(path), []

    for i in range(hd[4]):
        b = struct.unpack_from(BATCH, blob,
                               hd[12] + struct.calcsize(BATCH) * i)
        bucket, halves, prim = b[5], b[6] & 0xFF, b[7]

        if not (halves & FPACK_COLOR_PRIM) and (prim >> 8) != 0xFFFFFF:
            out.append("%s batch %d: prim %08X carries a colour its colour "
                       "cycle does not read" % (name, i, prim))
        if not (halves & FPACK_ALPHA_PRIM) and (prim & 0xFF) != 0xFF:
            out.append("%s batch %d: prim %08X carries an alpha its alpha "
                       "cycle does not read" % (name, i, prim))
        if (halves & FPACK_ALPHA_PRIM) and (prim & 0xFF) != 0xFF and \
                (bucket & FPACK_LIST_MASK) == FPACK_LIST_OP:
            out.append("%s batch %d: PRIM alpha %02X in the opaque list"
                       % (name, i, prim & 0xFF))
        want = PRIM_ANCHORS.get((name, i))
        if want is not None and (prim, halves & 12) != want:
            out.append("%s batch %d: prim %08X halves %X, expected %08X "
                       "halves %X" % (name, i, prim, halves & 12,
                                      want[0], want[1]))
    return out


def audit_pack(path):
    """(batches audited, [(batch, tex, w, h, fmt, clear, partial, total)])."""
    blob = open(path, "rb").read()

    if len(blob) < struct.calcsize(HDR):
        return None
    hd = struct.unpack_from(HDR, blob, 0)

    if not hd[0].startswith(MAGIC):
        return None
    batch_count, tex_count = hd[4], hd[5]
    off_batches, off_texs, off_texdata = hd[12], hd[13], hd[15]
    seen, hits = 0, []

    for i in range(batch_count):
        b = struct.unpack_from(BATCH, blob,
                               off_batches + struct.calcsize(BATCH) * i)
        tex, bucket = b[2], b[5]

        if (bucket & FPACK_LIST_MASK) != FPACK_LIST_OP:
            continue
        if tex < 0 or tex >= tex_count:
            continue
        (off, size, w, h, _pal, _clamp, fmt,
         _pad) = struct.unpack_from(TEX, blob,
                                    off_texs + struct.calcsize(TEX) * tex)

        if fmt not in (FPACK_TEX_ARGB1555, FPACK_TEX_ARGB4444) or size == 0:
            continue
        seen += 1
        data = blob[off_texdata + off:off_texdata + off + size]
        clear = partial = 0

        # The texels are twiddled, which reorders them and changes no
        # alpha, so the count does not need untwiddling.
        for (word,) in struct.iter_unpack("<H", data[:(size // 2) * 2]):
            a = texel_alpha(word, fmt)

            if a == 0:
                clear += 1
            elif a != 0xFF:
                partial += 1
        if clear or partial:
            hits.append((i, tex, w, h, fmt, clear, partial, size // 2))
    return seen, hits


def main():
    total_seen, bad, matched = 0, [], set()
    prim_bad = []

    for path in sorted(glob.glob(os.path.join(ROMDISK, "*"))):
        if os.path.isdir(path):
            continue
        try:
            r = audit_pack(path)
        except (struct.error, OSError):
            continue
        if r is None:
            continue
        seen, hits = r
        total_seen += seen
        prim_bad.extend(audit_prim(path))
        name = os.path.basename(path)

        for (i, tex, w, h, fmt, clear, partial, n) in hits:
            line = ("%s batch %d: the opaque list, texture %d (%dx%d, "
                    "%s) has %d clear and %d partly clear texels of %d"
                    % (name, i, tex, w, h,
                       "ARGB4444" if fmt == FPACK_TEX_ARGB4444
                       else "ARGB1555", clear, partial, n))
            if (name, i) in KNOWN:
                matched.add((name, i))
            else:
                bad.append(line)

    stale = sorted(set(KNOWN) - matched)

    for line in prim_bad:
        print("packalpha_check: " + line)
    if prim_bad:
        sys.exit("packalpha_check: %d batch(es) break PRIM's two halves"
                 % len(prim_bad))

    for line in bad:
        print("packalpha_check: " + line)
    for (name, i) in stale:
        print("packalpha_check: KNOWN %s batch %d matches nothing now -- "
              "drop the entry" % (name, i))
    if bad or stale:
        sys.exit("packalpha_check: %d batch(es) draw a texture's alpha in "
                 "the opaque list, %d stale KNOWN entr(ies)"
                 % (len(bad), len(stale)))
    print("packalpha_check: %d opaque textured batches, %d known-safe, "
          "none drawing alpha in the opaque list" % (total_seen, len(matched)))


if __name__ == "__main__":
    main()

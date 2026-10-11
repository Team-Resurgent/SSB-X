#!/usr/bin/env python3
"""mobjrun_check.py -- a MObj's picture or palette, reaching every batch it draws.

A MObj can change two things a poly header compiled at load cannot follow,
and the port carries each one per MObj (src/dc/fighter.h FPackMObjSub):

  * the picture: tex_first/tex_count, a run of textures every batch under
    the MObj steps through (fighter.c compiles one header per frame);
  * the palette: dpal_first/dpal_count/dpal_bank, one PVR bank the
    runtime rewrites whenever palette_id moves (objmodel.c
    dc_joint_material).

Both are one-per-MObj, and on the N64 neither is: the MObj sets up the
tile or TLUT and then EVERY draw of its joint reads it. A MObj that draws
several batches with different pictures breaks both shapes:

  * Final Destination's boss wall (Effects2_1): four joints, five stacked
    slices each, one palette run per MObj -- every slice drew the first
    slice. Fixed with FPACK_OWNRUN, a batch stepping its own run.
  * Run's crash: four joints, five slices each, four of them the same
    pictures in every joint, so they interned to ONE texture each in ONE
    joint's bank -- three joints' slices recoloured with the fourth.
    Fixed by ssb_assets.dpal_bind.

So this walks the packs THAT SHIPPED -- every SSBPACKA blob in the
romdisk and disc files, the ones embedded in a stage too -- and fails on

  * a batch under a picture-stepping MObj whose run does not start at its
    own picture (no FPACK_OWNRUN and its tex is not tex_first), or whose
    run leaves the texture table;
  * a textured batch under a palette-stepping MObj that is not a 4bpp
    paletted texture in that MObj's dpal_bank;
  * an FPackMObjs header shorter than its twelve words, or a palette-frame
    block (off_dpal/dpal_count) that leaves the pack.

`./run.sh test mobjrun` runs it alone; `./run.sh test` runs it with the rest.
Needs a build; no ROM.
"""
import collections
import glob
import os
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
GAME = os.path.join(ROOT, "src", "game", "ssb64")

# src/dc/fighter.h: FPackHeader, FPackBatch, FPackTex, FPackMObjs,
# FPackMObjSub
HDR = "<8s8I8I3ff8s8I"
BATCH = "<HHhBBHHIIII"
TEX = "<IIHHBBBB"
MAGIC = b"SSBPACKA"
MOBJHDR = 48           # FPackMObjs, twelve words
MOBJSUB = 68
SUB_TEX = 24            # tex_first, tex_count
SUB_DPAL = 60           # dpal_first, dpal_count, dpal_bank
FPACK_OWNRUN = 512
FPACK_TEX_PAL4 = 0


def audit(blob, tag, stats, bad):
    hd = struct.unpack_from(HDR, blob, 0)
    nb, nx = hd[4], hd[5]
    o_b, o_x = hd[12], hd[13]
    om = hd[-1]
    if not om:
        return
    nm, osub, _oj, obatch = struct.unpack_from("<4I", blob, om)
    odpal, ndpal = struct.unpack_from("<2I", blob, om + 40)
    stats["packs"] += 1
    # FPackMObjs is twelve words and the subs follow it. Six writers
    # once wrote ten, so the loader read off_dpal/dpal_count out of the
    # first MObjSub's flags and PRIM colour.
    if osub != om + MOBJHDR:
        bad.append("%s: FPackMObjs is %d bytes, not %d"
                   % (tag, osub - om, MOBJHDR))
        return
    if ndpal and (odpal < MOBJHDR or odpal + 32 * ndpal > len(blob)):
        bad.append("%s: %d palette frames at %#x" % (tag, ndpal, odpal))
    batches = [struct.unpack_from(BATCH, blob, o_b + i * 28)
               for i in range(nb)]
    texs = [struct.unpack_from(TEX, blob, o_x + i * 16) for i in range(nx)]
    under = collections.defaultdict(list)
    for bi in range(nb):
        mo = struct.unpack_from("<h", blob, obatch + 2 * bi)[0]
        if mo >= nm:
            bad.append("%s batch %d: MObj %d of %d" % (tag, bi, mo, nm))
        elif mo >= 0:
            under[mo].append(bi)

    for mo, lst in sorted(under.items()):
        tf, tc = struct.unpack_from("<2h", blob, osub + mo * MOBJSUB + SUB_TEX)
        _df, dc, bank = struct.unpack_from("<3h", blob,
                                           osub + mo * MOBJSUB + SUB_DPAL)
        if len({batches[bi][2] for bi in lst}) > 1:
            stats["multi_picture"] += 1
        if tc > 1:
            stats["picture_stepping"] += 1
            for bi in lst:
                b = batches[bi]
                own = b[5] & FPACK_OWNRUN
                first = b[2] if own else tf
                stats["ownrun"] += 1 if own else 0
                if first < 0 or first + tc > nx:
                    bad.append("%s batch %d: MObj %d's run %d..%d is outside "
                               "the %d textures" % (tag, bi, mo, first,
                                                    first + tc - 1, nx))
                elif not own and b[2] != tf:
                    bad.append("%s batch %d draws picture %d but MObj %d "
                               "steps a run from %d (no FPACK_OWNRUN)"
                               % (tag, bi, b[2], mo, tf))
        if dc > 1:
            stats["palette_stepping"] += 1
            for bi in lst:
                t = batches[bi][2]
                if t < 0:
                    continue
                if texs[t][6] != FPACK_TEX_PAL4 or texs[t][4] != bank:
                    bad.append("%s batch %d: texture %d (bank %d, format %d) "
                               "but MObj %d rewrites bank %d"
                               % (tag, bi, t, texs[t][4], texs[t][6], mo,
                                  bank))


def main():
    stats, bad = collections.Counter(), []
    paths = sorted(glob.glob(os.path.join(GAME, "romdisk", "*")) +
                   glob.glob(os.path.join(GAME, "disc", "*.mdl")))
    for path in paths:
        if os.path.isdir(path):
            continue
        data = open(path, "rb").read()
        at = data.find(MAGIC)
        while at >= 0:
            tag = os.path.basename(path) + ("@%#x" % at if at else "")
            try:
                audit(data[at:], tag, stats, bad)
            except struct.error:
                bad.append("%s: truncated pack" % tag)
            at = data.find(MAGIC, at + 8)
    if not stats["packs"]:
        sys.exit("mobjrun_check: no packs with MObjs under %s -- build first"
                 % GAME)
    for line in bad:
        print("mobjrun_check: " + line)
    if bad:
        sys.exit("mobjrun_check: %d batch(es) miss their MObj's picture or "
                 "palette" % len(bad))
    print("mobjrun_check: %d packs with MObjs; %d MObjs step pictures (%d "
          "batches on their own run), %d step palettes, %d draw several "
          "pictures; every batch follows its MObj"
          % (stats["packs"], stats["picture_stepping"], stats["ownrun"],
             stats["palette_stepping"], stats["multi_picture"]))


if __name__ == "__main__":
    main()

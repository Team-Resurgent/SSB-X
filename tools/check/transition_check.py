#!/usr/bin/env python3
"""ssb64-dc: the eleven transition wipes, against the ROM they came out of.

The wipes are the one thing this repository bakes whose texture is not in
the file. Their display lists point gsDPSetTextureImage at *segment 1* --
the copy of the last frame drawn that lbTransitionProcDisplay binds -- and
walk it in horizontal bands, one gsDPLoadTile each, with
gsDPSetTileSize moving the origin down the picture band by band. The
exporter collapses all of that into one FPACK_TEX_EXTERN texture and bakes
UVs against the whole 300x220 picture instead of against a band.

That is a transform with no runtime test under it: the target has no
display-list interpreter to disagree with, and the picture does not exist
until the game runs, so nothing on the Dreamcast can say the UVs are
wrong -- a mis-normalised wipe would just show the screen at the wrong
scale. This is the check that would catch it, and it reads the ROM
directly rather than the packs:

  * exactly one texture per wipe, and it is the extern one. A wipe that
    grew a real texture would mean the segment test had stopped working.
  * the UVs cover [0, 1] on both axes and leave it on neither. The
    picture is the whole model's skin in all eleven, so the extremes are
    the proof the band origins were handled: normalise against a band
    instead and V comes out in the tens.
  * every band the display list loads lies inside the 300x220 the game
    allocates (lbtransition.c:216), and the bands together cover it.
  * the AnimJoint table has one entry per DObj of the tree, every script
    it names walks clean under the decomp's own opcode set, and every
    pointer in the animation stays inside it -- which is what
    ssb_transexport rebases and src/dc/fighter.c relocates.

Usage: python3 tools/check/transition_check.py [--rom <rom.z64>]
"""
import os
import struct
import sys

sys.path[:0] = [os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), d)
                for d in ("lib", "export", "check")]
import ssb_extract               # noqa: E402
import ssb_assets as A           # noqa: E402
import ssb_meshexport as M       # noqa: E402
import ssb_transexport as T      # noqa: E402

EPS = 1e-4


def check(rom, name):
    import pygfxd

    fid, src = T.find_file(name)
    dobj_off = T.symbol_offsets(src, name, "DObjDesc")[0]
    anim_off = T.symbol_offsets(src, name, "AnimJoint")[0]

    f = A.get_file(rom, fid, ssb_extract)
    entry = ssb_extract.read_entry(rom, ssb_extract.RELOC_SEG, fid)
    reloc = A.walk_reloc(f, entry["reloc_intern"])
    root, nodes = A.read_dobj_tree(f, reloc, dobj_off)

    # The bands, straight off the display lists, before the baker sees
    # them: every gsDPLoadTile in every joint's list.
    bands = []
    for n in nodes:
        if n.dl_off is None:
            continue
        dl = A.flatten_dl(f, n.dl_off)
        for off in range(0, len(dl), 8):
            w0, w1 = struct.unpack_from(">II", dl, off)
            if w0 >> 24 == 0xF4:            # G_LOADTILE
                bands.append(((w0 >> 12) & 0xFFF, w0 & 0xFFF,
                              (w1 >> 12) & 0xFFF, w1 & 0xFFF))
    bands = [(a // 4, b // 4, c // 4, d // 4) for a, b, c, d in bands]

    baker = A.MeshBaker(f, pygfxd, [[] for _ in nodes],
                        extern_size=(T.PHOTO_W, T.PHOTO_H))
    verts, tris, joints = baker.bake(root)

    fails = []
    if len(baker.textures) != 1 or not baker.textures[0].get("extern"):
        fails.append("%s: %d textures, extern=%s -- the segment-1 tile was "
                     "not recognised"
                     % (name, len(baker.textures),
                        [t.get("extern", False) for t in baker.textures]))
        return fails, 0, 0

    us = [v[3] for v in verts]
    vs = [v[4] for v in verts]
    for axis, vals in (("U", us), ("V", vs)):
        if min(vals) < -EPS or max(vals) > 1.0 + EPS:
            fails.append("%s: %s runs [%.4f %.4f], outside the picture"
                         % (name, axis, min(vals), max(vals)))
        if abs(min(vals)) > EPS or abs(max(vals) - 1.0) > EPS:
            fails.append("%s: %s runs [%.4f %.4f], not the whole picture "
                         "-- the band origin is being subtracted"
                         % (name, axis, min(vals), max(vals)))

    if not bands:
        fails.append("%s: no gsDPLoadTile at all" % name)
    else:
        for uls, ult, lrs, lrt in bands:
            if lrs >= T.PHOTO_W or lrt >= T.PHOTO_H:
                fails.append("%s: a band loads to (%d,%d), past the %dx%d "
                             "lbTransitionSetupTransition allocates"
                             % (name, lrs, lrt, T.PHOTO_W, T.PHOTO_H))
                break
        rows = set()
        for _uls, ult, _lrs, lrt in bands:
            rows.update(range(ult, lrt + 1))
        if len(rows) < T.PHOTO_H:
            fails.append("%s: the bands cover %d of the picture's %d rows"
                         % (name, len(rows), T.PHOTO_H))

    # the animation, the way ssb_transexport reads it
    njoints = len(baker.nodes)
    table = [reloc.get(anim_off + 4 * k) for k in range(njoints)]
    driven = 0
    for k, t in enumerate(table):
        if t is None:
            continue
        driven += 1
        if not anim_off <= t < len(f):
            fails.append("%s: joint %d's script at 0x%04X is outside the "
                         "animation at 0x%04X" % (name, k, t, anim_off))
            continue
        try:
            A.animjoint_walk(f, reloc, t, set())
        except ValueError as e:
            fails.append("%s: joint %d's script: %s" % (name, k, e))
    if driven == 0:
        fails.append("%s: the AnimJoint table drives nothing" % name)
    for loc, target in reloc.items():
        if loc >= anim_off + 4 * njoints and target < anim_off:
            fails.append("%s: a pointer at 0x%04X leaves the animation"
                         % (name, loc))
            break

    print("  %-10s file %2d: %2d joints (%d driven), %4d tris, "
          "%2d bands over %d rows, UV [%.3f %.3f]x[%.3f %.3f]"
          % (name, fid, njoints, driven, len(tris), len(bands),
             len(rows) if bands else 0, min(us), max(us), min(vs), max(vs)))
    return fails, len(tris), driven


def main():
    argv = sys.argv[1:]
    rom_path = M.ROM_DEFAULT
    if "--rom" in argv:
        rom_path = argv[argv.index("--rom") + 1]
    rom = open(rom_path, "rb").read()

    fails = []
    tris = 0
    print("the eleven wipes of dLBTransitionDescs, against relocData:")
    for name, _what in T.TRANSITIONS:
        f, t, _d = check(rom, name)
        fails += f
        tris += t
    if fails:
        for line in fails:
            print("FAIL " + line)
        sys.exit("transition_check: %d problem(s)" % len(fails))
    print("transition_check: 11 wipes, %d triangles, every one skinned with "
          "the whole %dx%d photocopy and no band origin left in the UVs"
          % (tris, T.PHOTO_W, T.PHOTO_H))


if __name__ == "__main__":
    main()

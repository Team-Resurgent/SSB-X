#!/usr/bin/env python3
"""ssb64-dc: no pack the romdisk ships is bigger than fighter_init accepts.

`Fighter.txr[N]` (src/dc/fighter.h) is one PVR texture pointer per texture
of the pack currently loaded through it, and `fighter_init` REFUSES a pack
whose `tex_count` is larger:

    hd->joint_count > FIGHTER_MAX_JOINTS ||
    hd->tex_count > sizeof(f->txr) / sizeof(f->txr[0])

That refusal is a single dbglog line with the pack's name nowhere in it,
and it has cost time twice. Saffron City's layers
draw 35 distinct images where the biggest fighter draws 25, and the array
was 32, so the stage would not boot and said only "fighter: blob is not a
pack this build handles". It happened again with the geometry layers'
MatAnimJoints: an MObj that flips its picture carries one baked
tile per frame as a contiguous run (FPackMObjSub.tex_first), which took
Brinstar from 17 textures to 48 against an array of 40.

Both times the pack was correct and the CONSUMER was too small, both times
nothing in the build said so, and both times it was found by booting. The
exporters are free to grow a pack; this is the line that says when the
runtime has to grow with it.

So: read every pack the romdisk ships, take the two limits from the C
headers rather than repeating them here (a limit written twice is a limit
that drifts), and fail with the pack named and the margin stated.
"""
import os
import re
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
ROMDISK = os.path.join(ROOT, "src", "game", "ssb64", "romdisk")
FIGHTER_H = os.path.join(ROOT, "src", "dc", "fighter.h")

MAGIC = b"SSBPACKA"


def limits():
    """FIGHTER_MAX_JOINTS and the txr[] length, out of fighter.h itself."""
    src = open(FIGHTER_H).read()
    m = re.search(r"pvr_ptr_t\s+txr\[(\d+)\]", src)
    if not m:
        sys.exit("stgtex_check: fighter.h has no `pvr_ptr_t txr[N]`")
    tex = int(m.group(1))
    j = re.search(r"#define\s+FIGHTER_MAX_JOINTS\s+(\d+)", src)
    if not j:
        # It lives in whichever header fighter.h reaches; find it there.
        for name in ("objmodel.h", "fighter.h", "stage.h"):
            p = os.path.join(ROOT, "src", "dc", name)
            if not os.path.exists(p):
                continue
            j = re.search(r"#define\s+FIGHTER_MAX_JOINTS\s+(\d+)",
                          open(p).read())
            if j:
                break
    if not j:
        sys.exit("stgtex_check: cannot find FIGHTER_MAX_JOINTS")
    return int(j.group(1)), tex


def packs(blob):
    """Every embedded pack of one romdisk file, as (offset, counts).

    A stage file carries its layer pack and then its map objects' and
    ground actors' packs after it, so one file is many -- the same reason
    tools/check/texsheet.py scans rather than parses.
    """
    out = []
    i = blob.find(MAGIC)
    while i >= 0:
        if i + 128 <= len(blob):
            hd = struct.unpack_from("<8I", blob, i + 8)
            out.append((i, hd))
        i = blob.find(MAGIC, i + 8)
    return out


def main():
    max_joints, max_tex = limits()
    if not os.path.isdir(ROMDISK):
        print("stgtex_check: no romdisk built -- nothing to check")
        return 0
    worst_tex = worst_joint = None
    n_files = n_packs = 0
    bad = []
    for name in sorted(os.listdir(ROMDISK)):
        path = os.path.join(ROMDISK, name)
        if not os.path.isfile(path):
            continue
        blob = open(path, "rb").read()
        if MAGIC not in blob:
            continue
        n_files += 1
        for (off, hd) in packs(blob):
            joints, _v, _t, _b, tex = hd[0], hd[1], hd[2], hd[3], hd[4]
            n_packs += 1
            if worst_tex is None or tex > worst_tex[0]:
                worst_tex = (tex, name, off)
            if worst_joint is None or joints > worst_joint[0]:
                worst_joint = (joints, name, off)
            if tex > max_tex:
                bad.append("%s +0x%X: %d textures, and Fighter.txr holds %d"
                           % (name, off, tex, max_tex))
            if joints > max_joints:
                bad.append("%s +0x%X: %d joints, and FIGHTER_MAX_JOINTS is %d"
                           % (name, off, joints, max_joints))
    for b in bad:
        print("stgtex_check: %s" % b, file=sys.stderr)
    if bad:
        print("stgtex_check: fighter_init would refuse %d pack(s) at boot, "
              "with only \"blob is not a pack this build handles\" to say so"
              % len(bad), file=sys.stderr)
        return 1
    print("stgtex_check: %d pack(s) in %d romdisk file(s); the largest is "
          "%s +0x%X with %d of %d textures (%d spare), and %s +0x%X with "
          "%d of %d joints"
          % (n_packs, n_files, worst_tex[1], worst_tex[2], worst_tex[0],
             max_tex, max_tex - worst_tex[0], worst_joint[1], worst_joint[2],
             worst_joint[0], max_joints))
    return 0


if __name__ == "__main__":
    sys.exit(main())

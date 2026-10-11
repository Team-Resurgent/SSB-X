#!/usr/bin/env python3
"""ssb64-dc: no baked texture is combed.

A texture read with the wrong row stride comes out interlaced: every row
is a sensible picture on its own, but its neighbour is a different part
of the image, so the rows two apart agree and the rows next to each
other do not. That is what Clash's wallpaper, Run's crash and Final
Destination's backdrop shipped with until the mesh baker learned that
gsDPLoadTile counts the image in the size gsDPSetTextureImage declared
(the game loads those CI4 pictures as 8b, 80 bytes a row, and 80 read as
a CI4 stride took each row's right half from the row below).

This reads the packs, not the exporters, so it covers every baker at
once: for each texture, the fraction of texels that differ from the one
below (adjacent) against the fraction that differ from the one two rows
down (skip). A picture has adjacent <= skip give or take noise; a comb
has adjacent far above skip. Noisy textures have both high and pass.

Usage: python3 tools/check/comb_check.py [pack ...]   (default: every pack)
"""
import glob
import os
import sys

sys.path[:0] = [os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), d)
                for d in ("lib", "export", "check")]
import texsheet as T             # noqa: E402

GAME = os.path.join(T.REPO, "src", "game", "ssb64")

# (pack name, texture index): textures whose alternating rows are the
# picture, each looked at.
KNOWN = {
    # Mushroom Kingdom, a brick course: file 107's 16x4 CI4 at 0x3518
    # (masks 4, maskt 2, so no stride but 16 fits) is, in ROM, indices
    # 9999999998899999 / 8888888888888888 / 8899999999999999 / 88...8 --
    # light bricks with the mortar gap offset half a brick, dark mortar
    # rows between. The odd rows are uniform, so the TMEM odd-row swap
    # cannot change them either. The pack holds two periods, 16x8.
    ("Inishie", 8),
}


def comb(px, w, h):
    adj = sum(px[y * w + x] != px[(y + 1) * w + x]
              for y in range(h - 1) for x in range(w)) / ((h - 1) * w)
    skip = sum(px[y * w + x] != px[(y + 2) * w + x]
               for y in range(h - 2) for x in range(w)) / ((h - 2) * w)
    return adj, skip


def combed(adj, skip):
    return adj > 0.2 and adj > 2 * skip + 0.1


def main():
    paths = sys.argv[1:] or sorted(
        p for ext in T.PACK_EXT
        for d in ("romdisk", "disc")
        for p in glob.glob(os.path.join(GAME, d, "*" + ext)))
    if not paths:
        sys.exit("comb: no packs built -- run ./run.sh build first")
    fails, n = [], 0
    for path in paths:
        blob = open(path, "rb").read()
        for off in T.pack_offsets(blob):
            got = T.read_pack(blob, off)
            if got is None:
                continue
            hd, texs = got
            for t in texs:
                if not t["px"] or t["h"] < 4:
                    continue
                n += 1
                adj, skip = comb(t["px"], t["w"], t["h"])
                if combed(adj, skip) and (hd["name"], t["i"]) not in KNOWN:
                    fails.append("%s %s tex %d (%dx%d): adjacent rows "
                                 "differ %.2f, rows two apart %.2f"
                                 % (os.path.basename(path), hd["name"],
                                    t["i"], t["w"], t["h"], adj, skip))
    for f in fails:
        print("comb: FAIL " + f)
    print("comb: %d textures in %d files, %d combed"
          % (n, len(paths), len(fails)))
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())

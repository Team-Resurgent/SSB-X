#!/usr/bin/env python3
"""ssb64-dc: a two-DObj sprite-array effect, against the ROM it came out of.

The damage sparks and the metal dust are the
same effect built twice: a spawner with no model that makes a flyer every
fourth tic, a flyer whose flags are `EFFECT_FLAG_USERDATA | 0x1` and not
`0x4` so its tree is two DObjs built by hand, one quad under the child,
one MObj on it, and a MatAnimJoint that steps that MObj's sprite array
through seven 32x32 frames. Two sets of blocks, one shape -- so one
checker with two callers, the way tools/export/ssb_effectexport.py has one
`pack_flipbook` with two.

What both of them depend on, and nothing before them did: the flyer's
`MObjSub`'s flags word is **zero**, and gcDrawMObjForDObj
(objdisplay.c:1159) does not read zero as "set nothing":

    if (flags == MOBJ_FLAG_NONE)
    {
        flags = (MOBJ_FLAG_TEXTURE | 0x20 | MOBJ_FLAG_ALPHA);
    }

ALPHA is the bit that makes `sprites[texture_id_curr]` the texture image,
so a reader that took the zero at face value would draw frame 0 for all
seven tics and nothing would say so. The other two bits are a tile size
and a texture scale, which the earliest packed MObjs never asked for
-- tools/lib/ssb_assets.py once refused them outright --
and getting either wrong moves the picture inside its tile rather than
losing it.

So `check` below verifies, in the ROM's own terms:

  * the MObjSub's flags word really is zero, and its sprite array really
    holds every frame the MatAnimJoint reaches.
  * the tile size and texture scale the flags-zero default asks for leave
    the whole 32x32 sprite on the quad, once: the baked corners span the
    tile from 0 to 1 in both axes. That is the check on both formulas.
  * the script's opening SetVal0Rate leaves trau/trav/scau/scav on the
    MObjSub's own values for every tic, which is the only thing that
    makes baking the tile into the vertices once legitimate.
  * the sliced, rebased MatAnimJoint words play *identically* to the ones
    still in the ROM, tic for tic, over the whole animation and past its
    end -- the same check tools/check/slash_check.py makes, and for the same
    reason: a mis-sliced block is an unplayable script, not a wrong
    picture.
  * the frames the pack carries are that many different pictures, in the
    ROM's order. Distinctness alone would pass a flipbook that packed the
    array backwards, so each frame is also matched against the ROM sprite
    it is supposed to be -- by `cfg.texels`, which is the one leg the two
    effects do not share, because the sparks' sprites are CI4 read
    through the display list's TLUT and the metal dust's are IA16 read
    straight.

Not a program: tools/check/spark_check.py and tools/check/mdust_check.py are.
"""
import os
import struct
import sys

sys.path[:0] = [os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), d)
                for d in ("lib", "export", "check")]
import ssb_extract               # noqa: E402
import ssb_assets as A           # noqa: E402
import ssb_effectexport as S     # noqa: E402
from slash_check import read_pack   # noqa: E402


class Flipbook(object):
    """One effect's half of the difference.

    `syms` is the five reloc names -- the file id and the four blocks;
    `dl_is_links` says whether o_dobjsetup is the display list itself
    (the sparks) or a DObjDLLink array pointing at it (the metal dust);
    `pack` is the exporter entry point the build ships; `frames` is how
    far past the animation to replay it; and `texels` is called once per
    frame with (filedata, reloc, sub, k, w, h, picture) and returns the
    problems it found, if any.
    """

    def __init__(self, what, syms, dl_is_links, pack, frames, texels):
        self.what = what
        self.syms = syms
        self.dl_is_links = dl_is_links
        self.pack = pack
        self.frames = frames
        self.texels = texels


def check(rom, cfg):
    import pygfxd

    fails = []
    fid, mobj_off, dl_off, anim_off, mat_off = S.reloc_offsets(cfg.syms)
    f = A.get_file(rom, fid, ssb_extract)
    entry = ssb_extract.read_entry(rom, ssb_extract.RELOC_SEG, fid)
    reloc = A.walk_reloc(f, entry["reloc_intern"])

    subs = A.read_mobjsubs(f, reloc, mobj_off, 1)[0]
    if len(subs) != 1:
        return ["the MObjSub table holds %d MObjs, wanted one" % len(subs)]
    sub = subs[0]
    if sub["flags"] != 0:
        fails.append("MObjSub@0x%04X has flags 0x%04X; this effect is one of "
                     "the two that depend on the flags-zero default"
                     % (sub["off"], sub["flags"]))
    if sub["palettes"]:
        fails.append("MObjSub@0x%04X takes a palette of its own" % sub["off"])
    if not sub["sprites"]:
        fails.append("MObjSub@0x%04X has no sprite array" % sub["off"])

    # Which display list the child draws, and into which DL head.
    if cfg.dl_is_links:
        links = S.read_dl_links(f, reloc, dl_off, cfg.what)
        if len(links) != 1:
            return fails + ["the DObjDLLink array at 0x%04X holds %d links, "
                            "wanted one" % (dl_off, len(links))]
        head, dl = links[0]
    else:
        head, dl = S.SPARK_HEAD, dl_off

    script = reloc.get(reloc.get(mat_off, 0), None)
    if script is None:
        return fails + ["the MatAnimJoint table at 0x%04X has no script"
                        % mat_off]
    ids = sorted(int(v) for v in S.matanim_texture_ids(f, reloc, script))
    nframes = (max(ids) + 1) if ids else 1
    if ids != list(range(nframes)):
        fails.append("the script reaches frames %s, not 0..%d"
                     % (ids, nframes - 1))
    if nframes > len(sub["sprites"]):
        fails.append("the script reaches sprite %d and the array holds %d"
                     % (nframes - 1, len(sub["sprites"])))

    # -- the tile the flags-zero default asks for ----------------------
    # gcDrawMObjForDObj's own gsDPSetTileSize and gsSPTexture are the only
    # ones this display list ever gets: it sets neither itself, only the
    # branch into segment 0xE where the MObj's list is. So the corners
    # below are the whole check on both formulas.
    zero, one = (0.0, 0.0, 0.0), (1.0, 1.0, 1.0)
    root = A.DObjNode(-1, -1, None, zero, zero, one)
    stand = A.DObjNode(0, 0, None, zero, zero, one)
    child = A.DObjNode(1, 1, None, zero, zero, one)
    child.dl_links = [(head, dl)]
    stand.parent, root.children = root, [stand]
    child.parent, stand.children = stand, [child]
    baker = A.MeshBaker(f, pygfxd, [[], subs], mobj_batches=True)
    verts, tris, _joints = baker.bake(root)
    if len(tris) != 2:
        fails.append("the display list bakes to %d triangles, wanted the "
                     "two of one quad" % len(tris))
    for axis, name in ((3, "u"), (4, "v")):
        lo = min(v[axis] for v in verts)
        hi = max(v[axis] for v in verts)
        # 0xFFFF/65536 is the texture scale gsSPTexture is given, so the
        # far corner lands one part in 65536 short of the tile's edge.
        if lo != 0.0 or abs(hi - 1.0) > 1.0 / 32768.0:
            fails.append("the quad's %s spans %.6f..%.6f, not the whole "
                         "tile; the flags-zero tile size or texture scale "
                         "is wrong" % (name, lo, hi))
    if baker.batches and baker.batches[0]["mobj"] != (1, 0):
        fails.append("the batch reads MObj %r, wanted the child DObj's "
                     "first" % (baker.batches[0]["mobj"],))

    # -- the pack the build ships --------------------------------------
    pack = read_pack(cfg.pack(rom))

    # What src/dc/fighter.c's reloc walk does at load, read back as a file
    # the parser can walk: the pointer words become byte offsets and the
    # reloc list becomes the map play_matanim reads them through.
    words = pack["words"]
    body = bytearray(struct.pack(">%dI" % len(words), *words))
    body_reloc = {}
    for w in pack["relocs"]:
        body_reloc[w * 4] = words[w] * 4
        struct.pack_into(">I", body, w * 4, words[w] * 4)

    def play(data, rl, off, frame):
        """...or the reason it could not be played."""
        try:
            return A.play_matanim(data, rl, off, float(frame))
        except (ValueError, struct.error) as e:
            return "unplayable: %s" % e

    for t in range(cfg.frames):
        want = play(f, reloc, script, t)
        got = play(bytes(body), body_reloc, pack["entry"][0] * 4, t)
        if want != got:
            fails.append("the MatAnimJoint at tic %d plays %s packed, %s in "
                         "the ROM" % (t, got, want))

    # -- and what the script does to the tile ---------------------------
    # The script's first command is a SetVal0Rate over tracks 1..4, which
    # are trau, trav, scau, scav -- the four the tile size and texture
    # scale above are computed from. The pack bakes those into the
    # vertices once, so the port can only be right if the script leaves
    # them where the MObjSub has them. It does, for every tic of the
    # animation; this is what says so rather than assuming it, and it is
    # the line that would have to move if another effect's script really
    # scrolled its tile.
    for t in range(cfg.frames):
        st = play(f, reloc, script, t)
        if not isinstance(st, dict):
            continue
        for k in range(1, 5):
            name = A.MAT_TRACKS[k]
            if k not in st["mat"]:
                fails.append("at tic %d the script drives no %s and the "
                             "first command sets one" % (t, name))
                continue
            if abs(st["mat"][k] - sub[name]) > 1.0 / 65536.0:
                fails.append("at tic %d the script puts %s at %g and the "
                             "MObjSub the pack baked has it at %g"
                             % (t, name, st["mat"][k], sub[name]))

    # -- the flipbook, and its order -----------------------------------
    first, count = pack["subs"][0][8], pack["subs"][0][9]
    if count != nframes:
        fails.append("the pack carries %d frames and the script reaches %d"
                     % (count, nframes))
    if pack["subs"][0][0] != 0:
        fails.append("the pack's MObjSub flags word is 0x%04X and the ROM's "
                     "is zero" % pack["subs"][0][0])

    pictures, sizes = [], []
    for i in range(first, first + count):
        off, size, w, h = pack["texs"][i][:4]
        base = pack["texdata"] + off
        pictures.append(bytes(pack["blob"][base:base + size]))
        sizes.append((w, h))
    if len(set(pictures)) != count:
        fails.append("the %d frames hold %d distinct pictures"
                     % (count, len(set(pictures))))

    for k in range(count):
        if k >= len(sub["sprites"]):
            break
        w, h = sizes[k]
        fails.extend(cfg.texels(f, reloc, sub, k, w, h, pictures[k]))

    print("  1 MObj: %d frames from texture %2d, %d distinct, %dx%d, "
          "flags 0x%04X" % (count, first, len(set(pictures)),
                            sizes[0][0], sizes[0][1], pack["subs"][0][0]))
    print("  %d joints, %d batches, %d tris, %d matanim words, %d pointers"
          % (pack["njoint"], len(pack["batches"]), len(tris), len(words),
             len(pack["relocs"])))
    return fails


def main(cfg, done):
    """The two callers' shared entry point: --rom, run, report."""
    import ssb_meshexport as M

    argv = sys.argv[1:]
    rom_path = M.ROM_DEFAULT
    if "--rom" in argv:
        rom_path = argv[argv.index("--rom") + 1]
    fails = check(open(rom_path, "rb").read(), cfg)
    if fails:
        for m in fails:
            print("%s_check: %s" % (cfg.what, m))
        sys.exit("%s_check: %d problems" % (cfg.what, len(fails)))
    print("%s_check: %s" % (cfg.what, done))

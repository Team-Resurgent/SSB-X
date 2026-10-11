#!/usr/bin/env python3
"""ssb64-dc: the ten series emblems, against the ROM they came out of.

The emblem is the first model this repository packs with a *material*: it
carries no texture at all, its colour is the two light colours its MObj
chain holds, and a MatAnimJoint recolours it per player. So the pack
carries three things no other pack does -- the MObjSub records, the link
from a batch to the MObj that drew it, and the scripts themselves, sliced
out of relocData 35 and rebased onto their own words.

None of that has a runtime test under it. The target has no display-list
interpreter to disagree with, and a mis-sliced script does not crash: the
game's parser runs off the end of the block into whatever the next emblem
put there and stops on some other opcode, and the emblem simply comes up
the wrong colour -- or the right one, if the mistake lands on a matching
value. This is the check that would catch it:

  * every MObjSub is colour only. No sprite array, no palettes, and no
    flags outside the three the port's material carries -- which is what
    src/dc/objdisplay.c's gcDrawMObjForDObj is bounded to.
  * the MObjs are numbered in the order gcAddMObjAll hands them out, and
    every batch names one that belongs to its own joint. Get that wrong
    and a batch takes another joint's colour.
  * the sliced, rebased scripts play *identically* to the originals still
    in the ROM, at every one of the four colours. This is the real check
    on the slice: it replays the packed words through the same parser the
    game uses, with the pack's own entry indices and relocation list, and
    compares colour for colour.
  * the four colours differ from each other. An emblem that came up the
    same colour for every player would pass everything above.

Usage: python3 tools/check/emblem_check.py [--rom <rom.z64>]
"""
import os
import struct
import sys

sys.path[:0] = [os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), d)
                for d in ("lib", "export", "check")]
import ssb_extract               # noqa: E402
import ssb_assets as A           # noqa: E402
import ssb_meshexport as M       # noqa: E402
import ssb_emblemexport as E     # noqa: E402


def tree_order(root):
    """The DObjs in gcGetTreeDObjNext order: child, then sibling, then up.

    gcAddMObjAll walks the tree that way and steps its `MObjSub ***` once
    per DObj, so the pack's joint index and the tree walk have to agree --
    which they do because lbCommonSetupTreeDObjs builds the tree from the
    DObjDesc array in the array's own order. Recomputing it here is what
    would catch a tree where they did not.
    """
    out = []

    def walk(n):
        for c in n.children:
            out.append(c)
            walk(c)

    walk(root)
    return out


def check(rom, name):
    import pygfxd

    src = M.read_source(os.path.join(M.RELOC_DIR,
                                     "%d_FTEmblemModels.c" % E.EMBLEM_FILE))
    dobjs, subs, mats = E.read_layout(src)
    f = A.get_file(rom, E.EMBLEM_FILE, ssb_extract)
    entry = ssb_extract.read_entry(rom, ssb_extract.RELOC_SEG, E.EMBLEM_FILE)
    reloc = A.walk_reloc(f, entry["reloc_intern"])

    root, nodes = A.read_dobj_tree(f, reloc, dobjs[name])
    mobjsubs = A.read_mobjsubs(f, reloc, subs[name], len(nodes))
    fails = []

    order = tree_order(root)
    if [n.index for n in order] != list(range(len(nodes))):
        fails.append("%s: gcAddMObjAll's tree walk visits %s, not the "
                     "DObjDesc order the pack indexes by"
                     % (name, [n.index for n in order]))

    for lst in mobjsubs:
        for sub in lst:
            if sub["sprites"] or sub["palettes"]:
                fails.append("%s: MObjSub@0x%04X selects textures or "
                             "palettes" % (name, sub["off"]))
            if sub["flags"] & ~E.MOBJ_FLAGS_KNOWN:
                fails.append("%s: MObjSub@0x%04X flags 0x%04X outside the "
                             "colour set" % (name, sub["off"], sub["flags"]))

    baker = A.MeshBaker(f, pygfxd, mobjsubs, mobj_batches=True)
    verts, tris, joints = baker.bake(root)
    if baker.textures:
        fails.append("%s: %d textures; an emblem is untextured"
                     % (name, len(baker.textures)))

    joint_of = {}
    for j in joints:
        for b in range(j["batch_first"], j["batch_first"] + j["batch_count"]):
            joint_of[b] = j["node"].index
    first = []
    total = 0
    for lst in mobjsubs:
        first.append(total)
        total += len(lst)
    for i, b in enumerate(baker.batches):
        m = b["mobj"]
        if m is None:
            fails.append("%s: batch %d has no MObj" % (name, i))
        elif m[0] != joint_of[i]:
            fails.append("%s: batch %d is joint %d's but takes joint %d's "
                         "MObj" % (name, i, joint_of[i], m[0]))
        elif m[1] >= len(mobjsubs[m[0]]):
            fails.append("%s: batch %d wants MObj %d of joint %d's %d"
                         % (name, i, m[1], m[0], len(mobjsubs[m[0]])))

    # -- the slice, rebased and replayed --------------------------------
    anim_off = min(mats[name]) - 8
    table_off = max(mats[name])
    later = [o for o in sorted(subs.values()) if o > anim_off]
    anim_end = later[0] if later else len(f)
    scripts = A._ptr_list(reloc, table_off)

    # What ssb_emblemexport writes: the block's words with every pointer
    # turned into a word index, and the list of which words those were.
    words = list(struct.unpack(">%dI" % ((anim_end - anim_off) // 4),
                               f[anim_off:anim_end]))
    packed_reloc = []
    for loc, target in sorted(reloc.items()):
        if not anim_off <= loc < anim_end:
            continue
        words[(loc - anim_off) // 4] = (target - anim_off) // 4
        packed_reloc.append((loc - anim_off) // 4)

    # What src/dc/fighter.c does at load, read back as a file the parser
    # can walk: the pointer words become byte offsets into the block, and
    # the reloc list becomes the map play_matanim reads them through.
    blob = bytearray(struct.pack(">%dI" % len(words), *words))
    blob_reloc = {}
    for w in packed_reloc:
        blob_reloc[w * 4] = words[w] * 4
        struct.pack_into(">I", blob, w * 4, words[w] * 4)

    def play(data, rl, off, color):
        """...or the reason it could not be played. A block sliced short
        or an entry pointing at the wrong word does not come back as a
        wrong colour: the parser walks off the end or hits an opcode that
        is not one, and both are the failure this is looking for."""
        try:
            return A.play_matanim(data, rl, off, float(color))
        except (ValueError, struct.error) as e:
            return "unplayable: %s" % e

    # Frames past the four the game asks for as well: the script's five
    # colour blocks are followed by a Wait and a SetAnim whose target is
    # the script's own head, and that jump is the only *pointer* in the
    # whole block. Seeking to a colour never reaches it, so a relocation
    # left undone would go unnoticed here without these.
    for k, s in enumerate(scripts):
        entry_word = (s - anim_off) // 4
        for color in range(E.COLOR_NUM + 4):
            want = play(f, reloc, s, color)
            got = play(bytes(blob), blob_reloc, entry_word * 4, color)
            if want != got:
                fails.append("%s: MObj %d at colour %d plays %s packed, "
                             "%s in the ROM" % (name, k, color, got, want))

    if fails:
        return fails, len(tris), total

    shown = []
    for color in range(E.COLOR_NUM):
        # ext track 3 is light1 (ssb_assets.EXT_TRACKS): the emblem's own
        # colour, which is the whole point of the animation.
        v = A.play_matanim(f, reloc, scripts[0], float(color))["ext"]
        shown.append((int(v[3]) >> 8) & 0xFFFFFF if 3 in v else None)
    if len(set(shown)) != E.COLOR_NUM:
        fails.append("%s: the four colours are %s -- an emblem that does "
                     "not recolour" % (name, shown))

    print("  %-10s DObjDesc 0x%04X: %d joints, %2d MObjs, %d batches, "
          "%3d tris, %2d anim words, light1 %s"
          % (name, dobjs[name], len(nodes), total, len(baker.batches),
             len(tris), len(words),
             ",".join("%06X" % c for c in shown)))
    return fails, len(tris), total


def main():
    argv = sys.argv[1:]
    rom_path = M.ROM_DEFAULT
    if "--rom" in argv:
        rom_path = argv[argv.index("--rom") + 1]
    rom = open(rom_path, "rb").read()

    fails, tris, mobjs = [], 0, 0
    for name in E.EMBLEMS:
        bad, n, m = check(rom, name)
        fails += bad
        tris += n
        mobjs += m
    if fails:
        for line in fails:
            print("FAIL " + line)
        sys.exit("emblem_check: %d problems" % len(fails))
    print("emblem_check: %d emblems, %d triangles, %d MObjs, every script "
          "playing the same four colours packed as in the ROM"
          % (len(E.EMBLEMS), tris, mobjs))


if __name__ == "__main__":
    main()

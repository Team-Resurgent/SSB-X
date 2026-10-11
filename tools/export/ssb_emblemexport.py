#!/usr/bin/env python3
"""ssb64-dc: the ten series emblems of relocData 35, packed.

`mnVSResultsMakeEmblem` (mn/mnvsmode/mnvsresults.c:615) stands the winner's
series emblem behind the results screen: a small model out of
`35_FTEmblemModels.c`, picked by the winning fighter's kind out of three
parallel twelve-entry tables -- a DObjDesc tree, an `MObjSub ***` and an
`AObjEvent32 ***` -- which between them name ten distinct emblems.

What makes it the first model this repo packs with a *material* is the third
table. The emblem has no texture at all: it is untextured lit geometry, and
its colour is the two light colours the MObj chain carries. The MatAnimJoint
is not really an animation -- every script is five colour blocks and a loop,
and `gcAddMatAnimJointAll(gobj, joints, color)` seeks it to frame `color`,
which is the winning player's number. So frame 0 is red, 1 blue, 2 yellow,
3 green: the emblem comes up in the winner's colour, and the script is the
table that says which.

The pack therefore carries three things no other pack does (fighter.h
FPackMObjs): the MObjSub records, the link from a batch to the MObj that
drew it, and the MatAnimJoint scripts, rebased the way a .tra's AnimJoint
is. src/dc/objmodel.c builds the two pointer-array shapes back out of them
at load and hands them to the decomp's own gcAddMObjAll and
gcAddMatAnimJointAll, so the colour the emblem comes up in is chosen by the
game's parser, not by this tool.

Usage: python3 tools/export/ssb_emblemexport.py --name Mario --out mario.mdl
                                         [--rom <rom.z64>]
"""
import math
import os
import re
import struct
import sys

sys.path[:0] = [os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), d)
                for d in ("lib", "export", "check")]
import ssb_extract           # noqa: E402
import ssb_assets as A       # noqa: E402
import ssb_meshexport as M   # noqa: E402
import ssb_packexport as K   # noqa: E402

MAGIC = b"SSBPACKA"
ANIM_NAME_LEN = K.ANIM_NAME_LEN

# relocData 35. mnvsresults.c:78 reaches it through llFTEmblemModelsFileID;
# src/dc/scvsresults.c:137 has the same id in dMNVSResultsFileIDs[4].
EMBLEM_FILE = 35

# The three tables of mnVSResultsMakeEmblem, collapsed: entry i is the
# emblem the fighter kind i wears. Twelve kinds, ten emblems -- Luigi wears
# Mario's and Jigglypuff wears Pikachu's -- in nFTKind order (Mario, Fox,
# Donkey, Samus, Luigi, Link, Yoshi, Falcon, Kirby, Pikachu, Puff, Ness).
EMBLEM_OF_KIND = [
    "Mario", "Fox", "Donkey", "Metroid", "Mario", "Zelda",
    "Yoshi", "FZero", "Kirby", "PMonsters", "PMonsters", "Mother",
]
EMBLEMS = ["Mario", "Fox", "Donkey", "Metroid", "Zelda",
           "Yoshi", "FZero", "Kirby", "PMonsters", "Mother"]

# mnVSResultsMakeEmblem's `color`: the winning player's index, or
# colors[] = {0, 1, 3} by team. Four is the whole range, and the exporter
# checks the script says something for each of them.
COLOR_NUM = 4

# MObjSub.flags bits the port's material knows how to carry: the two light
# colours, the primitive and the environment. gcDrawMObjForDObj's other
# jobs -- selecting a texture out of the sprite array, loading a TLUT,
# recomputing tile size -- are not ported, and an emblem needs none of
# them (every MObjSub in file 35 has flags 0x3000, LIGHT1 | LIGHT2).
MOBJ_FLAGS_KNOWN = (A.MOBJ_FLAG_LIGHT1 | A.MOBJ_FLAG_LIGHT2 |
                    A.MOBJ_FLAG_PRIMCOLOR)


def read_layout(src):
    """Where each emblem's four blocks start, off the extractor's comments.

    The extractor names a block by the file offset it came from, and for
    this file it puts the offset in a comment rather than in the symbol:
    `/* DObjDesc: Mario @ 0x990 */`. The MatAnimJoint's `{NULL, table}`
    header is the one block with no comment of its own, but it sits
    exactly eight bytes below the first script and holds a pointer to the
    last -- which is checked, not assumed, in pack_emblem.
    """
    dobjs = {m.group(1): int(m.group(2), 16) for m in
             re.finditer(r"/\* DObjDesc: (\w+) @ 0x([0-9A-Fa-f]+)", src)}
    subs = {m.group(1): int(m.group(2), 16) for m in
            re.finditer(r"/\* MObjSub(?: array)?: (\w+) @ 0x([0-9A-Fa-f]+)",
                        src)}
    mats = {}
    for m in re.finditer(r"dFTEmblemModels_(\w+?)_MatAnimJoint_0x"
                         r"([0-9A-Fa-f]+)", src):
        mats.setdefault(m.group(1), set()).add(int(m.group(2), 16))
    return dobjs, subs, mats


def pack_emblem(rom, name):
    import pygfxd

    src = M.read_source(os.path.join(M.RELOC_DIR,
                                     "%d_FTEmblemModels.c" % EMBLEM_FILE))
    dobjs, subs, mats = read_layout(src)
    for tbl, what in ((dobjs, "DObjDesc"), (subs, "MObjSub"),
                      (mats, "MatAnimJoint")):
        if name not in tbl:
            sys.exit("%s: no %s block in relocData %d"
                     % (name, what, EMBLEM_FILE))

    f = A.get_file(rom, EMBLEM_FILE, ssb_extract)
    entry = ssb_extract.read_entry(rom, ssb_extract.RELOC_SEG, EMBLEM_FILE)
    reloc = A.walk_reloc(f, entry["reloc_intern"])

    root, nodes = A.read_dobj_tree(f, reloc, dobjs[name])
    mobjsubs = A.read_mobjsubs(f, reloc, subs[name], len(nodes))

    # -- the MObjSubs --------------------------------------------------
    for j, lst in enumerate(mobjsubs):
        for sub in lst:
            if sub["sprites"] or sub["palettes"]:
                sys.exit("%s: MObjSub@0x%04X selects textures or palettes; "
                         "the port's materials are colour only"
                         % (name, sub["off"]))
            if sub["flags"] & ~MOBJ_FLAGS_KNOWN:
                sys.exit("%s: MObjSub@0x%04X has flags 0x%04X, outside the "
                         "colour set 0x%04X"
                         % (name, sub["off"], sub["flags"], MOBJ_FLAGS_KNOWN))

    # -- the model -----------------------------------------------------
    # mobj_batches: which MObj drew a batch is part of its material key, so
    # every batch has exactly one MObj to take its animated colours from.
    baker = A.MeshBaker(f, pygfxd, mobjsubs, mobj_batches=True)
    verts, tris, joints = baker.bake(root)
    if baker.textures:
        sys.exit("%s: %d textures; an emblem is untextured lit geometry"
                 % (name, len(baker.textures)))

    joint_of = {}
    for j in joints:
        for b in range(j["batch_first"], j["batch_first"] + j["batch_count"]):
            joint_of[b] = j["node"].index
    secs = K.model_sections(
        [(n.parent.index, n.translate, n.rotate, n.scale)
         for n in baker.nodes],
        verts, tris,
        [(b["tri_first"], b["tri_count"], b["mat"], joint_of[i],
          b.get("bucket", 0)) for i, b in enumerate(baker.batches)],
        baker.textures, baker.palettes)

    # Global MObj numbering: joint by joint, in the order gcAddMObjAll
    # hands them to gcAddMObjForDObj, which is the order the chain ends up
    # in. joint_first/joint_count is how src/dc/objmodel.c rebuilds the
    # `MObjSub **` array per joint.
    joint_first, mobjs = [], []
    for lst in mobjsubs:
        joint_first.append((len(mobjs), len(lst)))
        mobjs.extend(lst)

    batch_mobj = []
    for i, b in enumerate(baker.batches):
        m = b["mobj"]
        if m is None or m[0] != joint_of[i]:
            # No segment-0xE branch under this joint's DL: the batch's
            # colours are the display list's own and nothing animates them.
            batch_mobj.append(-1)
            continue
        batch_mobj.append(joint_first[m[0]][0] + m[1])
    if any(m < 0 for m in batch_mobj):
        sys.exit("%s: %d batches have no MObj; an emblem's colour is its "
                 "MObj's" % (name, sum(1 for m in batch_mobj if m < 0)))

    # -- the material animation ----------------------------------------
    # One script per MObj, in the same order, out of the per-DObj table
    # the `{NULL, table}` block points at. Everything from that block to
    # the next emblem's is carried as words and rebased onto itself, the
    # way tools/export/ssb_transexport.py carries a wipe's AnimJoint.
    anim_off = min(mats[name]) - 8
    table_off = max(mats[name])
    if reloc.get(anim_off) is not None or reloc.get(anim_off + 4) != table_off:
        sys.exit("%s: no {NULL, table} MatAnimJoint header at 0x%04X"
                 % (name, anim_off))
    later = [o for o in sorted(subs.values()) if o > anim_off]
    anim_end = later[0] if later else len(f)
    if anim_end % 4 or anim_off % 4:
        sys.exit("%s: MatAnimJoint block 0x%04X..0x%04X is not word aligned"
                 % (name, anim_off, anim_end))

    scripts = A._ptr_list(reloc, table_off)
    if len(scripts) != len(mobjs):
        sys.exit("%s: %d MatAnimJoint scripts for %d MObjs"
                 % (name, len(scripts), len(mobjs)))

    words = list(struct.unpack(">%dI" % ((anim_end - anim_off) // 4),
                               f[anim_off:anim_end]))
    entries = []
    colors = []
    for s in scripts:
        if not anim_off <= s < anim_end:
            sys.exit("%s: script at 0x%04X is outside the block at 0x%04X"
                     % (name, s, anim_off))
        # Play it here too: the game's parser has no default case, so an
        # opcode it does not know is an infinite loop on the target. This
        # also gives emblem_check.py the colours to compare against.
        colors.append([A.play_matanim(f, reloc, s, float(c))["ext"]
                       for c in range(COLOR_NUM)])
        entries.append((s - anim_off) // 4)
    relocs = []
    for loc, target in sorted(reloc.items()):
        if not anim_off <= loc < anim_end:
            continue
        if not anim_off <= target < anim_end:
            sys.exit("%s: pointer at 0x%04X leaves the MatAnimJoint block"
                     % (name, loc))
        words[(loc - anim_off) // 4] = (target - anim_off) // 4
        relocs.append((loc - anim_off) // 4)

    # -- the pack ------------------------------------------------------
    world = baker.world_verts()
    lo, hi = baker.bounds()
    cx, cy, cz = ((lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2,
                  (lo[2] + hi[2]) / 2)
    radius = max(math.sqrt((p[0] - cx) ** 2 + (p[1] - cy) ** 2 +
                           (p[2] - cz) ** 2) for p in world)

    # Which texture each MObj's batch draws. With no sprite array to step
    # through that is the whole of the MObj's, so tex_count below is 1 and
    # this is tex_first; -1 where the batch is untextured, which is every
    # batch of an emblem.
    batch_tex = [-1] * len(mobjs)
    for i, b in enumerate(baker.batches):
        if batch_mobj[i] >= 0 and batch_tex[batch_mobj[i]] < 0:
            batch_tex[batch_mobj[i]] = b["mat"][0]

    subs_blob = b"".join(
        # tex_first/tex_count: the sprite array a MatAnimJoint steps
        # through (fighter.h FPackMObjSub). Neither of these packs has
        # one -- the flags check above refuses a texture-driving MObjSub
        # -- so every MObj here is one frame, the batch's own.
        A.pack_mobjsub(s, batch_tex[k], 1)
        for k, s in enumerate(mobjs))
    joint_blob = struct.pack("<%dh" % (2 * len(mobjsubs)),
                             *[v for pair in joint_first for v in pair])
    batch_blob = struct.pack("<%dh" % len(batch_mobj), *batch_mobj)
    entry_blob = struct.pack("<%di" % len(entries), *entries)
    reloc_blob = struct.pack("<%dI" % len(relocs), *relocs)
    words_blob = struct.pack("<%dI" % len(words), *words)

    def align(b):
        return b + b"\0" * (-len(b) % 4)

    header_size = 128
    mobjhdr_size = 48
    off = header_size
    offsets = {}
    sections = [("joints", secs["joints"]), ("verts", secs["verts"]),
                ("tris", secs["tris"]), ("batches", secs["batches"]),
                ("texs", secs["texs"]), ("pals", secs["pals"]),
                ("texdata", secs["texdata"]),
                ("mobjhdr", b"\0" * mobjhdr_size),
                ("subs", subs_blob), ("mjoint", joint_blob),
                ("mbatch", batch_blob), ("mentry", entry_blob),
                ("mreloc", reloc_blob), ("mwords", words_blob)]
    for key, sec in sections:
        offsets[key] = off
        off += len(align(sec))
    mobjhdr = struct.pack("<12I", len(mobjs), offsets["subs"],
                          offsets["mjoint"], offsets["mbatch"],
                          offsets["mentry"], offsets["mwords"], len(words),
                          offsets["mreloc"], len(relocs),
                          1, 0, 0)      # one MatAnimJoint, as every pack but
                                  # the dead explosion's has
    assert len(mobjhdr) == mobjhdr_size, len(mobjhdr)

    body = b""
    for key, sec in sections:
        body += align(mobjhdr if key == "mobjhdr" else sec)

    header = struct.pack("<8s8I8I3ff8s8I", MAGIC,
                         len(baker.nodes), len(verts), len(tris),
                         len(baker.batches), 0, 0, 0, 0,
                         offsets["joints"], offsets["verts"],
                         offsets["tris"], offsets["batches"],
                         offsets["texs"], offsets["pals"],
                         offsets["texdata"], 0,
                         cx, cy, cz, radius, name.encode()[:8],
                         0, 0, 0, 0, 0, 0, 0, offsets["mobjhdr"])
    assert len(header) == header_size, len(header)
    # colors[mobj][color][track]; track 3 is light1 (ssb_assets.EXT_TRACKS).
    shown = ["%06X" % ((int(c[3]) >> 8) & 0xFFFFFF) for c in colors[0]]
    print("%s: DObjDesc 0x%04X, MObjSub 0x%04X, MatAnimJoint 0x%04X: "
          "%d joints, %d verts, %d tris, %d batches, %d MObjs, "
          "%d anim words, %d pointers, light1 %s"
          % (name, dobjs[name], subs[name], anim_off, len(baker.nodes),
             len(verts), len(tris), len(baker.batches), len(mobjs),
             len(words), len(relocs), ",".join(shown)))
    return header + body


def main():
    argv = sys.argv[1:]
    rom_path = M.ROM_DEFAULT
    name = None
    out = None
    if "--rom" in argv:
        rom_path = argv[argv.index("--rom") + 1]
    if "--name" in argv:
        name = argv[argv.index("--name") + 1]
    if "--out" in argv:
        out = argv[argv.index("--out") + 1]
    if "--list" in argv:
        for i, n in enumerate(EMBLEM_OF_KIND):
            print("%2d %s" % (i, n))
        return
    if not name or not out:
        sys.exit("--name <Emblem> --out <file.mdl> are required")
    if name not in EMBLEMS:
        sys.exit("%s is not one of relocData 35's emblems: %s"
                 % (name, ", ".join(EMBLEMS)))

    rom = open(rom_path, "rb").read()
    blob = pack_emblem(rom, name)
    d = os.path.dirname(out)
    if d:
        os.makedirs(d, exist_ok=True)
    with open(out, "wb") as fp:
        fp.write(blob)
    print("wrote %s (%d bytes)" % (out, len(blob)))


if __name__ == "__main__":
    main()

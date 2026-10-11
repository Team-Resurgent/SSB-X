#!/usr/bin/env python3
"""ssb64-dc: the damage slash's materials, against the ROM they came out of.

The slash (ef/efmanager.c:2486) is the first model in the
port whose *picture* moves. Its two MObjs each carry a sprite array --
eight frames for the streak, five for the flash -- and each is handed a
MatAnimJoint that steps `nGCAnimTrackTextureIDCurrent` through it while
the primitive colour fades. The pack answers that by carrying every frame
as a texture of its own (FPackMObjSub.tex_first/tex_count) and by having
src/dc/fighter.c compile one poly header per frame, so the runtime picks
a header instead of loading a tile.

Nothing on the target disagrees with any of that. A mis-sliced script
does not crash -- the game's parser walks into whatever the next effect
left behind and stops on some other opcode -- and a flipbook that packed
the same picture eight times looks like a slash that simply does not
animate. This is the check that would catch either:

  * the DObjDesc tree walks in the order the pack indexes MObjs by, and
    every batch's MObj belongs to the batch's own joint.
  * every MObjSub is inside the flag set src/dc/objdisplay.c carries, has
    a sprite array and takes no palette of its own.
  * the sliced, rebased MatAnimJoint words play *identically* to the ones
    still in the ROM, tic for tic, over the whole animation and past its
    end. This is the real check on the slice.
  * the frames the script reaches are the frames the pack carries, and no
    two of them hold the same texels. A flipbook of one picture would
    pass everything above.

Usage: python3 tools/check/slash_check.py [--rom <rom.z64>]
"""
import os
import struct
import sys

sys.path[:0] = [os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), d)
                for d in ("lib", "export", "check")]
import ssb_extract               # noqa: E402
import ssb_assets as A           # noqa: E402
import ssb_meshexport as M       # noqa: E402
import ssb_effectexport as S     # noqa: E402

# How far past the animation to replay it. The slash's longer script runs
# thirteen tics; twenty reaches the End command and the words after it.
FRAMES = 20


def tree_order(root):
    """The DObjs in gcGetTreeDObjNext order: child, then sibling, then up.

    gcAddMObjAll walks the tree that way and steps its `MObjSub ***` once
    per DObj, so the pack's joint index and the tree walk have to agree.
    """
    out = []

    def walk(n):
        for c in n.children:
            out.append(c)
            walk(c)

    walk(root)
    return out


def read_pack(blob):
    """The parts of a .mdl this check reads back, by fighter.h's layout."""
    hd = struct.unpack_from("<8s8I8I3ff8s8I", blob, 0)
    (njoint, nvert, ntri, nbatch, ntex, npal, nanim, texdata_len) = hd[1:9]
    offs = hd[9:17]
    off_mobjs = hd[-1]
    texs = [struct.unpack_from("<2I2H4B", blob, offs[4] + i * 16)
            for i in range(ntex)]
    batches = [struct.unpack_from("<2Hh2B2H4I", blob, offs[3] + i * 28)
               for i in range(nbatch)]
    mo = struct.unpack_from("<10I", blob, off_mobjs)
    subs = [struct.unpack_from(A.MOBJSUB_FMT,
                               blob, mo[1] + i * A.MOBJSUB_PACK_SIZE)
            for i in range(mo[0])]
    joint = struct.unpack_from("<%dh" % (2 * njoint), blob, mo[2])
    batch_mobj = struct.unpack_from("<%dh" % nbatch, blob, mo[3])
    # alt_count runs of mobj_count, and alt_count is 1 for every pack but
    # the dead explosion's (fighter.h FPackMObjs).
    entry = struct.unpack_from("<%di" % (mo[0] * mo[9]), blob, mo[4])
    words = list(struct.unpack_from("<%dI" % mo[6], blob, mo[5]))
    relocs = struct.unpack_from("<%dI" % mo[8], blob, mo[7])
    return {"njoint": njoint, "texdata": offs[6], "texs": texs,
            "batches": batches, "subs": subs, "joint": joint,
            "batch_mobj": batch_mobj, "entry": entry, "alts": mo[9],
            "words": words,
            "relocs": relocs, "blob": blob}


def check(rom):
    import pygfxd

    fails = []
    fid, mobj_off, dobj_off, _anim, mat_off = S.reloc_offsets(
        ("llEFCommonEffects1FileID",
         "llEFCommonEffects1DamageSlashMObjSub",
         "llEFCommonEffects1DamageSlashDObjDesc",
         "llEFCommonEffects1DamageSlashAnimJoint",
         "llEFCommonEffects1DamageSlashMatAnimJoint"))
    f = A.get_file(rom, fid, ssb_extract)
    entry = ssb_extract.read_entry(rom, ssb_extract.RELOC_SEG, fid)
    reloc = A.walk_reloc(f, entry["reloc_intern"])

    root, nodes = A.read_dobj_tree(f, reloc, dobj_off, dl_links=True)
    order = tree_order(root)
    if [n.index for n in order] != list(range(len(nodes))):
        fails.append("gcAddMObjAll's tree walk visits %s, not the DObjDesc "
                     "order the pack indexes by" % [n.index for n in order])

    mobjsubs = A.read_mobjsubs(f, reloc, mobj_off, len(nodes))
    for lst in mobjsubs:
        for sub in lst:
            if sub["flags"] & ~S.SLASH_MOBJ_OK:
                fails.append("MObjSub@0x%04X flags 0x%04X outside 0x%04X"
                             % (sub["off"], sub["flags"], S.SLASH_MOBJ_OK))
            if sub["palettes"]:
                fails.append("MObjSub@0x%04X takes a palette of its own"
                             % sub["off"])
            if not sub["sprites"]:
                fails.append("MObjSub@0x%04X has no sprite array"
                             % sub["off"])

    baker = A.MeshBaker(f, pygfxd, mobjsubs, mobj_batches=True)
    _v, tris, joints = baker.bake(root)
    joint_of = {}
    for j in joints:
        for b in range(j["batch_first"], j["batch_first"] + j["batch_count"]):
            joint_of[b] = j["node"].index
    for i, b in enumerate(baker.batches):
        m = b["mobj"]
        if m is None:
            fails.append("batch %d has no MObj" % i)
        elif m[0] != joint_of[i]:
            fails.append("batch %d is joint %d's but takes joint %d's MObj"
                         % (i, joint_of[i], m[0]))

    # -- the pack the build ships --------------------------------------
    pack = read_pack(S.pack_slash(rom))

    # What src/dc/fighter.c's reloc walk does at load, read back as a file
    # the parser can walk: the pointer words become byte offsets and the
    # reloc list becomes the map play_matanim reads them through.
    words = pack["words"]
    body = bytearray(struct.pack(">%dI" % len(words), *words))
    body_reloc = {}
    for w in pack["relocs"]:
        body_reloc[w * 4] = words[w] * 4
        struct.pack_into(">I", body, w * 4, words[w] * 4)

    scripts = []
    for j in range(len(nodes)):
        arr = reloc.get(mat_off + 4 * j)
        for k in range(len(mobjsubs[j])):
            scripts.append(None if arr is None else reloc.get(arr + 4 * k))

    def play(data, rl, off, frame):
        """...or the reason it could not be played. A block sliced short
        or an entry pointing at the wrong word does not come back as a
        wrong picture: the parser walks off the end or hits an opcode
        that is not one, and both are the failure this is looking for."""
        try:
            return A.play_matanim(data, rl, off, float(frame))
        except (ValueError, struct.error) as e:
            return "unplayable: %s" % e

    for k, s in enumerate(scripts):
        if s is None:
            continue
        for t in range(FRAMES):
            want = play(f, reloc, s, t)
            got = play(bytes(body), body_reloc, pack["entry"][k] * 4, t)
            if want != got:
                fails.append("MObj %d at tic %d plays %s packed, %s in the "
                             "ROM" % (k, t, got, want))

    # -- the flipbook --------------------------------------------------
    seen = []
    for k, s in enumerate(scripts):
        sub = pack["subs"][k]
        first, count = sub[8], sub[9]
        ids = set() if s is None else S.matanim_texture_ids(f, reloc, s)
        want = 1 if not ids else int(max(ids)) + 1
        if count != want:
            fails.append("MObj %d carries %d frames and its script reaches "
                         "%d" % (k, count, want))
        if s is not None and sorted(int(v) for v in ids) != \
                list(range(count)):
            fails.append("MObj %d's script reaches frames %s, not 0..%d"
                         % (k, sorted(int(v) for v in ids), count - 1))
        pictures = []
        for i in range(first, first + count):
            off, size = pack["texs"][i][0], pack["texs"][i][1]
            base = pack["texdata"] + off
            pictures.append(bytes(pack["blob"][base:base + size]))
        if len(set(pictures)) != count:
            fails.append("MObj %d's %d frames hold %d distinct pictures"
                         % (k, count, len(set(pictures))))
        seen.append((first, count, len(set(pictures)),
                     pack["texs"][first][2], pack["texs"][first][3]))

    for k, (first, count, distinct, w, h) in enumerate(seen):
        print("  MObj %d: %d frames from texture %2d, %d distinct, %dx%d"
              % (k, count, first, distinct, w, h))
    print("  %d joints, %d MObjs, %d batches, %d tris, %d matanim words, "
          "%d pointers" % (len(nodes), len(pack["subs"]),
                           len(pack["batches"]), len(tris),
                           len(words), len(pack["relocs"])))
    return fails


def main():
    argv = sys.argv[1:]
    rom_path = M.ROM_DEFAULT
    if "--rom" in argv:
        rom_path = argv[argv.index("--rom") + 1]
    fails = check(open(rom_path, "rb").read())
    if fails:
        for m in fails:
            print("slash_check: %s" % m)
        sys.exit("slash_check: %d problems" % len(fails))
    print("slash_check: the slash's MObjs, their sprite arrays and their "
          "MatAnimJoints are the ROM's")


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""ssb64-dc: the dead explosion's pack, against the ROM it came out of.

The burst a fighter leaves when he is knocked off the screen (ef/efmanager.c:4777) is the last effect model the port owed, and it
is the first that is more than another quad. Three things about it are
new, and each is a way to be silently wrong:

  * **Two reloc symbols are named the wrong way round.**
    dEFManagerDeadExplodeEffectDesc lists o_dobjsetup first and o_mobjsub
    second (ef/eftypes.h:20-21), and what it lists first is
    llEFCommonEffects2DeadExplodeDefault**MObjSub**. So the DObjDesc
    array is at the symbol called MObjSub and the MObjSub table at the
    one called DObjDesc -- which is what the decomp's own relocData
    source says ("MIS-TYPED", 84_EFCommonEffects2.c:1371 and :1549). The
    names come out of the decomp's generator and are not the port's to
    correct, so the first thing checked here is that they really are that
    way round: the block the exporter reads as a tree parses as one, and
    the block it reads as materials parses as those.

  * **The combiner lerps between two flat colours.** All three shards
    compute (PRIM - ENV) * TEXEL0 + ENV, and ENV is where the *player's*
    colour arrives -- the maker writes it into two of the three MObjs at
    runtime (efmanager.c:4825-4835). Every batch the port had baked
    before this one reduced to PRIM * TEXEL0 with ENV unread, so a reader
    that kept doing that would draw all four players' explosions the same
    and nothing would say so. The PVR draws the lerp as base colour
    PRIM - ENV plus offset colour ENV -- and a base colour cannot go
    negative, so where PRIM is the *darker* of the two in a channel that
    channel clamps and the lerp flattens. This is where that is measured:
    every player, every shard, every tic, worst channel, printed and held
    to CLAMP_MAX below. It is the port's one inexactness here.

  * **The descriptor names four MatAnimJoints and the game swaps
    between them.** No other EFDesc in the game is mutated before it is
    used. All four are baked side by side (FPackMObjs.alt_count) and each
    is replayed here against the block still in the ROM, tic for tic and
    past its end -- the same check tools/check/slash_check.py makes on the one
    the slash has, and for the same reason: a mis-sliced block is an
    unplayable script rather than a wrong colour.

And, as the slash's check does: the tree walks in the order the pack
indexes MObjs by, every batch's MObj belongs to the batch's own joint,
and every MObjSub is inside the flag set src/dc/objdisplay.c carries.

Usage: python3 tools/check/dexp_check.py [--rom <rom.z64>]
"""
import os
import re
import struct
import sys

sys.path[:0] = [os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), d)
                for d in ("lib", "export", "check")]
import ssb_extract               # noqa: E402
import ssb_assets as A           # noqa: E402
import ssb_meshexport as M       # noqa: E402
import ssb_effectexport as S     # noqa: E402
from slash_check import read_pack, tree_order   # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
DECOMP = os.environ.get("SSB_DECOMP_DIR", "/opt/ssb-decomp-re")
if not os.path.isdir(DECOMP):
    DECOMP = os.path.join(ROOT, "ssb-decomp-re")
DECOMP_FILE = os.path.join(DECOMP, "src", "ef", "efmanager.c")

# How far past the animation to replay it. The longest of the twelve
# scripts blocks for 30 tics and then 6; forty-five reaches the End
# command and the words after it.
FRAMES = 45
PLAYERS = 4

# The worst a channel of PRIM may fall short of the same channel of ENV,
# out of 255, before this check calls it a regression. The PVR's base
# colour is PRIM - ENV and cannot go negative, so a shortfall of N is a
# channel that draws N/255 too bright wherever the texel is at full
# intensity -- which, on these three textures, is most of what is on
# screen: their alpha-weighted mean intensity is 0.88, 0.80 and 0.72.
#
# The number is measured, not chosen: eight of the twelve player/shard
# pairs are exact, three fall short by 14 to 17, and one -- player 2's
# third shard, PRIM 60FF00 against ENV FFFF00 -- falls short by 159 in
# red. The bound is that worst case, so this is a tripwire on the data
# and on material_of rather than a quality bar. Making it exact needs a
# second pass over a complementary texture, which is the cost.
CLAMP_MAX = 159


def decomp_table(name):
    """One `u8 dEFManager...[] = { .. };` out of the decomp's own source.

    The port's copy of these six tables is held to the decomp's character
    for character by tools/check/efmanager_check.py, so reading the decomp here
    is reading the port -- and it is the ROM's authority rather than the
    port's for what the player colours are.
    """
    with open(DECOMP_FILE) as f:
        text = f.read()
    m = re.search(name + r"\[[^\]]*\]\s*=\s*\{([^}]*)\}", text)
    if m is None:
        raise SystemExit("dexp_check: %s names no %s" % (DECOMP_FILE, name))
    return [int(v.strip(), 16) for v in m.group(1).split(",") if v.strip()]


def env_of_dl(f, dl_off):
    """The RGB the display list's own gsDPSetEnvColor sets, or None.

    Read straight out of the flattened commands rather than through the
    baker, so that what the pack carries is compared against the file and
    not against the same code path that put it there.
    """
    dl = A.flatten_dl(f, dl_off)
    env = None
    for o in range(0, len(dl), 8):
        w0, w1 = struct.unpack_from(">II", dl, o)
        if (w0 >> 24) == 0xFB:          # G_SETENVCOLOR
            env = w1 >> 8
        elif (w0 >> 24) == 0xDF:        # G_ENDDL
            break
    return env


def check(rom):
    import pygfxd

    fails = []
    fid, dobj_off, mobj_off, anim_off = S.reloc_offsets(
        ("llEFCommonEffects2FileID",
         "llEFCommonEffects2DeadExplodeDefaultMObjSub",
         "llEFCommonEffects2DeadExplodeDefaultDObjDesc",
         "llEFCommonEffects2DeadExplodeDefaultAnimJoint"))
    mat_offs = S.reloc_offsets(
        tuple("llEFCommonEffects2DeadExplode%dMatAnimJoint" % k
              for k in (1, 2, 3, 4)))
    f = A.get_file(rom, fid, ssb_extract)
    entry = ssb_extract.read_entry(rom, ssb_extract.RELOC_SEG, fid)
    reloc = A.walk_reloc(f, entry["reloc_intern"])

    # -- the two swapped symbols ---------------------------------------
    # The name says MObjSub and the bytes are a DObjDesc array: four
    # entries whose ids are 0, 1, 1, 1 and then the terminator, the stand
    # and its three shards. Nothing else in the file parses that way at
    # that offset, and the other symbol's block does not.
    root, nodes = A.read_dobj_tree(f, reloc, dobj_off, dl_links=True)
    ids = tuple(n.joint_id for n in nodes)
    if ids != S.DEXP_JOINT_IDS:
        fails.append("the block at ...DefaultMObjSub reads joint ids %s, "
                     "not the tree's %s" % (ids, S.DEXP_JOINT_IDS))
    try:
        A.read_dobj_tree(f, reloc, mobj_off, dl_links=True)
    except Exception:                                  # noqa: BLE001
        pass
    else:
        fails.append("the block at ...DefaultDObjDesc also parses as a "
                     "DObjDesc tree; the two symbols may not be swapped "
                     "after all")

    order = tree_order(root)
    if [n.index for n in order] != list(range(len(nodes))):
        fails.append("gcAddMObjAll's tree walk visits %s, not the DObjDesc "
                     "order the pack indexes by" % [n.index for n in order])
    # efmanager.c:4818-4823: the maker reaches the first shard as
    # dobj->child and the third as dobj->child->sib_next->sib_next.
    kids = [n.index for n in nodes[0].children]
    if kids != [1, 2, 3]:
        fails.append("the stand's children are %s; the maker colours "
                     "child and child->sib_next->sib_next" % kids)

    mobjsubs = A.read_mobjsubs(f, reloc, mobj_off, len(nodes))
    for lst in mobjsubs:
        for sub in lst:
            if sub["flags"] & ~S.DEXP_MOBJ_OK:
                fails.append("MObjSub@0x%04X flags 0x%04X outside 0x%04X"
                             % (sub["off"], sub["flags"], S.DEXP_MOBJ_OK))
            if sub["sprites"] or sub["palettes"]:
                fails.append("MObjSub@0x%04X carries a sprite array or a "
                             "palette" % sub["off"])
    if [len(lst) for lst in mobjsubs] != [0, 1, 1, 1]:
        fails.append("the MObjSub table gives the joints %s MObjs, not "
                     "none and one each" % [len(l) for l in mobjsubs])

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
    pack = read_pack(S.pack_deadexplode(rom))

    # -- the environment colour ----------------------------------------
    # FPackBatch is (tri_first, tri_count, tex, shaded, joint, bucket,
    # alpha_src, prim, light1, light2, env).
    baked_env = {}
    for i, b in enumerate(pack["batches"]):
        node = nodes[joint_of[i]]
        want = env_of_dl(f, node.dl_links[0][1])
        if not (b[5] & A.FPACK_ENVLERP):
            fails.append("batch %d does not lerp between ENV and PRIM; the "
                         "player's colour is the ENV" % i)
        if want is None:
            fails.append("joint %d's display list sets no environment "
                         "colour" % node.index)
        elif b[10] != want:
            fails.append("batch %d carries ENV %06X and its display list "
                         "sets %06X" % (i, b[10], want))
        baked_env[i] = want

    # -- the four MatAnimJoints ----------------------------------------
    # What src/dc/fighter.c's reloc walk does at load, read back as a file
    # the parser can walk (tools/check/slash_check.py's own trick).
    words = pack["words"]
    body = bytearray(struct.pack(">%dI" % len(words), *words))
    body_reloc = {}
    for w in pack["relocs"]:
        body_reloc[w * 4] = words[w] * 4
        struct.pack_into(">I", body, w * 4, words[w] * 4)

    nmobj = len(pack["subs"])
    if len(pack["entry"]) != PLAYERS * nmobj:
        fails.append("the pack holds %d MatAnimJoint entries for %d MObjs "
                     "and %d players" % (len(pack["entry"]), nmobj, PLAYERS))

    def play(data, rl, off, frame):
        try:
            return A.play_matanim(data, rl, off, float(frame))
        except (ValueError, struct.error) as e:
            return "unplayable: %s" % e

    prim_at = {}
    for a, mat_off in enumerate(mat_offs):
        scripts = []
        for j in range(len(nodes)):
            arr = reloc.get(mat_off + 4 * j)
            for k in range(len(mobjsubs[j])):
                scripts.append(None if arr is None
                               else reloc.get(arr + 4 * k))
        for k, s in enumerate(scripts):
            if s is None:
                fails.append("player %d's MatAnimJoint drives no MObj %d"
                             % (a, k))
                continue
            for t in range(FRAMES):
                want = play(f, reloc, s, t)
                got = play(bytes(body), body_reloc,
                           pack["entry"][a * nmobj + k] * 4, t)
                if want != got:
                    fails.append("player %d's MObj %d at tic %d plays %s "
                                 "packed, %s in the ROM" % (a, k, t, got, want))
                    break
                prim_at[(a, k, t)] = want["ext"].get(0)

    # -- the lerp cannot go negative -----------------------------------
    # The PVR's base colour is PRIM - ENV and cannot; a channel where ENV
    # is the brighter clamps to zero and loses that much of the lerp
    # (src/dc/fighter.c material_of, and lbCommonSpriteColorsOf before
    # it). ENV is the maker's own table for the first and third shards
    # and the display list's for the middle one.
    child = [decomp_table("dEFManagerDeadExplodeEnvColorChild" + c)
             for c in "RGB"]
    sib = [decomp_table("dEFManagerDeadExplodeEnvColorSibling" + c)
           for c in "RGB"]
    worst = {}
    for a in range(PLAYERS):
        for k in range(nmobj):
            if k == 0:
                env = (child[0][a], child[1][a], child[2][a])
            elif k == nmobj - 1:
                env = (sib[0][a], sib[1][a], sib[2][a])
            else:
                e = baked_env.get(k) or 0
                env = ((e >> 16) & 0xFF, (e >> 8) & 0xFF, e & 0xFF)
            for t in range(FRAMES):
                p = prim_at.get((a, k, t))
                if p is None:
                    continue
                p = int(p) & 0xFFFFFFFF
                rgb = ((p >> 24) & 0xFF, (p >> 16) & 0xFF, (p >> 8) & 0xFF)
                d = max(e - c for c, e in zip(rgb, env))
                if d > worst.get((a, k), 0):
                    worst[(a, k)] = d
    for (a, k), d in sorted(worst.items()):
        if d > CLAMP_MAX:
            fails.append("player %d shard %d clamps by %d of 255, past the "
                         "%d this check records" % (a, k, d, CLAMP_MAX))

    # -- what it came to -----------------------------------------------
    for i, b in enumerate(pack["batches"]):
        print("  batch %d: joint %d, %d tris, texture %d, ENV %06X, "
              "PRIM %08X" % (i, b[4], b[1], b[2], b[10], b[7]))
    for a in range(PLAYERS):
        print("  player %d: ENV child %02X%02X%02X sibling %02X%02X%02X, "
              "PRIM at tic 0 %s"
              % (a, child[0][a], child[1][a], child[2][a],
                 sib[0][a], sib[1][a], sib[2][a],
                 "/".join("%06X" % ((int(prim_at[(a, k, 0)]) >> 8) & 0xFFFFFF)
                          for k in range(nmobj) if (a, k, 0) in prim_at)))
    print("  base-colour clamp, worst channel of 255, player by shard: %s"
          % "  ".join("p%d %s" % (a, "/".join(str(worst.get((a, k), 0))
                                              for k in range(nmobj)))
                      for a in range(PLAYERS)))
    print("  %d joints, %d MObjs, %d batches, %d tris, %d matanim words in "
          "%d alternates" % (len(nodes), nmobj, len(pack["batches"]),
                             len(tris), len(words), PLAYERS))
    return fails


def main():
    argv = sys.argv[1:]
    rom_path = M.ROM_DEFAULT
    if "--rom" in argv:
        rom_path = argv[argv.index("--rom") + 1]
    fails = check(open(rom_path, "rb").read())
    if fails:
        for m in fails:
            print("dexp_check: %s" % m)
        sys.exit("dexp_check: %d problems" % len(fails))
    print("dexp_check: the dead explosion's tree, its three environment "
          "colours and its four MatAnimJoints are the ROM's")


if __name__ == "__main__":
    main()

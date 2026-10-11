#!/usr/bin/env python3
"""ssb64-dc: the eleven screen wipes of lb/lbtransition.c, packed.

`dLBTransitionDescs` (lbtransition.c:15) names eleven relocData files, one
per wipe, and each is the same three things: a DObjDesc tree, one display
list per joint, and an AnimJoint animation that folds the tree up. The game
loads whichever one `mnVSResultsFuncStart` rolled into a heap the size of
the largest and throws it away when the wipe ends.

What makes them different from every other model this repo bakes is their
texture. `gsDPSetTextureImage` points into *segment 1*, which
lbTransitionProcDisplay binds to sLBTransitionPhotoHeap -- a copy of the
last frame drawn, taken at scene start. There is nothing in the file to
bake, so the baker interns one extern texture (ssb_assets.EXTERN_TIMG) and
src/dc/lbtransition.c supplies the picture at runtime.

One pack per wipe, in the layout src/dc/fighter.h reads, with exactly one
animation: the file's own AnimJoint table.

Usage: python3 tools/export/ssb_transexport.py --name Star --out star.pack
                                        [--rom <rom.z64>]
The title screen's two bare animation trees (TREES) take the same
arguments: --name TitleLabels or --name TitlePressStart.
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

# The picture the wipes are skinned with, in the game's own numbers:
# lbtransition.c:216 allocates 300 * 220 * sizeof(u16) and copies that
# rectangle out of the framebuffer. src/dc/lbtransition.c has the same two.
PHOTO_W, PHOTO_H = 300, 220

# lbtransition.c:15 dLBTransitionDescs, in its own order: the transition id
# the game rolls (syUtilsRandIntRange over ARRAY_COUNT) indexes this list,
# so the port's table has to keep it. The comment beside each is the
# decomp's own name for the effect.
TRANSITIONS = [
    ("Aeroplane", "Paper Plane"),
    ("Check", "Checkered Board"),
    ("Gakubuthi", "Falling Board"),
    ("Kannon", "Doors"),
    ("Star", "Star"),
    ("Sudare1", "Vertical Lines"),
    ("Sudare2", "Diagonal Lines"),
    ("Camera", "Camera Shutter"),
    ("Block", "Collapsing Blocks"),
    ("RotScale", "Rotating Frame Zooming Out"),
    ("Curtain", "Curtain"),
]


def find_file(name):
    """(file id, source text) for relocData's NNN_LBTransition<name>.c."""
    for fn in sorted(os.listdir(M.RELOC_DIR)):
        m = re.fullmatch(r"(\d+)_LBTransition%s\.c" % re.escape(name), fn)
        if m:
            return int(m.group(1)), M.read_source(os.path.join(M.RELOC_DIR,
                                                               fn))
    sys.exit("no relocData file for transition %s" % name)


def symbol_offsets(src, name, kind):
    """Every dLBTransition<name>_<kind>_0xNNNN offset in the source, sorted.

    The extractor names each block by the file offset it came from, so the
    offsets the game's LBTransitionDesc would carry are readable straight
    off the symbols -- there is no table of them anywhere else.
    """
    pat = r"dLBTransition%s_%s_0x([0-9A-Fa-f]+)" % (re.escape(name), kind)
    return sorted({int(h, 16) for h in re.findall(pat, src)})


# DObjDesc trees that carry no geometry at all: the animation is the whole
# point, and a scene reads the joints' translate and scale off the DObjs
# every tic to place sprites. mn/mncommon/mntitle.c's labels and PRESS
# START are the two (mnTitleMakeLabels, mnTitleMakePressStart, which hand
# them to gcSetupCommonDObjs and gcAddAnimJointAll). The offsets are
# include/reloc_data.us.h's llMNTitle*DObjDesc and llMNTitle*AnimJoint.
TREES = {
    "TitleLabels": (167, 0x26130, 0x25350),
    "TitlePressStart": (167, 0x262C0, 0x258D0),
    # the two the title uses only after the opening movie.
    # The logo's is a root and four children (the three cutout sprites'
    # joints and the full logo's, mnTitleMakeLogo's else arm and
    # mnTitleLogoProcUpdate's child->sib_next x3); the fire's is a root
    # with three limbs of two, and mnTitleMakeLogoFireParticles hangs the
    # bank's generator on the second limb's leaf (child->sib_next->child).
    "TitleLogo": (167, 0x26020, 0x251D0),
    "TitleFire": (167, 0x28EB0, 0x29010),
}


def pack_transition(rom, name):
    fid, src = find_file(name)
    dobjs = symbol_offsets(src, name, "DObjDesc")
    anims = symbol_offsets(src, name, "AnimJoint")
    if len(dobjs) != 1:
        sys.exit("%s: %d DObjDesc blocks, expected one" % (name, len(dobjs)))
    if not anims:
        sys.exit("%s: no AnimJoint block" % name)
    # The lowest AnimJoint offset is the pointer table lbTransitionMake-
    # Transition hands gcAddAnimJointAll; everything above it is the
    # per-joint scripts that table points at.
    return pack_tree(rom, fid, name, dobjs[0], anims[0], photo=True)


def pack_tree(rom, fid, name, dobj_off, anim_off, photo):
    """One DObjDesc tree and its AnimJoint table as a pack. `photo` is a
    transition's: one segment-1 texture, and the animation runs to the end
    of the file. Without it the tree must be bare -- no display list on
    any joint -- and the animation is cut to the words its scripts reach,
    because the file goes on past it with other things."""
    import pygfxd

    f = A.get_file(rom, fid, ssb_extract)
    entry = ssb_extract.read_entry(rom, ssb_extract.RELOC_SEG, fid)
    reloc = A.walk_reloc(f, entry["reloc_intern"])
    root, nodes = A.read_dobj_tree(f, reloc, dobj_off)

    # PHOTO_W x PHOTO_H is lbtransition.c:216's own allocation, and the
    # size the UVs come out normalised against.
    baker = A.MeshBaker(f, pygfxd, [[] for _ in nodes],
                        extern_size=(PHOTO_W, PHOTO_H) if photo else None)
    verts, tris, joints = baker.bake(root)
    if not photo:
        if baker.batches or baker.textures:
            sys.exit("%s: %d batches, %d textures; a bare tree draws "
                     "nothing" % (name, len(baker.batches),
                                  len(baker.textures)))
    elif not any(t.get("extern") for t in baker.textures):
        sys.exit("%s: no segment-1 texture; this is not a transition" % name)
    elif len(baker.textures) != 1:
        sys.exit("%s: %d textures; a transition draws the photocopy and "
                 "nothing else" % (name, len(baker.textures)))

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

    # -- the one animation ---------------------------------------------
    # Unlike a fighter's, this animation is not a file of its own: it is
    # the tail of the model file. Everything from the pointer table on is
    # carried as the animation's words, and the pointers -- both the
    # table's and the scripts' own Jump/SetAnim/SetInterp targets -- are
    # rebased onto it, so fighter_init's reloc walk (src/dc/fighter.c:100)
    # makes them addresses the way lb/lbreloc.c does for the game.
    njoints = len(baker.nodes)
    table = [reloc.get(anim_off + 4 * k) for k in range(njoints)]
    body_off = anim_off + 4 * njoints
    if len(f) % 4:
        sys.exit("%s: file %d is %d bytes, not a whole number of words"
                 % (name, fid, len(f)))
    entries = [-1] * njoints
    floats = set()
    end = body_off
    for k, t in enumerate(table):
        if t is None:
            continue
        if not anim_off <= t < len(f):
            sys.exit("%s: joint %d's script at 0x%04X is outside the "
                     "animation at 0x%04X" % (name, k, t, anim_off))
        # Walk it once before it ships: the game's parser has no default
        # case, so an opcode it does not know is an infinite loop on the
        # target rather than an error (ssb_meshexport.read_anim_ex).
        seen, _ = A.animjoint_walk(f, reloc, t, floats)
        end = max([end] + [w + 4 for w in seen])
        entries[k] = (t - anim_off) // 4
    if photo:
        end = len(f)
    words = list(struct.unpack(">%dI" % ((end - anim_off) // 4),
                               f[anim_off:end]))
    relocs = []
    for loc, target in sorted(reloc.items()):
        if loc < body_off or loc >= end:
            continue
        if not anim_off <= target < end:
            sys.exit("%s: pointer at 0x%04X leaves the animation" %
                     (name, loc))
        words[(loc - anim_off) // 4] = (target - anim_off) // 4
        relocs.append((loc - anim_off) // 4)

    world = baker.world_verts()
    cx = cy = cz = radius = 0.0
    if world:
        lo, hi = baker.bounds()
        cx, cy, cz = ((lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2,
                      (lo[2] + hi[2]) / 2)
        radius = max(math.sqrt((p[0] - cx) ** 2 + (p[1] - cy) ** 2 +
                               (p[2] - cz) ** 2) for p in world)

    words_blob = struct.pack("<%dI" % len(words), *words)
    entries_blob = struct.pack("<%di" % njoints, *entries)
    reloc_blob = struct.pack("<%dI" % len(relocs), *relocs)

    def align(b):
        return b + b"\0" * (-len(b) % 4)

    header_size = 128
    off = header_size
    offsets = {}
    body = b""
    sections = [("joints", secs["joints"]), ("verts", secs["verts"]),
                ("tris", secs["tris"]), ("batches", secs["batches"]),
                ("texs", secs["texs"]), ("pals", secs["pals"]),
                ("texdata", secs["texdata"]),
                ("anims", b"\0" * (ANIM_NAME_LEN + 24)),
                ("entries", entries_blob + reloc_blob),
                ("words", words_blob)]
    for key, sec in sections:
        offsets[key] = off
        off += len(align(sec))
    directory = struct.pack("<%dsIIIIII" % ANIM_NAME_LEN,
                            name.encode()[:ANIM_NAME_LEN - 1],
                            offsets["words"], len(words),
                            offsets["entries"], 1,   # FPACK_ANIM_ANIMJOINT
                            offsets["entries"] + len(entries_blob),
                            len(relocs))
    for key, sec in sections:
        body += align(directory if key == "anims" else sec)

    header = struct.pack("<8s8I8I3ff8s8I", MAGIC,
                         njoints, len(verts), len(tris),
                         len(baker.batches), len(baker.textures),
                         len(baker.palettes), 1, len(secs["texdata"]),
                         offsets["joints"], offsets["verts"],
                         offsets["tris"], offsets["batches"],
                         offsets["texs"], offsets["pals"],
                         offsets["texdata"], offsets["anims"],
                         cx, cy, cz, radius, name.encode()[:8],
                         0, 0, 0, 0, 0, 0, 0, 0)
    assert len(header) == header_size, len(header)
    print("%s: file %d, DObjDesc 0x%04X, AnimJoint 0x%04X: %d joints, "
          "%d verts, %d tris, %d batches, %d driven joints, %d anim words, "
          "%d pointers"
          % (name, fid, dobj_off, anim_off, njoints, len(verts), len(tris),
             len(baker.batches), sum(1 for e in entries if e >= 0),
             len(words), len(relocs)))
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
        for i, (n, what) in enumerate(TRANSITIONS):
            print("%2d %-10s %s" % (i, n, what))
        return
    if not name or not out:
        sys.exit("--name <Transition> --out <file.pack> are required")
    if name in TREES:
        rom = open(rom_path, "rb").read()
        fid, dobj_off, anim_off = TREES[name]
        blob = pack_tree(rom, fid, name, dobj_off, anim_off, photo=False)
    elif name not in [n for n, _ in TRANSITIONS]:
        sys.exit("%s is not one of dLBTransitionDescs: %s"
                 % (name, ", ".join(n for n, _ in TRANSITIONS)))
    else:
        rom = open(rom_path, "rb").read()
        blob = pack_transition(rom, name)
    d = os.path.dirname(out)
    if d:
        os.makedirs(d, exist_ok=True)
    with open(out, "wb") as fp:
        fp.write(blob)
    print("wrote %s (%d bytes)" % (out, len(blob)))


if __name__ == "__main__":
    main()

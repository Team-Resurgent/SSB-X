#!/usr/bin/env python3
"""ssb64-dc: one DObjDesc tree out of a relocData file, packed as a model.

The general form of ssb_arrowexport.py: a scene that reaches a small model
as `lbRelocGetFileData(DObjDesc*, files[n], &ll<Name>DObjDesc)` -- a tree of
plain display lists, optionally with an `AObjEvent32 **` table beside it
(gcAddAnimJointAll) -- and has no other way to get it on a Dreamcast, where
there is no relocData file to point into. The baked pack is the same
FPack the arrows are (src/dc/fighter.h), instantiated by
dc_model_add_dobjs, and the table becomes the pack's one animation the way
the arrows' does.

Unlike the arrows, this takes textures and palettes as they come (the
baker carries them), and no seeded render state: the file's own display
lists set what they need. --noz is for a scene whose display proc turns
the Z-buffer off around the draw (mvOpeningCliffHillsProcDisplay).

Usage: python3 tools/export/ssb_scenemodelexport.py --file 68 --dobjdesc 0x37a0 \\
           --name CliffHil --noz --out romdisk/mvopeningcliffhills.mdl
       add --animjoint 0x6850 for a model with an AnimJoint table (or
       --anim 0x6850@70 for one kept in another file; both repeat, each
       an animation of the pack), and
       --entries 0,4,39-72 to bake only those entries of the DObjDesc
       array (a tree too big for one pack), --dllinks for a tree whose DObjs carry DObjDLLink arrays
       (gcDrawDObjTreeDLLinksForGObj), --animend <off> where the file goes on to other data after it.

       --dl 0x24708 in place of --dobjdesc for a scene that binds a LONE
       display list to one DObj (gcAddDObjForGObj) with no DObjDesc at
       all -- see one_node_tree below. Combine with --dllinks where that
       block is a DObjDLLink array rather than commands, or with
       --dlhead 1 where the scene draws that DObj with gcDrawDObjDLHead1
       rather than gcDrawDObjDLHead0.

       --mobjsub 0x042f8 --matanim 0x08788 where the scene calls
       gcAddMObjAll / gcAddMatAnimJointAll beside its tree: the model's
       materials and the scripts that move them, packed as the
       FPackMObjs ssb_emblemexport.py bakes and src/dc/objmodel.c's
       dc_model_add_mobjs_alt hands back to those same two functions.
"""
import math
import os
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

# MObjSub.flags the port's material carries: the two light colours, the
# primitive and the environment. ssb_emblemexport.py's own note says the
# rest -- gcDrawMObjForDObj's texture selection, TLUT load and tile
# resize are not ported, and a pack that needed them would be silently
# wrong. Every MObjSub the opening room reaches is 0x0200 (PRIMCOLOR) or
# 0x1200 (PRIMCOLOR | LIGHT1), inside this.
MOBJ_FLAGS_KNOWN = (A.MOBJ_FLAG_LIGHT1 | A.MOBJ_FLAG_LIGHT2 |
                    A.MOBJ_FLAG_PRIMCOLOR | A.MOBJ_FLAG_ENVCOLOR)


def read_anim(rom, njoints, default_fid, f, reloc, off, fid, end_off,
              direct=False):
    """One AnimJoint animation, its table at `off` in file `fid` (or in the
    model's own file when that is None), as (words, entries, relocs)
    rebased onto its own first word the way ssb_arrowexport.py carries the
    arrows'.

    `direct` is the other of the object system's two ways in: a scene
    that calls gcAddDObjAnimJoint(dobj, script, 0.0F) rather than
    gcAddAnimJointAll(gobj, table, 0.0F) hands over the AObjEvent32
    ITSELF, not a per-joint array of pointers to one. The room's tissue
    box and Master Hand's shadow are both that shape, and read as a
    table they come out empty -- the first word of the block is a
    command, not a relocated pointer, so every entry is -1 and the walk
    never starts. The script binds to joint 0, which is the only joint a
    one-DObj tree has."""
    if fid is not None and fid != default_fid:
        rom_f = A.get_file(rom, fid, ssb_extract)
        ent = ssb_extract.read_entry(rom, ssb_extract.RELOC_SEG, fid)
        f, reloc = rom_f, A.walk_reloc(rom_f, ent["reloc_intern"])
    if direct:
        if njoints != 1:
            sys.exit("scenemodel: --animdl binds one script to joint 0, "
                     "and this tree has %d joints" % njoints)
        table, body_off = [off], off
    else:
        table = [reloc.get(off + 4 * k) for k in range(njoints)]
        body_off = off + 4 * njoints
    if len(f) % 4:
        sys.exit("scenemodel: file is not a whole number of words")
    # A file that holds several models goes on after the animation with
    # other data (display lists, the next tree), so the words end where
    # the scripts' own walk does -- or at --animend, if given.
    floats = set()
    extent = body_off
    entries = [-1] * njoints
    for k, t in enumerate(table):
        if t is None:
            continue
        if not off <= t < len(f):
            sys.exit("scenemodel: joint %d's script at 0x%04X is outside "
                     "the animation at 0x%04X" % (k, t, off))
        seen, fl = A.animjoint_walk(f, reloc, t, floats)
        extent = max(extent, max(seen | fl) + 4)
        entries[k] = (t - off) // 4
    end = end_off if end_off is not None else extent
    words = list(struct.unpack(">%dI" % ((end - off) // 4), f[off:end]))
    relocs = []
    for loc, target in sorted(reloc.items()):
        if loc < body_off or loc >= end:
            continue
        if target < off:
            sys.exit("scenemodel: pointer at 0x%04X leaves the animation"
                     % loc)
        words[(loc - off) // 4] = (target - off) // 4
        relocs.append((loc - off) // 4)
    return words, entries, relocs


def one_node_tree(f, reloc, off, dl_links, dlhead=0):
    """The tree `gcAddDObjForGObj(gobj, <block>)` builds: one DObj at the
    identity carrying a display list, and nothing else.

    A scene reaches a lone display list this way rather than through a
    DObjDesc array -- the room's Sunlight, Outside, Haze, Tissues and
    Master Hand's shadow all do (mv/mvopening/mvopeningroom.c), and so
    does the magnifying glass, whose exporter built this by hand first
    (tools/export/ssb_magnifyexport.py). There is no DObjDesc to read, so the
    node's placement is the identity and the scene's own
    gcAddXObjForDObjFixed is what moves it, exactly as on the N64.

    `dl_links` reads the block as a DObjDLLink array instead of as
    commands -- the same trap read_dobj_tree documents, and three of the
    room's five are one.

    `dlhead` is which of the four DL heads the scene's display proc
    queues that one list into: gcDrawDObjDLHead0 says 0 and
    gcDrawDObjDLHead1 says 1. It reaches the baker as a one-entry
    DObjDLLink list, which is the same thing the heads are -- and it
    matters, because a display list that sets no render mode of its own
    (Master Hand's shadow sets none) has only the head id left to say
    which PVR list it belongs in, and MeshBaker.bucket_of reads heads 1
    and 3 as translucent."""
    root = A.DObjNode(-1, -1, None, (0.0, 0.0, 0.0), (0.0, 0.0, 0.0),
                      (1.0, 1.0, 1.0))
    node = A.DObjNode(0, 0, None if (dl_links or dlhead) else off,
                      (0.0, 0.0, 0.0), (0.0, 0.0, 0.0), (1.0, 1.0, 1.0))
    if dlhead and dl_links:
        sys.exit("scenemodel: --dllinks carries its own head ids, so "
                 "--dlhead has nothing to say about it")
    if dlhead:
        node.dl_links.append((dlhead, off))
    elif dl_links:
        k = 0
        while True:
            list_id = struct.unpack_from(">i", f, off + k * 8)[0]
            if list_id == 4:
                break
            dl = reloc.get(off + k * 8 + 4)
            if dl is not None:
                node.dl_links.append((list_id, dl))
            k += 1
            if k > 16:
                sys.exit("scenemodel: unterminated DObjDLLink array at "
                         "0x%04X" % off)
        if not node.dl_links:
            sys.exit("scenemodel: the DObjDLLink array at 0x%04X names no "
                     "display list" % off)
    node.parent = root
    root.children.append(node)
    return root, [node]


def read_matanim(f, reloc, off, rows, nmobjs):
    """The `AObjEvent32 ***` gcAddMatAnimJointAll takes, as (words,
    entries, relocs) -- one entry per MObj in the pack's global order.

    objanim.c:190-219 says the shape: entry j of this table is the DObj
    built from DObjDesc[j], and is either NULL or an `AObjEvent32 **`
    walked once per MObj on that joint, in the order gcAddMObjAll put
    them there. So the table is indexed by JOINT and stepped by MOBJ,
    and `rows` -- (the joint's index in the FILE's DObjDesc array, how
    many MObjs it carries) per joint of the pack, in pack order -- is
    what ties the two together. The two indices differ only for a tree
    split across several packs (--entries), where the pack's joint n
    was the file's entry rows[n][0].

    The words are the scripts themselves, carried and rebased onto
    their own first word the way read_anim carries an AnimJoint's; the
    extent is what the MatAnimJoint parser actually visits
    (animjoint_walk's `mat` arm, which is gcParseMObjMatAnimJoint's
    stream rather than the joint parser's), never a guessed end. The
    two pointer tables are NOT carried: src/dc/objmodel.c's
    dc_model_add_mobjs_alt builds both back out of mobj_joint and these
    entries before it hands them to the game's own walk."""
    entries = [-1] * nmobjs
    scripts, k = [], 0
    for orig_j, count in rows:
        row = reloc.get(off + 4 * orig_j)
        for i in range(count):
            s = reloc.get(row + 4 * i) if row is not None else None
            scripts.append((k, s))
            k += 1
    visited = set()
    for _, s in scripts:
        if s is None:
            continue
        seen, floats = A.animjoint_walk(f, reloc, s, mat=True)
        visited |= seen | floats
    if not visited:
        sys.exit("scenemodel: the MatAnimJoint at 0x%04X names no script"
                 % off)
    lo, hi = min(visited), max(visited) + 4
    words = list(struct.unpack(">%dI" % ((hi - lo) // 4), f[lo:hi]))
    for k, s in scripts:
        if s is None:
            continue
        if not lo <= s < hi:
            sys.exit("scenemodel: MObj %d's script at 0x%04X is outside "
                     "the MatAnimJoint block 0x%04X..0x%04X"
                     % (k, s, lo, hi))
        entries[k] = (s - lo) // 4
    relocs = []
    for loc, target in sorted(reloc.items()):
        if not lo <= loc < hi:
            continue
        if not lo <= target < hi:
            sys.exit("scenemodel: pointer at 0x%04X leaves the "
                     "MatAnimJoint block" % loc)
        words[(loc - lo) // 4] = (target - lo) // 4
        relocs.append((loc - lo) // 4)
    return words, entries, relocs


def pack_model(rom, fid, dobjdesc_off, anims, name, noz,
               dl_links=False, entries_sel=None, bare_dl=False, dlhead=0,
               mobjsub_off=None, matanim_off=None):
    import pygfxd

    f = A.get_file(rom, fid, ssb_extract)
    entry = ssb_extract.read_entry(rom, ssb_extract.RELOC_SEG, fid)
    reloc = A.walk_reloc(f, entry["reloc_intern"])

    if bare_dl:
        entries_sel = None
    if entries_sel is not None:
        # A tree over fighter.h's FIGHTER_MAX_JOINTS goes out as several
        # packs (scstaffroll.c's glyphs do the same): each takes only the
        # listed entries of the DObjDesc array, copied after the file's
        # end with a terminator, so the parent-by-id rule (a node's parent
        # is whatever last bound the slot below its id) still resolves
        # among the ones kept. The reloc entries that matter (each
        # entry's display list pointer) move with them.
        f = bytearray(f)
        reloc = dict(reloc)
        new_off = (len(f) + 3) & ~3
        f.extend(b"\0" * (new_off - len(f)))
        for idx in entries_sel:
            src = dobjdesc_off + idx * A.DOBJDESC_SIZE
            dst = len(f)
            f.extend(f[src:src + A.DOBJDESC_SIZE])
            for word in range(0, A.DOBJDESC_SIZE, 4):
                if src + word in reloc:
                    reloc[dst + word] = reloc[src + word]
        term = len(f)
        f.extend(b"\0" * A.DOBJDESC_SIZE)
        struct.pack_into(">I", f, term, A.DOBJ_ARRAY_MAX)
        f = bytes(f)
        dobjdesc_off = new_off
    if bare_dl:
        root, nodes = one_node_tree(f, reloc, dobjdesc_off, dl_links, dlhead)
    else:
        root, nodes = A.read_dobj_tree(f, reloc, dobjdesc_off,
                                       dl_links=dl_links)
    # The materials, where the scene calls gcAddMObjAll beside its tree.
    # Read before the bake, because which MObj drew a batch is part of
    # its material key (mobj_batches below) and the baker wants them.
    # Pack joint n was the FILE's DObjDesc entry orig_of[n]. They differ
    # only under --entries, and the MObjSub and MatAnimJoint tables are
    # both indexed by the FILE's number, so a split tree has to carry
    # this across the renumbering or every material lands on the wrong
    # joint (or, more likely, on none).
    orig_of = list(entries_sel) if entries_sel is not None else None
    if mobjsub_off is not None:
        mobjsubs = A.read_mobjsubs(f, reloc, mobjsub_off,
                                   max(orig_of) + 1 if orig_of else len(nodes))
        if orig_of:
            mobjsubs = [mobjsubs[k] for k in orig_of]
        for lst in mobjsubs:
            for sub in lst:
                # The same refusal ssb_emblemexport.py makes, for the
                # same reason: gcDrawMObjForDObj's texture-selecting,
                # TLUT-loading and tile-resizing jobs are not ported, so
                # a pack whose MObjSub asks for them would come out
                # silently wrong rather than loudly unbuilt.
                if sub["sprites"] or sub["palettes"]:
                    sys.exit("scenemodel: MObjSub@0x%04X selects textures "
                             "or palettes; the port's materials are colour "
                             "only" % sub["off"])
                if sub["flags"] & ~MOBJ_FLAGS_KNOWN:
                    sys.exit("scenemodel: MObjSub@0x%04X has flags 0x%04X, "
                             "outside the colour set 0x%04X"
                             % (sub["off"], sub["flags"], MOBJ_FLAGS_KNOWN))
    else:
        mobjsubs = [[] for _ in nodes]
    # bake_retry: a vertex can sample past a tile's clamp extent
    # (ssb_itemmodelexport.py does the same)
    baker = A.bake_retry(
        lambda force: A.MeshBaker(f, pygfxd, mobjsubs, force_extent=force,
                                  mobj_batches=mobjsub_off is not None),
        lambda b: b.bake(root))
    verts, tris, joints = baker.verts, baker.tris, baker.joints
    if not tris:
        sys.exit("scenemodel: file %d @0x%X baked no triangles"
                 % (fid, dobjdesc_off))

    joint_of = {}
    for j in joints:
        for b in range(j["batch_first"], j["batch_first"] + j["batch_count"]):
            joint_of[b] = j["node"].index
    flag = A.FPACK_NOZ if noz else 0
    secs = K.model_sections(
        [(n.parent.index, n.translate, n.rotate, n.scale)
         for n in baker.nodes],
        verts, tris,
        [(b["tri_first"], b["tri_count"], b["mat"], joint_of[i],
          b.get("bucket", 0) | flag)
         for i, b in enumerate(baker.batches)],
        baker.textures, baker.palettes)

    njoints = len(baker.nodes)
    anim_data = [read_anim(rom, njoints, fid, f, reloc, off, afid, aend,
                           direct)
                 for off, afid, aend, direct in anims]

    # -- the materials, and the scripts that move them -----------------
    # Global MObj numbering, joint by joint in the order gcAddMObjAll
    # hands them to gcAddMObjForDObj, which is the order the chain ends
    # up in. joint_first/joint_count is how src/dc/objmodel.c rebuilds
    # the `MObjSub **` array per joint. (ssb_emblemexport.py builds the
    # same three blobs; the difference here is that most joints of a
    # scene's model carry no MObj at all, so a batch with none is the
    # rule rather than an error -- -1 says its colours are its display
    # list's own, which src/dc/fighter.c reads everywhere.)
    joint_first, mobjs = [], []
    for lst in mobjsubs:
        joint_first.append((len(mobjs), len(lst)))
        mobjs.extend(lst)

    batch_mobj = []
    for i, b in enumerate(baker.batches):
        m = b.get("mobj")
        batch_mobj.append(joint_first[m[0]][0] + m[1]
                          if m is not None and m[0] == joint_of[i] else -1)
    batch_tex = [-1] * len(mobjs)
    for i, b in enumerate(baker.batches):
        if batch_mobj[i] >= 0 and batch_tex[batch_mobj[i]] < 0:
            batch_tex[batch_mobj[i]] = b["mat"][0]

    if matanim_off is not None and not mobjs:
        sys.exit("scenemodel: --matanim without any MObj to move")
    mat_words, mat_entries, mat_relocs = (
        read_matanim(f, reloc, matanim_off,
                     [(orig_of[j] if orig_of else j, c)
                      for j, (_, c) in enumerate(joint_first)],
                     len(mobjs))
        if matanim_off is not None else ([], [-1] * len(mobjs), []))

    world = baker.world_verts()
    lo, hi = baker.bounds()
    cx, cy, cz = ((lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2,
                  (lo[2] + hi[2]) / 2)
    radius = max(math.sqrt((p[0] - cx) ** 2 + (p[1] - cy) ** 2 +
                           (p[2] - cz) ** 2) for p in world)

    anim_count = len(anim_data)

    def align(b):
        return b + b"\0" * (-len(b) % 4)

    header_size = 128
    dir_size = ANIM_NAME_LEN + 24
    # entries and relocs of every animation, then all the words; each
    # animation's directory row points at its own slice of both.
    ent_blob, words_blob, rows = b"", b"", []
    fixed = ["joints", "verts", "tris", "batches", "texs", "pals",
             "texdata"]
    off = header_size
    offsets = {}
    for key in fixed:
        offsets[key] = off
        off += len(align(secs[key]))
    offsets["anims"] = off
    off += dir_size * anim_count
    ent_at = off
    for words, entries, relocs in anim_data:
        ent_blob += struct.pack("<%di" % njoints, *entries)
        ent_blob += struct.pack("<%dI" % len(relocs), *relocs)
    off += len(ent_blob)
    words_at = off
    for i, (words, entries, relocs) in enumerate(anim_data):
        rows.append(words_at + len(words_blob))
        words_blob += struct.pack("<%dI" % len(words), *words)
    directory, ent_off = b"", ent_at
    for i, (words, entries, relocs) in enumerate(anim_data):
        directory += struct.pack("<%dsIIIIII" % ANIM_NAME_LEN,
                                 ("%s%d" % (name, i)).encode()[:ANIM_NAME_LEN - 1]
                                 if anim_count > 1 else
                                 name.encode()[:ANIM_NAME_LEN - 1],
                                 rows[i], len(words),
                                 ent_off, 1,   # FPACK_ANIM_ANIMJOINT
                                 ent_off + 4 * njoints, len(relocs))
        ent_off += 4 * njoints + 4 * len(relocs)
    body = b"".join(align(secs[key]) for key in fixed)
    body += directory + ent_blob + words_blob
    off = header_size + len(body)

    # The FPackMObjs block, laid out exactly as ssb_emblemexport.py's is
    # and read by the same src/dc/fighter.c loader. It goes after the
    # animations rather than between the fixed sections, so a pack
    # without one is byte for byte what it would be without this section.
    mobjhdr_at = 0
    if mobjs:
        mobjhdr_size = 48
        sections = [
            ("mobjhdr", b"\0" * mobjhdr_size),
            ("subs", b"".join(A.pack_mobjsub(s, batch_tex[k], 1)
                              for k, s in enumerate(mobjs))),
            ("mjoint", struct.pack("<%dh" % (2 * njoints),
                                   *[v for pair in joint_first for v in pair])),
            ("mbatch", struct.pack("<%dh" % len(batch_mobj), *batch_mobj)),
            ("mentry", struct.pack("<%di" % len(mat_entries), *mat_entries)),
            ("mreloc", struct.pack("<%dI" % len(mat_relocs), *mat_relocs)),
            ("mwords", struct.pack("<%dI" % len(mat_words), *mat_words)),
        ]
        for key, sec in sections:
            offsets[key] = off
            off += len(align(sec))
        mobjhdr_at = offsets["mobjhdr"]
        mobjhdr = struct.pack("<12I", len(mobjs), offsets["subs"],
                              offsets["mjoint"], offsets["mbatch"],
                              offsets["mentry"], offsets["mwords"],
                              len(mat_words), offsets["mreloc"],
                              len(mat_relocs), 1, 0, 0)
        assert len(mobjhdr) == mobjhdr_size, len(mobjhdr)
        for key, sec in sections:
            body += align(mobjhdr if key == "mobjhdr" else sec)

    header = struct.pack("<8s8I8I3ff8s8I", MAGIC,
                         njoints, len(verts), len(tris),
                         len(baker.batches), len(baker.textures),
                         len(baker.palettes), anim_count,
                         len(secs["texdata"]),
                         offsets["joints"], offsets["verts"],
                         offsets["tris"], offsets["batches"],
                         offsets["texs"], offsets["pals"],
                         offsets["texdata"], offsets["anims"],
                         cx, cy, cz, radius, name.encode()[:8],
                         0, 0, 0, 0, 0, 0, 0, mobjhdr_at)
    assert len(header) == header_size, len(header)
    print("scenemodel: file %d, %s 0x%04X: %d joints, %d verts, "
          "%d tris, %d batches, %d textures, %d palettes, %d animations, "
          "%d anim words, %d pointers, %d MObjs, %d matanim words"
          % (fid, "DisplayList" if bare_dl else "DObjDesc",
             dobjdesc_off, njoints, len(verts), len(tris),
             len(baker.batches), len(baker.textures), len(baker.palettes),
             anim_count, sum(len(a[0]) for a in anim_data),
             sum(len(a[2]) for a in anim_data), len(mobjs),
             len(mat_words)))
    return header + body


def main():
    argv = sys.argv[1:]

    def opt(flag):
        return argv[argv.index(flag) + 1] if flag in argv else None

    rom_path = opt("--rom") or M.ROM_DEFAULT
    out = opt("--out")
    fid = opt("--file")
    dobj = opt("--dobjdesc")
    bare = opt("--dl")
    name = opt("--name")
    if dobj and bare:
        sys.exit("--dobjdesc and --dl are the two ways in; pick one")
    dobj = dobj or bare
    if not (out and fid and dobj and name):
        sys.exit("--file <id> (--dobjdesc <off> | --dl <off>) "
                 "--name <8 chars> --out <file.mdl> are required")
    rom = open(rom_path, "rb").read()
    animend = opt("--animend")
    anims = []
    # --animjoint 0xOFF is the model's own file's; --anim 0xOFF@FILE reads
    # the table from another (a scene that flies one model on tables kept
    # in a neighbour's file, mvopeningsector.c's arwings), and may repeat
    for i, a in enumerate(argv):
        if a in ("--animjoint", "--anim", "--animdl"):
            off, _, afid = argv[i + 1].partition("@")
            anims.append((int(off, 0), int(afid, 0) if afid else None,
                          int(animend, 0) if animend else None,
                          a == "--animdl"))
    entries = None
    if opt("--entries"):
        entries = []
        for part in opt("--entries").split(","):
            lo, _, hi = part.partition("-")
            entries.extend(range(int(lo), int(hi or lo) + 1))
    dlhead = int(opt("--dlhead") or "0", 0)
    if dlhead and bare is None:
        sys.exit("--dlhead is about the one list --dl names")
    mobjsub = opt("--mobjsub")
    matanim = opt("--matanim")
    if matanim and not mobjsub:
        sys.exit("--matanim moves the MObjs --mobjsub reads; give both")
    blob = pack_model(rom, int(fid, 0), int(dobj, 0), anims, name,
                      "--noz" in argv, "--dllinks" in argv, entries,
                      bare_dl=bare is not None, dlhead=dlhead,
                      mobjsub_off=int(mobjsub, 0) if mobjsub else None,
                      matanim_off=int(matanim, 0) if matanim else None)
    d = os.path.dirname(out)
    if d:
        os.makedirs(d, exist_ok=True)
    with open(out, "wb") as fp:
        fp.write(blob)
    print("wrote %s (%d bytes)" % (out, len(blob)))


if __name__ == "__main__":
    main()

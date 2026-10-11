#!/usr/bin/env python3
"""ssb64-dc: How to Play's control-stick diagram, packed.

relocData file 198 (SCExplainGraphics) is twenty-six libultra Sprites --
already exported, `ssb_spriteexport.py --file 198` -- and three things
that are not sprites at all. The original cut all three on the grounds that
"no tool in this port exports a small hand-authored DObjDesc/MatAnimJoint
model". Measured, the opposite holds: every
shape here is one a shipped exporter already reads.

  - `Stick` (`llSCExplainGraphicsStickDObjDesc`, 0x5300): two joints with
    `DObjDLLink` payloads on head 1, one `MObjSub` on joint 0 whose flags
    are MOBJ_FLAG_ALPHA alone -- so what the MObj contributes is the
    *image address*, `sprites[texture_id_curr]`, and the joint's own list
    loads it. The array holds five 64x64 IA8 pictures of the stick, and
    the five `Stick*MatAnimJoint` tables beside it are five different
    walks over those ids: Neutral {0,1}, HoldUp {2,0}, TapUp {0,1,2} on a
    loop, HoldForward {4}, TapForward {0,3,4} on a loop. Joint 1's list
    is untextured geometry with no MObj -- the "+" the diagram shows when
    `control_stick_args.sprite_status` is 2.

    So the model is baked once per picture and the five tables ship as
    five whole MatAnimJoints end to end (`FPackMObjs.alt_count`), exactly
    the way ef/efmanager.c's dead explosion carries one per player. The
    scene picks between them with `dSCExplainStickMatAnimJoints[sw]`,
    which is a C table, not a tool.

  - `TapSpark` (0x5a98/0x5b68): one quad, one `MObjSub` with three 32x32
    I4 pictures and a MatAnimJoint that steps them 0,1,2 and ends. The
    game binds the display list straight to a DObj
    (`gcAddDObjForGObj`, no DObjDesc), so the tree is built here the way
    tools/export/ssb_magnifyexport.py builds the magnifying glass's handle.

  - `SpecialMoveRGB` (0x5e40): one quad, one CI4 16x48 tile and its
    16-entry TLUT, no MObj at all. The same one-node shape again.

All three are drawn by `scExplainMakeControlStickCamera`'s own ortho
camera on DL link 27, under a display proc that clears G_ZBUFFER and sets
G_RM_AA_XLU_SURF -- so the batches are seeded translucent and bake with
FPACK_NOZ, which is what tools/export/ssb_arrowexport.py does with the identical
two lines of state.

Usage: python3 tools/export/ssb_explainexport.py --what stick --out scstick.mdl
                                          [--rom <rom.z64>]
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
import ssb_effectexport as E  # noqa: E402

# scexplain.c:309-311 and 415-417, the two lines of state both display
# procs set ahead of the geometry: gSPClearGeometryMode(G_ZBUFFER) and
# gDPSetRenderMode(G_RM_AA_XLU_SURF, G_RM_AA_XLU_SURF2).
EXPLAIN_RENDERMODE = A.G_RM_AA_XLU_SURF

# What each pack is, by --what: the FPackHeader name, the reloc symbols
# it reads, and how many joints its tree must come out as.
WHATS = {
    "stick": {
        "name": "SCStick",
        "dobj": "llSCExplainGraphicsStickDObjDesc",
        "mobj": "llSCExplainGraphicsStickMObjSub",
        "mats": ("llSCExplainGraphicsStickNeutralMatAnimJoint",
                 "llSCExplainGraphicsStickHoldUpMatAnimJoint",
                 "llSCExplainGraphicsStickTapUpMatAnimJoint",
                 "llSCExplainGraphicsStickHoldForwardMatAnimJoint",
                 "llSCExplainGraphicsStickTapForwardMatAnimJoint"),
        "njoints": 2,
    },
    "tapspark": {
        "name": "SCSpark",
        "dl": "llSCExplainGraphicsTapSparkDisplayList",
        "mobj": "llSCExplainGraphicsTapSparkMObjSub",
        "mats": ("llSCExplainGraphicsTapSparkMatAnimJoint",),
        "njoints": 1,
    },
    "rgb": {
        "name": "SCRGB",
        "dl": "llSCExplainGraphicsSpecialMoveRGBDisplayList",
        "njoints": 1,
    },
}


def one_node_tree(dl_off):
    """The tree `gcAddDObjForGObj(gobj, <display list>)` makes: one DObj
    at the identity carrying the list, under the synthetic root every
    read_dobj_tree returns. tools/export/ssb_magnifyexport.py's own."""
    root = A.DObjNode(-1, -1, None, (0.0, 0.0, 0.0), (0.0, 0.0, 0.0),
                      (1.0, 1.0, 1.0))
    node = A.DObjNode(0, 0, dl_off, (0.0, 0.0, 0.0), (0.0, 0.0, 0.0),
                      (1.0, 1.0, 1.0))
    node.parent = root
    root.children.append(node)
    return root, [node]


def pack_explain(rom, what):
    import pygfxd

    spec = WHATS[what]
    syms = ["llSCExplainGraphicsFileID"]
    for key in ("dobj", "dl", "mobj"):
        if spec.get(key):
            syms.append(spec[key])
    syms.extend(spec.get("mats", ()))
    vals = E.reloc_offsets(tuple(syms))
    off = dict(zip(syms, vals))
    fid = off["llSCExplainGraphicsFileID"]

    f = A.get_file(rom, fid, ssb_extract)
    entry = ssb_extract.read_entry(rom, ssb_extract.RELOC_SEG, fid)
    reloc = A.walk_reloc(f, entry["reloc_intern"])

    if spec.get("dobj"):
        # dl_links: the payload is a DObjDLLink array, which reads as
        # gsDPNoOpTag if it is mistaken for commands (the trap
        # ssb_itemmodelexport.py learned the hard way). The scene draws it
        # with gcDrawDObjTreeDLLinksForGObj.
        root, nodes = A.read_dobj_tree(f, reloc, off[spec["dobj"]],
                                       dl_links=True)
    else:
        root, nodes = one_node_tree(off[spec["dl"]])
    if len(nodes) != spec["njoints"]:
        sys.exit("%s: the tree reads %d joints, wanted %d"
                 % (what, len(nodes), spec["njoints"]))

    mobj_off = off.get(spec.get("mobj"))
    mat_offs = [off[s] for s in spec.get("mats", ())]

    def read_subs(frame):
        """The MObjs with every sprite array wound to `frame`. Both MObjs
        here are MOBJ_FLAG_ALPHA, so this is the whole of what the
        animation does to the picture: gcDrawMObjForDObj sets the image
        register to sprites[texture_id_curr] and the joint's own list
        loads it (objdisplay.c:1340)."""
        if mobj_off is None:
            return [[] for _ in nodes]
        subs = A.read_mobjsubs(f, reloc, mobj_off, len(nodes))
        for lst in subs:
            for sub in lst:
                if sub["sprites"]:
                    sub["texture_id_curr"] = min(frame,
                                                 len(sub["sprites"]) - 1)
                if sub["palettes"]:
                    sys.exit("%s: MObjSub@0x%04X selects a palette; the "
                             "port's materials here are texture and colour"
                             % (what, sub["off"]))
        return subs

    mobjsubs = read_subs(0)
    baker = A.MeshBaker(f, pygfxd, mobjsubs,
                        rendermode=[EXPLAIN_RENDERMODE] * 4,
                        mobj_batches=(mobj_off is not None))
    verts, tris, joints = baker.bake(root)
    if not tris:
        sys.exit("%s: the tree bakes to no geometry at all" % what)

    joint_of = {}
    for j in joints:
        for b in range(j["batch_first"], j["batch_first"] + j["batch_count"]):
            joint_of[b] = j["node"].index

    batch_mat = [b["mat"] for b in baker.batches]
    mobjs_section = None

    if mobj_off is not None:
        joint_first, mobjs = [], []
        for lst in mobjsubs:
            joint_first.append((len(mobjs), len(lst)))
            mobjs.extend(lst)
        if not mobjs:
            sys.exit("%s: the MObjSub table at 0x%04X is empty"
                     % (what, mobj_off))

        batch_mobj = []
        for i, b in enumerate(baker.batches):
            m = b["mobj"]
            batch_mobj.append(-1 if (m is None or m[0] != joint_of[i])
                              else joint_first[m[0]][0] + m[1])

        # One alternate per MatAnimJoint table, the way the dead
        # explosion's four are laid end to end (FPackMObjs.alt_count).
        # The `AObjEvent32 ***` is per DObj then per MObj, which is the
        # shape src/dc/objmodel.c rebuilds at load.
        alt_scripts = []
        for mat_off in mat_offs:
            scripts = []
            for j in range(len(nodes)):
                arr = reloc.get(mat_off + 4 * j)
                for k in range(len(mobjsubs[j])):
                    scripts.append(None if arr is None
                                   else reloc.get(arr + 4 * k))
            if len(scripts) != len(mobjs):
                sys.exit("%s: %d MatAnimJoint scripts for %d MObjs"
                         % (what, len(scripts), len(mobjs)))
            alt_scripts.append(scripts)

        # How many pictures each MObj shows: the highest texture_id_curr
        # any of its scripts ever reaches, over every alternate, plus one.
        # Nothing counts the sprite array for us -- the arrays of a file's
        # MObjs sit end to end -- so the script is what bounds it
        # (ssb_assets.matanim_frame_ids says the same thing at length).
        nframes = []
        for k, sub in enumerate(mobjs):
            ids = set()
            for scripts in alt_scripts:
                if scripts[k] is not None:
                    ids |= E.matanim_texture_ids(f, reloc, scripts[k])
            trunc = set(int(v) for v in ids)
            if min(trunc, default=0) < 0:
                sys.exit("%s: MObj %d's script sets texture_id_curr to %s"
                         % (what, k, sorted(ids)))
            n = 1 if not trunc else max(trunc) + 1
            if n > 1 and n > len(sub["sprites"]):
                sys.exit("%s: MObj %d's script reaches sprite %d and its "
                         "array holds %d"
                         % (what, k, n - 1, len(sub["sprites"])))
            nframes.append(n)

        batch_of = []
        for k in range(len(mobjs)):
            bs = [i for i, m in enumerate(batch_mobj) if m == k]
            if len(bs) != 1:
                sys.exit("%s: MObj %d draws %d batches" % (what, k, len(bs)))
            batch_of.append(bs[0])

        # Every later picture, baked beside the first: the same walk with
        # the sprite array wound on, which must produce the same geometry
        # and the same tile size or the vertices' UVs would be wrong for
        # it (ssb_effectexport.pack_entry's own check).
        later = [[] for _ in mobjs]
        for frame in range(1, max(nframes)):
            b2 = A.MeshBaker(f, pygfxd, read_subs(frame),
                             rendermode=[EXPLAIN_RENDERMODE] * 4,
                             mobj_batches=True)
            v2, t2, _j2 = b2.bake(root)
            if (len(v2), len(t2), len(b2.batches)) != \
                    (len(verts), len(tris), len(baker.batches)):
                sys.exit("%s: frame %d bakes to different geometry"
                         % (what, frame))
            for k in range(len(mobjs)):
                if frame < nframes[k]:
                    later[k].append(
                        b2.textures[b2.batches[batch_of[k]]["mat"][0]])

        frames, tex_first = [], []
        for k in range(len(mobjs)):
            own = baker.batches[batch_of[k]]["mat"][0]
            if own < 0:
                sys.exit("%s: MObj %d draws an untextured batch" % (what, k))
            tex_first.append(len(frames))
            frames.append(baker.textures[own])
            frames.extend(later[k])
        plain = {}
        for i, b in enumerate(baker.batches):
            if batch_mobj[i] < 0 and b["mat"][0] >= 0 and \
                    b["mat"][0] not in plain:
                plain[b["mat"][0]] = len(frames)
                frames.append(baker.textures[b["mat"][0]])
        baker.textures = frames
        batch_mat = [((tex_first[batch_mobj[i]] if batch_mobj[i] >= 0
                       else plain.get(b["mat"][0], -1)),) + tuple(b["mat"][1:])
                     for i, b in enumerate(baker.batches)]

        m_entries, m_words, m_relocs = [], [], []
        for scripts in alt_scripts:
            e, w, rl = E.matanim_block(f, reloc, scripts, what)
            base = len(m_words)
            m_entries += [-1 if x < 0 else x + base for x in e]
            m_words += w
            m_relocs += [x + base for x in rl]
        mobjs_section = {
            "count": len(mobjs),
            "alt": max(1, len(mat_offs)),
            "subs": b"".join(A.pack_mobjsub(sub, tex_first[k], nframes[k])
                             for k, sub in enumerate(mobjs)),
            "joint": struct.pack("<%dh" % (2 * len(mobjsubs)),
                                 *[v for pair in joint_first for v in pair]),
            "batch": struct.pack("<%dh" % len(batch_mobj), *batch_mobj),
            "entry": struct.pack("<%di" % len(m_entries), *m_entries),
            "words": struct.pack("<%dI" % len(m_words), *m_words),
            "reloc": struct.pack("<%dI" % len(m_relocs), *m_relocs),
        }

    secs = K.model_sections(
        [(n.parent.index, n.translate, n.rotate, n.scale)
         for n in baker.nodes],
        verts, tris,
        [(b["tri_first"], b["tri_count"], batch_mat[i], joint_of[i],
          b.get("bucket", 0) | A.FPACK_NOZ)
         for i, b in enumerate(baker.batches)],
        baker.textures, baker.palettes)

    world = baker.world_verts()
    lo, hi = baker.bounds()
    cx, cy, cz = ((lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2,
                  (lo[2] + hi[2]) / 2)
    radius = max(math.sqrt((p[0] - cx) ** 2 + (p[1] - cy) ** 2 +
                           (p[2] - cz) ** 2) for p in world)

    blob = E.build_pack(spec["name"], len(baker.nodes), secs,
                        (len(verts), len(tris), len(baker.batches),
                         len(baker.textures), len(baker.palettes)),
                        [], (cx, cy, cz), radius, mobjs_section)
    print("%s: file %d, %d joints, %d verts, %d tris, %d batches, "
          "%d textures, %d palettes, %d MObjs in %d alternates"
          % (what, fid, len(baker.nodes), len(verts), len(tris),
             len(baker.batches), len(baker.textures), len(baker.palettes),
             0 if mobjs_section is None else mobjs_section["count"],
             0 if mobjs_section is None else mobjs_section["alt"]))
    return blob


def main():
    argv = sys.argv[1:]
    rom_path = M.ROM_DEFAULT
    what = out = None
    if "--rom" in argv:
        rom_path = argv[argv.index("--rom") + 1]
    if "--what" in argv:
        what = argv[argv.index("--what") + 1]
    if "--out" in argv:
        out = argv[argv.index("--out") + 1]
    if "--list" in argv:
        for k in WHATS:
            print(k)
        return
    if not what or not out:
        sys.exit("--what <%s> --out <file.mdl> are required"
                 % "|".join(WHATS))
    if what not in WHATS:
        sys.exit("%s is not one of %s" % (what, ", ".join(WHATS)))

    rom = open(rom_path, "rb").read()
    blob = pack_explain(rom, what)
    d = os.path.dirname(out)
    if d:
        os.makedirs(d, exist_ok=True)
    with open(out, "wb") as fp:
        fp.write(blob)
    print("wrote %s (%d bytes)" % (out, len(blob)))


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""ssb64-dc: the character select's spotlight, packed.

`mnPlayersVSMakeSpotlight` (mn/mnplayers/mnplayersvs.c:4056) stands four
of these at y = -850 under the four gates, one per slot, and
`mnPlayersVSSpotlightProcUpdate` blinks each one on and off over a slot
whose fighter is chosen but not locked in, at that fighter's size.

The model is two joints and one quad, and everything interesting about it
is in the one display list its DObjDLLink payload names. The colour cycle
computes SHADE and nothing else; the *alpha* cycle computes
TEXEL0 * PRIMITIVE over a 32x32 I8 tile that lives in the file. So the
quad's whole shape is a texture the RDP never reads for colour -- which
is why the baker had to learn two things before this file could exist
(tools/lib/ssb_assets.py): that an I texel's alpha is its intensity, and that
a tile read by the alpha cycle alone is still a tile, one whose RGB must
ship white so the PVR's MODULATEALPHA is the identity on colour.

The primitive colour comes from the file's one MObjSub, whose flags are
0x0200 -- MOBJ_FLAG_PRIMCOLOR alone. That is inside what
src/dc/objdisplay.c's gcDrawMObjForDObj has carried, so
the pack carries the MObj the way an emblem's does (fighter.h
FPackMObjs) and src/dc/objmodel.c hands it to the game's own
gcAddMObjAll. There is no MatAnimJoint in the file, so every MObj's
script entry is -1 and the animation block is empty.

Usage: python3 tools/export/ssb_spotexport.py --out spotlight.mdl [--rom <rom.z64>]
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

# The name the pack carries and the port's file name. Eight characters is
# what FPackHeader.name holds.
NAME = "Spotlite"

# MObjSub.flags bits the port's material knows how to carry. The same
# three ssb_emblemexport.py names, for the same reason: gcDrawMObjForDObj's
# other jobs are not ported, and a pack that asks for one must fail the
# build rather than draw wrong.
MOBJ_FLAGS_KNOWN = (A.MOBJ_FLAG_LIGHT1 | A.MOBJ_FLAG_LIGHT2 |
                    A.MOBJ_FLAG_PRIMCOLOR)


def find_file():
    """(file id, source text) for relocData's NNN_MNPlayersSpotlight.c."""
    for fn in sorted(os.listdir(M.RELOC_DIR)):
        m = re.fullmatch(r"(\d+)_MNPlayersSpotlight\.c", fn)
        if m:
            return int(m.group(1)), M.read_source(
                os.path.join(M.RELOC_DIR, fn))
    sys.exit("no relocData file for MNPlayersSpotlight")


def symbol_offsets(src, kind):
    """Every dMNPlayersSpotlight_<kind>_0xNNNN offset, sorted.

    As tools/export/ssb_transexport.py does it: the extractor names each block
    by the file offset it came from, and the offsets the game's `ll` link
    labels carry are readable straight off the symbols.
    """
    pat = r"dMNPlayersSpotlight_%s_0x([0-9A-Fa-f]+)" % kind
    return sorted({int(h, 16) for h in re.findall(pat, src)})


def pack_spotlight(rom):
    import pygfxd

    fid, src = find_file()
    dobjs = symbol_offsets(src, "DObjDesc")
    subs = symbol_offsets(src, "MObjSub")
    if len(dobjs) != 1:
        sys.exit("spotlight: %d DObjDesc blocks, expected one" % len(dobjs))
    if not subs:
        sys.exit("spotlight: no MObjSub block")
    # The lowest is the `MObjSub ***` head the game hands gcAddMObjAll
    # (llMNPlayersSpotlightMObjSub); the ones above it are the records it
    # leads to, reached by walking rather than by name.
    dobj_off, subs_off = dobjs[0], subs[0]

    f = A.get_file(rom, fid, ssb_extract)
    entry = ssb_extract.read_entry(rom, ssb_extract.RELOC_SEG, fid)
    reloc = A.walk_reloc(f, entry["reloc_intern"])

    # dl_links: the payload is a DObjDLLink array, not a display list --
    # mnPlayersVSMakeSpotlight draws it with gcDrawDObjTreeDLLinksForGObj.
    root, nodes = A.read_dobj_tree(f, reloc, dobj_off, dl_links=True)
    mobjsubs = A.read_mobjsubs(f, reloc, subs_off, len(nodes))

    for lst in mobjsubs:
        for sub in lst:
            if sub["sprites"] or sub["palettes"]:
                sys.exit("spotlight: MObjSub@0x%04X selects textures or "
                         "palettes; the port's materials are colour only"
                         % sub["off"])
            if sub["flags"] & ~MOBJ_FLAGS_KNOWN:
                sys.exit("spotlight: MObjSub@0x%04X has flags 0x%04X, "
                         "outside the colour set 0x%04X"
                         % (sub["off"], sub["flags"], MOBJ_FLAGS_KNOWN))

    baker = A.MeshBaker(f, pygfxd, mobjsubs, mobj_batches=True)
    verts, tris, joints = baker.bake(root)
    if len(baker.textures) != 1:
        sys.exit("spotlight: %d textures, expected the one I8 mask"
                 % len(baker.textures))
    tex = baker.textures[0]
    if not tex["key"].alpha_only:
        sys.exit("spotlight: the tile at 0x%04X is read for colour; the "
                 "spotlight's is an alpha mask" % tex["key"].img)

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

    # As ssb_emblemexport.py numbers them: joint by joint, in the order
    # gcAddMObjAll hands them to gcAddMObjForDObj.
    joint_first, mobjs = [], []
    for lst in mobjsubs:
        joint_first.append((len(mobjs), len(lst)))
        mobjs.extend(lst)

    batch_mobj = []
    for i, b in enumerate(baker.batches):
        m = b["mobj"]
        batch_mobj.append(-1 if (m is None or m[0] != joint_of[i])
                          else joint_first[m[0]][0] + m[1])

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
    # No MatAnimJoint in the file: every MObj's script entry is -1 and the
    # word block is empty. dc_model_add_mobjs still calls
    # gcAddMatAnimJointAll, which skips a NULL script per MObj.
    entry_blob = struct.pack("<%di" % len(mobjs), *([-1] * len(mobjs)))

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
                ("mbatch", batch_blob), ("mentry", entry_blob)]
    for key, sec in sections:
        offsets[key] = off
        off += len(align(sec))
    mobjhdr = struct.pack("<12I", len(mobjs), offsets["subs"],
                          offsets["mjoint"], offsets["mbatch"],
                          offsets["mentry"], 0, 0, 0, 0,
                          1, 0, 0)      # one MatAnimJoint, as every pack but
                                  # the dead explosion's has
    assert len(mobjhdr) == mobjhdr_size, len(mobjhdr)

    body = b""
    for key, sec in sections:
        body += align(mobjhdr if key == "mobjhdr" else sec)

    header = struct.pack("<8s8I8I3ff8s8I", MAGIC,
                         len(baker.nodes), len(verts), len(tris),
                         len(baker.batches), len(baker.textures),
                         len(baker.palettes), 0, len(secs["texdata"]),
                         offsets["joints"], offsets["verts"],
                         offsets["tris"], offsets["batches"],
                         offsets["texs"], offsets["pals"],
                         offsets["texdata"], 0,
                         cx, cy, cz, radius, NAME.encode()[:8],
                         0, 0, 0, 0, 0, 0, 0, offsets["mobjhdr"])
    assert len(header) == header_size, len(header)
    print("spotlight: file %d, DObjDesc 0x%04X, MObjSub 0x%04X: %d joints, "
          "%d verts, %d tris, %d batches, %d MObjs, %d with one, "
          "tile %dx%d %s alpha-only"
          % (fid, dobj_off, subs_off, len(baker.nodes), len(verts),
             len(tris), len(baker.batches), len(mobjs),
             sum(1 for m in batch_mobj if m >= 0),
             tex["w"], tex["h"],
             A.N64_FORMATS[(tex["key"].fmt, tex["key"].siz)]))
    return header + body


def main():
    argv = sys.argv[1:]
    rom_path = M.ROM_DEFAULT
    out = None
    if "--rom" in argv:
        rom_path = argv[argv.index("--rom") + 1]
    if "--out" in argv:
        out = argv[argv.index("--out") + 1]
    if not out:
        sys.exit("--out <file.mdl> is required")

    rom = open(rom_path, "rb").read()
    blob = pack_spotlight(rom)
    d = os.path.dirname(out)
    if d:
        os.makedirs(d, exist_ok=True)
    with open(out, "wb") as fp:
        fp.write(blob)
    print("wrote %s (%d bytes)" % (out, len(blob)))


if __name__ == "__main__":
    main()

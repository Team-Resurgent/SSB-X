#!/usr/bin/env python3
"""ssb64-dc: the render mode each stage batch was bucketed by, against gfxdis.

The exporter reads the RDP render mode. A batch's PVR
list would otherwise come from DObjDLLink.list_id, which only exists on a stage
whose MPGroundData.layer_mask has that layer's bit set -- and three of the
seven exported stages set layer_mask to 0, so every batch in them defaulted
to the opaque list. Kongo Jungle's trees are cutout foliage drawn under
G_RM_AA_ZB_TEX_EDGE and they came out as solid quads.

The fix reads G_SETOTHERMODE_L out of the display lists. This is the oracle
under it, and it is deliberately a second implementation of the same read:

  * ours -- ssb_assets.MeshBaker replaying the flattened DL through pygfxd,
    applying each masked other-mode write to the head's word, seeded per
    layer from gr/grdisplay.c (ssb_stageexport.LAYER_RENDERMODE);
  * theirs -- glankk's gfxdis.f3dex2, the C reference tools/check/verify_dl.py
    already uses, run over the same bytes and read for the G_RM_* names it
    prints.

They must agree on the render mode in force at every batch, and the bucket
must be what that render mode says: FORCE_BL translucent, CVG_X_ALPHA
without it punch-through, neither opaque.

The other-mode *H* field sits beside it, for the same reason
and read the same two ways: the RDP's texture filter. sys/rdp.c's reset
display list leaves G_TF_BILERP in force, so the answer for every triangle
in every stage is "filtered" -- and that is exactly what makes this worth
checking, because a renderer that hardcodes the answer and a renderer that
reads it look identical until a display list disagrees.

The alpha-in-opaque report at the end is informational and never fails.
The RDP consults texel alpha only where the render mode says to, so a tile
with transparent texels drawn through G_RM_AA_*_OPA_SURF really is solid on
the N64 -- Yoshi's Island does this deliberately. It is printed because if
the render-mode read ever regresses, every stage collapses to opaque and
this count is where it shows.

Usage: python3 tools/check/rendermode_check.py [--rom <rom.z64>] [--stage Name]
"""
import os
import re
import subprocess
import sys

sys.path[:0] = [os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), d)
                for d in ("lib", "export", "check")]
import ssb_assets as A           # noqa: E402
import ssb_meshexport as M       # noqa: E402  (ROM_DEFAULT)
import ssb_paths as P            # noqa: E402
import ssb_stageexport as S      # noqa: E402

# The stages that export today, the same list tools/check/texfmt_check.py walks.
STAGES = ["Castle", "Sector", "Jungle", "Zebes", "Hyrule", "Yoster",
          "Inishie"]

LIST_NAME = {A.FPACK_LIST_OP: "OP", A.FPACK_LIST_PT: "PT",
             A.FPACK_LIST_TR: "TR"}

# gfxdis prints the cycle-1 and cycle-2 halves by name; the DLs always set
# the matching pair, which is the word ssb_assets carries.
GFXDIS_RM = {
    ("G_RM_AA_OPA_SURF", "G_RM_AA_OPA_SURF2"): A.G_RM_AA_OPA_SURF,
    ("G_RM_AA_ZB_OPA_SURF", "G_RM_AA_ZB_OPA_SURF2"): A.G_RM_AA_ZB_OPA_SURF,
    ("G_RM_AA_TEX_EDGE", "G_RM_AA_TEX_EDGE2"): A.G_RM_AA_TEX_EDGE,
    ("G_RM_AA_ZB_TEX_EDGE", "G_RM_AA_ZB_TEX_EDGE2"): A.G_RM_AA_ZB_TEX_EDGE,
    ("G_RM_AA_XLU_SURF", "G_RM_AA_XLU_SURF2"): A.G_RM_AA_XLU_SURF,
    ("G_RM_AA_ZB_XLU_SURF", "G_RM_AA_ZB_XLU_SURF2"): A.G_RM_AA_ZB_XLU_SURF,
}

RM_RE = re.compile(r"gsDPSetRenderMode\(\s*([A-Za-z0-9_]+)\s*,"
                   r"\s*([A-Za-z0-9_]+)\s*\)")

# The other-mode H field this check also models, added when
# the exporter started reading it. The reset display list (sys/rdp.c) leaves
# G_TF_BILERP in force, so every head starts filtered and only G_TF_POINT
# sets the pack's bit; G_TF_AVERAGE is the RDP's box filter and is filtering
# too. No stage display list writes this field at all, which is the finding
# the check exists to keep true.
TF_RE = re.compile(r"gsDPSetTextureFilter\(\s*([A-Za-z0-9_]+)\s*\)")
GFXDIS_TF = {"G_TF_POINT": A.FPACK_FILT_POINT,
             "G_TF_AVERAGE": 0, "G_TF_BILERP": 0}

# Other-mode L writes gfxdis names but that carry no render mode. Anything
# else it prints with SetOtherMode in it is a write this check cannot
# account for, and it fails rather than pass a mode it did not model.
OTHERMODE_OK = ("gsDPSetAlphaCompare", "gsDPSetDepthSource",
                "gsDPSetTextureLUT",
                "gsDPSetTextureConvert", "gsDPSetTextureDetail",
                "gsDPSetTexturePersp", "gsDPSetTextureLOD",
                "gsDPSetCombineKey", "gsDPSetColorDither",
                "gsDPSetAlphaDither", "gsDPSetCycleType",
                "gsDPPipelineMode")


def gfxdis_lines(data, tmp):
    with open(tmp, "wb") as fp:
        fp.write(data)
    return subprocess.run([P.GFXDIS, "-f", tmp],
                          capture_output=True, text=True).stdout.splitlines()


def oracle_triangle_modes(f_model, reloc, ground, layer, tmp, report):
    """The render mode in force at every triangle of one layer, from gfxdis.

    A second walk of the same tree, sharing nothing with the baker but the
    ROM bytes and read_dobj_tree: it flattens each display list, hands it to
    the C disassembler, and steps the printed macros keeping one render mode
    per DL head -- the head being 0 for gcDrawDObjTree and the link's
    list_id for gcDrawDObjDLLinks. Every triangle macro appends whatever
    mode is current.
    """
    links = bool(ground["layer_mask"] & (1 << layer))
    root, _nodes = A.read_dobj_tree(f_model, reloc,
                                    ground["layers"][layer][1],
                                    dl_links=links)
    mode = list(S.LAYER_RENDERMODE[links][layer])
    # the reset display list's G_TF_BILERP, per head, as the baker seeds
    # othermode_h with OTHERMODE_H_RESET
    filt = [0] * 4
    out = []
    bad = [0]

    def run(dl_off, head):
        for line in gfxdis_lines(A.flatten_dl(f_model, dl_off), tmp):
            line = line.strip()
            m = RM_RE.search(line)
            if m:
                key = (m.group(1), m.group(2))
                if key not in GFXDIS_RM:
                    report("  layer %d DL 0x%04X: gfxdis names a render "
                           "mode this check does not model: %s"
                           % (layer, dl_off, line))
                    bad[0] += 1
                else:
                    mode[head] = GFXDIS_RM[key]
                continue
            m = TF_RE.search(line)
            if m:
                if m.group(1) not in GFXDIS_TF:
                    report("  layer %d DL 0x%04X: gfxdis names a texture "
                           "filter this check does not model: %s"
                           % (layer, dl_off, line))
                    bad[0] += 1
                else:
                    filt[head] = GFXDIS_TF[m.group(1)]
                continue
            if "SetOtherMode" in line and \
                    not line.startswith(OTHERMODE_OK):
                report("  layer %d DL 0x%04X: unmodelled other-mode write: "
                       "%s" % (layer, dl_off, line))
                bad[0] += 1
            if line.startswith("gsSP1Triangle("):
                out.append((dl_off, mode[head], filt[head]))
            elif line.startswith("gsSP2Triangles("):
                out.append((dl_off, mode[head], filt[head]))
                out.append((dl_off, mode[head], filt[head]))

    def visit(node):
        # the order ssb_assets.MeshBaker.bake walks in
        if node.dl_pre_off is not None and node.parent is not None and \
                node.parent.index >= 0:
            run(node.dl_pre_off, 0)
        if node.dl_off is not None:
            run(node.dl_off, 0)
        for list_id, dl in node.dl_links:
            if list_id == 0:
                run(dl, list_id)
        for child in node.children:
            visit(child)

    visit(root)

    # then heads 2, 1 and 3, each a walk of its own, as bake does
    def visit_head(node, head):
        if node.index >= 0:
            for list_id, dl in node.dl_links:
                if list_id == head:
                    run(dl, list_id)
        for child in node.children:
            visit_head(child, head)

    for head in (2, 1, 3):
        visit_head(root, head)
    return out, bad[0]


def check_stage(rom, stage, report):
    """Bake one stage, then replay its display lists through gfxdis and
    check every triangle landed in the list its render mode asks for."""
    import pygfxd     # noqa: F401  (the baker needs it imported)

    ground = S.read_ground(rom, stage)
    fid = next(t[0] for t in ground["layers"] if t)
    f_model, _e, reloc, _s, _i = S.L.file_info(rom, fid)
    trace = []
    nodes, verts, tris, batches, textures, palettes = S.merge_layers(
        f_model, reloc, ground["layers"], ground["layer_mask"],
        ground["mobjsubs"], trace=trace)

    tmp = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..",
                       "build-dc", "rendermode_check.tmp")
    os.makedirs(os.path.dirname(tmp), exist_ok=True)

    bad = 0
    theirs = []
    for layer, target in enumerate(ground["layers"]):
        if target is None:
            continue
        got, n = oracle_triangle_modes(f_model, reloc, ground, layer, tmp,
                                       report)
        theirs += got
        bad += n

    # ours, per triangle: the batches tile the triangle pool in order
    ours = [None] * len(tris)
    for (tri_first, tri_count, mat, joint, bucket) in batches:
        for t in range(tri_first, tri_first + tri_count):
            ours[t] = bucket
    if len(theirs) != len(tris):
        report("  %s: gfxdis found %d triangles, the baker emitted %d"
               % (stage, len(theirs), len(tris)))
        return bad + 1

    per_list = {}
    point = 0
    for t, ((dl_off, mode, tf), bucket) in enumerate(zip(theirs, ours)):
        want = A.rendermode_list(mode)
        if want != (bucket & 3):
            report("  %s triangle %d (DL 0x%04X): %s wants %s, batch says %s"
                   % (stage, t, dl_off, A.rendermode_name(mode),
                      LIST_NAME[want], LIST_NAME.get(bucket & 3, "?")))
            bad += 1
        if tf != (bucket & A.FPACK_FILT_POINT):
            report("  %s triangle %d (DL 0x%04X): gfxdis says %s, batch "
                   "says %s"
                   % (stage, t, dl_off,
                      "G_TF_POINT" if tf else "filtered",
                      "G_TF_POINT" if bucket & A.FPACK_FILT_POINT
                      else "filtered"))
            bad += 1
        point += 1 if tf else 0
    for (tri_first, tri_count, mat, joint, bucket) in batches:
        key = "%s%s" % (LIST_NAME.get(bucket & 3, "?"),
                        "/noz" if bucket & A.FPACK_NOZ else "")
        per_list[key] = per_list.get(key, 0) + 1

    inherited = sum(1 for (layer, dl, head, mode) in trace
                    if mode is not None)
    solid = S.alpha_in_opaque(batches, textures, palettes)
    print("%-8s %3d batches over %d triangles: %s; %d opaque with alpha "
          "in the tile; %d point-sampled"
          % (stage, len(batches), len(tris),
             ", ".join("%s x%d" % (k, v)
                       for k, v in sorted(per_list.items())), len(solid),
             point))
    return bad


def main():
    argv = sys.argv[1:]
    rom_path = M.ROM_DEFAULT
    stages = STAGES
    if "--rom" in argv:
        rom_path = argv[argv.index("--rom") + 1]
    if "--stage" in argv:
        stages = [argv[argv.index("--stage") + 1]]
    if not os.path.exists(P.GFXDIS):
        print("missing %s -- this runs in the container: ./run.sh test oracle"
              % P.GFXDIS)
        return 1
    P.require_decomp()
    rom = open(rom_path, "rb").read()

    lines = []
    bad = 0
    for stage in stages:
        bad += check_stage(rom, stage, lines.append)
    for line in lines:
        print(line)
    if bad:
        print("FAIL: %d triangles disagree with gfxdis" % bad)
        return 1
    print("rendermode_check: every stage batch's PVR list is the one its "
          "RDP render mode asks for, and its texture filter the one "
          "other-mode H holds")
    return 0


if __name__ == "__main__":
    sys.exit(main())

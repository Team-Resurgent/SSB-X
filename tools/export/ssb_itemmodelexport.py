#!/usr/bin/env python3
"""ssb64-dc: the items' own models, baked out of ITCommonObject.

Every item's `ITAttributes.data` points at a `DObjDesc` array in
ITCommonObject (relocData 0x56), and `itManagerMakeItem` walks it --
`gcSetupCustomDObjsWithMObj` on the common arm, `itManagerSetupItemDObjs`
on the `is_item_dobjs` one -- handing each entry's own `dl` to
`gcAddDObjForGObj`. Those are N64 display lists, and the port draws
nothing from one: `gcSubmitDObj` (src/dc/objdisplay.c) submits `dobj->dv`,
the BAKED model, and `gcDrawDObjForGObj` skips a DObj whose `dv` is NULL.
So an item built that way has the right attributes, the right collision
and no picture at all.

This bakes the tree instead. It is `bake_map_object`'s own recipe
(tools/export/ssb_stageexport.py:967) over a much smaller tree -- every one of
the 34 is 2 to 4 joints, which is why the whole set fits in one run.

TWO OF THE THIRTY-FOUR DRAW THROUGH A DL-HEAD ARRAY. Heart's and Sword's
`dl` is not a display list: it is `{ s32 list_id; Gfx *dl }` pairs
terminated by list_id == 4, which gcDrawDObjDLLinks queues into
gSYTaskmanDLHeads[list_id]. Read as commands it disassembles to
gsDPNoOpTag and gsDPNoOp and bakes to nothing -- which is how both came
out empty for a step. `dobj_dl_links` is the test, and it needs both
halves: a list may open with a zero word (G_NOOP is 0x00000000), so a
small list_id alone is ambiguous, and the walk to the terminator is what
settles it.

EVERY DObjDesc ENTRY THAT NAMES AN MObjSub GETS AN MObj. On the N64
`gcSetupCustomDObjs` allocates one per entry, and a dozen ported files
write through `dobj->mobj`: four of them (`itgshell.c`, `itrshell.c`,
`itbombhei.c`, `itnbumper.c`) unconditionally, which with no MObj is a
store past address zero. Twelve of the 34 tables carry `p_mobjsubs` --
Star, FFlower, BombHei, GShell, RShell, MBall, Tosakinto, Lizardon,
Spear, Starmie, GBumper, NBumper -- and this file once packed
none of them, because the rule was `animated` (does the table carry a
MatAnimJoint), which is about whether a SCRIPT moves the materials and
says nothing about whether the item's own code reads them.

EACH MObj CARRIES ONE FRAME, the picture its own batch already baked --
except Star and Fire Flower, the only two of the 34 whose
table carries a MatAnimJoint script (item_models's own docstring). Both
scripts step `palette_id` alone, never `texture_id_curr` (measured, not
assumed -- the file header comment here used to say Fire Flower's flame
stepped a sprite array and it does not). `pack_item` bakes those two
through the SAME dynamic-palette-bank mechanism of
tools/export/ssb_effectexport.py: one CI4 picture, one PVR bank, the raw N-frame
TLUT data riding in the pack for src/dc/objmodel.c to rewrite at runtime
whenever `palette_id`'s frame changes. Green/Red Shell's own `palette_id`
difference is NOT a script (their table carries none) -- it is a constant
their own it/itgshell.c-derived code sets, unrelated to this file.

WHAT IS NOT HERE IS ANIMATION. An item's `AnimJoint`/`MatAnimJoint`
scripts are read out of region 1 of the item pack by `itGetPData`, at the
offsets the decomp names, and the port's own interpreter plays them; they
are not part of this pack and would be wrong in it -- they address joints
by the game's index, and this pack's joints are the same tree but its own
numbering. The one exception is a FIGHTER item (FIGHTER_ITEMS below), whose
scripts are in a fighter's file the item pack does not carry: those travel
in the model pack as an animation block, one entry per pack joint, the way
an effect pack's do.

THE MONSTER WEAPONS COME OUT OF THE SAME FILE. ITCommonData also carries
twelve `WeaponAttributes` tables, and seven of them are the Poké Ball
monsters' own weapons -- a DObjDesc tree in ITCommonObject like an item's,
reached through the weapon's table rather than through an item's. `--weapons`
bakes those, to `wp<name>.mdl`, and reports the five that do not with a
reason (KNOWN_UNBAKED); four of the five are the ITEMS' weapons, which
have been firing invisibly since their items landed.

Usage:
  python3 tools/export/ssb_itemmodelexport.py --all --outdir romdisk/
  python3 tools/export/ssb_itemmodelexport.py --weapons --outdir romdisk/
  python3 tools/export/ssb_itemmodelexport.py --item Capsule --out romdisk/itcapsule.mdl
  python3 tools/export/ssb_itemmodelexport.py --list
"""
import argparse
import math
import os
import struct
import sys

sys.path[:0] = [os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), d)
                for d in ("lib", "export", "check")]

import ssb_assets as A                  # noqa: E402
import ssb_itemexport as I              # noqa: E402
import ssb_logicexport as L             # noqa: E402
import ssb_packexport as K              # noqa: E402
from ssb_effectexport import build_pack  # noqa: E402

ROOT = I.ROOT
ROM_DEFAULT = os.path.join(ROOT, "baserom.z64")


class EmptyBake(ValueError):
    """An item whose DObjDesc bakes to no triangles.

    Nothing to write and no bounds to compute (`world_verts` is empty and
    `bounds` would take a min of nothing), so it is raised rather than
    written -- a pack that bakes empty is an invisible item, which is
    what this file is for. NO ITEM HITS THIS ANY MORE: Heart and Sword
    did, for one step, and dobj_dl_links is what fixed them. Kept because
    the next model that reads as no-ops should fail loudly here rather
    than ship as an item nobody can see.
    """


def item_models(rom):
    """Every item's name and the ITCommonObject offsets its four pointers
    name, as (name, data, mobjsubs, matanims).

    They come from the ROM's own relocation words at the table's pointer
    sites -- the same pairs tools/export/ssb_itemexport.py builds the item
    pack's fixups from -- rather than from block names, because more than half
    are INTERIOR: the first table in the file points at 0x670, inside a
    model block and not at the start of one. Three of the four are
    optional and are None when the ROM has no word there.

    Thirteen of the thirty-four carry MObjSubs (their display lists name
    their material through segment 0x0E rather than drawing a bare
    texture), and two of those -- Star and Fire Flower -- carry a
    MatAnimJoint as well, which is what tells the baker to keep the MObjs
    as objects a script can move instead of folding them into the batches.
    """
    freed, _, _, sites, _ = L.file_info(rom, I.ITCOMMONDATA)
    fixmap = dict(sorted(sites))
    out = []

    for (kind, name, off) in I.read_blocks(I.ITCOMMONDATA):
        if kind != "ItemAttributes" or off >= len(freed):
            continue

        p = [fixmap.get(off + 4 * i) for i in range(4)]

        if p[0] is None:
            raise AssertionError("%s: no relocation at its data pointer" % name)
        out.append((name, p[0], p[1], p[3]))
    return out


# ---- the item EFFECT display lists ------------------------------------
#
# ITCommonObject's other 48 blocks are the model vocabulary; ONE of them is
# a bare display list rather than a DObjDesc tree, and it is not an item's:
# it is the Container smash debris. itBoxContainerSmashMakeEffect (which
# BOTH itbox.c and ittaru.c reach) reads it as
# `attr->data - BoxDataStart + BoxEffectDisplayList` and hands the same
# `Gfx*` to ITCONTAINER_EFFECT_COUNT hand-made DObjs, each with its own
# random scale and spin, then runs a custom process over them.
#
# The port draws nothing from a DL, so it bakes -- as a ONE-JOINT model,
# which is the same graft shape bake_map_object uses for a map object with
# no DObjDesc of its own (tools/export/ssb_stageexport.py:967). The port's side
# then gives each debris DObj that joint's payload through
# dc_model_hidden_payload, which is the accessor src/dc/objmodel.h already
# has for a DObj made outside dc_model_add_dobjs.
# The STAGE items: the trees that are in the stage's own
# model file rather than in ITCommonObject. PowerBlock's `data` is
# StageInishieFile3's DObjDesc at 0x11F8 (relocData/260_GRInishieMap.c
# names it in that table's own initializer), and the port's item pack
# carries the attributes under the key 0xD8 -- which is also the key
# `sITModels` wants, because itemModelAddToGObj is handed an
# `o_attributes` and nothing else.
#
# (item name, the file the tree is in, the DObjDesc's offset, the
# MObjSub chain's offset or None, the .mdl name the port's table uses.)
#
# The MObjSub offset is the item's `p_mobjsubs` in the same file -- for
# the Piranha Plant that is mobjlink_0x0A68, and it is what gives its
# DObj an MObj for `gcAddMObjMatAnimJoint(dobj->mobj, ...)` and
# `dobj->mobj->anim_wait` to write to. PowerBlock names none.
STAGE_ITEMS = [
    ("PowerBlock", 155, 0x11F8, None, "PowerBlock"),
    ("Pakkun", 155, 0x0C30, 0x0A68, "Pakkun"),
    ("GLucky", 159, 0x0360, None, "GLucky"),
    ("Porygon", 159, 0x0EA0, None, "Porygon"),
    ("Marumine", 159, 0x0790, None, "Marumine"),
    ("Hitokage", 159, 0x1990, 0x17D0, "Hitokage"),
    ("Fushigibana", 159, 0x2340, 0x2180, "Fushigibana"),
    # Race to the Finish's barrel bomb. Its tree is
    # relocData 162 (GRBonus3File3) at 0x788, which is where
    # 295_GRBonus3Map.c's own ITAttributes initializer points its
    # `data` -- `DataStart TaruBomb 0x788` in the map group says the
    # same.
    ("TaruBomb", 162, 0x788, None, "TaruBomb"),
    # Break the Targets' target. Its tree is not in a
    # stage's model file either: relocData 150 (ITBonus1Object) is the
    # target and nothing else -- a 4 KB data pool with the sign's RGBA32
    # image in it, one Vtx[4] quad, the display list that draws it, and
    # the three-joint DObjDesc at 0x10F8 that 253's `data` points at.
    # Every Break the Targets course loads that one file
    # (sc1PBonusStageBonus1LoadFile), so one .mdl serves all twelve.
    ("Target", 150, 0x10F8, None, "Target"),
]

# The STAGE WEAPONS: the same shape, for a weapon a stage item
# FIRES rather than for an item. Saffron City's Venusaur spits a razor whose
# tree is in the stage's model file (DObjDesc 0x2A50, whose body entry is a
# DLLink rather than a DL) and whose attributes the item pack carries under
# 0x308. The port's weapon table keys by the WPDesc POINTER, so the row this
# bakes for is src/dc/wpmanager.c's -- and the name here is the .mdl's.
#
# (item name, the file the tree is in, the DObjDesc's offset, the MObjSub
# chain's offset or None, the .mdl name the port's table uses.)
STAGE_WEAPONS = [
    ("FushigibanaRazor", 159, 0x2A50, None, "FushigibanaRazor"),
]

# The FIGHTER items: PK Fire's pillar, whose ITAttributes
# sits at 0x34 in NessSpecial1 (relocData 240) and whose `data` and
# `anim_joints` point into NessSpecial3 (336) -- a four-entry DObjDesc at
# 0xA08 whose two drawing joints hang off DObjDLLink arrays, and the
# per-joint AnimJoint table at 0xAF0 that spins the one and pulses the
# other. itnesspkfire.c is compiled unmodified, so there is no MakeItem of
# the port's own to attach a named script in, the way itpowerblock.c and
# itpakkun.c do; the table is baked into the pack instead and
# src/dc/itemmodel.c hands it to itManagerMakeItem's own gcAddAnimAll.
#
# Both pointers are read off the table's own extern relocations rather
# than written here. (item name, the file the table is in, the table's
# offset in it, the .mdl name the port's table uses.)
#
# Link's Bomb is the same shape: its table at 0x40 in
# LinkMain (225) names a DObjDesc tree at 0x18D8 and its AnimJoint table at
# 0x1990, both in relocData 353, and itlinkbomb.c is compiled unmodified.
FIGHTER_ITEMS = [
    ("NessPKFire", 240, 0x34, "NessPKFire"),
    ("LinkBomb", 225, 0x40, "LinkBomb"),
]

#
# The same is true of Race to the Finish's barrel bomb, one stage over:
# itTaruBombContainerSmashMakeEffect is itBoxContainerSmashMakeEffect
# with a different list, and its list is in the STAGE's own files --
# `DisplayList TaruBombEffect` at 0x8A0 of relocData 162, named by the
# map group 295. So an entry carries the file its list is
# in and the group that NAMES it, which for the Container debris are one
# and the same.
#
# (key: (<block kind> <block name>, file the list is in, group that
# names it))
EFFECTS = {
    "boxsmash": ("EffectDisplayList Box", I.ITCOMMONOBJECT, I.ITCOMMONDATA),
    "tarubombsmash": ("DisplayList TaruBombEffect", 162, 295),
}


# ---- the display lists an item swaps in --------------------------------
#
# Five items replace their model's display list while they play:
# `dobj->dl = itGetPData(ip, DataStart, <list>)`. On the N64 that is the
# whole of it. In the port `dl` is the same union member as the DCDisplay
# the renderer calls through, so the write has to become a switch to a
# baked model instead (src/dc/itemmodel.c itemModelSetDisplayList), and
# these are those models: each list baked as a one-joint pack, attached to
# the item's own DObj with dc_model_hidden_payload the way the Container
# debris is.
#
# The material a list draws with is the MObj the DObj carries when it is
# drawn, so it is baked with that MObj: the Bob-omb's walk lists with its
# tree's own (node 1 -- its default list IS WalkRight, but the right one is
# baked too so the item can be switched back), the Bumper's with the
# `MObjSub NBumperWait` itNBumperAttachedInitVars puts on in the same
# breath. The three monsters' are plain sprites.
#
# key: (the list's block, where its MObj comes from -- None, ("item", name,
# node) or ("block", MObjSub block name))
ALT_DISPLAY_LISTS = {
    "bombheiwalkl": ("DisplayList BombHeiWalkLeft", ("item", "BombHei", 1)),
    "bombheiwalkr": ("DisplayList BombHeiWalkRight", ("item", "BombHei", 1)),
    "nbumperwait": ("DisplayList NBumperWait", ("block", "MObjSub NBumperWait")),
    "kamex": ("DisplayList Kamex", None),
    "sawamura": ("DisplayList Sawamura", None),
    "wark": ("DisplayList Wark", None),
}


def alt_file_name(key):
    return "italt%s.mdl" % key


def pack_alt(rom, key):
    """One swapped-in display list as a one-joint .mdl (ALT_DISPLAY_LISTS)."""
    fobj, _, reloc, _, _ = L.file_info(rom, I.ITCOMMONOBJECT)
    block, mobj_from = ALT_DISPLAY_LISTS[key]
    kind, name = block.split(" ", 1)
    off = block_offset(rom, kind, name)

    root = A.DObjNode(-1, -1, None, (0.0, 0.0, 0.0), (0.0, 0.0, 0.0),
                      (1.0, 1.0, 1.0))
    node = A.DObjNode(0, 0, off, (0.0, 0.0, 0.0), (0.0, 0.0, 0.0),
                      (1.0, 1.0, 1.0))
    node.parent = root
    root.children.append(node)

    if mobj_from is None:
        subs = [[]]
    elif mobj_from[0] == "item":
        _, item, index = mobj_from
        rows = [r for r in item_models(rom) if r[0] == item]
        if len(rows) != 1 or rows[0][2] is None:
            raise AssertionError("%s: %s has no MObjSub table" % (key, item))
        _, data, mobjsubs, _ = rows[0]
        _, tree = A.read_dobj_tree(fobj, reloc, data)
        subs = [A.read_mobjsubs(fobj, reloc, mobjsubs, len(tree))[index]]
        if not subs[0]:
            raise AssertionError("%s: %s's node %d carries no MObj"
                                 % (key, item, index))
    else:
        mkind, mname = mobj_from[1].split(" ", 1)
        subs = [[A.read_mobjsub(fobj, reloc, block_offset(rom, mkind, mname))]]

    return bake_tree("Alt%s" % key, fobj, reloc, root, [node], subs,
                     target=off)


def block_offset(rom, kind, name, group=None):
    """The offset of a named block, from the pinned decomp's own
    descriptions -- the same table I.read_blocks reads. `group` is the
    reloc file whose group NAMES the block, which for ITCommonObject's
    own blocks is ITCommonData."""
    for (k, n, off) in I.read_blocks(I.ITCOMMONDATA if group is None
                                     else group):
        if k == kind and n == name:
            return off
    raise AssertionError("no %s %s block" % (kind, name))


def pack_effect(rom, key):
    """One effect display list as a one-joint .mdl."""
    import pygfxd

    (block, fid, group) = EFFECTS[key]
    fobj, _, reloc, _, _ = L.file_info(rom, fid)
    kind, name = block.split(" ", 1)
    off = block_offset(rom, kind, name, group)

    root = A.DObjNode(-1, -1, None, (0.0, 0.0, 0.0), (0.0, 0.0, 0.0),
                      (1.0, 1.0, 1.0))
    nodes = [A.DObjNode(0, 0, off, (0.0, 0.0, 0.0), (0.0, 0.0, 0.0),
                        (1.0, 1.0, 1.0))]
    nodes[0].parent = root
    root.children.append(nodes[0])

    # one empty MObj list per node: the debris carries no material chain
    baker = A.bake_retry(
        lambda force: A.MeshBaker(fobj, pygfxd, [[]], force_extent=force),
        lambda b: b.bake(root))
    verts, tris, joints = baker.verts, baker.tris, baker.joints

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

    world = baker.world_verts()
    lo, hi = baker.bounds()
    cx, cy, cz = ((lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2,
                  (lo[2] + hi[2]) / 2)
    radius = max(math.sqrt((p[0] - cx) ** 2 + (p[1] - cy) ** 2 +
                           (p[2] - cz) ** 2) for p in world)

    if not tris:
        raise EmptyBake("%s: %d joint(s), no triangles" % (key, len(baker.nodes)))

    blob = build_pack("IT%s" % key.upper(), len(baker.nodes), secs,
                      (len(verts), len(tris), len(baker.batches),
                       len(baker.textures), len(baker.palettes)),
                      [], (cx, cy, cz), radius)
    print("  %s: %d joint(s), %d verts, %d tris, %d batch(es)"
          % (key, len(baker.nodes), len(verts), len(tris),
             len(baker.batches)))
    return blob


def dobj_dl_links(fobj, reloc, off):
    """True when `off` is a DObjDLLink array rather than a display list.

    Most items' `dl` is a plain list, which opens with a GBI command --
    Tomato's first word is 0xE7000000, gsDPPipeSync. Heart's and Sword's
    open with a small `list_id` and their SECOND word is a POINTER, which
    the file's own relocation table confirms: `{ s32 list_id; Gfx *dl }`
    pairs terminated by list_id == 4, which is how gcDrawDObjDLLinks
    queues each list into gSYTaskmanDLHeads[list_id]. Read as commands
    instead, the whole thing disassembles to gsDPNoOpTag and gsDPNoOp --
    which is exactly how these two came out baking to nothing.

    Both halves of the test are needed: a list may legitimately open with
    a zero word (G_NOOP is 0x00000000), so the small list_id alone is
    ambiguous, and the pointer alone is not enough either -- the walk to
    the list_id == 4 terminator is what settles it.
    """
    if off is None:
        return False
    k = 0
    while k < 32:
        at = off + k * 8
        if at + 8 > len(fobj):
            return False
        list_id = struct.unpack_from(">i", fobj, at)[0]
        if list_id == 4:
            return k > 0
        if list_id < 0 or list_id >= 4 or (at + 4) not in reloc:
            return False
        k += 1
    return False


# THE THREE TREES WHOSE DISPLAY PROC, NOT WHOSE DISPLAY LIST, SETS THE
# RENDER MODE. `MeshBaker.bucket_of` reads the mode out of the list it is
# replaying, and where the list sets none it falls back to what the DL
# head says -- head 0 is the opaque one. That is right for every item
# drawn by the two SHARED procs (it/itdisplay.c itDisplayOPAProcDisplay
# sets no mode at all and itDisplayXLUProcDisplay draws through the DL
# links, whose list_id is the head the baker already reads), and wrong for
# a proc that sets one itself: the picture went into the opaque list,
# where the PVR ignores its texture's alpha, and each of these three is a
# sprite whose TLUT entry 0 is the transparent background. All three drew
# as a solid rectangle of that entry's colour.
#
# The mode named here is the one the proc that draws the model ON SCREEN
# sets. Snorlax has two display procs and the fall's is the one that
# matters: `itKabigonCommonProcDisplay` (G_RM_AA_ZB_TEX_EDGE) covers the
# rise, which happens above the map's top bound and off camera, and
# `itKabigonFallProcDisplay` swaps in G_RM_AA_XLU_SURF for the drop the
# player sees. Clefairy has the pair too, but its own model only ever
# draws under `itPippiCommonProcDisplay`; the XLU one it installs for
# Hitmonlee and Starmie, whose lists set XLU themselves and who already
# bake translucent.
#
# DIVERGES in one way that no PVR list can express: G_RM_AA_XLU_SURF has
# no Z compare, so the N64 draws the falling Snorlax over the stage it
# passes through, and both PVR lists depth-test. A platform occludes the
# fall here where the N64 did not. The alpha is what was actually broken
# and what this fixes -- the sprite's alpha is one bit, so which of the
# two alpha lists it lands in changes no pixel.
PROC_RENDERMODE = {
    "Kabigon": A.G_RM_AA_XLU_SURF,          # it/itmonster/itkabigon.c:115
    "Pippi": A.G_RM_AA_ZB_TEX_EDGE,         # it/itmonster/itpippi.c:121
}

# The same thing on the weapon side: Spearow's swarm is drawn by
# `itPippiWeaponSwarmProcDisplay` (it/itmonster/itspear.c:363), which sets
# G_RM_AA_XLU_SURF and hands the tree to wpDisplayMain.
WEAPON_PROC_RENDERMODE = {
    "PippiSwarm": A.G_RM_AA_XLU_SURF,
}


# The five of ITCommonData's twelve weapon-attribute tables that have no
# model in the port yet, and what each one costs. A bake that fails on
# anything NOT in here fails the run -- so the day one of these starts
# working, or a NEW one stops, is a day this prints something different.
#
# FOUR OF THE FIVE ARE ITEM-FIRED, and that is the finding rather than a
# footnote: the Ray Gun, the Fire Flower and the Star Rod (base and smash)
# all spawn a weapon with no pack in src/dc/wpmanager.c's table, so every
# one of them fires INVISIBLY today. The monster weapons in this same block
# were never reached before because no monster was ported; these four have
# been reachable since their items landed, and nothing said so because a
# weapon with no pack is a working weapon with no picture -- the same
# failure row 33 found on the item side.
KNOWN_UNBAKED = {
    # LizardonFlame: no relocation word at the data slot at all.
    # Charizard's flame draws from a display list the DESCRIPTIONS name
    # rather than through an attributes pointer (its `DataStart Lizardon`
    # block is the model; the flame itself is a sub-list of it). Reading
    # it is a real piece of work, not a missing word -- see
    # itlizardon.c's own MakeItem, which is what would have to hand it
    # over.
    "LizardonFlame": "its attributes have no data pointer at all; the "
                     "flame is a display list of the model block rather "
                     "than a tree of its own",
    # The two that ITEMS fire, not monsters.
    "LGunAmmo": "its WPDesc flags are 0x00, so its data pointer is a "
                "display list and not a tree; it is baked as a one-quad "
                "weapon instead (tools/export/ssb_effectexport.py --what "
                "lgunammo)",
    "FFlowerFlame": "its data slot holds no relocation word either, so "
                    "the Fire Flower's flames have no model in the port",
    # Both Star Rod tables name the SAME data pointer, 0x5458, and it is
    # not a tree: the WPDesc's flags are 0x00, so it is a display list --
    # the one Yoshi's stars and Kirby's stars draw. The Star Rod's row in
    # src/dc/wpmanager.c names wpyoshistar.mdl.
    "StarRod": "its WPDesc flags are 0x00, so its data pointer (0x5458) is "
               "a display list and not a tree; the Star Rod draws "
               "wpyoshistar.mdl, the same list",
    "StarRodSmash": "the same display list as StarRod, and the same pack",
}


def weapon_models(rom):
    """Every monster weapon's name and the ITCommonObject offsets its four
    pointers name, as (name, data, mobjsubs, matanims). `data` may be None.

    The same walk as item_models, over `WeaponAttributes` instead of
    `ItemAttributes`. ITCommonData carries TEN of those: the eight the
    Poké Ball monsters fire -- kinds nWPKindIwarkRock (0x17) through
    nWPKindDogasSmog (0x1D), plus Pippi's swarm -- and two that ITEMS
    fire, the Ray Gun's ammo and the Fire Flower's flame. A fighter's own
    weapon attributes are in his own file, so this is the whole
    ITCommonData set and nothing else.

    SEVEN of the ten bake. The other three are in KNOWN_UNBAKED below,
    with the reason and what it costs.
    """
    freed, _, _, sites, _ = L.file_info(rom, I.ITCOMMONDATA)
    fixmap = dict(sorted(sites))
    out = []

    for (kind, name, off) in I.read_blocks(I.ITCOMMONDATA):
        if kind != "WeaponAttributes" or off >= len(freed):
            continue

        p = [fixmap.get(off + 4 * i) for i in range(4)]

        out.append((name, p[0], p[1], p[3]))
    return out


def pack_fighter_item(rom, name, afid, aoff):
    """A FIGHTER_ITEMS row as a .mdl: the tree and its AnimJoint, both
    found through the ITAttributes' extern relocations in `afid`."""
    import ssb_extract
    from ssb_effectexport import animjoint_block

    af = A.get_file(rom, afid, ssb_extract)
    ext = A.walk_reloc_extern(rom, ssb_extract, afid, af)
    _d, mobj, _a, matanim = struct.unpack_from(">IIII", af, aoff)
    if mobj != 0 or matanim != 0:
        raise AssertionError("%s: p_mobjsubs 0x%08X / p_matanim_joints "
                             "0x%08X; this bake carries neither"
                             % (name, mobj, matanim))
    if aoff not in ext or (aoff + 8) not in ext:
        raise AssertionError("%s: data or anim_joints is not an extern "
                             "relocation" % name)
    fid, target = ext[aoff]
    anim_fid, anim_off = ext[aoff + 8]
    if anim_fid != fid:
        raise AssertionError("%s: the tree is in file %d and its AnimJoint "
                             "in %d" % (name, fid, anim_fid))
    fobj, _, reloc, _, _ = L.file_info(rom, fid)
    root, nodes = A.read_dobj_tree(fobj, reloc, target)
    if any(dobj_dl_links(fobj, reloc, n.dl_off) for n in nodes):
        root, nodes = A.read_dobj_tree(fobj, reloc, target, dl_links=True)
    entries, words, relocs = animjoint_block(fobj, reloc, anim_off,
                                             len(nodes), name)
    blob = bake_tree(name, fobj, reloc, root, nodes, [[] for _ in nodes],
                     anims=[(name, entries, words, relocs)])
    print("%-14s file %d + 0x%-5X, AnimJoint 0x%X: %d joints, %d scripted"
          % (name, fid, target, anim_off, len(nodes),
             sum(1 for e in entries if e >= 0)))
    return blob


def pack_item(rom, name, target, mobjsubs_off=None,
              matanims_off=None, dump=False, prefix="IT",
              fid=I.ITCOMMONOBJECT, tex_files=None):
    """One item's tree as a .mdl, or its numbers when `dump`.

    `prefix` is what the PACK calls the model -- `ITCapsule`, `WPMBall`.
    The port's fighter packs and effect packs use the same three-or-so
    letter convention, and it is what a `fighter: <name>` line in a
    serial log names.

    `fid` is the file the tree lives in, ITCommonObject for all 34
    items and the weapon tables, and the STAGE's own model file for the
    stage items -- PowerBlock's tree is
    StageInishieFile3's DObjDesc at 0x11F8, which is what
    `DataStart PowerBlock` in the map group names. The walk is the same
    either way: a DObjDesc array and a relocation chain.

    `tex_files` is `ssb_stageexport.texture_files`' map -- where a
    file's images are, when it points at ANOTHER file's. ITCommonObject
    names none of its own (`pack_items` raises if it ever does), so the
    34 items and the weapon tables pass nothing; PowerBlock's texture is
    StageInishieFile2's, which is the same cross-file reference every
    Inishie layer already has."""
    fobj, _, reloc, _, _ = L.file_info(rom, fid)

    root, nodes = A.read_dobj_tree(fobj, reloc, target)

    # Two of the 34 draw through a DL-head array instead of a plain list
    # (see dobj_dl_links), and read_dobj_tree takes the flag for the whole
    # tree -- so the tree is read once to find the nodes' targets, and
    # again with the flag when any of them is an array.
    links = any(dobj_dl_links(fobj, reloc, n.dl_off) for n in nodes)

    if links:
        root, nodes = A.read_dobj_tree(fobj, reloc, target, dl_links=True)

    if not nodes:
        raise AssertionError("%s: the DObjDesc at 0x%X is empty"
                             % (name, target))

    subs = ([[] for _ in nodes] if mobjsubs_off is None else
            A.read_mobjsubs(fobj, reloc, mobjsubs_off, len(nodes)))
    # PROC_RENDERMODE's three trees, seeded by name: every caller here --
    # `--all`, `--weapons`, `--list`, `--emit-table`, the stage items --
    # goes through this one function, so a row in either table reaches the
    # bake without a call site having to remember it.
    rendermode = (WEAPON_PROC_RENDERMODE if prefix == "WP"
                  else PROC_RENDERMODE).get(name)

    # The table's MatAnimJoint, when it has one. Three of the
    # monsters' weapons do -- Beedrill's swarm, Blastoise's water and
    # Koffing's smog -- and wpManagerMakeWeapon hands it to gcAddAnimAll
    # (wp/wpmanager.c:278). The port's weapon path plays the scripts the
    # PACK carries (dc_model_add_mobjs), so a script this function dropped
    # never ran: each of the three sat on the first of its three pictures,
    # and the smog, whose MObjSub PRIM alpha is 00 and whose script's first
    # command sets it to FF and then fades it, was only ever visible because
    # the unlit draw path used to ignore PRIM alpha altogether.
    #
    # items' tables carry scripts too (Star, Fire Flower,
    # both palette_id-only) and this used to drop them the same way,
    # WEAPONS ONLY -- bake_tree's dpal block below is what added the
    # second palette an animated MObj needs, so the restriction to
    # `prefix == "WP"` is gone; any table with a MatAnimJoint bakes it,
    # item or weapon alike.
    scripts = None
    if matanims_off is not None:
        if mobjsubs_off is None:
            raise AssertionError("%s: a MatAnimJoint and no MObjSubs" % name)
        scripts = [s for row in A.read_matanim_table(
            fobj, reloc, matanims_off, [len(lst) for lst in subs])
            for s in row]

    def subs_at(frame, spans):
        """The MObjSub lists with every sprite array read `spans` long and
        sitting on `frame` -- ssb_effectexport.py read_tree's rule, for the
        reason given there: the script bounds a sprite array, not a NULL."""
        sb = A.read_mobjsubs(fobj, reloc, mobjsubs_off, len(nodes))
        k = 0
        for lst in sb:
            for sub in lst:
                arr = reloc.get(sub["off"] + 0x04)
                if arr is not None:
                    sub["sprites"] = [reloc.get(arr + 4 * i)
                                      for i in range(spans[k])]
                if sub["sprites"]:
                    i = min(frame, len(sub["sprites"]) - 1)
                    sub["texture_id_curr"] = (0 if sub["sprites"][i] is None
                                              else i)
                k += 1
        return sb

    return bake_tree(name, fobj, reloc, root, nodes, subs, prefix=prefix,
                     tex_files=tex_files, dump=dump, target=target,
                     rendermode=rendermode, scripts=scripts,
                     subs_at=subs_at)


def bake_tree(name, fobj, reloc, root, nodes, subs, prefix="IT",
              tex_files=None, dump=False, target=None, anims=(),
              rendermode=None, scripts=None, subs_at=None):
    """pack_item's bake, from the tree on: `subs` is one MObjSub list per
    node (empty lists for none). Shared with pack_alt, whose one-joint
    tree is made rather than read. `anims` is build_pack's animation
    blocks, which only a FIGHTER_ITEMS row carries."""
    import pygfxd

    # The material chain, when the item has one: entry i is the MObj list
    # DObjDesc[i]'s DObj carries.
    #
    # EVERY ONE OF THEM BECOMES AN MObj IN THE PACK. On the N64 a DObjDesc
    # entry that names an MObjSub gets an MObj -- gcSetupCustomDObjs
    # allocates one per entry -- and a dozen ported files write through
    # `dobj->mobj`; the first version of this file kept them only when the
    # table carried a MatAnimJoint (`animated`), which is a rule about
    # whether a SCRIPT moves the materials and has nothing to do with
    # whether the item's own code reads them. Ten of the twelve tables with
    # `p_mobjsubs` were therefore packed without one and `dobj->mobj` came
    # back NULL -- a store past address zero in itgshell.c, itrshell.c,
    # itbombhei.c and itnbumper.c.
    #
    # `mobj_batches` is what makes the baker keep a batch's MObj identity
    # instead of folding its colours into the batch; the folding still
    # happens (the batch carries frame 0's colours), so the picture is the
    # same and the pointer is real.
    has_mobjs = any(len(s) != 0 for s in subs)
    # bake_retry, because a vertex can sample past a tile's clamp extent
    # when that tile was baked as a repeating period: it re-bakes with the
    # overrunning image forced to a clamped extent until nothing does.
    # Hammer is the item that needs it. Same call ssb_meshexport.py makes.
    # A weapon is drawn Z-less: wpDisplayMain (wp/wpdisplay.c) runs
    # wpDisplayDrawNormal -- gSPClearGeometryMode(G_ZBUFFER) and
    # G_RM_AA_XLU_SURF into head 1 -- ahead of every weapon's draw, and
    # every weapon reaches it (wpManagerMakeWeapon's four display procs,
    # PK Thunder's and the Yoshi egg's wrappers). A list of its own that
    # turns the Z buffer back on gets it back; the rest bake
    # FPACK_ZALWAYS and draw over whatever they overlap, as the N64 did.
    seed = ({"ztrack": True, "zbuffer": False,     # ssb_effectexport
             "zcmp": [False, False, False, False]}  # WP_ZSEED
            if prefix == "WP" else None)

    def bake_with(sb, paletted=False):
        return A.bake_retry(
            lambda force: A.MeshBaker(fobj, pygfxd, sb,
                                      mobj_batches=has_mobjs,
                                      force_extent=force,
                                      texture_files=tex_files,
                                      rendermode=None if rendermode is None
                                      else [rendermode, None, None, None],
                                      paletted=paletted, seed=seed),
            lambda b: b.bake(root))

    # `scripts` is one MatAnimJoint script (or None) per MObj, in table
    # order. Two independent frame tracks a script may step, texture_id_curr
    # (a sprite array -- how many frames bake_tree must rebuild the texture
    # list to) and palette_id (a MOBJ_FLAG_PALETTE bank -- no extra texture,
    # just more raw TLUT bytes for src/dc/objmodel.c to rewrite from); either,
    # both, or neither per MObj. Frame 0 is re-read
    # with the texture spans so the first bake and the later ones see the
    # same sprite arrays.
    nframes = None
    pal_ids = None
    if scripts is not None:
        if len(scripts) != sum(len(lst) for lst in subs):
            raise AssertionError("%s: %d MatAnimJoint scripts for %d MObjs"
                                 % (name, len(scripts),
                                    sum(len(lst) for lst in subs)))
        nframes = []
        pal_ids = []
        for k, sc in enumerate(scripts):
            ids = set() if sc is None else set(
                int(v) for v in A.matanim_frame_ids(fobj, reloc, sc,
                                                    A.MATANIM_BIT_TEXID))
            if min(ids, default=0) < 0:
                raise AssertionError("%s: MObj %d's script indexes off the "
                                     "front of its sprite array" % (name, k))
            nframes.append(max(ids, default=0) + 1)
            pids = set() if sc is None else set(
                int(v) for v in A.matanim_frame_ids(fobj, reloc, sc,
                                                    A.MATANIM_BIT_PALETTEID))
            if min(pids, default=0) < 0:
                raise AssertionError("%s: MObj %d's script indexes off the "
                                     "front of its palette array" % (name, k))
            pal_ids.append(pids)
        subs = subs_at(0, nframes)
        flat = [sub for lst in subs for sub in lst]
        for k, sub in enumerate(flat):
            if nframes[k] > 1 and not sub["sprites"]:
                raise AssertionError("%s: MObj %d has no sprite array and its "
                                     "script reaches frame %d"
                                     % (name, k, nframes[k] - 1))
            ids = set(int(v) for v in A.matanim_frame_ids(
                fobj, reloc, scripts[k], A.MATANIM_BIT_TEXID)) \
                if scripts[k] is not None else set()
            for i in ids:
                if sub["sprites"] and sub["sprites"][i] is None:
                    raise AssertionError("%s: MObj %d's script selects frame "
                                         "%d and that entry is NULL"
                                         % (name, k, i))

    # A palette-animated MObj needs its CI4 tile baked paletted (a real PVR
    # bank, MESH_PALETTED's own comment) rather than resolved to direct
    # colour, the same opt-in EFENTRY_PALETTED gives
    # tools/export/ssb_effectexport.py -- and it has to be decided before the
    # FIRST bake, not patched in after.
    needs_dpal = pal_ids is not None and any(pal_ids)
    baker = bake_with(subs, paletted=needs_dpal)
    verts, tris, joints = baker.verts, baker.tris, baker.joints

    # A seeded tree that did not land in the seed's list means the seed
    # did not reach the batch -- a list that sets its own mode after all,
    # or a head other than 0 -- and the bug this guards is silent: the
    # model simply draws in the wrong list again.
    if rendermode is not None:
        want = A.rendermode_list(rendermode)
        for i, b in enumerate(baker.batches):
            if (b["bucket"] & A.FPACK_LIST_MASK) != want:
                raise AssertionError(
                    "%s: batch %d baked into list %d, not the %s the "
                    "display proc sets" % (name, i,
                                           b["bucket"] & A.FPACK_LIST_MASK,
                                           A.rendermode_name(rendermode)))

    joint_of = {}
    for j in joints:
        for b in range(j["batch_first"], j["batch_first"] + j["batch_count"]):
            joint_of[b] = j["node"].index

    # The MObj section, when the tree has one. Every MObjSub in the file
    # becomes an MObj here, in table order, and `joint_first` says which
    # run of them belongs to which joint -- the format the port's
    # dc_model_add_mobjs reads back.
    #
    # ONE FRAME PER MObj by default: the picture its own batch baked. An
    # MObj whose script steps `texture_id_curr` or `palette_id` gets more
    # (below) -- Star and Fire Flower -- but neither is
    # needed for `dobj->mobj` to be a pointer an item may write to.
    m_entries, m_words, m_relocs = [], [], []
    mo = None

    if has_mobjs:
        joint_first, mobjs = [], []
        for lst in subs:
            joint_first.append((len(mobjs), len(lst)))
            mobjs.extend(lst)

        # `-1` is the format's "this batch has no MObj", which is normal:
        # a tree may have a plain textured DObj beside MObj ones (the Fire
        # Flower has one of each). What is NOT normal is an MObj the batch
        # and the tree disagree about the joint of.
        batch_mobj = []
        batch_of = {}
        for i, b in enumerate(baker.batches):
            m = b["mobj"]
            if m is None:
                batch_mobj.append(-1)
                continue
            if m[0] != joint_of[i]:
                raise AssertionError(
                    "%s: batch %d is on joint %d and names MObj %d of joint "
                    "%d" % (name, i, joint_of[i], m[1], m[0]))
            k = joint_first[m[0]][0] + m[1]
            batch_of.setdefault(k, i)
            batch_mobj.append(k)

        # ONE FRAME PER MObj, and it is the picture its own batch already
        # baked -- so `tex_first` is that batch's `tex` and `tex_count` is
        # 1, and NEITHER the texture list nor the batches are rewritten.
        # A tex_count of 1 also means fighter.c compiles no extra frame
        # headers (it skips `n < 2`) and draw_batches keeps using the
        # batch's own, which is the frame-0 picture either way.
        tex_first, tex_count = [], []
        for k in range(len(mobjs)):
            own = baker.batches[batch_of[k]]["mat"][0]
            if not 0 <= own < len(baker.textures):
                raise AssertionError("%s: MObj %d draws an untextured batch "
                                     "(tex %d)" % (name, k, own))
            tex_first.append(own)
            tex_count.append(1)

        # WITH A MatAnimJoint, an MObj whose script actually steps
        # texture_id_curr (nframes[k] > 1) or palette_id (pal_ids[k])
        # needs to draw exactly the one batch this rebuild (or the dpal
        # block below) can name unambiguously -- ssb_effectexport.py's
        # weapon bake, which is where the check comes from. An MObj whose
        # script is None, or touches neither track, is left alone: a tree
        # may have a plain textured DObj beside animated MObjs (the Fire
        # Flower has one of each, and so does a batch with no MObj at all).
        distinct = [k for k in range(len(mobjs))
                   if scripts is not None and
                   (nframes[k] > 1 or pal_ids[k])]
        if scripts is not None:
            for k in distinct:
                bs = [i for i, m in enumerate(batch_mobj) if m == k]
                if len(bs) != 1:
                    raise AssertionError(
                        "%s: MObj %d draws %d batches; a script replaces "
                        "one picture" % (name, k, len(bs)))
        if distinct:
            # EVERY distinct MObj gets its OWN texture-list entry (or run
            # of them), appended rather than deduped with anyone else's --
            # never the SAME entry two MObjs' `tex_first` shares, which the
            # baker's own content-based interning would otherwise hand two
            # MObjs baking to identical frame-0 bytes (Star's own two
            # joints do). Sharing would mean the dpal block below, which
            # rewrites a texture's `pal` field in place to give a colliding
            # MObj its own bank, corrupts the OTHER MObj's texture too --
            # confirmed the hard way, reading the shipped tex[0].pal back
            # out of a first draft of this bake and finding mobj 0's
            # picture pointing at mobj 1's new bank. Untouched MObjs and
            # unmobjed batches keep the baseline `tex_first`/`mat` computed
            # above -- append-only, so their indices never move.
            later = {k: [] for k in distinct}
            if max(nframes[k] for k in distinct) > 1:
                for frame in range(1, max(nframes[k] for k in distinct)):
                    b2 = bake_with(subs_at(frame, nframes),
                                   paletted=needs_dpal)
                    if (len(b2.verts), len(b2.tris), len(b2.batches),
                            b2.palettes) != (len(verts), len(tris),
                                             len(baker.batches),
                                             baker.palettes):
                        raise AssertionError(
                            "%s: frame %d bakes to different geometry"
                            % (name, frame))
                    for k in distinct:
                        if frame < nframes[k]:
                            later[k].append(b2.textures[
                                b2.batches[batch_of[k]]["mat"][0]])
            for k in distinct:
                i = batch_of[k]
                own = baker.batches[i]["mat"][0]
                new_first = len(baker.textures)
                baker.textures = baker.textures + [baker.textures[own]] \
                    + later[k]
                tex_first[k] = new_first
                tex_count[k] = nframes[k]
                baker.batches[i]["mat"] = \
                    (new_first,) + tuple(baker.batches[i]["mat"][1:])

        # Dynamic recolour (mechanism, tools/ssb_effectexport.
        # py): a MOBJ_FLAG_PALETTE MObj whose script steps palette_id
        # through more than one value never bakes a second texture the way
        # a sprite array's frames do above -- the pixels never change, only
        # the 16-colour TLUT does. This names the ONE PVR bank the MObj's
        # already-baked (paletted, since needs_dpal forced it) texture
        # claimed and hands the runtime the raw N-frame colour data to
        # rewrite that bank's entries from, in place, whenever palette_id
        # changes (src/dc/objmodel.c dc_joint_material). `tex_first[k]`
        # names the right texture whether or not the rebuild above ran, and
        # `baker.batches[i]["mat"][0]` is post-rebuild already if it did.
        dpals, dpal, dpal_claimed = [], [], {}
        for k in range(len(mobjs)):
            first, count, bank = 0, 0, -1
            ids = pal_ids[k] if scripts is not None else set()
            if ids:
                sub = mobjs[k]
                if not (sub["flags"] & A.MOBJ_FLAG_PALETTE):
                    raise AssertionError(
                        "%s: MObj %d's script steps palette_id but "
                        "MOBJ_FLAG_PALETTE is not set" % (name, k))
                n = max(ids) + 1
                if n > len(sub["palettes"]):
                    raise AssertionError(
                        "%s: MObj %d's script reaches palette %d and its "
                        "array holds %d" % (name, k, n - 1,
                                            len(sub["palettes"])))
                own_tex = tex_first[k]
                bank = baker.textures[own_tex]["pal"] if own_tex >= 0 else -1
                if bank < 0:
                    raise AssertionError(
                        "%s: MObj %d animates a palette but its own "
                        "texture is not paletted" % (name, k))
                if bank in dpal_claimed:
                    # Two MObjs' own textures baked to the SAME bank (their
                    # frame-0 bytes matched byte for byte) -- one live bank
                    # cannot hold two joints' current frame at once, so this
                    # joint gets a bank of its own and every batch it draws
                    # is repointed at it (fix, Run's
                    # crash's four joints).
                    new_bank = len(baker.palettes)
                    baker.palettes.append(baker.palettes[bank])
                    bank = new_bank
                tex = [b["mat"][0] for b in baker.batches]
                A.dpal_bind(baker.textures, tex, batch_mobj, k, bank)
                for b, t in zip(baker.batches, tex):
                    if b["mat"][0] != t:
                        b["mat"] = (t,) + tuple(b["mat"][1:])
                dpal_claimed[bank] = k
                first, count = len(dpals), n
                for i in range(n):
                    dpals.append(A.read_palette(fobj, sub["palettes"][i]))
            dpal.append((first, count, bank))

        if scripts is not None:
            from ssb_effectexport import matanim_block
            m_entries, m_words, m_relocs = matanim_block(
                fobj, reloc, scripts, name)
        else:
            # No MatAnimJoint: still one entry per MObj, each -1. The
            # table is mobj_count * alt_count words whatever the scripts
            # are, and an empty one had dc_model_add_mobjs read its index
            # out of whatever the heap held past the pack -- 0 made the
            # castle bumper's script the pack's end, a stray word made an
            # item in How to Play spin in gcParseMObjMatAnimJoint
            # (2026-09-22, seventeen packs).
            m_entries = [-1] * len(mobjs)

        mo = {
            "count": len(mobjs),
            "subs": b"".join(
                A.pack_mobjsub(sub, tex_first[k], tex_count[k],
                               dpal=dpal[k])
                for k, sub in enumerate(mobjs)),
            "joint": struct.pack("<%dh" % (2 * len(joint_first)),
                                 *[v for pair in joint_first for v in pair]),
            "batch": struct.pack("<%dh" % len(batch_mobj), *batch_mobj),
            "entry": struct.pack("<%di" % len(m_entries), *m_entries),
            "words": struct.pack("<%dI" % len(m_words), *m_words),
            "reloc": struct.pack("<%dI" % len(m_relocs), *m_relocs),
            "dpals": struct.pack("<%dH" % (16 * len(dpals)),
                                 *[c for frame in dpals for c in frame]),
            "dpal_count": len(dpals),
        }

    # Two items bake to NOTHING -- Heart and Sword, 2 and 3 joints with a
    # display list each and no triangle out of either (see the file header's
    # own note). A model that bakes empty would ship as an invisible item,
    # which is the exact failure this file exists to fix, so it is printed
    # rather than passed over: `--list` shows the same two at zero.
    if not tris:
        raise EmptyBake("%s: %d joints, %d display lists, no triangles"
                        % (name, len(baker.nodes),
                           sum(1 for n in baker.nodes
                               if n.dl_off is not None)))

    if dump:
        # `mobjs` is what the section WILL hold, computed here rather than
        # after the frames rewrite below, because --list is the check that
        # the bake changed only where it was meant to.
        return dict(joints=len(baker.nodes), verts=len(verts), tris=len(tris),
                    batches=len(baker.batches), textures=len(baker.textures),
                    palettes=len(baker.palettes), target=target,
                    mobjs=sum(len(s) for s in subs))

    secs = K.model_sections(
        [(n.parent.index, n.translate, n.rotate, n.scale)
         for n in baker.nodes],
        verts, tris,
        [(b["tri_first"], b["tri_count"], b["mat"], joint_of[i],
          b.get("bucket", 0)) for i, b in enumerate(baker.batches)],
        baker.textures, baker.palettes)

    world = baker.world_verts()
    lo, hi = baker.bounds()
    cx, cy, cz = ((lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2,
                  (lo[2] + hi[2]) / 2)
    radius = max(math.sqrt((p[0] - cx) ** 2 + (p[1] - cy) ** 2 +
                           (p[2] - cz) ** 2) for p in world)

    # The name the pack carries is the item's own, uppercased into the
    # port's `ITCapsule` shape; the file name is lowercased, which is what
    # every other pack in the port uses.
    return build_pack("%s%s" % (prefix, name), len(baker.nodes), secs,
                      (len(verts), len(tris), len(baker.batches),
                       len(baker.textures), len(baker.palettes)),
                      list(anims), (cx, cy, cz), radius, mobjs=mo)


def item_attr_offset(rom, want):
    """The `o_attributes` the item pack gives `want`: the offset of its
    `ItemAttributes` block in ITCommonData. The same walk item_models
    makes, over the same block order, so the two agree by construction."""
    freed, _, _, _, _ = L.file_info(rom, I.ITCOMMONDATA)

    for (kind, name, off) in I.read_blocks(I.ITCOMMONDATA):
        if kind == "ItemAttributes" and off < len(freed) and name == want:
            return off
    raise AssertionError("no ItemAttributes block for %s" % want)


def file_name(name):
    return "it%s.mdl" % name.lower()


def weapon_file_name(name):
    return "wp%s.mdl" % name.lower()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--rom", default=ROM_DEFAULT)
    ap.add_argument("--item")
    ap.add_argument("--all", action="store_true")
    ap.add_argument("--list", action="store_true")
    ap.add_argument("--effect", choices=sorted(EFFECTS),
                    help="bake one item EFFECT display list instead")
    ap.add_argument("--weapons", action="store_true",
                    help="bake the eight monster weapon models instead")
    ap.add_argument("--stage-items", action="store_true",
                    help="bake the stage items' models, out of the "
                         "stage model files")
    ap.add_argument("--alts", action="store_true",
                    help="bake the display lists items swap in, to --outdir")
    ap.add_argument("--emit-table", action="store_true",
                    help="print src/dc/itemmodel.c's own table")
    ap.add_argument("--out")
    ap.add_argument("--outdir")
    args = ap.parse_args()

    rom = open(args.rom, "rb").read()

    if args.alts:
        # The swapped-in display lists (ALT_DISPLAY_LISTS), one .mdl each.
        if not args.outdir:
            sys.exit("--outdir is required with --alts")
        for key in sorted(ALT_DISPLAY_LISTS):
            blob = pack_alt(rom, key)
            path = os.path.join(args.outdir, alt_file_name(key))
            open(path, "wb").write(blob)
            print("%-14s %6d bytes -> %s" % (key, len(blob),
                                             alt_file_name(key)))
        return

    if args.effect:
        # One effect, straight to --out: these are not items and have no
        # o_attributes to be keyed by.
        blob = pack_effect(rom, args.effect)
        path = args.out or ("it%s.mdl" % args.effect)
        open(path, "wb").write(blob)
        print("%-14s %6d bytes -> %s" % (args.effect, len(blob), path))
        return

    if args.stage_items:
        # The stage items' models, out of the STAGE's own model file.
        # One .mdl each, named by the port's table, and
        # keyed by the ITAttributes offset the item pack carries -- which
        # is the decomp's own number, not this file's.
        if not args.outdir:
            sys.exit("--outdir is required with --stage-items")

        import ssb_stageexport as S

        for (name, fid, target, mobjsubs, mdl) in STAGE_ITEMS:
            blob = pack_item(rom, name, target, mobjsubs, fid=fid,
                             tex_files=S.texture_files(rom, fid))
            path = os.path.join(args.outdir, file_name(mdl))
            open(path, "wb").write(blob)
            print("%-14s file %d + 0x%-5X %6d bytes -> %s"
                  % (name, fid, target, len(blob), file_name(mdl)))

        # ... and the stage WEAPONS, the same way but for the wp prefix and
        # table. Fired by an item, carried by the item pack.
        for (name, fid, target, mobjsubs, mdl) in STAGE_WEAPONS:
            blob = pack_item(rom, name, target, mobjsubs, fid=fid,
                             prefix="WP",
                             tex_files=S.texture_files(rom, fid))
            path = os.path.join(args.outdir, weapon_file_name(mdl))
            open(path, "wb").write(blob)
            print("%-14s file %d + 0x%-5X %6d bytes -> %s"
                  % (name, fid, target, len(blob), weapon_file_name(mdl)))

        # ... and the FIGHTER items, the same shape once
        # more: the item pack carries the attributes, and the tree and its
        # AnimJoint are in the fighter's own files.
        for (name, afid, aoff, mdl) in FIGHTER_ITEMS:
            blob = pack_fighter_item(rom, name, afid, aoff)
            path = os.path.join(args.outdir, file_name(mdl))
            open(path, "wb").write(blob)
            print("%-14s %6d bytes -> %s" % (name, len(blob),
                                             file_name(mdl)))
        return

    if args.weapons:
        # The Poké Ball monsters' own weapons, in ITCommonObject like the
        # items -- but they are NOT items: an item is found by its
        # `o_attributes` in ITCommonData, and a weapon the same way. The
        # port's weapon table keys by the WPDesc POINTER, so nothing here
        # needs an offset; what the table needs is the FILE, and the pack
        # name inside it.
        if not args.outdir:
            sys.exit("--outdir is required with --weapons")

        total = 0
        bad = {}

        for (name, target, mobjsubs, matanims) in weapon_models(rom):
            if target is None:
                bad[name] = "no relocation word at its data site"
                print("%-14s %-7s  NO MODEL -- %s"
                      % (name, "-", bad[name]))
                continue
            try:
                blob = pack_item(rom, name, target, mobjsubs, matanims,
                                 prefix="WP")
            except (EmptyBake, ValueError, IndexError, AssertionError) as e:
                # Per weapon, and NOT fatal to the rest: one table failing
                # to decode is that table's problem, and the exit code at
                # the end is what says so.
                bad[name] = str(e)
                print("%-14s 0x%-5x  FAILED -- %s" % (name, target, e))
                continue
            path = os.path.join(args.outdir, weapon_file_name(name))
            open(path, "wb").write(blob)
            total += len(blob)
            print("%-14s 0x%-5x %6d bytes -> %s"
                  % (name, target, len(blob), weapon_file_name(name)))

        print("weapons: %d bytes, %d tables, %d baked"
              % (total, len(weapon_models(rom)),
                 len(weapon_models(rom)) - len(bad)))

        # A table that will not bake and is not ALREADY known to be
        # un-bakeable is a new finding, and the run fails on it.
        fresh = sorted(set(bad) - set(KNOWN_UNBAKED))
        for name in sorted(bad):
            print("weapons: %-14s %s -- %s"
                  % (name,
                     "KNOWN" if name in KNOWN_UNBAKED else "NEW",
                     KNOWN_UNBAKED.get(name, bad[name])))
        if fresh:
            sys.exit("weapons: %d newly un-bakeable: %s"
                     % (len(fresh), ", ".join(fresh)))
        return

    models = item_models(rom)

    if args.emit_table:
        # The loader's table, emitted rather than hand-written: its key is
        # the item's `o_attributes` -- the same offset the item pack's own
        # attribute records carry, so the two packs are keyed alike and
        # neither needs a name-to-kind mapping. A row appears even for an
        # item that bakes empty, with a NULL name, so the missing ones are
        # visible in the table rather than absent from it.
        skipped_names = set()
        print("static const ITModelRow sITModels[] =\n{")
        for (name, target, mobjsubs, matanims) in models:
            off = item_attr_offset(rom, name)
            try:
                pack_item(rom, name, target, mobjsubs, matanims, dump=True)
                file = '"%s"' % file_name(name)
            except EmptyBake:
                file = "NULL"
                skipped_names.add(name)
            print('    { %-8s %-22s NULL, FALSE },   /* %s */'
                  % ("0x%X," % off, file + ",", name))
        print("};")
        print("/* %d rows, %d of them with no pack: %s */"
              % (len(models), len(skipped_names),
                 ", ".join(sorted(skipped_names)) or "none"))
        return

    if args.list:
        print("%-14s %-7s %5s %6s %5s %6s %5s %5s" %
              ("item", "data", "joints", "verts", "tris", "texs", "batches",
               "mobjs"))
        for (name, target, mobjsubs, matanims) in models:
            try:
                d = pack_item(rom, name, target, mobjsubs, matanims,
                              dump=True)
            except EmptyBake as e:
                print("%-14s 0x%-5x  BAKES EMPTY -- %s"
                      % (name, target, str(e).split(": ", 1)[1]))
                continue
            print("%-14s 0x%-5x %5d %6d %5d %6d %5d %5d" %
                  (name, target, d["joints"], d["verts"], d["tris"],
                   d["textures"], d["batches"], d["mobjs"]))
        return

    if args.item:
        models = [m for m in models if m[0] == args.item]
        if not models:
            sys.exit("no item named %s" % args.item)
    elif not args.all:
        sys.exit("--item <Name>, --all or --list is required")

    if args.item and not args.out:
        sys.exit("--out is required with --item")
    if args.all and not args.outdir:
        sys.exit("--outdir is required with --all")

    total = 0
    skipped = []
    for (name, target, mobjsubs, matanims) in models:
        try:
            blob = pack_item(rom, name, target, mobjsubs, matanims)
        except EmptyBake as e:
            skipped.append(name)
            print("%-14s 0x%-5x  SKIPPED -- %s"
                  % (name, target, str(e).split(": ", 1)[1]))
            continue
        path = args.out if args.item else os.path.join(args.outdir,
                                                       file_name(name))
        open(path, "wb").write(blob)
        total += len(blob)
        print("%-14s 0x%-5x %6d bytes -> %s" %
              (name, target, len(blob), os.path.basename(path)))
    print("%d model(s), %d bytes%s"
          % (len(models) - len(skipped), total,
             "" if not skipped else
             "; %d SKIPPED (bake empty): %s"
             % (len(skipped), ", ".join(skipped))))


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""ssb64-dc: one fighter, packed for runtime loading.

This writes the transpiled model (tools/lib/ssb_meshexport.py) plus the
fighter's *entire* animation set as little-endian binaries the target
loads from disc: <name>.pack, the model, scripts, attributes and the
animations' directory, and <name>.anm beside it, the animations' words
(4.9 MB of the twelve VS fighters' 8.2 MB), which a scene that plays few
of them need not hold. The two together are everything a match needs
resident for that fighter.

The layout mirrors src/dc/fighter.h exactly: the SH-4 is little-endian, every
section is 4-byte aligned, and the loader just fixes offsets into pointers.
No byte-swapping, no relocation, no parsing at runtime.

Usage: python3 tools/export/ssb_packexport.py --fighter <Name> --out <file.pack>
                                       [--rom <rom.z64>] [--tree high|low]
writes <file>.pack and <file>.anm beside it.
"""
import os
import re
import struct
import sys
import zlib

sys.path[:0] = [os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), d)
                for d in ("lib", "export", "check")]
import ssb_extract           # noqa: E402
import ssb_assets as A       # noqa: E402
import ssb_meshexport as M   # noqa: E402
import ssb_paths as P        # noqa: E402
import ssb_fgmexport as F    # noqa: E402  (fgm_names)
import ssb_wpattrexport as W  # noqa: E402  (description_offsets)

MAGIC = b"SSBPACKA"
ANIM_NAME_LEN = 40
ANIM_DIR_ENTRY = ANIM_NAME_LEN + 24     # sizeof(FPackAnim), fighter.h
# the .anm file's header (fighter.h FPackAnm): its words start after it,
# and FPackAnim.off_words counts from the file's start
ANM_MAGIC = b"SSBANIM1"
ANM_HEADER_SIZE = 16
ANM_TIERS = 2                           # FPACK_ANM_TIERS, fighter.h
ANM_TIERS_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                              "anm_tiers.tsv")


def all_anim_files():
    """Every FT<Name>Anim* relocData file in the decomp, {file id: name}."""
    out = {}
    for fn in os.listdir(M.RELOC_DIR):
        m = re.fullmatch(r"(\d+)_(FT\w+?Anim\w*)\.c", fn)
        if m:
            out[int(m.group(1))] = m.group(2)
    return out


_NAMED_ANIMS = None


def named_anims():
    """Every animation file the game's own code names, as the set of
    FT<Name>Anim* symbols behind an `ll<Name>FileID` anywhere in the
    decomp's source but relocData -- ft/ftdata.c's motion tables and the
    sc/scsubsys/scsubsysdata<char>.c tables, which are where the
    presentation animations (Win, Pose, Selected, Claps, Doll) are named
    and why fighter_anims takes a fighter's files by NAME and not off
    the motion table.

    Used to tell a dead file from a live one when a script will not
    parse. The ROM carries animation files nothing reaches -- Mario's
    Win1, Yoshi's ten Unknowns, Master Hand's four -- and a pack may drop
    those; it may never drop one the game would ask for."""
    global _NAMED_ANIMS
    if _NAMED_ANIMS is None:
        found = set()
        for root, dirs, fns in os.walk(os.path.join(P.DECOMP_DIR, "src")):
            dirs[:] = [d for d in dirs if d != "relocData"]
            for fn in fns:
                if not fn.endswith((".c", ".h")):
                    continue
                with open(os.path.join(root, fn), errors="replace") as fp:
                    found |= set(re.findall(r"\bll(FT\w+?Anim\w*)FileID\b",
                                            fp.read()))
        _NAMED_ANIMS = frozenset(found)
    return _NAMED_ANIMS


def fighter_anims(fighter, rows):
    """The animation files a fighter's pack carries, (name, file id) in file
    id order -- file order is bank order, which is what the game's motion
    ids index: every FT<Name>Anim* file, plus every other fighter's file
    the ROM's motion table names by id. The game loads an animation by
    file id and never asks whose it is, so Luigi's table names Mario's
    files for most of his moves and Purin's names Kirby's -- a clone
    carries a copy of what it borrows, since a pack is one fighter's whole
    bank and nothing else is resident when he plays alone. `rows` are the
    MainMotion table's AND the SubMotion table's (file id, script, flags)
    rows off the ROM, concatenated: both tables name files by id and both
    borrow. Master Hand's last three SubMotion rows name
    llFTYoshiAnimUnknown8/9/10, every Polygon's name its original's, and
    Giant DK's and DK's name each other's, so feeding this only the
    MainMotion rows left those files in no pack at all."""
    files = all_anim_files()
    own = re.compile(r"FT%sAnim\w*" % re.escape(fighter))
    keep = set(fid for fid, name in files.items() if own.fullmatch(name))
    borrowed = set()
    for fid, _script, _flags in rows:
        if fid != 0 and fid in files and fid not in keep:
            borrowed.add(fid)
    keep |= borrowed
    out = sorted((fid, files[fid]) for fid in keep)
    if borrowed:
        donors = sorted(set(re.match(r"FT(\w+?)Anim", files[f]).group(1)
                            for f in borrowed))
        print("  %d animations borrowed from %s, named by the motion tables"
              % (len(borrowed), ", ".join(donors)))
    return [(name, fid) for fid, name in out]



# The most vertices a fighter pack may have: src/dc/ftmanager.c cuts one
# clip buffer per fighter slot to FTMANAGER_VCLIP_MAX before it knows which
# packs the scene will load (the character select loads all twelve after
# the pools are cut, as the game does). Kirby is the largest at 2107:
# fifteen copy hats and two electric skeletons (V14).
VCLIP_MAX = 2304

# FTAttributes scalars, in struct order from the top of the attributes
# block (fttypes.h). Every field is a BE float on ROM except jumps_max.
ATTR_FIELDS = [
    (0x00, "size"), (0x04, "walkslow_anim_length"),
    (0x08, "walkmiddle_anim_length"), (0x0C, "walkfast_anim_length"),
    (0x1C, "rebound_anim_length"), (0x20, "walk_speed_mul"),
    (0x24, "traction"), (0x28, "dash_speed"), (0x2C, "dash_decel"),
    (0x30, "run_speed"), (0x34, "kneebend_anim_length"),
    (0x38, "jump_vel_x"), (0x3C, "jump_height_mul"),
    (0x40, "jump_height_base"), (0x44, "jumpaerial_vel_x"),
    (0x48, "jumpaerial_height"), (0x4C, "air_accel"),
    (0x50, "air_speed_max_x"), (0x54, "air_friction"), (0x58, "gravity"),
    (0x5C, "tvel_base"), (0x60, "tvel_fast"),
    (0x68, "weight"), (0x6C, "attack1_followup_frames"),
    (0x70, "dash_to_run"),
    # fttypes.h:901, the shield's size: ftCommonGuardUpdateShieldCollision
    # scales YRotN by it
    (0x74, "shield_size"),
    # fttypes.h:902, the shield break's upward launch velocity:
    # ftCommonShieldBreakFlySetStatus sets vel_air.y to it
    (0x78, "shield_break_vel_y"),
    # fttypes.h:903, the half-width of the blob shadow on the floor:
    # ftShadowProcDisplay puts the strip's outer vertices at the
    # fighter's x +- this and reads the floor line between them. 200 for every
    # fighter but Donkey Kong, whose is 350.
    (0x7C, "shadow_size"),
    # fttypes.h:904-910: the jostle box (ft/ftmain.c:1509-1568, fighters
    # shouldering each other) and the camera's per-fighter numbers
    # (gm/gmcamera.c gmCameraUpdateInterests, gmCameraCalcFighterZoomRange)
    (0x80, "jostle_width"), (0x84, "jostle_x"),
    (0x8C, "cam_offset_y"), (0x90, "closeup_camera_zoom"),
    (0x94, "camera_zoom"), (0x98, "camera_zoom_base"),
    # MPObjectColl map_coll (fttypes.h:911), the collision diamond: top,
    # center, bottom, width; then Vec2f cliffcatch_coll (fttypes.h:912),
    # the ledge-grab box. mpcommon.c:849-851 copies them into
    # coll_data at fighter creation.
    (0x9C, "map_coll_top"), (0xA0, "map_coll_center"),
    (0xA4, "map_coll_bottom"), (0xA8, "map_coll_width"),
    (0xAC, "cliffcatch_coll_x"), (0xB0, "cliffcatch_coll_y"),
    # fttypes.h:921, the size of the halo a KO'd fighter hangs from.
    # ft/ftcommon/ftcommonrebirth.c:100 passes it straight to
    # efManagerRebirthHaloMakeEffect, which is real in the port
    # (it was once a stub, and this field was one of the
    # three src/dc/ftcommon.c listed as "zero, the pack carries the
    # physics"). It is 0xEC because the four words after it are
    # shade_color[3] and fog_color, and the is_have bitfield word that
    # follows those is 0x100.
    (0xEC, "halo_size"),
    # fttypes.h:876-878, the cargo walk's three animation lengths:
    # ftDonkeyThrowFWalk divides the frame by them when the walk changes
    # speed (ftdonkeythrowfwalk.c:23-60). Only Donkey Kong's are nonzero
    # (50, 35, 20); appended here rather than at 0x10 so FPackAttr's
    # earlier floats keep their places.
    (0x10, "throw_walkslow_anim_length"),
    (0x14, "throw_walkmiddle_anim_length"),
    (0x18, "throw_walkfast_anim_length"),
]
ATTR_JUMPS_MAX_OFF = 0x64
ATTR_SHADE_COLOR_OFF = 0xF0
# ft/fttypes.h:951 `sb32 cliff_status_ga[5]`: nMPKineticsGround/Air for
# each cliff state, which ftCommonCliffCommon2UpdateCollData indexes
# (ftcommoncliffclimb.c:237) with an ftCommonCliffStatusKind. That enum
# (ft/ftcommon.h:377-390) has *six* members, so EscapeSlow reads one word
# past the array, into what the decomp names unused_0x2CC -- the game's
# own out-of-bounds read. Export six words so the port reproduces it; the
# sixth is that word (0 on every fighter, so EscapeSlow ends grounded).
# ft/fttypes.h:950 `s32 effect_joint_ids[5]`: the five joints
# ftParamGetEffectJointPosition cycles through, one per call, so a
# fighter on fire spreads the flames over his body instead of trailing
# them from one bone. Every arm of ftParamMakeEffect that opens with
# that call reads this table; a pack without this field makes all
# five read as joint 0, so every flame, spark and electric hit comes
# out of TopN.
ATTR_EFFECT_JOINT_OFF = 0x2A4
ATTR_EFFECT_JOINT_NUM = 5
ATTR_CLIFF_GA_OFF = 0x2B8
ATTR_CLIFF_GA_NUM = 6
# The hit-detection half. fttypes.h:904 sb32 is_metallic,
# which picks the sparks a hit throws; :924-945 the twenty-two is_have_*
# bit-fields, one word, IDO packs them MSB first so is_have_attack11 is
# bit 31; :946 FTDamageCollDesc damage_coll_descs[11], the hurtboxes
# {joint_id, placement, is_grabbable, offset, size}, 36 bytes each,
# joint_id -1 ending the list; :947 Vec3f hit_detect_range, the box
# around the fighter a hitbox has to be inside before any hurtbox is
# tested (gm/gmcollision.c:1119). And animlock (:949): a pointer into
# the file's pre-attributes data, two words of joint bits
# ftParamSetAnimLocks reads; the relocData source records where.
ATTR_IS_METALLIC_OFF = 0x88
ATTR_IS_HAVE_OFF = 0x100
ATTR_DAMAGE_COLL_OFF = 0x104
ATTR_DAMAGE_COLL_NUM = 11
ATTR_HIT_DETECT_RANGE_OFF = 0x290
# The joint tables: fttypes.h:948 setup_parts, the two words
# lbCommonSetupFighterPartsDObjs tests MSB first to decide which DObjDesc
# entries become DObjs; :953 hiddenparts, the FTHiddenPart rows
# ftMainUpdateHiddenPartID makes and links per animation. Both are
# pointers in the game and the relocData source records where each
# target sits in the file; the pack carries the rows inline, FPackAttr
# (src/dc/fighter.h) sized for the widest table on the roster (Samus, 13).
ATTR_HIDDENPART_MAX = 16
# ft/fttypes.h:969 FTAttributes.thrown_status: a pointer in the game, into
# this fighter's own table of what status a completed grab's release puts
# the CAUGHT fighter into -- one row per nFTKind* (nFTKindEnumCount = 27),
# forward-throw then back-throw, FTThrownStatus{status1,status2} each.
# Every roster file has the identical 432-byte shape
# (dNameMain_thrown_status[54] FTThrownStatus rows, read here in pairs);
# the relocData source's own location comment gives the ROM offset, same
# as animlock/setup_parts above.
ATTR_THROWN_STATUS_NUM = 27
# The item half of FTAttributes. Three groups,
# all of them read by ft/ftcommon/ftcommonget.c, ftcommonitemthrow.c and
# it/itmain.c, none of which had a caller before the item pickup was
# ported -- which is why the pack once did not carry them and src/dc/ftcomputer.c:4566 recorded the
# gap in a DIVERGES paragraph.
#
# fttypes.h:917 FTItemPickup item_pickup: two {offset, range} Vec2f pairs,
# light then heavy. ftCommonGetFindItem builds the rectangle an item has
# to be inside before A picks it up: centre at the fighter's position plus
# lr*offset.x/offset.y, half-extent range, widened by the item's own map
# collision diamond. All-zero (what the pack used to give) makes it a
# single degenerate point and nothing is ever in reach.
#
# fttypes.h:918-920 itemthrow_vel_scale / itemthrow_damage_scale /
# heavyget_sfx: per-fighter percentages on a thrown item's speed and
# damage (ftCommonItemThrowProcUpdate multiplies both into the throw), and
# the grunt a fighter makes lifting a heavy one (itMainSetFighterHold,
# nSYAudioFGMVoiceEnd meaning "no voice"). The two scales are hex in the
# decomp initializer, so they are cross-checked here rather than through
# `expected` above, whose regex reads 0x0064 as decimal 64.
#
# fttypes.h:968/970 joint_itemheavy_id / joint_itemlight_id: which joint
# an item hangs off while held -- itMainSetFighterHold points the held
# item's new parent DObj at fp->joints[id], and itMainSetFighterRelease
# reads the same joint's world position to place the item on release.
ATTR_ITEM_PICKUP_OFF = 0xC4
ATTR_ITEMTHROW_SCALE_OFF = 0xE4
ATTR_HEAVYGET_SFX_OFF = 0xE8
# fttypes.h:913-916 dead_fgm_ids[2] / deadup_sfx / damage_sfx /
# smash_sfx[3]: the voice half, seven u16 FGM ids just before
# item_pickup. ftCommonDeadInitStatusVars queues the first two on a
# KO, ftCommonDeadUpStarSetStatus plays the star-KO scream,
# ftCommonDamageInitDamageVars the hurt voice, and ftMainParseMotionEvent's
# PlaySmashVoice picks one of the three at random. Until they were
# carried, all seven read 0 on the target -- nSYAudioFGMExplodeS -- so
# every one of those voices was a small explosion. 14 bytes, then two of
# padding to item_pickup's f32 alignment.
ATTR_VOICE_OFF = 0xB4
ATTR_VOICE_FIELDS = (("dead_fgm_ids", 2), ("deadup_sfx", 1),
                     ("damage_sfx", 1), ("smash_sfx", 3))
ATTR_JOINT_ITEMHEAVY_OFF = 0x334
ATTR_JOINT_ITEMLIGHT_OFF = 0x33C
# fttypes.h:957-963 joint_rfoot_id / joint_rfoot_rotate / joint_lfoot_id /
# joint_lfoot_rotate, then unk_0x31C / unk_0x320 past a 16-byte filler:
# the slope contour. mpCommonUpdateFighterSlopeContour
# (mp/mpcommon.c:361-405) bends each foot joint's two-bone leg so the
# foot rests on a sloped floor; the rotate is the leg's lower bone length,
# unk_0x31C how far below the foot joint's parent the foot may rise, and
# unk_0x320 the steepest angle it may drop below the fighter's root.
ATTR_SLOPE_FIELDS = (("joint_rfoot_id", 0x2FC, "i"),
                     ("joint_rfoot_rotate", 0x300, "f"),
                     ("joint_lfoot_id", 0x304, "i"),
                     ("joint_lfoot_rotate", 0x308, "f"),
                     ("unk_0x31C", 0x31C, "f"),
                     ("unk_0x320", 0x320, "f"))
# fttypes.h:964 Vec3f *translate_scales: a per-joint
# scale on the translation tracks, which lets one set of animations fit
# a differently built skeleton. Only Luigi's attributes point at one
# (relocData/221_LuigiMain.c:194, 29 rows: Mario's animations on his
# taller limbs); every other Main file's is NULL. The pack carries it
# inline, one row per joint the game's joints array has, padded with
# identity rows past the file's own count, and a flag for "none".
# ft/ftdef.h:10 FTPARTS_JOINT_NUM_MAX: the widest slot table a figatree can
# have is one per DObj of the fighter's tree, hidden parts included, and
# the game's joints array bounds that (Samus's tallest is 33).
FTPARTS_JOINT_NUM_MAX = 37
ATTR_IS_HAVE_NAMES = [
    "attack11", "attack12", "attackdash", "attacks3", "attackhi3",
    "attacklw3", "attacks4", "attackhi4", "attacklw4", "attackairn",
    "attackairf", "attackairb", "attackairhi", "attackairlw", "specialn",
    "specialairn", "specialhi", "specialairhi", "speciallw",
    "specialairlw", "catch", "voice",
]
# aggregate initializers `{ a, b, ... }, /* name */` -> the scalar names
# above, in order
ATTR_GROUPS = {
    "map_coll": ("map_coll_top", "map_coll_center", "map_coll_bottom",
                 "map_coll_width"),
    "cliffcatch_coll": ("cliffcatch_coll_x", "cliffcatch_coll_y"),
}


def thrown_status_rows(fighter, f, src, tm, tsrc):
    """The 27 thrown-status pairs, read where the Main file's location
    comment says they are and held to the decomp's own initializer.
    """
    flat = struct.unpack_from(">%di" % (ATTR_THROWN_STATUS_NUM * 4), f,
                              int(tm.group(1), 16))
    if int(tsrc.group(1)) != ATTR_THROWN_STATUS_NUM * 2:
        raise AssertionError("%s.thrown_status: decomp declares %s rows, "
                             "not %d" % (fighter, tsrc.group(1),
                                        ATTR_THROWN_STATUS_NUM * 2))
    rowcount = len(re.findall(r"\{[^{}]*\}", tsrc.group(2)))
    if rowcount != ATTR_THROWN_STATUS_NUM * 2:
        raise AssertionError("%s.thrown_status: decomp initializer has %d "
                             "rows, not %d" % (fighter, rowcount,
                                              ATTR_THROWN_STATUS_NUM * 2))
    return [
        [[flat[fk * 4 + d * 2 + 0], flat[fk * 4 + d * 2 + 1]]
         for d in range(2)]
        for fk in range(ATTR_THROWN_STATUS_NUM)
    ]


def fighter_attributes(rom, fighter):
    """The physics half of FTAttributes, read from the fighter's Main file
    on ROM and asserted against the decomp's typed initializer.

    The relocData source records where the attributes block sits ("Pre-
    attributes data (... 0xNNN bytes)"); the decomp initializer supplies
    the expected values (region-conditional fields may carry two)."""
    main_id, main_path = M.find_reloc_source(fighter + "Main")
    src = M.read_source(main_path)
    # The last pre-attributes entry ends where the attributes start; the
    # "Pre-attributes data (N words, 0xNNN bytes)" header says the same
    # number where the source has one (Fox's does not), and the two are
    # checked against each other where both exist.
    ends = [int(o, 16) + int(n) for o, n in
            re.findall(r"/\* @ 0x([0-9A-Fa-f]+), (\d+) bytes: FTAttributes\.",
                       src.split("FTAttributes d", 1)[0])]
    if not ends:
        raise ValueError("no pre-attributes entries in %s" % main_path)
    base = max(ends)
    m = re.search(r"Pre-attributes data \((\d+) words, 0x([0-9A-Fa-f]+) "
                  r"bytes\)", src)
    if m and int(m.group(2), 16) != base:
        raise AssertionError("%s: pre-attributes header says 0x%X, the "
                             "entries end at 0x%X"
                             % (main_path, int(m.group(2), 16), base))

    dm = re.search(r"FTAttributes d\w+_attr = \{(.*?)\n\};", src, re.S)
    if not dm:
        raise ValueError("no FTAttributes initializer in %s" % main_path)
    # scalar initializer values in order, region-#if branches both kept
    vals = re.findall(r"(-?\d+\.?\d*)[fF]?\s*,\s*/\* (\w+) \*/",
                      dm.group(1))
    expected = {}
    for num, name in vals:
        expected.setdefault(name, set()).add(float(num))
    for body, name in re.findall(r"\{([^{}]*)\}\s*,\s*/\* (\w+) \*/",
                                 dm.group(1)):
        if name in ATTR_GROUPS:
            nums = re.findall(r"(-?\d+\.?\d*)[fF]?", body)
            for field, num in zip(ATTR_GROUPS[name], nums):
                expected.setdefault(field, set()).add(float(num))

    f = A.get_file(rom, main_id, ssb_extract)
    out = {}
    for off, name in ATTR_FIELDS:
        v = struct.unpack_from(">f", f, base + off)[0]
        out[name] = v
        if name in expected and \
                not any(abs(v - e) < 1e-5 for e in expected[name]):
            raise AssertionError("%s.%s: ROM %r, decomp says %r"
                                 % (fighter, name, v, expected[name]))
    jumps = struct.unpack_from(">i", f, base + ATTR_JUMPS_MAX_OFF)[0]
    if not 1 <= jumps <= 6:
        raise AssertionError("%s.jumps_max: %d" % (fighter, jumps))
    out["jumps_max"] = jumps

    eff = list(struct.unpack_from(">%di" % ATTR_EFFECT_JOINT_NUM, f,
                                  base + ATTR_EFFECT_JOINT_OFF))
    em = re.search(r"\{([^{}]*)\}\s*,\s*/\* effect_joint_ids \*/",
                   dm.group(1))
    if not em:
        raise ValueError("no effect_joint_ids initializer in %s" % main_path)
    ewant = [int(t) for t in em.group(1).split(",") if t.strip()]
    if eff != ewant:
        raise AssertionError("%s.effect_joint_ids: ROM %r, decomp says %r"
                             % (fighter, eff, ewant))
    out["effect_joint_ids"] = eff

    ga = list(struct.unpack_from(">%di" % ATTR_CLIFF_GA_NUM, f,
                                 base + ATTR_CLIFF_GA_OFF))
    gm = re.search(r"\{([^{}]*)\}\s*,\s*/\* cliff_status_ga \*/",
                   dm.group(1))
    if not gm:
        raise ValueError("no cliff_status_ga initializer in %s" % main_path)
    want = [0 if t.strip() == "FALSE" else 1
            for t in gm.group(1).split(",") if t.strip()]
    want += [int(v) for v in sorted(expected["unused_0x2CC"])]
    if ga != want:
        raise AssertionError("%s.cliff_status_ga: ROM %r, decomp says %r"
                             % (fighter, ga, want))
    out["cliff_status_ga"] = ga

    # -- the hit-detection half, each against the initializer ------------
    metallic = struct.unpack_from(">i", f, base + ATTR_IS_METALLIC_OFF)[0]
    mm = re.search(r"(TRUE|FALSE)\s*,\s*/\* is_metallic \*/", dm.group(1))
    if not mm or metallic != (1 if mm.group(1) == "TRUE" else 0):
        raise AssertionError("%s.is_metallic: ROM %d" % (fighter, metallic))
    out["is_metallic"] = metallic

    have = struct.unpack_from(">I", f, base + ATTR_IS_HAVE_OFF)[0]
    for i, name in enumerate(ATTR_IS_HAVE_NAMES):
        hm = re.search(r"(\d)\s*,\s*/\* is_have_%s \*/" % name, dm.group(1))
        if not hm:
            raise ValueError("no is_have_%s in %s" % (name, main_path))
        if ((have >> (31 - i)) & 1) != int(hm.group(1)):
            raise AssertionError("%s.is_have_%s: ROM word 0x%08X"
                                 % (fighter, name, have))
    out["is_have"] = have

    rows = []
    for i in range(ATTR_DAMAGE_COLL_NUM):
        rows.append(struct.unpack_from(">3i6f", f, base + ATTR_DAMAGE_COLL_OFF
                                       + 36 * i))
    cm = re.search(r"/\* damage_coll_descs \*/\s*\{(.*?)\n\t\},",
                   dm.group(1), re.S)
    if not cm:
        raise ValueError("no damage_coll_descs initializer in %s" % main_path)
    want = set()
    for jm in re.finditer(r"\{\s*(-?\d+),\s*(\d+),\s*(TRUE|FALSE)", cm.group(1)):
        want.add((int(jm.group(1)), int(jm.group(2)),
                  1 if jm.group(3) == "TRUE" else 0))
    for r in rows:
        if (r[0], r[1], r[2]) not in want:
            raise AssertionError("%s.damage_coll_descs: ROM row %r not in "
                                 "the decomp's" % (fighter, r[:3]))
    out["damage_coll_descs"] = rows

    rng = struct.unpack_from(">3f", f, base + ATTR_HIT_DETECT_RANGE_OFF)
    rm = re.search(r"\{([^{}]*)\}\s*,\s*/\* hit_detect_range \*/", dm.group(1))
    if not rm:
        raise ValueError("no hit_detect_range initializer in %s" % main_path)
    wr = [float(v) for v in re.findall(r"(-?\d+\.?\d*)[fF]?", rm.group(1))]
    if any(abs(a - b) > 1e-5 for a, b in zip(rng, wr)):
        raise AssertionError("%s.hit_detect_range: ROM %r, decomp says %r"
                             % (fighter, rng, wr))
    out["hit_detect_range"] = list(rng)

    am = re.search(r"/\* @ 0x([0-9A-Fa-f]+), 8 bytes: FTAttributes\.animlock "
                   r"target", src)
    if not am:
        raise ValueError("no animlock location comment in %s" % main_path)
    lock = list(struct.unpack_from(">2I", f, int(am.group(1), 16)))
    lm = re.search(r"_animlock\[2\] = \{\s*0x([0-9A-Fa-f]+),\s*0x([0-9A-Fa-f]+)",
                   src)
    if not lm or lock != [int(lm.group(1), 16), int(lm.group(2), 16)]:
        raise AssertionError("%s.animlock: ROM %r" % (fighter, lock))
    out["animlock"] = lock

    # -- the joint tables, each against the source's typed rows ----------
    sm = re.search(r"/\* @ 0x([0-9A-Fa-f]+), 8 bytes: FTAttributes\."
                   r"setup_parts target", src)
    if not sm:
        raise ValueError("no setup_parts location comment in %s" % main_path)
    setup = list(struct.unpack_from(">2I", f, int(sm.group(1), 16)))
    if setup != M.read_setup_parts_words(src):
        raise AssertionError("%s.setup_parts: ROM %r, decomp says %r"
                             % (fighter, setup, M.read_setup_parts_words(src)))
    out["setup_parts"] = setup
    hm = re.search(r"/\* @ 0x([0-9A-Fa-f]+), (\d+) bytes: FTAttributes\."
                   r"hiddenparts target", src)
    if not hm:
        raise ValueError("no hiddenparts location comment in %s" % main_path)
    want_rows = M.read_hiddenparts(src, fighter)
    n = int(hm.group(2)) // 16
    rows = [struct.unpack_from(">4i", f, int(hm.group(1), 16) + 16 * i)
            for i in range(n)]
    if [tuple(r) for r in rows] != [tuple(r) for r in want_rows]:
        raise AssertionError("%s.hiddenparts: ROM %r, decomp says %r"
                             % (fighter, rows, want_rows))
    if len(rows) > ATTR_HIDDENPART_MAX:
        raise AssertionError("%s: %d hidden-part rows, FPackAttr holds %d"
                             % (fighter, len(rows), ATTR_HIDDENPART_MAX))
    out["hiddenparts"] = rows

    tm = re.search(r"/\* @ 0x([0-9A-Fa-f]+), 432 bytes: FTAttributes\."
                   r"thrown_status target", src)
    tsrc = re.search(r"FTThrownStatus \w+_thrown_status\[(\d+)\]\s*=\s*\{"
                     r"(.*?)\n\};", src, re.S)
    if not tm and not tsrc:
        # A fighter the game never lets anyone throw has no rows to give.
        # Master Hand is the one: BossMain carries six of
        # the ten FTAttributes.* blocks and is missing file_handles,
        # skeleton, textureparts_container and thrown_status, because he
        # cannot be thrown, grabbed or shielded. That is the game's own
        # table declining to spend 432 bytes, not a damaged file -- so
        # the pack carries zeros and ftcommon's throw path, which never
        # asks a fighter of his kind, never reads them.
        #
        # Both halves have to be missing. A Main with the location
        # comment and no initializer (or the other way round) is a file
        # that has lost one of them, which is the case this would
        # otherwise hide, so it still raises below.
        out["thrown_status"] = [[[0, 0], [0, 0]]
                                for _ in range(ATTR_THROWN_STATUS_NUM)]
    elif not tm:
        raise ValueError("no thrown_status location comment in %s" % main_path)
    elif not tsrc:
        raise ValueError("no thrown_status initializer in %s" % main_path)
    else:
        out["thrown_status"] = thrown_status_rows(fighter, f, src, tm, tsrc)
    # -- the item half, each against the initializer ---------------------
    pick = list(struct.unpack_from(">8f", f, base + ATTR_ITEM_PICKUP_OFF))
    pm = re.search(r"\{((?:\s*\{[^{}]*\}\s*,?)+)\s*\}\s*,\s*"
                   r"/\* item_pickup \*/", dm.group(1))
    if not pm:
        raise ValueError("no item_pickup initializer in %s" % main_path)
    wp = [float(v) for v in re.findall(r"(-?\d+\.?\d*)[fF]?", pm.group(1))]
    if len(wp) != 8 or any(abs(a - b) > 1e-5 for a, b in zip(pick, wp)):
        raise AssertionError("%s.item_pickup: ROM %r, decomp says %r"
                             % (fighter, pick, wp))
    out["item_pickup"] = pick

    scales = list(struct.unpack_from(">2H", f,
                                     base + ATTR_ITEMTHROW_SCALE_OFF))
    for i, name in enumerate(("itemthrow_vel_scale",
                              "itemthrow_damage_scale")):
        sm2 = re.search(r"0x([0-9A-Fa-f]+)\s*,\s*/\* %s \*/" % name,
                        dm.group(1))
        if not sm2:
            raise ValueError("no %s initializer in %s" % (name, main_path))
        if scales[i] != int(sm2.group(1), 16):
            raise AssertionError("%s.%s: ROM %d, decomp says %d"
                                 % (fighter, name, scales[i],
                                    int(sm2.group(1), 16)))
    out["itemthrow_scales"] = scales
    # The sound ids are symbolic in the decomp (nSYAudioVoice*,
    # nSYAudioFGM*), so each is numbered the way the compiler numbers it
    # (ssb_fgmexport.fgm_names, which tools/check/fgm_check.py holds to a real
    # C compiler) and compared with the ROM's word.
    fgm_ids = F.fgm_names()

    def fgm_field(name, n, off):
        rom_ids = list(struct.unpack_from(">%dH" % n, f, base + off))
        vm = re.search(r"(\{[^{}]*\}|\w+)\s*,\s*/\* %s \*/" % name,
                       dm.group(1))
        if not vm:
            raise ValueError("no %s initializer in %s" % (name, main_path))
        names = re.findall(r"\w+", vm.group(1))
        want = [fgm_ids[x] for x in names]
        if rom_ids != want:
            raise AssertionError("%s.%s: ROM %r, decomp says %r (%s)"
                                 % (fighter, name, rom_ids, want, names))
        return rom_ids

    out["heavyget_sfx"] = fgm_field("heavyget_sfx", 1,
                                    ATTR_HEAVYGET_SFX_OFF)[0]
    voice = []
    off = ATTR_VOICE_OFF
    for name, n in ATTR_VOICE_FIELDS:
        voice += fgm_field(name, n, off)
        off += 2 * n
    assert off + 2 == ATTR_ITEM_PICKUP_OFF, hex(off)
    out["voice_sfx"] = voice

    # fttypes.h:922-923 SYColorRGBA shade_color[3] and fog_color:
    # the team shade a duplicate fighter is drawn under and the
    # alpha scale on a colour animation's tint, read by
    # ft/ftdisplaymain.c's fog functions. Sixteen bytes after halo_size,
    # held to the relocData initializer's hex.
    colors = list(f[base + ATTR_SHADE_COLOR_OFF:base + ATTR_SHADE_COLOR_OFF + 16])
    want = []
    for name, shape in (("shade_color",
                         r"\{\s*\{[^{}]*\}\s*,\s*\{[^{}]*\}\s*,\s*\{[^{}]*\}\s*\}"),
                        ("fog_color", r"\{[^{}]*\}")):
        cm = re.search(r"(%s)\s*,\s*/\* %s \*/" % (shape, name), dm.group(1))
        if not cm:
            raise ValueError("no %s initializer in %s" % (name, main_path))
        want += [int(x, 0) for x in re.findall(r"0x[0-9A-Fa-f]+|\b\d+\b",
                                               cm.group(1))]
    if colors != want:
        raise AssertionError("%s.shade_color/fog_color: ROM %r, decomp says %r"
                             % (fighter, colors, want))
    out["shade_fog_colors"] = colors

    joints = {}
    for name, off in (("joint_itemheavy_id", ATTR_JOINT_ITEMHEAVY_OFF),
                      ("joint_itemlight_id", ATTR_JOINT_ITEMLIGHT_OFF)):
        v = struct.unpack_from(">i", f, base + off)[0]
        jm2 = re.search(r"(-?\d+)\s*,\s*/\* %s \*/" % name, dm.group(1))
        if not jm2:
            raise ValueError("no %s initializer in %s" % (name, main_path))
        if v != int(jm2.group(1)):
            raise AssertionError("%s.%s: ROM %d, decomp says %s"
                                 % (fighter, name, v, jm2.group(1)))
        # -1 is the table's own "no such joint", the same sentinel the
        # slope contour below carries, and it reaches a pack for the
        # first time with Master Hand: BossMain names
        # joint 5 to hold a heavy item and -1 for a light one. Every
        # reader of these two -- ftcommonitemshoot.c,
        # ftcommonitemswing.c, ftmain.c, ftparam.c, itmain.c -- is an
        # item path reached only by a fighter already holding one, so
        # the -1 is never indexed.
        if not -1 <= v < FTPARTS_JOINT_NUM_MAX:
            raise AssertionError("%s.%s: joint %d out of range"
                                 % (fighter, name, v))
        joints[name] = v
    out.update(joints)

    # -- the slope contour, against the initializer ----------
    slope = []
    for name, off, kind in ATTR_SLOPE_FIELDS:
        v = struct.unpack_from(">" + kind, f, base + off)[0]
        sm = re.search(r"(-?[0-9.]+)f?\s*,\s*/\* %s \*/" % name, dm.group(1))
        if not sm:
            raise ValueError("no %s initializer in %s" % (name, main_path))
        want = float(sm.group(1))
        if (kind == "i" and v != int(want)) or \
                (kind == "f" and abs(v - want) > 1e-4 * max(1.0, abs(want))):
            raise AssertionError("%s.%s: ROM %r, decomp says %s"
                                 % (fighter, name, v, sm.group(1)))
        if kind == "i" and not -1 <= v < FTPARTS_JOINT_NUM_MAX:
            raise AssertionError("%s.%s: joint %d out of range"
                                 % (fighter, name, v))
        slope.append(v)
    out["slope_contour"] = slope

    # -- translate_scales, against the initializer --------
    tsm = re.search(r"/\* @ 0x([0-9A-Fa-f]+), (\d+) bytes: FTAttributes\."
                    r"translate_scales target", src)
    tsinit = re.search(r"Vec3f \w+_translate_scales\[(\d+)\]\s*=\s*\{"
                       r"(.*?)\n\};", src, re.S)
    if tsm is None:
        if tsinit is not None or \
                not re.search(r"NULL,\s*/\* translate_scales \*/", src):
            raise AssertionError("%s.translate_scales: no location comment, "
                                 "but the initializer is not NULL" % fighter)
        out["translate_scales"] = None
    else:
        n = int(tsm.group(2)) // 12
        if tsinit is None or int(tsinit.group(1)) != n or \
                n > FTPARTS_JOINT_NUM_MAX:
            raise AssertionError("%s.translate_scales: %d rows at 0x%s and "
                                 "no initializer that agrees"
                                 % (fighter, n, tsm.group(1)))
        rows = [struct.unpack_from(">3f", f, int(tsm.group(1), 16) + 12 * i)
                for i in range(n)]
        want = [tuple(float(v) for v in re.findall(r"(-?\d+\.?\d*)[fF]",
                                                    row))
                for row in re.findall(r"\{([^{}]*)\}", tsinit.group(2))]
        if len(want) != n or any(len(w) != 3 or
                                 any(abs(a - b) > 1e-5
                                     for a, b in zip(r, w))
                                 for r, w in zip(rows, want)):
            raise AssertionError("%s.translate_scales: ROM %r, decomp says "
                                 "%r" % (fighter, rows, want))
        out["translate_scales"] = rows

    out["attr_base"] = base
    return out


# ft/ftdata.c's tables live in the ovl2 overlay (the fighter engine);
# symbols/symbols_us.txt gives each table's vram and the splat config the
# overlay's ROM start and load address.
SYMBOLS_US = os.path.join(P.DECOMP_DIR, "symbols", "symbols_us.txt")
SPLAT_US = os.path.join(P.DECOMP_DIR, "smashbrothers.us.yaml")


def symbol_vram(name):
    for line in open(SYMBOLS_US):
        m = re.match(r"%s\s*=\s*0x([0-9A-Fa-f]+)\s*;" % re.escape(name), line)
        if m:
            return int(m.group(1), 16)
    raise ValueError("no %s in %s" % (name, SYMBOLS_US))


def overlay_rom_offset(name, vram):
    """ROM offset of `vram` inside code segment `name` of the splat config."""
    m = re.search(r"- name: %s\n\s+type: code\n\s+start: (0x[0-9A-Fa-f]+)"
                  r"\n\s+vram: (0x[0-9A-Fa-f]+)" % re.escape(name),
                  open(SPLAT_US).read())
    if not m:
        raise ValueError("no code segment %s in %s" % (name, SPLAT_US))
    return vram - int(m.group(2), 16) + int(m.group(1), 16)


def symbol_size(name):
    """Bytes from `name` to the next symbol in symbols_us.txt."""
    syms = []
    for line in open(SYMBOLS_US):
        m = re.match(r"(\w+)\s*=\s*0x([0-9A-Fa-f]+)\s*;", line)
        if m:
            syms.append((int(m.group(2), 16), m.group(1)))
    syms.sort()
    for i, (addr, nm) in enumerate(syms):
        if nm == name:
            return syms[i + 1][0] - addr
    raise ValueError("no %s in %s" % (name, SYMBOLS_US))


def fighter_motion_rows(rom, fighter):
    """dFT<F>MotionDescs off the ROM, (animation file id, script offset or
    0x80000000, flags) per row; see fighter_motion_table."""
    sym = "dFT%sMotionDescs" % fighter
    base = overlay_rom_offset("ovl2", symbol_vram(sym))
    size = symbol_size(sym)
    if size % 12 != 0:
        raise AssertionError("%s: %d bytes is not a whole number of rows"
                             % (sym, size))
    return [struct.unpack_from(">3I", rom, base + 12 * i)
            for i in range(size // 12)]


# ft/fttypes.h:85-117 FTData, thirty words: the two fields this needs are
# `submotion` (word 26) and `submotion_array_count` (word 28, itself a
# pointer to the s32). dFT<F>Data is in overlay 2 with the MainMotion
# table; what it points at is in overlay 1, where sc/scsubsys lives.
FTDATA_OFF_SUBMOTION = 26 * 4
FTDATA_OFF_SUBMOTION_COUNT = 28 * 4


def fighter_submotion_rows(rom, fighter):
    """dFT<F>SubMotionDescs off the ROM, the same (file id, script offset
    or 0x80000000, flags) rows as fighter_motion_rows.

    The demo statuses' motion table (measurement):
    ftMainSetStatus's two bands above FTSTAT_OPENING2_START index THIS
    table, not the MainMotion one, and it is what poses a fighter on the
    character select, the results screen, the 1P stage cards and in every
    opening movie.

    It is reached through dFT<F>Data rather than by name because the
    decomp gives these tables no symbol: sc/scsubsys/scsubsysdata*.c was
    decompiled, so symbols_us.txt knows them only as D_ovl1_* addresses.
    FTData holds the pointer and a pointer to the count, and all 27 read
    back exactly equal to the decomp's C initializers."""
    base = overlay_rom_offset("ovl2", symbol_vram("dFT%sData" % fighter))
    tbl = struct.unpack_from(">I", rom, base + FTDATA_OFF_SUBMOTION)[0]
    cnt = struct.unpack_from(">I", rom, base + FTDATA_OFF_SUBMOTION_COUNT)[0]
    if tbl == 0 or cnt == 0:
        raise ValueError("dFT%sData has no SubMotion table" % fighter)
    n = struct.unpack_from(">i", rom, overlay_rom_offset("ovl1", cnt))[0]
    if not 0 < n < 256:
        raise AssertionError("dFT%sSubMotionDescs: %d rows is not credible"
                             % (fighter, n))
    off = overlay_rom_offset("ovl1", tbl)
    return [struct.unpack_from(">3I", rom, off + 12 * i) for i in range(n)]


# sc/scsubsys/scsubsysdata<kind>.c: the SubMotion rows' event scripts,
# `s32 D_ovl1_<addr>[]` arrays compiled into overlay 1 beside the table
# rather than living in a relocData file the pack already copies. The
# decomp source is the oracle for where one array ends and the next
# begins; the ROM is what is shipped.
SUBSCRIPT_ARRAY_RE = re.compile(
    r"s32\s+D_ovl1_([0-9A-Fa-f]{8})\s*\[\s*\]\s*=\s*\{(.*?)\n\};", re.S)
SUBSCRIPT_PTR_RE = re.compile(r"ftMotionCommand(?:Subroutine|Goto)\s*\(")
SUBSCRIPT_TARGET_RE = re.compile(r"D_ovl1_([0-9A-Fa-f]{8})")
# ft/ftdef.h FTMotionEvent, the opcode in a command word's top six bits
FTMOTION_EVENT_SUBROUTINE = 34
FTMOTION_EVENT_GOTO = 36


def fighter_submotion_scripts(rom, fighter):
    """The kind's SubMotion event scripts as one blob:
    (words, reloc_site_word_indices, {ovl1 address: byte offset}).

    Every `s32 D_ovl1_*[]` in sc/scsubsys/scsubsysdata<kind>.c, in source
    order, read out of overlay 1 and concatenated. A Subroutine or Goto
    word is rewritten from an overlay-1 address to a byte offset into this
    blob and its index returned as a relocation site, which is the same
    contract fighter_scripts uses for a MainMotion file's intern chain --
    src/dc/fighter.c makes both into pointers with one loop.

    The decomp source is the oracle twice over. It gives the array ORDER,
    and so each array's extent as the next one's address; and it names
    each Subroutine/Goto target, which is checked against the word the ROM
    actually holds. The extent rule is checked on every array that has a
    successor (61 of the game's 74) and the last array of each file is
    taken on the source's own word count, which those 61 prove.

    A kind whose file has no arrays (Metal Mario, Giant DK, the ten
    Polygons) returns three empties."""
    path = os.path.join(P.DECOMP_DIR, "src", "sc", "scsubsys",
                        "scsubsysdata%s.c" % fighter.lower())
    if not os.path.exists(path):
        return [], [], {}
    src = open(path).read()

    arrays = []
    for m in SUBSCRIPT_ARRAY_RE.finditer(src):
        addr = int(m.group(1), 16)
        nwords, targets = 0, []
        for e in [x.strip() for x in m.group(2).split("\n") if x.strip()]:
            if SUBSCRIPT_PTR_RE.match(e):
                t = SUBSCRIPT_TARGET_RE.search(e)
                if t is None:
                    raise AssertionError("%s: D_ovl1_%08X has a Subroutine or "
                                         "Goto to no D_ovl1_ array"
                                         % (os.path.basename(path), addr))
                targets.append((nwords + 1, int(t.group(1), 16)))
                nwords += 2
            else:
                nwords += 1
        arrays.append((addr, nwords, targets))
    if not arrays:
        return [], [], {}

    by_addr, off = {}, 0
    for addr, nwords, _t in arrays:
        by_addr[addr] = off
        off += 4 * nwords

    words, sites = [], []
    for j, (addr, nwords, targets) in enumerate(arrays):
        if j + 1 < len(arrays):
            span = (arrays[j + 1][0] - addr) // 4
            if span != nwords:
                raise AssertionError(
                    "%s: D_ovl1_%08X spans %d words to the next array and "
                    "%d in the source" % (os.path.basename(path), addr,
                                          span, nwords))
        base = overlay_rom_offset("ovl1", addr)
        aw = list(struct.unpack_from(">%dI" % nwords, rom, base))
        for k, target in targets:
            if aw[k] != target:
                raise AssertionError(
                    "%s: D_ovl1_%08X word %d is 0x%08X, and the source says "
                    "it points at D_ovl1_%08X"
                    % (os.path.basename(path), addr, k, aw[k], target))
            if target not in by_addr:
                raise AssertionError(
                    "%s: D_ovl1_%08X points at D_ovl1_%08X, which is not one "
                    "of this file's arrays"
                    % (os.path.basename(path), addr, target))
            op = aw[k - 1] >> 26
            if op not in (FTMOTION_EVENT_SUBROUTINE, FTMOTION_EVENT_GOTO):
                raise AssertionError(
                    "%s: D_ovl1_%08X word %d is a pointer behind opcode %d, "
                    "which is neither Subroutine nor Goto"
                    % (os.path.basename(path), addr, k, op))
            sites.append(len(words) + k)
            aw[k] = by_addr[target]
        words += aw
    return words, sites, by_addr


def fighter_submotion_table(rom, fighter, anims, kept, scripts):
    """dFT<F>SubMotionDescs as (pack anim index, script offset, flags)
    rows, the same shape fighter_motion_table returns, and checked against
    the decomp's sc/scsubsys/scsubsysdata*.c initializer row by row.

    `scripts` is fighter_submotion_scripts' address map. A row's script
    column is a byte offset into that blob, or -1 for a row with no
    script -- 77 of the game's 386 rows have one, and what it carries is
    the voice and the loop timing of a pose (Mario's "Here we go!" on his
    win) rather than the pose itself, which is why the column could be cut
    and still leave every fighter posed."""
    raw = fighter_submotion_rows(rom, fighter)
    by_fid = {fid: name for name, fid in anims}
    index = {name: i for i, name in enumerate(kept)}
    rows = []
    for fid, off, flags in raw:
        if off in (0, 0x80000000):
            script = -1
        elif off in scripts:
            script = scripts[off]
        else:
            raise AssertionError("dFT%sSubMotionDescs: a row's script is "
                                 "0x%08X, which is not one of this kind's "
                                 "D_ovl1_ arrays" % (fighter, off))
        rows.append((-1 if fid == 0 else index.get(by_fid.get(fid), -1),
                     script, flags))

    # the decomp's own initializer, for the record -- the same check
    # fighter_motion_table makes of ft/ftdata.c
    path = os.path.join(P.DECOMP_DIR, "src", "sc", "scsubsys",
                        "scsubsysdata%s.c" % fighter.lower())
    m = re.search(r"FTMotionDesc dFT%sSubMotionDescs\[\]\s*=\s*\{(.*?)\n\};"
                  % re.escape(fighter), open(path).read(), re.S)
    if not m:
        raise ValueError("no dFT%sSubMotionDescs in %s" % (fighter, path))
    crows = [r.strip().rstrip(",")
             for r in m.group(1).strip().split("\n") if r.strip()]
    differ = []
    for i, c in enumerate(crows):
        if i >= len(raw):
            differ.append(i)
            continue
        fid, off, flags = raw[i]
        anim, coff, cflags = [x.strip() for x in c.split(",")]
        nm = re.match(r"&ll(FT\w+?Anim\w*)FileID$", anim)
        if (nm.group(1) if nm else None) != (by_fid.get(fid) if fid else None) \
                or ("0x80000000" in coff) != (off == 0x80000000) \
                or int(cflags, 16) != flags:
            differ.append(i)
    if len(crows) != len(raw) or differ:
        print("  note: %s has %d SubMotion rows to the ROM's %d and differs "
              "from row %s on; the ROM's rows are what is shipped"
              % (os.path.basename(path), len(crows), len(raw),
                 differ[0] if differ else "-"))
    return rows


def fighter_motion_table(rom, fighter, anims, kept):
    """dFT<F>MotionDescs as (pack anim index, script offset, flags) rows --
    the table the game's motion ids index -- read off the ROM's copy of
    the table: FTMotionDesc is {animation file id, script offset into the
    fighter's MainMotion file or 0x80000000 for none, anim flags}, and the
    table runs to the next symbol. The animation is found by its file id
    among `anims` (name, file id) and kept only if the bank kept it. The
    decomp's initializer is checked against it row by row and any
    difference is reported: Link's, for one, lacks two empty rows the ROM
    has, and rows are what the motion ids count. script offset is -1 for
    none; bits 31..29 of flags = TransN/XRotN/YRotN, bit 3 = AnimJoint."""
    sym = "dFT%sMotionDescs" % fighter
    base = overlay_rom_offset("ovl2", symbol_vram(sym))
    raw = fighter_motion_rows(rom, fighter)
    by_fid = {fid: name for name, fid in anims}
    index = {name: i for i, name in enumerate(kept)}
    rows = []
    for i, (fid, off, flags) in enumerate(raw):
        script = off if off != 0x80000000 else -1
        if fid == 0:
            rows.append((-1, script, flags))
            continue
        # an animation the bank skipped (overrunning script), or one
        # outside the fighter's own FT<F>Anim* files (Kirby's copy
        # abilities play other fighters'), keeps its row so motion ids
        # stay aligned, and points nowhere
        rows.append((index.get(by_fid.get(fid), -1), script, flags))

    # the decomp's table, for the record
    src = open(M.FTDATA_SOURCE).read()
    m = re.search(r"FTMotionDesc %s\[\]\s*=\s*\{(.*?)\n\};" % sym, src, re.S)
    if not m:
        raise ValueError("no %s in %s" % (sym, M.FTDATA_SOURCE))
    crows = re.findall(r"\{\s*([^,]+),\s*([^,]+)\s*,\s*([\w| ]+)\}",
                       m.group(1))
    flag_bits = {"FTANIM_FLAG_NONE": 0,
                 "FTANIM_FLAG_TRANSN_JOINT": 0x80000000,
                 "FTANIM_FLAG_XROTN_JOINT": 0x40000000,
                 "FTANIM_FLAG_YROTN_JOINT": 0x20000000,
                 "FTANIM_FLAG_SUBMOTION_SCRIPT": 0x10,
                 "FTANIM_FLAG_ANIMJOINT": 0x08,
                 "FTANIM_FLAG_TRANSLATE_SCALES": 0x04,
                 "FTANIM_FLAG_SHIELDPOSE": 0x02,
                 "FTANIM_FLAG_ANIMLOCKS": 0x01}
    differ = []
    for i, (fidcol, offcol, flagcol) in enumerate(crows):
        if i >= len(rows):
            differ.append(i)
            continue
        nm = re.match(r"&ll(FT\w+)FileID", fidcol.strip())
        raw = 0
        for tok in flagcol.split("|"):
            tok = tok.strip()
            raw |= flag_bits[tok] if tok in flag_bits else int(tok, 16)
        fid, off, flags = struct.unpack_from(">3I", rom, base + 12 * i)
        if (nm is None) != (fid == 0) or raw != flags or \
                ("0x80000000" in offcol) != (off == 0x80000000) or \
                (nm is not None and by_fid.get(fid) != nm.group(1)):
            differ.append(i)
    if len(crows) != len(rows) or differ:
        print("  note: ft/ftdata.c's %s has %d rows to the ROM's %d and "
              "differs from row %s on; the ROM's rows are what is shipped"
              % (sym, len(crows), len(rows),
                 differ[0] if differ else "-"))
    return rows


def fighter_scripts(rom, fighter):
    """The fighter's MainMotion relocData file -- the ftMotionCommand
    scripts and the throw tables beside them -- followed by a copy of the
    common moveset file and, for a kind whose motion table reads two
    files, the second one. Returns (words, reloc_sites, extern_count,
    sub_base): the words in host order, the word indices whose value is
    an offset into those words that the loader turns into a pointer, how
    many extern sites were resolved into the copy, and the BYTE offset
    the rows carrying FTANIM_FLAG_SUBMOTION_SCRIPT lay their offsets
    onto. Every word is treated as a u32: the scripts are u32 command
    words and the throw tables s32 fields, and nothing narrower lives in
    these files.

    The pointers are the two chains lb/lbreloc.c walks: the file's intern
    chain, into itself, and its extern chain, into FTCommonMoveset -- the
    item swings (Beam Sword, Bat, Fan, Star Rod, Fire Flower, Hammer) and
    the thrown-damage scripts, which every fighter's own script calls as
    a subroutine. The game loads that file once and shares it; each pack
    carries its own copy (2096 bytes), its own intern chain rebased, so
    the loader has one base to add. A MainMotion extern into any other
    file is refused.

    Two files, and sub_base, are Metal Mario's. The game
    keeps both loaded and picks per row at run time (ft/ftmain.c:4747-
    4762); the port has one blob and one pack offset per row, so the pick
    is made HERE, when the row's offset is laid onto the blob. Nothing in
    src/dc changes and FTANIM_FLAG_SUBMOTION_SCRIPT is still carried
    verbatim in FPackMotion.flags. Every other kind has one file and
    sub_base is 0, which is the same arithmetic."""
    main_name, sub_name = M.motion_sources(fighter)
    # A kind with only a submotion slot (Giant DK, the Polygons) reads
    # that file for every row, so it IS the blob and sub_base stays 0.
    first = main_name or sub_name
    second = sub_name if (main_name and sub_name) else None
    common_fid, _cpath = M.find_reloc_source("FTCommonMoveset")
    common = A.get_file(rom, common_fid, ssb_extract)
    ce = ssb_extract.read_entry(rom, ssb_extract.RELOC_SEG, common_fid)
    if len(common) % 4 != 0 or ce["reloc_extern"] != 0xFFFF:
        raise AssertionError("FTCommonMoveset is not a self-contained word "
                             "file")

    words, sites, extern = [], [], 0

    def add_motion_file(name, base, common_base):
        """One MainMotion file's words at `base`, its intern chain rebased
        into itself and its extern chain into the copy at `common_base`."""
        nonlocal extern
        fid, _path = M.find_reloc_source(name + "MainMotion")
        f = A.get_file(rom, fid, ssb_extract)
        e = ssb_extract.read_entry(rom, ssb_extract.RELOC_SEG, fid)
        if len(f) % 4 != 0:
            raise AssertionError("%s MainMotion is %d bytes, not whole words"
                                 % (name, len(f)))
        fw = list(struct.unpack(">%dI" % (len(f) // 4), f))
        for site, target in sorted(A.walk_reloc(f, e["reloc_intern"]).items()):
            if target >= len(f):
                raise AssertionError("%s MainMotion reloc @0x%X -> 0x%X is "
                                     "past the file" % (name, site, target))
            fw[site // 4] = base + target
            sites.append((base + site) // 4)
        wattr = W.description_offsets(fid)
        for site, (target_fid, target) in sorted(
                A.walk_reloc_extern(rom, ssb_extract, fid, f).items()):
            if site in wattr.values():
                # Not a script pointer: WPAttributes.data, field 0 of a
                # weapon record this MainMotion file happens to hold.
                # Master Hand is the only fighter whose bullet rows live
                # in his motion file rather than a Special file ), so his are
                # the first externs here that leave
                # FTCommonMoveset -- both name his own BossModel, where
                # the bullet's DObjDesc array sits.
                #
                # The port does not read the pointer through this blob.
                # romdisk/wpattrs.bin owns the weapon records and bakes
                # every pointer field as a marker -- 0 for a slot the ROM
                # relocated, WATTR_NO_PTR for one it did not -- because
                # the only thing that reads `data` is
                # wpManagerIsModelLess, and only as NULL-or-not; the
                # model comes from the pack table keyed by WPDesc. So
                # this writes the same 0, and the record stays what
                # wpattrs.bin says it is.
                #
                # The offset comes from the decomp's own
                # relocFileDescriptions, the same oracle
                # ssb_wpattrexport uses, so a file whose weapon records
                # move is a mismatch there and not a silent zero here.
                fw[site // 4] = 0
                continue
            if target_fid != common_fid or target >= len(common):
                raise AssertionError("%s MainMotion extern @0x%X -> file %d "
                                     "+0x%X is neither FTCommonMoveset nor a "
                                     "WPAttributes.data slot"
                                     % (name, site, target_fid, target))
            fw[site // 4] = common_base + target
            sites.append((base + site) // 4)
            extern += 1
        return fw

    first_len = len(A.get_file(
        rom, M.find_reloc_source(first + "MainMotion")[0], ssb_extract))
    words += add_motion_file(first, 0, first_len)

    common_words = list(struct.unpack(">%dI" % (len(common) // 4), common))
    for site, target in sorted(A.walk_reloc(common,
                                            ce["reloc_intern"]).items()):
        if target >= len(common):
            raise AssertionError("FTCommonMoveset reloc @0x%X -> 0x%X is "
                                 "past the file" % (site, target))
        common_words[site // 4] = first_len + target
        sites.append((first_len + site) // 4)
    words += common_words

    sub_base = 0
    if second is not None:
        sub_base = 4 * len(words)
        words += add_motion_file(second, sub_base, first_len)
    sites.sort()
    return words, sites, extern, sub_base


def model_sections(nodes, verts, tris, batches, textures, palettes):
    """fighter.h's model struct sections, from plain data.

    nodes: (parent_index, translate, rotate, scale) tuples.
    batches: (tri_first, tri_count, mat, joint, bucket) with mat the
    baker's material key (tex, prim, light1, light2, uses_shade, lit,
    alpha_tex, alpha_shade, env).
    Shared by the fighter and stage exporters.
    """
    sec_joints = b"".join(
        struct.pack("<i9f", parent, *(list(t) + list(r) + list(sc)))
        for (parent, t, r, sc) in nodes)
    sec_verts = b"".join(
        struct.pack("<8f2BH", x, y, z, nx, ny, nz, u, v, alpha, joint, 0)
        for (x, y, z, u, v, nx, ny, nz, alpha, joint) in verts)
    sec_tris = b"".join(struct.pack("<3H", a, b, c) for (a, b, c) in tris)
    sec_batches = b""
    for batch in batches:
        (tri_first, tri_count, mat, joint, bucket) = batch[:5]
        # The sixth is the model part the batch belongs to (MeshBaker
        # _bake_part), 0 for a batch outside one and for every pack but a
        # fighter's; it rides alpha_src's high byte (fighter.h FPackBatch).
        part = batch[5] if len(batch) > 5 else 0
        # The tenth element is which MObj drew the batch, and it is only
        # ever set when the caller asked for it (MeshBaker mobj_batches);
        # the batch section has no room for it and ssb_emblemexport.py,
        # the one exporter that needs it, carries it in its own section.
        (tex, prim, l1, l2, uses_shade, lit, alpha_tex,
         alpha_shade, env) = mat[:9]
        # The eleventh element is "the colour cycle cross-fades two tiles"
        # (MeshBaker._combine_tex_lerp). A caller that built its own
        # material tuple by hand -- the shorter ones predate the key -- has
        # no such batch by construction.
        texlerp = mat[10] if len(mat) > 10 else False
        # shaded: 0 = flat material colour, 1 = N.L over vertex normals,
        # 2 = vertex colour (G_LIGHTING off -- stage geometry).
        shaded = (1 if lit else 2) if uses_shade else 0
        # alpha_src: which sources the RDP's *alpha* cycle reads, which is
        # programmed separately from the colour cycle and often disagrees
        # with it (fighter.h FPACK_ALPHA_*).
        alpha_src = (1 if alpha_tex else 0) | (2 if alpha_shade else 0)
        # ...and which halves of PRIM the two cycles read (the key's
        # thirteenth and fourteenth; fighter.h FPACK_ALPHA_PRIM,
        # FPACK_COLOR_PRIM). A material tuple built by hand predates the
        # split and means what it always did: PRIM whole.
        if len(mat) > 13:
            alpha_src |= (8 if mat[12] else 0) | (4 if mat[13] else 0)
        else:
            alpha_src |= 8 | 4
        # env: the flat colour the cycle lerps from, and the flag that
        # says it does. A batch whose combiner does not read ENV carries
        # zero and no flag, which is every batch but a dead explosion's
        # (fighter.h FPACK_ENVLERP).
        sec_batches += struct.pack("<2Hh2B2H4I", tri_first, tri_count,
                                   tex, shaded, joint,
                                   bucket | (0 if env is None
                                             else A.FPACK_ENVLERP)
                                          | (A.FPACK_TEXLERP if texlerp
                                             else 0),
                                   alpha_src | part << 8, prim, l1, l2,
                                   0 if env is None else env)
    sec_pals = b"".join(struct.pack("<16H", *pal) for pal in palettes)

    texdata = bytearray()
    sec_texs = b""
    for t in textures:
        while len(texdata) & 3:
            texdata.append(0)
        blob = A.pack_texture(t)
        sec_texs += struct.pack("<2I2H4B", len(texdata), len(blob),
                                t["w"], t["h"], max(t["pal"], 0),
                                (2 if t["clamp_u"] else 0) |
                                (1 if t["clamp_v"] else 0), t["fmt"], 0)
        texdata += blob
    return {"joints": sec_joints, "verts": sec_verts, "tris": sec_tris,
            "batches": sec_batches, "texs": sec_texs, "pals": sec_pals,
            "texdata": bytes(texdata)}


def fighter_costumes(rom, fighter, tree, built):
    """Every costume's materials over costume 0's mesh (fighter.h
    FPackCostumes): (textures, [[(tex, prim, light1, light2, env)] per
    batch] per costume).

    A costume is the frame each MObj's costume script is played to
    (lbCommonAddMObjForFighterPartsDObj), which moves colours and palette
    ids and nothing else, so each one is baked whole and must come out
    as costume 0's mesh with other materials: the same vertices, the same
    triangles, the same batches, and only the texture and the four
    colours of a batch differing -- and a texture part's frames, which
    are tiles too. The textures are all the costumes' together, costume
    0's first and in its order, so the batches' own indices stand."""
    import contextlib
    import io

    base = built["baker"]
    textures = list(base.textures)
    tex_ix = {(t["fid"], t["key"]): i for i, t in enumerate(textures)}
    costumes = []
    for c in range(M.costume_count(fighter)):
        if c == 0:
            b = base
        else:
            with contextlib.redirect_stdout(io.StringIO()):
                b = M.build_fighter(rom, fighter, tree, costume=c)["baker"]
            if b.verts != base.verts or b.tris != base.tris or \
                    len(b.batches) != len(base.batches) or b.palettes:
                raise AssertionError("%s: costume %d's mesh is not costume "
                                     "0's" % (fighter, c))
        rows = []
        for i, (x, y) in enumerate(zip(base.batches, b.batches)):
            mx, my = list(x["mat"]), list(y["mat"])
            for k in (0, 1, 2, 3, 8):
                mx[k] = my[k] = None
            # a texture part's frames: the same part and count, other tiles
            for m in (mx, my):
                if m[11] is not None:
                    m[11] = (m[11][0], len(m[11][1]))
            if (x["tri_first"], x["tri_count"], x["bucket"], x.get("part"),
                    x["node"].index, mx) != \
                    (y["tri_first"], y["tri_count"], y["bucket"], y.get("part"),
                     y["node"].index, my):
                raise AssertionError("%s: costume %d's batch %d differs from "
                                     "costume 0's in more than its texture "
                                     "and colours" % (fighter, c, i))
            def union(tex):
                if tex < 0:
                    return tex
                t = b.textures[tex]
                k = (t["fid"], t["key"])
                if k not in tex_ix:
                    tex_ix[k] = len(textures)
                    textures.append(t)
                return tex_ix[k]
            tex = union(y["mat"][0])
            env = y["mat"][8]
            frames = y["mat"][11]
            if frames is not None:
                frames = (frames[0], tuple(union(t) for t in frames[1]))
            rows.append((tex, y["mat"][1], y["mat"][2], y["mat"][3],
                         0 if env is None else env, frames))
        costumes.append(rows)
    return textures, costumes


def fighter_mobjs(fighter, built, textures, costumes):
    """The animated part MObjs (fighter.h FPackMObjs), or
    None for a fighter with none: every one but Samus, whose grapple beam
    flickers.

    Each MObj's frames are the tiles the bake gave its batches under the
    part "main" (ssb_meshexport.part_main_mobjs), costume 0's, which must
    be every costume's: a run of `textures`, appended where the bake's own
    indices are not already one, since the loader compiles frame k from
    tex_first + k. Returns the six blobs, the MObj count, and the words'
    and relocations' counts."""
    from ssb_stageexport import read_matanim_script

    main = built["main_mobjs"]
    if not main:
        return None
    baker = built["baker"]
    f, reloc = built["model_file"], built["model_reloc"]
    joint_blob = b""
    index = {}
    subs = []
    for j in range(len(baker.nodes)):
        offs, _scripts = main.get(j, ([], []))
        joint_blob += struct.pack("<hh", len(subs), len(offs))
        for mi, o in enumerate(offs):
            index[(j, mi)] = len(subs)
            subs.append((j, mi, A.read_mobjsub(f, reloc, o)))

    batch_mobj = []
    frames = [None] * len(subs)
    for i, b in enumerate(baker.batches):
        k = index.get(b.get("mobj")) if b.get("part") else None
        if k is None or b["node"].index != b["mobj"][0]:
            batch_mobj.append(-1)
            continue
        batch_mobj.append(k)
        rows = [c[i] for c in costumes]
        fr = rows[0][5]
        tiles = (rows[0][0],) if fr is None else fr[1]
        if any((r[5] if fr is not None else (r[0],)) !=
               (fr if fr is not None else (rows[0][0],)) for r in rows):
            raise AssertionError("%s: batch %d's animated MObj tiles differ "
                                 "between costumes" % (fighter, i))
        if -1 in tiles:
            raise AssertionError("%s: batch %d's animated MObj has a hole "
                                 "in its sprite array" % (fighter, i))
        if frames[k] is not None and frames[k] != tiles:
            raise AssertionError("%s: MObj %d draws different tiles in two "
                                 "batches" % (fighter, k))
        frames[k] = tiles

    subs_blob = b""
    appended = 0
    for k, (j, mi, sub) in enumerate(subs):
        tiles = frames[k]
        if tiles is None:
            # hung and played, as the game does, but the display list
            # never loads its tile (Samus's second beam MObj): no frames
            subs_blob += A.pack_mobjsub(sub, -1, 0)
            continue
        if tiles != tuple(range(tiles[0], tiles[0] + len(tiles))):
            first = len(textures)
            textures.extend(textures[t] for t in tiles)
            appended += len(tiles)
        else:
            first = tiles[0]
        subs_blob += A.pack_mobjsub(sub, first, len(tiles))

    words, entries, relocs = [], [], []
    start = {}
    for j in range(len(baker.nodes)):
        for sc in main.get(j, ([], []))[1]:
            if sc is None:
                entries.append(-1)
                continue
            if sc not in start:
                w, fx = read_matanim_script(f, reloc, sc)
                start[sc] = (len(words), w, fx)
                words.extend(w)
            entries.append(start[sc][0])
    for sc, (base, _w, fx) in start.items():
        for (wi, target, _op) in fx:
            if target not in start:
                raise AssertionError("%s: material script 0x%X jumps to "
                                     "0x%X, which is not a part script"
                                     % (fighter, sc, target))
            words[base + wi] = start[target][0]
            relocs.append(base + wi)
    print("mobjs: %d animated part MObjs on joints %s, frames %s, %d "
          "script words, %d textures appended"
          % (len(subs), sorted(main), [len(t) if t else 0 for t in frames],
             len(words),
             appended))
    return (subs_blob, joint_blob,
            struct.pack("<%dh" % len(batch_mobj), *batch_mobj),
            struct.pack("<%di" % len(entries), *entries),
            struct.pack("<%dI" % len(words), *words),
            struct.pack("<%dI" % len(relocs), *relocs) if relocs else b"",
            len(subs), len(words), len(relocs))


def anm_tier_rules(path=ANM_TIERS_PATH):
    """anm_tiers.tsv as (tier, pack, table, motion id) rows: `pack` is a
    pack's name as FPackHeader.name holds it (seven characters) or "*"
    for every pack, `table` is "main" or "sub", and the motion id a row
    of that table or None for all of them."""
    rules = []
    for ln, line in enumerate(open(path), 1):
        line = line.split("#", 1)[0].split()
        if not line:
            continue
        if len(line) != 4 or line[2] not in ("main", "sub") or \
                not 0 <= int(line[0]) < ANM_TIERS:
            raise ValueError("%s:%d: want <tier> <pack> main|sub <motion id>"
                             % (path, ln))
        rules.append((int(line[0]), line[1], line[2],
                      None if line[3] == "*" else int(line[3])))
    return rules


def anm_tiers(fighter, motion, submotion, count):
    """Each kept animation's tier in the .anm: the lowest tier a rule
    (anm_tier_rules) puts a motion row that plays it in, or ANM_TIERS,
    the rest of the file, when none does."""
    tiers = [ANM_TIERS] * count
    for tier, pack, table, mid in anm_tier_rules():
        if pack != "*" and pack != fighter[:7]:
            continue
        rows = submotion if table == "sub" else motion
        for i in (range(len(rows)) if mid is None else
                  [mid] if mid < len(rows) else []):
            ai = rows[i][0]
            if ai >= 0:
                tiers[ai] = min(tiers[ai], tier)
    return tiers


def pack_fighter(rom, fighter, tree):
    built = M.build_fighter(rom, fighter, tree)
    baker, setup = built["baker"], built["setup"]
    verts, tris, joints = built["verts"], built["tris"], built["joints"]
    lo, hi = built["lo"], built["hi"]
    textures, costumes = fighter_costumes(rom, fighter, tree, built)
    print("costumes: %d, %d textures over them all (%d for costume 0)"
          % (len(costumes), len(textures), len(baker.textures)))

    if len(verts) > VCLIP_MAX:
        raise AssertionError("%s: %d vertices; src/dc/ftmanager.c cuts its "
                             "clip pools to FTMANAGER_VCLIP_MAX = %d"
                             % (fighter, len(verts), VCLIP_MAX))
    world = baker.world_verts()
    cx, cy, cz = ((lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2,
                  (lo[2] + hi[2]) / 2)
    import math
    radius = max(math.sqrt((p[0] - cx) ** 2 + (p[1] - cy) ** 2 +
                           (p[2] - cz) ** 2) for p in world)

    mobjsec = fighter_mobjs(fighter, built, textures, costumes)

    joint_of = {}
    for j in joints:
        for b in range(j["batch_first"], j["batch_first"] + j["batch_count"]):
            joint_of[b] = j["node"].index

    secs = model_sections(
        [(n.parent.index, n.translate, n.rotate, n.scale)
         for n in baker.nodes],
        verts, tris,
        [(b["tri_first"], b["tri_count"], b["mat"], joint_of[i],
          b.get("bucket", 0), b.get("part", 0))
         for i, b in enumerate(baker.batches)],
        textures, baker.palettes)

    # -- animations: directory + per-anim entry tables + shared word pool ---
    anims = fighter_anims(fighter, fighter_motion_rows(rom, fighter)
                          + fighter_submotion_rows(rom, fighter))
    njoints = len(baker.nodes)
    entries_blob = b""
    words_blob = b""
    dir_blob = b""
    # each animation's figatree table raw (read_anim_ex "slots"), for the
    # fighter's tree walk to deal out at attach time; `entries` above is
    # the same table dealt to the skeleton alone, which the pose player
    # and every model without hidden parts still read
    slots_blob = b""
    slot_dirs = []
    max_slots = 0
    kept = 0
    animjoint = 0
    kept_names = []
    anim_words = []
    anim_dirs = []
    for name, fid in anims:
        # validate: every script is walked once before it is shipped, so a
        # pack never carries a word the game's parsers would spin on
        try:
            anim = M.read_anim_ex(rom, fid, ssb_extract, njoints, setup,
                                  validate=True, native_splines=True)
        except ValueError as e:
            # A pack may drop an animation the game cannot reach, and
            # only that one. The ROM keeps files nothing names -- every
            # fighter has some, and Master Hand has four --
            # and two of his, FTBossAnimUnknown3 and FTBossAnimUnknown5,
            # are the only files in the whole game whose scripts do not
            # parse. Dropping those costs nothing.
            #
            # Dropping one the game DOES name costs a fighter a motion
            # with no error and no picture, which is the failure this
            # turns into a build break. Across every pack the game ships
            # this arm is reached exactly twice, and both times here.
            if name in named_anims():
                raise AssertionError(
                    "%s: %s will not parse (%s), and the game names it"
                    % (fighter, name, e))
            print("  skipping %s: %s (named by nothing in the decomp)"
                  % (name, e))
            continue
        kept_names.append(name)
        nm = name.encode()[:ANIM_NAME_LEN - 1]
        words = anim["words"]
        off_e = len(entries_blob)
        entries_blob += struct.pack("<%di" % njoints, *anim["entries"])
        off_r = len(entries_blob)
        entries_blob += struct.pack("<%dI" % len(anim["relocs"]),
                                    *anim["relocs"])
        # the words' place in the .anm is decided below, tier by tier
        anim_words.append(struct.pack("<%d%s" % (len(words),
                                                 "I" if anim["kind"] else "H"),
                                      *words))
        anim_dirs.append((nm, len(words), off_e, anim["kind"], off_r,
                          len(anim["relocs"])))
        if len(anim["slots"]) > FTPARTS_JOINT_NUM_MAX:
            raise AssertionError("%s: %s has %d slots, the game's joints "
                                 "array %d" % (fighter, name,
                                               len(anim["slots"]),
                                               FTPARTS_JOINT_NUM_MAX))
        slot_dirs.append((len(slots_blob), len(anim["slots"])))
        slots_blob += struct.pack("<%di" % len(anim["slots"]), *anim["slots"])
        max_slots = max(max_slots, len(anim["slots"]))
        kept += 1
        animjoint += anim["kind"]
    dir_size = kept * ANIM_DIR_ENTRY

    # before the attribute blob: its last two words are this table's
    # offset and length (FPackAttr.off_submotion)
    subscript_words, subscript_sites, subscript_addrs = \
        fighter_submotion_scripts(rom, fighter)
    submotion = fighter_submotion_table(rom, fighter, anims, kept_names,
                                        subscript_addrs)
    motion = fighter_motion_table(rom, fighter, anims, kept_names)

    # The .anm's words, tier by tier (fighter.h FPackAttr.anm_tier_end):
    # every animation starts 4-aligned in the pool -- an AnimJoint's
    # words are u32 and the target reads them in place -- and a tier's
    # animations all end before the tier does.
    tiers = anm_tiers(fighter, motion, submotion, len(anim_words))
    anim_offs = [0] * len(anim_words)
    tier_end = []
    for t in range(ANM_TIERS + 1):
        for i in range(len(anim_words)):
            if tiers[i] == t:
                words_blob += b"\0" * (-len(words_blob) % 4)
                anim_offs[i] = len(words_blob)
                words_blob += anim_words[i]
        words_blob += b"\0" * (-len(words_blob) % 4)
        tier_end.append(len(words_blob))
    for i, (nm, n, off_e, kind, off_r, nr) in enumerate(anim_dirs):
        dir_blob += struct.pack("<%dsIIIIII" % ANIM_NAME_LEN, nm,
                                anim_offs[i], n, off_e, kind, off_r, nr)
    print("anm tiers: %s of %d bytes"
          % (", ".join("%d" % e for e in tier_end[:ANM_TIERS]),
             tier_end[ANM_TIERS]))

    attr = fighter_attributes(rom, fighter)
    attr_blob = struct.pack("<%df" % len(ATTR_FIELDS),
                            *[attr[n] for _, n in ATTR_FIELDS])
    attr_blob += struct.pack("<i", attr["jumps_max"])
    attr_blob += struct.pack("<%di" % ATTR_EFFECT_JOINT_NUM,
                             *attr["effect_joint_ids"])
    attr_blob += struct.pack("<%di" % ATTR_CLIFF_GA_NUM,
                             *attr["cliff_status_ga"])
    # the hit-detection half, in FPackAttr order (src/dc/fighter.h)
    attr_blob += struct.pack("<iI3f2I", attr["is_metallic"], attr["is_have"],
                             *attr["hit_detect_range"], *attr["animlock"])
    for r in attr["damage_coll_descs"]:
        attr_blob += struct.pack("<3i6f", *r)
    # the joint tables, FPackAttr order: setup_parts, the
    # hidden-part row count and rows, then off_slots -- patched in below
    # once the sections are placed
    attr_blob += struct.pack("<2I", *attr["setup_parts"])
    attr_blob += struct.pack("<i", len(attr["hiddenparts"]))
    for r in attr["hiddenparts"] + \
            [(0, 0, 0, 0)] * (ATTR_HIDDENPART_MAX - len(attr["hiddenparts"])):
        attr_blob += struct.pack("<4i", *r)
    # thrown_status: FPackAttr order, right after
    # hiddenparts and before off_slots/off_shield
    for row in attr["thrown_status"]:
        for pair in row:
            attr_blob += struct.pack("<2i", *pair)
    # the item half: the pickup rectangle, the
    # two thrown-item percentages plus the heavy-lift grunt, and the two
    # hand joints -- FPackAttr order, after thrown_status and before
    # off_slots/off_shield
    attr_blob += struct.pack("<8f", *attr["item_pickup"])
    attr_blob += struct.pack("<3H", attr["itemthrow_scales"][0],
                             attr["itemthrow_scales"][1],
                             attr["heavyget_sfx"])
    # the voice half, FPackAttr order: dead_fgm_ids[2], deadup_sfx,
    # damage_sfx, smash_sfx[3] -- ten u16s with the three above, so the
    # hand joints stay 4-aligned with no pad
    attr_blob += struct.pack("<7H", *attr["voice_sfx"])
    attr_blob += struct.pack("<2i", attr["joint_itemheavy_id"],
                             attr["joint_itemlight_id"])
    # the slope contour, FPackAttr order: rfoot id/rotate,
    # lfoot id/rotate, unk_0x31C, unk_0x320
    attr_blob += struct.pack("<ififff", *attr["slope_contour"])
    # translate_scales: the flag, then one row per joint
    # of the game's joints array, identity past the file's own rows
    ts = attr["translate_scales"]
    attr_blob += struct.pack("<i", 0 if ts is None else 1)
    for i in range(FTPARTS_JOINT_NUM_MAX):
        row = ts[i] if (ts is not None and i < len(ts)) else (1.0, 1.0, 1.0)
        attr_blob += struct.pack("<3f", *row)
    # shade_color[3] and fog_color, before off_slots
    attr_blob += bytes(attr["shade_fog_colors"])
    attr_off_slots_pos = len(attr_blob)
    attr_off_shield_pos = attr_off_slots_pos + 4
    attr_off_parts_pos = attr_off_slots_pos + 8
    attr_off_costumes_pos = attr_off_slots_pos + 12
    attr_blob = bytearray(attr_blob + struct.pack("<IIII", 0, 0, 0, 0))
    # the costumes: FPackCostumeMat per costume per batch
    costume_mats = b"".join(struct.pack("<hH4I", tex, 0, prim, l1, l2, env)
                            for rows in costumes
                            for (tex, prim, l1, l2, env, _fr) in rows)
    # the texture parts: the game's container bytes, each
    # part's frame count, and per batch drawn under a part's MObj its
    # tiles, costume-major (FPackTexParts)
    attr_off_texparts_pos = attr_off_slots_pos + 16
    tp = built["textureparts"]
    attr_blob += struct.pack("<I6s2B", 0,
                             tp if tp is not None else b"\0" * 6,
                             0 if tp is None else 1, 0)
    # fttypes.h:966 accesspart: an FTAccessPart whose
    # joint_id is the ROM's and whose three pointers are NULL -- the pack
    # carries the accessory itself as a part -- in 32 bytes, enough for
    # the struct at either pointer width, and the flag
    acc = built["accesspart"]
    attr_blob += struct.pack("<i28xI", acc["joint_id"] if acc else 0,
                             1 if acc else 0)
    # fttypes.h:972 skeleton: the joint word 0 names and a
    # mask of the skeleton ids with tags (FPackAttr.skeleton_joint/ids)
    sk = built["skeleton"]
    attr_blob += struct.pack("<iI", sk["joint"] if sk else 0,
                             sum(1 << sid for sid in (sk["tables"] if sk
                                                      else {})))
    # the demo statuses' motion table. It lives in the
    # ATTRIBUTE block, not the header: FPackHeader is exactly 128 bytes
    # with nothing spare and is written by eleven exporters, while
    # FPackAttr is the fighter-only block that already carries
    # off_shield, off_parts, off_costumes and off_texparts -- and a
    # SubMotion table is a fighter-only thing. The offset is patched in
    # below once the sections are placed, as off_shield's is.
    attr_off_submotion_pos = len(attr_blob)
    attr_blob += struct.pack("<2I", 0, len(submotion))
    # The animations' words are a file of their own, <name>.anm
    # (fighter.h FPackAnm): its id and length, so the loader can tell a
    # pair exported together from two that were not.
    anm_id = zlib.crc32(words_blob)
    attr_blob += struct.pack("<2I", anm_id, len(words_blob))
    attr_blob += struct.pack("<%dI" % ANM_TIERS, *tier_end[:ANM_TIERS])
    tp_frames = built["texture_part_frames"]
    texpart_batches = b""
    texpart_frames = []
    for i in range(len(baker.batches)):
        fr = costumes[0][i][5]
        # an animated part MObj's frames are its FPackMObjSub's, below
        if fr is None or fr[0] == "main":
            texpart_batches += struct.pack("<BBH", 0xFF, 0, 0)
            continue
        part, tiles = fr
        if len(tiles) != tp_frames[part]:
            raise AssertionError("%s: batch %d has %d frames of texture part "
                                 "%d, which has %d" % (fighter, i, len(tiles),
                                                       part, tp_frames[part]))
        texpart_batches += struct.pack("<BBH", part, 0, len(texpart_frames))
        for rows in costumes:
            if rows[i][5] is None or rows[i][5][0] != part:
                raise AssertionError("%s: batch %d's texture part differs "
                                     "between costumes" % (fighter, i))
            texpart_frames.extend(rows[i][5][1])
    if len(texpart_frames) > 0xFFFF:
        raise AssertionError("%s: %d texture part frames"
                             % (fighter, len(texpart_frames)))
    texpart_frames_blob = struct.pack("<%dh" % len(texpart_frames),
                                      *texpart_frames)
    print("textureparts: frames %s, %d batches, %d frame tiles"
          % (tp_frames, sum(1 for r in costumes[0]
                            if r[5] is not None and r[5][0] != "main"),
             len(texpart_frames)))
    # the model parts: FTAttributes.modelparts_container
    # as tags into the baked batches, FPackParts in src/dc/fighter.h
    part_table = built["part_table"]
    tag_count = len(baker.part_verts)
    if sorted(baker.part_verts) != list(range(1, tag_count + 1)):
        raise AssertionError("%s: part tags %r are not 1..%d"
                             % (fighter, sorted(baker.part_verts), tag_count))
    # the runtime's transform walks the tags as vertex runs in order
    runs = sorted(baker.part_verts.values())
    if runs != [baker.part_verts[t] for t in range(1, tag_count + 1)] or \
            any(a[0] + a[1] > b[0] for a, b in zip(runs, runs[1:])):
        raise AssertionError("%s: part vertex runs %r are out of tag order "
                             "or overlap" % (fighter, baker.part_verts))
    tag_joint = {}
    if built["accessory_tag"]:
        tag_joint[built["accessory_tag"]] = built["accesspart"]["joint_id"] - 4
    for tag, (_sid, i) in built["skeleton_tags"].items():
        tag_joint[tag] = i
    for i, (_tree, ids) in part_table.items():
        for tag, _fl in ids:
            if tag != M.PART_TAG_NONE and \
                    tag_joint.setdefault(tag, i) != i:
                raise AssertionError("%s: part tag %d on joints %d and %d"
                                     % (fighter, tag, tag_joint[tag], i))
    # FPackPartTag: its vertex run, its joint, and which skeleton id it
    # draws for, 0 for a model part or the accessory
    parts_tags = b"".join(struct.pack("<2H2B2x", *baker.part_verts[t],
                                      tag_joint[t],
                                      built["skeleton_tags"].get(t, (0,))[0])
                          for t in range(1, tag_count + 1))
    parts_joints = b""
    parts_ids = b""
    id_count = 0
    for i in range(njoints):
        tree, ids = part_table.get(i, (0, []))
        parts_joints += struct.pack("<H2B", id_count, len(ids), tree)
        for tag, fl in ids:
            parts_ids += struct.pack("<2B", tag, fl)
        id_count += len(ids)
    for t, (v0, vn) in baker.part_verts.items():
        if v0 + vn > len(verts) or v0 > 0xFFFF or vn > 0xFFFF:
            raise AssertionError("%s: part %d's vertices %d+%d" %
                                 (fighter, t, v0, vn))
    # the shield pose: FTAttributes.dobj_lookup and
    # shield_anim_joints out of the ShieldPose file, as the guard reads
    # them; FPackShieldPose in src/dc/fighter.h
    shield = M.read_shieldpose(rom, fighter, ssb_extract, attr["attr_base"],
                               sum(1 for i in setup if i < njoints))
    shield_words_blob = struct.pack("<%dI" % len(shield["words"]),
                                    *shield["words"])
    shield_reloc_blob = struct.pack("<%dI" % len(shield["relocs"]),
                                    *shield["relocs"])
    script_words, script_sites, script_extern, script_sub_base = \
        fighter_scripts(rom, fighter)
    # The SubMotion rows' scripts ride in the same blob as the MainMotion
    # file(s), so one off_script/off_script_reloc pair and one loop in
    # src/dc/fighter.c serve both. They go in FRONT of it:
    # fighter_scripts puts its copy of FTCommonMoveset immediately after
    # the first MainMotion file, and appending would have left it neither
    # first nor last. In front, the blob reads
    # [SubMotion scripts][MainMotion][FTCommonMoveset], so the copy is
    # still the last 524 words of a one-file kind's blob, which is what
    # the extern-site test reads it as. Everything the MainMotion half
    # computed -- its rows' offsets, its pointer words and its sites --
    # moves by the one base.
    script_main_base = 4 * len(subscript_words)
    script_site_set = set(script_sites)
    script_words = subscript_words + [
        w + script_main_base if i in script_site_set else w
        for i, w in enumerate(script_words)]
    script_sites = sorted(subscript_sites +
                          [s + script_main_base // 4 for s in script_sites])
    script_blob = struct.pack("<%dI" % len(script_words), *script_words)
    script_reloc_blob = struct.pack("<%dI" % len(script_sites), *script_sites)
    print("attr: gravity %.2f jump base %.1f walk x%.2f, %d hurtboxes, "
          "have 0x%08X; motion table "
          "%d rows (%d scripted); submotion table %d rows (%d posed, "
          "%d scripted in %d words); scripts %d words, %d pointers, "
          "%d extern into FTCommonMoveset%s"
          % (attr["gravity"], attr["jump_height_base"],
             attr["walk_speed_mul"],
             sum(1 for r in attr["damage_coll_descs"] if r[0] != -1),
             attr["is_have"], len(motion),
             sum(1 for _, sc, _f in motion if sc >= 0),
             len(submotion), sum(1 for ai, _s, _f in submotion if ai >= 0),
             sum(1 for _a, sc, _f in submotion if sc >= 0),
             len(subscript_words),
             len(script_words), len(script_sites), script_extern,
             ("; submotion rows lay onto +0x%X" % script_sub_base)
             if script_sub_base else ""))
    print("joints: setup_parts %08X %08X (%d of %d entries), %d hidden-part "
          "rows (roots %s); %d slot tables, the widest %d slots"
          % (attr["setup_parts"][0], attr["setup_parts"][1],
             sum(1 for i in setup if i < njoints), njoints,
             len(attr["hiddenparts"]),
             [r[0] for r in attr["hiddenparts"]], kept, max_slots))
    print("shield: size %.0f; file %d, %d DObjDesc rows, %d tables of %d, "
          "%d words, %d pointers"
          % (attr["shield_size"], shield["file_id"], shield["lookup_count"],
             len(shield["tables"]), shield["table_count"],
             len(shield["words"]), len(shield["relocs"])))

    # -- assemble: header, then sections, each 4-aligned --------------------
    def align(b):
        return b + b"\0" * (-len(b) % 4)

    header_size = 128
    off = header_size
    offsets = {}
    sections = [("joints", secs["joints"]), ("verts", secs["verts"]),
                ("tris", secs["tris"]), ("batches", secs["batches"]),
                ("texs", secs["texs"]), ("pals", secs["pals"]),
                ("attr", attr_blob), ("motion", None), ("submotion", None),
                ("script", script_blob), ("script_reloc", script_reloc_blob),
                ("anims", dir_blob), ("entries", entries_blob),
                ("slotdir", None), ("slots", slots_blob),
                ("shield_words", shield_words_blob),
                ("shield_reloc", shield_reloc_blob), ("shield", None),
                ("parts", None), ("parts_tags", parts_tags),
                ("parts_joints", parts_joints), ("parts_ids", parts_ids),
                ("costumes", None), ("costume_mats", costume_mats),
                ("texparts", None), ("texpart_batches", texpart_batches),
                ("texpart_frames", texpart_frames_blob)]
    if mobjsec is not None:
        sections += [("mobjs", None)] + list(zip(
            ("mobj_subs", "mobj_joint", "mobj_batch", "mobj_entry",
             "mobj_words", "mobj_reloc"), mobjsec[:6]))
    # The texels last: once they are in VRAM the loader cuts them off the
    # pack's buffer (src/dc/fighter.c fighter_drop_texels), which it can
    # do only to the tail.
    sections += [("texdata", secs["texdata"])]
    # sections whose bytes hold pack offsets are made once those are known
    late_size = {"motion": 12 * len(motion),     # sizeof(FPackMotion)
                 "submotion": 12 * len(submotion),
                 "slotdir": 8 * kept,            # sizeof(FPackAnimSlots)
                 "shield": 4 * 15,               # sizeof(FPackShieldPose)
                 "parts": 4 * 6,                 # sizeof(FPackParts)
                 "costumes": 4 * 2,              # sizeof(FPackCostumes)
                 "texparts": 4 * 4,              # sizeof(FPackTexParts)
                 "mobjs": 4 * 12}                # sizeof(FPackMObjs)
    for key, sec in sections:
        offsets[key] = off
        off += len(align(sec)) if sec is not None else late_size[key]
    # the motion rows point at their scripts by pack offset
    late = {}
    # FTANIM_FLAG_SUBMOTION_SCRIPT (0x10) picks the second MainMotion
    # file, which is what ft/ftmain.c:4747-4762 does with the pair of
    # p_file_* pointers; here it picks the base inside the one blob.
    late["motion"] = b"".join(
        struct.pack("<hhII", ai, 0, fl,
                    offsets["script"] + script_main_base
                    + (script_sub_base if fl & 0x10 else 0)
                    + sc if sc >= 0 else 0)
        for ai, sc, fl in motion)
    # the demo statuses' rows, whose scripts open the same blob, so their
    # offsets need no base at all.
    # FTANIM_FLAG_SUBMOTION_SCRIPT does NOT pick one here either: that
    # flag names the second MainMotion file, which is the MainMotion
    # table's business.
    late["submotion"] = b"".join(
        struct.pack("<hhII", ai, 0, fl,
                    offsets["script"] + sc if sc >= 0 else 0)
        for ai, sc, fl in submotion)
    struct.pack_into("<I", attr_blob, attr_off_submotion_pos,
                     offsets["submotion"])
    # each animation's slot table by pack offset, and the attributes'
    # pointer at the directory
    late["slotdir"] = b"".join(struct.pack("<II", offsets["slots"] + o, n)
                               for o, n in slot_dirs)
    struct.pack_into("<I", attr_blob, attr_off_slots_pos, offsets["slotdir"])
    # the shield pose's header: its words and pointer list by pack offset,
    # its rows and tables by word index (src/dc/fighter.h FPackShieldPose)
    late["shield"] = struct.pack("<4I2I8II", offsets["shield_words"],
                                 len(shield["words"]),
                                 offsets["shield_reloc"],
                                 len(shield["relocs"]),
                                 shield["lookup"], shield["lookup_count"],
                                 *shield["tables"], shield["table_count"])
    struct.pack_into("<I", attr_blob, attr_off_shield_pos, offsets["shield"])
    late["parts"] = struct.pack("<6I", tag_count, id_count,
                                offsets["parts_tags"], offsets["parts_joints"],
                                offsets["parts_ids"], built["accessory_tag"])
    struct.pack_into("<I", attr_blob, attr_off_parts_pos, offsets["parts"])
    late["costumes"] = struct.pack("<2I", len(costumes),
                                   offsets["costume_mats"])
    struct.pack_into("<I", attr_blob, attr_off_costumes_pos,
                     offsets["costumes"])
    late["texparts"] = struct.pack(
        "<2BH3I", tp_frames.get(0, 0), tp_frames.get(1, 0), 0,
        len(texpart_frames),
        offsets["texpart_batches"], offsets["texpart_frames"])
    struct.pack_into("<I", attr_blob, attr_off_texparts_pos,
                     offsets["texparts"] if tp is not None else 0)
    if mobjsec is not None:
        late["mobjs"] = struct.pack(
            "<12I", mobjsec[6], offsets["mobj_subs"], offsets["mobj_joint"],
            offsets["mobj_batch"], offsets["mobj_entry"],
            offsets["mobj_words"], mobjsec[7], offsets["mobj_reloc"],
            mobjsec[8], 1, 0, 0)
    for key in late:
        assert len(late[key]) == late_size[key], key
    off = header_size
    body = b""
    for key, sec in sections:
        sec = align(sec if sec is not None else late[key])
        assert offsets[key] == off
        body += sec
        off += len(sec)

    # The directory's word/entry offsets were relative to their own pools;
    # rebase them now the pools' positions are known.
    dirs = bytearray(body[offsets["anims"] - header_size:
                          offsets["anims"] - header_size + dir_size])
    for i in range(kept):
        base = i * ANIM_DIR_ENTRY
        w, n, e, k, r, nr = struct.unpack_from("<6I", dirs,
                                               base + ANIM_NAME_LEN)
        struct.pack_into("<6I", dirs, base + ANIM_NAME_LEN,
                         w + ANM_HEADER_SIZE, n, e + offsets["entries"],
                         k, r + offsets["entries"], nr)
    body = (body[:offsets["anims"] - header_size] + bytes(dirs) +
            body[offsets["anims"] - header_size + dir_size:])

    header = struct.pack("<8s8I8I3ff8s8I", MAGIC,
                         len(baker.nodes), len(verts), len(tris),
                         len(baker.batches), len(textures),
                         len(baker.palettes), kept, len(secs["texdata"]),
                         offsets["joints"], offsets["verts"],
                         offsets["tris"], offsets["batches"],
                         offsets["texs"], offsets["pals"],
                         offsets["texdata"], offsets["anims"],
                         # seven and a NUL, not eight: the field is a C
                         # string to every syDebugPrintf("%s", hd->name)
                         # in src/dc/fighter.c, and a name that fills it
                         # runs on into the next field -- "NPikachu" came
                         # off the disc as "NPikachu\x0d".
                         cx, cy, cz, radius, fighter.encode()[:7],
                         offsets["attr"], offsets["motion"], len(motion),
                         offsets["script"], len(script_words),
                         offsets["script_reloc"], len(script_sites),
                         offsets["mobjs"] if mobjsec is not None else 0)
    assert len(header) == header_size, len(header)
    print("pack: %d anims kept (%d AnimJoint), %d bytes textures, "
          "%d bytes animation"
          % (kept, animjoint, len(secs["texdata"]),
             len(entries_blob) + len(words_blob) + dir_size
             + len(slots_blob) + late_size["slotdir"]))
    anm = struct.pack("<8s2I", ANM_MAGIC, anm_id, len(words_blob))
    assert len(anm) == ANM_HEADER_SIZE
    return header + body, anm + bytes(words_blob)


def main():
    argv = sys.argv[1:]
    rom_path = M.ROM_DEFAULT
    fighter = M.FIGHTER_DEFAULT
    tree = "high"
    out_path = None
    if "--rom" in argv:
        rom_path = argv[argv.index("--rom") + 1]
    if "--fighter" in argv:
        fighter = argv[argv.index("--fighter") + 1]
    if "--tree" in argv:
        tree = argv[argv.index("--tree") + 1]
    if "--out" in argv:
        out_path = argv[argv.index("--out") + 1]
    if out_path is None:
        sys.exit("--out <file.pack> is required")

    rom = open(rom_path, "rb").read()
    blob, anm = pack_fighter(rom, fighter, tree)
    d = os.path.dirname(out_path)
    if d:
        os.makedirs(d, exist_ok=True)
    # the pack first: make's rule for the .anm is satisfied by a file no
    # older than the pack it came with (src/game/ssb64/Makefile)
    with open(out_path, "wb") as fp:
        fp.write(blob)
    anm_path = os.path.splitext(out_path)[0] + ".anm"
    with open(anm_path, "wb") as fp:
        fp.write(anm)
    print("wrote %s (%d bytes) and %s (%d bytes)"
          % (out_path, len(blob), anm_path, len(anm)))


if __name__ == "__main__":
    main()

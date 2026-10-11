#!/usr/bin/env python3
"""ssb64-dc: where the game's joint ids point, fighter by fighter.

FTStruct.joints[] is indexed by the ids a fighter's own data names --
hurtboxes, effect joints, the feet, hidden parts, and the joint a motion
script hangs a hitbox on -- and ft/ftmanager.c fills it from the model's
DObjDesc array through lbCommonSetupFighterPartsDObjs with the base
&fp->joints[nFTPartsJointCommonStart]: DObjDesc entry k is joints[4 + k].
The ROM says the same in its own words (ftManagerMakeFighter passes
fp + 2296, which is joints + 16, and the setup writes forward from there),
and so does every table below: under that base Mario's 103x112x95 hurtbox
sits on the torso and his effect joints are the head, the forearms and the
shins; under the old base of joints[1 + k] the same ids land on
forearms and hands, and three of the roster's ids land on entries the
game never instantiates, which the real game could not survive.

Slots 1 to 3 -- TransN, XRotN, YRotN -- are not entries of the array at
all. They are hidden parts (FTAttributes.hiddenparts, ft/ftmain.c
ftMainUpdateHiddenPartID), DObjs the game makes and links above the hip
when an animation's FTAnimDesc bit asks for one, and ejects when the next
does not; the figatree walk from TopN's child then visits them first, so
a flagged animation's leading tracks are theirs, not the skeleton's.

This is the check that would have caught the old base. For every
fighter it reads the tree, the setup_parts mask, the hidden-part rows,
the hurtbox and effect tables and the motion flags out of the decomp's
own sources and the ROM, asserts that every id resolves under the game's
base, and reports what the old base -- joints[1 + k], which the port
once used -- made of the same ids and how many animations carry each
hidden-part bit: the numbers the rework was sized by, kept so the
regression has a name.

Then the pack: each fighter's pack is built here, as the
romdisk builds it, and read back the way src/dc/fighter.c reads it. Its
FPackAttr must carry the setup mask and the hidden-part rows the source
declares, and every animation's raw slot table must be the figatree's
own header, entry for entry, read independently off the ROM -- with
FPackAnim's per-joint entries being that table dealt to the instantiated
entries in order, which is what the pose player still reads. That is the
data the rework's tree walk will deal out; this is where it is held to
the ROM before any code reads it.

Usage: python3 tools/check/joint_check.py [--rom <rom.z64>] [--fighter <Name>]
                                    [--no-pack]
"""
import argparse
import contextlib
import io
import os
import re
import struct
import sys
sys.path[:0] = [os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), d)
                for d in ("lib", "export", "check")]
import ssb_extract               # noqa: E402
import ssb_assets as A           # noqa: E402
import ssb_meshexport as M       # noqa: E402
import ssb_packexport as P       # noqa: E402

FIGHTERS = ["Mario", "Fox", "Donkey", "Samus", "Luigi", "Link",
            "Yoshi", "Captain", "Kirby", "Pikachu", "Purin", "Ness"]
TOPN, TRANSN, XROTN, YROTN, COMMON = 0, 1, 2, 3, 4   # ft/ftdef.h:1071
SLOT_NAMES = {0: "TopN", 1: "TransN", 2: "XRotN", 3: "YRotN"}
# FTAnimDesc (ft/fttypes.h:48): bit 31 XRotN, bit 30 TransN, bit 29 YRotN,
# then 24 bits of further hidden parts; the low five are not joints.
ANIM_LOW_FLAGS = 0x1F


def fighter_tree(rom, fighter):
    """(nodes, instantiated entry set) of the high-detail tree, the way
    ssb_meshexport.build_fighter finds them."""
    model_id, model_path = M.find_reloc_source(fighter + "Model")
    _, main_path = M.find_reloc_source(fighter + "Main")
    model_src = M.read_source(model_path)
    main_src = M.read_source(main_path)
    setup = M.read_setup_parts(main_src)
    (dobj, _mobj, _post), part_flags = M.find_commonparts(main_src)[0]
    tree_off = M.symbol_offset(model_src, dobj[0]) + dobj[1]
    f = A.get_file(rom, model_id, ssb_extract)
    entry = ssb_extract.read_entry(rom, ssb_extract.RELOC_SEG, model_id)
    reloc = A.walk_reloc(f, entry["reloc_intern"])
    _root, nodes = A.read_dobj_tree(f, reloc, tree_off,
                                    dls_pair=(part_flags & 0xF) == 1)
    return nodes, set(i for i in setup if i < len(nodes)), main_src


def ints_in(block):
    return [int(x) for x in re.findall(r"-?\b\d+\b", block)]


def read_tables(main_src, fighter):
    """Hidden-part rows, hurtbox joint ids, effect joint ids and the two
    foot ids, out of d<F>Main_attr and d<F>Main_hiddenparts."""
    rows = M.read_hiddenparts(main_src, fighter)
    m = re.search(r"/\* damage_coll_descs \*/\s*\{(.*?)\n\t\},", main_src, re.S)
    if not m:
        raise ValueError("%s: no damage_coll_descs" % fighter)
    hurt = []
    for r in re.findall(r"\{\s*(-?\d+),\s*(\d+),\s*(TRUE|FALSE)", m.group(1)):
        if int(r[0]) == -1:
            break
        hurt.append(int(r[0]))
    m = re.search(r"\{([^{}]*)\},\s*/\* effect_joint_ids \*/", main_src)
    if not m:
        raise ValueError("%s: no effect_joint_ids" % fighter)
    effect = ints_in(m.group(1))
    feet = [int(re.search(r"(\d+),\s*/\* joint_%sfoot_id \*/" % s,
                          main_src).group(1)) for s in ("r", "l")]
    return rows, hurt, effect, feet


def resolve(joint_id, base, nodes, live, hidden_roots):
    """What a joint id is under a base: ('slot', name) for the four fixed
    slots, ('entry', k, ok) for the array, ('past', k) beyond it."""
    if joint_id < COMMON:
        return ("slot", SLOT_NAMES[joint_id])
    k = joint_id - base
    if k < 0 or k >= len(nodes):
        return ("past", k)
    return ("entry", k, k in live or joint_id in hidden_roots)


# src/dc/fighter.h, as struct formats: the header, the FPackAttr prefix the
# hit and physics halves take (everything before setup_parts), the
# animation directory entry and the slot directory entry.
HEADER_FMT = "<8s8I8I3ff8s8I"
ATTR_PREFIX = (4 * len(P.ATTR_FIELDS) + 4 + 4 * P.ATTR_EFFECT_JOINT_NUM
               + 4 * P.ATTR_CLIFF_GA_NUM
               + 4 + 4 + 12 + 8 + 36 * P.ATTR_DAMAGE_COLL_NUM)
# setup_parts, hiddenpart_count, the hidden rows, thrown_status, then the
# item half (item_pickup's eight floats, the two throw percentages,
# heavyget_sfx and the seven voice ids, the two hand joints), the slope
# contour's feet, translate_scales, and finally
# the five section offsets, the first two of which this reader is after,
# the texture part container's eight bytes and the accessory's joint and flag.
ATTR_TABLES_FMT = "<2Ii%di%di8f10H2iififffi%df16BIIIII8Bi28xI" % (4 * P.ATTR_HIDDENPART_MAX,
                                              4 * P.ATTR_THROWN_STATUS_NUM,
                                              3 * P.FTPARTS_JOINT_NUM_MAX)
SHIELD_FMT = "<4I2I8II"        # FPackShieldPose (src/dc/fighter.h)
ANIM_FMT = "<%dsIIIIII" % P.ANIM_NAME_LEN
SLOTS_FMT = "<II"


def read_pack(blob):
    """The joint tables out of a fighter pack: (setup words, hidden rows,
    [(name, kind, nwords, entries, slots)] per animation)."""
    hd = struct.unpack_from(HEADER_FMT, blob, 0)
    joint_count, anim_count = hd[1], hd[7]
    off_anims, off_attr = hd[16], hd[22]
    if hd[0] != P.MAGIC or off_attr == 0:
        raise ValueError("not a fighter pack")
    t = struct.unpack_from(ATTR_TABLES_FMT, blob, off_attr + ATTR_PREFIX)
    setup = [t[0], t[1]]
    nrows = t[2]
    rows = [tuple(t[3 + 4 * i:7 + 4 * i]) for i in range(nrows)]
    off_slots, off_shield = t[-15], t[-14]
    anims = []
    for i in range(anim_count):
        (name, off_w, nwords, off_e, kind, _off_r, _nreloc) = \
            struct.unpack_from(ANIM_FMT, blob, off_anims + i * P.ANIM_DIR_ENTRY)
        entries = list(struct.unpack_from("<%di" % joint_count, blob, off_e))
        off_s, count = struct.unpack_from(SLOTS_FMT, blob, off_slots + 8 * i)
        slots = list(struct.unpack_from("<%di" % count, blob, off_s))
        anims.append((name.rstrip(b"\0").decode(), kind, nwords, entries,
                      slots))
    sp = struct.unpack_from(SHIELD_FMT, blob, off_shield)
    shield = {"words": list(struct.unpack_from("<%dI" % sp[1], blob, sp[0])),
              "relocs": list(struct.unpack_from("<%dI" % sp[3], blob, sp[2])),
              "lookup": sp[4], "lookup_count": sp[5],
              "tables": list(sp[6:14]), "table_count": sp[14]}
    return setup, rows, anims, joint_count, shield


def check_pack(rom, fighter, nodes, live, main_src, rows, failures):
    """Build the fighter's pack and hold its joint tables to the ROM.
    Returns (animations checked, animations whose table runs past the
    skeleton)."""
    with contextlib.redirect_stdout(io.StringIO()):
        blob, _anm = P.pack_fighter(rom, fighter, "high")
    setup, prow, anims, joint_count, shield = read_pack(blob)
    if setup != M.read_setup_parts_words(main_src):
        failures.append("%s: pack setup_parts %s, source %s"
                        % (fighter, setup, M.read_setup_parts_words(main_src)))
    if prow != [tuple(r) for r in rows]:
        failures.append("%s: pack hidden-part rows %s, source %s"
                        % (fighter, prow, rows))
    if joint_count != len(nodes):
        failures.append("%s: pack has %d joints, the tree %d entries"
                        % (fighter, joint_count, len(nodes)))
    by_name = {}
    # both motion tables, the way pack_fighter feeds it:
    # the SubMotion table borrows too -- Donkey's names Giant DK's Pose1P,
    # Luigi's Mario's Claps, Purin's Luigi's Unknown
    for name, fid in P.fighter_anims(fighter,
                                     P.fighter_motion_rows(rom, fighter)
                                     + P.fighter_submotion_rows(rom, fighter)):
        by_name[name.encode()[:P.ANIM_NAME_LEN - 1].decode()] = fid
    order = sorted(live)
    wide = 0
    for name, kind, nwords, entries, slots in anims:
        fid = by_name.get(name)
        if fid is None:
            failures.append("%s: pack animation %s is not in the ROM's bank"
                            % (fighter, name))
            continue
        f = A.get_file(rom, fid, ssb_extract)
        entry = ssb_extract.read_entry(rom, ssb_extract.RELOC_SEG, fid)
        reloc = A.walk_reloc(f, entry["reloc_intern"])
        wsize = 4 if kind else 2
        want = [-1 if reloc.get(i * 4) is None else reloc[i * 4] // wsize
                for i in range(A.figatree_slots(reloc))]
        if slots != want:
            failures.append("%s: %s slot table %s, the ROM's %s"
                            % (fighter, name, slots, want))
            continue
        if any(s != -1 and not 0 <= s < nwords for s in slots):
            failures.append("%s: %s names a word past its %d"
                            % (fighter, name, nwords))
        dealt = [-1] * joint_count
        for k, s in enumerate(slots):
            if s != -1 and k < len(order):
                dealt[order[k]] = s
        if entries != dealt:
            failures.append("%s: %s entries %s are not the slots dealt to "
                            "the instantiated entries %s"
                            % (fighter, name, entries, dealt))
        if any(s != -1 for s in slots[len(order):]):
            wide += 1
    # the shield pose: the pack's section against the file
    # read again off the ROM, word for word, and its tables sized to the
    # tree walk the guard makes -- XRotN, the instantiated entries, YRotN
    attr = P.fighter_attributes(rom, fighter)
    want = M.read_shieldpose(rom, fighter, ssb_extract, attr["attr_base"],
                             len(order))
    for key in ("words", "relocs", "lookup", "lookup_count", "tables",
                "table_count"):
        if shield[key] != want[key]:
            failures.append("%s: pack shield pose %s differs from the ROM's"
                            % (fighter, key))
    if shield["table_count"] != len(order) + 2:
        failures.append("%s: shield tables of %d for %d instantiated entries"
                        % (fighter, shield["table_count"], len(order)))
    return len(anims), wide


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--rom", default=M.ROM_DEFAULT)
    ap.add_argument("--fighter", action="append")
    ap.add_argument("--no-pack", action="store_true",
                    help="skip building and reading back the packs")
    args = ap.parse_args()
    rom = open(args.rom, "rb").read()
    fighters = args.fighter or FIGHTERS
    failures = []
    packed = 0
    packed_wide = 0
    port_misses = 0
    port_wrong = 0
    ids_total = 0
    anims_total = {TRANSN: 0, XROTN: 0, YROTN: 0, "more": 0}
    for fighter in fighters:
        nodes, live, main_src = fighter_tree(rom, fighter)
        rows, hurt, effect, feet = read_tables(main_src, fighter)
        hidden_roots = set(r[0] for r in rows)
        # the three fixed hidden parts are the three named slots, in the
        # order the FTAnimDesc bits name them
        named = [r[0] for r in rows[:3]]
        if sorted(named) != [TRANSN, XROTN, YROTN]:
            failures.append("%s: hidden parts 0..2 are roots %s, not TransN/"
                            "XRotN/YRotN" % (fighter, named))
        for i, (root, parent, _idx, kind) in enumerate(rows):
            if root >= COMMON and root - COMMON >= len(nodes):
                failures.append("%s: hidden part %d root %d is past the %d-"
                                "entry tree" % (fighter, i, root, len(nodes)))
            if parent >= COMMON and (parent - COMMON) not in live and \
                    parent not in hidden_roots:
                failures.append("%s: hidden part %d hangs off joint %d, "
                                "which is neither instantiated nor a hidden "
                                "part" % (fighter, i, parent))
        # every id the tables name, under the game's base and the port's
        named_ids = [("hurtbox", j) for j in hurt] + \
                    [("effect", j) for j in effect] + \
                    [("foot", j) for j in feet]
        for what, j in named_ids:
            ids_total += 1
            game = resolve(j, COMMON, nodes, live, hidden_roots)
            if game[0] == "past" or (game[0] == "entry" and not game[2]):
                failures.append("%s: %s joint %d is %s under joints[4 + k]"
                                % (fighter, what, j, game))
            port = resolve(j, TRANSN, nodes, live, hidden_roots)
            if port[0] == "past" or (port[0] == "entry" and not port[2]):
                port_misses += 1
            elif port != game:
                port_wrong += 1
        # the motion table's hidden-part bits
        counts = {TRANSN: 0, XROTN: 0, YROTN: 0, "more": 0}
        bit_of = {31: XROTN, 30: TRANSN, 29: YROTN}
        for fid, _script, flags in P.fighter_motion_rows(rom, fighter):
            if fid == 0:
                continue
            for bit in (31, 30, 29):
                if flags & (1 << bit):
                    counts[bit_of[bit]] += 1
            more = flags & ~ANIM_LOW_FLAGS & 0x1FFFFFFF
            if more:
                counts["more"] += 1
                for i in range(3, 32):
                    if more & (1 << (31 - i)) and i >= len(rows):
                        failures.append("%s: an animation names hidden part "
                                        "%d and the table has %d"
                                        % (fighter, i, len(rows)))
        for k in counts:
            anims_total[k] += counts[k]
        pack_note = ""
        if not args.no_pack:
            n, wide = check_pack(rom, fighter, nodes, live, main_src, rows,
                                 failures)
            packed += n
            packed_wide += wide
            pack_note = "; pack: %d slot tables, %d past the skeleton" \
                        % (n, wide)
        print("%-8s %2d entries, %2d instantiated, %d hidden parts (roots %s); "
              "%d hurtboxes, %d effect joints; animations linking TransN %d, "
              "XRotN %d, YRotN %d, others %d%s"
              % (fighter, len(nodes), len(live), len(rows),
                 [r[0] for r in rows], len(hurt), len(effect),
                 counts[TRANSN], counts[XROTN], counts[YROTN], counts["more"],
                 pack_note))
    if failures:
        for f in failures:
            print("joint_check: FAIL --", f)
        return 1
    if not args.no_pack:
        print("joint_check: %d packed slot tables over %d fighters are the "
              "ROM's figatree headers entry for entry, %d of them running "
              "past the skeleton; every pack's setup mask and hidden-part "
              "rows are the source's, and its shield pose the ROM's file word for "
              "word, eight tables sized to the walk from XRotN"
              % (packed, len(fighters), packed_wide))
    print("joint_check: %d joint ids over %d fighters resolve to an "
          "instantiated entry under joints[4 + k], the game's base and the "
          "port's; under joints[1 + k] %d of them "
          "pointed past the skeleton or at an entry the game never builds "
          "and %d at the wrong bone. %d animations link TransN, %d XRotN, "
          "%d YRotN, %d a further hidden part"
          % (ids_total, len(fighters), port_misses, port_wrong,
             anims_total[TRANSN], anims_total[XROTN], anims_total[YROTN],
             anims_total["more"]))
    return 0


if __name__ == "__main__":
    sys.exit(main())

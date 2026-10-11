#!/usr/bin/env python3
"""ssb64-dc: build-time GBI transpiler for an SSB64 fighter model.

The N64 walks the raw F3DEX_GBI_2 command stream at runtime. This tool
instead transpiles the whole model at BUILD time into a
flat mesh IR -- joint-local floats, unit normals, linear UVs, triangle
indices, material batches, twiddled PAL4BPP textures and ARGB1555 palettes --
so the SH-4 poses one matrix per joint and streams triangles.

Why this is deterministic / robust:
  * The fighter's model file is decompressed with the same vpk0 decoder the
    game uses (ssb_extract.decode_vpk0), so we read the exact bytes it loads.
  * Each display list is disassembled with pygfxd (gfxd_f3dex2, big-endian)
    and cut at its own G_ENDDL, so no guessed command counts.
  * Every pointer resolves by the SSB rule  byte_off = (ptr & 0xFFFF) * 4.
  * The DObjDesc array is replayed as the *tree* lbCommonSetupTreeDObjs
    builds, in gcDrawDObjTree order, with the accumulated joint matrix
    (gSPMatrix G_MTX_MUL) recorded per joint. The RSP transforms a vertex
    *at gsSPVertex time*, by whichever joint matrix is current, which is why
    a joint that loads only slots 3..5 can reference slots 0..5 left behind
    by its parent -- so each vertex is tagged with the joint that owns it and
    kept in that joint's local space, ready to be posed.
  * Materials merge the joint list's own combiner/light/tile state with the
    MObj chain reached through segment 0xE, exactly as gcDrawMObjForDObj
    assembles it; triangles are grouped into batches of constant material.
  * N64 tile addressing (gsSPTexture scale, gsDPSetTileSize origin, per-axis
    mask/mirror/clamp) is evaluated offline into a plain PoT image, so the
    baked UVs are linear and the PVR only has to clamp or repeat.
  * Fighter animation is a Figatree (AObjEvent16) script per joint, in its
    own relocData file. Those scripts are carried through as-is and run on
    the target by figatree.c; sampling them to per-frame poses would cost
    about twenty times the space and cannot express an animation that
    holds for a hundred thousand frames. tools/check/figatree_check.py holds
    figatree.c to the decomp's own interpreter.
  * The result is a skeleton plus one joint-local vertex pool: the target
    composes 25 joint matrices per frame and every triangle index resolves.

A library: build_fighter(rom, fighter, tree) is the entry point, and
tools/export/ssb_packexport.py writes what it returns into a fighter pack. A
fighter is named as the decomp names it (Mario, Kirby, Samus, Link, Yoshi,
Fox, Donkey, Luigi, Captain, Pikachu, Purin, Ness); the model and main
relocData files follow from the decomp's own tables.
"""
import os
import re
import struct
import sys

sys.path[:0] = [os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), d)
                for d in ("lib", "export", "check")]
import ssb_extract           # noqa: E402
import ssb_assets as A       # noqa: E402  (shared decode primitives)
import ssb_paths as P        # noqa: E402  (the decomp, from the build image)

ROM_DEFAULT = ssb_extract.ROM_DEFAULT
RELOC_DIR = P.RELOC_DIR
FTDATA_SOURCE = P.FTDATA_SOURCE

FIGHTER_DEFAULT = "Mario"
# d<F>Main_commonparts_container lists two FTCommonParts: entry 0 is the
# high-detail skeleton (nFTPartsDetailHigh), entry 1 the low-detail one.
TREE_ORDER = ["high", "low"]
# Costume id, i.e. the frame the costume matanim script is evaluated at.
# 0 is the default costume.
COSTUME = 0
# A model part with no display list (FTModelPart.dl NULL): the joint draws
# nothing while it is on. src/dc/fighter.h FPACK_PART_NONE.
PART_TAG_NONE = 0xFE


def read_source(path):
    """A decomp source with any REGION_JP branches dropped (this ROM is the
    US one) -- both the offsets in comments and the container rows differ by
    region, and the JP variant comes first in the file."""
    src = open(path).read()
    return re.sub(r"#if defined\(REGION_JP\)(.*?)(?:#else(.*?))?#endif",
                  lambda m: m.group(2) or "", src, flags=re.S)


def find_reloc_source(suffix):
    """(file id, path) of the one relocData source named NNN_<suffix>.c --
    the decomp's filenames carry both the id and the name, so a fighter's
    model and main files are found rather than hardcoded."""
    P.require_decomp()
    hits = [f for f in os.listdir(RELOC_DIR)
            if re.fullmatch(r"\d+_%s\.c" % re.escape(suffix), f)]
    if len(hits) != 1:
        raise ValueError("expected exactly one %s file in %s, found %s"
                         % (suffix, RELOC_DIR, hits))
    return int(hits[0].split("_", 1)[0]), os.path.join(RELOC_DIR, hits[0])


def ftdata_row(fighter):
    """ft/ftdata.c's `FTData dFT<fighter>Data` as a list of its fields'
    source text -- the table that says which relocData files a fighter
    kind is made of (fttypes.h:86 FTData). A VS fighter's are all its
    own, so nothing needed this; a 1P kind's are not. Giant Donkey Kong
    has no model and no motion file at all (dFTGDonkeyData names
    llDonkeyModelFileID and llDonkeyMainMotionFileID), and Metal Mario
    has his own main motion but Mario's submotion."""
    src = read_source(os.path.join(P.DECOMP_DIR, "src", "ft", "ftdata.c"))
    m = re.search(r"FTData dFT%sData\s*=\s*\{(.*?)\n\};"
                  % re.escape(fighter), src, re.S)
    if not m:
        raise ValueError("no dFT%sData in ft/ftdata.c" % fighter)
    return [f.strip() for f in m.group(1).split(",")]


def motion_sources(fighter):
    """The two `<X>MainMotion` relocData files a kind's motion table lays
    its script offsets onto: (file_mainmotion_id's, file_submotion_id's),
    either or both None. Both fields name an `ll<X>MainMotionFileID`.

    ft/ftmain.c:4747-4762 is what makes this a pair rather than one file:
    a motion row's offset is added to `*p_file_submotion` when the row
    carries FTANIM_FLAG_SUBMOTION_SCRIPT and to `*p_file_mainmotion`
    otherwise. Three shapes exist and all three matter:

      Mario     mainmotion=Mario  submotion=-      no row flagged
      GDonkey   mainmotion=-      submotion=Donkey every row flagged
      MMario    mainmotion=MMario submotion=Mario  123 of 168 flagged

    so a VS fighter and a Polygon each read one file, and Metal Mario
    reads two."""
    row = ftdata_row(fighter)

    def name(field):
        m = re.fullmatch(r"&ll(\w+)MainMotionFileID", field)
        return m.group(1) if m else None

    main, sub = name(row[1]), name(row[2])
    if main is None and sub is None:
        raise ValueError("dFT%sData names no MainMotion file" % fighter)
    return main, sub


def motion_source(fighter):
    """The one file a kind with a single motion blob reads -- its own
    where it has one, the submotion slot's otherwise."""
    main, sub = motion_sources(fighter)
    return main or sub


def read_setup_parts_words(main_src):
    """FTAttributes.setup_parts as the two u32 words the source declares
    (d<F>Main_setup_parts[2]), MSB of the first word being entry 0."""
    m = re.search(r"setup_parts\[2\] = \{\s*(0x[0-9A-Fa-f]+),"
                  r"\s*(0x[0-9A-Fa-f]+)", main_src)
    if not m:
        raise ValueError("no setup_parts[2] in the main source")
    return [int(m.group(1), 16), int(m.group(2), 16)]


def read_setup_parts(main_src):
    """Which DObjDesc entries the game instantiates, as a list of entry
    indices in order -- FTAttributes.setup_parts, two u32s of flags tested
    high bit first. The figatree table (and p_mobjsubs) are indexed by the
    walk over the DObjs that *exist*, so table slot k belongs to entry
    setup[k] -- while no hidden part is linked (see read_anim_ex: a linked
    hidden part takes a leading slot); a fighter
    with holes in the mask (Kirby: the copy-ability hat joints, Samus:
    0xFFF803FF) breaks the slot == entry coincidence Mario's contiguous
    mask allows."""
    w0, w1 = read_setup_parts_words(main_src)
    mask = (w0 << 32) | w1
    return [i for i in range(64) if (mask >> (63 - i)) & 1]


def read_hiddenparts(main_src, fighter):
    """FTAttributes.hiddenparts (ft/fttypes.h:683 FTHiddenPart) out of the
    main source's d<F>Main_hiddenparts[N]: rows of {root_joint_id,
    parent_joint_id, partindex_0x8, joint_kind}, which ft/ftmain.c
    ftMainUpdateHiddenPartID makes a DObj for and links into the tree when
    an animation's FTAnimDesc bit i asks for row i (bit 31 is row 0). The
    first three rows are XRotN, TransN and YRotN on every fighter."""
    m = re.search(r"FTHiddenPart\s+d%sMain_hiddenparts\[(\d+)\]\s*=\s*\{(.*?)\n\};"
                  % fighter, main_src, re.S)
    if not m:
        raise ValueError("%s: no hiddenparts table" % fighter)
    rows = [tuple(int(x, 16) if x.startswith("0x") else int(x)
                  for x in re.findall(r"0x[0-9A-Fa-f]+|-?\d+", r))
            for r in re.findall(r"\{([^{}]*)\}", m.group(2))]
    if len(rows) != int(m.group(1)) or any(len(r) != 4 for r in rows):
        raise ValueError("%s: hiddenparts table did not parse" % fighter)
    return rows


# FTAttributes (ft/fttypes.h:955-956): the two pointers into the fighter's
# ShieldPose file, at these offsets from the top of the attributes block
ATTR_DOBJ_LOOKUP_OFF = 0x2D8
ATTR_SHIELD_ANIM_JOINTS_OFF = 0x2DC
SHIELD_DIRECTIONS = 8


def read_shieldpose(rom, fighter, extract, attr_base, live_count):
    """The fighter's ShieldPose file (relocData NNN_<F>ShieldPose.c) as
    the guard reads it: FTAttributes.dobj_lookup, a DObjDesc row per DObj
    of the tree walk from XRotN (XRotN, the instantiated entries in walk
    order, YRotN, then a zero-scale sentinel), and shield_anim_joints[8],
    one AObjEvent32 *[N] dispatch table per 45-degree sector of the stick
    -- N the same walk -- whose scripts pose the fighter from that sector's
    lean to the next one's over 45 frames (ft/ftcommon/ftcommonguard1.c
    ftCommonGuardInitJoints/UpdateJoints, angle_i picking the table and
    angle_f the frame). No motion row in ft/ftdata.c carries
    FTANIM_FLAG_SHIELDPOSE, so the file holds no figatree: these two
    tables are all of it, and they are all the guard needs.

    Found the way the game finds them: the two attribute pointers are
    extern relocations of the main file (lb/lbreloc.c:171-201, a site's
    words_num naming the offset in the extern file), so the offsets come
    off the ROM, and the file is named by the decomp's filename. Returns

      words        the whole file as native u32 words (the DObjDesc rows'
                   f32 fields keep their bit patterns)
      relocs       word indices inside the scripts that hold a pointer --
                   Jump, SetAnim and SetInterp targets -- each holding the
                   target's word index, as read_anim_ex hands them out
      lookup       word index of the DObjDesc rows; lookup_count rows of
                   11 words {id, dl (zeroed: the port hangs no display
                   list on these), translate, rotate, scale}
      tables       word index of each direction's table; table_count
                   entries each, a word index or 0 for NULL, left as
                   indices for the loader to make pointers
      floats       word indices of the f32 values in the scripts

    Every script is walked once (ssb_assets.animjoint_walk); an undefined
    opcode or a pointer nothing relocated is a ValueError, as it is for an
    animation the bank keeps."""
    main_id, _main_path = find_reloc_source(fighter + "Main")
    f = A.get_file(rom, main_id, extract)
    e = extract.read_entry(rom, extract.RELOC_SEG, main_id)
    sites = {}
    idx = e["reloc_extern"]
    order = []
    while idx != 0xFFFF:
        nxt, wn = struct.unpack_from(">HH", f, idx * 4)
        sites[idx * 4] = wn * 4
        order.append(idx * 4)
        idx = nxt
    # the file ids the extern chain names sit past the file's data in ROM,
    # one u16 per site in chain order (lbreloc.c:176)
    data_off = extract.rom_table_hi(extract.RELOC_SEG, extract.FILE_COUNT) \
        + e["data_offset"] + e["compressed_size"] * 4
    ids = struct.unpack_from(">%dH" % len(order), rom, data_off)
    file_of = dict(zip(order, ids))

    wanted = [attr_base + ATTR_DOBJ_LOOKUP_OFF] + \
        [attr_base + ATTR_SHIELD_ANIM_JOINTS_OFF + 4 * k
         for k in range(SHIELD_DIRECTIONS)]
    have = [site for site in wanted if site in sites]
    if not have:
        # A fighter nobody can make guard has no pose to strike. Master Hand
        # is the one: BossMain leaves dobj_lookup
        # and all eight shield_anim_joints NULL, which is why its extern
        # chain names no ShieldPose file, and ftCommonGuardInitJoints --
        # the only reader -- is reached through a guard status he has no
        # row for. The pack carries an empty pose: no words, no rows, no
        # tables, and the loader hangs nothing off it.
        #
        # All nine or none. A Main that relocated some of them has lost
        # the rest, and that is the case this must not swallow, so a
        # partial set still raises below.
        return {"words": [], "relocs": [], "lookup": 0, "lookup_count": 0,
                "tables": [0] * SHIELD_DIRECTIONS, "table_count": 0,
                "floats": [], "file_id": 0}
    if len(have) != len(wanted):
        raise ValueError("%s: %d of the %d shield-pose attribute pointers "
                         "are extern relocations, not all or none"
                         % (fighter, len(have), len(wanted)))
    # the file is whichever the chain names -- Luigi's pointers reach into
    # Mario's ShieldPose file (ft/ftdata.c names llMarioShieldPoseFileID
    # for him, as it names Mario's animations), so the filename cannot say
    sp_id = file_of[wanted[0]]
    if any(file_of[w] != sp_id for w in wanted):
        raise ValueError("%s: the nine shield-pose pointers name files %s"
                         % (fighter, sorted(set(file_of[w] for w in wanted))))
    if not any(re.fullmatch(r"%d_\w+ShieldPose\.c" % sp_id, fn)
               for fn in os.listdir(RELOC_DIR)):
        raise ValueError("%s: file %d is not a ShieldPose file"
                         % (fighter, sp_id))
    lookup_off = sites[wanted[0]]
    table_offs = [sites[w] for w in wanted[1:]]

    sf = A.get_file(rom, sp_id, extract)
    se = extract.read_entry(rom, extract.RELOC_SEG, sp_id)
    if se["reloc_extern"] != 0xFFFF:
        raise ValueError("%s: the ShieldPose file has extern relocations"
                         % fighter)
    sreloc = A.walk_reloc(sf, se["reloc_intern"])
    table_count = live_count + 2            # XRotN, the entries, YRotN
    lookup_count = table_count + 1          # and the sentinel row
    if lookup_off % 4 or any(t % 4 for t in table_offs):
        raise ValueError("%s: ShieldPose tables are not word aligned"
                         % fighter)
    lookup_end = lookup_off + 44 * lookup_count
    if lookup_end > len(sf):
        raise ValueError("%s: %d DObjDesc rows at 0x%X run past the file"
                         % (fighter, lookup_count, lookup_off))
    sx, sy, sz = struct.unpack_from(">3f", sf, lookup_off + 44 * table_count + 32)
    if (sx, sy, sz) != (0.0, 0.0, 0.0):
        raise ValueError("%s: DObjDesc row %d is not the zero-scale sentinel"
                         % (fighter, table_count))
    for i in range(table_count):
        sx, sy, sz = struct.unpack_from(">3f", sf, lookup_off + 44 * i + 32)
        if sx == 0.0 or sy == 0.0 or sz == 0.0:
            raise ValueError("%s: DObjDesc row %d has a zero scale"
                             % (fighter, i))

    nwords = len(sf) // 4
    words = list(struct.unpack(">%dI" % nwords, sf[:nwords * 4]))
    relocs = []
    floats = set()
    in_table = {}
    for k, t in enumerate(table_offs):
        for j in range(table_count):
            in_table[t + 4 * j] = k
    for loc, target in sorted(sreloc.items()):
        if target >= len(sf):
            raise ValueError("%s: ShieldPose reloc @0x%X -> 0x%X is past "
                             "the file" % (fighter, loc, target))
        if lookup_off <= loc < lookup_end:
            if (loc - lookup_off) % 44 != 4:
                raise ValueError("%s: a DObjDesc row's pointer is not its dl"
                                 % fighter)
            words[loc // 4] = 0
        elif loc in in_table:
            words[loc // 4] = target // 4
        elif loc < lookup_off:
            raise ValueError("%s: ShieldPose pointer at 0x%X is before the "
                             "DObjDesc rows" % (fighter, loc))
        else:
            words[loc // 4] = target // 4
            relocs.append(loc // 4)
    for k, t in enumerate(table_offs):
        targets = []
        for j in range(table_count):
            loc = t + 4 * j
            if loc in sreloc:
                targets.append(sreloc[loc])
                A.animjoint_walk(sf, sreloc, sreloc[loc], floats)
            elif words[loc // 4] != 0:
                raise ValueError("%s: direction %d entry %d is neither NULL "
                                 "nor a pointer" % (fighter, k, j))
        # the table is exactly table_count long: its first script starts
        # on the word after it
        if not targets or min(targets) != t + 4 * table_count:
            raise ValueError("%s: direction %d table at 0x%X is not %d "
                             "entries long (first script at 0x%X)"
                             % (fighter, k, t, table_count,
                                min(targets) if targets else -1))
    return {"words": words, "relocs": relocs, "lookup": lookup_off // 4,
            "lookup_count": lookup_count,
            "tables": [t // 4 for t in table_offs],
            "table_count": table_count,
            "floats": sorted(b // 4 for b in floats), "file_id": sp_id}


_ANIMJOINT_IDS = None


def animjoint_file_ids():
    """File ids of the animations the game parses as AnimJoint scripts
    rather than figatrees: the rows of every FTMotionDesc table in ftdata.c
    that carry FTANIM_FLAG_ANIMJOINT (ft/fttypes.h:59 is_anim_joint), mapped
    to their relocData files by name. The bit belongs to the motion row,
    not the file, but no file is referenced both ways. See
    ssb_assets.AnimJoint for what the difference is."""
    global _ANIMJOINT_IDS
    if _ANIMJOINT_IDS is None:
        ids = {}
        for fn in os.listdir(RELOC_DIR):
            m = re.fullmatch(r"(\d+)_(FT\w+)\.c", fn)
            if m:
                ids[m.group(2)] = int(m.group(1))
        names = set()
        for line in open(FTDATA_SOURCE):
            if "FTANIM_FLAG_ANIMJOINT" in line:
                m = re.search(r"&ll(FT\w+)FileID", line)
                if m:
                    names.add(m.group(1))
        missing = sorted(n for n in names if n not in ids)
        if missing:
            raise ValueError("no relocData file for %s" % ", ".join(missing))
        _ANIMJOINT_IDS = frozenset(ids[n] for n in names)
    return _ANIMJOINT_IDS


def read_anim_ex(rom, file_id, extract, joint_count=None, slots_map=None,
                 validate=False, native_splines=False):
    """An animation file, ready for the target's interpreters, of either
    kind. Returns a dict:

      kind     0 for a figatree (AObjEvent16 scripts, u16 words), 1 for an
               AnimJoint animation (AObjEvent32 scripts, u32 words); see
               animjoint_file_ids for how they are told apart
      words    the whole decompressed file as native words of that size
      entries  a word index per joint in draw order, -1 where the animation
               drives nothing
      slots    the file's own table, raw: a word index per slot or -1 for
               a NULL entry, as many as the table has. This is what the
               game hands lbCommonAddFighterPartsFigatree, which deals
               slot k to the k-th DObj of the tree walk from TopN's child
               -- a walk that includes whichever hidden parts the animation
               linked, so the table is not indexed by skeleton entry and
               cannot be reduced to `entries` without knowing them
      dropped  how many driven slots fell outside `joint_count` (None:
               every slot the file has)
      relocs   (kind 1) word indices of the words that hold a pointer --
               Jump, SetAnim and SetInterp targets. Each such word holds the
               target's word index, for the loader to turn into an address
               the way the game's reloc walk does. (kind 0, with
               `native_splines`) the index of the low word of each 32-bit
               pointer in a spline table, the pair holding the target's
               word index; see native_figatree_splines
      floats   (kind 1) word indices of the f32 values in the stream, for
               a host build whose f32 is wider than the stream's
      file, reloc  the raw file and its reloc map, for the reference

    `slots_map` maps table slot k to a DObjDesc entry (see read_setup_parts);
    None is the identity, right for a fighter whose setup_parts mask is
    contiguous. The file is carried whole because a figatree Loop's jump is
    a relative word delta and an AnimJoint's pointers are absolute --
    copying each script out would break both -- and because at one to five
    kilobytes it is a twentieth of the same animation sampled to poses.

    With `native_splines`, the spline tables a figatree's SetTranslateInterp
    names are rewritten for a 32-bit little-endian reader
    (native_figatree_splines); only a target that relocates them wants that.

    With `validate`, every script the table names is walked once, statically
    (ssb_assets.figatree_walk / animjoint_walk), and an undefined opcode, an
    unrelocated pointer or a read past the file is a ValueError. That is
    what keeps such a script out of a pack: the game's parsers spin on an
    opcode they have no case for, so the guard is here, where the data is
    made, not in a parser the port runs unmodified. Without it the file is
    returned as it is, for a check that wants to see what the parsers do.
    """
    f = A.get_file(rom, file_id, extract)
    entry = extract.read_entry(rom, extract.RELOC_SEG, file_id)
    reloc = A.walk_reloc(f, entry["reloc_intern"])
    kind = 1 if file_id in animjoint_file_ids() else 0
    wsize = 4 if kind else 2
    # The table is as long as the file says, not a constant: it varies per
    # animation (22..33 across the roster), and a slot past the model's own
    # joints is a non-skeleton DObj the pack does not build (e.g.
    # FTMarioAnimDamageAir drives two) -- dropped, and counted so the caller
    # can say so.
    slots = A.figatree_slots(reloc)
    table = [reloc.get(i * 4) for i in range(slots)]
    if joint_count is None:
        joint_count = slots
    raw_slots = [-1 if t is None else t // wsize for t in table]

    nwords = len(f) // wsize
    words = list(struct.unpack(">%d%s" % (nwords, "I" if kind else "H"),
                               f[:nwords * wsize]))
    entries = [-1] * joint_count
    dropped = 0
    floats = set()
    splines = set() if native_splines and not kind else None
    for k, t in enumerate(table):
        if t is None:
            continue
        if t >= len(f):
            raise ValueError("slot %d script at byte %d is past the end "
                             "of file %d" % (k, t, file_id))
        try:
            if kind:
                A.animjoint_walk(f, reloc, t, floats)
            else:
                A.figatree_walk(f, t // 2, splines)
        except ValueError as e:
            if validate:
                raise ValueError("slot %d: %s" % (k, e))
        node = slots_map[k] if slots_map is not None and \
            k < len(slots_map) else (k if slots_map is None else -1)
        if 0 <= node < joint_count:
            entries[node] = t // wsize
        else:
            dropped += 1

    relocs = []
    if kind:
        for loc, target in sorted(reloc.items()):
            if loc >= slots * 4:
                words[loc // 4] = target // 4
                relocs.append(loc // 4)
    elif splines:
        relocs = native_figatree_splines(f, reloc, words, splines, file_id)
    return {"kind": kind, "words": words, "entries": entries,
            "slots": raw_slots, "dropped": dropped, "relocs": relocs,
            "floats": sorted(b // 4 for b in floats),
            "file": f, "reloc": reloc}


# sys/interp.h SYInterpDesc, as the ROM lays it out: u8 kind, s16
# points_num, f32 unk04, Vec3f *points, f32 length, f32 *keyframes,
# f32 *quartics -- 24 bytes, 4-aligned, the same on the SH4
SPLINE_DESC = struct.Struct(">BxhfIfII")


def spline_points_num(kind, points_num):
    """How many Vec3f of `points` sys/interp.c reads for a spline:
    syInterpCubicSplineTimeFrac indexes points[frame] and [frame + 1] for
    a Linear one, points[frame * 3 .. + 3] for BezierS3, and four control
    points from points[frame] -- frame up to points_num - 2, or
    points_num - 2 itself at t = 1 -- for Bezier and Catrom."""
    return {0: points_num, 1: 3 * (points_num - 1) + 1,
            2: points_num + 2, 3: points_num + 2}[kind]


def native_figatree_splines(f, reloc, words, splines, file_id):
    """Rewrite, in `words` (the figatree as native u16 words), each spline
    table a SetTranslateInterp names so that its bytes, read in place on a
    32-bit little-endian target, are the SYInterpDesc and arrays the game
    reads (ft/ftanim.c:350 aims AObj.interpolate straight at them). A
    figatree is carried as u16 words, which leaves every f32 and pointer
    in the table with its two halves in each other's place -- read that
    way, syInterpGetFracFrame walks keyframes that never pass t, megabytes
    of them every frame. Each pointer pair is left holding its target's
    word index, low word first; the returned list names the low words, for
    the loader to make addresses (src/dc/fighter.c). The tables and the
    arrays they name are all whole 4-aligned words of the file."""
    relocs = []
    done = set()

    def put_u32(byte_off, value):
        words[byte_off // 2] = value & 0xFFFF
        words[byte_off // 2 + 1] = (value >> 16) & 0xFFFF

    def put_floats(byte_off, count):
        if byte_off % 4 or byte_off + 4 * count > len(f):
            raise ValueError("file %d: spline array at 0x%X x%d is not "
                             "whole words of the file" % (file_id, byte_off,
                                                          count))
        for i in range(count):
            o = byte_off + 4 * i
            if o not in done:
                put_u32(o, struct.unpack_from(">I", f, o)[0])
                done.add(o)

    for desc in sorted(splines):
        if desc % 4 or desc + SPLINE_DESC.size > len(f):
            raise ValueError("file %d: spline table at 0x%X is not in the "
                             "file" % (file_id, desc))
        if desc in done:
            continue
        kind, points_num, _unk04, _p, _length, _k, _q = \
            SPLINE_DESC.unpack_from(f, desc)
        if kind > 3 or points_num < 2:
            raise ValueError("file %d: spline table at 0x%X has kind %d, "
                             "%d points" % (file_id, desc, kind, points_num))
        words[desc // 2] = kind
        words[desc // 2 + 1] = points_num & 0xFFFF
        for off in (4, 12):
            put_u32(desc + off, struct.unpack_from(">I", f, desc + off)[0])
        counts = {8: 3 * spline_points_num(kind, points_num),
                  16: points_num,
                  20: 5 * (points_num - 1) if kind else 0}
        for off, count in sorted(counts.items()):
            target = reloc.get(desc + off)
            if target is None:
                if count and off != 20:
                    raise ValueError("file %d: spline table at 0x%X has no "
                                     "pointer at +%d" % (file_id, desc, off))
                put_u32(desc + off, 0)
                continue
            put_floats(target, count)
            put_u32(desc + off, target // 2)
            relocs.append((desc + off) // 2)
        done.update(range(desc, desc + SPLINE_DESC.size, 4))
    return relocs


def read_anim(rom, file_id, extract, joint_count=None, slots_map=None):
    """read_anim_ex for a figatree, as (words, entries, dropped). An
    AnimJoint animation is a ValueError: the caller left on this form --
    tools/check/fsca_range_check.py -- reads figatrees only."""
    r = read_anim_ex(rom, file_id, extract, joint_count, slots_map)
    if r["kind"]:
        raise ValueError("file %d is an AnimJoint animation, not a figatree"
                         % file_id)
    return r["words"], r["entries"], r["dropped"]


def find_joint_trees(source):
    """File offsets of the DObjDesc arrays, from the typed decomp source's
    own headers: `/* DObjDesc: <name> @ 0xNNNN (N entries) */`."""
    return [(m.group(1), int(m.group(2), 16)) for m in
            re.finditer(r"/\* DObjDesc: (\S+) @ 0x([0-9A-Fa-f]+)", source)]


def symbol_offset(source, name):
    """File offset of a decomp symbol in the model file.

    Untyped blobs encode it in their own name (`..._gap_0x26D0_sub_0x320`);
    typed arrays carry it in the comment above their definition
    (`... @ file 0x2680`). Either way the number comes from the decomp, not
    from a constant in this script.
    """
    m = re.search(r"_gap_0x([0-9A-Fa-f]+)(?:_sub_0x([0-9A-Fa-f]+))?$", name)
    if m:
        return int(m.group(1), 16) + (int(m.group(2), 16) if m.group(2) else 0)
    d = re.search(r"\b%s\s*\[\s*\d*\s*\]\s*=\s*\{" % re.escape(name), source)
    if not d:
        raise ValueError("no definition of %s in the model source" % name)
    # Merged arrays (Kirby's and Purin's MObjSub chains) document their span
    # instead: "N slots spanning file 0..0x67".
    span = re.search(r"spanning file (0x[0-9A-Fa-f]+|\d+)\.\.",
                     source[max(0, d.start() - 1200):d.start()])
    if span:
        return int(span.group(1), 0)
    marks = list(re.finditer(r"@ (?:file )?0x([0-9A-Fa-f]+)",
                             source[:d.start()]))
    if not marks:
        raise ValueError("no `@ 0xNNNN` offset comment above %s" % name)
    # The comment gives where the BLOCK starts, and the decomp pads some
    # blocks ahead of the symbol: `/* ... @ file 0x389C */ PAD(4);
    # dYoshiModel_JointTree_post[21] = {` puts the symbol at 0x38A0. Read
    # without the pad, Yoshi's costume table (both trees) was one row
    # early and Luigi's two, so each joint took the joint BEFORE's
    # costume script -- a green Yoshi with red upper arms and thighs,
    # in every mode.
    between = source[marks[-1].end():d.start()]
    if re.search(r"=\s*\{", between):
        raise ValueError("another definition lies between %s and its "
                         "offset comment" % name)
    pad = sum(int(n, 0) for n in
              re.findall(r"^\s*PAD\((\w+)\);", between, re.M))
    return int(marks[-1].group(1), 16) + pad


def find_commonparts(source):
    """The two FTCommonPart rows, high-detail first, each row a triple of
    (symbol, byte offset into it).

    FTCommonPart is { DObjDesc *dobjdesc; MObjSub ***p_mobjsubs;
    AObjEvent32 ***p_costume_matanim_joints; u8 flags; }, so each row of
    d<F>Main_commonparts_container names the three tables we need. A field is
    either `(T)&symbol` / `(T)symbol` or, where the target has no symbol of
    its own (Link), `(T)((u8*)symbol + 0xNNN)`.

    Either of the last two may be a bare `NULL`, and then that entry of
    the returned triple is None: the kind has no MObjSub table and no
    costume material animations, so its display lists carry their own
    textures and there is nothing to fold a costume into. Every VS
    fighter names all three; Metal Mario and the ten Polygon kinds name
    only the tree, which is what one texture and one
    costume looks like in this table.
    """
    body = re.search(r"FTCommonPartContainer\s+\w+\s*=\s*\{(.*?)\n\};",
                     source, re.S)
    if not body:
        raise ValueError("no FTCommonPartContainer in the main source")
    def field(kind, nullable=False):
        pat = (r"\(%s\)\s*(?:&?(\w+)(?:\[(\d+)\])?"
               r"|\(\s*\(u8\*\)\s*(\w+)\s*\+\s*(0x[0-9A-Fa-f]+|\d+)\s*\))"
               % kind)
        # the alternation goes OUTSIDE the four groups, so a NULL row
        # still fills them -- with None -- and the indexing below holds
        return r"(?:NULL|%s)" % pat if nullable else pat
    rows = re.findall(field(r"DObjDesc\*") + r",\s*" +
                      field(r"MObjSub\*\*\*", True) + r",\s*" +
                      field(r"AObjEvent32\*\*\*", True) +
                      r",\s*(0x[0-9A-Fa-f]+|\d+)\s*\}", body.group(1))
    if len(rows) != 2:
        raise ValueError("expected 2 FTCommonPart rows, found %d" % len(rows))
    def one(r, i):
        if not any(r[i:i + 4]):           # NULL: the kind has no table
            return None
        if r[i]:                          # &sym or &sym[idx]: 4-byte cells
            return (r[i], int(r[i + 1] or 0) * 4)
        return (r[i + 2], int(r[i + 3], 0))
    return [([one(r, i) for i in (0, 4, 8)], int(r[12], 0)) for r in rows]


def costume_count(fighter):
    """How many costumes the game ever deals `fighter`: one past the
    largest id in its dFTParamCostumeIDs row (ft/ftparam.c:56), whose
    royal[4], team[3] and develop ids are what ftParamGetCostumeCommonID,
    ftParamGetCostumeTeamID and ftParamGetCostumeDevelopID hand out. The
    rows are in nFTKind order."""
    src = read_source(os.path.join(P.DECOMP_DIR, "src", "ft", "ftparam.c"))
    body = re.search(r"FTCostume dFTParamCostumeIDs\[[^\]]*\]\s*=\s*\{(.*?)\n\};",
                     src, re.S)
    if not body:
        raise ValueError("no dFTParamCostumeIDs in ft/ftparam.c")
    rows = re.findall(r"\{\s*\{([^}]*)\},\s*\{([^}]*)\},\s*(\d+)\s*\},?\s*"
                      r"//\s*([^\n]+)", body.group(1))
    names = {"Donkey": "Donkey Kong", "Captain": "Captain Falcon",
             "Purin": "Jigglypuff",
             # the 1P kinds, whose rows are all zeroes (one costume)
             # except Giant Donkey Kong's, which is Donkey Kong's
             "Boss": "Master Hand", "MMario": "Metal Mario",
             "GDonkey": "Giant Donkey Kong",
             "NMario": "Poly Mario", "NFox": "Poly Fox",
             "NDonkey": "Poly Donkey Kong", "NSamus": "Poly Samus",
             "NLuigi": "Poly Luigi", "NLink": "Poly Link",
             "NYoshi": "Poly Yoshi", "NCaptain": "Poly Captain Falcon",
             "NKirby": "Poly Kirby", "NPikachu": "Poly Pikachu",
             "NPurin": "Poly Jigglypuff", "NNess": "Poly Ness"}
    want = names.get(fighter, fighter)
    for royal, team, develop, name in rows:
        if name.strip() == want:
            ids = [int(v) for v in (royal + "," + team).split(",")] + \
                [int(develop)]
            return max(ids) + 1
    raise ValueError("no dFTParamCostumeIDs row for %s" % fighter)


def read_accesspart(rom, fighter, main_src):
    """FTAttributes.accesspart off the ROM: {"joint_id", "dl", "mobj",
    "cost"}, pointers as (file id, byte offset) or None -- Pikachu's hat
    and Jigglypuff's bow, the FTAccessPart { s32 joint_id; Gfx *dl;
    MObjSub **mobjsubs; AObjEvent32 **costume_matanim_joints } that
    ftManagerMakeFighter hangs on its joint for any costume but 0. None
    for everyone else."""
    m = re.search(r"/\* @ 0x([0-9A-Fa-f]+), 16 bytes: FTAttributes\."
                  r"accesspart target", main_src)
    if not m:
        if re.search(r"NULL,\s*/\* accesspart \*/", main_src):
            return None
        raise ValueError("%sMain: no accesspart" % fighter)
    main_id, _ = find_reloc_source(fighter + "Main")
    f = A.get_file(rom, main_id, ssb_extract)
    entry = ssb_extract.read_entry(rom, ssb_extract.RELOC_SEG, main_id)
    intern = A.walk_reloc(f, entry["reloc_intern"])
    extern = A.walk_reloc_extern(rom, ssb_extract, main_id, f)
    off = int(m.group(1), 16)

    def ptr(o):
        if o in intern:
            return (main_id, intern[o])
        if o in extern:
            return extern[o]
        if struct.unpack_from(">I", f, o)[0] != 0:
            raise AssertionError("%sMain+0x%X: a non-NULL word that is no "
                                 "pointer" % (fighter, o))
        return None

    return {"joint_id": struct.unpack_from(">i", f, off)[0],
            "dl": ptr(off + 4), "mobj": ptr(off + 8), "cost": ptr(off + 12)}


def read_textureparts(rom, fighter, main_src):
    """FTAttributes.textureparts_container off the ROM: the game's six
    bytes, FTTexturePart[2] of { u8 joint_id; u8 detail[2] }, or None for
    a fighter without one. The decomp gives some fighters only the first
    entry's four bytes and the second is whatever follows in the file,
    exactly as the game reads it; nothing asks for it (texture_part_frames
    finds no event naming it)."""
    m = re.search(r"/\* @ 0x([0-9A-Fa-f]+), \d+ bytes: FTAttributes\."
                  r"textureparts_container target", main_src)
    if not m:
        if re.search(r"NULL,\s*/\* textureparts_container \*/", main_src):
            return None
        raise ValueError("%sMain: no textureparts_container" % fighter)
    main_id, _ = find_reloc_source(fighter + "Main")
    f = A.get_file(rom, main_id, ssb_extract)
    off = int(m.group(1), 16)
    return bytes(f[off:off + 6])


def texture_part_frames(fighter):
    """{texture part: frame count} from the fighter's motion scripts: one
    past the largest texture id an ftMotionCommandSetTexturePartID in
    relocData <F>MainMotion.c sets. Those events are the only way the
    game changes one (ft/ftmain.c:598); a part no script names gets no
    frames."""
    out = {}
    for name in dict.fromkeys(n for n in motion_sources(fighter) if n):
        _, path = find_reloc_source(name + "MainMotion")
        for v in re.findall(r"ftMotionCommandSetTexturePartID\((\d+)\)",
                            read_source(path)):
            part, frame = int(v) >> 20, int(v) & 0xFFFFF
            out[part] = max(out.get(part, 0), frame + 1)
    return out


def read_skeleton(rom, fighter, main_src):
    """FTAttributes.skeleton off the ROM: the electric
    shock's skeleton, or None for a fighter without one.

    The attribute points at FTSkeleton *[3]: word 0 is not a pointer but a
    joint id ftDisplayMainDrawAll checks is built and has a display list
    before it draws the skeleton, and words 1 and 2 are one FTSkeleton
    table per skeleton id (GMColEventDefault's value), NULL where the
    fighter has none. A table is { Gfx *dl; u8 flags } per DObjDesc entry
    (joints[nFTPartsJointCommonStart + i]); flags nibble 1 makes dl a
    Gfx *dls[2], dls[0] drawn under the parent's matrix and dls[1] under
    the joint's own (ftDisplayMainDrawSkeleton). A table's length is its
    location comment's byte count in the relocData source.

    Returns {"joint": id, "tables": {skeleton id: {DObjDesc index:
    (file id, pre dl offset or None, dl offset or None)}}}."""
    m = re.search(r"/\* @ 0x([0-9A-Fa-f]+), 12 bytes: FTAttributes\."
                  r"skeleton target", main_src)
    if not m:
        if re.search(r"NULL,\s*/\* skeleton \*/", main_src):
            return None
        raise ValueError("%sMain: no skeleton" % fighter)
    main_id, _ = find_reloc_source(fighter + "Main")
    f = A.get_file(rom, main_id, ssb_extract)
    entry = ssb_extract.read_entry(rom, ssb_extract.RELOC_SEG, main_id)
    intern = A.walk_reloc(f, entry["reloc_intern"])
    extern = A.walk_reloc_extern(rom, ssb_extract, main_id, f)
    files = {main_id: (f, intern)}

    def ptr(o, fid=main_id):
        pf, pi = files[fid]
        if o in pi:
            return (fid, pi[o])
        if fid == main_id and o in extern:
            return extern[o]
        if struct.unpack_from(">I", pf, o)[0] != 0:
            raise AssertionError("%s file %d+0x%X: a non-NULL word that is "
                                 "no pointer" % (fighter, fid, o))
        return None

    off = int(m.group(1), 16)
    if off in intern or off in extern:
        raise AssertionError("%sMain: skeleton word 0 is a pointer" % fighter)
    out = {"joint": struct.unpack_from(">i", f, off)[0], "tables": {}}
    for sid in (1, 2):
        t = ptr(off + 4 * sid)
        if t is None:
            continue
        if t[0] != main_id:
            raise AssertionError("%sMain: skeleton table %d is in file %d"
                                 % (fighter, sid, t[0]))
        lm = re.search(r"/\* @ 0x%04X, (\d+) bytes: " % t[1], main_src)
        if not lm or int(lm.group(1)) % 8:
            raise AssertionError("%sMain: no 8-byte-row location comment for "
                                 "skeleton table %d at 0x%04X"
                                 % (fighter, sid, t[1]))
        rows = {}
        for i in range(int(lm.group(1)) // 8):
            o = t[1] + 8 * i
            d = ptr(o)
            flags = f[o + 4] & 0xF
            if d is None:
                continue
            if flags == 0:
                rows[i] = (d[0], None, d[1])
            elif flags == 1:
                fid = d[0]
                if fid not in files:
                    pf = A.get_file(rom, fid, ssb_extract)
                    pe = ssb_extract.read_entry(rom, ssb_extract.RELOC_SEG,
                                                fid)
                    files[fid] = (pf, A.walk_reloc(pf, pe["reloc_intern"]))
                pre, dl = ptr(d[1], fid), ptr(d[1] + 4, fid)
                for q in (pre, dl):
                    if q is not None and q[0] != fid:
                        raise AssertionError("%s: skeleton %d row %d's dls "
                                             "leave file %d"
                                             % (fighter, sid, i, fid))
                if pre is None and dl is None:
                    continue
                rows[i] = (fid, pre[1] if pre else None,
                           dl[1] if dl else None)
            else:
                raise AssertionError("%sMain: skeleton %d row %d flags %d"
                                     % (fighter, sid, i, flags))
        out["tables"][sid] = rows
    return out


def read_modelparts(rom, fighter, main_src, model_id, which):
    """FTAttributes.modelparts_container, off the ROM: {DObjDesc index:
    [part, ...]} with one dict per model part id at detail `which` (0 high,
    1 low): {"file", "dl", "mobj", "cost", "main", "flags"}, pointers as
    (file id, byte offset) or None.

    The container is FTModelPartDesc *[N] by DObjDesc index (fttypes.h:156,
    joints[nFTPartsJointCommonStart + i]) and each desc an FTModelPart
    [ids][2] of { Gfx *dl; MObjSub **mobjsubs; AObjEvent32
    **costume_matanim_joints; AObjEvent32 **main_matanim_joints; u8 flags }.
    Neither array carries its own length, so both are the decomp's: the
    relocData source's array sizes, at the offsets its location comments
    and desc names give -- checked against the ROM's own pointers.
    """
    m = re.search(r"/\* @ 0x([0-9A-Fa-f]+), (\d+) bytes: FTAttributes\."
                  r"modelparts_container target(?:(?!\*/).)*\*/\s*"
                  r"FTModelPartDesc \*\w+_modelparts_container\[(\d+)\]",
                  main_src, re.S)
    if not m:
        if re.search(r"NULL,\s*/\* modelparts_container \*/", main_src):
            return {}
        raise ValueError("%sMain: no modelparts_container" % fighter)
    cont_off, cont_n = int(m.group(1), 16), int(m.group(3))
    if int(m.group(2)) != cont_n * 4:
        raise AssertionError("%sMain: modelparts_container is %s bytes for "
                             "%d entries" % (fighter, m.group(2), cont_n))
    main_id, _ = find_reloc_source(fighter + "Main")
    f = A.get_file(rom, main_id, ssb_extract)
    entry = ssb_extract.read_entry(rom, ssb_extract.RELOC_SEG, main_id)
    intern = A.walk_reloc(f, entry["reloc_intern"])
    extern = A.walk_reloc_extern(rom, ssb_extract, main_id, f)

    def ptr(o):
        if o in intern:
            return (main_id, intern[o])
        if o in extern:
            return extern[o]
        if struct.unpack_from(">I", f, o)[0] != 0:
            raise AssertionError("%sMain+0x%X: a non-NULL word that is no "
                                 "pointer" % (fighter, o))
        return None

    out = {}
    for i in range(cont_n):
        d = ptr(cont_off + 4 * i)
        if d is None:
            continue
        dm = re.search(r"FTModelPart d%sMain_modelparts_desc_0x%03X\[(\d+)\]"
                       % (fighter, d[1]), main_src)
        if d[0] != main_id or not dm or int(dm.group(1)) % 2:
            raise AssertionError("%sMain: container entry %d -> %r has no "
                                 "desc in the source" % (fighter, i, d))
        rows = []
        for k in range(int(dm.group(1)) // 2):
            o = d[1] + 20 * (2 * k + which)
            dl = ptr(o)
            rows.append({"file": dl[0] if dl else None,
                         "dl": dl[1] if dl else None,
                         "mobj": ptr(o + 4), "cost": ptr(o + 8),
                         "main": ptr(o + 12), "flags": f[o + 16]})
        out[i] = rows
    return out


def part_main_mobjs(fighter, model_id, f, reloc, nodes, modelparts):
    """FTModelPart.main_matanim_joints: the MObjs a part
    hangs with a material script of its own, which
    lbCommonAddMObjForFighterPartsDObj adds, parses and plays, and which
    ftParamUpdateAnimKeys plays on from there. {DObjDesc index: ([MObjSub
    offset], [script offset or None])} -- Samus's grapple beam, whose six
    segments flicker through three tiles, and no other VS part.

    Every part a joint can wear must hang the same MObjs with the same
    scripts, so the pack can carry them per joint (fighter.h FPackMObjs);
    and the joint's tree entry must have no display list, so a batch's
    (entry, MObj) names one MObjSub and not the tree's as well."""
    out = {}
    for i in sorted(modelparts):
        rows = [r for r in modelparts[i] if r["dl"] is not None]
        mains = [r for r in rows if r["main"] is not None]
        if not mains:
            continue
        if len(mains) != len(rows) or \
                len({(r["file"], r["mobj"], r["main"]) for r in rows}) != 1:
            raise AssertionError("%s: joint %d's parts do not all play the "
                                 "same material scripts" % (fighter, i))
        r = mains[0]
        if r["file"] != model_id or r["mobj"] is None or \
                r["mobj"][0] != model_id or r["main"][0] != model_id:
            raise AssertionError("%s: joint %d's animated MObjs are not in "
                                 "the model file" % (fighter, i))
        if nodes[i].dl_off is not None:
            raise AssertionError("%s: joint %d has animated part MObjs and "
                                 "a display list of its own" % (fighter, i))
        subs = A._ptr_list(reloc, r["mobj"][1])
        scripts = [reloc.get(r["main"][1] + 4 * j) for j in range(len(subs))]
        out[i] = (subs, scripts)
    return out


def part_alternates(rom, fighter, model_id, f, reloc, nodes, modelparts,
                    costume=COSTUME, accesspart=None):
    """The model parts as MeshBaker.bake's `parts` argument, and the id
    table the pack carries: ({DObjDesc index: [(tag, dl, subs, file)]},
    {DObjDesc index: (tree tag, [(tag, flags), ...] by part id)}). A dl of
    None is the tree's own display list, and the tree tag is its tag, or
    PART_TAG_NONE where the tree entry has none.

    One tag per distinct (file, display list, MObjSubs, costume scripts) a
    joint can wear, numbered 1.. across the model, the joint's own display
    list first; a part with no display list (the game hides the joint
    with it) is tag PART_TAG_NONE. Part 0 must be the joint's own display
    list -- it is what lbCommonSetupFighterPartsDObjs builds -- so the
    default batches can carry part 0's tag."""
    files = {model_id: f}
    relocs = {model_id: reloc}
    alternates = {}
    table = {}
    tag = 0
    for i in sorted(modelparts):
        rows = modelparts[i]
        node = nodes[i]
        r0 = rows[0]
        # A joint whose tree entry has no display list starts with no part
        # on (ftmanager.c:814-824 deals it modelpart id -1), and part 0 is
        # then just another part; one that has one must have it as part 0.
        if node.dl_off is not None and \
                (r0["file"], r0["dl"]) != (model_id, node.dl_off):
            raise AssertionError("%s: joint %d's part 0 draws %r, the tree "
                                 "0x%X" % (fighter, i, (r0["file"], r0["dl"]),
                                           node.dl_off))
        keys = {}
        ids = []
        alternates[i] = []
        for r in rows:
            if r["dl"] is None:
                ids.append((PART_TAG_NONE, r["flags"]))
                continue
            key = (r["file"], r["dl"], r["mobj"], r["cost"])
            if key not in keys:
                tag += 1
                keys[key] = tag
                if r is r0 and node.dl_off is not None:
                    alternates[i].append((tag, None, None, None))
                else:
                    fid = r["file"]
                    if fid not in files:
                        files[fid] = A.get_file(rom, fid, ssb_extract)
                        e = ssb_extract.read_entry(rom, ssb_extract.RELOC_SEG,
                                                   fid)
                        relocs[fid] = A.walk_reloc(files[fid],
                                                   e["reloc_intern"])
                    pf, pr = files[fid], relocs[fid]
                    subs = None
                    if r["mobj"] is not None:
                        if r["mobj"][0] != fid:
                            raise AssertionError("%s: joint %d's MObjSubs are "
                                                 "not in its display list's "
                                                 "file" % (fighter, i))
                        subs = [A.read_mobjsub(pf, pr, o) for o in
                                A._ptr_list(pr, r["mobj"][1])]
                        scripts = [None] * len(subs)
                        if r["cost"] is not None:
                            scripts = [pr.get(r["cost"][1] + 4 * j)
                                       for j in range(len(subs))]
                        A.apply_costume(pf, pr, subs, scripts, costume)
                    # else gcRemoveMObjAll and no MObj added back, and a
                    # branch into segment 0xE lands on whatever the last
                    # MObj drawn left there (Luigi's high open hand): the
                    # joint's own is the nearest thing, so subs stays None
                    alternates[i].append((tag, r["dl"], subs,
                                          None if fid == model_id else pf))
            ids.append((keys[key], r["flags"]))
        table[i] = (ids[0][0] if node.dl_off is not None else PART_TAG_NONE,
                    ids)
    if accesspart is not None:
        # The accessory: one more tag, on its joint, baked like a part
        # (fighter.h FPackParts.accessory_tag) -- but drawn beside the
        # joint's own display list rather than in place of it.
        a = accesspart
        node = nodes[a["joint_id"] - 4]
        fid = a["dl"][0]
        if a["mobj"] is None or a["mobj"][0] != fid:
            raise AssertionError("%s: the accessory's MObjSubs are not in "
                                 "its display list's file" % fighter)
        if fid not in files:
            files[fid] = A.get_file(rom, fid, ssb_extract)
            e = ssb_extract.read_entry(rom, ssb_extract.RELOC_SEG, fid)
            relocs[fid] = A.walk_reloc(files[fid], e["reloc_intern"])
        pf, pr = files[fid], relocs[fid]
        subs = [A.read_mobjsub(pf, pr, o) for o in
                A._ptr_list(pr, a["mobj"][1])]
        scripts = [None] * len(subs)
        if a["cost"] is not None:
            scripts = [pr.get(a["cost"][1] + 4 * j) for j in range(len(subs))]
        A.apply_costume(pf, pr, subs, scripts, costume)
        tag += 1
        alternates.setdefault(node.index, []).append(
            (tag, a["dl"][1], subs, None if fid == model_id else pf))
        accessory_tag = tag
    else:
        accessory_tag = 0
    if tag >= PART_TAG_NONE:
        raise AssertionError("%s: %d model part tags" % (fighter, tag))
    return alternates, table, accessory_tag


def build_fighter(rom, fighter, tree="high", costume=COSTUME):
    """Decode one fighter's model completely: sources found, tree selected,
    costume folded, mesh baked. Returns a dict of everything the emitters
    need; shared by the header (main) and pack (ssb_packexport) outputs."""
    which = TREE_ORDER.index(tree)
    _, main_path = find_reloc_source(fighter + "Main")
    main_src = read_source(main_path)
    (dobj, mobj, post), part_flags = find_commonparts(main_src)[which]
    # WHOSE model file, read off the container rather than assumed to be
    # `<fighter>Model`. It is that for every VS fighter, but not for the
    # 1P kinds: dGDonkeyMain's container names dDonkeyModel_JointTree --
    # Giant Donkey Kong is Donkey Kong's geometry and animations with his
    # own FTAttributes, and there is no GDonkeyModel file at all. Every
    # model symbol is `d<Name>_...`, so the container names the file.
    model_name = dobj[0].split("_", 1)[0].lstrip("d")
    model_id, model_path = find_reloc_source(model_name)

    model_src = read_source(model_path)
    trees = find_joint_trees(model_src)
    setup = read_setup_parts(main_src)
    # Resolve the container's pointers to file offsets and find the tree by
    # *offset*; matching the labelled DObjDesc comments by name or order
    # would trip over Kirby's third, unreferenced tree and Link's high tree,
    # which has no symbol of its own.
    tree_off = symbol_offset(model_src, dobj[0]) + dobj[1]
    hits = sorted(set(t for t in trees if t[1] == tree_off))
    if len(hits) != 1:
        sys.exit("container's DObjDesc resolves to 0x%04X; the model source "
                 "labels trees at %s"
                 % (tree_off, sorted(set("0x%04X" % o for _, o in trees))))
    tree_name = hits[0][0]
    # NULL in the container, for a kind with one texture and one costume
    # (Metal Mario, the Polygons): no MObjSub table and no costume
    # material animations, so the joints' own display lists are the whole
    # material story and there is no costume to fold.
    mobj_off = None if mobj is None else symbol_offset(model_src, mobj[0]) + mobj[1]
    post_off = None if post is None else symbol_offset(model_src, post[0]) + post[1]

    f = A.get_file(rom, model_id, ssb_extract)
    entry = ssb_extract.read_entry(rom, ssb_extract.RELOC_SEG, model_id)
    reloc = A.walk_reloc(f, entry["reloc_intern"])
    print("file %d (%s): %d bytes decompressed"
          % (model_id, model_name, len(f)))

    import pygfxd
    # An FTCommonPart whose flags nibble is 1 (Yoshi) points each desc's dl
    # at a Gfx*[2]: dls[0] draws under the parent's matrix, dls[1] under the
    # joint's own (ftDisplayMainDrawDefault).
    root, nodes = A.read_dobj_tree(f, reloc, tree_off,
                                   dls_pair=(part_flags & 0xF) == 1)
    if mobj_off is None:
        mobjsubs = [[] for _ in nodes]
    else:
        mobjsubs = A.read_mobjsubs(f, reloc, mobj_off, len(nodes))
    if post_off is None:
        scripts = [[] for _ in mobjsubs]
    else:
        scripts = A.read_matanim_table(f, reloc, post_off,
                                       [len(s) for s in mobjsubs])
    for subs, scr in zip(mobjsubs, scripts):
        A.apply_costume(f, reloc, subs, scr, costume)
    print("joint tree %s @ 0x%04X: %d DObjDesc entries, %d instantiated "
          "(setup_parts)" % (tree_name, tree_off, len(nodes),
                             sum(1 for i in setup if i < len(nodes))))
    # An entry the mask leaves out is not built at spawn, but a hidden-part
    # row can name it as a root (ft/ftmain.c ftMainUpdateHiddenPartID makes
    # it, with the entry's display list, when an animation asks): those
    # keep their geometry, baked under their own joint, and the runtime
    # draws it only while the part is linked. Yoshi's entry 5 and Samus's
    # 20 and 21 are the ones with a display list; the rest of the hidden
    # roots are bare transforms. An entry that is neither masked in nor a
    # hidden root the game never draws, so neither does this.
    hidden_roots = set(r[0] - 4 for r in read_hiddenparts(main_src, fighter)
                       if r[0] >= 4)
    ghosts = [n.index for n in nodes
              if n.index not in setup and n.index not in hidden_roots
              and n.dl_off]
    if ghosts:
        print("note: entries %s have display lists but no setup_parts bit "
              "and no hidden-part row; the game never draws them, so "
              "neither does this" % ghosts)
    drawn_hidden = [n.index for n in nodes
                    if n.index not in setup and n.index in hidden_roots
                    and n.dl_off]
    if drawn_hidden:
        print("note: entries %s are hidden-part roots with display lists; "
              "baked, drawn only while linked" % drawn_hidden)
    if mobj_off is None:
        print("p_mobjsubs NULL: the joints' own display lists carry the "
              "textures")
    else:
        print("p_mobjsubs %s @ 0x%04X: %d MObjSubs across %d entries" %
              (mobj[0], mobj_off, sum(len(s) for s in mobjsubs),
               sum(1 for s in mobjsubs if s)))
    if post_off is None:
        print("p_costume_matanim_joints NULL: one costume, nothing to fold")
    else:
        print("p_costume_matanim_joints %s @ 0x%04X: %d scripts, costume %d" %
              (post[0], post_off,
               sum(1 for s in scripts for x in s if x is not None), costume))

    modelparts = read_modelparts(rom, fighter, main_src, model_id, which)
    accesspart = read_accesspart(rom, fighter, main_src)
    alternates, part_table, accessory_tag = part_alternates(
        rom, fighter, model_id, f, reloc, nodes, modelparts, costume,
        accesspart)
    print("modelparts_container: joints %s, %d part tags"
          % (sorted(part_table), sum(len(a) for a in alternates.values())))

    # the skeleton: one more tag per joint per skeleton id,
    # after the model parts' and the accessory's, baked by its own walk
    skeleton = read_skeleton(rom, fighter, main_src)
    skeleton_bake = {}
    skeleton_tags = {}
    tag = max([t for alts in alternates.values() for t, _d, _s, _f in alts]
              + [accessory_tag, 0])
    for sid, rows in sorted((skeleton or {"tables": {}})["tables"].items()):
        skeleton_bake[sid] = {}
        for i, (fid, pre, dl) in sorted(rows.items()):
            if fid != model_id:
                raise AssertionError("%s: skeleton %d entry %d is in file %d"
                                     % (fighter, sid, i, fid))
            if i not in setup and i not in hidden_roots:
                # never built, so never drawn (ftDisplayMainDrawSkeleton
                # walks the fighter's DObjs)
                continue
            if i >= len(nodes):
                raise AssertionError("%s: skeleton %d entry %d past the "
                                     "tree's %d" % (fighter, sid, i,
                                                    len(nodes)))
            tag += 1
            skeleton_bake[sid][i] = (tag, pre, dl)
            skeleton_tags[tag] = (sid, i)
    if tag >= PART_TAG_NONE:
        raise AssertionError("%s: %d part tags with the skeleton"
                             % (fighter, tag))
    if skeleton is not None:
        print("skeleton: joint %d, %s" % (skeleton["joint"], ", ".join(
            "id %d %d entries" % (sid, len(r))
            for sid, r in sorted(skeleton["tables"].items()))))

    main_mobjs = part_main_mobjs(fighter, model_id, f, reloc, nodes,
                                 modelparts)
    textureparts = read_textureparts(rom, fighter, main_src)
    tp_frames = texture_part_frames(fighter)
    frame_mobjs = {}
    for part, count in sorted(tp_frames.items()):
        if textureparts is None or part > 1:
            raise AssertionError("%s: a motion script sets texture part %d, "
                                 "which the fighter has no row for"
                                 % (fighter, part))
        joint_id, detail = textureparts[3 * part], \
            textureparts[3 * part + 1 + which]
        frame_mobjs[(joint_id - 4, detail)] = (part, count)
    print("textureparts_container: %s, frames %s"
          % (textureparts.hex() if textureparts else None, tp_frames))
    # an animated part MObj's tiles are baked as a texture part's are, one
    # per frame its script ever shows, under the part "main"
    for i, (subs, scripts) in sorted(main_mobjs.items()):
        for mi, sc in enumerate(scripts):
            n = A.matanim_frame_count(f, reloc, sc) if sc is not None else 0
            if n > 1:
                if (i, mi) in frame_mobjs:
                    raise AssertionError("%s: joint %d MObj %d is a texture "
                                         "part and an animated part MObj"
                                         % (fighter, i, mi))
                frame_mobjs[(i, mi)] = ("main", n)
    print("main_matanim_joints: joints %s" % sorted(main_mobjs))

    baker = A.bake_retry(
        lambda force: A.MeshBaker(f, pygfxd, mobjsubs, force_extent=force,
                                  frame_mobjs=frame_mobjs),
        lambda b: b.bake(root,
                         skip={n.index for n in nodes
                               if n.index not in setup
                               and n.index not in hidden_roots},
                         parts=alternates, skeleton=skeleton_bake))
    verts, tris, joints = baker.verts, baker.tris, baker.joints
    lo, hi = baker.bounds()

    for i, j in enumerate(joints):
        n = j["node"]
        print("  joint id=%2d dl=0x%04X tris=%3d origin=(%7.2f,%7.2f,%7.2f)"
              % (n.joint_id, n.dl_off or 0, j["tri_count"],
                 n.matrix[3][0], n.matrix[3][1], n.matrix[3][2]))
        for b in baker.batches[j["batch_first"]:
                               j["batch_first"] + j["batch_count"]]:
            tex, prim, l1, l2, shaded, lit = b["mat"][:6]
            print("      batch tris=%3d tex=%-2d prim=%08X l1=%08X l2=%08X%s"
                  % (b["tri_count"], tex, prim, l1, l2,
                     "" if shaded else "  (unshaded)"))
    for i, t in enumerate(baker.textures):
        print("  tex %d: img 0x%04X %dx%d phys -> %dx%d baked, pal %d, "
              "clamp %s%s" % (i, t["img"], t["phys_w"], t["phys_h"],
                              t["w"], t["h"], t["pal"],
                              "U" if t["clamp_u"] else "-",
                              "V" if t["clamp_v"] else "-"))
    print("baked %d joints, %d batches, %d verts, %d tris (%d "
          "gsSPModifyVertex)" % (len(joints), len(baker.batches), len(verts),
                                 len(tris), baker.modified_vtx))
    print("world bounds x[%.1f %.1f] y[%.1f %.1f] z[%.1f %.1f]"
          % (lo[0], hi[0], lo[1], hi[1], lo[2], hi[2]))
    return {"fighter": fighter, "model_id": model_id,
            "model_name": model_name, "tree_name": tree_name,
            "tree_off": tree_off, "setup": setup, "baker": baker,
            "verts": verts, "tris": tris, "joints": joints,
            "lo": lo, "hi": hi, "part_table": part_table,
            "accesspart": accesspart, "accessory_tag": accessory_tag,
            "skeleton": skeleton, "skeleton_tags": skeleton_tags,
            "textureparts": textureparts, "texture_part_frames": tp_frames,
            "main_mobjs": main_mobjs, "model_file": f, "model_reloc": reloc}

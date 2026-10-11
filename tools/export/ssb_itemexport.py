#!/usr/bin/env python3
"""ssb64-dc: the item data and models, packed.

This is the asset: `ITCommonData` (reloc file 0xFB)
is what every item's `o_attributes` reads through, and its absence is why
every ported `it/*.c` LEFT OUT its `itXxxMakeItem` wrapper -- the same
gap `itmanitmanager.c`'s header has carried.

The two files ship as ONE blob, and the reason is in the pointers.
`ITCommonData` is 3,392 bytes with **no internal relocations at all** --
so each of its named blocks (`ItemAttributes` x34, `WeaponAttributes`
x12, `AttackEvents` x6, `VelocitiesY Container`, `Angles`, and the
`AnimJoint`/`MatAnimJoint`/`DisplayList` sets) sits at a fixed offset --
but its **68 extern sites all point into `ITCommonObject` (0x56)**. Those
cannot be baked at export: a baked value would have to be the loader's
own base address, which export cannot know. The port's answer to exactly
this is MPK1's: carry a FIXUP TABLE of (site, target) pairs and let the
loader write the resolved pointer (src/dc/stage.c:224-236). With the
fixup targets being offsets into the model region, there is no second
blob for them to point across to, which is why one pack holds both files.

`IFCommonItem` (0x57) is NOT here. It is 160 bytes, two internal
relocations and one `Sprite Arrow`, it has no extern sites, and nothing
points into it -- it is the sprite exporter's job, not this one's.

Usage: python3 tools/export/ssb_itemexport.py --out romdisk/itcommon.itp
                                        [--rom <rom.z64>] [--dump]
"""
import os
import re
import struct
import sys

sys.path[:0] = [os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), d)
                for d in ("lib", "export", "check")]
import ssb_extract               # noqa: E402
import ssb_logicexport as L      # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
DECOMP = os.environ.get("SSB_DECOMP_DIR", "/opt/ssb-decomp-re")
if not os.path.isdir(DECOMP):
    DECOMP = os.path.join(ROOT, "ssb-decomp-re")
DESC = os.path.join(DECOMP, "tools", "relocFileDescriptions.us.txt")

ITCOMMONDATA = 0xFB     # llITCommonDataFileID
ITCOMMONOBJECT = 0x56   # llITCommonObjectFileID

HEADER = 80        # 4s19I: the four sections, then the six the three
                   # data sections added (monster events,
                   # attack events, weapon attributes),
                   # then the model region's own reloc table
                   # (version 2 -- see the note beside `ifixes` below)
NAME_MAX = 32


def read_blocks(fid):
    """The block entries the descriptions list for a file, in file order:
    (kind, name, offset). The same read tools/export/ssb_stageexport.py makes
    for a stage's map group, and for the same reason -- only the decomp says
    that the word at 0x50 is an `ItemAttributes` called Capsule."""
    want = "[%d]" % fid
    out, inside = [], False
    with open(DESC) as fp:
        for line in fp:
            line = line.strip()
            if line.startswith("["):
                inside = (line == want)
                continue
            if not inside or not line:
                continue
            parts = line.split()
            out.append((parts[0], parts[1], int(parts[2], 16)))
    if not out:
        raise AssertionError("no descriptions group [%d]" % fid)
    return out


def parse_item_attributes(fid=ITCOMMONDATA, path=None):
    """Every `ITAttributes` table the decomp writes for this file, field by
    field, in the order `it/ittypes.h` declares them.

    The pack must NOT carry the ROM's bytes for these, and that is the
    whole reason this parses C instead. `ITAttributes` opens with four
    POINTERS -- 16 bytes on the N64, 32 on a 64-bit host -- so a verbatim
    copy read as a host struct puts the entire bitfield run (flags, attack
    offsets, hurtbox, map coll, hitbox params, every field an item reads)
    sixteen bytes out. This is the third time this port has met that class
    of bug (`union AObjEvent32`'s eight bytes, `MPYakumonoDObj`'s
    flexible-array member) and the port's answer is always the same: ship
    a parsed structure laid out for the machine that reads it, the way
    `ssb_packexport.py` ships `FPackAttr` rather than `FTAttributes`.

    The pointers come out as the SYMBOL the initializer names
    (`dITCommonObject_Capsule_Item_data_DObjDesc`), which is a block of
    file 86 and becomes a region-1 offset; NULL stays None."""
    named = path is not None
    path = path or os.path.join(DECOMP, "src", "relocData",
                                "%d_ITCommonData.c" % fid)
    if not os.path.isfile(path):
        raise AssertionError("no typed initializer at %s" % path)
    text = open(path).read()

    # These tables are REGION-SPLIT: `#if defined(REGION_JP) ... #else ...
    # #endif` inside the initializer, both arms present in the text. The
    # port is REGION_US (the Makefile's -DREGION_US), so the JP arm has to
    # go before anything is counted -- Kamex carries five fields from one
    # and five from the other, and a parser that takes both reads 52
    # fields where the struct has 47. This is the same trap
    # dGRPupupuMap_item_weights set in, where the first draft
    # shipped the JP table.
    text = _take_region_arm(text, path, split_expected=not named)

    out = []
    for m in re.finditer(r'ITAttributes\s+(\w+)\[1\]\s*=\s*\{\{(.*?)\}\};',
                         text, re.S):
        name, body = m.group(1), m.group(2)

        out.append((name, _split_fields(body)))
    if not out:
        raise AssertionError("no ITAttributes tables parsed from %s" % path)
    return out


def _split_fields(body):
    """An initializer body as its top-level fields, comments stripped. A
    nested initializer (`{ 0, 0, 0 }`) is ONE field, which is what the
    brace-depth count below is for."""
    body = re.sub(r'/\*.*?\*/', '', body, flags=re.S)
    body = re.sub(r'//[^\n]*', '', body)
    vals, depth, cur = [], 0, ''
    for ch in body:
        if ch == '{':
            depth += 1
        elif ch == '}':
            depth -= 1
        if ch == ',' and depth == 0:
            vals.append(cur.strip()); cur = ''
        else:
            cur += ch
    if cur.strip():
        vals.append(cur.strip())
    return vals


def _take_region_arm(text, path, split_expected=True):
    """The REGION_US arm of a typed initializer. See parse_item_attributes
    for the whole argument: the tables are region-split, both arms are in
    the text, the port is REGION_US, and a parser that counts both reads
    more fields than the struct has.

    `split_expected` is the tripwire for a SILENT mis-parse. Every
    ITCommonData file is region-split, so finding no split there means
    the regex stopped matching -- the initializer moved or was reformatted
    -- and the US arm was never taken. A stage's map file is a different
    animal: most are split (276_GRBonus1LinkMap.c, 255_GRPupupuMap.c and
    a dozen more), but 295_GRBonus3Map.c genuinely has no `#if` at all,
    so the call sites that NAME a file pass False. They are not left
    unguarded: a guard this function fails to resolve -- a renamed macro,
    a nested arm, an `#ifdef` where an `#if defined` was -- leaves an
    `#if` behind, and the first check below catches exactly that. What
    False gives up is only the case where a file's every conditional
    vanished, which the caller then meets as a missing symbol."""
    before = text.count("#if defined(REGION_JP)")
    text = re.sub(r'#if defined\(REGION_JP\).*?#else(.*?)#endif',
                  lambda m: m.group(1), text, flags=re.S)
    if text.count("#if") != 0:
        raise AssertionError("%s still has %d preprocessor conditional(s) "
                             "after the REGION_JP arm was taken"
                             % (path, text.count("#if")))
    if split_expected and before == 0:
        raise AssertionError("%s has no REGION_JP split -- has the "
                             "initializer moved?" % path)
    return text


# The region-0 record: one per ItemAttributes table, in HOST layout --
# four region-1 offsets (or IT_ATTR_NO_PTR), then the scalars the
# decomp's initializer writes, in its own order. The port mirrors this
# struct with a size assert; nothing here is ever read as the N64's
# ITAttributes, which is the point.
IT_ATTR_NO_PTR = 0xFFFFFFFF
IT_ATTR_SIZE = 88                # 81 bytes of fields, pad to 84, then the key
IT_ATTR_KEY = 84                 # the block's own offset in region 0

# Regions a block's offset can be relative to. Region 2 holds the
# PARSED ItemAttributes records, because those cannot be read out of
# region 0's raw bytes (see parse_item_attributes).
REGION_DATA = 0
REGION_MODELS = 1
REGION_ATTRS = 2


# ---- the MONSTER EVENTS ------------------------------
#
# `ITMonsterEvent` is what Porygon's and Venusaur's own update reads to
# move their hitbox through a scripted sequence: an array indexed by
# `ip->event_id`, each entry a timer plus the hitbox numbers to install
# when the timer matches. The port cannot carry the ROM's bytes for it,
# for the same reason it cannot carry ITAttributes: the struct is
# BITFIELDS (`s32 angle : 10; u32 damage : 8; ub32 can_setoff : 1`), and
# IDO and GCC do not have to lay those out the same way -- which is the
# class of bug this file has already met three times.
#
# So they are parsed from the decomp's typed initializer and written as
# PLAIN HOST FIELDS, one record per event, keyed the way the attribute
# records are: by the `o_attributes` of the item whose table it is.
ITEM_EVENT_SIZE = 48
ITEM_EVENT_KEY = 44             # the o_attributes this table belongs to


def parse_monster_events(paths):
    """[(o_attributes, [event, ...]), ...] from the stage items' typed
    initializers.

    The values are the decomp's own, in the struct's field order: timer,
    angle, damage, size, knockback_scale, knockback_weight,
    knockback_base, element, can_setoff, shield_damage, fgm_id."""
    out = []

    for (item, off, path, sym) in paths:
        text = open(path).read()
        text = re.sub(r'#if defined\(REGION_JP\).*?#else(.*?)#endif',
                      lambda m: m.group(1), text, flags=re.S)
        m = re.search(r'ITMonsterEvent\s+%s\s*\[[^\]]*\]\s*=\s*\{(.*?)\n\};'
                      % re.escape(sym), text, re.S)
        if m is None:
            raise AssertionError("%s: no ITMonsterEvent table %s"
                                 % (path, sym))
        body = re.sub(r'/\*.*?\*/', '', m.group(1), flags=re.S)
        body = re.sub(r'//[^\n]*', '', body)
        events = []
        for row in re.finditer(r'\{([^{}]*)\}', body):
            vals = [_int(v) for v in row.group(1).split(',') if v.strip()]
            if len(vals) != 11:
                raise AssertionError("%s: an event with %d fields, want 11"
                                     % (sym, len(vals)))
            events.append(vals)
        if not events:
            raise AssertionError("%s: no events in %s" % (sym, path))
        out.append((off, events))
    return out


# The stage items whose own update reads a `HitParties` table, and the
# offset their ITDesc carries. See STAGE_ITEMS for the attributes.
STAGE_EVENTS = [
    ("Porygon", 0x16C, "264_GRYamabukiMap.c",
     "dGRYamabukiMap_Porygon_HitParties"),
    ("Fushigibana", 0x278, "264_GRYamabukiMap.c",
     "dGRYamabukiMap_Fushigibana_HitParties"),
]


# ... and the ATTACK EVENTS. `ITAttackEvent` is the same
# idea one size down -- a four-field hitbox script for explosions, read by
# `itGetAttackEvent(desc, off)` -- and it is BITFIELDS at its head too
# (`s32 angle : 10; u32 damage : 8`), so it is parsed and shipped the same
# way. Same key as the attributes: the item's `o_attributes`.
ITEM_AEVENT_SIZE = 20           # timer, angle, damage, size, then the key
ITEM_AEVENT_KEY = 16


def parse_attack_events(paths):
    """[(o_attributes, [event, ...]), ...] from the stage items' typed
    initializers. The values are the decomp's own, in the struct's field
    order: timer, angle, damage, size."""
    out = []

    for (item, off, path, sym) in paths:
        text = open(path).read()
        text = re.sub(r'#if defined\(REGION_JP\).*?#else(.*?)#endif',
                      lambda m: m.group(1), text, flags=re.S)
        m = re.search(r'ITAttackEvent\s+%s\s*\[[^\]]*\]\s*=\s*\{(.*?)\n\};'
                      % re.escape(sym), text, re.S)
        if m is None:
            raise AssertionError("%s: no ITAttackEvent table %s"
                                 % (path, sym))
        body = re.sub(r'/\*.*?\*/', '', m.group(1), flags=re.S)
        body = re.sub(r'//[^\n]*', '', body)
        events = []
        for row in re.finditer(r'\{([^{}]*)\}', body):
            vals = [_int(v) for v in row.group(1).split(',') if v.strip()]
            if len(vals) != 4:
                raise AssertionError("%s: an attack event with %d fields, "
                                     "want 4" % (sym, len(vals)))
            events.append(vals)
        if not events:
            raise AssertionError("%s: no events in %s" % (sym, path))
        out.append((off, events))
    return out


# The stage items whose explosion reads an `AttackEvents` table, and the
# offset their ITDesc carries. See STAGE_ITEMS for the attributes.
STAGE_ATTACK_EVENTS = [
    ("Marumine", 0x104, "264_GRYamabukiMap.c",
     "dGRYamabukiMap_Marumine_AttackEvents"),
    # itTaruBombExplodeInitVars reads four of these, one per explosion
    # frame, the way Voltorb's own does.
    ("TaruBomb", 0xA8, "295_GRBonus3Map.c",
     "dGRBonus3Map_TaruBomb_AttackEvents"),
]


# ---- the WEAPON ATTRIBUTES ---------------------------
#
# The last of the three tables the stage's own items read out of the map
# file: `WPAttributes`, which Saffron City's Charmander and Venusaur hand
# to `wpManagerMakeWeapon` for the flames and the razors they spit.
#
# It carries the same hazard as the other two -- it opens with four
# POINTERS (16 bytes on the N64 and on the SH4, 32 on a 64-bit host) and
# its scalar run is BITFIELDS (`u32 size : 16; s32 angle : 10; u32
# damage : 8`) -- and the same obstacle: the port has no stage map file
# for `p_weapon` to point at. So it is parsed from the decomp's typed
# initializer into HOST FIELDS, keyed by the `o_attributes` the WPDesc
# carries, and materialised back into a `WPAttributes` at load; the one
# reader is `itemPackWeaponAttr`, which `wpManagerMakeWeapon` consults
# before falling back to the byte overlay fighter weapons use.
#
# (That overlay is CORRECT for a fighter's own weapon and is left alone:
# four pointers are 16 bytes on both machines, which was
# measured rather than assumed.)
WATTR_NO_PTR = 0xFFFFFFFF
WATTR_SIZE = 64
WATTR_KEY = 60                  # the WPDesc's o_attributes

# The 29 initializer fields WPAttributes' declaration names, in order --
# the four pointers first, then attack_offsets[2] as one nested field.
WATTR_FIELDS = 29


def parse_weapon_attributes(paths):
    """[(o_attributes, [field, ...]), ...] from the stage weapons' typed
    initializers, in `wp/wptypes.h`'s own field order."""
    out = []

    for (item, off, path, sym) in paths:
        text = _take_region_arm(open(path).read(), path,
                                split_expected=False)
        m = re.search(r'WPAttributes\s+%s\s*=\s*\{(.*?)\n\};'
                      % re.escape(sym), text, re.S)
        if m is None:
            raise AssertionError("%s: no WPAttributes table %s"
                                 % (path, sym))
        vals = _split_fields(m.group(1))
        if len(vals) != WATTR_FIELDS:
            raise AssertionError("%s: %s has %d fields, want %d"
                                 % (path, sym, len(vals), WATTR_FIELDS))
        out.append((off, vals))
    return out


# The stage weapons whose WPDesc names a stage-file attributes table.
STAGE_WEAPONS = [
    ("Hitokage", 0x244, "264_GRYamabukiMap.c",
     "dGRYamabukiMap_HitokageFlame_WeaponAttributes"),
    ("Fushigibana", 0x308, "264_GRYamabukiMap.c",
     "dGRYamabukiMap_FushigibanaRazor_WeaponAttributes"),
    # Sector Z's two. Their `p_weapon` is the sector's own
    # `weapon_head` -- a SECOND stage head beside `item_head`, and both are
    # the same arithmetic off gMPCollisionGroundData -- so they go in the
    # same place the Gate's weapons do. Both name the SAME display list for
    # their `data` (file 153's 0x1C50): the 3D laser differs only in its
    # matrix.
    ("Sector", 0xBC, "262_GRSectorMap.c",
     "dGRSectorMap_ArwingLaser2D_WeaponAttributes"),
    ("Sector", 0xF0, "262_GRSectorMap.c",
     "dGRSectorMap_ArwingLaser3D_WeaponAttributes"),
]


def encode_weapon_attr(vals, ptrs, off):
    """One parsed WPAttributes as the WATTR_SIZE-byte record the port
    reads, in the order `item_pack_setup_wattr` walks."""
    def ptr(i):
        v = ptrs[i]
        return WATTR_NO_PTR if v is None else v

    def flat(i):
        # { { a, b, c }, { d, e, f } } -- attack_offsets[2]
        return [_int(x) for row in re.findall(r'\{([^{}]*)\}', vals[i])
                for x in row.split(',')]

    rec = struct.pack("<4I", ptr(0), ptr(1), ptr(2), ptr(3))
    rec += struct.pack("<6h", *flat(4))
    rec += struct.pack("<4h", *[_int(vals[i]) for i in (5, 6, 7, 8)])
    rec += struct.pack("<H", _int(vals[9]))          # size
    rec += struct.pack("<h", _int(vals[10]))         # angle
    rec += struct.pack("<H", _int(vals[11]))         # knockback_scale
    rec += struct.pack("<2B", _int(vals[12]), _int(vals[13]))   # damage, element
    rec += struct.pack("<H", _int(vals[14]))         # knockback_weight
    # shield_damage is `s32 : 8` and is really negative in one table (a
    # fighter's: Yoshi's Star, -3), so it is a signed byte; the reader
    # (src/dc/wpattrs.c wpAttrsSetupRecord) narrows it the same way.
    rec += struct.pack("<bB", _int(vals[15]), _int(vals[16]))   # shield_damage,
                                                     # attack_count
    # 17, 20..27: setoff, rehit_item, rehit_fighter, hop, reflect, absorb,
    # shield, then the two unused bits -- one flag word
    flags = 0
    for b, i in enumerate((17, 20, 21, 22, 23, 24, 25, 26, 27)):
        if _int(vals[i]):
            flags |= 1 << b
    rec += struct.pack("<H", flags)
    rec += struct.pack("<H", _int(vals[18]))         # sfx
    rec += struct.pack("<B", _int(vals[19]))         # priority
    rec += struct.pack("<H", _int(vals[28]))         # knockback_base
    if len(rec) > WATTR_KEY:
        raise AssertionError("weapon record is %d bytes of fields, more "
                             "than %d" % (len(rec), WATTR_KEY))
    # the fields come to 55 bytes; pad to 60 so the record is even and the
    # key is at a fixed offset. The port walks these with memcpy, not a
    # struct cast -- mixed u16/u8 fields would pick up compiler padding
    # the writer knows nothing about, the same reason ITItemAttr is read
    # field by field.
    rec += b"\0" * (WATTR_KEY - len(rec))
    return rec + struct.pack("<I", off)


_FGM_NAMES = None


def _fgm_names():
    global _FGM_NAMES
    if _FGM_NAMES is None:
        import ssb_fgmexport
        _FGM_NAMES = ssb_fgmexport.fgm_names()
    return _FGM_NAMES


def _int(tok):
    """One initializer token as an integer. Symbols are handled by the
    caller; this is for the literals the decomp writes."""
    tok = tok.strip()
    if tok.startswith("nSYAudioFGM") or tok.startswith("nSYAudioVoice"):
        # An FGM id, by the value gmsound.h's enum gives it. This returned
        # 0 until the G05 sweep found it, and 0 is nSYAudioFGMExplodeS: every
        # item that names its sounds -- 140 of ITCommonData's fields, the
        # stage items' and PK Fire's -- hit, dropped and was thrown with
        # the small explosion.
        names = _fgm_names()
        if tok not in names:
            raise AssertionError("no FGM id named %r in gmsound.h" % tok)
        return names[tok]
    m = re.match(r'^(-?0x[0-9A-Fa-f]+|-?\d+)$', tok)
    if not m:
        raise AssertionError("unparsed initializer token %r" % tok)
    return int(tok, 0)


# `ITAttributes` as it/ittypes.h declares it, for reading one out of the
# ROM's bytes where the decomp has no typed initializer (Link's Bomb, in
# LinkMain's untyped head). (name, C type, bit width or
# None). Checked against ittypes.h itself by item_attr_layout().
IT_ATTR_FIELDS = None


def item_attr_layout():
    """[(field, bit offset, bit width, signed)] for the N64's ITAttributes:
    big-endian, a bitfield taking the next bits of its 32-bit unit unless
    it would cross into the next unit, a plain field aligned to its own
    size from the next whole byte -- the MIPS rule IDO shares with GCC.
    Vec3h is three s16s."""
    global IT_ATTR_FIELDS
    if IT_ATTR_FIELDS is not None:
        return IT_ATTR_FIELDS
    text = open(os.path.join(DECOMP, "src", "it", "ittypes.h")).read()
    body = re.search(r"struct ITAttributes\s*\{(.*?)\n\};", text, re.S).group(1)
    body = re.sub(r"//[^\n]*|/\*.*?\*/", "", body, flags=re.S)
    out, bit = [], 0
    for decl in (d.strip() for d in body.split(";")):
        if not decl:
            continue
        m = re.match(r"^([\w ]+?)\s*(\**)\s*(\w+)\s*(?::\s*(\d+))?$", decl)
        if not m:
            raise AssertionError("ITAttributes: unparsed field %r" % decl)
        ctype, stars, name, width = m.group(1), m.group(2), m.group(3), m.group(4)
        if width is not None:
            width = int(width)
            if ctype not in ("ub32", "s32", "u32"):
                raise AssertionError("ITAttributes: bitfield %s of %s" % (name, ctype))
            if (bit % 32) + width > 32:
                bit = (bit + 31) & ~31
            out.append((name, bit, width, ctype == "s32"))
            bit += width
            continue
        size = {"s16": 16, "u16": 16, "Vec3h": 16}.get(ctype, 32 if stars else None)
        if size is None:
            raise AssertionError("ITAttributes: plain field %s of %s" % (name, ctype))
        bit = (bit + size - 1) & ~(size - 1)
        count = 3 if ctype == "Vec3h" else 1
        for k in range(count):
            out.append((name if count == 1 else "%s[%d]" % (name, k), bit, size,
                        ctype in ("s16", "Vec3h")))
            bit += size
    if bit != 0x48 * 8:
        raise AssertionError("ITAttributes lays out to %d bits, not the "
                             "N64's 0x48 bytes" % bit)
    IT_ATTR_FIELDS = out
    return out


def decode_item_attr(raw):
    """One ITAttributes out of the N64's 0x48 bytes, as the 47 initializer
    tokens parse_item_attributes() would give -- so encode_item_attr takes
    either. Pointers come out as "NULL" or "ptr"; the caller supplies what
    they name."""
    if len(raw) != 0x48:
        raise AssertionError("ITAttributes is 0x48 bytes, got 0x%X" % len(raw))
    word = int.from_bytes(raw, "big")
    vals, vec = [], []
    for (name, bit, width, signed) in item_attr_layout():
        v = (word >> (0x48 * 8 - bit - width)) & ((1 << width) - 1)
        if signed and v & (1 << (width - 1)):
            v -= 1 << width
        if width == 32 and len(vals) < 4:
            vals.append("NULL" if v == 0 else "ptr")
        elif "[" in name:
            vec.append(str(v))
            if len(vec) == 3:
                vals.append("{ %s }" % ", ".join(vec))
                vec = []
        else:
            vals.append(str(v))
    if len(vals) != 47:
        raise AssertionError("decoded %d fields, want 47" % len(vals))
    return vals


def cross_check_attr(name, vals, block_off, fixes):
    """The decomp's table against the ROM's own relocations.

    Two independent readings of the same four pointers: the decomp's
    typed initializer says NULL or names a symbol, and the ROM has a
    relocation word at that site or does not. They must agree field by
    field. That is the only place the parse can be checked against the
    ROM rather than against itself.

    It deliberately checks PRESENCE and not the target offset. A symbol
    like dITCommonObject_Capsule_Item_data_DObjDesc names a place inside
    a model block rather than a block start -- 43 of the 68 fixups are
    interior -- so there is no block name to compare a target against,
    and inventing a symbol-to-offset map is exactly the work the fixup
    table spares this file.
    """
    for i in range(4):
        site = block_off + 4 * i
        tok = re.sub(r'^\(\s*\w+\s*\*\s*\)\s*', '', vals[i].strip())
        has_fixup = site in fixes
        if (tok == "NULL") == has_fixup:
            raise AssertionError(
                "%s: pointer %d is %s, and the ROM %s a relocation there"
                % (name, i, "NULL" if tok == "NULL" else tok,
                   "has" if has_fixup else "has no"))
    return True


def encode_item_attr(vals, ptrs, off):
    """One parsed table as the 88-byte record the port reads.

    `ptrs` is the table's four pointer fields, in order, as region-1
    offsets or None -- and they come from the FIXUP TABLE, not from the
    decomp's symbols. The fixup is the ROM's own relocation word at that
    site, so it is the authority; the decomp's symbol is checked against
    it (cross_check_attr) rather than trusted for the value. That also
    spares this file a symbol-to-block-name mapping it would otherwise
    have to invent, since the descriptions name file 86's blocks and the
    decomp names its symbols.

    `off` is the table's own offset in region 0, and it is appended as the
    record's last word. That is the key the port's lookup uses, and it is
    the same number the decomp names by `&llITCommonData<Name>ItemAttributes`
    -- which is what an ITDesc now carries, so a table can be found from
    the descriptor alone.
    """
    if len(vals) != 47 or len(ptrs) != 4:
        raise AssertionError("expected 47 fields, got %d" % len(vals))

    def ptr(i):
        v = ptrs[i]
        return IT_ATTR_NO_PTR if v is None else v

    def flat(i):
        # { a, b, c }
        return [_int(x) for x in vals[i].strip("{} ").split(",")]

    rec = struct.pack("<4I", ptr(0), ptr(1), ptr(2), ptr(3))
    # 4..8: xlu, dobjs, colanim, hitlag, weight
    flags = 0
    for b in range(5):
        if _int(vals[4 + b]):
            flags |= 1 << b
    rec += struct.pack("<H", flags)
    rec += struct.pack("<3h", *[_int(v) for v in (vals[9], vals[10], vals[11])])
    rec += struct.pack("<3h", *[_int(v) for v in (vals[12], vals[13], vals[14])])
    rec += struct.pack("<3h", *flat(15))
    rec += struct.pack("<3h", *flat(16))
    rec += struct.pack("<4h", *[_int(vals[i]) for i in (17, 18, 19, 20)])
    rec += struct.pack("<H", _int(vals[21]))
    rec += struct.pack("<h", _int(vals[22]))
    rec += struct.pack("<H", _int(vals[23]))
    rec += struct.pack("<2B", _int(vals[24]), _int(vals[25]))
    rec += struct.pack("<H", _int(vals[26]))
    rec += struct.pack("<2B", _int(vals[27]), _int(vals[28]))
    # 29, 32, 33, 34, 35, 36: setoff, rehit_item, rehit_fighter, hop,
    # reflect, shield -- the second flag run
    flags2 = 0
    for b, i in enumerate((29, 32, 33, 34, 35, 36)):
        if _int(vals[i]):
            flags2 |= 1 << b
    rec += struct.pack("<H", flags2)
    rec += struct.pack("<H", _int(vals[30]))
    rec += struct.pack("<B", _int(vals[31]))
    rec += struct.pack("<H", _int(vals[37]))
    rec += struct.pack("<2B", _int(vals[38]), _int(vals[39]))
    rec += struct.pack("<3H", *[_int(vals[i]) for i in (42, 43, 44)])
    rec += struct.pack("<2H", _int(vals[45]), _int(vals[46]))
    # The fields come to 81 bytes and the record is padded to 84, so the
    # table is word-aligned and the port can walk it by index. The port
    # does NOT cast this to a struct -- mixed u8/u16 fields would pick up
    # compiler padding that the writer knows nothing about -- it reads the
    # fields out with memcpy, the way it reads every other pack header.
    if len(rec) != IT_ATTR_KEY - 3:
        raise AssertionError("record is %d bytes of fields, not %d"
                             % (len(rec), IT_ATTR_KEY - 3))
    return rec + b"\0\0\0" + struct.pack("<I", off)


# The STAGE items. Seven of dITManagerProcMakeList's ten
# "stage items" -- kinds 22..31 -- have their `ItemAttributes` in the
# STAGE's own map file rather than in ITCommonData: `ItemAttributes
# PowerBlock` is Mushroom Kingdom's (0xD8) and Saffron City's five are
# its own. Their ITDescs name the stage's field for the base --
# `&gGRCommonStruct.inishie.item_head` -- which the game points at the
# map file (grinishie.c:568).
#
# The port carries them HERE, keyed by the offsets the decomp's descs
# already carry, because `itemPackAttr` (src/dc/itempack.c) is the
# function that answers for an item's attributes and it answers for
# exactly one file. So each becomes one more region-2 record, and each
# desc's `p_file` is the one line that diverges.
#
# THE POINTERS ARE NOT CARRIED. Their targets are in the stage MODEL
# file -- PowerBlock's `data` is StageInishieFile3's DObjDesc at 0x11F8
# and its `anim_joints` the array at 0x13B0 -- so a faithful copy would
# be a WINDOW of that file appended to region 1 with its own fixups,
# carrying the offsets along so `itGetPData(ip, &DataStart, &AnimJoint)`
# still lands. That is real work and it buys exactly one thing: the two
# script attaches inside the item's own body. The port takes the other
# road -- the scripts are named blocks in the stage pack
# (`stage_anims`, tools/export/ssb_stageexport.py) and the item asks for them
# by name -- so `data` is a non-NULL marker (region 1's base, which is
# all `itManagerMakeItem`'s model-path test reads) and the rest are
# NO_PTR.
#
# (name, the stage's map file, the typed initializer holding the table,
# the table's C symbol, the offset the decomp's ITDesc carries.)
STAGE_ITEMS = [
    ("PowerBlock", 260, "260_GRInishieMap.c",
     "dGRInishieMap_PowerBlock_ItemAttributes", 0xD8),
    ("Pakkun", 260, "260_GRInishieMap.c",
     "dGRInishieMap_Pakkun_ItemAttributes", 0x120),
    ("GLucky", 264, "264_GRYamabukiMap.c",
     "dGRYamabukiMap_GLucky_ItemAttributes", 0xBC),
    ("Porygon", 264, "264_GRYamabukiMap.c",
     "dGRYamabukiMap_Porygon_ItemAttributes", 0x16C),
    ("Marumine", 264, "264_GRYamabukiMap.c",
     "dGRYamabukiMap_Marumine_ItemAttributes", 0x104),
    ("Hitokage", 264, "264_GRYamabukiMap.c",
     "dGRYamabukiMap_Hitokage_ItemAttributes", 0x1FC),
    ("Fushigibana", 264, "264_GRYamabukiMap.c",
     "dGRYamabukiMap_Fushigibana_ItemAttributes", 0x278),
    # Race to the Finish's barrel bomb. Same shape as the
    # seven above -- its table is in its own stage's map file and its
    # ITDesc carries the offset -- and the only one of the eight whose
    # stage is a bonus stage.
    ("TaruBomb", 295, "295_GRBonus3Map.c",
     "dGRBonus3Map_TaruBomb_ItemAttributes", 0xA8),
    # Break the Targets' target, the ninth and the odd one
    # out: its table is not in a STAGE's map file at all. ITBonus1Object
    # Header (relocData 253) IS the table -- one ITAttributes and nothing
    # else -- which is why the decomp's ITDesc carries offset 0 and names
    # `&gSC1PBonusStageItemFile`, the pointer sc1PBonusStageBonus1LoadFile
    # fills. The port diverges that p_file to &gITManagerCommonData the
    # way every other row here does and keeps the decomp's own 0, which
    # is free: ITCommonData's own lowest table is 0x50.
    #
    # `mfid` is the file the table is in rather than a map file, and is
    # documentation here the same way the others' are -- the loop below
    # reads the typed initializer by path and the key, and nothing else.
    ("Target", 253, "253_ITBonus1ObjectHeader.c",
     "dITBonus1ObjectHeader", 0x0),
]

# And the FIGHTER-owned items: PK Fire's pillar, whose table
# sits at 0x34 in NessSpecial1 beside the spark's WPAttributes. Its ITDesc
# is compiled unmodified -- it names &gFTNessFileSpecial1 -- so no p_file
# can diverge; src/dc/itempack.c's itemPackAttr answers for that file
# instead. The four pointers are not carried either, for the stage
# items' reason: they point into NessSpecial3, which this pack does not
# carry. `data` is the same non-NULL marker theirs is, so itManagerMakeItem
# takes the model path and src/dc/itemmodel.c answers for key 0x34 with
# itnesspkfire.mdl (tools/export/ssb_itemmodelexport.py FIGHTER_ITEMS). The
# other three are NO_PTR; the pillar's AnimJoint table rides in that .mdl
# and itemModelAnimJoints hands it to itManagerMakeItem's gcAddAnimAll,
# since itnesspkfire.c is compiled unmodified and has no MakeItem of the
# port's own to attach a named script in.
#
# Link's Bomb is the second, at 0x40 in LinkMain, and the
# first table here with NO initializer: LinkMain's head is one untyped u32
# array in the decomp (225_LinkMain.c's dLinkMain_file_handles), so its
# row names no source and the table is read out of the ROM's bytes through
# decode_item_attr -- the decoder every ITCommonData table, and PK Fire's
# below, is held to. Its `data` is the marker for the same reason the
# pillar's is (the tree is relocData 353's, itlinkbomb.mdl).
#
# (item, the file the table is in, its typed initializer's file and symbol
# or None, the offset its ITDesc carries.)
FIGHTER_ITEMS = [
    ("PKFire", 240, "240_NessSpecial1.c",
     "dNessSpecial1_PKFire_ItemAttributes", 0x34),
    ("LinkBomb", 225, None, None, 0x40),
]


def pack_items(rom, dump=False):
    freed, _, _, sites, ids = L.file_info(rom, ITCOMMONDATA)
    fobj, _, intern, osites, oids = L.file_info(rom, ITCOMMONOBJECT)

    if osites:
        # The model file must point at nothing but itself: a fixup target
        # has to be an offset this pack can name, and a reloc into a THIRD
        # file is one this format has no way to carry. Internal relocs are
        # expected -- 402 of them -- and are what the model region's own
        # fixups will be built from.
        raise AssertionError("ITCommonObject has %d extern site(s) into %s"
                             % (len(osites), sorted(set(oids))))
    if any(i != ITCOMMONOBJECT for i in ids):
        raise AssertionError("ITCommonData points at files other than "
                             "ITCommonObject: %s" % sorted(set(ids)))

    # The descriptions' group is the LOGICAL item file, not one ROM file:
    # a block's offset is into whichever of the two it lives in, and the
    # two are told apart by the range. 55 blocks -- every ItemAttributes,
    # WeaponAttributes, AttackEvents, VelocitiesY and Angles -- sit inside
    # ITCommonData, and the other 48 are the model vocabulary (DataStart,
    # DisplayList, DObjDesc, AnimJoint, MatAnimJoint, MObjSub) inside
    # ITCommonObject. So each entry is (name, region, offset) and the
    # loader resolves a name against the region it names.
    blocks = []
    for (kind, name, off) in read_blocks(ITCOMMONDATA):
        if off < len(freed):
            region = 0
        elif off < len(fobj):
            region = 1
        else:
            raise AssertionError("block %s %s at 0x%X is in neither file "
                                 "(%d, %d)" % (kind, name, off, len(freed),
                                               len(fobj)))
        blocks.append((kind, name, off, region))

    # Every extern word in ITCommonData is (site, target): the offset of the
    # reloc word within ITCommonData, and the offset in ITCommonObject it
    # names. That pair IS the fixup.
    fixes = sorted(sites)
    fixmap = dict(fixes)     # the same pairs, for site lookups
    for (site, target) in fixes:
        if target >= len(fobj):
            raise AssertionError("fixup at 0x%X names 0x%X, outside "
                                 "ITCommonObject" % (site, target))

    # The oracle. A pointer in an ItemAttributes table that aimed at
    # nothing would still export and would fail at run time as a DObjDesc
    # read out of the middle of another item's, so every fixup is checked
    # against the model file it must land in -- and must be word-aligned,
    # because everything it can name is.
    #
    # It is deliberately NOT "must be a block the descriptions list": the
    # first ItemAttributes in the file (0x50) points at 0x670, which is
    # inside a model block and not at the start of one. Interior pointers
    # are normal here -- a display list names its own sub-lists -- so the
    # block-start count is reported rather than required.
    model_offs = set(off for (k, n, off, r) in blocks if r == 1)
    on_block = 0
    for (site, target) in fixes:
        if target & 3:
            raise AssertionError("fixup at 0x%X names unaligned 0x%X"
                                 % (site, target))
        if target in model_offs:
            on_block += 1

    body = bytearray()
    body += freed
    while len(body) % 4:
        body += b"\0"
    off_models = HEADER + len(body)
    body += fobj
    while len(body) % 4:
        body += b"\0"

    # Region 2: one parsed record per ItemAttributes block, in the order
    # the blocks appear, so a block's region-2 offset is its index times
    # the record size. These supersede those blocks' raw bytes in region
    # 0 -- which stay, because the region still serves the blocks that
    # ARE read as bytes (VelocitiesY Container is pointer-free s16 rows).
    tables = dict(parse_item_attributes())
    attrs = []
    for (kind, name, off, region) in blocks:
        if kind != "ItemAttributes":
            continue
        sym = "dITCommonData_%s_ItemAttributes" % name
        if sym not in tables:
            raise AssertionError("no typed table for ItemAttributes %s" % name)
        cross_check_attr(sym, tables[sym], off, fixmap)
        # and every field the other way: the ROM's own 0x48 bytes, read
        # through ittypes.h's layout, must encode to the same record. This
        # is what caught the FGM names encoding as 0, and what makes
        # decode_item_attr trustworthy for tables with no initializer.
        if (encode_item_attr(tables[sym], [None] * 4, off) !=
                encode_item_attr(decode_item_attr(bytes(freed[off:off + 0x48])),
                                 [None] * 4, off)):
            raise AssertionError("%s: the ROM's bytes at 0x%X decode to "
                                 "other values than the initializer's"
                                 % (sym, off))
        ptrs = [fixmap.get(off + 4 * i) for i in range(4)]
        attrs.append(encode_item_attr(tables[sym], ptrs, off))

    # ... and the STAGE items', from their own files. No
    # cross_check_attr: it compares a table's four pointers against the
    # ROM's relocation words at that site, both of which live in
    # ITCommonData, and these tables' sites are in a map file this
    # function never opens. What CAN be checked is the one thing the
    # parse could silently get wrong -- that the decomp's initializer
    # names a non-NULL `data`, which is the field the marker stands in
    # for -- so that is what is checked.
    for (item, mfid, src, sym, key) in STAGE_ITEMS:
        staged = dict(parse_item_attributes(
            path=os.path.join(DECOMP, "src", "relocData", src)))
        if sym not in staged:
            raise AssertionError("%s: no typed table %s in %s"
                                 % (item, sym, src))
        if staged[sym][0].strip() == "NULL":
            raise AssertionError("%s: the decomp's table names no data "
                                 "block, but the port writes a marker"
                                 % item)
        if key in dict((b[2], b[1]) for b in blocks if b[0] ==
                       "ItemAttributes"):
            raise AssertionError("%s: key 0x%X is already an "
                                 "ITCommonData table's" % (item, key))
        ptrs = [0, None, None, None]      # region 1's base; see above
        attrs.append(encode_item_attr(staged[sym], ptrs, key))
        blocks.append(("ItemAttributes", item, key, REGION_ATTRS))
    for (item, fid, src, sym, key) in FIGHTER_ITEMS:
        ffile = bytes(L.file_info(rom, fid)[0])
        vals = decode_item_attr(ffile[key:key + 0x48])
        if src is not None:
            staged = dict(parse_item_attributes(
                path=os.path.join(DECOMP, "src", "relocData", src)))
            if sym not in staged:
                raise AssertionError("%s: no typed table %s in %s"
                                     % (item, sym, src))
            if (encode_item_attr(staged[sym], [None] * 4, key) !=
                    encode_item_attr(vals, [None] * 4, key)):
                raise AssertionError("%s: file %d's bytes at 0x%X decode to "
                                     "other values than %s" % (item, fid, key, sym))
            vals = staged[sym]
        if key in dict((b[2], b[1]) for b in blocks if b[0] ==
                       "ItemAttributes"):
            raise AssertionError("%s: key 0x%X is already another "
                                 "table's" % (item, key))
        if vals[0].strip() == "NULL":
            raise AssertionError("%s: the table names no data block, but "
                                 "the port writes a marker" % item)
        ptrs = [0, None, None, None]      # the stage items' marker
        attrs.append(encode_item_attr(vals, ptrs, key))
        blocks.append(("ItemAttributes", item, key, REGION_ATTRS))
    off_attrs = HEADER + len(body)
    body += b"".join(attrs)

    # ---- the MONSTER EVENTS: one 48-byte record per event,
    # keyed by the o_attributes of the item whose table it is. See
    # parse_monster_events for why they are not carried as bytes.
    events = []

    for (off, evs) in parse_monster_events(
            [(item, off, os.path.join(DECOMP, "src", "relocData", src), sym)
             for (item, off, src, sym) in STAGE_EVENTS]):
        for v in evs:
            events.append(struct.pack("<11I", *v) + struct.pack("<I", off))
    off_events = HEADER + len(body)
    body += b"".join(events)

    # ---- and the ATTACK EVENTS, the same shape one size
    # down: a four-field hitbox script for an explosion. See
    # parse_attack_events.
    aevents = []

    for (off, evs) in parse_attack_events(
            [(item, off, os.path.join(DECOMP, "src", "relocData", src), sym)
             for (item, off, src, sym) in STAGE_ATTACK_EVENTS]):
        for v in evs:
            aevents.append(struct.pack("<4I", *v) + struct.pack("<I", off))
    off_aevents = HEADER + len(body)
    body += b"".join(aevents)

    # ---- and the WEAPON ATTRIBUTES, for the stage's own
    # weapons. Their `data` is NULL in every case the port has (the flame
    # is an invisible hitbox drawn by particles), so the pointer fields
    # are markers the way the stage items' are. See
    # parse_weapon_attributes.
    wattrs = []

    for (off, vals) in parse_weapon_attributes(
            [(item, off, os.path.join(DECOMP, "src", "relocData", src), sym)
             for (item, off, src, sym) in STAGE_WEAPONS]):
        # The pointer fields are the decomp's own. A weapon whose `data`
        # is NULL has no model and takes the decomp's bare-DObj arm
        # (wpManagerAddModel) -- Saffron City's flame is that one. The
        # razor's `data` is REAL and names a tree in the stage's model
        # file, which the pack cannot carry either, so it becomes the same
        # marker the stage items' `data` is: non-NULL, which is all
        # wpManagerMakeWeapon's model path reads before the port's own
        # table answers for it (src/dc/wpmanager.c's sWPModels, keyed by
        # the WPDesc, which names the baked .mdl). The other three are
        # NO_PTR, as every stage item's are.
        ptrs = [None, None, None, None]
        for i in range(4):
            tok = re.sub(r'^\(\s*\w+\s*\*\s*\)\s*', '', vals[i].strip())
            if i == 0:
                ptrs[0] = 0 if tok != "NULL" else None
                continue
            if tok != "NULL":
                raise AssertionError("stage weapon at 0x%X names a %s it "
                                     "cannot point at" % (off, tok))
        if ptrs[0] is None and vals[0].strip() != "NULL":
            raise AssertionError("stage weapon at 0x%X: unparsed data" % off)
        wattrs.append(encode_weapon_attr(vals, ptrs, off))

    # ... and ITCommonData's OWN twelve: the Ray Gun's shot, the Fire
    # Flower's flame, the Star Rod's two stars and the eight Poke Ball
    # monsters' attacks. These were left to the byte overlay the comment
    # above calls correct for a fighter's weapon -- but a fighter's reads a
    # blob wpattrs.bin baked, and these read region 0, the ROM's own
    # big-endian bytes, through the SH4's little-endian bitfields. The
    # flame came out size 3585 (0x010E swapped) and damage 0, so no flame
    # ever hurt anyone. They are decoded here by ssb_wpattrexport's own
    # reader and held to the decomp's typed initializers the same way the
    # fighters' are; the pointers are the ROM's relocations into region 1,
    # as the ItemAttributes tables' are.
    import ssb_wpattrexport as WA
    wkeys = set(off for (_, off, _, _) in STAGE_WEAPONS)
    for (kind, name, off, region) in blocks:
        if kind != "WeaponAttributes":
            continue
        if region != 0:
            raise AssertionError("WeaponAttributes %s at 0x%X is not in "
                                 "ITCommonData" % (name, off))
        if off in wkeys:
            raise AssertionError("WeaponAttributes %s: key 0x%X is already "
                                 "a stage weapon's" % (name, off))
        wkeys.add(off)
        dec = WA.decode_record(bytes(freed[off:off + WA.ROM_RECORD]))
        WA.check_against_literal(ITCOMMONDATA, "251_ITCommonData.c",
                                 "dITCommonData_%s_WeaponAttributes" % name,
                                 off, dec)
        ptrs = [fixmap.get(off + 4 * i) for i in range(4)]
        for i in range(4):
            if ptrs[i] is None and struct.unpack_from(">I", freed,
                                                      off + 4 * i)[0] != 0:
                raise AssertionError("WeaponAttributes %s: pointer %d is "
                                     "set but not relocated" % (name, i))
        wattrs.append(encode_weapon_attr(WA.record_vals(dec), ptrs, off))
    off_wattrs = HEADER + len(body)
    body += b"".join(wattrs)

    off_blocks = HEADER + len(body)
    ai = 0
    for (kind, name, off, region) in blocks:
        # The name the DESCRIPTIONS spell, kind first -- `DataStart
        # Capsule`, `ItemAttributes Capsule`. The kind has to be in it:
        # one item's name covers several blocks of different kinds
        # (`DataStart Shell`, `DisplayList Shell`, `AnimJoint Shell`,
        # `MatAnimJoint Shell`), so the bare name is not unique even
        # within a region. A bare `-` entry has no kind and is its own
        # name.
        sym = name if kind == "-" else "%s %s" % (kind, name)
        if kind == "ItemAttributes":
            body += struct.pack("<32sII", sym.encode()[:NAME_MAX],
                                REGION_ATTRS, ai * IT_ATTR_SIZE)
            ai += 1
        else:
            body += struct.pack("<32sII", sym.encode()[:NAME_MAX], region, off)
    while len(body) % 4:
        body += b"\0"

    off_fixes = HEADER + len(body)
    for (site, target) in fixes:
        body += struct.pack("<II", site, off_models + target)

    # The model file's OWN pointers. Everything above is
    # ITCommonData reaching into ITCommonObject; these are ITCommonObject
    # reaching into itself -- a DisplayList naming a sub-list, a DObjDesc
    # naming a display list, and, which is what found them, a
    # MatAnimJoint script naming the next script. On the N64
    # lbRelocLoadAndRelocFile walks this chain as it walks the extern one
    # and writes an address into every site; the port carries the file's
    # words verbatim, so if these are not carried the words stay ROM
    # reloc descriptors -- {u16 next, u16 target/4} -- and the game reads
    # one as a pointer. `MatAnimJoint BombHeiWalk` has eight of them, and
    # a Bob-omb that walked long enough put gcParseMObjMatAnimJoint in
    # its `while (anim_wait <= 0)` loop forever.
    #
    # The whole chain is carried, not the scripts' share of it: it is the
    # file's own relocation list, the game applies all of it, and a
    # census of which blocks the port happens to read today would be one
    # more thing to fall out of date.
    ifixes = sorted(intern.items())
    for (site, target) in ifixes:
        if site + 4 > len(fobj) or target + 4 > len(fobj):
            raise AssertionError("ITCommonObject intern reloc at 0x%X names "
                                 "0x%X, outside the file" % (site, target))
    off_ifixes = HEADER + len(body)
    for (site, target) in ifixes:
        body += struct.pack("<II", off_models + site, off_models + target)

    head = struct.pack("<4sIIIIIIIIIIIIIIIIIII", b"ITCD", 2,
                       len(freed), len(fobj), len(blocks), len(fixes),
                       HEADER, off_models, off_blocks, off_fixes,
                       off_attrs, len(attrs), off_events, len(events),
                       off_aevents, len(aevents), off_wattrs, len(wattrs),
                       off_ifixes, len(ifixes))
    assert len(head) == HEADER, len(head)

    if dump:
        # What was WRITTEN, not what came in: an ItemAttributes entry
        # names region 2 and a record index now, and printing `blocks`
        # would show the raw region-0 offsets it no longer uses.
        ai = 0
        for (kind, name, off, region) in blocks:
            if kind == "ItemAttributes":
                print("  %-18s %-24s r%d record %d" % (kind, name,
                                                       REGION_ATTRS, ai))
                ai += 1
            else:
                print("  %-18s %-24s r%d 0x%04X" % (kind, name, region, off))
        print("blocks %d (34 attrs as %d records), fixups %d (%d at a block "
              "start, %d interior), model relocs %d, data %d B, models %d B"
              % (len(blocks), len(attrs), len(fixes), on_block,
                 len(fixes) - on_block, len(ifixes), len(freed), len(fobj)))

    return head + bytes(body)




def main():
    argv = sys.argv[1:]
    rom_path = os.path.join(ROOT, "baserom.z64")
    out = None
    if "--rom" in argv:
        rom_path = argv[argv.index("--rom") + 1]
    if "--out" in argv:
        out = argv[argv.index("--out") + 1]
    if not out:
        sys.exit("--out <file.itp> is required")

    rom = open(rom_path, "rb").read()
    blob = pack_items(rom, dump="--dump" in argv)
    d = os.path.dirname(out)
    if d:
        os.makedirs(d, exist_ok=True)
    with open(out, "wb") as fp:
        fp.write(blob)
    print("wrote %s (%d bytes)" % (out, len(blob)))


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""ssb_wpattrexport.py -- the fighters' weapon hitbox tables, off the ROM.

A fighter's projectile reads its `WPAttributes` through the decomp's own
arithmetic (wp/wpmanager.c wpManagerMakeWeapon, compiled verbatim in
src/dc/wpmanager.c):

    lbRelocGetFileData(WPAttributes*, *wp_desc->p_weapon, wp_desc->o_attributes)
    == *gFTData<Char><File> + (intptr_t)&ll<Char><File><Weapon>WeaponAttributes

where `gFTData<Char><File>` is the fighter's loaded relocData file and the
`ll*` symbol is an ABSOLUTE linker symbol whose address is the table's
byte offset inside that file (the same scheme mp/mpcollision.c leans on
for stage files, src/dc/mpshim.c). The port has no loaded relocData
files, so this tool makes both halves true another way:

  --out romdisk/wpattrs.bin   the tables, one blob per ROM file, each
                              record laid at its ROM offset so the
                              arithmetic above lands on it once
                              src/dc/wpattrs.c binds the file pointer to
                              the blob;
  --emit-ld wpattrs.ld        the `ll* = 0x..;` fragment that gives the
                              symbols their offsets at link time, the way
                              particlebanks.ld does for the particle
                              banks (tools/export/ssb_particleexport.py).

The record is the item pack's own 64-byte WATTR record
(tools/export/ssb_itemexport.py encode_weapon_attr, read back by
src/dc/itempack.c's decoder, which src/dc/wpattrs.c shares) -- host
fields, not the N64's 0x34 bytes, because the struct is BITFIELDS and IDO
and GCC need not lay those out alike. The N64 bytes are decoded here from
the ROM (not from the decomp's C: the `*Main` files' tables are not typed
literals there, they sit inside over-extended `d<X>Main_file_handles[]`
arrays) and, for the six files that DO have a typed literal, checked
against it field by field -- the bake's own oracle.

Pointer fields bake as markers: on target the only one anything reads is
`data`, and only as NULL-or-not (wpManagerIsModelLess picks the bare-DObj
arm; the model itself comes from the pack table keyed by WPDesc). A slot
that carries a reloc -- intern or extern -- is non-NULL in the game and
is written as 0 here; one that carries none is WATTR_NO_PTR.

Usage:
  python3 tools/export/ssb_wpattrexport.py --rom <z64> --out romdisk/wpattrs.bin
  python3 tools/export/ssb_wpattrexport.py --emit-ld <file> [--symbol-prefix _]
  python3 tools/export/ssb_wpattrexport.py --list
"""
import argparse
import os
import re
import struct
import sys

sys.path[:0] = [os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), d)
                for d in ("lib", "export", "check")]
import ssb_assets as A                  # noqa: E402
import ssb_extract                      # noqa: E402
import ssb_logicexport as L             # noqa: E402
from ssb_itemexport import (            # noqa: E402
    WATTR_SIZE, WATTR_FIELDS, encode_weapon_attr, _split_fields)
from ssb_paths import DECOMP_DIR        # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
ROM_DEFAULT = os.path.join(ROOT, "baserom.z64")
RELOC_HEADER = os.path.join(ROOT, "src", "dc", "decomp", "reloc_data.us.h")
RELOC_DESC = os.path.join(DECOMP_DIR, "tools", "relocFileDescriptions.us.txt")
RELOC_SRC = os.path.join(DECOMP_DIR, "src", "relocData")

MAGIC = b"WPAT"
VERSION = 2
ROM_RECORD = 0x34               # sizeof(WPAttributes) on the N64

# Every fighter weapon file the port's WPDescs name (the wp/ sources on
# src/game/ssb64/Makefile's link line), with the tables inside it. Each
# row: (file id, the gFTData* global that holds the file on target, the
# decomp's relocData source for the oracle, [tables]); each table:
# (ll* symbol, the name relocFileDescriptions.us.txt gives it, the typed
# literal's symbol in that source or None when the decomp has none).
#
# File 203 (MarioMain) is deliberately absent: its `@0x0` is the
# fighter's file_handles, not a table, and wpMarioFireballMakeWeapon
# overwrites the static WPDesc's file/offset pair with Special1's before
# any read (wp/wpmario/wpmariofireball.c:167). Its symbol still gets its
# offset in the .ld (LD_ONLY below) so the desc links.
WEAPON_FILES = [
    (204, "gFTMarioFileSpecial1", "204_MarioSpecial1.c", [
        ("llMarioSpecial1FireballWeaponAttributes", "Fireball",
         "dMarioSpecial1_Fireball_WeaponAttributes")]),
    (222, "gFTDataLuigiSpecial1", "222_LuigiSpecial1.c", [
        ("llLuigiSpecial1FireballWeaponAttributes", "Fireball",
         "dLuigiSpecial1_Fireball_WeaponAttributes")]),
    (210, "gFTDataFoxSpecial1", "210_FoxSpecial1.c", [
        ("llFoxSpecial1BlasterWeaponAttributes", "Blaster",
         "dFoxSpecial1_Blaster_WeaponAttributes")]),
    (217, "gFTDataSamusMain", "217_SamusMain.c", [
        ("llSamusMainBombWeaponAttributes", "Bomb", None)]),
    (218, "gFTDataSamusSpecial1", "218_SamusSpecial1.c", [
        ("llSamusSpecial1ChargeShotWeaponAttributes", "ChargeShot",
         "dSamusSpecial1_ChargeShot_WeaponAttributes")]),
    (247, "gFTDataYoshiMain", "247_YoshiMain.c", [
        ("llYoshiMainEggThrowWeaponAttributes", "EggThrow", None),
        ("llYoshiMainStarWeaponAttributes", "Star", None)]),
    (229, "gFTDataKirbyMain", "229_KirbyMain.c", [
        ("llKirbyMainCutterWeaponAttributes", "Cutter", None)]),
    (225, "gFTDataLinkMain", "225_LinkMain.c", [
        ("llLinkMainSpinAttackWeaponAttributes", "SpinAttack", None)]),
    (226, "gFTDataLinkSpecial1", "226_LinkSpecial1.c", [
        ("llLinkSpecial1BoomerangWeaponAttributes", "Boomerang",
         "dLinkSpecial1_Boomerang_WeaponAttributes")]),
    (243, "gFTDataPikachuMain", "243_PikachuMain.c", [
        ("llPikachuMainThunderHeadWeaponAttributes", "ThunderHead", None),
        ("llPikachuMainThunderTrailWeaponAttributes", "ThunderTrail", None)]),
    (244, "gFTDataPikachuSpecial1", "244_PikachuSpecial1.c", [
        ("llPikachuSpecial1ThunderJoltAirWeaponAttributes", "ThunderJoltAir",
         "dPikachuSpecial1_ThunderJoltAir_WeaponAttributes"),
        ("llPikachuSpecial1ThunderJoltGroundWeaponAttributes",
         "ThunderJoltGround",
         "dPikachuSpecial1_ThunderJoltGround_WeaponAttributes")]),
    (239, "gFTNessFileMain", "239_NessMain.c", [
        ("llNessMainPKThunderWeaponAttributes", "PKThunder", None),
        ("llNessMainPKThunderTrailWeaponAttributes", "PKThunderTrail", None)]),
    (240, "gFTNessFileSpecial1", "240_NessSpecial1.c", [
        ("llNessSpecial1PKFireWeaponAttributes", "PKFire",
         "dNessSpecial1_PKFire_WeaponAttributes")]),
    # Master Hand's finger rocket, the only weapon in the
    # game whose records live in a fighter's MainMotion rather than a
    # Special file -- which is also why BossMainMotion is the only
    # motion file whose extern relocations leave FTCommonMoveset (step
    # 23). The decomp types both literals, but names them by offset
    # rather than by weapon, so the oracle below is given those names.
    (249, "gFTDataBossMainMotion", "249_BossMainMotion.c", [
        ("llBossMainMotionBulletNormalWeaponAttributes", "BulletNormal",
         "dBossMainMotion_0x0774"),
        ("llBossMainMotionBulletHardWeaponAttributes", "BulletHard",
         "dBossMainMotion_0x07A8")]),
]

LD_ONLY = [
    (203, "llMarioMainFireballWeaponAttributes", "Fireball"),
]

# RAW tables: data an ITEM reads straight out of a fighter
# file this blob already stands in for. it/itfighter/itlinkbomb.c indexes
# gFTDataLinkMain by llLinkMainBombAttackEvents (four ITAttackEvents, the
# explosion's growing hitbox) and llLinkMainBombBloatScales (six f32s, the
# swell before it goes off). LinkMain's head is untyped in the decomp, so
# both are read off the ROM. Each is written in host fields, not the N64's
# bytes -- ITAttackEvent is bitfields -- and src/dc/wpattrs.c lays it at
# its offset by the records' spread rule. (file, symbol, kind, count.)
RAW_ATTACKEVENT = 1
RAW_F32 = 2
RAW_TABLES = [
    (225, "llLinkMainBombAttackEvents", RAW_ATTACKEVENT, 4),
    (225, "llLinkMainBombBloatScales", RAW_F32, 6),
]
RAW_ROM_SIZE = {RAW_ATTACKEVENT: 8, RAW_F32: 4}


def decode_attack_event(raw):
    """One ITAttackEvent out of the N64's 8 bytes: u8 timer, then a 32-bit
    unit holding `s32 angle : 10` and `u32 damage : 8` from bit 8, then
    u16 size at 4. (timer, angle, damage, size)."""
    word = struct.unpack(">I", raw[0:4])[0]
    return (raw[0], _sfield(word, 23, 10), _ufield(word, 13, 8),
            struct.unpack(">H", raw[4:6])[0])


def raw_symbols():
    """[(symbol, file id, offset)] for RAW_TABLES, off the reloc header."""
    header = reloc_offsets()
    out = []
    for (fid, sym, _, _) in RAW_TABLES:
        if sym not in header:
            sys.exit("wpattrs: %s declares no %s"
                     % (os.path.relpath(RELOC_HEADER, ROOT), sym))
        out.append((sym, fid, header[sym]))
    return out


def bake_raw(rom):
    """{file id: [(offset, kind, count, host bytes)]}"""
    header = reloc_offsets()
    out = {}
    for (fid, sym, kind, count) in RAW_TABLES:
        fdata = L.file_info(rom, fid)[0]
        off = header[sym]
        size = RAW_ROM_SIZE[kind]
        if off + count * size > len(fdata):
            sys.exit("wpattrs: %s runs past file %d" % (sym, fid))
        body = b""
        for k in range(count):
            raw = bytes(fdata[off + k * size:off + (k + 1) * size])
            if kind == RAW_ATTACKEVENT:
                t, a, d, sz = decode_attack_event(raw)
                body += struct.pack("<BxhBxH", t, a, d, sz)
            else:
                body += struct.pack("<f", struct.unpack(">f", raw)[0])
        out.setdefault(fid, []).append((off, kind, count, body))
    return out


def reloc_offsets():
    """Every ll* symbol's value out of the generated reloc header."""
    src = open(RELOC_HEADER).read()
    return {m.group(1): int(m.group(2), 16) for m in re.finditer(
        r"extern int (ll\w+); // (0x[0-9a-fA-F]+)", src)}


def description_offsets(fid):
    """{name: offset} of the WeaponAttributes blocks the decomp's own
    file description lists under [fid]."""
    out = {}
    group = None
    for line in open(RELOC_DESC):
        line = line.strip()
        m = re.match(r"\[(\d+)\]$", line)
        if m:
            group = int(m.group(1))
            continue
        if group != fid:
            continue
        m = re.match(r"WeaponAttributes (\w+) (0x[0-9A-Fa-f]+)$", line)
        if m:
            out[m.group(1)] = int(m.group(2), 16)
    return out


def symbols():
    """[(symbol, file id, description name)] for the whole table, with
    each symbol's offset checked against both the header and the
    description -- a mismatch aborts, since the .ld would be wrong."""
    header = reloc_offsets()
    out = []
    rows = [(fid, sym, name) for (fid, _, _, tables) in WEAPON_FILES
            for (sym, name, _) in tables] + LD_ONLY
    for (fid, sym, name) in rows:
        if sym not in header:
            sys.exit("wpattrs: %s declares no %s"
                     % (os.path.relpath(RELOC_HEADER, ROOT), sym))
        desc = description_offsets(fid)
        if name not in desc:
            sys.exit("wpattrs: relocFileDescriptions [%d] has no "
                     "WeaponAttributes %s" % (fid, name))
        if desc[name] != header[sym]:
            sys.exit("wpattrs: %s is 0x%x in the header and 0x%x in the "
                     "description" % (sym, header[sym], desc[name]))
        out.append((sym, fid, header[sym]))
    return out


def _sfield(word, hi, width):
    """`width` bits of `word` ending at bit `hi` (MSB-first, as IDO packs
    bitfields into a big-endian u32), sign-extended."""
    v = (word >> (hi - width + 1)) & ((1 << width) - 1)
    if v & (1 << (width - 1)):
        v -= 1 << width
    return v


def _ufield(word, hi, width):
    return (word >> (hi - width + 1)) & ((1 << width) - 1)


def decode_record(raw):
    """The N64's 0x34 bytes as the 29 initializer fields, in wp/wptypes.h's
    order, as ints (the four pointers excluded -- see the file comment)."""
    if len(raw) != ROM_RECORD:
        raise AssertionError("record is %d bytes, want 0x34" % len(raw))
    atk = struct.unpack_from(">6h", raw, 0x10)
    mc = struct.unpack_from(">4h", raw, 0x1C)
    w0, w1, w2, w3 = struct.unpack_from(">4I", raw, 0x24)
    # word 0x24: size:16 angle:10 (6 pad)
    # word 0x28: knockback_scale:10 damage:8 element:4 knockback_weight:10
    # word 0x2C: shield_damage:8 attack_count:2 can_setoff:1 sfx:10
    #            priority:3 rehit_item:1 rehit_fighter:1 hop:1 reflect:1
    #            absorb:1 shield:1 unused_b6:1 unused_b7:1
    # word 0x30: knockback_base:10
    return {
        "attack_offsets": atk,
        "map_coll": mc,
        "size": _ufield(w0, 31, 16),
        "angle": _sfield(w0, 15, 10),
        "knockback_scale": _ufield(w1, 31, 10),
        "damage": _ufield(w1, 21, 8),
        "element": _ufield(w1, 13, 4),
        "knockback_weight": _ufield(w1, 9, 10),
        "shield_damage": _sfield(w2, 31, 8),
        "attack_count": _ufield(w2, 23, 2),
        "can_setoff": _ufield(w2, 21, 1),
        "sfx": _ufield(w2, 20, 10),
        "priority": _ufield(w2, 10, 3),
        "can_rehit_item": _ufield(w2, 7, 1),
        "can_rehit_fighter": _ufield(w2, 6, 1),
        "can_hop": _ufield(w2, 5, 1),
        "can_reflect": _ufield(w2, 4, 1),
        "can_absorb": _ufield(w2, 3, 1),
        "can_shield": _ufield(w2, 2, 1),
        "unused_0x2F_b6": _ufield(w2, 1, 1),
        "unused_0x2F_b7": _ufield(w2, 0, 1),
        "knockback_base": _ufield(w3, 31, 10),
    }


# wp/wptypes.h's initializer order after the four pointers and the
# attack_offsets pair -- what parse_weapon_attributes returns as fields
# 5..28 and what encode_weapon_attr indexes.
SCALARS = ("map_coll_top", "map_coll_center", "map_coll_bottom",
           "map_coll_width", "size", "angle", "knockback_scale", "damage",
           "element", "knockback_weight", "shield_damage", "attack_count",
           "can_setoff", "sfx", "priority", "can_rehit_item",
           "can_rehit_fighter", "can_hop", "can_reflect", "can_absorb",
           "can_shield", "unused_0x2F_b6", "unused_0x2F_b7",
           "knockback_base")


def record_vals(dec):
    """A decoded record as the 29 initializer strings encode_weapon_attr
    takes, so the bytes it writes are the item pack's exactly."""
    a = dec["attack_offsets"]
    vals = ["0", "0", "0", "0",
            "{ { %d, %d, %d }, { %d, %d, %d } }" % tuple(a)]
    scal = list(dec["map_coll"]) + [dec[k] for k in SCALARS[4:]]
    return vals + [str(v) for v in scal]


def literal_fields(path, sym):
    """The 29 initializer fields of a typed `WPAttributes sym = {...};`.
    ssb_itemexport's parse_weapon_attributes insists on a REGION_JP split
    (every stage weapon has one); a fighter's may not (SamusSpecial1), so
    the US arm is taken here whether or not there is a JP one."""
    text = open(path).read()
    text = re.sub(r'#if defined\(REGION_JP\).*?#else(.*?)#endif',
                  lambda m: m.group(1), text, flags=re.S)
    m = re.search(r'WPAttributes\s+%s\s*=\s*\{(.*?)\n\};' % re.escape(sym),
                  text, re.S)
    if m is None:
        sys.exit("wpattrs: %s has no WPAttributes %s" % (path, sym))
    vals = _split_fields(m.group(1))
    if len(vals) != WATTR_FIELDS:
        sys.exit("wpattrs: %s: %s has %d fields, want %d"
                 % (path, sym, len(vals), WATTR_FIELDS))
    return vals


def check_against_literal(fid, src, sym, off, dec):
    """The oracle: the decoded record against the decomp's typed literal."""
    vals = literal_fields(os.path.join(RELOC_SRC, src), sym)
    want = record_vals(dec)
    # fields 4..28: the pointers are symbols in the literal, not ints
    for i in range(4, 29):
        lit = re.sub(r"\s+", " ", vals[i]).strip()
        if lit.startswith("nSYAudioFGM"):
            continue            # an FGM id the port resolves by name
        if lit != want[i]:
            sys.exit("wpattrs: [%d] %s field %d: ROM says %s, %s says %s"
                     % (fid, sym, i, want[i], src, lit))


def bake(rom):
    files = []
    for (fid, base, src, tables) in WEAPON_FILES:
        fdata, _, intern, sites, _ = L.file_info(rom, fid)
        reloc_sites = set(intern) | {s for (s, _) in sites}
        header = reloc_offsets()
        recs = []
        for (sym, name, lit) in tables:
            off = header[sym]
            raw = fdata[off:off + ROM_RECORD]
            dec = decode_record(raw)
            if lit is not None:
                check_against_literal(fid, src, lit, off, dec)
            ptrs = [0 if (off + 4 * i) in reloc_sites else None
                    for i in range(4)]
            rec = encode_weapon_attr(record_vals(dec), ptrs, off)
            if len(rec) != WATTR_SIZE:
                raise AssertionError("record is %d bytes" % len(rec))
            recs.append((off, rec, dec, ptrs))
        files.append((fid, base, recs))
    return files


def emit_bin(files, raws):
    out = MAGIC + struct.pack("<HH", VERSION, len(files))
    for (fid, _, recs) in files:
        mine = raws.get(fid, [])
        blob_size = max([off + ROM_RECORD for (off, _, _, _) in recs] +
                        [off + count * RAW_ROM_SIZE[kind]
                         for (off, kind, count, _) in mine])
        out += struct.pack("<HHHH", fid, len(recs), blob_size, len(mine))
        # in offset order: the loader spreads a file's records apart by
        # index when its WPAttributes is wider than the N64's (see
        # mapped_offsets), so the order in the file IS the rule
        for (off, rec, _, _) in sorted(recs, key=lambda r: r[0]):
            out += struct.pack("<I", off) + rec
        for (off, kind, count, body) in mine:
            out += struct.pack("<IHH", off, kind, count) + body
    if set(raws) - set(fid for (fid, _, _) in files):
        sys.exit("wpattrs: a raw table's file has no weapon record")
    return out


def mapped_offsets(record_size):
    """[(sym, fid, rom offset, offset in the blob)]. On the target the two
    are equal: sizeof(WPAttributes) is the N64's 0x34 there, so a blob
    with each record at its ROM offset holds them without overlap. A
    64-bit host's WPAttributes is wider (eight-byte pointers), so the
    host blob spreads a file's records apart by that excess, in offset
    order -- record k sits at off + k * (record_size - 0x34) -- and the
    host's fragment names those spread offsets. src/dc/wpattrs.c lays the
    records out by the same rule from sizeof(WPAttributes)."""
    delta = record_size - ROM_RECORD
    if delta < 0:
        sys.exit("wpattrs: --record-size %d is smaller than the N64's 0x34"
                 % record_size)
    by_file = {}
    for (sym, fid, off) in symbols():
        by_file.setdefault(fid, []).append((off, sym))
    out = []
    for fid in by_file:
        for k, (off, sym) in enumerate(sorted(by_file[fid])):
            out.append((sym, fid, off, off + k * delta))
    # a raw table moves by the records before it (src/dc/wpattrs.c)
    for (sym, fid, off) in raw_symbols():
        before = sum(1 for (o, _) in by_file.get(fid, []) if o < off)
        out.append((sym, fid, off, off + before * delta))
    return out


def emit_ld(prefix, record_size=ROM_RECORD):
    out = ["/* Generated by tools/export/ssb_wpattrexport.py -- do not edit.",
           " *",
           " * The fighters' weapon-attribute offsets, as the game's linker",
           " * had them: each ll<Char><File><Weapon>WeaponAttributes is the",
           " * table's byte offset inside that fighter's relocData file, and",
           " * wpManagerMakeWeapon adds it to the file pointer src/dc/wpattrs.c",
           " * binds to the baked blob (romdisk/wpattrs.bin) of that file. */"]
    if record_size != ROM_RECORD:
        out += ["/* For a host whose WPAttributes is %d bytes, not the N64's"
                % record_size,
                " * 0x34: a file's records are spread apart by the excess, in",
                " * offset order, and these are the spread offsets (see",
                " * mapped_offsets in the tool). */"]
    out.append("")
    for (sym, fid, off, mapped) in mapped_offsets(record_size):
        if mapped == off:
            out.append("%s%s = %#x; /* file %d */" % (prefix, sym, off, fid))
        else:
            out.append("%s%s = %#x; /* file %d, ROM offset %#x */"
                       % (prefix, sym, mapped, fid, off))
    return "\n".join(out) + "\n"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--rom", default=ROM_DEFAULT)
    ap.add_argument("--out", help="write romdisk/wpattrs.bin")
    ap.add_argument("--emit-ld", metavar="FILE",
                    help="write the linker fragment of table offsets")
    ap.add_argument("--symbol-prefix", default="",
                    help="the toolchain's C symbol prefix (\"_\" for sh-elf)")
    ap.add_argument("--record-size", type=lambda v: int(v, 0), default=ROM_RECORD,
                    help="sizeof(WPAttributes) where the fragment links "
                         "(default the N64's 0x34; the 64-bit host's is 72)")
    ap.add_argument("--list", action="store_true",
                    help="print every table as decoded off the ROM")
    args = ap.parse_args()

    if args.emit_ld:
        os.makedirs(os.path.dirname(os.path.abspath(args.emit_ld)),
                    exist_ok=True)
        with open(args.emit_ld, "w") as f:
            f.write(emit_ld(args.symbol_prefix, args.record_size))
        print("%s: %d table offsets" % (args.emit_ld, len(symbols())))
        return 0

    symbols()                   # the offset cross-check, on every bake
    rom = open(args.rom, "rb").read()
    files = bake(rom)

    if args.list:
        for (fid, base, recs) in files:
            print("[%d] %s" % (fid, base))
            for (off, _, dec, ptrs) in recs:
                print("  @0x%02x ptrs %s size %d angle %d dmg %d kbs %d "
                      "kbw %d kbb %d cnt %d sfx %d mapcoll %s" % (
                          off, "".join("p" if p is not None else "-"
                                       for p in ptrs),
                          dec["size"], dec["angle"], dec["damage"],
                          dec["knockback_scale"], dec["knockback_weight"],
                          dec["knockback_base"], dec["attack_count"],
                          dec["sfx"], dec["map_coll"]))
    if args.out:
        blob = emit_bin(files, bake_raw(rom))
        d = os.path.dirname(args.out)
        if d:
            os.makedirs(d, exist_ok=True)
        with open(args.out, "wb") as f:
            f.write(blob)
        n = sum(len(r) for (_, _, r) in files)
        print("wrote %s (%d bytes, %d tables in %d files)"
              % (args.out, len(blob), n, len(files)))
    elif not args.list:
        sys.exit("--out, --emit-ld or --list is required")
    return 0


if __name__ == "__main__":
    sys.exit(main())

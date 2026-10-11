#!/usr/bin/env python3
"""anm_tier_check.py -- each fighter's .anm holds its tiers.

A menu reads only a prefix of each fighter's <name>.anm (src/dc/fighter.h
FPackAttr.anm_tier_end, ftcommon.h ftManagerSetAnmTier), and an
animation outside it is read whole from the disc when a fighter binds it,
with a "not resident" line on the log. Whether that happens is decided
at export (tools/export/ssb_packexport.py, from anm_tiers.tsv). This
checks each fighter pack and .anm in the romdisk tree, which is what the
disc carries:

  - the tiers: in order, 4-aligned, inside the file, and no animation
    running across a tier's end;
  - tier 0 holds every animation a SubMotion row plays -- the whole of
    what the character selects, the results screen and the 1P card play
    (they load the SubMotion file alone);
  - every rule in anm_tiers.tsv holds for the packs it names;
  - tier 1 holds every animation of every status the Characters
    screen's motion tables name for that fighter (src/dc/mncharacters.c,
    each status resolved to its motion through the decomp's status
    tables, as tools/check/status_check.py reads them). A status those
    reach on their own -- a special's follow-on -- is not in the tables;
    the -DDB_CHARACTERS_TOUR trace is what covers those (anm_tiers.py).

`./run.sh test anmtier` runs it; `./run.sh test` runs it with the rest.
"""
import glob
import os
import re
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(ROOT, "tools", "export"))
import status_check as S     # noqa: E402
import ssb_packexport as X   # noqa: E402  (anm_tier_rules)

ROMDISK = os.path.join(ROOT, "src", "game", "ssb64", "romdisk")
CHARACTERS = os.path.join(ROOT, "src", "dc", "mncharacters.c")
ANM_HEADER = 16
DIR_ENTRY = 64          # sizeof(FPackAnim)
MOTION_ROW = 12         # sizeof(FPackMotion)

# nFTKind order (ft/ftdef.h): the Characters screen's pages, and the
# order of dMNCharactersAttack1MotionDescs' rows
KINDS = ["Mario", "Fox", "Donkey", "Samus", "Luigi", "Link", "Yoshi",
         "Captain", "Kirby", "Pikachu", "Purin", "Ness"]


def read_pack(stem):
    """The pack's anims (name, words offset in the .anm's words, bytes),
    its MainMotion and SubMotion rows' anim indices, and the tiers."""
    p = open(os.path.join(ROMDISK, stem + ".pack"), "rb").read()
    a = open(os.path.join(ROMDISK, stem + ".anm"), "rb").read()
    h = struct.unpack_from("<8s8I8I3ff8s8I", p, 0)
    anim_count, off_anims = h[7], h[16]
    name = h[21].rstrip(b"\0").decode()
    off_attr, off_motion, motion_count = h[22], h[23], h[24]
    magic, anm_id, anm_size = struct.unpack_from("<8s2I", a, 0)
    at = p.find(struct.pack("<2I", anm_id, anm_size), off_attr, off_motion)
    if magic != b"SSBANIM1" or at < 0 or len(a) != ANM_HEADER + anm_size:
        raise AssertionError("%s: the .anm is not this pack's" % stem)
    off_sub, sub_count = struct.unpack_from("<2I", p, at - 8)
    tiers = list(struct.unpack_from("<2I", p, at + 8))
    anims = []
    for i in range(anim_count):
        nm, ow, nw, _oe, kind, _or, _nr = struct.unpack_from(
            "<40s6I", p, off_anims + DIR_ENTRY * i)
        anims.append((nm.rstrip(b"\0").decode(), ow - ANM_HEADER,
                      nw * (4 if kind else 2)))
    main = [struct.unpack_from("<h", p, off_motion + MOTION_ROW * i)[0]
            for i in range(motion_count)]
    sub = [struct.unpack_from("<h", p, off_sub + MOTION_ROW * i)[0]
           for i in range(sub_count)]
    return name, anims, main, sub, tiers, anm_size


def characters_statuses(names):
    """fighter kind -> the status ids the Characters screen's tables name
    for it (mncharacters.c: the per-fighter special tables, the common
    table every fighter plays, the Attack1 rows by kind, and Kirby's and
    Purin's own jump and fall rows)."""
    text = S.strip_comments(open(CHARACTERS).read())
    out = {k: set() for k in KINDS}
    common = []
    demo = re.compile(r"FTSTATUS_CHARACTERS_DEMO\((\w+)\)")
    for m in re.finditer(r"^MNCharacters(?:Special)?Motion\s+(\w+)[^=\n]*=\s*\{",
                         text, re.M):
        table = m.group(1)
        body = S.braced(text, m.end() - 1)
        if table == "dMNCharactersAttack1MotionDescs":
            rows = S.groups(body)
            if len(rows) != len(KINDS):
                raise AssertionError("%s has %d rows, not one per kind"
                                     % (table, len(rows)))
            for kind, (_lead, row) in zip(KINDS, rows):
                out[kind] |= set(demo.findall(row))
            continue
        if table == "dMNCharactersCommonMotionDescs":
            common = S.groups(body)
            continue
        for kind in [k for k in KINDS if k in table]:
            out[kind] |= set(demo.findall(body))
    # the common table by motion kind and track, less what
    # mnCharactersGetMotion answers from another table: the specials and
    # Attack1 for everyone, and Kirby's and Purin's midair jumps and the
    # second track of their aerials (their own jump and fall rows, above)
    kind_of = lambda n: names["nMNCharactersMotionKind" + n]
    for mk, (_lead, row) in enumerate(common):
        if mk in (kind_of("SpecialHi"), kind_of("SpecialN"),
                  kind_of("SpecialLw"), kind_of("Attack1")):
            continue
        tracks = [demo.findall(t) for _l, t in S.groups(row)]
        for kind in KINDS:
            floaty = kind in ("Kirby", "Purin")
            if floaty and mk in (kind_of("JumpAerialF"), kind_of("JumpAerialB")):
                continue
            for track, found in enumerate(tracks):
                if floaty and track == 1 and \
                        kind_of("AttackAirStart") <= mk <= kind_of("AttackAirEnd"):
                    continue
                out[kind] |= set(found)
    return {k: {names[s] for s in v} for k, v in out.items()}


def status_motions(names):
    """(fighter kind, status id) -> motion id, from the decomp's tables"""
    rows = {}
    for path in sorted(glob.glob(os.path.join(S.DECOMP, "src", "ft", "**",
                                              "*status.h"), recursive=True)):
        rows.update(S.tables(path, names))
    dmap, _pmap = S.kind_map(names)
    special = names["nFTCommonStatusSpecialStart"]

    def motion(kind, status):
        if status >= special:
            table = dmap[KINDS.index(kind)]
            base = special
        elif status >= names["nFTCommonStatusActionStart"]:
            table, base = "dFTCommonActionStatusDescs", \
                names["nFTCommonStatusActionStart"]
        else:
            table, base = "dFTCommonNullStatusDescs", 0
        cells = rows[table][1][status - base]
        return eval(cells[0], {"__builtins__": {}}, names)
    return motion


def check():
    names = S.enum_values()
    errors = []
    packs = {}
    for path in sorted(glob.glob(os.path.join(ROMDISK, "*.anm"))):
        stem = os.path.splitext(os.path.basename(path))[0]
        packs[stem] = read_pack(stem)
    if not packs:
        return ["no .anm under %s: build the romdisk first" % ROMDISK]

    rules = X.anm_tier_rules()
    for stem, (name, anims, main, sub, tiers, size) in sorted(packs.items()):
        bounds = tiers + [size]
        if bounds != sorted(bounds) or any(t % 4 for t in bounds):
            errors.append("%s: tier ends %s of %d" % (stem, tiers, size))
            continue

        def tier_of(ai):
            _nm, off, n = anims[ai]
            for t, end in enumerate(bounds):
                if off + n <= end:
                    if t and off < bounds[t - 1]:
                        errors.append("%s: %s runs across the end of tier %d"
                                      % (stem, anims[ai][0], t - 1))
                    return t
            return len(bounds)

        for ai in range(len(anims)):
            tier_of(ai)
        for i, ai in enumerate(sub):
            if ai >= 0 and tier_of(ai) > 0:
                errors.append("%s: SubMotion row %d plays %s, which is not "
                              "in tier 0" % (stem, i, anims[ai][0]))
        for tier, pack, table, mid in rules:
            if pack not in ("*", name):
                continue
            rows = sub if table == "sub" else main
            for i in (range(len(rows)) if mid is None else
                      [mid] if mid < len(rows) else []):
                if rows[i] >= 0 and tier_of(rows[i]) > tier:
                    errors.append("%s: anm_tiers.tsv puts %s row %d (%s) in "
                                  "tier %d, the .anm in %d"
                                  % (stem, table, i, anims[rows[i]][0], tier,
                                     tier_of(rows[i])))

    motion = status_motions(names)
    statuses = characters_statuses(names)
    checked = 0
    for kind in KINDS:
        stem = kind.lower()
        if stem not in packs:
            errors.append("%s: no pack in the romdisk" % stem)
            continue
        _name, anims, main, _sub, _tiers, _size = packs[stem]
        for status in sorted(statuses[kind]):
            mid = motion(kind, status)
            if mid < 0 or mid >= len(main) or main[mid] < 0:
                continue
            checked += 1
            if tier_of_pack(packs[stem], main[mid]) > 1:
                errors.append("%s: the Characters screen plays status %d "
                              "(motion %d, %s), which is not in tier 1"
                              % (stem, status, mid, anims[main[mid]][0]))
    return errors, len(packs), checked


def tier_of_pack(pack, ai):
    _name, anims, _main, _sub, tiers, size = pack
    _nm, off, n = anims[ai]
    for t, end in enumerate(tiers + [size]):
        if off + n <= end:
            return t
    return len(tiers) + 1


def main():
    out = check()
    if isinstance(out, list):
        print("\n".join(out))
        return 1
    errors, npacks, checked = out
    for e in errors:
        print(e)
    print("anm tiers: %d packs, %d Characters statuses; %d problems"
          % (npacks, checked, len(errors)))
    return 1 if errors and "--check" in sys.argv[1:] else 0


if __name__ == "__main__":
    sys.exit(main())

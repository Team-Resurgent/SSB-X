#!/usr/bin/env python3
"""ssb64-dc: the animations the shipped fighter .anm files carry, against
the figatree / AnimJoint scripts still in the ROM.

figatree_check.py proves the interpreter on the ROM's own bytes for every
animation but never reads an exported .anm; anm_tier_check.py reads the
.anm and only its layout. This is the missing leg: what the loader
(src/dc/fighter.c fighter_attach_anm / anims_relocate) is handed decodes
to the same animations the ROM's file does.

How it works. The real exporter runs in process for every fighter pack in
the romdisk's list (ssb_packexport.pack_fighter), and the pack and .anm it
returns are parsed back with the loader's own layouts (FPackHeader's
directory, FPackAttr's tier ends and slot directory, FPackAnim). Each
animation's ROM source is its file id, which the directory names by the
decomp's file name. Then, per animation:

  * structure: kind and word count are the ROM file's; the slot table
    (FPackAnimSlots) is the ROM table's, word for word; every driven
    entry of the skeleton table is one of those slots; the reloc list is
    exactly the AnimJoint words that hold a pointer.
  * words: every shipped word is the ROM's, except the pointer words
    (AnimJoint: the word holds the target's word index, which times four
    is the ROM reloc's target; figatree: only the SYInterpDesc spline
    tables a SetTranslateInterp names, compared by content below).
  * the event stream of every slot's script, decoded by an independent
    decoder (both kinds), is identical: every command word, every value
    word, every Loop/Jump/SetAnim target as an EVENT INDEX; a spline
    table and its three arrays by value.
  * a replay, frame by frame, through ssb_assets.Figatree / AnimJoint of
    every slot on both sides, from the bytes the loader holds after its
    own relocation, to the end of the script plus a few frames (a
    looping one: REPLAY_FRAMES). Both sides raising the same kind of
    error (the reference has no SetTranslateInterp or SetInterp) is
    equal.

Tiers. The .anm is cut at each FPackAttr.anm_tier_end the way a menu's
read cuts it; the loader's skip rule (an animation whose words run past
the prefix is left for a later read) is applied, and every animation
inside a prefix is relocated as anims_relocate does and replayed FROM
THAT PREFIX (the smallest one holding it), after its bytes are compared
with the whole file's. No animation may run across a tier end.

NOT covered: which animation a motion id plays (status_check, the tier
check), the skeleton table's slot-to-joint mapping beyond "is one of the
slots" (it comes from the fighter's setup_parts, which the pack check
owns), and a SetTranslateInterp's replay (neither reference implements it;
its table is compared by content instead).

Usage: python3 tools/check/anm_body_check.py [--rom <rom.z64>]
           [--only <stem>] [--frames N] [--jobs N]
"""
import contextlib
import io
import multiprocessing
import os
import re
import struct
import sys
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path[:0] = [os.path.join(os.path.dirname(HERE), d)
                for d in ("lib", "export", "check")]
import ssb_extract               # noqa: E402
import ssb_assets as A           # noqa: E402
import ssb_meshexport as M       # noqa: E402
import ssb_packexport as X       # noqa: E402
import pack_anim_check as PA     # noqa: E402

ANM_HEADER = 16
DIR_ENTRY = 64
REPLAY_FRAMES = 150
EXTRA_FRAMES = 4
FAILS = []
STATS = {"packs": 0, "anims": 0, "slots": 0, "words": 0, "events": 0,
         "replays": 0, "replay_unique": 0, "tier_anims": [0, 0, 0],
         "splines": 0}

# Makefile FIGHTERS / LADDER / POLY / BOSS: the stem -> decomp name
STEMS = ("mario fox donkey samus luigi link yoshi captain kirby pikachu "
         "purin ness gdonkey mmario nmario nfox ndonkey nsamus nluigi nlink "
         "nyoshi ncaptain nkirby npikachu npurin nness boss").split()
SPECIAL = {"gdonkey": "GDonkey", "mmario": "MMario"}


def fighter_name(stem):
    if stem in SPECIAL:
        return SPECIAL[stem]
    if stem.startswith("n") and stem not in ("ness",):
        return "N" + stem[1].upper() + stem[2:]
    return stem[0].upper() + stem[1:]


def fail(tag, msg):
    FAILS.append("%s: %s" % (tag, msg))


# -- the shipped pack, parsed back --------------------------------------------

class Pack:
    pass


def parse_pack(blob, anm):
    p = Pack()
    h = struct.unpack_from("<8s8I8I3ff8s8I", blob, 0)
    p.njoints, p.anim_count, off_anims = h[1], h[7], h[16]
    p.name = h[21].rstrip(b"\0").decode()
    off_attr, off_motion = h[22], h[23]
    magic, anm_id, anm_size = struct.unpack_from("<8s2I", anm, 0)
    assert magic == b"SSBANIM1" and len(anm) == ANM_HEADER + anm_size
    assert zlib.crc32(anm[ANM_HEADER:]) == anm_id
    at = blob.find(struct.pack("<2I", anm_id, anm_size), off_attr, off_motion)
    assert at >= 0, "anm id not in the attribute block"
    p.tiers = list(struct.unpack_from("<2I", blob, at + 8))
    p.size = anm_size
    p.anm = anm
    p.dirs = []
    for i in range(p.anim_count):
        nm, ow, nw, oe, kind, orl, nrl = struct.unpack_from(
            "<40s6I", blob, off_anims + DIR_ENTRY * i)
        p.dirs.append({
            "name": nm.rstrip(b"\0").decode(), "off": ow - ANM_HEADER,
            "n": nw, "kind": kind, "nreloc": nrl,
            "entries": list(struct.unpack_from("<%di" % p.njoints, blob, oe)),
            "relocs": list(struct.unpack_from("<%dI" % nrl, blob, orl))})
    # FPackAttr.off_slots: the u32 in the attribute block that names
    # anim_count contiguous (off, count) pairs
    cands = []
    for pos in range(off_attr, off_motion - 3, 4):
        v = struct.unpack_from("<I", blob, pos)[0]
        if v + 8 * p.anim_count > len(blob) or v % 4:
            continue
        pairs = [struct.unpack_from("<II", blob, v + 8 * i)
                 for i in range(p.anim_count)]
        if all(b <= 64 for _a, b in pairs) and all(
                pairs[i][0] == pairs[i - 1][0] + 4 * pairs[i - 1][1]
                for i in range(1, len(pairs))):
            cands.append((pos, v))
    assert len(cands) >= 1, "no slot directory"
    # the exporter lays the slot blob right after its directory
    for (_pos, v) in cands[:1]:
        for i in range(p.anim_count):
            off, n = struct.unpack_from("<II", blob, v + 8 * i)
            p.dirs[i]["slots"] = list(struct.unpack_from("<%di" % n, blob,
                                                         off))
    return p


def anim_bytes(d):
    return d["n"] * (4 if d["kind"] else 2)


# -- decoders ----------------------------------------------------------------

def popcount(x):
    return bin(x).count("1")


def u16s_rom(f):
    return list(struct.unpack(">%dH" % (len(f) // 2), f[:len(f) // 2 * 2]))


def s16(v):
    return v - 0x10000 if v & 0x8000 else v


def spline_rom(f, reloc, d):
    """a SYInterpDesc by value, ROM layout"""
    if d % 4 or d + 24 > len(f):
        return ("oob",)
    kind, n, unk, _p, ln, _k, _q = M.SPLINE_DESC.unpack_from(f, d)
    out = [kind, n, struct.pack(">f", unk), struct.pack(">f", ln)]
    for off, cnt in ((8, 3 * M.spline_points_num(kind, n) if kind < 4 else 0),
                     (16, n), (20, 5 * (n - 1) if kind else 0)):
        t = reloc.get(d + off)
        if t is None:
            out.append(None)
        elif t + 4 * cnt > len(f):
            out.append(("oob",))
        else:
            out.append(tuple(struct.unpack_from(">I", f, t + 4 * i)[0]
                             for i in range(cnt)))
    return tuple(out)


def spline_ship(b, d):
    """the same, from the native table (bytes of the shipped u16 words)"""
    if d % 4 or d + 24 > len(b):
        return ("oob",)
    kind, n, unk, p, ln, k, q = struct.unpack_from("<BxhfIfII", b, d)
    out = [kind, n, struct.pack(">f", unk), struct.pack(">f", ln)]
    for ptr, cnt in ((p, 3 * M.spline_points_num(kind, n) if kind < 4 else 0),
                     (k, n), (q, 5 * (n - 1) if kind else 0)):
        if ptr == 0:
            out.append(None)
        elif ptr * 2 + 4 * cnt > len(b):
            out.append(("oob",))
        else:
            out.append(tuple(struct.unpack_from("<I", b, ptr * 2 + 4 * i)[0]
                             for i in range(cnt)))
    return tuple(out)


def decode_fig(w, start, spline):
    """a figatree script (list of u16) as events; Loop targets are event
    indices. `spline(byte offset)` gives a table's content."""
    ev, seen, pc = [], {}, start
    while len(ev) < 20000:
        if pc in seen:
            ev.append(("revisit", seen[pc]))
            break
        if not 0 <= pc < len(w):
            ev.append(("oob",))
            break
        v = w[pc]
        op, flags, tog = (v >> 11) & 0x1F, (v >> 1) & 0x3FF, v & 1
        seen[pc] = len(ev)
        if op == A.FIG_END:
            ev.append((v,))
            break
        if op > A.FIG_SETFLAGS:
            ev.append(("badop", v))
            break
        if op == A.FIG_LOOP:
            if pc + 1 >= len(w):
                ev.append(("oob-loop", v))
                break
            t = pc + 1 + int(s16(w[pc + 1]) / 2)
            if t in seen:
                ev.append((v, "to", seen[t]))
                break
            ev.append((v, "fwd", len(ev) + 1))
            seen[pc + 1] = len(ev)
            pc = t
            continue
        pc += 1
        if op == A.FIG_SETTRANSLATEINTERP:
            if pc >= len(w):
                ev.append(("oob",))
                break
            d = int(s16(w[pc]) / 2)
            ev.append((v, w[pc], spline((pc + d) * 2)))
            STATS["splines"] += 1
            pc += 1
            continue
        vals = []
        if tog:
            vals.append(w[pc] if pc < len(w) else None)
            pc += 1
        if op not in (A.FIG_BLOCK, A.FIG_SETFLAGS, A.FIG_ADDLENGTH):
            n = (2 if op in (A.FIG_SETVALRATE_BLOCK, A.FIG_SETVALRATE)
                 else 1) * popcount(flags)
            vals += w[pc:pc + n]
            if pc + n > len(w):
                vals.append("oob")
            pc += n
        ev.append((v,) + tuple(vals))
    STATS["events"] += len(ev)
    return ev


def first_diff(a, b):
    for k in range(max(len(a), len(b))):
        x = a[k] if k < len(a) else None
        y = b[k] if k < len(b) else None
        if x != y:
            return k, x, y
    return None


def fmt(ev):
    if ev is None:
        return "<stream ended>"
    return " ".join("0x%X" % x if isinstance(x, int) and x > 255 else repr(x)
                    for x in ev)[:200]


# -- the loader's view of a shipped animation --------------------------------

def loader_view(buf, d):
    """buf: the .anm bytes the loader holds (a tier prefix or the whole
    file). Returns BE data bytes plus pointer map ({byte loc: byte target})
    after anims_relocate's rebase, or raises ValueError where the C
    refuses."""
    nb = anim_bytes(d)
    raw = buf[ANM_HEADER + d["off"]:ANM_HEADER + d["off"] + nb]
    if len(raw) != nb:
        raise ValueError("words run past the buffer")
    if d["kind"]:
        w = list(struct.unpack("<%dI" % d["n"], raw))
        ptr = {}
        for r in d["relocs"]:
            if r >= d["n"] or w[r] >= d["n"]:
                raise ValueError("reloc %d -> %d outside %d words"
                                 % (r, w[r] if r < d["n"] else -1, d["n"]))
            ptr[r * 4] = w[r] * 4
        return struct.pack(">%dI" % d["n"], *w), ptr, w
    w = list(struct.unpack("<%dH" % d["n"], raw))
    for r in d["relocs"]:
        idx = (w[r] | (w[r + 1] << 16)) if r + 1 < d["n"] else d["n"]
        if idx >= d["n"]:
            raise ValueError("spline reloc %d -> %d outside %d words"
                             % (r, idx, d["n"]))
    return struct.pack(">%dH" % d["n"], *w), {}, w


# -- replay ------------------------------------------------------------------

def norm_exc(e):
    return "%s: %s" % (type(e).__name__, re.sub(r"0x[0-9A-Fa-f]+|\d+", "N",
                                                str(e)))


def replay(kind, data, ptr, starts, frames):
    """[frame][slot] -> repr of the step's dict, or the normalised error"""
    objs = []
    for s in starts:
        if s < 0:
            objs.append(None)
        elif kind:
            objs.append(A.AnimJoint(data, ptr, s * 4))
        else:
            objs.append(A.Figatree(data, s))
    dead = [o is None for o in objs]
    out = []
    left = None
    for fr in range(frames):
        row = []
        for j, o in enumerate(objs):
            if dead[j]:
                row.append(None)
                continue
            try:
                row.append(repr(o.step()))
            except (ValueError, IndexError, struct.error, KeyError) as e:
                dead[j] = True
                row.append(norm_exc(e))
                continue
            if o.dead and left is None:
                pass
        out.append(row)
        if all(dead[j] or objs[j].dead for j in range(len(objs))):
            if left is None:
                left = EXTRA_FRAMES
            left -= 1
            if left < 0:
                break
        else:
            left = None
    return out


JOBS = []       # (tag, kind, rom (data, ptr), ship (data, ptr), starts)


def replay_job(i):
    tag, kind, rom, ship, starts, frames = JOBS[i]
    a = replay(kind, rom[0], rom[1], starts, frames)
    b = replay(kind, ship[0], ship[1], starts, frames)
    if len(a) != len(b):
        return "%s: replay ran %d frames from the ROM, %d shipped" \
            % (tag, len(a), len(b))
    for t, (ra, rb) in enumerate(zip(a, b)):
        if ra != rb:
            j = next(k for k in range(len(ra)) if ra[k] != rb[k])
            return ("%s: replay differs at frame %d slot %d: ROM %s shipped "
                    "%s" % (tag, t, j, str(ra[j])[:140], str(rb[j])[:140]))
    return None


# -- the check ---------------------------------------------------------------

def check_pack(rom, stem, files, ids, cache, frames):
    name = fighter_name(stem)
    with contextlib.redirect_stdout(io.StringIO()):
        blob, anm = X.pack_fighter(rom, name, "high")
    p = parse_pack(blob, anm)
    STATS["packs"] += 1
    tiers = p.tiers + [p.size]
    if tiers != sorted(tiers) or any(t % 4 for t in tiers):
        fail(stem, "tier ends %s of %d" % (p.tiers, p.size))
    # no two animations overlap; none runs across a tier end
    spans = sorted((d["off"], d["off"] + anim_bytes(d), d["name"])
                   for d in p.dirs)
    for (a0, a1, an), (b0, _b1, bn) in zip(spans, spans[1:]):
        if a1 > b0:
            fail(stem, "%s and %s overlap" % (an, bn))
    if spans and spans[-1][1] > p.size:
        fail(stem, "%s runs past the .anm" % spans[-1][2])

    def tier_of(d):
        for t, end in enumerate(tiers):
            if d["off"] + anim_bytes(d) <= end:
                if t and d["off"] < tiers[t - 1]:
                    fail(stem, "%s runs across the end of tier %d"
                         % (d["name"], t - 1))
                return t
        return len(tiers)

    # the whole set the exporter should keep: every directory name is a
    # ROM animation file, in file-id order
    fids = []
    for d in p.dirs:
        if d["name"] not in ids:
            fail(stem, "%s names no ROM animation file" % d["name"])
            fids.append(None)
        else:
            fids.append(ids[d["name"]])
    if [f for f in fids if f is not None] != sorted(
            f for f in fids if f is not None):
        fail(stem, "directory is not in file id order")

    for d, fid in zip(p.dirs, fids):
        if fid is None:
            continue
        tag = "%s/%s" % (stem, d["name"])
        STATS["anims"] += 1
        if fid not in cache:
            f = A.get_file(rom, fid, ssb_extract)
            e = ssb_extract.read_entry(rom, ssb_extract.RELOC_SEG, fid)
            reloc = A.walk_reloc(f, e["reloc_intern"])
            kind = 1 if fid in M.animjoint_file_ids() else 0
            cache[fid] = (bytes(f), reloc, kind)
        f, reloc, kind = cache[fid]
        wsize = 4 if kind else 2
        if d["kind"] != kind:
            fail(tag, "kind %d, the ROM file's is %d" % (d["kind"], kind))
            continue
        if d["n"] != len(f) // wsize:
            fail(tag, "%d words, the ROM file has %d" % (d["n"],
                                                         len(f) // wsize))
            continue
        nslots = A.figatree_slots(reloc)
        rslots = [-1 if reloc.get(i * 4) is None else reloc[i * 4] // wsize
                  for i in range(nslots)]
        if d["slots"] != rslots:
            fail(tag, "slot table %s, the ROM's %s" % (d["slots"], rslots))
            continue
        STATS["slots"] += len(rslots)
        have = set(s for s in rslots if s >= 0)
        for j, e in enumerate(d["entries"]):
            if e >= 0 and e not in have:
                fail(tag, "skeleton entry %d = word %d is not one of the "
                          "file's slots" % (j, e))
                break
        if sum(1 for e in d["entries"] if e >= 0) > len(have) + 0 and \
                len(set(e for e in d["entries"] if e >= 0)) > len(have):
            fail(tag, "more skeleton entries than slots")

        # tier: the prefix the smallest tier holding it is read from
        t = tier_of(d)
        if t >= len(tiers):
            fail(tag, "in no tier")
            continue
        STATS["tier_anims"][min(t, 2)] += 1
        pre = p.anm[:ANM_HEADER + tiers[t]]
        if pre[ANM_HEADER + d["off"]:ANM_HEADER + d["off"] + anim_bytes(d)] \
                != p.anm[ANM_HEADER + d["off"]:ANM_HEADER + d["off"]
                         + anim_bytes(d)]:
            fail(tag, "tier %d prefix differs from the whole file" % t)
        try:
            data, ptr, w = loader_view(pre, d)
        except ValueError as e:
            fail(tag, "loader relocation: %s" % e)
            continue

        # words
        ship_ptr = ptr
        if kind:
            want = {loc: t_ for loc, t_ in reloc.items() if loc >= nslots * 4}
            got = {r * 4: w[r] * 4 for r in d["relocs"]}
            if got != want:
                k = sorted(set(got) ^ set(want) or
                           [k for k in got if got[k] != want.get(k)])[0]
                fail(tag, "reloc set differs at byte 0x%X: shipped %s ROM %s"
                     % (k, got.get(k), want.get(k)))
                continue
            rw = struct.unpack(">%dI" % d["n"], f)
            bad = next((i for i in range(d["n"])
                        if i * 4 not in got and w[i] != rw[i]), None)
            if bad is not None:
                fail(tag, "word %d is 0x%X, the ROM's 0x%X"
                     % (bad, w[bad], rw[bad]))
                continue
            STATS["words"] += d["n"]
            rom_src = PA.Src(f, reloc, rom=True)
            ship_src = PA.Src(data, ship_ptr)
            bad_ev = None
            for s in rslots:
                if s < 0:
                    continue
                a = PA.decode(rom_src, s * 4, False)
                b = PA.decode(ship_src, s * 4, False)
                if a != b:
                    k, x, y = first_diff(a, b)
                    bad_ev = "slot start word %d: first differing event %d: " \
                        "ROM [%s] shipped [%s]" % (s, k, PA.describe(x),
                                                   PA.describe(y))
                    break
            if bad_ev:
                fail(tag, bad_ev)
                continue
        else:
            rw = u16s_rom(f)
            sb = struct.pack("<%dH" % d["n"], *w)
            # spline tables and their arrays are the only words that may
            # differ; every other word must be the ROM's
            spl = set()
            for s in rslots:
                if s >= 0:
                    try:
                        A.figatree_walk(f, s, spl)
                    except ValueError:
                        pass
            allowed = set()
            for dsc in spl:
                kk, nn = struct.unpack_from(">Bxh", f, dsc)[0:2] if \
                    dsc + 4 <= len(f) else (0, 0)
                allowed.update(range(dsc // 2, dsc // 2 + 12))
                for off, cnt in ((8, 3 * M.spline_points_num(kk, nn)
                                  if kk < 4 else 0), (16, nn),
                                 (20, 5 * (nn - 1) if kk else 0)):
                    t_ = reloc.get(dsc + off)
                    if t_ is not None:
                        allowed.update(range(t_ // 2, (t_ + 4 * cnt) // 2))
            bad = next((i for i in range(d["n"])
                        if w[i] != rw[i] and i not in allowed), None)
            if bad is not None:
                fail(tag, "word %d is 0x%04X, the ROM's 0x%04X"
                     % (bad, w[bad], rw[bad]))
                continue
            STATS["words"] += d["n"]
            bad_ev = None
            for s in rslots:
                if s < 0:
                    continue
                a = decode_fig(rw, s, lambda o: spline_rom(f, reloc, o))
                b = decode_fig(w, s, lambda o: spline_ship(sb, o))
                if a != b:
                    k, x, y = first_diff(a, b)
                    bad_ev = "slot start word %d: first differing event %d: " \
                        "ROM [%s] shipped [%s]" % (s, k, fmt(x), fmt(y))
                    break
            if bad_ev:
                fail(tag, bad_ev)
                continue

        # replay job (unique by content)
        key = (fid, data, tuple(sorted(ptr.items())), tuple(rslots))
        if key in cache.setdefault("jobs", {}):
            STATS["replays"] += 1
            continue
        cache["jobs"][key] = True
        STATS["replays"] += 1
        STATS["replay_unique"] += 1
        JOBS.append((tag + " (tier %d)" % t, kind, (f, reloc if kind else {}),
                     (data, ptr), rslots, frames))


def main():
    argv = sys.argv[1:]
    rom_path = argv[argv.index("--rom") + 1] if "--rom" in argv else \
        M.ROM_DEFAULT
    only = argv[argv.index("--only") + 1] if "--only" in argv else None
    frames = int(argv[argv.index("--frames") + 1]) if "--frames" in argv \
        else REPLAY_FRAMES
    jobs = int(argv[argv.index("--jobs") + 1]) if "--jobs" in argv else \
        min(os.cpu_count() or 1, 8)
    if not os.path.exists(rom_path):
        print("anm_body_check: no baserom at %s -- skipping" % rom_path)
        return 0
    rom = open(rom_path, "rb").read()
    # the directory's names are cut at FPACK_ANIM_NAME_LEN - 1 = 39
    ids = {}
    for fid, nm in X.all_anim_files().items():
        k = nm[:39]
        if k in ids:
            print("anm_body_check: names %s collide at 39 bytes" % k)
            return 1
        ids[k] = fid
    cache = {}
    files = None
    for stem in STEMS:
        if only and stem != only:
            continue
        try:
            check_pack(rom, stem, files, ids, cache, frames)
        except Exception as e:                       # noqa: BLE001
            fail(stem, "check crashed: %s: %s" % (type(e).__name__, e))
    # the replays, in parallel; fork so the workers inherit JOBS
    if jobs > 1 and JOBS:
        pool = multiprocessing.get_context("fork").Pool(jobs)
        results = pool.imap(replay_job, range(len(JOBS)), chunksize=4)
    else:
        pool, results = None, map(replay_job, range(len(JOBS)))
    for r in results:
        if r:
            FAILS.append(r)
    if pool:
        pool.close()
        pool.join()

    for f in FAILS:
        print("FAIL " + f)
    print("anm_body_check: %(packs)d packs, %(anims)d animations (%(slots)d "
          "slot scripts, %(words)d words, %(events)d events, %(splines)d "
          "spline tables); %(replays)d replays (%(replay_unique)d unique, "
          "%(frames)d frames max) from tier prefixes 0/1/whole = %(t)s"
          % dict(STATS, frames=frames, t="/".join(map(str,
                                                     STATS["tier_anims"]))))
    print("anm_body_check: %d failures" % len(FAILS))
    return 1 if FAILS else 0


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""ssb64-dc: the AnimJoint / MatAnimJoint scripts the exporters ship,
against the scripts still in the ROM.

slash_check and emblem_check replay one pack's MatAnimJoints each. This is
the same oracle over every exporter that slices, rebases or re-serialises a
script, because a mis-sliced script does not crash: the parser walks into
whatever the neighbour left behind and stops on some other opcode.

How it works. Each exporter is run for real, in process, with a thin
recording hook on the function that reads the ROM's script (so the check
knows each exported script's ROM source) and the function or writer that
produces the shipped words. Every exported script is then turned back into
bytes-plus-pointer-map exactly as src/dc/fighter.c's reloc walk (packs) or
stage.c's fixup walk (.stg blocks) does at load, and compared with the ROM
script by an independent decoder (decode() below, which shares nothing
with the exporters' walkers but the opcode constants):

  * the decoded event stream -- every command word, every value word, and
    every Jump/SetAnim target as an EVENT INDEX, not an address -- must be
    identical, to the End or to the loop-back. SetInterp's SYInterpDesc is
    compared by content, with its three arrays.
  * the stream is then replayed tic for tic through ssb_assets.AnimJoint /
    play_matanim (the reference interpreters), well past the end, and the
    outputs compared. Where both sides raise the same kind of error (the
    reference AnimJoint does not implement SetInterp) that counts as equal.

Coverage (what is mapped back to a ROM source, and how):
  stage   MPK1 / GRA1 / BWP1 / BPL1 / BTG1 / BMP1 script blocks: each entry
          to the (file, offset) read_anim_script took it from; the shipped
          block is re-read from the writer's own bytes (MPK1 from the .stg).
  stage   layer AnimJoints (the 'layers' FPackAnim, read back out of the
          .stg) against MPGroundDesc.anim_joints' per-joint slots.
  stage   layer / object / ground-actor MatAnimJoints, from the packs'
          FPackMObjs blobs, against the per-MObj tables they came from.
  effect, item, weapon packs: animjoint_block / matanim_block's output
          against the table or script offsets they were handed.
NOT covered: which MObj an item/effect/weapon's matanim `scripts` list
names (the exporters pick the ROM offsets and this check takes them as
given), and fighter .anm files (figatree, a different format).

Usage: python3 tools/check/pack_anim_check.py [--rom <rom.z64>] [--only X]
"""
import contextlib
import io
import os
import re
import struct
import sys
import tempfile

sys.path[:0] = [os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), d)
                for d in ("lib", "export", "check")]
import ssb_extract               # noqa: E402
import ssb_assets as A           # noqa: E402
import ssb_meshexport as M       # noqa: E402
import ssb_stageexport as S      # noqa: E402
import ssb_effectexport as EE    # noqa: E402
import ssb_itemmodelexport as IM  # noqa: E402

REPLAY_TICS = 24
STATS = {"scripts": 0, "events": 0, "replays": 0, "interps": 0}
FAILS = []


# -- the independent decoder -------------------------------------------------

class Src:
    """A byte image plus its pointer map, the way the game sees a file."""

    def __init__(self, data, ptr, rom=False):
        self.data = data
        self.ptr = ptr
        # In the ROM a target of 0 is a NULL (the file's own header, never
        # a script); in a packed image offset 0 is a real word.
        self.rom = rom

    def target(self, o):
        t = self.ptr.get(o)
        return None if t is None or (self.rom and t == 0) else t

    def u32(self, o):
        if o < 0 or o + 4 > len(self.data):
            raise IndexError(o)
        return struct.unpack_from(">I", self.data, o)[0]


def popcount(x):
    return bin(x).count("1")


def value_words(op, flags, mat):
    """How many value words follow a command word (objanim.c's two
    parsers), or None for an opcode the parser has no case for."""
    if op in (2, 12, 15, 16):                    # Wait, AddLength, flags, cmd16
        return 0 if not mat else (0 if op in (2, 12) else None)
    if op in (3, 4, 7, 8, 9, 10, 11):
        return popcount(flags)
    if op in (5, 6):
        return 2 * popcount(flags)
    if op == 17:
        return popcount(flags & 0x3FF) if not mat else None
    if op in (18, 19, 20, 21):
        return popcount(flags) if mat else None
    return None


INTERP_PTRS = ((0x08, 12), (0x10, 4), (0x14, 4))


def interp_content(src, d):
    """A SYInterpDesc by value: its words with the three pointers replaced
    by the arrays they point at."""
    if d is None:
        return None
    try:
        words = [src.u32(d + 4 * i) for i in range(6)]
        n = struct.unpack_from(">h", src.data, d + 2)[0]
        if not 0 <= n <= 256:
            return ("bad-points", n)
        out = []
        for off, span in INTERP_PTRS:
            words[off // 4] = 0
            t = src.target(d + off)
            out.append(None if t is None else tuple(
                src.u32(t + 4 * i) for i in range(n * span // 4)))
        return (tuple(words), tuple(out))
    except (IndexError, struct.error):
        return ("oob",)


def decode(src, start, mat):
    """The script at `start` as a list of events, following Jump/SetAnim,
    until End, a jump to nowhere, or a loop back. Jump targets are event
    indices."""
    events = []
    seen = {}
    pc = start
    while len(events) < 20000:
        if pc in seen:
            events.append(("revisit", seen[pc]))
            break
        try:
            w = src.u32(pc)
        except IndexError:
            events.append(("oob", pc if pc < 0 else "end"))
            break
        op, flags = (w >> 25) & 0x7F, (w >> 15) & 0x3FF
        seen[pc] = len(events)
        if op == 0:
            events.append((w,))
            break
        if op in (1, 14):
            t = src.target(pc + 4)
            if t is None:
                events.append((w, None))
                break
            if t in seen:
                events.append((w, seen[t]))
                break
            events.append((w, len(events) + 1))
            pc = t
            continue
        if op == 13 and not mat:
            STATS["interps"] += 1
            events.append((w, interp_content(src, src.target(pc + 4))))
            pc += 8
            continue
        n = value_words(op, flags, mat)
        if n is None:
            events.append(("badop", w))
            break
        try:
            vals = tuple(src.u32(pc + 4 + 4 * i) for i in range(n))
        except IndexError:
            events.append(("oob-values", w))
            break
        events.append((w, vals))
        pc += 4 + 4 * n
    STATS["events"] += len(events)
    return events


def describe(ev):
    if ev is None:
        return "<stream ended>"
    return " ".join("0x%08X" % x if isinstance(x, int) and x > 255 else
                    repr(x) for x in ev)


def norm_exc(e):
    return "%s: %s" % (type(e).__name__, re.sub(r"0x[0-9A-Fa-f]+|\d+", "N",
                                                str(e)))


def replay(src, off, mat):
    out = []
    try:
        if mat:
            for t in range(REPLAY_TICS):
                r = A.play_matanim(src.data, src.ptr, off, float(t))
                out.append(repr(r))
        else:
            j = A.AnimJoint(src.data, src.ptr, off)
            for t in range(REPLAY_TICS):
                out.append(repr(j.step()))
    except (ValueError, IndexError, struct.error, KeyError) as e:
        out.append(norm_exc(e))
    return out


def compare(label, rom, roff, exp, eoff, mat):
    """Record a failure for the first differing event or replayed tic."""
    STATS["scripts"] += 1
    a = decode(rom, roff, mat)
    b = decode(exp, eoff, mat)
    if a != b:
        i = next((k for k in range(max(len(a), len(b)))
                  if (a[k] if k < len(a) else None) !=
                  (b[k] if k < len(b) else None)), 0)
        FAILS.append("%s: first differing event %d: ROM [%s] packed [%s]"
                     % (label, i, describe(a[i] if i < len(a) else None),
                        describe(b[i] if i < len(b) else None)))
        return
    ra = replay(rom, roff, mat)
    rb = replay(exp, eoff, mat)
    STATS["replays"] += 1
    if ra != rb:
        t = next(k for k in range(min(len(ra), len(rb)))
                 if ra[k] != rb[k]) if any(
            x != y for x, y in zip(ra, rb)) else min(len(ra), len(rb))
        FAILS.append("%s: replay differs at tic %d: ROM %s packed %s"
                     % (label, t, ra[t][:160] if t < len(ra) else None,
                        rb[t][:160] if t < len(rb) else None))


def pack_src(words, relocs):
    """What fighter.c's reloc walk makes of a pack's word block: pointer
    words (given as word indices) become byte offsets."""
    data = bytearray(struct.pack(">%dI" % len(words), *words))
    ptr = {}
    for w in relocs:
        ptr[w * 4] = words[w] * 4
        struct.pack_into(">I", data, w * 4, words[w] * 4)
    return Src(bytes(data), ptr)


def blocks_src(anims):
    """.stg script blocks ([{words, fixups}], fixups = (word, target
    script index)) laid end to end, pointer words filled from the fixups
    as stage.c does. Returns (Src, base byte offset per script)."""
    base, data, pos = [], bytearray(), 0
    for a in anims:
        base.append(pos)
        data += struct.pack(">%dI" % len(a["words"]), *a["words"])
        pos += 4 * len(a["words"])
    ptr = {}
    for i, a in enumerate(anims):
        for (w, t) in a["fixups"]:
            ptr[base[i] + 4 * w] = base[t]
    return Src(bytes(data), ptr), base


def rom_src(f, reloc):
    return Src(bytes(f), reloc, rom=True)


# -- stage .stg / block parsers (the loaders' layouts) -----------------------

def parse_anim_blocks(blk, hdr_fmt, i_count, i_nfix, i_anims, i_fix,
                      head_fmt):
    hd = struct.unpack_from(hdr_fmt, blk, 0)
    n, nfix, oa, of = hd[i_count], hd[i_nfix], hd[i_anims], hd[i_fix]
    hsz = struct.calcsize(head_fmt)
    anims, p = [], oa
    for _ in range(n):
        h = struct.unpack_from(head_fmt, blk, p)
        nw = h[0]
        name = h[-1].split(b"\0")[0].decode()
        words = list(struct.unpack_from("<%dI" % nw, blk, p + hsz))
        anims.append({"name": name, "words": words, "fixups": []})
        p += hsz + 4 * nw
    for k in range(nfix):
        i, w, t = struct.unpack_from("<3I", blk, of + 12 * k)
        anims[i]["fixups"].append((w, t))
    return anims


# (magic -> parser args) for the four anim-carrying .stg blocks
BLOCK_FORMATS = {
    b"GRA1": ("<4s13I", 4, 5, 11, 12, "<II32s"),
    b"MPK1": ("<4s14I", 2, 3, 9, 10, "<II32s"),
    b"BWP1": ("<4s9I", 2, 3, 7, 8, "<II32s"),
    b"BPL1": ("<4s9I", 2, 3, 7, 8, "<II32s"),
    b"BTG1": ("<4s7I", 2, 3, 5, 6, "<I32s"),
    b"BMP1": ("<4s7I", 2, 3, 5, 6, "<I32s"),
}


def check_block(label, magic, blk, dict_anims, srcs):
    """Parse the writer's bytes and compare each script with its ROM
    source. `srcs` is {id(words): (f, intern, off)}."""
    fmt = BLOCK_FORMATS[magic]
    got = parse_anim_blocks(blk, *fmt)
    if len(got) != len(dict_anims):
        FAILS.append("%s: %d scripts written, %d exported"
                     % (label, len(got), len(dict_anims)))
        return
    exp, base = blocks_src(got)
    for i, (g, d) in enumerate(zip(got, dict_anims)):
        if g["name"] != d["name"][:32] or g["words"] != d["words"]:
            FAILS.append("%s: script %d (%s) written differently from "
                         "exported" % (label, i, d["name"]))
            continue
        s = srcs.get(id(d["words"]))
        if s is None:
            continue
        f, intern, off = s
        compare("%s script %d %s @0x%X" % (label, i, d["name"], off),
                rom_src(f, intern), off, exp, base[i], False)


# -- the recording hooks ------------------------------------------------------

class Ctx:
    stage = None
    srcs = {}                 # id(words) -> (f, intern, off)
    mobj_refs = {}            # id(words) -> (f, reloc, [rom script|None])
    sections = []             # (kind, magic, blob, anims)
    layer = []                # (lay result, args)
    layermat = {}             # id(result) -> (f, intern, {key: rom off})
    layermat_sec = []         # (blobs, scripts, mobjs)
    objmat = []               # (scripts, blobs)
    map_obj = []              # read_map_object results
    ground = None


def hook_stage():
    o_ras, o_rms = S.read_anim_script, S.read_mobj_scripts
    o_rgm, o_mps = S.read_ground_matanim, S.mobj_pack_section
    o_rla, o_rlm = S.read_layer_anims, S.read_layer_matanims
    o_lms = S.layer_mobj_section
    o_gas, o_tps = S.ground_actors_section, S.tree_pack_section
    o_bps = S.bonus_placements_section
    o_rmo, o_rg = S.read_map_object, S.read_ground

    def ras(f, intern, off):
        r = o_ras(f, intern, off)
        Ctx.srcs[id(r[0])] = (f, intern, off)
        return r

    def rms(f, reloc, table_off):
        r = o_rms(f, reloc, table_off)
        dobs = A._ptr_list(reloc, table_off)
        scripts = A._ptr_list(reloc, dobs[0]) or dobs
        Ctx.mobj_refs[id(r[0])] = (f, reloc, list(scripts))
        return r

    def rgm(f, reloc, mo, subs, where):
        r = o_rgm(f, reloc, mo, subs, where)
        refs = []
        for j in range(len(subs)):
            t = reloc.get(mo + j * 4)
            for mi in range(len(subs[j])):
                refs.append(reloc.get(t + mi * 4))
        Ctx.mobj_refs[id(r[0])] = (f, reloc, refs)
        return r

    def mps(baker, mobjsubs, scripts, *a, **k):
        r = o_mps(baker, mobjsubs, scripts, *a, **k)
        Ctx.objmat.append((list(scripts), r,
                           sum(len(l) for l in mobjsubs)))
        return r

    def rla(stage, f, intern, ground, spans, nodes):
        r = o_rla(stage, f, intern, ground, spans, nodes)
        if r is not None:
            Ctx.layer.append((r, f, intern, ground, list(spans)))
        return r

    def rlm(stage, f, intern, ground, spans, layer_subs):
        r = o_rlm(stage, f, intern, ground, spans, layer_subs)
        if r is not None:
            keys = {}
            for (layer, jbase, nj) in spans:
                tbl = ground["layer_matanims"][layer]
                if tbl is None or layer not in S.LAYER_ANIM_LAYERS:
                    continue
                subs = layer_subs.get(layer) or []
                for k in range(nj):
                    t = intern.get(tbl[1] + k * 4)
                    if t is None:
                        continue
                    nm = len(subs[k]) if k < len(subs) else 0
                    for mi, s in enumerate(A._ptr_list(intern, t)[:nm]):
                        keys[(layer, k, mi)] = s
            Ctx.layermat[id(r)] = (f, intern, keys)
        return r

    def lms(mobjs, joint_count, scripts):
        r = o_lms(mobjs, joint_count, scripts)
        Ctx.layermat_sec.append((r, scripts, mobjs))
        return r

    def gas(ga):
        r = o_gas(ga)
        if ga is not None:
            Ctx.sections.append(("GRA1", r, ga["anims"]))
        return r

    def tps(bw, magic=b"BWP1"):
        r = o_tps(bw, magic)
        if bw is not None:
            Ctx.sections.append((magic.decode(), r, bw["anims"]))
        return r

    def bps(bt, magic):
        r = o_bps(bt, magic)
        if bt is not None:
            Ctx.sections.append((magic.decode(), r, bt["anims"]))
        return r

    def rmo(rom, stage, ground):
        r = o_rmo(rom, stage, ground)
        if r is not None:
            Ctx.map_obj.append(r)
        return r

    S.read_anim_script, S.read_mobj_scripts = ras, rms
    S.read_ground_matanim, S.mobj_pack_section = rgm, mps
    S.read_layer_anims, S.read_layer_matanims = rla, rlm
    S.layer_mobj_section = lms
    S.ground_actors_section, S.tree_pack_section = gas, tps
    S.bonus_placements_section = bps
    S.read_map_object = rmo


def reset_ctx(stage):
    Ctx.stage = stage
    Ctx.srcs, Ctx.mobj_refs = {}, {}
    Ctx.sections, Ctx.layer, Ctx.layermat = [], [], {}
    Ctx.layermat_sec, Ctx.objmat, Ctx.map_obj = [], [], []


def run_quiet(fn, argv):
    old = sys.argv
    sys.argv = argv
    try:
        with contextlib.redirect_stdout(io.StringIO()):
            fn()
    finally:
        sys.argv = old


def read_stg_pack(stg):
    """(pack header tuple, pack bytes offset, extras offset) of a .stg."""
    plen, extras = struct.unpack_from("<2I", stg, 8)
    return 16, extras, plen


def check_stage_outputs(stage, stg):
    # script blocks the section writers handed back
    for (magic, blob, anims) in Ctx.sections:
        check_block("%s %s" % (stage, magic), magic.encode(), blob, anims,
                    Ctx.srcs)
    # MPK1, which main() serialises inline: find it in the .stg
    if Ctx.map_obj and Ctx.map_obj[0].get("anims"):
        _, extras, _ = read_stg_pack(stg)
        at = stg.rfind(b"MPK1", extras)
        if at < 0:
            FAILS.append("%s: MPK1 expected in the .stg, not found" % stage)
        else:
            check_block("%s MPK1" % stage, b"MPK1", stg[at:],
                        Ctx.map_obj[0]["anims"], Ctx.srcs)

    # layer AnimJoints: read the 'layers' directory entry back out of the pack
    for (lay, f, intern, ground, spans) in Ctx.layer:
        base = 16
        hd = struct.unpack_from("<8s8I8I3ff8s8I", stg, base)
        nanim, anim_off = hd[7], hd[16]
        if nanim != 1:
            FAILS.append("%s: layer pack has %d anims, expected 1"
                         % (stage, nanim))
            continue
        d = struct.unpack_from("<%dsIIIIII" % S.SP_ANIM_NAME_LEN, stg,
                               base + anim_off)
        _name, woff, nw, eoff, _kind, roff, nr = d
        words = list(struct.unpack_from("<%dI" % nw, stg, base + woff))
        ents = list(struct.unpack_from("<%di" % len(lay["entries"]), stg,
                                       base + eoff))
        rels = list(struct.unpack_from("<%dI" % nr, stg, base + roff))
        if words != lay["words"] or ents != lay["entries"] or \
                rels != lay["relocs"]:
            FAILS.append("%s layers: the .stg's FPackAnim differs from "
                         "what read_layer_anims returned" % stage)
            continue
        exp = pack_src(words, rels)
        rsrc = rom_src(f, intern)
        want = {}
        for (layer, jbase, nj) in spans:
            tbl = ground["layer_anims"][layer]
            if tbl is None or layer not in S.LAYER_ANIM_LAYERS:
                continue
            for k in range(nj):
                t = intern.get(tbl[1] + 4 * k)
                if t is not None:
                    want[jbase + k] = t
        for j, e in enumerate(ents):
            if j in want:
                if e < 0:
                    FAILS.append("%s layers: joint %d has a ROM script at "
                                 "0x%X and none packed" % (stage, j,
                                                           want[j]))
                    continue
                compare("%s layers joint %d @0x%X" % (stage, j, want[j]),
                        rsrc, want[j], exp, e * 4, False)
            elif e >= 0:
                FAILS.append("%s layers: joint %d has a packed script and "
                             "no ROM one" % (stage, j))

    # layer MatAnimJoints, from the serialised blobs
    for (blobs, scripts, mobjs) in Ctx.layermat_sec:
        _s, _j, _b, entry_b, words_b, reloc_b = blobs
        ents = struct.unpack("<%di" % (len(entry_b) // 4), entry_b)
        words = struct.unpack("<%dI" % (len(words_b) // 4), words_b)
        rels = struct.unpack("<%dI" % (len(reloc_b) // 4), reloc_b)
        exp = pack_src(list(words), rels)
        if scripts is None:
            if any(e >= 0 for e in ents):
                FAILS.append("%s layer matanim: entries with no scripts"
                             % stage)
            continue
        f, intern, keys = Ctx.layermat[id(scripts)]
        rsrc = rom_src(f, intern)
        by_index = {mobjs["index"][k]: k for k in keys}
        for i, e in enumerate(ents):
            if i in by_index:
                key = by_index[i]
                if e < 0:
                    FAILS.append("%s layer matanim MObj %s: ROM script, "
                                 "none packed" % (stage, key))
                    continue
                compare("%s layer matanim %s @0x%X" % (stage, key,
                                                       keys[key]),
                        rsrc, keys[key], exp, e * 4, True)
            elif e >= 0:
                FAILS.append("%s layer matanim: MObj %d packed with no ROM "
                             "script" % (stage, i))

    # object / ground-actor MatAnimJoints (mobj_pack_section's blobs)
    for (scripts, blobs, nmobj) in Ctx.objmat:
        _s, _j, _b, entry_b, words_b, reloc_b = blobs
        ents = struct.unpack("<%di" % (len(entry_b) // 4), entry_b)
        words = struct.unpack("<%dI" % (len(words_b) // 4), words_b)
        rels = struct.unpack("<%dI" % (len(reloc_b) // 4), reloc_b)
        exp = pack_src(list(words), rels)
        for a, sc in enumerate(scripts):
            ref = Ctx.mobj_refs.get(id(sc[0]))
            if ref is None:
                continue
            f, reloc, refs = ref
            rsrc = rom_src(f, reloc)
            for k in range(nmobj):
                e = ents[a * nmobj + k]
                r = refs[k] if k < len(refs) else None
                if r is None:
                    if e >= 0:
                        FAILS.append("%s objmat alt %d MObj %d: packed "
                                     "with no ROM script" % (stage, a, k))
                    continue
                if e < 0:
                    FAILS.append("%s objmat alt %d MObj %d: ROM script "
                                 "@0x%X, none packed" % (stage, a, k, r))
                    continue
                compare("%s objmat alt %d MObj %d @0x%X" % (stage, a, k, r),
                        rsrc, r, exp, e * 4, True)


def run_stages(rom_path, tmp, only):
    hook_stage()
    names = sorted(S.STAGES)
    n = 0
    for stage in names:
        if only and only not in "stage:" + stage:
            continue
        reset_ctx(stage)
        out = os.path.join(tmp, stage + ".stg")
        try:
            run_quiet(S.main, ["ssb_stageexport", "--rom", rom_path,
                               "--out", out, "--stage", stage])
        except (SystemExit, Exception) as e:      # noqa: B902
            FAILS.append("stage %s: export did not run: %s"
                         % (stage, str(e)[:200]))
            continue
        check_stage_outputs(stage, open(out, "rb").read())
        n += 1
    # the shared Board the Platforms trees (BPL1)
    if not only or only in "stage:platforms":
        reset_ctx("platforms")
        out = os.path.join(tmp, "plat.pak")
        try:
            run_quiet(S.main, ["ssb_stageexport", "--rom", rom_path,
                               "--out", out, "--platforms"])
            check_stage_outputs("platforms", open(out, "rb").read())
            n += 1
        except (SystemExit, Exception) as e:      # noqa: B902
            FAILS.append("platforms: export did not run: %s" % str(e)[:200])
    return n


# -- effect / item / weapon packs --------------------------------------------

REC = []


def hook_blocks():
    o_aj, o_ma = EE.animjoint_block, EE.matanim_block

    def aj(f, reloc, anim_off, njoints, who, patch=None, direct=False):
        r = o_aj(f, reloc, anim_off, njoints, who, patch=patch,
                 direct=direct)
        REC.append(("aj", who, f, reloc, anim_off, njoints, patch, direct,
                    r))
        return r

    def ma(f, reloc, scripts, who):
        r = o_ma(f, reloc, scripts, who)
        REC.append(("ma", who, f, reloc, list(scripts), r))
        return r

    EE.animjoint_block, EE.matanim_block = aj, ma


def check_recorded(tag):
    for rec in REC:
        try:
            if rec[0] == "aj":
                _k, who, f, reloc, off, nj, patch, direct, (ents, words,
                                                            rels) = rec
                table = [off] if direct else \
                    [reloc.get(off + 4 * k) for k in range(nj)]
                for k, o in (patch or {}).items():
                    table[k] = o
                exp = pack_src(words, rels)
                rsrc = rom_src(f, reloc)
                for k, t in enumerate(table):
                    if t is None:
                        if ents[k] >= 0:
                            FAILS.append("%s %s joint %d: packed with no "
                                         "ROM script" % (tag, who, k))
                        continue
                    if ents[k] < 0:
                        FAILS.append("%s %s joint %d: ROM script @0x%X, "
                                     "none packed" % (tag, who, k, t))
                        continue
                    compare("%s %s AnimJoint joint %d @0x%X"
                            % (tag, who, k, t), rsrc, t, exp,
                            ents[k] * 4, False)
            else:
                _k, who, f, reloc, scripts, (ents, words, rels) = rec
                exp = pack_src(words, rels)
                rsrc = rom_src(f, reloc)
                for k, t in enumerate(scripts):
                    if t is None:
                        if ents[k] >= 0:
                            FAILS.append("%s %s MObj %d: packed with no ROM "
                                         "script" % (tag, who, k))
                        continue
                    compare("%s %s MatAnimJoint %d @0x%X"
                            % (tag, who, k, t), rsrc, t, exp,
                            ents[k] * 4, True)
        except Exception as e:                      # noqa: B902
            FAILS.append("%s %s: check raised %s" % (tag, rec[1], e))
    n = len(REC)
    del REC[:]
    return n


def run_effects(rom_path, tmp):
    names = ["halo", "quake", "orbs", "slash", "spark", "mdust",
             "deadexplode", "shield", "yoshiegg", "impactwave", "fireball",
             "blaster", "shock", "firespark", "arwinglaser"]
    for d in (EE.WPQUAD, EE.EFENTRY, EE.WPTREE, EE.WPLINK, EE.WPFLAT):
        names += sorted(d)
    done = 0
    for w in names:
        del REC[:]
        try:
            run_quiet(EE.main, ["ssb_effectexport", "--rom", rom_path,
                                "--out", os.path.join(tmp, w + ".mdl"),
                                "--what", w])
        except (SystemExit, Exception) as e:      # noqa: B902
            FAILS.append("effect %s: export did not run: %s"
                         % (w, str(e)[:200]))
            continue
        check_recorded("effect/weapon " + w)
        done += 1
    return done


def run_items(rom_path, tmp):
    done = 0
    for tag, extra in (("item", ["--all"]), ("monster weapon",
                                             ["--weapons"]),
                       ("stage item", ["--stage-items"])):
        del REC[:]
        try:
            run_quiet(IM.main, ["ssb_itemmodelexport", "--rom", rom_path,
                                "--outdir", tmp] + extra)
        except (SystemExit, Exception) as e:      # noqa: B902
            FAILS.append("%s export did not run cleanly: %s"
                         % (tag, str(e)[:200]))
        check_recorded(tag)
        done += 1
    return done


def main():
    argv = sys.argv[1:]
    rom_path = M.ROM_DEFAULT
    if "--rom" in argv:
        rom_path = argv[argv.index("--rom") + 1]
    only = argv[argv.index("--only") + 1] if "--only" in argv else None
    if not os.path.exists(rom_path):
        print("pack_anim_check: no baserom at %s -- skipping" % rom_path)
        return
    hook_blocks()
    with tempfile.TemporaryDirectory() as tmp:
        ns = run_stages(rom_path, tmp, only) if not only or \
            only.startswith("stage") else 0
        ne = ni = 0
        if not only or only == "effects":
            ne = run_effects(rom_path, tmp)
        if not only or only == "items":
            ni = run_items(rom_path, tmp)
    print("  %d stage exports, %d effect/weapon packs, %d item runs; "
          "%d scripts compared (%d events decoded, %d replayed, %d "
          "SetInterp descriptors)"
          % (ns, ne, ni, STATS["scripts"], STATS["events"],
             STATS["replays"], STATS["interps"]))
    if FAILS:
        for m in FAILS[:60]:
            print("FAIL " + m)
        sys.exit("pack_anim_check: %d problems" % len(FAILS))
    print("pack_anim_check: every exported AnimJoint/MatAnimJoint script "
          "plays as the ROM's does")


if __name__ == "__main__":
    main()

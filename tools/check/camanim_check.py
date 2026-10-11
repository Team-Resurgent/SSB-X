#!/usr/bin/env python3
"""ssb64-dc: the camera-animation banks (.cam) against the scripts still in
the ROM.

ssb_camanimexport.py cuts each CamAnimJoint (an AObjEvent32 script, run by
gcParseCObjCamAnimJoint) out of its relocData file into a bank that
src/dc/camanim.c loads, rebasing pointer words to word indices in the bank
and back to addresses at load. This is the pack_anim_check oracle over
those banks:

  * every bank is built for real (ssb_camanimexport.build) and its bytes
    parsed back with an independent reader of SSBCAM1's layout; each
    directory entry is turned into bytes-plus-pointer-map exactly as
    camanim_bank_load's reloc pass does;
  * the ROM source of each entry is named by the (file, offset) in BANKS,
    and that offset is cross-checked against the decomp's own link label
    in src/dc/decomp/reloc_data.us.h, read here by a second regex;
  * pack_anim_check's independent decoder compares the two event streams
    (commands, value words, Jump/SetAnim targets as event indices) and the
    directory's word count must equal the stream's extent exactly, so a
    neighbour's words cannot ride along or be cut off;
  * both are replayed through ssb_assets.AnimJoint to the script's End and
    30 tics past it. The reference interpreter is per-track-bit, not
    per-joint, so it covers camera scripts (tracks 25..34 = flag bits 0..9)
    unchanged; it models the parse, not gcParseCObjCamAnimJoint's
    CObj-specific writes.
NOT covered: the C parser (objanim.c's CObj path) itself -- no C oracle
exists for it; and which script a scene asks camanim_get for.

Usage: python3 tools/check/camanim_check.py [--rom <rom.z64>]
"""
import os
import re
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path[:0] = [os.path.join(os.path.dirname(HERE), d)
                for d in ("lib", "export", "check")]
import ssb_extract                # noqa: E402
import ssb_assets as A            # noqa: E402
import ssb_camanimexport as CX    # noqa: E402
import pack_anim_check as PA      # noqa: E402

HDR = os.path.join(os.path.dirname(os.path.dirname(HERE)), "src", "dc",
                   "decomp", "reloc_data.us.h")
STATS = {"banks": 0, "scripts": 0, "labels": 0}
FAILS = PA.FAILS


def header_labels():
    out = {}
    for m in re.finditer(r"^extern int (ll\w+CamAnimJoint|llMVOpeningRun\w+"
                         r"AnimJoint); // (0x[0-9a-fA-F]+)$",
                         open(HDR).read(), re.M):
        out[m.group(1)] = int(m.group(2), 16)
    return out


def parse_bank(blob):
    magic, count, nwords, nreloc, o_dir, o_words, o_rel = \
        struct.unpack_from("<8s6I", blob, 0)
    if magic != b"SSBCAM1\0":
        raise ValueError("bad magic")
    ents = []
    for i in range(count):
        nm, first, n = struct.unpack_from("<32sII", blob, o_dir + 40 * i)
        ents.append((nm.split(b"\0")[0].decode(), first, n))
    words = list(struct.unpack_from("<%dI" % nwords, blob, o_words))
    rels = list(struct.unpack_from("<%dI" % nreloc, blob, o_rel))
    return ents, words, rels


def main():
    argv = sys.argv[1:]
    rom_path = argv[argv.index("--rom") + 1] if "--rom" in argv else \
        PA.M.ROM_DEFAULT
    if not os.path.exists(rom_path):
        print("camanim_check: no baserom at %s -- skipping" % rom_path)
        return
    rom = open(rom_path, "rb").read()
    labels = header_labels()
    PA.REPLAY_TICS = 0                      # replaced per script below
    cache = {}
    for bank, (_out, entries) in sorted(CX.BANKS.items()):
        try:
            blob = CX.build(rom, entries, verbose=False)
            names, words, rels = parse_bank(blob)
        except (SystemExit, Exception) as e:     # noqa: B902
            FAILS.append("%s: export/parse failed: %s" % (bank, e))
            continue
        STATS["banks"] += 1
        if [n for n, _, _ in names] != [e[0] for e in entries]:
            FAILS.append("%s: directory names differ from BANKS" % bank)
            continue
        exp = PA.pack_src(words, rels)
        used = set()
        for ent, (nm, first, nw) in zip(entries, names):
            _n, fid, off = ent[:3]
            label = ent[3] if len(ent) > 3 else \
                "ll%sCamAnimJoint" % CX._label_stem(_n, fid)
            tag = "%s/%s (file %d @0x%X)" % (bank, nm, fid, off)
            if label in labels:
                STATS["labels"] += 1
                if labels[label] != off:
                    FAILS.append("%s: header label %s says 0x%X"
                                 % (tag, label, labels[label]))
            if fid not in cache:
                f = A.get_file(rom, fid, ssb_extract)
                e = ssb_extract.read_entry(rom, ssb_extract.RELOC_SEG, fid)
                cache[fid] = PA.rom_src(f, A.walk_reloc(f, e["reloc_intern"]))
            rsrc = cache[fid]
            mat = False
            a = PA.decode(rsrc, off, mat)
            # extent: the directory span must be exactly what the stream
            # reads (command words + their values), no more, no less
            span = 0
            for ev in a:
                if len(ev) == 1 or not isinstance(ev[0], int):
                    span += 1
                    continue
                n = PA.value_words(((ev[0] >> 25) & 0x7F),
                                   (ev[0] >> 15) & 0x3FF, mat)
                span += 1 + (1 if (ev[0] >> 25) & 0x7F in (1, 14, 13)
                             else (n or 0))
            if span != nw:
                FAILS.append("%s: directory says %d words, the ROM "
                             "script is %d" % (tag, nw, span))
            for k in range(first, first + nw):
                if k in used:
                    FAILS.append("%s: word %d claimed twice" % (tag, k))
                    break
                used.add(k)
            # replay length: every event may Wait; run to End + 30 tics
            PA.REPLAY_TICS = 3000
            PA.compare(tag, rsrc, off, exp, first * 4, mat)
            STATS["scripts"] += 1
        if len(used) != len(words):
            FAILS.append("%s: %d bank words, directory covers %d"
                         % (bank, len(words), len(used)))
    print("  %d banks, %d scripts compared (%d events decoded, %d replayed "
          "3000 tics each, %d link labels matched)"
          % (STATS["banks"], STATS["scripts"], PA.STATS["events"],
             PA.STATS["replays"], STATS["labels"]))
    if FAILS:
        for m in FAILS[:60]:
            print("FAIL " + m)
        sys.exit("camanim_check: %d problems" % len(FAILS))
    print("camanim_check: every exported camera script plays as the ROM's "
          "does")


if __name__ == "__main__":
    main()

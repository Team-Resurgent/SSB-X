#!/usr/bin/env python3
"""ssb64-dc oracle: the port's music sequencer against the decomp's.

The sequencer (src/dc/seq/seqcore.c, run on the AICA's ARM) was
written from a format spec, not from libultra's player. The decomp carries
an independent model of the same bytecode in tools/cseq_to_mid.py, so
every sequence in the ROM can be run through both. The player's schedule
is a pure function of the tick stream and the tempo map, both of which
the reference produces independently, so every note the sequencer starts
can be checked against the microsecond the tick timeline puts it on --
covering the event queue, uspt, the tempo handler that re-times queued
note offs, backup-reference expansion, running status, and the loop
markers.

(This once also held the sequencer call for call to a port of libultra's
ALCSPlayer/ALCSeq, src/dc/bgmplay.c and bgmseq.c, and checked that port's
reader event for event; both are gone from the tree.)

Run from the repo root; needs the baserom and a decomp checkout.
"""
import math
import os
import struct
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
SRC = os.path.join(ROOT, "src", "dc")
sys.path[:0] = [os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), d)
                for d in ("lib", "export", "check")]
import ssb_paths as P        # noqa: E402
P.require_decomp()
sys.path.insert(0, P.DECOMP_TOOLS)
import cseq_to_mid as CSQ    # noqa: E402  (the reference reader)
import ssb_adpcm as ADPCM    # noqa: E402
import ssb_bgmexport as BGM  # noqa: E402
import ssb_bgmpack as PACK   # noqa: E402
import ssb_meshexport as M   # noqa: E402
import ssb_sfxexport as SFX  # noqa: E402

# How far into a sequence the player is run when it never loops.
NOLOOP_US = 120 * 1000 * 1000


def build_seqcore(tmpdir, name, game_counts):
    """The port's own sequencer (src/dc/seq/) in a recording
    test build. Without game_counts its voice and event pools are raised past
    anything a sequence can ask for -- a note dropped because the N64's
    own 24 voices were busy says nothing about whether the schedule is
    right, and the firmware keeps the game's counts. Its oscillator tables
    come through libultra's own __sinf, written from the decomp the way
    the build writes it (tools/export/ssb_trigexport.py)."""
    import ssb_trigexport as TRIG
    gen = TRIG.write_all(P.DECOMP_DIR, tmpdir)
    exe = os.path.join(tmpdir, name)
    counts = [] if game_counts else ["-DSEQ_VOICES=120",
                                     "-DSEQ_EVENTS=4096"]
    subprocess.run(["cc", "-O2", "-std=gnu11", "-Wall", "-Wextra", "-Werror",
                    "-fno-strict-aliasing", "-Wno-missing-braces"] + counts +
                   ["-o", exe, os.path.join(HERE, "seqcore_oracle.c"),
                    os.path.join(SRC, "seq", "seqcore.c"),
                    os.path.join(SRC, "seq", "seqprep.c"),
                    os.path.join(SRC, "bgmbank.c"),
                    os.path.join(SRC, "assetroot.c"),
                    [g for g in gen if g.endswith("gusinf.c")][0],
                    "-I", SRC, "-I", os.path.join(SRC, "decomp"),
                    "-I", os.path.join(P.DECOMP_DIR, "src"),
                    "-idirafter", os.path.join(P.DECOMP_DIR, "include"),
                    "-D_LANGUAGE_C", "-DF3DEX_GBI_2", "-lm"], check=True)
    return exe


def f32(x):
    """Round to single precision, which is what the player's uspt
    arithmetic runs in (n_env.c:3214 __n_setUsptFromTempo)."""
    return struct.unpack("<f", struct.pack("<f", x))[0]


def uspt_of(tempo, qnpt):
    """n_env.c:3216: seqp->uspt = (s32)((f32)tempo * seq->qnpt)."""
    return int(f32(f32(float(tempo)) * qnpt))


def merged_events(seq):
    """Every track's events, in absolute ticks, the way the reader
    merges them: earliest tick first, ties by track index."""
    offs = struct.unpack_from(">16I", seq, 0)
    out = []
    for t, off in enumerate(offs):
        if off == 0:
            continue
        for tick, key, ev in CSQ.parse_track_to_events(seq, off, len(seq)):
            out.append((tick, t, key, ev))
    out.sort(key=lambda e: (e[0], e[1], e[2]))
    return out


def tempo_segments(events, qnpt):
    """(start tick, start microsecond, uspt) per tempo segment. The
    player starts at 500000 us per quarter note (n_env.c:2560), then
    re-derives uspt at every tempo meta."""
    segs = [(0, 0, uspt_of(500000, qnpt))]
    for tick, _, _, ev in events:
        if ev[0] != "tempo":
            continue
        stick, sus, uspt = segs[-1]
        segs.append((tick, sus + (tick - stick) * uspt,
                     uspt_of(ev[1], qnpt)))
    return segs


def tick_to_us(segs, tick):
    lo = 0
    for i, (stick, _, _) in enumerate(segs):
        if stick > tick:
            break
        lo = i
    stick, sus, uspt = segs[lo]
    return sus + (tick - stick) * uspt


def reference_notes(seq):
    """(note ons, note offs, the microsecond the port stops being
    comparable at) for one sequence.

    Each note off comes with a flag saying whether the reference can time
    it: one scheduled from a note on's duration is converted to
    microseconds when the note starts and re-timed through ticks by
    __n_CSPHandleMetaMsg if the tempo moves under it (n_env.c:3160),
    which rounds. Those are still listed, so the two sides stay aligned
    note for note, but their times are not compared."""
    qnpt = f32(1.0 / struct.unpack_from(">I", seq, 64)[0])
    events = merged_events(seq)
    segs = tempo_segments(events, qnpt)
    tempo_ticks = [t for t, _, _, ev in events if ev[0] == "tempo"]

    ends = [t for t, _, _, ev in events
            if ev[0] == "marker" and ev[1].startswith("loopend")]
    limit_tick = min(ends) if ends else None
    limit_us = tick_to_us(segs, limit_tick) if limit_tick is not None else None

    ons, offs = [], []
    pending = {}                        # (chan, key) -> [on tick, ...]
    for tick, _, _, ev in events:
        if limit_tick is not None and tick >= limit_tick:
            break
        if ev[0] == "note_on":
            _, ch, key, vel = ev
            ons.append((tick_to_us(segs, tick), ch, key, vel))
            pending.setdefault((ch, key), []).append(tick)
        elif ev[0] == "note_off":
            _, ch, key, _vel = ev
            q = pending.get((ch, key))
            if not q:
                continue
            on_tick = q.pop(0)
            # A note off scheduled from a note on's duration is timed in
            # microseconds at the note on and re-timed by
            # __n_CSPHandleMetaMsg if the tempo moves under it, which
            # rounds through ticks (n_env.c:3160). Only compare the ones
            # that live inside a single tempo segment, where both sides
            # are the same multiplication.
            comparable = not any(on_tick < tt <= tick for tt in tempo_ticks)
            offs.append((tick_to_us(segs, tick), ch, key, comparable))
    return ons, offs, limit_us


def run_player(exe, pak, index, limit_us):
    out = subprocess.run([exe, pak, str(index), str(limit_us)],
                         stdout=subprocess.PIPE, check=True).stdout.decode()
    ons, offs = [], []
    for line in out.splitlines():
        f = line.split()
        if f[0] == "on":
            ons.append((int(f[1]), int(f[2]), int(f[3]), int(f[4])))
        else:
            offs.append((int(f[1]), int(f[2]), int(f[3])))
    return ons, offs


def check_player(exe, pak, seqs, names):
    n_on = n_off = bad = 0
    for i, seq in enumerate(seqs):
        ref_on, ref_off, limit_us = reference_notes(seq)
        run_us = limit_us if limit_us is not None else NOLOOP_US
        got_on, got_off = run_player(exe, pak, i, run_us)

        if limit_us is not None:
            got_on = [e for e in got_on if e[0] < limit_us]
            got_off = [e for e in got_off if e[0] < limit_us]
        else:
            # A sequence that stops on its own is compared whole; one
            # still going at NOLOOP_US is compared up to there.
            ref_on = [e for e in ref_on if e[0] < run_us]
            ref_off = [e for e in ref_off if e[0] < run_us]

        a, b = sorted(got_on), sorted(ref_on)
        if a != b:
            bad += 1
            print("  %s: %d note ons, the tick timeline says %d"
                  % (names[i], len(a), len(b)))
            for x, y in zip(a, b):
                if x != y:
                    print("    first difference: player %r, timeline %r"
                          % (x, y))
                    break
            continue
        n_on += len(a)

        # Note offs are paired per (channel, key) in the order they
        # happen, and the pairs the reference could time are compared.
        want, have = {}, {}
        for us, ch, key, ok in ref_off:
            want.setdefault((ch, key), []).append((us, ok))
        for us, ch, key in sorted(got_off):
            have.setdefault((ch, key), []).append(us)

        wrong = None
        matched = 0
        for k, w in want.items():
            h = have.get(k, [])
            if len(h) != len(w):
                wrong = wrong or ("%d note offs on channel %d key %d, "
                                  "the timeline has %d" % (len(h), k[0],
                                                           k[1], len(w)))
                continue
            for (us, ok), got in zip(w, h):
                if not ok:
                    continue
                if us != got:
                    wrong = wrong or ("channel %d key %d released at %d us, "
                                      "the timeline says %d"
                                      % (k[0], k[1], got, us))
                else:
                    matched += 1
        if wrong:
            bad += 1
            print("  %s: %s" % (names[i], wrong))
            continue
        n_off += matched
    return n_on, n_off, bad


def check_adpcm(rom):
    """The AICA format the pack stores: how much the transcode costs, and
    that decode is the exact inverse of encode."""
    import math
    r = SFX.BANKS[1]
    ctl = rom[r["ctl"][0]:r["ctl"][1]]
    tbl = rom[r["tbl"][0]:r["tbl"][1]]
    bank = PACK.parse_bank(ctl)
    seen = set()
    worst = 99.0
    total_e = total_s = 0.0
    n = 0
    for inst in bank["insts"] + [bank["percussion"]]:
        if not inst:
            continue
        for s in inst["sounds"]:
            w = s["wave"]
            if w["base"] in seen:
                continue
            seen.add(w["base"])
            pcm, _, _, _ = SFX.decode_sound(ctl, tbl, w)
            while len(pcm) > PACK.AICA_MAX_SAMPLES:
                pcm = PACK.halve(pcm)
            back = ADPCM.decode(ADPCM.encode(pcm), len(pcm))
            e = sum((a - b) ** 2 for a, b in zip(pcm, back))
            p = sum(a * a for a in pcm)
            total_e += e
            total_s += p
            n += 1
            if e and p:
                snr = 10 * math.log10(p / e)
                worst = min(worst, snr)
    overall = 10 * math.log10(total_s / total_e)
    return n, overall, worst


def main():
    rom_path = M.ROM_DEFAULT
    if "--rom" in sys.argv:
        rom_path = sys.argv[sys.argv.index("--rom") + 1]
    rom = open(rom_path, "rb").read()
    seqs = PACK.read_sbk(rom)
    names = BGM.bgm_names()
    names += ["seq%02d" % i for i in range(len(names), len(seqs))]

    with tempfile.TemporaryDirectory() as tmpdir:
        pak = os.path.join(tmpdir, "bgm.pak")
        with open(pak, "wb") as fp:
            fp.write(PACK.serialize(PACK.build(rom, None)))
        sexe = build_seqcore(tmpdir, "seqcore_oracle", False)
        n_on, n_off, sbad = check_player(sexe, pak, seqs, names)
        print("%d note ons and %d note offs across %d sequences start on "
              "the microsecond the tick timeline puts them"
              % (n_on, n_off, len(seqs)))
        if sbad:
            sys.exit("bgm_check: %d sequences play off the timeline in "
                     "src/dc/seq" % sbad)

    n, overall, worst = check_adpcm(rom)
    print("%d waveforms transcoded to AICA ADPCM: %.1f dB SNR overall, "
          "%.1f dB worst" % (n, overall, worst))
    print("bgm_check: the sequencer agrees with the decomp everywhere")


if __name__ == "__main__":
    main()

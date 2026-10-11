#!/usr/bin/env python3
"""ssb64-dc: BGM rendered to WAV at build time.

The route the project chose for music: SSB64's 47 sequences are static per
scene, so instead of porting the n_audio compressed-MIDI sequencer, each
track is synthesized *here* -- the game's own sequence bytes
(S1_music.sbk, decoded with the decomp's engine-faithful cseq reader) over
the game's own instruments (B1_sounds1.ctl/.tbl, VADPCM decoded once) --
and the Dreamcast just streams the result from disc. A sequencer port
stays possible later if fidelity demands it; nothing here forecloses it.

What the synth honours: tempo changes, program changes, channel volume and
pan (CC 7/10), the percussion bank on channel 10, per-sound keymaps
(key/velocity ranges, keybase, cents detune), sample loops, and the AL
envelope (attack/decay to sustain, release on note-off). Vibrato/tremolo
LFOs and pitch bends are not rendered yet -- noted, audible-quality
polish, not structure.

Usage: python3 tools/export/ssb_bgmexport.py --seq <index|name> --out <file.wav>
                                      [--rom <rom.z64>] [--seconds N]
"""
import math
import os
import re
import struct
import sys
from collections import Counter

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path[:0] = [os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), d)
                for d in ("lib", "export", "check")]
import ssb_paths as P        # noqa: E402
P.require_decomp()
sys.path.insert(0, P.DECOMP_TOOLS)
import cseq_to_mid as CSQ    # noqa: E402
import ssb_meshexport as M   # noqa: E402
import ssb_sfxexport as SFX  # noqa: E402

SBK_ROM = (0xB277B0, 159248)         # S1_music.sbk (US)
RATE = 32000                         # the bank's rate == the N64 output rate
GMSOUND_H = P.GMSOUND_H


def bgm_names():
    src = open(GMSOUND_H).read()
    body = re.search(r"typedef enum gmMusicID\s*\{(.*?)\}", src, re.S)
    names = []
    for line in body.group(1).splitlines():
        m = re.match(r"\s*nSYAudioBGM(\w+)\s*,", line)
        if m and "=" not in line:
            names.append(m.group(1))
    return names


# ---- bank ----------------------------------------------------------------

def u32(d, o):
    return struct.unpack_from(">I", d, o)[0]


def parse_music_bank(rom):
    """B1_sounds1 with full structure: per-program instrument sound lists
    (plus percussion as program -1), each sound carrying envelope, keymap,
    pan/volume and its wavetable."""
    r = SFX.BANKS[1]
    ctl = rom[r["ctl"][0]:r["ctl"][1]]
    tbl = rom[r["tbl"][0]:r["tbl"][1]]
    bank_off = u32(ctl, 4)
    inst_count = struct.unpack_from(">h", ctl, bank_off)[0]
    percussion = u32(ctl, bank_off + 8)

    def read_inst(inst):
        sounds = []
        if inst == 0:
            return sounds
        n = struct.unpack_from(">h", ctl, inst + 14)[0]
        for k in range(n):
            snd = u32(ctl, inst + 16 + k * 4)
            if snd == 0:
                continue
            env_off = u32(ctl, snd)
            key_off = u32(ctl, snd + 4)
            wav_off = u32(ctl, snd + 8)
            s = {
                "pan": ctl[snd + 12], "volume": ctl[snd + 13],
                "attack_us": 0, "decay_us": 0, "release_us": 0,
                "attack_vol": 127, "decay_vol": 127,
                "vel_min": 0, "vel_max": 127,
                "key_min": 0, "key_max": 127, "key_base": 60, "detune": 0,
            }
            if env_off:
                (s["attack_us"], s["decay_us"], s["release_us"]) = \
                    struct.unpack_from(">3i", ctl, env_off)
                s["attack_vol"] = ctl[env_off + 12]
                s["decay_vol"] = ctl[env_off + 13]
            if key_off:
                s["vel_min"], s["vel_max"] = ctl[key_off], ctl[key_off + 1]
                s["key_min"], s["key_max"] = ctl[key_off + 2], ctl[key_off + 3]
                s["key_base"] = ctl[key_off + 4]
                s["detune"] = struct.unpack_from(">b", ctl, key_off + 5)[0]
            s["wave"] = {
                "base": u32(ctl, wav_off), "len": struct.unpack_from(
                    ">i", ctl, wav_off + 4)[0], "type": ctl[wav_off + 8],
                "loop_off": u32(ctl, wav_off + 0xC),
                "book_off": u32(ctl, wav_off + 0x10),
            }
            sounds.append(s)
        return sounds

    programs = {}
    for i in range(inst_count):
        programs[i] = read_inst(u32(ctl, bank_off + 12 + i * 4))
    programs[-1] = read_inst(percussion)
    return ctl, tbl, programs


class WaveCache:
    def __init__(self, ctl, tbl):
        self.ctl, self.tbl = ctl, tbl
        self.cache = {}

    def get(self, wave):
        """(np.float32 samples in [-1,1], loop_start, loop_end, loop_count)."""
        key = wave["base"]
        if key not in self.cache:
            s = dict(wave, rate=RATE, keybase=60, detune=0)
            pcm, ls, le, lc = SFX.decode_sound(self.ctl, self.tbl, s)
            arr = np.asarray(pcm, dtype=np.float32) / 32768.0
            self.cache[key] = (arr, ls, le, lc)
        return self.cache[key]


# ---- the renderer --------------------------------------------------------

def render(rom, seq_index, max_seconds):
    sbk = rom[SBK_ROM[0]:SBK_ROM[0] + SBK_ROM[1]]
    rev, count = struct.unpack_from(">hh", sbk, 0)
    assert rev == 0x5331, hex(rev)
    off, length = struct.unpack_from(">Ii", sbk, 4 + seq_index * 8)
    seq = sbk[off:off + length]
    track_offs = struct.unpack_from(">16I", seq, 0)
    division = u32(seq, 64)

    # Every track of a looping sequence carries one AL_CMIDI_LOOPSTART /
    # AL_CMIDI_LOOPEND pair (cseq_to_mid.py:129-141), the loopend with
    # count=255 -- forever. Each track jumps back on its own, so the song
    # repeats every lcm of the track bodies, and it is in the same state
    # at tick T and T+period for every track at once. Cutting the render
    # at [max(loopstart), max(loopstart) + period] therefore gives a
    # region every track plays identically on every pass -- which is what
    # the streamer repeats. Sequences with no pair (the victory fanfares,
    # the stage cards) play once and stop, as they do on the N64.
    events = []
    loops = []
    for t_off in track_offs:
        if t_off == 0:
            continue
        evs = CSQ.parse_track_to_events(seq, t_off, len(seq))
        ls = [tk for tk, _, e in evs
              if e[0] == "marker" and e[1] == "loopstart"]
        le = [tk for tk, _, e in evs
              if e[0] == "marker" and e[1].startswith("loopend")]
        if len(ls) == 1 and len(le) == 1:
            loops.append((ls[0], le[0]))
        events.extend(evs)
    events.sort(key=lambda e: (e[0], e[1]))

    loop_ticks = None
    if loops:
        bodies = [e - s for s, e in loops]
        base = Counter(bodies).most_common(1)[0][0]
        # Castle's track 11 loops 19 ticks (22 ms) short of the other
        # twelve, so on the N64 it drifts a little every pass. Snap bodies
        # within half a percent of the common one back onto it rather than
        # let the drift deny the whole sequence a loop.
        bodies = [base if abs(b - base) * 200 <= base else b
                  for b in bodies]
        period = 1
        for b in bodies:
            period = period * b // math.gcd(period, b)
        if period <= 24 * base:
            start = max(s for s, _ in loops)
            loop_ticks = (start, start + period)
        else:
            # ModeSelect: bodies that share no small multiple, so the
            # tracks never come back into phase. Repeat the whole render
            # instead -- imperfect, but it is music, not silence.
            loop_ticks = (0, max(e for _, e in loops))

    ctl, tbl, programs = parse_music_bank(rom)
    waves = WaveCache(ctl, tbl)

    # tick -> sample-position mapping, honouring tempo changes as we walk.
    tempo = 500000                  # MIDI default, µs per quarter
    spt = RATE * tempo / 1e6 / division
    cur_tick = 0
    cur_smp = 0.0
    tmap = [(0, 0.0, spt)]          # (tick, sample, samples per tick)

    chan_prog = [0] * 16
    chan_vol = [1.0] * 16
    chan_pan = [64] * 16
    active = {}                     # (ch, note) -> voice dict
    voices = []

    def to_smp(tick):
        return cur_smp + (tick - cur_tick) * spt

    for tick, _, ev in events:
        smp = to_smp(tick)
        if max_seconds and smp > max_seconds * RATE:
            break
        kind = ev[0]
        if kind == "tempo":
            cur_smp = smp
            cur_tick = tick
            tempo = ev[1]
            spt = RATE * tempo / 1e6 / division
            tmap.append((tick, cur_smp, spt))
        elif kind == "midi":
            status, d1, d2 = ev[1], ev[2], ev[3]
            ch = status & 0xF
            k4 = status & 0xF0
            if k4 == 0xC0:
                chan_prog[ch] = d1
            elif k4 == 0xB0:
                if d1 == 7:
                    chan_vol[ch] = d2 / 127.0
                elif d1 == 10:
                    chan_pan[ch] = d2
        elif kind == "note_on":
            ch, note, vel = ev[1], ev[2], ev[3]
            prog = -1 if ch == 9 else chan_prog[ch]
            for s in programs.get(prog, []):
                if not (s["key_min"] <= note <= s["key_max"] and
                        s["vel_min"] <= vel <= s["vel_max"]):
                    continue
                v = {"start": smp, "end": None, "sound": s, "note": note,
                     "vel": vel, "vol": chan_vol[ch],
                     "pan": min(127, max(0, chan_pan[ch] +
                                         s["pan"] - 64))}
                active[(ch, note)] = v
                voices.append(v)
                break
        elif kind == "note_off":
            v = active.pop((ev[1], ev[2]), None)
            if v is not None:
                v["end"] = smp

    def tick_to_smp(tick):
        """Ticks to samples through the tempo map the walk just built."""
        base = tmap[0]
        for e in tmap:
            if e[0] > tick:
                break
            base = e
        return base[1] + (tick - base[0]) * base[2]

    loop_start_smp = loop_end_smp = None
    if loop_ticks is not None:
        loop_start_smp = tick_to_smp(loop_ticks[0])
        loop_end_smp = tick_to_smp(loop_ticks[1])

    total = to_smp(events[-1][0]) if events else 0
    if loop_end_smp:
        total = loop_end_smp
    if max_seconds and total > max_seconds * RATE:
        # a hand-limited render is a clip, not the song: it has no loop
        total = max_seconds * RATE
        loop_start_smp = None
    tail = int(0.8 * RATE)
    n_out = int(total) + tail
    mix = np.zeros((n_out, 2), dtype=np.float32)

    rendered = 0
    for v in voices:
        s = v["sound"]
        start = int(v["start"])
        end = v["end"] if v["end"] is not None else total
        end = int(min(end, total))
        if end <= start:
            continue
        hold = end - start
        release = int(s["release_us"] * 1e-6 * RATE)
        length = min(hold + release, n_out - start)
        if length <= 0:
            continue

        src, ls, le, lc = waves.get(s["wave"])
        ratio = 2.0 ** ((v["note"] - s["key_base"]) / 12.0 +
                        s["detune"] / 1200.0)
        need = int(length * ratio) + 2
        if lc != 0 and le > ls:
            if need > len(src):
                reps = (need - ls) // (le - ls) + 1
                src = np.concatenate([src[:le]] +
                                     [src[ls:le]] * reps)[:need]
        if need > len(src):
            src = np.concatenate([src, np.zeros(need - len(src),
                                                dtype=np.float32)])
        pos = np.arange(length, dtype=np.float32) * ratio
        out = np.interp(pos, np.arange(len(src), dtype=np.float32), src)

        atk = max(1, int(s["attack_us"] * 1e-6 * RATE))
        dec = max(1, int(s["decay_us"] * 1e-6 * RATE))
        rel = max(1, release)
        # Attack, decay, then hold -- and the release scales whatever the
        # envelope had reached at the note off, which is what the player
        # does (__n_seqpReleaseVoice ramps from the current gain). Putting
        # the release in the same breakpoint list instead made the list
        # non-monotonic for any note shorter than attack+decay, and
        # np.interp renders a non-increasing xp as nonsense: 18% of Kongo
        # Jungle's notes and 5% of Hyrule's got a garbage envelope.
        t = np.arange(length, dtype=np.float32)
        env = np.interp(t, [0, atk, atk + dec, atk + dec + 1],
                        [0.0, s["attack_vol"] / 127.0,
                         s["decay_vol"] / 127.0, s["decay_vol"] / 127.0])
        env[atk + dec:] = s["decay_vol"] / 127.0
        if hold < length:
            fade = np.clip(1.0 - (t[hold:] - hold) / float(rel), 0.0, 1.0)
            env[hold:] *= fade

        gain = (v["vel"] / 127.0) * (s["volume"] / 127.0) * v["vol"]
        pan = v["pan"] / 127.0
        gl = np.cos(pan * np.pi / 2) * gain
        gr = np.sin(pan * np.pi / 2) * gain
        seg = out * env
        mix[start:start + length, 0] += seg * gl
        mix[start:start + length, 1] += seg * gr
        rendered += 1

    if loop_start_smp is not None:
        # Voices still ringing when a track jumps back are heard over the
        # top of the loop's first moments, so fold that ring-out onto the
        # loop point rather than leave it as a tail nothing plays.
        lstart = int(loop_start_smp)
        lend = int(total)
        n = min(tail, lend - lstart)
        if n > 0:
            mix[lstart:lstart + n] += mix[lend:lend + n]
        mix = mix[:lend]

    peak = float(np.max(np.abs(mix))) or 1.0
    mix *= min(0.9 / peak, 1.0)
    pcm = np.clip(mix * 32767.0, -32768, 32767).astype("<i2")
    return pcm, rendered, len(voices), loop_start_smp


def write_wav(path, pcm, loop_start=None):
    """Canonical 16-bit stereo WAV. A looping track also gets the standard
    `smpl` chunk (one forward loop, loop_start..end of data), which is
    what src/dc/bgm.c seeks back to and what every audio editor reads."""
    data = pcm.tobytes()
    frames = len(pcm)
    chunks = struct.pack("<4sIHHIIHH", b"fmt ", 16, 1, 2, RATE, RATE * 4,
                         4, 16)
    if loop_start is not None:
        chunks += struct.pack("<4sI9I", b"smpl", 36 + 24,
                              0, 0, int(1e9 / RATE), 60, 0, 0, 0, 1, 0)
        chunks += struct.pack("<6I", 0, 0, int(loop_start), frames - 1,
                              0, 0)
    chunks += struct.pack("<4sI", b"data", len(data)) + data
    d = os.path.dirname(path)
    if d:
        os.makedirs(d, exist_ok=True)
    with open(path, "wb") as fp:
        fp.write(struct.pack("<4sI4s", b"RIFF", 4 + len(chunks), b"WAVE"))
        fp.write(chunks)
    return 12 + len(chunks)


def export(rom, seq_index, names, out_path, seconds=0):
    pcm, rendered, total, loop = render(rom, seq_index, seconds)
    size = write_wav(out_path, pcm, loop)
    print("%2d %-20s %5d/%-5d notes  %6.1f s%-18s %8d B  %s"
          % (seq_index, names[seq_index], rendered, total,
             len(pcm) / RATE,
             ("  loop from %.1f s" % (loop / RATE)) if loop is not None
             else "  one-shot",
             size, out_path))
    return size


def stage_bgm(rom, stage):
    """The bgm id out of a stage's own MPGroundData, so a caller names
    its stage and gets the track the game would play there."""
    import ssb_stageexport as S
    return S.read_ground(rom, stage)["bgm"]


USAGE = """usage:
  ssb_bgmexport.py --all --out-dir DIR        every track, as <id>.wav
  ssb_bgmexport.py --seq <index|name> --out F one track
  ssb_bgmexport.py --stage <name> --out F     the track that stage plays
options: --rom FILE, --seconds N (clip, no loop), --force (with --all)"""


def main():
    argv = sys.argv[1:]
    rom_path = M.ROM_DEFAULT
    out_path = out_dir = stage = None
    seq = None
    seconds = 0
    if "--rom" in argv:
        rom_path = argv[argv.index("--rom") + 1]
    if "--out" in argv:
        out_path = argv[argv.index("--out") + 1]
    if "--out-dir" in argv:
        out_dir = argv[argv.index("--out-dir") + 1]
    if "--seq" in argv:
        seq = argv[argv.index("--seq") + 1]
    if "--stage" in argv:
        stage = argv[argv.index("--stage") + 1]
    if "--seconds" in argv:
        seconds = float(argv[argv.index("--seconds") + 1])

    names = bgm_names()
    rom = open(rom_path, "rb").read()

    if "--all" in argv:
        # The whole music library, keyed by the game's own gmMusicID, the
        # way it lands on the disc: src/dc/bgm.c opens <root>/<id>.wav.
        if not out_dir:
            sys.exit("--all needs --out-dir")
        force = "--force" in argv
        newer = max(os.path.getmtime(rom_path),
                    os.path.getmtime(os.path.abspath(__file__)))
        total = 0
        index = []
        for i in range(len(names)):
            path = os.path.join(out_dir, "%02d.wav" % i)
            if (not force and os.path.exists(path) and
                    os.path.getmtime(path) > newer):
                total += os.path.getsize(path)
                index.append("%02d %s" % (i, names[i]))
                print("%2d %-20s up to date" % (i, names[i]))
                continue
            total += export(rom, i, names, path)
            index.append("%02d %s" % (i, names[i]))
        with open(os.path.join(out_dir, "index.txt"), "w") as fp:
            fp.write("\n".join(index) + "\n")
        print("%d tracks, %.1f MB in %s" % (len(names),
                                            total / 1048576.0, out_dir))
        return

    if not out_path:
        sys.exit(USAGE)
    if stage:
        seq_index = stage_bgm(rom, stage)
        print("stage %s plays bgm %d (%s)"
              % (stage, seq_index, names[seq_index]))
        # the file is named by the id, so a wrong pairing cannot ship
        base = os.path.splitext(os.path.basename(out_path))[0]
        if base.isdigit() and int(base) != seq_index:
            sys.exit("%s: stage %s plays bgm %d, not %s"
                     % (out_path, stage, seq_index, base))
    elif seq is not None:
        seq_index = int(seq) if seq.isdigit() else names.index(seq)
    else:
        sys.exit(USAGE)
    export(rom, seq_index, names, out_path, seconds)


if __name__ == "__main__":
    main()

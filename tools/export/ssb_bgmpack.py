#!/usr/bin/env python3
"""ssb64-dc: the BGM bank and its sequences, packed for the AICA.

The port's first answer to music was to render each track to a WAV at
build time (tools/export/ssb_bgmexport.py) and stream it. Three tracks
came to 9.9 MB of the Dreamcast's 16 MB of RAM, which is what this
replaces: the Dreamcast runs the game's *own* sequences over the game's
*own* instruments, the way the N64 did, with every sample resident in
the AICA's 2 MB of sound RAM and nothing but 155 KB of sequence bytes
in main RAM.

What the pack carries, all little-endian, in the order the target reads
it:

  header    magic, counts, section offsets
  programs  one per ALBank instArray entry, plus the percussion
            instrument last (the game hangs it on channel 9;
            n_seqplayer.c:1085 __n_initFromBank)
  sounds    ALEnvelope + ALKeyMap + ALSound flattened, in soundArray
            order -- which __n_lookupSoundQuick (n_seqplayer.c:332)
            binary-searches, so the order is load-bearing
  waves     an offset into the sample blob, in samples, with the
            ALADPCMloop points
  seqs      the compressed-MIDI tracks, verbatim big-endian out of
            S1_music.sbk; the player writes loop counters back into
            them (n_env.c:3529), so they are copied to RAM, not mapped
  blob      every waveform, VADPCM-decoded with the decomp's codec and
            re-encoded as Yamaha 4-bit ADPCM (tools/lib/ssb_adpcm.py) --
            the AICA's own format, so the SH-4 never touches a sample
            after the upload

Usage: python3 tools/export/ssb_bgmpack.py --out <bgm.pak> [--rom <rom.z64>]
                                    [--seqs all|<name|index>,...]
"""
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path[:0] = [os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), d)
                for d in ("lib", "export", "check")]
import ssb_paths as P        # noqa: E402
P.require_decomp()
sys.path.insert(0, P.DECOMP_TOOLS)
import ssb_adpcm as ADPCM    # noqa: E402
import ssb_bgmexport as BGM  # noqa: E402  (SBK_ROM, bgm_names)
import ssb_meshexport as M   # noqa: E402  (ROM_DEFAULT)
import ssb_sfxexport as SFX  # noqa: E402  (BANKS, decode_sound)

MAGIC = b"SSBBGM1\0"

# The AICA addresses a channel's sample with 16-bit loop registers, so no
# single waveform may exceed this many samples from its start address.
AICA_MAX_SAMPLES = 65535


def halve(pcm):
    """Decimate 2:1 with a two-tap average -- the band limit that keeps a
    long wave inside AICA_MAX_SAMPLES. Five of the music bank's 117
    waveforms need it, all of them multi-second loop-forever pads whose
    content sits far below the new Nyquist; the player pitches them back
    by carrying rate_shift (bgm.h). Nothing else in the bank is touched."""
    return [(pcm[i] + pcm[i + 1]) >> 1 for i in range(0, len(pcm) & ~1, 2)]


def u32(d, o):
    return struct.unpack_from(">I", d, o)[0]


def s32(d, o):
    return struct.unpack_from(">i", d, o)[0]


def s16(d, o):
    return struct.unpack_from(">h", d, o)[0]


# ---- the bank ------------------------------------------------------------

def parse_bank(ctl):
    """B1_sounds1's single ALBank, with the instrument fields the sequence
    player actually reads (libaudio.h:263-289) and soundArray order kept."""
    rev, bank_count = struct.unpack_from(">hh", ctl, 0)
    assert rev == 0x4231 and bank_count == 1, (hex(rev), bank_count)
    bank = u32(ctl, 4)
    inst_count = s16(ctl, bank)
    rate = s32(ctl, bank + 4)
    percussion = u32(ctl, bank + 8)
    inst_offs = [u32(ctl, bank + 12 + i * 4) for i in range(inst_count)]

    def read_sound(snd):
        env_off = u32(ctl, snd + 0)
        key_off = u32(ctl, snd + 4)
        wav_off = u32(ctl, snd + 8)
        s = {"pan": ctl[snd + 12], "volume": ctl[snd + 13],
             "flags": ctl[snd + 14],
             "attack_us": 0, "decay_us": 0, "release_us": 0,
             "attack_vol": 127, "decay_vol": 127,
             "vel_min": 0, "vel_max": 127,
             "key_min": 0, "key_max": 127, "key_base": 60, "detune": 0}
        if env_off:
            s["attack_us"], s["decay_us"], s["release_us"] = \
                struct.unpack_from(">3i", ctl, env_off)
            s["attack_vol"] = ctl[env_off + 12]
            s["decay_vol"] = ctl[env_off + 13]
        if key_off:
            s["vel_min"], s["vel_max"] = ctl[key_off], ctl[key_off + 1]
            s["key_min"], s["key_max"] = ctl[key_off + 2], ctl[key_off + 3]
            s["key_base"] = ctl[key_off + 4]
            s["detune"] = struct.unpack_from(">b", ctl, key_off + 5)[0]
        s["wave"] = {"base": u32(ctl, wav_off),
                     "len": s32(ctl, wav_off + 4),
                     "type": ctl[wav_off + 8],
                     "loop_off": u32(ctl, wav_off + 0xC),
                     "book_off": u32(ctl, wav_off + 0x10)}
        return s

    def read_inst(off):
        if off == 0:
            return None
        n = s16(ctl, off + 14)
        inst = {"volume": ctl[off], "pan": ctl[off + 1],
                "priority": ctl[off + 2], "flags": ctl[off + 3],
                "trem_type": ctl[off + 4], "trem_rate": ctl[off + 5],
                "trem_depth": ctl[off + 6], "trem_delay": ctl[off + 7],
                "vib_type": ctl[off + 8], "vib_rate": ctl[off + 9],
                "vib_depth": ctl[off + 10], "vib_delay": ctl[off + 11],
                "bend_range": s16(ctl, off + 12), "sounds": []}
        for k in range(n):
            snd = u32(ctl, off + 16 + k * 4)
            if snd:
                inst["sounds"].append(read_sound(snd))
        return inst

    insts = [read_inst(o) for o in inst_offs]
    return {"rate": rate, "insts": insts,
            "percussion": read_inst(percussion)}


# ---- the sequences -------------------------------------------------------

def read_sbk(rom):
    sbk = rom[BGM.SBK_ROM[0]:BGM.SBK_ROM[0] + BGM.SBK_ROM[1]]
    rev, count = struct.unpack_from(">hh", sbk, 0)
    assert rev == 0x5331, hex(rev)
    out = []
    for i in range(count):
        off, length = struct.unpack_from(">Ii", sbk, 4 + i * 8)
        out.append(sbk[off:off + length])
    return out


# ---- packing -------------------------------------------------------------

def layout(bank):
    """The pack's program, sound and wave numbering, without decoding a
    sample: programs in instArray order with the percussion instrument
    appended, sounds flat in soundArray order (each tagged with its
    wave_index), and waves in first-use order. tools/export/ssb_sndsets.py
    numbers the waves a sequence reaches through this, so the indices it
    writes are the ones bgm.pak's wave table has."""
    programs = [i for i in bank["insts"]]
    perc_index = -1
    if bank["percussion"]:
        perc_index = len(programs)
        programs.append(bank["percussion"])

    waves = {}           # wave base -> index
    wave_list = []
    sounds = []
    prog_rows = []
    for inst in programs:
        if inst is None:
            prog_rows.append(None)
            continue
        first = len(sounds)
        for s in inst["sounds"]:
            w = s["wave"]
            if w["base"] not in waves:
                waves[w["base"]] = len(wave_list)
                wave_list.append(w)
            s["wave_index"] = waves[w["base"]]
            sounds.append(s)
        prog_rows.append((inst, first, len(inst["sounds"])))
    return programs, perc_index, sounds, wave_list, prog_rows


def build(rom, seq_pick):
    r = SFX.BANKS[1]
    ctl = rom[r["ctl"][0]:r["ctl"][1]]
    tbl = rom[r["tbl"][0]:r["tbl"][1]]
    bank = parse_bank(ctl)

    programs, perc_index, sounds, wave_list, prog_rows = layout(bank)

    # Decode every waveform once, then re-encode for the AICA.
    blob = bytearray()
    wave_rows = []
    longest = 0
    halved = 0
    for w in wave_list:
        pcm, ls, le, lc = SFX.decode_sound(ctl, tbl, w)
        longest = max(longest, len(pcm))
        shift = 0
        while len(pcm) > AICA_MAX_SAMPLES:
            pcm = halve(pcm)
            ls, le = ls // 2, le // 2
            shift += 1
        if shift:
            halved += 1
        n = len(pcm)
        if n & 1:                       # keep every wave byte-aligned
            pcm = list(pcm) + [pcm[-1]]
        enc = ADPCM.encode(pcm)
        wave_rows.append((len(blob) * 2, n, ls, le, lc, shift))
        blob += enc
        if len(blob) & 3:               # AICA start addresses are 4-aligned
            blob += b"\0" * (4 - (len(blob) & 3))

    seqs = read_sbk(rom)
    names = BGM.bgm_names()
    seqdata = bytearray()
    seq_rows = []
    for i, s in enumerate(seqs):
        name = names[i] if i < len(names) else "seq%02d" % i
        if seq_pick is not None and i not in seq_pick:
            seq_rows.append((0, 0, name))
            continue
        seq_rows.append((len(seqdata), len(s), name))
        seqdata += s
        while len(seqdata) & 3:
            seqdata += b"\0"

    return {"bank": bank, "prog_rows": prog_rows, "sounds": sounds,
            "wave_rows": wave_rows, "blob": bytes(blob),
            "seq_rows": seq_rows, "seqdata": bytes(seqdata),
            "perc_index": perc_index, "longest": longest, "halved": halved,
            "rate": bank["rate"]}


HDR = "<8s13I"
PROG = "<12BhHHH"        # 12 u8, bendRange, first, count, flags16
SOUND = "<3i12BHH"
WAVE = "<5I4B"
SEQ = "<2I24s"


def serialize(p):
    n_prog = len(p["prog_rows"])
    n_snd = len(p["sounds"])
    n_wav = len(p["wave_rows"])
    n_seq = len(p["seq_rows"])
    hdr_size = struct.calcsize(HDR)
    off_prog = hdr_size
    off_snd = off_prog + n_prog * struct.calcsize(PROG)
    off_wav = off_snd + n_snd * struct.calcsize(SOUND)
    off_seq = off_wav + n_wav * struct.calcsize(WAVE)
    off_seqdata = off_seq + n_seq * struct.calcsize(SEQ)
    off_blob = off_seqdata + len(p["seqdata"])
    off_blob = (off_blob + 31) & ~31

    out = bytearray()
    out += struct.pack(HDR, MAGIC, n_prog, n_snd, n_wav, n_seq,
                       off_prog, off_snd, off_wav, off_seq,
                       off_seqdata, len(p["seqdata"]),
                       off_blob, len(p["blob"]), p["rate"])
    assert len(out) == hdr_size, (len(out), hdr_size)
    for row in p["prog_rows"]:
        if row is None:                 # bank 1's slot 0 is a NULL program
            out += struct.pack(PROG, *([0] * 12), 0, 0, 0, 0)
            continue
        i, first, count = row
        out += struct.pack(PROG, i["volume"], i["pan"], i["priority"],
                           i["flags"], i["trem_type"], i["trem_rate"],
                           i["trem_depth"], i["trem_delay"], i["vib_type"],
                           i["vib_rate"], i["vib_depth"], i["vib_delay"],
                           i["bend_range"], first, count, 1)
    for s in p["sounds"]:
        out += struct.pack(SOUND, s["attack_us"], s["decay_us"],
                           s["release_us"], s["attack_vol"], s["decay_vol"],
                           s["vel_min"], s["vel_max"], s["key_min"],
                           s["key_max"], s["key_base"], s["detune"] & 0xFF,
                           s["pan"], s["volume"], s["flags"], 0,
                           s["wave_index"], 0)
    for off, n, ls, le, lc, shift in p["wave_rows"]:
        out += struct.pack(WAVE, off, n, ls, le, lc, shift, 0, 0, 0)
    for off, size, name in p["seq_rows"]:
        out += struct.pack(SEQ, off, size, name.encode()[:23])
    out += p["seqdata"]
    while len(out) < off_blob:
        out += b"\0"
    out += p["blob"]
    return bytes(out)


def main():
    argv = sys.argv[1:]
    rom_path = M.ROM_DEFAULT
    out_path = None
    seq_pick = None
    if "--rom" in argv:
        rom_path = argv[argv.index("--rom") + 1]
    if "--out" in argv:
        out_path = argv[argv.index("--out") + 1]
    if "--seqs" in argv:
        spec = argv[argv.index("--seqs") + 1]
        if spec != "all":
            names = BGM.bgm_names()
            seq_pick = set()
            for tok in spec.split(","):
                tok = tok.strip()
                seq_pick.add(int(tok) if tok.isdigit() else names.index(tok))
    if not out_path:
        sys.exit("--out is required")

    rom = open(rom_path, "rb").read()
    p = build(rom, seq_pick)
    data = serialize(p)
    d = os.path.dirname(out_path)
    if d:
        os.makedirs(d, exist_ok=True)
    with open(out_path, "wb") as fp:
        fp.write(data)

    live = sum(1 for _, size, _ in p["seq_rows"] if size)
    print("wrote %s: %d bytes" % (out_path, len(data)))
    print("  %d programs (percussion at %d), %d sounds, %d waves"
          % (len(p["prog_rows"]), p["perc_index"], len(p["sounds"]),
             len(p["wave_rows"])))
    print("  %d/%d sequences, %d bytes of cseq (main RAM)"
          % (live, len(p["seq_rows"]), len(p["seqdata"])))
    print("  %d bytes of ADPCM (AICA sound RAM); longest wave %d samples, "
          "%d band-limited to fit" % (len(p["blob"]), p["longest"],
                                      p["halved"]))


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""ssb64-dc: instrument-bank audio, converted at build time.

The game's samples are VADPCM in B1_sounds1.tbl (music instruments, 32 kHz)
and B1_sounds2.tbl (every sound effect, 44.1 kHz), described by ALBankFile
structs in the paired .ctl. Per the project rule -- convert at build time,
never decode N64 formats at runtime -- this walks both banks, decodes every
selected waveform to PCM16 with the decomp's own codec
(ssb-decomp-re/tools/audio_codec.py), and writes one little-endian pack the
Dreamcast plays directly on AICA channels: sample rate, loop points and
keybase ride along per sound, so the target does arithmetic, not decoding.

Formats are documented in ssb-decomp-re/MUSIC_AND_SFX_DISCOVERIES.md; struct
layouts from include/PR/libaudio.h.

Usage: python3 tools/lib/ssb_sfxexport.py --out <audio.pak> [--rom <rom.z64>]
                                      [--sfx-max N] [--music-insts N]
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
import ssb_vadpcm as VADPCM  # noqa: E402  (the decomp's VADPCM
                            #  decoder, scale boundary corrected)
import ssb_meshexport as M   # noqa: E402  (ROM_DEFAULT)

# ROM ranges (US), from MUSIC_AND_SFX_DISCOVERIES.md / smashbrothers.us.yaml.
BANKS = {
    1: {"ctl": (0xB4E5C0, 0xB54CE0), "tbl": (0xB54CE0, 0xC6B650)},
    2: {"ctl": (0xC6B650, 0xC7B1F0), "tbl": (0xC7B1F0, 0xF573D0)},
}

MAGIC = b"SSBSND1\0"


def u32(d, o):
    return struct.unpack_from(">I", d, o)[0]


def s32(d, o):
    return struct.unpack_from(">i", d, o)[0]


def s16(d, o):
    return struct.unpack_from(">h", d, o)[0]


def parse_bank(ctl):
    """The single ALBank of one .ctl: sample rate, and its instruments'
    sounds in soundArray order -- which for the SFX bank is the index the
    game's FGM `trigger` opcode uses."""
    rev, bank_count = struct.unpack_from(">hh", ctl, 0)
    assert rev == 0x4231 and bank_count == 1, (hex(rev), bank_count)
    bank = u32(ctl, 4)
    inst_count = s16(ctl, bank)
    rate = s32(ctl, bank + 4)
    percussion = u32(ctl, bank + 8)
    insts = [u32(ctl, bank + 12 + i * 4) for i in range(inst_count)]
    if percussion:
        insts.append(percussion)

    out = []
    for inst in insts:
        if inst == 0:       # bank 1's slot 0 is a NULL instrument
            continue
        sound_count = s16(ctl, inst + 14)
        for k in range(sound_count):
            snd = u32(ctl, inst + 16 + k * 4)
            if snd == 0:
                continue
            env_off = u32(ctl, snd + 0)
            key_off = u32(ctl, snd + 4)
            wav_off = u32(ctl, snd + 8)
            keybase, detune = (60, 0)
            if key_off:
                keybase = ctl[key_off + 4]
                detune = ctl[key_off + 5]
            base = u32(ctl, wav_off)
            length = s32(ctl, wav_off + 4)
            wtype = ctl[wav_off + 8]
            loop_off = u32(ctl, wav_off + 0xC)
            book_off = u32(ctl, wav_off + 0x10) if wtype == 0 else 0
            out.append({"rate": rate, "keybase": keybase, "detune": detune,
                        "base": base, "len": length, "type": wtype,
                        "loop_off": loop_off, "book_off": book_off})
    return out


def decode_sound(ctl, tbl, s):
    """One waveform to (PCM s16 list, loop_start, loop_end, loop_count)."""
    if s["type"] != 0:
        raise ValueError("AL_RAW16_WAVE not handled (none in this ROM)")
    order = s32(ctl, s["book_off"])
    npred = s32(ctl, s["book_off"] + 4)
    book = list(struct.unpack_from(">%dh" % (npred * order * 8), ctl,
                                   s["book_off"] + 8))
    raw = tbl[s["base"]:s["base"] + s["len"] // 9 * 9]
    pcm = VADPCM.adpcm_decode(raw, book, order, npred)
    loop_start = loop_end = loop_count = 0
    if s["loop_off"]:
        loop_start = u32(ctl, s["loop_off"])
        loop_end = u32(ctl, s["loop_off"] + 4)
        loop_count = u32(ctl, s["loop_off"] + 8)
    return pcm, loop_start, loop_end, loop_count


def main():
    argv = sys.argv[1:]
    rom_path = M.ROM_DEFAULT
    out_path = None
    sfx_max = 0x7FFFFFFF
    music_insts = 8
    if "--rom" in argv:
        rom_path = argv[argv.index("--rom") + 1]
    if "--out" in argv:
        out_path = argv[argv.index("--out") + 1]
    if "--sfx-max" in argv:
        sfx_max = int(argv[argv.index("--sfx-max") + 1])
    if "--music-insts" in argv:
        music_insts = int(argv[argv.index("--music-insts") + 1])
    if not out_path:
        sys.exit("--out is required")
    rom = open(rom_path, "rb").read()

    picks = []          # (bank, sound dict)
    for bank_id, take in ((2, sfx_max), (1, None)):
        r = BANKS[bank_id]
        ctl = rom[r["ctl"][0]:r["ctl"][1]]
        tbl = rom[r["tbl"][0]:r["tbl"][1]]
        sounds = parse_bank(ctl)
        if bank_id == 2:
            chosen = sounds[:take]
        else:
            # A taste of the music bank: the first sound of the first N
            # instruments -- enough to prove loop points and pitch math.
            seen = set()
            chosen = []
            for s in sounds:
                if s["base"] not in seen and len(chosen) < music_insts:
                    seen.add(s["base"])
                    chosen.append(s)
        for s in chosen:
            picks.append((bank_id, ctl, tbl, s))
        print("bank %d: %d sounds in .ctl, taking %d"
              % (bank_id, len(sounds), len(chosen)))

    dir_size = len(picks) * 32
    pcm_off = 16 + dir_size
    directory = b""
    blob = b""
    total_samples = 0
    for bank_id, ctl, tbl, s in picks:
        pcm, ls, le, lc = decode_sound(ctl, tbl, s)
        total_samples += len(pcm)
        directory += struct.pack("<6I4BI", pcm_off + len(blob), len(pcm),
                                 s["rate"], ls, le, lc,
                                 bank_id, s["keybase"], s["detune"], 0, 0)
        blob += struct.pack("<%dh" % len(pcm), *pcm)

    header = struct.pack("<8sII", MAGIC, len(picks), 0)
    d = os.path.dirname(out_path)
    if d:
        os.makedirs(d, exist_ok=True)
    with open(out_path, "wb") as fp:
        fp.write(header + directory + blob)
    print("wrote %s: %d sounds, %d samples, %d bytes"
          % (out_path, len(picks), total_samples,
             16 + dir_size + len(blob)))


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""ssb64-dc: the FGM staging, checked against independent authorities.

Three things can silently go wrong between gmsound.h and a sound coming
out of the AICA, and each is checked here against something that is not
this repo's own opinion:

  the ids     ssb_fgmexport.fgm_names() has to number gmFGMVoiceID
              exactly as the compiler does, because syAudioPlayFGM is
              called at runtime with the *compiled* value. The oracle is
              a real C compiler: the enum body is lifted out of the
              header, built with -DREGION_US (the port's own define) and
              asked to print every entry. This test exists because the
              first version of that parser matched only nSYAudioFGM* and
              dropped the 331 interleaved nSYAudioVoice* entries, which
              mis-numbered every effect from position 309 on -- the
              announcer was staged as the wrong sound for months.

  the header   nSYAudioFGMVoiceEnd is the header's own count of voice
  vs the ROM   scripts; fgm.ucd is the ROM's package of them. They agree
               only if the enum was read right.

  the clock   func_800293A8 is a client of the synthesizer and says
              when it wants to run again; that period is 184 samples of
              output, not a video frame. The oracle is the decomp's own
              two constants.

  the pools   the engine's voice, note and modulator counts are the
              game's own settings, three positional fields of
              dSYAudioPublicSettings -- and the voice count is also the
              length of the sound-player table over it.

  the samples  the port re-encodes VADPCM -> PCM16 -> Yamaha ADPCM, one
               generation of loss the N64 never took, and decimates the
               waveforms a channel's 16-bit loop registers cannot reach.
               Both are measured: SNR against the decomp's own decode,
               and the invariant that nothing leaves here over 65535
               samples or decodes to the wrong length.

Usage: python3 tools/check/fgm_check.py [--rom <rom.z64>]
"""
import math
import os
import re
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path[:0] = [os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), d)
                for d in ("lib", "export", "check")]
import ssb_paths as P        # noqa: E402
P.require_decomp()
sys.path.insert(0, P.DECOMP_TOOLS)
import ssb_adpcm as ADPCM    # noqa: E402
import ssb_fgmexport as F    # noqa: E402
import ssb_meshexport as M   # noqa: E402
import ssb_sfxexport as SFX  # noqa: E402

# One per character whose MainMotion the exporter can walk -- what a full
# roster asks the sound RAM for, and enough waveforms to put a number on
# the transcode without decoding all 322.
FIGHTERS = ["Mario", "Fox", "Donkey", "Samus", "Luigi", "Link", "Yoshi",
            "Kirby", "Pikachu", "Ness", "Purin", "Captain"]


def enum_from_compiler(tmpdir):
    """{name: value} for gmFGMVoiceID, as cc computes it with REGION_US."""
    src = open(P.GMSOUND_H).read()
    body = re.search(r"typedef enum gmFGMVoiceID\s*\{(.*?)\n\}", src, re.S)
    names = re.findall(r"^\s*(n\w+)\s*[,=]", body.group(1), re.M)
    names = list(dict.fromkeys(names))
    c = os.path.join(tmpdir, "enum.c")
    with open(c, "w") as fp:
        fp.write("#include <stdio.h>\ntypedef enum gmFGMVoiceID {%s} E;\n"
                 "int main(void){\n" % body.group(1))
        for n in names:
            fp.write('    printf("%s %d\\n", "{0}", (int){0});\n'.format(n))
        fp.write("    return 0;\n}\n")
    exe = os.path.join(tmpdir, "enum")
    subprocess.run(["cc", "-DREGION_US", "-o", exe, c], check=True)
    out = subprocess.run([exe], check=True, capture_output=True, text=True)
    return {l.split()[0]: int(l.split()[1]) for l in out.stdout.splitlines()}


def check_ids(rom, tmpdir):
    ours = F.fgm_names()
    theirs = enum_from_compiler(tmpdir)
    if ours != theirs:
        wrong = sorted(k for k in set(ours) | set(theirs)
                       if ours.get(k) != theirs.get(k))
        for k in wrong[:10]:
            print("  %s: exporter %s, compiler %s"
                  % (k, ours.get(k), theirs.get(k)))
        sys.exit("fgm_check: %d of %d gmFGMVoiceID entries are numbered "
                 "differently from the compiler's" % (len(wrong), len(theirs)))
    off, size = F.FGM_FILES["fgm.ucd"]
    scripts = len(F.package_entries(rom[off:off + size]))
    if ours["nSYAudioFGMVoiceEnd"] != scripts:
        sys.exit("fgm_check: gmFGMVoiceID ends at %d, fgm.ucd holds %d "
                 "voice scripts" % (ours["nSYAudioFGMVoiceEnd"], scripts))
    print("%d gmFGMVoiceID entries numbered exactly as cc -DREGION_US "
          "numbers them; nSYAudioFGMVoiceEnd == fgm.ucd's %d scripts"
          % (len(ours), scripts))
    return ours


def roster_sounds(rom, index_of):
    """The bank-2 sounds the game stages: every sample any tier-A sound
    set reaches (tools/export/ssb_sndsets.py, the same call ssb_fgmexport --sets
    makes), with the FGM ids behind them -- the realistic resident set,
    and more than any one scene holds at once."""
    import ssb_sndsets
    c = ssb_sndsets.build(rom)
    ids = sorted(set().union(*c["group_ids"].values()))
    snd = sorted(set().union(*(s for s, _w in c["sets"].values())))
    return ids, snd


def check_samples(rom, picks):
    """What the transcode costs, and that nothing leaves oversized."""
    r = SFX.BANKS[2]
    ctl = rom[r["ctl"][0]:r["ctl"][1]]
    tbl = rom[r["tbl"][0]:r["tbl"][1]]
    bank = SFX.parse_bank(ctl)
    total_e = total_s = 0.0
    worst = 99.0
    worst_i = -1
    adpcm = 0
    for i in picks:
        pcm, _, _, _ = SFX.decode_sound(ctl, tbl, bank[i])
        while len(pcm) > F.AICA_MAX_SAMPLES:
            pcm = F.halve(pcm)
        if len(pcm) & 1:
            pcm = list(pcm) + [pcm[-1]]
        enc = ADPCM.encode(pcm)
        adpcm += len(enc)
        back = ADPCM.decode(enc, len(pcm))
        if len(back) != len(pcm):
            sys.exit("fgm_check: sound %d decodes to %d samples, not %d"
                     % (i, len(back), len(pcm)))
        e = sum((a - b) ** 2 for a, b in zip(pcm, back))
        p = sum(a * a for a in pcm)
        total_e += e
        total_s += p
        if e and p:
            snr = 10 * math.log10(p / e)
            if snr < worst:
                worst, worst_i = snr, i
    print("%d bank-2 waveforms transcoded to AICA ADPCM: %.1f dB SNR "
          "overall, %.1f dB worst (sound %d); %d bytes of sound RAM"
          % (len(picks), 10 * math.log10(total_s / total_e), worst,
             worst_i, adpcm))
    return adpcm


def check_decimation(rom):
    """The waveforms a channel cannot address, brought inside it. One
    B1_sounds2 sound is 214,832 samples -- more than three times what the
    AICA's 16-bit loop registers reach -- and no voice in the h6 stage's
    set touches it, so it is checked here rather than by staging."""
    r = SFX.BANKS[2]
    ctl = rom[r["ctl"][0]:r["ctl"][1]]
    tbl = rom[r["tbl"][0]:r["tbl"][1]]
    bank = SFX.parse_bank(ctl)
    big = []
    for i, s in enumerate(bank):
        if s["len"] // 9 * 16 > F.AICA_MAX_SAMPLES:
            big.append(i)
    for i in big:
        pcm, ls, le, _ = SFX.decode_sound(ctl, tbl, bank[i])
        n0, shift = len(pcm), 0
        while len(pcm) > F.AICA_MAX_SAMPLES:
            pcm = F.halve(pcm)
            ls, le = ls // 2, le // 2
            shift += 1
        if len(pcm) != n0 >> shift or le > len(pcm):
            sys.exit("fgm_check: sound %d decimates wrong" % i)
    print("%d waveform(s) over %d samples decimate inside a channel's "
          "loop registers, loop points with them"
          % (len(big), F.AICA_MAX_SAMPLES))


def check_clock():
    """The engine's frame period, against the two numbers that set it.

    func_800293A8 is an ALPlayer client handler and returns how long the
    synthesizer should wait before calling it again, in microseconds:
    n_env.c:5339 sets that to `184000000 / n_syn->outputRate`, and
    n_alAudioFrame converts it straight back to samples with
    _n_timeToSamplesNoRound (n_env.c:2329), so the output rate cancels
    and the period is a fixed 184 samples of output. sys/audio.c:95 is
    the rate: 32000. The port had assumed the video frame and ran the
    engine at 60 Hz, which is 2.9x slow on every timer it has -- exactly
    the kind of value this file exists to stop being assumed."""
    env = open(os.path.join(P.DECOMP_DIR, "src", "libultra", "n_audio",
                            "n_env.c")).read()
    m = re.search(r"unk_alsound_0x44\s*=\s*\((\d+)\s*/\s*n_syn->outputRate\)",
                  env)
    if not m:
        sys.exit("fgm_check: n_env.c no longer sets the FGM client's period")
    numer = int(m.group(1))

    audio = open(os.path.join(P.DECOMP_DIR, "src", "sys", "audio.c")).read()
    m = re.search(r"^\s*(\d+),\s*//\s*Output rate", audio, re.M)
    if not m:
        sys.exit("fgm_check: sys/audio.c no longer states the output rate")
    rate = int(m.group(1))

    micros = numer // rate
    samples = round(micros * rate / 1000000.0 + 0.5)

    hdr = open(os.path.join(HERE, "..", "..", "src", "dc", "fgm.h")).read()
    m = re.search(r"^#define FGM_TICK_US\s+(\d+)", hdr, re.M)
    if not m:
        sys.exit("fgm_check: src/dc/fgm.h defines no FGM_TICK_US")
    ours = int(m.group(1))

    if ours != micros:
        sys.exit("fgm_check: the FGM engine's frame is %d us here and %d us "
                 "on the N64 (%d / %d)" % (ours, micros, numer, rate))
    print("the FGM frame runs every %d us -- %d samples at %d Hz, %.1f Hz"
          % (ours, samples, rate, 1000000.0 / ours))


def check_pools():
    """The three pool sizes, against the settings the game passes them in.

    sys/audio.c:91 dSYAudioPublicSettings is one positional initialiser,
    and three of its fields size the FGM engine: unk31, unk32 and
    sndplayers_num become func_80026204_26E04's unk_0x0, unk_0x2 and
    unk_0x4 (n_env.c:5298), which allocate the modulator, note and voice
    arrays. They are the three numbers after the fx type, so that is the
    anchor -- the field names in the header are not in the initialiser at
    all, only comments are, and two of those comments are `???`.

    The port has the game's 24 voices, not half of them, because a full voice pool
    is a dropped sound. The voice count is
    also sSYAudioSoundPlayers' length (src/dc/syaudio.c), which is why a
    full table and a full pool are one condition on both machines."""
    audio = open(os.path.join(P.DECOMP_DIR, "src", "sys", "audio.c")).read()
    m = re.search(r"AL_FX_NONE,[^\n]*\n"
                  r"\s*(\d+),[^\n]*\n"
                  r"\s*(\d+),[^\n]*\n"
                  r"\s*(\d+),", audio)
    if not m:
        sys.exit("fgm_check: sys/audio.c no longer states the pool sizes")
    want = {"FGM_MODS": int(m.group(1)),
            "FGM_NOTES": int(m.group(2)),
            "FGM_VOICES": int(m.group(3))}

    src = open(os.path.join(HERE, "..", "..", "src", "dc", "fgm.c")).read()
    for name, n in sorted(want.items()):
        m = re.search(r"^#define %s\s+(\d+)" % name, src, re.M)
        if not m:
            sys.exit("fgm_check: src/dc/fgm.c defines no %s" % name)
        if int(m.group(1)) != n:
            sys.exit("fgm_check: %s is %s here and %d on the N64"
                     % (name, m.group(1), n))

    tbl = open(os.path.join(HERE, "..", "..", "src", "dc", "syaudio.h")).read()
    m = re.search(r"^#define SYAUDIO_SNDPLAYERS_NUM\s+(\d+)", tbl, re.M)
    if not m:
        sys.exit("fgm_check: src/dc/syaudio.h defines no SYAUDIO_SNDPLAYERS_NUM")
    if int(m.group(1)) != want["FGM_VOICES"]:
        sys.exit("fgm_check: %s sound players over %d voices; the game has "
                 "one table slot per voice" % (m.group(1), want["FGM_VOICES"]))

    print("the engine holds %d voices, %d notes and %d modulators, and the "
          "game %d sound players over them"
          % (want["FGM_VOICES"], want["FGM_NOTES"], want["FGM_MODS"],
             want["FGM_VOICES"]))


def main():
    rom_path = M.ROM_DEFAULT
    if "--rom" in sys.argv:
        rom_path = sys.argv[sys.argv.index("--rom") + 1]
    rom = open(rom_path, "rb").read()
    with tempfile.TemporaryDirectory() as tmpdir:
        index_of = check_ids(rom, tmpdir)
    names, picks = roster_sounds(rom, index_of)
    print("%d FGM ids across the tier-A sound sets (%d fighters) reach %d "
          "of the bank's sounds" % (len(names), len(FIGHTERS), len(picks)))
    check_samples(rom, picks)
    check_decimation(rom)
    check_clock()
    check_pools()
    print("fgm_check: the ids match the compiler and the samples fit the "
          "hardware")


if __name__ == "__main__":
    main()

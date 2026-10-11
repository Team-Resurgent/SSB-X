#!/usr/bin/env python3
"""ssb64-dc: the FGM sound-effect engine's data, staged for the target.

A game sound effect is not a sample: syAudioPlayFGM(id) runs fgm.ucd[id]
(a voice script: pick an articulation, set volume/pan, play packed note
rows), each note runs fgm.tbl[n] (per-frame envelope bytecode whose
`trigger` starts a B1_sounds2 bank sound), and articulations spawn fgm.unk
LFOs for vibrato and sweeps. src/dc/fgm.c interprets all three on the
Dreamcast, driving AICA channels.

This stages the inputs: the three FGM files verbatim (big-endian; the
target reads them through be.h, like all logic data), and the samples:
every B1_sounds2 sound any of tools/export/ssb_sndsets.py's tier-A sound sets
reaches -- 264 of the 322, about 1.97 MB, more than sound RAM holds at
once. The romdisk carries them all; src/dc/sndres.c uploads the running
scene's share. The sample pack has ssb_sfxexport's directory layout --
each entry's original bank index included, so the engine can look sounds
up -- over a Yamaha ADPCM blob the AICA decodes in hardware, four times
smaller than the PCM16 this wrote first.

walk_ucd and walk_tbl are the build-time reading of the ucd (articulations
+ fork closure) and tbl (trigger args) bytecode that ssb_sndsets.py uses
to find which sounds a set reaches.

Usage: python3 tools/export/ssb_fgmexport.py --outdir <romdisk> [--rom <rom.z64>]
"""
import os
import re
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path[:0] = [os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), d)
                for d in ("lib", "export", "check")]
import ssb_paths as P        # noqa: E402
P.require_decomp()
sys.path.insert(0, P.DECOMP_TOOLS)
import ssb_meshexport as M   # noqa: E402
import ssb_sfxexport as SFX  # noqa: E402
import ssb_adpcm as ADPCM   # noqa: E402

# ROM ranges (US), per MUSIC_AND_SFX_DISCOVERIES.md.
# ssb_sfxexport's writer produces PCM16 under SSBSND1; this pack has the
# same directory layout with an ADPCM blob, so it gets its own magic and a
# stale PCM16 file is rejected rather than played as noise.
MAGIC = b"SSBSND2\0"

FGM_FILES = {
    "fgm.unk": (0xF573D0, 2080),
    "fgm.tbl": (0xF57BF0, 11728),
    "fgm.ucd": (0xF5A9C0, 19232),
}
GMSOUND_H = P.GMSOUND_H

# The AICA addresses a channel's sample with 16-bit loop registers, so no
# single waveform may exceed this many samples from its start address.
AICA_MAX_SAMPLES = 65535


def halve(pcm):
    """Decimate 2:1 with a two-tap average -- tools/ssb_bgmpack.halve, for
    the same reason: one B1_sounds2 waveform is 214,832 samples, three
    times what a channel can address. The engine pitches it back by
    doubling the playback frequency per halving (rate_shift, fgm.h)."""
    return [(pcm[i] + pcm[i + 1]) >> 1 for i in range(0, len(pcm) & ~1, 2)]

def fgm_names():
    """{name: value} for gmFGMVoiceID, evaluated the way the compiler does.

    The enum interleaves two prefixes -- nSYAudioFGM* (the sound effects)
    and nSYAudioVoice* (announcer, crowd and character voices), the first
    of them at position 309 -- so anything that matches only one prefix
    counts wrong from there on and silently mis-numbers every effect after
    it. Entries are otherwise dense from 0, with a trailing
    nSYAudioFGMVoiceEnd = 0x2B7 alias, and REGION_US blocks that only add
    entries. The port builds -DREGION_US (src/game/ssb64/Makefile), so
    those are taken and the #else arms dropped -- matching the compiled
    values syAudioPlayFGM is called with at runtime.
    """
    src = open(GMSOUND_H).read()
    body = re.search(r"typedef enum gmFGMVoiceID\s*\{(.*?)\n\}", src, re.S)
    if not body:
        raise ValueError("gmFGMVoiceID not found in %s" % GMSOUND_H)
    names = {}
    next_val = 0
    taking = [True]                     # #if/#else nesting, REGION_US on
    for line in body.group(1).splitlines():
        line = re.sub(r"//.*|/\*.*?\*/", "", line).strip()
        if line.startswith("#"):
            d = line[1:].split(None, 1)
            key = d[0]
            if key in ("if", "ifdef", "ifndef"):
                cond = d[1] if len(d) > 1 else ""
                on = ("REGION_US" in cond) == (key != "ifndef")
                taking.append(taking[-1] and on)
            elif key == "else":
                outer = taking[-2] if len(taking) > 1 else True
                taking[-1] = outer and not taking[-1]
            elif key == "endif":
                taking.pop()
            continue
        if not taking[-1] or not line:
            continue
        m = re.match(r"(n\w+)\s*(?:=\s*([^,]+))?,?$", line)
        if not m:
            raise ValueError("unparsed gmFGMVoiceID line: %r" % line)
        if m.group(2):
            expr = m.group(2).strip()
            next_val = names[expr] if expr in names else int(expr, 0)
        names[m.group(1)] = next_val
        next_val += 1
    return names


def package_entries(blob):
    """A SYAudioPackage: s32 count, u32 offsets[count] (file-relative)."""
    count = struct.unpack_from(">i", blob, 0)[0]
    offs = struct.unpack_from(">%dI" % count, blob, 4)
    return list(offs)


def walk_ucd(blob, off, artics, forks_seen):
    """One voice script's articulation indices (fork closure included)."""
    p = off

    def varint():
        nonlocal p
        v = blob[p]
        p += 1
        if v & 0x80:
            v = ((v & 0x7F) << 8) | blob[p]
            p += 1
        return v

    for _ in range(4096):
        instr = blob[p]
        p += 1
        if (instr & 0xF8) >= 0xD0:
            if instr == 0xD0:
                return
            elif instr == 0xD1:
                artics.add(varint())
            elif instr in (0xD2, 0xD3, 0xD5, 0xD6, 0xD7, 0xD8, 0xDC,
                           0xDD, 0xDE):
                p += 1
            elif instr == 0xD4:
                for _ in range(6):
                    varint()
            elif instr == 0xD9:
                target = varint()
                if target not in forks_seen:
                    forks_seen.add(target)
                    yield target
            # 0xDA, 0xDB, 0xDF, 0xE0: no args
        else:
            if (instr & 7) == 7:
                varint()
    raise ValueError("ucd script at 0x%X never ended" % off)


def walk_tbl(blob, off):
    """One articulation's trigger args (B1_sounds2 sound indices)."""
    p = off
    triggers = set()

    def varint():
        nonlocal p
        v = blob[p]
        p += 1
        if v & 0x80:
            v = ((v & 0x7F) << 8) | blob[p]
            p += 1
        return v

    for _ in range(8192):
        instr = blob[p]
        p += 1
        t = instr & 0xF
        if t & 0x8:
            v = blob[p]
            p += 1
            if v & 0x80:
                p += 1
        op = instr & 0xF0
        if op in (0x00, 0x10, 0x30, 0x50):
            p += 1
        elif op == 0x20:
            p += 2
        elif op == 0x40:
            p += 1
            varint()
        elif op == 0x60:
            triggers.add(varint())
        elif op == 0x70:
            return triggers
        elif op == 0x90:
            # jump_loop: static walk stops here -- everything reachable
            # inside the loop has been seen on the way in.
            return triggers
    raise ValueError("tbl script at 0x%X never ended" % off)


def main():
    argv = sys.argv[1:]
    rom_path = M.ROM_DEFAULT
    outdir = None
    if "--rom" in argv:
        rom_path = argv[argv.index("--rom") + 1]
    if "--outdir" in argv:
        outdir = argv[argv.index("--outdir") + 1]
    if not outdir:
        sys.exit("--outdir is required")
    rom = open(rom_path, "rb").read()

    os.makedirs(outdir, exist_ok=True)
    blobs = {}
    for name, (off, size) in FGM_FILES.items():
        blobs[name] = rom[off:off + size]
        with open(os.path.join(outdir, name), "wb") as fp:
            fp.write(blobs[name])

    index_of = fgm_names()
    ucd_offs = package_entries(blobs["fgm.ucd"])
    # The header's own end constant against the ROM's package: if the enum
    # were mis-numbered these would not agree, and every id below would be
    # wrong by however many entries were dropped.
    if index_of["nSYAudioFGMVoiceEnd"] != len(ucd_offs):
        sys.exit("gmFGMVoiceID ends at %d but fgm.ucd holds %d voice "
                 "scripts -- the enum was parsed wrong"
                 % (index_of["nSYAudioFGMVoiceEnd"], len(ucd_offs)))

    import ssb_sndsets     # imports this module; here, not at the top
    sounds = sorted(set(ssb_sndsets.tier_a_samples(rom)))
    print("%d bank-2 sounds across the tier-A sound sets" % len(sounds))

    # -- the sample pack: just those sounds, original index in the
    # directory, Yamaha ADPCM. PCM16 is what this wrote first and what the
    # 2 MB of sound RAM cannot afford: the whole of B1_sounds2 is
    # 5,329,168 samples, 10.6 MB uncompressed against 1.1 MB free once the
    # music bank is resident. At 4 bits a sample the AICA decodes in
    # hardware (AICA_SM_ADPCM) and a scene's whole closure fits. --
    r = SFX.BANKS[2]
    ctl = rom[r["ctl"][0]:r["ctl"][1]]
    tbl = rom[r["tbl"][0]:r["tbl"][1]]
    bank = SFX.parse_bank(ctl)
    dir_size = len(sounds) * 32
    # The samples start on a 32-byte boundary, and each is padded to one
    # below: src/dc/sndres.c reads them one at a time off the disc, and
    # KOS's ISO9660 driver streams a read (rather than copying it a
    # sector at a time through its cache) only when the file position is
    # 32-byte aligned (src/dc/assetroot.h).
    pcm_off = (16 + dir_size + 31) & ~31
    directory = b""
    blob = b""
    raw_bytes = 0
    halved = 0
    for idx in sounds:
        s = bank[idx]
        pcm, ls, le, lc = SFX.decode_sound(ctl, tbl, s)
        raw_bytes += len(pcm) * 2
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
        directory += struct.pack("<6I4BHH", pcm_off + len(blob), n,
                                 s["rate"], ls, le, lc,
                                 2, s["keybase"], s["detune"], shift, idx, 0)
        blob += ADPCM.encode(pcm)
        # The loader uploads with spu_memload_sq, which moves whole
        # 32-byte store-queue lines; pad so the last line of every sound
        # is inside the file rather than past the malloc that holds it.
        if len(blob) & 31:
            blob += b"\0" * (32 - (len(blob) & 31))
    with open(os.path.join(outdir, "fgm_sounds.pak"), "wb") as fp:
        head = struct.pack("<8sII", MAGIC, len(sounds), 0) + directory
        fp.write(head + b"\0" * (pcm_off - len(head)) + blob)
    print("fgm_sounds.pak: %d sounds, %d bytes ADPCM in sound RAM "
          "(%d as PCM16, %.1fx)%s"
          % (len(sounds), len(blob), raw_bytes,
             raw_bytes / float(len(blob) or 1),
             ", %d decimated" % halved if halved else ""))


if __name__ == "__main__":
    main()

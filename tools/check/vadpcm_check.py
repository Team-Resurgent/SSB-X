#!/usr/bin/env python3
"""ssb64-dc: our VADPCM decode against the decomp's, and against the ROM.

tools/lib/ssb_vadpcm.py is audio_codec.adpcm_decode with one line changed --
the scale-index boundary moved from `< 12` to `<= 12` (see that file for
what the old boundary cost). A one-line deviation from the decompilation's
own tooling has to be pinned from both sides, so this checks:

  * that the deviation is the only one -- the two decoders agree sample
    for sample on every waveform whose frames all sit below the boundary,
    which is most of both banks;
  * that the boundary is where the ROM says it is -- no frame in either
    bank carries a scale index above 12, so nothing is being waved
    through that the RSP would really have discarded;
  * that the frames at the boundary are the ones that matter -- they are
    counted, and the chiptune waveforms they cluster in are scored for
    how square they come out, which is the audible difference.

Usage: python3 tools/check/vadpcm_check.py [--rom <rom.z64>]
"""
import collections
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path[:0] = [os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), d)
                for d in ("lib", "export", "check")]
import ssb_paths as P        # noqa: E402
P.require_decomp()
sys.path.insert(0, P.DECOMP_TOOLS)
import audio_codec           # noqa: E402  (the decomp's, as the reference)
import ssb_meshexport as M   # noqa: E402
import ssb_sfxexport as SFX  # noqa: E402
import ssb_vadpcm as VADPCM  # noqa: E402

# The bank programs Mushroom Kingdom's NES medley is built from: the
# near-full-scale square waves, where the boundary frames cluster.
CHIPTUNE_PROGRAMS = (19, 20, 21)


def book_of(ctl, w):
    order = SFX.s32(ctl, w["book_off"])
    npred = SFX.s32(ctl, w["book_off"] + 4)
    book = list(struct.unpack_from(">%dh" % (npred * order * 8), ctl,
                                   w["book_off"] + 8))
    return book, order, npred


def squareness(pcm):
    """How much of a waveform sits within 15% of its peak -- 1.0 for an
    ideal two-level pulse, near zero for anything smooth."""
    peak = max(abs(v) for v in pcm) or 1
    return sum(1 for v in pcm if abs(v) > 0.85 * peak) / float(len(pcm))


def main():
    rom_path = M.ROM_DEFAULT
    if "--rom" in sys.argv:
        rom_path = sys.argv[sys.argv.index("--rom") + 1]
    rom = open(rom_path, "rb").read()

    checked = differ = boundary_waves = 0
    hist = collections.Counter()

    for bank_id in (1, 2):
        r = SFX.BANKS[bank_id]
        ctl = rom[r["ctl"][0]:r["ctl"][1]]
        tbl = rom[r["tbl"][0]:r["tbl"][1]]
        seen = set()
        for w in SFX.parse_bank(ctl):
            if w["base"] in seen:
                continue
            seen.add(w["base"])
            raw = tbl[w["base"]:w["base"] + w["len"] // 9 * 9]
            idx = [b >> 4 for b in raw[0::9]]
            hist.update(idx)
            book, order, npred = book_of(ctl, w)
            ours = VADPCM.adpcm_decode(raw, book, order, npred)
            if max(idx, default=0) > VADPCM.MAX_SCALE_IDX:
                sys.exit("vadpcm_check: bank %d waveform at 0x%X carries "
                         "scale index %d, past the boundary this decoder "
                         "acts on" % (bank_id, w["base"], max(idx)))
            if VADPCM.MAX_SCALE_IDX in idx:
                boundary_waves += 1
                continue        # the one place the two are meant to differ
            theirs = audio_codec.adpcm_decode(raw, book, order, npred)
            checked += 1
            if ours != theirs:
                differ += 1
                print("  bank %d, waveform at 0x%X: %d of %d samples differ"
                      % (bank_id, w["base"],
                         sum(1 for a, b in zip(ours, theirs) if a != b),
                         len(ours)))

    if differ:
        sys.exit("vadpcm_check: %d waveforms decode differently from the "
                 "decomp's codec below the scale boundary -- the deviation "
                 "is meant to be the boundary and nothing else" % differ)

    top = max(hist)
    print("%d waveforms with no boundary frame decode identically to "
          "audio_codec.adpcm_decode" % checked)
    print("scale indices used across both banks: %d..%d, %d frames at %d "
          "(%d waveforms carry one)"
          % (min(hist), top, hist[top], top, boundary_waves))

    # What the boundary is worth, on the waveforms it lands in.
    r = SFX.BANKS[1]
    ctl = rom[r["ctl"][0]:r["ctl"][1]]
    tbl = rom[r["tbl"][0]:r["tbl"][1]]
    import ssb_bgmexport as BGM
    _, _, programs = BGM.parse_music_bank(rom)
    worst = None
    for prog in CHIPTUNE_PROGRAMS:
        for j, s in enumerate(programs.get(prog, [])):
            w = s["wave"]
            raw = tbl[w["base"]:w["base"] + w["len"] // 9 * 9]
            n = sum(1 for b in raw[0::9] if (b >> 4) == VADPCM.MAX_SCALE_IDX)
            if n * 4 < len(raw) // 9:       # only the badly affected ones
                continue
            book, order, npred = book_of(ctl, w)
            was = squareness(audio_codec.adpcm_decode(raw, book, order, npred))
            now = squareness(VADPCM.adpcm_decode(raw, book, order, npred))
            print("  program %d sound %d: %d of %d frames at the boundary, "
                  "%.0f%% square -> %.0f%%"
                  % (prog, j, n, len(raw) // 9, was * 100, now * 100))
            if now <= was:
                worst = (prog, j, was, now)
    if worst:
        sys.exit("vadpcm_check: program %d sound %d got no cleaner "
                 "(%.2f -> %.2f); the correction is supposed to restore "
                 "the pulse" % worst)
    print("vadpcm_check: the boundary is the only deviation, and the ROM "
          "stays inside it")


if __name__ == "__main__":
    main()

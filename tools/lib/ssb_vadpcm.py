#!/usr/bin/env python3
"""ssb64-dc: VADPCM decode, with the one correction the ROM's data forces.

The decomp's own codec (ssb-decomp-re/tools/audio_codec.py) is what this
project decodes N64 samples with, per the repo rule: use the
decompilation's tooling as a dependency rather than writing our own. It
is right about everything here except one guard.

A VADPCM frame's header byte carries a 4-bit scale index; the residual
nibbles are shifted left by it. audio_codec.adpcm_decode reads

    scale = (1 << scale_idx) if scale_idx < 12 else 0

on the stated assumption that an out-of-range index is treated as zero by
the RSP microcode. The assumption is fine; the boundary is off by one.
Counting every frame in both of the ROM's banks:

    bank 1 (music)  126,745 frames, scale_idx 0..12, 411 of them at 12
    bank 2 (SFX)    333,073 frames, scale_idx 0..12, 4,756 of them at 12

Nothing in the ROM is out of range -- 12 is the largest index either bank
uses, and it is used deliberately, concentrated in the loudest waveforms,
which are exactly the ones that need the largest scale. Zeroing it throws
away a whole frame's residuals and leaves sixteen samples of bare
prediction.

What that cost is audible. The bank's chiptune instruments -- programs
19, 20 and 21, the near-full-scale square waves Mushroom Kingdom's medley
of the NES Super Mario Bros. themes is built from -- carry the bulk of
bank 1's index-12 frames: 68 of program 19 sound 3's 114 frames, 57 of
sound 4's 104, 30 of sound 2's 105. Scoring how much of a waveform sits
within 15% of its peak, which is what makes a square wave a square wave:

    program 19 sound 3    17.9% zeroed  ->  80.3% with the residual
    program 19 sound 4    17.2%         ->  63.0%
    program 19 sound 2    15.1%         ->  43.5%

A clean two-level pulse became ringing mush, in every path that decodes
these samples -- the pack the Dreamcast plays and the offline render
alike, which is why both sounded equally wrong against the N64.

So this module is audio_codec.adpcm_decode with `scale_idx <= 12`, and
tools/check/vadpcm_check.py pins the relationship: the two agree sample for
sample on every frame below the boundary, and no frame in either bank
goes above it.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path[:0] = [os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), d)
                for d in ("lib", "export", "check")]
import ssb_paths as P        # noqa: E402
P.require_decomp()
sys.path.insert(0, P.DECOMP_TOOLS)
import audio_codec           # noqa: E402

ADPCM_FRAME_BYTES = audio_codec.ADPCM_FRAME_BYTES

# The largest scale index the RSP can act on: the residual is a signed
# 4-bit nibble placed in the top of a 16-bit word, so a shift of 12 puts
# it exactly at full scale and anything beyond would overflow.
MAX_SCALE_IDX = 12


def adpcm_decode(adpcm_bytes, codebook, order, npredictors,
                 initial_state=None):
    """audio_codec.adpcm_decode, with the scale boundary at <= 12.

    Same arguments, same return: a list of s16 samples, nframes * 16 of
    them. Everything but the `scale` line is the decomp's algorithm.
    """
    coefs = []
    for p in range(npredictors):
        base = p * order * 8
        coefs.append([list(codebook[base + k * 8: base + k * 8 + 8])
                      for k in range(order)])

    state = list(initial_state) if initial_state else [0] * order
    out = []

    for fstart in range(0, len(adpcm_bytes), ADPCM_FRAME_BYTES):
        frame = adpcm_bytes[fstart: fstart + ADPCM_FRAME_BYTES]
        if len(frame) < ADPCM_FRAME_BYTES:
            break

        header = frame[0]
        scale_idx = header >> 4
        predictor_idx = header & 0x0F
        # DIVERGES from audio_codec.py, which stops at < 12. See above.
        scale = (1 << scale_idx) if scale_idx <= MAX_SCALE_IDX else 0

        residuals = []
        for b in frame[1:]:
            hi = (b >> 4) & 0x0F
            lo = b & 0x0F
            if hi >= 8:
                hi -= 16
            if lo >= 8:
                lo -= 16
            residuals.append(hi * scale)
            residuals.append(lo * scale)

        pred_book = coefs[predictor_idx]

        for half in range(2):
            half_resid = residuals[half * 8: half * 8 + 8]
            decoded = [0] * 8
            for j in range(8):
                acc = 0
                for k in range(order):
                    acc += state[k] * pred_book[k][j]
                for m in range(j):
                    acc += half_resid[m] * pred_book[order - 1][j - 1 - m]
                acc >>= 11
                decoded[j] = audio_codec._clip_s16(acc + half_resid[j])
            out.extend(decoded)
            state = decoded[8 - order: 8]

    return out

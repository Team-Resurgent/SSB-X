#!/usr/bin/env python3
"""ssb64-dc: Yamaha 4-bit ADPCM, the AICA's own sample format.

The N64's samples are VADPCM, decoded on the host with the decomp's codec
(tools/ssb_sfxexport.decode_sound). Storing the result as PCM16 is what
the port did for sound effects, and it is what the music bank cannot
afford: its 114 waveforms are 4,019,104 bytes of PCM16 against 2 MB of
AICA sound RAM. Yamaha ADPCM is 4 bits a sample -- 1,004,776 bytes, the
whole bank, resident, with room left for the effects.

This is the codec the AICA decodes in hardware (AICA_SM_ADPCM), so the
SH-4 never touches a sample after the upload. The format, as the chip
plays it:

  * Each nibble is a sign bit (8) and a magnitude m (0..7). The decoder
    keeps a predicted sample and a scale; a nibble moves the prediction by
    (2m + 1) * scale / 8 -- the magnitude rounded toward zero first, the
    sign applied after -- and clamps it to 16 bits.
  * The scale then grows or shrinks by SCALE_STEP[m] / 256 and is clamped
    to SCALE_MIN..SCALE_MAX. Both start at the chip's reset values:
    prediction 0, scale SCALE_MIN.

The encoder runs the same decoder alongside and, for each sample, picks
the magnitude whose step lands nearest the target: (2m + 1) / 8 of the
scale is closest to |d| at m = floor(4|d| / scale), capped at 7. `decode`
is the decoder alone and exists so tools/check/bgm_check.py can measure
what the round trip costs against the PCM the decomp's decoder produced.

The rounding is the part that matters. An earlier version here floored
the negative steps (Python's //) where the chip truncates, so its model of
the prediction walked away from what the AICA plays -- by up to half the
16-bit range on a test signal -- and bgm_check, decoding with the same
model, could not see it. On the same input this encoder's output is byte
for byte what KallistiOS's utils/wav2adpcm writes (at the pinned rev).
"""

# How the scale moves after a nibble of magnitude m, in 1/256ths.
SCALE_STEP = (230, 230, 230, 230, 307, 409, 512, 614)
SCALE_MIN = 127
SCALE_MAX = 24576


def _step(pred, scale, code):
    """One nibble through the decoder: the new (prediction, scale)."""
    m = code & 7
    move = ((2 * m + 1) * scale) >> 3
    pred = pred - move if code & 8 else pred + move
    pred = max(-32768, min(32767, pred))
    scale = max(SCALE_MIN, min(SCALE_MAX, (scale * SCALE_STEP[m]) >> 8))
    return pred, scale


def encode(pcm):
    """s16 samples -> packed nibbles, low nibble first. An odd sample count
    pads the last byte's high nibble with zero, which decodes as a tiny
    step and is why every wave is padded to an even length before this."""
    pred, scale = 0, SCALE_MIN
    out = bytearray((len(pcm) + 1) // 2)
    for i, s in enumerate(pcm):
        d = s - pred
        code = min(7, (abs(d) * 4) // scale) | (8 if d < 0 else 0)
        pred, scale = _step(pred, scale, code)
        out[i >> 1] |= code << 4 if i & 1 else code
    return bytes(out)


def decode(data, nsamples):
    """The inverse -- what the AICA plays."""
    pred, scale = 0, SCALE_MIN
    out = []
    for i in range(nsamples):
        b = data[i >> 1]
        pred, scale = _step(pred, scale, (b >> 4) if i & 1 else (b & 0xF))
        out.append(pred)
    return out

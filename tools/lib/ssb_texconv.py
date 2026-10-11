#!/usr/bin/env python3
"""ssb64-dc: the PVR texel primitives every exporter shares.

The PVR's twiddled layout, in 4bpp and 16bpp, done at build time so the
SH-4 uploads it as is, and the N64 RGBA5551 -> PVR ARGB1555 rotation.
ssb_assets.py re-exports them; tools/check/spotlight_check.py and
tools/check/particletex_check.py invert twidout to walk a baked texture back to
the ROM's texels.
"""
import struct


# ---- the twiddled layout

def _spread(v):
    """v's bits moved to the even positions: bit b to bit 2b."""
    out = 0
    for b in range(10):
        out |= ((v >> b) & 1) << (2 * b)
    return out


_SPREAD = [_spread(v) for v in range(1024)]


def twidout(x, y):
    """Where texel (x, y) sits in a twiddled PVR texture: the bits of y
    and x interleaved, y's in the even positions (Morton order, the same
    index KallistiOS's TWIDOUT computes). Coordinates are below 1024."""
    return _SPREAD[y & 1023] | (_SPREAD[x & 1023] << 1)


def twiddle_4bpp(src, w, h):
    """src: CI4 bytes, first texel in LOW nibble (PVR order).
    Returns twiddled 4bpp bytes. CRITICAL: each 16-bit word is emitted
    little-endian. The source data is N64 big-endian and the SH-4 is
    little-endian, so the swap happens here, once, at build time -- KOS's
    pvr_txr_load_ex writes native u16s and pvr_txr_load (store queues)
    copies bytes verbatim, so anything shipped pre-twiddled must already be
    in the target's word order. Verified byte-for-byte against KOS's own
    runtime twiddle (H1)."""
    out = bytearray((w * h) // 2)
    minv = min(w, h)
    mask = minv - 1
    for y in range(0, h, 2):
        for x in range(0, w, 2):
            a = src[(x + y * w) >> 1]
            b = src[(x + (y + 1) * w) >> 1]
            u16 = (a & 15) | ((b & 15) << 4) | ((a >> 4) << 8) | ((b >> 4) << 12)
            idx = twidout((x & mask) // 2, (y & mask) // 2) + \
                (x // minv + y // minv) * minv * minv // 4
            struct.pack_into("<H", out, idx * 2, u16)
    return out


def twiddle_16bpp(src, w, h):
    """src: 16bpp texels as a list of ints. Returns twiddled bytes.
    Same little-endian 16-bit word order as twiddle_4bpp."""
    out = bytearray(w * h * 2)
    minv = min(w, h)
    mask = minv - 1
    for y in range(h):
        for x in range(w):
            idx = twidout(x & mask, y & mask) + \
                (x // minv + y // minv) * minv * minv
            struct.pack_into("<H", out, idx * 2, src[y * w + x])
    return out


def rotate5551(n64):
    """N64 RGBA5551 (alpha bit 0) -> PVR ARGB1555 (alpha bit 15)."""
    return ((n64 >> 1) | ((n64 & 1) << 15)) & 0xFFFF

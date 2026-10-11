#!/usr/bin/env python3
"""ssb_roomwipeexport.py -- the opening Room's star wipe, off the ROM.

relocData file 63 (MVOpeningRoomTransition) is two flat display lists
the Room scene's tic-1040 wipe draws (mv/mvopening/mvopeningroom.c
mvOpeningRoomMakeTransition): the Overlay at 0x5A0, the inner star the
new scene shows through, and the Outline at 0xF40, the outer star whose
vertex colour is the red ring. Both are G_VTX/G_TRI1/G_TRI2 only, every
vertex at z 0, so the port needs nothing but the triangles' corners and
colours; src/dc/mvopeningroom.c projects them through the transition
camera with the DObj's animated scale and draws them as depth-ALWAYS
opaque polygons (see that file's wipe comment for why).

  --out romdisk/mvroomwipe.bin

Layout, little-endian:

  char  magic[4]  "RWIP"
  u32   version   1
  u32   overlay_tris, outline_tris
  then overlay_tris + outline_tris triangles, each three corners of
  { f32 x, f32 y, u32 argb }   (argb from the vertex colour, alpha FF)
"""

import argparse
import struct
import sys
import os

sys.path[:0] = [os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), d)
                for d in ("lib", "export", "check")]
import ssb_extract                      # noqa: E402
import ssb_assets as A                  # noqa: E402
from ssb_meshexport import ROM_DEFAULT  # noqa: E402

FILE_ID = 63
OVERLAY_DL = 0x5A0
OUTLINE_DL = 0xF40
VERSION = 1


def walk(f, off):
    """The DL's triangles as corner tuples, refusing any command a flat
    vertex-coloured list should not carry."""
    dl = A.flatten_dl(f, off)
    vbuf = [None] * 64
    tris = []
    for i in range(0, len(dl), 8):
        w0, w1 = struct.unpack_from('>II', dl, i)
        op = dl[i]
        if op == 0x01:
            n = (w0 >> 12) & 0xFF
            end = (w0 >> 1) & 0x7F
            for k, v in enumerate(A.decode_vtx(f, w1, n, lit=False)):
                vbuf[end - n + k] = v
        elif op in (0x05, 0x06):
            words = (w0,) if op == 0x05 else (w0, w1)
            for w in words:
                tris.append(tuple(vbuf[((w >> (16 - 8 * k)) & 0xFF) // 2]
                                  for k in range(3)))
    for t in tris:
        for v in t:
            if v is None or v[2] != 0:
                raise SystemExit("file 63: a corner off the z=0 plane "
                                 "or never loaded")
    return tris


def encode(tris):
    out = bytearray()
    for t in tris:
        for v in t:
            argb = 0xFF000000 | (v[5] << 16) | (v[6] << 8) | v[7]
            out += struct.pack('<ffI', float(v[0]), float(v[1]), argb)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--rom", default=ROM_DEFAULT)
    ap.add_argument("--out", required=True)
    args = ap.parse_args()

    rom = open(args.rom, 'rb').read()
    f = A.get_file(rom, FILE_ID, ssb_extract)
    overlay = walk(f, OVERLAY_DL)
    outline = walk(f, OUTLINE_DL)

    blob = bytearray(b'RWIP')
    blob += struct.pack('<III', VERSION, len(overlay), len(outline))
    blob += encode(overlay) + encode(outline)
    with open(args.out, 'wb') as fp:
        fp.write(blob)
    print("roomwipe: %d overlay + %d outline triangles, %d bytes"
          % (len(overlay), len(outline), len(blob)))


if __name__ == "__main__":
    main()

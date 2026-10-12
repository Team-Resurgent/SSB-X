#!/usr/bin/env python3
"""The Xbox dashboard's two pictures, made from the game's own art.

The dashboard shows a title image (128x128) beside the game and a save
image (64x64) beside its save. The N64 game has neither, so -- like the
Dreamcast port's VMU icon (tools/export/ssb_vmuart.py) -- both are made at
build time out of the game's logo art in the user's ROM, and nothing
ROM-derived is committed: the BMPs land in Platform/xbox/gen/ (gitignored),
and RXDK's bundler turns them into the DXT1 .xbx the dashboard needs
(Platform/xbox/titleimage.rdf, saveimage.rdf).

The picture: file 0's SmashLogo, the circle-and-cross emblem the menus draw
(read at its stored width, as a full circle -- see ssb_vmuart.py), filled
with the fire ramp of the title's "Smash" wordmark, top to bottom, on the
dark red of the title's background. The dashboard masks both pictures to a
circle, so a round emblem filling the frame is the right shape: nothing of
it is cut away and no empty margin shows inside the mask.

Usage: python tools/xbox/make_icons.py [--rom baserom.z64]
"""
import argparse
import os
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path[:0] = [os.path.join(ROOT, "tools", d) for d in ("lib", "export", "check")]
import ssb_vmuart as V  # noqa: E402

OUT = os.path.join(ROOT, "Platform", "xbox", "gen")
BACKGROUND = (0.20, 0.03, 0.02)     # a dark ember red under the emblem


def write_bmp(path, img):
    """24-bit bottom-up BMP: what the RXDK bundler reads."""
    row = (img.w * 3 + 3) & ~3
    data = bytearray()
    for y in range(img.h - 1, -1, -1):
        line = bytearray()
        for x in range(img.w):
            r, g, b, a = img.get(x, y)
            # over the background: DXT1 keeps no alpha the dashboard uses
            r = r * a + BACKGROUND[0] * (1 - a)
            g = g * a + BACKGROUND[1] * (1 - a)
            b = b * a + BACKGROUND[2] * (1 - a)
            line += bytes((int(min(1, b) * 255 + 0.5), int(min(1, g) * 255 + 0.5),
                           int(min(1, r) * 255 + 0.5)))
        line += b"\0" * (row - len(line))
        data += line
    hdr = struct.pack("<2sIHHI", b"BM", 54 + len(data), 0, 0, 54)
    dib = struct.pack("<IiiHHIIiiII", 40, img.w, img.h, 1, 24, 0, len(data), 2835, 2835, 0, 0)
    blob = hdr + dib + data
    # unchanged pictures keep their timestamps, so a Visual Studio build
    # does not re-bundle them every time
    if os.path.isfile(path):
        with open(path, "rb") as f:
            if f.read() == blob:
                return
    tmp = path + ".tmp"
    with open(tmp, "wb") as f:
        f.write(blob)
    os.replace(tmp, path)


def emblem_picture(rom, size):
    mask = V.sprite(rom, V.EMBLEM_FILE, V.EMBLEM, stored=True)
    smash = V.sprite(rom, V.TITLE_FILE, "Smash")
    stops = V.fire_stops(smash)
    big = V.resample(mask, size, size)
    out = V.Image(size, size)
    for y in range(size):
        r, g, b = V.ramp(stops, y / float(size - 1))
        for x in range(size):
            _, _, _, a = big.get(x, y)
            out.put(x, y, (r, g, b, a))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--rom", default=os.path.join(ROOT, "baserom.z64"))
    a = ap.parse_args()
    with open(a.rom, "rb") as f:
        rom = f.read()
    os.makedirs(OUT, exist_ok=True)
    for name, size in (("titleimage", 128), ("saveimage", 64)):
        write_bmp(os.path.join(OUT, name + ".bmp"), emblem_picture(rom, size))
    print("make_icons: titleimage.bmp 128x128, saveimage.bmp 64x64 -> Platform/xbox/gen")


if __name__ == "__main__":
    main()

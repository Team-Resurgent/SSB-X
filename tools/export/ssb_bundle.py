#!/usr/bin/env python3
"""ssb_bundle.py -- the disc's models.bnd: many small files, one extent.

A VS battle's load opens the 124 effect, weapon and item models back to
back (efDisplayInitAll, wpManagerPreloadAll, itModelPreloadAll -- the
preloads that keep every read out of the match). Loose on the GD-ROM each
one is a directory lookup, a fresh DMA stream, and the rest of its last
2048-byte sector wasted: 876 KB of models in 1,036 KB of track. This packs
them into one file that src/dc/assetroot.c mounts at boot and serves by
name from inside asset_open, so no loader knows the difference.

The layout, little-endian throughout (the SH4's):

    0   "BND2"   (BND1 had 24-byte names; the census packer's
                     opening-movie files need 32)
    4   u32 count
    8   u32 index_off   (always 16)
    12  u32 data_off    (the first entry's offset)
    16  count x { char name[32] (NUL padded), u32 off, u32 size }
        entries, each at a 32-byte offset, in the order the manifest
        names them; the file ends on a 32-byte boundary

Thirty-two, because KOS's iso_read hands a read to the GD-ROM's DMA stream
only when the file position is on a 32-byte boundary (src/dc/assetroot.h),
and an entry's first byte is a position. The ORDER is disc_layout.py's
BUNDLE_MODELS, the measured read order of the battle's preloads, so the
battle reads this file forward and never seeks inside it.

    ssb_bundle.py --out disc/models.bnd --src romdisk --src disc
    ssb_bundle.py --check disc/models.bnd --src romdisk --src disc
    ssb_bundle.py --names

--check rereads a bundle and holds it to the manifest and, entry by entry,
byte for byte to the loose files it was made from (./run.sh test bundle).

NOT YET TESTED ON REAL HARDWARE (2026-09-22): the bundle is verified only
by the host tests and an image served from the host filesystem, which does
not model the drive. The load-time gain and any effect on the
real-hardware stage-select crash are unmeasured. scripts/make_cdi.sh's
SSB_DISC_NO_BUNDLE=1 builds the loose-file disc for the comparison.
"""
import argparse
import os
import struct
import sys

sys.path[:0] = [os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), d)
                for d in ("lib", "export", "check")]
import disc_layout

MAGIC = b"BND2"
HEADER = struct.Struct("<4sIII")
ENTRY = struct.Struct("<32sII")
NAME_MAX = 31
ALIGN = 32


def align(n):
    return (n + ALIGN - 1) & ~(ALIGN - 1)


def find(name, srcs):
    for d in srcs:
        p = os.path.join(d, name)
        if os.path.isfile(p):
            return p
    raise SystemExit("ssb_bundle: %s is in none of %s" % (name, srcs))


def build(names, srcs):
    for n in names:
        if len(n.encode()) > NAME_MAX:
            raise SystemExit("ssb_bundle: name longer than %d: %s"
                             % (NAME_MAX, n))
    blobs = []
    for n in names:
        with open(find(n, srcs), "rb") as fp:
            blobs.append(fp.read())
    data_off = align(HEADER.size + ENTRY.size * len(names))
    index = []
    off = data_off
    for n, b in zip(names, blobs):
        index.append((n, off, len(b)))
        off = align(off + len(b))
    out = bytearray(off)
    HEADER.pack_into(out, 0, MAGIC, len(names), HEADER.size, data_off)
    for i, (n, o, s) in enumerate(index):
        ENTRY.pack_into(out, HEADER.size + ENTRY.size * i, n.encode(), o, s)
        out[o:o + s] = blobs[i]
    return bytes(out)


def read_index(blob):
    magic, count, index_off, data_off = HEADER.unpack_from(blob, 0)
    if magic != MAGIC:
        raise ValueError("not a bundle (magic %r)" % magic)
    entries = []
    for i in range(count):
        raw, off, size = ENTRY.unpack_from(blob, index_off + ENTRY.size * i)
        entries.append((raw.split(b"\0", 1)[0].decode(), off, size))
    return data_off, entries


def check(path, names, srcs):
    """Every way the bundle can disagree with the manifest or the loose
    files, as a list of lines; empty is a pass."""
    errs = []
    with open(path, "rb") as fp:
        blob = fp.read()
    try:
        data_off, entries = read_index(blob)
    except ValueError as e:
        return [str(e)]
    got = [e[0] for e in entries]
    if got != names:
        errs.append("index order is not disc_layout.bundle_names() "
                    "(%d names vs %d)" % (len(got), len(names)))
    if len(blob) % ALIGN:
        errs.append("file is %d bytes, not a multiple of %d"
                    % (len(blob), ALIGN))
    end = data_off
    for n, off, size in entries:
        if off % ALIGN:
            errs.append("%s at %d, not %d-aligned" % (n, off, ALIGN))
        if off < end:
            errs.append("%s at %d overlaps the entry before it" % (n, off))
        end = off + size
        if end > len(blob):
            errs.append("%s runs past the end of the file" % n)
            continue
        with open(find(n, srcs), "rb") as fp:
            loose = fp.read()
        if blob[off:off + size] != loose:
            errs.append("%s differs from its loose file" % n)
    return errs


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--out", help="bundle to write")
    ap.add_argument("--check", help="bundle to verify")
    ap.add_argument("--src", action="append", default=[],
                    help="directory the loose files are in (repeatable)")
    ap.add_argument("--names", action="store_true",
                    help="print the entry names, one per line, and stop")
    args = ap.parse_args()
    names = disc_layout.bundle_names()

    if args.names:
        print("\n".join(names))
        return 0
    if args.out:
        blob = build(names, args.src)
        with open(args.out, "wb") as fp:
            fp.write(blob)
        loose = sum(os.path.getsize(find(n, args.src)) for n in names)
        sectors = sum((os.path.getsize(find(n, args.src)) + 2047) // 2048
                      for n in names)
        print("ssb_bundle: %d files, %d bytes -> %s, %d bytes "
              "(%d sectors, %d loose)"
              % (len(names), loose, args.out, len(blob),
                 (len(blob) + 2047) // 2048, sectors))
    if args.check:
        errs = check(args.check, names, args.src)
        for e in errs:
            print("ssb_bundle: %s: %s" % (args.check, e), file=sys.stderr)
        if errs:
            return 1
        print("ssb_bundle: %s matches the manifest and %d loose files"
              % (args.check, len(names)))
    if not (args.out or args.check):
        ap.error("--out, --check or --names")
    return 0


if __name__ == "__main__":
    sys.exit(main())

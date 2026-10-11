#!/usr/bin/env python3
"""ssb64-dc: a relocData file's reloc chains, read out of the ROM.

file_info(rom, fid) returns the decompressed file, its file-table entry,
its intern chain, its extern sites as (site, target) byte offsets, and the
file id behind each extern site -- the ids the ROM stores, one u16 per
site, after the compressed data. The exporters that resolve a pointer into
another file (items, sprites, stages, weapon attributes) read it here.
"""
import os
import struct
import sys

sys.path[:0] = [os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), d)
                for d in ("lib", "export", "check")]
import ssb_extract           # noqa: E402
import ssb_assets as A       # noqa: E402


def file_info(rom, fid):
    f = A.get_file(rom, fid, ssb_extract)
    e = ssb_extract.read_entry(rom, ssb_extract.RELOC_SEG, fid)
    intern = A.walk_reloc(f, e["reloc_intern"])
    sites = []
    idx = e["reloc_extern"]
    while idx != 0xFFFF:
        nxt, wn = struct.unpack_from(">HH", f, idx * 4)
        sites.append((idx * 4, wn * 4))
        idx = nxt
    data_off = ssb_extract.rom_table_hi(
        ssb_extract.RELOC_SEG, ssb_extract.FILE_COUNT) + e["data_offset"]
    ids = struct.unpack_from(">%dH" % len(sites), rom,
                             data_off + e["compressed_size"] * 4)
    return f, e, intern, sites, list(ids)

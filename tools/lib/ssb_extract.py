#!/usr/bin/env python3
"""ssb64-dc: extract relocData files from the SSB64 US baserom.

Reads the LBReloc DMA table (seg 0x1AC870) and extracts one or more
relocData files to build/<reloc/>, vpk0-decompressing the compressed ones.
The vpk0 decoder is a faithful Python port of syDmaDecodeVpk0
(ssb-decomp-re/src/sys/dma.c) — the same algorithm the DC port must run.

Usage:
  python3 tools/lib/ssb_extract.py [--rom <rom.z64>] [--out build/reloc] <id>...

Prints per-file: id, compressed?, decompressed size, output path.
"""
import struct
import sys
import os

# Anchored to the repo root, not the caller's cwd, so a default resolves
# the same from a Makefile's directory as from the root.
REPO_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
ROM_DEFAULT = os.path.join(REPO_ROOT, "baserom.z64")
RELOC_DIR = os.path.join(REPO_ROOT, "build", "reloc")
RELOC_SEG = 0x1AC870
FILE_COUNT = 2132          # US
COMPRESSED_COUNT = 499     # first N files are vpk0-compressed (US)


def read_entry(data, seg, i):
    """Parse LBTableEntry (12 bytes, big-endian) at index i."""
    off = seg + i * 12
    w0, w1, w2 = struct.unpack(">III", data[off:off + 12])
    return {
        "is_compressed": (w0 >> 31) & 1,
        "data_offset": w0 & 0x7FFFFFFF,
        "reloc_intern": (w1 >> 16) & 0xFFFF,
        "compressed_size": w1 & 0xFFFF,        # in words (u32)
        "reloc_extern": (w2 >> 16) & 0xFFFF,
        "decompressed_size": w2 & 0xFFFF,      # in words (u32)
    }


def rom_table_hi(seg, file_count):
    """Data starts after the table: seg + (file_count + 1) * 12."""
    return seg + (file_count + 1) * 12


class BitReader:
    """MSB-first bit reader over a big-endian u16 stream (vpk0)."""

    def __init__(self, data):
        self.data = data
        self.csr = 0
        self.temp = 0
        self.bits = 0

    def read_u16(self):
        # Mirrors VPK0_READ_USHORT: also counts the 16 bits into the buffer.
        v = (self.data[self.csr] << 8) | self.data[self.csr + 1]
        self.csr += 2
        self.bits += 16
        return v

    def get_bits(self, n):
        if self.bits < n:
            # read_u16() already credits the 16 bits
            self.temp = ((self.temp << 16) | self.read_u16()) & 0xFFFFFFFF
        self.bits -= n
        return ((self.temp << (32 - n - self.bits)) & 0xFFFFFFFF) >> (32 - n)


class Node:
    __slots__ = ("left", "right", "value")

    def __init__(self):
        self.left = None
        self.right = None
        self.value = 0


def decode_vpk0(blob):
    """Port of syDmaDecodeVpk0 (dma.c). blob = full compressed bytes."""
    br = BitReader(blob)
    # magic "vpk0": two u16s, discarded from the bit stream
    br.read_u16()
    br.read_u16()
    br.bits -= 32
    # decompressed size (u32)
    size = br.read_u16() << 16 | br.read_u16()
    br.bits -= 32
    sample_method = br.get_bits(8)

    def build_tree():
        stack = [None] * 20
        size_s = 0
        while True:
            v = br.get_bits(1)
            if v != 0 and size_s < 2:
                break
            node = Node()
            if v != 0:
                node.left = stack[size_s - 2]
                node.right = stack[size_s - 1]
                stack[size_s - 2] = node
                size_s -= 1
            else:
                node.value = br.get_bits(8)
                stack[size_s] = node
                size_s += 1
        return stack[0]

    offsets_tree = build_tree()
    lengths_tree = build_tree()

    out = bytearray(size)
    out_ptr = 0
    while out_ptr < size:
        v = br.get_bits(1)
        if not v:
            out[out_ptr] = br.get_bits(8)
            out_ptr += 1
        else:
            if sample_method != 0:
                sp64 = 0
                n = offsets_tree
                while n.left is not None:
                    b = br.get_bits(1)
                    n = n.left if not b else n.right
                v = br.get_bits(n.value)
                if v <= 2:
                    sp64 = v + 1
                    n = offsets_tree
                    while n.left is not None:
                        b = br.get_bits(1)
                        n = n.left if not b else n.right
                    v = br.get_bits(n.value)
                copy_src = out_ptr - v * 4 - sp64 + 8
            else:
                n = offsets_tree
                while n.left is not None:
                    b = br.get_bits(1)
                    n = n.left if not b else n.right
                v = br.get_bits(n.value)
                copy_src = out_ptr - v
            n = lengths_tree
            while n.left is not None:
                b = br.get_bits(1)
                n = n.left if not b else n.right
            v = br.get_bits(n.value)
            while v > 0:
                out[out_ptr] = out[copy_src]
                out_ptr += 1
                copy_src += 1
                v -= 1
    return bytes(out)


def extract_file(data, file_id, outdir):
    e = read_entry(data, RELOC_SEG, file_id)
    base = rom_table_hi(RELOC_SEG, FILE_COUNT)
    rom_off = base + e["data_offset"]
    if e["is_compressed"]:
        blob = data[rom_off:rom_off + e["compressed_size"] * 4]
        raw = decode_vpk0(blob)
        assert len(raw) == e["decompressed_size"] * 4, \
            f"vpk0 size mismatch: {len(raw)} vs {e['decompressed_size']*4}"
        label = "vpk0"
    else:
        raw = data[rom_off:rom_off + e["decompressed_size"] * 4]
        label = "raw"
    os.makedirs(outdir, exist_ok=True)
    out = os.path.join(outdir, f"file_{file_id:04d}.bin")
    with open(out, "wb") as f:
        f.write(raw)
    print(f"file {file_id}: {label}, {len(raw)} bytes -> {out} "
          f"(rom 0x{rom_off:x})")
    return out


def main():
    rom = ROM_DEFAULT
    outdir = RELOC_DIR
    ids = []
    argv = sys.argv[1:]
    i = 0
    while i < len(argv):
        a = argv[i]
        if a == "--rom" and i + 1 < len(argv):
            rom = argv[i + 1]
            i += 2
        elif a == "--out" and i + 1 < len(argv):
            outdir = argv[i + 1]
            i += 2
        else:
            ids.append(a)
            i += 1
    if not ids:
        print(__doc__)
        sys.exit(1)
    data = open(rom, "rb").read()
    for fid in ids:
        extract_file(data, int(fid), outdir)


if __name__ == "__main__":
    main()

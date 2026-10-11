#!/usr/bin/env python3
"""What each machine's FPU is allowed to do, on both sides of the port.

The SH-4 has a fused multiply-accumulate, `fmac`, which computes a*b + c
with **one** rounding. The VR4300 has no such instruction: the decomp
builds with `-mips2` (ssb-decomp-re/Makefile:94), and a fused
multiply-add first appears in MIPS IV. So wherever GCC contracts the
game's own arithmetic into an `fmac` -- which its default
`-ffp-contract=fast` does, silently, with nothing in this repository
saying so -- the port computes a number the N64 could not have computed.

Two legs, one subject:

  rom      every code range smashbrothers.us.yaml names, scanned for a
           fused multiply-add encoding. This is the claim "the N64 never
           did this" made against the ROM's own bytes rather than
           against the ISA manual.

  objects  the built Dreamcast objects, scanned for `fmac` (and, for
           scripts/test_purity.sh, for `fsca` and `fsrra`), with the
           SH-4's constant pools excluded -- see pool_addresses below,
           because they are the reason a naive grep is wrong.

Usage:
  python3 tools/check/fpu_check.py                       # the rom leg
  python3 tools/check/fpu_check.py --objects a.o b.o ... # the object leg
  python3 tools/check/fpu_check.py --objects ... --insns fmac,fsca,fsrra
"""
import argparse
import os
import re
import struct
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
DECOMP = os.environ.get("SSB_DECOMP_DIR", "/opt/ssb-decomp-re")
if not os.path.isdir(DECOMP):
    DECOMP = os.path.join(ROOT, "ssb-decomp-re")
ROM_DEFAULT = os.path.join(ROOT, "baserom.z64")
OBJDUMP = os.environ.get("SH_OBJDUMP", "sh-elf-objdump")

# ---- the N64 leg -----------------------------------------------------
#
# MIPS opcode 6 bits. COP1X is 0b010011, and it is where MIPS IV put
# MADD.fmt, MSUB.fmt, NMADD.fmt and NMSUB.fmt -- the whole of the
# architecture's fused multiply-add. On MIPS II, which is what the game
# was compiled for, the opcode is not an instruction at all: it raises a
# reserved-instruction exception. So the test is the opcode, not the
# function field, and a hit either way would be news.
OP_COP1X = 0x13

# Which subsegment types of smashbrothers.us.yaml hold instructions.
CODE_TYPES = {"c", "hasm"}

SUB_LIST = re.compile(r"^\s+- \[(0x[0-9A-Fa-f]+),\s*([\w.]+),")
SUB_DICT = re.compile(r"^\s+- \{\s*start:\s*(0x[0-9A-Fa-f]+),\s*type:\s*([\w.]+)")
SEG_START = re.compile(r"^\s+start:\s*(0x[0-9A-Fa-f]+)\s*$")


def yaml_code_ranges(path):
    """[(start, end, type)] for every code subsegment, in ROM order.

    Splat's subsegment offsets are absolute ROM offsets, and a
    subsegment runs to wherever the next one starts -- which may be a
    .data or .rodata block inside the same segment, or the next segment
    entirely. So every boundary the file names is collected, sorted,
    and the code ones keep their span.
    """
    marks = []
    for line in open(path):
        m = SUB_LIST.match(line) or SUB_DICT.match(line)
        if m:
            marks.append((int(m.group(1), 16), m.group(2)))
            continue
        m = SEG_START.match(line)
        if m:
            marks.append((int(m.group(1), 16), "segment"))
    # A segment's own `start:` repeats its first subsegment's offset, and
    # two subsegments can share one, so collapse to distinct offsets
    # first: an offset is code if anything the yaml puts there is.
    kinds = {}
    for off, kind in marks:
        kinds.setdefault(off, set()).add(kind)
    offs = sorted(kinds)
    out = []
    for i, off in enumerate(offs):
        code = kinds[off] & CODE_TYPES
        if not code:
            continue
        if i + 1 < len(offs):
            out.append((off, offs[i + 1], sorted(code)[0]))
    return out


def scan_mips(data, ranges):
    """Fused multiply-adds in the ROM's code, and how much code that was."""
    hits = []
    words = 0
    for start, end, _kind in ranges:
        end = min(end, len(data))
        n = (end - start) // 4
        words += n
        for i, (w,) in enumerate(struct.iter_unpack(">I", data[start:start + n * 4])):
            if (w >> 26) == OP_COP1X:
                hits.append((start + i * 4, w))
    return hits, words


def rom_leg(rom_path):
    yaml_path = os.path.join(DECOMP, "smashbrothers.us.yaml")
    if not os.path.isfile(yaml_path):
        print(f"FAIL rom: no decomp at {DECOMP} -- set SSB_DECOMP_DIR")
        return 1
    data = open(rom_path, "rb").read()
    ranges = yaml_code_ranges(yaml_path)
    if len(ranges) < 500:
        print(f"FAIL rom: only {len(ranges)} code subsegments in the yaml")
        return 1

    # The check bites: a MADD.S planted in a copy of the first code
    # range has to come back, or the scan below proves nothing.
    madd_s = (OP_COP1X << 26) | 0x20            # MADD.S fd,fr,fs,ft
    planted = bytearray(data)
    at = ranges[0][0]
    planted[at:at + 4] = struct.pack(">I", madd_s)
    seen, _ = scan_mips(bytes(planted), ranges[:1])
    if not any(a == at for a, _ in seen):
        print("FAIL rom: a planted MADD.S was not found -- the scan is blind")
        return 1

    hits, words = scan_mips(data, ranges)
    if hits:
        print(f"FAIL rom: {len(hits)} fused multiply-add(s) in the ROM's "
              f"code, first at 0x{hits[0][0]:06X} (0x{hits[0][1]:08X})")
        return 1
    print(f"rom: {words} instructions over {len(ranges)} code subsegments, "
          f"no fused multiply-add -- the game was built -mips2, which has "
          f"no encoding for one")
    return 0


# ---- the Dreamcast leg -----------------------------------------------

DIS_LINE = re.compile(r"^\s*([0-9a-f]+):\t([0-9a-f ]+)\t(\S+)\s*(.*)$")
DIS_SECT = re.compile(r"^Disassembly of section (\S+):")
DIS_LIT = re.compile(r"^([0-9a-f]+) <")
# The three PC-relative loads. mova names the start of a block -- a
# constant pool or a switch table, told apart below; the other two name
# one entry each.
POOL_LOADS = {"mov.w": 2, "mov.l": 4, "mova": 4}


def disassemble(obj):
    """{section: [(addr, bytes, mnemonic, operands)]} for one object."""
    out = subprocess.run([OBJDUMP, "-d", obj], capture_output=True,
                         text=True, check=True).stdout
    sec = None
    rows = {}
    for line in out.splitlines():
        m = DIS_SECT.match(line)
        if m:
            sec = m.group(1)
            rows.setdefault(sec, [])
            continue
        m = DIS_LINE.match(line)
        if m:
            raw = bytes(int(b, 16) for b in m.group(2).split())
            rows.setdefault(sec, []).append(
                (int(m.group(1), 16), raw, m.group(3), m.group(4)))
    return rows


DIS_IMM = re.compile(r"#(-?\d+),(r\d+)$")


def switch_count(rows, i):
    """Entries the switch's own range check says its table has, or None.

    GCC guards a `braf` table with the unsigned range test that sends
    everything above the last case to the default:

        mov     #91,r1
        cmp/hi  r1,r9           ! the index
        bf.s    Lmova
    so the table is 92 entries and nothing else in the disassembly says
    so. `switch_table` below used to bound it by its own entries -- the
    lowest one is the first case label, and the cases sit under the table
    -- which holds only when GCC puts them there. It is free not to:
    ft/ftparam.c's ftParamMakeEffect has a 92-entry table followed by
    four hundred bytes of the function's own head before the first case,
    and reading into that lands on a `nop`, whose halfword is a
    displacement of 9 and collapses the bound to the table's second byte.
    Fifty of the table's default entries then read as `fmac`.

    Only the `mov #imm` form is read: past 127 GCC loads the bound from
    the constant pool instead, and then there is no count here to find.
    The guard is not always adjacent -- here twelve instructions of the
    function's epilogue sit between the branch and the `mova` -- so the
    window is generous, and a count picked up from the wrong compare is
    caught by the entry scan below, which still takes the lower of the
    two. Erring long falls back to the old bound; erring short
    under-covers, which fails loudly rather than passing quietly.
    """
    reg = None
    for a, _raw, mn, ops in reversed(rows[max(0, i - 24):i]):
        if reg is None:
            if mn in ("cmp/hi", "cmp/gt"):
                reg = ops.split(",")[0]
            continue
        m = DIS_IMM.match(ops)
        if m and mn == "mov" and m.group(2) == reg:
            n = int(m.group(1))
            return n + 1 if 0 <= n < 1024 else None
    return None


def switch_table(rows, i, code, size):
    """The extent of the switch table a `mova` at rows[i] introduces.

    GCC compiles a switch on the SH-4 as

        mova    Lbase,r0        ! r0 = the table
        add     r1,r1           ! or shll2, for a 4-byte table
        mov.w   @(r0,r1),r1     ! the entry
        braf    r1              ! PC + 4 + entry
        nop
    Lbase:  .word  Lcase - (the braf's PC + 4)  ...

    so the table is halfwords of *signed* displacement, and no PC-relative
    load names any of them past the first. What bounds it is the table
    itself: every case label is one of its own entries, and the ones that
    sit after the table -- there is always at least one, the layout puts
    the cases below it -- say where it stops. Reading entries until the
    next one would be at or past the lowest such label lands exactly on
    the first case, which is where code resumes.

    Returns (start, end) or None if this mova is not a switch table.
    """
    width = None
    braf_at = None
    for a, _raw, mn, ops in rows[i + 1:i + 7]:
        if mn in ("mov.w", "mov.l") and ops.startswith("@(r0,"):
            width = 2 if mn == "mov.w" else 4
        elif mn == "braf":
            braf_at = a
            break
    if width is None or braf_at is None:
        return None
    m = DIS_LIT.match(rows[i][3])
    if not m:
        return None
    base = int(m.group(1), 16)
    origin = braf_at + 4
    # the range check's count first, where there is one: it is the only
    # thing that bounds a table the cases do not immediately follow
    count = switch_count(rows, i)
    end = (base + count * width) if count is not None else (1 << 30)
    a = base
    while a + width <= min(end, size):
        raw = code[a:a + width]
        if len(raw) < width:
            break
        v = int.from_bytes(raw, "little", signed=True)
        target = origin + v
        if base < target < size:
            end = min(end, target)
        a += width
    return (base, min(end, size))


def data_ranges(rows, size):
    """The halfwords of a section that are constants, not instructions.

    The SH-4 has no immediate long, so every float, every address and
    every constant over eight bits is a word sitting *inside* .text,
    loaded by a PC-relative mov; a switch is a table of displacements in
    the same place. objdump disassembles all of it like any other code,
    and one halfword in sixteen decodes as an `fmac` -- which is why a
    grep of the disassembly finds ninety-three of them in a build that
    contains none. Every constant is reachable by a PC-relative load or
    it would not have been emitted, so the loads are what say where the
    data is.
    """
    code = bytearray(size)
    for addr, raw, _mn, _ops in rows:
        code[addr:addr + len(raw)] = raw
    out = set()
    for i, (_addr, _raw, mn, ops) in enumerate(rows):
        n = POOL_LOADS.get(mn)
        if n is None:
            continue
        if mn == "mova":
            span = switch_table(rows, i, bytes(code), size)
            if span is not None:
                out.update(range(span[0], span[1], 2))
                continue
        m = DIS_LIT.match(ops)
        if m:
            a = int(m.group(1), 16)
            out.update(range(a, a + n, 2))
    return out


def scan_object(obj, wanted):
    """{mnemonic: count} of real instructions, constants excluded."""
    found = {}
    for _sec, rows in disassemble(obj).items():
        if not rows:
            continue
        size = max(a + len(r) for a, r, _m, _o in rows)
        data = data_ranges(rows, size)
        for addr, _raw, mn, _ops in rows:
            if mn in wanted and addr not in data:
                found[mn] = found.get(mn, 0) + 1
    return found


def object_leg(objs, insns):
    fail = 0
    total = 0
    for obj in objs:
        found = scan_object(obj, insns)
        if found:
            what = ", ".join(f"{n} {m}" for m, n in sorted(found.items()))
            print(f"FAIL objects: {obj} emits {what}", file=sys.stderr)
            total += sum(found.values())
            fail = 1
    if not fail:
        print(f"objects: no {'/'.join(sorted(insns))} in {len(objs)} objects")
    return fail


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--rom", default=ROM_DEFAULT)
    ap.add_argument("--objects", nargs="+")
    ap.add_argument("--insns", default="fmac")
    args = ap.parse_args()

    if args.objects:
        return object_leg(args.objects, set(args.insns.split(",")))

    if not os.path.isfile(args.rom):
        print(f"no baserom at {args.rom} -- skipping")
        return 0
    rc = rom_leg(args.rom)
    if rc == 0:
        print("fpu_check: the N64's code has no fused multiply-add, so "
              "neither may the port's")
    return rc


if __name__ == "__main__":
    sys.exit(main())

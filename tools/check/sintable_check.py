#!/usr/bin/env python3
"""ssb64-dc: gSYSinTable, against the sine function and against the decomp.

tools/export/ssb_sinexport.py cuts 4 KB out of the boot segment's .data and emits
it as C. Two things about that are worth a check rather than a comment.

  * The address is arithmetic, not a segment. smashbrothers.us.yaml has no
    entry for the table -- it lives inside the `main` code segment -- so
    the exporter converts symbols/system.txt's vram address by that
    segment's own vram/ROM pair. Both are re-read here, so a table that
    slid would be caught by the decomp's own numbers rather than by the
    values looking wrong.

  * It is not the sine function. Its second half mirrors about index
    1023.5, not 1024: gSYSinTable[2047] is 0 where the rounded sine is 50,
    and every entry above 1024 is one place out. Computing the table --
    the obvious thing to do on a machine with an FPU and an FSCA
    instruction -- would put the particle interpreter's vortex arm and
    sys/matrix.c's fast sin and cos on a different curve from the N64's
    over half their domain. This measures how far, so the decision to
    carry 4 KB of ROM has a number on it.

Usage: python3 tools/check/sintable_check.py [--rom <rom.z64>]
"""
import argparse
import math
import os
import re
import sys

sys.path[:0] = [os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), d)
                for d in ("lib", "export", "check")]
import ssb_sinexport as SE      # noqa: E402

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
DECOMP = os.environ.get("SSB_DECOMP_DIR", "/opt/ssb-decomp-re")
if not os.path.isdir(DECOMP):
    DECOMP = os.path.join(REPO_ROOT, "ssb-decomp-re")


def symbol_vram():
    """gSYSinTable's vram address and size out of symbols/system.txt."""
    path = os.path.join(DECOMP, "symbols", "system.txt")
    with open(path) as f:
        m = re.search(r"^gSYSinTable\s*=\s*(0x[0-9A-Fa-f]+);\s*//\s*size:(0x[0-9A-Fa-f]+)",
                      f.read(), re.M)
    if m is None:
        raise ValueError(f"{path}: no gSYSinTable")
    return int(m.group(1), 16), int(m.group(2), 16)


def main_segment():
    """The `main` code segment's ROM start and vram, out of the yaml."""
    path = os.path.join(DECOMP, "smashbrothers.us.yaml")
    with open(path) as f:
        m = re.search(r"-\s*name:\s*main\s*\n\s*type:\s*code\s*\n"
                      r"\s*start:\s*(0x[0-9A-Fa-f]+)\s*\n\s*vram:\s*(0x[0-9A-Fa-f]+)",
                      f.read())
    if m is None:
        raise ValueError(f"{path}: no `main` code segment")
    return int(m.group(1), 16), int(m.group(2), 16)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--rom", default=SE.ROM_DEFAULT)
    args = ap.parse_args()

    if not os.path.isdir(DECOMP):
        print(f"no decomp checkout at {DECOMP} -- skipping")
        return 0

    # 1. The exporter's address is the decomp's, converted by the decomp's
    #    own segment.
    vram, size = symbol_vram()
    rom_start, rom_vram = main_segment()
    want_rom = vram - rom_vram + rom_start
    if (vram, size) != (SE.SINTABLE_VRAM, SE.SINTABLE_LEN * 2):
        print(f"FAIL symbol: system.txt has {vram:#x}/{size:#x}, the exporter "
              f"{SE.SINTABLE_VRAM:#x}/{SE.SINTABLE_LEN * 2:#x}")
        return 1
    if want_rom != SE.SINTABLE_ROM:
        print(f"FAIL address: the `main` segment puts {vram:#x} at ROM "
              f"{want_rom:#x}, the exporter reads {SE.SINTABLE_ROM:#x}")
        return 1
    print(f"address: {vram:#010x} is ROM {want_rom:#08x} by the yaml's "
          f"`main` segment ({rom_vram:#x} -> {rom_start:#x})")

    with open(args.rom, "rb") as f:
        rom = f.read()
    t = SE.read_table(rom)

    # 2. The rising half is the rounded sine, and the peak is exact.
    worst_rise = max(abs(t[i] - round(math.sin(i * math.pi / SE.SINTABLE_LEN) * 32768))
                     for i in range(SE.SINTABLE_LEN // 4 + 1))
    if worst_rise > 1 or t[0] != 0 or t[1024] != 32768:
        print(f"FAIL rise: t[0]={t[0]}, t[1024]={t[1024]}, worst |diff| "
              f"{worst_rise} over the first quarter")
        return 1

    # 3. And the whole table is that quarter mirrored about 1023.5.
    for i in range(SE.SINTABLE_LEN):
        if t[i] != t[SE.SINTABLE_LEN - 1 - i]:
            print(f"FAIL mirror: t[{i}]={t[i]}, t[{SE.SINTABLE_LEN - 1 - i}]="
                  f"{t[SE.SINTABLE_LEN - 1 - i]}")
            return 1
    print(f"shape: t[0..1024] is round(sin*32768) to within {worst_rise}, "
          f"and t[i] == t[{SE.SINTABLE_LEN - 1}-i] throughout")

    # 4. What computing it instead would have cost.
    diffs = [t[i] - round(math.sin(i * math.pi / SE.SINTABLE_LEN) * 32768)
             for i in range(SE.SINTABLE_LEN)]
    worst = max(diffs, key=abs)
    at = diffs.index(worst)
    off = sum(1 for d in diffs if abs(d) > 1)
    print(f"vs sin: {off} of {SE.SINTABLE_LEN} entries differ by more than 1, "
          f"worst {worst} at index {at} ({abs(worst) / 32768.0:.5f} of unit)")
    print("sintable_check: gSYSinTable matches the decomp's address and its "
          "own shape")
    return 0


if __name__ == "__main__":
    sys.exit(main())

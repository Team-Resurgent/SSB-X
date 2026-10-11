#!/usr/bin/env python3
"""The port's __sinf and __cosf against libultra's, three ways.

The port's gusinf.c and gucosf.c are ssb-decomp-re/src/libultra/gu's
sinf.c and cosf.c as tools/export/ssb_trigexport.py writes them at build
time, with one change that matters: each constant is written `{hi, lo}`
through a union that overlays a double, and that only reads correctly on
a big-endian machine. Compiled as written on the SH-4, the
leading coefficient comes out as -3.4e-17 instead of -0.1666666 and
sin(x) quietly becomes x -- which for the small angles most of the game
uses looks entirely plausible. So the swap is checked rather than
trusted, and so is everything around it:

  constants  the doubles the port's tables produce are the ones the ROM
             carries. smashbrothers.us.yaml puts these two functions'
             .rodata at 0x03FD60 and 0x03FF20; both hold the same eight
             doubles -- five polynomial coefficients, 1/PI, and PI split
             in two -- and the check reads them out of the baserom and
             compares. Nothing here trusts the hex in the source.

  text       every other line of the output is the decomp's line. The
             only differences allowed are the constant lines, whose
             expected form the check derives from the ROM itself, the two
             `#pragma weak` lines the port drops (the exporter says why),
             and comments and __libm_qnan_f the exporter adds.

  run        both functions swept against the C library's over float bit
             patterns, reporting how far apart they are. Not a
             correctness test -- libultra's polynomial is not required to
             be correctly rounded, and its answer is the one the port
             wants whatever glibc says -- but it is what catches the
             collapse above, and it is the number that says what the
             libultra swap changed. --exhaustive sweeps all 2^32 arguments, which for
             a float is a complete answer rather than a sample.

Usage: python3 tools/check/trig_check.py [--rom <rom.z64>] [--exhaustive]
                                   [--stride N]
"""
import argparse
import difflib
import os
import re
import struct
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
SRC = os.path.join(ROOT, "src", "dc")
DECOMP = os.environ.get("SSB_DECOMP_DIR", "/opt/ssb-decomp-re")
if not os.path.isdir(DECOMP):
    DECOMP = os.path.join(ROOT, "ssb-decomp-re")

ROM_DEFAULT = os.path.join(ROOT, "baserom.z64")

sys.path.insert(0, os.path.join(ROOT, "tools", "export"))
import ssb_trigexport  # noqa: E402

# Where main() has the exporter write the two files: a temporary
# directory, so the check needs no build first.
GEN = None

# The .rodata segments of smashbrothers.us.yaml, and the eight doubles
# each holds: P[0..4], rpi, pihi, pilo. (A ninth word pair, the `zero`
# both files declare, follows and is zero.)
FILES = {
    "gusinf.c": {"decomp": "sinf.c", "rodata": 0x03FD60},
    "gucosf.c": {"decomp": "cosf.c", "rodata": 0x03FF20},
}
NCONST = 8
QNAN_ROM = 0x03FF70     # libultra/gu/libm_vals, __libm_qnan_f


def rom_constants(rom, off):
    """The eight doubles a function's .rodata carries, big-endian."""
    return [struct.unpack_from(">d", rom, off + i * 8)[0] for i in range(NCONST)]


def source_constants(path):
    """The doubles the port's `du` initializers produce on this machine,
    in order, with the line each came from."""
    out = []
    with open(path) as f:
        for n, line in enumerate(f, 1):
            for m in re.finditer(r"\{\s*0x([0-9a-fA-F]{8}),\s*0x([0-9a-fA-F]{8})\s*\}",
                                 line):
                lo, hi = int(m.group(1), 16), int(m.group(2), 16)
                # As the C compiler will lay them out here: first member
                # at the low address, which on a little-endian machine is
                # the low half of the double.
                out.append((n, struct.unpack("<d", struct.pack("<II", lo, hi))[0]))
    return out


def check_constants(rom) -> int:
    for port, info in FILES.items():
        want = rom_constants(rom, info["rodata"])
        got = source_constants(os.path.join(GEN, port))
        if len(got) != NCONST:
            print(f"FAIL {port}: {len(got)} constants, the ROM has {NCONST}")
            return 1
        for i, ((line, v), w) in enumerate(zip(got, want)):
            if v != w:
                print(f"FAIL {port}:{line}: constant {i} is {v!r}, "
                      f"the ROM's is {w!r}")
                return 1
        print(f"constants: {port} matches the ROM at {info['rodata']:#08x} "
              f"({NCONST} doubles)")
    return 0


def expected_const_line(decomp_line, rom, off):
    """What a decomp constant line has to become, derived from the ROM:
    the same line with each word pair swapped."""
    def swap(m):
        return "{ 0x%s,\t0x%s }" % (m.group(2), m.group(1))
    return re.sub(r"\{\s*0x([0-9a-fA-F]{8}),\s*0x([0-9a-fA-F]{8})\s*\}",
                  swap, decomp_line)


def is_added(line):
    """A line the port may add: a comment, a blank, or the one definition
    the decomp keeps in assembly (libm_vals.s)."""
    s = line.strip()
    return (s == "" or s.startswith("/*") or s.startswith("*") or
            s.startswith("*/") or "__libm_qnan_f" in s)


def allowed_forms(line):
    """The forms a decomp line may take in the copy: itself, its constant
    pair swapped, or -- for a dropped alias -- any comment quoting it."""
    swapped = expected_const_line(line, None, None)
    if swapped != line:
        return ("const", [swapped])
    if line.startswith("#pragma weak"):
        return ("alias", [line])
    return ("same", [line])


def check_text(rom) -> int:
    for port, info in FILES.items():
        with open(os.path.join(DECOMP, "src", "libultra", "gu", info["decomp"])) as f:
            a = f.read().splitlines()
        with open(os.path.join(GEN, port)) as f:
            b = f.read().splitlines()

        # Every line of the decomp's file, in its order, in one of the
        # forms it is allowed to take. Anything the copy has in between
        # has to be a comment, a blank, or __libm_qnan_f.
        j, changed = 0, 0
        for i, line in enumerate(a, 1):
            kind, forms = allowed_forms(line)
            if kind == "alias":
                # dropped outright; a copy that kept it fails below, on
                # the line after it
                changed += 1
                continue
            found = -1
            for k in range(j, len(b)):
                if b[k] in forms:
                    found = k
                    break
                if not is_added(b[k]):
                    break
            if found < 0:
                nxt = b[j] if j < len(b) else "<end of file>"
                print(f"FAIL text: libultra/gu/{info['decomp']}:{i} is")
                print(f"       {line!r}")
                print(f"     the copy has")
                print(f"       {nxt!r}")
                if kind != "same":
                    print(f"     and the only form allowed here is")
                    print(f"       {forms[0]!r}")
                return 1
            if kind != "same":
                changed += 1
            j = found + 1

        # A line left unswapped needs no separate test: the walk above
        # looks for the swapped form and does not find it, and the
        # constants check reads the doubles it would have produced.
        print(f"text: {port} is libultra/gu/{info['decomp']} line for line, "
              f"{changed} of them changed -- the constants and the dropped "
              f"aliases, and nothing else")
    return 0


def build(tmpdir):
    exe = os.path.join(tmpdir, "trig_oracle")
    cmd = ["cc", "-O2", "-std=gnu99", "-Wall", "-fno-strict-aliasing",
           "-ffp-contract=off",
           "-Wno-missing-braces", "-o", exe,
           os.path.join(HERE, "trig_oracle.c"),
           os.path.join(GEN, "gusinf.c"), os.path.join(GEN, "gucosf.c"),
           "-I", os.path.join(SRC, "decomp"), "-I", os.path.join(DECOMP, "src"),
           "-idirafter", os.path.join(DECOMP, "include"),
           "-D_LANGUAGE_C", "-DF3DEX_GBI_2", "-DREGION_US", "-lm"]
    subprocess.run(cmd, check=True)
    return exe


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--rom", default=ROM_DEFAULT)
    ap.add_argument("--exhaustive", action="store_true",
                    help="every one of the 2^32 float arguments")
    ap.add_argument("--stride", type=int, default=1021)
    args = ap.parse_args()

    if not os.path.isdir(DECOMP):
        print(f"no decomp checkout at {DECOMP} -- skipping")
        return 0
    if not os.path.exists(args.rom):
        print(f"no baserom at {args.rom} -- skipping")
        return 0
    with open(args.rom, "rb") as f:
        rom = f.read()

    global GEN
    stride = 1 if args.exhaustive else args.stride
    with tempfile.TemporaryDirectory() as tmp:
        GEN = tmp
        ssb_trigexport.write_all(DECOMP, tmp)
        if check_constants(rom) != 0 or check_text(rom) != 0:
            return 1
        exe = build(tmp)
        out = subprocess.run([exe, str(stride)], check=True,
                             stdout=subprocess.PIPE).stdout.decode()
    if out.startswith("FAIL"):
        print(out.strip())
        return 1
    got = {}
    for line in out.splitlines():
        k, _, v = line.partition(" ")
        got[k] = v

    qnan = int(got["qnan"], 16)
    want_qnan = struct.unpack_from(">I", rom, QNAN_ROM)[0]
    if qnan != want_qnan:
        print(f"FAIL qnan: the port returns {qnan:#010x}, the ROM's "
              f"libm_vals has {want_qnan:#010x}")
        return 1
    print(f"qnan: __libm_qnan_f is {qnan:#010x}, the ROM's word at "
          f"{QNAN_ROM:#08x}")

    worst = int(got["worst"].split()[0])
    if worst > 1:
        print(f"FAIL run: {worst} ulp from the C library at "
              f"{got['worst'].split()[-1]} -- libultra is within 1")
        return 1
    tested, differ = int(got["compared"]), int(got["differ"])
    h = [int(x) for x in got["hist"].split()]
    print(f"run: {tested} arguments compared (stride {stride}), "
          f"{differ} differ from the C library's, worst {worst} ulp; "
          f"{h[0]} exact, {h[1]} off by one")
    print(f"run: {got['gaveup']} arguments past 2^28 where libultra returns "
          f"zero, {got['nans']} NaNs where it returns its own")
    print("trig_check: the port's __sinf and __cosf are libultra's")
    return 0


if __name__ == "__main__":
    sys.exit(main())

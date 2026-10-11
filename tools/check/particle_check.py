#!/usr/bin/env python3
"""ssb64-dc: the nine particle banks, against the decomp's own copy of them.

tools/export/ssb_particleexport.py cuts eighteen byte ranges out of the baserom and
swaps every multi-byte field in them, so the SH-4 can read the game's own
structs without a shim. That is a transformation nobody would notice going
wrong: a bank whose offsets are byteswapped one word too far still loads,
still pointerizes, and produces a particle at an address in the middle of a
texel. Nothing at runtime would say so.

The decomp has the answer, twice over.

  * `ssb-decomp-re/src/particles/<bank>_{scb,txb}.c` is the whole bank
    written out as C -- typed structs, `offsetof` for every internal
    pointer, and `#include`d pixel blobs. Compiled by a little-endian
    compiler it *is* the file the port wants, byte for byte. So this check
    compiles all eighteen and memcmps them against what the exporter wrote.
    A wrong swap, a missed field, a data[] length counted wrong: all of it
    is one byte somewhere, and one byte fails.
  * `ssb-decomp-re/tools/extractParticleTextures.py` is what cuts those
    pixel blobs out of the ROM in the first place, so the texture halves
    are not self-referential: our bytes are checked against the decomp's
    reader of the same ROM. It is imported and driven here rather than
    run, so that only its ROM reader and its `.inc.c` writer are used --
    its PNG previews want Pillow, which the build image does not carry,
    and a decoded preview is not part of this argument anyway.

Two smaller things it also checks, because both are tables typed out by
hand and neither has a runtime symptom:

  * the exporter's eighteen ROM ranges are the eighteen `particles/*`
    segments of smashbrothers.us.yaml, in the yaml's own order, each
    ending where the next begins;
  * the six bank symbols it emits into particlebanks.ld are the six of
    ssb-decomp-re/symbols/linker_constants.txt, each pointing at the
    segment that file points it at -- which is the one place the mapping
    from `ITManager` to the `itcommon` bank is written down;
  * every byte past the end of the compiled struct is zero. Fourteen of
    the eighteen segments run a few bytes past it -- the ROM pads them --
    so "the sizes differ" can only ever mean padding.

Usage: python3 tools/check/particle_check.py [--rom <rom.z64>] [--bank <name>]
"""
import argparse
import os
import re
import subprocess
import sys
import tempfile

sys.path[:0] = [os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), d)
                for d in ("lib", "export", "check")]
import ssb_particleexport as PE      # noqa: E402

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
DECOMP = os.environ.get("SSB_DECOMP_DIR", "/opt/ssb-decomp-re")
if not os.path.isdir(DECOMP):
    DECOMP = os.path.join(REPO_ROOT, "ssb-decomp-re")

# The last particle segment's end: the first audio segment after it.
YAML_TAIL = 0xB277B0


def yaml_ranges():
    """The eighteen particles/* segments of smashbrothers.us.yaml, in order."""
    path = os.path.join(DECOMP, "smashbrothers.us.yaml")
    segs = []
    with open(path) as f:
        for line in f:
            m = re.match(r"\s*-\s*\[(0x[0-9A-Fa-f]+),\s*\.rodata,\s*particles/(\w+)\]", line)
            if m:
                segs.append((int(m.group(1), 16), m.group(2)))
    out = {}
    for i, (start, name) in enumerate(segs):
        end = segs[i + 1][0] if i + 1 < len(segs) else YAML_TAIL
        out[name] = (start, end)
    return out


def linker_constants():
    """{"lEFCommonParticleScriptBankLo": "efcommon_scb_ROM_START", ...} out
    of the decomp's symbols/linker_constants.txt."""
    path = os.path.join(DECOMP, "symbols", "linker_constants.txt")
    out = {}
    with open(path) as f:
        for line in f:
            m = re.match(r"\s*(l\w*Particle\w+Bank(?:Lo|Hi))\s*=\s*(\w+);", line)
            if m:
                out[m.group(1)] = m.group(2)
    return out


def bank_symbol(src):
    """The one `const <type> d<Name>{Scb,Txb> = {` in a bank source file."""
    with open(src) as f:
        m = re.search(r"^const\s+\w+\s+(d\w+)\s*=\s*\{", f.read(), re.M)
    if m is None:
        raise ValueError(f"{src}: no bank symbol")
    return m.group(1)


sys.path.insert(0, os.path.join(DECOMP, "tools"))
import extractParticleTextures as EPT     # noqa: E402


def extract_inc(bank, inc_dir, rom):
    """The .inc.c pixel blobs <bank>_txb.c #includes, cut from the ROM by
    the decomp's own parse_textures and written by its own write_inc.
    Everything lands under inc_dir, which is ours -- the decomp checkout
    is never written to."""
    lo, hi = EPT.BANKS_BY_VERSION["us"][bank]
    out = os.path.join(inc_dir, "particles", bank)

    for ti, t in enumerate(EPT.parse_textures(rom[lo:hi])):
        if t["count"] == 0:
            continue
        fs = EPT.fmt_short(t["fmt"], t["siz"])
        for fi, img in enumerate(t["images"]):
            EPT.write_inc(os.path.join(out, f"tex_{ti}_img_{fi}.{fs}.inc.c"), img)
        for pi, pal in enumerate(t["palettes"]):
            EPT.write_inc(os.path.join(out, f"tex_{ti}_pal_{pi}.inc.c"), pal)


def compile_bank(bank, inc_dir, workdir):
    """Compile <bank>_scb.c and <bank>_txb.c for the host and dump both
    symbols. Returns {"scb": bytes, "txb": bytes}."""
    srcs = {k: os.path.join(DECOMP, "src", "particles", f"{bank}_{k}.c")
            for k in ("scb", "txb")}
    syms = {k: bank_symbol(v) for k, v in srcs.items()}
    out = {}
    for kind in ("scb", "txb"):
        c = os.path.join(workdir, f"ora_{bank}_{kind}.c")
        with open(c, "w") as f:
            f.write("#include <stdio.h>\n#include <ssb_types.h>\n")
            f.write(f'#include <particles/{bank}_{kind}.c>\n')
            f.write("int main(void) { fwrite(&%s, 1, sizeof(%s), stdout); return 0; }\n"
                    % (syms[kind], syms[kind]))
        exe = os.path.join(workdir, f"ora_{bank}_{kind}")
        subprocess.run(
            ["gcc", "-std=gnu99", "-w", "-D_LANGUAGE_C", "-DF3DEX_GBI_2", "-DREGION_US",
             "-I", os.path.join(REPO_ROOT, "src", "dc", "decomp"),
             "-I", os.path.join(DECOMP, "src"),
             "-idirafter", os.path.join(DECOMP, "include"),
             "-I", inc_dir, "-o", exe, c],
            check=True)
        out[kind] = subprocess.run([exe], check=True, stdout=subprocess.PIPE).stdout
    return out


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--rom", default=PE.ROM_DEFAULT)
    ap.add_argument("--bank", choices=sorted(PE.BANKS), action="append")
    args = ap.parse_args()

    if not os.path.isdir(DECOMP):
        print(f"no decomp checkout at {DECOMP} -- skipping")
        return 0

    banks = args.bank or sorted(PE.BANKS)
    with open(args.rom, "rb") as f:
        rom = f.read()

    # 1. The exporter's ROM ranges are the yaml's.
    yr = yaml_ranges()
    for bank in sorted(PE.BANKS):
        for kind in ("scb", "txb"):
            want = yr.get(f"{bank}_{kind}")
            got = PE.BANKS[bank][kind]
            if want != got:
                print(f"FAIL {bank}.{kind}: exporter has "
                      f"{got[0]:#x}..{got[1]:#x}, the yaml has {want}")
                return 1
    print("ranges: 18 particles/* segments match smashbrothers.us.yaml")

    # 2. The emitted bank addresses are the decomp's linker constants.
    lc = linker_constants()
    want = {}
    for bank, name in PE.SYMBOLS.items():
        for kind, what in (("scb", "Script"), ("txb", "Texture")):
            want[f"l{name}Particle{what}BankLo"] = f"{bank}_{kind}_ROM_START"
            want[f"l{name}Particle{what}BankHi"] = f"{bank}_{kind}_ROM_END"
    if want != lc:
        for k in sorted(set(want) | set(lc)):
            if want.get(k) != lc.get(k):
                print(f"FAIL {k}: the exporter says {want.get(k)}, "
                      f"linker_constants.txt says {lc.get(k)}")
        return 1
    emitted = PE.emit_ld("")
    for sym, seg in sorted(lc.items()):
        parts = seg.split("_")          # <bank>_<kind>_ROM_<START|END>
        lo, hi = PE.BANKS["_".join(parts[:-3])][parts[-3]]
        addr = lo if parts[-1] == "START" else hi
        if f"{sym} = {addr:#010x};" not in emitted:
            print(f"FAIL {sym}: not emitted as {addr:#010x}")
            return 1
    print(f"symbols: {len(lc)} bank addresses match symbols/linker_constants.txt")

    failed = 0
    with tempfile.TemporaryDirectory() as work:
        inc = os.path.join(work, "inc")
        for bank in banks:
            extract_inc(bank, inc, rom)
            oracle = compile_bank(bank, inc, work)
            for kind in ("scb", "txb"):
                ours = PE.export(rom, bank, kind)
                want = oracle[kind]
                if len(want) > len(ours):
                    print(f"FAIL {bank}.{kind}: the compiled bank is "
                          f"{len(want)} bytes, the ROM range only {len(ours)}")
                    failed += 1
                    continue
                pad = ours[len(want):]
                if ours[:len(want)] != want:
                    off = next(i for i in range(len(want)) if ours[i] != want[i])
                    print(f"FAIL {bank}.{kind}: byte {off:#x} is "
                          f"{ours[off]:#04x}, the decomp's is {want[off]:#04x}")
                    failed += 1
                elif any(pad):
                    print(f"FAIL {bank}.{kind}: {len(pad)} bytes of segment "
                          f"padding and not all zero")
                    failed += 1
                else:
                    print(f"ok   {bank}.{kind}: {len(want)} bytes"
                          + (f" + {len(pad)} zero pad" if pad else ""))

    if failed:
        print(f"particle_check: {failed} bank(s) differ from the decomp's own")
        return 1
    print(f"particle_check: {len(banks) * 2} banks byte-identical to the decomp's")
    return 0


if __name__ == "__main__":
    sys.exit(main())

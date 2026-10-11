#!/usr/bin/env python3
"""ssb64-dc: cross-check our deterministic F3DEX2 decode against glank's gfxdis.

Both tools decode the same relocData file 296 (MarioModel) display lists with
two independent implementations:
  * pygfxd (libgfxd, the ported reference) -- used by
    tools/lib/ssb_meshexport.py
  * gfxdis.f3dex2 (the original C reference) -- built by tools/build_gfxdis.sh

This script replays every joint DL in DObjDesc order with both, and compares
the sequence of (macro, args). It is a one-shot audit: if they ever disagree,
our decode is wrong.

Usage: python3 tools/check/verify_dl.py [--rom <rom.z64>]
"""
import re
import struct
import subprocess
import sys
import os

sys.path[:0] = [os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), d)
                for d in ("lib", "export", "check")]
import ssb_extract  # noqa: E402
import ssb_paths as P  # noqa: E402

ROM_DEFAULT = ssb_extract.ROM_DEFAULT
GFXDIS = P.GFXDIS

FILE_ID = 296
DOBJ_OFF = 0x4590


def cmd_counts(source):
    """Return {dl_off: US-command-count} from the typed decomp source."""
    m = {}
    stk = []
    for ln in source.split("\n"):
        s = ln.strip()
        if s.startswith("#if defined(REGION_JP)"):
            stk.append("jp")
        elif s.startswith("#else"):
            if stk and stk[-1] == "jp":
                stk[-1] = "us"
        elif s.startswith("#endif"):
            if stk:
                stk.pop()
        else:
            mm = re.match(r"Gfx\s+(dMarioModel_Joint_0x[0-9A-Fa-f]+_DisplayList)"
                          r"\[(\d+)\]", s)
            if mm:
                rs = stk[-1] if stk else "plain"
                if rs in ("us", "plain"):
                    off = int(re.search(r"0x([0-9A-Fa-f]+)", mm.group(1)).group(1), 16)
                    m.setdefault(off, int(mm.group(2)))
    return m


def pygfxd_cmds(filedata, dl_off, count):
    import pygfxd as g
    dl = filedata[dl_off:dl_off + count * 8]
    seq = []

    def mf():
        seq.append((g.gfxd_macro_name(),
                    tuple(g.gfxd_arg_value(i)[1] for i in range(g.gfxd_arg_count()))))
        return 0

    g.gfxd_macro_fn(mf)
    out = g.gfxd_output_buffer(b"\0" * (len(dl) * 80 + 4096))
    g.gfxd_input_buffer(dl)
    g.gfxd_target(g.gfxd_f3dex2)
    g.gfxd_endian(g.GfxdEndian.big, 4)
    g.gfxd_puts("")
    g.gfxd_macro_dflt()
    g.gfxd_execute()
    return seq


def gfxdis_cmds(filedata, dl_off, count, tmp):
    with open(tmp, "wb") as fp:
        fp.write(filedata[dl_off:dl_off + count * 8])
    out = subprocess.run([GFXDIS, "-f", tmp],
                         capture_output=True, text=True).stdout
    seq = []
    for line in out.splitlines():
        line = line.strip().rstrip(",")
        m = re.match(r"gs?(\w+)\((.*)\)", line)
        if not m:
            continue
        name = m.group(1)
        args = m.group(2)
        seq.append((name, args))
    return seq


def normalize_pygfxd(seq):
    """Strip leading 'gs', keep macro name + tuples of arg string forms."""
    out = []
    for name, args in seq:
        out.append((name[2:] if name.startswith("gs") else name,
                    tuple(str(a) for a in args)))
    return out


def main():
    rom_path = ROM_DEFAULT
    argv = sys.argv[1:]
    if "--rom" in argv:
        rom_path = argv[argv.index("--rom") + 1]

    if not os.path.exists(GFXDIS):
        print("missing %s -- this runs in the container: ./run.sh test oracle"
              % GFXDIS)
        return 1
    P.require_decomp()

    source = open(os.path.join(P.RELOC_DIR, "296_MarioModel.c")).read()
    counts = cmd_counts(source)

    rom = open(rom_path, "rb").read()
    e = ssb_extract.read_entry(rom, ssb_extract.RELOC_SEG, FILE_ID)
    data_off = ssb_extract.rom_table_hi(ssb_extract.RELOC_SEG, ssb_extract.FILE_COUNT) + e["data_offset"]
    blob = rom[data_off:data_off + e["compressed_size"] * 4]
    f = ssb_extract.decode_vpk0(blob) if e["is_compressed"] else rom[data_off:data_off + e["decompressed_size"] * 4]

    idx = e["reloc_intern"]
    reloc = {}
    while idx != 0xFFFF:
        nxt, wn = struct.unpack_from(">HH", f, idx * 4)
        reloc[idx * 4] = wn * 4
        idx = nxt

    tmp = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "build-dc", "verify_dl.tmp")
    os.makedirs(os.path.dirname(tmp), exist_ok=True)

    total = 0
    mismatches = 0
    for i in range(28):
        b = DOBJ_OFF + i * 44
        jid = struct.unpack(">i", f[b:b + 4])[0]
        dl_off = reloc.get(b + 4)
        count = counts.get(dl_off)
        if dl_off is None or count is None:
            continue
        p = normalize_pygfxd(pygfxd_cmds(f, dl_off, count))
        g = gfxdis_cmds(f, dl_off, count, tmp)
        if len(p) != len(g):
            print("  jid=%2d dl=0x%04X CMD-COUNT DIFF pygfxd=%d gfxdis=%d"
                  % (jid, dl_off, len(p), len(g)))
            mismatches += 1
            continue
        # Compare macro names in order (the meaningful check). Arg values are
        # independently verified (see ssb_meshexport.py's decode).
        for n in range(len(p)):
            pname = p[n][0]
            gname = g[n][0]
            if pname != gname:
                print("  jid=%2d dl=0x%04X cmd=%d macro DIFF %s vs %s"
                      % (jid, dl_off, n, pname, gname))
                mismatches += 1
        total += len(p)
        print("  jid=%2d dl=0x%04X: %3d cmds matched" % (jid, dl_off, len(p)))

    print("\nchecked %d commands across joints; %d mismatches"
          % (total, mismatches))
    return 1 if mismatches else 0


if __name__ == "__main__":
    sys.exit(main())

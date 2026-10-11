#!/usr/bin/env python3
"""rom_leak_check.py -- no ROM bytes in the repository, by content.

Path names are not proof that nothing ROM-derived was committed; bytes
are. This reads every blob (the current tree, or with --history every
blob ever committed) and looks for runs of the ROM's own bytes in two
shapes:

  * raw: a blob with a NUL in it is binary; every 32-byte window at a
    32-byte stride is looked up in the ROM.
  * as text: runs of 16 or more comma-separated integer literals (a C or
    Python table), encoded big-endian at the narrowest width that holds
    the run's largest value (u8, u16 or u32), and hex strings of 64 or
    more digits. Each run is probed in 16-byte windows.

Low-entropy windows (fewer than 8 distinct byte values: zero fill,
ramps of small ints, repeated words) are skipped, since they occur in
any ROM by chance. A blob is reported when 32 or more of its bytes match.
A hit is a lead, not a verdict: a constant table the decomp also spells
out (a sine table, a CRC table) is source, and it matches.

Without a ROM (--no-rom, the CI form) it can only check the raw shape's
precondition: no tracked binary file at all outside --allow. Nothing
derived from the ROM is shipped to make the full check possible there.

  python3 tools/check/rom_leak_check.py [--rom baserom.z64] [--history]
  python3 tools/check/rom_leak_check.py --no-rom
"""
import argparse
import fnmatch
import glob
import os
import re
import subprocess
import sys

N64_BE_MAGIC = b"\x80\x37\x12\x40"
WINDOW = 16
REPORT_BYTES = 32
MIN_DISTINCT = 8
MIN_LITERALS = 16

LIT = r"(?:0[xX][0-9A-Fa-f]+|\d+)"
RUN_RE = re.compile(r"%s(?:\s*,\s*%s){%d,}" % (LIT, LIT, MIN_LITERALS - 1))
HEX_RE = re.compile(r"(?<![0-9A-Fa-f])[0-9A-Fa-f]{64,}(?![0-9A-Fa-f])")


def load_rom(path):
    rom = open(path, "rb").read()
    if rom[:4] == N64_BE_MAGIC:
        return rom
    if rom[:4] == b"\x37\x80\x40\x12":   # .v64, byte-swapped
        return b"".join(rom[i + 1:i + 2] + rom[i:i + 1]
                        for i in range(0, len(rom), 2))
    if rom[:4] == b"\x40\x12\x37\x80":   # .n64, little-endian words
        return b"".join(rom[i:i + 4][::-1] for i in range(0, len(rom), 4))
    raise SystemExit("%s: not an N64 ROM (magic %s)" % (path, rom[:4].hex()))


def git(*args, data=None):
    return subprocess.run(("git",) + args, input=data, capture_output=True,
                          check=True).stdout


def blobs(history):
    """[(sha, path)], one entry per distinct blob."""
    seen = {}
    if history:
        out = git("rev-list", "--objects", "--all").decode()
        for line in out.splitlines():
            sha, _, path = line.partition(" ")
            if path and sha not in seen:
                seen[sha] = path
        types = git("cat-file", "--batch-check=%(objectname) %(objecttype)",
                    data="\n".join(seen).encode()).decode()
        keep = {l.split()[0] for l in types.splitlines()
                if l.split()[1:] == ["blob"]}
        return [(s, p) for s, p in seen.items() if s in keep]
    out = git("ls-files", "-s").decode()
    for line in out.splitlines():
        meta, _, path = line.partition("\t")
        mode, sha = meta.split()[:2]
        if mode != "160000":             # a submodule is not a blob
            seen.setdefault(sha, path)
    return list(seen.items())


def read_blobs(shas):
    """Yield (sha, bytes) through one cat-file --batch process."""
    proc = subprocess.Popen(["git", "cat-file", "--batch"],
                            stdin=subprocess.PIPE, stdout=subprocess.PIPE)
    for sha in shas:
        proc.stdin.write(sha.encode() + b"\n")
        proc.stdin.flush()
        header = proc.stdout.readline().split()
        size = int(header[2])
        data = proc.stdout.read(size)
        proc.stdout.read(1)
        yield sha, data
    proc.stdin.close()
    proc.wait()


def literal_runs(text):
    """Integer tables and long hex strings, as big-endian byte strings."""
    for m in RUN_RE.finditer(text):
        vals = [int(v, 0) if not v.startswith("0") or v[:2].lower() == "0x"
                or v == "0" else int(v, 10)
                for v in re.findall(LIT, m.group(0))]
        top = max(vals)
        width = 1 if top <= 0xFF else 2 if top <= 0xFFFF else 4
        if top > 0xFFFFFFFF:
            continue
        yield m.start(), b"".join(v.to_bytes(width, "big") for v in vals)
    for m in HEX_RE.finditer(text):
        s = m.group(0)
        yield m.start(), bytes.fromhex(s[:len(s) & ~1])


def matched_bytes(rom, data, stride):
    hits = 0
    first = None
    for i in range(0, len(data) - WINDOW + 1, stride):
        w = data[i:i + WINDOW]
        if len(set(w)) < MIN_DISTINCT:
            continue
        at = rom.find(w)
        if at >= 0:
            hits += WINDOW
            if first is None:
                first = at
    return hits, first


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--rom", help="default: baserom.z64")
    ap.add_argument("--history", action="store_true",
                    help="every blob ever committed, not just the tree")
    ap.add_argument("--no-rom", action="store_true",
                    help="only fail on tracked binary files")
    ap.add_argument("--allow", action="append", default=[],
                    help="a path glob a binary file may match")
    args = ap.parse_args()

    rom = None
    if not args.no_rom:
        path = args.rom
        if not path:
            found = glob.glob("baserom.z64")
            if len(found) != 1:
                raise SystemExit("no ROM: pass --rom, or --no-rom")
            path = found[0]
        rom = load_rom(path)

    todo = blobs(args.history)
    paths = dict(todo)
    seen_runs = set()
    bad = 0
    for sha, data in read_blobs(paths):
        path = paths[sha]
        if b"\0" in data[:8192]:
            if any(fnmatch.fnmatch(path, a) for a in args.allow):
                continue
            if rom is None:
                print("BINARY  %s  (%d bytes)" % (path, len(data)))
                bad += 1
                continue
            hits, at = matched_bytes(rom, data, 32)
            if hits >= REPORT_BYTES:
                print("RAW     %s  %d bytes match, first at ROM 0x%x"
                      % (path, hits, at))
                bad += 1
            continue
        if rom is None:
            continue
        text = data.decode("latin-1")
        total = 0
        where = []
        for off, run in literal_runs(text):
            if run in seen_runs:
                continue
            seen_runs.add(run)
            hits, at = matched_bytes(rom, run, WINDOW)
            if hits:
                total += hits
                line = text.count("\n", 0, off) + 1
                where.append("line %d -> ROM 0x%x (%d B)" % (line, at, hits))
        if total >= REPORT_BYTES:
            print("TEXT    %s  %d bytes match: %s"
                  % (path, total, "; ".join(where[:4])
                     + (" ..." if len(where) > 4 else "")))
            bad += 1

    print("%d blob(s) checked, %d flagged" % (len(todo), bad))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())

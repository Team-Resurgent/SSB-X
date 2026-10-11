#!/usr/bin/env python3
"""disc_order_check.py -- does the game read the disc forwards?

tools/export/disc_layout.py decides where each file SITS on the GD-ROM, and
tools/check/disc_check.py proves the finished image took that layout. Neither
knows what order the game actually ASKS for them in, and that is the half
that decides whether the drive streams or steps: a file opened behind the
one before it is a backward seek, and on a real drive a seek is a hundred
times the cost of the read it serves. A host-served image cannot show
this at all: it has no head to move.

So this reads a -DDB_IO_TRACE serial log (src/dc/db.h), which prints one
line per file at open, and scores the order against the manifest:

  * every BACKWARD open, with how far back it reaches;
  * every file opened more than once, which is a re-read;
  * per scene window, the longest ascending run.

`--check` fails when a window has more backward opens than
tools/check/disc_order_known.tsv allows, so a load that starts stepping is a
failed run rather than a slower one nobody measured. The baseline is a
file rather than a constant because some backward opens are real and
permanent: a scene that reuses the menu's sprite bank has to reach back to
where the menu's bank sits, and it is cheaper to seek than to carry a
second copy of 353 KB.

Producing a log:

  EXTRA_CFLAGS=-DDB_IO_TRACE ./run.sh disc, boot it, and keep the serial
  log (dcload-serial or a coder's cable) as build/scratch/io.log; then
  python3 tools/check/disc_order_check.py build/scratch/io.log
"""
import argparse
import os
import re
import sys

sys.path[:0] = [os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), d)
                for d in ("lib", "export", "check")]
import disc_layout

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
KNOWN = os.path.join(ROOT, "tools", "check", "disc_order_known.tsv")

OPEN = re.compile(r"^io: open (\S+) \((\d+) bytes\) at (\d+) ms")
WINDOW = re.compile(r"^io: (.+?) -- (\d+) files, (\d+) bytes")


def parse(path):
    """[(window, [(name, bytes, ms), ...]), ...] in the order they ran.

    A window closes on its `io: <what> --` line, which src/dc/scmanager.c
    prints once per scene; opens before the first one belong to the boot.
    """
    windows, cur = [], []

    for line in open(path, encoding="utf-8", errors="replace"):
        m = OPEN.match(line)
        if m:
            cur.append((m.group(1), int(m.group(2)), int(m.group(3))))
            continue
        m = WINDOW.match(line)
        if m:
            windows.append((m.group(1), cur))
            cur = []
    if cur:
        windows.append(("(unclosed)", cur))
    return [w for w in windows if w[1]]


def read_known():
    """window -> allowed backward opens."""
    out = {}
    if not os.path.exists(KNOWN):
        return out
    for line in open(KNOWN):
        line = line.split("#")[0].strip()
        if not line:
            continue
        what, n = line.rsplit("\t", 1)
        out[what.strip()] = int(n)
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("log", help="a -DDB_IO_TRACE serial log")
    ap.add_argument("--check", action="store_true",
                    help="fail when a window exceeds its allowance")
    ap.add_argument("--write", action="store_true",
                    help="record this run's counts as the allowance")
    args = ap.parse_args()

    # the bundle's entries in its place: models.bnd holds them in exactly
    # that order, so a bundled read and a loose one rank the same
    rank = {name: i for i, name in
            enumerate(disc_layout.manifest_order(bundled=False))}
    group = disc_layout.manifest_groups()
    windows = parse(args.log)

    if not windows:
        sys.exit("disc_order_check: no `io: open` lines in %s -- was it "
                 "built with -DDB_IO_TRACE?" % args.log)

    known, counts, failed = read_known(), {}, []
    seen_files, reread = {}, []

    for what, opens in windows:
        back, prev, run, best = [], -1, 0, 0

        for (name, nbytes, _ms) in opens:
            if name in seen_files:
                reread.append((what, name, nbytes))
            seen_files[name] = seen_files.get(name, 0) + 1

            r = rank.get(name)
            if r is None:
                continue        # not in the manifest: disc_layout fails on it
            if r < prev:
                back.append((name, prev - r, group.get(name, "?")))
                best, run = max(best, run), 1
            else:
                run += 1
            prev = r
        best = max(best, run)
        counts[what] = len(back)

        print("disc_order: %-14s %3d files, %3d backward, longest ascending "
              "run %d" % (what, len(opens), len(back), best))
        for (name, dist, grp) in back:
            print("disc_order:     back %4d to %s (%s)" % (dist, name, grp))

        allow = known.get(what)
        if args.check and allow is not None and len(back) > allow:
            failed.append("%s: %d backward opens, %d allowed"
                          % (what, len(back), allow))
        if args.check and allow is None:
            failed.append("%s: no allowance in %s"
                          % (what, os.path.relpath(KNOWN, ROOT)))

    if reread:
        print("disc_order: %d file(s) read again in a later window:" % len(reread))
        for (what, name, nbytes) in reread:
            print("disc_order:     %s re-read %d bytes during %s"
                  % (name, nbytes, what))

    if args.write:
        with open(KNOWN, "w") as fp:
            fp.write("# disc_order_check.py --check: backward opens allowed\n"
                     "# per scene window, recorded from a -DDB_IO_TRACE run.\n"
                     "# A window that reaches back further than this is a\n"
                     "# load that started stepping; fix the order in\n"
                     "# tools/export/disc_layout.py MANIFEST or raise the number\n"
                     "# here with the reason.\n")
            for what in sorted(counts):
                fp.write("%s\t%d\n" % (what, counts[what]))
        print("disc_order: wrote %s" % os.path.relpath(KNOWN, ROOT))

    if failed:
        for line in failed:
            print("disc_order_check: " + line)
        sys.exit("disc_order_check: %d window(s) read the disc backwards "
                 "more than allowed" % len(failed))
    return 0


if __name__ == "__main__":
    sys.exit(main())

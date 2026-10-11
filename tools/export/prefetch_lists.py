#!/usr/bin/env python3
"""prefetch_lists.py -- what each scene of the opening movie opens, for the
loader to read ahead (src/dc/assetroot.h, "Reading ahead").

Usage: tools/export/prefetch_lists.py <serial log> [--out src/dc/scprefetch.h]

The log is a -DDB_IO_TRACE boot from the N64 logo to the title (a plain
disc boot, no DB_BOOT_* knobs). Each scene's window is the run of lines
between two `io: <scene> -- N files` reports, and its list is every
file it opened in that window, in order:
  - opened off the medium (`io: open x (n bytes) at t ms`, or `... in
    models.bnd+off`), or served by the loader (`... read ahead`), since a
    log taken with the loader on must give the same lists;
  - but not an open of the hold list's copies (`... from RAM`): those
    never touch the medium after the first. The first is a medium open
    in the window that made it -- or, in a log taken with the loader
    on, the loader's copy taken over by the hold list, which logs as
    `from RAM` too, and so a name's first open always counts;
  - and not the sound loader's files (disc_layout.py's "audio" group),
    which src/dc/sndres.c reads a sample at a time at every scene start
    and which are no scene's to read ahead;
  - and not a file the first window opens: that is the run-long set
    the scene manager loads before any scene (the effect, weapon and
    item models), which no scene reads again.
Files a scene opens after its load (the Room's props appear through its
twenty seconds) are on its list too: the loader reads them while it
plays.

The Room puts two fighters on the desk at random (src/dc/mvopeningroom.c)
and the movie keeps their packs, so which fighter scenes open their own
pack changes from run to run. So the lists are made run-independent:
the Room's leaves out every playable fighter's pack, stock sprite and
particle bank, and each fighter's own scene names all three, last,
whether the log's run opened them there or not. At run time the scene manager takes
back the packs the Room did load (asset_prefetch_drop).

The scenes are the windows from `startup` to `title`, in the order they
ran; the scene manager queues the rows after the one it enters, so the
first row's list is never read ahead (nothing plays before the N64 logo)
and is written empty. Scene names map to nSCKind values through
sc_scene_name in src/dc/scmanager.c.

Regenerate after a change to what an opening scene loads; a stale list
shows in the same log as `missed` (a file it opens that is not listed)
and `unused` (one listed and not opened) on the scene's `read ahead`
line, and costs time or RAM but never correctness.
"""
import argparse
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import disc_layout  # noqa: E402

OPEN = re.compile(r"io: open (\S+) \((\d+) bytes\) at \d+ ms(.*)$")
REPORT = re.compile(r"io: (\w+) -- \d+ files,")


def scene_kinds():
    src = open(os.path.join(ROOT, "src/dc/scmanager.c"), encoding="utf-8").read()
    body = src[src.index("static const char *sc_scene_name"):]
    body = body[:body.index("\n}\n")]
    return {name: kind for kind, name in
            re.findall(r"case (nSCKind\w+):\s*return \"(\w+)\";", body)}


def windows(log):
    out, opens = [], []
    for line in open(log, encoding="utf-8", errors="replace"):
        line = line.rstrip("\n")
        m = OPEN.match(line)
        if m:
            opens.append((m.group(1), int(m.group(2)), m.group(3).strip()))
            continue
        m = REPORT.match(line)
        if m:
            out.append((m.group(1), opens))
            opens = []
    return out


# the eight scenes that each bring one fighter on, by file stem
FIGHTER_SCENES = {
    "openingmario": "mario", "openingdonkey": "donkey",
    "openingsamus": "samus", "openingfox": "fox", "openinglink": "link",
    "openingyoshi": "yoshi", "openingkirby": "kirby",
    "openingpikachu": "pikachu",
}


def fighter_files(stem):
    bank = disc_layout.FIGHTER_BANKS.get(stem)
    return ([stem + ".pack", stem + ".anm"]
            + ([bank + e for e in (".scb", ".txb", ".txp")] if bank else [])
            + ["ft" + stem + ".spr"])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("log")
    ap.add_argument("--out", default=os.path.join(ROOT, "src/dc/scprefetch.h"))
    ap.add_argument("--first", default="startup")
    ap.add_argument("--last", default="title")
    a = ap.parse_args()

    audio = set(dict(disc_layout.MANIFEST)["audio"])
    kinds = scene_kinds()
    wins = windows(a.log)
    names = [w for w, _ in wins]
    if a.first not in names or a.last not in names:
        sys.exit("prefetch_lists: %s has no %s..%s run of scenes"
                 % (a.log, a.first, a.last))
    i, j = names.index(a.first), names.index(a.last, names.index(a.first))
    # what the first window opens is loaded before any scene and kept
    # for the run (src/dc/scmanager.c scManagerRunScene's preload), so
    # no later scene opens it off the medium again
    resident = {name for name, n, how in wins[i][1]}
    ever = set()
    rows = []
    for k, (scene, opens) in enumerate(wins[i:j + 1]):
        if scene not in kinds:
            sys.exit("prefetch_lists: no nSCKind for scene %r" % scene)
        files, seen, size = [], set(), 0
        for name, n, how in opens:
            first = name not in ever
            ever.add(name)
            if (("from RAM" in how and not first) or name in audio
                    or "/" in name or name in resident):
                continue
            if name not in seen:
                seen.add(name)
                files.append(name)
                size += n
        if k == 0:
            files, size = [], 0
        if scene == "openingroom":
            random = {f for stem in disc_layout.FIGHTERS
                      for f in fighter_files(stem)}
            files = [f for f in files if f not in random]
        if scene in FIGHTER_SCENES:
            own = fighter_files(FIGHTER_SCENES[scene])
            files = [f for f in files if f not in own] + own
        size = sum(n for name, n, how in opens if name in files)
        rows.append((scene, kinds[scene], files, size))

    out = []
    out.append("/* scprefetch.h -- GENERATED by tools/export/prefetch_lists.py "
               "from a\n * -DDB_IO_TRACE boot through the opening movie; "
               "do not edit. What each\n * scene opens, in the order it "
               "runs, for the loader to read ahead\n * (src/dc/assetroot.h, "
               "\"Reading ahead\"; src/dc/scmanager.c queues it). */\n")
    out.append("#ifndef SSB_DC_SCPREFETCH_H\n#define SSB_DC_SCPREFETCH_H\n")
    for scene, kind, files, size in rows:
        if not files:
            continue
        out.append("/* %s: %d files, %d bytes in the log's run */"
                   % (scene, len(files), size))
        out.append("static const char *const sSCPrefetch_%s[] = {" % scene)
        for f in files:
            out.append("    \"%s\"," % f)
        out.append("};\n")
    out.append("typedef struct SCPrefetchScene\n{\n    s32 scene;\n"
               "    const char *const *files;\n    int count;\n"
               "} SCPrefetchScene;\n")
    out.append("static const SCPrefetchScene dSCPrefetchOpening[] = {")
    for scene, kind, files, size in rows:
        if files:
            out.append("    { %s, sSCPrefetch_%s, %d }," % (kind, scene, len(files)))
        else:
            out.append("    { %s, NULL, 0 }," % kind)
    out.append("};\n")
    out.append("#endif /* SSB_DC_SCPREFETCH_H */")
    open(a.out, "w", encoding="utf-8").write("\n".join(out) + "\n")
    total = sum(len(r[2]) for r in rows)
    print("prefetch_lists: %d scenes, %d files, %d bytes -> %s"
          % (len(rows), total, sum(r[3] for r in rows),
             os.path.relpath(a.out, ROOT)))


if __name__ == "__main__":
    main()

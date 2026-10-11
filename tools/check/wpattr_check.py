#!/usr/bin/env python3
"""wpattr_check.py -- hold src/game/ssb64/wpattrs.ld to the decomp.

The fighters' weapon-table symbols (ll<Char><File><Weapon>WeaponAttributes)
are absolute linker symbols: each one's ADDRESS is the table's byte
offset in the fighter's relocData file, and wpManagerMakeWeapon adds it
to the file pointer (src/dc/wpattrs.h). tools/export/ssb_wpattrexport.py writes
those offsets into wpattrs.ld off the generated reloc header, and this
refuses to let the three drift apart:

  * regenerating the fragment now reproduces the one in the tree, so it
    really is generated and nobody has edited it;
  * every symbol in it is declared `extern int` by reloc_data.us.h at
    that same offset, and every weapon-table symbol the exporter's table
    names is in the fragment;
  * no `int ll*WeaponAttributes;` definition survives in src/dc/*.c --
    one would collide with the fragment's, or (as a tentative definition
    merged by the linker) silently shadow it with a .bss address, which
    is the zeroed-stand-in bug the fragment replaced.

`./run.sh test wpattr` runs it alone; `./run.sh test` runs it with the rest.
"""
import os
import re
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
TOOL = os.path.join(ROOT, "tools", "export", "ssb_wpattrexport.py")
LD = os.path.join(ROOT, "src", "game", "ssb64", "wpattrs.ld")
HOST_LD = os.path.join(ROOT, "src", "game", "ssb64", "hosttest_wpattrs.ld")
RELOC_H = os.path.join(ROOT, "src", "dc", "decomp", "reloc_data.us.h")
DC = os.path.join(ROOT, "src", "dc")

# the weapon tables, and the raw tables an item reads
# out of the same file (ssb_wpattrexport.py RAW_TABLES)
DECL = re.compile(r"^extern int (ll\w+); // (0x[0-9a-f]+)$", re.M)
ASSIGN = re.compile(r"^_?(ll\w+) = (0x[0-9a-f]+);", re.M)
DEFINE = re.compile(r"^\s*int\s+(ll\w+WeaponAttributes)\s*;", re.M)


def rel(p):
    return os.path.relpath(p, ROOT)


def main():
    if not os.path.exists(LD):
        sys.exit("wpattr_check: %s is not in the tree -- build ssb64 first"
                 % rel(LD))
    with tempfile.TemporaryDirectory() as d:
        tmp = os.path.join(d, "wpattrs.ld")
        subprocess.run([sys.executable, TOOL, "--emit-ld", tmp,
                        "--symbol-prefix", "_"], check=True,
                       stdout=subprocess.DEVNULL)
        if open(tmp).read() != open(LD).read():
            sys.exit("wpattr_check: %s is stale or edited -- rebuild ssb64 "
                     "(make regenerates it from tools/export/ssb_wpattrexport.py)"
                     % rel(LD))

    truth = {n: int(v, 16) for (n, v) in DECL.findall(open(RELOC_H).read())}
    in_ld = {n: int(v, 16) for (n, v) in ASSIGN.findall(open(LD).read())}
    if not in_ld:
        sys.exit("wpattr_check: %s defines no ll*WeaponAttributes" % rel(LD))

    bad = []
    for name, off in sorted(in_ld.items()):
        if name not in truth:
            bad.append("%s: in the fragment, not declared in reloc_data.us.h"
                       % name)
        elif truth[name] != off:
            bad.append("%s: the fragment says 0x%x, the decomp says 0x%x"
                       % (name, off, truth[name]))
    if os.path.exists(HOST_LD):
        # the host's is spread for its 72-byte WPAttributes (the Makefile's
        # --record-size), so it is checked against its own regeneration
        with tempfile.TemporaryDirectory() as d:
            tmp = os.path.join(d, "hosttest_wpattrs.ld")
            subprocess.run([sys.executable, TOOL, "--emit-ld", tmp,
                            "--record-size", "72"], check=True,
                           stdout=subprocess.DEVNULL)
            if open(tmp).read() != open(HOST_LD).read():
                bad.append("%s is stale or edited" % rel(HOST_LD))

    # the tripwire: a surviving stand-in
    for n in sorted(os.listdir(DC)):
        if not n.endswith(".c"):
            continue
        for m in DEFINE.finditer(open(os.path.join(DC, n)).read()):
            bad.append("src/dc/%s defines `int %s;` -- it collides with the "
                       "fragment's absolute symbol" % (n, m.group(1)))
    if bad:
        sys.exit("wpattr_check: %d problem(s):\n  %s"
                 % (len(bad), "\n  ".join(bad)))

    # the other direction: every weapon-table symbol any compiled source
    # names is in the fragment (an unlisted one falls back to an implicit
    # undefined reference, which the link catches -- but say it here)
    used = set()
    ident = re.compile(r"\b(ll\w+WeaponAttributes)\b")
    for n in sorted(os.listdir(DC)):
        if n.endswith(".c"):
            for line in open(os.path.join(DC, n)):
                t = line.strip()
                if t.startswith(("*", "/*", "//")):
                    continue
                used |= set(ident.findall(line))
    # the item file's and the stage maps' tables are the item pack's
    # (src/dc/itemoffsets.h, tools/export/ssb_itemexport.py), not this
    # fragment's
    missing = sorted(u for u in used if u not in in_ld and u in truth and
                     not u.startswith(("llITCommonData", "llGR")))
    if missing:
        sys.exit("wpattr_check: %d symbol(s) src/dc/ uses are not in %s:\n  %s"
                 % (len(missing), rel(LD), "\n  ".join(missing)))
    print("wpattr_check: %s carries %d weapon-table offsets, all the "
          "decomp's own, and no stand-in survives" % (rel(LD), len(in_ld)))


if __name__ == "__main__":
    main()

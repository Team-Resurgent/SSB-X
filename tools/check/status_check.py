#!/usr/bin/env python3
"""status_check.py -- every fighter status row is the game's.

ftMainSetStatus installs a status's four procs, its motion and its attack
flags from one FTStatusDesc row. The port writes those tables in
src/dc/ftcommon.c with designated initializers, so a row nobody wrote is
not a compile error: it is all zero, and a fighter set to it had no procs
and froze where it stood (Kirby's and Jigglypuff's midair jumps, Captain
Falcon's third jab, DK's heavy-item throws, being carried or thrown by DK
or Fox). A row copied with one column wrong is quieter still.

This compares, row by row and column by column, each port table with the
decomp's (ft/ftcommon/ftcommonstatus.h, ft/ftchar/*/ft*status.h), and the
port's fighter-kind -> table map with ft/ftmain.c's
dFTMainSpecialStatusDescs. Enum names are evaluated from the decomp's
headers, so `[nFTKirbyStatusJumpAerialF1 - nFTCommonStatusSpecialStart]`
and the decomp's "Status 223" comment land on the same row.

Every difference must be in KNOWN below with the work item that
closes it, and every KNOWN entry must still differ, so the list cannot
outlive the fix. `./run.sh test status` runs it with --check; `./run.sh test`
runs it with the rest.
"""
import glob
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
DECOMP = os.environ.get("SSB_DECOMP_DIR", "/opt/ssb-decomp-re")
if not os.path.isdir(DECOMP):
    DECOMP = os.path.join(ROOT, "ssb-decomp-re")
PORT = os.path.join(ROOT, "src", "dc", "ftcommon.c")

SPECIAL = "dFTMainSpecialStatusDescs"

# (table, status id) -> why it still differs. A table name alone is a whole
# table the port does not carry.
#
# EMPTY: every status table the game has, the port has,
# and every row of each matches. The last two entries here were
# dFTBossSpecialStatusDescs and dFTMainSpecialStatusDescs[nFTKindBoss],
# both "not VS: Master Hand is 1P content", and both are now filled.
KNOWN = {}


def strip_comments(text):
    text = re.sub(r"/\*.*?\*/", " ", text, flags=re.S)
    return re.sub(r"//[^\n]*", " ", text)


def enum_values():
    """Every enumerator in the decomp's headers that evaluates. An enum can
    start from another header's enumerator, so the headers are walked
    until a pass adds nothing."""
    bodies = []
    paths = glob.glob(os.path.join(DECOMP, "src", "**", "*.h"), recursive=True)
    paths += glob.glob(os.path.join(DECOMP, "include", "**", "*.h"), recursive=True)
    for path in sorted(paths):
        text = strip_comments(open(path, errors="replace").read())
        for m in re.finditer(r"\benum\b\s*\w*\s*\{([^{}]*)\}", text):
            bodies.append(re.sub(r"#[^\n]*", " ", m.group(1)))
    names, count = {}, -1
    while len(names) != count:
        count = len(names)
        for body in bodies:
            cur = -1
            for item in body.split(","):
                item = item.strip()
                if not item:
                    continue
                if "=" in item:
                    name, expr = item.split("=", 1)
                    try:
                        cur = eval(expr.strip(), {"__builtins__": {}}, names)
                    except Exception:
                        cur = None
                else:
                    name = item
                    cur = None if cur is None else cur + 1
                if cur is not None:
                    names[name.strip()] = cur
    return names


def braced(text, start):
    """The text inside the brace group opening at text[start]."""
    depth, i = 0, start
    while True:
        depth += {"{": 1, "}": -1}.get(text[i], 0)
        i += 1
        if depth == 0:
            return text[start + 1:i - 1]


def groups(body):
    """(what precedes it, contents) for each top-level brace group."""
    out, depth, start, lead = [], 0, 0, ""
    for i, c in enumerate(body):
        if c == "{":
            if depth == 0:
                start, pre = i, lead
            depth += 1
        elif c == "}":
            depth -= 1
            if depth == 0:
                out.append((pre, body[start + 1:i]))
                lead = ""
        elif depth == 0:
            lead += c
    return out


def cells(row):
    return [c.strip() for c in re.sub(r"[{}]", ",", row).split(",") if c.strip()]


def tables(path, names):
    """table name -> (declared size or None, {index: cells})"""
    text = strip_comments(open(path, errors="replace").read())
    out = {}
    for m in re.finditer(r"^FTStatusDesc\s+(\w+)\s*\[([^\]]*)\]\s*=\s*\{", text, re.M):
        body = re.sub(r"#[^\n]*", " ", braced(text, m.end() - 1))
        rows, index = {}, 0
        for lead, row in groups(body):
            d = re.search(r"\[([^\]]*)\]\s*=", lead)
            if d:
                index = eval(d.group(1), {"__builtins__": {}}, names)
            rows[index] = cells(row)
            index += 1
        size = m.group(2).strip()
        out[m.group(1)] = (eval(size, {"__builtins__": {}}, names) if size else None, rows)
    return out


def kind_map(names):
    """(decomp's kind -> table, port's kind -> table)"""
    text = strip_comments(open(os.path.join(DECOMP, "src", "ft", "ftmain.c")).read())
    m = re.search(SPECIAL + r"\s*\[[^\]]*\]\s*=\s*\{", text)
    dec = [c.strip() for c in braced(text, m.end() - 1).split(",") if c.strip()]
    text = strip_comments(open(PORT).read())
    m = re.search(SPECIAL + r"\s*\[\]\s*=\s*\{", text)
    port = {}
    for kind, _, table in re.findall(r"\[(\w+)\]\s*=\s*(FT_SPECIAL_TABLE\((\w+)\)|FT_SPECIAL_NONE)",
                                     braced(text, m.end() - 1)):
        port[names[kind]] = (kind, table or None)
    return dec, port


def base_of(table, names):
    if table == "dFTCommonNullStatusDescs":
        return 0
    if table == "dFTCommonActionStatusDescs":
        return names["nFTCommonStatusActionStart"]
    return names["nFTCommonStatusSpecialStart"]


def differences():
    """[(key, text)] -- key is what KNOWN is looked up by."""
    names = enum_values()
    dec = {}
    for path in sorted(glob.glob(os.path.join(DECOMP, "src", "ft", "**", "*status.h"), recursive=True)):
        dec.update(tables(path, names))
    port = tables(PORT, names)
    out = []
    for table in sorted(set(dec) | set(port)):
        if table not in port:
            out.append((table, "%s: the port has no table (%d rows)" % (table, len(dec[table][1]))))
            continue
        if table not in dec:
            out.append((table, "%s: the decomp has no such table" % table))
            continue
        base = base_of(table, names)
        (psize, prows), (_, drows) = port[table], dec[table]
        if psize is not None and psize > len(drows):
            out.append((table, "%s: declared %d rows, the game's has %d" % (table, psize, len(drows))))
        for i in sorted(set(drows) | set(prows)):
            status = base + i
            key = (table, status)
            if i not in prows:
                out.append((key, "%s status %d: missing (game: %s)" % (table, status, ", ".join(drows[i]))))
            elif i not in drows:
                out.append((key, "%s status %d: past the game's table" % (table, status)))
            elif prows[i] != drows[i]:
                cols = ["column %d is %s, the game's %s" % (k, p, d)
                        for k, (d, p) in enumerate(zip(drows[i], prows[i])) if d != p]
                if len(prows[i]) != len(drows[i]):
                    cols.append("%d columns, the game's %d" % (len(prows[i]), len(drows[i])))
                out.append((key, "%s status %d: %s" % (table, status, "; ".join(cols))))
    dmap, pmap = kind_map(names)
    for kind_id, table in enumerate(dmap):
        kind, ptable = pmap.get(kind_id, ("kind %d" % kind_id, None))
        if ptable != table:
            out.append(((SPECIAL, kind), "%s[%s] is %s, the game's %s" % (SPECIAL, kind, ptable or "NONE", table)))
    return out


def main():
    check = "--check" in sys.argv[1:]
    diffs = differences()
    unknown = [text for key, text in diffs if key not in KNOWN]
    seen = {key for key, _ in diffs}
    stale = ["%s: %s" % (key, why) for key, why in KNOWN.items() if key not in seen]
    if not check:
        for key, text in diffs:
            print("%-60s %s" % (KNOWN.get(key, "UNLISTED"), text))
    for text in unknown:
        print("status_check: not the game's, and not in KNOWN: " + text)
    for text in stale:
        print("status_check: KNOWN entry now matches the game, remove it: " + text)
    print("status_check: %d difference(s) from the game's status tables, all in KNOWN"
          % len(diffs) if not (unknown or stale) else
          "status_check: %d unlisted, %d stale" % (len(unknown), len(stale)))
    return 1 if (unknown or stale) else 0


if __name__ == "__main__":
    sys.exit(main())

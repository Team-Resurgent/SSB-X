#!/usr/bin/env python3
"""io_check.py -- every disc call goes through the one I/O lock, and the
memory card stays out of it.

KOS's /cd is not safe for two threads at once: an fh/cache lock-order
deadlock, a directory cache used after its unlock, and fs_close freeing a
handle under a concurrent fs_read. The port
answers with one mutex in src/dc/assetroot.c that every file call takes
. The memory card is the other half: it is
vmufs_* and vmu_pkg_*, which keep their own mutex and never touch fs.c's
file table, so a card write that takes a second does not stand in front
of the disc. This holds the three things that make both true:

  1. No KOS or stdio file call (fs_open/fs_read/..., fopen/fread/...) in
     the port's sources outside src/dc/assetroot.c. Everything else reads
     through asset_*.
  2. No KOS memory-card call (vmufs_*, vmu_*) outside src/dc/vmucard.c.
     The lock itself is static in assetroot.c (io_take/io_give) since
     the card left it, so nothing outside that file can take it at all.
  3. In assetroot.c, every function that makes a file call either takes
     the lock itself (io_take ... io_give around the call), or is only
     ever used by functions that do -- the *_locked bodies and their
     helpers, followed to a fixpoint.

Host-only code is exempt: what sits under `#ifndef _arch_dreamcast`, the
`#else` of `#ifdef _arch_dreamcast`, or `#ifdef FT_HOSTTEST` runs off the
SH-4, on one thread, and reads with stdio on purpose. So is the host test
(src/game/ssb64/hosttest*, src/dc/test/).

tools/check/io_known.tsv lists allowed exceptions, one per line:
file<TAB>function<TAB>why, for rules 1 and 2. An entry that no longer
matches fails the run.

Source only. `./run.sh test io` runs it alone; `./run.sh test` with the rest.
"""
import glob
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
KNOWN_TSV = os.path.join(os.path.dirname(os.path.abspath(__file__)), "io_known.tsv")

HOME = "src/dc/assetroot.c"
VMU = "src/dc/vmucard.c"

CALLS = ("fs_open", "fs_read", "fs_write", "fs_seek", "fs_tell", "fs_close",
         "fs_total", "fs_mmap", "fs_readdir", "fs_stat", "fs_unlink",
         "fs_rename", "fs_copy", "fs_load", "fs_vmu_set_header",
         "fopen", "fread", "fwrite", "fseek", "ftell", "fclose",
         "open", "read", "write", "lseek", "close")
# Not a member (`->read(`, `.close(`) and not the tail of a longer name
# (`asset_read(` ends in `read(` after an underscore, a word character).
CALL = re.compile(r"(?<![\w.>])(%s)\s*\(" % "|".join(CALLS))
# KOS's memory-card calls: vmufs_write, vmu_pkg_build, vmu_draw_lcd, ...
# (the port's own vmucard_* have no underscore after "vmu")
VMU_CALL = re.compile(r"(?<![\w.>])(vmufs_\w+|vmu_\w+)\s*\(")


def sources():
    pats = ["src/dc/*.c", "src/dc/*.h", "src/dc/decomp/**/*.c",
            "src/dc/decomp/**/*.h", "src/game/ssb64/*.c",
            "src/game/ssb64/*.h"]
    out = set()
    for p in pats:
        out.update(glob.glob(os.path.join(ROOT, p), recursive=True))
    return sorted(os.path.relpath(f, ROOT) for f in out
                  if not os.path.basename(f).startswith("hosttest"))


def host_only_lines(lines):
    """The set of line indexes that only a host build compiles."""
    stack, host = [], set()   # frames: [kind, in_else]; kind dc/host/other
    for i, raw in enumerate(lines):
        s = raw.strip()
        m = re.match(r"#\s*(ifdef|ifndef|if|elif|else|endif)\b(.*)", s)
        if m:
            d, rest = m.group(1), m.group(2).strip()
            if d in ("ifdef", "ifndef", "if"):
                kind = "other"
                if rest in ("_arch_dreamcast", "defined(_arch_dreamcast)"):
                    kind = "dc" if d != "ifndef" else "host"
                elif rest in ("FT_HOSTTEST", "defined(FT_HOSTTEST)"):
                    kind = "host" if d != "ifndef" else "dc"
                elif d == "if" and rest in ("!defined(_arch_dreamcast)",):
                    kind = "host"
                stack.append([kind, False])
            elif d == "else" and stack:
                stack[-1][1] = True
            elif d == "elif" and stack:
                stack[-1] = ["other", True]
            elif d == "endif" and stack:
                stack.pop()
            continue
        if any((k == "dc" and e) or (k == "host" and not e)
               for k, e in stack):
            host.add(i)
    return host


def is_comment(line):
    t = line.lstrip()
    return t.startswith("*") or t.startswith("/*") or t.startswith("//")


def functions(lines):
    """(name, index of `{`, index of `}`) for each column-0 brace body,
    in the decomp's style (tools/check/vramfence_check.py)."""
    out, i = [], 0
    while i < len(lines):
        if lines[i] == "{":
            j = i + 1
            while j < len(lines) and lines[j] != "}":
                j += 1
            k, m = i - 1, None
            while k >= 0 and lines[k].strip():
                m = re.search(r"([A-Za-z_]\w*)\s*\(", lines[k])
                if m:
                    break
                k -= 1
            out.append((m.group(1) if m else "?", i, j))
            i = j + 1
        else:
            i += 1
    return out


def raw_calls(lines, host, a=0, b=None, pat=CALL):
    b = len(lines) if b is None else b
    return [(i, m.group(1)) for i in range(a, b)
            for m in [pat.search(lines[i])]
            if m and i not in host and not is_comment(lines[i])]


def owner(funcs, i):
    for name, a, b in funcs:
        if a < i < b:
            return name
    return None


def bracketed(lines, a, b, i, take, give):
    """TRUE when line i of the body (a, b) is inside a `take` ... `give`.

    A give counts only at the brace depth its take was made at: the one in
    an early-exit block (`if (...) { give(); return; }`) leaves the code
    after the block still locked. A block closing above a take's depth
    ends it too."""
    held, depth = [], 0
    tok = re.compile(r"(\{)|(\})|\b(%s)\s*\(|\b(%s)\s*\(" % (take, give))
    for k in range(a + 1, i):
        if is_comment(lines[k]):
            continue
        for m in tok.finditer(lines[k]):
            if m.group(1):
                depth += 1
            elif m.group(2):
                depth -= 1
                while held and depth < held[-1]:
                    held.pop()
            elif m.group(3):
                held.append(depth)
            elif held and held[-1] == depth:
                held.pop()
    return bool(held)


def main():
    known = {}
    if os.path.exists(KNOWN_TSV):
        for line in open(KNOWN_TSV, encoding="utf-8"):
            if line.strip() and not line.startswith("#"):
                f, fn, why = line.rstrip("\n").split("\t", 2)
                known[(f, fn)] = why
    matched, bad, counted, cards = set(), [], 0, 0

    for rel in sources():
        lines = open(os.path.join(ROOT, rel), encoding="utf-8",
                     errors="replace").read().splitlines()
        host = host_only_lines(lines)
        funcs = functions(lines)
        calls = raw_calls(lines, host)

        # rule 2: the card's calls
        for i, c in raw_calls(lines, host, pat=VMU_CALL):
            fn = owner(funcs, i) or "?"
            if rel == VMU:
                cards += 1
            elif (rel, fn) in known:
                matched.add((rel, fn))
            else:
                bad.append("%s:%d: %s() calls %s() -- the memory card is "
                           "src/dc/vmucard.c's (src/dc/vmucard.h)"
                           % (rel, i + 1, fn, c))

        if not calls:
            continue

        if rel != HOME:
            for i, c in calls:
                fn = owner(funcs, i) or "?"
                if (rel, fn) in known:
                    matched.add((rel, fn))
                    continue
                bad.append("%s:%d: %s() calls %s() -- file I/O goes "
                           "through asset_* (src/dc/assetroot.h)"
                           % (rel, i + 1, fn, c))
            continue

        # assetroot.c: who takes the lock, and who is only used by them
        # a column-0 brace with no declarator is a struct or table
        bodies = {n: (a, b) for n, a, b in funcs if n != "?"}
        takes = {n for n, (a, b) in bodies.items()
                 if any(re.search(r"\bio_take\s*\(", lines[k])
                        for k in range(a + 1, b) if not is_comment(lines[k]))}
        users = {n: set() for n in bodies}
        for n, (a, b) in bodies.items():
            for k in range(a + 1, b):
                if is_comment(lines[k]):
                    continue
                for other in bodies:
                    if other != n and re.search(r"\b%s\b" % re.escape(other), lines[k]):
                        users[other].add(n)
        safe = set(takes)
        changed = True
        while changed:
            changed = False
            for n in bodies:
                if n not in safe and users[n] and users[n] <= safe:
                    safe.add(n)
                    changed = True
        for i, c in calls:
            fn = owner(funcs, i)
            counted += 1
            if fn is None:
                bad.append("%s:%d: %s() outside any function" % (rel, i + 1, c))
            elif fn in takes:
                a, b = bodies[fn]
                if not bracketed(lines, a, b, i, "io_take", "io_give"):
                    bad.append("%s:%d: %s() calls %s() outside its own "
                               "io_take()/io_give()" % (rel, i + 1, fn, c))
            elif fn not in safe:
                why = sorted(users[fn] - safe) or ["nothing"]
                bad.append("%s:%d: %s() calls %s() and is used by %s, "
                           "which do not hold the lock"
                           % (rel, i + 1, fn, c, ", ".join(why)))

    stale = sorted(set(known) - matched)
    for line in bad:
        print("io_check: " + line)
    for f, fn in stale:
        print("io_check: io_known.tsv %s %s() matches nothing now -- drop "
              "the entry" % (f, fn))
    if bad or stale:
        sys.exit("io_check: %d violation(s), %d stale entr(ies)"
                 % (len(bad), len(stale)))
    print("io_check: %d file calls, all in %s and all under the I/O lock; "
          "%d memory-card calls, all in %s and none under it (%d known "
          "exception(s))" % (counted, HOME, cards, VMU, len(matched)))


if __name__ == "__main__":
    main()

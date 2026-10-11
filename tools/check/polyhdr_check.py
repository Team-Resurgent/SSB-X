#!/usr/bin/env python3
"""polyhdr_check.py -- every poly header the port submits was compiled.

`draw_line` in src/dc/db.c drew nothing at all for a long
stretch of development, and the reason is the whole of what this
checks. The tile accelerator takes a strip as a polygon header followed
by vertices, and a `pvr_poly_hdr_t` is only a header once
`pvr_poly_compile` has written one into it. A zeroed one is not a header,
it is eight words of zero; the TA reads `cmd == 0`, and discards
everything behind it in silence. No lines, no error, no dropped frames,
no assert -- the submission simply does not become pixels.

That is a uniquely bad failure to own, because every tool the port has
says the code is fine. The object is declared and used, so `git grep`
finds it; nothing is unused, so the build is clean; the frame still
completes, so the probes are clean and the frame time does not move. The
only instrument that can see it is a person looking at the television,
and the only reason it was ever found is that someone did and said the
lines were not there.

What made it possible was a refactor: the four lines that compiled
db.c's `gLineHdr` lived in the `main()` of src/game/ssb64, and
a70c39b moved 1,142 lines of instrument out of that main() into
src/dc/db.c without them. The submission came across, the producer did
not. So the shape this file looks for is not "an uninitialized variable"
-- `gLineHdr` was a file-scope static, zero-initialized and perfectly
legal -- but a header that is **consumed and never produced**.

The check, per file that submits one:

  1. every `pvr_prim`/`pvr_list_prim` of a `pvr_poly_hdr_t` resolves to
     the object submitted, and some `pvr_poly_compile` in the same file
     resolves to the same object;
  2. for a header that is a local, the compile must be in the **same
     function** -- a compile of a different function's `hdr` says
     nothing about this one;
  3. a file whose producer genuinely cannot be matched by name appears
     in INDIRECT below with the compile site written down, the way
     overlay_check.py makes its own exceptions arguable rather than
     assumed.

Comments are stripped before any of it, and that is load-bearing rather
than tidy: db.c now carries the lost `pvr_poly_compile(&gLineHdr, &cxt)`
line inside the comment that explains the bug, and a checker that reads
prose would accept the description of the fix in place of the fix.

Run against the tree as it stood at ce23ec4^ this reports db.c, which is
the only evidence that it works -- a checker for a bug that has already
been fixed otherwise passes for free.

Reads source only; no build and no ROM needed.
"""
import os
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "src" / "dc"

# Files whose header is produced somewhere a name match cannot reach, each
# with the site that produces it. Keep this as short as the argument for
# each entry is good.
INDIRECT = {
    "fighter.c": (
        "fighter_init compiles into the pack's own arrays through a "
        "helper that takes them as parameters -- `pvr_poly_compile(pass "
        "? tr : own, &cxt)` at fighter.c:224, where own/tr are f->hdr "
        "and f->hdr_tr. draw_batches then submits `hdr`, a local bound "
        "to one of those two. The producer and the consumer are the same "
        "pair of arrays; only the names in between differ."),
}

SUBMIT = re.compile(r'\bpvr_(?:list_)?prim\s*\(')
COMPILE = re.compile(r'\bpvr_poly_compile\s*\(')


def strip_comments(src):
    """Comments and string bodies out, newlines kept so lines still count."""
    def blank(m):
        return re.sub(r'[^\n]', ' ', m.group(0))

    src = re.sub(r'/\*.*?\*/', blank, src, flags=re.S)
    src = re.sub(r'//[^\n]*', blank, src)
    src = re.sub(r'"(?:\\.|[^"\\\n])*"', blank, src)
    return src


def functions(src):
    """[(name, first_line, last_line)] for the file's definitions.

    A definition is a line at column 0 that reaches a `{` at column 0;
    good enough for src/dc/, which is written that way throughout.
    """
    lines = src.split("\n")
    out = []
    i = 0
    while i < len(lines):
        m = re.match(r'^([A-Za-z_][\w \t\*]*?)\b([A-Za-z_]\w*)\s*\(', lines[i])
        if m and not lines[i].startswith(("#", "}")):
            j, name = i, m.group(2)
            while j < len(lines) and not lines[j].startswith("{"):
                if lines[j].rstrip().endswith(";"):
                    break
                j += 1
            if j < len(lines) and lines[j].startswith("{"):
                k = j + 1
                while k < len(lines) and not lines[k].startswith("}"):
                    k += 1
                out.append((name, i + 1, k + 1))
                i = k + 1
                continue
        i += 1
    return out


def enclosing(funcs, line):
    for name, a, b in funcs:
        if a <= line <= b:
            return name
    return None


def arg0(src, at):
    """The first argument of the call whose '(' follows `at`."""
    i = src.index("(", at)
    depth, j = 0, i
    while j < len(src):
        if src[j] == "(":
            depth += 1
        elif src[j] == ")":
            depth -= 1
            if depth == 0:
                break
        elif src[j] == "," and depth == 1:
            break
        j += 1
    return src[i + 1:j].strip()


def base(expr):
    """The object an argument expression names.

    `&gLineHdr` -> gLineHdr, `&st->wp_hdr` -> wp_hdr, `f->hdr + i` -> hdr,
    `&f->hdr_tr[i]` -> hdr_tr. Returns None for anything with no single
    object in it (a ternary, a cast, a call).
    """
    e = expr.strip()
    e = re.sub(r'^\&\s*', "", e)
    e = re.sub(r'\[[^\]]*\]', "", e)
    e = re.sub(r'\s*\+\s*\w+$', "", e)
    if re.search(r'[?:()]', e):
        return None
    m = re.findall(r'[A-Za-z_]\w*', e)
    return m[-1] if m else None


def hdr_objects(src):
    """Names declared `pvr_poly_hdr_t` in this file (value or pointer)."""
    names = set()
    for m in re.finditer(
            r'\bpvr_poly_hdr_t\s*\**\s*([A-Za-z_]\w*)', src):
        names.add(m.group(1))
    return names


def main():
    failures, checked, files_seen = [], 0, 0
    by_name, indirect = 0, 0

    for path in sorted(SRC.rglob("*.c")):
        if "/test/" in str(path):
            continue
        raw = path.read_text(errors="replace")
        if not SUBMIT.search(raw):
            continue
        src = strip_comments(raw)
        funcs = functions(src)
        rel = path.name

        # what this file declares as a header, plus header-typed members
        # it reaches through a pointer (stage.h's wp_hdr and the like)
        local_names = hdr_objects(src)
        for hdr in (ROOT / "src" / "dc").rglob("*.h"):
            local_names |= hdr_objects(strip_comments(
                hdr.read_text(errors="replace")))

        produced = {}          # base name -> set of functions compiling it
        for m in COMPILE.finditer(src):
            b = base(arg0(src, m.start()))
            fn = enclosing(funcs, src[:m.start()].count("\n") + 1)
            if b:
                produced.setdefault(b, set()).add(fn)
            else:
                produced.setdefault("*", set()).add(fn)

        submits = []
        for m in SUBMIT.finditer(src):
            a = arg0(src, m.start())
            b = base(a)
            if b is None or b not in local_names:
                continue           # a vertex, not a header
            submits.append((b, src[:m.start()].count("\n") + 1, a))

        if not submits:
            continue
        files_seen += 1

        for b, line, expr in submits:
            checked += 1
            if b in produced:
                by_name += 1
                fn = enclosing(funcs, line)
                # rule 2: a local must be compiled in its own function
                if re.search(r'^\s+pvr_poly_hdr_t\s+' + re.escape(b) + r'\b',
                             src, re.M) and fn not in produced[b] \
                        and "*" not in produced.get(b, ()):
                    failures.append(
                        "%s:%d submits local `%s` but the only "
                        "pvr_poly_compile of that name is in %s()"
                        % (rel, line, b, "/".join(sorted(
                            x or "?" for x in produced[b]))))
                continue
            if rel in INDIRECT:
                indirect += 1
                continue
            failures.append(
                "%s:%d submits `%s` to the TA and nothing in this file "
                "compiles it -- a zeroed pvr_poly_hdr_t is not a header, "
                "and the TA drops every strip behind it in silence"
                % (rel, line, expr))

    if failures:
        for f in failures:
            print("polyhdr_check: " + f, file=sys.stderr)
        return 1

    print("polyhdr_check: %d poly headers submitted across %d files, every "
          "one compiled before it is sent (%d matched to their own "
          "pvr_poly_compile, %d indirect)"
          % (checked, files_seen, by_name, indirect))
    return 0


if __name__ == "__main__":
    sys.exit(main())

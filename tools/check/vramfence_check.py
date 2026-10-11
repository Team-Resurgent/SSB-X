#!/usr/bin/env python3
"""vramfence_check.py -- every write to video RAM waits for the renderer.

The port's frame is update-then-draw (src/dc/taskman.c syTaskmanRunFrame),
and the PVR's own wait is the FIRST call of the draw. So a loader that runs
in the update -- a fighter pack's textures and palettes, a stage's
wallpaper, a particle bank, a sprite bank, the transition's photocopy of
the framebuffer -- writes video RAM while the PVR is still rendering the
frame the PREVIOUS draw submitted, out of the very memory being
overwritten. pvr_wait_ready() does not close that window: it means "ready
to accept a scene", and the TA accepts one into its own vertex buffer while
the render of the last one is still running.

src/dc/dcpvr.h dc_pvr_vram_fence() is the wait that does close it. This
asserts that every function which allocates, frees or writes video RAM
calls it first.

The symptom is a polygon drawn from half-written or already-freed texels
for one frame, wherever the load happened to land -- and at a scene change
for the whole load, because the old scene's last frame is the one drawn and
nothing replaces it until the new scene's first. The render is queued, not
started, for up to a retrace after pvr_scene_finish (KOS starts it at the
vertical blank), so this shows on any PVR;
-DDB_FENCE_TRACE prints what each fence found (src/dc/dcpvr.h).

Source only: it reads src/dc/*.c and nothing else.

`./run.sh test vramfence` runs it alone; `./run.sh test` runs it with the rest.
"""
import glob
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SRC = os.path.join(ROOT, "src", "dc")

FENCE = "dc_pvr_vram_fence("

# Everything that hands video RAM out, gives it back, or writes it. The
# allocation counts with the rest: it is the first half of an upload, and
# the block it returns may be one the renderer is still reading out of.
MUTATORS = (
    "pvr_mem_malloc",
    "pvr_mem_free",
    "pvr_txr_load",
    "pvr_txr_load_ex",
    "pvr_txr_load_dma",
    "pvr_set_pal_entry",
)
CALL = re.compile(r"\b(%s)\s*\(" % "|".join(MUTATORS))

# A function that mutates video RAM without fencing, and why that is not
# the bug above. Keyed by (file, function); the value is the reason. An
# entry that stops matching fails the run, so this list cannot outlive the
# code.
KNOWN = {}


def functions(path):
    """(name, first line, body lines) for each top-level function.

    The port is written in the decomp's brace style -- the declarator on
    its own line and `{` in column 0 -- so a function body is the run
    between a column-0 `{` and the column-0 `}` that closes it, and the
    declarator is the line above.
    """
    lines = open(path, encoding="utf-8", errors="replace").read().splitlines()
    out, i = [], 0

    while i < len(lines):
        if lines[i] == "{":
            j = i + 1
            while j < len(lines) and lines[j] != "}":
                j += 1
            decl = lines[i - 1] if i else ""
            m = re.search(r"([A-Za-z_][A-Za-z0-9_]*)\s*\(", decl)
            # A declarator wrapped over two lines keeps its name on the
            # first of them; walk back until one carries a name.
            k = i - 1
            while m is None and k > 0 and lines[k].strip():
                k -= 1
                m = re.search(r"([A-Za-z_][A-Za-z0-9_]*)\s*\(", lines[k])
            out.append((m.group(1) if m else "?", i, lines[i + 1:j]))
            i = j + 1
        else:
            i += 1
    return out


def audit(path):
    """[(function, line, call)] for each unfenced mutation."""
    bad = []

    for name, first, body in functions(path):
        fenced = None
        for n, line in enumerate(body):
            if line.lstrip().startswith("*") or line.lstrip().startswith("/*"):
                continue        # the comments that explain the rule
            if FENCE in line:
                fenced = n
                continue
            m = CALL.search(line)
            if m is None:
                continue
            if fenced is not None and fenced < n:
                continue
            bad.append((name, first + 2 + n, m.group(1)))
            break               # one line per function is the whole story
    return bad


def main():
    bad, matched, seen = [], set(), 0

    for path in sorted(glob.glob(os.path.join(SRC, "*.c"))):
        name = os.path.basename(path)

        for func, line, call in audit(path):
            seen += 1
            if (name, func) in KNOWN:
                matched.add((name, func))
            else:
                bad.append("%s:%d: %s() calls %s() with no "
                           "dc_pvr_vram_fence() before it"
                           % (name, line, func, call))

    stale = sorted(set(KNOWN) - matched)

    for line in bad:
        print("vramfence_check: " + line)
    for (name, func) in stale:
        print("vramfence_check: KNOWN %s %s() matches nothing now -- drop "
              "the entry" % (name, func))
    if bad or stale:
        sys.exit("vramfence_check: %d unfenced video RAM mutation(s), %d "
                 "stale KNOWN entr(ies)" % (len(bad), len(stale)))

    fenced = sum(1 for path in glob.glob(os.path.join(SRC, "*.c"))
                 for _n, _f, body in functions(path)
                 if any(FENCE in l for l in body))
    print("vramfence_check: %d function(s) mutate video RAM and every one "
          "fences first (%d known-safe)" % (fenced, len(matched)))


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""camera_check.py -- two camera traps the compiler cannot see, each of
which leaves a scene drawing nothing while every instrument reads healthy.

1. No scene may keep a FILLCOLOR clear camera that 3D cameras draw after.

On the N64, gcMakeDefaultCameraGObj(..., COBJ_FLAG_FILLCOLOR, colour) is a
harmless clear. On the port the fill is a translucent quad at the frame's
next sprite depth (src/dc/objdisplay.c gcPrepCameraViewport), and every
opaque depth sits behind every sprite depth. So a fill that runs before a
3D camera covers it completely, while every instrument reads healthy. That
cost the staff roll, the opening Room and the ending a blank screen each.
The fix every time is to drop the flag and keep the GObj and the colour.

This scans src/dc for gcMakeDefaultCameraGObj calls that pass
COBJ_FLAG_FILLCOLOR. The ones listed in KNOWN are fills the port relies
on, each with its reason. Any other one fails.

2. Every gmCameraMakeBattleCamera() call must be followed by setting
gGMCameraGObj->camera_mask. The function leaves the mask empty, and an
empty mask walks no DL link: the battle camera submits no stage or
fighter triangle. That was true of VS, the 1P game, Training, Auto Demo
and, until 2026-09-22, How to Play. The decomp has no such line; it is
port-only.
"""

import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
SRC = ROOT / "src" / "dc"

# file -> number of FILLCOLOR default cameras allowed, and why
KNOWN = {
    "mntitle.c": (3, "the fire camera's colour behind the flames and the "
                     "proceed camera's black frames; the title draws only "
                     "sprites after them (mn/mncommon/mntitle.c:1393, :492)"),
}

CALL = re.compile(r"gcMakeDefaultCameraGObj\s*\(([^;]*?)\)\s*;", re.S)


BATTLE = re.compile(r"gmCameraMakeBattleCamera\s*\(\s*\)\s*;")
MASK = re.compile(r"gGMCameraGObj->camera_mask\s*=")


def battle_mask_problems():
    out = []
    for path in sorted(SRC.glob("*.c")):
        if path.name == "gmcamera.c":
            continue
        text = path.read_text(errors="replace")
        for m in BATTLE.finditer(text):
            tail = text[m.end():m.end() + 2000]
            if not MASK.search(tail):
                line = text.count("\n", 0, m.start()) + 1
                out.append("%s:%d: gmCameraMakeBattleCamera() with no "
                           "gGMCameraGObj->camera_mask set after it"
                           % (path.name, line))
    return out


def main():
    bad = battle_mask_problems()
    seen = {}
    for path in sorted(SRC.glob("*.c")):
        text = path.read_text(errors="replace")
        for m in CALL.finditer(text):
            if "COBJ_FLAG_FILLCOLOR" not in m.group(1):
                continue
            line = text.count("\n", 0, m.start()) + 1
            seen.setdefault(path.name, []).append(line)
    for name, lines in seen.items():
        allowed = KNOWN.get(name, (0, ""))[0]
        if len(lines) > allowed:
            bad.append("%s: %d FILLCOLOR default camera(s) at line(s) %s, "
                       "%d allowed" % (name, len(lines),
                                       ", ".join(map(str, lines)), allowed))
    stale = [n for n in KNOWN if n not in seen]
    for b in bad:
        print("camera_check: " + b)
    for n in stale:
        print("camera_check: stale KNOWN entry %s" % n)
    if bad or stale:
        return 1
    print("camera_check: %d FILLCOLOR default camera(s), all in KNOWN; "
          "every battle camera gets its mask"
          % sum(len(v) for v in seen.values()))
    return 0


if __name__ == "__main__":
    sys.exit(main())

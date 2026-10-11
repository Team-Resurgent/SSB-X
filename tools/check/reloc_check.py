#!/usr/bin/env python3
"""reloc_check.py -- hold src/dc/decomp/reloc_data.us.h to the decomp.

Every ROM file, and every block inside one, has a symbol in the game's
link: llMarioMainFileID, llGRCastleMapMapHeader, llNessMainMotionAttackS4
and 5,043 more. They are absolute linker symbols -- the ADDRESS of
llGRCastleMapFileID is the file id -- which is why the game writes
&llGRCastleMapFileID everywhere it means one.

The decomp does not check its list in either: tools/genRelocSymbols.py
derives it from tools/relocFileDescriptions.us.txt so it stays true as
blocks are converted. The port runs that same tool
(tools/export/gen_reloc_header.sh), which is what this checks, in three parts:

  1. regenerating the header now reproduces the one in the tree, so it is
     really generated from the pinned decomp and nobody has edited it;
  2. every one of the 82 stage-map symbols src/dc/mpshim.c defines is in
     it, and declared `int` -- mpshim's definition has to agree with the
     decomp's declaration, and that is the one place the two meet;
  3. the three common statuses whose ONLY blocker was this header still
     compile. ftcommonattacks4.c, ftcommoncapturecaptain.c and
     ftcommoncapturekirby.c each name one ll* symbol and nothing else the
     port lacks; the other 44 of the 47 missing common statuses compiled
     without it. They are the tripwire: if the generated declarations
     stop reaching the compiler, these three fail and the other 44 do
     not, so the failure names its own cause.

Part 3 needs the cross-compiler, not a build. scripts/test_oracle.sh runs
this; `./run.sh test reloc` runs it alone.
"""

import os
import re
import subprocess
import sys
import tempfile

sys.path[:0] = [os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), d)
                for d in ("lib", "export", "check")]
from ssb_paths import DECOMP_DIR  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
HEADER = os.path.join(ROOT, "src", "dc", "decomp", "reloc_data.us.h")
GEN = os.path.join(ROOT, "tools", "export", "gen_reloc_header.sh")

# The 82 whose definitions live in src/dc/mpshim.c, off the
# MPSHIM_GROUND_FILES list in src/dc/decomp/reloc_data.h.
GROUNDS = """Castle Sector Jungle Zebes Hyrule Yoster Pupupu Yamabuki
Inishie PupupuSmall PupupuTest Explain YosterSmall Metal Zako Bonus3 Last
Bonus1Mario Bonus1Fox Bonus1Donkey Bonus1Samus Bonus1Luigi Bonus1Link
Bonus1Yoshi Bonus1Captain Bonus1Kirby Bonus1Pikachu Bonus1Purin Bonus1Ness
Bonus2Mario Bonus2Fox Bonus2Donkey Bonus2Samus Bonus2Luigi Bonus2Link
Bonus2Yoshi Bonus2Captain Bonus2Kirby Bonus2Pikachu Bonus2Purin
Bonus2Ness""".split()

# The three of the 47 missing common statuses that this header, and only
# this header, unblocks. Each is listed with the symbol it needs, so a
# failure here reads as what it is.
TRIPWIRE = {
    "ft/ftcommon/ftcommonattacks4.c":
        "llNessMainMotionAttackS4ReflectorFTSpecialColl",
    "ft/ftcommon/ftcommoncapturecaptain.c":
        "llCaptainMainMotionSpecialHiVec2h",
    "ft/ftcommon/ftcommoncapturekirby.c":
        "llKirbyMainMotionSpecialNFTKirbyCopy",
}

DECL = re.compile(r"^extern\s+(\w+)\s+(\w+)\s*;", re.M)


def declarations():
    if not os.path.exists(HEADER):
        return None
    return {m.group(2): m.group(1)
            for m in DECL.finditer(open(HEADER).read())}


def check_generated(failures):
    """The header in the tree is what the decomp's generator emits now."""
    with tempfile.TemporaryDirectory() as tmp:
        out = os.path.join(tmp, "reloc_data.us.h")
        r = subprocess.run([GEN, out], capture_output=True, text=True)
        if r.returncode != 0:
            failures.append("gen_reloc_header.sh failed: "
                            + r.stderr.strip())
            return
        fresh = open(out).read()
    if not os.path.exists(HEADER):
        failures.append("%s is missing; `make -C src/game/ssb64 "
                        "../../dc/decomp/reloc_data.us.h` builds it"
                        % os.path.relpath(HEADER, ROOT))
        return
    if open(HEADER).read() != fresh:
        failures.append(
            "%s is not what the decomp's generator emits: it was edited by "
            "hand, or the decomp pin moved without a rebuild. It is "
            "generated, never written -- see tools/export/gen_reloc_header.sh"
            % os.path.relpath(HEADER, ROOT))


def check_grounds(decls, failures):
    """mpshim.c's definitions and the decomp's declarations agree."""
    for name in GROUNDS:
        for sym in ("llGR%sMapFileID" % name, "llGR%sMapMapHeader" % name):
            kind = decls.get(sym)
            if kind is None:
                failures.append(
                    "%s: src/dc/mpshim.c defines it, the generated header "
                    "does not declare it" % sym)
            elif kind != "int":
                failures.append(
                    "%s is declared `%s`; src/dc/mpshim.c defines it `int`"
                    % (sym, kind))


def check_tripwire(failures):
    """The three statuses this header unblocks still compile."""
    d = DECOMP_DIR
    cc = ["kos-cc", "-c", "-o", "/dev/null", "-O2", "-m4-single", "-ml",
          "-D__DREAMCAST__", "-D_arch_dreamcast=1", "-D_arch_sub_pristine=1",
          "-isystem", "/opt/toolchains/dc/kos/include",
          "-isystem", "/opt/toolchains/dc/kos/kernel/arch/dreamcast/include",
          "-isystem", "/opt/toolchains/dc/kos/addons/include/",
          "-I", os.path.join(ROOT, "src", "dc"),
          "-I", os.path.join(ROOT, "src", "dc", "decomp"),
          "-I", os.path.join(d, "src"),
          "-idirafter", os.path.join(d, "include"),
          "-D_LANGUAGE_C", "-DF3DEX_GBI_2", "-DREGION_US",
          "-w"]
    for src, sym in sorted(TRIPWIRE.items()):
        r = subprocess.run(cc + [os.path.join(d, "src", src)],
                           capture_output=True, text=True)
        if r.returncode != 0:
            why = ""
            if sym in r.stderr:
                why = (" -- it cannot see %s, which only the generated "
                       "header declares" % sym)
            failures.append("%s does not compile%s\n    %s"
                            % (src, why, r.stderr.strip().splitlines()[0]
                               if r.stderr.strip() else "(no output)"))


def main():
    failures = []
    check_generated(failures)
    decls = declarations()
    if decls is None:
        return fail(failures or ["%s is missing" % HEADER])
    check_grounds(decls, failures)
    check_tripwire(failures)
    if failures:
        return fail(failures)
    print("reloc_check: %d declarations generated from the pinned decomp; "
          "mpshim's %d stage-map symbols agree with them; the %d common "
          "statuses they unblock compile"
          % (len(decls), 2 * len(GROUNDS), len(TRIPWIRE)))
    return 0


def fail(failures):
    for f in failures:
        print("reloc_check: " + f, file=sys.stderr)
    return 1


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""Write the traced half of anm_tiers.tsv from -DDB_ANIM_USE serial logs.

    python3 tools/export/anm_tiers.py LOG [LOG ...]

A -DDB_ANIM_USE build prints one line per animation a fighter binds, once
per scene and pack (src/dc/ftcommon.c ftMainNoteAnimUse):

    animuse: <scene kind> <pack> main|sub <motion id> <anim> <name>

Each line from a scene that reads a tier of the .anm (SCENE_TIERS, which
is src/dc/ftcommon.h's ftManagerSetAnmTier callers) becomes a rule
"<tier> <pack> <table> <motion id>" in the generated block of
anm_tiers.tsv, and the hand-written lines above the block are kept. A
SubMotion row is left out: the hand-written "0 * sub *" has them all.

The logs to feed it: the Characters screen with -DDB_CHARACTERS_TOUR
(every motion kind of every fighter, src/dc/db.h), and a boot through
each tier-0 scene. A rule is never dropped because a later log lacks
it: the block is the union of the logs given and the block already
there, so a partial trace adds without undoing the one before. Delete
the block by hand to start again.
"""
import os
import re
import sys

TSV = os.path.join(os.path.dirname(os.path.abspath(__file__)), "anm_tiers.tsv")
BEGIN = "# -- traced by tools/export/anm_tiers.py; edit above this line --"

# scene kind (sc/scdef.h) -> the tier its ftManagerSetAnmTier asks for
SCENE_TIERS = {
    14: 0,  # nSCKind1PIntro
    16: 0,  # nSCKindPlayersVS
    17: 0,  # nSCKind1PGamePlayers
    18: 0,  # nSCKindPlayers1PTraining
    19: 0,  # nSCKind1PBonus1Players
    20: 0,  # nSCKind1PBonus2Players
    24: 0,  # nSCKindVSResults
    26: 1,  # nSCKindCharacters
}

LINE = re.compile(r"animuse: (\d+) (\S+) (main|sub) (\d+) (\d+) (\S+)")


def read_block(lines, names):
    rules = {}
    for line in lines:
        body, _, note = line.partition("#")
        f = body.split()
        if len(f) == 4:
            key = (f[1], f[2], int(f[3]))
            rules[key] = min(int(f[0]), rules.get(key, 99))
            if note.strip():
                names[key] = note.strip()
    return rules


def main(argv):
    if not argv:
        print(__doc__.strip().split("\n\n")[1], file=sys.stderr)
        return 2
    text = open(TSV).read().split("\n")
    head = text[:text.index(BEGIN)] if BEGIN in text else text
    while head and head[-1] == "":
        head.pop()
    names = {}
    rules = (read_block(text[text.index(BEGIN) + 1:], names)
             if BEGIN in text else {})
    seen = 0
    for path in argv:
        for line in open(path, errors="replace"):
            m = LINE.search(line)
            if not m:
                continue
            seen += 1
            scene, pack, table, mid, _anim, name = m.groups()
            tier = SCENE_TIERS.get(int(scene))
            if tier is None or table == "sub":
                continue
            # a menu sets the same statuses for every fighter it shows,
            # so a MainMotion row one fighter played there is every pack's
            key = ("*" if tier == 0 else pack, table, int(mid))
            rules[key] = min(tier, rules.get(key, 99))
            names[key] = name
    out = head + ["", BEGIN]
    for key in sorted(rules, key=lambda k: (rules[k], k)):
        pack, table, mid = key
        out.append("%d\t%s\t%s\t%d%s" % (rules[key], pack, table, mid,
                                         "\t# " + names[key]
                                         if key in names else ""))
    open(TSV, "w").write("\n".join(out) + "\n")
    print("anm_tiers.tsv: %d traced rules from %d animuse lines"
          % (len(rules), seen))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

#!/usr/bin/env python3
"""ssb64-dc: src/dc/itemoffsets.h, against the pinned decomp's own table.

On the N64 the `llITCommonData*` symbols are the reloc files' own boundary
labels, so a symbol's ADDRESS is a byte offset into ITCommonData or
ITCommonObject -- which is what every use in the decomp means by `&sym`,
and what `itGetPData(ip, &A, &B)` subtracts and adds. The port has no such
labels (the addresses are unrelated globals, so no one pointer makes them
all an offset), so src/dc/itemoffsets.h writes the numbers out.

That makes the header a plain transcription, and a transcription is the one
thing a host test cannot check: every item test either hands a function a
buffer of its own choosing or points `gITManagerCommonData` at a faked
file, so a wrong number reads the wrong bytes *of the fake* and the test
still passes. It fails only against the real file, which is a disc probe.

So this is the oracle instead. src/dc/decomp/reloc_data.us.h is generated
from the decomp's `tools/relocFileDescriptions.us.txt` and carries each
symbol's value in its trailing comment; this reads both and refuses to
let them disagree.

  * every #define in itemoffsets.h matches reloc_data.us.h
  * itemoffsets.h defines nothing reloc_data.us.h has not got (a name the
    decomp never declares cannot be an offset into its files)
  * every `llITCommonData*` identifier used as code in src/dc/it*.c and in
    the host test is defined by itemoffsets.h -- the other half of the
    same guarantee: a use the header forgot would otherwise fall back to
    an implicit `int` declaration of 0, which is exactly the silent wrong
    read this file exists to prevent
"""
import os
import re
import sys

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
RELOC_H = os.path.join(REPO_ROOT, "src", "dc", "decomp", "reloc_data.us.h")
OFFSETS_H = os.path.join(REPO_ROOT, "src", "dc", "itemoffsets.h")
GAME = os.path.join(REPO_ROOT, "src", "game", "ssb64")
USERS = [os.path.join(GAME, "hosttest_ft.c")] + [
    os.path.join(GAME, "hosttest", n)
    for n in sorted(os.listdir(os.path.join(GAME, "hosttest")))
    if n.endswith(".c")
] + [
    os.path.join(REPO_ROOT, "src", "dc", n)
    for n in sorted(os.listdir(os.path.join(REPO_ROOT, "src", "dc")))
    if n.startswith("it") and n.endswith(".c")
]

DECL = re.compile(r'^extern int (llITCommonData[A-Za-z]+); // (0x[0-9a-f]+)$', re.M)
DEF = re.compile(r'^#define (llITCommonData[A-Za-z]+) +0x[0-9a-f]+$', re.M)
IDENT = re.compile(r'\b(llITCommonData[A-Za-z]+)\b')


def check_model_relocs():
    """The pack carries ITCommonObject's whole intern reloc chain.

    The file's own pointers -- a DisplayList naming a sub-list, a
    MatAnimJoint script naming the next script -- are a reloc chain the
    N64's lbRelocLoadAndRelocFile walks at load, writing an address into
    every site. The exporter used to drop the chain, so those words
    stayed ROM reloc descriptors ({u16 next, u16 target/4}), and the one
    the game read as a pointer first was MatAnimJoint BombHeiWalk's:
    gcParseMObjMatAnimJoint parses in a `while (anim_wait <= 0)` loop,
    so a Bob-omb that walked far enough hung the game thread.

    Nothing the port compiles could have caught that -- the pack is
    data, the loop is the decomp's, and the failure needed a four-minute
    match on the target to reach. So this is the oracle: every link of
    the ROM's chain is in the pack's table, at the right target, and the
    table has nothing the chain has not got.
    """
    import struct
    sys.path[:0] = [os.path.join(REPO_ROOT, "tools", d)
                    for d in ("lib", "export", "check")]
    import ssb_logicexport as L          # noqa: E402
    import ssb_itemexport as I           # noqa: E402

    pack_path = os.path.join(REPO_ROOT, "src", "game", "ssb64", "romdisk",
                             "itcommon.itp")
    if not os.path.exists(pack_path):
        sys.exit("itoffset_check: %s has not been built"
                 % os.path.relpath(pack_path, REPO_ROOT))
    blob = open(pack_path, "rb").read()
    if blob[:4] != b"ITCD":
        sys.exit("itoffset_check: %s is not an ITCD pack" % pack_path)

    ver, = struct.unpack_from("<I", blob, 4)
    off_models, = struct.unpack_from("<I", blob, 28)
    off_ifix, nifix = struct.unpack_from("<2I", blob, 72)
    if ver != 2:
        sys.exit("itoffset_check: itcommon.itp is version %d, and the model "
                 "reloc table arrived in version 2" % ver)

    have = {}
    for i in range(nifix):
        site, target = struct.unpack_from("<2I", blob, off_ifix + i * 8)
        have[site - off_models] = target - off_models

    rom = open(os.path.join(REPO_ROOT, "baserom.z64"), "rb").read()
    _fobj, _e, intern, _os, _oi = L.file_info(rom, I.ITCOMMONOBJECT)

    missing = sorted(s for s in intern if s not in have)
    wrong = sorted(s for s in intern if have.get(s, intern[s]) != intern[s])
    extra = sorted(s for s in have if s not in intern)
    if missing or wrong or extra:
        sys.exit("itoffset_check: itcommon.itp's model reloc table is not "
                 "ITCommonObject's chain: %d missing, %d at the wrong "
                 "target, %d the chain has not got%s"
                 % (len(missing), len(wrong), len(extra),
                    ("\n  first missing site 0x%05X" % missing[0])
                    if missing else ""))
    print("itoffset_check: itcommon.itp carries all %d of ITCommonObject's "
          "internal relocations" % len(intern))


def main():
    truth = dict(DECL.findall(open(RELOC_H).read()))
    if not truth:
        sys.exit("itoffset_check: no llITCommonData* declarations in %s"
                 % os.path.relpath(RELOC_H, REPO_ROOT))

    src = open(OFFSETS_H).read()
    defined = {}
    for m in re.finditer(r'^#define (llITCommonData[A-Za-z]+) +(0x[0-9a-f]+)$',
                         src, re.M):
        defined[m.group(1)] = m.group(2)

    bad = []
    for name, val in sorted(defined.items()):
        if name not in truth:
            bad.append("%s: defined here, not declared in reloc_data.us.h" % name)
        elif truth[name] != val:
            bad.append("%s: itemoffsets.h says %s, the decomp says %s"
                       % (name, val, truth[name]))
    if bad:
        sys.exit("itoffset_check: %d disagreement(s):\n  %s"
                 % (len(bad), "\n  ".join(bad)))

    # every use, in code rather than prose -- comments wrap the hyphenated
    # names across lines and would report fragments
    used = set()
    for path in USERS:
        for line in open(path):
            t = line.strip()
            if t.startswith(("*", "/*", "//")):
                continue
            used |= set(IDENT.findall(line))

    undeclared = sorted(used - set(defined))
    if undeclared:
        sys.exit("itoffset_check: %d llITCommonData* used but not defined:\n  %s"
                 % (len(undeclared), "\n  ".join(undeclared)))

    check_model_relocs()

    names = DECL.findall(src)
    print("itoffset_check: %d offsets match the decomp's own table, and the "
          "%d the port uses are all in it" % (len(defined), len(used)))
    if len(defined) < len(truth):
        print("itoffset_check: note -- the decomp names %d and this port uses "
              "%d; the rest arrive with the roster that reads them"
              % (len(truth), len(defined)))


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""The port's particle interpreter against the decomp's, four ways.

src/dc/lbparticle.c is lb/lbparticle.c with one function changed and one
function's macros replaced. That is a 2,900-line copy, and a copy is only
worth what checks it, so this checks it four times over.

  text     every line the port took is the decomp's line, character for
           character, in four contiguous runs that together are the whole
           file: the data and statics, everything up to
           lbParticleReadFloatBigEnd, everything from there to
           lbParticleStructFuncRun, and everything from the renderer to
           the end. Anything edited, reflowed, reordered or dropped shows
           up here with a line number. Between the runs the copy may have
           comments, and in one place two lines: the ones that install
           src/dc/lbpdraw.h's model of the RDP ahead of the renderer.

  run      the same driver (tools/check/lbparticle_oracle.c) built twice, once
           over each file, replaying all 119 scripts of the exported
           efcommon bank for a fixed number of frames from a fixed random
           seed, and the two traces compared byte for byte. The decomp's
           side is built from a copy with the port's own divergence
           pasted into it -- taken out of src/dc/lbparticle.c, not
           written again here -- so the one place the two files are
           meant to differ is not the whole of what this leg reports. A record per
           live particle per frame: position, velocity, the bytecode
           cursor and its loop and return pointers, lifetime, timers, both
           colours and both colour targets, size and size target, and how
           many structs the bank has taken. Children a script spawns are
           in the walk, so a MakeChild or a MakeGenerator opcode that
           behaved differently would show up as a different list.

  draw     lbParticleDrawTextures, which the text leg says is the
           decomp's line for line and which therefore needs nothing said
           about its arithmetic -- but which ends in eighteen GBI macros
           that src/dc/lbpdraw.h redefines, and nothing above can see
           those. So the same driver runs the renderer over the same
           scene in both builds: the decomp's writes the F3DEX2 its own
           gbi.h builds, the port's writes a record per rectangle, and
           this decodes the first into the second and compares every
           field. The rectangle itself goes back through libultra's
           gSPScisTextureRectangle on the way, because that macro's
           clamp is the one piece of the emission the port does not
           carry (the PVR clips the quad itself).

  bigend   lbParticleReadFloatBigEnd, at every offset of the script
           bank, against Python's own struct.unpack(">f") of the same
           four bytes. This is the leg the other two cannot be: both
           builds run on a little-endian machine, so a reader that
           reverses its bytes reverses them the same way twice, and text
           and run agree happily on a number neither the N64 nor the
           Dreamcast would ever have computed. That is what an unmodified copy would do -- see the DIVERGES comment in the port's copy.

The run leg is what survives a deliberate divergence: when a line of the
copy has to change, the text check is told about it and the run check is
what still says the change was harmless. One line has changed, and it is
the reader's.

It also prints a digest per script, which is what the target is checked
against: src/dc/db.c's -DDB_PARTICLE_SCRIPT runs one script on the
Dreamcast and logs the same sum, so the SH-4's floats can be held to the
host's over the same data.

Usage: python3 tools/check/lbparticle_check.py [--frames N] [--seed N]
                                         [--script N] [-v]
"""
import argparse
import os
import struct
import subprocess
import sys
import tempfile

sys.path[:0] = [os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), d)
                for d in ("lib", "export", "check")]
import ssb_particleexport as PE     # noqa: E402
import ssb_sinexport as SE          # noqa: E402
import ssb_trigexport as TE         # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
SRC = os.path.join(ROOT, "src", "dc")
DECOMP = os.environ.get("SSB_DECOMP_DIR", "/opt/ssb-decomp-re")
if not os.path.isdir(DECOMP):
    DECOMP = os.path.join(ROOT, "ssb-decomp-re")

PORT_FILE = os.path.join(SRC, "lbparticle.c")

# The four runs of lb/lbparticle.c the port took verbatim, 1-based and
# inclusive, and the one function it rewrote. The
# renderer is inside the fourth run: lbParticleDrawTextures is the
# decomp's to the character too, and what the port changed about it is
# the macros it ends in, which live in src/dc/lbpdraw.h and are pulled in
# by the two lines MODEL_LINES names.
# tools/check/lbparticle_check.py is the only place these numbers are written
# down twice; src/dc/lbparticle.c says the same in prose.
RUNS = [(6, 97), (99, 584), (598, 1449), (1450, 2954)]
DIVERGE = (585, 597)    # lbParticleReadFloatBigEnd

# What the port is allowed to have between run 3 and run 4 -- which is
# to say between lbParticleStructFuncRun and the renderer -- beside
# comments and blank lines.
MODEL_LINES = ["#define LBPDRAW_MODEL 1", '#include "lbpdraw.h"']

# The two lines that bracket the divergence. Both are the decomp's own
# and both are still in the copy, which is what lets the patched decomp
# source below be cut and pasted rather than described.
DIV_OPEN = "// 0x800CEBC0 - WARNING: Big Endian only!"
DIV_CLOSE = "// 0x800CEBF8"
# The line the bug was.
DIV_GONE = "\t*f = *(f32*)bytes;"

DECOMP_INC = ["-I", os.path.join(SRC, "decomp"),
              "-I", os.path.join(DECOMP, "src"),
              "-idirafter", os.path.join(DECOMP, "include"),
              "-D_LANGUAGE_C", "-DF3DEX_GBI_2", "-DREGION_US"]
DECOMP_WARN = ["-Wno-int-conversion", "-Wno-incompatible-pointer-types",
               "-Wno-unused-variable", "-Wno-unused-but-set-variable",
               "-Wno-parentheses", "-Wno-maybe-uninitialized",
               "-Wno-strict-aliasing"]
# The object system, the scene heap under it, and the two files the
# interpreter reaches outside itself: sys/utils.c's random and arctan,
# sys/matrix.c's three transform builders (lbParticleGetPosVelDObj).
DECOMP_SRCS = ["sys/objman.c", "sys/objhelper.c", "sys/objscript.c",
               "sys/objanim.c", "sys/interp.c", "sys/malloc.c",
               "sys/utils.c", "sys/vector.c", "sys/matrix.c",
               "libultra/gu/mtxutil.c", "libultra/gu/mtxcatf.c",
               "libultra/gu/normalize.c"]
# gusinf.c and gucosf.c are libultra's __sinf and __cosf,
# written from the decomp by tools/export/ssb_trigexport.py into the
# build directory. They are what the --wrap counter below wraps, and they
# are why the count is now the same function on both machines.
PORT_SRCS = ["taskman.c", "sysshim.c", "objdisplay.c", "mpshim.c",
             "lbpdraw.c"]

REC = struct.Struct("=4i10H8B10f16B")


def check_text() -> int:
    """Every line of the copy is the decomp's line."""
    with open(os.path.join(DECOMP, "src", "lb", "lbparticle.c")) as f:
        src = f.read().splitlines()
    with open(PORT_FILE) as f:
        port = f.read().splitlines()

    at = 0
    for lo, hi in RUNS:
        want = src[lo - 1:hi]
        found = -1
        for i in range(at, len(port) - len(want) + 1):
            if port[i:i + len(want)] == want:
                found = i
                break
        if found < 0:
            # Say where it first stops matching, at the best start we can
            # find, rather than only that it does not.
            best, best_hits = at, -1
            for i in range(at, len(port) - len(want) + 1):
                hits = sum(1 for a, b in zip(port[i:i + len(want)], want) if a == b)
                if hits > best_hits:
                    best, best_hits = i, hits
            for k, (a, b) in enumerate(zip(port[best:best + len(want)], want)):
                if a != b:
                    print(f"FAIL text: lb/lbparticle.c:{lo + k} is")
                    print(f"       {b!r}")
                    print(f"     src/dc/lbparticle.c:{best + k + 1} is")
                    print(f"       {a!r}")
                    break
            else:
                print(f"FAIL text: lb/lbparticle.c:{lo}-{hi} is not in the copy")
            return 1
        print(f"text: lb/lbparticle.c:{lo}-{hi} is src/dc/lbparticle.c:"
              f"{found + 1}-{found + len(want)}")
        at = found + len(want)

    # And nothing else is between the runs. The subsequence match above
    # says each run is contiguous in the copy; this says what the copy
    # put between them, which is one thing in one
    # place -- the two lines that install src/dc/lbpdraw.h's model of the
    # RDP, ahead of the renderer. Comments and blank lines are the
    # copy's own business.
    between_runs = []
    at = 0
    for lo, hi in RUNS:
        want = src[lo - 1:hi]
        i = port.index(want[0], at)
        while port[i:i + len(want)] != want:
            i = port.index(want[0], i + 1)
        between_runs.append(port[at:i])
        at = i + len(want)
    # The chunk before the renderer is the one this is about; what comes
    # before the first run is the file's own includes and what sits
    # between runs 2 and 3 is lbParticleReadFloatBigEnd, checked below.
    in_comment = False
    code = []
    for ln in between_runs[3]:
        t = ln.strip()
        if in_comment:
            in_comment = "*/" not in t
            continue
        if not t or t.startswith("//"):
            continue
        if t.startswith("/*"):
            in_comment = "*/" not in t
            continue
        code.append(t)
    if code != MODEL_LINES:
        print(f"FAIL text: what the copy has before the renderer is {code}, "
              f"not {MODEL_LINES}")
        return 1
    print(f"text: and before it, {' and '.join(MODEL_LINES)} -- "
          f"the RDP the renderer draws through, and nothing else")

    # And the divergence really is a divergence: the line it exists to
    # get rid of is gone. The rest of lbParticleReadFloatBigEnd shares
    # its signature and its braces with the copy and says nothing.
    if DIV_GONE in port:
        print(f"FAIL text: {DIV_GONE!r} is still in the copy -- "
              f"lb/lbparticle.c:{DIVERGE[0]}-{DIVERGE[1]} is the one "
              f"divergence and it has not been made")
        return 1
    print(f"text: lb/lbparticle.c:{DIVERGE[0]}-{DIVERGE[1]} "
          f"lbParticleReadFloatBigEnd is the only divergence "
          f"({DIVERGE[1] - DIVERGE[0] + 1} lines)")
    return 0


def between(lines, opener, closer, what):
    """The lines strictly between two unique marker lines."""
    for marker in (opener, closer):
        if lines.count(marker) != 1:
            raise SystemExit(f"{what}: {marker!r} appears "
                             f"{lines.count(marker)} times, wanted once")
    lo = lines.index(opener) + 1
    hi = lines.index(closer)
    if hi <= lo:
        raise SystemExit(f"{what}: {closer!r} comes before {opener!r}")
    return lines[lo:hi]


def patched_decomp(tmpdir):
    """The decomp's file with the port's divergence pasted into it.

    The run leg compares two traces byte for byte, and the port's copy
    now differs from the decomp's file in one function on purpose. Left
    alone, that one function would drown the leg: every float in every
    record would differ and nothing else could be seen. So the decomp's
    side gets the same reader -- lifted out of src/dc/lbparticle.c
    between the two markers the text leg just checked, never retyped --
    and what the leg reports is the other 2,200 lines.

    Which is to say this leg cannot see the divergence at all. Nothing
    can, from inside a pair of little-endian processes; that is what the
    bigend leg is for."""
    with open(os.path.join(DECOMP, "src", "lb", "lbparticle.c")) as f:
        src = f.read().splitlines(keepends=True)
    with open(PORT_FILE) as f:
        port = f.read().splitlines(keepends=True)

    strip = lambda ls: [l.rstrip("\n") for l in ls]      # noqa: E731
    want = between(strip(src), DIV_OPEN, DIV_CLOSE, "lb/lbparticle.c")
    if want != strip(src[DIVERGE[0] - 1:DIVERGE[1]]) + [""]:
        raise SystemExit(f"lb/lbparticle.c: {DIV_OPEN!r}..{DIV_CLOSE!r} is "
                         f"not lines {DIVERGE[0]}-{DIVERGE[1]} any more")

    lo = strip(src).index(DIV_OPEN) + 1
    hi = strip(src).index(DIV_CLOSE)
    plo = strip(port).index(DIV_OPEN) + 1
    phi = strip(port).index(DIV_CLOSE)

    out = os.path.join(tmpdir, "lbparticle_decomp.c")
    with open(out, "w") as f:
        f.writelines(src[:lo] + port[plo:phi] + src[hi:])
    print(f"run: the decomp's file with src/dc/lbparticle.c:{plo + 1}-{phi} "
          f"pasted over lb/lbparticle.c:{lo + 1}-{hi}")
    return out


def build(tmpdir, which, sintable):
    """The driver over one of the two files."""
    exe = os.path.join(tmpdir, "lbparticle_oracle_" + which)
    under = PORT_FILE if which == "port" else patched_decomp(tmpdir)
    cmd = ["cc", "-O2", "-std=gnu99", "-ffp-contract=off", "-DFT_HOSTTEST", "-DSSB_NO_DRAW",
           # this build links no gm/gmcollision.c and no fighter, so
           # src/dc/objdisplay.c's two joint-following matrix kinds (0x4F,
           # 0x50) compile to "no such kind" here
           "-DGC_NO_GMCOLLISION",
           # what the target build force-feeds the decomp's sources
           "-include", "sys/debug.h", "-o", exe,
           os.path.join(HERE, "lbparticle_oracle.c"), under, sintable]
    if which == "decomp":
        # The decomp's file still carries lbParticleDrawTextures, which is
        # never called here but has to link.
        cmd.append("-DORACLE_DECOMP")
    cmd += [os.path.join(SRC, s) for s in PORT_SRCS]
    cmd += TE.write_all(DECOMP, tmpdir)
    cmd += [os.path.join(DECOMP, "src", s) for s in DECOMP_SRCS]
    cmd += ["-I", SRC, "-I", os.path.join(ROOT, "src", "game", "ssb64", "hoststubs")]
    cmd += DECOMP_INC + DECOMP_WARN + ["-lm",
                                      "-Wl,--wrap=__sinf", "-Wl,--wrap=__cosf"]
    subprocess.run(cmd, check=True)
    return exe


def check_bigend(exe, banks) -> int:
    """lbParticleReadFloatBigEnd against Python, over the whole bank.

    The function's name is its specification and the decomp's own comment
    above it is the warning: it reads four bytes as a big-endian float,
    which the N64 got for free and nothing else does. So the oracle is
    Python's `>f` -- not the decomp's file, which on this machine reads
    exactly as wrongly as the copy did and would agree with it forever.

    Compared as bits, not as floats: some of the bank's bytes make a NaN
    whichever way round they are read, and a NaN is never equal to
    itself. Every offset is swept, header words and all -- most of them
    are not floats, but the reader does not know that, and the only
    question here is whether four bytes come back big-endian.

    All nine banks, not just the one the run leg replays: the reader is
    the same reader for every one of them and a sweep costs a fraction of
    a second."""
    total = revd = subn = 0

    for name, scb_path in banks:
        with open(scb_path, "rb") as f:
            scb = f.read()
        r = subprocess.run([exe, "--readfloat", scb_path],
                           check=True, stdout=subprocess.PIPE)
        n = len(scb) - 3
        if len(r.stdout) != n * 4:
            print(f"FAIL bigend: {name}: {len(r.stdout)} bytes back for "
                  f"{n} offsets")
            return 1

        bad = 0
        for off in range(n):
            got = struct.unpack_from("<I", r.stdout, off * 4)[0]
            want = struct.unpack_from(">I", scb, off)[0]
            if got != want:
                if bad == 0:
                    print(f"FAIL bigend: {name} offset {off:#x} is "
                          f"{scb[off:off + 4].hex()}, which is {want:#010x} "
                          f"big-endian; the reader returned {got:#010x}")
                bad += 1
        if bad:
            print(f"FAIL bigend: {name}: {bad} of {n} offsets")
            return 1

        # And what the copy did before, as a number about the
        # banks rather than a claim about a machine: the same sweep read
        # the other way round.
        total += n
        revd += sum(1 for off in range(n)
                    if struct.unpack_from("<I", scb, off)[0] !=
                       struct.unpack_from(">I", scb, off)[0])
        subn += sum(1 for off in range(n)
                    if (struct.unpack_from("<I", scb, off)[0] & 0x7F800000) == 0
                    and (struct.unpack_from("<I", scb, off)[0] & 0x007FFFFF) != 0)

    print(f"bigend: {total} offsets over {len(banks)} script banks through "
          f"lbParticleReadFloatBigEnd, every one of them the big-endian "
          f"float of its own four bytes")
    print(f"bigend: read the other way round -- lb/lbparticle.c:"
          f"{DIVERGE[0]}'s own arithmetic on a little-endian machine -- "
          f"{revd} of those {total} are a different number and {subn} are "
          f"subnormal, which is where a round float's zero low bytes go "
          f"when they become an exponent")
    return 0


# ---- draw ------------------------------------------------------------
#
# What the model cannot check about itself. The renderer is the decomp's
# line for line (the text leg), so its arithmetic needs no second
# opinion; what it ends in does. src/dc/lbpdraw.h redefines eighteen GBI
# macros, and the question this leg asks is whether those eighteen read
# their arguments the way libultra reads them -- so the decomp's build
# runs the same function through the real gbi.h into a real display list,
# the port's build runs it through the model into a record per rectangle,
# and this decodes the first into the second.

DRAW_REC = struct.Struct("=4f18i12B")
GFX_SIZE = 16

# What the port's record holds, in order, past the four floats.
DRAW_INTS = ["s", "t", "dsdx", "dtdy", "z", "image", "palette",
             "fmt", "siz", "width", "height", "cms", "cmt", "masks", "maskt",
             "combine", "ac", "tlut"]

G_TEXRECT = 0xE4
G_RDPHALF_1 = 0xE1
G_RDPHALF_2 = 0xF1
G_SETOTHERMODE_H = 0xE3
G_SETOTHERMODE_L = 0xE2
G_SETTIMG = 0xFD
G_SETCOMBINE = 0xFC
G_SETENVCOLOR = 0xFB
G_SETPRIMCOLOR = 0xFA
G_SETBLENDCOLOR = 0xF9
G_SETTILE = 0xF5
G_SETTILESIZE = 0xF2
G_LOADTLUT = 0xF0
G_SETPRIMDEPTH = 0xEE

# lb/lbparticle.c:2043-2069's three, by the RGB cycle's first source.
G_CCMUX_TEXEL0, G_CCMUX_PRIMITIVE, G_CCMUX_NOISE = 1, 3, 7
COMBINE_OF = {G_CCMUX_TEXEL0: 0, G_CCMUX_PRIMITIVE: 1, G_CCMUX_NOISE: 2}


def s16(v):
    """(s16)(f32) as the compiler does it: to int, then the low half."""
    v = int(v)                       # C truncates toward zero
    v &= 0xFFFF
    return v - 0x10000 if v & 0x8000 else v


def texrect_words(r):
    """libultra's gSPScisTextureRectangle over the port's own numbers.

    The macro is the one piece of the emission the port does not carry:
    it clamps the rectangle's upper-left at zero and walks s and t
    forward by however much it clamped, because the RDP's texrect
    coordinates are unsigned. The PVR clips the quad itself and is given
    the rectangle the arithmetic produced, so this is where the two are
    reconciled -- on the port's floats, by the decomp's own macro."""
    xl, yl, xh, yh = (s16(r["xl"]), s16(r["yl"]), s16(r["xh"]), s16(r["yh"]))
    dsdx, dtdy = s16(r["dsdx"]), s16(r["dtdy"])

    def walk(lo, d, v):
        if lo >= 0:
            return v
        step = (lo * d) >> 7
        return v - (max(step, 0) if d < 0 else min(step, 0))

    return {
        "xl": max(xl, 0) & 0xFFF, "yl": max(yl, 0) & 0xFFF,
        "xh": max(xh, 0) & 0xFFF, "yh": max(yh, 0) & 0xFFF,
        "s": walk(xl, dsdx, r["s"]) & 0xFFFF,
        "t": walk(yl, dtdy, r["t"]) & 0xFFFF,
        "dsdx": dsdx & 0xFFFF, "dtdy": dtdy & 0xFFFF,
    }


def decode_state():
    st = {k: 0 for k in DRAW_INTS}
    st.update({"prim": [0, 0, 0, 0], "env": [0, 0, 0, 0], "blend_alpha": 0,
               "dither_c": 0, "dither_a": 0, "timg": 0})
    return st


def decode_dl(cmds, st):
    """One call's display list into one record per G_TEXRECT.

    Only the commands lbParticleDrawTextures emits, and only the fields
    it chose: the RDP state each rectangle inherits is whatever the last
    command that set it said, which is the same thing src/dc/lbpdraw.c's
    model does and the reason the model is a struct that stands rather
    than a struct that is filled. `st` is carried from call to call for
    the same reason the model's is a static -- the RDP does not forget
    its colours between frames, and the renderer counts on that: it
    emits a texture load or a TLUT only when the one it wants is not the
    one already there."""
    timg = st["timg"]
    out = []
    i = 0
    while i < len(cmds):
        w0, w1 = cmds[i]
        i += 1
        cmd = w0 >> 24
        if cmd == G_SETTIMG:
            timg = w1
        elif cmd == G_LOADTLUT:
            st["palette"] = timg
        elif cmd == G_SETTILE and ((w1 >> 24) & 7) == 0:   # G_TX_RENDERTILE
            st["image"] = timg
            st["fmt"] = (w0 >> 21) & 7
            st["siz"] = (w0 >> 19) & 3
            st["cmt"] = (w1 >> 18) & 3
            st["maskt"] = (w1 >> 14) & 0xF
            st["cms"] = (w1 >> 8) & 3
            st["masks"] = (w1 >> 4) & 0xF
        elif cmd == G_SETTILESIZE and ((w1 >> 24) & 7) == 0:
            st["width"] = (((w1 >> 12) & 0xFFF) >> 2) + 1
            st["height"] = ((w1 & 0xFFF) >> 2) + 1
        elif cmd == G_SETPRIMCOLOR:
            st["prim"] = [(w1 >> 24) & 0xFF, (w1 >> 16) & 0xFF,
                          (w1 >> 8) & 0xFF, w1 & 0xFF]
        elif cmd == G_SETENVCOLOR:
            st["env"] = [(w1 >> 24) & 0xFF, (w1 >> 16) & 0xFF,
                         (w1 >> 8) & 0xFF, w1 & 0xFF]
        elif cmd == G_SETBLENDCOLOR:
            st["blend_alpha"] = w1 & 0xFF
        elif cmd == G_SETCOMBINE:
            a0 = (w0 >> 20) & 0xF
            if a0 not in COMBINE_OF:
                raise SystemExit(f"draw: a fourth combiner, RGB source {a0}")
            st["combine"] = COMBINE_OF[a0]
        elif cmd == G_SETPRIMDEPTH:
            st["z"] = (w1 >> 16) & 0xFFFF
        elif cmd in (G_SETOTHERMODE_H, G_SETOTHERMODE_L):
            length = (w0 & 0xFF) + 1
            shift = 32 - ((w0 >> 8) & 0xFF) - length
            if cmd == G_SETOTHERMODE_L and shift == 0:      # ALPHACOMPARE
                st["ac"] = w1
            elif cmd == G_SETOTHERMODE_H and shift == 14:   # TEXTLUT
                st["tlut"] = w1
            elif cmd == G_SETOTHERMODE_H and shift == 6:    # RGBDITHER
                st["dither_c"] = w1
            elif cmd == G_SETOTHERMODE_H and shift == 4:    # ALPHADITHER
                st["dither_a"] = w1
        elif cmd == G_TEXRECT:
            if (cmds[i][0] >> 24) != G_RDPHALF_1 or \
               (cmds[i + 1][0] >> 24) != G_RDPHALF_2:
                raise SystemExit("draw: G_TEXRECT without its two halves")
            rec = {k: v for k, v in st.items() if k != "timg"}
            rec["prim"] = list(st["prim"])
            rec["env"] = list(st["env"])
            rec["xh"] = (w0 >> 12) & 0xFFF
            rec["yh"] = w0 & 0xFFF
            rec["xl"] = (w1 >> 12) & 0xFFF
            rec["yl"] = w1 & 0xFFF
            rec["s"] = (cmds[i][1] >> 16) & 0xFFFF
            rec["t"] = cmds[i][1] & 0xFFFF
            rec["dsdx"] = (cmds[i + 1][1] >> 16) & 0xFFFF
            rec["dtdy"] = cmds[i + 1][1] & 0xFFFF
            i += 2
            out.append(rec)
    st["timg"] = timg
    return out


def split_calls(blob, parse):
    """The stream both builds write: a count, then that many of a thing."""
    calls = []
    at = 0
    while at < len(blob):
        (n,) = struct.unpack_from("=I", blob, at)
        at += 4
        at = parse(blob, at, n, calls)
    return calls


def port_calls(blob):
    def one(blob, at, n, calls):
        recs = []
        for _ in range(n):
            v = DRAW_REC.unpack_from(blob, at)
            at += DRAW_REC.size
            r = dict(zip(("xl", "yl", "xh", "yh"), v[0:4]))
            r.update(zip(DRAW_INTS, v[4:22]))
            r["image"] &= 0xFFFFFFFF
            r["palette"] &= 0xFFFFFFFF
            r["prim"] = list(v[22:26])
            r["env"] = list(v[26:30])
            r["blend_alpha"], r["dither_c"], r["dither_a"] = v[30:33]
            recs.append(r)
        calls.append(recs)
        return at
    return split_calls(blob, one)


def decomp_calls(blob):
    st = decode_state()

    def one(blob, at, n, calls):
        # n is the count of Gfx. A Gfx is a union whose widest member
        # uses `unsigned long`, so on this 64-bit host it is sixteen
        # bytes with the two command words at its front and eight bytes
        # of padding behind them -- not the N64's eight.
        stride = GFX_SIZE
        cmds = [struct.unpack_from("=II", blob, at + k * stride)
                for k in range(n)]
        calls.append(decode_dl(cmds, st))
        return at + n * stride
    return split_calls(blob, one)


def canon_addrs(calls):
    """Addresses by first appearance, because they are two processes.

    An image and a palette reach both sides as the address the bank sits
    at, and the bank is slurped into a malloc of its own in each of the
    two builds. So the numbers cannot agree and the sequence can: this
    replaces each address by when it was first seen, which says the same
    thing -- that the two runs picked the same frame of the same texture
    at the same moment, and reloaded it in the same places."""
    seen = {0: 0}
    for recs in calls:
        for r in recs:
            for f in ("image", "palette"):
                r[f] = seen.setdefault(r[f], len(seen))
    return calls


def check_draw(port_blob, decomp_blob) -> int:
    port = canon_addrs(port_calls(port_blob))
    dec = canon_addrs(decomp_calls(decomp_blob))

    if len(port) != len(dec):
        print(f"FAIL draw: {len(port)} calls from the port, {len(dec)} "
              f"from the decomp")
        return 1

    # Every field but three. The two dither modes and the alpha compare
    # are per-call rather than per-rectangle and are checked as such
    # below; zcmp is not in the display list at all -- it is the render
    # mode ef/efdisplay.c sets around the call, and this driver calls the
    # renderer directly.
    fields = (["xl", "yl", "xh", "yh", "s", "t", "dsdx", "dtdy"] +
              [f for f in DRAW_INTS if f not in ("s", "t", "dsdx", "dtdy")] +
              ["prim", "env", "blend_alpha", "dither_c", "dither_a"])

    rects = wide = 0
    cover = {"combine": {}, "cms": {}, "cmt": {}, "siz": {}, "fmt": {},
             "ac": {}}
    for ci, (pc, dc) in enumerate(zip(port, dec)):
        if len(pc) != len(dc):
            print(f"FAIL draw: call {ci}: {len(pc)} rectangles from the "
                  f"port, {len(dc)} from the decomp")
            return 1
        for ri, (a, b) in enumerate(zip(pc, dc)):
            got = dict(a)
            got.update(texrect_words(a))
            if any(abs(a[k]) > 32767.0 for k in ("xl", "yl", "xh", "yh")):
                wide += 1
            for f in fields:
                if got[f] != b[f]:
                    print(f"FAIL draw: call {ci} rectangle {ri} field {f}: "
                          f"the port says {got[f]!r}, the display list the "
                          f"decomp built says {b[f]!r}")
                    print(f"     port   {a}")
                    print(f"     decomp {b}")
                    return 1
            for k in cover:
                cover[k][b[k]] = cover[k].get(b[k], 0) + 1
            rects += 1
    if rects == 0:
        print("FAIL draw: the scene drew nothing")
        return 1
    print(f"draw: {rects} texture rectangles over {len(port)} calls -- every "
          f"field of every one against the F3DEX2 the decomp's own build "
          f"emitted for it: the rectangle through libultra's own "
          f"gSPScisTextureRectangle, the tile's cms/cmt/masks/maskt, the "
          f"format, size and extent, the image and palette addresses, the "
          f"combiner, the primitive depth, both colours, the blend colour, "
          f"the alpha compare and both dither modes")
    if wide:
        print(f"draw: {wide} of them reach past a signed 16-bit screen "
              f"coordinate, where the macro's own cast is undefined")
    # What the bank's own scripts happened to ask for, so that a field
    # every rectangle agreed on because it never varied says so.
    names = {"combine": ["MODULATEIA_PRIM", "env lerp", "noise"],
             "siz": ["4b", "8b", "16b", "32b"],
             "fmt": ["RGBA", "YUV", "CI", "IA", "I"],
             "cms": ["wrap", "mirror", "clamp", "clamp|mirror"],
             "cmt": ["wrap", "mirror", "clamp", "clamp|mirror"],
             "ac": {0: "none", 1: "threshold", 3: "dither"}}
    for k in ("combine", "fmt", "siz", "cms", "cmt", "ac"):
        parts = []
        for v in sorted(cover[k]):
            nm = names[k][v] if v < len(names[k]) else str(v)
            parts.append(f"{nm} {cover[k][v]}")
        print(f"draw:   {k}: {', '.join(parts)}")
    return 0


def digest(records, script_id):
    """A number a serial log can carry: the trace of one script, summed."""
    total = 0
    for r in records:
        if r[0] != script_id:
            continue
        total = (total + sum(struct.unpack("=%dI" % (REC.size // 4),
                                           REC.pack(*r)))) & 0xFFFFFFFF
    return total


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--frames", type=int, default=120)
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--script", type=int, action="append",
                    help="print a digest for this script id (repeatable)")
    ap.add_argument("--dump", action="store_true",
                    help="every record of every script as its bytes, the "
                    "way src/dc/db.c's -DDB_PARTICLE_DUMP prints them")
    ap.add_argument("--from", dest="first", type=int, default=0,
                    metavar="N", help="which eight frames of --script print "
                    "their particles' bytes (src/dc/db.c's "
                    "-DDB_PARTICLE_FROM)")
    ap.add_argument("-v", "--verbose", action="store_true")
    args = ap.parse_args()

    if not os.path.isdir(DECOMP):
        print(f"no decomp checkout at {DECOMP} -- skipping")
        return 0

    if check_text() != 0:
        return 1

    with tempfile.TemporaryDirectory() as tmp:
        rom_path = PE.ROM_DEFAULT
        if not os.path.exists(rom_path):
            print("no baserom -- the text check ran, the run check needs the bank")
            return 0
        with open(rom_path, "rb") as f:
            rom = f.read()
        scb = os.path.join(tmp, "efcommon.scb")
        txb = os.path.join(tmp, "efcommon.txb")
        for path, kind in ((scb, "scb"), (txb, "txb")):
            with open(path, "wb") as f:
                f.write(PE.export(rom, "efcommon", kind))
        # The run leg replays efcommon; the bigend leg sweeps every bank
        # the game has, because the reader is the same reader.
        banks = []
        for name in sorted(PE.BANKS):
            path = os.path.join(tmp, name + ".scb")
            with open(path, "wb") as f:
                f.write(PE.export(rom, name, "scb"))
            banks.append((name, path))
        sintable = os.path.join(tmp, "sintable.c")
        SE_table = SE.read_table(rom)
        with open(sintable, "w") as f:
            f.write(SE.emit_c(SE_table))

        out = {}
        drawn = {}
        for which in ("decomp", "port"):
            exe = build(tmp, which, sintable)
            if which == "port" and check_bigend(exe, banks) != 0:
                return 1
            r = subprocess.run(
                [exe, scb, txb, str(args.frames), str(args.seed)],
                check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            out[which] = r.stdout
            r = subprocess.run(
                [exe, "--draw", scb, txb, str(args.frames), str(args.seed)],
                check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            drawn[which] = r.stdout
            trig = {int(a): int(b) for a, b in
                    (ln.split()[1:] for ln in r.stderr.decode().splitlines()
                     if ln.startswith("trig "))}

    if len(out["port"]) != len(out["decomp"]):
        print(f"FAIL run: the port wrote {len(out['port'])} bytes of trace, "
              f"the decomp {len(out['decomp'])}")
        return 1
    if out["port"] != out["decomp"]:
        for i in range(0, len(out["port"]), REC.size):
            a = REC.unpack_from(out["port"], i)
            b = REC.unpack_from(out["decomp"], i)
            if a != b:
                print(f"FAIL run: script {b[0]} frame {b[1]} particle {b[2]}")
                print(f"     port   {a}")
                print(f"     decomp {b}")
                return 1
        return 1

    if check_draw(drawn["port"], drawn["decomp"]) != 0:
        return 1

    recs = [REC.unpack_from(out["port"], i)
            for i in range(0, len(out["port"]), REC.size)]
    scripts = sorted({r[0] for r in recs})
    print(f"run: {len(scripts)} scripts x {args.frames} frames, "
          f"{len(recs)} particle-frames, identical")

    # Seven scripts -- 103, 105 and 108-112 -- went
    # singular under this driver's start, and the reason written here was
    # that the vortex arm divides a velocity by a cosine
    # (lb/lbparticle.c:1364) and that a generator the game would have
    # aimed was being aimed at the angle that makes it zero. That was
    # half right and the wrong half mattered: the angles came out of
    # lbParticleReadFloatBigEnd, which was reading them backwards, so the
    # driver's start was not what put them there. With the bytecode's
    # floats read big-endian none of the 119 divides by zero. The check
    # stays, because the divide is still in the file and a bank the game
    # aims differently could still find it.
    bad = sorted({r[0] for r in recs
                  if any(v in (float("inf"), float("-inf")) or v != v
                         for v in r[22:32])})
    print(f"finite: {len(scripts) - len(bad)} of {len(scripts)} scripts stay "
          f"finite from this start" + (f"; {bad} do not" if bad else ""))

    warm = sorted(k for k, v in trig.items() if v)
    print(f"trig: {len(warm)} of {len(scripts)} scripts call __sinf or "
          f"__cosf. Only those scripts could differ on the Dreamcast, so they are "
          f"libultra's own (tools/export/ssb_trigexport.py), the same function on both "
          f"machines, so none of them should: {warm}")

    whole = 0
    for r in recs:
        whole = (whole + sum(struct.unpack("=%dI" % (REC.size // 4),
                                           REC.pack(*r)))) & 0xFFFFFFFF
    print(f"digest: the whole run, {args.frames} frames from seed "
          f"{args.seed}, is {whole:#010x} -- what src/dc/db.c's "
          f"-DDB_PARTICLE_TRACE prints on the Dreamcast")

    if args.verbose:
        for s in scripts:
            n = sum(1 for r in recs if r[0] == s)
            most = max((r[2] for r in recs if r[0] == s), default=-1) + 1
            print(f"  script {s:3d}: {n:5d} particle-frames, "
                  f"{most} at once, digest {digest(recs, s):#010x}")
    for s in (args.script or []):
        # The same walk src/dc/db.c's -DDB_PARTICLE_FIRST does, printed
        # the same way: the running digest for every frame, so the frame a
        # target disagrees on is one run, and every live particle's whole
        # record as its bytes for the eight frames from --from, so which
        # particle and which field of it is the next.
        acc = 0
        frames = {}
        for r in recs:
            if r[0] != s or r[1] >= args.frames:
                continue
            acc = (acc + sum(struct.unpack("=%dI" % (REC.size // 4),
                                           REC.pack(*r)))) & 0xFFFFFFFF
            fr = frames.setdefault(r[1], {"live": 0, "recs": []})
            fr["live"] += 1
            fr["recs"].append(r)
            fr["acc"] = acc
        for f in sorted(frames):
            fr = frames[f]
            print(f"  f{f} {fr['live']} live, digest {fr['acc']:#010x}")
            if not args.first <= f < args.first + 8:
                continue
            for i, r in enumerate(fr["recs"]):
                print(f"  s{s} f{f} i{i} {REC.pack(*r).hex()}")
        print(f"digest: script {s} over {args.frames} frames from seed "
              f"{args.seed} is {digest(recs, s):#010x}")

    if args.dump:
        # Every record of every script, in the order src/dc/db.c's
        # -DDB_PARTICLE_DUMP prints them, so the two machines' whole
        # disagreement is one diff rather than ten runs of bisection.
        seen = {}
        for r in recs:
            if r[1] >= args.frames:
                continue
            i = seen.get((r[0], r[1]), 0)
            seen[(r[0], r[1])] = i + 1
            print(f"  s{r[0]} f{r[1]} i{i} {REC.pack(*r).hex()}")

    print("lbparticle_check: the copy is the decomp's, and runs as it runs")
    return 0


if __name__ == "__main__":
    sys.exit(main())

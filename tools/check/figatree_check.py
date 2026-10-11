#!/usr/bin/env python3
"""Diff the target's Figatree interpreter against the Python reference.

A fighter animation is one script per joint. ssb_assets.Figatree is the
Python reading of the two functions that run one -- ftAnimParseDObjFigatree
followed by gcPlayDObjAnimJoint -- and this runs it against every animation
in the ROM, a frame at a time, comparing all ten joint tracks, against the
C interpreter the port runs:

  objanim.c   the game's own code -- ft/ftanim.c and sys/objanim.c compiled
              out of the decomp, driving real DObjs and AObjs through the
              port's scene heap. tools/check/objanim_oracle.c is the driver.

It is built twice. In *double* precision it must agree with the reference
exactly: same arithmetic, same order, so any difference at all is a porting
mistake. In the target's *single* precision it cannot, and the run reports
how far the two drift instead -- which is the number that says whether single
precision is good enough on the Dreamcast, and separates that question from
correctness.

Where the reference gives up -- on SetTranslateInterp, which it does not
implement, or on a script that reads past the end of its file -- the joint
stops being compared and is reported at the end, along with whether the C
side stopped too. A C interpreter that stops where the reference kept going
is a failure; one that keeps going where the reference stopped is not.

Thirty-five of the animations are not figatrees at all but AnimJoint scripts,
the 32-bit AObjEvent32 language (ssb_assets.AnimJoint), which the game parses
with gcParseDObjAnimJoint instead. They run through objanim.c -- that is
the decomp's own parser, unmodified -- against the AnimJoint reference.

The 1775 animations are checked in parallel. They share nothing but the
two binaries above -- one interpreter at two precisions -- which are
built once, before the pool forks, so the workers inherit them along with
the ROM; the report is assembled in the ROM's order however the workers
finish. Eight at a time by default, because two Master Hand animations
are most of the run whatever the pool size (see main()). --jobs N to
change it, --jobs 1 to run them one at a time.

Usage: python3 tools/check/figatree_check.py [--rom <rom.z64>] [--frames N]
                                       [--match <substring>] [--jobs N]
                                       [-v]
"""
import multiprocessing
import os
import re
import struct
import subprocess
import sys
import tempfile

sys.path[:0] = [os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), d)
                for d in ("lib", "export", "check")]
import ssb_extract           # noqa: E402
import ssb_assets as A       # noqa: E402
import ssb_meshexport as M   # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
SRC = os.path.join(ROOT, "src", "dc")
DECOMP = os.environ.get("SSB_DECOMP_DIR", "/opt/ssb-decomp-re")

# The reference computes in Python floats, i.e. doubles, so a double build has
# no excuse; the single-precision drift is measured, not bounded.
EXACT_TOL = 0.0
# Loud enough to catch a real divergence, loose enough for a Hermite whose
# intermediates run to a few hundred while the answer is around one -- and
# proportional, since one animation drops a joint in from -114,000 units and a
# float has seven digits wherever you put them.
DRIFT_ABS = 1e-2
DRIFT_REL = 1e-6

# The reference's refusals, for reporting. objanim_oracle.c reports 3 for
# the one condition it guards; 0 means it ran on where the reference did not.
ERRORS = {0: "runs on", 1: "SetTranslateInterp", 2: "runaway", 3: "overrun"}

# The decomp's own build of ft/ftanim.c and sys/*.c, warnings and all.
DECOMP_INC = ["-I", os.path.join(SRC, "decomp"),
              "-I", os.path.join(DECOMP, "src"),
              "-idirafter", os.path.join(DECOMP, "include"),
              "-D_LANGUAGE_C", "-DF3DEX_GBI_2", "-DREGION_US"]
DECOMP_WARN = ["-Wno-int-conversion", "-Wno-unused-variable",
               "-Wno-unused-but-set-variable", "-Wno-parentheses",
               "-Wno-maybe-uninitialized", "-Wno-strict-aliasing"]
DECOMP_SRCS = ["sys/objman.c", "sys/objhelper.c", "sys/objscript.c",
               "sys/objanim.c", "sys/interp.c", "sys/malloc.c",
               "ft/ftanim.c"]


def build_objanim(tmpdir, scalar):
    """The decomp's animation engine, ditto. -DSSB_F32_IS_F64 goes through
    the f32 typedef in src/dc/decomp/PR/ultratypes.h, so every scalar in the
    object system widens together and the structs stay self-consistent."""
    exe = os.path.join(tmpdir, "objanim_oracle_" + scalar)
    if not os.path.isdir(os.path.join(DECOMP, "src", "sys")):
        sys.exit("no decomp at %s -- set SSB_DECOMP_DIR" % DECOMP)
    cmd = ["cc", "-O2", "-std=gnu99", "-Wall", "-ffp-contract=off",
           "-DFT_HOSTTEST", "-DSSB_NO_DRAW",
           # no gm/gmcollision.c and no fighter in this link, so
           # src/dc/objdisplay.c's joint-following matrix kinds (0x4F,
           # 0x50) compile to "no such kind" here
           "-DGC_NO_GMCOLLISION",
           "-o", exe,
           os.path.join(HERE, "objanim_oracle.c"),
           os.path.join(SRC, "taskman.c"),
           os.path.join(SRC, "objdisplay.c"),
           os.path.join(SRC, "sysshim.c"),
           os.path.join(SRC, "mpshim.c")]
    cmd += [os.path.join(DECOMP, "src", s) for s in DECOMP_SRCS]
    cmd += ["-I", SRC, "-I", os.path.join(ROOT, "src", "game", "ssb64",
                                          "hoststubs")]
    cmd += DECOMP_INC + DECOMP_WARN + ["-lm"]
    if scalar != "float":
        cmd.append("-DSSB_F32_IS_F64")
    subprocess.run(cmd, check=True)
    return exe


ORACLES = [("objanim.c", build_objanim)]

# The two builds of each, and what each is held to: exact in double,
# measured in single. See EXACT_TOL above.
SCALARS = [("double", (EXACT_TOL, 0.0)), ("float", (DRIFT_ABS, DRIFT_REL))]


def record(scalar):
    """{u32 live mask, u32 error, scalar track[10]}, one per joint per frame."""
    return struct.Struct("=II%d%s" % (len(A.JOINT_TRACKS),
                                      "f" if scalar == "float" else "d"))


def run_c(exe, rec, anim, frames):
    """objanim_oracle.c's protocol, which knows both kinds: {words, joints,
    frames, kind, relocs, floats}, the words in the kind's own size, then
    the entries, the reloc list and the float list."""
    words, entries = anim["words"], anim["entries"]
    blob = struct.pack("=IIIIII", len(words), len(entries), frames,
                       anim["kind"], len(anim["relocs"]),
                       len(anim["floats"]))
    blob += struct.pack("=%d%s" % (len(words),
                                   "I" if anim["kind"] else "H"), *words)
    blob += struct.pack("=%di" % len(entries), *entries)
    blob += struct.pack("=%dI" % len(anim["relocs"]), *anim["relocs"])
    blob += struct.pack("=%dI" % len(anim["floats"]), *anim["floats"])
    # The decomp's parser has no runaway guard of its own -- its inner loop
    # ends when the clock passes the frame, and a script that never lets it
    # would spin. A frame of ten joints is microseconds, so a minute is
    # enough rope for any of them and short enough to name the animation.
    out = subprocess.run([exe], input=blob, stdout=subprocess.PIPE,
                         check=True, timeout=60).stdout
    want = frames * len(entries) * rec.size
    if len(out) != want:
        raise ValueError("oracle wrote %d bytes, expected %d"
                         % (len(out), want))
    return [(f[0], f[1], f[2:])
            for f in (rec.unpack_from(out, i)
                      for i in range(0, len(out), rec.size))]


def run_python(anim, frames):
    """The same, from ssb_assets.Figatree or ssb_assets.AnimJoint by the
    animation's kind. A joint whose script hits a command the reference
    refuses, or reads past the end of its file, is marked with an error
    instead of values; from there on it is not compared."""
    filedata, entries = anim["file"], anim["entries"]
    nwords = len(filedata) // (4 if anim["kind"] else 2)
    trees = [None if e < 0 or e >= nwords else
             (A.AnimJoint(filedata, anim["reloc"], e * 4) if anim["kind"]
              else A.Figatree(filedata, e))
             for e in entries]
    failed = [e >= nwords for e in entries]
    out = []
    for _ in range(frames):
        for j, fg in enumerate(trees):
            if fg is None or failed[j]:
                out.append((0, 1 if failed[j] else 0, None))
                continue
            try:
                v = fg.step()
            except (ValueError, struct.error):
                failed[j] = True
                out.append((0, 1, None))
                continue
            mask = 0
            vals = [0.0] * len(A.JOINT_TRACKS)
            for name, val in v.items():
                i = A.JOINT_TRACKS.index(name)
                mask |= 1 << i
                vals[i] = val
            out.append((mask, 0, vals))
    return out


def compare(got, want, njoints, tol_abs, tol_rel):
    """(error string or None, worst absolute delta, {joint: C error code} for
    the joints the reference gave up on)."""
    refused = {}
    worst = 0.0
    for k, (g, w) in enumerate(zip(got, want)):
        frame, joint = divmod(k, njoints)
        if w[1]:
            # The reference stopped here and stays stopped; record what the
            # C side did about it and leave this joint alone from now on.
            refused.setdefault(joint, g[1])
            continue
        if g[1] != 0:
            return ("C error %d at frame %d joint %d, where the reference "
                    "kept going" % (g[1], frame, joint), worst, refused)
        if g[0] != w[0]:
            return ("live tracks differ at frame %d joint %d: C %s, "
                    "reference %s" % (frame, joint, format(g[0], "010b"),
                                      format(w[0], "010b")),
                    worst, refused)
        for i in range(len(A.JOINT_TRACKS)):
            if not (w[0] >> i) & 1:
                continue
            d = abs(g[2][i] - w[2][i])
            worst = max(worst, d)
            if d > tol_abs + tol_rel * abs(w[2][i]):
                return ("frame %d joint %d track %s: C %.9f, reference %.9f"
                        % (frame, joint, A.JOINT_TRACKS[i], g[2][i], w[2][i]),
                        worst, refused)
    return None, worst, refused


# Set by main() before the pool forks, so every worker inherits the ROM
# bytes, the two built oracles and the record layouts without copying or
# re-reading them. Nothing writes to them after the fork.
_ROM = None
_FRAMES = 0
_BUILT = {}
_RECS = {}


def check_anim(item):
    """One animation against every oracle at every precision.

    Returns what main() needs to print and to tally rather than printing
    itself, so the workers may finish in any order and the report still
    comes out in the ROM's: (name, lines, bad, skipped, worst, refused,
    drift), where `bad` and `skipped` are the oracles this animation was
    one of those things for.
    """
    name, file_id = item
    lines, bad, skipped, refused = [], [], [], []
    worst, drift = {}, {}
    try:
        anim = M.read_anim_ex(_ROM, file_id, ssb_extract)
        want = run_python(anim, _FRAMES)
    except Exception as e:                           # noqa: BLE001
        lines.append("FAIL %-28s %s: %s" % (name, type(e).__name__, e))
        return name, lines, [o for o, _ in ORACLES], skipped, worst, \
            refused, drift

    for oracle, _ in ORACLES:
        failed = False
        for scalar, tol in SCALARS:
            try:
                got = run_c(_BUILT[(oracle, scalar)], _RECS[scalar],
                            anim, _FRAMES)
            except subprocess.TimeoutExpired:
                if not failed:
                    lines.append("FAIL %-28s (%s, %s) did not finish"
                                 % (name, oracle, scalar))
                    bad.append(oracle)
                    failed = True
                continue
            err, d, ref = compare(got, want, len(anim["entries"]), tol[0],
                                  tol[1])
            worst[(oracle, scalar)] = max(worst.get((oracle, scalar), 0.0), d)
            if scalar == "float":
                drift[oracle] = d
            elif ref:
                refused.append((name, oracle, ref))
            if err and not failed:
                lines.append("FAIL %-28s (%s, %s) %s"
                             % (name, oracle, scalar, err))
                bad.append(oracle)
                failed = True
    return name, lines, bad, skipped, worst, refused, drift


def main():
    argv = sys.argv[1:]
    rom_path = M.ROM_DEFAULT
    frames = 300
    match = "Anim"
    verbose = "-v" in argv
    if "--rom" in argv:
        rom_path = argv[argv.index("--rom") + 1]
    if "--frames" in argv:
        frames = int(argv[argv.index("--frames") + 1])
    if "--match" in argv:
        match = argv[argv.index("--match") + 1]
    # Eight, not every core. Two animations -- FTBossAnimUnknown3 and
    # FTBossAnimUnknown5, Master Hand's, the two this check already reports
    # as runaways -- are 24 of the 86 seconds a serial run takes, because
    # the decomp's parser has no guard and spins until the clock passes the
    # frame. They set the floor whatever the pool size is, so a ninth worker
    # buys no wall clock and costs a core the rest of the suite wants
    # (scripts/test_oracle.sh runs the other twenty-two alongside this one).
    jobs = min(os.cpu_count() or 1, 8)
    if "--jobs" in argv:
        jobs = max(1, int(argv[argv.index("--jobs") + 1]))

    rom = open(rom_path, "rb").read()
    anims = []
    for fn in sorted(os.listdir(M.RELOC_DIR)):
        m = re.fullmatch(r"(\d+)_(FT\w*%s\w*)\.c" % re.escape(match), fn)
        if m:
            anims.append((m.group(2), int(m.group(1))))
    if not anims:
        sys.exit("no animation files matching %r in %s" % (match, M.RELOC_DIR))

    tmp = tempfile.TemporaryDirectory()
    global _ROM, _FRAMES, _BUILT, _RECS
    _ROM, _FRAMES = rom, frames
    _RECS = {s: record(s) for s, _ in SCALARS}
    # Built here and not in the workers: a compiler's worth of work, and
    # the same two binaries for all 1775 animations.
    _BUILT = {(name, s): fn(tmp.name, s)
              for name, fn in ORACLES for s, _ in SCALARS}
    # ...and the same for the one thing read_anim_ex builds lazily: which
    # files are AnimJoint animations, which is a listing of relocData and a
    # read of ftdata.c. Warmed before the fork it is read once; left to the
    # workers it would be read once each.
    M.animjoint_file_ids()

    jobs = min(jobs, len(anims))
    print("checking %d animations, %d frames each, against %s%s"
          % (len(anims), frames, " and ".join(n for n, _ in ORACLES),
             "" if jobs == 1 else " (%d at a time)" % jobs))
    bad = {name: 0 for name, _ in ORACLES}
    skipped = {name: 0 for name, _ in ORACLES}
    worst = {(name, s): 0.0 for name, _ in ORACLES for s, _ in SCALARS}
    refused = []
    # fork, so the workers inherit _ROM and the built oracles rather than
    # pickling a 12 MB ROM to each of them. imap keeps the results in the
    # order the animations were listed, whatever order they finish in.
    if jobs > 1:
        pool = multiprocessing.get_context("fork").Pool(jobs)
        results = pool.imap(check_anim, anims, chunksize=1)
    else:
        pool = None
        results = map(check_anim, anims)

    for name, out, b, sk, w, ref, drift in results:
        for line in out:
            print(line)
        for o in b:
            bad[o] += 1
        for o in sk:
            skipped[o] += 1
        for k, v in w.items():
            worst[k] = max(worst[k], v)
        refused.extend(ref)
        if verbose:
            print("  %-28s %s" % (name, ", ".join(
                "%s drift %.2e" % (o, drift[o]) for o, _ in ORACLES)))
    if pool is not None:
        pool.close()
        pool.join()

    for name, oracle, joints in refused:
        print("note %-28s %-11s %s"
              % (name, oracle, ", ".join(
                  "joint %d (%s): %s" % (j, "reference refuses", ERRORS[e])
                  for j, e in sorted(joints.items()))))
    for oracle, _ in ORACLES:
        n = len(anims) - skipped[oracle]
        print("%s: %d/%d animations agree exactly in double precision; "
              "worst single-precision drift %.2e over %d frames%s"
              % (oracle, n - bad[oracle], n, worst[(oracle, "float")],
                 frames, " (%d AnimJoint animations not applicable)"
                 % skipped[oracle] if skipped[oracle] else ""))
    return 1 if any(bad.values()) else 0


if __name__ == "__main__":
    sys.exit(main())

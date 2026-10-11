#!/usr/bin/env python3
"""How large do the ROM's joint rotations get, and does the FSCA guard hold?

src/dc/fighter.c's joint_local feeds three angles to sinf/cosf per joint per
frame. The SH4's FSCA computes both from one instruction, but it takes a
16-bit angle index: SH4ZAM's shz_sincosf multiplies radians by 10430.37835
and truncates with ftrc. That is fine for ordinary angles and wrong for
enormous ones -- ftrc saturates past 2^31 / 10430.37835 rad, and the index
starts skipping table entries long before that.

The animation data has pathological values in it (one
animation holds still for 159,353 frames), so this measures rather
than assumes: it walks every FT animation in the ROM through the reference
Figatree interpreter and reports the largest rotation any joint reaches.

src/dc/mtx.h answers this by sending angles past MTX_FSCA_LIMIT to libm
instead, so what this checks is that the limit is where it needs to be: no
animation may sit between the limit and the point where the index stops
being exact, because that is the band where FSCA would be used outside the
error bound the renderer was signed off against.

Usage: python3 tools/check/fsca_range_check.py [--rom <rom.z64>] [--frames N]
"""
import os
import re
import sys

sys.path[:0] = [os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), d)
                for d in ("lib", "export", "check")]
import ssb_extract           # noqa: E402
import ssb_assets as A       # noqa: E402
import ssb_meshexport as M   # noqa: E402

FSCA_FACTOR = 10430.37835
FSCA_SATURATE = 2.0 ** 31 / FSCA_FACTOR
# Past this the float product loses integer precision and ftrc starts
# skipping indices, so sin/cos quantise more coarsely than the table does.
FSCA_EXACT_INDEX = 2.0 ** 24 / FSCA_FACTOR
# Must track MTX_FSCA_LIMIT in src/dc/mtx.h.
MTX_FSCA_LIMIT = 1024.0

ROT = ("rx", "ry", "rz")


def main():
    argv = sys.argv[1:]
    rom_path = M.ROM_DEFAULT
    frames = 300
    if "--rom" in argv:
        rom_path = argv[argv.index("--rom") + 1]
    if "--frames" in argv:
        frames = int(argv[argv.index("--frames") + 1])

    rom = open(rom_path, "rb").read()
    anims = []
    for fn in sorted(os.listdir(M.RELOC_DIR)):
        m = re.fullmatch(r"(\d+)_(FT\w*Anim\w*)\.c", fn)
        if m:
            anims.append((m.group(2), int(m.group(1))))
    if not anims:
        sys.exit("no animation files found in %s" % M.RELOC_DIR)

    worst = 0.0
    worst_at = None
    skipped = 0
    per_anim = {}
    print("walking %d animations, %d frames each" % (len(anims), frames))

    for name, file_id in anims:
        try:
            f = A.get_file(rom, file_id, ssb_extract)
            _, entries, _ = M.read_anim(rom, file_id, ssb_extract)
        except Exception:                            # noqa: BLE001
            skipped += 1
            continue
        nwords = len(f) // 2
        trees = [None if e < 0 or e >= nwords else A.Figatree(f, e)
                 for e in entries]
        dead = [t is None for t in trees]
        for _ in range(frames):
            for j, fg in enumerate(trees):
                if dead[j]:
                    continue
                try:
                    v = fg.step()
                except Exception:                    # noqa: BLE001
                    dead[j] = True
                    continue
                for t in ROT:
                    if t not in v:
                        continue
                    a = abs(v[t])
                    if a > worst:
                        worst, worst_at = a, (name, j, t)
                    if a > per_anim.get(name, 0.0):
                        per_anim[name] = a

    print()
    print("largest |rotation|      %.6g rad  (%s joint %d %s)"
          % (worst, worst_at[0], worst_at[1], worst_at[2]) if worst_at
          else "largest |rotation|  none")
    print("ftrc index stays exact below   %.6g rad" % FSCA_EXACT_INDEX)
    print("ftrc saturates at              %.6g rad" % FSCA_SATURATE)
    if skipped:
        print("(%d animations unreadable, skipped)" % skipped)

    over = sorted(((v, k) for k, v in per_anim.items()
                   if v >= FSCA_EXACT_INDEX), reverse=True)
    print()
    print("animations past the exact-index limit: %d of %d"
          % (len(over), len(per_anim)))
    for v, k in over[:25]:
        print("    %-32s %.6g rad" % (k, v))
    mario = sorted(((v, k) for k, v in per_anim.items()
                    if k.startswith("FTMario")), reverse=True)
    if mario:
        print("largest among FTMario*: %.6g rad (%s)" % (mario[0][0],
                                                         mario[0][1]))
    print()

    print("mtx.h sends angles past %.6g rad to libm" % MTX_FSCA_LIMIT)
    if MTX_FSCA_LIMIT >= FSCA_EXACT_INDEX:
        print("FAIL: MTX_FSCA_LIMIT is above the exact-index limit, so FSCA"
              " would run outside its error bound.")
        return 1
    band = [(v, k) for k, v in per_anim.items()
            if MTX_FSCA_LIMIT <= v < FSCA_EXACT_INDEX]
    if band:
        print("FAIL: %d animations sit between the limit and the exact-index"
              " bound:" % len(band))
        for v, k in sorted(band, reverse=True)[:10]:
            print("    %-32s %.6g rad" % (k, v))
        return 1
    print("ok: every angle taking the FSCA path is under %.6g rad, inside the"
          " exact-index range with %.0fx margin"
          % (MTX_FSCA_LIMIT, FSCA_EXACT_INDEX / MTX_FSCA_LIMIT))
    if over:
        print("    %d animations exceed it and take libm: %s"
              % (len(over), ", ".join(k for _, k in over)))
    return 0


if __name__ == "__main__":
    sys.exit(main())

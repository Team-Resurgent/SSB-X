#!/bin/bash
# ssb64-dc: keep approximate SH4 math out of the code that must match the N64.
#
# Two instructions the SH-4 has and the N64 did not, and they fail in
# opposite directions.
#
# FSCA and FSRRA are approximations. FSCA quantises the angle to one of 65536
# indices (9.6e-05 rad) and truncates; FSRRA is good to about 2^-21. Both are
# fine in the renderer and wrong in game logic, where this port's whole claim
# is that it computes what the N64 computed.
#
# The host cross-tests cannot catch a slip here. Off-target SH4ZAM falls back
# to exact libm (SHZ_BACKEND is SHZ_SW), so hosttest_ft and figatree_check
# would stay green while the target diverged. This grep looks at the SH4 code
# itself, which is the only place the divergence exists.
#
# It also catches a stray -ffast-math: KOS already passes -mfsrra -mfsca,
# which are inert alone but armed by it. With fast-math GCC would rewrite the
# decomp's own 1.0F / sqrtf(...) in mpcollision.c into FSRRA, changing
# collision with no code change anywhere in this repo.
#
# FMAC is the other way round: it is *more* accurate than what it replaces,
# and that is the problem. It computes a*b + c with one rounding where a
# multiply and an add round twice, and the VR4300 has no such instruction at
# all -- tools/check/fpu_check.py scans every code range of the ROM and finds
# not one. So an fmac anywhere in this build is a number the N64 could not have
# produced, and scripts/dc_env.sh passes -ffp-contract=off to stop GCC
# making them. This is the check that says it worked, and it looks at every
# object rather than the list below: nothing in the port needs the
# instruction, so nothing may have it.
#
# Both legs go through tools/check/fpu_check.py rather than grep, because the
# SH-4 keeps its constants and its switch tables inside .text and one
# halfword in sixteen of those disassembles as an `fmac`. A grep of
# `sh-elf-objdump -d` reports ninety-three in a build that has none.
set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$(cd "$SCRIPT_DIR/.." && pwd)"

# Objects whose float results are pinned to the decomp's, and by what:
#   ftcommon    hosttest_ft, fuzzed against verbatim ft/ftphysics.c copies
#   ftmanager   the spawn: the floor projection and the settle, hosttest_ft
#   ftshadow    ft/ftshadow.c, whose vertex positions are the floor line
#               interpolated at two x -- the picture is a renderer's, the
#               numbers under it are the game's
#   utils       sys/utils.c unmodified: the RNG and the trig helpers game
#               logic draws on (an fsca in syUtilsTan would be a different
#               random or a different angle)
#   vector      sys/vector.c unmodified: syVectorNorm3D and friends, under
#               the collision walk and the camera
#   gmcamera    the battle camera, verbatim; its angles are lbcommon's
#   lbcommon    the game's 4096-step sine table and the trig over it
#   figatree    tools/check/figatree_check.py, bit-exact over all 1775
#               animations
#   mpshim      syVectorNorm3D, verbatim sys/vector.c:6-19
#   mpcollision/mpprocess  compiled unmodified from the pinned decomp
#   fgm         the FGM engine; its sinf is deliberate (see fgm.c:555)
#   gmcollision  gm/gmcollision.c compiled unmodified: the hitbox maths --
#               a sphere against a box in a joint's own space, the
#               matrix chain that puts the two there -- and so what
#               lands and what whiffs
#   lbparticle  lb/lbparticle.c copied line for line: the bytecode
#               interpreter, whose __sinf, sqrtf and gSYSinTable reads
#               decide where every spark goes. tools/check/lbparticle_check.py
#               pins it to the decomp on the host; this is what says the
#               SH-4's build of it computes the same thing
#   matrix      sys/matrix.c unmodified: syMatrixRotRpyRF's six calls to
#               __sinf and __cosf, under the joint walk that gives a
#               generator its position
#   gusinf/gucosf  libultra's own __sinf and __cosf, written
#               from the decomp by tools/export/ssb_trigexport.py.
#               These are the ones the knockback vector goes through
#               (ftCommonDamageInitDamageVars), so an fsca here would be
#               the N64's sine replaced by an approximation of it in the
#               one place the port went to the trouble of not doing that
#   mtxutil/mtxcatf/normalize  libultra's gu, compiled unmodified;
#               normalize.c takes a sqrtf and must not get FSRRA
#   objman/objhelper/objscript/objanim/interp/ftanim  the object system,
#               also compiled unmodified from the pinned decomp. objanim.c
#               is the animation engine and ftanim.c the figatree parser
#               above it -- they are what moves
#               a fighter through a ledge climb, so a rounded sine there
#               would move him somewhere the N64 does not; interp.c is
#               the curve evaluator under them and takes a sqrtf.
#   the rest of what src/game/ssb64/Makefile compiles straight out of
#               $(SSB_DECOMP_DIR): the fighter's physics and its shared
#               status handlers, the six character specials, the particle
#               bank cache, the save-data pack and the controller subsystem.
#               A file missing from this list is how
#               ft/ftphysics.c -- eight fused multiply-adds in the
#               fighter's own integrator -- once sat outside it
PURE_OBJS="ftcommon.o ftmanager.o ftmain.o ftparam.o ftshadow.o utils.o vector.o gmcamera.o lbcommon.o mpshim.o mpcollision.o mpprocess.o fgm.o \
objman.o objhelper.o objscript.o objanim.o interp.o ftanim.o gmcollision.o \
lbparticle.o matrix.o gusinf.o gucosf.o mtxutil.o mtxcatf.o normalize.o \
ftphysics.o ftpublic.o ftcommondata.o efparticle.o symalloc.o lbbackup.o \
scsubsyscontroller.o \
ftcommonstopceil.o ftcommondownforwardback.o ftcommondownattack.o \
ftcommonrebound.o ftcommoncliffattack.o ftcommoncapturewait.o \
ftcommonlandingair.o \
ftmariospeciallw.o ftdonkeyspeciallw.o ftpurinspeciallw.o \
ftkirbyspeciallw.o ftpurinspecialn.o ftkirbycopypurinspecialn.o"
PURE_SRCS="src/dc/ftcommon.c src/dc/ftmanager.c src/dc/ftshadow.c src/dc/gmcamera.c src/dc/lbcommon.c src/dc/mpshim.c src/dc/fgm.c src/dc/lbparticle.c src/game/ssb64/gusinf.c src/game/ssb64/gucosf.c"

fail=0

# Source-level: these must not reach for the library at all.
for f in $PURE_SRCS; do
    [ -f "$f" ] || continue
    if grep -q '#include[[:space:]]*<sh4zam/' "$f"; then
        echo "purity: $f includes sh4zam" >&2
        fail=1
    fi
done

# Object-level: the instructions actually emitted.
# -not -name 'hosttest_*': `make hosttest` leaves its objects in the same
# directory, and they are the build machine's, not the Dreamcast's --
# scripts/test_host.sh is what holds those, and src/game/ssb64/Makefile's
# HOST_CFLAGS passes them the same -ffp-contract=off.
# aica_*.o are the AICA firmware's: ARM code, which
# sh-elf-objdump cannot read and the FPU rules do not concern.
ALL_OBJS="$(find src/game -name '*.o' -not -name 'hosttest_*' \
                 -not -name 'aica_*' 2>/dev/null | sort)"
if [ -z "$ALL_OBJS" ]; then
    echo "purity: no built objects found -- run a build first" >&2
    exit 1
fi

PURE_FOUND=""
for o in $ALL_OBJS; do
    base="${o##*/}"
    case " $PURE_OBJS " in *" $base "*) PURE_FOUND="$PURE_FOUND $o" ;; esac
done

# shellcheck disable=SC2086
scan() {
    what="$1"; shift
    if out="$(python3 tools/check/fpu_check.py --insns "$what" --objects "$@")"; then
        echo "purity: ${out#objects: }"
    else
        fail=1
    fi
}
# shellcheck disable=SC2086
scan fsca,fsrra $PURE_FOUND
# shellcheck disable=SC2086
scan fmac $ALL_OBJS

exit $fail

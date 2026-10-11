#!/bin/sh
# ssb64-dc: Dreamcast build environment.
#
#   source ./scripts/dc_env.sh
#
# Sourced by the build image's entrypoint, ahead of every command it runs.
# Nothing it points at lives in this repo: KallistiOS, the cross-compiler,
# the decomp the asset tools parse, and gfxdis all come from the image,
# which sets KOS_BASE and friends before this runs (docker/Dockerfile). The
# fallbacks below put them under DC_TOOLCHAIN_ROOT, for a host that has them
# installed by hand.
#
# On the host, dc_env.local.sh (gitignored, see dc_env.local.sh.example)
# supplies per-machine settings: the container runtime, the image, the ROM.

if [ -n "${BASH_SOURCE:-}" ]; then
    _root="${BASH_SOURCE[0]}"
elif [ -n "${ZSH_VERSION:-}" ]; then
    _root="$(eval 'echo ${(%):-%x}')"
else
    _root="$0"
fi
_scripts="$(cd "$(dirname "${_root}")" && pwd)"
_root="$(cd "${_scripts}/.." && pwd)"

# Per-machine override, if any. Inside the image the paths are fixed by its
# environment, and there is no host to have preferences about.
if [ -z "${SSB64_CONTAINER:-}" ] && [ -f "${_root}/dc_env.local.sh" ]; then
    . "${_root}/dc_env.local.sh"
fi

DC_TOOLCHAIN_ROOT="${DC_TOOLCHAIN_ROOT:-$HOME/dc-toolchain}"

export KOS_ARCH="dreamcast"
# All five are set by the build image. The defaults are for a hand-built
# host toolchain; none of them resolve into this repository.
export KOS_BASE="${KOS_BASE:-${DC_TOOLCHAIN_ROOT}/kos}"
export SSB_DECOMP_DIR="${SSB_DECOMP_DIR:-${DC_TOOLCHAIN_ROOT}/ssb-decomp-re}"
export SSB_GFXDIS="${SSB_GFXDIS:-${DC_TOOLCHAIN_ROOT}/bin/gfxdis.f3dex2}"
export SH4ZAM_DIR="${SH4ZAM_DIR:-${DC_TOOLCHAIN_ROOT}/sh4zam}"
export KOS_CC_BASE="${KOS_CC_BASE:-${DC_TOOLCHAIN_ROOT}/sh-elf}"
export KOS_CC_PREFIX="sh-elf"
export DC_ARM_BASE="${DC_TOOLCHAIN_ROOT}/arm-eabi"  # the AICA firmware compiler
export DC_ARM_PREFIX="arm-eabi"
export DC_TOOLS_BASE="${DC_TOOLCHAIN_ROOT}/bin"

export KOS_CMAKE_TOOLCHAIN="${KOS_BASE}/utils/cmake/kallistios.toolchain.cmake"
export KOS_GENROMFS="${KOS_BASE}/utils/genromfs/genromfs"
export KOS_MAKE="make"
export KOS_LOADER="dc-tool -x"

# Host tools built in-tree (bin2c, genromfs, scramble, makeip, vqenc, ...)
export PATH="${KOS_BASE}/utils/build_wrappers:${PATH}"

# environ_base.sh (which sources environ_dreamcast.sh) defines the real build
# vars: KOS_CC, KOS_CFLAGS, KOS_LDFLAGS, KOS_LIBS, KOS_INC_PATHS, etc.
# It appends to these, so reset first to make re-sourcing idempotent.
export KOS_INC_PATHS=""
export KOS_CFLAGS=""
export KOS_CPPFLAGS=""
export KOS_LDFLAGS=""
export KOS_AFLAGS=""
export KOS_CFLAGS="${KOS_CFLAGS} -O2 -fno-PIC -fno-PIE -fomit-frame-pointer"

# -ffp-contract=off: the SH-4 has a fused multiply-accumulate and the
# VR4300 does not. GCC's default (-ffp-contract=fast) rewrites `a*b + c`
# into one `fmac`, which rounds once where a multiply and an add round
# twice, so every such expression in the game's own arithmetic comes out
# a bit or two from what the N64 computed -- silently, with nothing in
# this repository saying so. The decomp builds with `-mips2`
# (ssb-decomp-re/Makefile:94), an ISA with no fused multiply-add
# encoding at all, and tools/check/fpu_check.py confirms it against the ROM's
# own code: not one in 363,356 instructions.
#
# It goes here, next to -O2, because it is the port's floating-point
# policy and not one file's: every Dreamcast build in this tree gets it,
# and a decomp file added tomorrow gets it without anyone remembering to. scripts/test_purity.sh greps every
# object for `fmac` and is what says it held.
#
# The renderer would be entitled to the instruction -- nothing there is
# pinned to the N64, and 84 of the 772 were in fighter.o, objdisplay.o,
# clip.o and objmodel.o -- but it does not need
# it. Building the game both ways and running the same match: 523,372
# bytes of text against 523,756, and an update of 713-748 us a frame
# against 714-749 on a draw of 16.0 ms. One flag for everything beats a
# list of exceptions that has to stay right.
export KOS_CFLAGS="${KOS_CFLAGS} -ffp-contract=off"

if [ ! -f "${KOS_BASE}/environ_base.sh" ]; then
    echo "dc_env.sh: no KallistiOS at ${KOS_BASE}." >&2
    echo "  Builds run in the container: ./run.sh image, then ./run.sh build <target>." >&2
    return 1 2>/dev/null || exit 1
fi
. "${KOS_BASE}/environ_base.sh"

unset _root _scripts

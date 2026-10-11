#!/bin/bash
# ssb64-dc: the build container. Everything but the host-side dc-tool runs in here.
#
#   scripts/ctr.sh name                       print the image tag
#   scripts/ctr.sh build [--local] [--prebuilt] [args]
#                                             get the image  (./run.sh image)
#   scripts/ctr.sh run [--net-host] <cmd...>  run a command in it
#
# The repo is bind-mounted at /work and commands run as you, so build outputs
# land in the working tree with your ownership. The image itself carries the
# cross-compiler, KallistiOS, the decomp and gfxdis at the revisions in
# docker/pins.env -- none of which live in this repo any more.
#
# `build` pulls the published image for these pins (amd64 and arm64) when
# there is one, and compiles it here when there is not, or with --local.
#
# Overrides, settable in dc_env.local.sh (gitignored):
#   CTR=docker|podman     force a runtime instead of probing
#   SSB64_IMAGE=<ref>     use an image built elsewhere
#   SSB64_REGISTRY=<repo> where to pull from (default: IMAGE_REGISTRY in pins.env)
#   SSB64_ROM=<file>      mount the baserom from outside the repo (at ./baserom.z64)
# Passed through when set: EXTRA_CFLAGS, EXTRA_LDFLAGS, MTX_BACKEND,
# SSB64_BUILD_JOBS, RELEASE.
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$REPO_ROOT"

. docker/pins.env
[ -f dc_env.local.sh ] && . ./dc_env.local.sh

# The tag records what is inside, so a bumped pin is a different image.
# IMAGE_LAYOUT moves when the Dockerfile adds something the build needs
# (arm1: the AICA's arm-eabi compiler; arm2: the baker's i386 cross-compiler and qemu on non-x86), so an older image is
# rebuilt rather than found wanting halfway through a build.
IMAGE_LAYOUT=arm2
IMAGE="${SSB64_IMAGE:-ssb64-dc/dev:${TOOLCHAIN_PROFILE}-${KOS_REV:0:7}-${DECOMP_REV:0:7}-${N64_REV:0:7}-${SH4ZAM_REV:0:7}-${IMG4DC_REV:0:7}-${IMAGE_LAYOUT}}"

pick_runtime() {
    if [ -n "${CTR:-}" ]; then
        echo "$CTR"
    elif command -v docker >/dev/null 2>&1 && docker info >/dev/null 2>&1; then
        echo docker
    elif command -v podman >/dev/null 2>&1; then
        echo podman
    else
        echo "no usable docker or podman (set CTR=... to force one)" >&2
        exit 1
    fi
}

case "${1:-}" in
name)
    echo "$IMAGE"
    ;;

build)
    shift
    RT="$(pick_runtime)"
    LOCAL=0
    if [ "${1:-}" = "--local" ]; then LOCAL=1; shift; fi
    # The published tag is this image's tag, so a bumped pin is a miss.
    REMOTE="${SSB64_REGISTRY:-$IMAGE_REGISTRY}:${IMAGE#*:}"
    if [ "$LOCAL" = 0 ] && [ "${1:-}" != "--prebuilt" ] && [ -z "${SSB64_IMAGE:-}" ] \
       && [ -n "${SSB64_REGISTRY:-${IMAGE_REGISTRY:-}}" ]; then
        if "$RT" pull "$REMOTE" 2>/dev/null; then
            "$RT" tag "$REMOTE" "$IMAGE"
            echo "pulled $REMOTE"
            exit 0
        fi
        echo "no published image for these pins; building here (this takes about an hour)"
    fi
    BUILD_ARGS=(--build-arg "TOOLCHAIN_PROFILE=$TOOLCHAIN_PROFILE"
                --build-arg "KOS_URL=$KOS_URL" --build-arg "KOS_REV=$KOS_REV"
                --build-arg "DECOMP_URL=$DECOMP_URL" --build-arg "DECOMP_REV=$DECOMP_REV"
                --build-arg "N64_URL=$N64_URL" --build-arg "N64_REV=$N64_REV"
                --build-arg "SH4ZAM_URL=$SH4ZAM_URL" --build-arg "SH4ZAM_REV=$SH4ZAM_REV"
                --build-arg "IMG4DC_URL=$IMG4DC_URL" --build-arg "IMG4DC_REV=$IMG4DC_REV")
    if [ "${1:-}" = "--prebuilt" ]; then
        shift
        # KallistiOS publishes this toolchain for amd64 only, and it is
        # Alpine/musl, so the rest of the image has to be Alpine too.
        BUILD_ARGS+=(--build-arg "TOOLCHAIN_FROM=${TOOLCHAIN_IMAGE}@${TOOLCHAIN_IMAGE_DIGEST}"
                     --build-arg "DEV_BASE=alpine:3.22")
        echo "using KallistiOS's prebuilt $TOOLCHAIN_PROFILE toolchain (amd64 only)"
    fi
    echo "building $IMAGE with $RT"
    exec "$RT" build -f docker/Dockerfile --target dev -t "$IMAGE" \
        "${BUILD_ARGS[@]}" "$@" "$REPO_ROOT"
    ;;

run)
    shift
    RT="$(pick_runtime)"
    if ! "$RT" image exists "$IMAGE" 2>/dev/null && \
       ! "$RT" image inspect "$IMAGE" >/dev/null 2>&1; then
        echo "no image $IMAGE -- run: ./run.sh image" >&2
        exit 1
    fi

    ARGS=(--rm --security-opt label=disable -e TERM
          -v "$REPO_ROOT:/work" -w /work)
    # The build knobs cross into the container by name, so
    # `EXTRA_CFLAGS=-DDB_BOOT_SCENE=nSCKindVSResults ./run.sh disc`
    # reaches the Makefile (scripts/build_target.sh, scripts/make_cdi.sh
    # forward them; src/dc/db.h lists the knobs).
    for v in EXTRA_CFLAGS EXTRA_LDFLAGS MTX_BACKEND SSB64_BUILD_JOBS \
             SSB_DISC_NO_BUNDLE RELEASE; do
        [ -n "${!v:-}" ] && ARGS+=(-e "$v")
    done
    # Same uid inside as out, so nothing in the tree ends up root-owned.
    if [ "$(basename "$RT")" = podman ]; then
        ARGS+=(--userns=keep-id)
    else
        ARGS+=(--user "$(id -u):$(id -g)")
    fi
    [ -t 0 ] && [ -t 1 ] && ARGS+=(-it)
    [ "${1:-}" = "--net-host" ] && { shift; ARGS+=(--network host); }
    # The baserom is copyrighted: mounted, never copied into an image.
    [ -n "${SSB64_ROM:-}" ] && ARGS+=(-v "$SSB64_ROM:/work/baserom.z64:ro")

    exec "$RT" run "${ARGS[@]}" "$IMAGE" "$@"
    ;;

*)
    sed -n '2,13p' "$0" >&2
    exit 2
    ;;
esac

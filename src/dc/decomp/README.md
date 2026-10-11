# decomp shim headers

`src/dc/decomp/` sits *ahead* of the pinned ssb-decomp-re tree on the
include path when a decomp source file is compiled straight into the port
(today: `src/mp/mpcollision.c`, `src/mp/mpprocess.c`, `src/sys/malloc.c`,
and from milestone 8 the rest of the `sys/` object system) — and also
whenever the port's own code includes a decomp header, which from milestone
8 on includes `src/dc/input.h` reaching for `sys/controller.h`. Each
header here shadows one decomp header by name — including under `PR/`
and `sys/`, since the whole directory goes on the include path; everything
else is used as it is. Most are full replacements. `macros.h`
and `sys/utils.h` are not: each pulls the real header in with
`#include_next` and adjusts one thing around it.

Include order and defines, from `src/game/ssb64/Makefile`:

    -I src/dc/decomp  -I $(SSB_DECOMP_DIR)/src  -idirafter $(SSB_DECOMP_DIR)/include
    -D_LANGUAGE_C -DF3DEX_GBI_2

`-idirafter` keeps the decomp's libc lookalikes (`include/stdlib.h`,
`string.h`, ...) behind the system ones. The two defines are what the
decomp's own build has: `Makefile:93` passes `-DF3DEX_GBI_2`, and IDO
predefines `_LANGUAGE_C` where GCC does not — `PR/gbi.h` guards its entire
body on it (`gbi.h:1061`), so without it every N64 graphics type is an
unknown type name.

## What is shadowed, and why

| Header | Why |
|---|---|
| `PR/ultratypes.h` | the original spells `u32`/`s32` as `unsigned long`/`long`, which is 64-bit on the x86-64 host the cross-tests run on. stdint keeps them 32-bit on both. |
| `PR/os.h` | libultra's OS is the one part of the N64 the port replaces outright. A few dozen names instead of 1051 lines — the message queue, the four thread calls (coroutines in `src/dc/sysshim.c`), `osGetTime`, the official button names by value — and it drops the `bcopy`/`bcmp`/`bzero` declarations, whose `int` lengths conflict with newlib's `size_t` ones in any TU that reaches both. |
| `gr/ground.h` | `GRFileInfo` and the three `lbReloc` entries `mpCollisionInitGroundData` wants (stubbed in `mpshim.c`; the port never calls it). |
| `sc/scene.h` | the real `sc/sctypes.h` (`SCBattleState`, `SCPlayerData`, which the fighter manager fills) behind the two enum headers it needs, plus `gSCManagerBattleState` and `scManagerRunPrintGObjStatus`; the real umbrella also pulls in the overlay table and every scene function. |
| `macros.h` | it does `#define __attribute__(x)` (macros.h:32), which deletes every GCC attribute for the rest of the translation unit — including the `aligned(8)` on `src/dc/mtx.h`'s `mtx4_t`, moving `Fighter.mtx` by four bytes in any file that reaches a decomp header first. The shim `#include_next`s the real header and undoes that one line; `mtx.h` carries a `_Static_assert` that fails the build if this shim is ever off the path. |
| `sys/utils.h` | it declares `f32 __sinf(f32)` and `f32 __cosf(f32)`, which collide with glibc's `float __sinf(float)` in the one build where `f32` is `double` — the double half of `tools/check/figatree_check.py`. Nothing the port compiles calls either, so the shim renames the decomp's pair (and only in that build) rather than shadowing 200 lines. `sys/objtypes.h:13` includes it, so every object-system file reaches it. |

## What is deliberately *not* shadowed

The fighter system's headers — `ft/fighter.h` and everything it reaches:
`ft/fttypes.h` (`FTStruct`, `FTAttributes`, `FTDesc`, `FTStatusDesc`),
`ft/ftfunctions.h`, the per-character headers, and through them
`gm/generic.h`, `ef/effect.h` and `lb/library.h` — are the decomp's,
unmodified. Four shims once stood in for
them (`ft/fighter.h`, `ft/ftchar/ftmario/ftmario.h`,
`ft/ftcommon/ftcommonfunctions.h`, `gm/generic.h`) and the port kept a
movement subset of `FTStruct` of its own; the subset is gone, and the
things GCC would not take in the real headers are patches in
`docker/patches/` (0003, 0004, 0005: the per-character headers moved below
the types they declare arrays of, and the `FTAnimDesc` and
`FTMotionEvent*` bit-fields following the ABI). Compiling against the real declarations is also what caught the jump
force's integer pointees.


The object system's own headers — `sys/obj.h`, `sys/objtypes.h`,
`sys/objdef.h`, `sys/objanim.h`, `sys/objdisplay.h`, `sys/taskman.h`,
`sys/controller.h`, `sys/rdp.h`, `sys/debug.h`, `sys/audio.h`, `PR/gbi.h`,
`PR/gu.h`, `PR/sp.h`, `PR/mbi.h` — are the decomp's, unmodified. Two of
them declare functions the port defines rather than compiles:
`sys/objdisplay.h` (`src/dc/objdisplay.c`, the PVR back end) and
`sys/taskman.h` (`src/dc/taskman.c`) — and so, since milestone 9, do the
fighter headers: `ft/ftmanager.h`, `ft/ftmain.h`, `ft/ftphysics.h`,
`ft/ftparam.h` and `ft/ftcommon/ftcommonfunctions.h` declare what
`src/dc/ftmanager.c` and `src/dc/ftcommon.c` define, so a ported function
whose signature drifts from the game's is a compile error, not a quiet
divergence. That is deliberate — the port keeps
the game's interface and replaces the body, so scene code goes on calling
the decomp's names. Until milestone 8 two of them were shadowed by
stripped stand-ins (`sys/obj.h` carried a cut-down `DObj`/`GObj`/`MObj`,
`PR/gbi.h` a two-word `Gfx`), because the collision files were the only
decomp code in the port and those were all they needed. The object system
needs the real structures, they compile for SH-4 as they stand, and the
collision code is strictly better off walking the game's real `DObj` than a
stand-in — so the stand-ins are gone and there is that much less to keep in
step by hand.

## What a shim cannot do

A shim can replace a header, and it can `#include_next` one and adjust a
macro. What it cannot do is change a **struct layout**, because it cannot
re-declare a type the real header also declares — and copying a 590-line
header to change six lines of it is the fork this project has said it will
not keep.

That case is real: `sys/objtypes.h` declares the two AObj script words as
bit-fields, and a bit-field's members are allocated from the opposite end of
their container on a little-endian ABI. So the remaining edits live in
`docker/patches/`, applied with `git apply` to the pinned checkout in the
image's `src` stage — never as a fork. Each is guarded so the decomp's own
N64 build is unchanged, and `docker/patches/README.md` carries the rules.

The decomp files themselves are never modified in this tree.

The runtime symbols these headers declare are defined in `src/dc/mpshim.c`
(for `mp/`) and `src/dc/sysshim.c` (for `sys/`: the libultra OS calls, the
RDP viewport helper and the video resolution behind it).

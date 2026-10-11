/* Shadows ssb-decomp-re/src/gr/ground.h, included by
 * src/mp/mpcollision.c:3. Used there for GRFileInfo (gr/grtypes.h:43-46,
 * the ground file table dMPCollisionGroundFileInfos at mpcollision.c:
 * 45-88) and, through it, the lbReloc file loading that
 * mpCollisionInitGroundData (mpcollision.c:3961-4011) does:
 *   lbRelocGetFileData      src/lb/library.h:7-8   (a cast macro)
 *   lbRelocGetExternHeapFile src/lb/lbreloc.h:69
 *   lbRelocGetFileSize       src/lb/lbreloc.h:67
 * The port never calls mpCollisionInitGroundData -- the stage pack's
 * tables are already in RAM (stage.c) -- so the three lbReloc entries
 * are link-time stubs in mpshim.c.
 *
 * It also stands in for what the real gr/ground.h pulls in transitively.
 * Those declarations come from the decomp's own object-system headers. */
#ifndef _GROUND_H_
#define _GROUND_H_

#include <stdint.h>

#include <ssb_types.h>
/* syDebugPrintf, for mpcollision.c's error paths */
#include <sys/debug.h>
/* syAudioPlayBGM, for mpCollisionSetPlayBGM (mpcollision.c:4013-4020) */
#include <sys/audio.h>
/* gcParseDObjAnimJoint / gcPlayDObjAnimJoint / gcPlayMObjMatAnim, the
 * yakumono animation calls */
#include <sys/objanim.h>

/* mpcollision.c:3736 calls this and no decomp header declares it -- IDO
 * takes the implicit declaration. GCC's would be wrong (int return, and
 * -Wimplicit-function-declaration on every build), so it is spelled out
 * here, matching sys/objanim.h's neighbours. */
extern void gcParseMObjMatAnimJoint(MObj *mobj);

/* gr/grtypes.h:19-24 spells GRFileInfo as `struct GRFileInfo` only, and
 * mpcollision.c:26 writes it without the keyword -- so the typedef, which
 * is all that is left of it here. */
typedef struct GRFileInfo GRFileInfo;

/* gr/grtypes.h, whole: the stage's damaging surfaces
 * and the moving stage pieces a fighter can be hurt or pushed by
 * (GRAttackColl, GRObstacle, GRHazard -- what ft/ftmain.c's ground-hit
 * search walks), the per-stage ground globals' union (GRStruct), and the
 * file-info pair.
 * Hyrule's tornado is the first tenant of that union (src/dc/grhyrule.c,
 * whose gGRCommonStruct is defined in src/dc/stage.c where the game
 * defines it, gr/grcommonsetup.c). */
#include <gr/grtypes.h>

/* gr/grcommonsetup.c:0x801313F0, defined in src/dc/stage.c. */
extern GRStruct gGRCommonStruct;

/* lb/library.h:7's, restated for the collision files that reach this
 * header without the library tree; the guard keeps the two from
 * colliding where a translation unit reaches both. */
#ifndef lbRelocGetFileData
#define lbRelocGetFileData(type, file, offset) \
    ((type)((uintptr_t)(file) + (intptr_t)(offset)))
#endif

extern size_t lbRelocGetFileSize(u32 id);
extern void *lbRelocGetExternHeapFile(u32 id, void *heap);

#endif /* _GROUND_H_ */

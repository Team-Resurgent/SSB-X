/* mpcommon.h -- the fighter-facing side of the map collision module.
 *
 * mpcollision.c and mpprocess.c are the decomp's own files, compiled
 * unmodified from $SSB_DECOMP_DIR/src/mp/ behind src/dc/decomp/ (see the
 * README there); their declarations come from <mp/map.h>. This header
 * adds the one port-side entry point, mpCollisionLoadGeometry, and the
 * definitions of the mp/mpcommon.c functions the port carries live in
 * mpcommon.c under the signatures mp/mpcommon.h already declares.
 *
 * The fighter travels through the GObj* parameters: ftGetStruct
 * (ft/fighter.h) reads
 * the FTStruct off its GObj. */
#ifndef SSB_DC_MPCOMMON_H
#define SSB_DC_MPCOMMON_H

#include <mp/map.h>

/* Bind a stage's collision tables and rebuild the per-line info, in
 * place of mpCollisionInitGroundData (mp/mpcollision.c:3961-4011),
 * which loads the ground file through lbReloc. The tables come out of
 * the scene's general heap, as they do in the game, so this resets that
 * heap first -- taskman.h -- and the previous stage's tables go with it.
 * syTaskmanMakeGeneralHeap must have run. Every yakumono comes up
 * static. */
void mpCollisionLoadGeometry(MPGeometryData *gdata);

#endif /* SSB_DC_MPCOMMON_H */

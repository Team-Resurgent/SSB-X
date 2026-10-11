/* Shadows ssb-decomp-re/src/gr/grcommon/gryamabuki.h, which
 * src/dc/gryamabuki.c and src/dc/itglucky.c include (the decomp reaches
 * it through gr/ground.h -> gr/grfunctions.h; this port's gr/ground.h is
 * the collision files' shim and stops short of that, so its ported users
 * name it directly).
 *
 * One signature differs from the decomp's, and it is the whole reason
 * this shim exists:
 *
 *   void grYamabukiGateAddAnimOffset(intptr_t offset);
 *
 * The game's `offset` is `&llGRYamabukiMapGateOpenAnimJoint` -- a linker
 * symbol's address, added to a map-file base pointer this port has no
 * file for (src/dc/gryamabuki.c's header says so at length). The port
 * names the script instead. Everything else here is the decomp's own
 * list.
 *
 * The Gate's OTHER half, which the ITEM calls: nITKindGLucky's own
 * grYamabukiGateSetClosedWait and grYamabukiGateClearMonsterGObj, and
 * the monster range's own data. Those signatures are the decomp's.
 */
#ifndef _GRYAMABUKI_H_
#define _GRYAMABUKI_H_

#include <ssb_types.h>
#include <sys/objdef.h>
#include <gr/grdef.h>

extern s32 dGRYamabukiMonsterAttackKind;
extern u16 dGRYamabukiMonsterMapObjKinds[];

extern void grYamabukiGateUpdateSleep(void);
extern sb32 grYamabukiGateCheckPlayersNear(void);
extern void grYamabukiGateMakeMonster(void);
extern void grYamabukiGateSetPositionFar(void);
extern void grYamabukiGateSetPositionNear(void);
extern void grYamabukiGateAddAnimOffset(const char *name);
extern void grYamabukiGateAddAnimOpen(void);
extern void grYamabukiGateAddAnimClose(void);
extern void grYamabukiGateAddAnimOpenEntry(void);
extern void grYamabukiGateUpdateWait(void);
extern void grYamabukiGateUpdateOpen(void);
extern void grYamabukiGateClearMonsterGObj(void);
extern void grYamabukiGateSetClosedWait(void);
extern void grYamabukiGateUpdateYakumonoPos(void);
extern void grYamabukiGateProcUpdate(GObj *ground_gobj);
extern void grYamabukiMakeGate(void);
extern void grYamabukiInitGroundVars(void);
extern GObj* grYamabukiMakeGround(void);

#endif

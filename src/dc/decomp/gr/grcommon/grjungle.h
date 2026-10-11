/* Shadows ssb-decomp-re/src/gr/grcommon/grjungle.h, which
 * src/dc/grjungle.c and src/dc/ftcommontaru.c include (the decomp reaches
 * it through gr/ground.h -> gr/grfunctions.h; this port's gr/ground.h is
 * the collision files' shim and stops short of that, so its two ported
 * users name it directly).
 *
 * One signature differs from the decomp's, and it is the whole reason
 * this shim exists:
 *
 *   void grJungleTaruCannAddAnimOffset(GObj *ground_gobj, intptr_t offset);
 *
 * The game's `offset` is `&llGRJungleMapTaruCannFillAnimJoint` -- a
 * linker symbol's address, added to a map-file base pointer that this
 * port has no file for (src/dc/grjungle.c's header says so at length).
 * The port names the script instead. Everything else here is the
 * decomp's own list.
 *
 * dGRJungleTaruCannTransformKinds is declared because grjungle.c defines
 * it -- the decomp's own initialized data, kept per doctrine, dead in
 * this port for the reason that file's header gives.
 */
#ifndef _GRJUNGLE_H_
#define _GRJUNGLE_H_

#include <ssb_types.h>
#include <sys/objdef.h>
#include <gr/grdef.h>

extern DObjTransformTypes dGRJungleTaruCannTransformKinds[];

extern void grJungleTaruCannAddAnimOffset(GObj *ground_gobj, const char *name);
extern void grJungleTaruCannAddAnimFill(GObj *ground_gobj);
extern void grJungleTaruCannAddAnimShoot(GObj *ground_gobj);
extern void grJungleTaruCannUpdateMove(GObj *ground_gobj);
extern void grJungleTaruCannUpdateRotate(GObj *ground_gobj);
extern void grJungleTaruCannProcUpdate(GObj *ground_gobj);
extern void grJungleMakeTaruCann(void);
extern GObj* grJungleMakeGround(void);
extern sb32 grJungleTaruCannCheckGetDamageKind(GObj *ground_gobj, GObj *fighter_gobj, s32 *kind);
extern void grJungleTaruCannGetPosition(Vec3f *pos);
extern f32 grJungleTaruCannGetRotate(void);

#endif

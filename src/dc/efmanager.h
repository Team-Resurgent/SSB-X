/* efmanager.h -- the port's own declaration beside ef/efmanager.c's
 * (ef/efmanager.h carries the game's): the effect bank loader, called
 * where the game calls efManagerInitEffects by every scene that makes
 * effects -- the battle and the results screen
 * (src/dc/efmanager.c efManagerLoadEffectBank). */
#ifndef SSB_DC_EFMANAGER_H
#define SSB_DC_EFMANAGER_H

#include <ef/effect.h>          /* GObj, LBParticle, Vec3f for the three below */

void efManagerLoadEffectBank(void);

/* Load every model effect's pack now rather than when the effect is first
 * made, so a match reads nothing off the disc (src/dc/efmanager.c). The
 * same for weapons (src/dc/wpattrs.h) and items (src/dc/itemmodel.h). */
void efManagerPreloadModels(void);

/* The shield's three effects:
 * the guard (ftcommon.c) calls them where the game does. ef/efmanager.h
 * declares the last two; the first the decomp declares nowhere, so it
 * is declared here beside them. */
GObj* efManagerShieldMakeEffect(GObj *fighter_gobj);
GObj* efManagerYoshiShieldMakeEffect(GObj *fighter_gobj);
LBParticle* efManagerEggBreakMakeEffect(Vec3f *pos);
LBParticle* efManagerSparkleWhiteMakeEffect(Vec3f *pos);
/* Samus's Bomb spark: the same plain-particle shape as
 * SparkleWhite above, a different script id. */
LBParticle* efManagerSparkleWhiteMultiExplodeMakeEffect(Vec3f *pos);
LBParticle* efManagerImpactShockMakeEffect(Vec3f *pos, s32 size);
/* The Poké Ball's opening rays: a model effect out of the
 * COMMON bank (EFCommonEffects3), the same file the halo, the quake, the
 * orbs and the slash come from -- so unlike the fighters' effects this one
 * needs no per-battle bank of its own. itMBallOpenInitVars calls it once
 * and KEEPS the GObj. ef/efmanager.h declares it; this is the port's own
 * beside it, for the same reason as the rest of this block. */
GObj* efManagerMBallRaysMakeEffect(Vec3f *pos);
/* The item pickup swirl (the item-use step): the rays' shape out of the
 * same common bank, made by itMainSetFighterHold at the hand the item
 * has gone into. ef/efmanager.h declares it too; this is here for the
 * same reason as the rest of the block. */
GObj* efManagerItemGetSwirlProcUpdate(Vec3f *pos);
GObj* efManagerFoxReflectorMakeEffect(GObj *fighter_gobj);
LBParticle* efManagerFireGrindMakeEffect(Vec3f *pos);
LBParticle* efManagerFoxBlasterGlowMakeEffect(Vec3f *pos);
/* Purin's Sing: a model-path effect like the shield pair
 * above, not a particle one. */
GObj* efManagerPurinSingMakeEffect(GObj *fighter_gobj);
/* Falcon Punch's flame: the same model-path shape as Sing. */
GObj* efManagerCaptainFalconPunchMakeEffect(GObj *fighter_gobj);
/* Falcon Kick's dust trail: the same model-path shape too. */
GObj* efManagerCaptainFalconKickMakeEffect(GObj *fighter_gobj);

#endif /* SSB_DC_EFMANAGER_H */

/* ftcommontwister.c -- ft/ftcommon/ftcommontwister.c, verbatim: the four
 * functions a fighter runs while Hyrule's tornado has it.
 *
 * SetStatus is where a fighter is caught: it drops a heavy item, lets go
 * of whoever it was holding or capturing, is put into the air, and goes
 * to nFTCommonStatusTwister with full capture immunity so nothing else
 * can touch it while it spins. ProcUpdate counts the sixty tics it
 * spends in there and then hands it to ShootFighter. ProcPhysics is what
 * it looks like: the fighter is dragged around the funnel's own position
 * at a radius that grows with the countdown, up in a rising spiral,
 * capped at 50 units a tic and facing tangentially. ShootFighter is the
 * throw out -- knockback off the twister's own hit parameters, and the
 * sixty-tick cooldown (fp->twister_wait) that keeps the same funnel from
 * catching the same fighter again the moment it lands.
 *
 * Its caller is ft/ftmain.c's
 * ftMainSetHitHazard, which routes nGMHitEnvironmentTwister here, and
 * ftMainSearchHitHazard runs the registered hazard over every fighter
 * every tic. The producer is
 * src/dc/grhyrule.c's twister, which registers the callback.
 *
 * DIVERGES, one, in ShootFighter. The decomp reads the twister's hit
 * parameters out of the ROM's own GRHyruleMap file, which it holds as
 * gMPCollisionGroundData with the FTThrowHitDesc 0xA8 bytes past it, and
 * reaches them with a subtraction between two linker symbols:
 *
 *   (FTThrowHitDesc*) (((uintptr_t)gMPCollisionGroundData -
 *        (intptr_t)&llGRHyruleMapMapHeader) +
 *        (intptr_t)&llGRHyruleMapTwisterThrowHitDesc)
 *
 * This port's stage pack (src/dc/stage.h) is its own format and has no
 * GRHyruleMap file in memory, and neither symbol exists to subtract, so
 * the two-operand dance has nothing to compute. The desc is its own
 * object here instead, holding the decomp's own values, taken from its
 * own decoded source rather than from the ROM
 * (ssb-decomp-re/src/relocData/265_GRHyruleMap.c:74,
 * dGRHyruleMap_TwisterThrow_HitDesc). Same data, reached directly --
 * which is what the arithmetic above comes to on the machine that has
 * the file. The TWISTER_POSITION and event data the twister itself needs
 * are reached through mpCollision's map-object tables, which the port's
 * pack does carry, so those are the game's reads, unaltered.
 */

#include <ft/fighter.h>
#include <ft/ftcommon.h>
#include <it/item.h>
#include <gr/ground.h>
#include <sc/scene.h>

/* func_800269C0_275C0, the FGM call. No decomp header declares it -- the
 * decomp relies on IDO's implicit declaration -- so the port's own
 * (src/dc/ftcommon.h:260) is the one used, rather than letting the call
 * fall through to an implicit int return. */
#include "ftcommon.h"

/* 265_GRHyruleMap.c:74 dGRHyruleMap_TwisterThrow_HitDesc, the decomp's
 * own decoded values. status_id 2, damage 14, angle 90 (straight up),
 * knockback_scale 60, knockback_weight 0, knockback_base 115, element 0
 * (nGMHitElementNone). See this file's header for why it is a port
 * object and not a read. */
static FTThrowHitDesc sFTCommonTwisterThrowHitDesc = { 2, 14, 90, 60, 0, 115, 0 };

/* ftcommontwister.c:9-18 0x801439D0 ftCommonTwisterProcUpdate, verbatim:
 * sixty tics of riding the funnel, counted in the status's own
 * release_wait. FTCOMMON_TORNADO_RELEASE_WAIT is 60.0F and the field is
 * a float, so the comparison is the game's own float one. */
void ftCommonTwisterProcUpdate(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    fp->status_vars.common.twister.release_wait++;

    if (fp->status_vars.common.twister.release_wait >= FTCOMMON_TORNADO_RELEASE_WAIT)
    {
        ftCommonTwisterShootFighter(fighter_gobj);
    }
}

/* ftcommontwister.c:21-53 0x80143A20 ftCommonTwisterProcPhysics, verbatim:
 * where the fighter is drawn -- dragged around the funnel's own position
 * on a spiral whose radius is `(400 * 1/60th-seconds-so-far + 100)/2`
 * and whose angle turns 1800 degrees per second, rising 500 units a
 * second. The velocity handed to the fighter is the difference between
 * that point and where it is, capped at 50 units a tic, and its own
 * rotation follows the same angle. angle_d is the release_wait times
 * 1/60, so this is a function of time in the funnel rather than of a
 * per-tic step.
 *
 * `unused[2]` is the decomp's own local, kept per this port's doctrine
 * (itbombhei.c:219 and friends). */
void ftCommonTwisterProcPhysics(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    GObj *tornado_gobj = fp->status_vars.common.twister.tornado_gobj;
    Vec3f pos = DObjGetStruct(tornado_gobj)->translate.vec.f;
    Vec3f vel;
    f32 mul;
    f32 angle_d;
    f32 mag;
    f32 unused[2];

    angle_d = (fp->status_vars.common.twister.release_wait * 0.016666668F);
    mul = (((400.0F * angle_d) + 100.0F) * 0.5F);

    pos.x += (mul * lbCommonCos(F_CLC_DTOR32(1800.0F * angle_d)));
    pos.z += (mul * lbCommonSin(F_CLC_DTOR32(1800.0F * angle_d)));
    pos.y += 500.0F * angle_d;

    syVectorDiff3D(&vel, &pos, &DObjGetStruct(fighter_gobj)->translate.vec.f);

    mag = syVectorMag3D(&vel);

    if (mag > 50.0F)
    {
        syVectorScale3D(&vel, 50.0F / mag);
    }
    fp->physics.vel_air = vel;

    DObjGetStruct(fighter_gobj)->rotate.vec.f.y = (fp->lr * F_CLC_DTOR32(90.0F)) + F_CLC_DTOR32(1800.0F * angle_d);
}

/* ftcommontwister.c:56-89 0x80143BC4 ftCommonTwisterSetStatus, verbatim.
 * The catch itself, and the order matters: the voice is cut first, a
 * heavy item is dropped out of the fighter's hands, whatever it was
 * holding or being held by is released (the two arms are exclusive -- a
 * fighter is either the one throwing or the one being thrown, never
 * both), and only then is the status set. Grounded fighters are put in
 * the air first, because the twister status is an aerial one.
 *
 * fp->twister_wait is NOT set here -- it is ShootFighter's, at the other
 * end, and it is what keeps the funnel from re-catching the fighter the
 * tic it comes out. `unused` is the decomp's, as above. */
void ftCommonTwisterSetStatus(GObj *fighter_gobj, GObj *tornado_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    ftParamStopVoiceRunProcDamage(fighter_gobj);

    if ((fp->item_gobj != NULL) && (itGetStruct(fp->item_gobj)->weight == nITWeightHeavy))
    {
        ftSetupDropItem(fp);
    }
    if (fp->catch_gobj != NULL)
    {
        ftCommonThrownSetStatusDamageRelease(fp->catch_gobj);

        fp->catch_gobj = NULL;
    }
    else if (fp->capture_gobj != NULL)
    {
        ftCommonThrownDecideFighterLoseGrip(fp->capture_gobj, fighter_gobj);
    }
    if (fp->ga == nMPKineticsGround)
    {
        mpCommonSetFighterAir(fp);
    }
    ftMainSetStatus(fighter_gobj, nFTCommonStatusTwister, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);
    ftMainPlayAnimEventsAll(fighter_gobj);
    ftPhysicsStopVelAll(fighter_gobj);

    fp->status_vars.common.twister.release_wait = 0;
    fp->status_vars.common.twister.tornado_gobj = tornado_gobj;

    ftParamSetCaptureImmuneMask(fp, FTCATCHKIND_MASK_ALL);
    func_800269C0_275C0(nSYAudioFGMHyruleTwisterTrapped);
}

/* ftcommontwister.c:92-114 0x80143CC4 ftCommonTwisterShootFighter,
 * verbatim but for the hit-descriptor read (see this file's header). The
 * throw out: z is flattened to the funnel's own plane first, knockback
 * comes off the twister's damage numbers through the common knockback
 * curve, and the damage the fighter actually takes is zero if it was
 * already in a hit status of its own -- the same "trade" rule every other
 * environmental hit follows. 1P mode logs the hit as a ground-environment
 * twister; the cooldown is set last, so a fighter that lands inside the
 * funnel is safe for sixty tics. */
void ftCommonTwisterShootFighter(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    FTThrowHitDesc *tornado = &sFTCommonTwisterThrowHitDesc;
    f32 knockback;
    s32 damage;

    DObjGetStruct(fighter_gobj)->translate.vec.f.z = 0.0F;

    knockback = ftParamGetCommonKnockback(fp->percent_damage, tornado->damage, tornado->damage, tornado->knockback_weight, tornado->knockback_scale, tornado->knockback_base, fp->attr->weight, 9, fp->handicap);

    if (ftParamGetBestHitStatusAll(fighter_gobj) != nGMHitStatusNormal)
    {
        damage = 0;
    }
    else damage = tornado->damage;

    ftCommonDamageInitDamageVars(fighter_gobj, -1, damage, knockback, tornado->angle, fp->lr, 0, tornado->element, 0, TRUE, TRUE, TRUE);
    ftParamUpdate1PGameDamageStats(fp, GMCOMMON_PLAYERS_MAX, nFTHitLogObjectGround, nGMHitEnvironmentTwister, 0, 0);

    if (damage != 0)
    {
        ftParamUpdateDamage(fp, damage);
    }
    fp->twister_wait = FTCOMMON_TORNADO_PICKUP_WAIT;
}

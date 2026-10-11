/* ftcommontaru.c -- ft/ftcommon/ftcommontarucann.c, verbatim: the five
 * functions a fighter runs while Kongo Jungle's barrel cannon has it.
 *
 * SetStatus is the catch: the voice is cut, a heavy item is dropped, a
 * held or held-by fighter is released, and the fighter goes to
 * nFTCommonStatusTaruCann with full capture immunity, invisible and
 * intangible -- it is inside the barrel, so nothing may touch it. The
 * status is a GROUND status, unlike the twister's, so the fighter is not
 * put in the air and its physics proc owns its position outright:
 * ProcPhysics is one line putting the fighter at the cannon's own DObj.
 * ProcUpdate is the two-ended timer -- it waits
 * FTCOMMON_TARUCANN_RELEASE_WAIT tics, then tells the cannon to play its
 * shoot animation (grJungleTaruCannAddAnimShoot) and starts
 * FTCOMMON_TARUCANN_SHOOT_WAIT running; half way through that the FGM
 * fires, and at its end ShootFighter throws the fighter out with an
 * angle that follows however far the cannon has rotated.
 *
 * ftMainSetHitHazard has been routing nGMHitEnvironmentTaruCann here,
 * and this SetStatus was a no-op stub in src/dc/ftcommon.c. The pattern is
 * the same as ftParamMakeRumble and ftCommonTwisterSetStatus: the callers
 * were finished long before the callee.
 *
 * DIVERGES, one, in ShootFighter, the same one ftcommontwister.c makes
 * and for the same reason: the decomp reads the cannon's hit parameters
 * out of the ROM's own GRJungleMap file, which it holds as
 * gMPCollisionGroundData with the FTThrowHitDesc 0xA8 bytes past it,
 *
 *   (FTThrowHitDesc*) (((uintptr_t)gMPCollisionGroundData -
 *        (intptr_t)&llGRJungleMapMapHeader) +
 *        (intptr_t)&llGRJungleMapTaruCannThrowHitDesc)
 *
 * and this port's stage pack has no GRJungleMap file in memory, so the
 * two-operand dance has nothing to compute. The desc is its own object
 * here instead, holding the decomp's own values, taken from its own
 * decoded source rather than from the ROM
 * (ssb-decomp-re/src/relocData/261_GRJungleMap.c:80,
 * dGRJungleMap_TaruCannThrow_HitDesc). Same data, reached directly --
 * which is what the arithmetic above comes to on the machine that has
 * the file.
 *
 * The knockback curve is NOT the twister's:
 * ftParamGetGroundHazardKnockback, the one function in ft/ftparam.c the
 * decomp's own comment marks "Used by Barrel Cannon on Kongo Jungle",
 * whose weight-zero arm is a different curve from the common one. It was
 * not in the port either; this step added it (src/dc/ftparam.c).
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

/* gr/grcommon/grjungle.h's grJungleTaruCannAddAnimShoot and
 * grJungleTaruCannGetRotate, which ProcUpdate and ShootFighter call. The
 * port's gr/ground.h shim stops short of the stage headers (src/dc/
 * grhyrule.c says why), so the one this file needs is named here. */
#include <gr/grcommon/grjungle.h>

/* 261_GRJungleMap.c:80 dGRJungleMap_TaruCannThrow_HitDesc, the decomp's
 * own decoded values. status_id 3, damage 0, angle 90 (straight up),
 * knockback_scale 0, knockback_weight 0, knockback_base 180, element 0
 * (nGMHitElementNone). See this file's header for why it is a port
 * object and not a read. */
static FTThrowHitDesc sFTCommonTaruCannThrowHitDesc = { 3, 0, 90, 0, 0, 180, 0 };

/* ftcommontarucann.c:9-37 0x80143E10 ftCommonTaruCannProcUpdate, verbatim.
 * Two timers, and the second runs only after the first has ended -- the
 * `shoot_wait != 0` block is the whole of the shooting half, so the
 * release countdown and the shoot countdown never overlap. The FGM is
 * the decomp's own `== FTCOMMON_TARUCANN_SHOOT_WAIT / 2`, i.e. half way
 * through the wait, not one tic before the end. */
void ftCommonTaruCannProcUpdate(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if (fp->status_vars.common.tarucann.shoot_wait != 0)
    {
        fp->status_vars.common.tarucann.shoot_wait--;

        if (fp->status_vars.common.tarucann.shoot_wait == (FTCOMMON_TARUCANN_SHOOT_WAIT / 2))
        {
            func_800269C0_275C0(nSYAudioFGMJungleTaruCannShoot);
        }
        if (fp->status_vars.common.tarucann.shoot_wait == 0)
        {
            ftCommonTaruCannShootFighter(fighter_gobj);

            return;
        }
    }
    fp->status_vars.common.tarucann.release_wait++;

    if ((fp->status_vars.common.tarucann.release_wait >= FTCOMMON_TARUCANN_RELEASE_WAIT) && (fp->status_vars.common.tarucann.shoot_wait == 0))
    {
        fp->status_vars.common.tarucann.shoot_wait = FTCOMMON_TARUCANN_SHOOT_WAIT;

        grJungleTaruCannAddAnimShoot(fp->status_vars.common.tarucann.tarucann_gobj);
    }
}

/* ftcommontarucann.c:40-49 0x80143EB0 ftCommonTaruCannProcInterrupt,
 * verbatim: the player may skip the wait and fire early, on either
 * attack button, but only while the shoot countdown has not started. */
void ftCommonTaruCannProcInterrupt(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if ((fp->status_vars.common.tarucann.shoot_wait == 0) && (fp->input.pl.button_tap & (fp->input.button_mask_a | fp->input.button_mask_b)))
    {
        fp->status_vars.common.tarucann.shoot_wait = FTCOMMON_TARUCANN_SHOOT_WAIT;

        grJungleTaruCannAddAnimShoot(fp->status_vars.common.tarucann.tarucann_gobj);
    }
}

/* ftcommontarucann.c:52-58 0x80143F04 ftCommonTaruCannProcPhysics,
 * verbatim: the whole of the status's motion. The fighter is wherever
 * the cannon is, which is whatever its own anim joints have moved its
 * root to -- the reason the port's cannon has a real DObj tree. */
void ftCommonTaruCannProcPhysics(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    GObj *tarucann_gobj = fp->status_vars.common.tarucann.tarucann_gobj;

    DObjGetStruct(fighter_gobj)->translate.vec.f = DObjGetStruct(tarucann_gobj)->translate.vec.f;
}

/* ftcommontarucann.c:61-90 0x80143F30 ftCommonTaruCannSetStatus, verbatim.
 * The catch. The order matters the same way the twister's does: the
 * voice is cut first, a heavy item is dropped out of the fighter's
 * hands, whatever it was holding or being held by is released (the two
 * arms are exclusive), and only then is the status set. Where the
 * twister puts a grounded fighter in the air first, this one does not:
 * the barrel cannon's status is a ground status and its own physics proc
 * places the fighter from the cannon. */
void ftCommonTaruCannSetStatus(GObj *fighter_gobj, GObj *tarucann_gobj)
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
    ftMainSetStatus(fighter_gobj, nFTCommonStatusTaruCann, 0.0F, 0.0F, FTSTATUS_PRESERVE_NONE);
    ftMainPlayAnimEventsAll(fighter_gobj);
    ftPhysicsStopVelAll(fighter_gobj);

    fp->status_vars.common.tarucann.shoot_wait = 0;
    fp->status_vars.common.tarucann.release_wait = 0;
    fp->status_vars.common.tarucann.tarucann_gobj = tarucann_gobj;

    ftParamSetHitStatusAll(fighter_gobj, nGMHitStatusIntangible);

    fp->is_invisible = TRUE;

    ftParamSetCaptureImmuneMask(fp, FTCATCHKIND_MASK_ALL);
    func_800269C0_275C0(nSYAudioFGMJungleTaruCannEnter);
}

/* ftcommontarucann.c:93-116 0x80144038 ftCommonTaruCannShootFighter,
 * verbatim but for the hit-descriptor read (see this file's header). The
 * throw out: z is flattened to the cannon's own plane first, the
 * knockback comes off the cannon's own numbers through the ground-hazard
 * curve, and the angle is the cannon's own rotation folded into the
 * fighter's facing -- a cannon rotated clockwise throws to the left of
 * where a fighter facing the same way would expect, which is the `*-1`
 * on `fp->lr`. 1P mode logs the hit as a ground-environment tarucann;
 * the sixteen-tic pickup cooldown is set last, so a fighter thrown out
 * cannot fall straight back in. */
void ftCommonTaruCannShootFighter(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    FTThrowHitDesc *tarucann = &sFTCommonTaruCannThrowHitDesc;
    f32 knockback;
    s32 angle;

    DObjGetStruct(fighter_gobj)->translate.vec.f.z = 0.0F;

    knockback = ftParamGetGroundHazardKnockback(fp->percent_damage, tarucann->damage, tarucann->damage, tarucann->knockback_weight, tarucann->knockback_scale, tarucann->knockback_base, fp->attr->weight, 9, 9);

    angle = ((I_CLC_RTOD32(grJungleTaruCannGetRotate()) * -fp->lr) + 90);
    angle -= (angle / 360) * 360;

    ftCommonDamageInitDamageVars(fighter_gobj, nFTCommonStatusDamageFlyRoll, tarucann->damage, knockback, angle, fp->lr, 0, tarucann->element, 0, TRUE, TRUE, FALSE);
    ftParamUpdate1PGameDamageStats(fp, GMCOMMON_PLAYERS_MAX, nFTHitLogObjectGround, nGMHitEnvironmentTaruCann, 0, 0);

    fp->playertag_wait = 0;
    fp->tarucann_wait = FTCOMMON_TARUCANN_PICKUP_WAIT;
}

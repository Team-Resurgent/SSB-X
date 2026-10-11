/* grhyrule.c -- gr/grcommon/grhyrule.c, verbatim: Hyrule Castle's
 * tornado, the first stage hazard this port has ported.
 *
 * The twister is a GObj on nGCCommonLinkIDGround with a seven-state
 * machine (Sleep, Wait, Summon, Move, Turn, Stop, Subside) and a real
 * lbParticle generator riding it. It waits 1600 to 2800 tics, spawns at
 * one of the map file's nMPMapObjKindTwister positions, moves back and
 * forth along the floor line it landed on (bounded by that line's two
 * edges, pulled 300 units in when a wall runs under them), and picks a
 * direction from whichever side of it the fighters on its own floor line
 * are standing on. Fighters it touches are thrown by the game's own
 * hazard path, not by anything here: it registers
 * grHyruleTwisterCheckGetDamageKind with ftMainCheckAddGroundObstacle,
 * ftMainSearchHitHazard calls it every tic for every fighter, and the
 * nGMHitEnvironmentTwister it answers with lands in
 * ftCommonTwisterSetStatus (src/dc/ftcommontwister.c).
 *
 * The consumer half (ftCommonTwisterSetStatus) is wired to the real
 * SetStatus. This file is the producer, so the hazard is whole.
 *
 * Everything it calls is already in the build: mpCollision's map-object
 * and floor queries (mp/mpcollision.c, unmodified), lbParticle
 * (src/dc/lbparticle.c), efParticleGetLoadBankID over
 * lGRHyruleParticleScriptBank* (src/game/ssb64/particlebanks.ld gives the
 * four symbols their ROM addresses, and syDmaReadRom turns each back into
 * the file tools/export/ssb_particleexport.py cut from the same range), and the
 * FGM (nSYAudioFGMHyruleTwisterAppear/Trapped).
 *
 * DIVERGES: gp->twister_pos_ids is the one pointer the twister keeps
 * across tics, and it comes off syTaskmanMalloc, the same scene heap
 * every other ported subsystem allocates from. grMainSetupMakeGround is
 * not called from here -- the port's stage setup calls
 * grHyruleMakeGround directly when the bound stage is nGRKindHyrule
 * (src/dc/stage.c), because it has no dGRMainSetupProcMakeList to index.
 * And the bank load in grHyruleTwisterInitVars is skipped in the host
 * build, which cannot do it at all; see the comment at that site.
 *
 * grHyruleTwisterInitVars' own error arm is the decomp's: a stage with no
 * twister positions loops on syDebugPrintf forever. That is reachable
 * only on a map file that has none, and the port's Hyrule pack carries
 * 26 map objects, so it is kept verbatim rather than softened.
 */

#include <gr/ground.h>
/* the decomp's grhyrule.c reaches these through gr/ground.h ->
 * gr/grfunctions.h, which pulls every stage's own header; this port's
 * gr/ground.h is the collision files' shim and stops short of that, so
 * the one it needs is named here. */
#include <gr/grcommon/grhyrule.h>
#include <ft/fighter.h>
#include <ef/effect.h>
#include <sc/scene.h>
#include <lb/lbparticle.h>
/* the port's own lbparticle.h beside the decomp's, for the four observers
 * it declares -- lb_particle_scripts/lb_particle_textures are what the
 * bank-load line at the foot of grHyruleTwisterInitVars reads. Quoted
 * rather than angled because src/dc/ is what -I../../dc names. */
#include "lbparticle.h"
#include <mp/mpcollision.h>
#include <sys/taskman.h>
#include <sys/utils.h>
#include <gm/gmsound.h>

/* func_800269C0_275C0, the FGM call grHyruleTwisterUpdateSummon ends on.
 * No decomp header declares it -- the decomp relies on IDO's implicit
 * declaration -- so the port's own (src/dc/ftcommon.h:260) is the one
 * used, the same way src/dc/ftcommontwister.c does it, rather than
 * letting the call fall through to an implicit int return. */
#include "ftcommon.h"
#include "lbpartex.h"          /* lbpTexLoadBank */

/* // // // // // // // // // // // //
 *                               //
 *          ENUMERATORS          //
 *                               //
 * // // // // // // // // // // // */

enum grHyruleTwisterStatus
{
    nGRHyruleTwisterStatusSleep,
    nGRHyruleTwisterStatusWait,
    nGRHyruleTwisterStatusSummon,
    nGRHyruleTwisterStatusMove,
    nGRHyruleTwisterStatusTurn,
    nGRHyruleTwisterStatusStop,
    nGRHyruleTwisterStatusSubside
};

/* // // // // // // // // // // // //
 *                               //
 *           FUNCTIONS           //
 *                               //
 * // // // // // // // // // // // */

/* grhyrule.c:35-59 0x8010A140 grHyruleTwisterMakeEffect, verbatim. The
 * twister's own particle script, on the bank grHyruleTwisterInitVars
 * loaded -- effect id 3 is the funnel itself and 7 the puff it leaves
 * behind when the fighter escapes it. A transform is added and the
 * particle processed before the caller is told where to put it, and both
 * failures unwind rather than returning a half-made effect. */
LBParticle* grHyruleTwisterMakeEffect(Vec3f *pos, s32 effect_id)
{
    LBParticle *pc = lbParticleMakeScriptID(gGRCommonStruct.hyrule.particle_bank_id | LBPARTICLE_MASK_GENLINK(0), effect_id);

    if (pc != NULL)
    {
        LBTransform *xf = lbParticleAddTransformForStruct(pc, nLBTransformStatusDefault);

        if (xf == NULL)
        {
            lbParticleEjectStruct(pc);

            return NULL;
        }
        LBParticleProcessStruct(pc);

        if (xf->users_num == 0)
        {
            return NULL;
        }
        xf->translate = *pos;
    }
    return pc;
}

/* grhyrule.c:62-118 0x8010A1E4 grHyruleMakeTwister, verbatim. Places the
 * funnel on the floor line under `pos` -- y is the projection distance,
 * not an absolute height -- then works out how far it may travel: each
 * of the line's two edges, pulled 300 units inward when the edge has a
 * wall running under it (nMPLineKindLWall on the right, nMPLineKindRWall
 * on the left), so the funnel turns before it walks off a ledge into a
 * wall. Returns NULL, and leaves no GObj behind, if there is no floor
 * under the position or no particle bank to draw it with. */
GObj* grHyruleMakeTwister(Vec3f *pos)
{
    s32 line_id;
    f32 floor_dist;
    GObj *twister_gobj;
    DObj *twister_dobj;
    LBParticle *pc;
    Vec3f edge_pos;
    s32 edge_under;

    if (mpCollisionCheckProjectFloor(pos, &line_id, &floor_dist, NULL, NULL) == FALSE)
    {
        return NULL;
    }
    twister_gobj = gcMakeGObjSPAfter(nGCCommonKindGround, NULL, nGCCommonLinkIDGround, GOBJ_PRIORITY_DEFAULT);

    if (twister_gobj != NULL)
    {
        twister_dobj = gcAddDObjForGObj(twister_gobj, NULL);

        twister_dobj->translate.vec.f = *pos;

        twister_dobj->translate.vec.f.y += floor_dist;

        pc = grHyruleTwisterMakeEffect(&twister_dobj->translate.vec.f, 3);

        if (pc == NULL)
        {
            gGRCommonStruct.hyrule.twister_xf = NULL;

            gcEjectGObj(twister_gobj);

            return NULL;
        }
        gGRCommonStruct.hyrule.twister_line_id = line_id;

        mpCollisionGetFloorEdgeL(line_id, &edge_pos);

        edge_under = mpCollisionGetEdgeUnderLLineID(line_id);

        if ((edge_under == -1) || (mpCollisionGetLineTypeID(edge_under) != nMPLineKindRWall))
        {
            gGRCommonStruct.hyrule.twister_leftedge_x = edge_pos.x;
        }
        else gGRCommonStruct.hyrule.twister_leftedge_x = edge_pos.x + 300.0F;

        mpCollisionGetFloorEdgeR(line_id, &edge_pos);

        edge_under = mpCollisionGetEdgeUnderRLineID(line_id);

        if ((edge_under == -1) || (mpCollisionGetLineTypeID(edge_under) != nMPLineKindLWall))
        {
            gGRCommonStruct.hyrule.twister_rightedge_x = edge_pos.x;
        }
        else gGRCommonStruct.hyrule.twister_rightedge_x = edge_pos.x - 300.0F;

        gGRCommonStruct.hyrule.twister_xf = pc->xf;
    }
    return twister_gobj;
}

/* grhyrule.c:121-127 0x8010A36C grHyruleTwisterUpdateSleep, verbatim: it
 * does nothing at all until the match leaves its Wait status, and then
 * waits a random 1600-2800 tics before its first appearance. */
void grHyruleTwisterUpdateSleep(void)
{
    if (gSCManagerBattleState->game_status != nSCBattleGameStatusWait)
    {
        gGRCommonStruct.hyrule.twister_status = nGRHyruleTwisterStatusWait;
        gGRCommonStruct.hyrule.twister_wait = syUtilsRandIntRange(1200) + 1600;
    }
}

/* grhyrule.c:130-158 0x8010A3B4 grHyruleTwisterUpdateWait, verbatim:
 * pick one of the map file's twister positions and spawn there. A
 * position with no floor or no particle under it leaves the wait at 80
 * tics and does not advance the status, which is the decomp's own
 * retry -- the whole spawn is attempted again at the next countdown's
 * end rather than the state machine being told it failed. */
void grHyruleTwisterUpdateWait(void)
{
    Vec3f pos;
    GObj *twister_gobj;

    gGRCommonStruct.hyrule.twister_wait--;

    if (gGRCommonStruct.hyrule.twister_wait == 0)
    {
        mpCollisionGetMapObjPositionID(gGRCommonStruct.hyrule.twister_pos_ids[syUtilsRandIntRange(gGRCommonStruct.hyrule.twister_pos_count)], &pos);

        twister_gobj = grHyruleMakeTwister(&pos);

        if (twister_gobj == NULL)
        {
            gGRCommonStruct.hyrule.twister_wait = syUtilsRandIntRange(1200) + 1600;
        }
        else
        {
            gGRCommonStruct.hyrule.twister_wait = 80;
            gGRCommonStruct.hyrule.twister_gobj = twister_gobj;
            gGRCommonStruct.hyrule.twister_status = nGRHyruleTwisterStatusSummon;
        }
    }
}

/* grhyrule.c:161-182 0x8010A444 grHyruleTwisterUpdateSummon, verbatim:
 * the eighty tics of warning. On the last one it picks a side to drift
 * toward (10 units a tic), draws its own speed-change window, hand the
 * funnel to the fighter system as a ground obstacle, and plays the
 * appear sound -- which is the first moment a fighter can be caught. */
void grHyruleTwisterUpdateSummon(void)
{
    s32 lr;

    gGRCommonStruct.hyrule.twister_wait--;

    if (gGRCommonStruct.hyrule.twister_wait == 0)
    {
        gGRCommonStruct.hyrule.twister_status = nGRHyruleTwisterStatusMove;
        gGRCommonStruct.hyrule.twister_wait = syUtilsRandIntRange(600) + 520;

        lr = ((syUtilsRandUShort() % 2) != 0) ? +1 : -1;

        gGRCommonStruct.hyrule.twister_turn_wait = 0;
        gGRCommonStruct.hyrule.twister_vel = lr * 10.0F;
        gGRCommonStruct.hyrule.twister_speed_wait = syUtilsRandIntRange(120) + 180;

        ftMainCheckAddGroundObstacle(gGRCommonStruct.hyrule.twister_gobj, grHyruleTwisterCheckGetDamageKind);

        func_800269C0_275C0(nSYAudioFGMHyruleTwisterAppear);
    }
}

/* grhyrule.c:185-198 0x8010A4F4 grHyruleTwisterDecLifetimeCheckStop,
 * verbatim: the shared countdown Move and Turn both run. TRUE means the
 * funnel has lived out its `twister_wait` and moves to Stop, which is
 * where it waits for whoever it has caught. */
sb32 grHyruleTwisterDecLifetimeCheckStop(void)
{
    gGRCommonStruct.hyrule.twister_wait--;

    if (gGRCommonStruct.hyrule.twister_wait == 0)
    {
        gGRCommonStruct.hyrule.twister_status = nGRHyruleTwisterStatusStop;

        return TRUE;
    }
    else return FALSE;
}

/* grhyrule.c:201-238 0x8010A52C grHyruleTwisterGetLR, verbatim: which
 * way to drift, off the fighters standing on the funnel's own floor
 * line. Every one of them on the x-right of the funnel counts for +1 and
 * every one on the left for -1; a tie is a coin toss and an empty floor
 * line is 0, which the caller reads as "nothing to chase". */
s32 grHyruleTwisterGetLR(void)
{
    s32 players_rside = 0;
    s32 players_lside = 0;
    GObj *fighter_gobj = gGCCommonLinks[nGCCommonLinkIDFighter];
    f32 twister_pos_x = DObjGetStruct(gGRCommonStruct.hyrule.twister_gobj)->translate.vec.f.x;

    while (fighter_gobj != NULL)
    {
        FTStruct *fp = ftGetStruct(fighter_gobj);

        if ((fp->ga == nMPKineticsGround) && (fp->coll_data.floor_line_id == gGRCommonStruct.hyrule.twister_line_id))
        {
            if (fp->joints[nFTPartsJointTopN]->translate.vec.f.x > twister_pos_x)
            {
                players_rside++;
            }
            else players_lside++;
        }
        fighter_gobj = fighter_gobj->link_next;
    }
    if ((players_lside != 0) || (players_rside != 0))
    {
        if (players_lside == players_rside)
        {
            return ((syUtilsRandUShort() % 2) != 0) ? -1 : +1;
        }
        else if (players_rside < players_lside)
        {
            return -1;
        }
        else return +1;
    }
    else return 0;
}

/* grhyrule.c:241-300 0x8010A610 grHyruleTwisterUpdateMove, verbatim. Two
 * timers run under the movement: twister_turn_wait is a committed turn
 * (the funnel keeps its slow 10-unit drift through it and doubles its
 * speed when it expires), and twister_speed_wait is how long until it
 * will consider a new one -- a 1-in-5 chance, then a random 300-480 tic
 * turn at 50 units a tic toward the fighters. Reaching either edge
 * bounces (status Turn, 120 tics) and the funnel is re-seated on its
 * floor line every tic, so it walks the shape of the ground. */
void grHyruleTwisterUpdateMove(void)
{
    Vec3f *pos = &DObjGetStruct(gGRCommonStruct.hyrule.twister_gobj)->translate.vec.f;
    f32 ground_level;
    f32 pos_x;
    s32 lr;

    if (grHyruleTwisterDecLifetimeCheckStop() == FALSE)
    {
        if (gGRCommonStruct.hyrule.twister_turn_wait != 0)
        {
            gGRCommonStruct.hyrule.twister_turn_wait--;

            if (gGRCommonStruct.hyrule.twister_turn_wait == 0)
            {
                if (gGRCommonStruct.hyrule.twister_vel < 0.0F)
                {
                    gGRCommonStruct.hyrule.twister_vel = -10.0F;
                }
                else gGRCommonStruct.hyrule.twister_vel = 10.0F;
            }
        }
        else
        {
            gGRCommonStruct.hyrule.twister_speed_wait--;

            if (gGRCommonStruct.hyrule.twister_speed_wait == 0)
            {
                lr = grHyruleTwisterGetLR();

                if ((lr != 0) && (syUtilsRandIntRange(5) == 0))
                {
                    gGRCommonStruct.hyrule.twister_turn_wait = syUtilsRandIntRange(180) + 300;
                    gGRCommonStruct.hyrule.twister_vel = lr * 50.0F;
                }
            }
        }
        pos_x = pos->x + gGRCommonStruct.hyrule.twister_vel;

        if ((gGRCommonStruct.hyrule.twister_rightedge_x < pos_x) || (pos_x < gGRCommonStruct.hyrule.twister_leftedge_x))
        {
            if (gGRCommonStruct.hyrule.twister_rightedge_x < pos_x)
            {
                pos->x = (gGRCommonStruct.hyrule.twister_rightedge_x - 10.0F);
            }
            else pos->x = (gGRCommonStruct.hyrule.twister_leftedge_x + 10.0F);

            gGRCommonStruct.hyrule.twister_status = nGRHyruleTwisterStatusTurn;
            gGRCommonStruct.hyrule.twister_turn_wait = 120;
        }
        else pos->x = pos_x;

        mpCollisionGetFCCommonFloor(gGRCommonStruct.hyrule.twister_line_id, pos, &ground_level, NULL, NULL);

        pos->y += ground_level;

        gGRCommonStruct.hyrule.twister_xf->translate = *pos;
    }
}

/* grhyrule.c:303-311 0x8010A7CC grHyruleTwisterUpdateTurn, verbatim: the
 * 120 tics after a bounce. Nothing moves during them, only the countdown
 * (a lifetime countdown like Move's, so a funnel can still expire here),
 * and the velocity is reversed on the tic it reaches zero. */
void grHyruleTwisterUpdateTurn(void)
{
    if (grHyruleTwisterDecLifetimeCheckStop() == FALSE)
    {
        gGRCommonStruct.hyrule.twister_turn_wait--;

        if (gGRCommonStruct.hyrule.twister_turn_wait == 0)
        {
            gGRCommonStruct.hyrule.twister_status = nGRHyruleTwisterStatusMove;
            gGRCommonStruct.hyrule.twister_turn_wait = 0;
            gGRCommonStruct.hyrule.twister_vel = -gGRCommonStruct.hyrule.twister_vel;
        }
    }
}

/* grhyrule.c:314-336 0x8010A824 grHyruleTwisterUpdateStop, verbatim. The
 * funnel's life is over but it will not leave while anybody is still
 * caught in it -- it spins here until the last fighter's status is no
 * longer nFTCommonStatusTwister. Then it unregisters itself as a hazard
 * (so the last trapped fighter's own escape throw is not a second one),
 * puffs, and ejects its own GObj. */
void grHyruleTwisterUpdateStop(void)
{
    GObj *fighter_gobj = gGCCommonLinks[nGCCommonLinkIDFighter];

    while (fighter_gobj != NULL)
    {
        FTStruct *fp = ftGetStruct(fighter_gobj);

        if (fp->status_id != nFTCommonStatusTwister)
        {
            fighter_gobj = fighter_gobj->link_next;
        }
        else return;
    }
    gGRCommonStruct.hyrule.twister_status = nGRHyruleTwisterStatusSubside;
    gGRCommonStruct.hyrule.twister_wait = 32;

    ftMainClearGroundObstacle(gGRCommonStruct.hyrule.twister_gobj);

    if (gGRCommonStruct.hyrule.twister_xf != NULL)
    {
        grHyruleTwisterMakeEffect(&gGRCommonStruct.hyrule.twister_xf->translate, 7);
    }
    gcEjectGObj(gGRCommonStruct.hyrule.twister_gobj);
}

/* grhyrule.c:339-353 0x8010A8B4 grHyruleTwisterUpdateSubside, verbatim:
 * thirty-two tics of the last puff of the funnel the GObj was ejected
 * out from under, and then back to Wait. The particle generator is
 * ejected here and not in UpdateStop because it outlives the GObj. */
void grHyruleTwisterUpdateSubside(void)
{
    gGRCommonStruct.hyrule.twister_wait--;

    if (gGRCommonStruct.hyrule.twister_wait == 0)
    {
        gGRCommonStruct.hyrule.twister_status = nGRHyruleTwisterStatusWait;
        gGRCommonStruct.hyrule.twister_wait = syUtilsRandIntRange(1200) + 1600;

        if (gGRCommonStruct.hyrule.twister_xf != NULL)
        {
            lbParticleEjectStructID(gGRCommonStruct.hyrule.twister_xf->generator_id, 1);
        }
    }
}

/* grhyrule.c:356-390 0x8010A91C grHyruleTwisterProcUpdate, verbatim: the
 * seven-way dispatch, on the ground GObj's own process. */
void grHyruleTwisterProcUpdate(GObj *ground_gobj)
{
    switch (gGRCommonStruct.hyrule.twister_status)
    {
    case nGRHyruleTwisterStatusSleep:
        grHyruleTwisterUpdateSleep();
        break;

    case nGRHyruleTwisterStatusWait:
        grHyruleTwisterUpdateWait();
        break;

    case nGRHyruleTwisterStatusSummon:
        grHyruleTwisterUpdateSummon();
        break;

    case nGRHyruleTwisterStatusMove:
        grHyruleTwisterUpdateMove();
        break;

    case nGRHyruleTwisterStatusTurn:
        grHyruleTwisterUpdateTurn();
        break;

    case nGRHyruleTwisterStatusStop:
        grHyruleTwisterUpdateStop();
        break;

    case nGRHyruleTwisterStatusSubside:
        grHyruleTwisterUpdateSubside();
        break;
    }
}

/* grhyrule.c:393-428 0x8010A9C8 grHyruleTwisterInitVars, verbatim: the
 * stage's twister positions, as a copy of the map objects' own IDs (the
 * map file's table is read once and the array the state machine indexes
 * is its own, off the scene heap), then the status to Sleep and the
 * particle bank loaded. The count check is the decomp's own halt -- see
 * this file's header for why it is kept. */
void grHyruleTwisterInitVars(void)
{
    s32 i;
    s32 pos_count;
    s32 pos_ids[10];

    gGRCommonStruct.hyrule.twister_pos_count = pos_count = mpCollisionGetMapObjCountKind(nMPMapObjKindTwister);

    if ((pos_count == 0) || (pos_count > ARRAY_COUNT(pos_ids)))
    {
        while (TRUE)
        {
            syDebugPrintf("Twister positions are error!\n");
            scManagerRunPrintGObjStatus();
        }
    }
    gGRCommonStruct.hyrule.twister_pos_ids = (u8*) syTaskmanMalloc(pos_count * sizeof(*gGRCommonStruct.hyrule.twister_pos_ids), 0x0);

    mpCollisionGetMapObjIDsKind(nMPMapObjKindTwister, pos_ids);

    for (i = 0; i < pos_count; i++)
    {
        gGRCommonStruct.hyrule.twister_pos_ids[i] = pos_ids[i];
    }
    gGRCommonStruct.hyrule.twister_status = nGRHyruleTwisterStatusSleep;

#ifdef FT_HOSTTEST
    /* DIVERGES, and only in this build, for the reason
     * efManagerLoadEffectBank states at length (src/dc/efmanager.c): on
     * x86-64 an `LBScript *` is eight bytes and the bank's own arrays are
     * four, so lbParticleSetupBankID would walk this file at twice the
     * stride and write off the end of the block. The bank is therefore
     * not loaded here -- and the host test cannot have the real one,
     * because the shape of the data is the 32-bit machine's, not the
     * code's.
     *
     * What the host suite does instead (hosttest_ft.c
     * host_load_particle_bank) is build bank 0 out of the same two files
     * with the arithmetic done at the right width, and it adds the
     * funnel's generator to it, so the whole seven-state machine and
     * grHyruleTwisterMakeEffect run for real in that build. Left at zero
     * rather than -1 for the reason ftmanager.c gives: a zero id at least
     * names a bank that exists there. */
    gGRCommonStruct.hyrule.particle_bank_id = 0;
#else
    gGRCommonStruct.hyrule.particle_bank_id = efParticleGetLoadBankID
    (
        (uintptr_t)&lGRHyruleParticleScriptBankLo,
        (uintptr_t)&lGRHyruleParticleScriptBankHi,
        (uintptr_t)&lGRHyruleParticleTextureBankLo,
        (uintptr_t)&lGRHyruleParticleTextureBankHi
    );
    /* DIVERGES: the textures, bound by name as src/dc/mntitle.c binds
     * the title's. efParticleGetLoadBankID walks the bank's scripts and
     * its N64 textures, but the PVR draws from the pre-converted
     * grhyrule.txp, and lbpdraw.c skips any particle whose image has no
     * bound PVR texture -- so without this the funnel was never drawn at
     * all, though it caught fighters. */
    if (lbpTexLoadBank(gGRCommonStruct.hyrule.particle_bank_id, "grhyrule") != 0)
    {
        syDebugPrintf("hyrule: grhyrule.txp did not load\n");
    }
#endif

    /* The port's one line, not the decomp's, and the only thing this
     * function does that the game does not: the same trade
     * src/dc/efmanager.c's efManagerLoadEffectBank makes, for the same
     * reason and in the same shape (its "particles:" line). The target is
     * the only place the bank walk can be checked at all, and these are
     * the numbers a serial log can hold it to -- how many twister
     * positions the stage's own map file answered with (the decomp's
     * count check wants 1..10) and what lbParticleSetupBankID's walk
     * found in the two files: grhyrule_scb.c declares 8 scripts and
     * grhyrule_txb.c 3 textures, so a run that prints the bank id and
     * those two counts has run the whole thing. A stage whose positions
     * came back empty never reaches this line; it halts on the decomp's
     * own loop above, so anything printed here means the check passed.
     * Disc probe: `--serial`, grep "twister:". */
    syDebugPrintf("twister: %d position(s), particle bank %d --"
                  " %d scripts, %d textures\n",
                  (int)pos_count,
                  (int)gGRCommonStruct.hyrule.particle_bank_id,
                  (int)lb_particle_scripts(gGRCommonStruct.hyrule.particle_bank_id),
                  (int)lb_particle_textures(gGRCommonStruct.hyrule.particle_bank_id));
}

/* grhyrule.c:431-438 0x8010AB20 grHyruleMakeGround, verbatim: Hyrule's
 * own ground GObj, on link 1 alongside the stage's, carrying the twister
 * state machine at process priority 4 -- the same priority the port's
 * own grCommonSetupInitAll gives mpCollisionAdvanceUpdateTic, and the
 * same as every other stage's ground process in the game. */
GObj* grHyruleMakeGround(void)
{
    GObj *ground_gobj = gcMakeGObjSPAfter(nGCCommonKindGround, NULL, nGCCommonLinkIDGround, GOBJ_PRIORITY_DEFAULT);

    gcAddGObjProcess(ground_gobj, grHyruleTwisterProcUpdate, nGCProcessKindFunc, 4);
    grHyruleTwisterInitVars();

    return ground_gobj;
}

/* grhyrule.c:441-466 0x8010AB74 grHyruleTwisterCheckGetDamageKind,
 * verbatim: the ground-hazard callback ftMainSearchHitHazard calls for
 * every fighter every tic. A fighter is caught when it is off its own
 * twister cooldown, is not already in one, is not twister-immune, and
 * has no hit status of its own to spend -- and then only inside a 300
 * unit wide, 600 up / 300 down box around the funnel.
 *
 * The box is measured from the funnel's own DObj, so it is the same
 * position the particle transform is at, and the y test is asymmetric on
 * purpose: the funnel reaches further up than down. */
sb32 grHyruleTwisterCheckGetDamageKind(GObj *ground_gobj, GObj *fighter_gobj, s32 *kind)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    f32 dist_y;
    f32 dist_x;

    if
    (
        (fp->twister_wait == 0)                                &&
        (fp->status_id != nFTCommonStatusTwister)              &&
        !(fp->capture_immune_mask & FTCATCHKIND_MASK_TWISTER)  &&
        (ftParamGetBestHitStatusAll(fighter_gobj) == nGMHitStatusNormal)
    )
    {
        DObj *gr_dobj = DObjGetStruct(ground_gobj);
        DObj *ft_dobj = DObjGetStruct(fighter_gobj);

        if (gr_dobj->translate.vec.f.x < ft_dobj->translate.vec.f.x)
        {
            dist_x = -(gr_dobj->translate.vec.f.x - ft_dobj->translate.vec.f.x);
        }
        else dist_x = (gr_dobj->translate.vec.f.x - ft_dobj->translate.vec.f.x);

        dist_y = ft_dobj->translate.vec.f.y - gr_dobj->translate.vec.f.y;

        if ((dist_x < 300.0F) && (dist_y < 600.0F) && (dist_y > -300.0F))
        {
            *kind = nGMHitEnvironmentTwister;

            return TRUE;
        }
    }
    return FALSE;
}

/* grhyrule.c:469-477 0x8010AC70 grHyruleTwisterCheckGetPosition, verbatim:
 * where the funnel is, if it is on the move. Its one caller in the game
 * is ft/ftcomputer.c's own hazard avoidance, which the port has not
 * ported yet, so this is dead code today -- driven by the host test and
 * kept per this port's standing doctrine. */
sb32 grHyruleTwisterCheckGetPosition(Vec3f *pos)
{
    if ((gGRCommonStruct.hyrule.twister_status == nGRHyruleTwisterStatusMove) || (gGRCommonStruct.hyrule.twister_status == nGRHyruleTwisterStatusTurn))
    {
        *pos = DObjGetStruct(gGRCommonStruct.hyrule.twister_gobj)->translate.vec.f;

        return TRUE;
    }
    else return FALSE;
}

/* grpupupu.c -- gr/grcommon/grpupupu.c, verbatim: Dream Land's Whispy
 * Woods, the port's fifth stage hazard.
 *
 * Whispy is three machines in one GObj's process. The WIND machine
 * (Sleep/Wait/Turn/Open/Blow/Stop) decides when he blows: a random 960
 * to 2100 tics of waiting, then he faces whichever side of the map has
 * more fighters (grPupupuWhispyGetLR), opens his mouth, and blows for a
 * random 240 to 320 tics. The BLOW itself is grPupupuWhispySetWindPush,
 * a per-fighter box test that adds a push to coll_data.vel_push that
 * falls off with distance from GRPUPUPU_WHISPY_POS_X, signed by the
 * facing. On top of that sit the FACE machine (the eyes' turn and blink
 * timers) and the two FLOWER machines, whose whole job is to swap the
 * texture the mouth and eyes are drawn with as the blow runs.
 *
 * The trees are the map file's, and there are FOUR of them: the Whispy
 * skeleton and eyes (which is the tree MPGroundData.map_nodes names),
 * the mouth, and the flowers behind and in front. Each is its own GObj
 * built off its own DObjDesc block, which is why this stage is the one
 * the port's MPK1 block had to grow a list of objects for
 * (src/dc/stage.h). The eyes' and the mouth's materials are ANIMATED,
 * and the two flower GObjs exist only to play a texture-swapping script.
 *
 * DIVERGES, four:
 *
 *  - The map reads are the pack's. The game reaches every tree and every
 *    script as `map_head + &llGRPupupuMap<Name>`, a linker symbol the
 *    port has no address space for; here they are stage_bind_map_object
 *    and stage_map_anim, by name.
 *
 *  - The MatAnimJoints are the pack's MObj `alt`s, not scripts handed to
 *    gcAddAnimAll's third parameter. This is the same trade Yoshi's
 *    Island's clouds make and for the same reason: the port bakes an
 *    object's materials into MObjs and picks a state with
 *    stage_map_set_anim, where the game re-adds the whole tree's table.
 *    The eyes carry two alts and the mouth eight, in the order the
 *    stage table lists them.
 *
 *  - The anim calls are `#ifdef FT_HOSTTEST`-guarded, for
 *    grJungleMakeTaruCann's reason: `union AObjEvent32` is eight bytes
 *    on x86-64, so the interpreter walks a script twice as fast as it
 *    should and never terminates. The host checks the exported words
 *    through stage_map_anim instead; the target is the only place the
 *    trees actually move.
 *
 *  - Every object draws with gcDrawDObjTreeForGObj, where the game gives
 *    the flowers-in-front GObj grDisplayLayer3PriProcDisplay. The port's
 *    packs carry their own display, so which bucket a tree lands in is
 *    the baker's, not this file's.
 *
 * dGRPupupuWhispyLeavesEffectAttributes and
 * dGRPupupuWhispyDustEffectPositions are the file's own initialized
 * data, ported verbatim.
 */

#include <gr/ground.h>
/* the decomp's grpupupu.c reaches these through gr/ground.h ->
 * gr/grfunctions.h; this port's gr/ground.h is the collision files'
 * shim and stops short of that, so the ones it needs are named here. */
#include <gr/grcommon/grpupupu.h>
#include <gr/grvars.h>            /* GRPUPUPU_*, GRPupupuEffect */
#include <ft/fighter.h>
#include <lb/lbparticle.h>
#include <ef/effect.h>
#include <sc/scene.h>
#include <sys/utils.h>

#include "ftcommon.h"
#include "lbpartex.h"          /* lbpTexLoadBank */
#include "stage.h"

/* // // // // // // // // // // // //
 *                               //
 *          ENUMERATORS          //
 *                               //
 * // // // // // // // // // // // */

enum grPupupuWhispyWindStatus
{
    nGRPupupuWhispyWindStatusSleep,
    nGRPupupuWhispyWindStatusWait,
    nGRPupupuWhispyWindStatusTurn,
    nGRPupupuWhispyWindStatusOpen,
    nGRPupupuWhispyWindStatusBlow,
    nGRPupupuWhispyWindStatusStop,
    nGRPupupuWhispyWindStatusEnumCount
};

enum grPupupuWhispyMouthStatus
{
    nGRPupupuWhispyMouthStatusStretch,
    nGRPupupuWhispyMouthStatusTurn,
    nGRPupupuWhispyMouthStatusOpen,
    nGRPupupuWhispyMouthStatusClose,
    nGRPupupuWhispyMouthStatusEnumCount
};

enum grPupupuWhispyMouthTexture
{
    nGRPupupuWhispyMouthTextureOpen,
    nGRPupupuWhispyMouthTextureBlow,
    nGRPupupuWhispyMouthTextureClose,
    nGRPupupuWhispyMouthTextureEnumCount
};

enum grPupupuWhispyEyesStatus
{
    nGRPupupuWhispyEyesStatusTurn,
    nGRPupupuWhispyEyesStatusBlink,
    nGRPupupuWhispyEyesStatusEnumCount
};

enum grPupupuWhispyEyesTexture
{
    nGRPupupuWhispyEyesTexture0,
    nGRPupupuWhispyEyesTexture1,
    nGRPupupuWhispyEyesTexture2,
    nGRPupupuWhispyEyesTextureEnumCount
};

enum grPupupuFlowerStatus
{
    nGRPupupuFlowerStatusDefault,
    nGRPupupuFlowerStatusWindStart,
    nGRPupupuFlowerStatusWindLoopStart,
    nGRPupupuFlowerStatusWindLoop,
    nGRPupupuFlowerStatusWindLoopEnd,
    nGRPupupuFlowerStatusWindStop,
    nGRPupupuFlowerStatusEnumCount
};

/* The four map objects' indices into the stage pack, in the order
 * grPupupuInitAll builds them -- which is also the order the export's
 * `map_object` list names them. grPupupuUpdateBlink's own `map_gobj[0]`
 * and grPupupuWhispyUpdateTurn's `map_gobj[1]` are the eyes' and the
 * mouth's, so these four have to stay in step with the stage table. */
enum grPupupuMapObject
{
    nGRPupupuMapObjectWhispyEyes,
    nGRPupupuMapObjectWhispyMouth,
    nGRPupupuMapObjectFlowersBack,
    nGRPupupuMapObjectFlowersFront,
    nGRPupupuMapObjectEnumCount
};

/* // // // // // // // // // // // //
 *                               //
 *       INITIALIZED DATA        //
 *                               //
 * // // // // // // // // // // // */

/* grpupupu.c:74-78 0x8012E870 dGRPupupuWhispyEyesAnims, verbatim but for
 * what its two slots hold. The game's are `&llGRPupupuMapWhispyEyes
 * LeftTurnAnimJoint` and `...LeftTurnMatAnimJoint`, addresses into the
 * map file it adds to map_head; the port's are the first the pack's
 * script of that name and the second the MatAnimJoint `alt` its own
 * pack's MObjs want, so slot [1] is an index here where the game's is an
 * offset. -1 is the decomp's own `0x0`: the blink has no material.
 *
 * [side][status][2]: side 0 is the left-facing set and 1 the right,
 * which is also the order the stage table's `matanim` list uses. */
static intptr_t dGRPupupuWhispyEyesAnims[nGRPupupuWhispyEyesStatusEnumCount][2][2];

/* The names behind dGRPupupuWhispyEyesAnims, in the same shape. */
static const char *dGRPupupuWhispyEyesAnimNames
    [nGRPupupuWhispyEyesStatusEnumCount][2] =
{
    { "WhispyEyesLeftTurn",  "WhispyEyesRightTurn"  },
    { "WhispyEyesLeftBlink", "WhispyEyesRightBlink" },
};

/* grpupupu.c:81-98 0x8012E890 dGRPupupuWhispyMouthAnims, the same
 * reduction. The mouth's four states per side are the stage table's
 * `matanim` list in order, so an entry's alt is `side * 4 + status`. */
static intptr_t
    dGRPupupuWhispyMouthAnims[nGRPupupuWhispyMouthStatusEnumCount][2][2];

static const char *dGRPupupuWhispyMouthAnimNames
    [nGRPupupuWhispyMouthStatusEnumCount][2] =
{
    { "WhispyMouthLeftStretch", "WhispyMouthRightStretch" },
    { "WhispyMouthLeftTurn",    "WhispyMouthRightTurn"    },
    { "WhispyMouthLeftOpen",    "WhispyMouthRightOpen"    },
    { "WhispyMouthLeftClose",   "WhispyMouthRightClose"   },
};

/* grpupupu.c:101-105 0x8012E8D0 dGRPupupuWhispyMouthTextures and
 * grpupupu.c:108-112 0x8012E8E8 dGRPupupuWhispyEyesTextures, verbatim:
 * the two texture-swap tables. Each entry is a `Texture` block the game
 * hands gcAddAnimJointAll, and the port's pack carries each under the
 * decomp's own symbol name (`WhispyMouthLeftOpenTexture`), because two
 * of them share a plain name with an AnimJoint in the same group.
 *
 * [side][texture]: the mouth's three are the mouth's own textures on the
 * flowers BEHIND Whispy, and the eyes' three are the eyes' on the
 * flowers IN FRONT -- which is what the game does, and looks backwards
 * until you see the two GObjs are just script players. */
static const char *dGRPupupuWhispyMouthTextureNames
    [nGRPupupuWhispyMouthTextureEnumCount][2] =
{
    { "WhispyMouthLeftOpenTexture",  "WhispyMouthRightOpenTexture"  },
    { "WhispyMouthLeftBlowTexture",  "WhispyMouthRightBlowTexture"  },
    { "WhispyMouthLeftCloseTexture", "WhispyMouthRightCloseTexture" },
};

static const char *dGRPupupuWhispyEyesTextureNames
    [nGRPupupuWhispyEyesTextureEnumCount][2] =
{
    { "WhispyEyesLeft0Texture", "WhispyEyesRight0Texture" },
    { "WhispyEyesLeft1Texture", "WhispyEyesRight1Texture" },
    { "WhispyEyesLeft2Texture", "WhispyEyesRight2Texture" },
};

static AObjEvent32 *
    dGRPupupuWhispyMouthTextures[nGRPupupuWhispyMouthTextureEnumCount][2];
static AObjEvent32 *
    dGRPupupuWhispyEyesTextures[nGRPupupuWhispyEyesTextureEnumCount][2];

/* grpupupu.c:115-119 0x8012E900 dGRPupupuWhispyLeavesEffectAttributes,
 * verbatim: where the leaves the blow throws start, and how far they are
 * turned, per facing. */
GRPupupuEffect dGRPupupuWhispyLeavesEffectAttributes[] =
{
    { { -715.0F, 450.0F, -696.0F }, F_CLC_DTOR32(-157.0F) },
    { { -205.0F, 450.0F, -762.0F }, F_CLC_DTOR32( -13.0F) }
};

/* grpupupu.c:122-126 0x8012E920 dGRPupupuWhispyDustEffectPositions,
 * verbatim: where the dust the front flowers kick up starts. */
Vec3f dGRPupupuWhispyDustEffectPositions[] =
{
    { -715.0F, 100.0F, 0.0F },
    { -205.0F, 100.0F, 0.0F }
};

/* // // // // // // // // // // // //
 *                               //
 *           FUNCTIONS           //
 *                               //
 * // // // // // // // // // // // */

/* grpupupu.c:135-162 0x801058E0 grPupupuWhispyGetLR, verbatim: which way
 * Whispy should face, counted off the fighters' own root joint positions
 * against his own x. -1 is "equal on both sides", which the caller reads
 * as "keep the facing you have"; 0 is left and 1 is right. */
s32 grPupupuWhispyGetLR(GObj *ground_gobj)
{
    s32 players_rside = 0;
    s32 players_lside = 0;
    GObj *fighter_gobj = gGCCommonLinks[nGCCommonLinkIDFighter];

    (void)ground_gobj;

    while (fighter_gobj != NULL)
    {
        FTStruct *fp = ftGetStruct(fighter_gobj);

        if (fp->joints[nFTPartsJointTopN]->translate.vec.f.x > GRPUPUPU_WHISPY_POS_X)
        {
            players_rside++;
        }
        else players_lside++;

        fighter_gobj = fighter_gobj->link_next;
    }
    if (players_rside == players_lside)
    {
        return -1;
    }
    else if (players_rside < players_lside)
    {
        return 0;
    }
    else return 1;
}

/* grpupupu.c:165-203 0x8010595C grPupupuWhispySetWindPush, verbatim: the
 * blow. Every fighter inside the windbox -- a y band and an x band on
 * the facing side -- takes a push that is
 * GRPUPUPU_WHISPY_WIND_VEL_BASE minus the distance from Whispy times
 * GRPUPUPU_WHISPY_WIND_DIST_DECAY, signed by the facing. A push that
 * comes out at or below zero is dropped, so the falloff is what bounds
 * the wind rather than the box's edge. */
void grPupupuWhispySetWindPush(void)
{
    GObj *fighter_gobj = gGCCommonLinks[nGCCommonLinkIDFighter];
    sb32 lr_wind = gGRCommonStruct.pupupu.lr_players;

    while (fighter_gobj != NULL)
    {
        FTStruct *fp = ftGetStruct(fighter_gobj);
        DObj *joint = fp->joints[nFTPartsJointTopN];
        f32 dist_x;
        Vec3f push;
        f32 pos_x = joint->translate.vec.f.x, pos_y = joint->translate.vec.f.y;

        if ((pos_y <= GRPUPUPU_WHISPY_WINDBOX_TOP) && (pos_y >= GRPUPUPU_WHISPY_WINDBOX_BOTTOM))
        {
            if
            (
                ((lr_wind == 0) && (pos_x >= GRPUPUPU_WHISPY_WINDBOX_EDGELEFT)  && (pos_x <= GRPUPUPU_WHISPY_POS_X)) ||
                ((lr_wind != 0) && (pos_x <= GRPUPUPU_WHISPY_WINDBOX_EDGERIGHT) && (pos_x >= GRPUPUPU_WHISPY_POS_X))
            )
            {
                dist_x = ((pos_x < GRPUPUPU_WHISPY_POS_X) ? -(pos_x - GRPUPUPU_WHISPY_POS_X) : (pos_x - GRPUPUPU_WHISPY_POS_X));

                push.x = GRPUPUPU_WHISPY_WIND_VEL_BASE - (dist_x * GRPUPUPU_WHISPY_WIND_DIST_DECAY);

                if (push.x > 0.0F)
                {
                    push.z = 0.0F;
                    push.y = 0.0F;

                    push.x = (lr_wind == 0) ? -push.x : push.x;

                    ftParamSetVelPush(fighter_gobj, &push);
                }
            }
        }
        fighter_gobj = fighter_gobj->link_next;
    }
}

/* grpupupu.c:206-212 0x80105AF0 grPupupuWhispyUpdateSleep, verbatim:
 * Whispy does nothing at all until the match is actually running, which
 * is his state when the stage loads. */
void grPupupuWhispyUpdateSleep(void)
{
    if (gSCManagerBattleState->game_status != nSCBattleGameStatusWait)
    {
        gGRCommonStruct.pupupu.whispy_status = nGRPupupuWhispyWindStatusWait;
    }
}

/* grpupupu.c:215-247 0x80105B18 grPupupuWhispyLeavesMakeEffect, verbatim
 * but for what it reads the leaves' start from (below): one generator
 * out of the stage's own particle bank, script 0, with a transform
 * attached and parked at the facing's own position and angle. A NULL
 * generator, a NULL transform or a transform nothing claimed all eject
 * cleanly and leave leaves_xf NULL, which grPupupuWhispyUpdateBlow
 * checks before ejecting through it.
 *
 * DIVERGES, and only in the reading: the game indexes
 * dGRPupupuWhispyLeavesEffectAttributes by lr_players, and the port
 * does too; what differs is that the attributes' `rotate` is the whole
 * Vec3f-adjacent struct here, so the y write names the field. */
void grPupupuWhispyLeavesMakeEffect(void)
{
    LBParticle *pc;
    LBTransform *xf;

    xf = NULL;
    pc = lbParticleMakeScriptID(gGRCommonStruct.pupupu.particle_bank_id | LBPARTICLE_MASK_GENLINK(0), 0);

    if (pc != NULL)
    {
        xf = lbParticleAddTransformForStruct(pc, nLBTransformStatusReady);

        if (xf == NULL)
        {
            lbParticleEjectStruct(pc);
        }
        else
        {
            LBParticleProcessStruct(pc);

            if (xf->users_num == 0)
            {
                xf = NULL;
            }
            else
            {
                xf->translate = dGRPupupuWhispyLeavesEffectAttributes[gGRCommonStruct.pupupu.lr_players].pos;
                xf->rotate.y = dGRPupupuWhispyLeavesEffectAttributes[gGRCommonStruct.pupupu.lr_players].rotate;
            }
        }
    }
    gGRCommonStruct.pupupu.leaves_xf = xf;
}

/* grpupupu.c:250-277 0x80105BE8 grPupupuWhispyUpdateWait, verbatim: the
 * countdown between blows. When it runs out the facing is re-counted,
 * and Whispy either turns to face the other way (the eyes and mouth
 * switching to their Turn scripts as he does) or opens up where he
 * already is. -1 leaves the facing alone, which is the decomp's own
 * reading of GetLR. */
void grPupupuWhispyUpdateWait(void)
{
    if (gGRCommonStruct.pupupu.whispy_wind_wait != 0)
    {
        gGRCommonStruct.pupupu.whispy_wind_wait--;
    }
    else
    {
        s32 lr = grPupupuWhispyGetLR(gGRCommonStruct.pupupu.map_gobj[nGRPupupuMapObjectWhispyEyes]);

        if (lr == -1)
        {
            lr = gGRCommonStruct.pupupu.lr_players;
        }
        if (lr != gGRCommonStruct.pupupu.lr_players)
        {
            gGRCommonStruct.pupupu.whispy_eyes_status = nGRPupupuWhispyEyesStatusTurn;
            gGRCommonStruct.pupupu.whispy_mouth_status = nGRPupupuWhispyMouthStatusTurn;
            gGRCommonStruct.pupupu.whispy_status = nGRPupupuWhispyWindStatusTurn;
            gGRCommonStruct.pupupu.lr_players = lr;
        }
        else
        {
            gGRCommonStruct.pupupu.whispy_mouth_status = nGRPupupuWhispyMouthStatusOpen;
            gGRCommonStruct.pupupu.whispy_status = nGRPupupuWhispyWindStatusOpen;
        }
    }
}

/* grpupupu.c:280-287 0x80105C70 grPupupuWhispyUpdateTurn, verbatim: the
 * turn is over when the mouth tree's own script has run out. Both the
 * mouth and the eyes were set to Turn by the caller; only the mouth's
 * frame is watched. */
void grPupupuWhispyUpdateTurn(void)
{
    if (gGRCommonStruct.pupupu.map_gobj[nGRPupupuMapObjectWhispyMouth]->anim_frame <= 0.0F)
    {
        gGRCommonStruct.pupupu.whispy_mouth_status = nGRPupupuWhispyMouthStatusOpen;
        gGRCommonStruct.pupupu.whispy_status = nGRPupupuWhispyWindStatusOpen;
    }
}

/* grpupupu.c:290-306 0x80105CAC grPupupuWhispyUpdateOpen, verbatim: the
 * open script finishing is what starts the blow. The wind's length is
 * rolled here, both flower machines are told to start, the leaves
 * spawn, and the wind sound plays. */
void grPupupuWhispyUpdateOpen(void)
{
    if (gGRCommonStruct.pupupu.map_gobj[nGRPupupuMapObjectWhispyMouth]->anim_frame <= 0.0F)
    {
        gGRCommonStruct.pupupu.whispy_status = nGRPupupuWhispyWindStatusBlow;

        gGRCommonStruct.pupupu.flowers_back_status = gGRCommonStruct.pupupu.flowers_front_status = nGRPupupuFlowerStatusWindStart;

        gGRCommonStruct.pupupu.whispy_wind_duration = syUtilsRandIntRange(GRPUPUPU_WHISPY_WIND_DURATION_RANDOM) + GRPUPUPU_WHISPY_WIND_DURATION_BASE;

        gGRCommonStruct.pupupu.rumble_wait = 0;

        grPupupuWhispyLeavesMakeEffect();

        func_800269C0_275C0(nSYAudioFGMPupupuWhispyWind);
    }
}

/* grpupupu.c:309-318 0x80105D20 grPupupuWhispyUpdateWindRumble, verbatim:
 * the controller shake under the blow, one quake effect every
 * GRPUPUPU_WHISPY_WIND_RUMBLE_WAIT tics. The wait is armed AFTER the
 * effect, so the first one is on the blow's own first tic. */
void grPupupuWhispyUpdateWindRumble(void)
{
    if (gGRCommonStruct.pupupu.rumble_wait == 0)
    {
        efManagerQuakeMakeEffect(0);

        gGRCommonStruct.pupupu.rumble_wait = GRPUPUPU_WHISPY_WIND_RUMBLE_WAIT;
    }
    gGRCommonStruct.pupupu.rumble_wait--;
}

/* grpupupu.c:321-339 0x80105D6C grPupupuWhispyUpdateBlow, verbatim: the
 * blow counting down, the mouth closing behind it and the flower
 * machines told to end. The leaves generator is ejected through
 * `leaves_xf` -- once, at the end -- so the leaves stop with the wind.
 * The rumble runs every tic of the blow, this call's own tail. */
void grPupupuWhispyUpdateBlow(void)
{
    gGRCommonStruct.pupupu.whispy_wind_duration--;

    if (gGRCommonStruct.pupupu.whispy_wind_duration == 0)
    {
        gGRCommonStruct.pupupu.whispy_mouth_status = nGRPupupuWhispyMouthStatusClose;

        gGRCommonStruct.pupupu.flowers_back_status = gGRCommonStruct.pupupu.flowers_front_status = nGRPupupuFlowerStatusWindLoopEnd;

        gGRCommonStruct.pupupu.whispy_status = nGRPupupuWhispyWindStatusStop;

        if (gGRCommonStruct.pupupu.leaves_xf != NULL)
        {
            lbParticleEjectStructID(gGRCommonStruct.pupupu.leaves_xf->generator_id, 1);
        }
    }
    grPupupuWhispyUpdateWindRumble();
}

/* grpupupu.c:342-349 0x80105DD8 grPupupuWhispyUpdateStop, verbatim: the
 * close script finishing is what re-arms the wait, so the whole cycle is
 * gated on the mouth's own animation rather than on a timer. */
void grPupupuWhispyUpdateStop(void)
{
    if (gGRCommonStruct.pupupu.map_gobj[nGRPupupuMapObjectWhispyMouth]->anim_frame <= 0.0F)
    {
        gGRCommonStruct.pupupu.whispy_wind_wait = syUtilsRandIntRange(GRPUPUPU_WHISPY_WAIT_DURATION_RANDOM) + GRPUPUPU_WHISPY_WAIT_DURATION_BASE;
        gGRCommonStruct.pupupu.whispy_status = nGRPupupuWhispyWindStatusWait;
    }
}

/* grpupupu.c:352-372 0x80105E05 grPupupuWhispyUpdateBlink, verbatim, and
 * the file's only two oddities. It runs every tic from
 * grPupupuUpdateWhispyStatus' tail, whatever the wind is doing, and it
 * only acts while `whispy_eyes_status` is -1 -- i.e. while the eyes'
 * last script has already been handed over. The blink_wait counts down;
 * at exactly 0 or exactly -10 it blinks, which is how the -10 arm reads:
 * the timer has been SITTING at 0 and one tic later is -10, so the two
 * arms are "blink twice in a row" rather than a range.
 *
 * The mouth is only stretched if it is idle AND Whispy is not blowing,
 * and the timer is only re-rolled if this was not the -10 arm. */
void grPupupuWhispyUpdateBlink(void)
{
    if ((gGRCommonStruct.pupupu.whispy_eyes_status == -1) && (gGRCommonStruct.pupupu.map_gobj[nGRPupupuMapObjectWhispyEyes]->anim_frame <= 0.0F))
    {
        gGRCommonStruct.pupupu.whispy_blink_wait--;

        if ((gGRCommonStruct.pupupu.whispy_blink_wait == 0) || (gGRCommonStruct.pupupu.whispy_blink_wait == -10))
        {
            gGRCommonStruct.pupupu.whispy_eyes_status = nGRPupupuWhispyEyesStatusBlink;

            if ((gGRCommonStruct.pupupu.map_gobj[nGRPupupuMapObjectWhispyMouth]->anim_frame <= 0.0F) && (gGRCommonStruct.pupupu.whispy_status != nGRPupupuWhispyWindStatusBlow))
            {
                gGRCommonStruct.pupupu.whispy_mouth_status = nGRPupupuWhispyMouthStatusStretch;
            }
            if (gGRCommonStruct.pupupu.whispy_blink_wait != 0)
            {
                gGRCommonStruct.pupupu.whispy_blink_wait = syUtilsRandIntRange(GRPUPUPU_WHISPY_BLINK_WAIT_RANDOM) + GRPUPUPU_WHISPY_BLINK_WAIT_BASE;
            }
        }
    }
}

/* grpupupu.c:375-404 0x80105EF4 grPupupuUpdateWhispyStatus, verbatim: the
 * wind machine's six states, then the blink whatever the state. */
void grPupupuUpdateWhispyStatus(void)
{
    switch (gGRCommonStruct.pupupu.whispy_status)
    {
    case nGRPupupuWhispyWindStatusSleep:
        grPupupuWhispyUpdateSleep();
        break;

    case nGRPupupuWhispyWindStatusWait:
        grPupupuWhispyUpdateWait();
        break;

    case nGRPupupuWhispyWindStatusTurn:
        grPupupuWhispyUpdateTurn();
        break;

    case nGRPupupuWhispyWindStatusOpen:
        grPupupuWhispyUpdateOpen();
        break;

    case nGRPupupuWhispyWindStatusBlow:
        grPupupuWhispyUpdateBlow();
        break;

    case nGRPupupuWhispyWindStatusStop:
        grPupupuWhispyUpdateStop();
        break;
    }
    grPupupuWhispyUpdateBlink();
}

/* grpupupu.c:407-416 0x80105F94 grPupupuFlowersBackWindStart, verbatim:
 * the mouth's texture goes to Open on a timer, then the back flowers
 * hand over to their loop start. */
void grPupupuFlowersBackWindStart(void)
{
    gGRCommonStruct.pupupu.flowers_back_wait--;

    if (gGRCommonStruct.pupupu.flowers_back_wait == 0)
    {
        gGRCommonStruct.pupupu.whispy_mouth_texture = nGRPupupuWhispyMouthTextureOpen;
        gGRCommonStruct.pupupu.flowers_back_status = nGRPupupuFlowerStatusWindLoopStart;
    }
}

/* grpupupu.c:419-427 0x80105FC4 grPupupuFlowersBackLoopStart, verbatim:
 * the loop-start script running out swaps the mouth's texture to Blow --
 * which is the visual the whole machine is built around -- and hands to
 * the loop proper with a 15-tic wait. */
void grPupupuFlowersBackLoopStart(void)
{
    if (gGRCommonStruct.pupupu.map_gobj[nGRPupupuMapObjectFlowersBack]->anim_frame <= 0.0F)
    {
        gGRCommonStruct.pupupu.whispy_mouth_texture = nGRPupupuWhispyMouthTextureBlow;
        gGRCommonStruct.pupupu.flowers_back_status = nGRPupupuFlowerStatusWindLoop;
        gGRCommonStruct.pupupu.flowers_back_wait = 15;
    }
}

/* grpupupu.c:430-440 0x80106008 grPupupuFlowersBackLoopEnd, verbatim: the
 * blow is over, so the mouth's texture goes to Close and the flowers
 * stop. */
void grPupupuFlowersBackLoopEnd(void)
{
    gGRCommonStruct.pupupu.flowers_back_wait--;

    if (gGRCommonStruct.pupupu.flowers_back_wait == 0)
    {
        gGRCommonStruct.pupupu.whispy_mouth_texture = nGRPupupuWhispyMouthTextureClose;
        gGRCommonStruct.pupupu.flowers_back_status = nGRPupupuFlowerStatusWindStop;
        gGRCommonStruct.pupupu.flowers_back_wait = 15;
    }
}

/* grpupupu.c:443-459 0x80106044 grPupupuFlowersBackUpdateAll, verbatim:
 * three states, and no arm for WindLoop or WindStop -- the back flowers
 * do nothing at all between the loop starting and the blow ending. */
void grPupupuFlowersBackUpdateAll(void)
{
    switch (gGRCommonStruct.pupupu.flowers_back_status)
    {
    case nGRPupupuFlowerStatusWindStart:
        grPupupuFlowersBackWindStart();
        break;

    case nGRPupupuFlowerStatusWindLoopStart:
        grPupupuFlowersBackLoopStart();
        break;

    case nGRPupupuFlowerStatusWindLoopEnd:
        grPupupuFlowersBackLoopEnd();
        break;
    }
}

/* grpupupu.c:462-471 0x801060B0 grPupupuFlowersFrontWindStart, verbatim:
 * the front flowers' start swaps the EYES' texture to 0 on a timer, the
 * mirror of the back flowers' mouth texture. */
void grPupupuFlowersFrontWindStart(void)
{
    gGRCommonStruct.pupupu.flowers_front_wait--;

    if (gGRCommonStruct.pupupu.flowers_front_wait == 0)
    {
        gGRCommonStruct.pupupu.whispy_eyes_texture = nGRPupupuWhispyEyesTexture0;
        gGRCommonStruct.pupupu.flowers_front_status = nGRPupupuFlowerStatusWindLoopStart;
    }
}

/* grpupupu.c:474-507 0x801060E0 grPupupuWhispyDustMakeEffect, verbatim:
 * the leaves' twin, script 1 out of the same bank, its own position
 * table and its rotation signed by the facing (0 for the right-facing
 * set, 180 for the left). */
void grPupupuWhispyDustMakeEffect(void)
{
    LBParticle *pc;
    LBTransform *xf;

    xf = NULL;
    pc = lbParticleMakeScriptID(gGRCommonStruct.pupupu.particle_bank_id | LBPARTICLE_MASK_GENLINK(0), 1);

    if (pc != NULL)
    {
        xf = lbParticleAddTransformForStruct(pc, nLBTransformStatusReady);

        if (xf == NULL)
        {
            lbParticleEjectStruct(pc);
        }
        else
        {
            LBParticleProcessStruct(pc);

            if (xf->users_num == 0)
            {
                xf = NULL;
            }
            else
            {
                xf->translate = dGRPupupuWhispyDustEffectPositions[gGRCommonStruct.pupupu.lr_players];

                xf->rotate.y = (gGRCommonStruct.pupupu.lr_players == 1) ? 0.0F : F_CST_DTOR32(180.0F);
            }
        }
    }
    gGRCommonStruct.pupupu.dust_xf = xf;
}

/* grpupupu.c:510-520 0x801061CC grPupupuFlowersFrontLoopStart, verbatim:
 * the eyes' texture to 1, and the dust spawns here rather than at the
 * start of the blow. */
void grPupupuFlowersFrontLoopStart(void)
{
    if (gGRCommonStruct.pupupu.map_gobj[nGRPupupuMapObjectFlowersFront]->anim_frame <= 0.0F)
    {
        gGRCommonStruct.pupupu.whispy_eyes_texture = nGRPupupuWhispyEyesTexture1;
        gGRCommonStruct.pupupu.flowers_front_status = nGRPupupuFlowerStatusWindLoop;
        gGRCommonStruct.pupupu.flowers_front_wait = 22;

        grPupupuWhispyDustMakeEffect();
    }
}

/* grpupupu.c:523-539 0x80106220 grPupupuFlowersFrontLoopEnd, verbatim,
 * and the only place the WIND IS ACTUALLY PUSHED on a per-state basis:
 * the loop's own arms do it too (below), but this one is the last chance
 * before the status changes, so the push is on the else. The eyes'
 * texture goes to 2 and the dust is ejected at the same time. */
void grPupupuFlowersFrontLoopEnd(void)
{
    gGRCommonStruct.pupupu.flowers_front_wait--;

    if (gGRCommonStruct.pupupu.flowers_front_wait == 0)
    {
        gGRCommonStruct.pupupu.whispy_eyes_texture = nGRPupupuWhispyEyesTexture2;
        gGRCommonStruct.pupupu.flowers_front_status = nGRPupupuFlowerStatusWindStop;
        gGRCommonStruct.pupupu.flowers_front_wait = 22;

        if (gGRCommonStruct.pupupu.dust_xf != NULL)
        {
            lbParticleEjectStructID(gGRCommonStruct.pupupu.dust_xf->generator_id, 1);
        }
    }
    else grPupupuWhispySetWindPush();
}

/* grpupupu.c:542-562 0x80106290 grPupupuFlowersFrontUpdateAll, verbatim:
 * four states, and this is where the blow actually happens -- both the
 * loop's own arm and the loop-end's else push. The wind is therefore
 * gated on the FRONT flowers' machine, not on whispy_status, which is
 * why the push stops a few tics before the blow's own countdown does. */
void grPupupuFlowersFrontUpdateAll(void)
{
    switch (gGRCommonStruct.pupupu.flowers_front_status)
    {
    case nGRPupupuFlowerStatusWindStart:
        grPupupuFlowersFrontWindStart();
        break;

    case nGRPupupuFlowerStatusWindLoopStart:
        grPupupuFlowersFrontLoopStart();
        break;

    case nGRPupupuFlowerStatusWindLoop:
        grPupupuWhispySetWindPush();
        break;

    case nGRPupupuFlowerStatusWindLoopEnd:
        grPupupuFlowersFrontLoopEnd();
        break;
    }
}

/* grpupupu.c:565-625 0x80106314 grPupupuUpdateGObjAnims, verbatim but for
 * how the four trees are handed their scripts (see this file's header).
 * Each block is the decomp's own: the eyes and the mouth get an
 * AnimJoint AND a MatAnimJoint (gcAddAnimAll), the two flower GObjs a
 * plain AnimJoint set (gcAddAnimJointAll). Every one is gated on its own
 * status being set, and each is cleared to -1 as soon as it is handed
 * over, which is what makes these one-shots.
 *
 * The port's split: the AnimJoint goes to the game's own
 * gcAddAnimJointAll over a two-entry array (the tree's first joint's
 * script, then NULL), exactly as grJungleMakeTaruCann hands over the
 * cannon's; the MatAnimJoint is the pack's MObj `alt`, selected by
 * stage_map_set_anim. A -1 alt is the decomp's own 0x0 -- the eyes'
 * blink, which has no material at all. */
void grPupupuUpdateGObjAnims(void)
{
#ifdef FT_HOSTTEST
    /* DIVERGES, and only in this build: AObjEvent32 is eight bytes on
     * x86-64, so the game's interpreter cannot be given any of these
     * scripts. The host test checks the exported words through
     * stage_map_anim instead, and drives the state machine with
     * anim_frame set by hand -- the same shape every gr/ test has had
     * since Hyrule. */
    static const s32 ids[] = {
        nGRPupupuMapObjectWhispyEyes, nGRPupupuMapObjectWhispyMouth,
        nGRPupupuMapObjectFlowersBack, nGRPupupuMapObjectFlowersFront,
    };

    gGRCommonStruct.pupupu.whispy_eyes_status = -1;
    gGRCommonStruct.pupupu.whispy_mouth_status = -1;
    gGRCommonStruct.pupupu.whispy_mouth_texture = -1;
    gGRCommonStruct.pupupu.whispy_eyes_texture = -1;

    (void)ids;
#else
    if (gGRCommonStruct.pupupu.whispy_eyes_status != -1)
    {
        s32 st = gGRCommonStruct.pupupu.whispy_eyes_status;
        s32 lr = gGRCommonStruct.pupupu.lr_players;
        AObjEvent32 *anim[STAGE_MAP_JOINTS_MAX];

        stage_map_anim_array(dGRPupupuWhispyEyesAnimNames[st][lr], anim);

        gcAddAnimAll(gGRCommonStruct.pupupu.map_gobj[nGRPupupuMapObjectWhispyEyes], anim, NULL, 0.0F);
        gcPlayAnimAll(gGRCommonStruct.pupupu.map_gobj[nGRPupupuMapObjectWhispyEyes]);

        stage_map_set_anim(DObjGetStruct(gGRCommonStruct.pupupu.map_gobj[nGRPupupuMapObjectWhispyEyes]),
                           nGRPupupuMapObjectWhispyEyes,
                           (s32)dGRPupupuWhispyEyesAnims[gGRCommonStruct.pupupu.whispy_eyes_status][gGRCommonStruct.pupupu.lr_players][1]);

        gGRCommonStruct.pupupu.whispy_eyes_status = -1;
    }
    if (gGRCommonStruct.pupupu.whispy_mouth_status != -1)
    {
        s32 st = gGRCommonStruct.pupupu.whispy_mouth_status;
        s32 lr = gGRCommonStruct.pupupu.lr_players;
        AObjEvent32 *anim[STAGE_MAP_JOINTS_MAX];

        stage_map_anim_array(dGRPupupuWhispyMouthAnimNames[st][lr], anim);

        gcAddAnimAll(gGRCommonStruct.pupupu.map_gobj[nGRPupupuMapObjectWhispyMouth], anim, NULL, 0.0F);
        gcPlayAnimAll(gGRCommonStruct.pupupu.map_gobj[nGRPupupuMapObjectWhispyMouth]);

        stage_map_set_anim(DObjGetStruct(gGRCommonStruct.pupupu.map_gobj[nGRPupupuMapObjectWhispyMouth]),
                           nGRPupupuMapObjectWhispyMouth,
                           (s32)dGRPupupuWhispyMouthAnims[gGRCommonStruct.pupupu.whispy_mouth_status][gGRCommonStruct.pupupu.lr_players][1]);

        gGRCommonStruct.pupupu.whispy_mouth_status = -1;
    }
    if (gGRCommonStruct.pupupu.whispy_mouth_texture != -1)
    {
        s32 tx = gGRCommonStruct.pupupu.whispy_mouth_texture;
        s32 lr = gGRCommonStruct.pupupu.lr_players;
        AObjEvent32 *anim[STAGE_MAP_JOINTS_MAX];

        stage_map_anim_array(dGRPupupuWhispyMouthTextureNames[tx][lr],
                             anim);

        gcAddAnimJointAll(gGRCommonStruct.pupupu.map_gobj[nGRPupupuMapObjectFlowersBack], anim, 0.0F);
        gcPlayAnimAll(gGRCommonStruct.pupupu.map_gobj[nGRPupupuMapObjectFlowersBack]);

        gGRCommonStruct.pupupu.whispy_mouth_texture = -1;
    }
    if (gGRCommonStruct.pupupu.whispy_eyes_texture != -1)
    {
        s32 tx = gGRCommonStruct.pupupu.whispy_eyes_texture;
        s32 lr = gGRCommonStruct.pupupu.lr_players;
        AObjEvent32 *anim[STAGE_MAP_JOINTS_MAX];

        stage_map_anim_array(dGRPupupuWhispyEyesTextureNames[tx][lr],
                             anim);

        gcAddAnimJointAll(gGRCommonStruct.pupupu.map_gobj[nGRPupupuMapObjectFlowersFront], anim, 0.0F);
        gcPlayAnimAll(gGRCommonStruct.pupupu.map_gobj[nGRPupupuMapObjectFlowersFront]);

        gGRCommonStruct.pupupu.whispy_eyes_texture = -1;
    }
#endif
}

/* The per-DObj array the game's own walk wants: one entry per joint of
 * the tree the script is attached to, with the script on the joint its
 * own map-file table names and NULL everywhere else.
 *
 * `gcAddAnimAll` advances one entry per joint of the tree it walks
 * (sys/objanim.c:228-244, `anim_joints++` beside `gcGetTreeDObjNext`),
 * so the array has to be as long as the tree: a three-joint tree walked
 * against a two-entry array reads the entry past its end and hands
 * `gcAddDObjAnimJoint` whatever was there, which is a garbage script and
 * a hung stage. A joint with no script gets a NULL entry, which the same
 * walk reads as "nothing to play here" rather than as an error.
 *
 * The port builds the array where the game reads a table out of the map
 * file: the joint is `stage_map_anim_joint`'s, carried from the export
 * (`AnimJoint WhispyEyesLeftTurn 0x11A0` is `{NULL, NULL, slot}` for the
 * eyes' three joints, so its script is on joint 2, not joint 0). */

/* grpupupu.c:628-634 0x80106490 grPupupuProcUpdate, verbatim: the wind
 * machine, then both flower machines, then whatever animations any of
 * the three just asked for. */
void grPupupuProcUpdate(GObj *ground_gobj)
{
    grPupupuUpdateWhispyStatus();
    grPupupuFlowersBackUpdateAll();
    grPupupuFlowersFrontUpdateAll();
    grPupupuUpdateGObjAnims();
}

/* grpupupu.c:637-660 0x801064C8 grPupupuMakeMapGObj, verbatim but for
 * what builds the tree: the game takes a `map_head + offset` DObjDesc, an
 * MObjSub offset and a display proc, and the port takes the index of the
 * object the stage pack carries (stage.h's stage_bind_map_object, which
 * also wires the display and the gcPlayAnimAll process, so the decomp's
 * own two lines for those are inside it).
 *
 * The name is NOT the decomp's, and can't be: grpupupu.h declares that
 * one with three `intptr_t` map-file offsets, which is exactly what the
 * port has no address space for, so the port's shape of the same step
 * gets its own name rather than a silently different signature under the
 * game's. Nothing outside this file calls it. */
static GObj* grPupupuMakeMapObject(s32 index, s32 dl_link)
{
    GObj *ground_gobj = gcMakeGObjSPAfter(nGCCommonKindGround, NULL, nGCCommonLinkIDGround, GOBJ_PRIORITY_DEFAULT);

    stage_bind_map_object(ground_gobj, index, dl_link);

    return ground_gobj;
}

/* grpupupu.c:663-690 0x8010658C grPupupuInitAll, verbatim but for the map
 * reads and the anim tables (see this file's header): the four GObjs,
 * their two display links (4 for the three on the layer-0 proc, 16 for
 * the flowers in front), the anim tables resolved from the pack by name,
 * the whole state (Whispy starts asleep, facing right, with the two
 * waits rolled), and the particle bank -- whose load is the same
 * efParticleGetLoadBankID call Hyrule's twister and Kongo Jungle's
 * cannon make, `lGRPupupuParticle*` being in
 * src/game/ssb64/particlebanks.ld. */
void grPupupuInitAll(void)
{
    s32 i, j;

    gGRCommonStruct.pupupu.map_gobj[nGRPupupuMapObjectWhispyEyes]   = grPupupuMakeMapObject(nGRPupupuMapObjectWhispyEyes, 4);
    gGRCommonStruct.pupupu.map_gobj[nGRPupupuMapObjectWhispyMouth]  = grPupupuMakeMapObject(nGRPupupuMapObjectWhispyMouth, 4);
    gGRCommonStruct.pupupu.map_gobj[nGRPupupuMapObjectFlowersBack]  = grPupupuMakeMapObject(nGRPupupuMapObjectFlowersBack, 4);
    gGRCommonStruct.pupupu.map_gobj[nGRPupupuMapObjectFlowersFront] = grPupupuMakeMapObject(nGRPupupuMapObjectFlowersFront, 16);

    /* The tables, by name: slot [0] is the script the pack carries under
     * that name and slot [1] the MObj `alt` the object's own pack wants.
     * The mouth's alts are its stage-table list in order, four per side,
     * so an entry's alt is `side * 4 + status`; the eyes' are two, one
     * per side, and their blink has none. */
    for (i = 0; i < nGRPupupuWhispyEyesStatusEnumCount; i++)
    {
        for (j = 0; j < 2; j++)
        {
            dGRPupupuWhispyEyesAnims[i][j][0] = (intptr_t)
                stage_map_anim(dGRPupupuWhispyEyesAnimNames[i][j]);
            dGRPupupuWhispyEyesAnims[i][j][1] =
                (i == nGRPupupuWhispyEyesStatusTurn) ? j : -1;
        }
    }
    for (i = 0; i < nGRPupupuWhispyMouthStatusEnumCount; i++)
    {
        for (j = 0; j < 2; j++)
        {
            dGRPupupuWhispyMouthAnims[i][j][0] = (intptr_t)
                stage_map_anim(dGRPupupuWhispyMouthAnimNames[i][j]);
            dGRPupupuWhispyMouthAnims[i][j][1] = j * 4 + i;
        }
    }
    for (i = 0; i < nGRPupupuWhispyMouthTextureEnumCount; i++)
    {
        for (j = 0; j < 2; j++)
        {
            dGRPupupuWhispyMouthTextures[i][j] =
                stage_map_anim(dGRPupupuWhispyMouthTextureNames[i][j]);
        }
    }
    for (i = 0; i < nGRPupupuWhispyEyesTextureEnumCount; i++)
    {
        for (j = 0; j < 2; j++)
        {
            dGRPupupuWhispyEyesTextures[i][j] =
                stage_map_anim(dGRPupupuWhispyEyesTextureNames[i][j]);
        }
    }

    gGRCommonStruct.pupupu.whispy_eyes_status   =
    gGRCommonStruct.pupupu.whispy_mouth_status  =
    gGRCommonStruct.pupupu.whispy_mouth_texture =
    gGRCommonStruct.pupupu.whispy_eyes_texture  = -1;

    gGRCommonStruct.pupupu.whispy_status        = 0;

    gGRCommonStruct.pupupu.lr_players           = 1;

    gGRCommonStruct.pupupu.whispy_wind_wait     = syUtilsRandIntRange(GRPUPUPU_WHISPY_WAIT_DURATION_RANDOM) + GRPUPUPU_WHISPY_WAIT_DURATION_BASE;
    gGRCommonStruct.pupupu.whispy_blink_wait    = syUtilsRandIntRange(GRPUPUPU_WHISPY_BLINK_WAIT_RANDOM)    + GRPUPUPU_WHISPY_BLINK_WAIT_BASE;

    gGRCommonStruct.pupupu.flowers_back_status  =
    gGRCommonStruct.pupupu.flowers_front_status = 0;

    gGRCommonStruct.pupupu.flowers_back_wait    = 15;
    gGRCommonStruct.pupupu.flowers_front_wait   = 22;

#ifdef FT_HOSTTEST
    /* DIVERGES, and only in this build, for the reason
     * grHyruleTwisterInitVars gives: lbParticleSetupBankID walks a bank
     * whose arrays are 32-bit pointers, so on x86-64 it reads and writes
     * at twice the stride. The host test builds bank 0 itself out of the
     * same two files -- host_load_particle_bank -- and leaves the id here
     * at zero, "a bank that at least exists". */
    gGRCommonStruct.pupupu.particle_bank_id = 0;
#else
    gGRCommonStruct.pupupu.particle_bank_id = efParticleGetLoadBankID(&lGRPupupuParticleScriptBankLo, &lGRPupupuParticleScriptBankHi, &lGRPupupuParticleTextureBankLo, &lGRPupupuParticleTextureBankHi);
    /* DIVERGES: the textures, bound by name as src/dc/mntitle.c binds
     * the title's. efParticleGetLoadBankID walks the bank's scripts and
     * its N64 textures, but the PVR draws from the pre-converted
     * grpupupu.txp, and lbpdraw.c skips any particle whose image has no
     * bound PVR texture -- so without this Whispy's leaves and dust were
     * never drawn at all. */
    if (lbpTexLoadBank(gGRCommonStruct.pupupu.particle_bank_id, "grpupupu") != 0)
    {
        syDebugPrintf("pupupu: grpupupu.txp did not load\n");
    }

    /* The port's one line, not the decomp's, and the same trade
     * src/dc/gryoster.c makes: the target is the only place the pack's
     * own numbers can be checked at all. The four trees, their counts and
     * their script names are what prove the export landed -- every one of
     * the four objects' assemblies is a separate DObjDesc block, and the
     * forty scripts the two animated trees and the two flower GObjs play
     * are named, not offset. Disc probe: `--serial`, grep "whispy:". */
    syDebugPrintf("whispy: %d map object(s), %d/%d/%d/%d joint(s), %d anim script(s), bank %d\n",
                  (int)stage_bound()->map_object_count,
                  (int)((stage_bound()->map_models[0].hd != NULL) ? stage_bound()->map_models[0].hd->joint_count : -1),
                  (int)((stage_bound()->map_models[1].hd != NULL) ? stage_bound()->map_models[1].hd->joint_count : -1),
                  (int)((stage_bound()->map_models[2].hd != NULL) ? stage_bound()->map_models[2].hd->joint_count : -1),
                  (int)((stage_bound()->map_models[3].hd != NULL) ? stage_bound()->map_models[3].hd->joint_count : -1),
                  (int)stage_bound()->map_anim_count,
                  (int)gGRCommonStruct.pupupu.particle_bank_id);
#endif
}

/* grpupupu.c:693-701 0x801066D4 grPupupuMakeGround, verbatim: the
 * stage's own ground GObj carries the three machines at priority 4. */
GObj* grPupupuMakeGround(void)
{
    GObj *ground_gobj = gcMakeGObjSPAfter(nGCCommonKindGround, NULL, nGCCommonLinkIDGround, GOBJ_PRIORITY_DEFAULT);

    gcAddGObjProcess(ground_gobj, grPupupuProcUpdate, nGCProcessKindFunc, 4);
    grPupupuInitAll();

    return ground_gobj;
}

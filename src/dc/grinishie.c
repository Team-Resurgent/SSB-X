/* grinishie.c -- gr/grcommon/grinishie.c, verbatim but for the map reads
 * and the half that waits on the item roster: Mushroom Kingdom's scales,
 * the port's sixth stage hazard.
 *
 * The stage's hazard is a PAIR of platforms hung from two chains. Each
 * hangs on its own set of yakumono lines, and every fighter standing on
 * one adds its own weight (grInishieScaleGetPressure) to that side. The
 * two weights drive `splat_accelerate` toward the heavier side and
 * `splat_alt` toward the difference, the platforms move by `splat_alt`
 * in OPPOSITE directions off their base heights, and the two chains
 * take up the slack so neither platform ever floats free of its string.
 * Load one side past 1100 and the pair FALLS -- the lines go off, the
 * platforms drop to a deadzone a thousand units under the map, and 180
 * tics later they play the retract script and rise back to level. The
 * stage's own code keeps the collision lines at the platforms' own
 * positions every tic (grInishieScaleProcUpdate's tail), which is what
 * makes a falling platform a falling floor.
 *
 * The trees are the map file's, and this stage is the one that made the
 * port need a THIRD object shape. `MPGroundData.map_nodes` names
 * `MapHead`, and that block is a DISPLAY LIST -- RDP pipe-sync and
 * SetTextureImage, not a DObjDesc array. The game uses it exactly that
 * way: `gcAddDObjForGObj(gobj, map_head + &llGRInishieMapMapHead)` sets
 * `new_dobj->dv`, one empty DObj per platform (grinishie.c:372). So
 * object 0 of this stage's pack is a one-joint object carrying that
 * list, and object 1 is the scale's own five-joint DObjDesc tree beside
 * it -- which has NO display list at all, and is read only for the
 * joints that give the two chains their lengths.
 *
 * DIVERGES, five:
 *
 *  - The map reads are the pack's. Every tree, script and map_obj kind
 *    the game reaches as `map_head + &llGRInishieMap<Name>` is
 *    stage_bind_map_object and stage_map_anim here, by name, and the
 *    object INDICES are the pack's order rather than the decomp's
 *    linker symbols. Note that this file's two orders disagree: the
 *    game builds the scale tree first and the platforms after, while
 *    object 0 has to be the platform's display list, because object 0
 *    is what map_nodes names (tools/export/ssb_stageexport.py's Inishie
 *    entry). Nothing reads object 0 as "the tree", so the swap is free.
 *
 *  - grInishieInitHeaders is NOT ported. Its whole body turns a
 *    MPGroundData pointer into the `map_head` and `item_head` offsets
 *    every other function in the decomp adds to a linker symbol -- and
 *    the port has neither the pointer nor the symbols. `map_head` is
 *    read in exactly one place (the retract script's name, below), so
 *    the field stays NULL and grInishieMakeGround goes straight to the
 *    makers.
 *
 *  - grInishieMakeScale's two platform GObs come from
 *    stage_bind_map_object, which registers gcPlayAnimAll itself and
 *    draws the pack's own batches (gcDrawDObjTreeForGObj) where the
 *    game's `gcDrawDObjDLHead0` queues the one display list. For a
 *    one-joint pack the two draw the same thing. The SCALE TREE's GObj
 *    gets a gcPlayAnimAll process it has nothing to animate, which the
 *    game's own setup does not add.
 *
 *    The platform's `gcAddXObjForDObjFixed(platform_dobj,
 *    nGCMatrixKindTra, 0)` is not carried either: it narrows the joint
 *    to a translation-only transform, and the port bakes the transform
 *    kinds into the pack with the RpyR equivalent installed for every
 *    joint (src/dc/objmodel.c). This is the same trade gryoster.c's
 *    third divergence makes.
 *
 *  - The retract script is stage_map_anim("ScaleRetract") rather than
 *    `map_head + &llGRInishieMapScaleRetractAnimJoint`, and the call is
 *    `#ifdef FT_HOSTTEST`-guarded for grJungleMakeTaruCann's reason:
 *    `union AObjEvent32` is eight bytes on x86-64, so the interpreter
 *    walks a script twice as fast as it should and never terminates.
 *    The host checks the exported words through stage_map_anim instead;
 *    the target is the only place the platforms actually retract.
 *
 *  - dGRInishieScaleTransformKinds is ported verbatim and is dead, the
 *    same way Kongo Jungle's dGRJungleTaruCannTransformKinds is: it is
 *    grModelSetupGroundDObjs' argument and the port builds the tree out
 *    of the baked pack. It is kept because it is the file's own
 *    initialized data.
 *
 * THE `?` BLOCK IS IN: grInishieMakePowerBlock and
 * the six functions its state machine is made of, plus
 * grInishiePowerBlockSetDamage/CheckGetDamageKind, which are what the
 * ITEM calls (src/dc/itpowerblock.c). It is the first of the ten "stage
 * items" -- nITKindPowerBlock is entry 22 of dITManagerProcMakeList --
 * to be reachable from a stage, and the hazard kind it registers
 * (nGMHitEnvironmentPowerBlock) has its consumer in src/dc/ftmain.c.
 *
 * THE PIRANHA PLANTS ARE IN: grInishieMakePakkun and
 * grInishiePakkunSetWaitFighter, with the item itself in
 * src/dc/itpakkun.c. They are the last of this file's three hazards, so
 * grInishieMakeGround below is the decomp's own line for line.
 *
 * The FIGHTER side of a pipe, ft/ftcommon/ftcommondokan.c, is compiled
 * unmodified and grInishiePakkunSetWaitFighter, its only
 * caller, is live.
 *
 * dGRInishieScaleMapObjKinds, dGRInishieScaleLineGroups and
 * dGRInishiePakkunMapObjKinds are the file's own initialized data,
 * ported verbatim.
 */

#include <gr/ground.h>
/* the decomp's grinishie.c reaches these through gr/ground.h ->
 * gr/grfunctions.h; this port's gr/ground.h is the collision files' shim
 * and stops short of that, so the ones it needs are named here, the same
 * way grpupupu.c names its own. */
#include <gr/grcommon/grinishie.h>
#include <gr/grvars.h>            /* GRCommonGroundVarsInishie */
#include <it/item.h>              /* ITStruct, itGetStruct -- the `?`
                                     block's own damage callback reads it */
#include <ft/ftmain.h>            /* ftMainCheckAddGroundHazard */
#include <ft/fighter.h>
#include <mp/mpdef.h>             /* nMPMapObjKind* */
#include <ef/effect.h>
#include <sc/scene.h>
#include <sys/utils.h>

#include "stage.h"

/* The two objects this stage's pack carries, in the pack's own order --
 * which is map_nodes FIRST and the scale tree second; see this file's
 * header for why that is the reverse of the decomp's build order. */
#define GRINISHIE_OBJ_PLATFORM 0    /* `MapHead`, one DObj carrying a DL */
#define GRINISHIE_OBJ_SCALE    1    /* `Scale`, the five-joint tree */

/* // // // // // // // // // // // //
 *                               //
 *       INITIALIZED DATA        //
 *                               //
 * // // // // // // // // // // // */

/* grinishie.c:14 0x8012EB30 */
u16 dGRInishieScaleMapObjKinds[] = { nMPMapObjKindScaleL, nMPMapObjKindScaleR };

/* grinishie.c:17 0x8012EB34. The two yakumono LINE GROUPS the platforms
 * hang on, one per side, and the IDs every mpCollisionSetYakumono* below
 * is called with. */
u8 dGRInishieScaleLineGroups[] = { 0x01, 0x02 };

/* grinishie.c:20 0x8012EB38 -- dead here, see this file's header. */
DObjTransformTypes dGRInishieScaleTransformKinds[] =
{
    { nGCMatrixKindTra, nGCMatrixKindNull, 0x01 },
    { nGCMatrixKindTra, nGCMatrixKindNull, 0x01 },
    { nGCMatrixKindTra, nGCMatrixKindNull, 0x00 },
    { nGCMatrixKindTra, nGCMatrixKindNull, 0x01 },
    { nGCMatrixKindTra, nGCMatrixKindNull, 0x00 }
};

/* grinishie.c:33 0x8012EB48 -- the Piranha Plants' own map_obj kinds,
 * read only by the left-out grInishieMakePakkun. Kept for the step that
 * brings it in. */
u16 dGRInishiePakkunMapObjKinds[] = { nMPMapObjKindPakkunL, nMPMapObjKindPakkunR };

/* // // // // // // // // // // // //
 *                               //
 *          ENUMERATORS          //
 *                               //
 * // // // // // // // // // // // */

enum grInishieScaleStatus
{
    nGRInishieScaleStatusWait,
    nGRInishieScaleStatusFall,
    nGRInishieScaleStatusSleep,
    nGRInishieScaleStatusRetract
};

/* grinishie.c:46-52. The `?` block's four states, and they are the
 * STAGE's rather than the item's: the item has one status of its own
 * (src/dc/itpowerblock.c) and this machine is what decides when a new
 * block is made. */
enum grInishiePowerBlockStatus
{
    nGRInishiePowerBlockStatusWait,
    nGRInishiePowerBlockStatusMake,
    nGRInishiePowerBlockStatusSleep,
    nGRInishiePowerBlockStatusDamage
};

/* // // // // // // // // // // // //
 *                               //
 *           FUNCTIONS           //
 *                               //
 * // // // // // // // // // // // */

/* grinishie.c:61 0x80108CD0, verbatim. Remembers, per player, whether
 * they were grounded last tic and for how long -- `players_tt` is the
 * "has been standing on something for a few tics" timer that
 * grInishieScaleGetPressure weighs eightfold. */
void grInishieScaleUpdateFighterStatsGA(void)
{
    GObj *fighter_gobj = gGCCommonLinks[nGCCommonLinkIDFighter];

    while (fighter_gobj != NULL)
    {
        FTStruct *fp = ftGetStruct(fighter_gobj);
        s32 player = fp->player;

        if (fp->ga == nMPKineticsGround)
        {
            if (gGRCommonStruct.inishie.players_ga[player] != nMPKineticsGround)
            {
                gGRCommonStruct.inishie.players_tt[player] = 1;
            }
            else if (gGRCommonStruct.inishie.players_tt[player] != 0)
            {
                gGRCommonStruct.inishie.players_tt[player]--;
            }
        }
        else gGRCommonStruct.inishie.players_tt[player] = 0;

        gGRCommonStruct.inishie.players_ga[player] = fp->ga;

        fighter_gobj = fighter_gobj->link_next;
    }
}

/* grinishie.c:90 0x80108D50, verbatim: the weight one side of the scale
 * carries this tic. A fighter counts only if it is standing on a floor
 * line whose yakumono id is this side's, and it counts 1.4 + its own
 * (1 - weight) -- so a heavy fighter, one whose `weight` attribute is
 * high, presses LESS. A fighter that has been grounded a few tics
 * presses eight times as much, which is the difference between landing
 * on a platform and standing on it. */
f32 grInishieScaleGetPressure(s32 line_id)
{
    GObj *fighter_gobj = gGCCommonLinks[nGCCommonLinkIDFighter];
    f32 pressure = 0.0F;

    while (fighter_gobj != NULL)
    {
        FTStruct *fp = ftGetStruct(fighter_gobj);

        if (fp->ga == nMPKineticsGround)
        {
            if ((fp->coll_data.floor_line_id != -2) && (mpCollisionSetDObjNoID(fp->coll_data.floor_line_id) == line_id))
            {
                f32 weight = (1.0F - fp->attr->weight) + 1.4F;

                if (gGRCommonStruct.inishie.players_tt[fp->player] != 0)
                {
                    pressure += (weight * 8.0F);
                }
                else pressure += weight;
            }
        }
        fighter_gobj = fighter_gobj->link_next;
    }
    return pressure;
}

/* grinishie.c:118 0x801085E0, verbatim: the level state. The two
 * pressures drive the tilt, with a deadzone rule that is the game's own
 * -- with weight on BOTH sides the acceleration decays by 7% a tic
 * rather than being pushed to zero, so a balanced load settles instead
 * of snapping. Past 1100 on either side the pair FALLS and two sparkle
 * effects fire at the platforms' own positions. The tail moves both
 * platforms by `splat_alt` in opposite directions and then the two
 * string DObjs to keep the chains taut. */
void grInishieScaleUpdateWait(void)
{
    DObj *l_dobj;
    DObj *r_dobj;
    f32 l_weight;
    f32 r_weight;
    f32 alt;
    sb32 ud;

    grInishieScaleUpdateFighterStatsGA();

    l_weight = grInishieScaleGetPressure(dGRInishieScaleLineGroups[0]);
    r_weight = grInishieScaleGetPressure(dGRInishieScaleLineGroups[1]);

    if ((l_weight == 0.0F) && (r_weight == 0.0F))
    {
        if (gGRCommonStruct.inishie.splat_alt != 0.0F)
        {
            if (gGRCommonStruct.inishie.splat_alt < 0.0F)
            {
                gGRCommonStruct.inishie.splat_alt += 8.0F;

                if (gGRCommonStruct.inishie.splat_alt > 0.0F)
                {
                    gGRCommonStruct.inishie.splat_alt = 0.0F;
                }
            }
            else
            {
                gGRCommonStruct.inishie.splat_alt -= 8.0F;

                if (gGRCommonStruct.inishie.splat_alt < 0.0F)
                {
                    gGRCommonStruct.inishie.splat_alt = 0.0F;
                }
            }
        }
        gGRCommonStruct.inishie.splat_accelerate = 0.0F;
    }
    else
    {
        gGRCommonStruct.inishie.splat_accelerate += (r_weight - l_weight);

        if ((l_weight != 0.0F) && (r_weight != 0.0F) && (gGRCommonStruct.inishie.splat_accelerate != 0.0F))
        {
            gGRCommonStruct.inishie.splat_accelerate *= 0.93F;
        }
        else if (gGRCommonStruct.inishie.splat_accelerate > 0.0F)
        {
            gGRCommonStruct.inishie.splat_accelerate -= 0.9F;

            if (gGRCommonStruct.inishie.splat_accelerate < 0.0F)
            {
                gGRCommonStruct.inishie.splat_accelerate = 0.0F;
            }
        }
        else if (gGRCommonStruct.inishie.splat_accelerate < 0.0F)
        {
            gGRCommonStruct.inishie.splat_accelerate += 0.9F;

            if (gGRCommonStruct.inishie.splat_accelerate > 0.0F)
            {
                gGRCommonStruct.inishie.splat_accelerate = 0.0F;
            }
        }
        gGRCommonStruct.inishie.splat_alt += gGRCommonStruct.inishie.splat_accelerate;
    }
    alt = ABSF(gGRCommonStruct.inishie.splat_alt);

    l_dobj = gGRCommonStruct.inishie.scale[0].platform_dobj;
    r_dobj = gGRCommonStruct.inishie.scale[1].platform_dobj;

    if (alt > 1100.0F)
    {
        ud = 0;

        if (gGRCommonStruct.inishie.splat_alt < 0.0F)
        {
            /* The decomp has an empty `if (splat_accelerate != 0.0F)` in
             * here -- a permuter artifact with no body, so it is not
             * carried. */
            ud = 1;
        }
        gGRCommonStruct.inishie.splat_accelerate = 0.0F;

        if (ud != 0)
        {
            gGRCommonStruct.inishie.splat_alt = -1100.0F;
        }
        else gGRCommonStruct.inishie.splat_alt = 1100.0F;

        gGRCommonStruct.inishie.splat_status = nGRInishieScaleStatusFall;

        efManagerSparkleWhiteScaleMakeEffect(&l_dobj->translate.vec.f, 1.0F);
        efManagerSparkleWhiteScaleMakeEffect(&r_dobj->translate.vec.f, 1.0F);
    }
    l_dobj->translate.vec.f.y = gGRCommonStruct.inishie.scale[0].platform_base_y + gGRCommonStruct.inishie.splat_alt;
    r_dobj->translate.vec.f.y = gGRCommonStruct.inishie.scale[1].platform_base_y - gGRCommonStruct.inishie.splat_alt;

    gGRCommonStruct.inishie.scale[0].string_dobj->translate.vec.f.y = l_dobj->translate.vec.f.y - gGRCommonStruct.inishie.scale[0].string_length;
    gGRCommonStruct.inishie.scale[1].string_dobj->translate.vec.f.y = r_dobj->translate.vec.f.y - gGRCommonStruct.inishie.scale[1].string_length;
}

/* grinishie.c:224 0x80109118, verbatim: the fall. Both platforms drop
 * together, accelerating 3.0 a tic to a terminal 70.0, until BOTH are
 * under the map floor less a thousand -- and then the stage's two
 * collision lines go OFF, which is what makes the platforms stop being
 * floors. 180 tics of sleep follow before they retract.
 *
 * The deadzone reads gMPCollisionGroundData->map_bound_bottom, the same
 * header field src/dc/stage.c publishes from the pack. */
void grInishieScaleUpdateFall(void)
{
    f32 deadzone;

    gGRCommonStruct.inishie.splat_accelerate += 3.0F;

    if (gGRCommonStruct.inishie.splat_accelerate > 70.0F)
    {
        gGRCommonStruct.inishie.splat_accelerate = 70.0F;
    }
    gGRCommonStruct.inishie.scale[0].platform_dobj->translate.vec.f.y -= gGRCommonStruct.inishie.splat_accelerate;
    gGRCommonStruct.inishie.scale[1].platform_dobj->translate.vec.f.y -= gGRCommonStruct.inishie.splat_accelerate;

    deadzone = gMPCollisionGroundData->map_bound_bottom + (-1000.0F);

    if ((gGRCommonStruct.inishie.scale[0].platform_dobj->translate.vec.f.y < deadzone) && (gGRCommonStruct.inishie.scale[1].platform_dobj->translate.vec.f.y < deadzone))
    {
        gGRCommonStruct.inishie.splat_status = nGRInishieScaleStatusSleep;
        gGRCommonStruct.inishie.splat_accelerate = 0.0F;

        mpCollisionSetYakumonoOffID(dGRInishieScaleLineGroups[0]);
        mpCollisionSetYakumonoOffID(dGRInishieScaleLineGroups[1]);

        gGRCommonStruct.inishie.splat_wait = 180;
    }
}

/* grinishie.c:252 0x80109220, verbatim: after the 180-tic sleep, hand
 * BOTH platforms the map file's one retract script. See this file's
 * header for the name and the host guard. */
void grInishieScaleUpdateStep(void)
{
    gGRCommonStruct.inishie.splat_wait--;

    if (gGRCommonStruct.inishie.splat_wait == 0)
    {
        gGRCommonStruct.inishie.splat_status = nGRInishieScaleStatusRetract;

#ifdef FT_HOSTTEST
        /* DIVERGES, and only in this build: the script is not attached,
         * because the host cannot walk one. */
#else
        gcAddDObjAnimJoint(gGRCommonStruct.inishie.scale[0].platform_dobj, stage_map_anim("ScaleRetract"), 0.0F);
        gcAddDObjAnimJoint(gGRCommonStruct.inishie.scale[1].platform_dobj, stage_map_anim("ScaleRetract"), 0.0F);
#endif
    }
}

/* grinishie.c:266 0x8010929C, verbatim: the rise. The two platforms walk
 * `splat_alt` back to zero at 10.0 a tic, and when it crosses the script
 * is stopped by hand -- `anim_wait = AOBJ_ANIM_NULL` and
 * `flags = DOBJ_FLAG_NONE` -- and the two collision lines go back ON.
 * The tail is UpdateWait's tail: the platforms move by `splat_alt` and
 * the chains follow. */
void grInishieScaleUpdateRetract(void)
{
    DObj *l_dobj;
    DObj *r_dobj;
    sb32 is_complete = FALSE;

    if (gGRCommonStruct.inishie.splat_alt != 0.0F)
    {
        if (gGRCommonStruct.inishie.splat_alt < 0.0F)
        {
            gGRCommonStruct.inishie.splat_alt += 10.0F;

            if (gGRCommonStruct.inishie.splat_alt >= 0.0F)
            {
                is_complete = TRUE;
            }
        }
        else
        {
            gGRCommonStruct.inishie.splat_alt -= 10.0F;

            if (gGRCommonStruct.inishie.splat_alt <= 0.0F)
            {
                is_complete = TRUE;
            }
        }
    }
    l_dobj = gGRCommonStruct.inishie.scale[0].platform_dobj;
    r_dobj = gGRCommonStruct.inishie.scale[1].platform_dobj;

    if (is_complete != FALSE)
    {
        gGRCommonStruct.inishie.splat_alt = 0.0F;

        l_dobj->anim_wait = AOBJ_ANIM_NULL;
        l_dobj->flags = DOBJ_FLAG_NONE;

        mpCollisionSetYakumonoOnID(dGRInishieScaleLineGroups[0]);

        r_dobj->anim_wait = AOBJ_ANIM_NULL;
        r_dobj->flags = DOBJ_FLAG_NONE;

        mpCollisionSetYakumonoOnID(dGRInishieScaleLineGroups[1]);

        gGRCommonStruct.inishie.splat_status = nGRInishieScaleStatusWait;
    }
    l_dobj->translate.vec.f.y = gGRCommonStruct.inishie.scale[0].platform_base_y + gGRCommonStruct.inishie.splat_alt;
    r_dobj->translate.vec.f.y = gGRCommonStruct.inishie.scale[1].platform_base_y - gGRCommonStruct.inishie.splat_alt;

    gGRCommonStruct.inishie.scale[0].string_dobj->translate.vec.f.y = l_dobj->translate.vec.f.y - gGRCommonStruct.inishie.scale[0].string_length;
    gGRCommonStruct.inishie.scale[1].string_dobj->translate.vec.f.y = r_dobj->translate.vec.f.y - gGRCommonStruct.inishie.scale[1].string_length;
}

/* grinishie.c:320 0x801093EC, verbatim: the machine. The tail is the
 * part that matters -- the two platforms' own positions are written into
 * the collision lines they hang on EVERY tic, whichever state ran, which
 * is what makes a moving platform a moving floor rather than a moving
 * picture. */
void grInishieScaleProcUpdate(GObj *ground_gobj)
{
    switch (gGRCommonStruct.inishie.splat_status)
    {
    case nGRInishieScaleStatusWait:
        grInishieScaleUpdateWait();
        break;

    case nGRInishieScaleStatusFall:
        grInishieScaleUpdateFall();
        break;

    case nGRInishieScaleStatusSleep:
        grInishieScaleUpdateStep();
        break;

    case nGRInishieScaleStatusRetract:
        grInishieScaleUpdateRetract();
        break;
    }
    mpCollisionSetYakumonoPosID(dGRInishieScaleLineGroups[0], &gGRCommonStruct.inishie.scale[0].platform_dobj->translate.vec.f);
    mpCollisionSetYakumonoPosID(dGRInishieScaleLineGroups[1], &gGRCommonStruct.inishie.scale[1].platform_dobj->translate.vec.f);
}

/* grinishie.c:345 0x801094A0, verbatim but for the map reads (see this
 * file's header). The scale tree's GObj is built first and is the one
 * that never gets a process; the platforms are built in the loop, and
 * the machine lands on the LAST of them, because `ground_gobj` is
 * reassigned inside the loop -- which is the decomp's own shape and is
 * kept.
 *
 * The two chain lengths are the tree's own joint translates:
 * `map_dobjs[0]->y + map_dobjs[3]->y` for the left string and
 * `map_dobjs[0]->y + map_dobjs[1]->y` for the right. Those are pack
 * joints 0/3 and 0/1 of object GRINISHIE_OBJ_SCALE, and the pack keeps
 * the map file's own joint order, which is why they can be read by
 * index. */
void grInishieMakeScale(void)
{
    GObj *ground_gobj;
    DObj *map_dobjs[5];
    DObj *platform_dobj;
    s32 i;
    s32 mapobj;
    Vec3f yakumono_pos;

    ground_gobj = gcMakeGObjSPAfter(nGCCommonKindGround, NULL, nGCCommonLinkIDGround, GOBJ_PRIORITY_DEFAULT);

    stage_bind_map_object(ground_gobj, GRINISHIE_OBJ_SCALE, 6);

    for (i = 0; i < 5; i++)
    {
        map_dobjs[i] = stage_map_joint(GRINISHIE_OBJ_SCALE, i);
    }
    gGRCommonStruct.inishie.scale[0].string_dobj = map_dobjs[4];
    gGRCommonStruct.inishie.scale[0].string_length = map_dobjs[0]->translate.vec.f.y + map_dobjs[3]->translate.vec.f.y;

    gGRCommonStruct.inishie.scale[1].string_dobj = map_dobjs[2];
    gGRCommonStruct.inishie.scale[1].string_length = map_dobjs[0]->translate.vec.f.y + map_dobjs[1]->translate.vec.f.y;

    for (i = 0; i < ARRAY_COUNT(gGRCommonStruct.inishie.scale); i++)
    {
        ground_gobj = gcMakeGObjSPAfter(nGCCommonKindGround, NULL, nGCCommonLinkIDGround, GOBJ_PRIORITY_DEFAULT);

        /* the platform is the pack's one-joint `MapHead` object: bind
         * returns the DObj itself, which is the joint the game sets a
         * position on and hands the retract script to. A second bind of
         * the same object index overwrites the Stage's map_joints entry for
         * it, which nothing reads -- the stage keeps its own pointers
         * exactly as the decomp does. */
        platform_dobj = stage_bind_map_object(ground_gobj, GRINISHIE_OBJ_PLATFORM, 6);
        gGRCommonStruct.inishie.scale[i].platform_dobj = platform_dobj;

        mpCollisionGetMapObjIDsKind(dGRInishieScaleMapObjKinds[i], &mapobj);
        mpCollisionGetMapObjPositionID(mapobj, &yakumono_pos);

        platform_dobj->translate.vec.f = yakumono_pos;

        gGRCommonStruct.inishie.scale[i].platform_base_y = yakumono_pos.y;

        mpCollisionSetYakumonoOnID(dGRInishieScaleLineGroups[i]);
    }
    gcAddGObjProcess(ground_gobj, grInishieScaleProcUpdate, nGCProcessKindFunc, 4);

    gGRCommonStruct.inishie.splat_status = nGRInishieScaleStatusWait;
    gGRCommonStruct.inishie.splat_alt = 0.0F;
    gGRCommonStruct.inishie.splat_accelerate = 0.0F;

    for (i = 0; i < (ARRAY_COUNT(gGRCommonStruct.inishie.players_ga) + ARRAY_COUNT(gGRCommonStruct.inishie.players_tt)) / 2; i++)
    {
        gGRCommonStruct.inishie.players_tt[i] = 0;
        gGRCommonStruct.inishie.players_ga[i] = 0;
    }
}

/* grinishie.c:553 0x80109C0C, verbatim but for the two makers this file
 * left out and grInishieInitHeaders (see this file's header). */
/* grinishie.c:413-430 grInishieMakePakkun 0x801097F8, verbatim. TWO
 * plants, one at each of the map's own `nMPMapObjKindPakkunL` and `R`
 * positions (dGRInishiePakkunMapObjKinds above, the file's own
 * initialized data) -- the pipes either side of the mushroom. A map with
 * neither gives a mapobj id that names nothing and a plant at (0,0,0),
 * which is what the game does too: unlike the `?` block's own maker,
 * this one has no count check at all.
 *
 * The array is gGRCommonStruct.inishie.pakkun_gobj, which is also what
 * grInishiePakkunSetWaitFighter below walks. */
void grInishieMakePakkun(void)
{
    s32 i;
    Vec3f pos;
    Vec3f vel;
    s32 mapobj;

    for (i = 0; i < (s32)ARRAY_COUNT(gGRCommonStruct.inishie.pakkun_gobj); i++)
    {
        mpCollisionGetMapObjIDsKind(dGRInishiePakkunMapObjKinds[i], &mapobj);
        mpCollisionGetMapObjPositionID(mapobj, &pos);

        vel.x = vel.y = vel.z = 0.0F;

        gGRCommonStruct.inishie.pakkun_gobj[i] = itManagerMakeItemSetupCommon(NULL, nITKindPakkun, &pos, &vel, ITEM_FLAG_PARENT_GROUND);

#ifndef FT_HOSTTEST
        /* The port's own line, the same trade `pblock:` above makes.
         * The two pipes are the map's, and a plant that failed to make
         * is a NULL in this same array -- which is exactly what
         * grInishiePakkunSetWaitFighter passes through. One line per
         * plant, at make time. Disc probe: `--serial`, grep "pakkun:". */
        syDebugPrintf("pakkun: plant %d at map object %d, (%.0f, %.0f), "
                      "gjobj %p\n", (int)i, (int)mapobj, pos.x, pos.y,
                      (void *)gGRCommonStruct.inishie.pakkun_gobj[i]);
#endif
    }
}

/* grinishie.c:424-431 grInishiePakkunSetWaitFighter 0x80109818,
 * verbatim: tell both plants a fighter is at a pipe, which is what
 * cancels a rise in progress (itPakkunCommonSetWaitFighter sets the flag
 * itPakkunAppearProcUpdate reads next tic). Its caller is
 * ft/ftcommon/ftcommondokan.c:108, the fighter crouching into a pipe,
 * compiled unmodified. */
void grInishiePakkunSetWaitFighter(void)
{
    s32 i;

    for (i = 0; i < (s32)ARRAY_COUNT(gGRCommonStruct.inishie.pakkun_gobj); i++)
    {
        itPakkunCommonSetWaitFighter(gGRCommonStruct.inishie.pakkun_gobj[i]);
    }
}

/* grinishie.c:432-440 grInishiePowerBlockUpdateWait 0x80109838,
 * verbatim: nothing happens until the battle leaves its pre-match hold,
 * and then the first block is armed -- 1800 tics, thirty seconds. */
void grInishiePowerBlockUpdateWait(void)
{
    if (gSCManagerBattleState->game_status != nSCBattleGameStatusWait)
    {
        gGRCommonStruct.inishie.pblock_status = nGRInishiePowerBlockStatusMake;
        gGRCommonStruct.inishie.pblock_appear_wait = 1800;
    }
}

/* grinishie.c:442-447 grInishiePowerBlockSetWait 0x8010986C, verbatim:
 * what the ITEM calls when its damage animation has played out, which is
 * what makes the block cycle. */
void grInishiePowerBlockSetWait(void)
{
    gGRCommonStruct.inishie.pblock_status = nGRInishiePowerBlockStatusMake;
    gGRCommonStruct.inishie.pblock_appear_wait = 1800;
}

/* grinishie.c:449-474 grInishiePowerBlockUpdateMake 0x80109888,
 * verbatim. The block appears at ONE of the stage's own
 * nMPMapObjKindPowerBlock positions, drawn at random -- the map file
 * names several and the mushroom moves between them over the course of a
 * match. A make that fails re-arms the timer rather than leaving the
 * stage with no block at all. */
void grInishiePowerBlockUpdateMake(void)
{
    GObj *pblock_gobj;
    Vec3f pos;
    Vec3f vel;

    gGRCommonStruct.inishie.pblock_appear_wait--;

    if (gGRCommonStruct.inishie.pblock_appear_wait == 0)
    {
        s32 pblock_pos_id = gGRCommonStruct.inishie.pblock_pos_ids[syUtilsRandIntRange(gGRCommonStruct.inishie.pblock_pos_count)];

        mpCollisionGetMapObjPositionID(pblock_pos_id, &pos);

        vel.x = vel.y = vel.z = 0.0F;

        pblock_gobj = itManagerMakeItemSetupCommon(NULL, nITKindPowerBlock, &pos, &vel, ITEM_FLAG_PARENT_GROUND);

        if (pblock_gobj != NULL)
        {
            gGRCommonStruct.inishie.pblock_gobj = pblock_gobj;
            gGRCommonStruct.inishie.pblock_status = nGRInishiePowerBlockStatusSleep;

#ifndef FT_HOSTTEST
            /* the port's own, and the one line that says the hazard is
             * real: a `?` block exists on the field at an id the map
             * named. Rare enough to print every time. */
            syDebugPrintf("pblock: made at map object %d, (%.0f, %.0f), "
                          "gjobj %p\n", (int)pblock_pos_id, pos.x, pos.y,
                          (void *)pblock_gobj);
#endif
        }
        else grInishiePowerBlockSetWait();
    }
}

/* grinishie.c:477-485 grInishiePowerBlockUpdateDamage 0x8010992C,
 * verbatim: two tics after the hit, the hazard registration comes off.
 * The ITEM is what destroys itself, in its own damage animation's tail. */
void grInishiePowerBlockUpdateDamage(void)
{
    gGRCommonStruct.inishie.pblock_appear_wait--;

    if (gGRCommonStruct.inishie.pblock_appear_wait == 0)
    {
        ftMainClearHazard(gGRCommonStruct.inishie.pblock_gobj);
    }
}

/* grinishie.c:488-505 grInishiePowerBlockProcUpdate 0x80109968,
 * verbatim. */
void grInishiePowerBlockProcUpdate(GObj *ground_gobj)
{
    switch (gGRCommonStruct.inishie.pblock_status)
    {
    case nGRInishiePowerBlockStatusWait:
        grInishiePowerBlockUpdateWait();
        break;

    case nGRInishiePowerBlockStatusMake:
        grInishiePowerBlockUpdateMake();
        break;

    case nGRInishiePowerBlockStatusDamage:
        grInishiePowerBlockUpdateDamage();
        break;
    }
}

/* grinishie.c:507-534 grInishieMakePowerBlock 0x801099D4, verbatim but
 * for the attack descriptor's address.
 *
 * DIVERGES: the decomp computes `attack_coll` out of the map FILE's own
 * base -- `(uintptr_t)gMPCollisionGroundData -
 * (intptr_t)&llGRInishieMapMapHeader` plus
 * `&llGRInishieMapPowerBlockGRAttackColl` -- which is arithmetic the port
 * has no image for. The port's stage pack carries that one GRAttackColl
 * as its own `attack_coll` (tools/export/ssb_stageexport.py's stage entry,
 * Stage.attack_coll), so it is read from there.
 *
 * The positions ARE the map's, and the count is what decides whether the
 * stage can run: a Mushroom Kingdom with no nMPMapObjKindPowerBlock
 * object loops forever printing, which is the DECOMP'S OWN error arm and
 * is kept verbatim rather than softened. */
void grInishieMakePowerBlock(void)
{
    s32 pos_count, i, pos_ids[10];

    gcAddGObjProcess(gcMakeGObjSPAfter(nGCCommonKindGround, NULL, nGCCommonLinkIDGround, GOBJ_PRIORITY_DEFAULT), grInishiePowerBlockProcUpdate, nGCProcessKindFunc, 4);

    gGRCommonStruct.inishie.pblock_pos_count = pos_count = mpCollisionGetMapObjCountKind(nMPMapObjKindPowerBlock);

    if ((pos_count == 0) || (pos_count > (s32)ARRAY_COUNT(pos_ids)))
    {
        while (TRUE)
        {
            syDebugPrintf("PowerBlock positions are error!\n");
            scManagerRunPrintGObjStatus();
        }
    }
    gGRCommonStruct.inishie.pblock_pos_ids = (u8*) syTaskmanMalloc(pos_count * sizeof(*gGRCommonStruct.inishie.pblock_pos_ids), 0x0);

    mpCollisionGetMapObjIDsKind(nMPMapObjKindPowerBlock, pos_ids);

    for (i = 0; i < pos_count; i++)
    {
        gGRCommonStruct.inishie.pblock_pos_ids[i] = pos_ids[i];
    }
    gGRCommonStruct.inishie.pblock_status = nGRInishiePowerBlockStatusWait;
    gGRCommonStruct.inishie.attack_coll = stage_bound()->attack_coll;

#ifndef FT_HOSTTEST
    /* The port's own line, the same trade grjungle.c's `tarucann:` and
     * grcastle.c's `castle:` make. This stage's `?` block is a picture
     * thirty seconds into a match, and a serial log is the only place
     * its numbers can be checked at all: how many positions the map
     * tagged, whether the attack descriptor came through the pack, and
     * which object ids they are. Disc probe: `--serial`, grep "pblock:". */
    syDebugPrintf("pblock: %d position(s), attack_coll %s, status %d\n",
                  (int)gGRCommonStruct.inishie.pblock_pos_count,
                  (gGRCommonStruct.inishie.attack_coll != NULL) ? "yes" : "NO",
                  (int)gGRCommonStruct.inishie.pblock_status);
#endif
}

/* grinishie.c:536-543 grInishiePowerBlockSetDamage 0x80109B4C,
 * verbatim: the item calls this when it is hit, and it registers the
 * item as a ground HAZARD for two tics. */
void grInishiePowerBlockSetDamage(void)
{
    ftMainCheckAddGroundHazard(gGRCommonStruct.inishie.pblock_gobj, grInishiePowerBlockCheckGetDamageKind);

    gGRCommonStruct.inishie.pblock_appear_wait = 2;
    gGRCommonStruct.inishie.pblock_status = nGRInishiePowerBlockStatusDamage;
}

/* grinishie.c:545-561 grInishiePowerBlockCheckGetDamageKind 0x80109B8C,
 * verbatim: a GROUNDED fighter that is not the one the block last
 * damaged gets the nGMHitEnvironmentPowerBlock hit, with the stage's own
 * attack descriptor. That kind's consumer has been in src/dc/ftmain.c
 * and this is what fires it. */
sb32 grInishiePowerBlockCheckGetDamageKind(GObj *item_gobj, GObj *fighter_gobj, GRAttackColl **gr_attack_coll, s32 *kind)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if (fp->ga == nMPKineticsGround)
    {
        ITStruct *ip = itGetStruct(item_gobj);

        if (fighter_gobj != ip->damage_gobj)
        {
            *gr_attack_coll = gGRCommonStruct.inishie.attack_coll;
            *kind = nGMHitEnvironmentPowerBlock;

            return TRUE;
        }
    }
    return FALSE;
}

/* grinishie.c:570-580 grInishieMakeGround 0x80109C0C, verbatim: the
 * stage's entry point. The Scale is the stage's platform hazard and the
 * `?` block its other one; the Piranha Plants
 * (grInishieMakePakkun) are the third and are still to come. */
GObj* grInishieMakeGround(void)
{
    grInishieMakeScale();
    grInishieMakePakkun();
    grInishieMakePowerBlock();

    return NULL;
}

/* grinishie.c:583 0x80109C48, verbatim. The decomp's own comment asks
 * whether anything calls it; nothing does -- the alt and the
 * acceleration it reports are the two fields grInishieScaleUpdateWait
 * keeps, and the game has no reader for either. Ported for the same
 * reason the decomp keeps it. */
void grInishieScaleGetPlatformInfo(f32 *alt, f32 *accel)
{
    *alt = 1100.0F - ((gGRCommonStruct.inishie.splat_alt < 0.0F) ? -gGRCommonStruct.inishie.splat_alt : gGRCommonStruct.inishie.splat_alt);

    *accel = (gGRCommonStruct.inishie.splat_accelerate < 0.0F) ? -gGRCommonStruct.inishie.splat_accelerate : gGRCommonStruct.inishie.splat_accelerate;
}

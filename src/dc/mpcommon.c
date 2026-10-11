/* mpcommon.c -- ssb-decomp-re/src/mp/mpcommon.c, the fighter-facing
 * collision routines, plus the port's geometry loader.
 *
 * Every function below names the decomp function and line range it is
 * copied from. Bodies are the decomp's text with `ftGetStruct` reading
 * the FTStruct off the GObj* (ft/fighter.h ftGetStruct). Deviations are
 * marked DIVERGES at the line they happen.
 *
 * Not carried over from mp/mpcommon.c, and why:
 *   mpCommonRunItemCollisionDefault (968-976): no items yet.
 *
 * mpCommonRunWeaponCollisionDefault (978-985) comes with
 * wpManagerMakeWeapon (src/dc/wpmanager.c): it is the weapon twin of the
 * fighter routine below, over the same trio of helpers.
 */
#include "mpcommon.h"

#include <stdint.h>             /* intptr_t, the slope contour's one cast */
#include <stdlib.h>
#include <string.h>

#include <ft/ftcommon.h>       /* FTCOMMON_ATTACKAIR_SKIPLANDING_VEL_Y_MAX */
#include <ft/ftpublic.h>       /* ftPublicPlayCliffReact */
#include <wp/weapon.h>         /* wpGetStruct, WPStruct */
#include <it/item.h>           /* itGetStruct, ITStruct */
#include <sc/scene.h>
#include <sys/debug.h>         /* syDebugPrintf */
#include <sys/utils.h>         /* syUtilsTan, syUtilsArcTan2 */
#include <ft/ftparam.h>        /* func_ovl2_800EBC0C, 800EBD08 */
#include <gm/gmcollision.h>    /* gmCollisionGetFighterPartsWorldPosition, 800EE018 */

#include "ftcommon.h"
#include "taskman.h"
#include "overlay.h"

/* ---- geometry loader ------------------------------------------------ */

/* The collision layer's DObjs, one per yakumono id (mp/mpcollision.c:
 * 3595-3613 mpCollisionAllocYakumono sizes this off the layer's
 * DObjDesc list). No ground-layer animation drives them yet, so each
 * stays at the DObj zero state: translate 0, no anim_joint, user_data.s
 * == nMPYakumonoStatusNone -- exactly what the walk code tests for a
 * static line. */
static DObj *sYakumonoDObjs;

void mpCollisionLoadGeometry(MPGeometryData *gdata)
{
    s32 yakumonos_num;
    s32 i;

    /* mp/mpcollision.c:3971-3978 */
    if (gdata == NULL)
    {
        syDebugPrintf("not found cll data!\n");
        scManagerRunPrintGObjStatus();
    }

    /* Everything below comes out of the scene's general heap, which is
     * where the game puts it too (syTaskmanMalloc, mpcollision.c:3432,
     * 3604-3605, 3924). Nothing is freed one allocation at a time, and
     * nothing needs to be: syTaskmanStartTask empties the whole region on
     * the way into every scene, so a stage's tables live exactly as long
     * as the scene that loaded them.
     *
     * This used to reset the region itself, from back when the port bound
     * a stage before starting a scene rather than inside one. It cannot
     * now -- the object pools are cut from the same region and are
     * already in it by the time a scene's func_start runs. */

    for (i = 0; i < nMPLineKindEnumCount; i++)
    {
        gMPCollisionLineGroups[i].line_id = NULL;
        gMPCollisionLineGroups[i].line_count = 0;
    }
    gMPCollisionVertexInfo = NULL;
    gMPCollisionYakumonoDObjs = NULL;
    sYakumonoDObjs = NULL;
    gMPCollisionSpeeds = NULL;

    /* mp/mpcollision.c:3979-3995 mpCollisionInitGroundData, from the
     * table pointers instead of gMPCollisionGroundData->map_geometry */
    gMPCollisionGeometry = gdata;

    gMPCollisionVertexData  = gdata->vertex_data;
    gMPCollisionVertexIDs   = gdata->vertex_id;
    gMPCollisionVertexLinks = gdata->vertex_links;
    gMPCollisionMapObjs     = gdata->mapobjs;

    gMPCollisionLinesNum = mpCollisionAllocLinesGetCountTotal();

    mpCollisionInitLineIDsAll();
    mpCollisionAllocVertexInfo();
    func_ovl2_800FB554();

    /* mp/mpcollision.c:3595-3613 mpCollisionAllocYakumono. DIVERGES:
     * the game counts the collision layer's DObjDescs; the stage pack
     * carries no DObj list for that layer, so the count is the highest
     * yakumono id the line groups reference, plus one. The DObjs
     * themselves are the port's static stand-ins (sYakumonoDObjs). */
    yakumonos_num = 0;
    for (i = 0; i < gdata->yakumono_count; i++)
    {
        if (gdata->line_info[i].yakumono_id + 1 > yakumonos_num)
        {
            yakumonos_num = gdata->line_info[i].yakumono_id + 1;
        }
    }
    /* mp/mpcollision.c:3604-3605 mpCollisionAllocYakumono, at its own
     * alignments. calloc's zeroing is the port's: the game's DObjs come
     * from gcAddDObj, which zeroes them, and the walk code reads
     * anim_joint.event32 and translate before anything writes them. */
    sYakumonoDObjs = syTaskmanMalloc(yakumonos_num * sizeof(DObj), 0x8);
    memset(sYakumonoDObjs, 0, yakumonos_num * sizeof(DObj));
    gMPCollisionYakumonoDObjs = syTaskmanMalloc(
        yakumonos_num * sizeof(DObj *), 0x4);
    gMPCollisionSpeeds = syTaskmanMalloc(yakumonos_num * sizeof(Vec3f), 0x4);
    for (i = 0; i < yakumonos_num; i++)
    {
        gMPCollisionYakumonoDObjs->dobjs[i] = &sYakumonoDObjs[i];
        gMPCollisionSpeeds[i].x = gMPCollisionSpeeds[i].y = gMPCollisionSpeeds[i].z = 0.0F;
    }
    gMPCollisionYakumonosNum = yakumonos_num;

    /* mp/mpcollision.c:4029-4040 mpCollisionClearYakumonoAll, over the
     * count instead of the DObjDesc list. Done again in
     * grCommonSetupInitAll, where the game does it -- this one is what
     * makes the stand-ins safe to read in the window before the layers
     * are built. */
    for (i = 0; i < yakumonos_num; i++)
    {
        gMPCollisionYakumonoDObjs->dobjs[i]->user_data.s = nMPYakumonoStatusNone;
    }
    gMPCollisionUpdateTic = 0;

#ifdef FT_HOSTTEST
    /* The host has no geometry layers and never calls
     * grCommonSetupInitAll's real body, so mpcommon's stand-ins ARE its
     * yakumono DObjs and this is the only place the bounds can be sized.
     * hosttest_ft.c's CPU targeting tests read gMPCollisionBounds. */
    mpCollisionInitYakumonoAll();
#endif

    /* mp/mpcollision.c:3784-3891, the stage bounds, on the target USED TO
     * BE CALLED HERE and is not any more: it reads
     * gMPCollisionYakumonoDObjs, and
     * until the geometry layers are built those are mpcommon's zeroed
     * stand-ins rather than the collision layer's own DObjs. The game
     * calls it in grCommonSetupInitAll, after the four layers and after
     * mpCollisionClearYakumonoAll (gr/grcommonsetup.c:29-31), and
     * src/dc/stage.c now calls it there too. Sizing the stage's bounds
     * against a platform parked at the origin is the bug that hid here
     * for as long as the stand-ins were all there was.
     *
     * mp/mpcollision.c:3997-4010: light colour and angles are the PVR
     * scene's business, not read here. */
}

/* ---- mp/mpcommon.c ---------------------------------------------------- */

/* mp/mpcommon.c:21-94 mpCommonCheckSetFighterCliffEdge. The `*translate =
 * edge_pos` snap (line 86) is the game's -- it is what makes a slow walk
 * off an edge settle 40 units in from it, in Ottotto. */
sb32 mpCommonCheckSetFighterCliffEdge(GObj *fighter_gobj, s32 floor_line_id)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    MPObjectColl *map_coll = &fp->coll_data.map_coll;
    Vec3f *translate = fp->coll_data.p_translate;
    Vec3f edge_pos;
    Vec3f sp4C;
    Vec3f angle;
    u32 flags;
    f32 floor_dist;

    if (mpCollisionCheckExistLineID(floor_line_id) == FALSE)
    {
        return FALSE;
    }
    else
    {
        mpCollisionGetFloorEdgeL(floor_line_id, &edge_pos);

        if (translate->x <= edge_pos.x)
        {
            if ((fp->lr == -1) && (fp->input.pl.stick_range.x > -60))
            {
                edge_pos.x += 40.0F;

                mpCollisionGetFCCommonFloor(floor_line_id, &edge_pos, &floor_dist, &flags, &angle);

                edge_pos.y += floor_dist;
                sp4C.x = map_coll->width + edge_pos.x;
                sp4C.y = (map_coll->center + edge_pos.y) - map_coll->bottom;

                if (mpCollisionCheckLWallLineCollisionSame(&edge_pos, &sp4C, NULL, NULL, NULL, NULL) == FALSE)
                {
                    fp->lr = -1;

                    goto setground;
                }
            }
        }
        else if ((fp->lr == +1) && (fp->input.pl.stick_range.x < 60))
        {
            mpCollisionGetFloorEdgeR(floor_line_id, &edge_pos);

            edge_pos.x -= 40.0F;

            mpCollisionGetFCCommonFloor(floor_line_id, &edge_pos, &floor_dist, &flags, &angle);

            edge_pos.y += floor_dist;
            sp4C.x = edge_pos.x - map_coll->width;
            sp4C.y = (map_coll->center + edge_pos.y) - map_coll->bottom;

            if (mpCollisionCheckRWallLineCollisionSame(&edge_pos, &sp4C, NULL, NULL, NULL, NULL) == FALSE)
            {
                fp->lr = +1;

                goto setground;
            }
        }
    }
    return FALSE;

setground:
    *translate = edge_pos;

    fp->coll_data.floor_line_id = floor_line_id;
    fp->coll_data.floor_flags = flags;
    fp->coll_data.floor_angle = angle;
    fp->coll_data.floor_dist = 0.0F;

    fp->coll_data.mask_stat |= MAP_FLAG_FLOOREDGE;

    return TRUE;
}

/* mp/mpcommon.c:96-161 mpCommonCheckSetFighterEdge */
sb32 mpCommonCheckSetFighterEdge(GObj *fighter_gobj, s32 floor_line_id)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    MPObjectColl *map_coll = &fp->coll_data.map_coll;
    Vec3f *translate = fp->coll_data.p_translate;
    Vec3f edge_pos;
    Vec3f sp4C;
    Vec3f sp40;
    Vec3f angle;
    u32 flags;
    f32 floor_dist;

    if (mpCollisionCheckExistLineID(floor_line_id) == FALSE)
    {
        return FALSE;
    }
    else
    {
        mpCollisionGetFloorEdgeL(floor_line_id, &edge_pos);

        if (translate->x <= edge_pos.x)
        {
            mpCollisionGetFCCommonFloor(floor_line_id, &edge_pos, &floor_dist, &flags, &angle);

            sp4C.x = edge_pos.x + 1.0F;
            sp4C.y = edge_pos.y + 1.0F;

            sp40.x = map_coll->width + edge_pos.x;
            sp40.y = (map_coll->center + edge_pos.y) - map_coll->bottom;

            if (mpCollisionCheckLWallLineCollisionSame(&sp4C, &sp40, NULL, NULL, NULL, NULL) == FALSE)
            {
                goto setground;
            }
        }
        else
        {
            mpCollisionGetFloorEdgeR(floor_line_id, &edge_pos);

            mpCollisionGetFCCommonFloor(floor_line_id, &edge_pos, &floor_dist, &flags, &angle);

            sp4C.x = edge_pos.x - 1.0F;
            sp4C.y = edge_pos.y + 1.0F;

            sp40.x = edge_pos.x - map_coll->width;
            sp40.y = (map_coll->center + edge_pos.y) - map_coll->bottom;

            if (mpCollisionCheckRWallLineCollisionSame(&sp4C, &sp40, NULL, NULL, NULL, NULL) == FALSE)
            {
                goto setground;
            }
        }
    }
    return FALSE;

setground:
    *translate = edge_pos;

    fp->coll_data.floor_line_id = floor_line_id;
    fp->coll_data.floor_flags = flags;
    fp->coll_data.floor_angle = angle;
    fp->coll_data.floor_dist = 0.0F;

    return TRUE;
}

/* mp/mpcommon.c:163-220 mpCommonRunFighterAllCollisions -- the grounded
 * walk: walls, then the current floor (edge behaviour by flags), then a
 * floor crossed on the way */
sb32 mpCommonRunFighterAllCollisions(MPCollData *coll_data, GObj *fighter_gobj, u32 flags)
{
    s32 floor_line_id = coll_data->floor_line_id;
    sb32 is_floor = FALSE;

    if (mpProcessCheckTestLWallCollision(coll_data) != FALSE)
    {
        mpProcessRunLWallCollision(coll_data);

        coll_data->is_coll_end = TRUE;
    }
    if (mpProcessCheckTestRWallCollision(coll_data) != FALSE)
    {
        mpProcessRunRWallCollision(coll_data);

        coll_data->is_coll_end = TRUE;
    }
    if (mpProcessCheckTestFloorCollisionNew(coll_data) != FALSE)
    {
        if (coll_data->mask_stat & MAP_FLAG_FLOOR)
        {
            mpProcessRunFloorEdgeAdjust(coll_data);

            is_floor = TRUE;
        }
    }
    else if (flags & MAP_PROC_TYPE_CLIFFEDGE)
    {
        mpCommonCheckSetFighterCliffEdge(fighter_gobj, floor_line_id);

        coll_data->is_coll_end = TRUE;
    }
    else if (flags & MAP_PROC_TYPE_STOPEDGE)
    {
        if (mpCommonCheckSetFighterEdge(fighter_gobj, floor_line_id) != FALSE)
        {
            is_floor = TRUE;
        }
        else coll_data->is_coll_end = TRUE;
    }
    else coll_data->is_coll_end = TRUE;

    if (mpProcessCheckTestFloorCollision(coll_data, floor_line_id) != FALSE)
    {
        mpProcessSetLandingFloor(coll_data);

        if (coll_data->mask_stat & MAP_FLAG_FLOOR)
        {
            mpProcessRunFloorEdgeAdjust(coll_data);

            is_floor = TRUE;
        }
        coll_data->mask_stat &= ~MAP_FLAG_FLOOREDGE;
        coll_data->is_coll_end = FALSE;
    }
    return is_floor;
}

/* mp/mpcommon.c:222-228 */
sb32 mpCommonCheckFighterOnFloor(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    return mpProcessUpdateMain(&fp->coll_data, mpCommonRunFighterAllCollisions, fighter_gobj, MAP_PROC_TYPE_DEFAULT);
}

/* mp/mpcommon.c:230-240 */
sb32 mpCommonProcFighterOnFloor(GObj *fighter_gobj, void (*proc_map)(GObj*))
{
    if (mpCommonCheckFighterOnFloor(fighter_gobj) == FALSE)
    {
        proc_map(fighter_gobj);

        return FALSE;
    }
    else return TRUE;
}

/* mp/mpcommon.c:242-248 */
sb32 mpCommonCheckFighterOnCliffEdge(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    return mpProcessUpdateMain(&fp->coll_data, mpCommonRunFighterAllCollisions, fighter_gobj, MAP_PROC_TYPE_CLIFFEDGE);
}

/* mp/mpcommon.c:250-256 */
sb32 mpCommonCheckFighterOnEdge(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    return mpProcessUpdateMain(&fp->coll_data, mpCommonRunFighterAllCollisions, fighter_gobj, MAP_PROC_TYPE_STOPEDGE);
}

/* mp/mpcommon.c:258-268 */
sb32 mpCommonProcFighterOnEdge(GObj *fighter_gobj, void (*proc_map)(GObj*))
{
    if (mpCommonCheckFighterOnEdge(fighter_gobj) == FALSE)
    {
        proc_map(fighter_gobj);

        return FALSE;
    }
    else return TRUE;
}

/* mp/mpcommon.c:270-274 */
void mpCommonSetFighterFallOnGroundBreak(GObj *fighter_gobj)
{
    mpCommonProcFighterOnFloor(fighter_gobj, ftCommonFallSetStatus);
}

/* mp/mpcommon.c:276-289 -- the map proc of Wait and the walks: leaving
 * the floor slowly teeters (Ottotto), otherwise falls */
void mpCommonProcFighterOnCliffEdge(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if (mpCommonCheckFighterOnCliffEdge(fighter_gobj) == FALSE)
    {
        if (fp->coll_data.mask_stat & MAP_FLAG_FLOOREDGE)
        {
            ftCommonOttottoSetStatus(fighter_gobj);
        }
        else ftCommonFallSetStatus(fighter_gobj);
    }
}

/* mp/mpcommon.c:291-298 */
void mpCommonSetFighterFallOnEdgeBreak(GObj *fighter_gobj)
{
    if (mpCommonCheckFighterOnEdge(fighter_gobj) == FALSE)
    {
        ftCommonFallSetStatus(fighter_gobj);
    }
}

/* mp/mpcommon.c:299-405, verbatim but for one cast: the
 * slope contour, fp->proc_slope for every status (ftcommon.c's
 * ftMainSetStatus). On a sloped floor each foot flagged by the motion
 * script's SetSlopeContour event is moved onto the floor line under it,
 * clamped by attr->unk_0x320 below the root and attr->unk_0x31C below the
 * foot's parent, and the leg bent to reach it (ft/ftparam.c 800EBC0C /
 * 800EBD08, src/dc/ftparam.c). DIVERGES: 800EBC0C's unused first
 * parameter is an s32 the game passes fp to; the port casts through
 * intptr_t, which the 64-bit host build needs. */
// 0x800DDF74
sb32 func_ovl2_800DDF74(GObj *fighter_gobj, FTStruct *fp, FTAttributes *attr, DObj *target_joint, Vec3f *vec)
{
    Vec3f sp64;
    Vec3f vec_translate;
    Vec3f sp4C;
    f32 sp48;
    DObj *joint;
    f32 tangent;
    u32 sp3C;
    f32 ternary;
    f32 translate_y;

    if (mpCollisionGetFCCommonFloor(fp->coll_data.floor_line_id, vec, &sp48, &sp3C, &sp64) != FALSE)
    {
        translate_y = (vec->y + sp48) - DObjGetStruct(fighter_gobj)->translate.vec.f.y;
    }
    else
    {
        mpCollisionGetFloorEdgeL(fp->coll_data.floor_line_id, &vec_translate);

        if (vec_translate.x < vec->x)
        {
            mpCollisionGetFloorEdgeR(fp->coll_data.floor_line_id, &vec_translate);
        }
        translate_y = vec_translate.y - DObjGetStruct(fighter_gobj)->translate.vec.f.y;
    }

    ternary = ABSF(translate_y);

    if (ternary < 0.0001F)
    {
        return FALSE;
    }

    vec->y += translate_y;

    joint = DObjGetStruct(fighter_gobj);

    if (vec->y < joint->translate.vec.f.y)
    {
        ternary = ((joint->translate.vec.f.x < vec->x) ? -(joint->translate.vec.f.x - vec->x) : (joint->translate.vec.f.x - vec->x));

        tangent = DObjGetStruct(fighter_gobj)->translate.vec.f.y - ternary * syUtilsTan(attr->unk_0x320);

        if (vec->y < tangent)
        {
            vec->y = tangent;
        }
    }

    sp4C.x = sp4C.y = sp4C.z = 0;

    gmCollisionGetFighterPartsWorldPosition(target_joint, &sp4C);

    if (vec->y > sp4C.y - attr->unk_0x31C)
    {
        vec->y = sp4C.y - attr->unk_0x31C;
    }
    return TRUE;
}

// 0x800DE150
void mpCommonUpdateFighterSlopeContour(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    FTAttributes *attr = fp->attr;
    DObj *joint;
    Vec3f sp30;
    f32 sp2C;

    if (fp->ga == nMPKineticsGround)
    {
        if ((fp->coll_data.floor_line_id != -1) && (fp->coll_data.floor_line_id != -2) && (fp->hitlag_tics <= 0))
        {
            if (fp->slope_contour & FTSLOPECONTOUR_FLAG_RFOOT)
            {
                joint = fp->joints[attr->joint_rfoot_id];

                func_ovl2_800EBC0C((intptr_t)fp, &sp30, &sp2C, attr->joint_rfoot_rotate, joint);

                if (func_ovl2_800DDF74(fighter_gobj, fp, attr, joint, &sp30) != FALSE)
                {
                    func_ovl2_800EE018(fp->joints[attr->joint_rfoot_id], &sp30);
                    func_ovl2_800EBD08(fp->joints[attr->joint_rfoot_id], attr->joint_rfoot_rotate, &sp30, sp2C);
                }
            }
            if (fp->slope_contour & FTSLOPECONTOUR_FLAG_LFOOT)
            {
                joint = fp->joints[attr->joint_lfoot_id];

                func_ovl2_800EBC0C((intptr_t)fp, &sp30, &sp2C, attr->joint_lfoot_rotate, joint);

                if (func_ovl2_800DDF74(fighter_gobj, fp, attr, joint, &sp30) != FALSE)
                {
                    func_ovl2_800EE018(fp->joints[attr->joint_lfoot_id], &sp30);
                    func_ovl2_800EBD08(fp->joints[attr->joint_lfoot_id], attr->joint_lfoot_rotate, &sp30, sp2C);
                }
            }
            if (fp->slope_contour & FTSLOPECONTOUR_FLAG_FULL)
            {
                DObjGetStruct(fighter_gobj)->rotate.vec.f.x = (syUtilsArcTan2(fp->coll_data.floor_angle.x, fp->coll_data.floor_angle.y) * fp->lr);
            }
        }
    }
    else return;
}

/* mp/mpcommon.c:408-414 */
void mpCommonSetFighterProjectFloor(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    mpProcessSetCollProjectFloorID(&fp->coll_data);
}

/* mp/mpcommon.c:416-420 */
void mpCommonUpdateFighterProjectFloor(GObj *fighter_gobj)
{
    mpCommonSetFighterProjectFloor(fighter_gobj);
}

/* mp/mpcommon.c:422-470 mpCommonSetFighterLandingParams, verbatim: the
 * crowd half is a landing that follows a big knockback, taken near either
 * blast-zone edge -- a save, and the audience gasps for it (ft/ftmain.c:1821
 * clears public_knockback the moment the fighter is back inside those
 * bounds, so reaching here with it still set is what "recovered from the
 * edge" means). The switch under it hands back the special move a fighter
 * spends in the air (Mario's tornado, Samus's charge, ...). */
void mpCommonSetFighterLandingParams(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if (fp->public_knockback != 0.0F)
    {
        if (fp->public_knockback >= 100.0F)
        {
            if
            (
                (fp->joints[nFTPartsJointTopN]->translate.vec.f.x < (gMPCollisionBounds.current.left + 450.0F)) ||
                (fp->joints[nFTPartsJointTopN]->translate.vec.f.x > (gMPCollisionBounds.current.right - 450.0F))
            )
            {
                ftPublicPlayCliffReact(fighter_gobj, fp->public_knockback);
            }
        }
        fp->public_knockback = 0.0F;
    }
    switch (fp->fkind)
    {
    case nFTKindMario:
    case nFTKindMMario:
    case nFTKindNMario:
        fp->passive_vars.mario.is_expend_tornado = FALSE;
        break;

    case nFTKindSamus:
    case nFTKindNSamus:
        fp->passive_vars.samus.charge_recoil = 0;
        return;

    case nFTKindLuigi:
    case nFTKindNLuigi:
        fp->passive_vars.mario.is_expend_tornado = FALSE;
        break;

    case nFTKindCaptain:
    case nFTKindNCaptain:
        fp->passive_vars.captain.falcon_punch_unk = FALSE;
        break;

    case nFTKindPurin:
    case nFTKindNPurin:
        fp->passive_vars.purin.unk_0x0 = FALSE;
        break;
    }
}

/* mp/mpcommon.c:12 -- the Pass floor check threads its per-status
 * proc_map predicate (ftCommonFallSpecialProcPass, etc.) through this
 * file-static; mpCommonCheckFighterPassCliff sets it before the walk. */
static sb32 (*sMPCommonProcPass)(GObj*);

/* mp/mpcommon.c:472-579 mpCommonRunFighterSpecialCollisions -- the
 * airborne walk: walls, ceiling (with the head-bonk flag), floor
 * (landing, or projection only), then the ledge probes */
sb32 mpCommonRunFighterSpecialCollisions(MPCollData *coll_data, GObj *fighter_gobj, u32 flags)
{
    FTStruct *this_fp = ftGetStruct(fighter_gobj);
    FTStruct *cliffcatch_fp;
    GObj *cliffcatch_gobj;
    sb32 is_ceilstop = FALSE;
    sb32 is_collide;

    if (mpProcessCheckTestLWallCollisionAdjNew(coll_data) != FALSE)
    {
        mpProcessRunLWallCollisionAdjNew(coll_data);
    }
    if (mpProcessCheckTestRWallCollisionAdjNew(coll_data) != FALSE)
    {
        mpProcessRunRWallCollisionAdjNew(coll_data);
    }
    if (mpProcessCheckTestCeilCollisionAdjNew(coll_data) != FALSE)
    {
        mpProcessRunCeilCollisionAdjNew(coll_data);

        if (coll_data->mask_stat & MAP_FLAG_CEIL)
        {
            mpProcessRunCeilEdgeAdjust(coll_data);
        }
        if ((flags & MAP_PROC_TYPE_CEILHEAVY) && (this_fp->physics.vel_air.y >= 30.0F))
        {
            coll_data->mask_curr |= MAP_FLAG_CEILHEAVY;

            is_ceilstop = TRUE;

            coll_data->is_coll_end = TRUE;
        }
    }
    /* mp/mpcommon.c:504-514: the Pass variant threads its proc_map
     * (sMPCommonProcPass) through the floor check so a helpless-falling
     * fighter can drop through a soft platform its ProcPass allows;
     * every other status still takes the NULL form. */
    is_collide = (flags & MAP_PROC_TYPE_PASS)
        ? mpProcessCheckTestFloorCollisionAdjNew(coll_data, sMPCommonProcPass, fighter_gobj)
        : mpProcessRunFloorCollisionAdjNewNULL(coll_data);

    if (is_collide != FALSE)
    {
        if (flags & MAP_PROC_TYPE_PROJECT)
        {
            mpProcessSetCollideFloor(coll_data);

            if (coll_data->mask_stat & MAP_FLAG_FLOOR)
            {
                mpProcessRunFloorEdgeAdjust(coll_data);
            }
            else mpProcessSetCollProjectFloorID(coll_data);
        }
        else
        {
            mpProcessSetLandingFloor(coll_data);
            mpCommonSetFighterLandingParams(fighter_gobj);

            if (coll_data->mask_stat & MAP_FLAG_FLOOR)
            {
                mpProcessRunFloorEdgeAdjust(coll_data);

                coll_data->is_coll_end = TRUE;

                return TRUE;
            }
        }
    }
    else mpProcessSetCollProjectFloorID(coll_data);

    if ((flags & MAP_PROC_TYPE_CLIFF) && (this_fp->cliffcatch_wait == 0))
    {
        if ((mpProcessCheckTestLCliffCollision(coll_data) != FALSE) || (mpProcessCheckTestRCliffCollision(coll_data) != FALSE))
        {
            /* mp/mpcommon.c:553-569: a ledge another fighter already
             * holds from the same side is refused */
            cliffcatch_gobj = gGCCommonLinks[nGCCommonLinkIDFighter];

            while (cliffcatch_gobj != NULL)
            {
                if (cliffcatch_gobj != fighter_gobj)
                {
                    cliffcatch_fp = ftGetStruct(cliffcatch_gobj);

                    if ((cliffcatch_fp->is_cliff_hold) && (this_fp->coll_data.cliff_id == cliffcatch_fp->coll_data.cliff_id) && (this_fp->lr == cliffcatch_fp->lr))
                    {
                        return is_ceilstop;
                    }
                    else goto next_gobj; // Bruh
                }
            next_gobj:
                cliffcatch_gobj = cliffcatch_gobj->link_next;
            }
            mpCommonSetFighterLandingParams(fighter_gobj);

            coll_data->is_coll_end = TRUE;

            return TRUE;
        }
    }
    return is_ceilstop;
}

/* mp/mpcommon.c:609-617 mpCommonCheckFighterPass 0x800DE758 -- the same
 * walk as the pair below without the ledge half: stash the pass
 * predicate, then the special collisions with PASS alone. Fire Fox's
 * aerial ProcMap uses it: mid-flight Fox passes through a
 * drop-through platform for fifteen tics and grabs no ledge. */
sb32 mpCommonCheckFighterPass(GObj *fighter_gobj, sb32 (*proc_map)(GObj*))
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    sMPCommonProcPass = proc_map;

    return mpProcessUpdateMain(&fp->coll_data, mpCommonRunFighterSpecialCollisions, fighter_gobj, MAP_PROC_TYPE_PASS);
}

/* mp/mpcommon.c:619-627 mpCommonCheckFighterPassCliff -- the airborne
 * pass/ledge walk ftCommonFallSpecial's ProcMap runs: stash the pass
 * predicate, then the special collisions with PASS | CLIFF. */
sb32 mpCommonCheckFighterPassCliff(GObj *fighter_gobj, sb32 (*proc_map)(GObj*))
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    sMPCommonProcPass = proc_map;

    return mpProcessUpdateMain(&fp->coll_data, mpCommonRunFighterSpecialCollisions, fighter_gobj, MAP_PROC_TYPE_PASS | MAP_PROC_TYPE_CLIFF);
}

/* mp/mpcommon.c:581-587 */
sb32 mpCommonCheckFighterLanding(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    return mpProcessUpdateMain(&fp->coll_data, mpCommonRunFighterSpecialCollisions, fighter_gobj, MAP_PROC_TYPE_DEFAULT);
}

/* mp/mpcommon.c:589-599 */
sb32 mpCommonProcFighterLanding(GObj *fighter_gobj, void (*proc_map)(GObj*))
{
    if (mpCommonCheckFighterLanding(fighter_gobj) != FALSE)
    {
        proc_map(fighter_gobj);

        return TRUE;
    }
    else return FALSE;
}

/* mp/mpcommon.c:601-607 -- airborne, projected onto the floor below */
sb32 mpCommonCheckFighterProject(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    return mpProcessUpdateMain(&fp->coll_data, mpCommonRunFighterSpecialCollisions, fighter_gobj, MAP_PROC_TYPE_PROJECT);
}

/* mp/mpcommon.c:725-838 mpCommonProcFighterDamage 0x800DEA20, verbatim:
 * the map-collision callback a launched (Fly/tumble)
 * fighter runs. It tests the four faces the way the ordinary fighter
 * collision does, but records what it hit in the damage status vars
 * (coll_mask_curr / _ignore) and, off hitlag with a steep enough
 * approach, lands the fighter -- which is how ftCommonDamageAirCommon
 * ProcMap and ftCommonDamageFallProcMap learn a tumble reached the
 * ground or a wall. */
sb32 mpCommonProcFighterDamage(MPCollData *coll_data, GObj *fighter_gobj, u32 flags)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    sb32 is_collide = FALSE;

    if (mpProcessCheckTestLWallCollisionAdjNew(coll_data) != FALSE)
    {
        mpProcessRunLWallCollisionAdjNew(coll_data);

        if (!(fp->status_vars.common.damage.coll_mask_prev & MAP_FLAG_LWALL) && (lbCommonMag2D(&coll_data->pos_diff) > 30.0F) && (syVectorAngleDiff3D(&coll_data->pos_diff, &coll_data->lwall_angle) > F_CLC_DTOR32(110.0F)))
        {
            fp->status_vars.common.damage.coll_mask_curr |= MAP_FLAG_LWALL;

            is_collide = TRUE;

            coll_data->is_coll_end = TRUE;
        }
        else if (!(coll_data->mask_prev & MAP_FLAG_LWALL))
        {
            fp->status_vars.common.damage.coll_mask_ignore |= MAP_FLAG_LWALL;
        }
    }
    if (mpProcessCheckTestRWallCollisionAdjNew(coll_data) != FALSE)
    {
        mpProcessRunRWallCollisionAdjNew(coll_data);

        if (!(fp->status_vars.common.damage.coll_mask_prev & MAP_FLAG_RWALL) && (lbCommonMag2D(&coll_data->pos_diff) > 30.0F) && (syVectorAngleDiff3D(&coll_data->pos_diff, &coll_data->rwall_angle) > F_CLC_DTOR32(110.0F)))
        {
            fp->status_vars.common.damage.coll_mask_curr |= MAP_FLAG_RWALL;

            is_collide = TRUE;

            coll_data->is_coll_end = TRUE;
        }
        else if (!(coll_data->mask_prev & MAP_FLAG_RWALL))
        {
            fp->status_vars.common.damage.coll_mask_ignore |= MAP_FLAG_RWALL;
        }
    }
    if (mpProcessCheckTestCeilCollisionAdjNew(coll_data) != FALSE)
    {
        mpProcessRunCeilCollisionAdjNew(coll_data);

        if (coll_data->mask_stat & MAP_FLAG_CEIL)
        {
            mpProcessRunCeilEdgeAdjust(coll_data);
        }
        if (!(fp->status_vars.common.damage.coll_mask_prev & MAP_FLAG_CEIL) && (lbCommonMag2D(&coll_data->pos_diff) > 30.0F) && (syVectorAngleDiff3D(&coll_data->pos_diff, &coll_data->ceil_angle) > F_CLC_DTOR32(110.0F)))
        {
            fp->status_vars.common.damage.coll_mask_curr |= MAP_FLAG_CEIL;

            is_collide = TRUE;

            coll_data->is_coll_end = TRUE;
        }
        else if (!(coll_data->mask_prev & MAP_FLAG_CEIL))
        {
            fp->status_vars.common.damage.coll_mask_ignore |= MAP_FLAG_CEIL;
        }
    }
    if (mpProcessRunFloorCollisionAdjNewNULL(coll_data) != FALSE)
    {
        if (fp->hitlag_tics > 0)
        {
            mpProcessSetCollideFloor(coll_data);

            if (coll_data->mask_stat & MAP_FLAG_FLOOR)
            {
                mpProcessRunFloorEdgeAdjust(coll_data);
            }
            else mpProcessSetCollProjectFloorID(coll_data);
        }
        else
        {
            if (syVectorAngleDiff3D(&coll_data->pos_diff, &coll_data->floor_angle) > F_CLC_DTOR32(110.0F))
            {
                mpProcessSetLandingFloor(coll_data);
                mpCommonSetFighterLandingParams(fighter_gobj);

                if (coll_data->mask_stat & MAP_FLAG_FLOOR)
                {
                    mpProcessRunFloorEdgeAdjust(coll_data);

                    fp->status_vars.common.damage.coll_mask_curr |= MAP_FLAG_FLOOR;

                    is_collide = TRUE;

                    coll_data->is_coll_end = TRUE;
                }
            }
            else
            {
                mpProcessSetCollideFloor(coll_data);

                if (coll_data->mask_stat & MAP_FLAG_FLOOR)
                {
                    mpProcessRunFloorEdgeAdjust(coll_data);

                    if (!(coll_data->mask_prev & MAP_FLAG_FLOOR))
                    {
                        fp->status_vars.common.damage.coll_mask_ignore |= MAP_FLAG_FLOOR;

                        fp->status_vars.common.damage.wall_collide_angle = coll_data->floor_angle;
                    }
                }
                else mpProcessSetCollProjectFloorID(coll_data);
            }
        }
    }
    else mpProcessSetCollProjectFloorID(coll_data);

    return is_collide;
}

/* mp/mpcommon.c:840-849 mpCommonCheckFighterDamageCollision 0x800DEDAC,
 * verbatim: rolls the damage collision mask forward a
 * frame and runs the pass above. */
sb32 mpCommonCheckFighterDamageCollision(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    fp->status_vars.common.damage.coll_mask_prev = fp->status_vars.common.damage.coll_mask_curr;
    fp->status_vars.common.damage.coll_mask_curr = 0;
    fp->status_vars.common.damage.coll_mask_ignore = 0;

    return mpProcessUpdateMain(&fp->coll_data, mpCommonProcFighterDamage, fighter_gobj, MAP_PROC_TYPE_DEFAULT);
}

/* mp/mpcommon.c:629-635 */
sb32 mpCommonCheckFighterCliff(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    return mpProcessUpdateMain(&fp->coll_data, mpCommonRunFighterSpecialCollisions, fighter_gobj, MAP_PROC_TYPE_CLIFF);
}

/* mp/mpcommon.c:637-655 */
sb32 mpCommonProcFighterCliff(GObj *fighter_gobj, void (*proc_map)(GObj*))
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if (mpCommonCheckFighterCliff(fighter_gobj) != FALSE)
    {
        if (fp->coll_data.mask_stat & MAP_FLAG_CLIFF_MASK)
        {
            ftCommonCliffCatchSetStatus(fighter_gobj);

            return TRUE;
        }
        else proc_map(fighter_gobj);

        return TRUE;
    }
    else return FALSE;
}

/* mp/mpcommon.c:657-663 */
sb32 mpCommonCheckFighterCeilHeavyCliff(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    return mpProcessUpdateMain(&fp->coll_data, mpCommonRunFighterSpecialCollisions, fighter_gobj, MAP_PROC_TYPE_CEILHEAVY | MAP_PROC_TYPE_CLIFF);
}

/* mp/mpcommon.c:665-671 */
sb32 mpCommonCheckFighterCeilHeavy(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    return mpProcessUpdateMain(&fp->coll_data, mpCommonRunFighterSpecialCollisions, fighter_gobj, MAP_PROC_TYPE_CEILHEAVY);
}

/* mp/mpcommon.c:673-683 */
void mpCommonSetFighterWaitOrLanding(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if (fp->physics.vel_air.y > FTCOMMON_ATTACKAIR_SKIPLANDING_VEL_Y_MAX)
    {
        ftCommonWaitSetStatus(fighter_gobj);
    }
    else ftCommonLandingSetStatus(fighter_gobj);
}

/* mp/mpcommon.c:685-689 */
void mpCommonProcFighterWaitOrLanding(GObj *fighter_gobj)
{
    mpCommonProcFighterLanding(fighter_gobj, mpCommonSetFighterWaitOrLanding);
}

/* mp/mpcommon.c:691-695 */
void mpCommonProcFighterProject(GObj *fighter_gobj)
{
    mpCommonCheckFighterProject(fighter_gobj);
}

/* mp/mpcommon.c:697-701 */
void mpCommonProcFighterCliffWaitOrLanding(GObj *fighter_gobj)
{
    mpCommonProcFighterCliff(fighter_gobj, mpCommonSetFighterWaitOrLanding);
}

/* mp/mpcommon.c:703-723 -- the map proc of every jump and fall */
void mpCommonProcFighterCliffFloorCeil(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if (mpCommonCheckFighterCeilHeavyCliff(fighter_gobj) != FALSE)
    {
        if (fp->coll_data.mask_stat & MAP_FLAG_CLIFF_MASK)
        {
            ftCommonCliffCatchSetStatus(fighter_gobj);
        }
        else if (fp->coll_data.mask_stat & MAP_FLAG_FLOOR)
        {
            mpCommonSetFighterWaitOrLanding(fighter_gobj);
        }
        else if (fp->coll_data.mask_curr & MAP_FLAG_CEILHEAVY)
        {
            ftCommonStopCeilSetStatus(fighter_gobj);
        }
    }
}

/* mp/mpcommon.c:852-868 */
void mpCommonUpdateFighterKinetics(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if (fp->ga == nMPKineticsAir)
    {
        if (mpCommonCheckFighterLanding(fighter_gobj) != FALSE)
        {
            mpCommonSetFighterGround(fp);
        }
    }
    else if (mpCommonCheckFighterOnFloor(fighter_gobj) == FALSE)
    {
        mpCommonSetFighterAir(fp);
    }
}

/* mp/mpcommon.c:870-880 */
void mpCommonSetFighterWaitOrFall(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if (fp->ga == nMPKineticsAir)
    {
        ftCommonFallSetStatus(fighter_gobj);
    }
    else ftCommonWaitSetStatus(fighter_gobj);
}

/* mp/mpcommon.c:882-892 mpCommonSetFighterGround, verbatim */
void mpCommonSetFighterGround(FTStruct *fp)
{
    fp->physics.vel_ground.x = fp->physics.vel_air.x * fp->lr;

    fp->ga = nMPKineticsGround;

    fp->jumps_used = 0;

    fp->stat_flags.ga = nMPKineticsGround;
}

/* mp/mpcommon.c:894-902 mpCommonSetFighterAir, verbatim */
void mpCommonSetFighterAir(FTStruct *fp)
{
    fp->ga = nMPKineticsAir;

    fp->physics.vel_air.z = fp->joints[nFTPartsJointTopN]->translate.vec.f.z = 0.0F;

    fp->jumps_used = 1;
}

/* mp/mpcommon.c:904-934 */
void mpCommonRunDefaultCollision(MPCollData *coll_data, GObj *gobj, u32 flags)
{
    (void)gobj;
    (void)flags;

    if (mpProcessCheckTestLWallCollisionAdjNew(coll_data) != FALSE)
    {
        mpProcessRunLWallCollisionAdjNew(coll_data);
    }
    if (mpProcessCheckTestRWallCollisionAdjNew(coll_data) != FALSE)
    {
        mpProcessRunRWallCollisionAdjNew(coll_data);
    }
    if (mpProcessCheckTestCeilCollisionAdjNew(coll_data) != FALSE)
    {
        mpProcessRunCeilCollisionAdjNew(coll_data);

        if (coll_data->mask_stat & MAP_FLAG_CEIL)
        {
            mpProcessRunCeilEdgeAdjust(coll_data);
        }
    }
    if (mpProcessRunFloorCollisionAdjNewNULL(coll_data) != FALSE)
    {
        mpProcessSetCollideFloor(coll_data);

        if (coll_data->mask_stat & MAP_FLAG_FLOOR)
        {
            mpProcessRunFloorEdgeAdjust(coll_data);
        }
    }
    else mpProcessSetCollProjectFloorID(coll_data);
}

/* mp/mpcommon.c:936-947 */
void mpCommonCopyCollDataStats(MPCollData *this_coll_data, Vec3f *pos, MPCollData *other_coll_data)
{
    this_coll_data->pos_prev = *pos;

    this_coll_data->p_map_coll = &other_coll_data->map_coll;
    this_coll_data->mask_curr = 0;
    this_coll_data->mask_unk = 0;
    this_coll_data->mask_stat = 0;
    this_coll_data->is_coll_end = FALSE;
    this_coll_data->update_tic = other_coll_data->update_tic;
}

/* mp/mpcommon.c:949-956 */
void mpCommonResetCollDataStats(MPCollData *coll_data)
{
    coll_data->p_map_coll = &coll_data->map_coll;

    coll_data->update_tic = gMPCollisionUpdateTic;
    coll_data->mask_curr = 0;
}

/* mp/mpcommon.c:958-966 -- one collision pass from an arbitrary start
 * position (the ledge release uses it to step back onto the map) */
void mpCommonRunFighterCollisionDefault(GObj *fighter_gobj, Vec3f *pos, MPCollData *coll_data)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    mpCommonCopyCollDataStats(&fp->coll_data, pos, coll_data);
    mpCommonRunDefaultCollision(&fp->coll_data, fighter_gobj, MAP_PROC_TYPE_DEFAULT);
    mpCommonResetCollDataStats(&fp->coll_data);
}

/* mp/mpcommon.c:968-976 mpCommonRunItemCollisionDefault 0x800DF058,
 * verbatim -- the item twin of the fighter routine above.
 * First caller is itManagerMakeItem's ITEM_FLAG_COLLPROJECT arm, one of
 * four (ground/default/fighter/weapon/item parents); on the port today
 * every item spawns with flags 0, so this is wired but not yet
 * exercised outside the host test, the same position wpManagerMakeWeapon's
 * own WEAPON_FLAG_COLLPROJECT caller of the weapon twin was in before a
 * projectile actually used it. */
void mpCommonRunItemCollisionDefault(GObj *item_gobj, Vec3f *pos, MPCollData *coll_data)
{
    ITStruct *ip = itGetStruct(item_gobj);

    mpCommonCopyCollDataStats(&ip->coll_data, pos, coll_data);
    mpCommonRunDefaultCollision(&ip->coll_data, item_gobj, MAP_PROC_TYPE_DEFAULT);
    mpCommonResetCollDataStats(&ip->coll_data);
}

/* mp/mpcommon.c:978-985 mpCommonRunWeaponCollisionDefault 0x800DF09C,
 * verbatim -- the weapon twin of the fighter routine above, run once at
 * spawn (wpManagerMakeWeapon, WEAPON_FLAG_COLLPROJECT) to seat a
 * projectile on the map from its owner's position. */
void mpCommonRunWeaponCollisionDefault(GObj *weapon_gobj, Vec3f *pos, MPCollData *coll_data)
{
    WPStruct *wp = wpGetStruct(weapon_gobj);

    mpCommonCopyCollDataStats(&wp->coll_data, pos, coll_data);
    mpCommonRunDefaultCollision(&wp->coll_data, weapon_gobj, MAP_PROC_TYPE_DEFAULT);
    mpCommonResetCollDataStats(&wp->coll_data);
}

/* The bzero arm of syDmaLoadOverlay for overlay 2, of which mp/mpcommon
 * is a part (smashbrothers.us.yaml). sYakumonoDObjs points into the scene
 * heap -- the previous scene's, once taskman has emptied it. */
void mpCommonOverlayLoad(void)
{
    OVERLAY_CLEAR(sYakumonoDObjs);
    OVERLAY_CLEAR(sMPCommonProcPass);
}

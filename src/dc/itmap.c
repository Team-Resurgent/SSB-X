/* itmap.c -- it/itmap.c, verbatim, all eighteen functions (seventeen
 * named plus one the decomp itself marks unused): the item's map-
 * collision wrappers around mp/mpprocess.c's wall/floor/ceiling tests
 * (compiled unmodified into this build already), the angle-based
 * rebound math, and the ground/air kinetics setters. Function-for-
 * function against the game's own ITStruct (it/ittypes.h); every
 * function names its decomp line range.
 *
 * Like the rest of the item system, the whole file is portable today.
 * Every mpProcess* wall/floor/ceiling test it calls is mp/mpprocess.c,
 * compiled unmodified into both builds (src/game/ssb64/Makefile:1554,
 * HOST_DECOMP_SRCS at :2487). wp/wpmap.c leans on the same family, and
 * this file's own tests (below) cross-check several calls against wpmap.c's
 * already-proven host behaviour. lbCommonSim2D/Reflect2D/
 * Scale2D/Mag2D and itMainSetSpinVelLR were all already in the build.
 */
#include <it/item.h>
#include <sc/scene.h>

#ifdef FT_HOSTTEST
#include "hoststubs.h"
#else
#include <kos.h>
#endif

/* // // // // // // // // // // // //
 *                                   //
 *           FUNCTIONS               //
 *                                   //
 * // // // // // // // // // // // // */

/* it/itmap.c:11-48 itMapProcLRWallCheckFloor 0x80173480, verbatim. */
sb32 itMapProcLRWallCheckFloor(MPCollData *coll_data, GObj *item_gobj, u32 flags)
{
    s32 floor_line_id = coll_data->floor_line_id;
    sb32 is_collide_floor = FALSE;

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
            is_collide_floor = TRUE;
        }
    }
    else coll_data->is_coll_end = TRUE;

    if (mpProcessCheckTestFloorCollision(coll_data, floor_line_id) != FALSE)
    {
        mpProcessSetLandingFloor(coll_data);

        if (coll_data->mask_stat & MAP_FLAG_FLOOR)
        {
            mpProcessRunFloorEdgeAdjust(coll_data);
            is_collide_floor = TRUE;
        }
        coll_data->is_coll_end = FALSE;
    }
    return is_collide_floor;
}

/* it/itmap.c:51-54 itMapTestLRWallCheckFloor 0x8017356C, verbatim. */
sb32 itMapTestLRWallCheckFloor(GObj *item_gobj)
{
    return mpProcessUpdateMain(&itGetStruct(item_gobj)->coll_data, itMapProcLRWallCheckFloor, item_gobj, 0);
}

/* it/itmap.c:57-66 itMapCheckLRWallProcNoFloor 0x801735A0, verbatim. */
sb32 itMapCheckLRWallProcNoFloor(GObj *item_gobj, void (*proc_map)(GObj*))
{
    if (itMapTestLRWallCheckFloor(item_gobj) == FALSE)
    {
        proc_map(item_gobj);

        return FALSE;
    }
    else return TRUE;
}

/* it/itmap.c:69-96 itMapProcAllCheckCollEnd 0x801735E0, verbatim. */
sb32 itMapProcAllCheckCollEnd(MPCollData *coll_data, GObj *item_gobj, u32 flags)
{
    ITStruct *ip = itGetStruct(item_gobj);
    DObj *joint = DObjGetStruct(item_gobj);

    if (mpProcessCheckTestLWallCollisionAdjNew(coll_data) != FALSE)
    {
        coll_data->is_coll_end = TRUE;
    }
    if (mpProcessCheckTestRWallCollisionAdjNew(coll_data) != FALSE)
    {
        coll_data->is_coll_end = TRUE;
    }
    if (mpProcessCheckTestCeilCollisionAdjNew(coll_data) != FALSE)
    {
        coll_data->is_coll_end = TRUE;
    }
    if (mpProcessRunFloorCollisionAdjNewNULL(coll_data) != FALSE)
    {
        coll_data->is_coll_end = TRUE;

        func_800269C0_275C0(nSYAudioFGMItemMapCollide);

        ip->spin_step = 0.0F;
        joint->rotate.vec.f.z = 0.0F;
    }
    return coll_data->is_coll_end;
}

/* it/itmap.c:99-102 itMapTestAllCheckCollEnd 0x80173680, verbatim. */
sb32 itMapTestAllCheckCollEnd(GObj *item_gobj)
{
    return mpProcessUpdateMain(&itGetStruct(item_gobj)->coll_data, itMapProcAllCheckCollEnd, item_gobj, 0);
}

/* it/itmap.c:105-147 itMapProcAllCheckCollisionFlag 0x801736B4, verbatim. */
sb32 itMapProcAllCheckCollisionFlag(MPCollData *coll_data, GObj *item_gobj, u32 coll_flags)
{
    ITStruct *ip = itGetStruct(item_gobj);
    DObj *joint = DObjGetStruct(item_gobj);

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
        mpProcessSetLandingFloor(coll_data);

        if (coll_data->mask_stat & MAP_FLAG_FLOOR)
        {
            mpProcessRunFloorEdgeAdjust(coll_data);
            func_800269C0_275C0(nSYAudioFGMItemMapCollide);

            ip->spin_step = 0.0F;
            joint->rotate.vec.f.z = 0.0F;

            coll_data->is_coll_end = TRUE;
        }
    }
    if (coll_data->mask_curr & coll_flags)
    {
        return TRUE;
    }
    else return FALSE;
}

/* it/itmap.c:150-153 itMapTestAllCollisionFlag 0x801737B8, verbatim. */
sb32 itMapTestAllCollisionFlag(GObj *item_gobj, u32 flag)
{
    return mpProcessUpdateMain(&itGetStruct(item_gobj)->coll_data, itMapProcAllCheckCollisionFlag, item_gobj, flag);
}

/* it/itmap.c:156-231 itMapCheckCollideAllRebound 0x801737EC -- modify
 * velocity based on angle of collision, verbatim. */
sb32 itMapCheckCollideAllRebound(GObj *item_gobj, u32 check_flags, f32 mod_vel, Vec3f *pos)
{
    ITStruct *ip = itGetStruct(item_gobj);
    MPCollData *coll_data = &ip->coll_data;
    Vec3f *translate = &DObjGetStruct(item_gobj)->translate.vec.f;
    Vec3f mod_pos;
    sb32 return_bool = FALSE;
    u16 coll_flags = (ip->coll_data.mask_prev ^ ip->coll_data.mask_curr) & ip->coll_data.mask_curr & MAP_FLAG_MAIN_MASK;

    if (coll_flags & check_flags & MAP_FLAG_LWALL)
    {
        if (lbCommonSim2D(&ip->physics.vel_air, &coll_data->lwall_angle) < 0.0F)
        {
            lbCommonReflect2D(&ip->physics.vel_air, &coll_data->lwall_angle);

            mod_pos.x = translate->x + coll_data->map_coll.width;
            mod_pos.y = translate->y + coll_data->map_coll.center;

            return_bool = TRUE;

            func_800269C0_275C0(nSYAudioFGMItemMapCollide);
        }
    }
    if (coll_flags & check_flags & MAP_FLAG_RWALL)
    {
        if (lbCommonSim2D(&ip->physics.vel_air, &coll_data->rwall_angle) < 0.0F)
        {
            lbCommonReflect2D(&ip->physics.vel_air, &coll_data->rwall_angle);

            mod_pos.x = translate->x - coll_data->map_coll.width;
            mod_pos.y = translate->y + coll_data->map_coll.center;

            return_bool = TRUE;

            func_800269C0_275C0(nSYAudioFGMItemMapCollide);
        }
    }
    if (coll_flags & check_flags & MAP_FLAG_CEIL)
    {
        if (lbCommonSim2D(&ip->physics.vel_air, &coll_data->ceil_angle) < 0.0F)
        {
            lbCommonReflect2D(&ip->physics.vel_air, &coll_data->ceil_angle);

            mod_pos.x = translate->x;
            mod_pos.y = translate->y + coll_data->map_coll.top;

            return_bool = TRUE;
        }
    }
    if (coll_flags & check_flags & MAP_FLAG_FLOOR)
    {
        if (lbCommonSim2D(&ip->physics.vel_air, &coll_data->floor_angle) < 0.0F)
        {
            lbCommonReflect2D(&ip->physics.vel_air, &coll_data->floor_angle);

            mod_pos.x = translate->x;
            mod_pos.y = translate->y + coll_data->map_coll.bottom;

            return_bool = TRUE;

            func_800269C0_275C0(nSYAudioFGMItemMapCollide);
        }
    }
    if (return_bool != FALSE)
    {
        lbCommonScale2D(&ip->physics.vel_air, mod_vel);

        if (pos != NULL)
        {
            pos->x = mod_pos.x;
            pos->y = mod_pos.y;
            pos->z = translate->z;
        }
    }
    return return_bool;
}

/* it/itmap.c:234-256 itMapSetGroundRebound 0x80173A48, verbatim. */
void itMapSetGroundRebound(Vec3f *vel, Vec3f *floor_angle, f32 ground_rebound)
{
    f32 scale;
    f32 inverse;
    f32 rebound;

    scale = lbCommonMag2D(vel);

    if (scale != 0.0F)
    {
        inverse = 1.0F / scale;
        rebound = scale * ground_rebound * 0.5F;

        lbCommonReflect2D(vel, floor_angle);

        vel->x *= inverse;
        vel->y *= inverse;
        vel->x += floor_angle->x;
        vel->y += floor_angle->y;
        vel->x *= rebound;
        vel->y *= rebound;
    }
}

/* it/itmap.c:259-298 itMapCheckDestroyDropped 0x80173B24, verbatim. */
sb32 itMapCheckDestroyDropped(GObj *item_gobj, f32 common_rebound, f32 ground_rebound, void (*proc_status)(GObj*))
{
    ITStruct *ip = itGetStruct(item_gobj);
    s32 unused;
    sb32 is_collide_floor = itMapTestAllCollisionFlag(item_gobj, MAP_FLAG_FLOOR);

    if (itMapCheckCollideAllRebound(item_gobj, (MAP_FLAG_CEIL | MAP_FLAG_RWALL | MAP_FLAG_LWALL), common_rebound, NULL) != FALSE)
    {
        itMainSetSpinVelLR(item_gobj);
    }
    if (is_collide_floor != FALSE)
    {
        itMapSetGroundRebound(&ip->physics.vel_air, &ip->coll_data.floor_angle, ground_rebound);
        itMainSetSpinVelLR(item_gobj);

        ip->times_landed++;

        if (ip->times_landed == ITEM_LANDING_DESPAWN_CHECK)
        {
            if
            (
                (gSCManagerBattleState->game_type != nSCBattleGameTypeExplain)   &&
                (ip->times_thrown != 0)                                 &&
                (
                    (ip->times_thrown == ITEM_THROW_NUM_MAX)
                    ||
                    (syUtilsRandIntRange(ITEM_THROW_DESPAWN_RANDOM) == 0)
                )
            )
            {
                return TRUE;
            }
        }
        if ((ip->times_landed == ITEM_LANDING_NUM_MAX) && (proc_status != NULL))
        {
            proc_status(item_gobj);
        }
    }
    return FALSE;
}

/* it/itmap.c:301-324 itMapCheckLanding 0x80173C68, verbatim. */
sb32 itMapCheckLanding(GObj *item_gobj, f32 common_rebound, f32 ground_rebound, void (*proc_map)(GObj*))
{
    ITStruct *ip = itGetStruct(item_gobj);
    s32 unused;
    sb32 is_collide_floor = itMapTestAllCollisionFlag(item_gobj, MAP_FLAG_FLOOR);

    if (itMapCheckCollideAllRebound(item_gobj, (MAP_FLAG_CEIL | MAP_FLAG_RWALL | MAP_FLAG_LWALL), common_rebound, NULL) != FALSE)
    {
        itMainSetSpinVelLR(item_gobj);
    }
    if (is_collide_floor != FALSE)
    {
        lbCommonReflect2D(&ip->physics.vel_air, &ip->coll_data.floor_angle);
        lbCommonScale2D(&ip->physics.vel_air, ground_rebound);
        itMainSetSpinVelLR(item_gobj);

        if (proc_map != NULL)
        {
            proc_map(item_gobj);
        }
        return TRUE;
    }
    else return FALSE;
}

/* it/itmap.c:327-352 itMapCheckMapReboundProcAll 0x80173D24, verbatim. */
sb32 itMapCheckMapReboundProcAll(GObj *item_gobj, f32 common_rebound, f32 ground_rebound, void (*proc_map)(GObj*))
{
    ITStruct *ip = itGetStruct(item_gobj);
    MPCollData *coll_data = &ip->coll_data;
    sb32 is_collide_any = itMapTestAllCollisionFlag(item_gobj, MAP_FLAG_MAIN_MASK);

    if (itMapCheckCollideAllRebound(item_gobj, (MAP_FLAG_CEIL | MAP_FLAG_RWALL | MAP_FLAG_LWALL), common_rebound, NULL) != FALSE)
    {
        itMainSetSpinVelLR(item_gobj);
    }
    if (coll_data->mask_curr & MAP_FLAG_FLOOR)
    {
        lbCommonReflect2D(&ip->physics.vel_air, &coll_data->floor_angle);
        lbCommonScale2D(&ip->physics.vel_air, ground_rebound);
        itMainSetSpinVelLR(item_gobj);
    }
    if (is_collide_any != FALSE)
    {
        if (proc_map != NULL)
        {
            proc_map(item_gobj);
        }
        return TRUE;
    }
    else return FALSE;
}

/* it/itmap.c:355-368 itMapCheckDestroyLanding 0x80173DF4, verbatim. */
sb32 itMapCheckDestroyLanding(GObj *item_gobj, f32 common_rebound)
{
    sb32 is_collide_floor = itMapTestAllCollisionFlag(item_gobj, MAP_FLAG_FLOOR);

    if (itMapCheckCollideAllRebound(item_gobj, (MAP_FLAG_CEIL | MAP_FLAG_RWALL | MAP_FLAG_LWALL), common_rebound, NULL) != FALSE)
    {
        itMainSetSpinVelLR(item_gobj);
    }
    if (is_collide_floor != FALSE)
    {
        return TRUE;
    }
    else return FALSE;
}

/* it/itmap.c:371-378 itMapCheckMapProcAll 0x80173E58, verbatim. */
sb32 itMapCheckMapProcAll(GObj *item_gobj, void (*proc_map)(GObj*))
{
    if ((itMapTestAllCollisionFlag(item_gobj, MAP_FLAG_MAIN_MASK) != FALSE) && (proc_map != NULL))
    {
        proc_map(item_gobj);
    }
    return FALSE;
}

/* it/itmap.c:381-392 func_ovl3_80173E9C 0x80173E9C, verbatim -- the
 * decomp's own comment marks this unused; kept anyway, the same as
 * every other decomp function this port carries across byte for byte. */
sb32 func_ovl3_80173E9C(GObj *item_gobj, void (*proc)(GObj*))
{
    if ((itMapTestAllCollisionFlag(item_gobj, MAP_FLAG_MAIN_MASK) != FALSE))
    {
        if (proc != NULL)
        {
            proc(item_gobj);
        }
        return TRUE;
    }
    else return FALSE;
}

/* it/itmap.c:395-406 itMapCheckMapReboundProcNoFloor 0x80173EE8, verbatim. */
sb32 itMapCheckMapReboundProcNoFloor(GObj *item_gobj, f32 common_rebound, void (*proc_map)(GObj*))
{
    if ((itMapTestAllCollisionFlag(item_gobj, MAP_FLAG_FLOOR) != FALSE) && (proc_map != NULL))
    {
        proc_map(item_gobj);
    }
    if (itMapCheckCollideAllRebound(item_gobj, (MAP_FLAG_CEIL | MAP_FLAG_RWALL | MAP_FLAG_LWALL), common_rebound, NULL) != FALSE)
    {
        itMainSetSpinVelLR(item_gobj);
    }
    return FALSE;
}

/* it/itmap.c:409-413 itMapSetGround 0x80173F54, verbatim. */
void itMapSetGround(ITStruct *ip)
{
    ip->ga = nMPKineticsGround;
    ip->physics.vel_ground = ip->physics.vel_air.x * ip->lr;
}

/* it/itmap.c:416-419 itMapSetAir 0x80173F78, verbatim. */
void itMapSetAir(ITStruct *ip)
{
    ip->ga = nMPKineticsAir;
}

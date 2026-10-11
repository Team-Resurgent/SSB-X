/* gmcamera.c -- gm/gmcamera.c, the battle camera. Function-for-function;
 * every function names its decomp line range, and its body is that
 * function's text. What the port does not carry, and why:
 *  - the 1P game's three cameras are all here: PlayerFollow (bonus
 *    stages), MapZoom (Master Hand's status table) and Anim (the boss
 *    rung's two camera scripts, romdisk/sc1pgameboss.cam,
 *    src/dc/camanim.h).
 *  - the matrix functions (962-1038, 1292-1361): the display list side. The PVR back end
 *    (src/dc/objdisplay.c) reads CObj.vec and CObj.projection.persp
 *    itself, and the game's 0x4C matrix kind -- perspective and look-at
 *    folded with a 32000 rescale for the RSP's fixed point -- has
 *    nothing to do on a float rasteriser, so gmCameraMakeBattleCamera
 *    asks for the plain PerspFastF + LookAt pair.
 *  - the wallpaper camera (1209-1239): the port draws each stage's
 *    wallpaper from the battle camera instead (src/dc/stage.c
 *    grWallpaperMakeDecideKind says why). The interface camera, the
 *    off-screen arrows' camera, the magnify camera (with the two matrix
 *    functions of its own), and the screen-flash and effect cameras
 *    (1241-1290, 1484-1510) are all carried.
 * Every angle goes through lb/lbcommon.c's table trig (src/dc/lbcommon.c),
 * as the game's does.
 */
#include "gmcamera.h"
#include "perf.h"
#include "overlay.h"

#include "ftcommon.h"
#include "lbcommon.h"
#include "mtx.h"
#include "objpvr.h"

#include <ft/ftparam.h>          /* func_ovl2_800EB924 */
#include <mp/map.h>
#include <if/ifcommon.h>
#include <if/interface.h>
#include <sc/scene.h>
#include <wp/weapon.h>           /* wpGetStruct, is_camera_follow */
#include <sys/vector.h>
#include <sys/objanim.h>        /* gcPlayCamAnim, gcAddCObjCamAnimJoint */
#include <sys/debug.h>
#include <sys/rdp.h>

#include <math.h>
#include <string.h>

extern void grZebesAcidGetLevelInfo(f32 *current, f32 *step);   /* src/dc/grzebes.c */

/* gmcamera.c:15 */
static GCPersp dGMCameraPerspDefault = { NULL, 0, 38.0F, 15.0F / 11.0F, 256.0F, 39936.0F, 1.0F };

/* gmcamera.c:18-24 */
static CObjVec dGMCameraCObjVecDefault =
{
    NULL,
    { 1500.0F, 0.0F, 0.0F },
    {    0.0F, 0.0F, 0.0F },
    {    0.0F, 1.0F, 0.0F }
};

/* gmcamera.c:28-37 dGMCameraFuncList, every row the game's */
static void (*dGMCameraFuncList[/* */])(GObj*) =
{
    gmCameraDefaultFuncCamera,
    gmCameraPlayerZoomFuncCamera,
    gmCameraAnimFuncCamera,
    gmCameraInishieFuncCamera,
    gmCameraMapZoomFuncCamera,
    gmCameraPlayerFollowFuncCamera,
    gmCameraZebesFuncCamera
};

/* gmcamera.c:40-47 */
static f32 dGMCameraPlayerZoomRanges[/* */] =
{
    0.00F,  // No players
    1.50F,  // 1 Player
    1.32F,  // 2 Players
    1.16F,  // 3 Players
    1.00F   // 4 Players
};

/* gmcamera.c:65 gGMCameraMatrix: the battle camera's look-at times its
 * perspective, in the RSP's row-vector layout, which is what
 * ft/ftparam.c's func_ovl2_800EB924 projects a world point through
 * for the player tags and the magnifiers. The game builds it in
 * gmCameraDefaultProcDisplay (:990-1017) from the same CObj the pass
 * is about to draw; the port builds it at the end of the camera's own
 * update (gmCameraRunFuncCamera), which reads the CObj in the same
 * state, because the port's display side keeps its matrices in its
 * own layout (objpvr.h) and nothing else reads this one. */
Mtx44f gGMCameraMatrix;

/* gmcamera.c:56-68 */
GObj *gGMCameraGObj;
f32 gGMCameraPauseCameraEyeY;
f32 gGMCameraPauseCameraEyeX;
GMCamera gGMCameraStruct;

/* gmcamera.c:77-98 0x8010B810, verbatim */
u32 gmCameraGetBoundsMask(Vec3f *pos)
{
    u32 bounds = 0;

    if (pos->x < gMPCollisionGroundData->camera_bound_left)
    {
        bounds |= CAMERA_FLAG_BOUND_LEFT;
    }
    if (pos->x > gMPCollisionGroundData->camera_bound_right)
    {
        bounds |= CAMERA_FLAG_BOUND_RIGHT;
    }
    if (pos->y < gMPCollisionGroundData->camera_bound_bottom)
    {
        bounds |= CAMERA_FLAG_BOUND_BOTTOM;
    }
    if (pos->y > gMPCollisionGroundData->camera_bound_top)
    {
        bounds |= CAMERA_FLAG_BOUND_TOP;
    }
    return bounds;
}

/* gmcamera.c:101-128 0x8010B8BC, verbatim */
void gmCameraSetBoundsPosition(Vec3f *pos)
{
    while (TRUE)
    {
        u32 bounds = gmCameraGetBoundsMask(pos);

        if (bounds != 0)
        {
            if (bounds & CAMERA_FLAG_BOUND_LEFT)
            {
                pos->x = gMPCollisionGroundData->camera_bound_left;
            }
            else if (bounds & CAMERA_FLAG_BOUND_RIGHT)
            {
                pos->x = gMPCollisionGroundData->camera_bound_right;
            }
            else if (bounds & CAMERA_FLAG_BOUND_BOTTOM)
            {
                pos->y = gMPCollisionGroundData->camera_bound_bottom;
            }
            else if (bounds & CAMERA_FLAG_BOUND_TOP)
            {
                pos->y = gMPCollisionGroundData->camera_bound_top;
            }
        }
        else break;
    }
}

/* gmcamera.c:131-152 0x8010B98C, verbatim: the 1P game's team bounds */
u32 gmCameraGetTeamBoundsMask(Vec3f *pos)
{
    u32 bounds = 0;

    if (pos->x < gMPCollisionGroundData->camera_bound_team_left)
    {
        bounds |= CAMERA_FLAG_BOUND_LEFT;
    }
    if (pos->x > gMPCollisionGroundData->camera_bound_team_right)
    {
        bounds |= CAMERA_FLAG_BOUND_RIGHT;
    }
    if (pos->y < gMPCollisionGroundData->camera_bound_team_bottom)
    {
        bounds |= CAMERA_FLAG_BOUND_BOTTOM;
    }
    if (pos->y > gMPCollisionGroundData->camera_bound_team_top)
    {
        bounds |= CAMERA_FLAG_BOUND_TOP;
    }
    return bounds;
}

/* gmcamera.c:155-182 0x8010BA38, verbatim */
void gmCameraSetTeamBoundsPosition(Vec3f *pos)
{
    while (TRUE)
    {
        u32 bounds = gmCameraGetTeamBoundsMask(pos);

        if (bounds != 0)
        {
            if (bounds & CAMERA_FLAG_BOUND_LEFT)
            {
                pos->x = gMPCollisionGroundData->camera_bound_team_left;
            }
            else if (bounds & CAMERA_FLAG_BOUND_RIGHT)
            {
                pos->x = gMPCollisionGroundData->camera_bound_team_right;
            }
            else if (bounds & CAMERA_FLAG_BOUND_BOTTOM)
            {
                pos->y = gMPCollisionGroundData->camera_bound_team_bottom;
            }
            else if (bounds & CAMERA_FLAG_BOUND_TOP)
            {
                pos->y = gMPCollisionGroundData->camera_bound_team_top;
            }
        }
        else break;
    }
}

/* gmcamera.c:185-189 0x8010BB08, verbatim: a fighter KO'd off the top
 * is followed at the top of the camera's bounds, scaled across */
void gmCameraSetDeadUpStarPosition(Vec3f *pos)
{
    pos->x = (gMPCollisionGroundData->camera_bound_top * pos->x) / gMPCollisionGroundData->map_bound_top;
    pos->y = gMPCollisionGroundData->camera_bound_top;
}

/* gmcamera.c:192-201 0x8010BB58, verbatim */
f32 gmCameraGetPlayerNumZoomRange(s32 players_num)
{
    f32 zoom = dGMCameraPlayerZoomRanges[players_num];

    if (gSCManagerBattleState->game_type == nSCBattleGameTypeExplain)
    {
        zoom *= 0.75F;
    }
    return zoom;
}

/* gmcamera.c:204-214 0x8010BB98, verbatim */
static f32 gmCameraCalcFighterZoomRange(FTStruct *fp, f32 camera_zoom)
{
    camera_zoom *= fp->camera_zoom_frame;
    camera_zoom *= fp->camera_zoom_range;

    if ((fp->status_id == nFTCommonStatusWait) && (fp->status_total_tics >= 120))
    {
        camera_zoom *= 0.75F;
    }
    return camera_zoom;
}

/* gmcamera.c:217-228 0x8010BBE4, verbatim */
f32 gmCameraGetTargetAtY(f32 dist)
{
    if (dist > 2000.0F)
    {
        return 0.0682F;
    }
    else if (dist < 1000.0F)
    {
        return 0.0F;
    }
    else return ((dist - 1000.0F) / 1000) * 0.0682F;
}

/* gmcamera.c:231-466 0x8010BC54, verbatim: every
 * fighter's point of interest, biased ahead of its facing, and every
 * camera-following weapon's, into a box, and the box into a centre and
 * half-extents. */
void gmCameraUpdateInterests(Vec3f *vec, f32 *hz, f32 *vt)
{
    s32 players_num;
    s32 i;
    FTCamera cams[GMCOMMON_PLAYERS_MAX];
    FTStruct *fp;
    WPStruct *wp;
    f32 pos_top;
    f32 pos_left;
    f32 pos_bottom;
    f32 ft_right;
    f32 ft_top;
    f32 ft_left;
    f32 ft_bottom;
    f32 zoom;
    GObj *fighter_gobj;
    FTStruct *cam_fp;
    f32 wp_top;
    f32 wp_bottom;
    f32 wp_left;
    f32 wp_right;
    f32 adjust;
    f32 gm_top;
    f32 gm_bottom;
    f32 gm_left;
    f32 gm_right;
    f32 pos_right;
    GObj *weapon_gobj;
    s32 lr;
    Vec3f weapon_pos;

    fighter_gobj = gGCCommonLinks[nGCCommonLinkIDFighter];

    players_num = 0;

    while (fighter_gobj != NULL)
    {
        fp = ftGetStruct(fighter_gobj);

        switch (fp->camera_mode)
        {
        default:
            if (players_num >= ARRAY_COUNT(cams))
            {
                while (TRUE)
                {
                    syDebugPrintf("Player Num is Over for Camera!\n");
                    scManagerRunPrintGObjStatus();
                }
            }
            cams[players_num].target_fp = fp;

            switch (fp->camera_mode)
            {
            default:
                cams[players_num].target_pos = DObjGetStruct(fighter_gobj)->translate.vec.f;
                break;

            case nFTCameraModeEntry:
            case nFTCameraModeExplain:
                cams[players_num].target_pos = fp->entry_pos;
                break;

            case nFTCameraModeDeadUp:
                cams[players_num].target_pos = fp->status_vars.common.dead.pos;
                break;
            }
            cams[players_num].target_pos.y += fp->attr->cam_offset_y;

            if ((gSCManagerBattleState->game_type == nSCBattleGameType1PGame) && (gSCManagerBattleState->players[fp->player].is_spgame_enemy != FALSE))
            {
                switch (fp->camera_mode)
                {
                default: 
                    break;

                    // WHAT!?!? There are a few ways to match this, but it appears to be a control flow issue more than anything.
                    // Other solution #1: cast &cams[players_num].target_pos to an integer
                    // Other solution #2: explicitly define any of the Vec3f* functions below as s32 functions (just forget about it)
                }
                gmCameraSetTeamBoundsPosition(&cams[players_num].target_pos);
            }
            else switch (fp->camera_mode)
            {
            case nFTCameraModeDeadUp:
                gmCameraSetDeadUpStarPosition(&cams[players_num].target_pos);
                break;

            default:
                gmCameraSetBoundsPosition(&cams[players_num].target_pos);
                break;
            }
            players_num++;
            break;

        case nFTCameraModeGhost:
            break;
        }
        fighter_gobj = fighter_gobj->link_next;
    }
    if (players_num != 0)
    {
        ft_top = 65536.0F;
        ft_bottom = -65536.0F;
        ft_left = -65536.0F;
        ft_right = 65536.0F;

        gm_bottom = 65536.0F;
        gm_top = -65536.0F;
        gm_left = 65536.0F;
        gm_right = -65536.0F;

        zoom = gmCameraGetPlayerNumZoomRange(players_num);

        for (i = 0; i < players_num; i++)
        {
            cam_fp = cams[i].target_fp;

            adjust = gmCameraCalcFighterZoomRange(cam_fp, zoom);

            lr = ((cam_fp->camera_mode == nFTCameraModeEntry) || (cam_fp->camera_mode == nFTCameraModeExplain)) ?
            cam_fp->status_vars.common.entry.lr : cam_fp->lr;

            if (lr == -1)
            {
                pos_left = cams[i].target_pos.x - (1000.0F * adjust);
                pos_right = cams[i].target_pos.x + (700.0F * adjust);
            }
            else
            {
                pos_left = cams[i].target_pos.x - (700.0F * adjust);
                pos_right = cams[i].target_pos.x + (1000.0F * adjust);
            }
            if (gm_left > pos_left)
            {
                gm_left = pos_left;
            }
            if (gm_right < pos_right)
            {
                gm_right = pos_right;
            }
            pos_top = cams[i].target_pos.y + (700.0F * adjust);
            pos_bottom = cams[i].target_pos.y + (-700.0F * adjust);

            if (gm_bottom > pos_bottom)
            {
                gm_bottom = pos_bottom;
            }
            if (gm_top < pos_top)
            {
                gm_top = pos_top;
            }
            if (cams[i].target_pos.x < ft_right) // ft_right = ft_right?
            {
                ft_right = cams[i].target_pos.x;
            }
            if (cams[i].target_pos.x > ft_left) // ft_left = ft_left?
            {
                ft_left = cams[i].target_pos.x;
            }
            if (cams[i].target_pos.y < ft_top) // ft_top = ft_top?
            {
                ft_top = cams[i].target_pos.y;
            }
            if (cams[i].target_pos.y > ft_bottom) // ft_bottom = ft_bottom?
            {
                ft_bottom = cams[i].target_pos.y;
            }
            cams[i].unk_ftcobj_0x10 = adjust;
        }
        weapon_gobj = gGCCommonLinks[nGCCommonLinkIDWeapon];

        while (weapon_gobj != NULL)
        {
            wp = wpGetStruct(weapon_gobj);

            if (wp->is_camera_follow)
            {
                weapon_pos = DObjGetStruct(weapon_gobj)->translate.vec.f;

                wp_left = ft_right - 1000.0F; // wp_left = left?
                wp_right = ft_left + 1000.0F; // wp_right = right?
                wp_bottom = ft_top - 1000.0F; // wp_bottom = bottom?
                wp_top = ft_bottom + 1000.0F; // wp_top = top?

                gmCameraSetBoundsPosition(&weapon_pos);

                if (weapon_pos.x < wp_left)
                {
                    weapon_pos.x = wp_left;
                }
                if (weapon_pos.x > wp_right)
                {
                    weapon_pos.x = wp_right;
                }
                if (weapon_pos.y < wp_bottom)
                {
                    weapon_pos.y = wp_bottom;
                }
                if (weapon_pos.y > wp_top)
                {
                    weapon_pos.y = wp_top;
                }
                if ((weapon_pos.x - 1000.0F) < gm_left) // gm_left = gm_left?
                {
                    gm_left = (weapon_pos.x - 1000.0F);
                }
                if ((weapon_pos.x + 1000.0F) > gm_right) // gm_right = gm_right?
                {
                    gm_right = (weapon_pos.x + 1000.0F);
                }
                if ((weapon_pos.y - 1000.0F) < gm_bottom) // gm_bottom = gm_bottom?
                {
                    gm_bottom = (weapon_pos.y - 1000.0F);
                }
                if ((weapon_pos.y + 1000.0F) > gm_top) // gm_top = gm_top?
                {
                    gm_top = (weapon_pos.y + 1000.0F);
                }
            }
            weapon_gobj = weapon_gobj->link_next;
        }
        *hz = (gm_right - gm_left) * 0.5F;
        *vt = (gm_top - gm_bottom) * 0.5F;

        vec->x = ((gm_left + gm_right) * 0.5F);
        vec->y = (0.5F - gmCameraGetTargetAtY((*vt < *hz) ? *hz : *vt)) * (gm_bottom + gm_top);
        vec->z = 0.0F;
    }
    else
    {
        vec->x = vec->y = vec->z = 0.0F;

        *hz = *vt = 2000.0F;
    }
}

/* gmcamera.c:469-488 0x8010C200, verbatim */
void gmCameraGetClampDimensionsMax(f32 hz, f32 vt, f32 *max)
{
    f32 maxd;

    vt /= lbCommonTan(F_CLC_DTOR32(gGMCameraStruct.fovy * 0.5F));

    hz /= ((lbCommonTan(F_CLC_DTOR32(gGMCameraStruct.fovy * 0.5F)) *
    gGMCameraStruct.viewport_width) / gGMCameraStruct.viewport_height);

    maxd = (hz > vt) ? hz : vt;

    if (maxd < 2500.0F)
    {
        maxd = 2500.0F;
    }
    if (maxd > 30000.0F)
    {
        maxd = 30000.0F;
    }
    *max = maxd;
}

/* gmcamera.c:491-504 0x8010C320, verbatim */
void gmCameraGetAdjustAtAngle(Vec3f *at, Vec3f *vec, f32 x, f32 y)
{
    f32 angle_x, angle_y;

    angle_x = gGMCameraPauseCameraEyeY + y + gMPCollisionGroundData->light_angle.z;

    vec->y = -lbCommonSin(angle_x);
    vec->z = lbCommonCos(angle_x);

    angle_y = gGMCameraPauseCameraEyeX + x;

    vec->x = (lbCommonSin(angle_y) * vec->z);
    vec->z *= lbCommonCos(angle_y);
}

/* gmcamera.c:507-532 0x8010C3C0, verbatim: the tilt that follows the
 * target around the stage */
void func_ovl2_8010C3C0(Vec3f *at, Vec3f *vec)
{
    f32 x, y;

    y = -F_CLC_DTOR32((at->y + (-900.0F)) / 133.0F);

    if (y > F_CLC_DTOR32(5.0F)) // 0.08726647F
    {
        y = F_CLC_DTOR32(5.0F);
    }
    if (y < F_CLC_DTOR32(-7.0F)) // -0.122173056F
    {
        y = F_CLC_DTOR32(-7.0F);
    }
    x = -F_CLC_DTOR32(at->x / 133.0F);

    if (x > F_CLC_DTOR32(17.5F)) // 0.30543265F
    {
        x = F_CLC_DTOR32(17.5F);
    }
    if (x < F_CLC_DTOR32(-17.5F)) // -0.30543265F
    {
        x = F_CLC_DTOR32(-17.5F);
    }
    gmCameraGetAdjustAtAngle(at, vec, x, y);
}

/* gmcamera.c:535-538 0x8010C4A4, verbatim: Mushroom Kingdom's straight-on
 * view */
void gmCameraUpdateInishieFocus(Vec3f *arg0, Vec3f *arg1)
{
    gmCameraGetAdjustAtAngle(arg0, arg1, 0.0F, 0.0F);
}

/* gmcamera.c:541-552 0x8010C4D0, verbatim: the pan rate for the distance */
f32 func_ovl2_8010C4D0(void)
{
    if (gGMCameraStruct.target_dist > 15000.0F)
    {
        return 0.1F;
    }
    else if (gGMCameraStruct.target_dist < 2000.0F)
    {
        return 0.05F;
    }
    return ((1.0F - ((gGMCameraStruct.target_dist - 2000.0F) / 13000.0F)) * 0.05F) + .05F; // Needs to be two different 0.05s lol
}

/* gmcamera.c:555-566 0x8010C55C, verbatim */
void gmCameraPan(CObj *cobj, Vec3f *pos, f32 scale)
{
    f32 mag;
    Vec3f add;

    syVectorDiff3D(&add, pos, &cobj->vec.at);

    mag = syVectorMag3D(&add) * scale;

    syVectorNorm3D(&add);
    syVectorScale3D(&add, mag);
    syVectorAdd3D(&cobj->vec.at, &add);
}

/* gmcamera.c:569-585 0x8010C5C0, verbatim: the eye eases toward its
 * place behind the look-at */
void func_ovl2_8010C5C0(CObj *cobj, Vec3f *scale)
{
    Vec3f sp34;
    Vec3f pan;
    f32 mag;

    pan.x = cobj->vec.at.x + (gGMCameraStruct.target_dist * scale->x);
    pan.y = cobj->vec.at.y + (gGMCameraStruct.target_dist * scale->y);
    pan.z = cobj->vec.at.z + (gGMCameraStruct.target_dist * scale->z);

    syVectorDiff3D(&sp34, &pan, &cobj->vec.eye);

    mag = syVectorMag3D(&sp34) * 0.1F;

    syVectorNorm3D(&sp34);
    syVectorScale3D(&sp34, mag);
    syVectorAdd3D(&cobj->vec.eye, &sp34);
}

/* gmcamera.c:588-602 0x8010C670, verbatim: the distance eases too */
void func_ovl2_8010C670(f32 dist)
{
    f32 temp_f0;
    f32 temp_f14;

    temp_f0 = gGMCameraStruct.target_dist - dist;
    temp_f14 = temp_f0 * 0.075F;

    if (temp_f0 <= temp_f14)
    {
        gGMCameraStruct.target_dist = dist;
    }
    else gGMCameraStruct.target_dist -= temp_f14;
}

/* gmcamera.c:605-609 0x8010C6B8, verbatim */
void gmCameraApplyVel(CObj *cobj)
{
    syVectorAdd3D(&cobj->vec.at, &gGMCameraStruct.vel_at);

    gGMCameraStruct.vel_at.x = gGMCameraStruct.vel_at.y = gGMCameraStruct.vel_at.z = 0.0F;
}

/* gmcamera.c:612-615 0x8010C6FC, verbatim */
void gmCameraApplyFOV(CObj *cobj)
{
    cobj->projection.persp.fovy = gGMCameraStruct.fovy;
}

/* gmcamera.c:618-621 0x8010C70C, verbatim */
void gmCameraAdjustFOV(f32 fovy)
{
    gGMCameraStruct.fovy += ((fovy - gGMCameraStruct.fovy) * 0.1F);
}

/* gmcamera.c:624-644 0x8010C734, verbatim: the battle camera's frame */
void gmCameraDefaultFuncCamera(GObj *camera_gobj)
{
    CObj *cobj;
    f32 sp48;
    Vec3f sp3C;
    Vec3f sp30;
    f32 sp2C;
    f32 sp28;

    cobj = CObjGetStruct(camera_gobj);

    gmCameraUpdateInterests(&sp30, &sp2C, &sp28);
    gmCameraAdjustFOV(38.0F);
    gmCameraGetClampDimensionsMax(sp2C, sp28, &sp48);
    func_ovl2_8010C670(sp48);
    gmCameraPan(cobj, &sp30, func_ovl2_8010C4D0());
    func_ovl2_8010C3C0(&cobj->vec.at, &sp3C);
    func_ovl2_8010C5C0(cobj, &sp3C);
    gmCameraApplyVel(cobj);
    gmCameraApplyFOV(cobj);
}

/* gmcamera.c:647-673 0x8010C7D0, verbatim: the eye never sinks below
 * 3000 over Planet Zebes' acid */
void gmCameraUpdateAcidZoom(CObj *cobj, Vec3f *scale)
{
    Vec3f add;
    Vec3f pos;
    f32 unused;
    f32 mag;
    f32 current;
    f32 step;

    pos.x = cobj->vec.at.x + (gGMCameraStruct.target_dist * scale->x);
    pos.y = cobj->vec.at.y + (gGMCameraStruct.target_dist * scale->y);
    pos.z = cobj->vec.at.z + (gGMCameraStruct.target_dist * scale->z);

    grZebesAcidGetLevelInfo(&current, &step);

    current += (step + 3000.0F);

    if (pos.y < current)
    {
        pos.y = current;
    }
    syVectorDiff3D(&add, &pos, &cobj->vec.eye);
    mag = syVectorMag3D(&add) * 0.1F;
    syVectorNorm3D(&add);
    syVectorScale3D(&add, mag);
    syVectorAdd3D(&cobj->vec.eye, &add);
}

/* gmcamera.c:676-694 0x8010C8C4, verbatim */
void gmCameraZebesFuncCamera(GObj *camera_gobj)
{
    CObj *cobj = CObjGetStruct(camera_gobj);
    f32 max;
    Vec3f scale;
    Vec3f sp30;
    f32 hz;
    f32 vt;

    gmCameraUpdateInterests(&sp30, &hz, &vt);
    gmCameraAdjustFOV(38.0F);
    gmCameraGetClampDimensionsMax(hz, vt, &max);
    func_ovl2_8010C670(max);
    gmCameraPan(cobj, &sp30, func_ovl2_8010C4D0());
    func_ovl2_8010C3C0(&cobj->vec.at, &scale);
    gmCameraUpdateAcidZoom(cobj, &scale);
    gmCameraApplyVel(cobj);
    gmCameraApplyFOV(cobj);
}

/* gmcamera.c:697-730 0x8010C9A0, verbatim: the close-up on one fighter
 * (the entry focus's third variant, the results screen) */
void gmCameraUpdatePlayerZoom(GObj *camera_gobj)
{
    CObj *cobj;
    FTStruct *fp;
    Vec3f eye;
    Vec3f angle;
    Vec3f pos;

    cobj = CObjGetStruct(camera_gobj);

    pos = DObjGetStruct(gGMCameraStruct.pzoom_fighter_gobj)->translate.vec.f;
    fp = ftGetStruct(gGMCameraStruct.pzoom_fighter_gobj);

    pos.y += fp->attr->cam_offset_y;

    gmCameraAdjustFOV(gGMCameraStruct.pzoom_fov);

    gGMCameraStruct.target_dist = gGMCameraStruct.pzoom_dist;

    gmCameraPan(cobj, &pos, gGMCameraStruct.pzoom_pan_scale);

    eye.y = gGMCameraPauseCameraEyeX + gGMCameraStruct.pzoom_eye_x;
    eye.x = gGMCameraPauseCameraEyeY + gGMCameraStruct.pzoom_eye_y;

    angle.y = -lbCommonSin(eye.x);
    angle.z = lbCommonCos(eye.x);
    angle.x = lbCommonSin(eye.y) * angle.z;
    angle.z *= lbCommonCos(eye.y);

    func_ovl2_8010C5C0(cobj, &angle);
    gmCameraApplyVel(cobj);
    gmCameraApplyFOV(cobj);
}

/* gmcamera.c:733-740 0x8010CA7C, verbatim */
sb32 gmCameraCheckPausePlayerOutBounds(Vec3f *pos)
{
    if ((gmCameraGetBoundsMask(pos) != 0) || (pos->z < -1000.0F) || (pos->z > 1000.0F))
    {
        return TRUE;
    }
    else return FALSE;
}

/* gmcamera.c:743-750 0x8010CAE0, verbatim */
void gmCameraPlayerZoomFuncCamera(GObj *camera_gobj)
{
    if (gmCameraCheckPausePlayerOutBounds(&DObjGetStruct(gGMCameraStruct.pzoom_fighter_gobj)->translate.vec.f) != FALSE)
    {
        dGMCameraFuncList[gGMCameraStruct.status_default](camera_gobj);
    }
    else gmCameraUpdatePlayerZoom(camera_gobj);
}

/* gmcamera.c:832-869 gmCameraPlayerFollowFuncCamera 0x8010CDAC,
 * verbatim: the 1P bonus stages' camera, and the only
 * scene in the game that asks for it.
 *
 * It is gmCameraUpdatePlayerZoom's shape with the pause-bounds test and
 * the zoom-range table taken out -- ONE fighter, followed flat: the
 * fighter's own position lifted by its cam_offset_y, pinned to z=0,
 * clamped to the stage's camera bounds, and the eye set at a fixed
 * distance and a fixed pair of angles the caller passes in. Nothing
 * here reads how many players there are, because a bonus stage has
 * one.
 *
 * The two `unused` locals are the decomp's own; the compiler is told
 * about them at the top of this file the way it is for every other
 * function that carries a pair. */
/* gmcamera.c:776-790 0x8010CBE4, verbatim. A scripted camera, carried
 * along by a constant velocity: the script moves eye and at, and
 * vel_all shifts both each tic -- the boss rung's approach drifts with
 * Master Hand's entry. */
void gmCameraUpdateAnimVel(GObj *camera_gobj)
{
    CObj *cobj;

    gcPlayCamAnim(camera_gobj);

    cobj = CObjGetStruct(camera_gobj);

    cobj->vec.at.x += gGMCameraStruct.vel_all.x;
    cobj->vec.at.y += gGMCameraStruct.vel_all.y;
    cobj->vec.at.z += gGMCameraStruct.vel_all.z;
    cobj->vec.eye.x += gGMCameraStruct.vel_all.x;
    cobj->vec.eye.y += gGMCameraStruct.vel_all.y;
    cobj->vec.eye.z += gGMCameraStruct.vel_all.z;
}

/* gmcamera.c:793-801 0x8010CC74, verbatim: the script runs to its end,
 * then the camera goes back to the stage's default. */
void gmCameraAnimFuncCamera(GObj *camera_gobj)
{
    gmCameraUpdateAnimVel(camera_gobj);

    if (CObjGetStruct(camera_gobj)->anim_wait == AOBJ_ANIM_NULL)
    {
        gmCameraSetStatusDefault();
    }
}

/* gmcamera.c:804-829 0x8010CA80, verbatim. The 1P game's
 * map-zoom camera, kept for the same reason as PlayerFollow: the 1P game asks
 * for it. Three of Master Hand's motions call gmCameraSetStatusMapZoom --
 * ftbossokuhikouki2.c, ftbossokupunch2.c and ftbossokutsubushi.c, the flight,
 * the fist rocket and the vertical slap -- and naming their procs in
 * dFTBossSpecialStatusDescs is what stops the linker dead-stripping them, so
 * this camera must exist.
 *
 * It eases `at` toward zoom_origin_pos by func_ovl2_8010C4D0()'s
 * fraction of the gap and `eye` toward zoom_target_pos by a tenth of
 * its own, at a 38-degree FOV: the close-up that follows a big attack
 * in. Every callee was already here. */
void gmCameraMapZoomFuncCamera(GObj *camera_gobj)
{
    CObj *cobj;
    Vec3f sp30;
    f32 sp2C;
    f32 sp28;

    cobj = CObjGetStruct(camera_gobj);

    gmCameraAdjustFOV(38.0F);

    syVectorDiff3D(&sp30, &gGMCameraStruct.zoom_origin_pos, &cobj->vec.at);
    sp28 = syVectorMag3D(&sp30);
    sp2C = func_ovl2_8010C4D0() * sp28;
    syVectorNorm3D(&sp30);
    syVectorScale3D(&sp30, sp2C);
    syVectorAdd3D(&cobj->vec.at, &sp30);

    syVectorDiff3D(&sp30, &gGMCameraStruct.zoom_target_pos, &cobj->vec.eye);
    sp2C = syVectorMag3D(&sp30) * 0.1F;
    syVectorNorm3D(&sp30);
    syVectorScale3D(&sp30, sp2C);
    syVectorAdd3D(&cobj->vec.eye, &sp30);
    gmCameraApplyVel(cobj);
    gmCameraApplyFOV(cobj);
}

void gmCameraPlayerFollowFuncCamera(GObj *camera_gobj)
{
    f32 eye_x;
    f32 eye_y;
    Vec3f angle;
    Vec3f pos;
    CObj *cobj;
    FTStruct *fp;

    cobj = CObjGetStruct(camera_gobj);
    pos = DObjGetStruct(gGMCameraStruct.pfollow_fighter_gobj)->translate.vec.f;
    fp = ftGetStruct(gGMCameraStruct.pfollow_fighter_gobj);

    pos.y += fp->attr->cam_offset_y;
    pos.z = 0.0F;

    gmCameraSetBoundsPosition(&pos);
    gmCameraAdjustFOV(gGMCameraStruct.pfollow_fov);

    gGMCameraStruct.target_dist = gGMCameraStruct.pfollow_dist;

    gmCameraPan(cobj, &pos, gGMCameraStruct.pfollow_pan_scale);

    eye_x = gGMCameraPauseCameraEyeX + gGMCameraStruct.pfollow_eye_x;
    eye_y = gGMCameraPauseCameraEyeY + gGMCameraStruct.pfollow_eye_y;

    angle.y = -lbCommonSin(eye_y);
    angle.z = lbCommonCos(eye_y);
    angle.x = lbCommonSin(eye_x) * angle.z;
    angle.z *= lbCommonCos(eye_x);

    func_ovl2_8010C5C0(cobj, &angle);
    gmCameraApplyVel(cobj);
    gmCameraApplyFOV(cobj);
}

/* gmcamera.c:753-773 0x8010CB48, verbatim */
void gmCameraInishieFuncCamera(GObj *camera_gobj)
{
    CObj *cobj;
    f32 sp48;
    Vec3f sp3C;
    Vec3f sp30;
    f32 sp2C;
    f32 sp28;

    cobj = CObjGetStruct(camera_gobj);

    gmCameraUpdateInterests(&sp30, &sp2C, &sp28);
    gmCameraAdjustFOV(38.0F);
    gmCameraGetClampDimensionsMax(sp2C, sp28, &sp48);
    func_ovl2_8010C670(sp48);
    gmCameraPan(cobj, &sp30, func_ovl2_8010C4D0());
    gmCameraUpdateInishieFocus(&cobj->vec.at, &sp3C);
    func_ovl2_8010C5C0(cobj, &sp3C);
    gmCameraApplyVel(cobj);
    gmCameraApplyFOV(cobj);
}

/* gmcamera.c:871-874 0x8010CED4, verbatim: the camera GObj's process */
void gmCameraRunFuncCamera(GObj *camera_gobj)
{
    gGMCameraStruct.func_camera(camera_gobj);

    gmCameraUpdateMatrix(camera_gobj);
}

/* The matrix lines of gmcamera.c:990-1017 gmCameraDefaultProcDisplay
 * (see gGMCameraMatrix above). The port's proj * view acts on column
 * vectors, mtx.h's way; the game's guMtxCatF(look_at, persp) acts on
 * row vectors, so the one is the other's transpose. The 32000 rescale
 * the game applies when the matrix grows too large for the RSP's fixed
 * point scales the whole matrix and cancels in the divide, so it is
 * not repeated. */
void gmCameraUpdateMatrix(GObj *camera_gobj)
{
    float m[16];
    s32 i, j;

    gcCameraMatrixF(CObjGetStruct(camera_gobj), m);

    for (i = 0; i < 4; i++)
    {
        for (j = 0; j < 4; j++)
        {
            gGMCameraMatrix[i][j] = m[j * 4 + i];
        }
    }
}

/* gmcamera.c:877-883 0x8010CEF4. DIVERGES, defensively: every camera the decomp's
 * list names is ported, so an id outside it or a NULL row is a bug, and
 * is refused loudly rather than jumped through NULL. */
void gmCameraSetStatus(s32 status_id)
{
    if (status_id < 0 || status_id >= (s32)ARRAY_COUNT(dGMCameraFuncList) ||
        dGMCameraFuncList[status_id] == NULL)
    {
        syDebugPrintf("gmcamera: camera status %d is not ported\n", status_id);
        scManagerRunPrintGObjStatus();
    }
    gGMCameraStruct.status_prev = gGMCameraStruct.status_curr;
    gGMCameraStruct.status_curr = status_id;
    gGMCameraStruct.func_camera = dGMCameraFuncList[status_id];
}

/* gmcamera.c:935-947 0x8010D0A4, verbatim. What
 * ftbossokuhikouki2.c, ftbossokupunch2.c and ftbossokutsubushi.c call
 * to aim the camera above before they play their attack. */
void gmCameraSetStatusMapZoom(Vec3f *origin, Vec3f *target)
{
    Vec3f dist;

    gmCameraSetStatus(nGMCameraStatusMapZoom);

    gGMCameraStruct.zoom_origin_pos = *origin;
    gGMCameraStruct.zoom_target_pos = *target;

    syVectorDiff3D(&dist, origin, target);

    gGMCameraStruct.target_dist = syVectorMag3D(&dist);
}

/* gmcamera.c:924-933 0x8010D030, verbatim. The script is a
 * camanim_get out of a bank (src/dc/camanim.h), not a file offset. */
void gmCameraSetStatusAnim(AObjEvent32 *camanim_joint, f32 anim_frame, Vec3f *vel)
{
    gmCameraSetStatus(nGMCameraStatusAnim);

    gGMCameraStruct.vel_all = *vel;

    gcAddCObjCamAnimJoint(CObjGetStruct(gGMCameraGObj), camanim_joint, anim_frame);
    gmCameraUpdateAnimVel(gGMCameraGObj);
}

/* gmcamera.c:886-889 0x8010CF20, verbatim */
void gmCameraSetStatusDefault(void)
{
    gmCameraSetStatus(gGMCameraStruct.status_default);
}

/* gmcamera.c:892-902 0x8010CF44, verbatim */
void gmCameraSetStatusPlayerZoom(GObj *fighter_gobj, f32 eye_x, f32 eye_y, f32 dist, f32 pan_scale, f32 fov)
{
    gmCameraSetStatus(nGMCameraStatusPlayerZoom);

    gGMCameraStruct.pzoom_fighter_gobj = fighter_gobj;
    gGMCameraStruct.pzoom_eye_x = eye_x;
    gGMCameraStruct.pzoom_eye_y = eye_y;
    gGMCameraStruct.pzoom_dist = dist;
    gGMCameraStruct.pzoom_pan_scale = pan_scale;
    gGMCameraStruct.pzoom_fov = fov;
}

/* gmcamera.c:905-915 0x8010CFA8, verbatim */
void gmCameraSetStatusPlayerFollow(GObj *fighter_gobj, f32 eye_x, f32 eye_y, f32 dist, f32 pan_scale, f32 fov)
{
    gmCameraSetStatus(nGMCameraStatusPlayerFollow);

    gGMCameraStruct.pfollow_fighter_gobj = fighter_gobj;
    gGMCameraStruct.pfollow_eye_x = eye_x;
    gGMCameraStruct.pfollow_eye_y = eye_y;
    gGMCameraStruct.pfollow_dist = dist;
    gGMCameraStruct.pfollow_pan_scale = pan_scale;
    gGMCameraStruct.pfollow_fov = fov;
}

/* gmcamera.c:918-921 0x8010D00C, verbatim */
void gmCameraSetStatusPrev(void)
{
    gmCameraSetStatus(gGMCameraStruct.status_prev);
}

/* gmcamera.c:950-953 0x8010D128, verbatim */
void gmCameraSetVelAt(Vec3f *vel)
{
    gGMCameraStruct.vel_at = *vel;
}

/* gmcamera.c:1040-1104 0x8010DA24 gmCameraDefaultProcDisplay, the battle
 * camera's own display proc. The only thing it adds on the N64 over the
 * object system's default (func_80017DBC) is display-list bookkeeping.
 *
 * It is here for the two lines in the middle of it. The off-screen
 * arrows and the magnifying glasses are raised by the fighters' display
 * procs as the walk goes past them (src/dc/ftdisplaymain.c), and this is
 * where they are lowered again -- once per frame, before the fighters
 * are asked. Nothing else in the game clears them.
 *
 * DIVERGES: the six capture passes are gone. Each is the same DL-link
 * list walked with a render mode set into the two display-list heads
 * beforehand, which is the N64's way of saying "these links are opaque
 * and these are translucent"; the PVR takes that from the batch itself
 * (fighter.h FPACK_LIST_*) and the frame already runs every camera once
 * per list (src/dc/taskman.c), so the port walks camera_mask once and
 * the scene sets the mask (scvsbattle.c:167). syTaskmanUpdateDLBuffers
 * goes with them. */
void gmCameraDefaultProcDisplay(GObj *camera_gobj)
{
    CObj *cobj = CObjGetStruct(camera_gobj);

    DBPERF_LAP_BEGIN();
    gcSetCameraMatrixMode(3);
    gcPrepCameraViewport(cobj, 0);
    gcPrepCameraMatrix(NULL, cobj);
    gcInitDLs();
    DBPERF_LAP(DBP_CAMPREP);
    gcRunFuncCamera(cobj, 0);
    DBPERF_LAP(DBP_CAMFUNC);

    gIFCommonPlayerInterface.magnify_mode = 0;
    gIFCommonPlayerInterface.arrows_flags = 0;

    gcCaptureCameraGObj(camera_gobj, (cobj->flags & COBJ_FLAG_IDENTIFIER) ? 1 : 0);
    DBPERF_LAP(DBP_CAPTURE);

    func_80017CC8(cobj);
}

/* gmcamera.c:1106-1194 0x8010D7E8. DIVERGES: the two matrix kinds are the
 * plain perspective and look-at the PVR back end reads, see the file
 * comment. The display proc was the back end's own (func_80017DBC,
 * src/dc/objdisplay.c) is not used; the game's is, for what
 * gmCameraDefaultProcDisplay clears. The viewport is the
 * game's: the 300x220 inside the ten-pixel frame,
 * which the back end maps clip space onto (objpvr.h) and the tags
 * project through (ft/ftparam.c). */
GObj *gmCameraMakeDefaultCamera(u8 tk1, u8 tk2, void (*proc)(GObj*))
{
    GObj *camera_gobj;
    CObj *cobj;
    Vec3f sp4C;

    camera_gobj = gcMakeCameraGObj
    (
        nGCCommonKindMainCamera,
        NULL,
        nGCCommonLinkIDCamera,
        GOBJ_PRIORITY_DEFAULT,
        gmCameraDefaultProcDisplay,
        50,
        0,
        ~0,
        FALSE,
        nGCProcessKindFunc,
        proc,
        3,
        FALSE
    );
    gGMCameraGObj = camera_gobj;

    cobj = CObjGetStruct(camera_gobj);

    gcAddXObjForCamera(cobj, tk1, 0);

    if (tk2 != nGCMatrixKindNull)
    {
        gcAddXObjForCamera(cobj, tk2, 0);
    }
    cobj->projection.persp = dGMCameraPerspDefault;
    cobj->vec = dGMCameraCObjVecDefault;

    syRdpSetViewport
    (
        &cobj->viewport,
        gGMCameraStruct.viewport_ulx,
        gGMCameraStruct.viewport_uly,
        gGMCameraStruct.viewport_lrx,
        gGMCameraStruct.viewport_lry
    );

    // This (f32) cast is NECESSARY! scissor_ulx through scissor_lry are signed integers!
    cobj->projection.persp.aspect =
    ((f32)(gGMCameraStruct.viewport_lrx - gGMCameraStruct.viewport_ulx) / (f32)(gGMCameraStruct.viewport_lry - gGMCameraStruct.viewport_uly));

    cobj->flags |= COBJ_FLAG_DLBUFFERS;

    gGMCameraStruct.target_dist = 10000.0F;

    gGMCameraPauseCameraEyeY = gGMCameraPauseCameraEyeX = 0.0F;

    sp4C.y = -lbCommonSin(gGMCameraPauseCameraEyeY);
    sp4C.z = lbCommonCos(gGMCameraPauseCameraEyeY);
    sp4C.x = lbCommonSin(gGMCameraPauseCameraEyeX) * sp4C.z;
    sp4C.z *= lbCommonCos(gGMCameraPauseCameraEyeX);

    cobj->vec.at.x = cobj->vec.at.z = 0.0F;
    cobj->vec.at.y = 300.0F;

    cobj->vec.eye.x = (gGMCameraStruct.target_dist * sp4C.x);
    cobj->vec.eye.y = cobj->vec.at.y + (gGMCameraStruct.target_dist * sp4C.y);
    cobj->vec.eye.z = (gGMCameraStruct.target_dist * sp4C.z);

    gGMCameraStruct.vel_at.x = gGMCameraStruct.vel_at.y = gGMCameraStruct.vel_at.z = 0;

    switch (gSCManagerBattleState->gkind)
    {
    case nGRKindZebes:
        gGMCameraStruct.status_curr = nGMCameraStatusZebes;
        break;

    case nGRKindInishie:
        gGMCameraStruct.status_curr = nGMCameraStatusInishie;
        break;

    default:
        gGMCameraStruct.status_curr = nGMCameraStatusDefault;
        // No break? Doesn't match otherwise :brainshock:
    }
    gGMCameraStruct.status_default = gGMCameraStruct.status_prev = gGMCameraStruct.status_curr;
    gGMCameraStruct.func_camera = dGMCameraFuncList[gGMCameraStruct.status_curr];
    gGMCameraStruct.fovy = 38.0F;

    return camera_gobj;
}

/* gmcamera.c:1197-1200 0x8010DB00. DIVERGES: the game passes matrix kind
 * 0x4C, its own look-at function (see the file comment); the port asks
 * for the pair the PVR back end reads. */
void gmCameraMakeBattleCamera(void)
{
    gmCameraMakeDefaultCamera(nGCMatrixKindPerspFastF, nGCMatrixKindLookAt, gmCameraRunFuncCamera);
}

/* gmcamera.c:1202-1206 0x8010DB2C, verbatim: the opening movie's
 * camera, whose matrix kind 8 is the roll look-at (objdisplay.c
 * gcCameraLookAtF) -- each scene's CObjDesc fills up.x with a roll. */
GObj* gmCameraMakeMovieCamera(void (*func_camera)(GObj*))
{
    return gmCameraMakeDefaultCamera(nGCMatrixKindPerspFastF, 8, func_camera);
}

/* gmcamera.c:1526-1536 0x8010E56C, verbatim */
void gmCameraSetViewportDimensions(s32 ulx, s32 uly, s32 lrx, s32 lry)
{
    gGMCameraStruct.viewport_ulx = ulx;
    gGMCameraStruct.viewport_uly = uly;
    gGMCameraStruct.viewport_lrx = lrx;
    gGMCameraStruct.viewport_lry = lry;
    gGMCameraStruct.viewport_center_x = (ulx + lrx) / 2;
    gGMCameraStruct.viewport_center_y = (uly + lry) / 2;
    gGMCameraStruct.viewport_width = lrx - ulx;
    gGMCameraStruct.viewport_height = lry - uly;
}

/* ---- gmcamera.c:1241-1290, the screen flash's camera ---
 *
 * The camera that draws if/ifscreenflash.c's wash: DL link 22, one GObj
 * on it, priority 20 -- made before the interface camera's 20, so the
 * frame walks it first and the HUD draws over the flash (sys/objman.c
 * gcDLLinkGObjTail keeps equal priorities in the order they were made,
 * gcDrawAll walks the camera list from its head, highest priority
 * first: the battle's 50, the arrows' 35, the magnify's 30, this, the
 * interface's, the effect camera's 15). */

/* gmcamera.c:1241-1265 0x8010DC24.
 *
 * DIVERGES, the arrows camera's divergence again: the N64 holds one
 * viewport, so after the magnify camera's pass it is one glass's
 * 18*scale square, and under magnify_mode this camera puts the battle
 * camera's viewport and scissor back before capturing its link. The
 * port's viewport is per camera and the flash is a quad in screen pixels
 * that no viewport moves (lbCommonSpriteFillRect), so the branch is kept
 * doing what its gSPViewport and gDPSetScissor did: publish the battle
 * camera's rectangle. */
void gmCameraScreenFlashProcDisplay(GObj *camera_gobj)
{
    CObj *cobj = CObjGetStruct(camera_gobj);

    if (gIFCommonPlayerInterface.magnify_mode != 0)
    {
        gcPrepCameraViewport(CObjGetStruct(gGMCameraGObj), 0);
    }
    gcCaptureCameraGObj(camera_gobj, (cobj->flags & COBJ_FLAG_IDENTIFIER) ? 1 : 0);
    func_80017CC8(cobj);
}

/* gmcamera.c:1267-1290 0x8010DDC4, verbatim. */
void gmCameraScreenFlashMakeCamera(void)
{
    CObj *cobj = CObjGetStruct
    (
        gcMakeCameraGObj
        (
            nGCCommonKindScissorCamera,
            NULL,
            nGCCommonLinkIDCamera,
            GOBJ_PRIORITY_DEFAULT,
            gmCameraScreenFlashProcDisplay,
            20,
            COBJ_MASK_DLLINK(22),
            ~0,
            FALSE,
            nGCProcessKindFunc,
            NULL,
            1,
            FALSE
        )
    );
    cobj->flags |= COBJ_FLAG_DLBUFFERS;
}

/* ---- gmcamera.c:1292-1404, the magnify camera ---------
 *
 * The magnifying glass draws an off-screen fighter a second time, under
 * this camera, through a viewport of its own (if/ifcommon.c
 * ifCommonPlayerMagnifyUpdateViewport). The camera carries two XObjs of
 * the game's own kinds -- 0x4D, whose matrix function is
 * gmCameraPlayerMagnifyFuncMatrix, and 0x4E, gmCameraOrthoLookAtFuncMatrix
 * -- and neither is loaded by the camera: gcPrepCameraMatrix builds both
 * into their XObjs and the fighters' display procs load whichever they
 * are about to draw with, the first for the fighter and the second for
 * the handle of the glass.
 *
 * DIVERGES:
 *   - the two matrix functions keep their signatures and write nothing
 *     into `mtx`: a fixed-point Mtx has no reader here. Each builds the
 *     float (view, projection) pair the port's rasteriser reads
 *     (src/dc/objpvr.h) into a static of its own, and
 *     gmCameraLoadXObjMatrix installs the pair for an XObj's kind where
 *     the N64 loads the XObj's Mtx.
 *   - gmCameraPlayerMagnifyProcDisplay runs them itself where the
 *     decomp's gcPrepCameraMatrix would have dispatched the two kinds
 *     through sGCMatrixFuncList (objdisplay.c:2757): the port's
 *     gcPrepCameraMatrix knows the object system's kinds and not a
 *     scene's, and this camera is the only one in the game that has any.
 *   - gmCameraPrepProjectionFuncMatrix (1355) is one gSPMatrix and is
 *     not carried; nothing the port reaches installs it.
 */

static mtx4_t sGMCameraMagnifyViewF, sGMCameraMagnifyProjF;
static mtx4_t sGMCameraOrthoLookAtViewF, sGMCameraOrthoLookAtProjF;

/* gmcamera.c:1292-1338 0x8010DE48. The look-at is the battle camera's
 * distance, straight down +Z at a point 300 units up; the scale of the
 * glass is how tall 900 world units are on screen from there, in
 * eighteenths, capped at three. The projection of (0, 900, 0) goes
 * through the same function the fighters' own detection uses, on the
 * game's guMtxCatF(look_at, persp) -- built here as the port's
 * persp * look_at and transposed into the row-vector layout that
 * function reads (gmCameraUpdateMatrix above). */
sb32 gmCameraPlayerMagnifyFuncMatrix(Mtx *mtx, CObj *cobj, Gfx **dls)
{
    Mtx44f spA4;
    mtx4_t persp, look, m;
    CObj *main_cobj = CObjGetStruct(gGMCameraGObj);
    Vec3f *eye;
    Vec3f *at;
    Vec3f sp50;
    f32 dist_x;
    f32 dist_y;
    f32 dist_z;
    s32 i, j;

    (void)mtx;
    (void)cobj;
    (void)dls;

    eye = &main_cobj->vec.eye;
    at = &main_cobj->vec.at;

    dist_x = eye->x - at->x;
    dist_y = eye->y - at->y;
    dist_z = eye->z - at->z;

    gcMtxLookAt(look, 0.0F, 300.0F, sqrtf(SQUARE(dist_x) + SQUARE(dist_y) + SQUARE(dist_z)), 0.0F, 300.0F, 0.0F, 0.0F, 1.0F, 0.0F);
    mtx_proj(persp, main_cobj->projection.persp.fovy,
             main_cobj->projection.persp.aspect,
             main_cobj->projection.persp.near,
             main_cobj->projection.persp.far);
    mtx_mul(m, persp, look);
    for (i = 0; i < 4; i++)
    {
        for (j = 0; j < 4; j++)
        {
            spA4[i][j] = m[j * 4 + i];
        }
    }

    sp50.z = 0.0F;
    sp50.y = 900.0F;
    sp50.x = 0.0F;

    func_ovl2_800EB924(main_cobj, spA4, &sp50, &dist_x, &dist_y);

    gIFCommonPlayerInterface.magnify_scale = (dist_y / 18.0F);

    if (gIFCommonPlayerInterface.magnify_scale > 3.0F)
    {
        gIFCommonPlayerInterface.magnify_scale = 3.0F;
    }
    mtx_ortho(sGMCameraMagnifyProjF, -450.0F, 450.0F, -450.0F, 450.0F, 256.0F, 39936.0F);
    memcpy(sGMCameraMagnifyViewF, look, sizeof(look));

    return 0;
}

/* gmcamera.c:1340-1353 0x8010E00C: the same ortho box and eye the
 * arrows camera has (gmCameraMakePlayerArrowsCamera below), so that a
 * DObj placed in screen units draws in screen units. The handle of the
 * glass is drawn under it. */
sb32 gmCameraOrthoLookAtFuncMatrix(Mtx *mtx, CObj *cobj, Gfx **dls)
{
    f32 width;
    f32 height;

    (void)mtx;
    (void)cobj;
    (void)dls;

    width = (gGMCameraStruct.viewport_width / 2);
    height = (gGMCameraStruct.viewport_height / 2);

    mtx_ortho(sGMCameraOrthoLookAtProjF, -width, width, -height, height, 100.0F, 12800.0F);
    gcMtxLookAt(sGMCameraOrthoLookAtViewF, 0.0F, 0.0F, 1000.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F);

    return 0;
}

void gmCameraLoadXObjMatrix(XObj *xobj)
{
    switch (xobj->kind)
    {
    case 0x4D:
        gcSetCameraMatrixF(sGMCameraMagnifyViewF, sGMCameraMagnifyProjF);
        break;

    case 0x4E:
        gcSetCameraMatrixF(sGMCameraOrthoLookAtViewF, sGMCameraOrthoLookAtProjF);
        break;

    default:
        syDebugPrintf("gmcamera: no matrix pair for XObj kind %d\n", xobj->kind);
        scManagerRunPrintGObjStatus();
        break;
    }
}

/* gmcamera.c:1363-1375 0x8010E134. Runs only while a fighter's display
 * proc raised magnify_mode this frame (src/dc/ftdisplaymain.c). No
 * viewport of its own: each fighter sets the glass's square itself. */
void gmCameraPlayerMagnifyProcDisplay(GObj *camera_gobj)
{
    if (gIFCommonPlayerInterface.magnify_mode != 0)
    {
        CObj *cobj = CObjGetStruct(camera_gobj);
        s32 i;

        /* gcPrepCameraMatrix(gSYTaskmanDLHeads, cobj) -- see DIVERGES */
        for (i = 0; i < cobj->xobjs_num; i++)
        {
            XObj *xobj = cobj->xobjs[i];

            if (xobj == NULL)
            {
                continue;
            }
            if (xobj->kind == 0x4D)
            {
                gmCameraPlayerMagnifyFuncMatrix(NULL, cobj, NULL);
            }
            else if (xobj->kind == 0x4E)
            {
                gmCameraOrthoLookAtFuncMatrix(NULL, cobj, NULL);
            }
        }

        gcCaptureCameraGObj(camera_gobj, (cobj->flags & COBJ_FLAG_IDENTIFIER) ? 1 : 0);
        func_80017CC8(cobj);
    }
}

/* gmcamera.c:1377-1404 0x8010E1A4, verbatim. DL link 9 is the
 * fighters' (ft/ftdef.h FTDISPLAY_DLLINK_DEFAULT): they draw themselves
 * under this camera, and the glass around each from inside their own
 * display proc. */
void gmCameraMakePlayerMagnifyCamera(void)
{
    CObj *cobj = CObjGetStruct
    (
        gcMakeCameraGObj
        (
            nGCCommonKindPlayerMagnifyCamera,
            NULL,
            nGCCommonLinkIDCamera,
            GOBJ_PRIORITY_DEFAULT,
            gmCameraPlayerMagnifyProcDisplay,
            30,
            COBJ_MASK_DLLINK(9),
            ~0,
            FALSE,
            nGCProcessKindFunc,
            NULL,
            1,
            FALSE
        )
    );
    lbCommonInitCameraVec(cobj, 0x4D, 0);
    lbCommonInitCameraOrtho(cobj, 0x4E, 1);

    cobj->flags |= COBJ_FLAG_DLBUFFERS;

    gIFCommonPlayerInterface.magnify_mode = 0;
}

/* gmcamera.c:1404-1419 0x8010E254, verbatim but for the two GBI calls.
 *
 * The arrows camera is drawn only when something is off screen: the
 * fighters' own display procs raise gIFCommonPlayerInterface.arrows_flags
 * on the way past (ftdisplaymain.c:1146), gmCameraMakePlayerArrowsCamera
 * lowers it again at the end of the frame, and a frame in which no
 * fighter left the viewport never runs this camera's pass at all.
 *
 * DIVERGES, and both are the same divergence twice: the N64's RSP holds
 * one viewport and one projection at a time, so this camera inherits the
 * viewport the battle camera left (it never sets one of its own) and
 * gcPrepCameraMatrix's gSPMatrix replaces the projection. The port's
 * viewport is per camera, so gmCameraMakePlayerArrowsCamera sets the
 * battle viewport on the CObj -- the same rectangle -- and this calls
 * gcPrepCameraViewport for it, which func_80017D3C would have done for a
 * camera with the default proc. */
void gmCameraPlayerArrowsProcDisplay(GObj *camera_gobj)
{
    gcSetCameraMatrixMode(1);

    if (gIFCommonPlayerInterface.arrows_flags != 0)
    {
        CObj *cobj = CObjGetStruct(camera_gobj);

        gcPrepCameraViewport(cobj, 0);
        gcPrepCameraMatrix(NULL, cobj);

        gcCaptureCameraGObj(camera_gobj, (cobj->flags & COBJ_FLAG_IDENTIFIER) ? 1 : 0);
        func_80017CC8(cobj);
    }
}

/* gmcamera.c:1422-1452 0x8010E2D4.
 *
 * DIVERGES: the game asks for matrix kind 0x54, which is
 * gmCameraOrthoLookAtFuncMatrix -- an ortho box the size of the battle
 * viewport, with the eye a thousand units out on +Z looking at the
 * origin, up +Y. The port asks instead for the two plain kinds the PVR
 * back end reads (src/dc/objdisplay.c), and writes that box and that
 * look-at into the CObj, which is the same matrix by another road. The
 * numbers below are gmcamera.c:1342-1346's. */
void gmCameraMakePlayerArrowsCamera(void)
{
    CObj *cobj = CObjGetStruct
    (
        gcMakeCameraGObj
        (
            nGCCommonKindPlayerArrowsCamera,
            NULL,
            nGCCommonLinkIDCamera,
            GOBJ_PRIORITY_DEFAULT,
            gmCameraPlayerArrowsProcDisplay,
            35,
            COBJ_MASK_DLLINK(8),
            ~0,
            FALSE,
            nGCProcessKindFunc,
            NULL,
            1,
            FALSE
        )
    );
    gcAddXObjForCamera(cobj, nGCMatrixKindOrtho, 1);
    gcAddXObjForCamera(cobj, nGCMatrixKindLookAt, 1);

    cobj->projection.ortho.l = -(f32)(gGMCameraStruct.viewport_width / 2);
    cobj->projection.ortho.r = +(f32)(gGMCameraStruct.viewport_width / 2);
    cobj->projection.ortho.b = -(f32)(gGMCameraStruct.viewport_height / 2);
    cobj->projection.ortho.t = +(f32)(gGMCameraStruct.viewport_height / 2);
    cobj->projection.ortho.n = 100.0F;
    cobj->projection.ortho.f = 12800.0F;
    cobj->projection.ortho.scale = 1.0F;

    cobj->vec.eye.x = cobj->vec.eye.y = 0.0F;
    cobj->vec.eye.z = 1000.0F;
    cobj->vec.at.x = cobj->vec.at.y = cobj->vec.at.z = 0.0F;
    cobj->vec.up.x = cobj->vec.up.z = 0.0F;
    cobj->vec.up.y = 1.0F;

    syRdpSetViewport
    (
        &cobj->viewport,
        (f32)gGMCameraStruct.viewport_ulx,
        (f32)gGMCameraStruct.viewport_uly,
        (f32)gGMCameraStruct.viewport_lrx,
        (f32)gGMCameraStruct.viewport_lry
    );

    cobj->flags |= COBJ_FLAG_DLBUFFERS;

    gIFCommonPlayerInterface.arrows_flags = 0;
}

/* gmcamera.c:1456-1481 0x8010E438, verbatim. The HUD's camera: its
 * proc_display is lb/lbcommon.c's sprite pass over DL links 23 and 24
 * (every if/ifcommon.c interface GObj is on 23), its viewport the
 * battle's 300x220, from which lbCommonDrawSprite takes the scissor.
 * COBJ_FLAG_DLBUFFERS asks the N64 for the camera's own display-list
 * buffers; the flag is kept, and the port's draw has nothing to do
 * with it. */
GObj* gmCameraMakeInterfaceCamera(void)
{
    GObj *camera_gobj = gcMakeCameraGObj
    (
        nGCCommonKindScissorCamera,
        NULL,
        nGCCommonLinkIDCamera,
        GOBJ_PRIORITY_DEFAULT,
        lbCommonDrawSprite,
        20,
        COBJ_MASK_DLLINK(24) | COBJ_MASK_DLLINK(23),
        ~0,
        FALSE,
        nGCProcessKindFunc,
        NULL,
        1,
        FALSE
    );
    CObj *cobj = CObjGetStruct(camera_gobj);

    syRdpSetViewport(&cobj->viewport, (f32)gGMCameraStruct.viewport_ulx, (f32)gGMCameraStruct.viewport_uly, (f32)gGMCameraStruct.viewport_lrx, (f32)gGMCameraStruct.viewport_lry);

    cobj->flags |= COBJ_FLAG_DLBUFFERS;

    return camera_gobj;
}

/* ---- gmcamera.c:1484-1510, the effects camera ----------
 *
 * DL link 25, priority 15: the last camera the frame walks, and what it
 * captures is the fourth of ef/efdisplay.c's display GObjs
 * (efDisplayZPerspXLUProcDisplay on link 25, camera_mask link 3 -- the
 * one src/dc/lbpdraw.h's EFDISPLAY_DLLINK_MASK leaves out of the battle
 * camera for exactly this camera to take). It sets no matrix and no
 * viewport of its own, so what it draws is projected by whatever the
 * previous camera left, on the N64 as here; the decomp's own note is
 * "no apparent effect when it's disabled". Both verbatim. */

// 0x8010E458
void gmCameraEffectProcDisplay(GObj *camera_gobj)
{
    CObj *cobj = CObjGetStruct(camera_gobj);

    gcCaptureCameraGObj(camera_gobj, (cobj->flags & COBJ_FLAG_IDENTIFIER) ? 1 : 0);
}

// 0x8010E498 - Not entirely sure what this does, no apparent effect when it's disabled...
GObj* gmCameraMakeEffectCamera(void)
{
    GObj *camera_gobj = gcMakeCameraGObj
    (
        nGCCommonKindScissorCamera,
        NULL,
        nGCCommonLinkIDCamera,
        GOBJ_PRIORITY_DEFAULT,
        gmCameraEffectProcDisplay,
        15,
        COBJ_MASK_DLLINK(25),
        ~0,
        FALSE,
        nGCProcessKindFunc,
        NULL,
        1,
        FALSE
    );
    CObj *cobj = CObjGetStruct(camera_gobj);

    syRdpSetViewport(&cobj->viewport, (f32)gGMCameraStruct.viewport_ulx, (f32)gGMCameraStruct.viewport_uly, (f32)gGMCameraStruct.viewport_lrx, (f32)gGMCameraStruct.viewport_lry);

    cobj->flags |= COBJ_FLAG_DLBUFFERS;

    return camera_gobj;
}

/* gmcamera.c:1539-1549 0x8010E5F4, verbatim */
sb32 gmCameraCheckTargetInBounds(f32 pos_x, f32 pos_y)
{
    f32 viewport_width = (gGMCameraStruct.viewport_width / 2);
    f32 viewport_height = (gGMCameraStruct.viewport_height / 2);

    if ((pos_x < -viewport_width) || (pos_x > viewport_width) || (pos_y < -viewport_height) || (pos_y > viewport_height))
    {
        return FALSE;
    }
    return TRUE;
}

/* The bzero arm of syDmaLoadOverlay for overlay 2, of which this
 * file is a part: sc/scmanager.c calls it on the way into every
 * scene that loads that overlay. src/dc/overlay.h says why the port
 * needs it written out. */
void gmCameraOverlayLoad(void)
{
    OVERLAY_CLEAR(gGMCameraMatrix);
    OVERLAY_CLEAR(gGMCameraGObj);
    OVERLAY_CLEAR(gGMCameraPauseCameraEyeY);
    OVERLAY_CLEAR(gGMCameraPauseCameraEyeX);
    OVERLAY_CLEAR(gGMCameraStruct);
    OVERLAY_CLEAR(sGMCameraMagnifyViewF);
    OVERLAY_CLEAR(sGMCameraMagnifyProjF);
    OVERLAY_CLEAR(sGMCameraOrthoLookAtViewF);
    OVERLAY_CLEAR(sGMCameraOrthoLookAtProjF);
}

/* wpdisplay.c -- wp/wpdisplay.c, a weapon's display side: the four
 * renderers wpManagerMakeWeapon picks its proc_display from, the render
 * dispatcher they share, and PK Thunder's own coloured renderer.
 *   - wpDisplayMain            -- the dispatcher: normal weapons bracket
 *     proc_display between "draw normal" and "draw z-buffer" render-mode
 *     sets; the two develop display modes divert to the collision
 *     visualisers instead;
 *   - wpDisplayDLHead1 / wpDisplayDObjDLLinks / func_ovl3_80167618 /
 *     wpDisplayDObjTreeDLLinks -- the four proc_display kinds, each
 *     wpDisplayMain with a different underlying DObj draw;
 *   - wpDisplayPKThunderProcDisplay -- Ness's PK Thunder, which sets a
 *     per-trail-segment prim/env colour before the same draw;
 *   - wpDisplayHitCollisions / wpDisplayMapCollisions / wpDisplayDrawNormal
 *     / wpDisplayDrawZBuffer -- the develop-mode collision debug draws and
 *     the render-mode sets, all N64 RDP work with no PVR counterpart.
 * Function-for-function against the game's own WPStruct (wp/wptypes.h);
 * every function names its decomp address, and what is left out is marked
 * DIVERGES where it happens.
 *
 * The whole file is display, so its divergences are all of one shape: the
 * N64 walks a DObj into gSYTaskmanDLHeads[] and sets an RDP render mode per
 * pass; the port has no display-list heads (src/dc/taskman.c) and bakes the
 * render mode into each batch's FPackBatch.bucket (src/dc/fighter.c routes
 * the bucket to its PVR list). So:
 *   - wpDisplayDrawNormal / wpDisplayDrawZBuffer are no-ops here -- the
 *     G_ZBUFFER geometry-mode flip and the XLU-vs-ZB-XLU render mode they
 *     set are properties of the baked bucket, chosen once at bake time;
 *   - all four proc_display kinds resolve to dc_model_proc_display, not the
 *     decomp's own gcDrawDObjDLHead1/gcDrawDObjDLLinksForGObj/
 *     gcDrawDObjTreeDLLinksForGObj -- those three only record each joint's
 *     matrix (dc_joint_submit), the same gap src/dc/itdisplay.c's header
 *     describes for items, and every weapon with a real model reaches one
 *     of them (wpManagerAddModel's dc_model_add_dobjs, wp/wpmanager.c
 *     above). Only the fourth kind (lbCommonDObjScaleXProcDisplay, under
 *     func_ovl3_80167618, oddly marked "Unused?" below) ever actually drew
 *     anything, which is why most weapons with a model never appeared
 *     (found from a real bug report -- items and Kongo Jungle's TaruCann
 *     had the identical bug first);
 *   - the collision visualisers (wpDisplayHitCollisions/MapCollisions) are
 *     develop display modes only (nDBDisplayMode default is Master, which
 *     never reaches them in play), and the cube/edge/diamond debug DLs they
 *     emit (dFTDisplayMain*CollisionDL) are not baked into any DC pack --
 *     so they are divergence stubs, as the fighter's own collision debug
 *     folded away (src/dc/ftdisplaymain.c);
 *   - PK Thunder's gDPSetPrimColor/gDPSetEnvColor become the clone's live
 *     colour overrides (src/dc/fighter.h fighter_set_prim_color/env_color),
 *     the same substitution src/dc/efmanager.c makes for the impact wave.
 *
 * All of proc_display's targets are already in the DC build; wpdisplay
 * itself adds no decomp source. The weapon GObj gets its baked model and
 * its proc_display from wpManagerMakeWeapon (next step); PK Thunder in
 * particular arrives with Ness under ft/ftchar/, so its coloured path is
 * wired here but not exercised until then. */
#include <wp/weapon.h>
#include <ft/fighter.h>
#include <sys/develop.h>       /* nDBDisplayMode* */
#include <sys/matrix.h>

#include "objmodel.h"          /* dc_model_of, dc_model_proc_display */

#ifndef FT_HOSTTEST
#include "fighter.h"           /* fighter_set_prim_color/env_color */
#endif

// // // // // // // // // // // //
//                               //
//       INITIALIZED DATA        //
//                               //
// // // // // // // // // // // //

// 0x80188E10
SYColorRGB dWPDisplayPKThunderPrimColors[/* */] = { { 0x5E, 0xA3, 0xFF }, { 0x98, 0xBD, 0xFF }, { 0xC2, 0xD9, 0xFF }, { 0xB3, 0xF1, 0xFF } };

// 0x80188E1C
SYColorRGB dWPDisplayPKThunderEnvColors[/* */] = { { 0x3A, 0x00, 0x83 }, { 0x5B, 0x00, 0xB2 }, { 0x86, 0x33, 0xD9 }, { 0xA7, 0x74, 0xF8 } };

#ifdef FT_HOSTTEST
/* The cross-test has no clone and no renderer; the PK Thunder colours land
 * in two words it can read back, the way src/dc/efmanager.c's impact wave
 * does (gEFManagerImpactWaveLastPrim/Env). */
u32 gWPDisplayPKThunderLastPrim;
u32 gWPDisplayPKThunderLastEnv;
#endif

// // // // // // // // // // // //
//                               //
//           FUNCTIONS           //
//                               //
// // // // // // // // // // // //

// 0x80166E80
void wpDisplayHitCollisions(GObj *weapon_gobj) // Render weapon hitboxes
{
    /* DIVERGES: develop-mode hitbox visualiser. The N64 walks each live
     * WPAttackColl hitbox into gSYTaskmanDLHeads[0] as a scaled/translated
     * dFTDisplayMainHitCollision{Cube,Blend,Edge}DL. Those debug DLs are
     * not baked into any DC pack and this runs only under the develop
     * display modes (never Master), so nothing is drawn -- as the
     * fighter's own hit-collision debug folded away. */
    (void)weapon_gobj;
}

// 0x801671F0
void wpDisplayMapCollisions(GObj *weapon_gobj) // Render weapon ECB?
{
    /* DIVERGES: develop-mode map-collision diamond, the same case as
     * wpDisplayHitCollisions -- dFTDisplayMainMapCollision{Top,Bottom}DL
     * into gSYTaskmanDLHeads[1], unbaked, develop-only. */
    (void)weapon_gobj;
}

// 0x80167454
void wpDisplayDrawNormal(void)
{
    /* DIVERGES: sets G_RM_AA_XLU_SURF with G_ZBUFFER cleared into head 1.
     * The render mode and z-buffer state are baked per batch into
     * FPackBatch.bucket, so there is nothing to set here. */
}

// 0x801674B8
void wpDisplayDrawZBuffer(void)
{
    /* DIVERGES: the G_ZBUFFER-on, G_RM_AA_ZB_XLU_SURF counterpart of
     * wpDisplayDrawNormal -- baked into the bucket, so a no-op here. */
}

// 0x80167520
void wpDisplayMain(GObj *weapon_gobj, void (*proc_display)(GObj*))
{
    WPStruct *wp = wpGetStruct(weapon_gobj);

    if (wp->display_mode == nDBDisplayModeMapCollision)
    {
        wpDisplayDrawNormal();

        proc_display(weapon_gobj);

        wpDisplayDrawZBuffer();

        wpDisplayMapCollisions(weapon_gobj);
    }
    else if ((wp->display_mode == nDBDisplayModeMaster) || (wp->attack_coll.attack_state == nGMAttackStateOff))
    {
        wpDisplayDrawNormal();

        proc_display(weapon_gobj);

        wpDisplayDrawZBuffer();
    }
    else wpDisplayHitCollisions(weapon_gobj);
}

// 0x801675D0
void wpDisplayDLHead1(GObj *weapon_gobj)
{
    wpDisplayMain(weapon_gobj, dc_model_proc_display);
}

// 0x801675F4
void wpDisplayDObjDLLinks(GObj *weapon_gobj)
{
    wpDisplayMain(weapon_gobj, dc_model_proc_display);
}

// 0x80167618
void func_ovl3_80167618(GObj *weapon_gobj)
{
    wpDisplayMain(weapon_gobj, lbCommonDObjScaleXProcDisplay); // Unused?
}

// 0x8016763C
void wpDisplayDObjTreeDLLinks(GObj *weapon_gobj)
{
    wpDisplayMain(weapon_gobj, dc_model_proc_display);
}

/* The N64 issues gDPSetPrimColor/gDPSetEnvColor into head 1 for the trail
 * segment, then gcDrawDObjDLLinksForGObj. Here the two colours are the
 * clone's live overrides and the draw is dc_model_proc_display -- the same
 * shape as src/dc/efmanager.c's impact wave. Split out so the host arm can
 * record what it would have set. */
static void wpDisplayPKThunderSetColors(GObj *weapon_gobj, s32 index)
{
    u32 prim = ((u32)dWPDisplayPKThunderPrimColors[index].r << 24)
             | ((u32)dWPDisplayPKThunderPrimColors[index].g << 16)
             | ((u32)dWPDisplayPKThunderPrimColors[index].b << 8) | 0xFF;
    u32 env  = ((u32)dWPDisplayPKThunderEnvColors[index].r << 24)
             | ((u32)dWPDisplayPKThunderEnvColors[index].g << 16)
             | ((u32)dWPDisplayPKThunderEnvColors[index].b << 8) | 0xFF;
#ifndef FT_HOSTTEST
    Fighter *f = dc_model_of(weapon_gobj);

    if (f != NULL)
    {
        fighter_set_prim_color(f, prim);
        /* env is packed RGBA here; fighter_set_env_color takes RGBA too. */
        fighter_set_env_color(f, env);
    }
#else
    (void)weapon_gobj;
    gWPDisplayPKThunderLastPrim = prim;
    gWPDisplayPKThunderLastEnv = env;
#endif
}

static void wpDisplayPKThunderDraw(GObj *weapon_gobj)
{
#ifndef FT_HOSTTEST
    dc_model_proc_display(weapon_gobj);
#else
    (void)weapon_gobj;
#endif
}

// 0x80167660
void wpDisplayPKThunderProcDisplay(GObj *weapon_gobj)
{
    WPStruct *wp = wpGetStruct(weapon_gobj);
    s32 index = wp->weapon_vars.pkthunder_trail.trail_id;

    if (wp->display_mode == nDBDisplayModeMapCollision)
    {
        wpDisplayDrawNormal();

        wpDisplayPKThunderSetColors(weapon_gobj, index);

        wpDisplayPKThunderDraw(weapon_gobj);

        wpDisplayDrawZBuffer();

        wpDisplayMapCollisions(weapon_gobj);
    }
    else if ((wp->display_mode == nDBDisplayModeMaster) || (wp->attack_coll.attack_state == nGMAttackStateOff))
    {
        wpDisplayDrawNormal();

        wpDisplayPKThunderSetColors(weapon_gobj, index);

        wpDisplayPKThunderDraw(weapon_gobj);

        wpDisplayDrawZBuffer();
    }
    else wpDisplayHitCollisions(weapon_gobj);
}

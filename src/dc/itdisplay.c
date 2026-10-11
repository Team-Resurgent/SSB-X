/* itdisplay.c -- it/itdisplay.c, an item's display side: the visibility
 * gate every proc_display shares, the four render dispatchers a made
 * item's proc_display is set to, and the develop-mode collision debug
 * draws that dispatch, on this port, never reaches. Function-for-
 * function against the game's own ITStruct (it/ittypes.h); every
 * function names its decomp address, and what is left out is marked
 * DIVERGES.
 *
 * The file mirrors wp/wpdisplay.c's shape closely enough
 * to reuse its reasoning wholesale:
 *   - itDisplayCheckItemVisible is pure ITStruct/FTStruct logic and
 *     ports verbatim;
 *   - itDisplayOPAProcDisplay / itDisplayXLUProcDisplay / itDisplay-
 *     ColAnimOPAProcDisplay / itDisplayColAnimXLUProcDisplay are the
 *     verbatim dispatch logic itManagerMakeItem will set as an item's
 *     proc_display (next step's keystone), branching on display_mode/
 *     is_hold/hitstatus/attack_state between the normal draw, the
 *     develop map-collision draw-then-diamond, and the develop hit-
 *     collision visualiser;
 *   - itDisplayHitCollisions / itDisplayMapCollisions are DIVERGES
 *     stubs, the same case as wpDisplayHitCollisions/MapCollisions:
 *     develop-mode-only debug draws (nDBDisplayMode defaults to
 *     Master, which never reaches either), walking gSYTaskmanDLHeads[]
 *     with scaled/translated dFTDisplayMain*CollisionDL display lists
 *     that are not baked into any DC pack -- so nothing to draw, as the
 *     fighter's and the weapon's own hit-collision debug already folded
 *     away;
 *   - itDisplayColAnimOPA / itDisplayColAnimXLU set an RDP 2-cycle
 *     blend (gDPSetCycleType/gDPSetRenderMode) around the draw so the
 *     env colour tints it -- the render-mode half is a bucket property
 *     baked at model-bake time (the same "nothing to set here" as
 *     wpDisplayDrawNormal/ZBuffer), and the env-colour half becomes the
 *     clone's live colour override (fighter_set_env_color), the same
 *     substitution wpDisplayPKThunderSetColors made for PK Thunder's
 *     trail and src/dc/efmanager.c makes for the impact wave. DIVERGES:
 *     the actual draw is dc_model_proc_display (src/dc/objmodel.c), not
 *     the decomp's gcDrawDObjTreeForGObj/gcDrawDObjTreeDLLinksForGObj --
 *     those two only record each joint's matrix (dc_joint_submit), the
 *     same as a fighter's gcDrawDObjTree pass; a fighter's own display
 *     proc calls dc_model_proc_display to actually submit the baked
 *     batches to the PVR afterwards, and an item's four dispatchers need
 *     the same second step or nothing of the model ever reaches the
 *     screen (found from a real bug report: an item's map-spawn marker
 *     drew, its model never did). Safe to call on an item with no model
 *     yet the same way the old calls were -- dc_model_proc_display reads
 *     the DObj's dv, which is NULL until itemModelAddToGObj runs, and
 *     returns immediately.
 *
 * All four dispatchers' draw targets are already in the DC build; this
 * step adds no other decomp source. An item GObj gets its baked model
 * and proc_display from itManagerMakeItem, still ahead (needs the full
 * item roster) -- so everything here is wired but not yet exercised by
 * a real item, the same position wp/wpdisplay.c was in for PK Thunder
 * until Ness arrived. */
#include <it/item.h>
#include <ft/fighter.h>
#include <sys/develop.h>       /* nDBDisplayMode* */

#include "objmodel.h"          /* dc_model_proc_display, dc_model_of */

#ifndef FT_HOSTTEST
#include "fighter.h"           /* fighter_set_env_color */
#else
/* The cross-test has no clone and no renderer; the env colour word
 * lands here for it to read back, the same shape gWPDisplayPKThunder-
 * LastPrim/LastEnv use in wpdisplay.c. */
u32 gITDisplayColAnimLastEnv;
#endif

// // // // // // // // // // // //
//                               //
//           FUNCTIONS           //
//                               //
// // // // // // // // // // // //

// 0x80171410
void itDisplayHitCollisions(GObj *item_gobj)
{
    /* DIVERGES: develop-mode hitbox/hurtbox visualiser. The N64 walks
     * every live ITAttackColl hitbox and the ITDamageColl hurtbox into
     * gSYTaskmanDLHeads[0] as scaled/translated dFTDisplayMainHit-
     * Collision{Cube,Blend,Edge}DL / HurtCollisionCuboidDL. Those debug
     * DLs are not baked into any DC pack and this runs only under the
     * develop display modes (never Master), so nothing is drawn -- as
     * wp/wpdisplay.c's wpDisplayHitCollisions already found. */
    (void)item_gobj;
}

// 0x801719AC
void itDisplayMapCollisions(GObj *item_gobj)
{
    /* DIVERGES: the develop-mode map-collision diamond, the same case
     * as itDisplayHitCollisions above and wpDisplayMapCollisions --
     * dFTDisplayMainMapCollision{Top,Bottom}DL into gSYTaskmanDLHeads[1],
     * unbaked, develop-only. */
    (void)item_gobj;
}

// 0x80171C10
sb32 itDisplayCheckItemVisible(ITStruct *ip)
{
    FTStruct *fp;

    if (ip->owner_gobj == NULL)
    {
        return TRUE;
    }
    else if (!(ip->is_hold))
    {
        return TRUE;
    }

    fp = ftGetStruct(ip->owner_gobj);

    if (!(fp->is_item_show))
    {
        return FALSE;
    }
    else if (fp->is_invisible)
    {
        return FALSE;
    }
    else return TRUE;
}

// 0x80171C7C
void itDisplayOPAProcDisplay(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    if (itDisplayCheckItemVisible(ip) != FALSE)
    {
        if ((ip->display_mode == nDBDisplayModeMaster) || (ip->is_hold))
        {
            dc_model_proc_display(item_gobj);
        }
        else if (ip->display_mode == nDBDisplayModeMapCollision)
        {
            dc_model_proc_display(item_gobj);
            itDisplayMapCollisions(item_gobj);
        }
        else if ((ip->damage_coll.hitstatus == nGMHitStatusNone) && (ip->attack_coll.attack_state == nGMAttackStateOff))
        {
            dc_model_proc_display(item_gobj);
        }
        else itDisplayHitCollisions(item_gobj);
    }
}

// 0x80171D38
void itDisplayXLUProcDisplay(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    if (itDisplayCheckItemVisible(ip) != FALSE)
    {
        if ((ip->display_mode == nDBDisplayModeMaster) || (ip->is_hold))
        {
            dc_model_proc_display(item_gobj);
        }
        else if (ip->display_mode == nDBDisplayModeMapCollision)
        {
            dc_model_proc_display(item_gobj);
            itDisplayMapCollisions(item_gobj);
        }
        else if ((ip->damage_coll.hitstatus == nGMHitStatusNone) && (ip->attack_coll.attack_state == nGMAttackStateOff))
        {
            dc_model_proc_display(item_gobj);
        }
        else itDisplayHitCollisions(item_gobj);
    }
}

/* it/itdisplay.c:237-256 itDisplayColAnimOPA 0x80171DF4. The 2-cycle
 * blend's render-mode set (gDPSetCycleType/gDPSetRenderMode, both PASS
 * on entry and restored to 1-cycle AA_ZB_OPA_SURF on exit) is baked
 * into the model's batch bucket at bake time, so it is not set here --
 * see the file header. The env colour it brackets the draw with is the
 * clone's live override. */
static void itDisplaySetColAnimEnv(GObj *item_gobj, ITStruct *ip)
{
    u32 env;

    if (ip->colanim.is_use_color1)
    {
        env = ((u32)ip->colanim.color1.r << 24) | ((u32)ip->colanim.color1.g << 16)
            | ((u32)ip->colanim.color1.b << 8) | (u32)ip->colanim.color1.a;
    }
    else env = 0;

#ifndef FT_HOSTTEST
    {
        /* every model the item draws: a switched-in display list's pack
         * takes the flash too (src/dc/itemmodel.c) */
        Fighter *models[DC_MODEL_TREE_MODELS];
        int n = dc_model_models_of(item_gobj, models, DC_MODEL_TREE_MODELS);
        int i;

        for (i = 0; i < n; i++)
        {
            fighter_set_env_color(models[i], env);
        }
    }
#else
    (void)item_gobj;
    gITDisplayColAnimLastEnv = env;
#endif
}

void itDisplayColAnimOPA(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    itDisplaySetColAnimEnv(item_gobj, ip);

    dc_model_proc_display(item_gobj);
}

// 0x80171F4C
void itDisplayColAnimOPAProcDisplay(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    if (itDisplayCheckItemVisible(ip) != FALSE)
    {
        if ((ip->display_mode == nDBDisplayModeMaster) || (ip->is_hold))
        {
            itDisplayColAnimOPA(item_gobj);
        }
        else if (ip->display_mode == nDBDisplayModeMapCollision)
        {
            itDisplayColAnimOPA(item_gobj);
            itDisplayMapCollisions(item_gobj);
        }
        else if ((ip->damage_coll.hitstatus == nGMHitStatusNone) && (ip->attack_coll.attack_state == nGMAttackStateOff))
        {
            itDisplayColAnimOPA(item_gobj);
        }
        else itDisplayHitCollisions(item_gobj);
    }
}

/* it/itdisplay.c:283-312 itDisplayColAnimXLU 0x80172008. Same shape as
 * itDisplayColAnimOPA above, but the decomp sets the identical env
 * colour into both gSYTaskmanDLHeads[0] and [1] (the OPA and XLU
 * buffers) -- here that collapses to the one clone-colour call, since
 * the port has no second display-list head to mirror it into. */
void itDisplayColAnimXLU(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    itDisplaySetColAnimEnv(item_gobj, ip);

    dc_model_proc_display(item_gobj);
}

// 0x8017224C
void itDisplayColAnimXLUProcDisplay(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    if (itDisplayCheckItemVisible(ip) != FALSE)
    {
        if ((ip->display_mode == nDBDisplayModeMaster) || (ip->is_hold))
        {
            itDisplayColAnimXLU(item_gobj);
        }
        else if (ip->display_mode == nDBDisplayModeMapCollision)
        {
            itDisplayColAnimXLU(item_gobj);
            itDisplayMapCollisions(item_gobj);
        }
        else if ((ip->damage_coll.hitstatus == nGMHitStatusNone) && (ip->attack_coll.attack_state == nGMAttackStateOff))
        {
            itDisplayColAnimXLU(item_gobj);
        }
        else itDisplayHitCollisions(item_gobj);
    }
}

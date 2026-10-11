/* itpippi.c -- it/itmonster/itpippi.c, verbatim: Clefairy's `ITDesc`, its
 * empty `ITStatusDesc`, the METRONOME TABLE that dispatches into all
 * twelve other monsters, Clefairy's two display procs, and its `MakeItem`.
 * Function-for-function against the game's own ITStruct (it/ittypes.h);
 * every function names its decomp line range.
 *
 * `dITPippiStatusProcList` names a `SetStatus` from every OTHER monster
 * file -- Onix's, Snorlax's, Goldeen's, Meowth's, Charizard's, Beedrill's,
 * Blastoise's, Chansey's, Starmie's, Hitmonlee's, Koffing's and Mew's --
 * which is why this file could not be ported until all twelve were, and
 * why it is the one the item table could not reference before now.
 *
 * Clefairy is the metronome: `itPippiCommonSelectMonster` rolls an index
 * into that table and RECONFIGURES ITSELF into whichever monster it picked
 * rather than spawning one. It is not a copy of the twelve -- it becomes
 * one, on the GObj it already has, by calling that monster's own
 * `SetStatus`. What makes the trick work is that the twelve share the
 * rise-out-of-the-ball procs and their `MakeItem`s' per-monster setup is
 * skipped; what makes it visible is the four fix-ups before the dispatch:
 *
 *  - Spear and Kamex get a coin-flip 180-degree model turn and `lr`, so
 *    the two that fly sideways pick a side.
 *  - Pippi, Goldeen and Chansey have their attack state turned off --
 *    they are the three that do not attack.
 *  - Hitmonlee gets `multi = ITSAWAMURA_KICK_WAIT` and Charizard
 *    `multi = ITLIZARDON_LIFETIME`, because their attack states READ
 *    `multi` as a wait they were supposed to have been given at spawn --
 *    and Clefairy never went through their `MakeItem`.
 *  - Sawamura and Starmie get `proc_display` replaced with
 *    `itPippiCommonMoveDLProcDisplay`, the XLU variant, and their DL head
 *    moved to 18. Those two are the metamorphosis: their own
 *    `MakeItem`s install that display proc directly, and a Clefairy that
 *    becomes one has to do it by hand.
 *
 * The empty `dITPippiStatusDesc` is the decomp's own `// why` and is
 * guarded by `#if !defined(DAIRANTOU_OPT0)` -- the arm the game ships,
 * since that switch is not defined. Nothing reads it: the metronome's
 * twelve statuses are the OTHER files' tables.
 *
 * DIVERGES: `dITPippiItemDesc`'s third field is a number rather than the
 * decomp's `&llITCommonDataPippiItemAttributes` symbol -- see
 * src/dc/itemoffsets.h -- and the `itGetMonsterAnimNode` call drops its
 * `&`. No function body diverges.
 */
#include <it/item.h>
#include <sys/develop.h>
#include <sys/matrix.h>

#include "itmonster.h"          /* itGetMonsterAnimNode */
#include "itemoffsets.h"        /* llITCommonData* offsets */
#include "objmodel.h"           /* dc_model_proc_display */

#ifdef FT_HOSTTEST
#include "hoststubs.h"
#else
#include <kos.h>
#endif

/* func_800269C0_275C0, the FGM call -- see src/dc/ftcommon.h. */
#include "ftcommon.h"

/* // // // // // // // // // // // //
 *                                   //
 *       INITIALIZED DATA            //
 *                                   //
 * // // // // // // // // // // // // */

/* it/itmonster/itpippi.c:13-27 dITPippiStatusProcList 0x8018B370,
 * verbatim, and the reason this file is the last one ported: every entry
 * is another monster's own SetStatus. */
void (*dITPippiStatusProcList[/* */])(GObj*) =
{
    itIwarkAttackSetStatus,
    itKabigonJumpSetStatus,
    itTosakintoAppearSetStatus,
    itNyarsAttackSetStatus,
    itLizardonFallSetStatus,
    itSpearFlySetStatus,
    itKamexAppearSetStatus,
    itMLuckyAppearSetStatus,
    itStarmieNFollowSetStatus,
    itSawamuraFallSetStatus,
    itDogasAttackSetStatus,
    itMewFlySetStatus
};

/* it/itmonster/itpippi.c:30-52 dITPippiItemDesc 0x8018B3A0, verbatim. Its
 * attack state is `nGMAttackStateNew` and its transform is TraRotRpyR --
 * the plainest of the thirteen, because the metronome replaces everything
 * that matters when it picks. */
ITDesc dITPippiItemDesc =
{
    nITKindPippi,                           /* Item Kind */
    &gITManagerCommonData,                  /* Pointer to item file data? */
    (intptr_t)llITCommonDataPippiItemAttributes,    /* Offset of item attributes in file? */

    /* DObj transformation struct */
    {
        nGCMatrixKindTraRotRpyR,            /* Main matrix transformations */
        nGCMatrixKindNull,                  /* Secondary matrix transformations? */
        0                                   /* ??? */
    },

    nGMAttackStateNew,                      /* Hitbox Update State */
    itPippiCommonProcUpdate,                /* Proc Update */
    itPippiCommonProcMap,                   /* Proc Map */
    NULL,                                   /* Proc Hit */
    NULL,                                   /* Proc Shield */
    NULL,                                   /* Proc Hop */
    NULL,                                   /* Proc Set-Off */
    NULL,                                   /* Proc Reflector */
    NULL                                    /* Proc Damage */
};

#if !defined(DAIRANTOU_OPT0)

/* it/itmonster/itpippi.c:56-58 dITPippiStatusDesc 0x8018B3D4, verbatim --
 * eight NULLs and the decomp's own comment on the line above it. Nothing
 * reads it; the metronome's statuses live in the twelve tables its own
 * dispatch list points at. Kept because removing it would be a rewrite,
 * and because the `#if` is the arm the game ships. */
ITStatusDesc dITPippiStatusDesc = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };

#endif

/* // // // // // // // // // // // //
 *                                   //
 *           FUNCTIONS               //
 *                                   //
 * // // // // // // // // // // // // */

/* it/itmonster/itpippi.c:67-108 itPippiCommonSelectMonster 0x80183210,
 * verbatim. THE METRONOME. The four fix-up blocks before the dispatch are
 * the whole of what a Clefairy-that-became-something-else needs and did
 * not get from that monster's MakeItem, and each is there for a stated
 * reason -- see this file's header. */
void itPippiCommonSelectMonster(GObj *item_gobj)
{
    s32 kind;
    s32 index;
    ITStruct *ip = itGetStruct(item_gobj);
    DObj *dobj = DObjGetStruct(item_gobj);

    index = syUtilsRandIntRange(ARRAY_COUNT(dITPippiStatusProcList));

    kind = index + nITKindMBallMonsterStart;

    if ((kind == nITKindSpear) || (kind == nITKindKamex))
    {
        if (syUtilsRandIntRange(2) == 0)
        {
            dobj->rotate.vec.f.y = F_CST_DTOR32(180.0F);

            ip->lr = +1;
        }
        else ip->lr = -1;
    }
    if ((kind == nITKindPippi) || (kind == nITKindTosakinto) || (kind == nITKindMLucky))
    {
        ip->attack_coll.attack_state = nGMAttackStateOff;
    }
    if (kind == nITKindSawamura)
    {
        ip->multi = ITSAWAMURA_KICK_WAIT;
    }
    if ((kind == nITKindSawamura) || (kind == nITKindStarmie))
    {
        item_gobj->proc_display = itPippiCommonMoveDLProcDisplay;

        gcMoveGObjDLHead(item_gobj, 18, item_gobj->dl_link_priority);
    }
    if (kind == nITKindLizardon)
    {
        ip->multi = ITLIZARDON_LIFETIME;
    }
    dITPippiStatusProcList[index](item_gobj);
}

/* it/itmonster/itpippi.c:110-141 itPippiCommonProcDisplay 0x80183344,
 * verbatim: the four-arm display every monster's own common display proc
 * is a copy of, in ZB_TEX_EDGE. */
void itPippiCommonProcDisplay(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    gDPPipeSync(gSYTaskmanDLHeads[0]++);

    if (itDisplayCheckItemVisible(ip) != FALSE)
    {
        if ((ip->display_mode == nDBDisplayModeMaster) || (ip->is_hold))
        {
            gDPSetRenderMode(gSYTaskmanDLHeads[0]++, G_RM_AA_ZB_TEX_EDGE, G_RM_AA_ZB_TEX_EDGE2);

            dc_model_proc_display(item_gobj);
        }
        else if (ip->display_mode == nDBDisplayModeMapCollision)
        {
            gDPSetRenderMode(gSYTaskmanDLHeads[0]++, G_RM_AA_ZB_TEX_EDGE, G_RM_AA_ZB_TEX_EDGE2);

            dc_model_proc_display(item_gobj);
            itDisplayMapCollisions(item_gobj);
        }
        else if ((ip->damage_coll.hitstatus == nGMHitStatusNone) && (ip->attack_coll.attack_state == nGMAttackStateOff))
        {
            gDPSetRenderMode(gSYTaskmanDLHeads[0]++, G_RM_AA_ZB_TEX_EDGE, G_RM_AA_ZB_TEX_EDGE2);

            dc_model_proc_display(item_gobj);
        }
        else itDisplayHitCollisions(item_gobj);
    }
    gDPPipeSync(gSYTaskmanDLHeads[0]++);
}

/* it/itmonster/itpippi.c:143-174 itPippiCommonMoveDLProcDisplay 0x801834A0,
 * verbatim: the same four arms in XLU, installed on a Hitmonlee or a
 * Starmie the metronome picked. Its decomp name says "MoveDL" because the
 * N64's `dobj->dl` half is the model; the port draws `dobj->dv`, so the
 * only real difference from the proc above is the render mode. */
void itPippiCommonMoveDLProcDisplay(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    gDPPipeSync(gSYTaskmanDLHeads[0]++);

    if (itDisplayCheckItemVisible(ip) != FALSE)
    {
        if ((ip->display_mode == nDBDisplayModeMaster) || (ip->is_hold))
        {
            gDPSetRenderMode(gSYTaskmanDLHeads[0]++, G_RM_AA_XLU_SURF, G_RM_AA_XLU_SURF2);

            dc_model_proc_display(item_gobj);
        }
        else if (ip->display_mode == nDBDisplayModeMapCollision)
        {
            gDPSetRenderMode(gSYTaskmanDLHeads[0]++, G_RM_AA_XLU_SURF, G_RM_AA_XLU_SURF2);

            dc_model_proc_display(item_gobj);
            itDisplayMapCollisions(item_gobj);
        }
        else if ((ip->damage_coll.hitstatus == nGMHitStatusNone) && (ip->attack_coll.attack_state == nGMAttackStateOff))
        {
            gDPSetRenderMode(gSYTaskmanDLHeads[0]++, G_RM_AA_XLU_SURF, G_RM_AA_XLU_SURF2);

            dc_model_proc_display(item_gobj);
        }
        else itDisplayHitCollisions(item_gobj);
    }
    gDPPipeSync(gSYTaskmanDLHeads[0]++);
}

/* it/itmonster/itpippi.c:176-190 itPippiCommonProcUpdate 0x801835FC,
 * verbatim. The rise, then the METRONOME -- so a Clefairy picked by the
 * ball spends ITMONSTER_RISE_STOP_WAIT frames as itself before becoming
 * anything else. */
sb32 itPippiCommonProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    if (ip->multi == 0)
    {
        ip->physics.vel_air.x = ip->physics.vel_air.y = 0.0F;

        itPippiCommonSelectMonster(item_gobj);
    }
    ip->multi--;

    return FALSE;
}

/* it/itmonster/itpippi.c:192-202 itPippiCommonProcMap 0x80183650,
 * verbatim. */
sb32 itPippiCommonProcMap(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    if (itMapTestAllCollisionFlag(item_gobj, MAP_FLAG_FLOOR) != FALSE)
    {
        ip->physics.vel_air.y = 0.0F;
    }
    return FALSE;
}

/* it/itmonster/itpippi.c:204-231 itPippiMakeItem 0x80183690, verbatim.
 * Note what it does NOT do that its twelve targets' MakeItems each do: no
 * itMainClearOwnerStats, no `interact_mask` narrowing, no per-monster
 * XObjs. Everything a monster needs beyond the rise comes either from its
 * own SetStatus or from the metronome's fix-ups -- which is exactly why
 * those fix-ups exist. */
GObj* itPippiMakeItem(GObj *parent_gobj, Vec3f *pos, Vec3f *vel, u32 flags)
{
    GObj *item_gobj = itManagerMakeItem(parent_gobj, &dITPippiItemDesc, pos, vel, flags);

    if (item_gobj != NULL)
    {
        DObj *dobj = DObjGetStruct(item_gobj);
        ITStruct *ip = itGetStruct(item_gobj);

        ip->multi = ITMONSTER_RISE_STOP_WAIT;

        ip->physics.vel_air.x = ip->physics.vel_air.z = 0.0F;
        ip->physics.vel_air.y = ITMONSTER_RISE_VEL_Y;

        gcAddXObjForDObjFixed(dobj, 0x48, 0);

        dobj->translate.vec.f = *pos;

        dobj->translate.vec.f.y -= ip->attr->map_coll_bottom;

        gcAddDObjAnimJoint(dobj, itGetMonsterAnimNode(ip, llITCommonDataPippiDataStart), 0.0F);
        func_800269C0_275C0(nSYAudioVoiceMBallPippiAppear);

        item_gobj->proc_display = itPippiCommonProcDisplay;
    }
    return item_gobj;
}

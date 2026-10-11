/* ftcommon.c -- see ftcommon.h. Function-for-function port from
 * ssb-decomp-re: every function names the decomp file, line range and
 * ROM address it comes from, and its body is that function's text with
 * the substitutions the header describes. Anything else is marked
 * DIVERGES at the line it happens.
 *
 * Action-state ids, motion ids, animation flags and every FTCOMMON_*
 * threshold come from the decomp's own <ft/ftdef.h> and <ft/ftcommon.h>
 * (see src/dc/decomp/README.md); no constant is transcribed here.
 */
#include "ftcommon.h"

#include <gm/gmcollision.h>
#include <sc/sc1pmode/sc1pgame.h>
#include <if/ifcommon.h>   /* the HUD calls the KO makes; the port stubs them (ifcommon.c) */
#include <it/item.h>          /* ITStruct, nITWeightHeavy: types only, item.c isn't compiled in */

#include <ft/ftcommon.h>
#include <ft/ftanim.h>
#include <ft/ftdef.h>       /* FTSTAT_CHARDATA_START: ftMainGetStatusDesc below */

#ifdef FT_HOSTTEST
#include "hoststubs.h"
#else
#include <kos.h>
#endif
#include <math.h>
#include <stddef.h>             /* offsetof, for the Kirby motion-file image */
#include <string.h>

#include "efmanager.h"         /* the shield's three effects */
#include "lbcommon.h"          /* the tree helpers the hidden parts and the attach use */
#include "mpcommon.h"
#include "objmodel.h"

#include <sc/scene.h>
#include <sys/debug.h>

#include <sys/objanim.h>
#include "overlay.h"
#include "wpattrs.h"           /* wpAttrsBind, from ftCommonOverlayLoad */

/* The N64 C buttons are the jump buttons (R|L|D|U_CBUTTONS in the
 * decomp); sy input keeps the PR bit values under N64_ names. */
#define FT_JUMP_BUTTONS (N64_C_UP | N64_C_DOWN | N64_C_LEFT | N64_C_RIGHT)

FTStatusDesc dFTCommonNullStatusDescs[nFTCommonStatusActionStart];
FTStatusDesc dFTCommonActionStatusDescs[nFTCommonStatusLandingAirNull + 1 - nFTCommonStatusActionStart];
FTStatusDesc dFTMarioSpecialStatusDescs[nFTMarioStatusSpecialAirLw - nFTCommonStatusSpecialStart + 1];
FTStatusDesc dFTDonkeySpecialStatusDescs[nFTDonkeyStatusHeavyThrowB4 - nFTCommonStatusSpecialStart + 1];
FTStatusDesc dFTPurinSpecialStatusDescs[nFTPurinStatusSpecialAirLw - nFTCommonStatusSpecialStart + 1];
FTStatusDesc dFTKirbySpecialStatusDescs[nFTKirbyStatusCopyYoshiSpecialAirNRelease - nFTCommonStatusSpecialStart + 1];
FTStatusDesc dFTFoxSpecialStatusDescs[nFTFoxStatusSpecialLwScopeEnd - nFTCommonStatusSpecialStart + 1];
FTStatusDesc dFTSamusSpecialStatusDescs[nFTSamusStatusSpecialAirLw - nFTCommonStatusSpecialStart + 1];
FTStatusDesc dFTCaptainSpecialStatusDescs[nFTCaptainStatusSpecialAirHi - nFTCommonStatusSpecialStart + 1];
FTStatusDesc dFTYoshiSpecialStatusDescs[nFTYoshiStatusSpecialAirNRelease - nFTCommonStatusSpecialStart + 1];
FTStatusDesc dFTLinkSpecialStatusDescs[nFTLinkStatusSpecialAirLw - nFTCommonStatusSpecialStart + 1];
FTStatusDesc dFTLuigiSpecialStatusDescs[nFTLuigiStatusSpecialAirLw - nFTCommonStatusSpecialStart + 1];
FTStatusDesc dFTNessSpecialStatusDescs[nFTNessStatusSpecialAirLwEnd - nFTCommonStatusSpecialStart + 1];
FTStatusDesc dFTPikachuSpecialStatusDescs[nFTPikachuStatusSpecialAirHiEnd - nFTCommonStatusSpecialStart + 1];
FTStatusDesc dFTBossSpecialStatusDescs[nFTBossStatusAppear - nFTCommonStatusSpecialStart + 1];

/* ft/ftmain.c:78-106 dFTMainSpecialStatusDescs: which fighter's special
 * table a status id past nFTCommonStatusSpecialStart indexes. The game's
 * is 27 bare pointers, one per FTKind, the nametag kinds sharing their
 * base fighter's table and Luigi's Tornado living in a table of his own
 * even though the setter is Mario's.
 *
 * DIVERGES: each table here carries its length. The game's tables are
 * complete, so it indexes without a bound; the port's can stop short of
 * the game's, and a row skipped inside one is all zero.
 * ftMainGetStatusDesc answers NULL for both, and ftMainSetStatus prints
 * the status id and aborts -- loudly, on the spot, naming the number,
 * which is the only useful thing to do with a status the port cannot
 * describe. tools/check/status_check.py lists every row still missing.
 *
 * A FT_SPECIAL_NONE row is a promise that nothing sets a status past
 * nFTCommonStatusSpecialStart on that kind. The evidence comes from all
 * SIX demux lists, the fkind switches in ftcommonattack1.c, and
 * dFTCommonEntryAppearStatusIDs. Pikachu has a table of four rows
 * (his two Appear statuses and his Neutral-B), and all six of his demux
 * slots were NULL and no fkind switch names a special status of his.
 * BOSS is the other potential NONE with unclean evidence: five of his
 * slots hold Mario's setters. He is 1P content and nothing in this port
 * spawns him, so it is latent rather than live. */
typedef struct FTSpecialTable
{
    FTStatusDesc *rows;
    s32 count;

} FTSpecialTable;

#define FT_SPECIAL_TABLE(t) { (t), (s32)ARRAY_COUNT(t) }
#define FT_SPECIAL_NONE     { NULL, 0 }

static const FTSpecialTable dFTMainSpecialStatusDescs[] = {
    [nFTKindMario]    = FT_SPECIAL_TABLE(dFTMarioSpecialStatusDescs),
    [nFTKindFox]      = FT_SPECIAL_TABLE(dFTFoxSpecialStatusDescs),
    [nFTKindDonkey]   = FT_SPECIAL_TABLE(dFTDonkeySpecialStatusDescs),
    [nFTKindSamus]    = FT_SPECIAL_TABLE(dFTSamusSpecialStatusDescs),
    /* the row here is dFTLuigiSpecialStatusDescs, not Mario's: Mario's
 * four setters serve Luigi too and set the same status NUMBERS, and
 * this array is the only thing that decides whose table those
 * numbers index. It was FT_SPECIAL_NONE until which
 * made every special Luigi had an abort */
    [nFTKindLuigi]    = FT_SPECIAL_TABLE(dFTLuigiSpecialStatusDescs),
    [nFTKindLink]     = FT_SPECIAL_TABLE(dFTLinkSpecialStatusDescs),
    [nFTKindYoshi]    = FT_SPECIAL_TABLE(dFTYoshiSpecialStatusDescs),
    [nFTKindCaptain]  = FT_SPECIAL_TABLE(dFTCaptainSpecialStatusDescs),
    [nFTKindKirby]    = FT_SPECIAL_TABLE(dFTKirbySpecialStatusDescs),
    [nFTKindPikachu]  = FT_SPECIAL_TABLE(dFTPikachuSpecialStatusDescs),
    [nFTKindPurin]    = FT_SPECIAL_TABLE(dFTPurinSpecialStatusDescs),
    [nFTKindNess]     = FT_SPECIAL_TABLE(dFTNessSpecialStatusDescs),
    [nFTKindBoss]     = FT_SPECIAL_TABLE(dFTBossSpecialStatusDescs),
    [nFTKindMMario]   = FT_SPECIAL_TABLE(dFTMarioSpecialStatusDescs),
    [nFTKindNMario]   = FT_SPECIAL_TABLE(dFTMarioSpecialStatusDescs),
    [nFTKindNFox]     = FT_SPECIAL_TABLE(dFTFoxSpecialStatusDescs),
    [nFTKindNDonkey]  = FT_SPECIAL_TABLE(dFTDonkeySpecialStatusDescs),
    [nFTKindNSamus]   = FT_SPECIAL_TABLE(dFTSamusSpecialStatusDescs),
    [nFTKindNLuigi]   = FT_SPECIAL_TABLE(dFTLuigiSpecialStatusDescs),
    [nFTKindNLink]    = FT_SPECIAL_TABLE(dFTLinkSpecialStatusDescs),
    [nFTKindNYoshi]   = FT_SPECIAL_TABLE(dFTYoshiSpecialStatusDescs),
    [nFTKindNCaptain] = FT_SPECIAL_TABLE(dFTCaptainSpecialStatusDescs),
    [nFTKindNKirby]   = FT_SPECIAL_TABLE(dFTKirbySpecialStatusDescs),
    [nFTKindNPikachu] = FT_SPECIAL_TABLE(dFTPikachuSpecialStatusDescs),
    [nFTKindNPurin]   = FT_SPECIAL_TABLE(dFTPurinSpecialStatusDescs),
    [nFTKindNNess]    = FT_SPECIAL_TABLE(dFTNessSpecialStatusDescs),
    [nFTKindGDonkey]  = FT_SPECIAL_TABLE(dFTDonkeySpecialStatusDescs),
};

void ftCommonWalkSetStatusParam(GObj *fighter_gobj,
                                       f32 anim_frame_begin);
sb32 ftCommonWalkCheckInterruptCommon(GObj *fighter_gobj);
sb32 ftCommonKneeBendCheckInterruptCommon(GObj *fighter_gobj);
sb32 ftCommonPassCheckInterruptCommon(GObj *fighter_gobj);
sb32 ftCommonSquatCheckInterruptCommon(GObj *fighter_gobj);
sb32 ftCommonSpecialLwCheckInterruptCommon(GObj *fighter_gobj);
sb32 ftCommonTurnCheckInterruptCommon(GObj *fighter_gobj);
sb32 ftCommonWaitCheckInterruptCommon(GObj *fighter_gobj);
sb32 ftCommonAttackDashCheckInterruptCommon(GObj *fighter_gobj);
sb32 ftCommonAttackS4CheckInterruptDash(GObj *fighter_gobj);
sb32 ftCommonAttackS4CheckInterruptTurn(GObj *fighter_gobj);
sb32 ftCommonAttackS4CheckInterruptCommon(GObj *fighter_gobj);
sb32 ftCommonAttackHi4CheckInterruptKneeBend(GObj *fighter_gobj);
sb32 ftCommonAttackHi4CheckInterruptCommon(GObj *fighter_gobj);
sb32 ftCommonAttackLw4CheckInterruptSquat(GObj *fighter_gobj);
sb32 ftCommonAttackLw4CheckInterruptCommon(GObj *fighter_gobj);
#define FTCOMMON_GROUNDATTACK_COMMON 0
#define FTCOMMON_GROUNDATTACK_SQUAT 1
#define FTCOMMON_GROUNDATTACK_TURN 2

static sb32 ftCommonAttackCheckInterruptGround(GObj *fighter_gobj, s32 spelling);
void ftCommonCliffWaitSetStatus(GObj *fighter_gobj);
void ftCommonCliffCommonProcPhysics(GObj *fighter_gobj);
void ftCommonCliffCommonProcDamage(GObj *fighter_gobj);

/* ---- the fighter's parts: the game's own animation engine on the
 * fighter's own DObjs ------------------------------------------------- */

/* ft/ftparam.c:364-477 ftParamUpdateAnimKeys 0x800E82B8, verbatim.
 * A fighter whose attributes carry translate_scales (Luigi,
 * relocData/221_LuigiMain.c:194: Mario's animations on his taller limbs)
 * plays its translation tracks scaled per joint, unless the motion's
 * FTAnimDesc bit turns the scaling off for it (ftMainSetStatus's
 * toggle); a motion_id of -2 replays the joints that have an animation
 * at AOBJ_ANIM_END. The port took only the unscaled motion_id != -2
 * branch until then. The parser is picked per motion: a figatree's
 * AObjEvent16 script through ftAnimParseDObjFigatree, an AnimJoint's
 * AObjEvent32 script -- every Appear is one -- through
 * gcParseDObjAnimJoint. The material walks play the MObjs a part hangs
 * with its own script (fighter.h FPackMObjs): Samus's grapple beam, the
 * only VS ones, whose tiles flicker. */
void ftParamUpdateAnimKeys(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    DObj **p_joint = &fp->joints[nFTPartsJointTopN];
    DObj *joint;
    MObj *mobj;
    FTParts *parts;
    f32 anim_wait_bak;
    s32 i;

    if (fp->motion_id != -2)
    {
        if (fp->is_have_translate_scale)
        {
            Vec3f *translate_scales = fp->attr->translate_scales;

            for (i = 0; i < ARRAY_COUNT(fp->joints); i++, p_joint++, translate_scales++)
            {
                joint = *p_joint;

                if (joint != NULL)
                {
                    if (fp->anim_desc.flags.is_anim_joint)
                    {
                        gcParseDObjAnimJoint(joint);
                    }
                    else ftAnimParseDObjFigatree(joint);
                    
                    lbCommonPlayTranslateScaledDObjAnim(joint, translate_scales);

                    mobj = joint->mobj;

                    while (mobj != NULL)
                    {
                        gcParseMObjMatAnimJoint(mobj);
                        gcPlayMObjMatAnim(mobj);

                        mobj = mobj->next;
                    }
                }
            }
        }
        else for (i = 0; i < ARRAY_COUNT(fp->joints); i++, p_joint++)
        {
            joint = *p_joint;

            if (joint != NULL)
            {
                if (fp->anim_desc.flags.is_anim_joint)
                {
                    gcParseDObjAnimJoint(joint);
                }
                else ftAnimParseDObjFigatree(joint);
                
                gcPlayDObjAnimJoint(joint);

                mobj = joint->mobj;

                while (mobj != NULL)
                {
                    gcParseMObjMatAnimJoint(mobj);
                    gcPlayMObjMatAnim(mobj);

                    mobj = mobj->next;
                }
            }
        }
    }
    else if (fp->is_have_translate_scale)
    {
        Vec3f *translate_scales = fp->attr->translate_scales;

        for (i = 0; i < ARRAY_COUNT(fp->joints); i++, p_joint++, translate_scales++)
        {
            joint = *p_joint;

            if (joint != NULL)
            {
                parts = ftGetParts(joint);

                if ((parts != NULL) && (parts->is_have_anim != FALSE))
                {
                    anim_wait_bak = joint->anim_wait;

                    joint->anim_wait = AOBJ_ANIM_END;

                    lbCommonPlayTranslateScaledDObjAnim(joint, translate_scales);

                    joint->anim_wait = anim_wait_bak;
                }
            }
        }
    }
    else for (i = 0; i < ARRAY_COUNT(fp->joints); i++, p_joint++)
    {
        joint = *p_joint;

        if (joint != NULL)
        {
            parts = ftGetParts(joint);

            if ((parts != NULL) && (parts->is_have_anim != FALSE))
            {
                anim_wait_bak = joint->anim_wait;

                joint->anim_wait = AOBJ_ANIM_END;

                gcPlayDObjAnimJoint(joint);

                joint->anim_wait = anim_wait_bak;
            }
        }
    }
}

/* ---- the hidden parts -------------------
 *
 * TransN, XRotN and YRotN are not entries of the model: they are rows of
 * FTAttributes.hiddenparts, DObjs the game makes and links into the tree
 * when an animation's FTAnimDesc bit asks for one and ejects when the
 * next does not (ft/ftmain.c:4636-4652, the loop in ftMainSetStatus
 * below). The rows come out of the pack (fighter.h FPackAttr), and the
 * three functions are the game's. */

/* ft/ftmain.c:4084-4200 ftMainUpdateHiddenPartID 0x800E69C4. Verbatim but
 * for what is marked DIVERGES:
 * - the display list of a root at or past CommonStart is the pack
 * joint's payload (dc_model_hidden_payload), NULL where the entry
 * bakes no geometry, as the game's is NULL where the DObjDesc has no
 * dl. The detail pick between the high and low trees is gone: the
 * pack carries the high tree.
 * - lbCommonAddMObjForFighterPartsDObj: a root's own MObjs are costume
 * MObjs, which the pack bakes (fighter.h FPackCostumes), so there is
 * nothing to hang; a part's animated ones come with the part
 * (src/dc/ftparam.c ftParamGetPartMObjs).
 * - parts->flags is 0, as ftManagerMakeFighter leaves every part's.
 * - lbCommonInitDObj(root, 0x4B, 0, 0, ...): the walk here composes
 * Tra, RotRpyR and Sca as three XObjs (objdisplay.c gcDObjLocalMatrix
 * has no TraRotRpyRSca), the same product in the same order. */
void ftMainUpdateHiddenPartID(FTStruct *fp, s32 hiddenpart_id)
{
    FTHiddenPart *hiddenpart;
    void *dl;
    DObj *root_joint;
    DObj *child_joint;
    DObj *sibling_joint;
    FTAttributes *attr;
    DObj *parent_joint;
    FTParts *parts;

    attr = fp->attr;
    hiddenpart = &attr->hiddenparts[hiddenpart_id];

    if (hiddenpart->root_joint_id >= nFTPartsJointCommonStart)
    {
        dl = dc_model_hidden_payload(dc_model_of(fp->fighter_gobj),
                                     hiddenpart->root_joint_id - nFTPartsJointCommonStart);
    }
    else dl = NULL;

    root_joint = gcAddDObjForGObj(fp->fighter_gobj, dl);
    root_joint->sib_prev->sib_next = NULL;
    root_joint->sib_prev = NULL;

    if (hiddenpart->root_joint_id >= nFTPartsJointCommonStart)
    {
        fp->modelpart_status[hiddenpart->root_joint_id - nFTPartsJointCommonStart].modelpart_id_base = fp->modelpart_status[hiddenpart->root_joint_id - nFTPartsJointCommonStart].modelpart_id_curr = (dl != NULL) ? 0 : -1;
    }
    parent_joint = fp->joints[hiddenpart->parent_joint_id];

    switch (hiddenpart->joint_kind)
    {
    case 0:
        if (parent_joint->child != NULL)
        {
            sibling_joint = parent_joint->child->sib_next;
            child_joint = parent_joint->child;

            while (sibling_joint != NULL)
            {
                child_joint = sibling_joint;
                sibling_joint = sibling_joint->sib_next;
            }
            child_joint->sib_next = root_joint;
            root_joint->sib_prev = child_joint;
        }
        else parent_joint->child = root_joint;
        root_joint->parent = parent_joint;
        break;

    case 1:
        if (parent_joint->child != NULL)
        {
            child_joint = parent_joint->child;
            child_joint->sib_prev = root_joint;
            root_joint->sib_next = child_joint;
        }
        parent_joint->child = root_joint;
        root_joint->parent = parent_joint;
        break;

    case 2:
        child_joint = parent_joint->child->sib_next;
        child_joint->sib_prev = root_joint;
        root_joint->sib_next = child_joint;
        child_joint = parent_joint->child;
        child_joint->sib_next = root_joint;
        root_joint->parent = parent_joint;
        root_joint->sib_prev = child_joint;
        break;

    case 3:
        if (parent_joint->child != NULL)
        {
            child_joint = parent_joint->child;
            root_joint->child = child_joint;

            do
            {
                child_joint->parent = root_joint;
                child_joint = child_joint->sib_next;
            }
            while (child_joint != NULL);
        }
        parent_joint->child = root_joint;

        root_joint->parent = parent_joint;
        break;
    }
    fp->joints[hiddenpart->root_joint_id] = root_joint;

    root_joint->user_data.p = parts = ftManagerGetNextPartsAlloc();

    parts->flags = 0;
    parts->joint_id = hiddenpart->root_joint_id;

    if (hiddenpart->partindex_0x8 != 0)
    {
        lbCommonInitDObj(root_joint, nGCMatrixKindTra, nGCMatrixKindRotRpyR, nGCMatrixKindSca, fp->unk_ft_0x149);
    }
}

/* ft/ftmain.c:4202-4296 ftMainAddHiddenPartID 0x800E6CE0, verbatim: a
 * TransN the last animation left unlinked (the tail of ftMainSetStatus)
 * goes back above its parent's children before the next attach. */
void ftMainAddHiddenPartID(FTStruct *fp, s32 hiddenpart_id)
{
    DObj *new_parent_joint;
    DObj *sibling_joint;
    FTHiddenPart *hiddenpart;
    DObj *root_joint;
    DObj *child_joint;
    DObj *new_child_joint;
    DObj *parent_joint;

    hiddenpart = &fp->attr->hiddenparts[hiddenpart_id];
    root_joint = fp->joints[hiddenpart->root_joint_id];

    if (hiddenpart->root_joint_id == nFTPartsJointTransN)
    {
        parent_joint = root_joint->parent;
        child_joint = root_joint->child;
        new_parent_joint = parent_joint;
        new_child_joint = child_joint;

        if (new_child_joint != NULL)
        {
            sibling_joint = root_joint->sib_prev;

            new_child_joint = child_joint;
            child_joint = sibling_joint;

            if (sibling_joint == NULL)
            {
                new_parent_joint->child = new_child_joint;
            }
            else
            {
                child_joint->sib_next = new_child_joint;
                new_child_joint->sib_prev = root_joint->sib_prev;
            }
            child_joint = new_child_joint->sib_next;
            new_child_joint->parent = new_parent_joint;

            while (child_joint != NULL)
            {
                new_child_joint = child_joint;
                child_joint->parent = new_parent_joint;
                child_joint = child_joint->sib_next;
            }

            new_child_joint->sib_next = root_joint->sib_next;
            new_parent_joint = root_joint->sib_next;

            if (new_parent_joint != NULL)
            {
                new_parent_joint->sib_prev = new_child_joint;
            }
        }
        else
        {
            sibling_joint = root_joint->sib_prev;

            if (sibling_joint == NULL)
            {
                new_parent_joint->child = root_joint->sib_next;
            }
            else sibling_joint->sib_next = root_joint->sib_next;

            new_parent_joint = root_joint->sib_next;

            if (new_parent_joint != NULL)
            {
                new_parent_joint->sib_prev = root_joint->sib_prev;
            }
        }
        root_joint->child = NULL;
        root_joint->sib_prev = NULL;
        root_joint->sib_next = NULL;
        root_joint->parent = NULL;

        new_parent_joint = fp->joints[hiddenpart->parent_joint_id];

        if (new_parent_joint->child != NULL)
        {
            new_child_joint = new_parent_joint->child;
            root_joint->child = new_parent_joint->child;

            do
            {
                new_child_joint->parent = root_joint;
                new_child_joint = new_child_joint->sib_next;
            }
            while (new_child_joint != NULL);
        }
        new_parent_joint->child = root_joint;
        root_joint->parent = new_parent_joint;
    }
}

/* ft/ftmain.c:4298-4362 ftMainEjectHiddenPartID 0x800E6E00, verbatim */
void ftMainEjectHiddenPartID(FTStruct *fp, s32 hiddenpart_id)
{
    FTHiddenPart *hiddenpart = &fp->attr->hiddenparts[hiddenpart_id];
    DObj *root_joint = fp->joints[hiddenpart->root_joint_id];
    DObj *parent_joint;
    DObj *child_joint;
    DObj *sibling_joint;
    DObj *new_sibling_joint;

    ftManagerSetPrevPartsAlloc(ftGetParts(root_joint));

    child_joint = root_joint->child;
    parent_joint = root_joint->parent;

    if (child_joint != NULL)
    {
        sibling_joint = child_joint;

        if (root_joint->sib_prev == NULL)
        {
            parent_joint->child = child_joint;
        }
        else
        {
            root_joint->sib_prev->sib_next = sibling_joint;
            sibling_joint->sib_prev = (child_joint = root_joint)->sib_prev;
        }
        new_sibling_joint = sibling_joint->sib_next;
        sibling_joint->parent = parent_joint;

        while (new_sibling_joint != NULL)
        {
            sibling_joint = new_sibling_joint;
            new_sibling_joint->parent = parent_joint;
            new_sibling_joint = new_sibling_joint->sib_next;
        }
        sibling_joint->sib_next = root_joint->sib_next;
        new_sibling_joint = root_joint->sib_next;

        if (new_sibling_joint != NULL)
        {
            new_sibling_joint->sib_prev = sibling_joint;
        }
    }
    else
    {
        if (root_joint->sib_prev == NULL)
        {
            parent_joint->child = root_joint->sib_next;
        }
        else root_joint->sib_prev->sib_next = root_joint->sib_next;

        if (root_joint->sib_next != NULL)
        {
            root_joint->sib_next->sib_prev = root_joint->sib_prev;
        }
    }
    fp->joints[hiddenpart->root_joint_id] = NULL;
    root_joint->sib_next = NULL;
    root_joint->sib_prev = NULL;
    root_joint->child = NULL;

    gcEjectDObj(root_joint);
}

/* The figatree table as lbCommonAddFighterPartsFigatree deals it: a
 * pointer per slot, in the file's order.
 *
 * DIVERGES: the game copies the animation file into the figatree heap
 * (ft/ftmain.c:4622 lbRelocGetForceExternHeapFile) and the file's
 * relocated header is the table. The pack keeps the file in place --
 * its words in the fighter's .anm, read with the pack (fighter.h
 * FPackAnm) -- and its header as word indices (fighter.h
 * FPackAnimSlots), which this turns into pointers on the caller's stack
 * for the one attach. The
 * game reads one entry per DObj of the walk and its file has exactly
 * that many; a tree with more DObjs than the table has slots would read
 * past the header there, and gets AOBJ_ANIM_NULL here.
 * ftMainNoteFigatreeSlots below says so the first time the two differ,
 * either way, so the assumption is checked rather than trusted. */
static void ftMainSetupFigatree(GObj *fighter_gobj, const FPackAnim *anim,
                                const FPackAnimSlots *slots, void **figatree)
{
    Fighter *model = dc_model_of(fighter_gobj);
    const u8 *blob = model->blob;
    const s32 *table = (const s32 *)(blob + slots->off);
    /* in the pack, or in the .anm beside it (fighter.h FPackAnm) -- read
 * now if the tier the scene loaded does not hold it */
    const u8 *words = fighter_anim_words_need(model, anim);
    u32 k;

    for (k = 0; k < FTPARTS_JOINT_NUM_MAX; k++)
    {
        s32 w = (k < slots->count) ? table[k] : -1;

        if ((w < 0) || (words == NULL))
        {
            figatree[k] = NULL;
        }
        else if (anim->kind == FPACK_ANIM_ANIMJOINT)
        {
            figatree[k] = (AObjEvent32 *)words + w;
        }
        else
        {
            figatree[k] = (u16 *)words + w;
        }
    }
}

#ifdef DB_ANIM_USE
/* -DDB_ANIM_USE (src/dc/db.h): each animation a fighter binds, once per
 * scene and pack, as the table and the motion id that named it -- what
 * tools/export/anm_tiers.py reads to decide which of the .anm's tiers
 * (fighter.h FPackAttr.anm_tier_end) an animation belongs in. */
#define FT_ANIMUSE_PACKS 16
#define FT_ANIMUSE_BITS 1024
static void ftMainNoteAnimUse(const Fighter *model, sb32 is_sub, s32 motion_id,
                              s32 anim)
{
    static const void *packs[FT_ANIMUSE_PACKS];
    static u32 seen[FT_ANIMUSE_PACKS][2][FT_ANIMUSE_BITS / 32];
    static s32 scene = -1;
    const void *hd = (model->anm_src != NULL) ? model->anm_src->hd : model->hd;
    s32 i;

    if (scene != gSCManagerSceneData.scene_curr)
    {
        scene = gSCManagerSceneData.scene_curr;
        memset(packs, 0, sizeof(packs));
        memset(seen, 0, sizeof(seen));
    }
    for (i = 0; i < FT_ANIMUSE_PACKS; i++)
    {
        if (packs[i] == hd || packs[i] == NULL)
        {
            packs[i] = hd;
            break;
        }
    }
    if (i == FT_ANIMUSE_PACKS || motion_id < 0 || motion_id >= FT_ANIMUSE_BITS)
    {
        syDebugPrintf("animuse: %d %.8s %s %d %d %s (not deduplicated)\n",
                      (int)scene, model->hd->name, is_sub ? "sub" : "main",
                      (int)motion_id, (int)anim, model->anims[anim].name);
        return;
    }
    if (seen[i][is_sub][motion_id / 32] & (1u << (motion_id % 32)))
    {
        return;
    }
    seen[i][is_sub][motion_id / 32] |= 1u << (motion_id % 32);
    syDebugPrintf("animuse: %d %.8s %s %d %d %s\n", (int)scene,
                  model->hd->name, is_sub ? "sub" : "main", (int)motion_id,
                  (int)anim, model->anims[anim].name);
}
#endif

/* The tree the attach is about to walk, hidden parts linked, against the
 * table's length: the game's file has one slot per DObj, and a run that
 * finds otherwise says so once (see ftMainSetupFigatree). */
static void ftMainNoteFigatreeSlots(GObj *fighter_gobj, const FPackAnim *anim,
                                    const FPackAnimSlots *slots)
{
    static sb32 noted = FALSE;
    FTStruct *fp = ftGetStruct(fighter_gobj);
    DObj *dobj;
    u32 dobjs = 0;

    if (noted != FALSE)
    {
        return;
    }
    for (dobj = fp->joints[nFTPartsJointTopN]->child; dobj != NULL;
         dobj = lbCommonGetTreeDObjNextFromRoot(dobj, fp->joints[nFTPartsJointTopN]->child))
    {
        dobjs++;
    }
    if (dobjs != slots->count)
    {
        noted = TRUE;
        syDebugPrintf("ft : %s %s has %u slots for a tree of %u DObjs\n",
                      dc_model_of(fighter_gobj)->hd->name, anim->name,
                      (unsigned)slots->count, (unsigned)dobjs);
    }
}

/* ---- ft/ftmain.c:4365-4823 ftMainSetStatus 0x80149E70, the slice this
 * port needs: reset the per-status flags, pick the status table, switch
 * status, start the motion table's animation, install the status's four
 * callbacks. ------------------------------------------------------------ */

/* The port's: a row the port's designated initializers skipped is all
 * zero, and no row in the game's tables is (a zero motion, zero flags and
 * four NULL procs). Such a row used to lock the fighter in place with no
 * procs at all; treated as absent, it aborts and names its status. */
static sb32 ftMainStatusDescIsUnwritten(const FTStatusDesc *desc)
{
    return (desc->mflags.motion_id == 0) && (desc->mflags.attack_id == 0) &&
           (desc->sflags.halfword == 0) && (desc->proc_update == NULL) &&
           (desc->proc_interrupt == NULL) && (desc->proc_physics == NULL) &&
           (desc->proc_map == NULL);
}

/* ft/ftmain.c:4553-4560: the two OPENING bands, which a demo status
 * lands in. Above FTSTAT_OPENING1_START the kind's own
 * table answers, above FTSTAT_OPENING2_START the common one; a row is a
 * motion id counted in the SubMotion table and the one proc a demo
 * fighter runs. NULL for every other status, which is what tells
 * ftMainSetStatus to read a status table instead.
 *
 * The decomp indexes both tables unguarded, and neither carries a row
 * count, so a status past the end of a kind's table would read whatever
 * follows it. The port can bound it for free: every row's motion id IS
 * its own status id (row k of the common table is 0x10000+k, row 0 of
 * every per-kind table is FTSTAT_OPENING1_START), so a row that does
 * not name the status asked for is not that status's row. That refuses
 * the sentinel a kind with no opening statuses of its own carries
 * (motion id 0xFFFFFFFF, scsubsysdata.c) and an overrun into the next
 * table alike -- Mario's four rows are followed by Fox's, whose first
 * row reads 0x1000F and would answer for 0x10013 without it. */
static const FTOpeningDesc *ftMainGetOpeningDesc(FTStruct *fp, s32 status_id)
{
    const FTOpeningDesc *table;
    s32 i;

    if (status_id >= FTSTAT_CHARDATA_START)
    {
        status_id -= FTSTAT_CHARDATA_START;
    }
    if (status_id >= FTSTAT_OPENING1_START)
    {
        /* D_ovl1_80390D20 has one entry per FTKind and a NULL after
 * them; <ft/fighter.h> declares it without a length, so the
 * enum is the bound. */
        if ((u32)fp->fkind >= nFTKindEnumCount)
        {
            return NULL;
        }
        table = D_ovl1_80390D20[fp->fkind];
        i = status_id - FTSTAT_OPENING1_START;
    }
    else if (status_id >= FTSTAT_OPENING2_START)
    {
        table = &D_ovl1_80390BE8;
        i = status_id - FTSTAT_OPENING2_START;
    }
    else return NULL;

    if ((table == NULL) || (i < 0) || (table[i].motion_id != status_id))
    {
        return NULL;
    }
    return &table[i];
}

/* ft/ftmain.c:4549-4576: which table a status id indexes. The game has
 * five (null, action, per-fighter special, two opening tables); the port
 * carries all five. The opening pair works through ftMainGetOpeningDesc
 * above, which ftMainSetStatus asks first. The ft/ftmain.c:4550-4552
 * strip resolves a demo fighter's FTSTATUS_CHARACTERS_DEMO-encoded status
 * id back to the same row a battle fighter's would use. The special table
 * is per fighter, indexed through dFTMainSpecialStatusDescs above.
 * tools/check/status_check.py holds every row to the game's. */
static FTStatusDesc *ftMainGetStatusDesc(FTStruct *fp, s32 status_id)
{
    if (status_id >= FTSTAT_CHARDATA_START)
    {
        status_id -= FTSTAT_CHARDATA_START;
    }
    if (status_id >= nFTCommonStatusSpecialStart)
    {
        s32 i = status_id - nFTCommonStatusSpecialStart;
        const FTSpecialTable *table;

        if ((u32)fp->fkind >= ARRAY_COUNT(dFTMainSpecialStatusDescs))
        {
            return NULL;
        }
        table = &dFTMainSpecialStatusDescs[fp->fkind];

        if (table->rows != NULL && i < table->count &&
            !ftMainStatusDescIsUnwritten(&table->rows[i]))
        {
            return &table->rows[i];
        }
        return NULL;
    }
    if (status_id >= nFTCommonStatusActionStart)
    {
        s32 i = status_id - nFTCommonStatusActionStart;

        if (i < (s32)ARRAY_COUNT(dFTCommonActionStatusDescs) &&
            !ftMainStatusDescIsUnwritten(&dFTCommonActionStatusDescs[i]))
        {
            return &dFTCommonActionStatusDescs[i];
        }
        return NULL;
    }
    if (ftMainStatusDescIsUnwritten(&dFTCommonNullStatusDescs[status_id]))
    {
        return NULL;
    }
    return &dFTCommonNullStatusDescs[status_id];
}

/* ft/ftmain.c:4365-4823 ftMainSetStatus 0x80149E70. The game's order:
 * the hidden-part walk, the DObjDesc bind-pose reset (over the
 * pack's FPackJoint array), the three named joints' resets, the figatree
 * walked down the tree and TransN unlinked. The animation attach happens
 * alongside. DIVERGES, in order of appearance: the Demo kind and the
 * two opening tables; ftParamSetModelPartDetailAll (parts); the
 * figatree heap copy (4622: the pack plays in place, ftMainSetupFigatree
 * makes the table); the shield-pose arm (4617-4620: no motion row in
 * ft/ftdata.c carries FTANIM_FLAG_SHIELDPOSE, so the arm is dead) --
 * the ShieldPose file holds the guard's tables, which the pack carries,
 * and no figatree; the translate scales. The script start (4747-4800) is
 * the game's, reading the pack's copy of the MainMotion file. */
void ftMainSetStatus(GObj *fighter_gobj, s32 status_id, f32 frame_begin, f32 anim_speed, u32 flags)
{
#ifdef FT_TRACE_KO
    extern u32 dSYTaskmanUpdateCount;       /* sys/taskman.h */
    if (status_id == nFTCommonStatusDeadDown)
    {
        syDebugPrintf("ko : DeadDown set from %p at tic %lu\n",
                      __builtin_return_address(0), (unsigned long)dSYTaskmanUpdateCount);
    }
#endif
    FTStruct *fp = ftGetStruct(fighter_gobj);
    FTStatusDesc *status_desc;
    const FTOpeningDesc *opening_desc;  /* ft/ftmain.c's opening_struct */
    const FPackMotion *motions;         /* its script_array: the pack's */
    u32 motion_count;
    GMStatFlags status_flags;
    GMStatFlags attack_flags;
    void *event_script_ptr;
    f32 anim_frame;
    s32 i;

    status_flags = fp->stat_flags;

    if (fp->is_events_forward)
    {
        ftMainUpdateMotionEventsForwardEffect(fighter_gobj);

        if (fp->proc_accessory != NULL)
        {
            fp->proc_accessory(fighter_gobj);
        }
    }
    fp->status_id = status_id;

    if (!(flags & FTSTATUS_PRESERVE_HIT) && (fp->is_attack_active))
    {
        ftParamClearAttackCollAll(fighter_gobj);
    }
    if (!(flags & FTSTATUS_PRESERVE_THROWPOINTER))
    {
        fp->throw_gobj = NULL;
    }
    if (!(flags & FTSTATUS_PRESERVE_HITSTATUS))
    {
        if (fp->is_hitstatus_nodamage)
        {
            ftParamSetHitStatusPartAll(fighter_gobj, nGMHitStatusNormal);
        }
        if (fp->hitstatus != nGMHitStatusNormal)
        {
            ftParamSetHitStatusAll(fighter_gobj, nGMHitStatusNormal);
        }
    }
    if (fp->is_damage_coll_modify)
    {
        ftParamResetFighterDamageCollsAll(fighter_gobj);
    }
    if (!(flags & FTSTATUS_PRESERVE_MODELPART) && (fp->is_modelpart_modify))
    {
        ftParamResetModelPartAll(fighter_gobj);
    }
    if (!(flags & FTSTATUS_PRESERVE_TEXTUREPART) && (fp->is_texturepart_modify))
    {
        ftParamResetTexturePartAll(fighter_gobj);
    }
    if (!(flags & FTSTATUS_PRESERVE_COLANIM) && (dGMColScriptsDescs[fp->colanim.colanim_id].is_unlocked != FALSE))
    {
        ftParamResetStatUpdateColAnim(fighter_gobj);
    }
    if (!(flags & FTSTATUS_PRESERVE_EFFECT) && (fp->is_effect_attach))
    {
        ftParamProcStopEffect(fighter_gobj);
    }
    fp->is_reflect = FALSE;
    fp->is_absorb = FALSE;
    fp->is_shield = FALSE;

    if (!(flags & FTSTATUS_PRESERVE_FASTFALL))
    {
        fp->is_fastfall = FALSE;
    }
    fp->is_invisible = FALSE;
    fp->is_shadow_hide = FALSE;
    fp->is_rebirth = FALSE;
    fp->is_playertag_hide = FALSE;
    fp->is_effect_skip = FALSE;

    if (fp->pkind != nFTPlayerKindDemo)
    {
        gmRumbleStopRumbleID(fp->player, 2);
        gmRumbleStopRumbleID(fp->player, 3);

        if (!(flags & FTSTATUS_PRESERVE_RUMBLE))
        {
            gmRumbleStopRumbleID(fp->player, 7);
        }
        fp->joints[nFTPartsJointTopN]->rotate.vec.f.y = fp->lr * F_CLC_DTOR32(90.0F);

        DObjGetStruct(fighter_gobj)->rotate.vec.f.z = 0.0F;

        fp->joints[nFTPartsJointTopN]->rotate.vec.f.x = DObjGetStruct(fighter_gobj)->rotate.vec.f.z;

        fp->physics.vel_air.z = 0.0F;
        fp->physics.vel_ground.z = 0.0F;
    }
    fp->is_jostle_ignore = FALSE;
    fp->is_hitstun = FALSE;

    fp->damage_mul = 1.0F;

    if ((fp->ga == nMPKineticsGround) && !(flags & FTSTATUS_PRESERVE_DAMAGEPLAYER))
    {
        fp->damage_player = -1;
    }
    if (!(flags & FTSTATUS_PRESERVE_SLOPECONTOUR))
    {
        fp->slope_contour = 0;
    }
    fp->coll_data.ignore_line_id = -1;

    fp->is_item_show = TRUE;
    fp->is_cliff_hold = FALSE;

    ftParamSetCaptureImmuneMask(fp, FTCATCHKIND_MASK_NONE);

    fp->is_ghost = FALSE;
    fp->is_damage_resist = FALSE;
    fp->is_menu_ignore = FALSE;

    if (fp->camera_mode != nFTCameraModeEntry)
    {
        fp->camera_mode = nFTCameraModeDefault;
    }
    fp->camera_zoom_range = 1.0F;

    if (!(flags & FTSTATUS_PRESERVE_PLAYERTAG))
    {
        fp->playertag_wait = 0;
    }
    fp->is_special_interrupt = FALSE;
    fp->is_catchstatus = FALSE;
    fp->is_ignore_dead = FALSE;

    if (!(flags & FTSTATUS_PRESERVE_SHUFFLETIME))
    {
        fp->shuffle_tics = 0;
    }
    if (!(flags & FTSTATUS_PRESERVE_LOOPSFX))
    {
        ftParamStopLoopSFX(fp);
    }
    fp->knockback_resist_status = 0.0F;
    fp->damage_knockback_stack = 0.0F;

    if (!(flags & FTSTATUS_PRESERVE_AFTERIMAGE))
    {
        fp->afterimage.drawstatus = -1;
    }
    if ((status_id != nFTCommonStatusWait) && (status_id != nFTCommonStatusWalkSlow) && (status_id != nFTCommonStatusWalkMiddle) && (status_id != nFTCommonStatusWalkFast))
    {
        fp->attack1_followup_frames = 0.0F;
    }
    if ((fp->pkind != nFTPlayerKindDemo) && (fp->dl_link != FTDISPLAY_DLLINK_DEFAULT))
    {
        ftParamMoveDLLink(fighter_gobj, 9);
    }
    fp->status_total_tics = 0;

    /* ft/ftmain.c:4549-4577. The two opening bands answer first: when
 * one does, there is no FTStatusDesc at all and the whole
 * status-table half of this function is skipped, which is the
 * game's `status_struct == NULL`. */
    opening_desc = ftMainGetOpeningDesc(fp, status_id);
    status_desc = opening_desc != NULL ? NULL
                                       : ftMainGetStatusDesc(fp, status_id);

    if (status_desc == NULL && opening_desc == NULL)
    {
        syDebugPrintf("ft : status %d has no table row in this port\n",
                      status_id);
        scManagerRunPrintGObjStatus();
    }
    if (status_desc != NULL && fp->pkind != nFTPlayerKindDemo)
    {
        if ((status_desc->mflags.attack_id == nFTMotionAttackIDNone) || (status_desc->mflags.attack_id != fp->motion_attack_id))
        {
            ftParamSetMotionID(fp, status_desc->mflags.attack_id);
        }
        attack_flags = status_desc->sflags;

        if ((attack_flags.attack_id == nFTStatusAttackIDNone) || (attack_flags.attack_id != fp->stat_flags.attack_id))
        {
            ftParamSetStatUpdate(fp, status_desc->sflags.halfword);
        }
    }
    if (fp->proc_status != NULL)
    {
        fp->proc_status(fighter_gobj);
        fp->proc_status = NULL;
    }
    ftParamUpdate1PGameAttackStats(fp, status_flags.halfword);

    /* ft/ftmain.c:4600-4611: an opening row's motion id counts in the
 * SubMotion table (src/dc/fighter.h FPackAttr.off_submotion), a
 * status row's in the MainMotion one. */
    if (opening_desc != NULL)
    {
        fp->motion_id = opening_desc->motion_id - FTSTAT_OPENING2_START;
        motions = dc_model_of(fighter_gobj)->submotions;
        motion_count = dc_model_of(fighter_gobj)->submotion_count;
    }
    else
    {
        fp->motion_id = status_desc->mflags.motion_id;
        motions = dc_model_of(fighter_gobj)->motions;
        motion_count = dc_model_of(fighter_gobj)->motion_count;
    }

    if (fp->motion_id >= 0 && motions != NULL &&
        (u32)fp->motion_id < motion_count)
    {
        const FPackMotion *m = &motions[fp->motion_id];

        if (m->anim >= 0 && (u32)m->anim < dc_model_of(fighter_gobj)->hd->anim_count)
        {
            Fighter *model = dc_model_of(fighter_gobj);
            void *figatree[FTPARTS_JOINT_NUM_MAX];
            u32 anim_desc_bak;
            u32 anim_desc_update;
            DObj *joint;
            DObj *transn_parent;
            DObj *transn_child;

            /* ft/ftmain.c:4622-4626: the animation file, its table of
 * scripts made pointers (ftMainSetupFigatree above) */
            ftMainSetupFigatree(fighter_gobj, &model->anims[m->anim],
                                &model->slots[m->anim], figatree);
#ifdef DB_ANIM_USE
            ftMainNoteAnimUse(model, opening_desc != NULL, fp->motion_id,
                              m->anim);
#endif

            /* ft/ftmain.c:4629-4651: which hidden parts the last
 * animation had and this one wants, bit by bit from 31 down
 * -- made, re-added or ejected. The pack's flags word is
 * the ROM's FTMotionDesc.anim_desc word. */
            anim_desc_bak =
            fp->anim_desc.word & ~(FTANIM_FLAG_SUBMOTION_SCRIPT | FTANIM_FLAG_ANIMJOINT | FTANIM_FLAG_TRANSLATE_SCALES | FTANIM_FLAG_SHIELDPOSE | FTANIM_FLAG_ANIMLOCKS);

            fp->anim_desc.word = m->flags;

            anim_desc_update =
            fp->anim_desc.word & ~(FTANIM_FLAG_SUBMOTION_SCRIPT | FTANIM_FLAG_ANIMJOINT | FTANIM_FLAG_TRANSLATE_SCALES | FTANIM_FLAG_SHIELDPOSE | FTANIM_FLAG_ANIMLOCKS);

            for (i = 0; ((anim_desc_bak != 0) || (anim_desc_update != 0)); i++, anim_desc_update <<= 1, anim_desc_bak <<= 1)
            {
                if (!(anim_desc_bak & (1 << 31)))
                {
                    if (anim_desc_update & (1 << 31))
                    {
                        ftMainUpdateHiddenPartID(fp, i);
                    }
                }
                else if (anim_desc_update & (1 << 31))
                {
                    ftMainAddHiddenPartID(fp, i);
                }
                else ftMainEjectHiddenPartID(fp, i);
            }
            /* ft/ftmain.c:4652-4666: every part back to the pose its
 * DObjDesc gave it. The pack's FPackJoint array is that
 * list, entry k at joints[CommonStart + k]. */
            for (i = 0; i < (s32)model->hd->joint_count; i++)
            {
                const FPackJoint *pj = &model->joints[i];

                joint = fp->joints[nFTPartsJointCommonStart + i];

#ifdef DB_JOINT_TRACE
                if (joint != NULL && ((u32)joint < 0x8C000000u || (u32)joint >= 0x8D000000u))
                {
                    int k;

                    dbglog(DBG_WARNING, "ft: bad joint slot %d = %08lx (kind %d, motion %d, model joints %d, status %d, pkind %d)\n",
                           (int)(nFTPartsJointCommonStart + i), (unsigned long)joint,
                           (int)fp->fkind, (int)fp->motion_id, (int)model->hd->joint_count,
                           (int)fp->status_id, (int)fp->pkind);
                    for (k = 0; k < (int)ARRAY_COUNT(fp->joints); k++)
                        dbglog(DBG_WARNING, "ft:   joints[%d] = %08lx\n", k,
                               (unsigned long)fp->joints[k]);
                    joint = NULL;
                }
#endif
                if (joint != NULL)
                {
                    joint->translate.vec.f.x = pj->t[0];
                    joint->translate.vec.f.y = pj->t[1];
                    joint->translate.vec.f.z = pj->t[2];

                    joint->rotate.vec.f.x = pj->r[0];
                    joint->rotate.vec.f.y = pj->r[1];
                    joint->rotate.vec.f.z = pj->r[2];

                    joint->scale.vec.f.x = pj->s[0];
                    joint->scale.vec.f.y = pj->s[1];
                    joint->scale.vec.f.z = pj->s[2];

                    joint->flags = DOBJ_FLAG_NONE;
                }
            }
            /* ft/ftmain.c:4667-4693 */
            if (fp->anim_desc.flags.is_use_transn_joint)
            {
                joint = fp->joints[nFTPartsJointTransN];

                joint->translate.vec.f.x = joint->translate.vec.f.y = joint->translate.vec.f.z = 0.0F;

                joint->rotate.vec.f.z = 0.0F;

                joint->flags = DOBJ_FLAG_NONE;
            }
            if (fp->anim_desc.flags.is_use_xrotn_joint)
            {
                joint = fp->joints[nFTPartsJointXRotN];

                joint->translate.vec.f.x = joint->translate.vec.f.y = joint->translate.vec.f.z = 0.0F;

                joint->rotate.vec.f.x = joint->rotate.vec.f.y = joint->rotate.vec.f.z = 0.0F;

                joint->scale.vec.f.x = joint->scale.vec.f.y = joint->scale.vec.f.z = 1.0F;

                joint->flags = DOBJ_FLAG_NONE;
            }
            if (fp->anim_desc.flags.is_use_yrotn_joint)
            {
                joint = fp->joints[nFTPartsJointYRotN];

                joint->translate.vec.f.x = joint->translate.vec.f.y = joint->translate.vec.f.z = 0.0F;

                joint->rotate.vec.f.x = joint->rotate.vec.f.y = joint->rotate.vec.f.z = 0.0F;

                joint->scale.vec.f.x = joint->scale.vec.f.y = joint->scale.vec.f.z = 1.0F;

                joint->flags = DOBJ_FLAG_NONE;
            }
            /* ft/ftmain.c:4694-4721 */
            ftMainNoteFigatreeSlots(fighter_gobj, &model->anims[m->anim],
                                    &model->slots[m->anim]);
            lbCommonAddFighterPartsFigatree(fp->joints[nFTPartsJointTopN]->child, figatree, frame_begin);

            if (anim_speed != DObjGetStruct(fighter_gobj)->anim_speed)
            {
                gcSetAnimSpeed(fighter_gobj, anim_speed);
            }
            if (fp->anim_desc.flags.is_use_transn_joint)
            {
                joint = fp->joints[nFTPartsJointTransN];

                transn_parent = joint->parent;
                transn_child = joint->child;
                transn_parent->child = transn_child;
                transn_child->parent = transn_parent;
                transn_child->sib_next = joint;
                joint->sib_prev = transn_child;
                joint->parent = transn_child->parent;
                joint->child = NULL;
            }
            /* ft/ftmain.c:4724-4733 */
            if (fp->is_use_animlocks)
            {
                if (!(fp->anim_desc.flags.is_use_animlocks))
                {
                    ftParamSetAnimLocks(fp);
                }
            }
            else if (fp->anim_desc.flags.is_use_animlocks)
            {
                ftParamClearAnimLocks(fp);
            }
            fp->is_use_animlocks = fp->anim_desc.flags.is_use_animlocks;

            /* ft/ftmain.c:4735-4742: a motion whose
 * FTAnimDesc sets is_have_translate_scale plays unscaled on a
 * fighter that has the table, and every other motion scaled.
 * The decomp reads a local attr; the port reads fp->attr. */
            if (fp->attr->translate_scales != NULL)
            {
                if (fp->anim_desc.flags.is_have_translate_scale)
                {
                    fp->is_have_translate_scale = FALSE;
                }
                else fp->is_have_translate_scale = TRUE;
            }
            /* ft/ftmain.c:4747-4800: the motion's script, if it has one,
 * from the pack's copy of the MainMotion file */
            if (m->script != 0)
            {
                event_script_ptr = (u8 *)dc_model_of(fighter_gobj)->blob + m->script;
            }
            else event_script_ptr = NULL;

            fp->motion_scripts[0][0].p_script = fp->motion_scripts[1][0].p_script = event_script_ptr;

            anim_frame = DObjGetStruct(fighter_gobj)->anim_speed - frame_begin;

            fp->motion_scripts[0][0].script_wait = fp->motion_scripts[1][0].script_wait = anim_frame;
            fp->motion_scripts[0][0].script_id = fp->motion_scripts[1][0].script_id = 0;

            for (i = 1; i < ARRAY_COUNT(fp->motion_scripts[0]); i++)
            {
                fp->motion_scripts[0][i].p_script = fp->motion_scripts[1][i].p_script = NULL;
            }
            if (frame_begin != 0.0F)
            {
                ftMainPlayAnimEventsForward(fighter_gobj);
            }
            else
            {
                ftMainPlayAnimEventsAll(fighter_gobj);
                ftMainRunUpdateColAnim(fighter_gobj);
            }
        }
        else for (i = 0; i < ARRAY_COUNT(fp->motion_scripts[0]); i++)
        {
            fp->motion_scripts[0][i].p_script = fp->motion_scripts[1][i].p_script = NULL;
        }
    }
    else for (i = 0; i < ARRAY_COUNT(fp->motion_scripts[0]); i++)
    {
        fp->motion_scripts[0][i].p_script = fp->motion_scripts[1][i].p_script = NULL;
    }
    /* ft/ftmain.c:4801-4822. The guard was dropped while the port had no
 * opening tables -- a demo fighter always took a status row, so
 * writing that row's procs was the only thing to do. With the tables
 * it is the game's three arms again: a battle fighter
 * takes its status row's four procs, a demo fighter in an opening
 * status takes that row's one proc and nothing else, and a demo
 * fighter in any other status is left with no proc_update at all.
 * A NULL status_desc has already been reported above. */
    if (fp->pkind != nFTPlayerKindDemo && status_desc != NULL)
    {
        fp->proc_update = status_desc->proc_update;
        fp->proc_interrupt = status_desc->proc_interrupt;
        fp->proc_physics = status_desc->proc_physics;
        fp->proc_map = status_desc->proc_map;
        fp->proc_slope = mpCommonUpdateFighterSlopeContour;
        fp->proc_accessory = NULL;
        fp->proc_damage = NULL;
        fp->proc_trap = NULL;
        fp->proc_hit = NULL;
        fp->proc_shield = NULL;
        fp->proc_passive = NULL;
        fp->proc_lagupdate = NULL;
        fp->proc_lagstart = NULL;
        fp->proc_lagend = NULL;
    }
    else if (opening_desc != NULL)
    {
        fp->proc_update = opening_desc->proc_update;
    }
    else fp->proc_update = NULL;
}

/* ft/ftanimend.c:4-13 ftAnimEndCheckSetStatus 0x800D9490, verbatim.
 * gobj->anim_frame counts up from frame_begin while the animation runs
 * (ft/ftanim.c:83) and ftAnimParseDObjFigatree replaces it with the
 * leftover wait -- zero or less -- the moment a joint's script reaches
 * its End command (ft/ftanim.c:118-121). */
sb32 ftAnimEndCheckSetStatus(GObj *fighter_gobj,
                                    void (*proc_status)(GObj *))
{
    if (fighter_gobj->anim_frame <= 0.0F)
    {
        proc_status(fighter_gobj);

        return TRUE;
    }
    else return FALSE;
}

/* ft/ftanimend.c:16-19 0x800D94C4 */
void ftAnimEndSetWait(GObj *fighter_gobj)
{
    ftAnimEndCheckSetStatus(fighter_gobj, ftCommonWaitSetStatus);
}

/* ft/ftanimend.c:22-25 0x800D94E8 */
void ftAnimEndSetFall(GObj *fighter_gobj)
{
    ftAnimEndCheckSetStatus(fighter_gobj, ftCommonFallSetStatus);
}

/* ---- ft/ftphysics.c: not here any more ------------------------------
 * The whole file is compiled unmodified -- all 31
 * functions, where this held 22 hand copies of them. The declarations
 * come from the decomp's own <ft/ftphysics.h> through <ft/fighter.h>,
 * as they always did, so every call below is unchanged.
 *
 * Two DIVERGES went with the copies. ftPhysicsSetGroundVelTransferAir
 * had dropped its jostle and z-transfer half; ftPhysicsApplyGroundVelTransN
 * had dropped a branch argued unreachable. Both are the game's again.
 * src/game/ssb64/Makefile has the rule.
 * ------------------------------------------------------------------ */

/* ft/ftcommon/ftcommonwalk.c:94-100 ftCommonWalkProcPhysics 0x8013E548 */
void ftCommonWalkProcPhysics(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    ftPhysicsSetGroundVelAbsStickRange(fp, fp->attr->walk_speed_mul, fp->attr->traction);
    ftPhysicsSetGroundVelTransferAir(fighter_gobj);
}

/* ft/ftparam.c:245-248 ftParamGetStickAngleRads 0x800E7FFC */
f32 ftParamGetStickAngleRads(FTStruct *fp)
{
    return atan2f(fp->input.pl.stick_range.y, ABS(fp->input.pl.stick_range.x));
}

/* ---- ft/ftcommon/ftcommonkneebend.c --------------------------------- */

/* ftcommonkneebend.c:~ ftCommonKneeBendCheckButtonTap 0x8013F450 */
sb32 ftCommonKneeBendCheckButtonTap(FTStruct *fp)
{
    return (fp->input.pl.button_tap & FT_JUMP_BUTTONS) != 0;
}

/* ftcommonkneebend.c ftCommonKneeBendGetInputTypeCommon 0x8013F474 */
s32 ftCommonKneeBendGetInputTypeCommon(FTStruct *fp)
{
    if ((fp->input.pl.stick_range.y >= FTCOMMON_KNEEBEND_STICK_RANGE_MIN) && (fp->tap_stick_y <= FTCOMMON_KNEEBEND_BUFFER_TICS_MAX))
    {
        return FTCOMMON_KNEEBEND_INPUT_TYPE_STICK;
    }
    if (ftCommonKneeBendCheckButtonTap(fp) != FALSE)
    {
        return FTCOMMON_KNEEBEND_INPUT_TYPE_BUTTON;
    }
    return FTCOMMON_KNEEBEND_INPUT_TYPE_NONE;
}

/* ftcommonkneebend.c:50-62 ftCommonKneeBendSetStatusParam 0x8013F3B4:
 * the game's form, which KneeBend and GuardKneeBend share */
void ftCommonKneeBendSetStatusParam(GObj *fighter_gobj, s32 status_id, s32 input_source)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    ftMainSetStatus(fighter_gobj, status_id, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);

    fp->status_vars.common.kneebend.jump_force = fp->input.pl.stick_range.y;
    fp->status_vars.common.kneebend.anim_frame = 0.0F;
    fp->status_vars.common.kneebend.input_source = input_source;
    fp->status_vars.common.kneebend.is_shorthop = FALSE;

    fp->is_special_interrupt = TRUE;
}

/* ftcommonkneebend.c:65-68 ftCommonKneeBendSetStatus 0x8013F408 */
void ftCommonKneeBendSetStatus(GObj *fighter_gobj, s32 input_source)
{
    ftCommonKneeBendSetStatusParam(fighter_gobj, nFTCommonStatusKneeBend, input_source);
}

/* ftcommonkneebend.c:71-74 ftCommonGuardKneeBendSetStatus 0x8013F42C: the
 * jump out of a shield */
void ftCommonGuardKneeBendSetStatus(GObj *fighter_gobj, s32 input_source)
{
    ftCommonKneeBendSetStatusParam(fighter_gobj, nFTCommonStatusGuardKneeBend, input_source);
}

/* ftcommonkneebend.c:100-133 ftCommonKneeBendCheckInterruptCommon
 * 0x8013F4D0, whole as of the item-use step: a fighter holding the Hammer
 * jumps out of the Hammer's own kneebend, not this one, and that arm --
 * dropped from because ft/fthammer.c was not in the build --
 * comes first. */
sb32 ftCommonKneeBendCheckInterruptCommon(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    s32 input_source;

    if (ftHammerCheckHoldHammer(fighter_gobj) != FALSE)
    {
        return ftCommonHammerKneeBendCheckInterruptCommon(fighter_gobj);
    }
    input_source = ftCommonKneeBendGetInputTypeCommon(fp);

    if (input_source != FTCOMMON_KNEEBEND_INPUT_TYPE_NONE)
    {
        ftCommonKneeBendSetStatus(fighter_gobj, input_source);

        return TRUE;
    }
    else return FALSE;
}

/* ftcommonkneebend.c:121-132 ftCommonKneeBendGetInputTypeRun 0x8013F53C:
 * a run jumps off a lower stick than a stand does -- 44, and strictly
 * greater, against the standing threshold's 53 -- which is the whole
 * difference between this and GetInputTypeCommon above */
s32 ftCommonKneeBendGetInputTypeRun(FTStruct *fp)
{
    if ((fp->input.pl.stick_range.y > FTCOMMON_KNEEBEND_RUN_STICK_RANGE_MIN) && (fp->tap_stick_y <= FTCOMMON_KNEEBEND_BUFFER_TICS_MAX))
    {
        return FTCOMMON_KNEEBEND_INPUT_TYPE_STICK;
    }
    if (ftCommonKneeBendCheckButtonTap(fp) != FALSE)
    {
        return FTCOMMON_KNEEBEND_INPUT_TYPE_BUTTON;
    }
    return FTCOMMON_KNEEBEND_INPUT_TYPE_NONE;
}

/* ftcommonkneebend.c:135-152 ftCommonKneeBendCheckInterruptRun 0x8013F598,
 * whole too -- the same hammer arm ahead of it. */
sb32 ftCommonKneeBendCheckInterruptRun(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    s32 input_source;

    if (ftHammerCheckHoldHammer(fighter_gobj) != FALSE)
    {
        return ftCommonHammerKneeBendCheckInterruptCommon(fighter_gobj);
    }
    input_source = ftCommonKneeBendGetInputTypeRun(fp);

    if (input_source != FTCOMMON_KNEEBEND_INPUT_TYPE_NONE)
    {
        ftCommonKneeBendSetStatus(fighter_gobj, input_source);

        return TRUE;
    }
    else return FALSE;
}

/* ftcommonkneebend.c:157-169 ftCommonGuardKneeBendCheckInterruptGuard
 * 0x8013F604 */
sb32 ftCommonGuardKneeBendCheckInterruptGuard(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    s32 input_source = ftCommonKneeBendGetInputTypeCommon(fp);

    if ((input_source != FTCOMMON_KNEEBEND_INPUT_TYPE_NONE) && (fp->input.pl.button_hold & fp->input.button_mask_z))
    {
        ftCommonGuardKneeBendSetStatus(fighter_gobj, input_source);

        return TRUE;
    }
    else return FALSE;
}

/* ---- ft/ftcommon/ftcommonjump.c ------------------------------------- */

/* ftcommonjump.c:18-101 ftCommonJumpGetJumpForceButton 0x8013F6A0 and
 * ftCommonJumpSetStatus 0x8013F880, verbatim -- including the s32
 * pointees and the s32 vel_x/vel_y the setter keeps: the game truncates
 * a button jump's force, and a stick jump's, to whole units before the
 * height multiplier sees them. (Until the port carried them
 * as floats and jumped fractionally higher than the N64.) */
// 0x8013F6A0
void ftCommonJumpGetJumpForceButton(s32 stick_range_x, s32 *jump_vel_x, s32 *jump_vel_y, sb32 is_shorthop)
{
    f32 sqrt_vel_x;
    f32 vel_y;
    f32 vel_x;

    vel_x = ABS(stick_range_x);

    sqrt_vel_x = sqrtf(1.0F - SQUARE(vel_x / F_CONTROLLER_RANGE_MAX));

    if (is_shorthop == FALSE)
    {
        vel_y = (FTCOMMON_KNEEBEND_BUTTON_LONG_FORCE * sqrt_vel_x) + FTCOMMON_KNEEBEND_BUTTON_LONG_MIN;

        if ((SQUARE(vel_x) + SQUARE(vel_y)) > SQUARE(F_CONTROLLER_RANGE_MAX))
        {
            vel_y = sqrtf(SQUARE(F_CONTROLLER_RANGE_MAX) - SQUARE(vel_x));
        }
        if (vel_y < FTCOMMON_KNEEBEND_BUTTON_LONG_MIN)
        {
            vel_y = FTCOMMON_KNEEBEND_BUTTON_LONG_MIN;
        }
    }
    else
    {
        vel_y = (FTCOMMON_KNEEBEND_BUTTON_SHORT_FORCE * sqrt_vel_x) + FTCOMMON_KNEEBEND_BUTTON_SHORT_MIN;

        if ((SQUARE(vel_x) + SQUARE(vel_y)) > SQUARE(F_CONTROLLER_RANGE_MAX))
        {
            vel_y = sqrtf(SQUARE(F_CONTROLLER_RANGE_MAX) - SQUARE(vel_x));
        }
        if (vel_y < FTCOMMON_KNEEBEND_BUTTON_SHORT_MIN)
        {
            vel_y = FTCOMMON_KNEEBEND_BUTTON_SHORT_MIN;
        }
    }
    if (vel_y > FTCOMMON_KNEEBEND_BUTTON_HEIGHT_CLAMP)
    {
        vel_y = FTCOMMON_KNEEBEND_BUTTON_HEIGHT_CLAMP;
    }
 *jump_vel_x = (stick_range_x >= 0) ? vel_x : -vel_x;

 *jump_vel_y = vel_y;
}

// 0x8013F880
void ftCommonJumpSetStatus(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    FTAttributes *attr = fp->attr;
    s32 status_id;
    s32 vel_x, vel_y;

    mpCommonSetFighterAir(fp);

    status_id = ((fp->input.pl.stick_range.x * fp->lr) > FTCOMMON_KNEEBEND_JUMP_F_OR_B_RANGE) ? nFTCommonStatusJumpF : nFTCommonStatusJumpB;

    ftMainSetStatus(fighter_gobj, status_id, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);

    switch (fp->status_vars.common.kneebend.input_source)
    {
    case FTCOMMON_KNEEBEND_INPUT_TYPE_BUTTON:
        ftCommonJumpGetJumpForceButton(fp->input.pl.stick_range.x, &vel_x, &vel_y, fp->status_vars.common.kneebend.is_shorthop);
        break;

    case FTCOMMON_KNEEBEND_INPUT_TYPE_STICK:
    default:
        vel_x = fp->input.pl.stick_range.x;
        vel_y = fp->status_vars.common.kneebend.jump_force;

        if (vel_y < FTCOMMON_KNEEBEND_STICK_RANGE_MIN)
        {
            vel_y = FTCOMMON_KNEEBEND_STICK_RANGE_MIN;
        }
        break;
    }
    fp->physics.vel_air.y = (vel_y * attr->jump_height_mul) + attr->jump_height_base;
    fp->physics.vel_air.x = vel_x * attr->jump_vel_x;

    fp->tap_stick_y = FTINPUT_STICKBUFFER_TICS_MAX;

    fp->is_special_interrupt = TRUE;
}

/* ftcommonkneebend.c ftCommonKneeBendProcUpdate 0x8013F2A0. DIVERGES:
 * the game reads the jump-squat frame off the GObj's countdown; the
 * pack player counts up, so the status keeps its own frame counter. */
void ftCommonKneeBendProcUpdate(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    FTAttributes *attr = fp->attr;

    fp->status_vars.common.kneebend.anim_frame += DObjGetStruct(fighter_gobj)->anim_speed;

    if ((fp->status_vars.common.kneebend.input_source == FTCOMMON_KNEEBEND_INPUT_TYPE_BUTTON) && (fp->status_vars.common.kneebend.anim_frame <= FTCOMMON_KNEEBEND_SHORTHOP_FRAMES) && (fp->input.pl.button_release & FT_JUMP_BUTTONS))
    {
        fp->status_vars.common.kneebend.is_shorthop = TRUE;
    }
    if (attr->kneebend_anim_length <= fp->status_vars.common.kneebend.anim_frame)
    {
        ftCommonJumpSetStatus(fighter_gobj);
    }
}

/* ftcommonkneebend.c:33-47 ftCommonKneeBendProcInterrupt 0x8013F334,
 * verbatim since an up-B or an up smash out of jumpsquat
 * takes the jump instead, and only if neither fires does the stick keep
 * raising the jump's force. AttackHi4CheckInterruptKneeBend is the
 * no-tap-window one -- the stick is already up, which is why the
 * fighter is in jumpsquat at all. SpecialHi is part of this cascade,
 * which was the last place neither check was called from before. */
void ftCommonKneeBendProcInterrupt(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if (ftCommonSpecialHiCheckInterruptCommon(fighter_gobj) == FALSE)
    {
        if (ftCommonAttackHi4CheckInterruptKneeBend(fighter_gobj) == FALSE)
        {
            if (fp->status_vars.common.kneebend.jump_force < fp->input.pl.stick_range.y)
            {
                fp->status_vars.common.kneebend.jump_force = fp->input.pl.stick_range.y;
            }
        }
    }
}

/* ---- ft/ftcommon/ftcommonjumpaerial.c ------------------------------- */

/* ftcommonjump.c:9-16 ftCommonJumpProcInterrupt 0x8013F660 and
 * ftcommonfall.c:9-16 ftCommonFallProcInterrupt 0x8013F9A0 -- the same
 * three-check cascade, which is why one function stands for both here
 * (and, through dFTCommonActionStatusDescs, for JumpAerialF/B's
 * ftCommonJumpAerialProcInterrupt 0x8013FB2C as well; all four are
 * identical in the decomp). Both the aerial command demux and the
 * aerials are implemented, so this cascade is now the game's, whole --
 * no DIVERGES left in it. */
void ftCommonJumpProcInterrupt(GObj *fighter_gobj)
{
    if ((ftCommonSpecialAirCheckInterruptCommon(fighter_gobj) == FALSE) && (ftCommonAttackAirCheckInterruptCommon(fighter_gobj) == FALSE))
    {
        ftCommonJumpAerialCheckInterruptCommon(fighter_gobj);
    }
}

/* ---- ft/ftcommon/ftcommonlanding.c ---------------------------------- */

/* ftcommonlanding.c:60-68 ftCommonLandingSetStatusParam 0x80142D44 */
void ftCommonLandingSetStatusParam(GObj *fighter_gobj, s32 status_id,
                                          sb32 is_allow_interrupt,
                                          f32 anim_speed)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    mpCommonSetFighterGround(fp);
    ftMainSetStatus(fighter_gobj, status_id, 0.0F, anim_speed, FTSTATUS_PRESERVE_NONE);

    fp->status_vars.common.landing.is_allow_interrupt = is_allow_interrupt;
}

/* ftcommonlanding.c:71-79 ftCommonLandingSetStatus 0x80142D9C */
void ftCommonLandingSetStatus(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if ((fp->is_fastfall) && (fp->physics.vel_air.y <= -fp->attr->tvel_fast))
    {
        ftCommonLandingSetStatusParam(fighter_gobj, nFTCommonStatusLandingHeavy, TRUE, FTCOMMON_LANDING_HEAVY_ANIM_SPEED);
    }
    else ftCommonLandingSetStatusParam(fighter_gobj, nFTCommonStatusLandingLight, TRUE, FTCOMMON_LANDING_LIGHT_ANIM_SPEED);
}

/* ftcommonlanding.c:83-86 ftCommonLandingAirNullSetStatus 0x80142E10 --
 * the aerial's landing when the fighter's own pack has no LandingAir
 * animation for the attack it was doing, played at the speed the motion
 * script asked for. ftCommonAttackAirProcMap is the one
 * caller, and the reason this finally has one. */
void ftCommonLandingAirNullSetStatus(GObj *fighter_gobj, f32 anim_speed)
{
    ftCommonLandingSetStatusParam(fighter_gobj, nFTCommonStatusLandingAirNull, FALSE, anim_speed);
}

/* ftcommonlanding.c:89-92 ftCommonLandingFallSpecialSetStatus 0x80142E3C --
 * the landing ftCommonFallSpecial's ProcMap hands off to when a helpless
 * fighter touches down. */
void ftCommonLandingFallSpecialSetStatus(GObj *fighter_gobj, sb32 is_allow_interrupt, f32 anim_speed)
{
    ftCommonLandingSetStatusParam(fighter_gobj, nFTCommonStatusLandingFallSpecial, is_allow_interrupt, anim_speed);
}

/* ---- ft/ftcommon/ftcommonwait.c ------------------------------------- */

/* ftcommonwait.c:16-35 ftCommonWaitSetStatus 0x8013E1C8, verbatim:
 * a Hammer holder stands in HammerWait, a standing
 * fighter may be special-interrupted (Link catching his boomerang), and
 * the idle player tag is armed. */
void ftCommonWaitSetStatus(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if (ftHammerCheckHoldHammer(fighter_gobj) != FALSE)
    {
        ftHammerSetStatusHammerWait(fighter_gobj);
    }
    else
    {
        if (fp->ga == nMPKineticsAir)
        {
            mpCommonSetFighterGround(fp);
        }
        ftMainSetStatus(fighter_gobj, nFTCommonStatusWait, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);

        fp->is_special_interrupt = TRUE;

        ftParamSetPlayerTagWait(fighter_gobj, 120);
    }
}

/* ftcommonwait.c:37-46 ftCommonWaitCheckInputSuccess 0x8013E258 */
sb32 ftCommonWaitCheckInputSuccess(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if (((fp->input.pl.stick_range.x * fp->lr) < 0) || (ABS(fp->input.pl.stick_range.x) < 8))
    {
        return TRUE;
    }
    else return FALSE;
}

/* ftcommonwait.c:49-58 ftCommonWaitCheckInterruptCommon 0x8013E2A0 */
sb32 ftCommonWaitCheckInterruptCommon(GObj *fighter_gobj)
{
    if (ftCommonWaitCheckInputSuccess(fighter_gobj) != FALSE)
    {
        ftCommonWaitSetStatus(fighter_gobj);

        return TRUE;
    }
    else return FALSE;
}

/* ---- ft/ftcommon/ftcommonwalk.c ------------------------------------- */

/* ftcommonwalk.c:35-56 ftCommonWalkGetWalkAnimLength 0x8013E2E0 */
f32 ftCommonWalkGetWalkAnimLength(FTStruct *fp, s32 status_id)
{
    f32 walk_anim_length;

    switch (status_id)
    {
    case nFTCommonStatusWalkSlow:
        walk_anim_length = fp->attr->walkslow_anim_length;
        break;

    case nFTCommonStatusWalkMiddle:
        walk_anim_length = fp->attr->walkmiddle_anim_length;
        break;

    default:
        walk_anim_length = fp->attr->walkfast_anim_length;
        break;
    }
    return walk_anim_length;
}

/* ftcommonwalk.c:59-73 ftCommonWalkGetWalkStatus 0x8013E340 */
s32 ftCommonWalkGetWalkStatus(s8 stick_range_x)
{
    s32 status_id;

    stick_range_x = ABS(stick_range_x);

    if (stick_range_x >= FTCOMMON_WALKFAST_STICK_RANGE_MIN)
    {
        status_id = nFTCommonStatusWalkFast;
    }
    else if (stick_range_x >= FTCOMMON_WALKMIDDLE_STICK_RANGE_MIN)
    {
        status_id = nFTCommonStatusWalkMiddle;
    }
    else status_id = nFTCommonStatusWalkSlow;

    return status_id;
}

/* ftcommonwalk.c:102-114 ftCommonWalkSetStatusParam 0x8013E580,
 * verbatim since: the new walk plays its first frame's
 * events now, and a slow or middle walk may be interrupted by the
 * specials on the frame it starts. */
void ftCommonWalkSetStatusParam(GObj *fighter_gobj,
                                       f32 anim_frame_begin)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    s32 status_id = ftCommonWalkGetWalkStatus(fp->input.pl.stick_range.x);

    ftMainSetStatus(fighter_gobj, status_id, anim_frame_begin, 1.0F, FTSTATUS_PRESERVE_NONE);
    ftMainPlayAnimEventsAll(fighter_gobj);

    if (status_id != nFTCommonStatusWalkFast)
    {
        fp->is_special_interrupt = TRUE;
    }
}

/* ftcommonwalk.c:117-121 ftCommonWalkSetStatusDefault 0x8013E5F4 */
void ftCommonWalkSetStatusDefault(GObj *fighter_gobj)
{
    ftCommonWalkSetStatusParam(fighter_gobj, 0.0F);
}

/* ftcommonwalk.c:124-133 ftCommonWalkCheckInputSuccess 0x8013E614 */
sb32 ftCommonWalkCheckInputSuccess(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if ((fp->input.pl.stick_range.x * fp->lr) >= 8)
    {
        return TRUE;
    }
    else return FALSE;
}

/* ftcommonwalk.c:136-146 ftCommonWalkCheckInterruptCommon 0x8013E648 */
sb32 ftCommonWalkCheckInterruptCommon(GObj *fighter_gobj)
{
    if (ftCommonWalkCheckInputSuccess(fighter_gobj) != FALSE)
    {
        ftCommonWalkSetStatusDefault(fighter_gobj);

        return TRUE;
    }
    else return FALSE;
}

/* ---- ft/ftcommon/ftcommonturn.c ------------------------------------- */

/* ftcommonturn.c ftCommonTurnCheckInputSuccess 0x8013ED90 */
sb32 ftCommonTurnCheckInputSuccess(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if ((fp->input.pl.stick_range.x * fp->lr) <= FTCOMMON_TURN_STICK_RANGE_MIN)
    {
        return TRUE;
    }
    else return FALSE;
}

/* ftcommonturn.c:109-124 ftCommonTurnSetStatus 0x8013E908, verbatim */
void ftCommonTurnSetStatus(GObj *fighter_gobj, s32 lr_dash)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    fp->motion_vars.flags.flag1 = 0;

    ftMainSetStatus(fighter_gobj, nFTCommonStatusTurn, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);
    ftMainPlayAnimEventsAll(fighter_gobj);

    fp->status_vars.common.turn.is_allow_turn_direction = FALSE;
    fp->status_vars.common.turn.is_disable_sa_interrupts = FALSE;
    fp->status_vars.common.turn.button_mask = 0;
    fp->status_vars.common.turn.lr_dash = lr_dash;
    fp->status_vars.common.turn.attacks4_buffer = (lr_dash != 0) ? 0 : 256;
    fp->status_vars.common.turn.lr_turn = -fp->lr;
}

/* ftcommonturn.c:134-139 ftCommonTurnSetStatusInvertLR 0x8013E9A8: the
 * turn a dash the other way becomes, with the dash's own lr already in
 * the status vars for ftCommonTurnProcInterrupt to dash out of */
void ftCommonTurnSetStatusInvertLR(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    ftCommonTurnSetStatus(fighter_gobj, -fp->lr);
}

/* ftcommonturn.c:127-131 ftCommonTurnSetStatusCenter 0x8013E988 */
void ftCommonTurnSetStatusCenter(GObj *fighter_gobj)
{
    ftCommonTurnSetStatus(fighter_gobj, 0);
}

/* ftcommonturn.c:154-164 ftCommonTurnCheckInterruptCommon 0x8013EA04 */
sb32 ftCommonTurnCheckInterruptCommon(GObj *fighter_gobj)
{
    if (ftCommonTurnCheckInputSuccess(fighter_gobj) != FALSE)
    {
        ftCommonTurnSetStatusCenter(fighter_gobj);

        return TRUE;
    }
    else return FALSE;
}

/* ftcommonturn.c:10-27 ftCommonTurnProcUpdate 0x8013E690, verbatim. The
 * flip is cued by the Turn motion script (relocData/202_MarioMainMotion.c
 * dMarioMainMotion_Turn: WaitAsync(6), SetFlag1(1)), which the engine
 * runs. */
void ftCommonTurnProcUpdate(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if (fp->motion_vars.flags.flag1 != 0)
    {
        fp->motion_vars.flags.flag1 = 0;

        fp->status_vars.common.turn.is_allow_turn_direction = TRUE;
        fp->status_vars.common.turn.is_disable_sa_interrupts = TRUE;

        fp->lr = -fp->lr;
        fp->physics.vel_ground.x = -fp->physics.vel_ground.x;
    }
    if (fighter_gobj->anim_frame <= 0.0F)
    {
        ftCommonWaitSetStatus(fighter_gobj);
    }
}

/* ftcommonturn.c:30-106 ftCommonTurnProcInterrupt 0x8013E6E4. The
 * cascade is the game's, whole, closed its last two
 * holes (the grab and the four smashes); SpecialLw came,
 * SpecialN at SpecialHi at, the appeal at.
 * The dash out of the turn (lines 81-92) is the game's now.
 * DIVERGES: the
 * game keeps everything from the grab down inside two nested tests and
 * falls off the end of the function when either fires; these are early
 * returns, which is the same control flow written flat. */
void ftCommonTurnProcInterrupt(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if (fp->status_vars.common.turn.is_allow_turn_direction != FALSE)
    {
        fp->input.pl.button_tap |= fp->status_vars.common.turn.button_mask;
    }
    /* ftcommonturn.c:40-46: the neutral-B, up-B and Down-B, under the same
 * is_disable_sa_interrupts gate the game puts all three specials
 * behind (SpecialLw, SpecialN, SpecialHi). */
    if ((fp->status_vars.common.turn.is_disable_sa_interrupts != FALSE) && (ftCommonSpecialNCheckInterruptCommon(fighter_gobj) != FALSE))
    {
        return;
    }
    if ((fp->status_vars.common.turn.is_disable_sa_interrupts != FALSE) && (ftCommonSpecialHiCheckInterruptCommon(fighter_gobj) != FALSE))
    {
        return;
    }
    if ((fp->status_vars.common.turn.is_disable_sa_interrupts != FALSE) && (ftCommonSpecialLwCheckInterruptCommon(fighter_gobj) != FALSE))
    {
        return;
    }
    /* ftcommonturn.c:49-60: the grab, then the forward smash -- and
 * neither is behind the is_disable_sa_interrupts gate the three
 * specials above are. The turn's own forward smash reads
 * attack4.lr, which is turn.lr_turn under another name, so for the
 * first six frames of the turn the stick is measured against the
 * way the turn is going; after that attacks4_buffer has run past 6
 * and the common check takes over, measuring it against the
 * fighter's own facing. The buffer counts to 256 and stops.
 * the grab's slot here is the game's, and was the
 * one ground cascade left without it. */
    if (ftCommonCatchCheckInterruptCommon(fighter_gobj) != FALSE)
    {
        return;
    }
    if (fp->status_vars.common.turn.attacks4_buffer < 256)
    {
        fp->status_vars.common.turn.attacks4_buffer++;
    }
    if (((fp->status_vars.common.turn.attacks4_buffer < 6) ? ftCommonAttackS4CheckInterruptTurn(fighter_gobj)
                                                           : ftCommonAttackS4CheckInterruptCommon(fighter_gobj)) != FALSE)
    {
        return;
    }
    /* ftcommonturn.c:63-70: the up and down smash, the tilts and the
 * jab -- the run of seven minus the forward smash just done */
    if ((fp->status_vars.common.turn.is_disable_sa_interrupts != FALSE) && (ftCommonAttackCheckInterruptGround(fighter_gobj, FTCOMMON_GROUNDATTACK_TURN) != FALSE))
    {
        return;
    }
    if (ftCommonGuardOnCheckInterruptCommon(fighter_gobj) != FALSE)
    {
        return;
    }
    if (ftCommonAppealCheckInterruptCommon(fighter_gobj) != FALSE)
    {
        return;
    }
    if (ftCommonKneeBendCheckInterruptCommon(fighter_gobj) != FALSE)
    {
        return;
    }
    /* ftcommonturn.c:81-92: the stick tapped the way the turn is going
 * arms the dash, and the frame the turn lets go of the direction it
 * takes it -- a dash out of a turn, the game's second way in */
    ftCommonDashCheckTurn(fighter_gobj);

    if (fp->status_vars.common.turn.is_allow_turn_direction != FALSE)
    {
        if (fp->status_vars.common.turn.lr_dash != 0)
        {
            if ((fp->input.pl.stick_range.x * fp->status_vars.common.turn.lr_turn) >= FTCOMMON_DASH_STICK_RANGE_MIN)
            {
                ftCommonDashSetStatus(fighter_gobj, 0);
            }
        }
    }
    if (fp->input.pl.button_tap & fp->input.button_mask_a)
    {
        fp->status_vars.common.turn.button_mask |= fp->input.button_mask_a;
    }
    if (fp->input.pl.button_tap & fp->input.button_mask_b)
    {
        fp->status_vars.common.turn.button_mask |= fp->input.button_mask_b;
    }
    if (fp->status_vars.common.turn.is_allow_turn_direction != FALSE)
    {
        fp->status_vars.common.turn.is_allow_turn_direction = FALSE;
    }
}

/* ---- ft/ftcommon/ftcommonappeal.c: the taunt ----------
 *
 * The L-tap taunt, status 189. Three functions, verbatim (the grab's
 * call since Kirby's copy arm since).
 * Its cascade slot is the game's: between GuardOn and KneeBend in every
 * ground status that has it (fighter.h:61's ground macro, and the walk,
 * squat, squatwait, squatrv, landing, ottotto, turn, dash and run
 * cascades). Its status row is the plain one -- ftAnimEndSetWait ends
 * it, ground friction moves it, the cliff-edge map holds it on. */

/* ftcommonappeal.c:10-18 ftCommonAppealProcInterrupt 0x8014E6A0. Verbatim
 * since on the animation's flag1 event the appeal tries the
 * grab first (a buffered Z+A cancels the taunt into a grab) and, catching
 * nothing, cancels into a shield -- which is what the game does when no
 * grab is buffered. */
void ftCommonAppealProcInterrupt(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if ((fp->motion_vars.flags.flag1 != 0) && (ftCommonCatchCheckInterruptCommon(fighter_gobj) == FALSE))
    {
        ftCommonGuardOnCheckInterruptCommon(fighter_gobj);
    }
}

/* ftcommonappeal.c:21-36 ftCommonAppealSetStatus 0x8014E6E0, verbatim
 * since: a Kirby holding a power he swallowed (not one he
 * spawned with) drops it when he taunts. */
void ftCommonAppealSetStatus(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if ((fp->fkind == nFTKindKirby) || (fp->fkind == nFTKindNKirby))
    {
        if ((fp->passive_vars.kirby.copy_id != nFTKindKirby) && (fp->passive_vars.kirby.is_ignore_losecopy == FALSE))
        {
            ftKirbySpecialNLoseCopy(fighter_gobj);
        }
    }
    ftMainSetStatus(fighter_gobj, nFTCommonStatusAppeal, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);

    fp->motion_vars.flags.flag1 = 0;
}

/* ftcommonappeal.c:39-49 ftCommonAppealCheckInterruptCommon 0x8014E764 */
sb32 ftCommonAppealCheckInterruptCommon(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if (fp->input.pl.button_tap & fp->input.button_mask_l)
    {
        ftCommonAppealSetStatus(fighter_gobj);

        return TRUE;
    }
    else return FALSE;
}

/* ---- ft/ftcommon/ftcommondash.c, ftcommonrun.c, ftcommonrunbrake.c
 * and ftcommonturnrun.c: the dash and the run ------------
 *
 * Four files, four statuses -- Dash (15), Run (16), RunBrake (17) and
 * TurnRun (19) -- and the loop between them: a stick tapped past 56 is
 * a dash, a dash held past the fighter's `dash_to_run` frame becomes a
 * run, letting the stick go is RunBrake, and pushing it the other way
 * is TurnRun, which flips the fighter mid-stride and goes back to Run.
 * Every number is the fighter's own attribute -- `dash_speed`,
 * `dash_decel`, `run_speed`, `dash_to_run` -- and they have been in the
 * pack since (tools/export/ssb_packexport.py), unread until now.
 *
 * Verbatim but for what is marked DIVERGES, which in this section is
 * always the same thing: the dash's and the run's interrupt cascades
 * name the specials, the grab, the smashes and the dash attack, none of
 * which is ported (the appeal is too -- its check is in
 * the dash's `next:` block and the run's cascade). The frame windows
 * they hang on -- 3, 5, 20 and `dash_to_run` -- are the game's, and so
 * is the order of what is left. */

/* ftcommondash.c:10-20 ftCommonDashProcUpdate 0x8013EA40 */
void ftCommonDashProcUpdate(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if (fighter_gobj->anim_frame <= 0.0F)
    {
        fp->physics.vel_ground.x *= 0.75F;

        ftCommonWaitSetStatus(fighter_gobj);
    }
}

/* ftcommondash.c:23-83 ftCommonDashProcInterrupt 0x8013EA90. The
 * cascade is the game's, whole, closed its last two
 * holes: SpecialN in the first two windows (ported at and
 * called from neither until now) and the dash attack in the second.
 * The forward smash joined at, the grabs at. The three
 * frame windows, the goto,
 * the three grab arms ( CatchCheckInterruptCommon in the
 * first and third windows, CatchCheckInterruptDashRun in the second)
 * and everything else left in them are the game's. The first window is
 * also the one the roll reads (ftcommondash.c:36): Z inside
 * the dash's first three frames rolls. */
void ftCommonDashProcInterrupt(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if ((fp->motion_vars.flags.flag1 != 0) && (fighter_gobj->anim_frame <= 5.0F))
    {
        if ((ftCommonSpecialNCheckInterruptCommon(fighter_gobj) == FALSE) &&
            (ftCommonCatchCheckInterruptCommon(fighter_gobj) == FALSE) &&
            (ftCommonAttackS4CheckInterruptDash(fighter_gobj) == FALSE))
        {
            if ((fighter_gobj->anim_frame <= 3.0F) && (ftCommonEscapeCheckInterruptDash(fighter_gobj) != FALSE))
            {
                return;
            }
            else if (ftCommonGuardOnCheckInterruptCommon(fighter_gobj) != FALSE)
            {
                return;
            }
            else goto next;
        }
    }
    else if (fighter_gobj->anim_frame <= 20.0F)
    {
        if ((ftCommonSpecialNCheckInterruptCommon(fighter_gobj) == FALSE) &&
            (ftCommonCatchCheckInterruptDashRun(fighter_gobj) == FALSE))
        {
            if ((ftCommonAttackDashCheckInterruptCommon(fighter_gobj) == FALSE) &&
                (((fp->input.pl.stick_range.x * fp->lr) >= 0) || (ftCommonDashCheckInterruptCommon(fighter_gobj) == FALSE)))
            {
                /* the shield out of a dash slides for what is left of the
 * twenty frames (ftCommonGuardOnSetStatus's slide_tics) */
                if (ftCommonGuardOnCheckInterruptDashRun(fighter_gobj, 20.0F - fighter_gobj->anim_frame) != FALSE)
                {
                    return;
                }
                else goto next;
            }
        }
    }
    else if ((ftCommonCatchCheckInterruptCommon(fighter_gobj) == FALSE) &&
             (ftCommonDashCheckInterruptCommon(fighter_gobj) == FALSE) &&
             (ftCommonGuardOnCheckInterruptCommon(fighter_gobj) == FALSE))
    {
    next:
        if ((ftCommonAppealCheckInterruptCommon(fighter_gobj) == FALSE) &&
            (ftCommonKneeBendCheckInterruptRun(fighter_gobj) == FALSE))
        {
            ftCommonRunCheckInterruptDash(fighter_gobj);
        }
    }
}

/* ftcommondash.c:86-96 ftCommonDashProcPhysics 0x8013EC58: the dash
 * runs at dash_speed for seven frames and then brakes at dash_decel */
void ftCommonDashProcPhysics(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    FTAttributes *attr = fp->attr;

    if (fighter_gobj->anim_frame >= FTCOMMON_DASH_DECELERATE_BEGIN)
    {
        ftPhysicsSetGroundVelFriction(fp, attr->dash_decel);
    }
    ftPhysicsSetGroundVelTransferAir(fighter_gobj);
}

/* ftcommondash.c:99-109 ftCommonDashProcMap 0x8013ECB0: past the frame
 * the dash could become a run, the fighter stops at a ledge instead of
 * walking off it */
void ftCommonDashProcMap(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    FTAttributes *attr = fp->attr;

    if (attr->dash_to_run <= fighter_gobj->anim_frame)
    {
        mpCommonProcFighterOnCliffEdge(fighter_gobj);
    }
    else mpCommonSetFighterFallOnGroundBreak(fighter_gobj);
}

/* ftcommondash.c:112-122 ftCommonDashSetStatus 0x8013ED00. The stick
 * buffer is spent on the way in, so the dash cannot immediately dash
 * again off the same tap; flag is what tells ProcInterrupt above which
 * window this dash is in. */
void ftCommonDashSetStatus(GObj *fighter_gobj, u32 flag)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    ftMainSetStatus(fighter_gobj, nFTCommonStatusDash, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);
    ftMainPlayAnimEventsAll(fighter_gobj);

    fp->physics.vel_ground.x = fp->attr->dash_speed;
    fp->tap_stick_x = FTINPUT_STICKBUFFER_TICS_MAX;
    fp->motion_vars.flags.flag1 = flag;
}

/* ftcommondash.c:125-143 ftCommonDashCheckInterruptCommon 0x8013ED64:
 * the same tap the roll reads, 56 of stick inside three tics, and
 * against the facing it is a turn with the dash's own flag set */
sb32 ftCommonDashCheckInterruptCommon(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if ((ABS(fp->input.pl.stick_range.x) >= FTCOMMON_DASH_STICK_RANGE_MIN) && (fp->tap_stick_x < FTCOMMON_DASH_BUFFER_TICS_MAX))
    {
        if ((fp->input.pl.stick_range.x * fp->lr) < 0)
        {
            ftCommonTurnSetStatusInvertLR(fighter_gobj);

            return TRUE;
        }
        ftParamSetStickLR(fp);
        ftCommonDashSetStatus(fighter_gobj, 1);

        return TRUE;
    }
    return FALSE;
}

/* ftcommondash.c:146-158 ftCommonDashCheckTurn 0x8013EDFC: inside a
 * turn, the stick tapped the way the turn is going arms the dash out of
 * it (the turn's ProcInterrupt above reads lr_dash the next frame) */
sb32 ftCommonDashCheckTurn(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if (((fp->input.pl.stick_range.x * fp->status_vars.common.turn.lr_turn) >= FTCOMMON_DASH_STICK_RANGE_MIN) && (fp->tap_stick_x < FTCOMMON_DASH_BUFFER_TICS_MAX))
    {
        fp->status_vars.common.turn.lr_dash = fp->status_vars.common.turn.lr_turn;
        fp->status_vars.common.turn.attacks4_buffer = 0;

        return TRUE;
    }
    else return FALSE;
}

/* ---- Kirby's copy demux -------------------------------
 *
 * ftcommonspecialn.c:9-38 dFTKirbySpecialNStatusList 0x80188C00 and
 * ftcommonspecialair.c:10-39 dFTKirbySpecialAirNStatusList 0x80188A50.
 *
 * A SECOND demux, one level below the six. dFTCommonSpecialNStatusList
 * sends Kirby (8) and NKirby (22) to ftKirbySpecialNSetStatusSelect,
 * which does not set a status itself -- it indexes THESE tables by
 * fp->passive_vars.kirby.copy_id, the FTKind Kirby last swallowed. So
 * Kirby's Neutral-B is whatever mouth he is wearing, and the inhale
 * itself is just the copy_id == nFTKindKirby row.
 *
 * All 27 slots of both are the decomp's. Boss (12) and the polygon tail
 * (14-26) get Kirby's own inhale, because a polygon has no special to
 * copy.
 *
 * Note the shape the decomp chose: Luigi (4) and MMario (13) share
 * Mario's copy setter exactly as they share his own, and GDonkey (26)
 * is unreachable -- the decomp says so itself, Kirby's copy id for a
 * giant DK is always 2. Both are kept verbatim. */
void (*dFTKirbySpecialNStatusList[nFTKindEnumCount])(GObj*) =
{
    ftKirbyCopyMarioSpecialNSetStatus, /*  0 Mario    */
    ftKirbyCopyFoxSpecialNSetStatus, /*  1 Fox      */
    ftKirbyCopyDonkeySpecialNStartSetStatus, /*  2 Donkey   */
    ftKirbyCopySamusSpecialNStartSetStatus, /*  3 Samus    */
    ftKirbyCopyMarioSpecialNSetStatus, /*  4 Luigi    (Mario's file, Luigi's rows) */
    ftKirbyCopyLinkSpecialNSetStatus, /*  5 Link     */
    ftKirbyCopyYoshiSpecialNSetStatus, /*  6 Yoshi    */
    ftKirbyCopyCaptainSpecialNSetStatus, /*  7 Captain  */
    ftKirbySpecialNStartSetStatus, /*  8 Kirby    */
    ftKirbyCopyPikachuSpecialNSetStatus, /*  9 Pikachu  */
    ftKirbyCopyPurinSpecialNSetStatus, /* 10 Purin */
    ftKirbyCopyNessSpecialNSetStatus, /* 11 Ness */
    ftKirbySpecialNStartSetStatus, /* 12 Boss     */
    ftKirbyCopyMarioSpecialNSetStatus, /* 13 MMario   */
    ftKirbySpecialNStartSetStatus, /* 14 NMario   */
    ftKirbySpecialNStartSetStatus, /* 15 NFox     */
    ftKirbySpecialNStartSetStatus, /* 16 NDonkey  */
    ftKirbySpecialNStartSetStatus, /* 17 NSamus   */
    ftKirbySpecialNStartSetStatus, /* 18 NLuigi   */
    ftKirbySpecialNStartSetStatus, /* 19 NLink    */
    ftKirbySpecialNStartSetStatus, /* 20 NYoshi   */
    ftKirbySpecialNStartSetStatus, /* 21 NCaptain */
    ftKirbySpecialNStartSetStatus, /* 22 NKirby   */
    ftKirbySpecialNStartSetStatus, /* 23 NPikachu */
    ftKirbySpecialNStartSetStatus, /* 24 NPurin   */
    ftKirbySpecialNStartSetStatus, /* 25 NNess    */
    ftKirbySpecialNStartSetStatus, /* 26 GDonkey  */
};

void (*dFTKirbySpecialAirNStatusList[nFTKindEnumCount])(GObj*) =
{
    ftKirbyCopyMarioSpecialAirNSetStatus, /*  0 Mario    */
    ftKirbyCopyFoxSpecialAirNSetStatus, /*  1 Fox      */
    ftKirbyCopyDonkeySpecialAirNStartSetStatus, /*  2 Donkey   */
    ftKirbyCopySamusSpecialAirNStartSetStatus, /*  3 Samus    */
    ftKirbyCopyMarioSpecialAirNSetStatus, /*  4 Luigi    (Mario's file, Luigi's rows) */
    ftKirbyCopyLinkSpecialAirNSetStatus, /*  5 Link     */
    ftKirbyCopyYoshiSpecialAirNSetStatus, /*  6 Yoshi    */
    ftKirbyCopyCaptainSpecialAirNSetStatus, /*  7 Captain  */
    ftKirbySpecialAirNStartSetStatus, /*  8 Kirby    */
    ftKirbyCopyPikachuSpecialAirNSetStatus, /*  9 Pikachu  */
    ftKirbyCopyPurinSpecialAirNSetStatus, /* 10 Purin */
    ftKirbyCopyNessSpecialAirNSetStatus, /* 11 Ness */
    ftKirbySpecialAirNStartSetStatus, /* 12 Boss     */
    ftKirbyCopyMarioSpecialAirNSetStatus, /* 13 MMario   */
    ftKirbySpecialAirNStartSetStatus, /* 14 NMario   */
    ftKirbySpecialAirNStartSetStatus, /* 15 NFox     */
    ftKirbySpecialAirNStartSetStatus, /* 16 NDonkey  */
    ftKirbySpecialAirNStartSetStatus, /* 17 NSamus   */
    ftKirbySpecialAirNStartSetStatus, /* 18 NLuigi   */
    ftKirbySpecialAirNStartSetStatus, /* 19 NLink    */
    ftKirbySpecialAirNStartSetStatus, /* 20 NYoshi   */
    ftKirbySpecialAirNStartSetStatus, /* 21 NCaptain */
    ftKirbySpecialAirNStartSetStatus, /* 22 NKirby   */
    ftKirbySpecialAirNStartSetStatus, /* 23 NPikachu */
    ftKirbySpecialAirNStartSetStatus, /* 24 NPurin   */
    ftKirbySpecialAirNStartSetStatus, /* 25 NNess    */
    ftKirbySpecialAirNStartSetStatus, /* 26 GDonkey  */
};

/* ftcommonspecialn.c:80-85 ftKirbySpecialNSetStatusSelect 0x80151060 and
 * ftcommonspecialair.c:144-149 ftKirbySpecialAirNSetStatusSelect
 * 0x80150ED0, verbatim: read copy_id, call through the table. */
void ftKirbySpecialNSetStatusSelect(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    dFTKirbySpecialNStatusList[fp->passive_vars.kirby.copy_id](fighter_gobj);
}

void ftKirbySpecialAirNSetStatusSelect(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    dFTKirbySpecialAirNStatusList[fp->passive_vars.kirby.copy_id](fighter_gobj);
}

/* ftcommonspecialn.c:42-69 dFTCommonSpecialNStatusList 0x8038B9E8.
 * The Neutral-B command demux: 27 setters indexed by FTKind, so pressing
 * B with a neutral stick reaches the fighter's own SpecialN setter.
 * Verbatim, every slot filled: the Mario-derived kinds the game routes
 * through Mario's setter are Luigi (4), Boss (12), Metal Mario (13),
 * polygon Mario (14) and NLuigi (18), plus NYoshi (20), the decomp's own
 * "un bro momento". */
void (*dFTCommonSpecialNStatusList[nFTKindEnumCount])(GObj*) =
{
    ftMarioSpecialNSetStatus,   /*  0 Mario   */
    ftFoxSpecialNSetStatus,     /*  1 Fox     */
    ftDonkeySpecialNStartSetStatus, /*  2 Donkey  */
    ftSamusSpecialNStartSetStatus, /*  3 Samus */
    ftMarioSpecialNSetStatus,   /*  4 Luigi   */
    ftLinkSpecialNSetStatus,    /*  5 Link    */
    ftYoshiSpecialNSetStatus,   /*  6 Yoshi   */
    ftCaptainSpecialNSetStatus, /*  7 Captain */
    ftKirbySpecialNSetStatusSelect, /*  8 Kirby   */
    ftPikachuSpecialNSetStatus, /*  9 Pikachu */
    ftPurinSpecialNSetStatus,   /* 10 Purin   */
    ftNessSpecialNSetStatus,    /* 11 Ness */
    ftMarioSpecialNSetStatus,   /* 12 Boss    */
    ftMarioSpecialNSetStatus,   /* 13 MMario  */
    ftMarioSpecialNSetStatus,   /* 14 NMario  */
    ftFoxSpecialNSetStatus,     /* 15 NFox    */
    ftDonkeySpecialNStartSetStatus, /* 16 NDonkey */
    ftSamusSpecialNStartSetStatus, /* 17 NSamus */
    ftMarioSpecialNSetStatus,   /* 18 NLuigi  */
    ftLinkSpecialNSetStatus,    /* 19 NLink   */
    ftMarioSpecialNSetStatus,   /* 20 NYoshi  (un bro momento) */
    ftCaptainSpecialNSetStatus, /* 21 NCaptain */
    ftKirbySpecialNSetStatusSelect, /* 22 NKirby  */
    ftPikachuSpecialNSetStatus, /* 23 NPikachu */
    ftPurinSpecialNSetStatus,   /* 24 NPurin  */
    ftNessSpecialNSetStatus,    /* 25 NNess */
    ftDonkeySpecialNStartSetStatus, /* 26 GDonkey */
};

/* ftcommonspecialn.c:88-114 ftCommonSpecialNCheckInterruptCommon
 * 0x80151098: the B tap with the stick held vertically neutral, on a
 * fighter that has a Neutral-B -- against the facing it first sets the
 * turn (ftParamSetStickLR), then dispatches to the fighter's SpecialN
 * setter. Verbatim. */
sb32 ftCommonSpecialNCheckInterruptCommon(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    FTAttributes *attr = fp->attr;

    if ((fp->input.pl.button_tap & fp->input.button_mask_b) && (attr->is_have_specialn))
    {
        if ((fp->input.pl.stick_range.y < FTCOMMON_SPECIALHI_STICK_RANGE_MIN) && (fp->input.pl.stick_range.y > FTCOMMON_SPECIALLW_STICK_RANGE_MIN))
        {
            if ((fp->input.pl.stick_range.x * fp->lr) < FTCOMMON_SPECIALN_TURN_STICK_RANGE_MIN)
            {
                ftParamSetStickLR(fp);
            }
            dFTCommonSpecialNStatusList[fp->fkind](fighter_gobj);

            return TRUE;
        }
    }
    return FALSE;
}

/* ftcommonspecialhi.c:10-39 dFTCommonSpecialHiStatusList 0x80188CE0.
 * The Up-B command demux: 27 setters indexed by FTKind, so pressing B with
 * the stick held up reaches the fighter's own SpecialHi setter -- Mario's is
 * the Super Jump Punch. Verbatim, every slot filled. NOTE the one place
 * this table differs from the Neutral-B one: NYoshi (20) is NOT Mario
 * here -- the decomp gives it Yoshi's own ftYoshiSpecialHiSetStatus. */
void (*dFTCommonSpecialHiStatusList[nFTKindEnumCount])(GObj*) =
{
    ftMarioSpecialHiSetStatus,  /*  0 Mario   */
    ftFoxSpecialHiStartSetStatus, /*  1 Fox   */
    ftDonkeySpecialHiSetStatus, /*  2 Donkey  */
    ftSamusSpecialHiSetStatus,  /*  3 Samus   */
    ftMarioSpecialHiSetStatus,  /*  4 Luigi   */
    ftLinkSpecialHiSetStatus,   /*  5 Link    */
    ftYoshiSpecialHiSetStatus,  /*  6 Yoshi   */
    ftCaptainSpecialHiSetStatus, /*  7 Captain */
    ftKirbySpecialHiSetStatus,  /*  8 Kirby   */
    ftPikachuSpecialHiStartSetStatus, /*  9 Pikachu */
    ftPurinSpecialHiSetStatus,  /* 10 Purin   */
    ftNessSpecialHiStartSetStatus, /* 11 Ness */
    ftMarioSpecialHiSetStatus,  /* 12 Boss    */
    ftMarioSpecialHiSetStatus,  /* 13 MMario  */
    ftMarioSpecialHiSetStatus,  /* 14 NMario  */
    ftFoxSpecialHiStartSetStatus, /* 15 NFox  */
    ftDonkeySpecialHiSetStatus, /* 16 NDonkey */
    ftSamusSpecialHiSetStatus,  /* 17 NSamus  */
    ftMarioSpecialHiSetStatus,  /* 18 NLuigi  */
    ftLinkSpecialHiSetStatus,   /* 19 NLink   */
    ftYoshiSpecialHiSetStatus,  /* 20 NYoshi Yoshi's own here, NOT Mario) */
    ftCaptainSpecialHiSetStatus, /* 21 NCaptain */
    ftKirbySpecialHiSetStatus,  /* 22 NKirby  */
    ftPikachuSpecialHiStartSetStatus, /* 23 NPikachu */
    ftPurinSpecialHiSetStatus,  /* 24 NPurin  */
    ftNessSpecialHiStartSetStatus, /* 25 NNess */
    ftDonkeySpecialHiSetStatus, /* 26 GDonkey */
};

/* ftcommonspecialhi.c:48-60 ftCommonSpecialHiCheckInterruptCommon 0x80151160:
 * the B tap with the stick held up, on a fighter that has an Up-B, dispatches
 * to the fighter's SpecialHi setter. Simpler than the Neutral-B check -- no
 * facing turn, one stick test (y at or above the up threshold). Verbatim. */
sb32 ftCommonSpecialHiCheckInterruptCommon(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    FTAttributes *attr = fp->attr;

    if ((fp->input.pl.button_tap & fp->input.button_mask_b) && (attr->is_have_specialhi) && (fp->input.pl.stick_range.y >= FTCOMMON_SPECIALHI_STICK_RANGE_MIN))
    {
        dFTCommonSpecialHiStatusList[fp->fkind](fighter_gobj);

        return TRUE;
    }
    else return FALSE;
}

/* ---- ft/ftcommon/ftcommonspecialair.c -- the AERIAL command demux, the
 * last of the three and the twin of the two ground demuxes above (
 * steps 33/35). One B tap while airborne, read against the stick's
 * vertical, reaches the fighter's own SpecialAir setter. Until this step
 * every air special the port had built -- eight fighters' worth by
 * -- was reachable only through its ground path (the ground
 * setter's own air branch) or a direct call from the disc probe. ---- */

/* ftcommonspecialair.c:42-71 dFTCommonSpecialAirNStatusList 0x80188ABC.
 * The airborne Neutral-B: 27 setters indexed by FTKind, verbatim. Mario's
 * setter also serves 4 Luigi, 12 Boss, 13 Metal Mario, 14 polygon Mario,
 * 18 NLuigi and 20 NYoshi -- the decomp's own "more bro momento", the
 * aerial twin of the ground table's. Kirby (8) and NKirby (22) go through
 * ftKirbySpecialAirNSetStatusSelect to the copy table above. */
void (*dFTCommonSpecialAirNStatusList[nFTKindEnumCount])(GObj*) =
{
    ftMarioSpecialAirNSetStatus,       /*  0 Mario   */
    ftFoxSpecialAirNSetStatus,         /*  1 Fox     */
    ftDonkeySpecialAirNStartSetStatus, /*  2 Donkey  */
    ftSamusSpecialAirNStartSetStatus,  /*  3 Samus   */
    ftMarioSpecialAirNSetStatus,       /*  4 Luigi   */
    ftLinkSpecialAirNSetStatus,        /*  5 Link    */
    ftYoshiSpecialAirNSetStatus,       /*  6 Yoshi   */
    ftCaptainSpecialAirNSetStatus,     /*  7 Captain */
    ftKirbySpecialAirNSetStatusSelect, /*  8 Kirby   */
    ftPikachuSpecialAirNSetStatus,     /*  9 Pikachu */
    ftPurinSpecialAirNSetStatus,       /* 10 Purin   */
    ftNessSpecialAirNSetStatus,        /* 11 Ness */
    ftMarioSpecialAirNSetStatus,       /* 12 Boss    */
    ftMarioSpecialAirNSetStatus,       /* 13 MMario  */
    ftMarioSpecialAirNSetStatus,       /* 14 NMario  */
    ftFoxSpecialAirNSetStatus,         /* 15 NFox    */
    ftDonkeySpecialAirNStartSetStatus, /* 16 NDonkey */
    ftSamusSpecialAirNStartSetStatus,  /* 17 NSamus  */
    ftMarioSpecialAirNSetStatus,       /* 18 NLuigi  */
    ftLinkSpecialAirNSetStatus,        /* 19 NLink   */
    ftMarioSpecialAirNSetStatus,       /* 20 NYoshi  (more bro momento) */
    ftCaptainSpecialAirNSetStatus,     /* 21 NCaptain */
    ftKirbySpecialAirNSetStatusSelect, /* 22 NKirby  */
    ftPikachuSpecialAirNSetStatus,     /* 23 NPikachu */
    ftPurinSpecialAirNSetStatus,       /* 24 NPurin  */
    ftNessSpecialAirNSetStatus,        /* 25 NNess */
    ftDonkeySpecialAirNStartSetStatus, /* 26 GDonkey */
};

/* ftcommonspecialair.c:74-103 dFTCommonSpecialAirHiStatusList 0x80188B28.
 * The airborne Up-B, verbatim. NOTE the decomp's slot 12 (Boss): it is
 * ftMarioSpecialAir*N*SetStatus, not the Hi one -- an off-by-one-column
 * slip in the original table, kept here as everywhere else. NOTE also,
 * as in the ground Up-B table, that NYoshi (20) is NOT routed to Mario:
 * it gets Yoshi's own ftYoshiSpecialAirHiSetStatus. */
void (*dFTCommonSpecialAirHiStatusList[nFTKindEnumCount])(GObj*) =
{
    ftMarioSpecialAirHiSetStatus,      /*  0 Mario   */
    ftFoxSpecialAirHiStartSetStatus,   /*  1 Fox     */
    ftDonkeySpecialAirHiSetStatus,     /*  2 Donkey  */
    ftSamusSpecialAirHiSetStatus,      /*  3 Samus   */
    ftMarioSpecialAirHiSetStatus,      /*  4 Luigi   */
    ftLinkSpecialAirHiSetStatus,       /*  5 Link    */
    ftYoshiSpecialAirHiSetStatus,      /*  6 Yoshi   */
    ftCaptainSpecialAirHiSetStatus,    /*  7 Captain */
    ftKirbySpecialAirHiSetStatus,      /*  8 Kirby   */
    ftPikachuSpecialAirHiStartSetStatus, /*  9 Pikachu */
    ftPurinSpecialAirHiSetStatus,      /* 10 Purin   */
    ftNessSpecialAirHiStartSetStatus,  /* 11 Ness */
    ftMarioSpecialAirNSetStatus,       /* 12 Boss    (the decomp's own AirN in the Hi table) */
    ftMarioSpecialAirHiSetStatus,      /* 13 MMario  */
    ftMarioSpecialAirHiSetStatus,      /* 14 NMario  */
    ftFoxSpecialAirHiStartSetStatus,   /* 15 NFox    */
    ftDonkeySpecialAirHiSetStatus,     /* 16 NDonkey */
    ftSamusSpecialAirHiSetStatus,      /* 17 NSamus  */
    ftMarioSpecialAirHiSetStatus,      /* 18 NLuigi  */
    ftLinkSpecialAirHiSetStatus,       /* 19 NLink   */
    ftYoshiSpecialAirHiSetStatus,      /* 20 NYoshi Yoshi's own here, NOT Mario) */
    ftCaptainSpecialAirHiSetStatus,    /* 21 NCaptain */
    ftKirbySpecialAirHiSetStatus,      /* 22 NKirby  */
    ftPikachuSpecialAirHiStartSetStatus, /* 23 NPikachu */
    ftPurinSpecialAirHiSetStatus,      /* 24 NPurin  */
    ftNessSpecialAirHiStartSetStatus,  /* 25 NNess */
    ftDonkeySpecialAirHiSetStatus,     /* 26 GDonkey */
};

/* ftcommonspecialair.c:106-135 dFTCommonSpecialAirLwStatusList 0x80188B94.
 * The airborne Down-B, and the widest of the three: Donkey's slots (2, 16
 * and 26) are NULL in the GAME -- his Hand Slap has no aerial form at all
 * -- so those three NULLs are the decomp's, not a port hole, and
 * is_have_specialairlw is 0 in all three kinds' attributes, so nothing
 * calls through them. NYoshi (20) is routed to Yoshi here. */
void (*dFTCommonSpecialAirLwStatusList[nFTKindEnumCount])(GObj*) =
{
    ftMarioSpecialAirLwSetStatus,      /*  0 Mario   */
    ftFoxSpecialAirLwStartSetStatus,   /*  1 Fox     */
    NULL,                              /*  2 Donkey  (the GAME's NULL: no aerial Hand Slap) */
    ftSamusSpecialAirLwSetStatus,      /*  3 Samus   */
    ftMarioSpecialAirLwSetStatus,      /*  4 Luigi   */
    ftLinkSpecialAirLwSetStatus,       /*  5 Link */
    ftYoshiSpecialAirLwStartSetStatus, /*  6 Yoshi   */
    ftCaptainSpecialAirLwSetStatus,    /*  7 Captain */
    ftKirbySpecialAirLwStartSetStatus, /*  8 Kirby    */
    ftPikachuSpecialAirLwStartSetStatus, /*  9 Pikachu */
    ftPurinSpecialAirLwSetStatus,      /* 10 Purin   */
    ftNessSpecialAirLwStartSetStatus,  /* 11 Ness */
    ftMarioSpecialAirLwSetStatus,      /* 12 Boss    */
    ftMarioSpecialAirLwSetStatus,      /* 13 MMario  */
    ftMarioSpecialAirLwSetStatus,      /* 14 NMario  */
    ftFoxSpecialAirLwStartSetStatus,   /* 15 NFox    */
    NULL,                              /* 16 NDonkey (the GAME's NULL) */
    ftSamusSpecialAirLwSetStatus,      /* 17 NSamus  */
    ftMarioSpecialAirLwSetStatus,      /* 18 NLuigi  */
    ftLinkSpecialAirLwSetStatus,       /* 19 NLink */
    ftYoshiSpecialAirLwStartSetStatus, /* 20 NYoshi Yoshi's own here) */
    ftCaptainSpecialAirLwSetStatus,    /* 21 NCaptain */
    ftKirbySpecialAirLwStartSetStatus, /* 22 NKirby   */
    ftPikachuSpecialAirLwStartSetStatus, /* 23 NPikachu */
    ftPurinSpecialAirLwSetStatus,      /* 24 NPurin  */
    ftNessSpecialAirLwStartSetStatus,  /* 25 NNess */
    NULL,                              /* 26 GDonkey (the GAME's NULL) */
};

/* ftcommonspecialair.c:152-192 ftCommonSpecialAirCheckInterruptCommon
 * 0x80150F08: the B tap in the air. One function does all three
 * directions, unlike the ground pair -- the stick's vertical picks the
 * table (up, down, or the neutral band between them), each gated on the
 * fighter's own is_have_specialair* attribute, and the hammer swallows
 * the press outright. The neutral band alone turns the fighter first,
 * exactly as the ground Neutral-B check does. Verbatim. */
sb32 ftCommonSpecialAirCheckInterruptCommon(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    FTAttributes *attr = fp->attr;

    if (fp->input.pl.button_tap & fp->input.button_mask_b)
    {
        if (ftHammerCheckHoldHammer(fighter_gobj) == FALSE)
        {
            if (fp->input.pl.stick_range.y >= FTCOMMON_SPECIALHI_STICK_RANGE_MIN)
            {
                if (attr->is_have_specialairhi)
                {
                    dFTCommonSpecialAirHiStatusList[fp->fkind](fighter_gobj);

                    return TRUE;
                }
            }
            else if (fp->input.pl.stick_range.y <= FTCOMMON_SPECIALLW_STICK_RANGE_MIN)
            {
                if (attr->is_have_specialairlw)
                {
                    dFTCommonSpecialAirLwStatusList[fp->fkind](fighter_gobj);

                    return TRUE;
                }
            }
            else if (attr->is_have_specialairn)
            {
                if ((fp->input.pl.stick_range.x * fp->lr) < FTCOMMON_SPECIALN_TURN_STICK_RANGE_MIN)
                {
                    ftParamSetStickLR(fp);
                }
                dFTCommonSpecialAirNStatusList[fp->fkind](fighter_gobj);

                return TRUE;
            }
        }
    }
    return FALSE;
}

/* ftcommonrun.c:10-25 ftCommonRunProcInterrupt 0x8013EE50, verbatim.
 * Handles SpecialN, the dash attack, the appeal, and the run grab. */
void ftCommonRunProcInterrupt(GObj *fighter_gobj)
{
    if
    (
        (ftCommonSpecialNCheckInterruptCommon(fighter_gobj) == FALSE)    &&
        (ftCommonCatchCheckInterruptDashRun(fighter_gobj) == FALSE)      &&
        (ftCommonAttackDashCheckInterruptCommon(fighter_gobj) == FALSE)  &&
        (ftCommonGuardOnCheckInterruptDashRun(fighter_gobj, 4) == FALSE) &&
        (ftCommonAppealCheckInterruptCommon(fighter_gobj) == FALSE)      &&
        (ftCommonKneeBendCheckInterruptRun(fighter_gobj) == FALSE)       &&
        (ftCommonTurnRunCheckInterruptRun(fighter_gobj) == FALSE)
    )
    {
        ftCommonRunBrakeCheckInterruptRun(fighter_gobj);
    }
}

/* ftcommonrun.c:28-35 ftCommonRunSetStatus 0x8013EEE8 */
void ftCommonRunSetStatus(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    ftMainSetStatus(fighter_gobj, nFTCommonStatusRun, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);

    fp->physics.vel_ground.x = fp->attr->run_speed;
}

/* ftcommonrun.c:38-56 ftCommonRunCheckInterruptDash 0x8013EF2C: the
 * dash becomes a run on exactly one frame -- the first at or past
 * dash_to_run, which is why the window is a frame of animation speed
 * wide -- and only with the stick still held forward */
sb32 ftCommonRunCheckInterruptDash(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    FTAttributes *attr = fp->attr;

    if (attr->dash_to_run <= fighter_gobj->anim_frame)
    {
        if (fighter_gobj->anim_frame < (attr->dash_to_run + DObjGetStruct(fighter_gobj)->anim_speed))
        {
            if ((fp->input.pl.stick_range.x * fp->lr) >= FTCOMMON_RUN_STICK_RANGE_MIN)
            {
                ftCommonRunSetStatus(fighter_gobj);

                return TRUE;
            }
        }
    }
    return FALSE;
}

/* ftcommonrunbrake.c:66-74 ftCommonRunBrakeProcInterrupt 0x8013EFB0.
 * DIVERGES: none -- both its checks are ported. */
void ftCommonRunBrakeProcInterrupt(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if ((ftCommonKneeBendCheckInterruptRun(fighter_gobj) == FALSE) && (fp->motion_vars.flags.flag1 != 0) && (fighter_gobj->anim_frame <= 4.0F))
    {
        ftCommonTurnRunCheckInterruptRun(fighter_gobj);
    }
}

/* ftcommonrunbrake.c:77-84 ftCommonRunBrakeProcPhysics 0x8013F014 */
void ftCommonRunBrakeProcPhysics(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    FTAttributes *attr = fp->attr;

    ftPhysicsSetGroundVelFriction(fp, attr->traction * 1.25F);
    ftPhysicsSetGroundVelTransferAir(fighter_gobj);
}

/* ftcommonrunbrake.c:87-94 ftCommonRunBrakeSetStatus 0x8013F05C: flag
 * is whether the brake may still be turned out of */
void ftCommonRunBrakeSetStatus(GObj *fighter_gobj, u32 flag)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    ftMainSetStatus(fighter_gobj, nFTCommonStatusRunBrake, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);

    fp->motion_vars.flags.flag1 = flag;
}

/* ftcommonrunbrake.c:97-108 ftCommonRunBrakeCheckInterruptRun 0x8013F0A0 */
sb32 ftCommonRunBrakeCheckInterruptRun(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if ((fp->input.pl.stick_range.x * fp->lr) < FTCOMMON_RUN_STICK_RANGE_MIN)
    {
        ftCommonRunBrakeSetStatus(fighter_gobj, 1);

        return TRUE;
    }
    else return FALSE;
}

/* ftcommonrunbrake.c:111-129 ftCommonRunBrakeCheckInterruptTurnRun
 * 0x8013F0EC. The REGION_US arm is in: this build is the USA ROM's
 * (docker/patches and tools/check/reloc_check.py pin it), and the clamp is
 * that version's fix for the slide a turn-run out of a brake used to
 * keep. */
sb32 ftCommonRunBrakeCheckInterruptTurnRun(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if (ABS(fp->input.pl.stick_range.x) < FTCOMMON_RUN_STICK_RANGE_MIN)
    {
        ftCommonRunBrakeSetStatus(fighter_gobj, 0);

        if (fp->physics.vel_ground.x > fp->attr->run_speed)
        {
            fp->physics.vel_ground.x = fp->attr->run_speed;
        }
        return TRUE;
    }
    else return FALSE;
}

/* ftcommonturnrun.c:139-151 ftCommonTurnRunProcUpdate 0x8013F170: the
 * flip is the animation script's flag1 event, as the roll's is, and it
 * turns the run's velocity around with the fighter */
void ftCommonTurnRunProcUpdate(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if (fp->motion_vars.flags.flag1 != 0)
    {
        fp->motion_vars.flags.flag1 = 0;

        fp->lr = -fp->lr;
        fp->physics.vel_ground.x = -fp->physics.vel_ground.x;
    }
    ftAnimEndCheckSetStatus(fighter_gobj, ftCommonRunSetStatus);
}

/* ftcommonturnrun.c:154-162 ftCommonTurnRunProcInterrupt 0x8013F1C0:
 * flag2, the script's second event, is what lets the turn-run be braked
 * out of */
void ftCommonTurnRunProcInterrupt(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if ((ftCommonKneeBendCheckInterruptRun(fighter_gobj) == FALSE) && (fp->motion_vars.flags.flag2 != 0))
    {
        ftCommonRunBrakeCheckInterruptTurnRun(fighter_gobj);
    }
}

/* ftcommonturnrun.c:165-173 ftCommonTurnRunSetStatus 0x8013F208 */
void ftCommonTurnRunSetStatus(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    ftMainSetStatus(fighter_gobj, nFTCommonStatusTurnRun, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);

    fp->motion_vars.flags.flag1 = 0;
    fp->motion_vars.flags.flag2 = 0;
}

/* ftcommonturnrun.c:176-187 ftCommonTurnRunCheckInterruptRun 0x8013F248 */
sb32 ftCommonTurnRunCheckInterruptRun(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if ((fp->input.pl.stick_range.x * fp->lr) <= FTCOMMON_TURNRUN_STICK_RANGE_MIN)
    {
        ftCommonTurnRunSetStatus(fighter_gobj);

        return TRUE;
    }
    else return FALSE;
}

/* ---- ft/ftcommon/ftcommonsquat.c and ftcommonpass.c ----------------- */

/* ---- ft/ftcommon/ftcommonottotto.c -- the teeter at a ledge --------- */

/* ftcommonottotto.c:38-41 ftCommonOttottoProcUpdate 0x80142850 */
void ftCommonOttottoWaitSetStatus(GObj *fighter_gobj);

void ftCommonOttottoProcUpdate(GObj *fighter_gobj)
{
    ftAnimEndCheckSetStatus(fighter_gobj, ftCommonOttottoWaitSetStatus);
}

/* ftcommonottotto.c:44-56 ftCommonOttottoProcInterrupt 0x80142874.
 * The ftCommonOttottoCheckInterrupt macro (lines 7-30), unrolled in the
 * game's order; the attack cascade is the ground one. */
void ftCommonOttottoProcInterrupt(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if (ftCommonSpecialNCheckInterruptCommon(fighter_gobj) != FALSE)
    {
        return;
    }
    if (ftCommonSpecialHiCheckInterruptCommon(fighter_gobj) != FALSE)
    {
        return;
    }
    if (ftCommonSpecialLwCheckInterruptCommon(fighter_gobj) != FALSE)
    {
        return;
    }
    /* the grab, the ground cascade's slot: after the last special,
 * before the attacks */
    if (ftCommonCatchCheckInterruptCommon(fighter_gobj) != FALSE)
    {
        return;
    }
    if (ftCommonAttackCheckInterruptGround(fighter_gobj, FTCOMMON_GROUNDATTACK_COMMON) != FALSE)
    {
        return;
    }
    if (ftCommonGuardOnCheckInterruptCommon(fighter_gobj) != FALSE)
    {
        return;
    }
    if (ftCommonAppealCheckInterruptCommon(fighter_gobj) != FALSE)
    {
        return;
    }
    if (ftCommonKneeBendCheckInterruptCommon(fighter_gobj) != FALSE)
    {
        return;
    }
    if (ftCommonDashCheckInterruptCommon(fighter_gobj) != FALSE)
    {
        return;
    }
    if (ftCommonPassCheckInterruptCommon(fighter_gobj) != FALSE)
    {
        return;
    }
    if (ftCommonDokanStartCheckInterruptCommon(fighter_gobj) != FALSE)
    {
        return;
    }
    if (ftCommonSquatCheckInterruptCommon(fighter_gobj) != FALSE)
    {
        return;
    }
    if (ftCommonTurnCheckInterruptCommon(fighter_gobj) != FALSE)
    {
        return;
    }
    if ((fp->input.pl.stick_range.x * fp->lr) >= FTCOMMON_OTTOTTO_WALK_STICK_RANGE_MIN)
    {
        ftCommonWalkCheckInterruptCommon(fighter_gobj);
    }
}

/* ftcommonottotto.c:58-87 ftCommonOttottoProcMap 0x801429F4: hold the
 * teeter until the fighter is well clear of the edge again */
void ftCommonOttottoProcMap(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    Vec3f pos;
    f32 dist_x;

    if (mpCommonCheckFighterOnFloor(fighter_gobj) == FALSE)
    {
        ftCommonFallSetStatus(fighter_gobj);
    }
    else
    {
        if (fp->lr == +1)
        {
            mpCollisionGetFloorEdgeR(fp->coll_data.floor_line_id, &pos);
        }
        else mpCollisionGetFloorEdgeL(fp->coll_data.floor_line_id, &pos);

        if (DObjGetStruct(fighter_gobj)->translate.vec.f.x < pos.x)
        {
            dist_x = -(DObjGetStruct(fighter_gobj)->translate.vec.f.x - pos.x);
        }
        else dist_x = DObjGetStruct(fighter_gobj)->translate.vec.f.x - pos.x;

        if (dist_x > FTCOMMON_OTTOTTO_WALK_DIST_X_MIN)
        {
            ftCommonWaitSetStatus(fighter_gobj);
        }
    }
}

/* ftcommonottotto.c:90-97 ftCommonOttottoWaitSetStatus 0x80142AC4 */
void ftCommonOttottoWaitSetStatus(GObj *fighter_gobj)
{
    ftMainSetStatus(fighter_gobj, nFTCommonStatusOttottoWait, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);
}

/* ftcommonottotto.c:100-110 ftCommonOttottoSetStatus 0x80142B08 */
void ftCommonOttottoSetStatus(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    ftMainSetStatus(fighter_gobj, nFTCommonStatusOttotto, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);

    fp->physics.vel_air.x = fp->physics.vel_air.y = fp->physics.vel_air.z = 0.0F;
    fp->physics.vel_ground.x = 0.0F;
}

/* ---- ft/ftcommon/ftcommonstopceil.c: not here any more --------------
 * The file is compiled unmodified. The copy that stood
 * here had dropped its ftMainPlayAnimEventsAll call with no marker; the
 * head bonk now plays the motion's events on entry, as the game's does.
 * -------------------------------------------------------------------- */

/* ---- ft/ftcommon/ftcommoncliffcatchwait.c -- the ledge hang -------- */

/* ftcommoncliffcatchwait.c:10-13 ftCommonCliffCatchProcUpdate 0x80144B30 */
void ftCommonCliffCatchProcUpdate(GObj *fighter_gobj)
{
    ftAnimEndCheckSetStatus(fighter_gobj, ftCommonCliffWaitSetStatus);
}

/* ftcommoncliffcatchwait.c:16-31 ftCommonCliffCommonProcPhysics
 * 0x80144B54: the hang pose is placed relative to the ledge vertex, the
 * animation's TransN translate supplying the offset (z into x, y into
 * y), scaled by the model size. */
void ftCommonCliffCommonProcPhysics(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    f32 scale = fp->attr->size;
    Vec3f vel;

    if (fp->lr == +1)
    {
        mpCollisionGetFloorEdgeL(fp->coll_data.cliff_id, &vel);
    }
    else mpCollisionGetFloorEdgeR(fp->coll_data.cliff_id, &vel);

    DObjGetStruct(fighter_gobj)->translate.vec.f.x = ((fp->joints[nFTPartsJointTransN]->translate.vec.f.z * fp->lr * scale) + vel.x);
    DObjGetStruct(fighter_gobj)->translate.vec.f.y = ((fp->joints[nFTPartsJointTransN]->translate.vec.f.y * scale) + vel.y);
}

/* ftcommoncliffcatchwait.c:33-36 ftCommonCliffCommonProcMap 0x80144C1C:
 * a fighter on a ledge runs no collision at all */
void ftCommonCliffCommonProcMap(GObj *fighter_gobj)
{
    (void)fighter_gobj;

    return;
}

/* ftcommoncliffcatchwait.c:39-67 ftCommonCliffCatchSetStatus 0x80144C24,
 * verbatim since put back the ledge-grab flash, the damage
 * hook and the barrel-cannon immunity. */
void ftCommonCliffCatchSetStatus(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    Vec3f pos;

    mpCommonSetFighterGround(fp);
    ftMainSetStatus(fighter_gobj, nFTCommonStatusCliffCatch, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);
    ftMainPlayAnimEventsAll(fighter_gobj);
    mpCommonSetFighterAir(fp);
    ftPhysicsStopVelAll(fighter_gobj);

    fp->coll_data.floor_line_id = -1;

    ftCommonCliffCommonProcPhysics(fighter_gobj);

    fp->is_cliff_hold = TRUE;

    if (fp->lr == +1)
    {
        mpCollisionGetFloorEdgeL(fp->coll_data.cliff_id, &pos);
    }
    else mpCollisionGetFloorEdgeR(fp->coll_data.cliff_id, &pos);

    efManagerFlashMiddleMakeEffect(&pos);

    fp->proc_damage = ftCommonCliffCommonProcDamage;

    ftParamSetCaptureImmuneMask(fp, FTCATCHKIND_MASK_TARUCANN);
}

/* ftcommoncliffcatchwait.c:70-87 ftCommonCliffCommonProcDamage
 * 0x80144CF8: step off the ledge back onto the map, one collision pass
 * from beside it */
void ftCommonCliffCommonProcDamage(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    MPObjectColl *map_coll = &fp->coll_data.map_coll;
    Vec3f pos;

    if (fp->lr == +1)
    {
        mpCollisionGetFloorEdgeL(fp->coll_data.cliff_id, &pos);
    }
    else mpCollisionGetFloorEdgeR(fp->coll_data.cliff_id, &pos);

    pos.x -= ((map_coll->width + 30.0F) * fp->lr);
    pos.y -= map_coll->center;

    mpCommonRunFighterCollisionDefault(fighter_gobj, &pos, &fp->coll_data);
}

/* ftcommoncliffcatchwait.c:98-119 ftCommonCliffWaitSetStatus 0x80144DF4,
 * verbatim: the player-tag timer is back since, the damage
 * hook and capture immunity since G13. */
void ftCommonCliffWaitSetStatus(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    ftMainSetStatus(fighter_gobj, nFTCommonStatusCliffWait, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);

    fp->status_vars.common.cliffwait.is_allow_interrupt = FALSE;

    if (fp->percent_damage < FTCOMMON_CLIFF_DAMAGE_HIGH)
    {
        fp->status_vars.common.cliffwait.fall_wait = FTCOMMON_CLIFF_FALL_WAIT_DAMAGE_LOW;
    }
    else fp->status_vars.common.cliffwait.fall_wait = FTCOMMON_CLIFF_FALL_WAIT_DAMAGE_HIGH;

    fp->is_cliff_hold = TRUE;

    ftParamSetPlayerTagWait(fighter_gobj, 120);

    fp->proc_damage = ftCommonCliffCommonProcDamage;

    ftParamSetCaptureImmuneMask(fp, FTCATCHKIND_MASK_TARUCANN);
}

/* ftcommoncliffcatchwait.c:122-137 ftCommonCliffWaitCheckFall 0x80144E84,
 * verbatim as of a fighter whose ledge-hang timer runs out
 * now drops into the DamageFall tumble it can tech out of, not a plain
 * Fall (the DIVERGES this carried is gone -- DamageFall exists). */
sb32 ftCommonCliffWaitCheckFall(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    fp->status_vars.common.cliffwait.fall_wait--;

    if (fp->status_vars.common.cliffwait.fall_wait == 0)
    {
        fp->cliffcatch_wait = FTCOMMON_CLIFF_CATCH_WAIT;

        ftCommonCliffCommonProcDamage(fighter_gobj);
        ftCommonDamageFallSetStatusFromCliffWait(fighter_gobj);

        return TRUE;
    }
    else return FALSE;
}

/* ---- ft/ftcommon/ftcommoncliffclimb.c and ftcommoncliffescape.c --
 * the release from the ledge. Every one is a two-part animation: part 1
 * plays hanging (the fighter still pinned to the ledge vertex by
 * ftCommonCliffCommonProcPhysics), part 2 is repositioned onto the
 * ledge and then moved by the animation's own TransN root motion.
 * ------------------------------------------------------------------- */

/* ftcommoncliffclimb.c:200-203 ftCommonCliffCommon2ProcUpdate 0x80145290 */
void ftCommonCliffCommon2ProcUpdate(GObj *fighter_gobj)
{
    ftAnimEndCheckSetStatus(fighter_gobj, mpCommonSetFighterWaitOrFall);
}

/* ftcommoncliffclimb.c:206-244 ftCommonCliffCommon2ProcPhysics
 * 0x801452B4. Grounded, the root motion becomes ground velocity and the
 * floor angle carries it. Airborne, the game does not integrate at all:
 * it walks the animated position forward, adds the cliff line's own
 * speed so a moving platform takes the fighter with it, drops it onto
 * that line's floor and makes the velocity whatever gets it there --
 * which is how a roll follows a slope exactly.
 * DIVERGES: line 189's `pos.y += TransN.y` is the raw joint translate,
 * unscaled, where the hang (ftcommoncliffcatchwait.c:30) scales the same
 * value by TopN. The port keeps the decomp's asymmetry rather than
 * "fixing" it. */
void ftCommonCliffCommon2ProcPhysics(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    Vec3f pos;
    Vec3f vel;
    Vec3f *translate;
    f32 y;

    if (fp->ga == nMPKineticsGround)
    {
        ftPhysicsApplyGroundVelTransN(fighter_gobj);
    }
    else
    {
        translate = &DObjGetStruct(fighter_gobj)->translate.vec.f;

        pos = *translate;

        ftPhysicsGetAirVelTransN(fp, &vel.x, NULL, &vel.z);

        pos.x += vel.x;
        pos.z += vel.z;

        mpCollisionGetSpeedLineID(fp->status_vars.common.cliffmotion.cliff_id, &vel);

        pos.x += vel.x;

        if (mpCollisionGetFCCommonFloor(fp->status_vars.common.cliffmotion.cliff_id, &pos, &y, NULL, NULL) != FALSE)
        {
            pos.y += y;

            pos.y += fp->joints[nFTPartsJointTransN]->translate.vec.f.y;

            fp->physics.vel_air.x = pos.x - translate->x;
            fp->physics.vel_air.y = pos.y - translate->y;
            fp->physics.vel_air.z = pos.z - translate->z;
        }
        else ftPhysicsApplyAirVelTransNAll(fighter_gobj);
    }
}

/* ftcommoncliffclimb.c:247-258 ftCommonCliffClimbCommon2ProcMap
 * 0x801453F0: a climb-up that is grounded falls if its ground goes
 * away, and one that is airborne lands when it reaches the floor */
void ftCommonCliffClimbCommon2ProcMap(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if (fp->ga == nMPKineticsGround)
    {
        mpCommonSetFighterFallOnGroundBreak(fighter_gobj);
    }
    else if (mpCommonCheckFighterLanding(fighter_gobj) != FALSE)
    {
        mpCommonSetFighterGround(fp);
    }
}

/* ftcommoncliffclimb.c:261-272 ftCommonCliffAttackEscape2ProcMap
 * 0x80145440: as above, but a roll runs off the *edge* rather than
 * breaking on the ground, so it can roll off the far side */
void ftCommonCliffAttackEscape2ProcMap(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if (fp->ga == nMPKineticsGround)
    {
        mpCommonSetFighterFallOnEdgeBreak(fighter_gobj);
    }
    else if (mpCommonCheckFighterLanding(fighter_gobj) != FALSE)
    {
        mpCommonSetFighterGround(fp);
    }
}

/* ftcommoncliffclimb.c:275-301 ftCommonCliffCommon2UpdateCollData
 * 0x80145490: leave the ledge and stand on it -- 5 units in from the
 * vertex, on the cliff's own line, snapped down onto its floor. Whether
 * the fighter is grounded there is the per-fighter cliff_status_ga
 * table's call (fttypes.h:951); Mario's ledge attack is the one entry
 * that says airborne. */
void ftCommonCliffCommon2UpdateCollData(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    MPCollData *coll_data = &fp->coll_data;
    Vec3f *translate = &DObjGetStruct(fighter_gobj)->translate.vec.f;

    if (fp->attr->cliff_status_ga[fp->status_vars.common.cliffmotion.status_id] == nMPKineticsGround)
    {
        mpCommonSetFighterGround(fp);
    }
    if (fp->lr == +1)
    {
        mpCollisionGetFloorEdgeL(coll_data->cliff_id, translate);

        translate->x += 5.0F;
    }
    else
    {
        mpCollisionGetFloorEdgeR(coll_data->cliff_id, translate);

        translate->x -= 5.0F;
    }
    coll_data->floor_line_id = coll_data->cliff_id;

    mpCollisionGetFCCommonFloor(coll_data->floor_line_id, translate, &coll_data->floor_dist, &coll_data->floor_flags, &coll_data->floor_angle);

    translate->y += coll_data->floor_dist;

    coll_data->floor_dist = 0.0F;
}

/* ftcommoncliffclimb.c:304-312 ftCommonCliffCommon2InitStatusVars
 * 0x8014557C */
void ftCommonCliffCommon2InitStatusVars(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if (fp->ga == nMPKineticsGround)
    {
        fp->is_jostle_ignore = TRUE;
    }
}

/* ftcommoncliffclimb.c:315-321 ftCommonCliffClimbQuick2SetStatus
 * 0x801455A0 */
void ftCommonCliffClimbQuick2SetStatus(GObj *fighter_gobj)
{
    ftCommonCliffCommon2UpdateCollData(fighter_gobj);
    ftMainSetStatus(fighter_gobj, nFTCommonStatusCliffClimbQuick2, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);
    ftCommonCliffCommon2InitStatusVars(fighter_gobj);
}

/* ftcommoncliffclimb.c:324-330 ftCommonCliffClimbSlow2SetStatus
 * 0x801455E0 */
void ftCommonCliffClimbSlow2SetStatus(GObj *fighter_gobj)
{
    ftCommonCliffCommon2UpdateCollData(fighter_gobj);
    ftMainSetStatus(fighter_gobj, nFTCommonStatusCliffClimbSlow2, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);
    ftCommonCliffCommon2InitStatusVars(fighter_gobj);
}

/* ftcommoncliffescape.c:59-65 ftCommonCliffEscapeQuick2SetStatus
 * 0x8014590C */
void ftCommonCliffEscapeQuick2SetStatus(GObj *fighter_gobj)
{
    ftCommonCliffCommon2UpdateCollData(fighter_gobj);
    ftMainSetStatus(fighter_gobj, nFTCommonStatusCliffEscapeQuick2, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);
    ftCommonCliffCommon2InitStatusVars(fighter_gobj);
}

/* ftcommoncliffescape.c:68-73 ftCommonCliffEscapeSlow2SetStatus
 * 0x8014594C */
void ftCommonCliffEscapeSlow2SetStatus(GObj *fighter_gobj)
{
    ftCommonCliffCommon2UpdateCollData(fighter_gobj);
    ftMainSetStatus(fighter_gobj, nFTCommonStatusCliffEscapeSlow2, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);
    ftCommonCliffCommon2InitStatusVars(fighter_gobj);
}

/* ftcommoncliffclimb.c:113-116 0x801451A8 and 119-122 0x801451CC */
void ftCommonCliffClimbQuick1ProcUpdate(GObj *fighter_gobj)
{
    ftAnimEndCheckSetStatus(fighter_gobj, ftCommonCliffClimbQuick2SetStatus);
}

void ftCommonCliffClimbSlow1ProcUpdate(GObj *fighter_gobj)
{
    ftAnimEndCheckSetStatus(fighter_gobj, ftCommonCliffClimbSlow2SetStatus);
}

/* ftcommoncliffescape.c:23-26 0x80145824 and 29-32 0x80145848 */
void ftCommonCliffEscapeQuick1ProcUpdate(GObj *fighter_gobj)
{
    ftAnimEndCheckSetStatus(fighter_gobj, ftCommonCliffEscapeQuick2SetStatus);
}

void ftCommonCliffEscapeSlow1ProcUpdate(GObj *fighter_gobj)
{
    ftAnimEndCheckSetStatus(fighter_gobj, ftCommonCliffEscapeSlow2SetStatus);
}

/* ftcommoncliffclimb.c:125-150 ftCommonCliffClimbQuick1SetStatus
 * 0x801451F0 and ftCommonCliffClimbSlow1SetStatus 0x80145240;
 * ftcommoncliffescape.c:35-56 the two escape halves, verbatim,
 * each sets fp->proc_damage: the hook that steps a
 * fighter hit off the ledge back beside it before the knockback. */
void ftCommonCliffClimbQuick1SetStatus(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    ftMainSetStatus(fighter_gobj, nFTCommonStatusCliffClimbQuick1, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);

    fp->is_cliff_hold = TRUE;

    fp->proc_damage = ftCommonCliffCommonProcDamage;
}

void ftCommonCliffClimbSlow1SetStatus(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    ftMainSetStatus(fighter_gobj, nFTCommonStatusCliffClimbSlow1, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);

    fp->is_cliff_hold = TRUE;

    fp->proc_damage = ftCommonCliffCommonProcDamage;
}

void ftCommonCliffEscapeQuick1SetStatus(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    ftMainSetStatus(fighter_gobj, nFTCommonStatusCliffEscapeQuick1, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);

    fp->is_cliff_hold = TRUE;

    fp->proc_damage = ftCommonCliffCommonProcDamage;
}

void ftCommonCliffEscapeSlow1SetStatus(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    ftMainSetStatus(fighter_gobj, nFTCommonStatusCliffEscapeSlow1, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);

    fp->is_cliff_hold = TRUE;

    fp->proc_damage = ftCommonCliffCommonProcDamage;
}

/* ftcommoncliffclimb.c:9-54 ftCommonCliffQuickProcUpdate 0x80144EE0 and
 * ftCommonCliffSlowProcUpdate 0x80144F64: the wind-up animation ends and
 * whichever release the stick or the Z press asked for begins. All three
 * arms are the game's. The ledge attack functions are supplied by
 * ft/ftcommon/ftcommoncliffattack.c. */
void ftCommonCliffQuickProcUpdate(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if (fighter_gobj->anim_frame <= 0.0F)
    {
        switch (fp->status_vars.common.cliffmotion.status_id)
        {
        case nFTCommonCliffKindClimbQuick:
            ftCommonCliffClimbQuick1SetStatus(fighter_gobj);
            break;

        case nFTCommonCliffKindAttackQuick:
            ftCommonCliffAttackQuick1SetStatus(fighter_gobj);
            break;

        case nFTCommonCliffKindEscapeQuick:
            ftCommonCliffEscapeQuick1SetStatus(fighter_gobj);
            break;
        }
    }
}

void ftCommonCliffSlowProcUpdate(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if (fighter_gobj->anim_frame <= 0.0F)
    {
        switch (fp->status_vars.common.cliffmotion.status_id)
        {
        case nFTCommonCliffKindClimbSlow:
            ftCommonCliffClimbSlow1SetStatus(fighter_gobj);
            break;

        case nFTCommonCliffKindAttackSlow:
            ftCommonCliffAttackSlow1SetStatus(fighter_gobj);
            break;

        case nFTCommonCliffKindEscapeSlow:
            ftCommonCliffEscapeSlow1SetStatus(fighter_gobj);
            break;
        }
    }
}

/* ftcommoncliffclimb.c:57-77 ftCommonCliffQuickOrSlowSetStatus
 * 0x80144FE8: one wind-up status for all three releases, with which
 * release it was remembered in cliffmotion.status_id -- the index
 * cliff_status_ga is read with. Above 100% damage it is the slow one.
 * Verbatim since put back its proc_damage. */
void ftCommonCliffQuickOrSlowSetStatus(GObj *fighter_gobj, s32 status_input)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    s32 status_id;
    s32 status_queue;

    if (fp->percent_damage < FTCOMMON_CLIFF_DAMAGE_HIGH)
    {
        status_id = nFTCommonStatusCliffQuick, status_queue = nFTCommonCliffKindClimbQuick;
    }
    else status_id = nFTCommonStatusCliffSlow, status_queue = nFTCommonCliffKindClimbSlow;

    ftMainSetStatus(fighter_gobj, status_id, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);
    ftMainPlayAnimEventsAll(fighter_gobj);

    fp->status_vars.common.cliffmotion.status_id = status_input + status_queue;
    fp->status_vars.common.cliffmotion.cliff_id = fp->coll_data.cliff_id;

    fp->is_cliff_hold = TRUE;

    fp->proc_damage = ftCommonCliffCommonProcDamage;
}

/* ftcommoncliffclimb.c:82-118 ftCommonCliffClimbOrFallCheckInterruptCommon
 * 0x80145084: stick up or toward the stage climbs, stick away drops */
sb32 ftCommonCliffClimbOrFallCheckInterruptCommon(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if ((ABS(fp->input.pl.stick_range.x) >= FTCOMMON_CLIFF_MOTION_STICK_RANGE_MIN) || (ABS(fp->input.pl.stick_range.y) >= FTCOMMON_CLIFF_MOTION_STICK_RANGE_MIN))
    {
        f32 angle = ftParamGetStickAngleRads(fp);

        if ((angle > F_CST_DTOR32(50.0F)) || ((angle > F_CST_DTOR32(-50.0F)) && ((fp->input.pl.stick_range.x * fp->lr) >= 0)))
        {
            if (fp->status_vars.common.cliffwait.is_allow_interrupt != FALSE)
            {
                ftCommonCliffQuickOrSlowSetStatus(fighter_gobj, 0);

                return TRUE;
            }
            else return FALSE;
        }
        else if (fp->status_vars.common.cliffwait.is_allow_interrupt != FALSE)
        {
            fp->cliffcatch_wait = FTCOMMON_CLIFF_CATCH_WAIT;

            ftCommonCliffCommonProcDamage(fighter_gobj);
            ftCommonFallSetStatus(fighter_gobj);

            return TRUE;
        }
        else return FALSE;
    }
    else fp->status_vars.common.cliffwait.is_allow_interrupt = TRUE;

    return FALSE;
}

/* ftcommoncliffescape.c:9-20 ftCommonCliffEscapeCheckInterruptCommon
 * 0x801457E0: Z rolls up onto the stage. `status_input` 2 is
 * nFTCommonCliffKindEscape{Quick,Slow} once the damage tier picks the
 * base (ftcommoncliffclimb.c:69-71). */
sb32 ftCommonCliffEscapeCheckInterruptCommon(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if (fp->input.pl.button_tap & fp->input.button_mask_z)
    {
        ftCommonCliffQuickOrSlowSetStatus(fighter_gobj, 2);

        return TRUE;
    }
    else return FALSE;
}

/* ftcommoncliffcatchwait.c:89-96 ftCommonCliffWaitProcInterrupt
 * 0x80144DA4, whole. The ledge attack's check leads the cascade,
 * ahead of the roll and the climb, as it does in the game. */
void ftCommonCliffWaitProcInterrupt(GObj *fighter_gobj)
{
    if ((ftCommonCliffAttackCheckInterruptCommon(fighter_gobj) == FALSE) && (ftCommonCliffEscapeCheckInterruptCommon(fighter_gobj) == FALSE) && (ftCommonCliffClimbOrFallCheckInterruptCommon(fighter_gobj) == FALSE))
    {
        ftCommonCliffWaitCheckFall(fighter_gobj);
    }
}

/* ---- the attacks: the jab and the three tilts ------------
 * ft/ftcommon/ftcommonattack1.c, ftcommonattacks3.c, ftcommonattackhi3.c,
 * ftcommonattacklw3.c, verbatim. The item branches are back,
 * so all four are whole. The hitboxes are
 * the motion-command scripts' (src/dc/ftmain.c); these only choose the
 * status. */

/* ftCommonGetCheckInterruptCommon (the pickup the attack cascades ask for
 * before they start an attack), ftCommonHeavyThrowCheckInterruptCommon
 * and ftCommonLightThrowCheckItemTypeThrow are implemented via
 * ft/ftcommon/ftcommonget.c and ftcommonitemthrow.c, compiled unmodified.
 * The real ones link and the six cascades below reach them with nothing
 * else changed. */

/* ---- ft/ftcommon/ftcommoncatch1.c -- the grab, the reach
 *
 * The grab attempt: Z held with A tapped (or A tapped while shielding, or
 * Z in the jab's first frames) reaches out on the Catch status, which
 * counts the animation's pull-frame event down and, catching nobody, ends
 * into Wait (ftCommonCatchProcUpdate). Reachable from every ground state
 * whose cascade calls one of the CatchCheckInterrupt* below -- wired this
 * step into Wait, Walk, Squat, SquatWait, Ottotto and Landing (the ground
 * cascade's slot, right after the last special and before the attacks),
 * the Guard macro (the shield-grab, between Escape and GuardKneeBend) and
 * Attack11 (the jab-cancel grab), the game's order in each.
 *
 * ftCommonCatchSetStatus calls ftParamSetCatchParams, enabling the catch
 * search (ftMainProcSearchCatch, already live) to find a fighter and
 * fire the two procs ported below --
 * the catcher into CatchPull -> CatchWait (ftcommoncatch2.c), the caught
 * into CapturePulled -> CaptureWait (ftcommoncapturepulled.c,
 * ftcommoncapturewait.c), held at the catcher's grab joint each frame.
 *
 * CatchWait's interrupt counts throw_wait down and calls
 * ftCommonThrowCheckInterruptCatchWait, which fires the F/B throw
 * (ftcommonthrow.c / ftcommonthrown1.c / thrown2.c, statuses 169/170).
 *
 * ftCommonCatchCheckInterruptDashRun (below) is wired into the Dash and
 * Run interrupts' frame windows, alongside the CatchCheckInterruptCommon
 * arms -- the grab is reachable out of a dash or a run, not only standing.
 * (There is no turn grab: ftCommonTurnRunProcInterrupt has no catch arm.) */

/* ftcommoncatch1.c:9-32 ftCommonCatchProcUpdate 0x80149A10: counts the
 * pull-frame event the animation raises (flag2) down; catching nobody it
 * ends into Wait. */
void ftCommonCatchProcUpdate(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if (fp->status_vars.common.catchmain.catch_pull_frame_begin > 0.0F)
    {
        fp->status_vars.common.catchmain.catch_pull_frame_begin -= fp->status_vars.common.catchmain.catch_pull_anim_frames;

        if (fp->status_vars.common.catchmain.catch_pull_frame_begin <= 0.0F)
        {
            fp->status_vars.common.catchmain.catch_pull_frame_begin = 0.0F;
        }
    }
    if (fp->motion_vars.flags.flag2 != 0)
    {
        fp->status_vars.common.catchmain.catch_pull_frame_begin = fp->motion_vars.flags.flag2;

        fp->status_vars.common.catchmain.catch_pull_anim_frames = fp->status_vars.common.catchmain.catch_pull_frame_begin / fp->motion_vars.flags.flag1;

        fp->motion_vars.flags.flag2 = 0;
    }
    ftAnimEndSetWait(fighter_gobj);
}

/* ftcommoncatch1.c:35-60 ftCommonCatchCaptureSetStatusRelease 0x80149AC8:
 * falls, and releases the caught fighter if there is one. Verbatim; the
 * catch_gobj arm is dead until the two-body catch (catch_gobj is only set
 * by the connect this step defers), but must link. */
void ftCommonCatchCaptureSetStatusRelease(GObj *fighter_gobj)
{
    FTStruct *this_fp = ftGetStruct(fighter_gobj);
    FTStruct *catch_fp;
    GObj *catch_gobj;

    ftCommonFallSetStatus(fighter_gobj);

    catch_gobj = this_fp->catch_gobj;

    if (catch_gobj != NULL)
    {
        catch_fp = ftGetStruct(catch_gobj);

        ftCommonThrownReleaseFighterLoseGrip(catch_gobj);

        if (catch_fp->ga == nMPKineticsGround)
        {
            ftCommonWaitSetStatus(catch_gobj);
        }
        else ftCommonFallSetStatus(catch_gobj);

        catch_fp->capture_gobj = NULL;
        this_fp->catch_gobj = NULL;
    }
}

/* ftcommoncatch1.c:71-78 ftCommonCatchProcMap 0x80149B78: walking off the
 * edge in a grab falls (and releases). */
void ftCommonCatchProcMap(GObj *fighter_gobj)
{
    if (mpCommonCheckFighterOnEdge(fighter_gobj) == FALSE)
    {
        ftCommonCatchCaptureSetStatusRelease(fighter_gobj);
    }
}

/* ftcommoncatch1.c:81-101 ftCommonCatchSetStatus 0x80149BA8. */
void ftCommonCatchSetStatus(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    ftMainSetStatus(fighter_gobj, nFTCommonStatusCatch, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);
    ftMainPlayAnimEventsAll(fighter_gobj);

    fp->motion_vars.flags.flag1 = 1;
    fp->motion_vars.flags.flag2 = 0;

    fp->status_vars.common.catchmain.catch_pull_anim_frames = 0.0F;
    fp->status_vars.common.catchmain.catch_pull_frame_begin = 0.0F;

    /* The connect: arms is_catchstatus and hands the catch search
 * (ftMainProcSearchCatch, already live) the two procs it fires on a find:
 * ftCommonCatchPullProcCatch pulls this fighter into
 * CatchPull, ftCommonCapturePulledProcCapture pulls the caught one into
 * CapturePulled. The grab no longer whiffs. */
    ftParamSetCatchParams(fp, FTCATCHKIND_MASK_COMMON, ftCommonCatchPullProcCatch, ftCommonCapturePulledProcCapture);

    fp->is_shield_catch = FALSE;

    if (((fp->fkind == nFTKindSamus) || (fp->fkind == nFTKindNSamus)) && (efManagerSamusGrappleBeamGlowMakeEffect(fighter_gobj) != NULL))
    {
        fp->is_effect_attach = TRUE;
    }
}

/* ftcommoncatch1.c:104-121 ftCommonCatchCheckInterruptGuard 0x80149C60:
 * the shield-grab -- A while shielding. is_shield_catch is carried from
 * whether the shield was poked (halves throw damage the day throws land).
 * Verbatim. */
sb32 ftCommonCatchCheckInterruptGuard(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    FTAttributes *attr = fp->attr;
    sb32 is_shield_catch = fp->status_vars.common.guard.is_setoff;

    if ((fp->input.pl.button_tap & fp->input.button_mask_a) && (attr->is_have_catch))
    {
        ftCommonCatchSetStatus(fighter_gobj);

        fp->is_shield_catch = is_shield_catch;

        return TRUE;
    }
    else return FALSE;
}

/* ftcommoncatch1.c:123-141 ftCommonCatchCheckInterruptCommon 0x80149CE0,
 * whole: the game asks the ITEM THROW first and the grab second,
 * because A with something in your hands throws it. The ITEM THROW arm is
 * ftCommonLightThrowCheckItemTypeThrow and
 * ftCommonLightThrowDecideSetStatus are ft/ftcommon/ftcommonitemthrow.c's
 * own, compiled unmodified now. */
sb32 ftCommonCatchCheckInterruptCommon(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    FTAttributes *attr = fp->attr;

    if (ftCommonLightThrowCheckItemTypeThrow(fp) != FALSE)
    {
        ftCommonLightThrowDecideSetStatus(fighter_gobj);

        return TRUE;
    }
    else if ((fp->input.pl.button_hold & fp->input.button_mask_z) && (fp->input.pl.button_tap & fp->input.button_mask_a) && (attr->is_have_catch))
    {
        ftCommonCatchSetStatus(fighter_gobj);

        return TRUE;
    }
    else return FALSE;
}

/* ftcommoncatch1.c:144-160 ftCommonCatchCheckInterruptDashRun 0x80149DA0:
 * out of a dash or a run, Z held plus an A tap is the grab -- the same
 * arm as the standing grab, reached from the dash and run windows.
 * Whole as of the item-use step, and the first arm is the DASH item
 * throw -- an item thrown out of a run goes into LightThrowDash, not
 * through the stick-direction decision the standing throw makes. */
sb32 ftCommonCatchCheckInterruptDashRun(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    FTAttributes *attr = fp->attr;

    if (ftCommonLightThrowCheckItemTypeThrow(fp) != FALSE)
    {
        ftCommonItemThrowSetStatus(fighter_gobj, nFTCommonStatusLightThrowDash);

        return TRUE;
    }
    else if ((fp->input.pl.button_hold & fp->input.button_mask_z) && (fp->input.pl.button_tap & fp->input.button_mask_a) && (attr->is_have_catch))
    {
        ftCommonCatchSetStatus(fighter_gobj);

        return TRUE;
    }
    else return FALSE;
}

/* ftcommoncatch1.c:165-182 ftCommonCatchCheckInterruptAttack11 0x80145A30:
 * a Z within the jab's first two frames grabs instead -- or, with
 * something in your hands, throws it, which is the arm the item-use step
 * puts back. Its status is LightThrowDash here too, the same as out of a
 * run: the game treats a jab-cancelled throw as a dash throw. */
sb32 ftCommonCatchCheckInterruptAttack11(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    FTAttributes *attr = fp->attr;

    if (ftCommonLightThrowCheckItemTypeThrow(fp) != FALSE)
    {
        ftCommonItemThrowSetStatus(fighter_gobj, nFTCommonStatusLightThrowDash);

        return TRUE;
    }
    else if ((fp->input.pl.button_tap & fp->input.button_mask_z) && (attr->is_have_catch))
    {
        ftCommonCatchSetStatus(fighter_gobj);

        return TRUE;
    }
    else return FALSE;
}

/* ---- the two-body catch, the connect
 *
 * When ftCommonCatchSetStatus armed is_catchstatus (above), ftMainProcSearchCatch
 * finds a fighter in the grab box and fires proc_catch on the catcher and
 * proc_capture on the caught. These are the common grab's pair
 * (ftcommoncatch2.c, ftcommoncapturepulled.c, ftcommoncapturewait.c): the
 * catcher goes CatchPull -> CatchWait; the caught goes CapturePulled ->
 * CaptureWait, positioned at the catcher's item-heavy (hand) joint every
 * frame by ftCommonCapturePulledRotateScale. The throw that ends the hold
 * is ftCommonThrowCheckInterruptCatchWait's, below. */

/* ftcommoncatch2.c:19-29 ftCommonCatchPullProcUpdate 0x80149EC0: the pull
 * animation runs out into CatchWait, and the caught fighter is told to
 * leave CapturePulled for CaptureWait (its physics proc reads the flag). */
void ftCommonCatchPullProcUpdate(GObj *fighter_gobj)
{
    FTStruct *this_fp = ftGetStruct(fighter_gobj);

    if (ftAnimEndCheckSetStatus(fighter_gobj, ftCommonCatchWaitSetStatus) != FALSE)
    {
        FTStruct *catch_fp = ftGetStruct(this_fp->catch_gobj);

        catch_fp->status_vars.common.capture.is_goto_pulled_wait = TRUE;
    }
}

/* ftcommoncatch2.c:10 0x801886D0: where on the heavy-item joint the grab
 * swirl goes. */
Vec3f dFTCommonCatchPullEffectOffset = { 0.0F, 0.0F, 0.0F };

/* ftcommoncatch2.c:32-54 ftCommonCatchPullProcCatch 0x80149F04: proc_catch.
 * The catcher enters CatchPull, records the caught fighter (search_gobj),
 * goes immune to being caught itself, and makes the swirl at its hand.
 * Verbatim. */
void ftCommonCatchPullProcCatch(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    Vec3f pos;

    ftMainSetStatus(fighter_gobj, nFTCommonStatusCatchPull, fp->status_vars.common.catchmain.catch_pull_frame_begin, 1.0F, (FTSTATUS_PRESERVE_SLOPECONTOUR | FTSTATUS_PRESERVE_EFFECT));

    fp->catch_gobj = fp->search_gobj;

    fp->is_catch_or_capture = FALSE;

    ftParamSetCaptureImmuneMask(fp, FTCATCHKIND_MASK_ALL);

    if (fp->proc_slope != NULL)
    {
        fp->proc_slope(fighter_gobj);
    }
    pos = dFTCommonCatchPullEffectOffset;

    gmCollisionGetFighterPartsWorldPosition(fp->joints[fp->attr->joint_itemheavy_id], &pos);
    efManagerCatchSwirlMakeEffect(&pos);
    ftParamMakeRumble(fp, 9, 0);
}

/* ftcommoncatch2.c:57-66 ftCommonCatchWaitProcInterrupt 0x80149FCC: the
 * hold. Counts throw_wait down, then asks the throw check whether to
 * throw. */
void ftCommonCatchWaitProcInterrupt(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if (fp->status_vars.common.catchwait.throw_wait != 0)
    {
        fp->status_vars.common.catchwait.throw_wait--;
    }
    ftCommonThrowCheckInterruptCatchWait(fighter_gobj);
}

/* ftcommoncatch2.c:69-88 ftCommonCatchWaitSetStatus 0x8014A000: enter the
 * hold. The Link/Yoshi model-part swaps stay verbatim (dead for Mario, but
 * ftParamSetModelPartID is ported and the fkind guards keep them cold). */
void ftCommonCatchWaitSetStatus(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    ftMainSetStatus(fighter_gobj, nFTCommonStatusCatchWait, 0.0F, 1.0F, FTSTATUS_PRESERVE_SLOPECONTOUR);

    fp->status_vars.common.catchwait.throw_wait = FTCOMMON_CATCH_THROW_WAIT;

    ftParamSetCaptureImmuneMask(fp, FTCATCHKIND_MASK_ALL);

    if ((fp->fkind == nFTKindLink) || (fp->fkind == nFTKindNLink))
    {
        ftParamSetModelPartID(fighter_gobj, 21, 0);
        ftParamSetModelPartID(fighter_gobj, 19, -1);
    }
    else if ((fp->fkind == nFTKindYoshi) || (fp->fkind == nFTKindNYoshi))
    {
        ftParamSetModelPartID(fighter_gobj, 7, 1);
    }
}

/* lb/lbcommon.c:1477-1603 func_ovl0_800C9A38 0x800C9A38: the joint's
 * rotation-only world matrix -- each row of the joint's world matrix
 * normalized to unit length, so scale is stripped and only orientation and
 * translation remain. Verbatim, but placed here (a file-local static beside
 * its only caller) rather than in the port's lbcommon.c: that file is
 * host-linked in isolation by tools/check/shade_check.py with no
 * gm/gmcollision.c, and this function calls into it. ftcommon.c already links
 * the collision layer, so its definition is at home here (the prototype is the
 * decomp's, in lb/lbcommon.h). */
void func_ovl0_800C9A38(Mtx44f mtx, DObj *dobj)
{
    FTParts *parts = ftGetParts(dobj);
    FTStruct *fp = ftGetStruct(dobj->parent_gobj);
    Mtx44f *p;
    f32 scale;
    DObj *parent_dobj;
    Mtx44f f;

    if ((fp->is_use_animlocks) || (dobj->parent == DOBJ_PARENT_NULL))
    {
        func_ovl2_800EDBA4(dobj);

        p = &parts->mtx_translate;

        scale = sqrtf(SQUARE((*p)[0][0]) + SQUARE((*p)[0][1]) + SQUARE((*p)[0][2]));

        if (scale != 0.0F)
        {
            scale = 1.0F / scale;
        }
        mtx[0][0] = (*p)[0][0] * scale;
        mtx[0][1] = (*p)[0][1] * scale;
        mtx[0][2] = (*p)[0][2] * scale;

        scale = sqrtf(SQUARE((*p)[1][0]) + SQUARE((*p)[1][1]) + SQUARE((*p)[1][2]));

        if (scale != 0.0F)
        {
            scale = 1.0F / scale;
        }
        mtx[1][0] = (*p)[1][0] * scale;
        mtx[1][1] = (*p)[1][1] * scale;
        mtx[1][2] = (*p)[1][2] * scale;

        scale = sqrtf(SQUARE((*p)[2][0]) + SQUARE((*p)[2][1]) + SQUARE((*p)[2][2]));

        if (scale != 0.0F)
        {
            scale = 1.0F / scale;
        }
        mtx[2][0] = (*p)[2][0] * scale;
        mtx[2][1] = (*p)[2][1] * scale;
        mtx[2][2] = (*p)[2][2] * scale;

        mtx[3][0] = (*p)[3][0];
        mtx[3][1] = (*p)[3][1];
        mtx[3][2] = (*p)[3][2];
    }
    else
    {
        parent_dobj = dobj->parent;

        gmCollisionTransformMatrixAll(dobj, parts, parts->unk_dobjtrans_0x10);

        p = &parts->unk_dobjtrans_0x10;

        scale = sqrtf(SQUARE((*p)[0][0]) + SQUARE((*p)[0][1]) + SQUARE((*p)[0][2]));

        if (scale != 0.0F)
        {
            scale = 1.0F / scale;
        }
        f[0][0] = (*p)[0][0] * scale;
        f[0][1] = (*p)[0][1] * scale;
        f[0][2] = (*p)[0][2] * scale;

        scale = sqrtf(SQUARE((*p)[1][0]) + SQUARE((*p)[1][1]) + SQUARE((*p)[1][2]));

        if (scale != 0.0F)
        {
            scale = 1.0F / scale;
        }
        f[1][0] = (*p)[1][0] * scale;
        f[1][1] = (*p)[1][1] * scale;
        f[1][2] = (*p)[1][2] * scale;

        scale = sqrtf(SQUARE((*p)[2][0]) + SQUARE((*p)[2][1]) + SQUARE((*p)[2][2]));

        if (scale != 0.0F)
        {
            scale = 1.0F / scale;
        }
        f[2][0] = (*p)[2][0] * scale;
        f[2][1] = (*p)[2][1] * scale;
        f[2][2] = (*p)[2][2] * scale;

        f[3][0] = (*p)[3][0];
        f[3][1] = (*p)[3][1];
        f[3][2] = (*p)[3][2];

        func_ovl2_800EDBA4(parent_dobj);

        p = &ftGetParts(parent_dobj)->mtx_translate;

        scale = sqrtf(SQUARE((*p)[0][0]) + SQUARE((*p)[0][1]) + SQUARE((*p)[0][2]));

        if (scale != 0.0F)
        {
            scale = 1.0F / scale;
        }
        f[0][0] *= scale;
        f[1][0] *= scale;
        f[2][0] *= scale;

        scale = sqrtf(SQUARE((*p)[1][0]) + SQUARE((*p)[1][1]) + SQUARE((*p)[1][2]));

        if (scale != 0.0F)
        {
            scale = 1.0F / scale;
        }
        f[0][1] *= scale;
        f[1][1] *= scale;
        f[2][1] *= scale;

        scale = sqrtf(SQUARE((*p)[2][0]) + SQUARE((*p)[2][1]) + SQUARE((*p)[2][2]));

        if (scale != 0.0F)
        {
            scale = 1.0F / scale;
        }
        f[0][2] *= scale;
        f[1][2] *= scale;
        f[2][2] *= scale;

        func_ovl2_800ED490(mtx, *p, f);
    }
}

/* ftcommoncapturepulled.c:11-41 ftCommonCapturePulledRotateScale 0x8014A5F0
 * (REGION_US). Places the caught fighter at the catcher's item-heavy joint:
 * func_ovl0_800C9A38 gives the joint's rotation-only world matrix,
 * func_ovl2_800EDA0C reads its Euler angles into the caught body's rotate,
 * and gmCollisionGetWorldPosition carries the (negated, scaled) child-joint
 * offset through it to a world position. All three matrix helpers are the
 * decomp collision layer's own (gm/gmcollision.c, compiled in), so this is
 * a straight port. */
void ftCommonCapturePulledRotateScale(GObj *fighter_gobj, Vec3f *this_pos, Vec3f *rotate)
{
    FTStruct *this_fp = ftGetStruct(fighter_gobj);
    FTStruct *capture_fp = ftGetStruct(this_fp->capture_gobj);
    DObj *joint = DObjGetStruct(fighter_gobj)->child;
    Mtx44f mtx;

    func_ovl0_800C9A38(mtx, capture_fp->joints[capture_fp->attr->joint_itemheavy_id]);
    func_ovl2_800EDA0C(mtx, rotate);

    this_pos->x = (-joint->translate.vec.f.x * DObjGetStruct(fighter_gobj)->scale.vec.f.x);
    this_pos->y = (-joint->translate.vec.f.y * DObjGetStruct(fighter_gobj)->scale.vec.f.y);
    this_pos->z = (-joint->translate.vec.f.z * DObjGetStruct(fighter_gobj)->scale.vec.f.z);

    gmCollisionGetWorldPosition(mtx, this_pos);
}

/* ftcommoncapturepulled.c:44-58 ftCommonCapturePulledProcPhysics 0x8014A6B4:
 * every frame, snap x/z to the catcher's hand; when the pull ends (the flag
 * the catcher set), enter CaptureWait. */
void ftCommonCapturePulledProcPhysics(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    Vec3f pos;

    ftCommonCapturePulledRotateScale(fighter_gobj, &pos, &DObjGetStruct(fighter_gobj)->rotate.vec.f);

    DObjGetStruct(fighter_gobj)->translate.vec.f.x = pos.x;
    DObjGetStruct(fighter_gobj)->translate.vec.f.z = pos.z;

    if ((fp->status_id == nFTCommonStatusCapturePulled) && (fp->status_vars.common.capture.is_goto_pulled_wait != FALSE))
    {
        ftCommonCaptureWaitSetStatus(fighter_gobj);
    }
}

/* ftcommoncapturepulled.c:61-104 ftCommonCapturePulledProcMap 0x8014A72C:
 * the y follows the catcher's floor line -- lands the caught body on the
 * same ground, or projects it toward the platform edge when off it. */
void ftCommonCapturePulledProcMap(GObj *fighter_gobj)
{
    FTStruct *this_fp = ftGetStruct(fighter_gobj);
    GObj *capture_gobj = this_fp->capture_gobj;
    FTStruct *capture_fp = ftGetStruct(capture_gobj);
    Vec3f *this_pos = &DObjGetStruct(fighter_gobj)->translate.vec.f;
    Vec3f capture_pos;
    f32 dist_y;

    if (mpCollisionGetFCCommonFloor(capture_fp->coll_data.floor_line_id, this_pos, &dist_y, &this_fp->coll_data.floor_flags, &this_fp->coll_data.floor_angle) != FALSE)
    {
        this_fp->coll_data.floor_line_id = capture_fp->coll_data.floor_line_id;

        if (dist_y >= 0.0F)
        {
            this_pos->y += dist_y;

            this_fp->ga = nMPKineticsGround;
            this_fp->jumps_used = 0;
        }
        else
        {
            this_pos->y += dist_y * 0.5F;

            this_fp->ga = nMPKineticsAir;
            this_fp->jumps_used = 1;
        }
    }
    else
    {
        if (capture_fp->lr == +1)
        {
            mpCollisionGetFloorEdgeR(capture_fp->coll_data.floor_line_id, &capture_pos);
        }
        else mpCollisionGetFloorEdgeL(capture_fp->coll_data.floor_line_id, &capture_pos);

        this_pos->y = this_pos->y + ((capture_pos.y - this_pos->y) * 0.5F);

        mpCommonSetFighterProjectFloor(fighter_gobj);

        this_fp->ga = nMPKineticsAir;
        this_fp->jumps_used = 1;
    }
}

/* ftcommoncapturepulled.c:107-146 ftCommonCapturePulledProcCapture 0x8014A860:
 * proc_capture, run on the caught fighter (fighter_gobj), catcher is
 * capture_gobj. Drops any held heavy item (ftSetupDropItem is the decomp
 * macro), releases any prior grab of its own, records the catcher, faces
 * away from it, and enters CapturePulled -- then runs one physics+map pass
 * so it starts the frame already in the catcher's hand. */
void ftCommonCapturePulledProcCapture(GObj *fighter_gobj, GObj *capture_gobj)
{
    FTStruct *this_fp = ftGetStruct(fighter_gobj);
    FTStruct *capture_fp;

    ftParamStopVoiceRunProcDamage(fighter_gobj);

    if ((this_fp->item_gobj != NULL) && (itGetStruct(this_fp->item_gobj)->weight == nITWeightHeavy))
    {
        ftSetupDropItem(this_fp);
    }
    if (this_fp->catch_gobj != NULL)
    {
        ftCommonThrownSetStatusDamageRelease(this_fp->catch_gobj);

        this_fp->catch_gobj = NULL;
    }
    else if (this_fp->capture_gobj != NULL)
    {
        ftCommonThrownDecideFighterLoseGrip(this_fp->capture_gobj, fighter_gobj);
    }
    this_fp->is_catch_or_capture = FALSE;

    this_fp->capture_gobj = capture_gobj;

    capture_fp = ftGetStruct(capture_gobj);

    this_fp->lr = -capture_fp->lr;

    ftMainSetStatus(fighter_gobj, nFTCommonStatusCapturePulled, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);
    ftMainPlayAnimEventsAll(fighter_gobj);

    this_fp->status_vars.common.capture.is_goto_pulled_wait = FALSE;

    ftParamSetCaptureImmuneMask(this_fp, FTCATCHKIND_MASK_ALL);
    ftParamMakeRumble(this_fp, 9, 0);
    ftPhysicsStopVelAll(fighter_gobj);
    ftCommonCapturePulledProcPhysics(fighter_gobj);
    ftCommonCapturePulledProcMap(fighter_gobj);
}

/* ftcommoncapturewait.c:10-43 ftCommonCaptureWaitProcMap 0x8014A980: like
 * CapturePulled's map but the y snaps flush to the floor (the pull is over,
 * the body just sits in the hand). */
void ftCommonCaptureWaitProcMap(GObj *fighter_gobj)
{
    FTStruct *this_fp = ftGetStruct(fighter_gobj);
    GObj *capture_gobj = this_fp->capture_gobj;
    FTStruct *capture_fp = ftGetStruct(capture_gobj);
    Vec3f *this_pos = &DObjGetStruct(fighter_gobj)->translate.vec.f;
    Vec3f capture_pos;
    f32 dist_y;

    if (mpCollisionGetFCCommonFloor(capture_fp->coll_data.floor_line_id, this_pos, &dist_y, &this_fp->coll_data.floor_flags, &this_fp->coll_data.floor_angle) != FALSE)
    {
        this_fp->coll_data.floor_line_id = capture_fp->coll_data.floor_line_id;

        this_pos->y += dist_y;

        this_fp->ga = nMPKineticsGround;
        this_fp->jumps_used = 0;
    }
    else
    {
        if (capture_fp->lr == +1)
        {
            mpCollisionGetFloorEdgeR(capture_fp->coll_data.floor_line_id, &capture_pos);
        }
        else mpCollisionGetFloorEdgeL(capture_fp->coll_data.floor_line_id, &capture_pos);

        this_pos->y = capture_pos.y;

        mpCommonSetFighterProjectFloor(fighter_gobj);

        this_fp->ga = nMPKineticsAir;
        this_fp->jumps_used = 1;
    }
}

/* ftcommoncapturewait.c:46-60 ftCommonCaptureWaitSetStatus 0x8014AA58: the
 * caught fighter's hold. The Yoshi arm (swallow -> invisible/intangible)
 * stays verbatim, cold for Mario. */
void ftCommonCaptureWaitSetStatus(GObj *fighter_gobj)
{
    FTStruct *this_fp = ftGetStruct(fighter_gobj);
    FTStruct *capture_fp = ftGetStruct(this_fp->capture_gobj);

    ftMainSetStatus(fighter_gobj, nFTCommonStatusCaptureWait, 0.0F, 1.0F, (FTSTATUS_PRESERVE_TEXTUREPART | FTSTATUS_PRESERVE_MODELPART));

    if ((capture_fp->fkind == nFTKindYoshi) || (capture_fp->fkind == nFTKindNYoshi))
    {
        this_fp->is_invisible = TRUE;

        ftParamSetHitStatusAll(fighter_gobj, nGMHitStatusIntangible);
    }
    ftParamSetCaptureImmuneMask(this_fp, FTCATCHKIND_MASK_ALL);
}

/* the grab, the throw. CatchWait's interrupt, once the
 * throw_wait counts out or the player mashes A/B or shoves the stick,
 * fires ftCommonThrowCheckInterruptCatchWait, which puts the catcher into
 * ThrowF (169) or ThrowB (170) and the caught fighter into a Thrown status
 * (ThrownCommon 186 forward, ThrownMarioBStart 182 -> ThrownMarioB 185
 * back for Mario). The catcher's ThrowProcUpdate reads two figatree flags:
 * flag1 flips its facing/velocity mid-animation, flag2 is the release
 * frame -- it hands the caught fighter its arc and knockback through
 * ftCommonThrownReleaseThrownUpdateStats and lets go. The thrown fighter
 * rides the catcher's hand (ThrownProcPhysics) until that release, then
 * flies. Verbatim from ftcommonthrow.c / ftcommonthrown1.c / thrown2.c,
 * with the DK-shoulder, Kirby-swallow and Samus-grapple-glow arms (the
 * glow since). */

/* ft/ftcommon/ftcommonthrown2.c:19 sFTCommonThrownScriptID 0x8018CF80: the
 * throw's damage-script id, stashed by ReleaseThrownUpdateStats and read
 * back by ProcStatus when the thrown fighter's damage vars are (re)armed. */
s32 sFTCommonThrownScriptID;

/* ft/ftcommon/ftcommonthrow.c:12-50 ftCommonThrowProcUpdate 0x8014A0C0:
 * the thrower's per-frame update. flag1 (a figatree event) mirrors the
 * throw mid-motion; flag2 (the release event) hands the caught fighter its
 * knockback and cuts it loose. At anim end -> Wait/Fall (DK's F-throw goes
 * to the shoulder carry instead). Verbatim. */
void ftCommonThrowProcUpdate(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if (fp->motion_vars.flags.flag1 != 0)
    {
        fp->motion_vars.flags.flag1 = 0;

        fp->lr = -fp->lr;

        fp->physics.vel_ground.x = -fp->physics.vel_ground.x;
    }
    if (fp->motion_vars.flags.flag2 != 0)
    {
        /* The caught fighter can die (e.g. an off-stage KO) and respawn
 * between being queued into this throw and this delayed release
 * event firing; ftManagerInitFighter's own respawn reset (verbatim
 * decomp) NULLs the caught fighter's capture_gobj as part of that,
 * with nothing in ftcommondead.c/ftcommonrebirth.c aware a throw
 * was in flight. Releasing into that state used to walk
 * ftCommonThrownProcPhysics -> ftCommonCapturePulledRotateScale's
 * unconditional ftGetStruct(NULL)->attr dereference and crash hard.
 * Guard on the caught fighter still actually pointing back at us. */
        if (fp->catch_gobj != NULL && ftGetStruct(fp->catch_gobj)->capture_gobj == fighter_gobj)
        {
            ftCommonThrownProcPhysics(fp->catch_gobj);
            ftCommonThrownReleaseThrownUpdateStats(fp->catch_gobj, (fp->motion_vars.flags.flag2 == 1) ? -fp->lr : fp->lr, (fp->status_id == nFTCommonStatusThrowB) ? 1 : 0, TRUE);
        }

        fp->motion_vars.flags.flag2 = 0;

        fp->catch_gobj = NULL;

        ftParamSetCaptureImmuneMask(fp, FTCATCHKIND_MASK_NONE);
    }
    if (fighter_gobj->anim_frame <= 0.0F)
    {
        if ((fp->fkind == nFTKindDonkey) || (fp->fkind == nFTKindNDonkey) || (fp->fkind == nFTKindGDonkey))
        {
            if (fp->status_id == nFTCommonStatusThrowF)
            {
                ftCommonCaptureShoulderedSetStatus(fp->catch_gobj);
                ftDonkeyThrowFWaitSetStatus(fighter_gobj);

                return;
            }
        }
        mpCommonSetFighterWaitOrFall(fighter_gobj);
    }
}

/* ft/ftcommon/ftcommonthrow.c:52-108 ftCommonThrowSetStatus 0x8014A1E8:
 * chooses ThrowF or ThrowB from is_throwf / stick, sets the catcher's
 * status and immune mask, and queues the caught fighter's Thrown status
 * out of the attr's per-kind thrown_status table. Verbatim. */
void ftCommonThrowSetStatus(GObj *fighter_gobj, sb32 is_throwf)
{
    FTStruct *this_fp = ftGetStruct(fighter_gobj);
    s32 status_id;
    GObj *catch_gobj;
    FTStruct *catch_fp;
    FTThrownStatus *thrown_status;

    catch_gobj = this_fp->catch_gobj;
    catch_fp = ftGetStruct(catch_gobj);

    if ((is_throwf != FALSE) || ((this_fp->input.pl.stick_range.x * this_fp->lr) >= 0))
    {
        if ((this_fp->fkind == nFTKindKirby) || (this_fp->fkind == nFTKindNKirby))
        {
            status_id = nFTKirbyStatusThrowF;

            mpCommonSetFighterAir(this_fp);
        }
        else status_id = nFTCommonStatusThrowF;
        thrown_status = &this_fp->attr->thrown_status[catch_fp->fkind].ft_thrown[0];
    }
    else
    {
        status_id = nFTCommonStatusThrowB;
        thrown_status = &this_fp->attr->thrown_status[catch_fp->fkind].ft_thrown[1];
    }
    ftMainSetStatus(fighter_gobj, status_id, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);
    ftMainPlayAnimEventsAll(fighter_gobj);
    ftParamSetCaptureImmuneMask(this_fp, FTCATCHKIND_MASK_ALL);

    this_fp->motion_vars.flags.flag2 = 0;
    this_fp->motion_vars.flags.flag1 = 0;

    if ((this_fp->fkind == nFTKindSamus) || (this_fp->fkind == nFTKindNSamus))
    {
        if (efManagerSamusGrappleBeamGlowMakeEffect(fighter_gobj) != NULL)
        {
            this_fp->is_effect_attach = TRUE;
        }
    }
    if (thrown_status->status1 != -1)
    {
        ftCommonThrownSetStatusQueue(catch_gobj, thrown_status->status1, thrown_status->status2);
    }
    else ftCommonThrownSetStatusImmediate(catch_gobj, thrown_status->status2);

    if ((this_fp->fkind == nFTKindKirby) || (this_fp->fkind == nFTKindNKirby))
    {
        if (status_id == nFTKirbyStatusThrowF)
        {
            this_fp->is_ignore_dead = TRUE;
            catch_fp->is_ignore_dead = TRUE;
        }
    }
}

/* ft/ftcommon/ftcommonthrow.c:110-129 ftCommonThrowCheckInterruptCatchWait
 * 0x8014A394: CatchWait's interrupt. Once throw_wait is out or A/B is
 * tapped -> forward throw; a firm back/forward stick shove -> that throw;
 * otherwise keep holding. Verbatim. */
sb32 ftCommonThrowCheckInterruptCatchWait(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    sb32 is_throwf = FALSE;

    if ((fp->status_vars.common.catchwait.throw_wait == 0) || (fp->input.pl.button_tap & (fp->input.button_mask_a | fp->input.button_mask_b)))
    {
        is_throwf = TRUE;
    }
    else if ((fp->input.pl.stick_prev.x >= FTCOMMON_CATCH_THROW_STICK_RANGE_MIN) || (fp->input.pl.stick_range.x < FTCOMMON_CATCH_THROW_STICK_RANGE_MIN))
    {
        if ((fp->input.pl.stick_prev.x <= -FTCOMMON_CATCH_THROW_STICK_RANGE_MIN) || (fp->input.pl.stick_range.x > -FTCOMMON_CATCH_THROW_STICK_RANGE_MIN))
        {
            return FALSE;
        }
    }
    ftCommonThrowSetStatus(fighter_gobj, is_throwf);

    return TRUE;
}

/* ft/ftcommon/ftcommonthrown1.c:10-28 ftCommonThrownProcUpdate 0x8014AAF0:
 * the thrown fighter's per-frame update -- at anim end it flips to its
 * queued status (unless the catcher is a DK still mid-F-throw). Verbatim. */
void ftCommonThrownProcUpdate(GObj *fighter_gobj)
{
    FTStruct *this_fp = ftGetStruct(fighter_gobj);

    if (fighter_gobj->anim_frame <= 0.0F)
    {
        FTStruct *capture_fp = ftGetStruct(this_fp->capture_gobj);

        if
        (
            (capture_fp->fkind != nFTKindDonkey)                      &&
            (capture_fp->fkind != nFTKindNDonkey)                     &&
            (capture_fp->fkind != nFTKindGDonkey)                     ||
            (capture_fp->status_id != nFTCommonStatusThrowF)
        )
        {
            ftCommonThrownSetStatusImmediate(fighter_gobj, this_fp->status_vars.common.thrown.status_id);
        }
    }
}

/* ft/ftcommon/ftcommonthrown1.c:30-36 ftCommonThrownProcPhysics 0x8014AB64:
 * keeps the thrown fighter pinned to the catcher's hand joint, the same
 * RotateScale the connect used. Verbatim. */
void ftCommonThrownProcPhysics(GObj *fighter_gobj)
{
    DObj *joint = DObjGetStruct(fighter_gobj);
    ftCommonCapturePulledRotateScale(fighter_gobj, &joint->translate.vec.f, &joint->rotate.vec.f);
}

/* ft/ftcommon/ftcommonthrown1.c:38-60 ftCommonThrownProcMap 0x8014AB8C:
 * the thrown fighter shares the catcher's floor line while held; if it
 * leaves it, project onto its own floor. Verbatim. */
void ftCommonThrownProcMap(GObj *fighter_gobj)
{
    FTStruct *this_fp = ftGetStruct(fighter_gobj);
    GObj *capture_gobj = this_fp->capture_gobj;
    FTStruct *capture_fp = ftGetStruct(capture_gobj);
    Vec3f *this_pos = &DObjGetStruct(fighter_gobj)->translate.vec.f;
    Vec3f unused;
    f32 dist_y;

    if (capture_fp->coll_data.floor_line_id != -1)
    {
        if (mpCollisionGetFCCommonFloor(capture_fp->coll_data.floor_line_id, this_pos, &dist_y, &this_fp->coll_data.floor_flags, &this_fp->coll_data.floor_angle) != FALSE)
        {
            this_fp->coll_data.floor_line_id = capture_fp->coll_data.floor_line_id;

            return;
        }
    }
    mpCommonSetFighterProjectFloor(fighter_gobj);
}

/* ft/ftcommon/ftcommonthrown1.c:62-86 ftCommonThrownSetStatusQueue
 * 0x8014AC0C: sends the thrown fighter into status_id_new now and remembers
 * status_id_queue for when that animation ends (Mario's back throw is the
 * two-stage MarioBStart -> MarioB). Verbatim. */
void ftCommonThrownSetStatusQueue(GObj *fighter_gobj, s32 status_id_new, s32 status_id_queue)
{
    FTStruct *this_fp = ftGetStruct(fighter_gobj);
    FTStruct *capture_fp = ftGetStruct(this_fp->capture_gobj);

    this_fp->ga = nMPKineticsAir;
    this_fp->jumps_used = 1;

    ftMainSetStatus(fighter_gobj, status_id_new, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);
    ftMainPlayAnimEventsAll(fighter_gobj);

    if ((capture_fp->fkind == nFTKindYoshi) || (capture_fp->fkind == nFTKindNYoshi))
    {
        this_fp->is_invisible = TRUE;

        ftParamSetHitStatusAll(fighter_gobj, nGMHitStatusIntangible);
    }
    ftParamSetCaptureImmuneMask(this_fp, FTCATCHKIND_MASK_ALL);

    this_fp->status_vars.common.thrown.status_id = status_id_queue;
}

/* ft/ftcommon/ftcommonthrown1.c:88-117 ftCommonThrownSetStatusImmediate
 * 0x8014ACB4: the one-stage thrown status (Mario's forward throw ->
 * ThrownCommon); a Mario/Luigi back throw rumbles. Verbatim. */
void ftCommonThrownSetStatusImmediate(GObj *fighter_gobj, s32 status_id)
{
    FTStruct *this_fp = ftGetStruct(fighter_gobj);
    FTStruct *capture_fp = ftGetStruct(this_fp->capture_gobj);

    this_fp->ga = nMPKineticsAir;
    this_fp->jumps_used = 1;

    ftMainSetStatus(fighter_gobj, status_id, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);
    ftMainPlayAnimEventsAll(fighter_gobj);

    if ((capture_fp->fkind == nFTKindYoshi) || (capture_fp->fkind == nFTKindNYoshi))
    {
        this_fp->is_invisible = TRUE;

        ftParamSetHitStatusAll(fighter_gobj, nGMHitStatusIntangible);
    }
    ftParamSetCaptureImmuneMask(this_fp, FTCATCHKIND_MASK_ALL);

    if
    (
        (capture_fp->fkind == nFTKindMario)   ||
        (capture_fp->fkind == nFTKindMMario)  ||
        (capture_fp->fkind == nFTKindLuigi)   ||
        (capture_fp->fkind == nFTKindNMario)  ||
        (capture_fp->fkind == nFTKindNLuigi)
    )
    {
        if (capture_fp->status_id == nFTCommonStatusThrowB)
        {
            ftParamMakeRumble(this_fp, 7, 0);
        }
    }
}

/* ft/ftcommon/ftcommonthrown2.c:106-114 ftCommonThrownProcStatus 0x8014AF98:
 * the proc_status the thrown fighter carries once released -- re-arms its
 * throw-source params and the throw's damage script each frame. Verbatim. */
void ftCommonThrownProcStatus(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    ftParamSetThrowParams(fp, fp->capture_gobj);

    fp->status_vars.common.damage.script_id = sFTCommonThrownScriptID;
}

/* ft/ftcommon/ftcommonthrown2.c:116-185 ftCommonThrownReleaseThrownUpdateStats
 * 0x8014AFD0: the release frame -- computes the throw's knockback (minus the
 * victim's resist) and staled damage from the catcher's throw_desc, arms the
 * damage vars, updates the battle/stale stats, rumbles both players, and
 * severs capture_gobj. Verbatim. */
void ftCommonThrownReleaseThrownUpdateStats(GObj *fighter_gobj, s32 lr, s32 script_id, sb32 is_proc_status)
{
    FTStruct *this_fp = ftGetStruct(fighter_gobj);
    GObj *capture_gobj = this_fp->capture_gobj;
    FTStruct *capture_fp = ftGetStruct(capture_gobj);
    FTThrowHitDesc *ft_throw;
    f32 knockback_final;
    s32 damage;
    f32 knockback_resist;
    f32 knockback_calc;

    knockback_resist = (this_fp->knockback_resist_status < this_fp->knockback_resist_passive) ? this_fp->knockback_resist_passive : this_fp->knockback_resist_status;

    sFTCommonThrownScriptID = script_id;

    if (this_fp->hitstatus != nGMHitStatusNormal)
    {
        ftParamSetHitStatusAll(fighter_gobj, nGMHitStatusNormal);
    }
    if (!(this_fp->is_catch_or_capture))
    {
        ftCommonThrownReleaseFighterLoseGrip(fighter_gobj);
    }
    mpCommonSetFighterAir(this_fp);

    ft_throw = capture_fp->throw_desc;

    knockback_calc = ftParamGetCommonKnockback(this_fp->percent_damage, ft_throw->damage, ft_throw->damage, ft_throw->knockback_weight, ft_throw->knockback_scale, ft_throw->knockback_base, this_fp->attr->weight, capture_fp->handicap, this_fp->handicap);

    knockback_final = knockback_calc - knockback_resist;

    if (knockback_calc <= knockback_resist)
    {
        knockback_final = 0.0F;
    }
    damage = ftParamGetStaledDamage(capture_fp->player, ft_throw->damage, capture_fp->motion_attack_id, capture_fp->motion_count);

    if (capture_fp->is_shield_catch)
    {
        damage = ((damage * 0.5F) + 0.999F);
    }
    if (ftParamGetBestHitStatusAll(fighter_gobj) != nGMHitStatusNormal)
    {
        damage = 0;
    }
    if (is_proc_status != FALSE)
    {
        this_fp->proc_status = ftCommonThrownProcStatus;
    }
    ftCommonDamageInitDamageVars(fighter_gobj, ft_throw->status_id, damage, knockback_final, ft_throw->angle, lr, 1, ft_throw->element, capture_fp->player_num, TRUE, TRUE, TRUE);
    ftParamUpdate1PGameDamageStats(this_fp, capture_fp->player, nFTHitLogObjectFighter, capture_fp->fkind, capture_fp->stat_flags.halfword, capture_fp->stat_count);

    if (damage != 0)
    {
        ftParamUpdateDamage(this_fp, damage);
        ftParamUpdatePlayerBattleStats(capture_fp->player, this_fp->player, damage);
        ftParamUpdateStaleQueue(capture_fp->player, this_fp->player, capture_fp->motion_attack_id, capture_fp->motion_count);

        if ((s32) ((damage * 0.75F) + 4.0F) > 0)
        {
            ftParamMakeRumble(this_fp, 0, (s32) ((damage * 0.75F) + 4.0F));
        }
        if ((s32) ((damage * 0.5F) + 2.0F) > 0)
        {
            ftParamMakeRumble(capture_fp, 5, (s32) ((damage * 0.5F) + 2.0F));
        }
    }
    this_fp->capture_gobj = NULL;
}

/* ftcommonattack100.c: ftCommonAttack100StartSetStatus. The file is
 * compiled unmodified and gives the five fighters who have a rapid jab
 * their fifteen rows. */

/* The one reloc symbol ft/ftcommon/ftcommonattack100.c names,
 * llKirbyMainMotionftKirbyAttack100Effect, is an absolute offset now and
 * lives with Kirby's motion-file image (ftKirbyMainMotionBindOffsets,
 * below the Reflector's). */

/* ftCommonAttack100StartCheckInterruptCommon was hand-copied here at
 * when the rest of ftcommonattack100.c was out of the
 * build compiles the whole file unmodified, so the copy is
 * gone and the decomp's own is what the jab calls. The
 * ftCommonAttack100CheckFighterKind macro it uses goes with it. */

/* ftcommonattack1.c:10-22 */
#define ftCommonAttack13CheckFighterKind(fp)\
(                                           \
    ((fp)->fkind == nFTKindMario)   ||      \
    ((fp)->fkind == nFTKindMMario)  ||      \
    ((fp)->fkind == nFTKindNMario)  ||      \
    ((fp)->fkind == nFTKindLuigi)   ||      \
    ((fp)->fkind == nFTKindNLuigi)  ||      \
    ((fp)->fkind == nFTKindCaptain) ||      \
    ((fp)->fkind == nFTKindNCaptain)||      \
    ((fp)->fkind == nFTKindLink)    ||      \
    ((fp)->fkind == nFTKindNLink)   ||      \
    ((fp)->fkind == nFTKindNess)    ||      \
    ((fp)->fkind == nFTKindNNess)           \
)

/* ftcommonattack1.c:31-44 ftCommonAttack11ProcUpdate 0x80147DD4 */
void ftCommonAttack11ProcUpdate(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if ((fp->motion_vars.flags.flag1 != 0) && (fp->status_vars.common.attack1.is_goto_followup != FALSE))
    {
        if ((fp->fkind == nFTKindPikachu) || (fp->fkind == nFTKindNPikachu))
        {
            ftCommonAttack11SetStatus(fighter_gobj);
        }
        else ftCommonAttack12SetStatus(fighter_gobj);
    }
    else ftAnimEndSetWait(fighter_gobj);
}

/* ftcommonattack1.c:47-61 ftCommonAttack12ProcUpdate 0x80147E48 */
void ftCommonAttack12ProcUpdate(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if ((fp->fkind != nFTKindCaptain) && (fp->fkind != nFTKindNCaptain) && (fp->motion_vars.flags.flag1 != 0) && (fp->is_goto_attack100))
    {
        ftCommonAttack100StartSetStatus(fighter_gobj);
    }
    else if ((fp->motion_vars.flags.flag1 != 0) && (fp->status_vars.common.attack1.is_goto_followup != FALSE))
    {
        ftCommonAttack13SetStatus(fighter_gobj);
    }
    else ftAnimEndSetWait(fighter_gobj);
}

/* ftcommonattack1.c:64-73 ftCommonAttack13ProcUpdate 0x80147EDC */
void ftCommonAttack13ProcUpdate(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if (((fp->fkind == nFTKindCaptain) || (fp->fkind == nFTKindNCaptain)) && (fp->motion_vars.flags.flag1 != 0) && (fp->is_goto_attack100))
    {
        ftCommonAttack100StartSetStatus(fighter_gobj);
    }
    else ftAnimEndSetWait(fighter_gobj);
}

/* ftcommonattack1.c:76-99 ftCommonAttack11ProcInterrupt 0x80147F44 */
void ftCommonAttack11ProcInterrupt(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if (fp->status_vars.common.attack1.interrupt_catch_timer < 2)
    {
        fp->status_vars.common.attack1.interrupt_catch_timer++;

        if (ftCommonCatchCheckInterruptAttack11(fighter_gobj) != FALSE)
        {
            return;
        }
    }
    if (ftCommonAttack100StartCheckInterruptCommon(fighter_gobj) == FALSE)
    {
        if ((fp->fkind == nFTKindPikachu) || (fp->fkind == nFTKindNPikachu))
        {
            if (ftCommonAttack11CheckGoto(fighter_gobj) != FALSE)
            {
                return;
            }
        }
        else ftCommonAttack12CheckGoto(fighter_gobj);
    }
}

/* ftcommonattack1.c:102-108 ftCommonAttack12ProcInterrupt 0x80147FEC */
void ftCommonAttack12ProcInterrupt(GObj *fighter_gobj)
{
    if (ftCommonAttack13CheckGoto(fighter_gobj) == FALSE)
    {
        ftCommonAttack100StartCheckInterruptCommon(fighter_gobj);
    }
}

/* ftcommonattack1.c:111-114 ftCommonAttack13ProcInterrupt 0x80148024 */
void ftCommonAttack13ProcInterrupt(GObj *fighter_gobj)
{
    ftCommonAttack100StartCheckInterruptCommon(fighter_gobj);
}

/* ftcommonattack1.c:117-124 ftCommonAttack11ProcStatus 0x80148044 */
void ftCommonAttack11ProcStatus(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    ftParamSetMotionID(fp, nFTMotionAttackIDAttack11);
    ftParamSetStatUpdate(fp, fp->stat_flags.halfword);
    ftParamUpdate1PGameAttackStats(fp, 0);
}

/* ftcommonattack1.c:127-146 ftCommonAttack11SetStatus 0x80148088 */
void ftCommonAttack11SetStatus(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    FTAttributes *attr = fp->attr;

    if (ftCommonGetCheckInterruptCommon(fighter_gobj) == FALSE)
    {
        fp->proc_status = ftCommonAttack11ProcStatus;

        ftMainSetStatus(fighter_gobj, nFTCommonStatusAttack11, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);
        ftMainPlayAnimEventsAll(fighter_gobj);

        fp->motion_vars.flags.flag1 = 0;
        fp->status_vars.common.attack1.is_goto_followup = FALSE;
        fp->status_vars.common.attack1.interrupt_catch_timer = 0;
        fp->attack1_input_count = 0;
        fp->attack1_status_id = fp->status_id;
        fp->is_goto_attack100 = FALSE;
        fp->attack1_followup_frames = attr->attack1_followup_frames;
    }
}

/* ftcommonattack1.c:149-190 ftCommonAttack12SetStatus 0x8014811C */
void ftCommonAttack12SetStatus(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if (ftCommonGetCheckInterruptCommon(fighter_gobj) == FALSE)
    {
        ftMainSetStatus(fighter_gobj, nFTCommonStatusAttack12, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);
        ftMainPlayAnimEventsAll(fighter_gobj);

        fp->motion_vars.flags.flag1 = 0;
        fp->status_vars.common.attack1.is_goto_followup = FALSE;
        fp->attack1_status_id = fp->status_id;

        switch (fp->fkind)
        {
        case nFTKindMario:
        case nFTKindMMario:
        case nFTKindNMario:
            fp->attack1_followup_frames = FTCOMMON_ATTACK1_FOLLOWUP_FRAMES_DEFAULT;
            break;

        case nFTKindLuigi:
        case nFTKindNLuigi:
            fp->attack1_followup_frames = FTCOMMON_ATTACK1_FOLLOWUP_FRAMES_DEFAULT;
            break;

        case nFTKindCaptain:
        case nFTKindNCaptain:
            fp->attack1_followup_frames = FTCOMMON_ATTACK1_FOLLOWUP_FRAMES_DEFAULT;
            break;

        case nFTKindLink:
        case nFTKindNLink:
            fp->attack1_followup_frames = FTCOMMON_ATTACK1_FOLLOWUP_FRAMES_DEFAULT;
            break;

        case nFTKindNess:
        case nFTKindNNess:
            fp->attack1_followup_frames = FTCOMMON_ATTACK1_FOLLOWUP_FRAMES_DEFAULT;
            break;
        }
    }
}

/* ftcommonattack1.c:193-235 ftCommonAttack13SetStatus 0x801481E0 */
void ftCommonAttack13SetStatus(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    s32 status_id;

    if (ftCommonGetCheckInterruptCommon(fighter_gobj) == FALSE)
    {
        switch (fp->fkind)
        {
        case nFTKindMario:
        case nFTKindMMario:
        case nFTKindNMario:
            status_id = nFTMarioStatusAttack13;
            break;

        case nFTKindLuigi:
        case nFTKindNLuigi:
            status_id = nFTMarioStatusAttack13;
            break;

        case nFTKindCaptain:
        case nFTKindNCaptain:
            status_id = nFTCaptainStatusAttack13;
            break;

        case nFTKindLink:
        case nFTKindNLink:
            status_id = nFTLinkStatusAttack13;
            break;

        case nFTKindNess:
        case nFTKindNNess:
            status_id = nFTNessStatusAttack13;
            break;

        default:
            return;
        }
        ftMainSetStatus(fighter_gobj, status_id, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);
        ftMainPlayAnimEventsAll(fighter_gobj);

        fp->motion_vars.flags.flag1 = 0;
        fp->status_vars.common.attack1.is_goto_followup = FALSE;
        fp->attack1_status_id = fp->status_id;
    }
}

/* ftcommonattack1.c:238-318 ftCommonAttack1CheckInterruptCommon
 * 0x801482C0, whole as of the item-use step -- the held-item branch
 * (lines 244-269) is back: a jab with something in your hand is a
 * forward throw, a Z-jab a drop, a Beam Sword a swing and a Ray Gun a
 * shot, and only an empty hand reaches the jab cascade. */
sb32 ftCommonAttack1CheckInterruptCommon(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    FTAttributes *attr = fp->attr;

    if (fp->input.pl.button_tap & fp->input.button_mask_a)
    {
        if (fp->item_gobj != NULL)
        {
            if (itGetStruct(fp->item_gobj)->type == nITTypeThrow)
            {
                ftCommonItemThrowSetStatus(fighter_gobj, nFTCommonStatusLightThrowF);

                return TRUE;
            }
            if (fp->input.pl.button_hold & fp->input.button_mask_z)
            {
                ftCommonItemThrowSetStatus(fighter_gobj, nFTCommonStatusLightThrowDrop);

                return TRUE;
            }
            switch (itGetStruct(fp->item_gobj)->type)
            {
            case nITTypeSwing:
                ftCommonItemSwingSetStatus(fighter_gobj, nFTItemSwingTypeAttack1);
                return TRUE;

            case nITTypeShoot:
                ftCommonItemShootSetStatus(fighter_gobj);
                return TRUE;
            }
        }
        if (fp->attack1_followup_frames != 0.0F)
        {
            switch (fp->attack1_status_id)
            {
            case nFTCommonStatusAttack11:
                if ((fp->fkind == nFTKindPikachu) || (fp->fkind == nFTKindNPikachu))
                {
                    if (attr->is_have_attack11)
                    {
                        ftCommonAttack11SetStatus(fighter_gobj);

                        fp->attack1_input_count++;

                        return TRUE;
                    }
                    break;
                }
                else if (attr->is_have_attack12)
                {
                    ftCommonAttack12SetStatus(fighter_gobj);

                    fp->attack1_input_count++;

                    return TRUE;
                }
                break;

            case nFTCommonStatusAttack12:
                if (ftCommonAttack13CheckFighterKind(fp))
                {
                    ftCommonAttack13SetStatus(fighter_gobj);

                    return TRUE;
                }
                break;
            }
        }
        else if (attr->is_have_attack11)
        {
            ftCommonAttack11SetStatus(fighter_gobj);

            return TRUE;
        }
    }
    if (fp->attack1_followup_frames != 0.0F)
    {
        fp->attack1_followup_frames -= DObjGetStruct(fighter_gobj)->anim_speed;
    }
    return FALSE;
}

/* ftcommonattack1.c:321-341 ftCommonAttack11CheckGoto 0x80148470 */
sb32 ftCommonAttack11CheckGoto(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    FTAttributes *attr = fp->attr;

    if (fp->attack1_followup_frames != 0.0F)
    {
        fp->attack1_followup_frames -= DObjGetStruct(fighter_gobj)->anim_speed;

        if ((fp->input.pl.button_tap & fp->input.button_mask_a) && (attr->is_have_attack11))
        {
            if (fp->motion_vars.flags.flag1 != 0)
            {
                ftCommonAttack11SetStatus(fighter_gobj);

                return TRUE;
            }
            fp->status_vars.common.attack1.is_goto_followup = TRUE;
        }
    }
    return FALSE;
}

/* ftcommonattack1.c:344-364 ftCommonAttack12CheckGoto 0x80148518 */
sb32 ftCommonAttack12CheckGoto(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    FTAttributes *attr = fp->attr;

    if (fp->attack1_followup_frames != 0.0F)
    {
        fp->attack1_followup_frames -= DObjGetStruct(fighter_gobj)->anim_speed;

        if ((fp->input.pl.button_tap & fp->input.button_mask_a) && (attr->is_have_attack12))
        {
            if (fp->motion_vars.flags.flag1 != 0)
            {
                ftCommonAttack12SetStatus(fighter_gobj);

                return TRUE;
            }
            fp->status_vars.common.attack1.is_goto_followup = TRUE;
        }
    }
    return FALSE;
}

/* ftcommonattack1.c:367-392 ftCommonAttack13CheckGoto 0x801485C0 */
sb32 ftCommonAttack13CheckGoto(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if (!(ftCommonAttack13CheckFighterKind(fp)))
    {
        return FALSE;
    }
    else
    {
        if (fp->attack1_followup_frames != 0.0F)
        {
            fp->attack1_followup_frames -= DObjGetStruct(fighter_gobj)->anim_speed;

            if (fp->input.pl.button_tap & fp->input.button_mask_a)
            {
                if (fp->motion_vars.flags.flag1 != 0)
                {
                    ftCommonAttack13SetStatus(fighter_gobj);

                    return TRUE;
                }
                fp->status_vars.common.attack1.is_goto_followup = TRUE;
            }
        }
    }
    return FALSE;
}

/* The game asks whether a fighter has an animation for a motion by
 * reading its FTMotionDesc row off FTData (fp->data->mainmotion->
 * motion_desc[id].anim_file_id != 0, ftcommonattacks3.c:19,31 and
 * ftcommonattackhi3.c:14). DIVERGES only in where the row is: the
 * pack's motion table carries the same row (FPackMotion.anim, -1 for
 * none). */
static sb32 ftCommonMotionHasAnim(GObj *fighter_gobj, s32 motion_id)
{
    const Fighter *model = dc_model_of(fighter_gobj);

    return (motion_id < (s32)model->motion_count) &&
           (model->motions[motion_id].anim >= 0);
}

/* ftcommonattacks3.c:10-41 ftCommonAttackS3SetStatus 0x80148690 */
void ftCommonAttackS3SetStatus(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    f32 stick_angle;
    s32 status_id;

    if (ftCommonGetCheckInterruptCommon(fighter_gobj) == FALSE)
    {
        if (ftCommonMotionHasAnim(fighter_gobj, nFTCommonMotionAttackS3HiS))
        {
            stick_angle = ftParamGetStickAngleRads(fp);

            status_id = (stick_angle > FTCOMMON_ATTACKS3_5ANGLE_HI_MIN)  ? nFTCommonStatusAttackS3Hi  : // High-Angled Forward Tilt
                        (stick_angle > FTCOMMON_ATTACKS3_5ANGLE_HIS_MIN) ? nFTCommonStatusAttackS3HiS : // Middle High-Angled Forward Tilt
                        (stick_angle < FTCOMMON_ATTACKS3_5ANGLE_LW_MIN)  ? nFTCommonStatusAttackS3Lw  : // Low-Angled Forward Tilt
                        (stick_angle < FTCOMMON_ATTACKS3_5ANGLE_LWS_MIN) ? nFTCommonStatusAttackS3LwS : // Middle Low-Angled Forward Tilt
                                                                           nFTCommonStatusAttackS3;     // Default Forward Tilt
        }
        else if (ftCommonMotionHasAnim(fighter_gobj, nFTCommonMotionAttackS3Hi))
        {
            stick_angle = ftParamGetStickAngleRads(fp);

            status_id = (stick_angle > FTCOMMON_ATTACKS3_3ANGLE_HI_MIN)  ? nFTCommonStatusAttackS3Hi  : // High-Angled Forward Tilt
                        (stick_angle < FTCOMMON_ATTACKS3_3ANGLE_LW_MIN)  ? nFTCommonStatusAttackS3Lw  : // Low-Angled Forward Tilt
                                                                           nFTCommonStatusAttackS3;     // Default Forward Tilt
        }
        else status_id = nFTCommonStatusAttackS3;

        ftMainSetStatus(fighter_gobj, status_id, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);
        ftMainPlayAnimEventsAll(fighter_gobj);
    }
}

/* ftcommonattacks3.c:44-81 ftCommonAttackS3CheckInterruptCommon
 * 0x801487A8, whole as of the item-use step: the held-item branch
 * (lines 52-70) is back, and it differs from the jab's in that a held Z
 * throws forward rather than dropping. */
sb32 ftCommonAttackS3CheckInterruptCommon(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    FTAttributes *attr = fp->attr;

    if ((fp->input.pl.button_tap & fp->input.button_mask_a) && ((fp->input.pl.stick_range.x * fp->lr) >= FTCOMMON_ATTACKS3_STICK_RANGE_MIN))
    {
        if (((ftParamGetStickAngleRads(fp) < 0.0F) ? -ftParamGetStickAngleRads(fp) : ftParamGetStickAngleRads(fp)) <= F_CST_DTOR32(50.0F)) // 0.87266463F
        {
            if (fp->item_gobj != NULL)
            {
                if ((fp->input.pl.button_hold & fp->input.button_mask_z) || (itGetStruct(fp->item_gobj)->type == nITTypeThrow))
                {
                    ftCommonItemThrowSetStatus(fighter_gobj, nFTCommonStatusLightThrowF);

                    return TRUE;
                }
                switch (itGetStruct(fp->item_gobj)->type)
                {
                case nITTypeSwing:
                    ftCommonItemSwingSetStatus(fighter_gobj, nFTItemSwingTypeAttack3);
                    return TRUE;

                case nITTypeShoot:
                    ftCommonItemShootSetStatus(fighter_gobj);
                    return TRUE;
                }
            }
            if (attr->is_have_attacks3)
            {
                ftCommonAttackS3SetStatus(fighter_gobj);

                return TRUE;
            }
        }
    }
    return FALSE;
}

/* ftcommonattackhi3.c:10-28 ftCommonAttackHi3SetStatus 0x801488B0 */
void ftCommonAttackHi3SetStatus(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    f32 stick_angle;
    s32 status_id;

    if (ftCommonMotionHasAnim(fighter_gobj, nFTCommonMotionAttackHi3F))
    {
        stick_angle = syUtilsArcTan2(fp->input.pl.stick_range.y, fp->input.pl.stick_range.x * fp->lr);

        status_id = (stick_angle < F_CLC_DTOR32( 77.0F)) /* 1.3439035F */ ? nFTCommonStatusAttackHi3F : // WHAT
                    (stick_angle > F_CLC_DTOR32(103.0F)) /* 1.7976892F */ ? nFTCommonStatusAttackHi3B : // WHAT
                                                                            nFTCommonStatusAttackHi3;
    }
    else status_id = nFTCommonStatusAttackHi3;

    ftMainSetStatus(fighter_gobj, status_id, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);
    ftMainPlayAnimEventsAll(fighter_gobj);
}

/* ftcommonattackhi3.c:31-54 ftCommonAttackHi3CheckInterruptCommon
 * 0x80148978, whole as of the item-use step: an up-tilt with something
 * in your hands throws it upward instead. */
sb32 ftCommonAttackHi3CheckInterruptCommon(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    FTAttributes *attr = fp->attr;

    if ((fp->input.pl.button_tap & fp->input.button_mask_a) && (fp->input.pl.stick_range.y >= FTCOMMON_ATTACKHI3_STICK_RANGE_MIN))
    {
        if (ftParamGetStickAngleRads(fp) > F_CST_DTOR32(50.0F)) // 0.87266463F
        {
            if (ftCommonLightThrowCheckItemTypeThrow(fp) != FALSE)
            {
                ftCommonItemThrowSetStatus(fighter_gobj, nFTCommonStatusLightThrowHi);

                return TRUE;
            }
            else if (attr->is_have_attackhi3)
            {
                ftCommonAttackHi3SetStatus(fighter_gobj);

                return TRUE;
            }
        }
    }
    return FALSE;
}

/* ftcommonattacklw3.c:10-19 ftCommonAttackLw3ProcUpdate 0x80148A34 */
void ftCommonAttackLw3ProcUpdate(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if ((fp->motion_vars.flags.flag1 != 0) && (fp->status_vars.common.attacklw3.is_goto_attacklw3 != FALSE))
    {
        ftCommonAttackLw3SetStatus(fighter_gobj);
    }
    else ftAnimEndCheckSetStatus(fighter_gobj, ftCommonSquatWaitSetStatus);
}

/* ftcommonattacklw3.c:22-25 ftCommonAttackLw3ProcInterrupt 0x80148A94 */
void ftCommonAttackLw3ProcInterrupt(GObj *fighter_gobj)
{
    ftCommonAttackLw3CheckInterruptSelf(fighter_gobj);
}

/* ftcommonattacklw3.c:28-45 ftCommonAttackLw3CheckInterruptSelf 0x80148AB4 */
sb32 ftCommonAttackLw3CheckInterruptSelf(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    FTAttributes *attr = fp->attr;

    if ((fp->input.pl.button_tap & fp->input.button_mask_a) && (attr->is_have_attacklw3))
    {
        if (fp->motion_vars.flags.flag1 != 0)
        {
            ftCommonAttackLw3SetStatus(fighter_gobj);

            return TRUE;
        }
        else fp->status_vars.common.attacklw3.is_goto_attacklw3 = TRUE;
    }
    return FALSE;
}

/* ftcommonattacklw3.c:48-58 ftCommonAttackLw3InitStatusVars 0x80148B28 */
void ftCommonAttackLw3InitStatusVars(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    fp->motion_vars.flags.flag1 = 0;
    fp->status_vars.common.attacklw3.is_goto_attacklw3 = FALSE;

    ftParamSetMotionID(fp, nFTMotionAttackIDAttackLw3);
    ftParamSetStatUpdate(fp, fp->stat_flags.halfword);
    ftParamUpdate1PGameAttackStats(fp, 0);
}

/* ftcommonattacklw3.c:61-72 ftCommonAttackLw3SetStatus 0x80148B84 */
void ftCommonAttackLw3SetStatus(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if (ftCommonGetCheckInterruptCommon(fighter_gobj) == FALSE)
    {
        fp->proc_status = ftCommonAttackLw3InitStatusVars;

        ftMainSetStatus(fighter_gobj, nFTCommonStatusAttackLw3, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);
        ftMainPlayAnimEventsAll(fighter_gobj);
    }
}

/* ftcommonattacklw3.c:75-98 ftCommonAttackLw3CheckInterruptCommon
 * 0x80148BF4, whole as of the item-use step, the down-tilt's mirror of
 * the up-tilt above. */
sb32 ftCommonAttackLw3CheckInterruptCommon(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    FTAttributes *attr = fp->attr;

    if ((fp->input.pl.button_tap & fp->input.button_mask_a) && (fp->input.pl.stick_range.y <= FTCOMMON_ATTACKLW3_STICK_RANGE_MIN))
    {
        if (ftParamGetStickAngleRads(fp) < F_CST_DTOR32(-50.0F)) // -0.87266463F
        {
            if (ftCommonLightThrowCheckItemTypeThrow(fp) != FALSE)
            {
                ftCommonItemThrowSetStatus(fighter_gobj, nFTCommonStatusLightThrowLw);

                return TRUE;
            }
            else if (attr->is_have_attacklw3)
            {
                ftCommonAttackLw3SetStatus(fighter_gobj);

                return TRUE;
            }
        }
    }
    return FALSE;
}

/* ---- ft/ftcommon/ftcommonattacks4.c, ftcommonattackhi4.c and
 * ftcommonattacklw4.c -- the SMASH attacks, the last
 * interrupt hole in the ground cascades and the last seven rows of
 * dFTCommonActionStatusDescs (202-208, grown past but left zeroed by
 *).
 *
 * Ported here rather than compiled unmodified, and left that way by the
 * item-use step: the six checks' item branches are restored below, so
 * what keeps the three files out of the Makefile's unmodified list is
 * now only the two arms named next.
 *
 * Pikachu's and Ness's arms are the game's since: Pikachu's
 * forward smash throws a Thunder Shock spark off his tail on each motion
 * flag (efManagerPikachuThunderShockMakeEffect, src/dc/efmanager.c), with
 * the effect pause and resume hooks on the fighter, and Ness's bat
 * reflects while flag1 is up, through the reflector FTSpecialColl
 * ftNessMainMotionBindOffsets binds.
 *
 * The union aliasing the game leans on is inherited, not reproduced:
 * ftCommonAttack4StatusVars.lr sits at offset 0x10 of
 * FTCommonStatusVars, which is exactly where ftCommonTurnStatusVars
 * puts lr_turn -- so ftCommonAttackS4CheckInterruptTurn below measures
 * the stick against the direction the turn is heading, not the
 * direction the fighter is still facing. Nothing writes attack4.lr;
 * the turn does. Likewise attack4.is_goto_attacklw4 at 0x8 is written
 * by no one at all, and ftCommonAttackLw4CheckInterruptSquat reads
 * whatever the status before the squat left in that word. Both are
 * verbatim: the port compiles the decomp's own ft/ftcommon.h, so the
 * offsets are the game's. ------------------------------------------- */

/* ftcommonattacks4.c:12-68 ftCommonAttackS4ProcUpdate 0x8014FE40,
 * verbatim, the fallthrough from Pikachu's arm into Ness's included. */
void ftCommonAttackS4ProcUpdate(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    Vec3f offset;

    switch (fp->fkind)
    {
    case nFTKindPikachu:
    case nFTKindNPikachu:
        if ((fp->motion_vars.flags.flag1 != 0) || (fp->motion_vars.flags.flag2 != 0))
        {
            fp->status_vars.common.attack4.gfx_id += syUtilsRandIntRange((FTCOMMON_ATTACKS4_THUNDERSHOCK_GFX_ID_MAX - 1)) + 1;

            if (fp->status_vars.common.attack4.gfx_id >= FTCOMMON_ATTACKS4_THUNDERSHOCK_GFX_ID_MAX)
            {
                fp->status_vars.common.attack4.gfx_id -= FTCOMMON_ATTACKS4_THUNDERSHOCK_GFX_ID_MAX;
            }
            if (fp->motion_vars.flags.flag1 != 0)
            {
                fp->motion_vars.flags.flag1 = 0;

                offset.x = -FTCOMMON_ATTACKS4_THUNDERSHOCK_GFX_OFF_X;
                offset.z = FTCOMMON_ATTACKS4_THUNDERSHOCK_GFX_OFF_Z;
                offset.y = FTCOMMON_ATTACKS4_THUNDERSHOCK_GFX_OFF_Y;
            }
            if (fp->motion_vars.flags.flag2 != 0)
            {
                fp->motion_vars.flags.flag2 = 0;

                offset.x = FTCOMMON_ATTACKS4_THUNDERSHOCK_GFX_OFF_X;
                offset.z = FTCOMMON_ATTACKS4_THUNDERSHOCK_GFX_OFF_Z;
                offset.y = FTCOMMON_ATTACKS4_THUNDERSHOCK_GFX_OFF_Y;

            }
            gmCollisionGetFighterPartsWorldPosition(fp->joints[11], &offset);
            func_ovl2_800EE018(fp->joints[nFTPartsJointTopN], &offset);

            if (efManagerPikachuThunderShockMakeEffect(fighter_gobj, &offset, fp->status_vars.common.attack4.gfx_id) != NULL)
            {
                fp->is_effect_attach = TRUE;
            }
        }
        // Fallthrough, should break here for efficiency
    case nFTKindNess:
    case nFTKindNNess:
        if ((fp->motion_vars.flags.flag1 != 0) && !(fp->is_reflect))
        {
            fp->is_reflect = TRUE;
        }
        if ((fp->motion_vars.flags.flag1 == 0) && (fp->is_reflect))
        {
            fp->is_reflect = FALSE;
        }
        break;
    }
    ftAnimEndSetWait(fighter_gobj);
}

/* reloc_data.us.h's; the value is ftNessMainMotionBindOffsets' .set below */
extern int llNessMainMotionAttackS4ReflectorFTSpecialColl;

/* ftcommonattacks4.c:71-127 ftCommonAttackS4SetStatus 0x8014FFE0: the
 * five-way, three-way or one-way forward smash, picked by which
 * angled motions the fighter's pack actually carries -- the same shape
 * ftCommonAttackS3SetStatus above uses for the forward tilt, and the
 * same DIVERGES in reading it: the game asks
 * fp->data->mainmotion->motion_desc[..].anim_file_id != 0 and the port
 * asks ftCommonMotionHasAnim, because fp->data->mainmotion is not
 * filled here. Pikachu's and Ness's flag clears and tails are the
 * game's. */
void ftCommonAttackS4SetStatus(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    f32 stick_angle;
    s32 status_id;

    if (ftCommonMotionHasAnim(fighter_gobj, nFTCommonMotionAttackS4HiS))
    {
        stick_angle = ftParamGetStickAngleRads(fp);

        status_id = (stick_angle > FTCOMMON_ATTACKS4_5ANGLE_HI_MIN)  ? nFTCommonStatusAttackS4Hi  : // High-Angled Forward Smash
                    (stick_angle > FTCOMMON_ATTACKS4_5ANGLE_HIS_MIN) ? nFTCommonStatusAttackS4HiS : // Middle High-Angled Forward Smash
                    (stick_angle < FTCOMMON_ATTACKS4_5ANGLE_LW_MIN)  ? nFTCommonStatusAttackS4Lw  : // Low-Angled Forward Smash
                    (stick_angle < FTCOMMON_ATTACKS4_5ANGLE_LWS_MIN) ? nFTCommonStatusAttackS4LwS : // Middle Low-Angled Forward Smash
                                                                       nFTCommonStatusAttackS4;     // Default Forward Smash
    }
    else if (ftCommonMotionHasAnim(fighter_gobj, nFTCommonMotionAttackS4Hi))
    {
        stick_angle = ftParamGetStickAngleRads(fp);

        status_id = (stick_angle > FTCOMMON_ATTACKS4_3ANGLE_HI_MIN)  ? nFTCommonStatusAttackS4Hi  : // High-Angled Forward Smash
                    (stick_angle < FTCOMMON_ATTACKS4_3ANGLE_LW_MIN)  ? nFTCommonStatusAttackS4Lw  : // Low-Angled Forward Smash
                                                                       nFTCommonStatusAttackS4;     // Default Forward Smash
    }
    else status_id = nFTCommonStatusAttackS4;

    switch (fp->fkind)
    {
    case nFTKindPikachu:
    case nFTKindNPikachu:
        fp->motion_vars.flags.flag2 = 0;
        fp->motion_vars.flags.flag1 = 0;
        break;

    case nFTKindNess:
    case nFTKindNNess:
        fp->motion_vars.flags.flag1 = 0;
        break;
    }
    ftMainSetStatus(fighter_gobj, status_id, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);
    ftMainPlayAnimEventsAll(fighter_gobj);

    switch (fp->fkind)
    {
    case nFTKindPikachu:
    case nFTKindNPikachu:
        fp->status_vars.common.attack4.gfx_id = 0;

        fp->proc_lagstart = ftParamProcPauseEffect;
        fp->proc_lagend = ftParamProcResumeEffect;
        break;

    case nFTKindNess:
    case nFTKindNNess:
        fp->special_coll = (FTSpecialColl*) ((uintptr_t)gFTNessFileMainMotion + (intptr_t)&llNessMainMotionAttackS4ReflectorFTSpecialColl);
        break;
    }
}

/* ftcommonattacks4.c:130-168 ftCommonAttackS4CheckInterruptDash
 * 0x801501E0: the forward smash out of a dash, the one caller that does
 * not measure the stick against a buffer -- inside the dash's first
 * five frames the stick is already forward, so holding it and pressing
 * A is the whole input. Whole as of the item-use step: the item arm
 * (lines 138-158) is back ahead of the attribute gate. Note the game's
 * own precedence bug in the Z test, which the port keeps -- && binds
 * tighter than ||, so a Ray Gun with ammo falls through to the swing/
 * shoot switch and one without goes straight to the throw. */
sb32 ftCommonAttackS4CheckInterruptDash(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    FTAttributes *attr = fp->attr;

    if (((fp->input.pl.stick_range.x * fp->lr) >= FTCOMMON_ATTACKS4_STICK_RANGE_MIN) && (fp->input.pl.button_tap & fp->input.button_mask_a))
    {
        GObj *item_gobj = fp->item_gobj;

        if (item_gobj != NULL)
        {
            ITStruct *ip = itGetStruct(item_gobj);

            if ((fp->input.pl.button_hold & fp->input.button_mask_z) || ((ip->type == nITTypeThrow) || (ip->type == nITTypeShoot) && (itMainCheckShootNoAmmo(item_gobj) != FALSE)))
            {
                ftCommonItemThrowSetStatus(fighter_gobj, nFTCommonStatusLightThrowF4);

                return TRUE;
            }
            switch (ip->type)
            {
            case nITTypeSwing:
                ftCommonItemSwingSetStatus(fighter_gobj, nFTItemSwingTypeAttack4);
                return TRUE;

            case nITTypeShoot:
                ftCommonItemShootSetStatus(fighter_gobj);
                return TRUE;
            }
        }
        if (attr->is_have_attacks4)
        {
            ftCommonAttackS4SetStatus(fighter_gobj);

            return TRUE;
        }
    }
    return FALSE;
}

/* ftcommonattacks4.c:171-210 ftCommonAttackS4CheckInterruptTurn
 * 0x8015030C: the forward smash out of a turn, in the six frames the
 * turn's attacks4_buffer allows. attack4.lr is turn.lr_turn under
 * another name (see the section header), so the stick is measured
 * against the way the turn is going. Whole as of the item-use step:
 * the item arm (lines 179-199) is back, and unlike the dash's it picks
 * LightThrowF4 or LightThrowB4 by which way the stick points, and calls
 * ftParamSetStickLR before the swing and the shoot. */
sb32 ftCommonAttackS4CheckInterruptTurn(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    FTAttributes *attr = fp->attr;

    if (((fp->input.pl.stick_range.x * fp->status_vars.common.attack4.lr) >= FTCOMMON_ATTACKS4_STICK_RANGE_MIN) && (fp->input.pl.button_tap & fp->input.button_mask_a))
    {
        GObj *item_gobj = fp->item_gobj;

        if (item_gobj != NULL)
        {
            ITStruct *ip = itGetStruct(item_gobj);

            if ((fp->input.pl.button_hold & fp->input.button_mask_z) || ((ip->type == nITTypeThrow) || (ip->type == nITTypeShoot) && (itMainCheckShootNoAmmo(item_gobj) != FALSE)))
            {
                ftCommonItemThrowSetStatus(fighter_gobj, ((fp->input.pl.stick_range.x * fp->lr) >= 0) ? nFTCommonStatusLightThrowF4 : nFTCommonStatusLightThrowB4);

                return TRUE;
            }
            switch (ip->type)
            {
            case nITTypeSwing:
                ftParamSetStickLR(fp);
                ftCommonItemSwingSetStatus(fighter_gobj, nFTItemSwingTypeAttack4);
                return TRUE;

            case nITTypeShoot:
                ftParamSetStickLR(fp);
                ftCommonItemShootSetStatus(fighter_gobj);
                return TRUE;
            }
        }
        if (attr->is_have_attacks4)
        {
            ftParamSetStickLR(fp);
            ftCommonAttackS4SetStatus(fighter_gobj);

            return TRUE;
        }
    }
    return FALSE;
}

/* ftcommonattacks4.c:213-256 ftCommonAttackS4CheckInterruptCommon
 * 0x80150470: the forward smash everywhere else -- the stick tapped
 * sideways within three frames of the A press, which is what separates
 * it from the forward tilt (ftCommonAttackS3CheckInterruptCommon, which
 * takes the same input with the tap window expired). Whole as of the
 * item-use step; its item arm is the turn's, verbatim. */
sb32 ftCommonAttackS4CheckInterruptCommon(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    FTAttributes *attr = fp->attr;

    if ((ABS(fp->input.pl.stick_range.x) >= FTCOMMON_ATTACKS4_STICK_RANGE_MIN) && (fp->tap_stick_x < FTCOMMON_ATTACKS4_BUFFER_TICS_MAX) && (fp->input.pl.button_tap & fp->input.button_mask_a))
    {
        GObj *item_gobj = fp->item_gobj;

        if (item_gobj != NULL)
        {
            ITStruct *ip = itGetStruct(item_gobj);

            if ((fp->input.pl.button_hold & fp->input.button_mask_z) || ((ip->type == nITTypeThrow) || (ip->type == nITTypeShoot) && (itMainCheckShootNoAmmo(item_gobj) != FALSE)))
            {
                ftCommonItemThrowSetStatus(fighter_gobj, ((fp->input.pl.stick_range.x * fp->lr) >= 0) ? nFTCommonStatusLightThrowF4 : nFTCommonStatusLightThrowB4);

                return TRUE;
            }
            switch (ip->type)
            {
            case nITTypeSwing:
                ftParamSetStickLR(fp);
                ftCommonItemSwingSetStatus(fighter_gobj, nFTItemSwingTypeAttack4);
                return TRUE;

            case nITTypeShoot:
                ftParamSetStickLR(fp);
                ftCommonItemShootSetStatus(fighter_gobj);
                return TRUE;
            }
        }
        if (attr->is_have_attacks4)
        {
            ftParamSetStickLR(fp);
            ftCommonAttackS4SetStatus(fighter_gobj);

            return TRUE;
        }
    }
    return FALSE;
}

/* ftcommonattackhi4.c:11-15 ftCommonAttackHi4SetStatus 0x801505F0,
 * verbatim -- the up smash has one status and no angles */
void ftCommonAttackHi4SetStatus(GObj *fighter_gobj)
{
    ftMainSetStatus(fighter_gobj, nFTCommonStatusAttackHi4, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);
    ftMainPlayAnimEventsAll(fighter_gobj);
}

/* ftcommonattackhi4.c:18-25 ftCommonAttackHi4CheckInputSuccess
 * 0x80150628, verbatim */
sb32 ftCommonAttackHi4CheckInputSuccess(FTStruct *fp)
{
    if ((fp->input.pl.stick_range.y >= FTCOMMON_ATTACKHI4_STICK_RANGE_MIN) && (fp->input.pl.button_tap & fp->input.button_mask_a))
    {
        return TRUE;
    }
    else return FALSE;
}

/* ftcommonattackhi4.c:28-45 ftCommonAttackHi4CheckInterruptMain
 * 0x80150660, whole as of the item-use step: an up smash with a
 * throwable in hand is an up throw instead. */
sb32 ftCommonAttackHi4CheckInterruptMain(FTStruct *fp)
{
    FTAttributes *attr = fp->attr;

    if (ftCommonLightThrowCheckItemTypeThrow(fp) != FALSE)
    {
        ftCommonItemThrowSetStatus(fp->fighter_gobj, nFTCommonStatusLightThrowHi4);

        return TRUE;
    }
    if (attr->is_have_attackhi4)
    {
        ftCommonAttackHi4SetStatus(fp->fighter_gobj);

        return TRUE;
    }
    return FALSE;
}

/* ftcommonattackhi4.c:48-57 ftCommonAttackHi4CheckInterruptKneeBend
 * 0x801506CC: the up smash out of jumpsquat, the one caller with no tap
 * window -- the stick is already up, which is why the fighter is in
 * jumpsquat at all. Verbatim. */
sb32 ftCommonAttackHi4CheckInterruptKneeBend(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if (ftCommonAttackHi4CheckInputSuccess(fp) != FALSE)
    {
        return ftCommonAttackHi4CheckInterruptMain(fp);
    }
    else return FALSE;
}

/* ftcommonattackhi4.c:60-68 ftCommonAttackHi4CheckInterruptCommon
 * 0x8015070C, verbatim: the four-frame tap window is what separates the
 * up smash from the up tilt. */
sb32 ftCommonAttackHi4CheckInterruptCommon(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if ((ftCommonAttackHi4CheckInputSuccess(fp) != FALSE) && (fp->tap_stick_y < FTCOMMON_ATTACKHI4_BUFFER_TICS_MAX))
    {
        return ftCommonAttackHi4CheckInterruptMain(fp);
    }
    else return FALSE;
}

/* ftcommonattacklw4.c:11-15 ftCommonAttackLw4SetStatus 0x80150760,
 * verbatim */
void ftCommonAttackLw4SetStatus(GObj *fighter_gobj)
{
    ftMainSetStatus(fighter_gobj, nFTCommonStatusAttackLw4, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);
    ftMainPlayAnimEventsAll(fighter_gobj);
}

/* ftcommonattacklw4.c:18-25 ftCommonAttackLw4CheckInputSuccess
 * 0x80150798, verbatim */
sb32 ftCommonAttackLw4CheckInputSuccess(FTStruct *fp)
{
    if ((fp->input.pl.stick_range.y <= FTCOMMON_ATTACKLW4_STICK_RANGE_MIN) && (fp->input.pl.button_tap & fp->input.button_mask_a))
    {
        return TRUE;
    }
    else return FALSE;
}

/* ftcommonattacklw4.c:28-45 ftCommonAttackLw4CheckInterruptMain
 * 0x801507D0, whole too -- the down smash's mirror of Hi4's throw
 * arm above. */
sb32 ftCommonAttackLw4CheckInterruptMain(FTStruct *fp)
{
    FTAttributes *attr = fp->attr;

    if (ftCommonLightThrowCheckItemTypeThrow(fp) != FALSE)
    {
        ftCommonItemThrowSetStatus(fp->fighter_gobj, nFTCommonStatusLightThrowLw4);

        return TRUE;
    }
    if (attr->is_have_attacklw4)
    {
        ftCommonAttackLw4SetStatus(fp->fighter_gobj);

        return TRUE;
    }
    return FALSE;
}

/* ftcommonattacklw4.c:48-57 ftCommonAttackLw4CheckInterruptSquat
 * 0x80150838: the down smash out of a squat, gated not by a tap window
 * but by attack4.is_goto_attacklw4 -- a word no code in the game ever
 * writes (see the section header). Verbatim, alias and all. */
sb32 ftCommonAttackLw4CheckInterruptSquat(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if ((ftCommonAttackLw4CheckInputSuccess(fp) != FALSE) && (fp->status_vars.common.attack4.is_goto_attacklw4 != FALSE))
    {
        return ftCommonAttackLw4CheckInterruptMain(fp);
    }
    else return FALSE;
}

/* ftcommonattacklw4.c:60-68 ftCommonAttackLw4CheckInterruptCommon
 * 0x80150884, verbatim: the four-frame tap window, the down tilt's
 * separator. */
sb32 ftCommonAttackLw4CheckInterruptCommon(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if ((ftCommonAttackLw4CheckInputSuccess(fp) != FALSE) && (fp->tap_stick_y < FTCOMMON_ATTACKLW4_BUFFER_TICS_MAX))
    {
        return ftCommonAttackLw4CheckInterruptMain(fp);
    }
    else return FALSE;
}

/* ---- ft/ftcommon/ftcommonattackdash.c -- the dash attack, status 192,
 * the last row below the smashes that had no entry in
 * dFTCommonActionStatusDescs. Two functions, both whole as of the
 * item-use step -- the check's item arm came back with
 * ft/ftcommon/ftcommonitemthrow.c and ftcommonitemswing.c. ---------- */

/* ftcommonattackdash.c:11-14 ftCommonAttackDashSetStatus 0x8014F670,
 * verbatim. Note what it does NOT do: no ftMainPlayAnimEventsAll, unlike
 * every smash and tilt setter. The decomp's own asymmetry. */
void ftCommonAttackDashSetStatus(GObj *fighter_gobj)
{
    ftMainSetStatus(fighter_gobj, nFTCommonStatusAttackDash, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);
}

/* ftcommonattackdash.c:17-48 ftCommonAttackDashCheckInterruptCommon
 * 0x8014F69C: A alone, no stick test at all -- the fighter is already
 * running, and the dash and run cascades are the only two that call
 * this. Whole as of the item-use step: a dash attack with something in
 * your hands is a dash throw, or a swing if it swings. Note there is no
 * shoot arm here -- the game gives a running fighter no way to fire. */
sb32 ftCommonAttackDashCheckInterruptCommon(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    FTAttributes *attr = fp->attr;

    if (fp->input.pl.button_tap & fp->input.button_mask_a)
    {
        if (fp->item_gobj != NULL)
        {
            if ((fp->input.pl.button_hold & fp->input.button_mask_z) || (itGetStruct(fp->item_gobj)->type == nITTypeThrow))
            {
                ftCommonItemThrowSetStatus(fighter_gobj, nFTCommonStatusLightThrowDash);

                return TRUE;
            }
            if (itGetStruct(fp->item_gobj)->type == nITTypeSwing)
            {
                ftCommonItemSwingSetStatus(fighter_gobj, nFTItemSwingTypeAttackDash);

                return TRUE;
            }
        }
        if (attr->is_have_attackdash)
        {
            ftCommonAttackDashSetStatus(fighter_gobj);

            return TRUE;
        }
    }
    return FALSE;
}

/* ft/fighter.h:47-70 ftCommonGroundCheckInterrupt, the attack part of
 * it, whole since the three smashes, then the three tilts,
 * then the jab, in the game's order. Every ground cascade below runs it
 * where the macro puts the seven.
 *
 * DIVERGES in one thing only, `spelling'. The game writes this run of
 * seven out three times, and the three spellings differ in exactly two
 * of the entries:
 * COMMON (ft/fighter.h:53-55) -- what every ordinary ground status
 * takes: S4Common, Hi4Common, Lw4Common.
 * SQUAT (ftcommonsquat.c:14-16 and 34-36, the Squat and SquatWait
 * macros) -- Lw4Squat instead of Lw4Common: the down smash out of a
 * squat is gated by is_goto_attacklw4, not by the tap window.
 * TURN (ftcommonturn.c:56-70) -- no S4 at all here, because the turn
 * runs its own ahead of the is_disable_sa_interrupts gate, picking
 * S4Turn or S4Common off attacks4_buffer.
 * One function with a selector stands for the three spellings. */
static sb32 ftCommonAttackCheckInterruptGround(GObj *fighter_gobj, s32 spelling)
{
    if ((spelling != FTCOMMON_GROUNDATTACK_TURN) && (ftCommonAttackS4CheckInterruptCommon(fighter_gobj) != FALSE))
    {
        return TRUE;
    }
    return (ftCommonAttackHi4CheckInterruptCommon(fighter_gobj) != FALSE) ||
           (((spelling == FTCOMMON_GROUNDATTACK_SQUAT) ? ftCommonAttackLw4CheckInterruptSquat(fighter_gobj)
                                                       : ftCommonAttackLw4CheckInterruptCommon(fighter_gobj)) != FALSE) ||
           (ftCommonAttackS3CheckInterruptCommon(fighter_gobj) != FALSE)  ||
           (ftCommonAttackHi3CheckInterruptCommon(fighter_gobj) != FALSE) ||
           (ftCommonAttackLw3CheckInterruptCommon(fighter_gobj) != FALSE) ||
           (ftCommonAttack1CheckInterruptCommon(fighter_gobj) != FALSE);
}

/* ---- ft/ftcommon/ftcommonattackair.c -- the aerials, the
 * air twin of the four ground attacks above and the last interrupt hole
 * in the aerial cascades. Ported here rather than compiled unmodified,
 * a copy the item-use step left alone: the file itself is verbatim now
 * that its item branches link, but it is already transcribed here and
 * moving it into the Makefile's unmodified list is a separate, purely
 * mechanical change. ------------------------------------------------- */

/* ftcommonattackair.c:11-29 ftCommonAttackAirLwProcHit 0x801508E0: the
 * down-air's on-hit callback, and Link's alone -- his down-air spikes,
 * bounces him back up off whatever it hit, and rewinds far enough to hit
 * again, with rehit_timer counting the gap. Every other fighter's down-air
 * has no proc_hit at all (the setter below installs this one only for
 * AttackAirLw, and only Link's branch does anything). Verbatim. */
void ftCommonAttackAirLwProcHit(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if ((fp->fkind == nFTKindLink) || (fp->fkind == nFTKindNLink))
    {
        ftParamClearAttackCollAll(fighter_gobj);

        fp->is_fastfall = FALSE;

        fp->physics.vel_air.y = FTCOMMON_ATTACKAIRLW_LINK_REHIT_BOUNCE_VEL_Y;

        if (fighter_gobj->anim_frame > FTCOMMON_ATTACKAIRLW_LINK_REHIT_FRAME_BEGIN)
        {
            ftMainSetStatus(fighter_gobj, nFTCommonStatusAttackAirLw, FTCOMMON_ATTACKAIRLW_LINK_REHIT_FRAME_BEGIN, 1.0F, FTSTATUS_PRESERVE_NONE);
        }
        fp->status_vars.common.attackair.rehit_timer = FTCOMMON_ATTACKAIRLW_LINK_REHIT_TIMER;
    }
}

/* ftcommonattackair.c:32-50 ftCommonAttackAirLwProcUpdate 0x80150980,
 * verbatim: the other half of Link's rehit -- when the timer runs out
 * inside the window the two hitboxes are refreshed so the same swing can
 * connect again. For everyone else this is ftAnimEndSetFall and nothing
 * more, which is why only the AttackAirLw row names it. */
void ftCommonAttackAirLwProcUpdate(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if ((fp->fkind == nFTKindLink) || (fp->fkind == nFTKindNLink))
    {
        if (fp->status_vars.common.attackair.rehit_timer != 0)
        {
            fp->status_vars.common.attackair.rehit_timer--;

            if ((fp->status_vars.common.attackair.rehit_timer == 0) && (fighter_gobj->anim_frame < FTCOMMON_ATTACKAIRLW_LINK_REHIT_FRAME_END))
            {
                ftParamRefreshAttackCollID(fighter_gobj, 0);
                ftParamRefreshAttackCollID(fighter_gobj, 1);
            }
        }
    }
    ftAnimEndSetFall(fighter_gobj);
}

/* ftcommonattackair.c:53-75 ftCommonAttackAirProcMap 0x80150A08, verbatim:
 * every AttackAir row's map column, and the whole of the port's landing
 * lag. Three outcomes when the fighter touches down mid-aerial. If the
 * motion script left a lag percentage in flag1 and Z was not tapped
 * recently (the L-cancel: tics_since_last_z inside
 * FTCOMMON_ATTACKAIR_SMOOTHLANDING_TICS_MAX skips this arm entirely), he
 * plays the LandingAir animation for the attack he was doing -- or, where
 * his own pack has no such animation, the generic LandingAirNull at the
 * lag's own speed, which is what the has-an-animation test picks
 * between. Otherwise a fast enough descent
 * (vel_air.y above FTCOMMON_ATTACKAIR_SKIPLANDING_VEL_Y_MAX, i.e. barely
 * falling) skips the landing altogether into Wait, and anything else takes
 * the ordinary Landing. */
void ftCommonAttackAirProcMap(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if (mpCommonCheckFighterLanding(fighter_gobj) != FALSE)
    {
        if ((fp->motion_vars.flags.flag1 != 0) && (fp->tics_since_last_z > FTCOMMON_ATTACKAIR_SMOOTHLANDING_TICS_MAX))
        {
            s32 landing_motion_id = nFTCommonMotionLandingAirStart - nFTCommonMotionAttackAirStart;

            /* DIVERGES only in where the row is read: the game's
 * fp->data->mainmotion->motion_desc[..].anim_file_id != 0 is
 * the pack's motion table here, through the same
 * ftCommonMotionHasAnim the ground attacks use. */
            if (ftCommonMotionHasAnim(fighter_gobj, fp->motion_id + landing_motion_id))
            {
                ftCommonLandingAirSetStatus(fighter_gobj);
            }
            else ftCommonLandingAirNullSetStatus(fighter_gobj, F_PCT_TO_DEC(fp->motion_vars.flags.flag1));
        }
        else if (fp->physics.vel_air.y > FTCOMMON_ATTACKAIR_SKIPLANDING_VEL_Y_MAX)
        {
            ftCommonWaitSetStatus(fighter_gobj);
        }
        else ftCommonLandingSetStatus(fighter_gobj);
    }
}

/* ftcommonattackair.c:78-209 ftCommonAttackAirCheckInterruptCommon
 * 0x80150B00: the A tap in the air. The stick picks one of five -- a
 * neutral stick (both axes inside
 * FTCOMMON_ATTACKAIR_DIRECTION_STICK_RANGE_MIN) is the neutral air,
 * otherwise the stick's angle past +/-50 degrees is up or down and the rest
 * is forward or back against the facing -- each gated on the fighter's own
 * is_have_attackair* attribute. The chosen status is set with
 * FTSTATUS_PRESERVE_FASTFALL (an aerial does not cancel a fast fall),
 * flag1 is cleared so a landing before the script sets it costs no lag,
 * the anim events are played (which is what arms the hitboxes), and
 * tics_since_last_z is pushed to FTINPUT_ZTRIGLAST_TICS_MAX so the swing
 * cannot be L-cancelled by a Z tap that happened before it started.
 *
 * Whole as of the item-use step: both item halves are back. The
 * LightThrowAir arm (lines 91-143) takes over the whole direction
 * decision when a throwable is held -- a neutral stick with something
 * that is not nITTypeThrow simply drops it and returns FALSE, and every
 * other direction picks one of the eight nFTCommonStatusLightThrowAir*
 * statuses, doubled by whether the stick was held into it inside
 * FTCOMMON_LIGHTTHROWAIR4_BUFFER_TICS_MAX (a smash throw) or not. The
 * is_goto_shoot arm (lines 179-187) fires a held shootable instead of
 * swinging, and only off the neutral and forward airs -- the two the
 * else branch sets the flag on. */
sb32 ftCommonAttackAirCheckInterruptCommon(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    FTAttributes *attr = fp->attr;
    s32 status_id;
    ub32 is_have_attackair;
    f32 angle;
    sb32 is_goto_shoot = FALSE;

    if (fp->input.pl.button_tap & fp->input.button_mask_a)
    {
        if (ftHammerCheckHoldHammer(fighter_gobj) == FALSE)
        {
            if (ftCommonLightThrowCheckItemTypeThrow(fp) != FALSE)
            {
                if ((ABS(fp->input.pl.stick_range.x) < FTCOMMON_ATTACKAIR_DIRECTION_STICK_RANGE_MIN) && (ABS(fp->input.pl.stick_range.y) < FTCOMMON_ATTACKAIR_DIRECTION_STICK_RANGE_MIN))
                {
                    if (itGetStruct(fp->item_gobj)->type == nITTypeThrow)
                    {
                        ftCommonItemThrowSetStatus(fighter_gobj, nFTCommonStatusLightThrowAirF);

                        return TRUE;
                    }
                    else ftSetupDropItem(fp);

                    return FALSE;
                }
                else
                {
                    angle = ftParamGetStickAngleRads(fp);

                    if (angle > F_CST_DTOR32(50.0F)) // 0.87266463F
                    {
                        if (fp->hold_stick_y < FTCOMMON_LIGHTTHROWAIR4_BUFFER_TICS_MAX)
                        {
                            status_id = nFTCommonStatusLightThrowAirHi4;
                        }
                        else status_id = nFTCommonStatusLightThrowAirHi;
                    }
                    else if (angle < F_CST_DTOR32(-50.0F)) // -0.87266463F
                    {
                        if (fp->hold_stick_y < FTCOMMON_LIGHTTHROWAIR4_BUFFER_TICS_MAX)
                        {
                            status_id = nFTCommonStatusLightThrowAirLw4;
                        }
                        else status_id = nFTCommonStatusLightThrowAirLw;
                    }
                    else if ((fp->input.pl.stick_range.x * fp->lr) >= 0)
                    {
                        if (fp->hold_stick_x < FTCOMMON_LIGHTTHROWAIR4_BUFFER_TICS_MAX)
                        {
                            status_id = nFTCommonStatusLightThrowAirF4;
                        }
                        else status_id = nFTCommonStatusLightThrowAirF;
                    }
                    else if (fp->hold_stick_x < FTCOMMON_LIGHTTHROWAIR4_BUFFER_TICS_MAX)
                    {
                        status_id = nFTCommonStatusLightThrowAirB4;
                    }
                    else status_id = nFTCommonStatusLightThrowAirB;

                    ftCommonItemThrowSetStatus(fighter_gobj, status_id);

                    return TRUE;
                }
            }
            else
            {
                if ((ABS(fp->input.pl.stick_range.x) < FTCOMMON_ATTACKAIR_DIRECTION_STICK_RANGE_MIN) && (ABS(fp->input.pl.stick_range.y) < FTCOMMON_ATTACKAIR_DIRECTION_STICK_RANGE_MIN))
                {
                    status_id = nFTCommonStatusAttackAirN;
                    is_have_attackair = attr->is_have_attackairn;
                    is_goto_shoot = TRUE;
                }
                else
                {
                    angle = ftParamGetStickAngleRads(fp);

                    if (angle > F_CST_DTOR32(50.0F)) // 0.87266463F
                    {
                        status_id = nFTCommonStatusAttackAirHi;
                        is_have_attackair = attr->is_have_attackairhi;
                    }
                    else if (angle < F_CST_DTOR32(-50.0F)) // -0.87266463F
                    {
                        status_id = nFTCommonStatusAttackAirLw;
                        is_have_attackair = attr->is_have_attackairlw;
                    }
                    else if ((fp->input.pl.stick_range.x * fp->lr) >= 0)
                    {
                        status_id = nFTCommonStatusAttackAirF;
                        is_have_attackair = attr->is_have_attackairf;
                        is_goto_shoot = TRUE;
                    }
                    else
                    {
                        status_id = nFTCommonStatusAttackAirB;
                        is_have_attackair = attr->is_have_attackairb;
                    }
                }
            }
            if (is_goto_shoot != FALSE)
            {
                if ((fp->item_gobj != NULL) && (itGetStruct(fp->item_gobj)->type == nITTypeShoot))
                {
                    ftCommonItemShootAirSetStatus(fighter_gobj);

                    return TRUE;
                }
            }
            if (is_have_attackair)
            {
                fp->motion_vars.flags.flag1 = 0;

                ftMainSetStatus(fighter_gobj, status_id, 0.0F, 1.0F, FTSTATUS_PRESERVE_FASTFALL);

                if (status_id == nFTCommonStatusAttackAirLw)
                {
                    fp->proc_hit = ftCommonAttackAirLwProcHit;

                    fp->status_vars.common.attackair.rehit_timer = 0;
                }
                ftMainPlayAnimEventsAll(fighter_gobj);

                fp->tics_since_last_z = FTINPUT_ZTRIGLAST_TICS_MAX;

                return TRUE;
            }
        }
    }
    return FALSE;
}


/* ---- the damage side a landed hit calls into (ftMainProcParams,
 * src/dc/ftmain.c): ft/ftcommon/ftcommondamage.c. A hit pays the percent
 * and the hitlag and launches the victim into a Damage
 * status, which slides or arcs them under real knockback and returns them
 * to Wait/Fall when hitstun ends, the game's own way. Shield break, the
 * reflectors (ftmain.c's reflect path), the shield stun and the rebound
 * are all real.
 *
 * The level-3 clamp is gone as of a hit strong enough to
 * launch (damage_level 3) now sets a real DamageFly Lw/N/Hi and the
 * Air-common procs carry it into DamageFall, the tumble -- see "the
 * Fly/tumble launch" block below. Its landings are real too: the ground
 * tech (ftcommonpassive*.c), the knockdown floor (DownBounce/DownWait/
 * DownStand), and the wall bounce (ftcommonwalldamage.c). */

/* ftHammerCheckHoldHammer, ftCommonHammerFallSetStatus,
 * ftCommonHammerFallCheckInterruptCommon, ftHammerProcInterrupt and
 * ftCommonHammerFallProcInterrupt are implemented via ft/fthammer.c.
 * The five
 * ft/ftcommon/ftcommonhammer*.c unmodified. Nothing else in this file
 * changed for them: the callers were already written the game's way and
 * gated on ftHammerCheckHoldHammer, so making it real is what wires the
 * Hammer's six statuses in. */

/* ftCommonDokanStartCheckInterruptCommon, crouching into Mushroom
 * Kingdom's warp pipe: the whole of
 * ft/ftcommon/ftcommondokan.c is compiled unmodified
 * (the Makefile's FTCOMMON_STATUS_OBJS), with rows 62-65 below. */

/* the electric hit's colanim (ftparam.c) */
extern sb32 ftParamCheckSetSkeletonColAnimID(GObj *fighter_gobj, s32 damage_level);

/* ftKirbySpecialNDamageCheckLoseCopy was stubbed here on the claim
 * that it is unreachable with damage_level clamped to 2 and that
 * Kirby's ftchar is not ported. The second half stopped being true
 * -- ft/ftchar/ftkirby/ftkirbyspecialn.c is compiled
 * now -- so the real one links and the stub is gone. */

/* the sleep element's status setter used to be stubbed here, on the
 * claim that no ported move reaches it. It is now ported,
 * beside FuraFura's -- see ftcommonfurasleep.c's section below. */

/* ftKirbySpecialNApplyCaptureDamage is hand-copied here, verbatim
 * 59, when Yoshi's egg turned out to charge its five points of
 * damage through Kirby's helper -- the decomp's own joke, which it
 * annotates "Br0h why". ft/ftchar/ftkirby/ftkirbyspecialn.c is compiled
 * unmodified, so the copy is gone and the egg calls the real one. */

/* Yoshi's egg is modeled: efManagerYoshiEggExplodeMakeEffect and
 * efManagerYoshiEggLayMakeEffect are in src/dc/efmanager.c with their
 * SetAnim and ProcUpdate (romdisk/efegglay.mdl). A caught fighter is
 * visibly inside an egg again -- it sets is_invisible itself
 * (ftCommonYoshiEggSetStatus) -- and the egg cracking open has its burst. */

/* Kirby's Final Cutter blade and its three flashes (romdisk/efcut{up,down,
 * draw,trail}.mdl) are baked and the makers are in src/dc/efmanager.c.
 * ftKirbySpecialHiUpdateEffect clears motion_vars.flags.flag2 inside its
 * `if (make(...) != NULL)`, so the flag condition works correctly. */

/* Kirby's inhale wind is a script from his particle bank. The per-fighter
 * banks are loaded (ftManagerSetupFilesAllKind). efManagerKirbyInhaleWindMakeEffect,
 * efManagerCaptureKirbyStarMakeEffect and efManagerLoseKirbyStarMakeEffect
 * are in src/dc/efmanager.c with their pack
 * (), and so is efManagerStarSplashMakeEffect. */

/* ft/ftcommon/ftcommonthrown2.c:29-64 ftCommonThrownReleaseFighterLoseGrip
 * 0x8014ADB0: drops a fighter out of the grab -- for a mid-throw fighter,
 * reseats it below its own joint 4 world position, then reruns default
 * collision against the interactor's floor and kicks it airborne if it has
 * left the ground. Verbatim. */
void ftCommonThrownReleaseFighterLoseGrip(GObj *fighter_gobj)
{
    FTStruct *this_fp = ftGetStruct(fighter_gobj);
    GObj *interact_gobj;
    FTStruct *interact_fp;
    Vec3f pos;

    if (this_fp->is_catch_or_capture)
    {
        interact_gobj = this_fp->catch_gobj;
    }
    else interact_gobj = this_fp->capture_gobj;

    interact_fp = ftGetStruct(interact_gobj);

    if ((this_fp->status_id >= nFTCommonStatusThrownStart) && (this_fp->status_id <= nFTCommonStatusThrownEnd))
    {
        pos.x = pos.y = pos.z = 0.0F;

        gmCollisionGetFighterPartsWorldPosition(this_fp->joints[4], &pos);

        pos.y -= 300.0F;

        DObjGetStruct(fighter_gobj)->translate.vec.f = pos;
    }
    mpCommonRunFighterCollisionDefault(fighter_gobj, &DObjGetStruct(interact_gobj)->translate.vec.f, &interact_fp->coll_data);

    if (this_fp->ga == nMPKineticsGround)
    {
        if ((this_fp->coll_data.floor_line_id == -1) || (this_fp->coll_data.floor_dist != 0.0F))
        {
            mpCommonSetFighterAir(this_fp);
        }
    }
}

/* ft/ftcommon/ftcommonthrown2.c:66-79 ftCommonThrownDecideFighterLoseGrip
 * 0x8014AECC: parts a catcher (fighter_gobj) from the fighter it holds --
 * the barrel cannon, the tornado, a hit on either fighter, and the
 * Kirby/Yoshi/Falcon captures all let go through here. Verbatim. It was an
 * empty stub until, and a grab never broke. */
void ftCommonThrownDecideFighterLoseGrip(GObj *fighter_gobj, GObj *interact_gobj)
{
    FTStruct *this_fp = ftGetStruct(fighter_gobj);
    FTStruct *interact_fp = ftGetStruct(interact_gobj);

    if (this_fp->is_catch_or_capture)
    {
        ftCommonThrownReleaseFighterLoseGrip(fighter_gobj);
    }
    else ftCommonThrownReleaseFighterLoseGrip(interact_gobj);

    interact_fp->capture_gobj = NULL;
    this_fp->catch_gobj = NULL;
}

/* ft/ftcommon/ftcommonthrown2.c:187-197 ftCommonThrownUpdateDamageStats
 * 0x8014B2AC: a hit breaks the grab while the held fighter still keeps
 * hold, so that fighter takes the damage of the throw's second hit
 * (throw_desc[1]). Verbatim. */
void ftCommonThrownUpdateDamageStats(FTStruct *this_fp)
{
    GObj *capture_gobj = this_fp->capture_gobj;
    FTStruct *capture_fp = ftGetStruct(capture_gobj);
    FTThrowHitDesc *ft_throw = &capture_fp->throw_desc[1];
    s32 damage = ftParamGetStaledDamage(capture_fp->player, ft_throw->damage, capture_fp->motion_attack_id, capture_fp->motion_count);

    ftParamUpdateDamage(this_fp, damage);
    ftParamUpdatePlayerBattleStats(capture_fp->player, this_fp->player, damage);
    ftParamUpdateStaleQueue(capture_fp->player, this_fp->player, capture_fp->motion_attack_id, capture_fp->motion_count);
}

/* ft/ftcommon/ftcommonthrown2.c:200-258 ftCommonThrownSetStatusDamageRelease
 * 0x8014B330: the catcher is hit, and the fighter it held is launched free,
 * away from it, by the throw's second hit (throw_desc[1]). Verbatim. */
void ftCommonThrownSetStatusDamageRelease(GObj *fighter_gobj)
{
    FTStruct *this_fp = ftGetStruct(fighter_gobj);
    GObj *capture_gobj = this_fp->capture_gobj;
    FTStruct *capture_fp = ftGetStruct(capture_gobj);
    FTThrowHitDesc *ft_throw;
    f32 knockback_final;
    s32 lr;
    s32 damage;
    f32 knockback_resist;
    f32 knockback_calc;

    knockback_resist = (this_fp->knockback_resist_status < this_fp->knockback_resist_passive) ? this_fp->knockback_resist_passive : this_fp->knockback_resist_status;

    if (this_fp->hitstatus != nGMHitStatusNormal)
    {
        ftParamSetHitStatusAll(fighter_gobj, nGMHitStatusNormal);
    }
    if (!(this_fp->is_catch_or_capture))
    {
        ftCommonThrownReleaseFighterLoseGrip(fighter_gobj);
    }
    if (this_fp->ga == nMPKineticsAir)
    {
        mpCommonSetFighterAir(this_fp);
    }
    ft_throw = &capture_fp->throw_desc[1];

    knockback_calc = ftParamGetCommonKnockback(this_fp->percent_damage, ft_throw->damage, ft_throw->damage, ft_throw->knockback_weight, ft_throw->knockback_scale, ft_throw->knockback_base, this_fp->attr->weight, capture_fp->handicap, this_fp->handicap);

    knockback_final = knockback_calc - knockback_resist;

    if (knockback_calc <= knockback_resist)
    {
        knockback_final = 0.0F;
    }
    lr = (DObjGetStruct(fighter_gobj)->translate.vec.f.x < DObjGetStruct(capture_gobj)->translate.vec.f.x) ? +1 : -1;

    damage = ftParamGetStaledDamage(capture_fp->player, ft_throw->damage, capture_fp->motion_attack_id, capture_fp->motion_count);

    if (capture_fp->is_shield_catch)
    {
        damage = ((damage * 0.5F) + 0.999F);
    }
    if (ftParamGetBestHitStatusAll(fighter_gobj) != nGMHitStatusNormal)
    {
        damage = 0;
    }
    ftCommonDamageInitDamageVars(fighter_gobj, ft_throw->status_id, damage, knockback_final, ft_throw->angle, lr, 1, nGMHitElementNormal, capture_fp->player_num, FALSE, FALSE, TRUE);
    ftParamUpdate1PGameDamageStats(this_fp, capture_fp->player, nFTHitLogObjectFighter, capture_fp->fkind, capture_fp->stat_flags.halfword, capture_fp->stat_count);

    if (damage != 0)
    {
        ftParamUpdateDamage(this_fp, damage);
        ftParamUpdatePlayerBattleStats(capture_fp->player, this_fp->player, damage);
        ftParamUpdateStaleQueue(capture_fp->player, this_fp->player, capture_fp->motion_attack_id, capture_fp->motion_count);
    }
    this_fp->capture_gobj = NULL;
}

/* ft/ftcommon/ftcommonthrown2.c:11 dFTCommonThrownNoDamageKnockback
 * 0x801886E0: the damage-free launch below. Verbatim. */
FTThrowHitDesc dFTCommonThrownNoDamageKnockback = { -1, 0, 361, 0, 0, 20, 0 };

/* ft/ftcommon/ftcommonthrown2.c:261-287 ftCommonThrownSetStatusNoDamageRelease
 * 0x8014B5B4: the held fighter is hit out of the grab, and the catcher is
 * pushed back with a damage-free launch. Verbatim. */
void ftCommonThrownSetStatusNoDamageRelease(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    FTThrowHitDesc *ft_throw = &dFTCommonThrownNoDamageKnockback;
    f32 knockback_calc;
    f32 knockback_resist;
    f32 knockback_final;

    if (fp->knockback_resist_status < fp->knockback_resist_passive)
    {
        knockback_resist = fp->knockback_resist_passive;
    }
    else knockback_resist = fp->knockback_resist_status;

    DObjGetStruct(fighter_gobj)->translate.vec.f.z = 0.0F;

    knockback_calc = ftParamGetCommonKnockback(fp->percent_damage, ft_throw->damage, ft_throw->damage, ft_throw->knockback_weight, ft_throw->knockback_scale, ft_throw->knockback_base, fp->attr->weight, 9, fp->handicap);

    knockback_final = knockback_calc - knockback_resist;

    if (knockback_calc <= knockback_resist)
    {
        knockback_final = 0.0F;
    }
    ftCommonDamageInitDamageVars(fighter_gobj, ft_throw->status_id, 0, knockback_final, ft_throw->angle, fp->lr, 1, ft_throw->element, 0, TRUE, TRUE, FALSE);
    ftParamUpdate1PGameDamageStats(fp, GMCOMMON_PLAYERS_MAX, nFTHitLogObjectNone, 0, 0, 0);
}
/* ftSetupDropItem itself is a macro (ft/fighter.h:24-29, `{ Vec3f vel = 0;
 * itMainSetFighterDrop((fp)->item_gobj, &vel, 1.0F); }`), and the
 * it/itmain.c function it expands to was a stub here from--
 * an empty body, because no fighter could be holding anything. It is the
 * real one in src/dc/itmain.c as of the item-use step, so every
 * ftSetupDropItem in this file (the twister, the barrel cannon, the
 * capture, the KO) now really does put the item back on the stage. */
/* ftDonkeyThrowFDamageSetStatus was a stub here until  the
 * arm of ftCommonDamage that sends a carrying DK into ThrowFDamage when
 * he is hit. ft/ftchar/ftdonkey/ftdonkeythrowfdamage.c is compiled now. */

/* ft/ftmanager.c:399-415 ftManagerDestroyFighterWeapons 0x800D7994,
 * verbatim since: ftCommonDeadResetCommonVars
 * takes a KO'd fighter's boomerang off the field -- Link's, or Kirby's
 * copy of it. It was a stub, so the boomerang flew on after the KO. The
 * grab arm, ftCommonThrownDecideDeadResult, is below since G02. The
 * other leaf, `itMainDestroyItem`, is the real function in src/dc/
 * itmain.c since. */
void ftManagerDestroyFighterWeapons(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    switch (fp->fkind)
    {
    case nFTKindKirby:
    case nFTKindNKirby:
        ftKirbyCopyLinkSpecialNDestroyBoomerang(fighter_gobj);
        break;

    case nFTKindLink:
    case nFTKindNLink:
        ftLinkSpecialNDestroyBoomerang(fighter_gobj);
        break;
    }
}

/* ft/ftcommon/ftcommonthrown2.c:82-103 ftCommonThrownDecideDeadResult
 * 0x8014AF2C: a KO'd fighter lets go of, or is let go by, whoever it was
 * grabbing, and both go to Wait or Fall. Verbatim. */
void ftCommonThrownDecideDeadResult(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    GObj *interact_gobj = fp->catch_gobj;

    if (interact_gobj != NULL)
    {
        ftCommonThrownDecideFighterLoseGrip(fighter_gobj, interact_gobj);

        goto next;
    }
    else interact_gobj = fp->capture_gobj;

    if (interact_gobj != NULL)
    {
        ftCommonThrownDecideFighterLoseGrip(interact_gobj, fighter_gobj);

    next:
        mpCommonSetFighterWaitOrFall(fighter_gobj);
        mpCommonSetFighterWaitOrFall(interact_gobj);
    }
}

/* ft/ftcommon/ftcommondamage.c:13-28, verbatim: which Damage status a
 * hit picks, by damage level (0-3, clamped to 0-2 here) and angle index
 * (0 down, 1 neutral, 2 up). Row 3 (Fly*) is dead with the clamp above;
 * kept so the array matches the ROM byte for byte. */
s32 dFTCommonDamageStatusGroundIDs[][3] =
{
    { nFTCommonStatusDamageLw1,   nFTCommonStatusDamageN1,    nFTCommonStatusDamageHi1   },
    { nFTCommonStatusDamageLw2,   nFTCommonStatusDamageN2,    nFTCommonStatusDamageHi2   },
    { nFTCommonStatusDamageLw3,   nFTCommonStatusDamageN3,    nFTCommonStatusDamageHi3   },
    { nFTCommonStatusDamageFlyLw, nFTCommonStatusDamageFlyN,  nFTCommonStatusDamageFlyHi }
};

s32 dFTCommonDamageStatusAirIDs[][3] =
{
    { nFTCommonStatusDamageAir1,  nFTCommonStatusDamageAir1,  nFTCommonStatusDamageAir1  },
    { nFTCommonStatusDamageAir2,  nFTCommonStatusDamageAir2,  nFTCommonStatusDamageAir2  },
    { nFTCommonStatusDamageAir3,  nFTCommonStatusDamageAir3,  nFTCommonStatusDamageAir3  },
    { nFTCommonStatusDamageFlyLw, nFTCommonStatusDamageFlyN,  nFTCommonStatusDamageFlyHi }
};

/* ft/ftcommon/ftcommondamage.c:36-65 ftCommonDamageSetDustEffectInterval
 * 0x80140340, verbatim */
void ftCommonDamageSetDustEffectInterval(FTStruct *fp)
{
    f32 vel = (fp->ga == nMPKineticsAir) ? syVectorMag3D(&fp->physics.vel_damage_air) : ABSF(fp->physics.vel_damage_ground);
    s32 make_effect_wait;

    if (vel < FTCOMMON_DAMAGE_EFFECT_KNOCKBACK_LOW)
    {
        make_effect_wait = FTCOMMON_DAMAGE_EFFECT_WAIT_LOW;
    }
    else if (vel < FTCOMMON_DAMAGE_EFFECT_KNOCKBACK_MID_LOW)
    {
        make_effect_wait = FTCOMMON_DAMAGE_EFFECT_WAIT_MID_LOW;
    }
    else if (vel < FTCOMMON_DAMAGE_EFFECT_KNOCKBACK_MID)
    {
        make_effect_wait = FTCOMMON_DAMAGE_EFFECT_WAIT_MID;
    }
    else if (vel < FTCOMMON_DAMAGE_EFFECT_KNOCKBACK_MID_HIGH)
    {
        make_effect_wait = FTCOMMON_DAMAGE_EFFECT_WAIT_MID_HIGH;
    }
    else if (vel < FTCOMMON_DAMAGE_EFFECT_KNOCKBACK_HIGH)
    {
        make_effect_wait = FTCOMMON_DAMAGE_EFFECT_WAIT_HIGH;
    }
    else make_effect_wait = FTCOMMON_DAMAGE_EFFECT_WAIT_DEFAULT;

    fp->status_vars.common.damage.dust_effect_int = make_effect_wait;
}

/* ft/ftcommon/ftcommondamage.c:67-82 ftCommonDamageUpdateDustEffect
 * 0x80140454, verbatim */
void ftCommonDamageUpdateDustEffect(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if (fp->status_vars.common.damage.dust_effect_int != 0)
    {
        fp->status_vars.common.damage.dust_effect_int--;

        if (fp->status_vars.common.damage.dust_effect_int == 0)
        {
            ftParamMakeEffect(fighter_gobj, nEFKindDustExpandLarge, 4, NULL, NULL, fp->lr, FALSE, FALSE);
            ftCommonDamageSetDustEffectInterval(fp);
        }
    }
}

/* ft/ftcommon/ftcommondamage.c:84-98 ftCommonDamageDecHitStunSetPublic
 * 0x801404B8, verbatim */
void ftCommonDamageDecHitStunSetPublic(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if (fp->status_vars.common.damage.hitstun_tics != 0)
    {
        fp->status_vars.common.damage.hitstun_tics--;

        if (fp->status_vars.common.damage.hitstun_tics == 0)
        {
            fp->public_knockback = fp->status_vars.common.damage.public_knockback;
        }
    }
}

/* ft/ftcommon/ftcommondamage.c:100-111 ftCommonDamageCommonProcUpdate
 * 0x801404E0, verbatim: the ground-table Damage statuses' Proc Update
 * (ftcommonstatus.h rows 37-49, Hi/N/Lw x3, Air1-3, E1). */
void ftCommonDamageCommonProcUpdate(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    ftCommonDamageDecHitStunSetPublic(fighter_gobj);

    if ((fighter_gobj->anim_frame <= 0.0F) && (fp->status_vars.common.damage.hitstun_tics == 0))
    {
        mpCommonSetFighterWaitOrFall(fighter_gobj);
    }
}

/* ft/ftcommon/ftcommondamage.c:113-124 ftCommonDamageAirCommonProcUpdate
 * 0x8014053C, verbatim: the Fly-tier statuses' Proc Update (DamageE2,
 * DamageFly Hi/N/Lw/Top/Roll). When the flinch animation runs out and
 * hitstun is spent, the launch drops into the DamageFall tumble. */
void ftCommonDamageAirCommonProcUpdate(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    ftCommonDamageUpdateDustEffect(fighter_gobj);
    ftCommonDamageDecHitStunSetPublic(fighter_gobj);

    if ((fighter_gobj->anim_frame <= 0.0F) && (fp->status_vars.common.damage.hitstun_tics == 0))
    {
        ftCommonDamageFallSetStatusFromDamage(fighter_gobj);
    }
}

/* ft/ftcommon/ftcommondamage.c:127-138 ftCommonDamageCheckSetInvincible
 * 0x801405A0, verbatim */
void ftCommonDamageCheckSetInvincible(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if ((fp->hitlag_tics <= 0) && (fp->status_vars.common.damage.is_knockback_over != FALSE))
    {
        fp->status_vars.common.damage.is_knockback_over = FALSE;

        ftParamSetTimedHitStatusInvincible(fp, 1);
    }
}

/* ft/ftcommon/ftcommondamage.c:140-163 ftCommonDamageSetStatus
 * 0x801405E4, verbatim: the electric statuses' proc_passive re-enters
 * the real Damage status once the flash motion (DamageE1/E2) finishes. */
void ftCommonDamageSetStatus(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if (fp->hitlag_tics <= 0)
    {
        ftMainSetStatus(fighter_gobj, fp->status_vars.common.damage.status_id, 0.0F, 1.0F, (FTSTATUS_PRESERVE_DAMAGEPLAYER | FTSTATUS_PRESERVE_SHUFFLETIME));
        ftMainPlayAnimEventsAll(fighter_gobj);

        if (fp->status_id == nFTCommonStatusDamageFlyRoll)
        {
            ftCommonDamageFlyRollUpdateModelPitch(fighter_gobj);
        }
        fp->is_hitstun = TRUE;

        if (fp->status_vars.common.damage.is_knockback_over != FALSE)
        {
            fp->status_vars.common.damage.is_knockback_over = FALSE;

            ftParamSetTimedHitStatusInvincible(fp, 1);
        }
    }
}

/* ft/ftcommon/ftcommondamage.c:165-188 ftCommonDamageCommonProcInterrupt
 * 0x80140674. The real ftCommonFallProcInterrupt (ftcommonfall.c:9-16) is
 * the SpecialAir check, then AttackAir, then
 * ftCommonJumpAerialCheckInterruptCommon -- which is exactly the port's
 * own ftCommonJumpProcInterrupt, so this reuses it rather than defining a
 * second copy of the same cascade. Whole since(54 made the
 * SpecialAir check real, 55 the AttackAir one). */
void ftCommonDamageCommonProcInterrupt(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if (fp->status_vars.common.damage.hitstun_tics == 0)
    {
        fp->is_hitstun = FALSE;

        if (fp->ga == nMPKineticsAir)
        {
            if (ftHammerCheckHoldHammer(fighter_gobj) != FALSE)
            {
                ftCommonHammerFallProcInterrupt(fighter_gobj);
            }
            else ftCommonJumpProcInterrupt(fighter_gobj);
        }
        else if (ftHammerCheckHoldHammer(fighter_gobj) != FALSE)
        {
            ftHammerProcInterrupt(fighter_gobj);
        }
        else ftCommonWaitProcInterrupt(fighter_gobj);
    }
}

/* ft/ftcommon/ftcommondamage.c:191-201 ftCommonDamageAirCommonProcInterrupt
 * 0x8014070C, verbatim: once hitstun ends, the Fly-tier statuses hand
 * their interrupt to the DamageFall tumble's (act out of the air). */
void ftCommonDamageAirCommonProcInterrupt(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if (fp->status_vars.common.damage.hitstun_tics == 0)
    {
        fp->is_hitstun = FALSE;

        ftCommonDamageFallProcInterrupt(fighter_gobj);
    }
}

/* ft/ftcommon/ftcommondamage.c:203-211 ftCommonDamageFlyRollUpdateModelPitch
 * 0x80140744, verbatim. The level-3 id tables pick DamageFly Lw/N/Hi, so
 * DamageFlyRoll is still never entered, but kept: ftCommonDamageSetStatus
 * and ftCommonDamageInitDamageVars both check for it by name, the game's way. */
void ftCommonDamageFlyRollUpdateModelPitch(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    fp->joints[4]->rotate.vec.f.x = syUtilsArcTan2(fp->physics.vel_air.x + fp->physics.vel_damage_air.x, fp->physics.vel_air.y + fp->physics.vel_damage_air.y) * fp->lr;

    ftParamsUpdateFighterPartsTransformAll(fp->joints[4]);
}

/* ft/ftcommon/ftcommondamage.c:213-236 ftCommonDamageCommonProcPhysics
 * 0x801407A8, verbatim */
void ftCommonDamageCommonProcPhysics(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if (fp->ga == nMPKineticsAir)
    {
        if (fp->status_vars.common.damage.hitstun_tics == 0)
        {
            ftPhysicsApplyAirVelDriftFastFall(fighter_gobj);
        }
        else ftPhysicsApplyAirVelFriction(fighter_gobj);
    }
    else ftPhysicsApplyGroundVelFriction(fighter_gobj);

    if (fp->status_id == nFTCommonStatusDamageFlyRoll)
    {
        ftCommonDamageFlyRollUpdateModelPitch(fighter_gobj);
    }
    if ((fp->throw_gobj != NULL) && (syVectorMag3D(&fp->physics.vel_damage_air) < 70.0F))
    {
        ftParamClearAttackCollAll(fighter_gobj);
    }
}

/* ft/ftcommon/ftcommondamage.c:238-258 ftCommonDamageCommonProcLagUpdate
 * 0x80140878, verbatim: DI, the stick nudge on a hitlag frame. */
void ftCommonDamageCommonProcLagUpdate(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if (fp->hitlag_tics != 0)
    {
        if ((SQUARE(fp->input.pl.stick_range.x) + SQUARE(fp->input.pl.stick_range.y)) >= SQUARE(FTCOMMON_DAMAGE_SMASH_DI_RANGE_MIN))
        {
            if ((fp->tap_stick_x < FTCOMMON_DAMAGE_SMASH_DI_BUFFER_TICS_MAX) || (fp->tap_stick_y < FTCOMMON_DAMAGE_SMASH_DI_BUFFER_TICS_MAX))
            {
                Vec3f *translate = &DObjGetStruct(fighter_gobj)->translate.vec.f;

                translate->x += fp->input.pl.stick_range.x * FTCOMMON_DAMAGE_SMASH_DI_RANGE_MUL;
                translate->y += fp->input.pl.stick_range.y * FTCOMMON_DAMAGE_SMASH_DI_RANGE_MUL;

                fp->tap_stick_x = fp->tap_stick_y = FTINPUT_STICKBUFFER_TICS_MAX;
            }
        }
    }
}

/* ft/ftcommon/ftcommondamage.c:261-278 ftCommonDamageAirCommonProcMap
 * 0x8014093C, verbatim: a launched fighter that touches a wall becomes
 * WallDamage (real as of), and one that touches the floor
 * techs (Passive/PassiveStand, real as of) or, failing that,
 * knocks down (DownBounce, the knockdown floor of). No DIVERGES:
 * every arm of the chain is now ported. */
void ftCommonDamageAirCommonProcMap(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if
    (
        (mpCommonCheckFighterDamageCollision(fighter_gobj) != FALSE)        &&
        (ftCommonWallDamageCheckGoto(fighter_gobj) == FALSE)                &&
        (fp->status_vars.common.damage.coll_mask_curr & MAP_FLAG_FLOOR)     &&
        (ftCommonPassiveStandCheckInterruptDamage(fighter_gobj) == FALSE)   &&
        (ftCommonPassiveCheckInterruptDamage(fighter_gobj) == FALSE)
    )
    {
        ftCommonDownBounceSetStatus(fighter_gobj);
    }
}

/* ft/ftcommon/ftcommondamage.c:284-311 ftCommonDamageGetKnockbackAngle
 * 0x801409BC, verbatim: angle 361 is Sakurai angle (auto), the angle
 * every one of Mario's jabs and tilts sends. */
f32 ftCommonDamageGetKnockbackAngle(s32 angle_i, sb32 ga, f32 knockback)
{
    f32 angle_f;

    if (angle_i != 361)
    {
        angle_f = F_CLC_DTOR32(angle_i);
    }
    else if (ga == nMPKineticsAir)
    {
        angle_f = FTCOMMON_DAMAGE_SAKURAI_ANGLE_DEFAULT_AR;
    }
    else if (knockback < FTCOMMON_DAMAGE_SAKURAI_KNOCKBACK_LOW)
    {
        angle_f = FTCOMMON_DAMAGE_SAKURAI_ANGLE_LOW_GR;
    }
    else
    {
        angle_f = F_CLC_DTOR32((((knockback - FTCOMMON_DAMAGE_SAKURAI_KNOCKBACK_LOW) / 0.099998474F) * FTCOMMON_DAMAGE_SAKURAI_ANGLE_HIGH_GD) + 1.0F);

        if (angle_f > FTCOMMON_DAMAGE_SAKURAI_ANGLE_HIGH_GR)
        {
            angle_f = FTCOMMON_DAMAGE_SAKURAI_ANGLE_HIGH_GR;
        }
    }
    return angle_f;
}

/* ft/ftcommon/ftcommondamage.c:313-333 ftCommonDamageGetDamageLevel
 * 0x80140A94, verbatim */
s32 ftCommonDamageGetDamageLevel(f32 hitstun)
{
    s32 damage_level;

    if (hitstun < FTCOMMON_DAMAGE_LEVEL_HITSTUN_LOW)
    {
        damage_level = 0; /* DamageX1 */
    }
    else if (hitstun < FTCOMMON_DAMAGE_LEVEL_HITSTUN_MID)
    {
        damage_level = 1; /* DamageX2 */
    }
    else if (hitstun < FTCOMMON_DAMAGE_LEVEL_HITSTUN_HIGH)
    {
        damage_level = 2; /* DamageX3 */
    }
    else damage_level = 3; /* airborne: Fly/tumble */

    return damage_level;
}

/* ft/ftcommon/ftcommondamage.c:335-356 ftCommonDamageSetPublic
 * 0x80140B00, verbatim. ftPublicCommonCheck is the crowd, and
 * it is ft/ftpublic.c itself, compiled unmodified. */
void ftCommonDamageSetPublic(FTStruct *this_fp, f32 knockback, f32 angle)
{
    GObj *attacker_gobj = ftParamGetPlayerNumGObj(this_fp->damage_player_num);
    sb32 is_force_curr_knockback;

    this_fp->status_vars.common.damage.public_knockback = knockback;
    this_fp->public_knockback = 0.0F;

    if ((angle > FTCOMMON_DAMAGE_PUBLIC_REACT_GASP_ANGLE_LOW) && (angle < FTCOMMON_DAMAGE_PUBLIC_REACT_GASP_ANGLE_HIGH))
    {
        this_fp->status_vars.common.damage.public_knockback *= FTCOMMON_DAMAGE_PUBLIC_REACT_GASP_KNOCKBACK_MUL;
    }
    if ((attacker_gobj != NULL) && (ftGetStruct(attacker_gobj)->public_knockback >= FTCOMMON_DAMAGE_KNOCKBACK_VERYHIGH))
    {
        is_force_curr_knockback = TRUE;
    }
    else is_force_curr_knockback = FALSE;

    ftPublicCommonCheck(this_fp->fighter_gobj, this_fp->status_vars.common.damage.public_knockback, is_force_curr_knockback);
}

/* ft/ftcommon/ftcommondamage.c:358-382 ftCommonDamageCheckElementSetColAnim
 * 0x80140BCC, verbatim. */
sb32 ftCommonDamageCheckElementSetColAnim(GObj *fighter_gobj, s32 element, s32 damage_level)
{
    sb32 is_set_colanim;

    switch (element)
    {
    case nGMHitElementFire:
        is_set_colanim = ftParamCheckSetFighterColAnimID(fighter_gobj, damage_level + nGMColAnimFighterDamageFireStart, 0);
        break;

    case nGMHitElementElectric:
        is_set_colanim = ftParamCheckSetSkeletonColAnimID(fighter_gobj, damage_level);
        break;

    case nGMHitElementFreezing:
        is_set_colanim = ftParamCheckSetFighterColAnimID(fighter_gobj, damage_level + nGMColAnimFighterDamageIceStart, 0);
        break;

    default:
        is_set_colanim = ftParamCheckSetFighterColAnimID(fighter_gobj, nGMColAnimFighterDamageCommon, 0);
        break;
    }
    return is_set_colanim;
}

/* ft/ftcommon/ftcommondamage.c:384-417 ftCommonDamageCheckMakeScreenFlash
 * 0x80140C50, verbatim; ifScreenFlashSetColAnimID is src/dc/ifscreenflash.c's
 * since. */
void ftCommonDamageCheckMakeScreenFlash(f32 knockback, s32 element)
{
    switch (element)
    {
    case nGMHitElementFire:
        if (knockback > FTCOMMON_DAMAGE_KNOCKBACK_VERYHIGH)
        {
            ifScreenFlashSetColAnimID(nGMColAnimScreenFlashDamageFire, 0);
        }
        break;

    case nGMHitElementElectric:
        if (knockback > FTCOMMON_DAMAGE_KNOCKBACK_VERYHIGH)
        {
            ifScreenFlashSetColAnimID(nGMColAnimScreenFlashDamageElectric, 0);
        }
        break;

    case nGMHitElementFreezing:
        if (knockback > FTCOMMON_DAMAGE_KNOCKBACK_VERYHIGH)
        {
            ifScreenFlashSetColAnimID(nGMColAnimScreenFlashDamageIce, 0);
        }
        break;

    default:
        if (knockback > FTCOMMON_DAMAGE_KNOCKBACK_VERYHIGH)
        {
            ifScreenFlashSetColAnimID(nGMColAnimScreenFlashDamageNormal, 0);
        }
        break;
    }
}

/* ft/ftcommon/ftcommondamage.c:419-444 ftCommonDamageCheckCatchResist
 * 0x80140D30, verbatim; the Donkey Kong arm is dead for Mario (fkind
 * never matches) but kept so the range check reads the same as the ROM. */
sb32 ftCommonDamageCheckCatchResist(FTStruct *fp)
{
    if (fp->damage_element == nGMHitElementSleep)
    {
        return FALSE;
    }
    if (fp->damage_knockback == 0.0F)
    {
        return TRUE;
    }
    if ((fp->hitlag_tics > 0) && (fp->is_knockback_paused) && (fp->damage_knockback < (fp->damage_knockback_stack + 30.0F)))
    {
        return TRUE;
    }
    if ((fp->fkind == nFTKindDonkey) || (fp->fkind == nFTKindNDonkey) || (fp->fkind == nFTKindGDonkey))
    {
        if ((fp->status_id >= nFTDonkeyStatusThrowFStart) && (fp->status_id <= nFTDonkeyStatusThrowFEnd) && (ftCommonDamageGetDamageLevel(ftParamGetHitStun(fp->damage_knockback)) < 3))
        {
            return TRUE;
        }
    }
    return FALSE;
}

/* ft/ftcommon/ftcommondamage.c:446-460 ftCommonDamageUpdateCatchResist
 * 0x80140E2C, verbatim: what a hit does to a fighter whose hold resists
 * it -- the damage flash, or, while a hit is still landing, the cargo
 * carry's own damage status (ftDonkeyThrowFDamageSetStatus). */
void ftCommonDamageUpdateCatchResist(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if ((fp->damage_knockback == 0.0F) || ((fp->hitlag_tics > 0) && (fp->is_knockback_paused) && (fp->damage_knockback < (fp->damage_knockback_stack + 30.0F))))
    {
        ftCommonDamageSetDamageColAnim(fighter_gobj);
    }
    else
    {
        ftParamStopVoiceRunProcDamage(fighter_gobj);
        ftDonkeyThrowFDamageSetStatus(fighter_gobj);
    }
}

/* ft/ftcommon/ftcommondamage.c:462-470 ftCommonDamageCheckCaptureKeepHold
 * 0x80140EC0, verbatim. */
sb32 ftCommonDamageCheckCaptureKeepHold(FTStruct *fp)
{
    if (fp->damage_queue < FTCOMMON_DAMAGE_CATCH_RELEASE_THRESHOLD)
    {
        return TRUE;
    }
    else return FALSE;
}

/* ft/ftcommon/ftcommondamage.c:472-659 ftCommonDamageInitDamageVars
 * 0x80140EE4, verbatim but for one line marked below: knockback into a
 * velocity and a Damage status, the whole reason a hit now launches
 * someone instead of just adding percent. */
void ftCommonDamageInitDamageVars(GObj *this_gobj, s32 status_id_replace, s32 damage, f32 knockback, s32 angle_start, s32 damage_lr,
s32 damage_index, s32 element, s32 damage_player_num, sb32 is_rumble, sb32 is_force_damage_sfx, sb32 is_allow_losecopy)
{
    FTStruct *this_fp = ftGetStruct(this_gobj);
    GObj *attacker_gobj;
    f32 angle_end = ftCommonDamageGetKnockbackAngle(angle_start, this_fp->ga, knockback);
    f32 vel_x = __cosf(angle_end) * knockback;
    f32 vel_y = __sinf(angle_end) * knockback;
    f32 hitstun_tics = ftParamGetHitStun(knockback);
    s32 damage_level;
    s32 status_id_set;
    s32 status_id_var;
    Vec3f vel_damage;
    f32 angle_diff;

    this_fp->status_vars.common.damage.hitstun_tics = hitstun_tics;

    if (this_fp->status_vars.common.damage.hitstun_tics == 0)
    {
        this_fp->status_vars.common.damage.hitstun_tics = 1;
    }
    damage_level = ftCommonDamageGetDamageLevel(hitstun_tics);

    /*  the level-3 clamp is gone -- a launch now reaches the
 * Fly/tumble it always meant to (the id tables' row 3 picks
 * DamageFly Lw/N/Hi, the Air-common procs and DamageFall below carry
 * it, the block comment above records the DIVERGES that remain). */
    if (status_id_replace != -1)
    {
        damage_level = 3;
    }
    this_fp->lr = damage_lr;

    if (this_fp->ga == nMPKineticsAir)
    {
        status_id_var = status_id_set = dFTCommonDamageStatusAirIDs[damage_level][damage_index];

        this_fp->physics.vel_damage_air.x = -vel_x * this_fp->lr;
        this_fp->physics.vel_damage_air.y = vel_y;
        this_fp->physics.vel_damage_ground = 0.0F;
    }
    else
    {
        vel_damage.x = -vel_x * this_fp->lr;
        vel_damage.y = vel_y;
        vel_damage.z = 0.0F;

        angle_diff = syVectorAngleDiff3D(&this_fp->coll_data.floor_angle, &vel_damage);

        if (angle_diff < F_CST_DTOR32(90.0F))
        {
            status_id_var = status_id_set = dFTCommonDamageStatusGroundIDs[damage_level][damage_index];

            mpCommonSetFighterAir(this_fp);

            this_fp->physics.vel_damage_air.x = vel_damage.x;
            this_fp->physics.vel_damage_air.y = vel_damage.y;
            this_fp->physics.vel_damage_ground = 0.0F;
        }
        else if (damage_level == 3)
        {
            mpCommonSetFighterAir(this_fp);

            status_id_var = status_id_set = dFTCommonDamageStatusGroundIDs[damage_level][damage_index];

            if (angle_diff > F_CST_DTOR32(100.0F))
            {
                this_fp->physics.vel_damage_air.x = vel_damage.x;
                this_fp->physics.vel_damage_air.y = -vel_damage.y * 0.8F;
                this_fp->physics.vel_damage_ground = 0.0F;

                ftParamMakeEffect(this_gobj, nEFKindImpactWave, nFTPartsJointTopN, NULL, NULL, this_fp->lr, 0, 0);
                ftParamMakeEffect(this_gobj, nEFKindQuakeMag0, nFTPartsJointTopN, NULL, NULL, this_fp->lr, 0, 0);
            }
            else
            {
                this_fp->physics.vel_damage_air.x = vel_damage.x;
                this_fp->physics.vel_damage_air.y = vel_damage.y;
                this_fp->physics.vel_damage_ground = 0.0F;
            }
        }
        else
        {
            status_id_var = status_id_set = dFTCommonDamageStatusGroundIDs[damage_level][damage_index];

            this_fp->physics.vel_damage_ground = (-vel_x * this_fp->lr);
            this_fp->physics.vel_damage_air.x = this_fp->coll_data.floor_angle.y * (-vel_x * this_fp->lr);
            this_fp->physics.vel_damage_air.y = -this_fp->coll_data.floor_angle.x * (-vel_x * this_fp->lr);
        }
    }
    this_fp->physics.vel_air.x = this_fp->physics.vel_air.y = this_fp->physics.vel_air.z = this_fp->physics.vel_ground.x = 0.0F;

    if ((damage_level == 3) && (this_fp->ga == nMPKineticsAir))
    {
        if ((angle_end > FTCOMMON_DAMAGE_FIGHTER_FLYTOP_ANGLE_LOW) && (angle_end < FTCOMMON_DAMAGE_FIGHTER_FLYTOP_ANGLE_HIGH))
        {
            status_id_var = status_id_set = nFTCommonStatusDamageFlyTop;
        }
        else if ((this_fp->percent_damage >= FTCOMMON_DAMAGE_FIGHTER_FLYROLL_DAMAGE_MIN) && (syUtilsRandFloat() < FTCOMMON_DAMAGE_FIGHTER_FLYROLL_RANDOM_CHANCE))
        {
            status_id_var = status_id_set = nFTCommonStatusDamageFlyRoll;
        }
    }
    if (status_id_replace != -1)
    {
        status_id_set = status_id_replace;
    }

    if (((element == nGMHitElementElectric) && (status_id_set >= nFTCommonStatusDamageStart)) && (status_id_set <= nFTCommonStatusDamageEnd))
    {
        status_id_var = status_id_set;

        status_id_set = (damage_level == 3) ? nFTCommonStatusDamageE2 : nFTCommonStatusDamageE1;
    }
    this_fp->damage_player_num = damage_player_num;

    ftCommonDamageSetPublic(this_fp, knockback, angle_end);

    if (damage != 0)
    {
        ftCommonDamageCheckElementSetColAnim(this_gobj, element, damage_level);
    }
    ftCommonDamageCheckMakeScreenFlash(knockback, element);

    if ((damage_level == 3) && (is_allow_losecopy != FALSE))
    {
        ftKirbySpecialNDamageCheckLoseCopy(this_gobj);
    }
    ftMainSetStatus(this_gobj, status_id_set, 0.0F, 1.0F, FTSTATUS_PRESERVE_DAMAGEPLAYER);
    ftMainPlayAnimEventsAll(this_gobj);

    if (knockback >= 65000.0F)
    {
        this_fp->status_vars.common.damage.is_knockback_over = TRUE;
    }
    else this_fp->status_vars.common.damage.is_knockback_over = FALSE;

    if ((this_fp->status_id == nFTCommonStatusDamageE1) || (this_fp->status_id == nFTCommonStatusDamageE2))
    {
        this_fp->proc_passive = ftCommonDamageSetStatus;
        this_fp->status_vars.common.damage.status_id = status_id_var;
    }
    else this_fp->proc_passive = ftCommonDamageCheckSetInvincible;

    this_fp->proc_lagupdate = ftCommonDamageCommonProcLagUpdate;

    this_fp->tap_stick_x = this_fp->tap_stick_y = FTINPUT_STICKBUFFER_TICS_MAX;

    this_fp->damage_knockback_stack = knockback;

    if ((damage_level == 3) || (is_rumble != FALSE))
    {
        ftParamMakeRumble(this_fp, 2, 0);
    }
    if (this_fp->status_id == nFTCommonStatusDamageFlyRoll)
    {
        ftCommonDamageFlyRollUpdateModelPitch(this_gobj);
    }
    ftCommonDamageSetDustEffectInterval(this_fp);

    if (this_fp->status_vars.common.damage.dust_effect_int != 0)
    {
        this_fp->status_vars.common.damage.dust_effect_int = 1;
    }
    if (((hitstun_tics >= FTCOMMON_DAMAGE_FIGHTER_DAMAGEVOICE_MIN) && (this_fp->attr->damage_sfx != nSYAudioFGMVoiceEnd)) || (is_force_damage_sfx != FALSE))
    {
        func_800269C0_275C0(this_fp->attr->damage_sfx);
    }

    this_fp->is_hitstun = TRUE;
    this_fp->tics_since_last_z = FTINPUT_ZTRIGLAST_TICS_MAX;

    if ((damage_level == 3) && (knockback >= FTCOMMON_DAMAGE_FIGHTER_PLAYERTAG_KNOCKBACK_MIN))
    {
        ftParamSetPlayerTagWait(this_gobj, FTCOMMON_DAMAGE_FIGHTER_PLAYERTAG_HIDE_FRAMES);
    }
    this_fp->status_vars.common.damage.coll_mask_curr = 0;

    attacker_gobj = ftParamGetPlayerNumGObj(damage_player_num);

    if (attacker_gobj != NULL)
    {
        FTStruct *attacker_fp = ftGetStruct(attacker_gobj);

        attacker_fp->attack_count++;
        attacker_fp->attack_knockback = knockback;
    }
}

/* ft/ftcommon/ftcommondamage.c:661-675 ftCommonDamageGotoDamageStatus
 * 0x80141560, verbatim: sleep is dead (no move reaches it, see above). */
void ftCommonDamageGotoDamageStatus(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if (fp->is_cliff_hold)
    {
        fp->cliffcatch_wait = FTCOMMON_CLIFF_CATCH_WAIT;
    }
    if (fp->damage_element == nGMHitElementSleep)
    {
        ftCommonFuraSleepSetStatus(fighter_gobj);
    }
    else ftCommonDamageInitDamageVars(fighter_gobj, -1, fp->damage_queue, fp->damage_knockback, fp->damage_angle, fp->damage_lr, fp->damage_index, fp->damage_element, fp->damage_player_num, FALSE, FALSE, TRUE);
}

/* ft/ftcommon/ftcommondamage.c:677-692 ftCommonDamageUpdateDamageColAnim/
 * SetDamageColAnim 0x801415F8/0x80141648, verbatim */
void ftCommonDamageUpdateDamageColAnim(GObj *fighter_gobj, f32 knockback, s32 element)
{
    if (ftCommonDamageCheckElementSetColAnim(fighter_gobj, element, ftCommonDamageGetDamageLevel(ftParamGetHitStun(knockback))) != 0)
    {
        ftMainRunUpdateColAnim(fighter_gobj);
    }
}

void ftCommonDamageSetDamageColAnim(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    ftCommonDamageUpdateDamageColAnim(fighter_gobj, fp->damage_knockback, fp->damage_element);
}

/* ft/ftcommon/ftcommondamage.c:694-842 ftCommonDamageUpdateMain
 * 0x80141670, verbatim: a hit on a fighter holding someone (catch_gobj),
 * on a fighter being held (capture_gobj), or on a fighter carrying an item
 * breaks the grab or drops the item before the tail, a fresh hit, runs. */
void ftCommonDamageUpdateMain(GObj *fighter_gobj)
{
    FTStruct *this_fp = ftGetStruct(fighter_gobj);
    GObj *grab_gobj = this_fp->catch_gobj;
    FTStruct *grab_fp;

    if (grab_gobj != NULL)
    {
        grab_fp = ftGetStruct(grab_gobj);

        if (ftCommonDamageCheckCatchResist(this_fp) != FALSE)
        {
            if (grab_fp->damage_knockback != 0)
            {
                if (ftCommonDamageCheckCaptureKeepHold(grab_fp) != FALSE)
                {
                    grab_fp->damage_lag = this_fp->damage_lag;
                    grab_fp->hitlag_mul = this_fp->hitlag_mul;

                    ftCommonDamageUpdateCatchResist(fighter_gobj);

                    grab_fp->damage_kind = nFTDamageKindColAnim;
                }
                else
                {
                    ftCommonThrownDecideFighterLoseGrip(fighter_gobj, grab_gobj);
                    ftParamStopVoiceRunProcDamage(fighter_gobj);
                    ftCommonDamageGotoDamageStatus(fighter_gobj);

                    grab_fp->damage_kind = nFTDamageKindStatus;
                }
            }
            else ftCommonDamageUpdateCatchResist(fighter_gobj);
        }
        else if (grab_fp->damage_knockback != 0)
        {
            if (ftCommonDamageCheckCaptureKeepHold(grab_fp) != FALSE)
            {
                ftCommonThrownUpdateDamageStats(grab_fp);
            }
            ftCommonThrownDecideFighterLoseGrip(fighter_gobj, grab_gobj);
            ftParamStopVoiceRunProcDamage(fighter_gobj);
            ftCommonDamageGotoDamageStatus(fighter_gobj);

            grab_fp->damage_kind = nFTDamageKindStatus;
        }
        else
        {
            ftParamStopVoiceRunProcDamage(grab_gobj);
            ftCommonThrownSetStatusDamageRelease(grab_gobj);

            if (this_fp->is_catch_or_capture)
            {
                ftCommonThrownReleaseFighterLoseGrip(fighter_gobj);
            }
            this_fp->catch_gobj = NULL;

            ftParamStopVoiceRunProcDamage(fighter_gobj);
            ftCommonDamageGotoDamageStatus(fighter_gobj);
        }
        return;
    }
    else grab_gobj = this_fp->capture_gobj;

    if (grab_gobj != NULL)
    {
        grab_fp = ftGetStruct(grab_gobj);

        if (ftCommonDamageCheckCaptureKeepHold(this_fp) != FALSE)
        {
            if (grab_fp->damage_knockback != 0)
            {
                if (ftCommonDamageCheckCatchResist(grab_fp) != FALSE)
                {
                    this_fp->damage_lag = grab_fp->damage_lag;
                    this_fp->hitlag_mul = grab_fp->hitlag_mul;
                    grab_fp->damage_kind = nFTDamageKindCatch;

                    ftCommonDamageSetDamageColAnim(fighter_gobj);
                }
                else
                {
                    ftCommonThrownUpdateDamageStats(this_fp);
                    ftCommonThrownDecideFighterLoseGrip(grab_gobj, fighter_gobj);
                    ftParamStopVoiceRunProcDamage(fighter_gobj);
                    ftCommonDamageGotoDamageStatus(fighter_gobj);

                    grab_fp->damage_kind = nFTDamageKindStatus;
                }
            }
            else
            {
                grab_fp->hitlag_tics = ftParamGetHitLag(this_fp->damage_lag, grab_fp->status_id, grab_fp->hitlag_mul);

                this_fp->input.pl.button_tap = this_fp->input.pl.button_release = 0;

                if (this_fp->proc_lagstart != NULL)
                {
                    this_fp->proc_lagstart(fighter_gobj);
                }
                ftCommonDamageSetDamageColAnim(fighter_gobj);
            }
        }
        else if (grab_fp->damage_knockback != 0)
        {
            ftCommonThrownDecideFighterLoseGrip(grab_gobj, fighter_gobj);
            ftParamStopVoiceRunProcDamage(fighter_gobj);
            ftCommonDamageGotoDamageStatus(fighter_gobj);

            grab_fp->damage_kind = nFTDamageKindStatus;
        }
        else
        {
            ftCommonThrownDecideFighterLoseGrip(grab_gobj, fighter_gobj);
            ftParamStopVoiceRunProcDamage(fighter_gobj);
            ftCommonDamageGotoDamageStatus(fighter_gobj);
            ftParamStopVoiceRunProcDamage(grab_gobj);
            ftCommonThrownSetStatusNoDamageRelease(grab_gobj);
        }
        return;
    }
    if (this_fp->item_gobj != NULL)
    {
        if ((itGetStruct(this_fp->item_gobj)->weight == nITWeightHeavy) && ((this_fp->fkind == nFTKindDonkey) || (this_fp->fkind == nFTKindNDonkey) || (this_fp->fkind == nFTKindGDonkey)))
        {
            if (ftCommonDamageCheckCatchResist(this_fp) != FALSE)
            {
                ftCommonDamageUpdateCatchResist(fighter_gobj);
            }
            else
            {
                ftSetupDropItem(this_fp);
                ftParamStopVoiceRunProcDamage(fighter_gobj);
                ftCommonDamageGotoDamageStatus(fighter_gobj);
            }
            return;
        }
    }
    if ((this_fp->damage_element != nGMHitElementSleep) && ((this_fp->damage_knockback == 0.0F) || ((this_fp->hitlag_tics > 0) && (this_fp->is_knockback_paused) && (this_fp->damage_knockback < (this_fp->damage_knockback_stack + 30.0F)))))
    {
        ftCommonDamageSetDamageColAnim(fighter_gobj);
    }
    else
    {
        ftParamStopVoiceRunProcDamage(fighter_gobj);
        ftCommonDamageGotoDamageStatus(fighter_gobj);
    }
}
/* ---- the Fly/tumble launch ------------------------------
 *
 * A hit strong enough to send someone (damage_level 3, knockback >= 60)
 * now sets a real DamageFly status instead of the level-2 stand-in the
 * clamp used to leave. The flinch runs, then ftCommonDamageAirCommonProc
 * Update (above) drops the fighter into DamageFall -- the tumble -- from
 * ft/ftcommon/ftcommondamagefall.c. DamageFall falls under air drift,
 * lets the fighter act out of it, catches a ledge, and on the ground
 * knocks down. Everything here is verbatim but for the three cuts the
 * ProcMap block comment names.
 *
 * The aerial hammer's check (ftCommonHammerFallCheckInterruptDamageFall)
 * is ftcommonhammerfall.c's, compiled unmodified; the wall bounce
 * (ftcommonwalldamage.c) is ported verbatim just below, and the ground
 * tech (ftcommonpassive*.c) since. */

/* ---- ft/ftcommon/ftcommonwalldamage.c, verbatim (the wall
 * bounce -- the launch family's last real branch). A launched fighter
 * (in a DamageFly tier) whose map proc finds a wall/ceiling next to it
 * (ftCommonWallDamageCheckGoto, called from ftCommonDamageAirCommonProcMap
 * where this was the FALSE stub) slams into it:
 * SetStatus reflects the stacked knockback off the surface normal, keeps
 * 0.8 of it, spawns the blue impact wave ('s efimpactwave.mdl)
 * and a quake, and enters the WallDamage status (56); ProcUpdate holds it
 * for the reflected hitstun, then drops back into DamageFall to finish the
 * fall. Deps all present: the 2D vector helpers (src/dc/lbcommon.c), the
 * impact wave and quake makers (src/dc/efmanager.c), ftParamGetHitStun /
 * ftParamMakeRumble / ftParamSetTimedHitStatusIntangible (src/dc/ftparam.c).
 * Nothing diverges -- every service it names is real. */

/* ft/ftcommon/ftcommonwalldamage.c:12-23 ftCommonWallDamageProcUpdate
 * 0x80141AC0, verbatim */
void ftCommonWallDamageProcUpdate(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    ftCommonDamageUpdateDustEffect(fighter_gobj);
    ftCommonDamageDecHitStunSetPublic(fighter_gobj);

    if (fp->status_vars.common.damage.hitstun_tics == 0)
    {
        ftCommonDamageFallSetStatusFromDamage(fighter_gobj);
    }
}

/* ft/ftcommon/ftcommonwalldamage.c:26-60 ftCommonWallDamageSetStatus
 * 0x80141B08, verbatim */
void ftCommonWallDamageSetStatus(GObj *fighter_gobj, Vec3f *angle, Vec3f *pos)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    Vec3f vel_air;
    f32 knockback;

    efManagerImpactWaveMakeEffect(pos, SYVECTOR_AXIS_Z, syUtilsArcTan2(-angle->x, angle->y));
    efManagerQuakeMakeEffect(2);

    vel_air = fp->physics.vel_air;

    lbCommonAdd2D(&vel_air, &fp->physics.vel_damage_air);
    lbCommonReflect2D(&vel_air, angle);
    lbCommonScale2D(&vel_air, 0.8F);

    fp->physics.vel_damage_air = vel_air;

    fp->physics.vel_air.x = fp->physics.vel_air.y = fp->physics.vel_air.z = 0.0F;

    fp->lr = (fp->physics.vel_damage_air.x < 0.0F) ? +1 : -1;

    knockback = lbCommonMag2D(&vel_air);

    fp->status_vars.common.damage.hitstun_tics = ftParamGetHitStun(knockback);

    ftMainSetStatus(fighter_gobj, nFTCommonStatusWallDamage, 0.0F, 2.0F, (FTSTATUS_PRESERVE_DAMAGEPLAYER | FTSTATUS_PRESERVE_PLAYERTAG));

    fp->damage_knockback_stack = knockback;

    ftParamMakeRumble(fp, 2, 0);

    ftParamSetTimedHitStatusIntangible(fp, FTCOMMON_WALLDAMAGE_INTANGIBLE_TIMER);

    fp->is_hitstun = FALSE;
}

/* ft/ftcommon/ftcommonwalldamage.c:63-99 ftCommonWallDamageCheckGoto
 * 0x80141C6C, verbatim */
sb32 ftCommonWallDamageCheckGoto(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    Vec3f pos;

    pos.x = DObjGetStruct(fighter_gobj)->translate.vec.f.x;
    pos.y = DObjGetStruct(fighter_gobj)->translate.vec.f.y;
    pos.z = 0.0F;

    if (fp->status_vars.common.damage.coll_mask_curr & MAP_FLAG_LWALL)
    {
        pos.x += fp->coll_data.map_coll.width;
        pos.y += fp->coll_data.map_coll.center;

        ftCommonWallDamageSetStatus(fighter_gobj, &fp->coll_data.lwall_angle, &pos);

        return TRUE;
    }
    else if (fp->status_vars.common.damage.coll_mask_curr & MAP_FLAG_RWALL)
    {
        pos.x -= fp->coll_data.map_coll.width;
        pos.y += fp->coll_data.map_coll.center;

        ftCommonWallDamageSetStatus(fighter_gobj, &fp->coll_data.rwall_angle, &pos);

        return TRUE;
    }
    else if (fp->status_vars.common.damage.coll_mask_curr & MAP_FLAG_CEIL)
    {
        pos.y += fp->coll_data.map_coll.top;

        ftCommonWallDamageSetStatus(fighter_gobj, &fp->coll_data.ceil_angle, &pos);

        return TRUE;
    }
    else return FALSE;
}

/* ftCommonHammerFallCheckInterruptDamageFall was a FALSE stub here from
 * the last of the hammer's; ft/ftcommon/ftcommonhammerfall.c
 * is compiled as of the item-use step and carries the real one. */

/* ft/ftcommon/ftcommonpassive.c:9-34 + ftcommonpassivestand.c:9-43, verbatim
 * (the ground tech). A fighter still inside the tech window
 * (tics_since_last_z < FTCOMMON_PASSIVE_BUFFER_TICS_MAX, a Z press buffered
 * within 20 frames of hitting the ground or a wall) recovers instantly
 * instead of knocking down: neutral (Passive) if the stick is centred, or a
 * forward/back roll (PassiveStandF/B) if it is pushed >= FTCOMMON_PASSIVE_F_
 * OR_B_RANGE toward or away from the facing. Both SetStatus fns ground an
 * airborne fighter (mpCommonSetFighterGround) and fold the launch velocity
 * along the floor (ftParamVelDamageTransferGround, ftparam.c). These are the
 * damage-side entries ftCommonDamageAirCommonProcMap and ftCommonDamageFall
 * ProcMap test before falling through to ftCommonDownBounceSetStatus. */
void ftCommonPassiveSetStatus(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if (fp->ga == nMPKineticsAir)
    {
        mpCommonSetFighterGround(fp);
    }
    ftMainSetStatus(fighter_gobj, nFTCommonStatusPassive, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);
    ftParamVelDamageTransferGround(fp);
}

sb32 ftCommonPassiveCheckInterruptDamage(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if (fp->tics_since_last_z < FTCOMMON_PASSIVE_BUFFER_TICS_MAX)
    {
        ftCommonPassiveSetStatus(fighter_gobj);

        return TRUE;
    }
    else return FALSE;
}

void ftCommonPassiveStandSetStatus(GObj *fighter_gobj, s32 status_id)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if (fp->ga == nMPKineticsAir)
    {
        mpCommonSetFighterGround(fp);
    }
    ftMainSetStatus(fighter_gobj, status_id, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);
    ftParamVelDamageTransferGround(fp);
}

sb32 ftCommonPassiveStandCheckInterruptDamage(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    s32 status_id;

    if (fp->tics_since_last_z < FTCOMMON_PASSIVE_BUFFER_TICS_MAX)
    {
        if (ABS(fp->input.pl.stick_range.x) >= FTCOMMON_PASSIVE_F_OR_B_RANGE)
        {
            if ((fp->input.pl.stick_range.x * fp->lr) >= 0)
            {
                status_id = nFTCommonStatusPassiveStandF;
            }
            else status_id = nFTCommonStatusPassiveStandB;

            ftCommonPassiveStandSetStatus(fighter_gobj, status_id);

            return TRUE;
        }
    }
    return FALSE;
}

/* ft/ftcommon/ftcommondamagefall.c:11-20 ftCommonDamageFallProcInterrupt
 * 0x80143560, whole as of the item-use step: the HammerFall arm behind
 * the aerial jump was a FALSE stub, so this stood as a call to
 * ftCommonJumpProcInterrupt, which is the same three checks without it.
 * ft/ftcommon/ftcommonhammerfall.c carries the real
 * ftCommonHammerFallCheckInterruptDamageFall now, so the tumbling
 * Hammer-holder swings out of the tumble the way the game lets them. */
void ftCommonDamageFallProcInterrupt(GObj *fighter_gobj)
{
    if
    (
        (ftCommonSpecialAirCheckInterruptCommon(fighter_gobj) == FALSE)     && 
        (ftCommonAttackAirCheckInterruptCommon(fighter_gobj) == FALSE)      &&
        (ftCommonJumpAerialCheckInterruptCommon(fighter_gobj) == FALSE)
    )
    {
        ftCommonHammerFallCheckInterruptDamageFall(fighter_gobj);
    }
}

/* ft/ftcommon/ftcommondamagefall.c:23-38 ftCommonDamageFallProcMap
 * 0x801435B0, verbatim: a tumbling fighter that reaches a ledge grabs it
 * (CliffCatch); one that reaches flat ground techs (Passive/PassiveStand,
 * real as of) or, failing that, knocks down (DownBounce). */
void ftCommonDamageFallProcMap(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if (mpCommonCheckFighterCliff(fighter_gobj) != FALSE)
    {
        if (fp->coll_data.mask_stat & MAP_FLAG_CLIFF_MASK)
        {
            ftCommonCliffCatchSetStatus(fighter_gobj);
        }
        else if ((ftCommonPassiveStandCheckInterruptDamage(fighter_gobj) == FALSE) && (ftCommonPassiveCheckInterruptDamage(fighter_gobj) == FALSE))
        {
            ftCommonDownBounceSetStatus(fighter_gobj);
        }
    }
}

/* ft/ftcommon/ftcommondamagefall.c:41-47 ftCommonDamageFallClampRumble
 * 0x80143630, verbatim */
void ftCommonDamageFallClampRumble(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    ftPhysicsClampAirVelXMax(fp);
    ftParamMakeRumble(fp, 3, 0);
}

/* ft/ftcommon/ftcommondamagefall.c:50-55 ftCommonDamageFallSetStatusFromDamage
 * 0x80143664, verbatim: the entry the Fly-tier Proc Update takes. */
void ftCommonDamageFallSetStatusFromDamage(GObj *fighter_gobj)
{
    ftMainSetStatus(fighter_gobj, nFTCommonStatusDamageFall, 0.0F, 1.0F, (FTSTATUS_PRESERVE_PLAYERTAG | FTSTATUS_PRESERVE_FASTFALL));
    ftCommonDamageFallClampRumble(fighter_gobj);
}

/* ft/ftcommon/ftcommondamagefall.c:58-67 ftCommonDamageFallSetStatusFromCliffWait
 * 0x801436A0, verbatim: a fighter whose ledge-hang timer runs out falls
 * as a tumble it can tech out of, now that DamageFall exists (the entry
 * ftCommonCliffWaitCheckFall below takes -- one DIVERGES removed). */
void ftCommonDamageFallSetStatusFromCliffWait(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    ftMainSetStatus(fighter_gobj, nFTCommonStatusDamageFall, 0.0F, 1.0F, FTSTATUS_PRESERVE_FASTFALL);
    ftCommonDamageFallClampRumble(fighter_gobj);

    fp->tics_since_last_z = FTINPUT_ZTRIGLAST_TICS_MAX;
}

/* ---- the shield break ------------------------------------
 *
 * The shield decays a point per FTCOMMON_GUARD_DECAY_INT it is held
 * (ftCommonGuardUpdateShieldVars above); when it reaches zero the
 * guard's three shield_health == 0 sites call ftCommonShieldBreakFlyCommon-
 * SetStatus and the fighter is launched into a five-status chain that ends
 * back in Wait: ShieldBreakFly (up), ShieldBreakFall, ShieldBreakDown (U or
 * D on the ground), ShieldBreakStand, then FuraFura (the dizzy). Each hands
 * to the next when its animation runs out; FuraFura ends when the breakout
 * counter, mashed down, hits zero. Verbatim from ft/ftcommon/ftcommon-
 * shieldbreak{fly,fall,down,stand}.c and ftcommonfurafura.c, but for the
 * DIVERGES on the entry.
 *
 * Two helper pairs come with it, both used only here for now. The two
 * ftCommonDownBounce* functions the Down status shares with the knockdown
 * family (ftcommondownwaitbounce.c), and the two capture breakout-var
 * functions FuraFura shares with the grab (ftcommoncapture.c). */

/* the ftMainSetStatus preserve masks, each #defined at the top of its
 * decomp file (ftcommonshieldbreak{fall,down,stand}.c). */
#define FTCOMMON_SHIELDBREAKFALL_STATUS_FLAGS (FTSTATUS_PRESERVE_DAMAGEPLAYER | FTSTATUS_PRESERVE_TEXTUREPART | FTSTATUS_PRESERVE_MODELPART | FTSTATUS_PRESERVE_HITSTATUS | FTSTATUS_PRESERVE_COLANIM)
#define FTCOMMON_SHIELDBREAKDOWN_STATUS_FLAGS (FTSTATUS_PRESERVE_TEXTUREPART | FTSTATUS_PRESERVE_MODELPART | FTSTATUS_PRESERVE_HITSTATUS | FTSTATUS_PRESERVE_COLANIM)
#define FTCOMMON_SHIELDBREAKSTAND_STATUS_FLAGS (FTSTATUS_PRESERVE_TEXTUREPART | FTSTATUS_PRESERVE_MODELPART | FTSTATUS_PRESERVE_HITSTATUS | FTSTATUS_PRESERVE_COLANIM)

/* dFTCommonDataDownBounceSFX (the per-kind bounce FGM) is in the linked
 * ft/ftcommondata.c dependency, declared in ft/ftcommondata.h; used by
 * ftCommonDownBounceUpdateEffects below. */

/* ftCommonCaptureTrappedInitBreakoutVars and ftCommonCaptureTrapped-
 * UpdateBreakoutVars (the mash that burns down breakout_wait) were
 * hand-copied here from ft/ftcommon/ftcommoncapture.c.
 * compiles that file itself -- Kirby's mouth needed two knockback
 * helpers out of it -- so both come from the decomp now and the
 * copies are gone. ftCommonCaptureShoulderedSetStatus came off the
 * same file and was a STUB; it is real code as of the same step. */

/* ft/ftcommon/ftcommondownwaitbounce.c:74-95 ftCommonDownBounceCheckUpOrDown
 * 0x80144398, verbatim: the hip joint's (index 4, nFTPartsJointCommonStart)
 * x rotation says whether the fighter lands on its back (down) or face (up). */
sb32 ftCommonDownBounceCheckUpOrDown(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    f32 rot_x = fp->joints[4]->rotate.vec.f.x;

    rot_x /= F_CST_DTOR32(360.0F);

    rot_x -= (s32)rot_x;

    if ((rot_x < -0.5F) || (rot_x > 0.0F) && (rot_x < 0.5F))
    {
        return 1;
    }
    else return 0;
}

/* ft/ftcommon/ftcommondownwaitbounce.c:98-105 ftCommonDownBounceUpdateEffects
 * 0x80144428, verbatim: the impact wave, the per-kind bounce FGM, and the
 * rumble. */
void ftCommonDownBounceUpdateEffects(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    ftParamMakeEffect(fighter_gobj, nEFKindImpactWave, nFTPartsJointTopN, NULL, NULL, fp->lr, FALSE, FALSE);
    func_800269C0_275C0(dFTCommonDataDownBounceSFX[fp->fkind]);
    ftParamMakeRumble(fp, 4, 0);
}

/* The knockdown floor, the rest of ftcommondownwaitbounce.c
 * plus ftcommondownstand.c: a landed tumble bounces once (DownBounce), lies
 * there (DownWait) until an auto-stand timer (or the player's stick-up/Z)
 * fires, then gets up (DownStand) into Wait. This replaces the step-14
 * DIVERGES where a landed tumble stood straight back up into Wait -- the
 * same "whiff to Wait" placeholder the grab used at before
 * connected it. The two helpers DownBounceSetStatus shares (CheckUpOrDown,
 * UpdateEffects) are already ported above for the shield break.
 *
 * The getups it can also break into -- DownForward/DownBack (the get-up
 * rolls, ftcommondownforwardback.c) and DownAttack (the get-up attack,
 * ftcommondownattack.c) -- are compiled in unmodified as dependencies (the
 * Makefile's FTCOMMON_STATUS_OBJS), so their CheckInterrupt arms are the
 * real decomp functions; this step registers their six status rows (75-80)
 * so those arms route somewhere real. The ground tech (Passive/PassiveStand)
 * that also breaks in from the damage side is real as of.
 * The DownStand interrupt's DokanStart fallthrough (Mushroom Kingdom's
 * pipe) is back since. */

#define FTCOMMON_DOWNBOUNCE_STATUS_FLAGS (FTSTATUS_PRESERVE_PLAYERTAG | FTSTATUS_PRESERVE_TEXTUREPART | FTSTATUS_PRESERVE_SLOPECONTOUR | FTSTATUS_PRESERVE_MODELPART)

/* ft/ftcommon/ftcommondownstand.c:21-35 ftCommonDownStandSetStatus 0x80144580,
 * verbatim: get up facing the way the fighter is lying (D from a back/D
 * status, U otherwise). */
void ftCommonDownStandSetStatus(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    s32 status_id;

    if ((fp->status_id == nFTCommonStatusDownBounceD) || (fp->status_id == nFTCommonStatusDownWaitD))
    {
        status_id = nFTCommonStatusDownStandD;
    }
    else status_id = nFTCommonStatusDownStandU;

    ftMainSetStatus(fighter_gobj, status_id, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);

    fp->motion_vars.flags.flag1 = 0;
}

/* ft/ftcommon/ftcommondownstand.c:38-49 ftCommonDownStandCheckInterruptCommon
 * 0x801445D8, verbatim: the player's manual get-up -- stick pushed up past
 * the range/angle thresholds, or a Z tap. */
sb32 ftCommonDownStandCheckInterruptCommon(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if (((fp->input.pl.stick_range.y >= FTCOMMON_DOWNWAIT_STAND_STICK_RANGE_MIN) && (ftParamGetStickAngleRads(fp) >= F_CST_DTOR32(50.0F)) || (fp->input.pl.button_tap & fp->input.button_mask_z)))
    {
        ftCommonDownStandSetStatus(fighter_gobj);

        return TRUE;
    }
    else return FALSE;
}

/* ft/ftcommon/ftcommondownstand.c:10-18 ftCommonDownStandProcInterrupt
 * 0x80144530, verbatim. During the get-up (flag1 set) the fighter can
 * jump (KneeBend), drop through a platform (Pass) or crouch into a pipe
 * (DokanStart, since). */
void ftCommonDownStandProcInterrupt(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if ((fp->motion_vars.flags.flag1 != 0) && (ftCommonKneeBendCheckInterruptCommon(fighter_gobj) == FALSE) && (ftCommonPassCheckInterruptCommon(fighter_gobj) == FALSE))
    {
        ftCommonDokanStartCheckInterruptCommon(fighter_gobj);
    }
}

/* ft/ftcommon/ftcommondownwaitbounce.c:40-58 ftCommonDownWaitSetStatus
 * 0x80144294, verbatim: lie there (DownWaitD/U), arm the auto-stand timer,
 * grant capture immunity, and halve incoming damage. */
void ftCommonDownWaitSetStatus(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    s32 status_id;

    if (fp->status_id == nFTCommonStatusDownBounceD)
    {
        status_id = nFTCommonStatusDownWaitD;
    }
    else status_id = nFTCommonStatusDownWaitU;

    ftMainSetStatus(fighter_gobj, status_id, 0.0F, 1.0F, FTCOMMON_DOWNBOUNCE_STATUS_FLAGS);

    fp->status_vars.common.downwait.stand_wait = FTCOMMON_DOWNWAIT_STAND_WAIT;

    ftParamSetCaptureImmuneMask(fp, (FTCATCHKIND_MASK_CAPTAINSPECIALHI | FTCATCHKIND_MASK_COMMON | FTCATCHKIND_MASK_KIRBYSPECIALN | FTCATCHKIND_MASK_YOSHISPECIALN));

    fp->damage_mul = 0.5F;
}

/* ft/ftcommon/ftcommondownwaitbounce.c:18-28 ftCommonDownWaitProcUpdate
 * 0x80144220, verbatim: count the auto-stand timer down; at zero, stand. */
void ftCommonDownWaitProcUpdate(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    fp->status_vars.common.downwait.stand_wait--;

    if (fp->status_vars.common.downwait.stand_wait == 0)
    {
        ftCommonDownStandSetStatus(fighter_gobj);
    }
}

/* ft/ftcommon/ftcommondownwaitbounce.c:31-37 ftCommonDownWaitProcInterrupt
 * 0x80144254, verbatim: get-up attack, then get-up roll, then the manual
 * stand -- any of the three shortcuts the auto-stand timer (all three
 * routes registered this step). */
void ftCommonDownWaitProcInterrupt(GObj *fighter_gobj)
{
    if ((ftCommonDownAttackCheckInterruptDownWait(fighter_gobj) == FALSE) && (ftCommonDownForwardOrBackCheckInterruptCommon(fighter_gobj) == FALSE))
    {
        ftCommonDownStandCheckInterruptCommon(fighter_gobj);
    }
}

/* ft/ftcommon/ftcommondownwaitbounce.c:107-129 ftCommonDownBounceSetStatus
 * 0x80144498, verbatim: land the fighter, pick back/face-down (DownBounceD/U)
 * from the hip joint, play the bounce effects, and carry the launch velocity
 * into the ground slide. */
void ftCommonDownBounceSetStatus(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    s32 status_id;

    if (fp->ga == nMPKineticsAir)
    {
        mpCommonSetFighterGround(fp);
    }
    if (ftCommonDownBounceCheckUpOrDown(fighter_gobj) != 0)
    {
        status_id = nFTCommonStatusDownBounceD;
    }
    else status_id = nFTCommonStatusDownBounceU;

    ftMainSetStatus(fighter_gobj, status_id, 0.0F, 1.0F, FTSTATUS_PRESERVE_PLAYERTAG);
    ftCommonDownBounceUpdateEffects(fighter_gobj);

    fp->status_vars.common.downbounce.attack_buffer = 0;
    fp->damage_mul = 0.5F;

    ftParamVelDamageTransferGround(fp);
}

/* ft/ftcommon/ftcommondownwaitbounce.c:61-77 ftCommonDownBounceProcUpdate
 * 0x80144308, verbatim: buffer an A/B for the get-up attack, and when the
 * bounce anim settles (anim_frame <= 0) drop into DownWait -- unless a
 * get-up attack or roll takes over first. */
void ftCommonDownBounceProcUpdate(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if (fp->status_vars.common.downbounce.attack_buffer != 0)
    {
        fp->status_vars.common.downbounce.attack_buffer--;
    }
    if (fp->input.pl.button_tap & (fp->input.button_mask_a | fp->input.button_mask_b))
    {
        fp->status_vars.common.downbounce.attack_buffer = FTCOMMON_DOWNBOUNCE_ATTACK_BUFFER;
    }
    if ((fighter_gobj->anim_frame <= 0.0F) && (ftCommonDownAttackCheckInterruptDownBounce(fighter_gobj) == FALSE) && (ftCommonDownForwardOrBackCheckInterruptCommon(fighter_gobj) == FALSE))
    {
        ftCommonDownWaitSetStatus(fighter_gobj);
    }
}

/* ft/ftcommon/ftcommonshieldbreakfly.c:11-14 ftCommonShieldBreakFlyProcUpdate
 * 0x80149440, verbatim */
void ftCommonShieldBreakFlyProcUpdate(GObj *fighter_gobj)
{
    ftAnimEndCheckSetStatus(fighter_gobj, ftCommonShieldBreakFallSetStatus);
}

/* ft/ftcommon/ftcommonshieldbreakfly.c:17-20 ftCommonShieldBreakFlyProcMap
 * 0x80149464, verbatim: landing mid-fly goes straight to the ground down. */
void ftCommonShieldBreakFlyProcMap(GObj *fighter_gobj)
{
    mpCommonProcFighterLanding(fighter_gobj, ftCommonShieldBreakDownSetStatus);
}

/* ft/ftcommon/ftcommonshieldbreakfly.c:23-40 ftCommonShieldBreakFlySetStatus
 * 0x80149488, verbatim */
void ftCommonShieldBreakFlySetStatus(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    FTAttributes *attr = fp->attr;

    mpCommonSetFighterAir(fp);
    ftMainSetStatus(fighter_gobj, nFTCommonStatusShieldBreakFly, 0.0F, 1.0F, FTSTATUS_PRESERVE_DAMAGEPLAYER);
    ftMainPlayAnimEventsAll(fighter_gobj);

    fp->physics.vel_air.x = 0.0F;
    fp->physics.vel_air.y = attr->shield_break_vel_y;

    ftParamCheckSetFighterColAnimID(fighter_gobj, nGMColAnimFighterShieldBreakFly, 0);
    func_800269C0_275C0(nSYAudioFGMShieldBreak);
}

/* ft/ftcommon/ftcommonshieldbreakfly.c:43-64 ftCommonShieldBreakFlyCommonSetStatus
 * 0x80149510, the burst at YRotN (or Yoshi's egg shattering) restored by
 * -- all three makers are src/dc/efmanager.c's. */
void ftCommonShieldBreakFlyCommonSetStatus(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    Vec3f offset;

    offset.x = 0.0F;
    offset.y = 0.0F;
    offset.z = 0.0F;

#ifdef FT_HOSTTEST
    /* hosttest_ft.c's mock is a three-joint tree with no YRotN */
    if (fp->joints[nFTPartsJointYRotN] == NULL)
    {
        gmCollisionGetFighterPartsWorldPosition(fp->joints[nFTPartsJointTopN], &offset);
    }
    else
#endif
    gmCollisionGetFighterPartsWorldPosition(fp->joints[nFTPartsJointYRotN], &offset);

    if (fp->fkind == nFTKindYoshi)
    {
        efManagerYoshiEggExplodeMakeEffect(&DObjGetStruct(fighter_gobj)->translate.vec.f);
        efManagerEggBreakMakeEffect(&DObjGetStruct(fighter_gobj)->translate.vec.f);
    }
    else efManagerShieldBreakMakeEffect(&offset);

    ftParamUpdate1PGameDamageStats(fp, fp->shield_player, nFTHitLogObjectNone, 0, 0, 0);

    if ((gSCManagerBattleState->game_type == nSCBattleGameType1PGame) && (fp->shield_damage != 0))
    {
        if ((fp->shield_player == gSCManagerSceneData.player) && (fp->shield_player != fp->player))
        {
            gSC1PGameBonusShieldBreaker = TRUE;
        }
    }
    ftCommonShieldBreakFlySetStatus(fighter_gobj);
}

/* ft/ftcommon/ftcommonshieldbreakfall.c:20-23 ftCommonShieldBreakFallProcMap
 * 0x80149720, verbatim */
void ftCommonShieldBreakFallProcMap(GObj *fighter_gobj)
{
    mpCommonProcFighterLanding(fighter_gobj, ftCommonShieldBreakDownSetStatus);
}

/* ft/ftcommon/ftcommonshieldbreakfall.c:26-33 ftCommonShieldBreakFallSetStatus
 * 0x80149744, verbatim */
void ftCommonShieldBreakFallSetStatus(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    ftMainSetStatus(fighter_gobj, nFTCommonStatusShieldBreakFall, 0.0F, 1.0F, FTCOMMON_SHIELDBREAKFALL_STATUS_FLAGS);
    ftPhysicsClampAirVelXMax(fp);
    ftParamMakeRumble(fp, 3, 0);
}

/* ft/ftcommon/ftcommonshieldbreakdown.c:16-19 ftCommonShieldBreakDownProcUpdate
 * 0x80149680, verbatim */
void ftCommonShieldBreakDownProcUpdate(GObj *fighter_gobj)
{
    ftAnimEndCheckSetStatus(fighter_gobj, ftCommonShieldBreakStandSetStatus);
}

/* ft/ftcommon/ftcommonshieldbreakdown.c:22-35 ftCommonShieldBreakDownSetStatus
 * 0x801496A4, verbatim */
void ftCommonShieldBreakDownSetStatus(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    s32 status_id;

    if (fp->ga == nMPKineticsAir)
    {
        mpCommonSetFighterGround(fp);
    }
    status_id = (ftCommonDownBounceCheckUpOrDown(fighter_gobj) != 0) ? nFTCommonStatusShieldBreakDownD : nFTCommonStatusShieldBreakDownU;

    ftMainSetStatus(fighter_gobj, status_id, 0.0F, 1.0F, FTCOMMON_SHIELDBREAKDOWN_STATUS_FLAGS);
    ftCommonDownBounceUpdateEffects(fighter_gobj);
}

/* ft/ftcommon/ftcommonshieldbreakstand.c:16-19 ftCommonShieldBreakStandProcUpdate
 * 0x801497A0, verbatim */
void ftCommonShieldBreakStandProcUpdate(GObj *fighter_gobj)
{
    ftAnimEndCheckSetStatus(fighter_gobj, ftCommonFuraFuraSetStatus);
}

/* ft/ftcommon/ftcommonshieldbreakstand.c:22-27 ftCommonShieldBreakStandSetStatus
 * 0x801497C4, verbatim */
void ftCommonShieldBreakStandSetStatus(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    s32 status_id = (fp->status_id == nFTCommonStatusShieldBreakDownD) ? nFTCommonStatusShieldBreakStandD : nFTCommonStatusShieldBreakStandU;

    ftMainSetStatus(fighter_gobj, status_id, 0.0F, 1.0F, FTCOMMON_SHIELDBREAKSTAND_STATUS_FLAGS);
}

/* ft/ftcommon/ftcommonfurafura.c:11-30 ftCommonFuraFuraProcUpdate 0x80149810,
 * verbatim (the shield_health ternary is the game's, both arms 30): the dizzy
 * counts breakout_wait down, mashing amplified, and ends in Wait at zero. */
void ftCommonFuraFuraProcUpdate(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    s32 breakout_wait;

    fp->shield_health = (fp->fkind == nFTKindYoshi) ? 30 : 30;

    fp->breakout_wait--;

    breakout_wait = fp->breakout_wait;

    ftCommonCaptureTrappedUpdateBreakoutVars(fp);

    fp->breakout_wait += (fp->breakout_wait - breakout_wait) * 3;

    if (fp->breakout_wait <= 0)
    {
        ftCommonWaitSetStatus(fighter_gobj);
    }
}

/* ft/ftcommon/ftcommonfurafura.c:33-51 ftCommonFuraFuraSetStatus 0x801498A4,
 * verbatim: the breakout window is longer the less damaged the fighter is. */
void ftCommonFuraFuraSetStatus(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    s32 breakout_wait;

    ftMainSetStatus(fighter_gobj, nFTCommonStatusFuraFura, 0.0F, 1.0F, (FTSTATUS_PRESERVE_TEXTUREPART | FTSTATUS_PRESERVE_MODELPART));

    fp->shield_health = (fp->fkind == nFTKindYoshi) ? 30 : 30;

    breakout_wait = FTCOMMON_FURAFURA_BREAKOUT_WAIT_DEFAULT - fp->percent_damage;

    if (breakout_wait <= 0)
    {
        breakout_wait = 0;
    }
    breakout_wait += FTCOMMON_FURAFURA_BREAKOUT_WAIT_MIN;

    ftCommonCaptureTrappedInitBreakoutVars(fp, breakout_wait);
    ftParamCheckSetFighterColAnimID(fighter_gobj, nGMColAnimFighterFuraFura, 0);
}

/* ---- ft/ftcommon/ftcommonfurasleep.c: the sleep --------
 *
 * FuraSleep is the dizzy's twin and shares its whole shape: the same
 * breakout counter, the same mash amplification, the same end into Wait.
 * The two differ only in where they come from and how long they last --
 * FuraFura is the shield break's tail (ftcommonshieldbreakstand.c hands
 * off to it), FuraSleep is a hit whose element is nGMHitElementSleep
 * (ftCommonDamageGotoDamageStatus, ported above), and the sleep's floor
 * is 75 tics against the dizzy's 60. The element is motion-script data
 * (ft/ftmain.c:226 copies it out of the FTMotionEventMakeAttack1 event
 * into the hitbox, ftmain.c:2861 out of the hitbox into damage_element),
 * so which of the roster's moves reach this is a question about the
 * ROM's fighter scripts and not about any code here.
 *
 * The setter was stubbed to nothing from's damage work until this
 * step; nothing else about the family was ported, which left status 165
 * an empty row. Both functions are verbatim. */

/* ft/ftcommon/ftcommonfurasleep.c:11-27 ftCommonFuraSleepProcUpdate
 * 0x80149940, verbatim. Identical to FuraFura's update but for the
 * shield_health line, which the sleep has no reason to write. */
void ftCommonFuraSleepProcUpdate(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    s32 breakout_wait;

    fp->breakout_wait--;

    breakout_wait = fp->breakout_wait;

    ftCommonCaptureTrappedUpdateBreakoutVars(fp);

    fp->breakout_wait += (fp->breakout_wait - breakout_wait) * 3;

    if (fp->breakout_wait <= 0)
    {
        ftCommonWaitSetStatus(fighter_gobj);
    }
}

/* ft/ftcommon/ftcommonfurasleep.c:30-47 ftCommonFuraSleepSetStatus
 * 0x801499A4, verbatim. FTCOMMON_FURASLEEP_BREAKOUT_WAIT_MIN is the
 * REGION_US 75 (ft/ftcommon.h:267), as the rest of the port is. */
void ftCommonFuraSleepSetStatus(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    s32 breakout_wait;

    ftMainSetStatus(fighter_gobj, nFTCommonStatusFuraSleep, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);

    breakout_wait = FTCOMMON_FURASLEEP_BREAKOUT_WAIT_DEFAULT - fp->percent_damage;

    if (breakout_wait <= 0)
    {
        breakout_wait = 0;
    }
    breakout_wait += FTCOMMON_FURASLEEP_BREAKOUT_WAIT_MIN;

    ftCommonCaptureTrappedInitBreakoutVars(fp, breakout_wait);
    ftParamCheckSetFighterColAnimID(fighter_gobj, nGMColAnimFighterFuraSleep, 0);
}

/* ---- ft/ftcommon/ftcommonguard1.c and ftcommonguard2.c: the shield
 * ----------------------------------------------------------
 *
 * Six statuses: GuardOn (the ShieldOn animation, XRotN and YRotN linked
 * by its flags), Guard (motion -1: the pose held, animated from XRotN by
 * the shield-pose tables), GuardOff (ShieldOff), GuardSetOff (shield
 * stun after a blocked hit, ftmain.c ftMainUpdateShieldStatFighter),
 * and GuardKneeBend and GuardPass, KneeBend and Pass entered with Z held
 * (above). The pose comes out of FTAttributes.dobj_lookup and
 * shield_anim_joints -- the ShieldPose file, in the pack since this step
 * (fighter.h FPackShieldPose) -- and the shield the hit pass tests is
 * YRotN's scale (ftCommonGuardUpdateShieldCollision); the bubble the
 * player sees is the effect efManagerShieldMakeEffect attaches to that
 * joint (src/dc/efmanager.c,). Verbatim but for what is
 * marked DIVERGES: the guard's interrupt cascade has the file's holes. */

/* ftcommonguard1.c:9-17 ftCommonGuardCheckInterrupt, the macro, as a
 * function with the ported subset in the game's order. The roll is in
 * since in the game's place -- second, so a stick tilt beats
 * a shield drop or a shield jump on the same frame. The shield-grab is in
 * since in its place -- third, after the roll. Whole as of
 * the item-use step: the item throw is first, ahead of all four, so
 * shielding with something in your hands and pressing A throws it
 * rather than dropping the shield. No DIVERGES left in it. */
/* ft/ftcommon/ftcommonfunctions.h:368 spells this one
 * `ftCommonLightThrowCheckInterruptGuardOnOn', with the On doubled, and
 * ftcommonitemthrow.c:253 defines it with a single one -- so the decomp's
 * own ftcommonguard1.c compiles its caller on an implicit declaration and
 * the linker resolves it anyway. The port does not let an implicit
 * declaration stand (see the hosttest trap), so the real spelling is
 * declared here rather than the header's typo repaired: the header is
 * pinned decomp text. */
extern sb32 ftCommonLightThrowCheckInterruptGuardOn(GObj *fighter_gobj);

static sb32 ftCommonGuardCheckInterrupt(GObj *fighter_gobj)
{
    return (ftCommonLightThrowCheckInterruptGuardOn(fighter_gobj) != FALSE) ||
           (ftCommonEscapeCheckInterruptGuard(fighter_gobj) != FALSE) ||
           (ftCommonCatchCheckInterruptGuard(fighter_gobj) != FALSE) ||
           (ftCommonGuardKneeBendCheckInterruptGuard(fighter_gobj) != FALSE) ||
           (ftCommonGuardPassCheckInterruptGuard(fighter_gobj) != FALSE);
}

/* ftcommonguard1.c:26-32 ftCommonGuardCheckScheduleRelease 0x80148120 */
void ftCommonGuardCheckScheduleRelease(FTStruct *fp)
{
    if (!(fp->input.pl.button_hold & fp->input.button_mask_z))
    {
        fp->status_vars.common.guard.is_release = TRUE;
    }
}

/* ftcommonguard1.c:35-48 ftCommonGuardOnSetHitStatusYoshi 0x80148144 */
void ftCommonGuardOnSetHitStatusYoshi(GObj *fighter_gobj)
{
    ftParamSetHitStatusPartID(fighter_gobj, 5,  nGMHitStatusInvincible);
    ftParamSetHitStatusPartID(fighter_gobj, 6,  nGMHitStatusInvincible);
    ftParamSetHitStatusPartID(fighter_gobj, 7,  nGMHitStatusInvincible);
    ftParamSetHitStatusPartID(fighter_gobj, 15, nGMHitStatusInvincible);
    ftParamSetHitStatusPartID(fighter_gobj, 11, nGMHitStatusInvincible);
    ftParamSetHitStatusPartID(fighter_gobj, 16, nGMHitStatusInvincible);
    ftParamSetHitStatusPartID(fighter_gobj, 12, nGMHitStatusInvincible);
    ftParamSetHitStatusPartID(fighter_gobj, 27, nGMHitStatusInvincible);
    ftParamSetHitStatusPartID(fighter_gobj, 22, nGMHitStatusInvincible);
    ftParamSetHitStatusPartID(fighter_gobj, 28, nGMHitStatusInvincible);
    ftParamSetHitStatusPartID(fighter_gobj, 23, nGMHitStatusInvincible);
}

/* ftcommonguard1.c:51-64 ftCommonGuardSetHitStatusYoshi 0x80148214 */
void ftCommonGuardSetHitStatusYoshi(GObj *fighter_gobj)
{
    ftParamSetHitStatusPartID(fighter_gobj, 5,  nGMHitStatusNormal);
    ftParamSetHitStatusPartID(fighter_gobj, 6,  nGMHitStatusIntangible);
    ftParamSetHitStatusPartID(fighter_gobj, 7,  nGMHitStatusIntangible);
    ftParamSetHitStatusPartID(fighter_gobj, 15, nGMHitStatusIntangible);
    ftParamSetHitStatusPartID(fighter_gobj, 11, nGMHitStatusIntangible);
    ftParamSetHitStatusPartID(fighter_gobj, 16, nGMHitStatusIntangible);
    ftParamSetHitStatusPartID(fighter_gobj, 12, nGMHitStatusIntangible);
    ftParamSetHitStatusPartID(fighter_gobj, 27, nGMHitStatusIntangible);
    ftParamSetHitStatusPartID(fighter_gobj, 22, nGMHitStatusIntangible);
    ftParamSetHitStatusPartID(fighter_gobj, 28, nGMHitStatusIntangible);
    ftParamSetHitStatusPartID(fighter_gobj, 23, nGMHitStatusIntangible);
}

/* ftcommonguard1.c:67-70 ftCommonGuardOffSetHitStatusYoshi 0x801482E4 */
void ftCommonGuardOffSetHitStatusYoshi(GObj *fighter_gobj)
{
    ftParamSetHitStatusPartAll(fighter_gobj, nGMHitStatusNormal);
}

/* ftcommonguard1.c:73-128 ftCommonGuardUpdateShieldVars 0x80148304 */
void ftCommonGuardUpdateShieldVars(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if (fp->is_shield)
    {
        if (fp->status_vars.common.guard.shield_decay_wait != 0)
        {
            fp->status_vars.common.guard.shield_decay_wait--;

            if (fp->status_vars.common.guard.shield_decay_wait == 0)
            {
                fp->shield_health--;

                if (fp->shield_health != 0)
                {
                    fp->status_vars.common.guard.shield_decay_wait = FTCOMMON_GUARD_DECAY_INT;

                    goto lag_decrement;
                }
                else goto lag_end;
            }
        }
    lag_decrement:
        if (fp->status_vars.common.guard.release_lag != 0)
        {
            fp->status_vars.common.guard.release_lag--;
        }
        if ((fp->status_vars.common.guard.release_lag == 0) && (fp->status_vars.common.guard.is_release != FALSE))
        {
        lag_end:
            if (fp->fkind == nFTKindYoshi)
            {
                ftParamResetModelPartAll(fighter_gobj);
                ftCommonGuardOffSetHitStatusYoshi(fighter_gobj);

                if (fp->is_effect_attach)
                {
                    // 0x801886C0
                    Vec3f egg_effect_offset = { 0.0F, 0.0F, 0.0F };

                    gmCollisionGetFighterPartsWorldPosition(fp->joints[nFTPartsJointYRotN], &egg_effect_offset);
                    efManagerEggBreakMakeEffect(&egg_effect_offset);
                }
            }
            ftParamProcStopEffect(fighter_gobj);

            fp->is_shield = FALSE;
        }
    }
}

/* ftcommonguard1.c:131-152 ftCommonGuardUpdateShieldCollision 0x80148408:
 * the shield the hit pass tests (ftmain.c, through
 * gmCollisionCheckFighterAttackShieldCollide) is YRotN's scale */
void ftCommonGuardUpdateShieldCollision(FTStruct *fp)
{
    Vec3f *scale = &fp->joints[nFTPartsJointYRotN]->scale.vec.f;
    f32 scale_final;
    f32 scale_mul;

    if (fp->fkind == nFTKindYoshi)
    {
        scale_mul = 1.0F;
    }
    else scale_mul = fp->shield_health / FTCOMMON_GUARD_SIZE_HEALTH_DIV;

    scale_final =
    (
        (
            (FTCOMMON_GUARD_SIZE_SCALE_MUL_INIT * scale_mul) +
            FTCOMMON_GUARD_SIZE_SCALE_MUL_ADD
        ) * fp->attr->shield_size
    ) / FTCOMMON_GUARD_SIZE_SCALE_MUL_DIV;

    scale->x = scale->y = scale->z = scale_final;
}

/* ftcommonguard1.c:155-185 ftCommonGuardUpdateShieldAngle 0x80148488: the
 * stick's angle, facing-relative, as a 45-degree sector (angle_i, which
 * picks the shield-pose table) and the frame within it (angle_f), and
 * its length as the lean (shield_rotate_range) */
void ftCommonGuardUpdateShieldAngle(FTStruct *fp)
{
    f32 angle_r = syUtilsArcTan2(fp->input.pl.stick_range.y, fp->input.pl.stick_range.x * fp->lr);
    f32 angle_d;
    f32 range;

    if (angle_r < 0.0F)
    {
        angle_r += F_CST_DTOR32(360.0F);
    }
    angle_d = F_CLC_RTOD32(angle_r);

    if (angle_d < 0.0F)
    {
        angle_d = 0.0F;
    }
    if (angle_d > FTCOMMON_GUARD_ANGLE_MAX)
    {
        angle_d = FTCOMMON_GUARD_ANGLE_MAX;
    }
    fp->status_vars.common.guard.angle_i = (angle_d / 45.0F);
    fp->status_vars.common.guard.angle_f = (angle_d - (fp->status_vars.common.guard.angle_i * 45.0F));

    range = sqrtf(SQUARE(fp->input.pl.stick_range.x) + SQUARE(fp->input.pl.stick_range.y)) / F_CONTROLLER_RANGE_MAX;

    if (range > 1.0F)
    {
        range = 1.0F;
    }
    fp->status_vars.common.guard.shield_rotate_range = range;
}

/* ftcommonguard1.c:188-197 ftCommonGuardGetJointTransform 0x8014857C: the
 * joint's animated pose pulled back toward its DObjDesc's by 1 - range */
void ftCommonGuardGetJointTransform(DObj *joint, DObjDesc *dobjdesc, f32 range)
{
    joint->rotate.vec.f.x = ((joint->rotate.vec.f.x - dobjdesc->rotate.x) * range) + dobjdesc->rotate.x;
    joint->rotate.vec.f.y = ((joint->rotate.vec.f.y - dobjdesc->rotate.y) * range) + dobjdesc->rotate.y;
    joint->rotate.vec.f.z = ((joint->rotate.vec.f.z - dobjdesc->rotate.z) * range) + dobjdesc->rotate.z;

    joint->translate.vec.f.x = ((joint->translate.vec.f.x - dobjdesc->translate.x) * range) + dobjdesc->translate.x;
    joint->translate.vec.f.y = ((joint->translate.vec.f.y - dobjdesc->translate.y) * range) + dobjdesc->translate.y;
    joint->translate.vec.f.z = ((joint->translate.vec.f.z - dobjdesc->translate.z) * range) + dobjdesc->translate.z;
}

/* ftcommonguard1.c:200-219 ftCommonGuardGetJointTransformScale 0x80148600 */
void ftCommonGuardGetJointTransformScale(DObj *joint, DObjDesc *dobjdesc, f32 range, Vec3f *scale)
{
    f32 scale_translate;

    joint->rotate.vec.f.x = ((joint->rotate.vec.f.x - dobjdesc->rotate.x) * range) + dobjdesc->rotate.x;
    joint->rotate.vec.f.y = ((joint->rotate.vec.f.y - dobjdesc->rotate.y) * range) + dobjdesc->rotate.y;
    joint->rotate.vec.f.z = ((joint->rotate.vec.f.z - dobjdesc->rotate.z) * range) + dobjdesc->rotate.z;

    // y tho

    scale_translate = dobjdesc->translate.x * scale->x;
    joint->translate.vec.f.x = ((joint->translate.vec.f.x - scale_translate) * range) + scale_translate;

    scale_translate = dobjdesc->translate.y * scale->y;
    joint->translate.vec.f.y = ((joint->translate.vec.f.y - scale_translate) * range) + scale_translate;

    scale_translate = dobjdesc->translate.z * scale->z;
    joint->translate.vec.f.z = ((joint->translate.vec.f.z - scale_translate) * range) + scale_translate;
}

/* ftcommonguard1.c:222-265 ftCommonGuardUpdateJoints 0x801486D0: while the
 * ShieldOn and ShieldOff figatrees play, YRotN alone takes the pose
 * table's last script -- the walk from XRotN ends on it, so its index is
 * one less than the count of joints from XRotN on */
void ftCommonGuardUpdateJoints(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    DObj *yrotn_joint = fp->joints[nFTPartsJointYRotN];
    DObj **p_joint = &fp->joints[nFTPartsJointXRotN];
    s32 joint_num;
    s32 i;
    Vec3f *scale = &fp->attr->translate_scales[nFTPartsJointYRotN];

    if (fp->is_shield)
    {
        ftCommonGuardUpdateShieldAngle(fp);

        for (i = nFTPartsJointXRotN, joint_num = 0; i < ARRAY_COUNT(fp->joints); i++, p_joint++)
        {
            if (*p_joint != NULL)
            {
                joint_num++;
            }
        }
        joint_num--;

        gcAddDObjAnimJoint(yrotn_joint, fp->attr->shield_anim_joints[fp->status_vars.common.guard.angle_i][joint_num], fp->status_vars.common.guard.angle_f);
        gcParseDObjAnimJoint(yrotn_joint);

        if (fp->is_have_translate_scale)
        {
            lbCommonPlayTranslateScaledDObjAnim(yrotn_joint, scale);
        }
        else gcPlayDObjAnimJoint(yrotn_joint);

        if (fp->is_have_translate_scale)
        {
            ftCommonGuardGetJointTransformScale(yrotn_joint, &fp->attr->dobj_lookup[joint_num], fp->status_vars.common.guard.shield_rotate_range, scale);
        }
        else ftCommonGuardGetJointTransform(yrotn_joint, &fp->attr->dobj_lookup[joint_num], fp->status_vars.common.guard.shield_rotate_range);

        yrotn_joint->anim_wait = AOBJ_ANIM_NULL;

        ftCommonGuardUpdateShieldCollision(fp);
        ftParamsUpdateFighterPartsTransformAll(fp->joints[nFTPartsJointYRotN]);
    }
}

/* ftcommonguard1.c:268-371 ftCommonGuardInitJoints 0x80148804: the held
 * shield's pose, every frame -- the sector's table dealt down the walk
 * from XRotN, played, and each joint pulled toward its DObjDesc row by
 * the lean; dobj_lookup[0] is XRotN's row, [1..] the entries', the last
 * YRotN's (fighter.h FPackShieldPose) */
void ftCommonGuardInitJoints(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    FTAttributes *attr = fp->attr;
    DObj **p_joint = &fp->joints[nFTPartsJointCommonStart];
    DObjDesc *dobjdesc = &attr->dobj_lookup[1];
    DObj *joint;
    Vec3f *scale;
    s32 i;

    if (fp->status_id != nFTCommonStatusGuardSetOff)
    {
        ftCommonGuardUpdateShieldAngle(fp);
    }
    fp->anim_desc.flags.is_anim_joint = TRUE;

    lbCommonAddDObjAnimJointAll
    (
        fp->joints[nFTPartsJointXRotN],
        attr->shield_anim_joints[fp->status_vars.common.guard.angle_i],
        fp->status_vars.common.guard.angle_f
    );
    ftMainPlayAnimEventsAll(fighter_gobj);

    if (fp->is_have_translate_scale)
    {
        scale = &fp->attr->translate_scales[nFTPartsJointCommonStart];

        for (i = nFTPartsJointCommonStart; i < ARRAY_COUNT(fp->joints); i++, p_joint++, scale++)
        {
            joint = *p_joint;

            if (joint != NULL)
            {
                if (joint->anim_wait != AOBJ_ANIM_NULL)
                {
                    ftCommonGuardGetJointTransformScale
                    (
                        joint,
                        dobjdesc,
                        fp->status_vars.common.guard.shield_rotate_range,
                        scale
                    );
                    joint->anim_wait = AOBJ_ANIM_NULL;
                }
                dobjdesc++;
            }
        }
        joint = fp->joints[nFTPartsJointYRotN];

        if (joint->anim_wait != AOBJ_ANIM_NULL)
        {
            ftCommonGuardGetJointTransformScale
            (
                joint,
                dobjdesc,
                fp->status_vars.common.guard.shield_rotate_range,
                &fp->attr->translate_scales[nFTPartsJointYRotN]
            );
            joint->anim_wait = AOBJ_ANIM_NULL;
        }
    }
    else
    {
        for (i = nFTPartsJointCommonStart; i < ARRAY_COUNT(fp->joints); i++, p_joint++)
        {
            joint = *p_joint;

            if (joint != NULL)
            {
                if (joint->anim_wait != AOBJ_ANIM_NULL)
                {
                    ftCommonGuardGetJointTransform
                    (
                        joint,
                        dobjdesc,
                        fp->status_vars.common.guard.shield_rotate_range
                    );
                    joint->anim_wait = AOBJ_ANIM_NULL;
                }
                dobjdesc++;
            }
        }
        joint = fp->joints[nFTPartsJointYRotN];

        if (joint->anim_wait != AOBJ_ANIM_NULL)
        {
            ftCommonGuardGetJointTransform
            (
                joint,
                dobjdesc,
                fp->status_vars.common.guard.shield_rotate_range
            );
            joint->anim_wait = AOBJ_ANIM_NULL;
        }
    }
    ftCommonGuardUpdateShieldCollision(fp);
    ftParamsUpdateFighterPartsTransform(fp->joints[nFTPartsJointXRotN]);
}

/* ftcommonguard1.c:374-414 ftCommonGuardOnProcUpdate 0x80148A88 */
void ftCommonGuardOnProcUpdate(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    ftCommonGuardCheckScheduleRelease(fp);
    ftCommonGuardUpdateShieldVars(fighter_gobj);

    if (fp->shield_health == 0)
    {
        ftCommonShieldBreakFlyCommonSetStatus(fighter_gobj);
    }
    else
    {
        if (fighter_gobj->anim_frame <= 0.0F)
        {
            if (fp->status_vars.common.guard.is_release != FALSE)
            {
                if (fp->fkind == nFTKindYoshi)
                {
                    ftCommonGuardOffSetHitStatusYoshi(fighter_gobj);
                }
                ftCommonGuardOffSetStatus(fighter_gobj);
            }
            else
            {
                if (fp->fkind == nFTKindYoshi)
                {
                    fp->status_vars.common.guard.effect_gobj = efManagerYoshiShieldMakeEffect(fighter_gobj);

                    ftParamHideModelPartAll(fighter_gobj);
                    ftCommonGuardSetHitStatusYoshi(fighter_gobj);

                    fp->is_shield = TRUE;

                    ftCommonGuardUpdateJoints(fighter_gobj);
                }
                ftCommonGuardSetStatus(fighter_gobj);
            }
        }
        else ftCommonGuardUpdateJoints(fighter_gobj);
    }
}

/* ftcommonguard1.c:417-423 ftCommonGuardCommonProcInterrupt 0x80148B84,
 * verbatim since put back the pipe fallthrough. */
void ftCommonGuardCommonProcInterrupt(GObj *fighter_gobj)
{
    if (!(ftCommonGuardCheckInterrupt(fighter_gobj)))
    {
        ftCommonDokanStartCheckInterruptCommon(fighter_gobj);
    }
}

/* ftcommonguard1.c:426-455 ftCommonGuardOnSetStatus 0x80148BFC */
void ftCommonGuardOnSetStatus(GObj *fighter_gobj, s32 slide_tics)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    ftMainSetStatus(fighter_gobj, nFTCommonStatusGuardOn, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);
    ftMainPlayAnimEventsAll(fighter_gobj);

    if (fp->shield_health != 0)
    {
        if (fp->fkind == nFTKindYoshi)
        {
            ftCommonGuardOnSetHitStatusYoshi(fighter_gobj);
        }
        else
        {
            fp->status_vars.common.guard.effect_gobj = efManagerShieldMakeEffect(fighter_gobj);
            fp->is_shield = TRUE;
        }
    }
    ftCommonGuardUpdateJoints(fighter_gobj);

    fp->status_vars.common.guard.release_lag = FTCOMMON_GUARD_RELEASE_LAG;
    fp->status_vars.common.guard.shield_decay_wait = FTCOMMON_GUARD_DECAY_INT;
    fp->status_vars.common.guard.is_release = FALSE;
    fp->status_vars.common.guard.slide_tics = slide_tics;
    fp->status_vars.common.guard.is_setoff = FALSE;

    func_800269C0_275C0(nSYAudioFGMGuardOn);
}

/* ftcommonguard1.c:458-470 ftCommonGuardOnCheckInterruptSuccess 0x80148CBC */
sb32 ftCommonGuardOnCheckInterruptSuccess(GObj *fighter_gobj, s32 slide_tics)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if ((fp->input.pl.button_hold & fp->input.button_mask_z) && (fp->shield_health != 0))
    {
        ftCommonGuardOnSetStatus(fighter_gobj, slide_tics);

        return TRUE;
    }
    else return FALSE;
}

/* ftcommonguard1.c:473-476 ftCommonGuardOnCheckInterruptCommon 0x80148D0C */
sb32 ftCommonGuardOnCheckInterruptCommon(GObj *fighter_gobj)
{
    return ftCommonGuardOnCheckInterruptSuccess(fighter_gobj, 0);
}

/* ftcommonguard1.c:479-482 ftCommonGuardOnCheckInterruptDashRun 0x80148D2C */
sb32 ftCommonGuardOnCheckInterruptDashRun(GObj *fighter_gobj, s32 slide_tics)
{
    return ftCommonGuardOnCheckInterruptSuccess(fighter_gobj, slide_tics);
}

/* ftcommonguard1.c:485-501 ftCommonGuardProcUpdate 0x80148D4C */
void ftCommonGuardProcUpdate(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    ftCommonGuardCheckScheduleRelease(fp);
    ftCommonGuardUpdateShieldVars(fighter_gobj);

    if (fp->shield_health == 0)
    {
        ftCommonShieldBreakFlyCommonSetStatus(fighter_gobj);
    }
    else if ((fp->status_vars.common.guard.is_release != FALSE) || !(fp->is_shield))
    {
        ftCommonGuardOffSetStatus(fighter_gobj);
    }
    else ftCommonGuardInitJoints(fighter_gobj);
}

/* ftcommonguard1.c:504-512 ftCommonGuardSetStatus 0x80148DDC */
void ftCommonGuardSetStatus(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    ftMainSetStatus(fighter_gobj, nFTCommonStatusGuard, 0.0F, 1.0F, (FTSTATUS_PRESERVE_MODELPART | FTSTATUS_PRESERVE_HITSTATUS | FTSTATUS_PRESERVE_EFFECT));
    ftCommonGuardInitJoints(fighter_gobj);

    fp->is_shield = TRUE;
}

/* ftcommonguard2.c:10-46 ftCommonGuardSetStatusFromEscape 0x80148E30 */
void ftCommonGuardSetStatusFromEscape(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    ftMainSetStatus(fighter_gobj, nFTCommonStatusGuardOn, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE); // Why? It overwrites this with Guard later down.
    ftMainPlayAnimEventsAll(fighter_gobj);

    if (fp->shield_health != 0)
    {
        if (fp->fkind == nFTKindYoshi)
        {
            fp->status_vars.common.guard.effect_gobj = efManagerYoshiShieldMakeEffect(fighter_gobj);

            ftParamHideModelPartAll(fighter_gobj);
            ftCommonGuardSetHitStatusYoshi(fighter_gobj);
        }
        else fp->status_vars.common.guard.effect_gobj = efManagerShieldMakeEffect(fighter_gobj);

        fp->is_shield = TRUE;
    }
    ftCommonGuardUpdateJoints(fighter_gobj);

    fp->status_vars.common.guard.release_lag = FTCOMMON_GUARD_RELEASE_LAG;
    fp->status_vars.common.guard.shield_decay_wait = FTCOMMON_GUARD_DECAY_INT;
    fp->status_vars.common.guard.is_release = FALSE;
    fp->status_vars.common.guard.slide_tics = 0;
    fp->status_vars.common.guard.is_setoff = FALSE;

    ftMainSetStatus(fighter_gobj, nFTCommonStatusGuard, 0.0F, 1.0F, (FTSTATUS_PRESERVE_MODELPART | FTSTATUS_PRESERVE_HITSTATUS | FTSTATUS_PRESERVE_EFFECT));

    ftCommonGuardInitJoints(fighter_gobj);

    fp->is_shield = TRUE;
}

/* ftcommonguard2.c:49-60 ftCommonGuardCheckInterruptEscape 0x80148F24:
 * the roll's way back into the shield, for when the roll is ported */
sb32 ftCommonGuardCheckInterruptEscape(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if ((fp->input.pl.button_hold & fp->input.button_mask_z) && (fp->shield_health != 0))
    {
        ftCommonGuardSetStatusFromEscape(fighter_gobj);

        return TRUE;
    }
    else return FALSE;
}

/* ftcommonguard2.c:63-77 ftCommonGuardOffProcUpdate 0x80148F74 */
void ftCommonGuardOffProcUpdate(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    ftCommonGuardUpdateShieldVars(fighter_gobj);

    if (fp->shield_health == 0)
    {
        ftCommonShieldBreakFlyCommonSetStatus(fighter_gobj);
    }
    else if (fighter_gobj->anim_frame <= 0.0F)
    {
        ftCommonWaitSetStatus(fighter_gobj);
    }
    else ftCommonGuardUpdateJoints(fighter_gobj);
}

/* ftcommonguard2.c:80-92 ftCommonGuardOffSetStatus 0x80148FF0 */
void ftCommonGuardOffSetStatus(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    ub32 flag = fp->is_shield;

    ftMainSetStatus(fighter_gobj, nFTCommonStatusGuardOff, 0.0F, 1.0F, (FTSTATUS_PRESERVE_MODELPART | FTSTATUS_PRESERVE_HITSTATUS | FTSTATUS_PRESERVE_EFFECT));
    ftMainPlayAnimEventsAll(fighter_gobj);

    fp->is_shield = flag;

    ftCommonGuardUpdateJoints(fighter_gobj);
    func_800269C0_275C0(nSYAudioFGMGuardOff);
}

/* ftcommonguard2.c:95-112 ftCommonGuardSetOffProcUpdate 0x80149074 */
void ftCommonGuardSetOffProcUpdate(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    ftCommonGuardCheckScheduleRelease(fp);

    fp->status_vars.common.guard.setoff_frames--;

    if (fp->status_vars.common.guard.setoff_frames <= 0.0F)
    {
        if (fp->status_vars.common.guard.is_release != FALSE)
        {
            ftCommonGuardOffSetStatus(fighter_gobj);
        }
        else ftCommonGuardSetStatus(fighter_gobj);
    }
    else ftCommonGuardInitJoints(fighter_gobj);
}

/* ftcommonguard2.c:115-132 ftCommonGuardSetOffSetStatus 0x80149108: shield
 * stun, entered by ftmain.c ftMainUpdateShieldStatFighter's caller for
 * a blocked hit that did not break the shield */
void ftCommonGuardSetOffSetStatus(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    ftMainSetStatus(fighter_gobj, nFTCommonStatusGuardSetOff, 0.0F, 1.0F, (FTSTATUS_PRESERVE_MODELPART | FTSTATUS_PRESERVE_HITSTATUS | FTSTATUS_PRESERVE_EFFECT));

    fp->status_vars.common.guard.setoff_frames = (fp->shield_damage * FTCOMMON_GUARD_SETOFF_MUL) + FTCOMMON_GUARD_SETOFF_ADD;

    fp->physics.vel_ground.x = ((fp->lr == fp->shield_lr) ? -1 : +1) * (fp->status_vars.common.guard.setoff_frames * FTCOMMON_GUARD_VEL_MUL);

    if (fp->status_vars.common.guard.effect_gobj != NULL)
    {
        EFStruct *ep = efGetStruct(fp->status_vars.common.guard.effect_gobj);

        ep->effect_vars.shield.is_damage_shield = TRUE;
    }
    fp->is_shield = TRUE;

    fp->status_vars.common.guard.is_setoff = TRUE;
}
/* ftcommonshieldbreakfly.c:70-80 ftCommonShieldBreakFlyReflectorSetStatus
 * 0x80149608, verbatim since (it was a do-nothing stub):
 * a projectile that beats a reflector's resist shatters it -- the shards
 * where it stood, and the fighter into ShieldBreakFly. */
void ftCommonShieldBreakFlyReflectorSetStatus(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    FTSpecialColl *special_coll = fp->special_coll;
    Vec3f offset = special_coll->offset;

    gmCollisionGetFighterPartsWorldPosition(fp->joints[special_coll->joint_id], &offset);
    efManagerReflectBreakMakeEffect(&offset, fp->reflect_lr);
    ftCommonShieldBreakFlySetStatus(fighter_gobj);
}
/* Kongo Jungle's barrel cannon (ftCommonTaruCannSetStatus) and its twin,
 * Hyrule's ftCommonTwisterSetStatus, are real files: src/dc/ftcommontaru.c and
 * src/dc/ftcommontwister.c.
 *
 * The two stubs were the same finding twice, and makes it three:
 * ftMainSetHitHazard had been routing nGMHitEnvironmentTaruCann here
 * since, so the callers were finished long before the callee. Grep
 * for who calls a function before treating "waiting on X" as a work
 * estimate. */

/* ---- ft/ftcommon/ftcommonescape.c: the roll ------------
 *
 * The decomp's whole file, its seven functions in its order, verbatim
 * but for the one place marked DIVERGES. Two statuses, EscapeF (156)
 * and EscapeB (157): the roll the shield lets go into, forward or back
 * by the stick.
 *
 * The roll moves itself. Its status rows' physics proc is
 * ftPhysicsApplyGroundVelTransN -- the ground velocity comes out of the
 * TransN joint the figatree animates, not out of any number this file
 * writes -- and the about-face halfway through the backward roll is the
 * animation script's own event: ft/ftmain.c:630 (src/dc/ftmain.c:505)
 * writes motion_vars.flags.flag1 from the script and ProcUpdate below
 * turns it into a flipped fp->lr on the next frame. What the file does
 * write is zero: at the end of the animation both velocity vectors go
 * to zero and the fighter goes to Wait.
 *
 * The way in is the shield's interrupt cascade, which is the hole
 * ftCommonGuardCheckInterrupt has carried sinceand which
 * this step closes. The file's two other ways in are the dash
 * (ftCommonDashProcInterrupt) and Donkey's and Samus's neutral-B.
 * Yoshi's roll goes back into his shield rather than to Wait, because
 * his shield is the egg and the egg is a status of its own. */

/* ftcommonescape.c:10-29 ftCommonEscapeProcUpdate 0x801491D0 */
void ftCommonEscapeProcUpdate(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if (fp->motion_vars.flags.flag1 != 0)
    {
        fp->motion_vars.flags.flag1 = 0;
        fp->lr = -fp->lr;
    }
    if (fighter_gobj->anim_frame <= 0.0F)
    {
        fp->physics.vel_air.x = fp->physics.vel_air.y = fp->physics.vel_air.z = 0.0F;
        fp->physics.vel_ground.x = fp->physics.vel_ground.y = fp->physics.vel_ground.z = 0.0F;

        if ((fp->fkind != nFTKindYoshi) && (fp->fkind != nFTKindNYoshi) || (ftCommonGuardCheckInterruptEscape(fighter_gobj) == FALSE))
        {
            ftCommonWaitSetStatus(fighter_gobj);
        }
    }
}

/* ftcommonescape.c:32-35 ftCommonEscapeProcInterrupt 0x80149268, whole as
 * of the item-use step: the body is one call,
 * ftCommonLightThrowCheckInterruptEscape (ftcommonitemthrow.c, compiled
 * unmodified now) -- throwing a held item out of the roll, which is what
 * status_vars.common.escape.itemthrow_buffer_tics is for. It was an empty
 * body from because there were no items to throw, and with it
 * the buffer counted nothing down. */
void ftCommonEscapeProcInterrupt(GObj *fighter_gobj)
{
    ftCommonLightThrowCheckInterruptEscape(fighter_gobj);
}

/* ftcommonescape.c:38-43 ftCommonEscapeProcStatus 0x80149288 */
void ftCommonEscapeProcStatus(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    fp->motion_vars.flags.flag1 = 0;
}

/* ftcommonescape.c:46-58 ftCommonEscapeSetStatus 0x80149294 */
void ftCommonEscapeSetStatus(GObj *fighter_gobj, s32 status_id, s32 itemthrow_buffer_tics)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    fp->proc_status = ftCommonEscapeProcStatus;

    ftMainSetStatus(fighter_gobj, status_id, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);
    ftMainPlayAnimEventsAll(fighter_gobj);

    fp->is_jostle_ignore = TRUE;

    fp->status_vars.common.escape.itemthrow_buffer_tics = itemthrow_buffer_tics;
}

/* ftcommonescape.c:61-68 ftCommonEscapeGetStatus 0x801492F8: forward or
 * back by the sign of the stick against the facing, and only while the
 * stick's tilt is fresh (tap_stick_x is the tics since the last tap, so
 * < 4 is a tap this instant rather than a stick already held over) */
s32 ftCommonEscapeGetStatus(FTStruct *fp)
{
    if ((ABS(fp->input.pl.stick_range.x) >= FTCOMMON_ESCAPE_STICK_RANGE_MIN) && (fp->tap_stick_x < FTCOMMON_ESCAPE_BUFFER_TICS_MAX))
    {
        return ((fp->input.pl.stick_range.x * fp->lr) >= 0) ? nFTCommonStatusEscapeF : nFTCommonStatusEscapeB;
    }
    else return -1;
}

/* ftcommonescape.c:71-83 ftCommonEscapeCheckInterruptSpecialNDonkey
 * 0x8014935C: Donkey's and Samus's charged neutral-B roll out of the
 * charge. No ported caller yet (ft/ftchar/ftdonkey, ft/ftchar/ftsamus). */
sb32 ftCommonEscapeCheckInterruptSpecialNDonkey(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    s32 status_id = ftCommonEscapeGetStatus(fp);

    if (status_id != -1)
    {
        ftCommonEscapeSetStatus(fighter_gobj, status_id, 0);

        return TRUE;
    }
    else return FALSE;
}

/* ftcommonescape.c:86-97 ftCommonEscapeCheckInterruptDash 0x801493A4:
 * Z out of a dash's first three frames is a forward roll, whatever the
 * stick says. ftCommonDashProcInterrupt calls it. */
sb32 ftCommonEscapeCheckInterruptDash(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if (fp->input.pl.button_tap & fp->input.button_mask_z)
    {
        ftCommonEscapeSetStatus(fighter_gobj, nFTCommonStatusEscapeF, 0);

        return TRUE;
    }
    else return FALSE;
}

/* ftcommonescape.c:100-112 ftCommonEscapeCheckInterruptGuard 0x801493EC:
 * the roll out of the shield, and the only way in the port has. The 5
 * is the item-throw buffer the roll out of a shield carries and the
 * other two ways in do not. */
sb32 ftCommonEscapeCheckInterruptGuard(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    s32 status_id = ftCommonEscapeGetStatus(fp);

    if (status_id != -1)
    {
        ftCommonEscapeSetStatus(fighter_gobj, status_id, 5);

        return TRUE;
    }
    else return FALSE;
}

/* ---- ft/ftcommon/ftcommonspeciallw.c: the Down-B -------------------
 * Was a hand copy here from with a NULL for every fighter
 * whose Down-B file was not yet in and a guard that returned FALSE on
 * one. Link's Bomb was the last, so the decomp's file is
 * compiled unmodified instead (src/game/ssb64/Makefile). */

/* ---- the interrupt cascades (ft/fighter.h's
 * ftCommonGroundCheckInterrupt and ftcommonlanding.c:8-25's
 * ftCommonLandingCheckInterrupt macros), ported statuses only, in the
 * game's order: SpecialN, SpecialHi, SpecialLw, (Catch, S4, Hi4, Lw4,)
 * S3, Hi3, Lw3, Attack1, Guard, (Appeal,) KneeBend, (Dash,) Pass, Squat,
 * (Wait,) Turn, Walk. SpecialLw joined; SpecialN leads them
 * as of now that its demux (step 33) is live -- a standing
 * (or walking, squatting, landing) B-tap now reaches the fireball with no
 * synthetic probe. SpecialHi joined right after it at its
 * own demux (step 35) live the same way -- a stick-up B-tap now reaches
 * the Super Jump Punch with no direct call. GuardOn joined,
 * where the game has it, between the jab and KneeBend. */

/* ftcommonwait.c:9-13 ftCommonWaitProcInterrupt 0x8013E070 */
void ftCommonWaitProcInterrupt(GObj *fighter_gobj)
{
    if (ftCommonSpecialNCheckInterruptCommon(fighter_gobj) != FALSE)
    {
        return;
    }
    if (ftCommonSpecialHiCheckInterruptCommon(fighter_gobj) != FALSE)
    {
        return;
    }
    if (ftCommonSpecialLwCheckInterruptCommon(fighter_gobj) != FALSE)
    {
        return;
    }
    /* the grab, the ground cascade's slot: after the last special,
 * before the attacks */
    if (ftCommonCatchCheckInterruptCommon(fighter_gobj) != FALSE)
    {
        return;
    }
    if (ftCommonAttackCheckInterruptGround(fighter_gobj, FTCOMMON_GROUNDATTACK_COMMON) != FALSE)
    {
        return;
    }
    if (ftCommonGuardOnCheckInterruptCommon(fighter_gobj) != FALSE)
    {
        return;
    }
    if (ftCommonAppealCheckInterruptCommon(fighter_gobj) != FALSE)
    {
        return;
    }
    if (ftCommonKneeBendCheckInterruptCommon(fighter_gobj) != FALSE)
    {
        return;
    }
    if (ftCommonDashCheckInterruptCommon(fighter_gobj) != FALSE)
    {
        return;
    }
    if (ftCommonPassCheckInterruptCommon(fighter_gobj) != FALSE)
    {
        return;
    }
    if (ftCommonSquatCheckInterruptCommon(fighter_gobj) != FALSE)
    {
        return;
    }
    if (ftCommonTurnCheckInterruptCommon(fighter_gobj) != FALSE)
    {
        return;
    }
    ftCommonWalkCheckInterruptCommon(fighter_gobj);
}

/* ftcommonwalk.c:76-91 ftCommonWalkProcInterrupt 0x8013E390 */
void ftCommonWalkProcInterrupt(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    s32 status_id;

    if (ftCommonSpecialNCheckInterruptCommon(fighter_gobj) != FALSE)
    {
        return;
    }
    if (ftCommonSpecialHiCheckInterruptCommon(fighter_gobj) != FALSE)
    {
        return;
    }
    if (ftCommonSpecialLwCheckInterruptCommon(fighter_gobj) != FALSE)
    {
        return;
    }
    /* the grab, the ground cascade's slot: after the last special,
 * before the attacks */
    if (ftCommonCatchCheckInterruptCommon(fighter_gobj) != FALSE)
    {
        return;
    }
    if (ftCommonAttackCheckInterruptGround(fighter_gobj, FTCOMMON_GROUNDATTACK_COMMON) != FALSE)
    {
        return;
    }
    if (ftCommonGuardOnCheckInterruptCommon(fighter_gobj) != FALSE)
    {
        return;
    }
    if (ftCommonAppealCheckInterruptCommon(fighter_gobj) != FALSE)
    {
        return;
    }
    if (ftCommonKneeBendCheckInterruptCommon(fighter_gobj) != FALSE)
    {
        return;
    }
    if (ftCommonDashCheckInterruptCommon(fighter_gobj) != FALSE)
    {
        return;
    }
    if (ftCommonPassCheckInterruptCommon(fighter_gobj) != FALSE)
    {
        return;
    }
    if (ftCommonSquatCheckInterruptCommon(fighter_gobj) != FALSE)
    {
        return;
    }
    if (ftCommonWaitCheckInterruptCommon(fighter_gobj) != FALSE)
    {
        return;
    }
    status_id = ftCommonWalkGetWalkStatus(ABS(fp->input.pl.stick_range.x));

    if (status_id != fp->status_id)
    {
        f32 div = ftCommonWalkGetWalkAnimLength(fp, fp->status_id);
        f32 mul = ftCommonWalkGetWalkAnimLength(fp, status_id);

        ftCommonWalkSetStatusParam(fighter_gobj, (s32)((fighter_gobj->anim_frame / div) * mul));
    }
}

/* ftcommonlanding.c:8-27 ftCommonLandingCheckInterrupt, the macro, as a
 * function in the game's order: the three ground attack rows
 * (S4/Hi4/Lw4, S3/Hi3/Lw3, Attack1) are ftCommonAttackCheckInterruptGround's
 * COMMON spelling, and the warp pipe is last. */
static sb32 ftCommonLandingCheckInterrupt(GObj *fighter_gobj)
{
    return (ftCommonSpecialNCheckInterruptCommon(fighter_gobj) != FALSE)   ||
           (ftCommonSpecialHiCheckInterruptCommon(fighter_gobj) != FALSE)  ||
           (ftCommonSpecialLwCheckInterruptCommon(fighter_gobj) != FALSE)  ||
           (ftCommonCatchCheckInterruptCommon(fighter_gobj) != FALSE)      ||
           (ftCommonAttackCheckInterruptGround(fighter_gobj, FTCOMMON_GROUNDATTACK_COMMON) != FALSE) ||
           (ftCommonGuardOnCheckInterruptCommon(fighter_gobj) != FALSE)    ||
           (ftCommonAppealCheckInterruptCommon(fighter_gobj) != FALSE)     ||
           (ftCommonKneeBendCheckInterruptCommon(fighter_gobj) != FALSE)   ||
           (ftCommonDashCheckInterruptCommon(fighter_gobj) != FALSE)       ||
           (ftCommonPassCheckInterruptCommon(fighter_gobj) != FALSE)       ||
           (ftCommonDokanStartCheckInterruptCommon(fighter_gobj) != FALSE);
}

/* ftcommonlanding.c:35-57 ftCommonLandingProcInterrupt 0x80142B70,
 * verbatim: on the one frame the landing opens to
 * interrupts, a stick held down goes straight to SquatWait rather than
 * through Squat's crouch. */
void ftCommonLandingProcInterrupt(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if (fighter_gobj->anim_frame < FTCOMMON_LANDING_INTERRUPT_BEGIN)
    {
        return;
    }
    else if ((fp->status_vars.common.landing.is_allow_interrupt != FALSE) && !(ftCommonLandingCheckInterrupt(fighter_gobj)))
    {
        if ((fighter_gobj->anim_frame >= FTCOMMON_LANDING_INTERRUPT_BEGIN) && (fighter_gobj->anim_frame < (FTCOMMON_LANDING_INTERRUPT_BEGIN + DObjGetStruct(fighter_gobj)->anim_speed)))
        {
            if (ftCommonSquatWaitCheckInterruptLanding(fighter_gobj) != FALSE) return;
        }
        else if (ftCommonSquatCheckInterruptCommon(fighter_gobj) != FALSE) return;

        if (ftCommonTurnCheckInterruptCommon(fighter_gobj) == FALSE)
        {
            ftCommonWalkCheckInterruptCommon(fighter_gobj);
        }
    }
}

/* ---- ft/ftcommon/ftcommonentry.c ------------------------------------ */

/* ftcommonentry.c:11-40 dFTCommonEntryAppearStatusIDs: which per-fighter
 * status the warp-in is, facing right and facing left.filled
 * twenty-five of the twenty-seven rows and filled Pikachu's, so
 * twenty-six are the decomp's own and every playable kind warps in.
 *
 * Master Hand's row was the one left empty until his table existed; it is
 * filled sinceand all twenty-seven kinds are the decomp's
 * own. (NPikachu is a Poly and needs no table of his
 * own: his row below is the common EntryNull, like the other eleven.)
 *
 * The twelve Poly rows are nFTCommonStatusEntryNull, which is a COMMON
 * status (nFTCommonStatusActionStart, 6) with its row in
 * dFTCommonActionStatusDescs and ftCommonEntryNullProcUpdate on it -- the
 * Fighting Polygons do not warp in, they stand still for
 * FTCOMMON_ENTRY_WAIT tics and then wait. It costs nothing to be right
 * about them: the row was already there. */
static const s32 dFTCommonEntryAppearStatusIDs[nFTKindEnumCount][2] =
{
    [nFTKindMario]    = { nFTMarioStatusAppearR,        nFTMarioStatusAppearL        },
    [nFTKindFox]      = { nFTFoxStatusAppearR,          nFTFoxStatusAppearL          },
    [nFTKindDonkey]   = { nFTDonkeyStatusAppearR,       nFTDonkeyStatusAppearL       },
    [nFTKindSamus]    = { nFTSamusStatusAppearR,        nFTSamusStatusAppearL        },
    /* Luigi's warp-in is Mario's status numbers, as his specials are;
 * dFTMainSpecialStatusDescs[nFTKindLuigi] is what makes them his own
 * table's rows. */
    [nFTKindLuigi]    = { nFTMarioStatusAppearR,        nFTMarioStatusAppearL        },
    [nFTKindLink]     = { nFTLinkStatusAppearR,         nFTLinkStatusAppearL         },
    [nFTKindYoshi]    = { nFTYoshiStatusAppearR,        nFTYoshiStatusAppearL        },
    [nFTKindCaptain]  = { nFTCaptainStatusAppearRStart, nFTCaptainStatusAppearLStart },
    [nFTKindKirby]    = { nFTKirbyStatusAppearR,        nFTKirbyStatusAppearL        },
    /* the last playable row: Pikachu's was empty because it had
 * no table for it to point into. */
    [nFTKindPikachu]  = { nFTPikachuStatusAppearR,      nFTPikachuStatusAppearL      },
    [nFTKindPurin]    = { nFTPurinStatusAppearR,        nFTPurinStatusAppearL        },
    [nFTKindNess]     = { nFTNessStatusAppearRStart,    nFTNessStatusAppearLStart    },
    /* The last empty row, filled. It was a DIVERGES from
 * to: dFTMainSpecialStatusDescs[nFTKindBoss] was
 * FT_SPECIAL_NONE, so a filled row would have been
 * ftCommonAppearSetStatus walking into the abort rather than an
 * entrance. The table is the decomp's and rung 13 spawns him. Both
 * facings are the one status, as the game has it -- a hand has no
 * left and right. */
    [nFTKindBoss]     = { nFTBossStatusAppear,          nFTBossStatusAppear          },
    [nFTKindMMario]   = { nFTMarioStatusAppearR,        nFTMarioStatusAppearL        },
    [nFTKindNMario]   = { nFTCommonStatusEntryNull,     nFTCommonStatusEntryNull     },
    [nFTKindNFox]     = { nFTCommonStatusEntryNull,     nFTCommonStatusEntryNull     },
    [nFTKindNDonkey]  = { nFTCommonStatusEntryNull,     nFTCommonStatusEntryNull     },
    [nFTKindNSamus]   = { nFTCommonStatusEntryNull,     nFTCommonStatusEntryNull     },
    [nFTKindNLuigi]   = { nFTCommonStatusEntryNull,     nFTCommonStatusEntryNull     },
    [nFTKindNLink]    = { nFTCommonStatusEntryNull,     nFTCommonStatusEntryNull     },
    [nFTKindNYoshi]   = { nFTCommonStatusEntryNull,     nFTCommonStatusEntryNull     },
    [nFTKindNCaptain] = { nFTCommonStatusEntryNull,     nFTCommonStatusEntryNull     },
    [nFTKindNKirby]   = { nFTCommonStatusEntryNull,     nFTCommonStatusEntryNull     },
    [nFTKindNPikachu] = { nFTCommonStatusEntryNull,     nFTCommonStatusEntryNull     },
    [nFTKindNPurin]   = { nFTCommonStatusEntryNull,     nFTCommonStatusEntryNull     },
    [nFTKindNNess]    = { nFTCommonStatusEntryNull,     nFTCommonStatusEntryNull     },
    [nFTKindGDonkey]  = { nFTDonkeyStatusAppearR,       nFTDonkeyStatusAppearL       },
};

/* The port's own: whether a kind has its warp-in statuses ported, which
 * is whether the table above has its row. ftCommonAppearSetStatus, which
 * the countdown calls on every fighter (if/ifcommon.c's entry focus),
 * stands a kind without one on the spot instead. All 27 rows are the
 * decomp's and this answers TRUE for all of them. It is kept rather than
 * retired because it is what the abort is guarded on and what the host
 * test sweeps; ftCommonEntryAppearIsLive below is the stronger question. */
sb32 ftCommonEntryHasAppear(s32 fkind)
{
    return fkind >= 0 && fkind < nFTKindEnumCount &&
           dFTCommonEntryAppearStatusIDs[fkind][0] != 0;
}

/* The port's own census: a row in dFTCommonEntryAppearStatusIDs and a
 * row in the fighter's OWN status table are TWO facts, and it is the
 * second one that aborts. This
 * answers both at once for one kind and one facing, so the roster can be
 * swept in a loop rather than trusted a kind at a time. entry_id is 0 for
 * facing right, 1 for facing left, as ftCommonAppearSetStatus computes
 * it. */
sb32 ftCommonEntryAppearIsLive(s32 fkind, s32 entry_id)
{
    const FTSpecialTable *table;
    s32 status_id;

    if (!ftCommonEntryHasAppear(fkind) || (u32)entry_id > 1)
    {
        return FALSE;
    }
    status_id = dFTCommonEntryAppearStatusIDs[fkind][entry_id];

    if (status_id < nFTCommonStatusSpecialStart)
    {
        /* the twelve Polys' EntryNull, which is a common action status */
        if (status_id < nFTCommonStatusActionStart)
        {
            return FALSE;
        }
        status_id -= nFTCommonStatusActionStart;

        return status_id < (s32)ARRAY_COUNT(dFTCommonActionStatusDescs) &&
               dFTCommonActionStatusDescs[status_id].proc_update != NULL;
    }
    if ((u32)fkind >= ARRAY_COUNT(dFTMainSpecialStatusDescs))
    {
        return FALSE;
    }
    table = &dFTMainSpecialStatusDescs[fkind];
    status_id -= nFTCommonStatusSpecialStart;

    return table->rows != NULL && status_id < table->count &&
           table->rows[status_id].proc_update != NULL;
}

/* ftcommonentry.c:51-61 ftCommonEntrySetStatus 0x8013D930, verbatim */
void ftCommonEntrySetStatus(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    ftMainSetStatus(fighter_gobj, nFTCommonStatusEntry, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);

    fp->is_invisible = TRUE;
    fp->is_shadow_hide = TRUE;
    fp->is_ghost = TRUE;
    fp->is_playertag_hide = TRUE;
}

/* ftcommonentry.c:64-89 ftCommonEntryNullProcUpdate 0x8013D994,
 * verbatim since-- the Boss branch was dropped while
 * nothing spawned him. He does not walk out of the entrance to Wait
 * like everyone else: ftBossWaitSetStatus hands him to his own
 * chooser, and he keeps the position the entrance left him at. */
void ftCommonEntryNullProcUpdate(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if (fp->status_vars.common.entry.entry_wait != 0)
    {
        fp->status_vars.common.entry.entry_wait--;

        if (fp->status_vars.common.entry.entry_wait == 0)
        {
            if (fp->fkind == nFTKindBoss)
            {
                ftBossWaitSetStatus(fighter_gobj);
            }
            else
            {
                fp->lr = fp->status_vars.common.entry.lr;

                DObjGetStruct(fighter_gobj)->translate.vec.f = fp->entry_pos;

                fp->coll_data.floor_line_id = fp->status_vars.common.entry.floor_line_id;

                ftCommonWaitSetStatus(fighter_gobj);
            }
        }
    }
}

/* ftcommonentry.c:92-110 ftCommonAppearUpdateEffects 0x8013DA14,
 * verbatim since: flag1 is the Poke Ball opening in
 * Pikachu's and Jigglypuff's entrances (efManagerMBallRaysMakeEffect,
 * efmballrays.mdl), flag2 un-hides the shadow partway through every
 * fighter's. Both flags come from the motion script's own events. */
void ftCommonAppearUpdateEffects(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if (fp->motion_vars.flags.flag1 != 0)
    {
        if ((fp->fkind == nFTKindPikachu) || (fp->fkind == nFTKindPurin) || (fp->fkind == nFTKindNPikachu) || (fp->fkind == nFTKindNPurin))
        {
            efManagerMBallRaysMakeEffect(&fp->entry_pos);
        }
        fp->motion_vars.flags.flag1 = 0;
    }
    if (fp->motion_vars.flags.flag2 != 0)
    {
        fp->motion_vars.flags.flag2 = 0;

        fp->is_shadow_hide = FALSE;
    }
}

/* ftcommonentry.c:113-131 ftCommonAppearProcUpdate 0x8013DA94,
 * verbatim since this is the end of Master Hand's own
 * Appear, and what hands him to ftBossWaitSetStatus -- his chooser --
 * rather than to the common Wait. Without it he finished his entrance
 * and stood in FTBossAnimDefault for the rest of the match. */
void ftCommonAppearProcUpdate(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    ftCommonAppearUpdateEffects(fighter_gobj);

    if (fighter_gobj->anim_frame <= 0.0F)
    {
        fp->lr = fp->status_vars.common.entry.lr;

        DObjGetStruct(fighter_gobj)->translate.vec.f = fp->entry_pos;

        fp->coll_data.floor_line_id = fp->status_vars.common.entry.floor_line_id;

        if (fp->fkind == nFTKindBoss)
        {
            ftBossWaitSetStatus(fighter_gobj);
        }
        else ftCommonWaitSetStatus(fighter_gobj);
    }
}

/* ftcommonentry.c:134-155 ftCommonAppearProcPhysics 0x8013DB2C: the
 * warp-in is root motion -- TransN's animated translate, read back off
 * the unlinked joint -- laid onto the spawn point. Velocity is zero throughout
 * so the two agree. */
void ftCommonAppearProcPhysics(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    DObj *topn_joint = fp->joints[nFTPartsJointTopN];
    DObj *transn_joint = fp->joints[nFTPartsJointTransN];

    DObjGetStruct(fighter_gobj)->translate.vec.f.y = fp->entry_pos.y + transn_joint->translate.vec.f.y;

    if (fp->status_vars.common.entry.is_rotate != FALSE)
    {
        DObjGetStruct(fighter_gobj)->translate.vec.f.x = fp->entry_pos.x - transn_joint->translate.vec.f.x;
        DObjGetStruct(fighter_gobj)->translate.vec.f.z = fp->entry_pos.z - transn_joint->translate.vec.f.z;

        topn_joint->rotate.vec.f.y = F_CST_DTOR32(180.0F);
    }
    else
    {
        DObjGetStruct(fighter_gobj)->translate.vec.f.x = fp->entry_pos.x + transn_joint->translate.vec.f.x;
        DObjGetStruct(fighter_gobj)->translate.vec.f.z = fp->entry_pos.z + transn_joint->translate.vec.f.z;
    }
}

/* ftcommonentry.c:158-168 ftCommonAppearInitStatusVars 0x8013DBAC, verbatim */
void ftCommonAppearInitStatusVars(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    fp->is_ghost = TRUE;

    fp->camera_mode = nFTCameraModeEntry;

    fp->is_shadow_hide = TRUE;
    fp->is_playertag_hide = TRUE;
}

/* ftcommonentry.c:171-268 ftCommonAppearSetStatus 0x8013DBE0, verbatim
 * since-- the switch's Boss arm, Master Hand's target
 * search, was the last thing out of it. Every playable fighter's
 * entrance vehicle -- each a full model out of that fighter's own
 * Special2 file, baked -- is made
 * below, and so is Captain's state: is_rotate and his display-link move,
 * read by his own status rows and ftCaptainAppearStartProcUpdate (the
 * car drives in from behind the stage when he faces left, so he starts
 * on link 1 and walks back to FTDISPLAY_DLLINK_DEFAULT himself). */
void ftCommonAppearSetStatus(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    s32 status_id;
    s32 entry_id;

    entry_id = (fp->lr == +1) ? 0 : 1;

    /* DIVERGES: every one of the
 * 27 kinds has its Appear row. A kind without one would appear on
 * the spot instead -- what the entry hid comes back and it stands
 * where it was spawned -- rather than read the row as status 0,
 * which is DeadDown. Found inwith Fox, the first fighter
 * to reach this without a row; kept because it is cheap and because
 * the alternative to a missing row is a fighter that dies on the
 * first tic of the match. */
    if (!ftCommonEntryHasAppear(fp->fkind))
    {
        fp->is_invisible = FALSE;
        fp->is_shadow_hide = FALSE;
        fp->is_ghost = FALSE;
        fp->is_playertag_hide = FALSE;
        mpCommonSetFighterWaitOrFall(fighter_gobj);
        return;
    }
    fp->entry_pos = DObjGetStruct(fighter_gobj)->translate.vec.f;

    fp->status_vars.common.entry.is_rotate = FALSE;

    fp->status_vars.common.entry.lr = fp->lr;

    fp->lr = 0;

    fp->status_vars.common.entry.floor_line_id = fp->coll_data.floor_line_id;

    status_id = dFTCommonEntryAppearStatusIDs[fp->fkind][entry_id];

    /* ftcommonentry.c:191-240, the switch's nine effect arms: Mario's pipe,
 * Donkey's barrel and Samus's point (81), Kirby's star
 * (82), Link's wave and beam (83), Yoshi's egg, Fox's Arwing, Captain
 * Falcon's car, and the Poke Ball Pikachu and Jigglypuff are thrown in
 * from. MMario takes Mario's in the decomp too -- the
 * giant Mario of the 1P game rises out of the same pipe -- and GDonkey
 * takes Donkey's. Kirby's star and the Poke Ball read the entrance's
 * facing: each sweeps in from the side the fighter faces, and the two
 * sweeps are two animations of the one pack. Link's is the only arm in
 * the switch that makes TWO effects. The Boss arm, Master Hand's
 * target search, is the 1P game's and is left out. */
    switch (fp->fkind)
    {
    case nFTKindMario:
    case nFTKindLuigi:
    case nFTKindMMario:
        efManagerMarioEntryDokanMakeEffect(&fp->entry_pos, fp->fkind);
        break;

    case nFTKindDonkey:
    case nFTKindGDonkey:
        efManagerDonkeyEntryTaruMakeEffect(&fp->entry_pos);
        break;

    case nFTKindSamus:
        efManagerSamusEntryPointMakeEffect(&fp->entry_pos);
        break;

    case nFTKindKirby:
        efManagerKirbyEntryStarMakeEffect(&fp->entry_pos,
                                          fp->status_vars.common.entry.lr);
        break;

    /* the only entrance in the game that makes TWO effects */
    case nFTKindLink:
        efManagerLinkEntryWaveMakeEffect(&fp->entry_pos);
        efManagerLinkEntryBeamMakeEffect(&fp->entry_pos);
        break;

    case nFTKindYoshi:
        efManagerYoshiEntryEggMakeEffect(&fp->entry_pos);
        break;

    case nFTKindFox:
        efManagerFoxEntryArwingMakeEffect(&fp->entry_pos,
                                          fp->status_vars.common.entry.lr);
        break;

    case nFTKindPikachu:
    case nFTKindPurin:
        efManagerMBallThrownMakeEffect(&fp->entry_pos, fp->status_vars.common.entry.lr);
        break;

    /* ftcommonentry.c:233-238. Both the state and effect halves of this
 * arm are now implemented. The
 * arm is whole again and the hoisted test is gone with it. */
    case nFTKindCaptain:
        if (fp->status_vars.common.entry.lr == -1)
        {
            fp->status_vars.common.entry.is_rotate = TRUE;
        }
        efManagerCaptainEntryCarMakeEffect(&fp->entry_pos, fp->status_vars.common.entry.lr);
        break;

    /* ftcommonentry.c:238-249, verbatim. Master Hand's
 * arm makes no effect: it picks his target, the first fighter on
 * gGCCommonLinks that is not himself. The decomp's own comment says
 * the loop "assumes Master Hand has found its target, since it is
 * not his own object", and that is exactly what it does -- the link
 * list is the rung's two fighters, so the one that is not him is
 * the human. ftbosswait.c's chooser reads target_gobj every time it
 * picks an attack, so without this arm he never had one to pick. */
    case nFTKindBoss:
    {
        GObj *boss_target_gobj = gGCCommonLinks[nGCCommonLinkIDFighter];

        while (boss_target_gobj != NULL)
        {
            if (boss_target_gobj != fighter_gobj)
            {
                break;
            }
            else boss_target_gobj = boss_target_gobj->link_next;
        }
        fp->passive_vars.boss.p->target_gobj = boss_target_gobj;
        break;
    }
    }
    mpCommonSetFighterAir(fp);
    ftMainSetStatus(fighter_gobj, status_id, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);
    ftCommonAppearInitStatusVars(fighter_gobj);

    fp->status_vars.common.entry.entry_wait = FTCOMMON_ENTRY_WAIT;

    fp->motion_vars.flags.flag1 = 0;
    fp->motion_vars.flags.flag2 = 0;
    fp->motion_vars.flags.flag0 = 0;

    /* ftcommonentry.c:265-268, verbatim */
    if ((fp->fkind == nFTKindCaptain) && (fp->status_vars.common.entry.lr == -1))
    {
        ftParamMoveDLLink(fighter_gobj, 1);
    }
}

/* ftcommonentry.c:285-317, the four Ness entry procs, verbatim. His
 * warp-in is the roster's longest -- five statuses where
 * everyone but Captain Falcon has two -- because he arrives in the PSI
 * ring: a Start that plays while the ring closes, a Wait that holds him
 * inside it, and an End as it opens. These are the update procs and the
 * two setters that walk between them; the ring itself is the motion's
 * own animation, so nothing here is an effect call.
 *
 * ftcommonentry.c:271-283 ftCommonAppearSetPosition is just below. */

/* ftcommonentry.c:271-283 ftCommonAppearSetPosition 0x8013DDF8, verbatim.
 * The 1P game's entry drop: the fighter keeps where it was put as its
 * entry_pos, is lifted to halfway between the camera's top bound and
 * the map's, and falls in. It was NOT in the port until--
 * nothing the port compiled called it -- and sc1PGameWaitStageTeamUpdate
 * is the caller that changed that: it is how each next member of the
 * Yoshi, Kirby and Polygon teams arrives. */
void ftCommonAppearSetPosition(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    fp->camera_mode = 3;

    fp->entry_pos = DObjGetStruct(fighter_gobj)->translate.vec.f;

    DObjGetStruct(fighter_gobj)->translate.vec.f.y = (gMPCollisionGroundData->camera_bound_top + gMPCollisionGroundData->map_bound_top) * 0.5F;

    ftCommonFallSetStatus(fighter_gobj);
}

// 0x8013DE60
void ftNessAppearStartProcUpdate(GObj *fighter_gobj)
{
    ftCommonAppearUpdateEffects(fighter_gobj);
    ftAnimEndCheckSetStatus(fighter_gobj, ftNessAppearWaitSetStatus);
}

// 0x8013DE90
void ftNessAppearWaitProcUpdate(GObj *fighter_gobj)
{
    ftCommonAppearUpdateEffects(fighter_gobj);
    ftAnimEndCheckSetStatus(fighter_gobj, ftNessAppearEndSetStatus);
}

// 0x8013DEC0
void ftNessAppearWaitSetStatus(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    ftMainSetStatus(fighter_gobj, nFTNessStatusAppearWait, 0.0F, 1.0F, (FTSTATUS_PRESERVE_MODELPART | FTSTATUS_PRESERVE_COLANIM));
    ftCommonAppearInitStatusVars(fighter_gobj);

    fp->is_shadow_hide = FALSE;
}

// 0x8013DF14
void ftNessAppearEndSetStatus(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    ftMainSetStatus(fighter_gobj, ((fp->status_vars.common.entry.lr == +1) ? nFTNessStatusAppearREnd : nFTNessStatusAppearLEnd), 0.0F, 1.0F, (FTSTATUS_PRESERVE_MODELPART | FTSTATUS_PRESERVE_COLANIM));
    ftCommonAppearInitStatusVars(fighter_gobj);

    fp->is_shadow_hide = FALSE;
}

/* ftcommonentry.c:320-343, Captain Falcon's two entry procs, verbatim.
 * His warp-in is the roster's only OTHER multi-status
 * one -- Start then End, four statuses because each half has a facing --
 * and the only one that moves the fighter between display links: he
 * arrives inside the Blue Falcon, and facing left the car drives in from
 * behind the stage, so ftCommonAppearSetStatus below puts him on link 1
 * and this proc puts him back on FTDISPLAY_DLLINK_DEFAULT the moment the
 * car's own animation has carried him in front of z = -1000. Both halves
 * read status_vars.common.entry.lr, not fp->lr, which the setter zeroed. */

// 0x8013DF7C
void ftCaptainAppearStartProcUpdate(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    ftCommonAppearUpdateEffects(fighter_gobj);

    if ((fp->status_vars.common.entry.lr == -1) && (fp->dl_link != FTDISPLAY_DLLINK_DEFAULT) && (DObjGetStruct(fighter_gobj)->translate.vec.f.z > -1000.0F))
    {
        ftParamMoveDLLink(fighter_gobj, FTDISPLAY_DLLINK_DEFAULT);
    }
    ftAnimEndCheckSetStatus(fighter_gobj, ftCaptainAppearEndSetStatus);
}

// 0x8013E008
void ftCaptainAppearEndSetStatus(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    ftMainSetStatus(fighter_gobj, ((fp->status_vars.common.entry.lr == +1) ? nFTCaptainStatusAppearREnd : nFTCaptainStatusAppearLEnd), 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);
    ftCommonAppearInitStatusVars(fighter_gobj);

    fp->is_shadow_hide = FALSE;
}

/* ---- the KO: ft/ftcommon/ftcommondead.c, ftcommonsleep.c
 * and ftcommonrebirth.c ------------------------------------------------
 * ftMainProcPhysicsMap's blast-line check (ft/ftmain.c:1815) is live
 * again, so crossing a map bound is what it is in the game: one of the
 * four Dead statuses, a 45-tic wait that pays the score and the stock,
 * then either Sleep (out of stocks) or the Rebirth platform -- the halo
 * lowering the fighter over 90 tics, RebirthStand, RebirthWait, and
 * Fall off the platform with the game's own invincibility timer.
 *
 * The 1P-game arms (ftCommonDeadUpdateScore's GAMERULE_1PGAME and
 * GAMERULE_BONUS branches, ftCommonDeadCheckRebirth's is_spgame_enemy
 * branch and ftCommonDeadCheckInterruptCommon's map_bound_team_* box)
 * were cut while 1P mode was out of the port's target; brought the
 * mode in and they are the game's again. Before that a team rung's
 * enemies never died off its team box, never respawned as the next
 * member, and a rung could only end on the clock.
 * The effects (the explosion, the quake, the white sparkle) and the HUD
 * (the score effect, the stock snap, the damage break anim, the end
 * sound queue) were stubs once and are the game's now.
 * halo_size was on this list until which is when
 * something read it; fp->attr's dead_fgm_ids and deadup_sfx were on it
 * until the audio step carried the voice half of FTAttributes in the
 * pack (src/dc/fighter.h FPackAttr) -- before that they read 0,
 * nSYAudioFGMExplodeS, and every KO voice was an explosion.
 * ---------------------------------------------------------------- */

/* if/ifcommon.c's battle-end sound queue: L5 (src/dc/ifcommon.c). */
void ftCommonDeadAddDeadSFXSoundQueue(u16 sfx_id)
{
    func_800269C0_275C0(sfx_id);
    ifCommonBattleEndAddSoundQueueID(sfx_id);
}

/* ft/ftcommon/ftcommondead.c:31-60 ftCommonDeadUpdateRumble 0x8013BC8C,
 * verbatim (gmRumbleSetPlayerRumbleParams under it is src/dc/gmrumble.c's) */
void ftCommonDeadUpdateRumble(FTStruct *this_fp)
{
    s32 i;

    ftParamMakeRumble(this_fp, 0, 30);

    for (i = 0; i < (s32)ARRAY_COUNT(gSCManagerBattleState->players); i++)
    {
        if ((i != this_fp->player) && (gSCManagerBattleState->players[i].pkind == nFTPlayerKindMan))
        {
            GObj *fighter_gobj = gSCManagerBattleState->players[i].fighter_gobj;

            if (fighter_gobj != NULL)
            {
                FTStruct *other_fp = ftGetStruct(fighter_gobj);

                if ((!(gSCManagerBattleState->game_rules & SCBATTLE_GAMERULE_STOCK)) || (other_fp->stock_count != -1))
                {
                    ftParamMakeRumble(other_fp, 1, 15);
                }
                else continue;
            }
        }
    }
}

/* ft/ftcommon/ftcommondead.c:63-107 ftCommonDeadUpdateScore 0x8013BD64.
 * This is the whole of what a KO is worth: the victim's fall, the
 * killer's score and per-victim KO tally (or a self-destruct when
 * damage_player says nobody hit them), and the stock, verbatim: the
 * 1P game counts its stocks down itself (the rules there are 1PGAME,
 * not STOCK), and a bonus stage's only KO is the player falling off it,
 * which is the FAILURE banner. */
void ftCommonDeadUpdateScore(FTStruct *this_fp)
{
    ifCommonPlayerDamageStartBreakAnim(this_fp);
    ifCommonPlayerStockMakeStockSnap(this_fp);

    gSCManagerBattleState->players[this_fp->player].falls++;

    if (gSCManagerBattleState->is_show_score)
    {
        ifCommonPlayerScoreMakeEffect(this_fp, -1);
    }
    if ((this_fp->damage_player != -1) && (this_fp->damage_player != GMCOMMON_PLAYERS_MAX))
    {
        gSCManagerBattleState->players[this_fp->damage_player].score++;

        gSCManagerBattleState->players[this_fp->damage_player].total_kos_players[this_fp->player]++;

        if (gSCManagerBattleState->is_show_score)
        {
            ifCommonPlayerScoreMakeEffect(ftGetStruct(gSCManagerBattleState->players[this_fp->damage_player].fighter_gobj), 1);
        }
    }
    else gSCManagerBattleState->players[this_fp->player].total_selfdestructs++;

    if (gSCManagerBattleState->game_rules & SCBATTLE_GAMERULE_STOCK)
    {
        this_fp->stock_count--;

        gSCManagerBattleState->players[this_fp->player].stock_count--;

        ifCommonBattleUpdateScoreStocks(this_fp);
    }
    if (gSCManagerBattleState->game_rules & SCBATTLE_GAMERULE_1PGAME)
    {
        this_fp->stock_count--;

        gSCManagerBattleState->players[this_fp->player].stock_count--;

        sc1PGameSetPlayerDefeatStats(this_fp->player, this_fp->team_order);
    }
    if (gSCManagerBattleState->game_rules & SCBATTLE_GAMERULE_BONUS)
    {
        ifCommonAnnounceEndMessage();
    }
}

/* ft/ftcommon/ftcommondead.c:110-141 ftCommonDeadCheckRebirth 0x8013BF94,
 * verbatim: a 1P enemy does not come back, the next member of its team
 * comes on instead. */
void ftCommonDeadCheckRebirth(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if (gSCManagerBattleState->game_rules & SCBATTLE_GAMERULE_STOCK)
    {
        if (fp->stock_count == -1)
        {
            ftCommonSleepSetStatus(fighter_gobj);
            return;
        }
    }
    else if (gSCManagerBattleState->game_rules & SCBATTLE_GAMERULE_1PGAME)
    {
        if (gSCManagerBattleState->players[fp->player].is_spgame_enemy != FALSE)
        {
            sc1PGameSpawnEnemyTeamNext(fighter_gobj);
            return;
        }
        else if (fp->stock_count == -1)
        {
            ftCommonSleepSetStatus(fighter_gobj);
            return;
        }
    }
    ftCommonRebirthDownSetStatus(fighter_gobj);
}

/* ft/ftcommon/ftcommondead.c:144-159 ftCommonDeadResetCommonVars
 * 0x8013C050, verbatim -- a KO destroys the fighter's live weapons and
 * whatever it was holding along with the voice, the thrown result, the
 * kinetics kind and the dropped floor line. */
void ftCommonDeadResetCommonVars(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    ftParamStopVoiceRunProcDamage(fighter_gobj);
    ftManagerDestroyFighterWeapons(fighter_gobj);
    ftCommonThrownDecideDeadResult(fighter_gobj);

    fp->ga = nMPKineticsAir;
    fp->coll_data.floor_line_id = -1;

    if (fp->item_gobj != NULL)
    {
        itMainDestroyItem(fp->item_gobj);
    }
}

/* ft/ftcommon/ftcommondead.c:162-171 ftCommonDeadResetSpecialStats
 * 0x8013C0B0, verbatim. is_ghost is what takes the fighter out of the
 * hit search and out of ftCommonDeadCheckInterruptCommon's own bounds
 * test, so a dead fighter cannot be KO'd twice. */
void ftCommonDeadResetSpecialStats(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    fp->star_invincible_tics = 0;
    fp->is_ghost = TRUE;
    fp->is_shadow_hide = TRUE;

    ftParamTryUpdateItemMusic();
}

/* ft/ftcommon/ftcommondead.c:174-184 ftCommonDeadCommonProcUpdate
 * 0x8013C0EC, verbatim */
void ftCommonDeadCommonProcUpdate(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    fp->status_vars.common.dead.wait--;

    if (fp->status_vars.common.dead.wait == 0)
    {
        ftCommonDeadCheckRebirth(fighter_gobj);
    }
}

/* ft/ftcommon/ftcommondead.c:187-208 ftCommonDeadInitStatusVars
 * 0x8013C120, verbatim */
void ftCommonDeadInitStatusVars(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    fp->status_vars.common.dead.wait = FTCOMMON_DEAD_WAIT;

    ftPhysicsStopVelAll(fighter_gobj);

    fp->is_invisible = TRUE;
    fp->is_menu_ignore = TRUE;
    fp->is_playertag_hide = TRUE;

    efManagerQuakeMakeEffect(2);
    ftCommonDeadUpdateRumble(fp);
    ftCommonDeadUpdateScore(fp);

    if (fp->attr->dead_fgm_ids[0] != nSYAudioFGMVoiceEnd)
    {
        ftCommonDeadAddDeadSFXSoundQueue(fp->attr->dead_fgm_ids[0]);
    }
    if (fp->attr->dead_fgm_ids[1] != nSYAudioFGMVoiceEnd)
    {
        ftCommonDeadAddDeadSFXSoundQueue(fp->attr->dead_fgm_ids[1]);
    }
}

/* The explosion sound id the three "blown off the side or the bottom"
 * deaths pick: ftcommondead.c's three identical bonus-gkind tests
 * (0x8013C1C4, 0x8013C30C, 0x8013C454), one copy here; a bonus stage
 * gets the small one. */
static u32 ftCommonDeadGetExplodeFGMID(void)
{
    if (((gSCManagerBattleState->gkind >= nGRKindBonus1Start) && (gSCManagerBattleState->gkind <= nGRKindBonus1End)) ||
        ((gSCManagerBattleState->gkind >= nGRKindBonus2Start) && (gSCManagerBattleState->gkind <= nGRKindBonus2End)))
    {
        return nSYAudioFGMDeadExplodeS;
    }
    return nSYAudioFGMDeadExplodeL;
}

/* ft/ftcommon/ftcommondead.c:211-263 ftCommonDeadDownSetStatus
 * 0x8013C1C4, verbatim but for the shared FGM helper above */
void ftCommonDeadDownSetStatus(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    Vec3f pos;

    ftCommonDeadResetCommonVars(fighter_gobj);
    ftMainSetStatus(fighter_gobj, nFTCommonStatusDeadDown, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);
    ftCommonDeadResetSpecialStats(fighter_gobj);
    ftCommonDeadInitStatusVars(fighter_gobj);

    pos = DObjGetStruct(fighter_gobj)->translate.vec.f;

    if (gSCManagerBattleState->game_type != nSCBattleGameTypeBonus)
    {
        if (pos.x > gMPCollisionGroundData->camera_bound_right)
        {
            pos.x = gMPCollisionGroundData->camera_bound_right;
        }
        if (pos.x < gMPCollisionGroundData->camera_bound_left)
        {
            pos.x = gMPCollisionGroundData->camera_bound_left;
        }
    }
    efManagerDeadExplodeMakeEffect(&pos, fp->player, 0);
    ifScreenFlashSetColAnimID(nGMColAnimScreenFlashDeadExplode, 0);

    ftCommonDeadAddDeadSFXSoundQueue(ftCommonDeadGetExplodeFGMID());
}

/* ft/ftcommon/ftcommondead.c:266-318 ftCommonDeadRightSetStatus
 * 0x8013C30C, verbatim but for the shared FGM helper */
void ftCommonDeadRightSetStatus(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    Vec3f pos;

    ftCommonDeadResetCommonVars(fighter_gobj);
    ftMainSetStatus(fighter_gobj, nFTCommonStatusDeadLeftRight, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);
    ftCommonDeadResetSpecialStats(fighter_gobj);
    ftCommonDeadInitStatusVars(fighter_gobj);

    pos = DObjGetStruct(fighter_gobj)->translate.vec.f;

    if (gSCManagerBattleState->game_type != nSCBattleGameTypeBonus)
    {
        if (pos.y > gMPCollisionGroundData->camera_bound_top)
        {
            pos.y = gMPCollisionGroundData->camera_bound_top;
        }
        if (pos.y < gMPCollisionGroundData->camera_bound_bottom)
        {
            pos.y = gMPCollisionGroundData->camera_bound_bottom;
        }
    }
    efManagerDeadExplodeMakeEffect(&pos, fp->player, 1);
    ifScreenFlashSetColAnimID(nGMColAnimScreenFlashDeadExplode, 0);

    ftCommonDeadAddDeadSFXSoundQueue(ftCommonDeadGetExplodeFGMID());
}

/* ft/ftcommon/ftcommondead.c:321-373 ftCommonDeadLeftSetStatus
 * 0x8013C454, verbatim but for the shared FGM helper. Same status as
 * Right; only the explosion's type argument differs (1 vs 3). */
void ftCommonDeadLeftSetStatus(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    Vec3f pos;

    ftCommonDeadResetCommonVars(fighter_gobj);
    ftMainSetStatus(fighter_gobj, nFTCommonStatusDeadLeftRight, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);
    ftCommonDeadResetSpecialStats(fighter_gobj);
    ftCommonDeadInitStatusVars(fighter_gobj);

    pos = DObjGetStruct(fighter_gobj)->translate.vec.f;

    if (gSCManagerBattleState->game_type != nSCBattleGameTypeBonus)
    {
        if (pos.y > gMPCollisionGroundData->camera_bound_top)
        {
            pos.y = gMPCollisionGroundData->camera_bound_top;
        }
        if (pos.y < gMPCollisionGroundData->camera_bound_bottom)
        {
            pos.y = gMPCollisionGroundData->camera_bound_bottom;
        }
    }
    efManagerDeadExplodeMakeEffect(&pos, fp->player, 3);
    ifScreenFlashSetColAnimID(nGMColAnimScreenFlashDeadExplode, 0);

    ftCommonDeadAddDeadSFXSoundQueue(ftCommonDeadGetExplodeFGMID());
}

/* ft/ftcommon/ftcommondead.c:376-461 ftCommonDeadUpStarProcUpdate
 * 0x8013C59C, verbatim: the three-stage star KO -- launch up and fade
 * into the fog colour over 180 tics, twinkle and pay the score, then
 * the 45-tic Dead wait. */
void ftCommonDeadUpStarProcUpdate(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    switch (fp->motion_vars.flags.flag1)
    {
    case 1:
        fp->colanim.color1.a = 128 - ((fp->status_vars.common.dead.wait * 128) / FTCOMMON_DEADUP_WAIT);
        break;

    default:
        break;
    }
    if (fp->status_vars.common.dead.wait != 0)
    {
        fp->status_vars.common.dead.wait--;
    }
    if (fp->status_vars.common.dead.wait == 0)
    {
        switch (fp->motion_vars.flags.flag1)
        {
        case 0:
            fp->physics.vel_air.y = ((gMPCollisionGroundData->camera_bound_top * 0.6F) - DObjGetStruct(fighter_gobj)->translate.vec.f.y) / FTCOMMON_DEADUP_WAIT;
            fp->physics.vel_air.z = FTCOMMON_DEADUPSTAR_VEL_Z;

            fp->colanim.is_use_color1 = TRUE;

            fp->colanim.color1.r = gMPCollisionGroundData->fog_color.r;
            fp->colanim.color1.g = gMPCollisionGroundData->fog_color.g;
            fp->colanim.color1.b = gMPCollisionGroundData->fog_color.b;
            fp->colanim.color1.a = 0;

            fp->status_vars.common.dead.wait = FTCOMMON_DEADUP_WAIT;

            fp->motion_vars.flags.flag1++;
            break;

        case 1:
            ftPhysicsStopVelAll(fighter_gobj);
            efManagerSparkleWhiteDeadMakeEffect(&fp->joints[nFTPartsJointTopN]->translate.vec.f, 5.0F);

            fp->is_invisible = TRUE;
            fp->is_menu_ignore = TRUE;

            ftCommonDeadUpdateScore(fp);
            ftCommonDeadAddDeadSFXSoundQueue(nSYAudioFGMDeadUpStar);

            fp->is_playertag_hide = TRUE;
            fp->colanim.is_use_color1 = FALSE;

            fp->status_vars.common.dead.wait = FTCOMMON_DEAD_WAIT;

            fp->motion_vars.flags.flag1++;
            break;

        case 2:
            ftCommonDeadCheckRebirth(fighter_gobj);
            break;

        default:
            break;
        }
    }
}

/* ft/ftcommon/ftcommondead.c:464-499 ftCommonDeadUpStarSetStatus
 * 0x8013C740, verbatim. */
void ftCommonDeadUpStarSetStatus(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    ftCommonDeadResetCommonVars(fighter_gobj);
    ftMainSetStatus(fighter_gobj, nFTCommonStatusDeadUpStar, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);
    ftPhysicsStopVelAll(fighter_gobj);

    fp->status_vars.common.dead.pos = DObjGetStruct(fighter_gobj)->translate.vec.f;

    fp->camera_mode = nFTCameraModeDeadUp;

    fp->status_vars.common.dead.wait = 1;

    fp->motion_vars.flags.flag1 = 0;

    ftCommonDeadResetSpecialStats(fighter_gobj);
    ftParamSetPlayerTagWait(fighter_gobj, 1);

    if (fp->attr->deadup_sfx != nSYAudioFGMVoiceEnd)
    {
        func_800269C0_275C0(fp->attr->deadup_sfx);
    }
    ftParamMoveDLLink(fighter_gobj, 1);
    ftParamResetFighterColAnim(fighter_gobj);
}

/* ft/ftcommon/ftcommondead.c:502-598 ftCommonDeadUpFallProcUpdate
 * 0x8013C80C, verbatim but for the shared FGM helper: the rarer top
 * KO -- teleported above the camera, falling back through it to the
 * bottom camera bound, then the explosion and the Dead wait. */
void ftCommonDeadUpFallProcUpdate(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    switch (fp->motion_vars.flags.flag1)
    {
    case 1:
        if (DObjGetStruct(fighter_gobj)->translate.vec.f.y < gMPCollisionGroundData->map_bound_bottom)
        {
            fp->physics.vel_air.y = 0.0F;
        }
        break;

    default:
        break;
    }
    if (fp->status_vars.common.dead.wait != 0)
    {
        fp->status_vars.common.dead.wait--;
    }
    if (fp->status_vars.common.dead.wait == 0)
    {
        switch (fp->motion_vars.flags.flag1)
        {
        case 0:
            fp->physics.vel_air.y = (gMPCollisionGroundData->camera_bound_bottom - DObjGetStruct(fighter_gobj)->translate.vec.f.y) / FTCOMMON_DEADUP_WAIT;

            DObjGetStruct(fighter_gobj)->translate.vec.f.z = CObjGetStruct(gGMCameraGObj)->vec.eye.z - 3000.0F;

            if (DObjGetStruct(fighter_gobj)->translate.vec.f.z < 2000.0F)
            {
                DObjGetStruct(fighter_gobj)->translate.vec.f.z = 2000.0F;
            }
            DObjGetStruct(fighter_gobj)->translate.vec.f.x = CObjGetStruct(gGMCameraGObj)->vec.eye.x;
            DObjGetStruct(fighter_gobj)->translate.vec.f.y = CObjGetStruct(gGMCameraGObj)->vec.eye.y + 3000.0F;

            if (DObjGetStruct(fighter_gobj)->translate.vec.f.y > gMPCollisionGroundData->map_bound_top)
            {
                DObjGetStruct(fighter_gobj)->translate.vec.f.y = gMPCollisionGroundData->map_bound_top;
            }
            fp->status_vars.common.dead.wait = FTCOMMON_DEADUP_WAIT;

            fp->motion_vars.flags.flag1++;
            break;

        case 1:
            ftPhysicsStopVelAll(fighter_gobj);
            ifScreenFlashSetColAnimID(nGMColAnimScreenFlashDeadExplode, 0);
            efManagerQuakeMakeEffect(2);
            ftCommonDeadUpdateRumble(fp);

            fp->is_playertag_hide = TRUE;
            fp->is_invisible = TRUE;
            fp->is_menu_ignore = TRUE;

            ftCommonDeadUpdateScore(fp);

            ftCommonDeadAddDeadSFXSoundQueue(ftCommonDeadGetExplodeFGMID());

            if (fp->attr->dead_fgm_ids[0] != nSYAudioFGMVoiceEnd)
            {
                ftCommonDeadAddDeadSFXSoundQueue(fp->attr->dead_fgm_ids[0]);
            }
            if (fp->attr->dead_fgm_ids[1] != nSYAudioFGMVoiceEnd)
            {
                ftCommonDeadAddDeadSFXSoundQueue(fp->attr->dead_fgm_ids[1]);
            }
            fp->status_vars.common.dead.wait = FTCOMMON_DEAD_WAIT;
            fp->motion_vars.flags.flag1++;
            break;

        case 2:
            ftCommonDeadCheckRebirth(fighter_gobj);
            break;

        default:
            break;
        }
    }
}

/* ft/ftcommon/ftcommondead.c:601-625 ftCommonDeadUpFallSetStatus
 * 0x8013CAAC. DIVERGES: ftParamSetModelPartDetailAll is a no-op
 * (src/dc/ftparam.c: the pack has no detail levels to swap). */
void ftCommonDeadUpFallSetStatus(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    ftCommonDeadResetCommonVars(fighter_gobj);
    ftMainSetStatus(fighter_gobj, nFTCommonStatusDeadUpFall, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);
    ftPhysicsStopVelAll(fighter_gobj);

    fp->status_vars.common.dead.pos = DObjGetStruct(fighter_gobj)->translate.vec.f;

    fp->camera_mode = nFTCameraModeDeadUp;

    fp->status_vars.common.dead.wait = 1;

    fp->motion_vars.flags.flag1 = 0;

    ftCommonDeadResetSpecialStats(fighter_gobj);
    ftParamSetPlayerTagWait(fighter_gobj, 1);

    if (fp->attr->deadup_sfx != nSYAudioFGMVoiceEnd)
    {
        func_800269C0_275C0(fp->attr->deadup_sfx);
    }
    ftParamMoveDLLink(fighter_gobj, 19);
    ftParamSetModelPartDetailAll(fighter_gobj, nFTPartsDetailHigh);
}

/* ft/ftcommon/ftcommondead.c:628-647 ftCommonDeadCheckInterruptCommon
 * 0x8013CB7C, the blast-line check ftMainProcPhysicsMap runs on every
 * fighter every frame, after the frame's position has settled. The
 * is_spgame_enemy arm dies on the stage's team box (STG5's map_team,
 * src/dc/stage.c) instead of its blast lines -- a team rung's enemies
 * go out nearer the stage than the player does. */
sb32 ftCommonDeadCheckInterruptCommon(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    Vec3f *pos = &fp->joints[nFTPartsJointTopN]->translate.vec.f;

    if (fp->fkind == nFTKindBoss)
    {
        return FALSE;
    }
    if (fp->is_ignore_dead)
    {
        return FALSE;
    }
    if (fp->is_limit_map_bounds)
    {
        if (pos->y < gMPCollisionGroundData->map_bound_bottom)
        {
            pos->y = gMPCollisionGroundData->map_bound_bottom + 500.0F;

            fp->physics.vel_air.x = 0.0F;
            fp->physics.vel_air.y = 0.0F;
            fp->physics.vel_air.z = 0.0F;
        }
        else if (pos->y > gMPCollisionGroundData->map_bound_top)
        {
            pos->y = gMPCollisionGroundData->map_bound_top - 500.0F;

            fp->physics.vel_air.x = 0.0F;
            fp->physics.vel_air.y = 0.0F;
            fp->physics.vel_air.z = 0.0F;
        }
        if (pos->x > gMPCollisionGroundData->map_bound_right)
        {
            pos->x = gMPCollisionGroundData->map_bound_right - 500.0F;

            fp->physics.vel_air.x = 0.0F;
            fp->physics.vel_air.y = 0.0F;
            fp->physics.vel_air.z = 0.0F;
        }
        else if (pos->x < gMPCollisionGroundData->map_bound_left)
        {
            pos->x = gMPCollisionGroundData->map_bound_left + 500.0F;

            fp->physics.vel_air.x = 0.0F;
            fp->physics.vel_air.y = 0.0F;
            fp->physics.vel_air.z = 0.0F;
        }
        return FALSE;
    }
    else if (!(fp->is_ghost))
    {
#ifdef FT_TRACE_KO
        /* -DFT_TRACE_KO: say what the check saw the tic it fires, for a
 * fighter that dies where he should not ('s Fox) */
        if (pos->y < gMPCollisionGroundData->map_bound_bottom ||
            pos->y > gMPCollisionGroundData->map_bound_top ||
            pos->x < gMPCollisionGroundData->map_bound_left ||
            pos->x > gMPCollisionGroundData->map_bound_right)
        {
            syDebugPrintf("ko : kind %d status %d at (%.1f, %.1f) bounds x %.0f..%.0f y %.0f..%.0f; root (%.1f, %.1f) ghost %d\n",
                          (int)fp->fkind, (int)fp->status_id, pos->x, pos->y,
                          gMPCollisionGroundData->map_bound_left, gMPCollisionGroundData->map_bound_right,
                          gMPCollisionGroundData->map_bound_bottom, gMPCollisionGroundData->map_bound_top,
                          DObjGetStruct(fighter_gobj)->translate.vec.f.x,
                          DObjGetStruct(fighter_gobj)->translate.vec.f.y, (int)fp->is_ghost);
        }
#endif
        if ((gSCManagerBattleState->game_type == nSCBattleGameType1PGame) && (gSCManagerBattleState->players[fp->player].is_spgame_enemy != FALSE))
        {
            if (pos->y < gMPCollisionGroundData->map_bound_team_bottom)
            {
                ftCommonDeadDownSetStatus(fighter_gobj);

                return TRUE;
            }
            if (pos->x > gMPCollisionGroundData->map_bound_team_right)
            {
                ftCommonDeadRightSetStatus(fighter_gobj);

                return TRUE;
            }
            if (pos->x < gMPCollisionGroundData->map_bound_team_left)
            {
                ftCommonDeadLeftSetStatus(fighter_gobj);

                return TRUE;
            }
            if (pos->y > gMPCollisionGroundData->map_bound_team_top)
            {
                if (syUtilsRandFloat() < (1.0F / 6.0F))
                {
                    ftCommonDeadUpFallSetStatus(fighter_gobj);

                    return TRUE;
                }
                else ftCommonDeadUpStarSetStatus(fighter_gobj);

                return TRUE;
            }
        }
        else if (pos->y < gMPCollisionGroundData->map_bound_bottom)
        {
            ftCommonDeadDownSetStatus(fighter_gobj);

            return TRUE;
        }
        else if (pos->x > gMPCollisionGroundData->map_bound_right)
        {
            ftCommonDeadRightSetStatus(fighter_gobj);

            return TRUE;
        }
        else if (pos->x < gMPCollisionGroundData->map_bound_left)
        {
            ftCommonDeadLeftSetStatus(fighter_gobj);

            return TRUE;
        }
        else if (pos->y > gMPCollisionGroundData->map_bound_top)
        {
            if (syUtilsRandFloat() < (1.0F / 6.0F))
            {
                ftCommonDeadUpFallSetStatus(fighter_gobj);
            }
            else ftCommonDeadUpStarSetStatus(fighter_gobj);

            return TRUE;
        }
    }
    return FALSE;
}

/* ---- ft/ftcommon/ftcommonsleep.c: out of stocks -------------------
 * The whole file, verbatim. Outside a team battle every branch of
 * ProcUpdate is false, so Sleep is exactly what it looks like: the
 * fighter is invisible, ghosted, and never leaves the status. The
 * team-battle stock steal is kept because it is logic, not content --
 * the day a team battle can be set up it works. */

/* ft/ftcommon/ftcommonsleep.c:12-46 ftCommonSleepCheckIgnorePauseMenu
 * 0x8013D580, verbatim (the pause menu that calls it is mn/'s) */
sb32 ftCommonSleepCheckIgnorePauseMenu(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    s32 steal_from_player[GMCOMMON_PLAYERS_MAX];
    s32 active_teammate_count;
    s32 player;
    s32 stock_count;

    (void)steal_from_player;    /* the decomp fills it here and never reads it */

    if ((gSCManagerBattleState->game_rules & SCBATTLE_GAMERULE_STOCK) && (gSCManagerBattleState->is_team_battle == TRUE) && (fp->status_id == nFTCommonStatusSleep))
    {
        if (fp->status_vars.common.sleep.stock_steal_wait == 0)
        {
            for (active_teammate_count = 0, stock_count = 0, player = 0; player < (s32)ARRAY_COUNT(gSCManagerBattleState->players); player++)
            {
                if ((player != fp->player) && (gSCManagerBattleState->players[player].pkind != nFTPlayerKindNot) && (fp->team == gSCManagerBattleState->players[player].player))
                {
                    if (gSCManagerBattleState->players[player].stock_count > 0)
                    {
                        if (stock_count < gSCManagerBattleState->players[player].stock_count)
                        {
                            active_teammate_count = 0;
                            stock_count = gSCManagerBattleState->players[player].stock_count;
                        }
                        steal_from_player[active_teammate_count] = player;

                        active_teammate_count++;
                    }
                }
            }
            if (active_teammate_count != 0)
            {
                return TRUE; /* Do not bring up pause menu */
            }
        }
    }
    return FALSE; /* Bring up pause menu */
}

/* ft/ftcommon/ftcommonsleep.c:50-119 ftCommonSleepProcUpdate
 * 0x8013D6D0, verbatim */
void ftCommonSleepProcUpdate(GObj *fighter_gobj)
{
    FTStruct *this_fp = ftGetStruct(fighter_gobj);
    GObj *steal_gobj;
    s32 active_teammate_count;
    s32 steal_from_player[GMCOMMON_PLAYERS_MAX];
    s32 player;
    s32 random_steal_target;
    s32 stock_count;

    if ((gSCManagerBattleState->game_rules & SCBATTLE_GAMERULE_STOCK) && (gSCManagerBattleState->is_team_battle == TRUE))
    {
        if (this_fp->status_vars.common.sleep.stock_steal_wait != 0)
        {
            this_fp->status_vars.common.sleep.stock_steal_wait--;

            if (this_fp->status_vars.common.sleep.stock_steal_wait == 0)
            {
                this_fp->stock_count = 0;
                gSCManagerBattleState->players[this_fp->player].stock_count = 0;

                ftCommonRebirthDownSetStatus(fighter_gobj);
            }
        }
        else
        {
            if (this_fp->input.pl.button_tap & START_BUTTON)
            {
                for (active_teammate_count = 0, stock_count = 0, player = 0; player < (s32)ARRAY_COUNT(gSCManagerBattleState->players); player++)
                {
                    if ((player != this_fp->player) && (gSCManagerBattleState->players[player].pkind != nFTPlayerKindNot) && (this_fp->team == gSCManagerBattleState->players[player].player))
                    {
                        if (gSCManagerBattleState->players[player].stock_count > 0)
                        {
                            if (stock_count < gSCManagerBattleState->players[player].stock_count)
                            {
                                active_teammate_count = 0;
                                stock_count = gSCManagerBattleState->players[player].stock_count;
                            }
                            steal_from_player[active_teammate_count] = player;
                            active_teammate_count++;
                        }
                    }
                }
                if (active_teammate_count != 0)
                {
                    random_steal_target = syUtilsRandIntRange(active_teammate_count);

                    gSCManagerBattleState->players[steal_from_player[random_steal_target]].stock_count--;

                    steal_gobj = gSCManagerBattleState->players[steal_from_player[random_steal_target]].fighter_gobj;

                    ftGetStruct(steal_gobj)->stock_count--;

                    this_fp->stock_count = -2;

                    gSCManagerBattleState->players[this_fp->player].stock_count = -2;

                    this_fp->status_vars.common.sleep.stock_steal_wait = FTCOMMON_SLEEP_STOCK_STEAL_WAIT;

                    ifCommonPlayerStockStealMakeInterface(this_fp->player, steal_from_player[random_steal_target]);

                    func_800269C0_275C0(nSYAudioFGMStockSteal);
                }
            }
        }
    }
}

/* ft/ftcommon/ftcommonsleep.c:122-137 ftCommonSleepSetStatus
 * 0x8013D8B0, verbatim */
void ftCommonSleepSetStatus(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    ftMainSetStatus(fighter_gobj, nFTCommonStatusSleep, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);

    fp->is_invisible = TRUE;
    fp->is_shadow_hide = TRUE;
    fp->is_ghost = TRUE;
    fp->is_menu_ignore = TRUE;

    fp->status_vars.common.sleep.stock_steal_wait = 0;

    fp->camera_mode = nFTCameraModeGhost;

    fp->is_playertag_hide = TRUE;
}

/* ---- ft/ftcommon/ftcommonrebirth.c: the respawn platform ----------
 * The whole file, verbatim. ftMainRespawn's DIVERGES: the fighter
 * comes back at the stage's
 * Rebirth mapobj, offset so two respawning fighters do not share a
 * halo, lowered from map_bound_top on the halo's own quadratic over 90
 * tics, standing at 315 tics, and dropped off the platform at 390 with
 * ftParamSetTimedHitStatusInvincible's timer running.
 *
 * The platform itself is a real floor: coll_data.floor_line_id = -2 is
 * the game's "standing on nothing the line table knows about", which
 * mp/mpcollision.c and ftMainProcPhysicsMap both already honour (the
 * floor-speed lookup skips -2), so nothing had to be added for it. */

/* ft/ftcommon/ftcommonrebirth.c:11 dFTCommonRebirthOffsetsX */
f32 dFTCommonRebirthOffsetsX[/* */] = { 0.0F, -1000.0F, 1000.0F, -2000.0F };

/* ft/ftcommon/ftcommonrebirth.c:20-103 ftCommonRebirthDownSetStatus
 * 0x8013CF60, verbatim including the decomp's own `goto loop` (the
 * halo-number search restarts from the head of the fighter link every
 * time it takes a number). The halo is real since the
 * effect is made here, hangs off the fighter's TopN, and is taken away
 * by ftMainSetStatus's ftParamProcStopEffect when the fighter leaves
 * RebirthWait (src/dc/efmanager.c, src/dc/ftparam.c). */
void ftCommonRebirthDownSetStatus(GObj *this_gobj)
{
    FTStruct *this_fp = ftGetStruct(this_gobj);
    FTDesc rebirth_vars = dFTManagerDefaultFighterDesc;
    GObj *other_gobj;
    FTStruct *other_fp;
    s32 halo_number;
    s32 halo_mapobj;
    Vec3f halo_spawn_pos;

    rebirth_vars.lr = this_fp->lr;
    rebirth_vars.damage = 0;

    mpCollisionGetMapObjIDsKind(nMPMapObjKindRebirth, &halo_mapobj);
    mpCollisionGetMapObjPositionID(halo_mapobj, &halo_spawn_pos);

    halo_number = 0;

loop:
    other_gobj = gGCCommonLinks[nGCCommonLinkIDFighter];

    while (other_gobj != NULL)
    {
        if (other_gobj != this_gobj)
        {
            other_fp = ftGetStruct(other_gobj);

            if ((other_fp->status_id >= nFTCommonStatusRebirthDown) &&
                (other_fp->status_id <= nFTCommonStatusRebirthWait))
            {
                if (halo_number == other_fp->status_vars.common.rebirth.halo_number)
                {
                    halo_number++;

                    goto loop;
                }
            }
            else goto next_gobj;
        }
    next_gobj:
        other_gobj = other_gobj->link_next;
    }
    rebirth_vars.pos.x = dFTCommonRebirthOffsetsX[halo_number] + halo_spawn_pos.x;
    rebirth_vars.pos.y = gMPCollisionGroundData->map_bound_top;
    rebirth_vars.pos.z = 0.0F;

    ftManagerInitFighter(this_gobj, &rebirth_vars);
    ifCommonPlayerDamageStopBreakAnim(this_fp);
    mpCommonSetFighterGround(this_fp);

    this_fp->coll_data.floor_line_id = -2;
    this_fp->coll_data.floor_flags = MAP_VERTEX_COLL_PASS;
    this_fp->coll_data.floor_angle.y = 1.0F;
    this_fp->coll_data.floor_angle.x = 0.0F;
    this_fp->coll_data.floor_angle.z = 0.0F;

    ftMainSetStatus(this_gobj, nFTCommonStatusRebirthDown, 100.0F, 1.0F, FTSTATUS_PRESERVE_NONE);
    ftMainPlayAnimEventsAll(this_gobj);
    ftPhysicsStopVelAll(this_gobj);

    this_fp->status_vars.common.rebirth.halo_lower_wait = FTCOMMON_REBIRTH_HALO_LOWER_WAIT;
    this_fp->status_vars.common.rebirth.halo_despawn_wait = FTCOMMON_REBIRTH_HALO_DESPAWN_WAIT;
    this_fp->status_vars.common.rebirth.pos = DObjGetStruct(this_gobj)->translate.vec.f;
    this_fp->status_vars.common.rebirth.halo_offset.x = dFTCommonRebirthOffsetsX[halo_number] + halo_spawn_pos.x;
    this_fp->status_vars.common.rebirth.halo_offset.y = halo_spawn_pos.y;
    this_fp->status_vars.common.rebirth.halo_offset.z = 0.0F;

    this_fp->is_menu_ignore = TRUE;
    this_fp->is_ghost = TRUE;
    this_fp->is_shadow_hide = TRUE;
    this_fp->is_rebirth = TRUE;
    this_fp->camera_mode = nFTCameraModeGhost;

    this_fp->status_vars.common.rebirth.halo_number = halo_number;

    this_fp->camera_zoom_range = 0.6F;

    if (efManagerRebirthHaloMakeEffect(this_gobj, this_fp->attr->halo_size) != NULL)
    {
        this_fp->is_effect_attach = TRUE;
    }
    ftParamCheckSetFighterColAnimID(this_gobj, nGMColAnimFighterRebirth, 0);
    ftParamSetPlayerTagWait(this_gobj, 1);
}

/* ft/ftcommon/ftcommonrebirth.c:106-118
 * ftCommonRebirthCommonUpdateHaloWait 0x8013D1D4, verbatim */
void ftCommonRebirthCommonUpdateHaloWait(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if (fp->status_vars.common.rebirth.halo_despawn_wait != 0)
    {
        fp->status_vars.common.rebirth.halo_despawn_wait--;
    }
    if (fp->status_vars.common.rebirth.halo_lower_wait != 0)
    {
        fp->status_vars.common.rebirth.halo_lower_wait--;
    }
}

/* ft/ftcommon/ftcommonrebirth.c:121-135 ftCommonRebirthDownProcUpdate
 * 0x8013D200, verbatim */
void ftCommonRebirthDownProcUpdate(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    ftCommonRebirthCommonUpdateHaloWait(fighter_gobj);

    if (fp->status_vars.common.rebirth.halo_despawn_wait == (FTCOMMON_REBIRTH_HALO_DESPAWN_WAIT - FTCOMMON_REBIRTH_HALO_UNK_WAIT))
    {
        fp->camera_mode = nFTCameraModeDefault;
    }
    if (fp->status_vars.common.rebirth.halo_despawn_wait == (FTCOMMON_REBIRTH_HALO_DESPAWN_WAIT - FTCOMMON_REBIRTH_HALO_STAND_WAIT))
    {
        ftCommonRebirthStandSetStatus(fighter_gobj);
    }
}

/* ft/ftcommon/ftcommonrebirth.c:138-143 ftCommonRebirthCommonProcMap
 * 0x8013D264, verbatim: the platform's descent, y as a quadratic in
 * the remaining lower_wait. This is the proc_map of all three Rebirth
 * rows, so it runs instead of the collision walk -- which is why
 * floor_line_id -2 never has to resolve to a real line. */
void ftCommonRebirthCommonProcMap(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    DObjGetStruct(fighter_gobj)->translate.vec.f.y =
        (((fp->status_vars.common.rebirth.pos.y - fp->status_vars.common.rebirth.halo_offset.y) / 8100.0F) *
         SQUARE(fp->status_vars.common.rebirth.halo_lower_wait)) + fp->status_vars.common.rebirth.halo_offset.y;
}

/* ft/ftcommon/ftcommonrebirth.c:146-150 ftCommonRebirthStandProcUpdate
 * 0x8013D2AC, verbatim */
void ftCommonRebirthStandProcUpdate(GObj *fighter_gobj)
{
    ftCommonRebirthCommonUpdateHaloWait(fighter_gobj);
    ftAnimEndCheckSetStatus(fighter_gobj, ftCommonRebirthWaitSetStatus);
}

/* ft/ftcommon/ftcommonrebirth.c:153-165 ftCommonRebirthStandSetStatus
 * 0x8013D2DC, verbatim */
void ftCommonRebirthStandSetStatus(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    ftMainSetStatus(fighter_gobj, nFTCommonStatusRebirthStand, 0.0F, 1.0F, (FTSTATUS_PRESERVE_PLAYERTAG | FTSTATUS_PRESERVE_EFFECT | FTSTATUS_PRESERVE_COLANIM));
    ftMainPlayAnimEventsAll(fighter_gobj);

    fp->is_menu_ignore = TRUE;
    fp->is_ghost = TRUE;
    fp->is_shadow_hide = TRUE;
    fp->is_rebirth = TRUE;

    fp->camera_zoom_range = 0.6F;
}

/* ft/ftcommon/ftcommonrebirth.c:168-180 ftCommonRebirthWaitProcUpdate
 * 0x8013D358, verbatim: the drop off the platform. ftCommonFallSetStatus
 * clears is_ghost and is_rebirth through ftMainSetStatus, so the
 * fighter is a target again the moment it leaves. */
void ftCommonRebirthWaitProcUpdate(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    ftCommonRebirthCommonUpdateHaloWait(fighter_gobj);

    if (fp->status_vars.common.rebirth.halo_despawn_wait == 0)
    {
        ftParamSetTimedHitStatusInvincible(fp, FTCOMMON_REBIRTH_INVINCIBLE_FRAMES);
        ftCommonFallSetStatus(fighter_gobj);
    }
}

/* ft/fighter.h:47-70 ftCommonGroundCheckInterrupt as a function, in the
 * game's order: the three specials, the grab, the smashes, tilts and jab
 * (ftCommonAttackCheckInterruptGround), then GuardOn, Appeal, KneeBend,
 * Dash, Pass, Dokan, Squat, Turn, Walk. It is a function and not the
 * macro because ftCommonRebirthWaitProcInterrupt is the one caller that
 * wants the value rather than the side effect. */
static sb32 ftCommonCheckInterruptGroundPorted(GObj *fighter_gobj)
{
    return (ftCommonSpecialNCheckInterruptCommon(fighter_gobj) != FALSE) ||
           (ftCommonSpecialHiCheckInterruptCommon(fighter_gobj) != FALSE) ||
           (ftCommonSpecialLwCheckInterruptCommon(fighter_gobj) != FALSE) ||
           (ftCommonCatchCheckInterruptCommon(fighter_gobj) != FALSE)     ||
           (ftCommonAttackCheckInterruptGround(fighter_gobj, FTCOMMON_GROUNDATTACK_COMMON) != FALSE)  ||
           (ftCommonGuardOnCheckInterruptCommon(fighter_gobj) != FALSE)  ||
           (ftCommonAppealCheckInterruptCommon(fighter_gobj) != FALSE)   ||
           (ftCommonKneeBendCheckInterruptCommon(fighter_gobj) != FALSE) ||
           (ftCommonDashCheckInterruptCommon(fighter_gobj) != FALSE)     ||
           (ftCommonPassCheckInterruptCommon(fighter_gobj) != FALSE)     ||
           (ftCommonDokanStartCheckInterruptCommon(fighter_gobj) != FALSE) ||
           (ftCommonSquatCheckInterruptCommon(fighter_gobj) != FALSE)    ||
           (ftCommonTurnCheckInterruptCommon(fighter_gobj) != FALSE)     ||
           (ftCommonWalkCheckInterruptCommon(fighter_gobj) != FALSE);
}

/* ft/ftcommon/ftcommonrebirth.c:183-191 ftCommonRebirthWaitProcInterrupt
 * 0x8013D3A4, verbatim: stepping off the platform under control keeps
 * the invincibility. */
void ftCommonRebirthWaitProcInterrupt(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if (ftCommonCheckInterruptGroundPorted(fighter_gobj))
    {
        ftParamSetTimedHitStatusInvincible(fp, FTCOMMON_REBIRTH_INVINCIBLE_FRAMES);
    }
}

/* ft/ftcommon/ftcommonrebirth.c:194-209 ftCommonRebirthWaitSetStatus
 * 0x8013D518, verbatim */
void ftCommonRebirthWaitSetStatus(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    ftMainSetStatus(fighter_gobj, nFTCommonStatusRebirthWait, 0.0F, 1.0F, (FTSTATUS_PRESERVE_PLAYERTAG | FTSTATUS_PRESERVE_EFFECT | FTSTATUS_PRESERVE_COLANIM));

    fp->is_ghost = TRUE;
    fp->is_shadow_hide = TRUE;
    fp->is_rebirth = TRUE;

    fp->camera_zoom_range = 0.6F;
}

/* ---- ft/ftcommon/ftcommonstatus.h dFTCommonNullStatusDescs, statuses
 * 0-5: the four deaths, Sleep, and Entry.
 * Entry is the invisible wait at the spawn point, playing Wait until
 * the entry focus starts the Appear; the four deaths and Sleep are the
 * KO. DeadDown and DeadLeftRight play no animation at all (script id
 * -1) -- the fighter is invisible for the whole 45 tics. ------------ */
FTStatusDesc dFTCommonNullStatusDescs[nFTCommonStatusActionStart] = {
    [nFTCommonStatusDeadDown] = { { -1, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonDeadCommonProcUpdate, NULL, NULL, NULL },
    [nFTCommonStatusDeadLeftRight] = { { -1, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonDeadCommonProcUpdate, NULL, NULL, NULL },
    [nFTCommonStatusDeadUpStar] = { { nFTCommonMotionDamageFall, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonDeadUpStarProcUpdate, NULL, NULL, NULL },
    [nFTCommonStatusDeadUpFall] = { { nFTCommonMotionDamageFall, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonDeadUpFallProcUpdate, NULL, NULL, NULL },
    [nFTCommonStatusSleep] = { { -1, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonSleepProcUpdate, NULL, NULL, NULL },
    [nFTCommonStatusEntry] = { { nFTCommonMotionWait, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } }, NULL, NULL, NULL, NULL },
};

/* ---- ft/ftchar/ftmario/ftmariostatus.h dFTMarioSpecialStatusDescs,
 * statuses 220-228, now COMPLETE. Attack13 (220), AppearR/L (221-222),
 * the fireball SpecialN/SpecialAirN (223-224,), the Up-B
 * SpecialHi/SpecialAirHi (225-226,) and the Tornado (227-228,
 *). Mario's four special moves are all table-present. ftmariospecialhi.c
 * is promoted to host+target (its callee closure complete), so naming its
 * procs does not break the host link,
 * and the Up-B rows go live the way the fireball's did in. ------ */
FTStatusDesc dFTMarioSpecialStatusDescs[nFTMarioStatusSpecialAirLw - nFTCommonStatusSpecialStart + 1] = {
    [nFTMarioStatusAttack13 - nFTCommonStatusSpecialStart] = { { nFTMarioMotionAttack13, nFTMotionAttackIDAttack13 }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDAttack13 } },
        ftAnimEndSetWait, NULL, ftPhysicsApplyGroundVelFriction,
        mpCommonSetFighterFallOnEdgeBreak },
    [nFTMarioStatusAppearR - nFTCommonStatusSpecialStart] = { { nFTMarioMotionAppearR, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTMotionAttackIDNone } },
        ftCommonAppearProcUpdate, NULL, ftCommonAppearProcPhysics,
        mpCommonUpdateFighterProjectFloor },
    [nFTMarioStatusAppearL - nFTCommonStatusSpecialStart] = { { nFTMarioMotionAppearL, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTMotionAttackIDNone } },
        ftCommonAppearProcUpdate, NULL, ftCommonAppearProcPhysics,
        mpCommonUpdateFighterProjectFloor },
    /* ftmariostatus.h statuses 223-224: the fireball, both
 * halves. The grounded and airborne rows share ftMarioSpecialNProcUpdate
 * (ftAnimEndCheckSetStatus back to Wait/Fall) and differ only in
 * kinetics, the friction proc, and which ProcMap watches the edge. Both
 * carry Is-projectile TRUE, as the ROM's do -- ftMarioSpecialNProcAccessory
 * spawns the wpMarioFireball. ftmariospecialn.c is compiled unmodified. */
    [nFTMarioStatusSpecialN - nFTCommonStatusSpecialStart] = { { nFTMarioMotionSpecialN, nFTMotionAttackIDSpecialN }, { { 0, FALSE, nMPKineticsGround, TRUE, nFTMotionAttackIDSpecialN } },
        ftMarioSpecialNProcUpdate, NULL, ftPhysicsApplyGroundVelFriction,
        ftMarioSpecialNProcMap },
    [nFTMarioStatusSpecialAirN - nFTCommonStatusSpecialStart] = { { nFTMarioMotionSpecialAirN, nFTMotionAttackIDSpecialN }, { { 0, FALSE, nMPKineticsAir, TRUE, nFTMotionAttackIDSpecialN } },
        ftMarioSpecialNProcUpdate, NULL, ftPhysicsApplyAirVelDrift,
        ftMarioSpecialAirNProcMap },
    /* ftmariostatus.h statuses 225-226: the Super Jump Punch,
 * both halves. Unlike the fireball these carry all four procs -- the
 * shared ftMarioSpecialHiProcUpdate/Interrupt/Physics/Map -- and differ
 * only in kinetics (Ground vs Air). Not a projectile. ProcMap routes to
 * ftCommonFallSpecialSetStatus for the helpless fall after the punch;
 * ftmariospecialhi.c and ftcommonfallspecial.c are compiled unmodified
 * (host+target since). */
    [nFTMarioStatusSpecialHi - nFTCommonStatusSpecialStart] = { { nFTMarioMotionSpecialHi, nFTMotionAttackIDSpecialHi }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTMotionAttackIDSpecialHi } },
        ftMarioSpecialHiProcUpdate, ftMarioSpecialHiProcInterrupt, ftMarioSpecialHiProcPhysics,
        ftMarioSpecialHiProcMap },
    [nFTMarioStatusSpecialAirHi - nFTCommonStatusSpecialStart] = { { nFTMarioMotionSpecialAirHi, nFTMotionAttackIDSpecialHi }, { { 0, FALSE, nMPKineticsAir, FALSE, nFTMotionAttackIDSpecialHi } },
        ftMarioSpecialHiProcUpdate, ftMarioSpecialHiProcInterrupt, ftMarioSpecialHiProcPhysics,
        ftMarioSpecialHiProcMap },
    /* ftmariostatus.h statuses 227-228, verbatim -- including the status
 * attack id, which in the ROM's table is the *motion* enum's
 * nFTMotionAttackIDSpecialLw rather than nFTStatusAttackIDSpecialLw.
 * The two happen to be the same number; the decomp writes what the
 * bytes say and so does this. */
    [nFTMarioStatusSpecialLw - nFTCommonStatusSpecialStart] = { { nFTMarioMotionSpecialLw, nFTMotionAttackIDSpecialLw }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTMotionAttackIDSpecialLw } },
        ftMarioSpecialLwProcUpdate, NULL, ftMarioSpecialLwProcPhysics,
        ftMarioSpecialLwProcMap },
    [nFTMarioStatusSpecialAirLw - nFTCommonStatusSpecialStart] = { { nFTMarioMotionSpecialAirLw, nFTMotionAttackIDSpecialLw }, { { 0, FALSE, nMPKineticsAir, FALSE, nFTMotionAttackIDSpecialLw } },
        ftMarioSpecialAirLwProcUpdate, NULL, ftMarioSpecialAirLwProcPhysics,
        ftMarioSpecialAirLwProcMap },
};

/* ---- ft/ftchar/ftboss/ftbossstatus.h dFTBossSpecialStatusDescs,
 * statuses 220-252, complete.
 * Every one of its 33 rows is here, transcribed by
 * tools/scratch/genboss.py off the decomp's own header rather than by
 * hand, and every one of the 44 ftBoss* procs they name is defined --
 * steps 24, 25 and 27 put the 36 files of ft/ftchar/ftboss/ and
 * wp/wpboss/ in, compiled unmodified.
 *
 * With this row, dFTMainSpecialStatusDescs above has no FT_SPECIAL_NONE
 * left: Master Hand was the last kind on it. The comment there explains
 * why his was the one that could not be cleared on evidence -- five of
 * his six special demux slots hold MARIO's setters, the shape that made
 * Luigi's NONE row wrong -- and it is moot now that he
 * has his own table.
 *
 * 24 of the 33 share mpCommonUpdateFighterProjectFloor for Proc Map,
 * one reaches the port's own ftCommonAppearProcUpdate, and the rest are
 * ftBoss* one apiece. Nothing here is a stand-in. ------------------- */
FTStatusDesc dFTBossSpecialStatusDescs[nFTBossStatusAppear - nFTCommonStatusSpecialStart + 1] = {
    [nFTBossStatusDefault - nFTCommonStatusSpecialStart] =
        { { nFTBossMotionDefault, nFTMotionAttackIDNone },
          { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
          NULL, ftBossDefaultProcInterrupt,
          NULL, mpCommonUpdateFighterProjectFloor },
    [nFTBossStatusWait - nFTCommonStatusSpecialStart] =
        { { nFTBossMotionDefault, nFTMotionAttackIDNone },
          { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
          NULL, ftBossWaitProcInterrupt,
          ftBossWaitProcPhysics, mpCommonUpdateFighterProjectFloor },
    [nFTBossStatusMove - nFTCommonStatusSpecialStart] =
        { { nFTBossMotionDefault, nFTMotionAttackIDNone },
          { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
          NULL, NULL,
          ftBossMoveProcPhysics, ftBossMoveProcMap },
    [nFTBossStatusHippataku - nFTCommonStatusSpecialStart] =
        { { nFTBossMotionHippataku, nFTMotionAttackIDNone },
          { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
          ftBossHippatakuProcUpdate, NULL,
          ftPhysicsApplyAirVelTransNAll, mpCommonUpdateFighterProjectFloor },
    [nFTBossStatusHarau - nFTCommonStatusSpecialStart] =
        { { nFTBossMotionHarau, nFTMotionAttackIDNone },
          { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
          ftBossHarauProcUpdate, NULL,
          ftBossHarauProcPhysics, mpCommonUpdateFighterProjectFloor },
    [nFTBossStatusOkuhikouki1 - nFTCommonStatusSpecialStart] =
        { { nFTBossMotionOkuhikouki1, nFTMotionAttackIDNone },
          { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
          ftBossOkuhikouki1ProcUpdate, NULL,
          ftPhysicsApplyAirVelTransNAll, mpCommonUpdateFighterProjectFloor },
    [nFTBossStatusOkuhikouki2 - nFTCommonStatusSpecialStart] =
        { { nFTBossMotionOkuhikouki2, nFTMotionAttackIDNone },
          { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
          ftBossOkuhikouki2ProcUpdate, NULL,
          ftBossOkuhikouki2ProcPhysics, mpCommonUpdateFighterProjectFloor },
    [nFTBossStatusOkuhikouki3 - nFTCommonStatusSpecialStart] =
        { { nFTBossMotionOkuhikouki3, nFTMotionAttackIDNone },
          { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
          ftBossOkuhikouki3ProcUpdate, NULL,
          ftPhysicsApplyAirVelTransNAll, mpCommonUpdateFighterProjectFloor },
    [nFTBossStatusWalk - nFTCommonStatusSpecialStart] =
        { { nFTBossMotionWalk, nFTMotionAttackIDNone },
          { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
          ftBossWalkProcUpdate, NULL,
          NULL, NULL },
    [nFTBossStatusWalkLoop - nFTCommonStatusSpecialStart] =
        { { nFTBossMotionWalkLoop, nFTMotionAttackIDNone },
          { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
          NULL, NULL,
          ftBossWalkLoopProcPhysics, ftBossWalkLoopProcMap },
    [nFTBossStatusWalkWait - nFTCommonStatusSpecialStart] =
        { { nFTBossMotionWalkWait, nFTMotionAttackIDNone },
          { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
          ftBossWalkWaitProcUpdate, NULL,
          NULL, mpCommonUpdateFighterProjectFloor },
    [nFTBossStatusWalkShoot - nFTCommonStatusSpecialStart] =
        { { nFTBossMotionWalkShoot, nFTMotionAttackIDNone },
          { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
          ftBossWalkShootProcUpdate, NULL,
          NULL, mpCommonUpdateFighterProjectFloor },
    [nFTBossStatusGootsubusuUp - nFTCommonStatusSpecialStart] =
        { { nFTBossMotionGootsubusuUp, nFTMotionAttackIDNone },
          { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
          NULL, NULL,
          ftBossGootsubusuUpProcPhysics, ftBossGootsubusuUpProcMap },
    [nFTBossStatusGootsubusuWait - nFTCommonStatusSpecialStart] =
        { { nFTBossMotionGootsubusuWait, nFTMotionAttackIDNone },
          { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
          NULL, NULL,
          ftBossGootsubusuWaitProcPhysics, ftBossGootsubusuWaitProcMap },
    [nFTBossStatusGootsubusuEnd - nFTCommonStatusSpecialStart] =
        { { nFTBossMotionGootsubusuEnd, nFTMotionAttackIDNone },
          { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
          ftBossGootsubusuEndProcUpdate, NULL,
          ftPhysicsApplyAirVelTransNAll, mpCommonUpdateFighterProjectFloor },
    [nFTBossStatusGootsubusuDown - nFTCommonStatusSpecialStart] =
        { { nFTBossMotionGootsubusuDown, nFTMotionAttackIDNone },
          { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
          NULL, NULL,
          NULL, ftBossGootsubusuDownProcMap },
    [nFTBossStatusTsutsuku1 - nFTCommonStatusSpecialStart] =
        { { nFTBossMotionTsutsuku1, nFTMotionAttackIDNone },
          { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
          ftBossTsutsuku1ProcUpdate, NULL,
          ftPhysicsApplyAirVelTransNAll, mpCommonUpdateFighterProjectFloor },
    [nFTBossStatusTsutsuku3 - nFTCommonStatusSpecialStart] =
        { { nFTBossMotionTsutsuku3, nFTMotionAttackIDNone },
          { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
          ftBossTsutsuku3ProcUpdate, NULL,
          ftPhysicsApplyAirVelTransNAll, mpCommonUpdateFighterProjectFloor },
    [nFTBossStatusTsutsuku2 - nFTCommonStatusSpecialStart] =
        { { nFTBossMotionTsutsuku2, nFTMotionAttackIDNone },
          { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
          NULL, NULL,
          ftBossTsutsuku2ProcPhysics, mpCommonUpdateFighterProjectFloor },
    [nFTBossStatusDrill - nFTCommonStatusSpecialStart] =
        { { nFTBossMotionDrill, nFTMotionAttackIDNone },
          { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
          ftBossDrillProcUpdate, NULL,
          ftBossDrillProcPhysics, ftBossDrillProcMap },
    [nFTBossStatusOkukouki - nFTCommonStatusSpecialStart] =
        { { nFTBossMotionOkukouki, nFTMotionAttackIDNone },
          { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
          ftBossOkukoukiProcUpdate, NULL,
          ftPhysicsApplyAirVelTransNAll, mpCommonUpdateFighterProjectFloor },
    [nFTBossStatusYubideppou1 - nFTCommonStatusSpecialStart] =
        { { nFTBossMotionYubideppou1, nFTMotionAttackIDNone },
          { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
          ftBossYubideppou1ProcUpdate, NULL,
          ftPhysicsApplyAirVelTransNAll, mpCommonUpdateFighterProjectFloor },
    [nFTBossStatusYubideppou3 - nFTCommonStatusSpecialStart] =
        { { nFTBossMotionYubideppou3, nFTMotionAttackIDNone },
          { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
          ftBossYubideppou3ProcUpdate, NULL,
          ftBossYubideppou3ProcPhysics, mpCommonUpdateFighterProjectFloor },
    [nFTBossStatusYubideppou2 - nFTCommonStatusSpecialStart] =
        { { nFTBossMotionYubideppou2, nFTMotionAttackIDNone },
          { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
          NULL, NULL,
          ftBossYubideppou2ProcPhysics, mpCommonUpdateFighterProjectFloor },
    [nFTBossStatusOkupunch1 - nFTCommonStatusSpecialStart] =
        { { nFTBossMotionOkupunch1, nFTMotionAttackIDNone },
          { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
          ftBossOkupunch1ProcUpdate, NULL,
          ftPhysicsApplyAirVelTransNYZ, mpCommonUpdateFighterProjectFloor },
    [nFTBossStatusOkupunch2 - nFTCommonStatusSpecialStart] =
        { { nFTBossMotionOkupunch2, nFTMotionAttackIDNone },
          { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
          ftBossOkupunch2ProcUpdate, NULL,
          ftBossOkupunch2ProcPhysics, mpCommonUpdateFighterProjectFloor },
    [nFTBossStatusOkupunch3 - nFTCommonStatusSpecialStart] =
        { { nFTBossMotionOkupunch3, nFTMotionAttackIDNone },
          { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
          ftBossOkupunch3ProcUpdate, NULL,
          ftPhysicsApplyAirVelTransNYZ, mpCommonUpdateFighterProjectFloor },
    [nFTBossStatusOkutsubushi - nFTCommonStatusSpecialStart] =
        { { nFTBossMotionOkutsubushi, nFTMotionAttackIDNone },
          { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
          ftBossOkutsubushiProcUpdate, NULL,
          ftBossOkutsubushiProcPhysics, mpCommonUpdateFighterProjectFloor },
    [nFTBossStatusOkutsubushiStart - nFTCommonStatusSpecialStart] =
        { { nFTBossMotionOkupunch1, nFTMotionAttackIDNone },
          { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
          ftBossOkutsubushiStartProcUpdate, NULL,
          ftPhysicsApplyAirVelTransNYZ, mpCommonUpdateFighterProjectFloor },
    [nFTBossStatusDeadLeft - nFTCommonStatusSpecialStart] =
        { { nFTBossMotionDeadLeft, nFTMotionAttackIDNone },
          { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
          ftBossDeadLeftProcUpdate, NULL,
          NULL, NULL },
    [nFTBossStatusDeadCenter - nFTCommonStatusSpecialStart] =
        { { nFTBossMotionDeadCenter, nFTMotionAttackIDNone },
          { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
          NULL, NULL,
          ftBossDeadCenterProcPhysics, mpCommonUpdateFighterProjectFloor },
    [nFTBossStatusDeadRight - nFTCommonStatusSpecialStart] =
        { { nFTBossMotionDeadRight, nFTMotionAttackIDNone },
          { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
          ftBossDeadLeftProcUpdate, NULL,
          NULL, NULL },
    [nFTBossStatusAppear - nFTCommonStatusSpecialStart] =
        { { nFTBossMotionAppear, nFTMotionAttackIDNone },
          { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
          ftCommonAppearProcUpdate, NULL,
          ftBossAppearProcPhysics, mpCommonUpdateFighterProjectFloor },
};

/* ---- ft/ftchar/ftdonkey/ftdonkeystatus.h dFTDonkeySpecialStatusDescs,
 * statuses 222-234: Giant Punch, Spinning Kong and the Hand Slap --
 * Donkey's whole special-move set, the third
 * fighter's after Mario's and Purin's. Every row verbatim; SpecialHi/AirHi's
 * own procs (ftDonkeySpecialHiProcUpdate etc.) come from ftdonkeyspecialhi.c,
 * compiled unmodified, the same shape as ftDonkeySpecialLw*'s own. The
 * array was sized through SpecialLwEnd from the start, so neither step's
 * rows needed a resize, only initializer entries; rows 220-221 (AppearR/L)
 * are the only holes left.
 *
 * Giant Punch is eight rows -- Start, Loop, End and Full, ground and air
 * each -- and twenty-four procs in ftdonkeyspecialn.c, with no weapon, no
 * it/, and no new leaf: the charge is passive_vars.donkey.charge_level
 * (it survives across statuses and resets on damage through
 * fp->proc_damage, which the setters assign and ftParamProcDamage calls),
 * the bonus damage goes onto fp->attack_colls in ftDonkeySpecialNEndProc-
 * Update, and the only visual is ftParamCheckSetFighterColAnimID's full-charge
 * colanim. All eight motions
 * are FTANIM_FLAG_NONE in ft/ftdata.c (GiantPunch* rows 1592-1599), so no
 * host mock-motion entry is needed. The End/Full pairs share every proc
 * column: End vs Full is decided by charge level in the setter, and the
 * same ftDonkeySpecialNEndProcUpdate serves both, reading status_id to
 * know which. */
FTStatusDesc dFTDonkeySpecialStatusDescs[nFTDonkeyStatusHeavyThrowB4 - nFTCommonStatusSpecialStart + 1] = {
    /* 220 DonkeyAppearR */
    [nFTDonkeyStatusAppearR - nFTCommonStatusSpecialStart] = { { nFTDonkeyMotionAppearR, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonAppearProcUpdate, NULL,
        ftCommonAppearProcPhysics, mpCommonUpdateFighterProjectFloor },
    /* 221 DonkeyAppearL */
    [nFTDonkeyStatusAppearL - nFTCommonStatusSpecialStart] = { { nFTDonkeyMotionAppearL, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonAppearProcUpdate, NULL,
        ftCommonAppearProcPhysics, mpCommonUpdateFighterProjectFloor },
    [nFTDonkeyStatusSpecialNStart - nFTCommonStatusSpecialStart] = { { nFTDonkeyMotionSpecialNStart, nFTMotionAttackIDSpecialN }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDSpecialN } },
        ftDonkeySpecialNStartProcUpdate, ftDonkeySpecialNStartProcInterrupt,
        ftPhysicsApplyGroundVelFriction, ftDonkeySpecialNStartProcMap },
    [nFTDonkeyStatusSpecialAirNStart - nFTCommonStatusSpecialStart] = { { nFTDonkeyMotionSpecialAirNStart, nFTMotionAttackIDSpecialN }, { { 0, FALSE, nMPKineticsAir, FALSE, nFTStatusAttackIDSpecialN } },
        ftDonkeySpecialAirNStartProcUpdate, ftDonkeySpecialNStartProcInterrupt,
        ftPhysicsApplyAirVelFriction, ftDonkeySpecialAirNStartProcMap },
    [nFTDonkeyStatusSpecialNLoop - nFTCommonStatusSpecialStart] = { { nFTDonkeyMotionSpecialNLoop, nFTMotionAttackIDSpecialN }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDSpecialN } },
        ftDonkeySpecialNLoopProcUpdate, ftDonkeySpecialNLoopProcInterrupt,
        ftPhysicsApplyGroundVelFriction, ftDonkeySpecialNLoopProcMap },
    [nFTDonkeyStatusSpecialAirNLoop - nFTCommonStatusSpecialStart] = { { nFTDonkeyMotionSpecialAirNLoop, nFTMotionAttackIDSpecialN }, { { 0, FALSE, nMPKineticsAir, FALSE, nFTStatusAttackIDSpecialN } },
        ftDonkeySpecialNLoopProcUpdate, ftDonkeySpecialNLoopProcInterrupt,
        ftPhysicsApplyAirVelFriction, ftDonkeySpecialAirNLoopProcMap },
    [nFTDonkeyStatusSpecialNEnd - nFTCommonStatusSpecialStart] = { { nFTDonkeyMotionSpecialNEnd, nFTMotionAttackIDSpecialN }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDSpecialN } },
        ftDonkeySpecialNEndProcUpdate, NULL, ftPhysicsApplyGroundVelFriction,
        mpCommonSetFighterFallOnEdgeBreak },
    [nFTDonkeyStatusSpecialAirNEnd - nFTCommonStatusSpecialStart] = { { nFTDonkeyMotionSpecialAirNEnd, nFTMotionAttackIDSpecialN }, { { 0, FALSE, nMPKineticsAir, FALSE, nFTStatusAttackIDSpecialN } },
        ftDonkeySpecialNEndProcUpdate, NULL, ftPhysicsApplyAirVelFriction,
        ftDonkeySpecialAirNEndProcMap },
    [nFTDonkeyStatusSpecialNFull - nFTCommonStatusSpecialStart] = { { nFTDonkeyMotionSpecialNFull, nFTMotionAttackIDSpecialN }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDSpecialN } },
        ftDonkeySpecialNEndProcUpdate, NULL, ftPhysicsApplyGroundVelFriction,
        mpCommonSetFighterFallOnEdgeBreak },
    [nFTDonkeyStatusSpecialAirNFull - nFTCommonStatusSpecialStart] = { { nFTDonkeyMotionSpecialAirNFull, nFTMotionAttackIDSpecialN }, { { 0, FALSE, nMPKineticsAir, FALSE, nFTStatusAttackIDSpecialN } },
        ftDonkeySpecialNEndProcUpdate, NULL, ftPhysicsApplyAirVelFriction,
        ftDonkeySpecialAirNEndProcMap },
    [nFTDonkeyStatusSpecialHi - nFTCommonStatusSpecialStart] = { { nFTDonkeyMotionSpecialHi, nFTMotionAttackIDSpecialHi }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDSpecialHi } },
        ftDonkeySpecialHiProcUpdate, NULL, ftDonkeySpecialHiProcPhysics,
        ftDonkeySpecialHiProcMap },
    [nFTDonkeyStatusSpecialAirHi - nFTCommonStatusSpecialStart] = { { nFTDonkeyMotionSpecialAirHi, nFTMotionAttackIDSpecialHi }, { { 0, FALSE, nMPKineticsAir, FALSE, nFTStatusAttackIDSpecialHi } },
        ftDonkeySpecialAirHiProcUpdate, NULL, ftDonkeySpecialAirHiProcPhysics,
        ftDonkeySpecialAirHiProcMap },
    [nFTDonkeyStatusSpecialLwStart - nFTCommonStatusSpecialStart] = { { nFTDonkeyMotionSpecialLwStart, nFTMotionAttackIDSpecialLw }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDSpecialLw } },
        ftDonkeySpecialLwStartProcUpdate, NULL, ftPhysicsApplyGroundVelFriction,
        mpCommonSetFighterFallOnEdgeBreak },
    [nFTDonkeyStatusSpecialLwLoop - nFTCommonStatusSpecialStart] = { { nFTDonkeyMotionSpecialLwLoop, nFTMotionAttackIDSpecialLw }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDSpecialLw } },
        ftDonkeySpecialLwLoopProcUpdate, ftDonkeySpecialLwLoopProcInterrupt,
        ftPhysicsApplyGroundVelFriction, mpCommonSetFighterFallOnEdgeBreak },
    [nFTDonkeyStatusSpecialLwEnd - nFTCommonStatusSpecialStart] = { { nFTDonkeyMotionSpecialLwEnd, nFTMotionAttackIDSpecialLw }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDSpecialLw } },
        ftAnimEndSetWait, NULL, ftPhysicsApplyGroundVelFriction,
        mpCommonSetFighterFallOnEdgeBreak },

    /* ---- 235-245, the CARGO THROW ----
 *
 * Donkey Kong's forward throw does not throw: it picks the caught
 * fighter up and carries him, and these eleven statuses are the
 * whole of what he can do while carrying -- stand, three walking
 * speeds, turn, jump, fall, land, take a hit, and the two that
 * finally throw (ground and air). Eight decomp files, 596 lines,
 * every one compiling unmodified.
 *
 * Every row carries nFTStatusAttackIDThrowF and nMPKineticsGround,
 * the last including ThrowFFall and ThrowAirFF -- the game marks
 * them Ground and their physics say otherwise
 * (ftPhysicsApplyAirVelDriftFastFall, ftPhysicsApplyAirVelDrift).
 * That is the game's own, and it is the third fighter table here to
 * carry a ground/air marking its physics contradicts. ---- */
    /* 235 DonkeyThrowFWait */
    [nFTDonkeyStatusThrowFWait - nFTCommonStatusSpecialStart] = { { nFTDonkeyMotionThrowFWait, nFTMotionAttackIDThrowF }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDThrowF } },
        NULL, ftDonkeyThrowFWaitProcInterrupt,
        ftPhysicsApplyGroundVelFriction, ftDonkeyThrowFCommonProcMap },
    /* 236 DonkeyThrowFWalkSlow */
    [nFTDonkeyStatusThrowFWalkSlow - nFTCommonStatusSpecialStart] = { { nFTDonkeyMotionThrowFWalkSlow, nFTMotionAttackIDThrowF }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDThrowF } },
        NULL, ftDonkeyThrowFWalkProcInterrupt,
        ftCommonWalkProcPhysics, ftDonkeyThrowFCommonProcMap },
    /* 237 DonkeyThrowFWalkMiddle */
    [nFTDonkeyStatusThrowFWalkMiddle - nFTCommonStatusSpecialStart] = { { nFTDonkeyMotionThrowFWalkMiddle, nFTMotionAttackIDThrowF }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDThrowF } },
        NULL, ftDonkeyThrowFWalkProcInterrupt,
        ftCommonWalkProcPhysics, ftDonkeyThrowFCommonProcMap },
    /* 238 DonkeyThrowFWalkFast */
    [nFTDonkeyStatusThrowFWalkFast - nFTCommonStatusSpecialStart] = { { nFTDonkeyMotionThrowFWalkFast, nFTMotionAttackIDThrowF }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDThrowF } },
        NULL, ftDonkeyThrowFWalkProcInterrupt,
        ftCommonWalkProcPhysics, ftDonkeyThrowFCommonProcMap },
    /* 239 DonkeyThrowFTurn */
    [nFTDonkeyStatusThrowFTurn - nFTCommonStatusSpecialStart] = { { nFTDonkeyMotionThrowFTurn, nFTMotionAttackIDThrowF }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDThrowF } },
        ftDonkeyThrowFTurnProcUpdate, ftDonkeyThrowFTurnProcInterrupt,
        ftPhysicsApplyGroundVelFriction, ftDonkeyThrowFCommonProcMap },
    /* 240 DonkeyThrowFKneeBend */
    [nFTDonkeyStatusThrowFKneeBend - nFTCommonStatusSpecialStart] = { { nFTDonkeyMotionThrowFKneeBend, nFTMotionAttackIDThrowF }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDThrowF } },
        ftDonkeyThrowFKneeBendProcUpdate, ftDonkeyThrowFKneeBendProcInterrupt,
        ftPhysicsApplyGroundVelFriction, ftDonkeyThrowFCommonProcMap },
    /* 241 DonkeyThrowFFall */
    [nFTDonkeyStatusThrowFFall - nFTCommonStatusSpecialStart] = { { nFTDonkeyMotionThrowFFall, nFTMotionAttackIDThrowF }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDThrowF } },
        NULL, ftDonkeyThrowFFallProcInterrupt,
        ftPhysicsApplyAirVelDriftFastFall, ftDonkeyThrowFFallProcMap },
    /* 242 DonkeyThrowFLanding */
    [nFTDonkeyStatusThrowFLanding - nFTCommonStatusSpecialStart] = { { nFTDonkeyMotionThrowFLanding, nFTMotionAttackIDThrowF }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDThrowF } },
        ftDonkeyThrowFLandingProcUpdate, NULL,
        ftPhysicsApplyGroundVelFriction, ftDonkeyThrowFCommonProcMap },
    /* 243 DonkeyThrowFDamage */
    [nFTDonkeyStatusThrowFDamage - nFTCommonStatusSpecialStart] = { { nFTDonkeyMotionThrowFDamage, nFTMotionAttackIDThrowF }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDThrowF } },
        ftDonkeyThrowFDamageProcUpdate, NULL,
        ftCommonDamageCommonProcPhysics, mpCommonUpdateFighterKinetics },
    /* 244 DonkeyThrowFF */
    [nFTDonkeyStatusThrowFF - nFTCommonStatusSpecialStart] = { { nFTDonkeyMotionThrowFF, nFTMotionAttackIDThrowF }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDThrowF } },
        ftDonkeyThrowFFProcUpdate, NULL,
        ftPhysicsApplyGroundVelFriction, ftDonkeyThrowFFProcMap },
    /* 245 DonkeyThrowAirFF */
    [nFTDonkeyStatusThrowAirFF - nFTCommonStatusSpecialStart] = { { nFTDonkeyMotionThrowAirFF, nFTMotionAttackIDThrowF }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDThrowF } },
        ftDonkeyThrowFFProcUpdate, NULL,
        ftPhysicsApplyAirVelDrift, ftDonkeyThrowAirFFProcMap },
    /* ftdonkeystatus.h statuses 246-249: the heavy item's throws
 * (ftcommonitemthrow.c). The motion IDs are the common ones, as the
 * game's are; F4 and B4 are the smash throws. */
    [nFTDonkeyStatusHeavyThrowF - nFTCommonStatusSpecialStart] = { { nFTCommonMotionHeavyThrowF, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDItemThrow } },
        ftCommonItemThrowProcUpdate, NULL,
        ftCommonItemThrowProcPhysics, mpCommonUpdateFighterKinetics },
    [nFTDonkeyStatusHeavyThrowB - nFTCommonStatusSpecialStart] = { { nFTCommonMotionHeavyThrowB, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDItemThrow } },
        ftCommonItemThrowProcUpdate, NULL,
        ftCommonItemThrowProcPhysics, mpCommonUpdateFighterKinetics },
    [nFTDonkeyStatusHeavyThrowF4 - nFTCommonStatusSpecialStart] = { { nFTCommonMotionHeavyThrowF4, nFTMotionAttackIDNone }, { { 0, TRUE, nMPKineticsGround, FALSE, nFTStatusAttackIDItemThrow } },
        ftCommonItemThrowProcUpdate, NULL,
        ftCommonItemThrowProcPhysics, mpCommonUpdateFighterKinetics },
    [nFTDonkeyStatusHeavyThrowB4 - nFTCommonStatusSpecialStart] = { { nFTCommonMotionHeavyThrowB4, nFTMotionAttackIDNone }, { { 0, TRUE, nMPKineticsGround, FALSE, nFTStatusAttackIDItemThrow } },
        ftCommonItemThrowProcUpdate, NULL,
        ftCommonItemThrowProcPhysics, mpCommonUpdateFighterKinetics },
};

/* ---- ft/ftchar/ftpurin/ftpurinstatus.h dFTPurinSpecialStatusDescs,
 * statuses 230-235: Rollout then Rest. ftpurinspecialn.c's
 * nine procs were compiled unmodified's sweep -- what
 * was missing was never the code, only these two rows and the demux
 * table entry below (ftCommonSpecialNCheckInterruptCommon and its wiring
 * into all eight ground cascades are the Neutral-B family's shared
 * infrastructure. Purin needed only her own setter named in the one table
 * both dispatch
 * through). SpecialN's grounded ProcPhysics is the same
 * ftPhysicsApplyGroundVelTransN Mario's Up-B reads ('s ASan
 * find): the host mock's motion table carries an entry for Purin's
 * SpecialN motion id for the same reason. Both rows name
 * nFTPurinMotionSpecialLw for Rest -- the grounded and airborne halves
 * play the same animation, and the only thing that differs is which way
 * the fighter is falling and which of ftpurinspeciallw.c's two ProcMaps
 * watches for the change.
 *
 * Statuses 232-233 are Sing: ftpurinspecialhi.c's six procs,
 * compiled unmodified, finish Purin's whole special-move set -- Neutral-B
 * and Down-B were already live. Both rows share ftPurinSpecialHiProcUpdate
 * and differ only in kinetics/ProcPhysics/ProcMap, the same shape every
 * ground/air special pair in this file has. ProcUpdate's own branch (on
 * fp->motion_vars.flags.flag1) calls efManagerPurinSingMakeEffect
 * (src/dc/efmanager.c) to attach the note cloud (efsing.mdl). */
FTStatusDesc dFTPurinSpecialStatusDescs[nFTPurinStatusSpecialAirLw - nFTCommonStatusSpecialStart + 1] = {
    /* the rapid jab: three rows, the procs common and the
 * motion this fighter's own. */
    /* Purin Attack100Start */
    [nFTPurinStatusAttack100Start - nFTCommonStatusSpecialStart] = { { nFTPurinMotionAttack100Start, nFTMotionAttackIDAttack100 }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDAttack100 } },
        ftCommonAttack100StartProcUpdate, NULL,
        ftPhysicsApplyGroundVelFriction, mpCommonSetFighterFallOnEdgeBreak },
    /* Purin Attack100Loop */
    [nFTPurinStatusAttack100Loop - nFTCommonStatusSpecialStart] = { { nFTPurinMotionAttack100Loop, nFTMotionAttackIDAttack100 }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDAttack100 } },
        ftCommonAttack100LoopProcUpdate, ftCommonAttack100LoopProcInterrupt,
        ftPhysicsApplyGroundVelFriction, mpCommonSetFighterFallOnEdgeBreak },
    /* Purin Attack100End */
    [nFTPurinStatusAttack100End - nFTCommonStatusSpecialStart] = { { nFTPurinMotionAttack100End, nFTMotionAttackIDAttack100 }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDAttack100 } },
        ftAnimEndSetWait, NULL,
        ftPhysicsApplyGroundVelFriction, mpCommonSetFighterFallOnEdgeBreak },
    /* ftpurinstatus.h statuses 223-227, the midair jumps: one row per
 * jump, the procs ftcommonjumpaerial.c's. */
    [nFTPurinStatusJumpAerialF1 - nFTCommonStatusSpecialStart] = { { nFTPurinMotionJumpAerialF1, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonJumpAerialProcUpdate, ftCommonJumpAerialProcInterrupt,
        ftCommonJumpAerialProcPhysics, mpCommonProcFighterCliffWaitOrLanding },
    [ftStatus_purin_JumpAerialF2 - nFTCommonStatusSpecialStart] = { { nFTPurinMotionJumpAerialF2, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonJumpAerialProcUpdate, ftCommonJumpAerialProcInterrupt,
        ftCommonJumpAerialProcPhysics, mpCommonProcFighterCliffWaitOrLanding },
    [nFTPurinStatusJumpAerialF3 - nFTCommonStatusSpecialStart] = { { nFTPurinMotionJumpAerialF3, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonJumpAerialProcUpdate, ftCommonJumpAerialProcInterrupt,
        ftCommonJumpAerialProcPhysics, mpCommonProcFighterCliffWaitOrLanding },
    [nFTPurinStatusJumpAerialF4 - nFTCommonStatusSpecialStart] = { { nFTPurinMotionJumpAerialF4, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonJumpAerialProcUpdate, ftCommonJumpAerialProcInterrupt,
        ftCommonJumpAerialProcPhysics, mpCommonProcFighterCliffWaitOrLanding },
    [nFTPurinStatusJumpAerialF5 - nFTCommonStatusSpecialStart] = { { nFTPurinMotionJumpAerialF5, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonJumpAerialProcUpdate, ftCommonJumpAerialProcInterrupt,
        ftCommonJumpAerialProcPhysics, mpCommonProcFighterCliffWaitOrLanding },

    /* 228 PurinAppearR */
    [nFTPurinStatusAppearR - nFTCommonStatusSpecialStart] = { { nFTPurinMotionAppearR, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonAppearProcUpdate, NULL,
        ftCommonAppearProcPhysics, mpCommonUpdateFighterProjectFloor },
    /* 229 PurinAppearL */
    [nFTPurinStatusAppearL - nFTCommonStatusSpecialStart] = { { nFTPurinMotionAppearL, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonAppearProcUpdate, NULL,
        ftCommonAppearProcPhysics, mpCommonUpdateFighterProjectFloor },
    [nFTPurinStatusSpecialN - nFTCommonStatusSpecialStart] = { { nFTPurinMotionSpecialN, nFTMotionAttackIDSpecialN }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDSpecialN } },
        ftAnimEndSetWait, NULL, ftPhysicsApplyGroundVelTransN,
        ftPurinSpecialNProcMap },
    [nFTPurinStatusSpecialAirN - nFTCommonStatusSpecialStart] = { { nFTPurinMotionSpecialAirN, nFTMotionAttackIDSpecialN }, { { 0, FALSE, nMPKineticsAir, FALSE, nFTStatusAttackIDSpecialN } },
        ftAnimEndSetFall, NULL, ftPurinSpecialAirNProcPhysics,
        ftPurinSpecialAirNProcMap },
    [nFTPurinStatusSpecialHi - nFTCommonStatusSpecialStart] = { { nFTPurinMotionSpecialHi, nFTMotionAttackIDSpecialHi }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDSpecialHi } },
        ftPurinSpecialHiProcUpdate, NULL, ftPhysicsApplyGroundVelFriction,
        ftPurinSpecialHiProcMap },
    [nFTPurinStatusSpecialAirHi - nFTCommonStatusSpecialStart] = { { nFTPurinMotionSpecialHi, nFTMotionAttackIDSpecialHi }, { { 0, FALSE, nMPKineticsAir, FALSE, nFTStatusAttackIDSpecialHi } },
        ftPurinSpecialHiProcUpdate, NULL, ftPhysicsApplyAirVelFriction,
        ftPurinSpecialAirHiProcMap },
    [nFTPurinStatusSpecialLw - nFTCommonStatusSpecialStart] = { { nFTPurinMotionSpecialLw, nFTMotionAttackIDSpecialLw }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDSpecialLw } },
        ftAnimEndSetWait, NULL, ftPhysicsApplyGroundVelFriction,
        ftPurinSpecialLwProcMap },
    [nFTPurinStatusSpecialAirLw - nFTCommonStatusSpecialStart] = { { nFTPurinMotionSpecialLw, nFTMotionAttackIDSpecialLw }, { { 0, FALSE, nMPKineticsAir, FALSE, nFTStatusAttackIDSpecialLw } },
        ftAnimEndSetFall, NULL, ftPhysicsApplyAirVelFriction,
        ftPurinSpecialAirLwProcMap },
};

/* ---- ft/ftchar/ftkirby/ftkirbystatus.h dFTKirbySpecialStatusDescs.
 * tools/check/status_check.py lists the rows still missing. */
FTStatusDesc dFTKirbySpecialStatusDescs[nFTKirbyStatusCopyYoshiSpecialAirNRelease - nFTCommonStatusSpecialStart + 1] = {
    /* 250 KirbyAppearR */
    [nFTKirbyStatusAppearR - nFTCommonStatusSpecialStart] = { { nFTKirbyMotionAppearR, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonAppearProcUpdate, NULL,
        ftCommonAppearProcPhysics, mpCommonUpdateFighterProjectFloor },
    /* 251 KirbyAppearL */
    [nFTKirbyStatusAppearL - nFTCommonStatusSpecialStart] = { { nFTKirbyMotionAppearL, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonAppearProcUpdate, NULL,
        ftCommonAppearProcPhysics, mpCommonUpdateFighterProjectFloor },
    /* statuses 256-259: the Final Cutter, Kirby's Up-B. All
 * four are is_projectile TRUE -- the blade is the projectile, and the
 * flag is what lets a reflector turn it. SpecialHi and SpecialAirHi
 * share an update (the rise is the same move whichever way it began)
 * and a map proc, and differ only in kinetics and in the physics leaf;
 * SpecialHiLanding is the ground shockwave, and SpecialAirHiFall is
 * the plummet after the blade leaves, the only one of the four with
 * no update at all. Every proc is ftkirbyspecialhi.c's, compiled
 * unmodified, and the four effects its update asks for are baked
 * (romdisk/efcut*.mdl,). */
    [nFTKirbyStatusSpecialHi - nFTCommonStatusSpecialStart] = { { nFTKirbyMotionSpecialHi, nFTMotionAttackIDSpecialHi }, { { 0, FALSE, nMPKineticsGround, TRUE, nFTStatusAttackIDSpecialHi } },
        ftKirbySpecialHiProcUpdate, NULL, ftKirbySpecialHiProcPhysics,
        ftKirbySpecialHiProcMap },
    [nFTKirbyStatusSpecialHiLanding - nFTCommonStatusSpecialStart] = { { nFTKirbyMotionSpecialHiLanding, nFTMotionAttackIDSpecialHi }, { { 0, FALSE, nMPKineticsGround, TRUE, nFTStatusAttackIDSpecialHi } },
        ftKirbySpecialHiLandingProcUpdate, NULL, ftKirbySpecialHiLandingProcPhysics,
        mpCommonSetFighterFallOnGroundBreak },
    [nFTKirbyStatusSpecialAirHi - nFTCommonStatusSpecialStart] = { { nFTKirbyMotionSpecialAirHi, nFTMotionAttackIDSpecialHi }, { { 0, FALSE, nMPKineticsAir, TRUE, nFTStatusAttackIDSpecialHi } },
        ftKirbySpecialHiProcUpdate, NULL, ftKirbySpecialAirHiProcPhysics,
        ftKirbySpecialHiProcMap },
    [nFTKirbyStatusSpecialAirHiFall - nFTCommonStatusSpecialStart] = { { nFTKirbyMotionSpecialAirHiFall, nFTMotionAttackIDSpecialHi }, { { 0, FALSE, nMPKineticsAir, TRUE, nFTStatusAttackIDSpecialHi } },
        NULL, NULL, ftKirbySpecialAirHiFallProcPhysics,
        ftKirbySpecialAirHiFallProcMap },
    [nFTKirbyStatusSpecialLwStart - nFTCommonStatusSpecialStart] = { { nFTKirbyMotionSpecialLwStart, nFTMotionAttackIDSpecialLw }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDSpecialLw } },
        ftKirbySpecialLwStartProcUpdate, NULL, NULL,
        ftKirbySpecialLwStartProcMap },
    [nFTKirbyStatusSpecialLwUnk - nFTCommonStatusSpecialStart] = { { nFTKirbyMotionSpecialLwUnk, nFTMotionAttackIDSpecialLw }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDSpecialLw } },
        ftKirbySpecialLwUnkProcUpdate, NULL, ftPhysicsApplyGroundVelFriction,
        ftKirbySpecialLwUnkProcMap },
    [nFTKirbyStatusSpecialLwHold - nFTCommonStatusSpecialStart] = { { nFTKirbyMotionSpecialLwHold, nFTMotionAttackIDSpecialLw }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDSpecialLw } },
        ftKirbySpecialLwHoldProcUpdate, NULL, ftKirbySpecialLwHoldProcPhysics,
        ftKirbySpecialLwHoldProcMap },
    [nFTKirbyStatusSpecialLwEnd - nFTCommonStatusSpecialStart] = { { nFTKirbyMotionSpecialLwEnd, nFTMotionAttackIDSpecialLw }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDSpecialLw } },
        ftAnimEndSetFall, NULL, ftPhysicsApplyAirVelTransNAll,
        mpCommonProcFighterWaitOrLanding },
    [nFTKirbyStatusSpecialAirLwStart - nFTCommonStatusSpecialStart] = { { nFTKirbyMotionSpecialAirLwStart, nFTMotionAttackIDSpecialLw }, { { 0, FALSE, nMPKineticsAir, FALSE, nFTStatusAttackIDSpecialLw } },
        ftKirbySpecialAirLwStartProcUpdate, NULL, NULL,
        ftKirbySpecialAirLwStartProcMap },
    [nFTKirbyStatusSpecialAirLwHold - nFTCommonStatusSpecialStart] = { { nFTKirbyMotionSpecialAirLwHold, nFTMotionAttackIDSpecialLw }, { { 0, FALSE, nMPKineticsAir, FALSE, nFTStatusAttackIDSpecialLw } },
        ftKirbySpecialAirLwHoldProcUpdate, NULL, NULL,
        ftKirbySpecialAirLwHoldProcMap },
    [nFTKirbyStatusSpecialAirLwLanding - nFTCommonStatusSpecialStart] = { { nFTKirbyMotionSpecialAirLwLanding, nFTMotionAttackIDSpecialLw }, { { 0, FALSE, nMPKineticsAir, FALSE, nFTStatusAttackIDSpecialLw } },
        ftKirbySpecialLwHoldProcUpdate, NULL, ftKirbySpecialLwHoldProcPhysics,
        ftKirbySpecialLwHoldProcMap },
    [nFTKirbyStatusSpecialAirLwFall - nFTCommonStatusSpecialStart] = { { nFTKirbyMotionSpecialAirLwFall, nFTMotionAttackIDSpecialLw }, { { 0, FALSE, nMPKineticsAir, FALSE, nFTStatusAttackIDSpecialLw } },
        ftKirbySpecialAirLwFallProcUpdate, NULL, NULL,
        ftKirbySpecialAirLwHoldProcMap },
    [nFTKirbyStatusSpecialAirLwEnd - nFTCommonStatusSpecialStart] = { { nFTKirbyMotionSpecialAirLwEnd, nFTMotionAttackIDSpecialLw }, { { 0, FALSE, nMPKineticsAir, FALSE, nFTStatusAttackIDSpecialLw } },
        ftAnimEndSetFall, NULL, ftPhysicsApplyAirVelTransNAll,
        mpCommonProcFighterWaitOrLanding },
    /* ---- Kirby's Neutral-B: the inhale -----------------
 * ft/ftchar/ftkirby/ftkirbystatus.h statuses 269-286, eighteen rows,
 * nine on the ground and nine in the air, procs from
 * ft/ftchar/ftkirby/ftkirbyspecialn.c (0x80160000, compiled
 * unmodified). Start -> Loop -> End is the mouth opening, holding
 * and closing; Catch/Eat/Throw/Copy are what happens when the
 * vacuum lands on a fighter; Wait is Kirby standing there with a
 * full mouth, and Turn is him turning around with one. The ground
 * and air nines are the same nine and switch between each other on
 * landing and on walking off, which is why so many rows share a
 * proc across the two halves.
 *
 * Two verbatim quirks worth not "fixing": the two Catch rows carry
 * motion -1 (no script of their own -- the catch keeps whatever the
 * mouth was playing), and SpecialAirNCatch is nMPKineticsGround
 * even though it is the air row, because a caught fighter is being
 * reeled in rather than flown. */
    /* 269 SpecialNStart */
    [nFTKirbyStatusSpecialNStart - nFTCommonStatusSpecialStart] = { { nFTKirbyMotionSpecialNStart, nFTMotionAttackIDSpecialN }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDSpecialN } },
        ftKirbySpecialNStartProcUpdate, NULL,
        ftPhysicsApplyGroundVelFriction, ftKirbySpecialNStartProcMap },
    /* 270 SpecialNLoop */
    [nFTKirbyStatusSpecialNLoop - nFTCommonStatusSpecialStart] = { { nFTKirbyMotionSpecialNLoop, nFTMotionAttackIDSpecialN }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDSpecialN } },
        ftKirbySpecialNLoopProcUpdate, ftKirbySpecialNLoopProcInterrupt,
        ftPhysicsApplyGroundVelFriction, ftKirbySpecialNLoopProcMap },
    /* 271 SpecialNEnd */
    [nFTKirbyStatusSpecialNEnd - nFTCommonStatusSpecialStart] = { { nFTKirbyMotionSpecialNEnd, nFTMotionAttackIDSpecialN }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDSpecialN } },
        ftAnimEndSetWait, NULL,
        ftPhysicsApplyGroundVelFriction, ftKirbySpecialNEndProcMap },
    /* 272 SpecialNCatch */
    [nFTKirbyStatusSpecialNCatch - nFTCommonStatusSpecialStart] = { { -1, nFTMotionAttackIDSpecialN }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDSpecialN } },
        ftKirbySpecialNCatchProcUpdate, NULL,
        ftPhysicsApplyGroundVelFriction, ftKirbySpecialNCatchProcMap },
    /* 273 SpecialNEat */
    [nFTKirbyStatusSpecialNEat - nFTCommonStatusSpecialStart] = { { nFTKirbyMotionSpecialNEat, nFTMotionAttackIDSpecialN }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDSpecialN } },
        ftKirbySpecialNEatProcUpdate, NULL,
        ftPhysicsApplyGroundVelFriction, ftKirbySpecialNEatProcMap },
    /* 274 SpecialNThrow */
    [nFTKirbyStatusSpecialNThrow - nFTCommonStatusSpecialStart] = { { nFTKirbyMotionSpecialNThrow, nFTMotionAttackIDSpecialN }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDSpecialN } },
        ftKirbySpecialNThrowProcUpdate, NULL,
        ftPhysicsApplyGroundVelTransN, ftKirbySpecialNThrowProcMap },
    /* 275 SpecialNWait */
    [nFTKirbyStatusSpecialNWait - nFTCommonStatusSpecialStart] = { { nFTKirbyMotionSpecialNWait, nFTMotionAttackIDSpecialN }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDSpecialN } },
        NULL, ftKirbySpecialNWaitProcInterrupt,
        ftPhysicsApplyGroundVelFriction, ftKirbySpecialNWaitProcMap },
    /* 276 SpecialNTurn */
    [nFTKirbyStatusSpecialNTurn - nFTCommonStatusSpecialStart] = { { nFTKirbyMotionSpecialNTurn, nFTMotionAttackIDSpecialN }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDSpecialN } },
        ftKirbySpecialNTurnProcUpdate, NULL,
        ftPhysicsApplyGroundVelFriction, ftKirbySpecialNTurnProcMap },
    /* 277 SpecialNCopy */
    [nFTKirbyStatusSpecialNCopy - nFTCommonStatusSpecialStart] = { { nFTKirbyMotionSpecialNCopy, nFTMotionAttackIDSpecialN }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDSpecialN } },
        ftKirbySpecialNCopyProcUpdate, NULL,
        ftPhysicsApplyGroundVelFriction, ftKirbySpecialNCopyProcMap },
    /* 278 SpecialAirNStart */
    [nFTKirbyStatusSpecialAirNStart - nFTCommonStatusSpecialStart] = { { nFTKirbyMotionSpecialAirNStart, nFTMotionAttackIDSpecialN }, { { 0, FALSE, nMPKineticsAir, FALSE, nFTStatusAttackIDSpecialN } },
        ftKirbySpecialAirNStartProcUpdate, NULL,
        ftPhysicsApplyAirVelFriction, ftKirbySpecialAirNStartProcMap },
    /* 279 SpecialAirNLoop */
    [nFTKirbyStatusSpecialAirNLoop - nFTCommonStatusSpecialStart] = { { nFTKirbyMotionSpecialAirNLoop, nFTMotionAttackIDSpecialN }, { { 0, FALSE, nMPKineticsAir, FALSE, nFTStatusAttackIDSpecialN } },
        ftKirbySpecialNLoopProcUpdate, ftKirbySpecialAirNLoopProcInterrupt,
        ftPhysicsApplyAirVelFriction, ftKirbySpecialAirNLoopProcMap },
    /* 280 SpecialAirNEnd */
    [nFTKirbyStatusSpecialAirNEnd - nFTCommonStatusSpecialStart] = { { nFTKirbyMotionSpecialAirNEnd, nFTMotionAttackIDSpecialN }, { { 0, FALSE, nMPKineticsAir, FALSE, nFTStatusAttackIDSpecialN } },
        ftAnimEndSetFall, NULL,
        ftPhysicsApplyAirVelFriction, ftKirbySpecialAirNEndProcMap },
    /* 281 SpecialAirNCatch */
    [nFTKirbyStatusSpecialAirNCatch - nFTCommonStatusSpecialStart] = { { -1, nFTMotionAttackIDSpecialN }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDSpecialN } },
        ftKirbySpecialNCatchProcUpdate, NULL,
        ftPhysicsApplyAirVelFriction, ftKirbySpecialAirNCatchProcMap },
    /* 282 SpecialAirNEat */
    [nFTKirbyStatusSpecialAirNEat - nFTCommonStatusSpecialStart] = { { nFTKirbyMotionSpecialAirNEat, nFTMotionAttackIDSpecialN }, { { 0, FALSE, nMPKineticsAir, FALSE, nFTStatusAttackIDSpecialN } },
        ftKirbySpecialNEatProcUpdate, NULL,
        ftPhysicsApplyAirVelFriction, ftKirbySpecialAirNEatProcMap },
    /* 283 SpecialAirNThrow */
    [nFTKirbyStatusSpecialAirNThrow - nFTCommonStatusSpecialStart] = { { nFTKirbyMotionSpecialAirNThrow, nFTMotionAttackIDSpecialN }, { { 0, FALSE, nMPKineticsAir, FALSE, nFTStatusAttackIDSpecialN } },
        ftKirbySpecialAirNThrowProcUpdate, NULL,
        ftPhysicsApplyAirVelTransNAll, ftKirbySpecialAirNThrowProcMap },
    /* 284 SpecialAirNWait */
    [nFTKirbyStatusSpecialAirNWait - nFTCommonStatusSpecialStart] = { { nFTKirbyMotionSpecialAirNWait, nFTMotionAttackIDSpecialN }, { { 0, FALSE, nMPKineticsAir, FALSE, nFTStatusAttackIDSpecialN } },
        NULL, ftKirbySpecialAirNWaitProcInterrupt,
        ftKirbySpecialAirNWaitProcPhysics, ftKirbySpecialAirNWaitProcMap },
    /* 285 SpecialAirNTurn */
    [nFTKirbyStatusSpecialAirNTurn - nFTCommonStatusSpecialStart] = { { nFTKirbyMotionSpecialAirNTurn, nFTMotionAttackIDSpecialN }, { { 0, FALSE, nMPKineticsAir, FALSE, nFTStatusAttackIDSpecialN } },
        ftKirbySpecialAirNTurnProcUpdate, NULL,
        ftKirbySpecialAirNWaitProcPhysics, ftKirbySpecialAirNTurnProcMap },
    /* 286 SpecialAirNCopy */
    [nFTKirbyStatusSpecialAirNCopy - nFTCommonStatusSpecialStart] = { { nFTKirbyMotionSpecialAirNCopy, nFTMotionAttackIDSpecialN }, { { 0, FALSE, nMPKineticsAir, FALSE, nFTStatusAttackIDSpecialN } },
        ftKirbySpecialAirNCopyProcUpdate, NULL,
        ftPhysicsApplyAirVelFriction, ftKirbySpecialAirNCopyProcMap },

    /* ----  the copy half. Statuses 231-249 and 293-302, from
 * the six ftkirbycopy*specialn.c files whose original fighter the port
 * already has. Statuses 250-255 (AppearR/L, Pikachu's and Ness's copies)
 * stayed zero at: those two fighters' weapons were not in the
 * build. (287-292, Link's, stayed zero for the same reason and are
 * filled by below; 252-253, Pikachu's, by; 254-255,
 * Ness's, by.) Statuses 293-294 are Purin's, whose file has been
 * compiled unmodified -- until this step her rows were
 * PAST THE END of this array, which made reachable by putting
 * her setter in dFTKirbySpecialNStatusList slot 10.
 */
    /* ---- the RAPID JAB ----
 *
 * ft/ftcommon/ftcommonattack100.c compiles unmodified, so these are
 * the three rows the five fighters who have one need. The procs are
 * common; what is per-fighter is the motion and, for Captain Falcon
 * alone, the physics -- his is ftPhysicsApplyGroundVelTransN where
 * the other four use the friction, because he walks forward while
 * he jabs. ---- */
    [nFTKirbyStatusAttack100Start - nFTCommonStatusSpecialStart] = { { nFTKirbyMotionAttack100Start, nFTMotionAttackIDAttack100 }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDAttack100 } },
        ftCommonAttack100StartProcUpdate, NULL,
        ftPhysicsApplyGroundVelFriction, mpCommonSetFighterFallOnEdgeBreak },
    [nFTKirbyStatusAttack100Loop - nFTCommonStatusSpecialStart] = { { nFTKirbyMotionAttack100Loop, nFTMotionAttackIDAttack100 }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDAttack100 } },
        ftCommonAttack100LoopProcUpdate, ftCommonAttack100LoopProcInterrupt,
        ftPhysicsApplyGroundVelFriction, mpCommonSetFighterFallOnEdgeBreak },
    [nFTKirbyStatusAttack100End - nFTCommonStatusSpecialStart] = { { nFTKirbyMotionAttack100End, nFTMotionAttackIDAttack100 }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDAttack100 } },
        ftAnimEndSetWait, NULL,
        ftPhysicsApplyGroundVelFriction, mpCommonSetFighterFallOnEdgeBreak },
    /* ftkirbystatus.h statuses 223-227, the midair jumps: one row per
 * jump, the procs ftcommonjumpaerial.c's. */
    [nFTKirbyStatusJumpAerialF1 - nFTCommonStatusSpecialStart] = { { nFTKirbyMotionJumpAerialF1, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonJumpAerialProcUpdate, ftCommonJumpAerialProcInterrupt,
        ftCommonJumpAerialProcPhysics, mpCommonProcFighterCliffWaitOrLanding },
    [nFTKirbyStatusJumpAerialF2 - nFTCommonStatusSpecialStart] = { { nFTKirbyMotionJumpAerialF2, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonJumpAerialProcUpdate, ftCommonJumpAerialProcInterrupt,
        ftCommonJumpAerialProcPhysics, mpCommonProcFighterCliffWaitOrLanding },
    [nFTKirbyStatusJumpAerialF3 - nFTCommonStatusSpecialStart] = { { nFTKirbyMotionJumpAerialF3, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonJumpAerialProcUpdate, ftCommonJumpAerialProcInterrupt,
        ftCommonJumpAerialProcPhysics, mpCommonProcFighterCliffWaitOrLanding },
    [nFTKirbyStatusJumpAerialF4 - nFTCommonStatusSpecialStart] = { { nFTKirbyMotionJumpAerialF4, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonJumpAerialProcUpdate, ftCommonJumpAerialProcInterrupt,
        ftCommonJumpAerialProcPhysics, mpCommonProcFighterCliffWaitOrLanding },
    [nFTKirbyStatusJumpAerialF5 - nFTCommonStatusSpecialStart] = { { nFTKirbyMotionJumpAerialF5, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonJumpAerialProcUpdate, ftCommonJumpAerialProcInterrupt,
        ftCommonJumpAerialProcPhysics, mpCommonProcFighterCliffWaitOrLanding },

    /* ---- 228-230, KIRBY'S FORWARD THROW ----
 *
 * Kirby's forward throw is not the common one. ftCommonThrowSetStatus
 * branches on his fkind (ftcommonthrow.c:65-70, verbatim in this file
 * since): he goes AIRBORNE and takes nFTKirbyStatusThrowF
 * instead of nFTCommonStatusThrowF, because what he does is leap and
 * slam. That branch has been running since -- and until this
 * step it set status 228 into an EMPTY ROW. Motion 0, no procs, so the
 * throw played the wrong animation and never reached the fall or the
 * landing. It is the step-58 shape a fourth time: a caller wired to a
 * row that does not exist yet, and nothing but a table read to find
 * it.
 *
 * ft/ftchar/ftkirby/ftkirbythrowf.c compiles unmodified and its
 * callee sweep is EMPTY -- six procs, seventy-two lines, and not one
 * symbol the port did not already have.
 *
 * All three rows are marked nMPKineticsGround and the first two are
 * plainly not: ThrowF's physics is ftPhysicsApplyAirVelTransNAll and
 * ThrowFFall has no physics proc at all. The landing's is the one
 * that asks -- ftKirbyThrowFLandingProcPhysics reads fp->ga at
 * runtime and picks. That is the game's, and it is the fourth table
 * here whose ga column its own physics contradicts. ---- */
    /* 228 KirbyThrowF */
    [nFTKirbyStatusThrowF - nFTCommonStatusSpecialStart] = { { nFTKirbyMotionThrowF, nFTMotionAttackIDThrowF }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDThrowF } },
        ftKirbyThrowFProcUpdate, NULL,
        ftPhysicsApplyAirVelTransNAll, ftKirbyThrowFProcMap },
    /* 229 KirbyThrowFFall */
    [nFTKirbyStatusThrowFFall - nFTCommonStatusSpecialStart] = { { nFTKirbyMotionThrowFFall, nFTMotionAttackIDThrowF }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDThrowF } },
        NULL, NULL,
        NULL, ftKirbyThrowFProcMap },
    /* 230 KirbyThrowFLanding */
    [nFTKirbyStatusThrowFLanding - nFTCommonStatusSpecialStart] = { { nFTKirbyMotionThrowFLanding, nFTMotionAttackIDThrowF }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDThrowF } },
        ftCommonThrowProcUpdate, NULL,
        ftKirbyThrowFLandingProcPhysics, ftKirbyThrowFLandingProcMap },
    /* 231 CopyMarioSpecialN */
    [nFTKirbyStatusCopyMarioSpecialN - nFTCommonStatusSpecialStart] = { { nFTKirbyMotionCopyMarioSpecialN, nFTMotionAttackIDSpecialNCopyMario }, { { 0, FALSE, nMPKineticsGround, TRUE, nFTStatusAttackIDSpecialNCopyMario } },
        ftKirbyCopyMarioSpecialNProcUpdate, NULL,
        ftPhysicsApplyGroundVelFriction, ftKirbyCopyMarioSpecialNProcMap },
    /* 232 CopyMarioSpecialAirN */
    [nFTKirbyStatusCopyMarioSpecialAirN - nFTCommonStatusSpecialStart] = { { nFTKirbyMotionCopyMarioSpecialAirN, nFTMotionAttackIDSpecialNCopyMario }, { { 0, FALSE, nMPKineticsAir, TRUE, nFTStatusAttackIDSpecialNCopyMario } },
        ftKirbyCopyMarioSpecialNProcUpdate, NULL,
        ftPhysicsApplyAirVelDrift, ftKirbyCopyMarioSpecialAirNProcMap },
    /* 233 CopyLuigiSpecialN */
    [nFTKirbyStatusCopyLuigiSpecialN - nFTCommonStatusSpecialStart] = { { nFTKirbyMotionCopyLuigiSpecialN, nFTMotionAttackIDSpecialNCopyLuigi }, { { 0, FALSE, nMPKineticsGround, TRUE, nFTStatusAttackIDSpecialNCopyLuigi } },
        ftKirbyCopyMarioSpecialNProcUpdate, NULL,
        ftPhysicsApplyGroundVelFriction, ftKirbyCopyMarioSpecialNProcMap },
    /* 234 CopyLuigiSpecialAirN */
    [nFTKirbyStatusCopyLuigiSpecialAirN - nFTCommonStatusSpecialStart] = { { nFTKirbyMotionCopyLuigiSpecialAirN, nFTMotionAttackIDSpecialNCopyLuigi }, { { 0, FALSE, nMPKineticsAir, TRUE, nFTStatusAttackIDSpecialNCopyLuigi } },
        ftKirbyCopyMarioSpecialNProcUpdate, NULL,
        ftPhysicsApplyAirVelDrift, ftKirbyCopyMarioSpecialAirNProcMap },
    /* 235 CopyFoxSpecialN */
    [nFTKirbyStatusCopyFoxSpecialN - nFTCommonStatusSpecialStart] = { { nFTKirbyMotionCopyFoxSpecialN, nFTMotionAttackIDSpecialNCopyFox }, { { 0, FALSE, nMPKineticsGround, TRUE, nFTStatusAttackIDSpecialNCopyFox } },
        ftKirbyCopyFoxSpecialNProcUpdate, ftKirbyCopyFoxSpecialNProcInterrupt,
        ftPhysicsApplyGroundVelFriction, mpCommonSetFighterFallOnEdgeBreak },
    /* 236 CopyFoxSpecialAirN */
    [nFTKirbyStatusCopyFoxSpecialAirN - nFTCommonStatusSpecialStart] = { { nFTKirbyMotionCopyFoxSpecialAirN, nFTMotionAttackIDSpecialNCopyFox }, { { 0, FALSE, nMPKineticsAir, TRUE, nFTStatusAttackIDSpecialNCopyFox } },
        ftKirbyCopyFoxSpecialNProcUpdate, ftKirbyCopyFoxSpecialNProcInterrupt,
        ftPhysicsApplyAirVelDrift, mpCommonProcFighterWaitOrLanding },
    /* 237 CopySamusSpecialNStart */
    [nFTKirbyStatusCopySamusSpecialNStart - nFTCommonStatusSpecialStart] = { { nFTKirbyMotionCopySamusSpecialNStart, nFTMotionAttackIDSpecialNCopySamus }, { { 0, FALSE, nMPKineticsGround, TRUE, nFTStatusAttackIDSpecialNCopySamus } },
        ftKirbyCopySamusSpecialNStartProcUpdate, ftKirbyCopySamusSpecialNStartProcInterrupt,
        ftPhysicsApplyGroundVelFriction, ftKirbyCopySamusSpecialNStartProcMap },
    /* 238 CopySamusSpecialNLoop */
    [nFTKirbyStatusCopySamusSpecialNLoop - nFTCommonStatusSpecialStart] = { { nFTKirbyMotionCopySamusSpecialNLoop, nFTMotionAttackIDSpecialNCopySamus }, { { 0, FALSE, nMPKineticsGround, TRUE, nFTStatusAttackIDSpecialNCopySamus } },
        ftKirbyCopySamusSpecialNLoopProcUpdate, ftKirbyCopySamusSpecialNLoopProcInterrupt,
        ftPhysicsApplyGroundVelFriction, ftKirbyCopySamusSpecialNLoopProcMap },
    /* 239 CopySamusSpecialNEnd */
    [nFTKirbyStatusCopySamusSpecialNEnd - nFTCommonStatusSpecialStart] = { { nFTKirbyMotionCopySamusSpecialNEnd, nFTMotionAttackIDSpecialNCopySamus }, { { 0, FALSE, nMPKineticsGround, TRUE, nFTStatusAttackIDSpecialNCopySamus } },
        ftKirbyCopySamusSpecialNEndProcUpdate, NULL,
        ftPhysicsApplyGroundVelFriction, ftKirbyCopySamusSpecialNEndProcMap },
    /* 240 CopySamusSpecialAirNStart */
    [nFTKirbyStatusCopySamusSpecialAirNStart - nFTCommonStatusSpecialStart] = { { nFTKirbyMotionCopySamusSpecialAirNStart, nFTMotionAttackIDSpecialNCopySamus }, { { 0, FALSE, nMPKineticsAir, TRUE, nFTStatusAttackIDSpecialNCopySamus } },
        ftKirbyCopySamusSpecialNStartProcUpdate, NULL,
        ftPhysicsApplyAirVelDrift, ftKirbyCopySamusSpecialAirNStartProcMap },
    /* 241 CopySamusSpecialAirNEnd */
    [nFTKirbyStatusCopySamusSpecialAirNEnd - nFTCommonStatusSpecialStart] = { { nFTKirbyMotionCopySamusSpecialAirNEnd, nFTMotionAttackIDSpecialNCopySamus }, { { 0, FALSE, nMPKineticsAir, TRUE, nFTStatusAttackIDSpecialNCopySamus } },
        ftKirbyCopySamusSpecialNEndProcUpdate, NULL,
        ftPhysicsApplyAirVelFriction, ftKirbyCopySamusSpecialAirNEndProcMap },
    /* 242 CopyDonkeySpecialNStart */
    [nFTKirbyStatusCopyDonkeySpecialNStart - nFTCommonStatusSpecialStart] = { { nFTKirbyMotionCopyDonkeySpecialNStart, nFTMotionAttackIDSpecialNCopyDonkey }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDSpecialNCopyDonkey } },
        ftKirbyCopyDonkeySpecialNStartProcUpdate, ftKirbyCopyDonkeySpecialNStartProcInterrupt,
        ftPhysicsApplyGroundVelFriction, ftKirbyCopyDonkeySpecialNStartProcMap },
    /* 243 CopyDonkeySpecialAirNStart */
    [nFTKirbyStatusCopyDonkeySpecialAirNStart - nFTCommonStatusSpecialStart] = { { nFTKirbyMotionCopyDonkeySpecialAirNStart, nFTMotionAttackIDSpecialNCopyDonkey }, { { 0, FALSE, nMPKineticsAir, FALSE, nFTStatusAttackIDSpecialNCopyDonkey } },
        ftKirbyCopyDonkeySpecialAirNStartProcUpdate, ftKirbyCopyDonkeySpecialNStartProcInterrupt,
        ftPhysicsApplyAirVelFriction, ftKirbyCopyDonkeySpecialAirNStartProcMap },
    /* 244 CopyDonkeySpecialNLoop */
    [nFTKirbyStatusCopyDonkeySpecialNLoop - nFTCommonStatusSpecialStart] = { { nFTKirbyMotionCopyDonkeySpecialNLoop, nFTMotionAttackIDSpecialNCopyDonkey }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDSpecialNCopyDonkey } },
        ftKirbyCopyDonkeySpecialNLoopProcUpdate, ftKirbyCopyDonkeySpecialNLoopProcInterrupt,
        ftPhysicsApplyGroundVelFriction, ftKirbyCopyDonkeySpecialNLoopProcMap },
    /* 245 CopyDonkeySpecialAirNLoop */
    [nFTKirbyStatusCopyDonkeySpecialAirNLoop - nFTCommonStatusSpecialStart] = { { nFTKirbyMotionCopyDonkeySpecialAirNLoop, nFTMotionAttackIDSpecialNCopyDonkey }, { { 0, FALSE, nMPKineticsAir, FALSE, nFTStatusAttackIDSpecialNCopyDonkey } },
        ftKirbyCopyDonkeySpecialNLoopProcUpdate, ftKirbyCopyDonkeySpecialNLoopProcInterrupt,
        ftPhysicsApplyAirVelFriction, ftKirbyCopyDonkeySpecialAirNLoopProcMap },
    /* 246 CopyDonkeySpecialNEnd */
    [nFTKirbyStatusCopyDonkeySpecialNEnd - nFTCommonStatusSpecialStart] = { { nFTKirbyMotionCopyDonkeySpecialNEnd, nFTMotionAttackIDSpecialNCopyDonkey }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDSpecialNCopyDonkey } },
        ftKirbyCopyDonkeySpecialNEndProcUpdate, NULL,
        ftPhysicsApplyGroundVelFriction, mpCommonSetFighterFallOnEdgeBreak },
    /* 247 CopyDonkeySpecialAirNEnd */
    [nFTKirbyStatusCopyDonkeySpecialAirNEnd - nFTCommonStatusSpecialStart] = { { nFTKirbyMotionCopyDonkeySpecialAirNEnd, nFTMotionAttackIDSpecialNCopyDonkey }, { { 0, FALSE, nMPKineticsAir, FALSE, nFTStatusAttackIDSpecialNCopyDonkey } },
        ftKirbyCopyDonkeySpecialNEndProcUpdate, NULL,
        ftPhysicsApplyAirVelFriction, ftKirbyCopyDonkeySpecialAirNEndProcMap },
    /* 248 CopyDonkeySpecialNFull */
    [nFTKirbyStatusCopyDonkeySpecialNFull - nFTCommonStatusSpecialStart] = { { nFTKirbyMotionCopyDonkeySpecialNFull, nFTMotionAttackIDSpecialNCopyDonkey }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDSpecialNCopyDonkey } },
        ftKirbyCopyDonkeySpecialNEndProcUpdate, NULL,
        ftPhysicsApplyGroundVelFriction, mpCommonSetFighterFallOnEdgeBreak },
    /* 249 CopyDonkeySpecialAirNFull */
    [nFTKirbyStatusCopyDonkeySpecialAirNFull - nFTCommonStatusSpecialStart] = { { nFTKirbyMotionCopyDonkeySpecialAirNFull, nFTMotionAttackIDSpecialNCopyDonkey }, { { 0, FALSE, nMPKineticsAir, FALSE, nFTStatusAttackIDSpecialNCopyDonkey } },
        ftKirbyCopyDonkeySpecialNEndProcUpdate, NULL,
        ftPhysicsApplyAirVelFriction, ftKirbyCopyDonkeySpecialAirNEndProcMap },
    /* ----  statuses 287-292, Kirby's copy of Link's
 * boomerang -- the first of the three mouths had to leave
 * NULL, and it opened because this step ports the weapon that was
 * blocking it. ftkirbycopylinkspecialn.c is a mirror of Link's own
 * file, differing only in which passive var holds the boomerang
 * (copylink_boomerang_gobj, not link.boomerang_gobj) and in the six
 * status ids below; wp/wplink/wplinkboomerang.c branches on
 * nFTKindKirby to pick the right owner and the right GetSetStatus.
 * Two mouths were still NULL then, Pikachu's and Ness's, filled at
 * and. ---- */
    /* ----  statuses 252-253, Kirby's copy of Pikachu's
 * Thunder Jolt -- the ninth mouth, and it opened the same way Link's
 * did, because the step that ports the weapon is the step that can
 * fill it. ftkirbycopypikachuspecialn.c is a mirror of Pikachu's own
 * file with two differences and no third: Kirby's spawn point needs
 * an x/y offset off the joint that Pikachu's does not, and the two
 * status ids are his. Unlike the boomerang, the jolt itself never
 * looks at who threw it -- wpPikachuThunderJoltAirMakeWeapon takes
 * the fighter GObj and asks nothing about its fkind -- so this half
 * is a one-way dependency, kept in the same step only because the
 * weapon it needs arrives here. ONE mouth is still NULL: Ness's,
 * which waits on it/ and. ---- */
    /* 252 KirbyCopyPikachuSpecialN */
    [nFTKirbyStatusCopyPikachuSpecialN - nFTCommonStatusSpecialStart] = { { nFTKirbyMotionCopyPikachuSpecialN, nFTMotionAttackIDSpecialNCopyPikachu }, { { 0, FALSE, nMPKineticsGround, TRUE, nFTStatusAttackIDSpecialNCopyPikachu } },
        ftAnimEndSetWait, NULL,
        ftPhysicsApplyGroundVelFriction, ftKirbyCopyPikachuSpecialNProcMap },
    /* 253 KirbyCopyPikachuSpecialAirN */
    [nFTKirbyStatusCopyPikachuSpecialAirN - nFTCommonStatusSpecialStart] = { { nFTKirbyMotionCopyPikachuSpecialAirN, nFTMotionAttackIDSpecialNCopyPikachu }, { { 0, FALSE, nMPKineticsAir, TRUE, nFTStatusAttackIDSpecialNCopyPikachu } },
        ftAnimEndSetFall, NULL,
        ftPhysicsApplyAirVelFriction, ftKirbyCopyPikachuSpecialAirNProcMap },
    /* 254 KirbyCopyNessSpecialN */
    [nFTKirbyStatusCopyNessSpecialN - nFTCommonStatusSpecialStart] = { { nFTKirbyMotionCopyNessSpecialN, nFTMotionAttackIDSpecialNCopyNess }, { { 0, FALSE, nMPKineticsGround, TRUE, nFTStatusAttackIDSpecialNCopyNess } },
        ftAnimEndSetWait, NULL,
        ftPhysicsApplyGroundVelFriction, ftKirbyCopyNessSpecialNProcMap },
    /* 255 KirbyCopyNessSpecialAirN */
    [nFTKirbyStatusCopyNessSpecialAirN - nFTCommonStatusSpecialStart] = { { nFTKirbyMotionCopyNessSpecialAirN, nFTMotionAttackIDSpecialNCopyNess }, { { 0, FALSE, nMPKineticsAir, TRUE, nFTStatusAttackIDSpecialNCopyNess } },
        ftAnimEndSetFall, NULL,
        ftPhysicsApplyAirVelFriction, ftKirbyCopyNessSpecialAirNProcMap },
    /* 287 KirbyCopyLinkSpecialN */
    [nFTKirbyStatusCopyLinkSpecialN - nFTCommonStatusSpecialStart] = { { nFTKirbyMotionCopyLinkSpecialN, nFTMotionAttackIDSpecialNCopyLink }, { { 0, FALSE, nMPKineticsGround, TRUE, nFTStatusAttackIDSpecialNCopyLink } },
        ftKirbyCopyLinkSpecialNProcUpdate, NULL,
        ftPhysicsApplyGroundVelFriction, ftKirbyCopyLinkSpecialNProcMap },
    /* 288 KirbyCopyLinkSpecialNGet */
    [nFTKirbyStatusCopyLinkSpecialNGet - nFTCommonStatusSpecialStart] = { { nFTKirbyMotionCopyLinkSpecialNGet, nFTMotionAttackIDSpecialNCopyLink }, { { 0, FALSE, nMPKineticsGround, TRUE, nFTStatusAttackIDSpecialNCopyLink } },
        ftAnimEndSetWait, NULL,
        ftPhysicsApplyGroundVelFriction, mpCommonSetFighterFallOnGroundBreak },
    /* 289 KirbyCopyLinkSpecialNEmpty */
    [nFTKirbyStatusCopyLinkSpecialNEmpty - nFTCommonStatusSpecialStart] = { { nFTKirbyMotionCopyLinkSpecialNEmpty, nFTMotionAttackIDSpecialNCopyLink }, { { 0, FALSE, nMPKineticsGround, TRUE, nFTStatusAttackIDSpecialNCopyLink } },
        ftAnimEndSetWait, NULL,
        ftPhysicsApplyGroundVelFriction, ftKirbyCopyLinkSpecialNEmptyProcMap },
    /* 290 KirbyCopyLinkSpecialAirN */
    [nFTKirbyStatusCopyLinkSpecialAirN - nFTCommonStatusSpecialStart] = { { nFTKirbyMotionCopyLinkSpecialAirN, nFTMotionAttackIDSpecialNCopyLink }, { { 0, FALSE, nMPKineticsAir, TRUE, nFTStatusAttackIDSpecialNCopyLink } },
        ftKirbyCopyLinkSpecialAirNProcUpdate, NULL,
        ftPhysicsApplyAirVelFriction, ftKirbyCopyLinkSpecialAirNProcMap },
    /* 291 KirbyCopyLinkSpecialAirNReturn */
    [nFTKirbyStatusCopyLinkSpecialAirNReturn - nFTCommonStatusSpecialStart] = { { nFTKirbyMotionCopyLinkSpecialAirNReturn, nFTMotionAttackIDSpecialNCopyLink }, { { 0, FALSE, nMPKineticsAir, TRUE, nFTStatusAttackIDSpecialNCopyLink } },
        ftAnimEndSetFall, NULL,
        ftPhysicsApplyAirVelFriction, mpCommonProcFighterWaitOrLanding },
    /* 292 KirbyCopyLinkSpecialAirNEmpty */
    [nFTKirbyStatusCopyLinkSpecialAirNEmpty - nFTCommonStatusSpecialStart] = { { nFTKirbyMotionCopyLinkSpecialAirNEmpty, nFTMotionAttackIDSpecialNCopyLink }, { { 0, FALSE, nMPKineticsAir, TRUE, nFTStatusAttackIDSpecialNCopyLink } },
        ftAnimEndSetFall, NULL,
        ftPhysicsApplyAirVelFriction, ftKirbyCopyLinkSpecialAirNEmptyProcMap },
    /* 293 CopyPurinSpecialN */
    [nFTKirbyStatusCopyPurinSpecialN - nFTCommonStatusSpecialStart] = { { nFTKirbyMotionCopyPurinSpecialN, nFTMotionAttackIDSpecialNCopyPurin }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDSpecialNCopyPurin } },
        ftAnimEndSetWait, NULL,
        ftPhysicsApplyGroundVelTransN, ftKirbyCopyPurinSpecialNProcMap },
    /* 294 CopyPurinSpecialAirN */
    [nFTKirbyStatusCopyPurinSpecialAirN - nFTCommonStatusSpecialStart] = { { nFTKirbyMotionCopyPurinSpecialAirN, nFTMotionAttackIDSpecialNCopyPurin }, { { 0, FALSE, nMPKineticsAir, FALSE, nFTStatusAttackIDSpecialNCopyPurin } },
        ftAnimEndSetFall, NULL,
        ftKirbyCopyPurinSpecialAirNProcPhysics, ftKirbyCopyPurinSpecialAirNProcMap },
    /* 295 CopyCaptainSpecialN */
    [nFTKirbyStatusCopyCaptainSpecialN - nFTCommonStatusSpecialStart] = { { nFTKirbyMotionCopyCaptainSpecialN, nFTMotionAttackIDSpecialNCopyCaptain }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDSpecialNCopyCaptain } },
        ftAnimEndSetWait, NULL,
        ftKirbyCopyCaptainSpecialNProcPhysics, ftKirbyCopyCaptainSpecialNProcMap },
    /* 296 CopyCaptainSpecialAirN */
    [nFTKirbyStatusCopyCaptainSpecialAirN - nFTCommonStatusSpecialStart] = { { nFTKirbyMotionCopyCaptainSpecialAirN, nFTMotionAttackIDSpecialNCopyCaptain }, { { 0, FALSE, nMPKineticsAir, FALSE, nFTStatusAttackIDSpecialNCopyCaptain } },
        ftAnimEndSetFall, NULL,
        ftKirbyCopyCaptainSpecialAirNProcPhysics, ftKirbyCopyCaptainSpecialAirNProcMap },
    /* 297 CopyYoshiSpecialN */
    [nFTKirbyStatusCopyYoshiSpecialN - nFTCommonStatusSpecialStart] = { { nFTKirbyMotionCopyYoshiSpecialN, nFTMotionAttackIDSpecialNCopyYoshi }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDSpecialNCopyYoshi } },
        ftAnimEndSetWait, NULL,
        ftPhysicsApplyGroundVelFriction, ftKirbyCopyYoshiSpecialNProcMap },
    /* 298 CopyYoshiSpecialNCatch */
    [nFTKirbyStatusCopyYoshiSpecialNCatch - nFTCommonStatusSpecialStart] = { { nFTKirbyMotionCopyYoshiSpecialNCatch, nFTMotionAttackIDSpecialNCopyYoshi }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDSpecialNCopyYoshi } },
        ftKirbyCopyYoshiSpecialNCatchProcUpdate, NULL,
        ftPhysicsApplyGroundVelFriction, ftKirbyCopyYoshiSpecialNCatchProcMap },
    /* 299 CopyYoshiSpecialNRelease */
    [nFTKirbyStatusCopyYoshiSpecialNRelease - nFTCommonStatusSpecialStart] = { { nFTKirbyMotionCopyYoshiSpecialNRelease, nFTMotionAttackIDSpecialNCopyYoshi }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDSpecialNCopyYoshi } },
        ftKirbyCopyYoshiSpecialNReleaseProcUpdate, NULL,
        ftPhysicsApplyGroundVelFriction, ftKirbyCopyYoshiSpecialNReleaseProcMap },
    /* 300 CopyYoshiSpecialAirN */
    [nFTKirbyStatusCopyYoshiSpecialAirN - nFTCommonStatusSpecialStart] = { { nFTKirbyMotionCopyYoshiSpecialAirN, nFTMotionAttackIDSpecialNCopyYoshi }, { { 0, FALSE, nMPKineticsAir, FALSE, nFTStatusAttackIDSpecialNCopyYoshi } },
        ftAnimEndSetFall, NULL,
        ftPhysicsApplyAirVelFriction, ftKirbyCopyYoshiSpecialAirNProcMap },
    /* 301 CopyYoshiSpecialAirNCatch */
    [nFTKirbyStatusCopyYoshiSpecialAirNCatch - nFTCommonStatusSpecialStart] = { { nFTKirbyMotionCopyYoshiSpecialAirNCatch, nFTMotionAttackIDSpecialNCopyYoshi }, { { 0, FALSE, nMPKineticsAir, FALSE, nFTStatusAttackIDSpecialNCopyYoshi } },
        ftKirbyCopyYoshiSpecialAirNCatchProcUpdate, NULL,
        ftPhysicsApplyAirVelFriction, ftKirbyCopyYoshiSpecialAirNCatchProcMap },
    /* 302 CopyYoshiSpecialAirNRelease */
    [nFTKirbyStatusCopyYoshiSpecialAirNRelease - nFTCommonStatusSpecialStart] = { { nFTKirbyMotionCopyYoshiSpecialAirNRelease, nFTMotionAttackIDSpecialNCopyYoshi }, { { 0, FALSE, nMPKineticsAir, FALSE, nFTStatusAttackIDSpecialNCopyYoshi } },
        ftKirbyCopyYoshiSpecialAirNReleaseProcUpdate, NULL,
        ftPhysicsApplyAirVelFriction, ftKirbyCopyYoshiSpecialAirNReleaseProcMap },
};

/* ---- ft/ftchar/ftfox/ftfoxstatus.h dFTFoxSpecialStatusDescs,
 * statuses 225-226: the Blaster, Fox's Neutral-B.
 * ftfoxspecialn.c's five procs are decomp-verbatim and needed no new
 * infrastructure -- ftPhysicsApplyGroundVelFriction/ApplyAirVelDrift,
 * mpCommonSetFighterFallOnEdgeBreak/ProcFighterWaitOrLanding and the
 * step-39 wpFoxBlasterMakeWeapon were all already in the port.
 *
 * Statuses 236-245 are the Reflector, Fox's Down-B:
 * ftfoxspeciallw.c's twenty-three procs, decomp-verbatim. Start/Hit/End/
 * Loop/Turn, ground and air each; the ground five share ftPhysicsApply-
 * GroundVelFriction and mpCommonSetFighterFallOnEdgeBreak, the air five
 * ftFoxSpecialAirLwCommonProcPhysics (a delayed, gentler gravity) and
 * mpCommonProcFighterWaitOrLanding; the Turn rows play the Loop motion.
 * Only the Loop pair has a ProcInterrupt (the turn check). All eight
 * motions (Shine*, ft/ftdata.c:1088-1095) are FTANIM_FLAG_NONE, so no
 * mock-motion entry. What the file needed was the hurt sphere
 * (dFoxMainMotion_LwReflectorFTSpecialColl and its bind, above) and the
 * reflector effect (ef/efmanager.c, whose maker returns NULL until the
 * model exists).
 *
 * Statuses 227-235 are Fire Fox, Fox's Up-B:
 * ftfoxspecialhi.c's thirty procs, decomp-verbatim. A ground and an air
 * half of Start (the crouch), Hold (the charge, FTFOX_FIREFOX_LAUNCH_DELAY
 * = 35 tics), the travel and End, plus the aerial-only Bound he takes when
 * he slams a surface head-on. Each ground/air pair swaps status the moment
 * the map proc says he left or met the floor, so the two halves share
 * their update procs; the travel pair share ftFoxSpecialHiProcUpdate and
 * differ only in physics (ground friction along the slope versus the
 * angled 115.0 velocity the air half decelerates by cos/sin). Five of the
 * nine motions (the aerial ones, ft/ftdata.c:1080-1087) carry
 * FTANIM_FLAG_XROTN_JOINT; nFTFoxMotionSpecialAirHiBound's row is the one
 * whose physics proc (ftPhysicsApplyAirVelTransNYZ) actually reads it.
 * Rows 220-224 (Attack100 and AppearR/L) are filled below. */
FTStatusDesc dFTFoxSpecialStatusDescs[nFTFoxStatusSpecialLwScopeEnd - nFTCommonStatusSpecialStart + 1] = {
    /* the rapid jab: three rows, the procs common and the
 * motion this fighter's own. */
    /* Fox Attack100Start */
    [nFTFoxStatusAttack100Start - nFTCommonStatusSpecialStart] = { { nFTFoxMotionAttack100Start, nFTMotionAttackIDAttack100 }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDAttack100 } },
        ftCommonAttack100StartProcUpdate, NULL,
        ftPhysicsApplyGroundVelFriction, mpCommonSetFighterFallOnEdgeBreak },
    /* Fox Attack100Loop */
    [nFTFoxStatusAttack100Loop - nFTCommonStatusSpecialStart] = { { nFTFoxMotionAttack100Loop, nFTMotionAttackIDAttack100 }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDAttack100 } },
        ftCommonAttack100LoopProcUpdate, ftCommonAttack100LoopProcInterrupt,
        ftPhysicsApplyGroundVelFriction, mpCommonSetFighterFallOnEdgeBreak },
    /* Fox Attack100End */
    [nFTFoxStatusAttack100End - nFTCommonStatusSpecialStart] = { { nFTFoxMotionAttack100End, nFTMotionAttackIDAttack100 }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDAttack100 } },
        ftAnimEndSetWait, NULL,
        ftPhysicsApplyGroundVelFriction, mpCommonSetFighterFallOnEdgeBreak },

    /* 223 FoxAppearR */
    [nFTFoxStatusAppearR - nFTCommonStatusSpecialStart] = { { nFTFoxMotionAppearR, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonAppearProcUpdate, NULL,
        ftCommonAppearProcPhysics, mpCommonUpdateFighterProjectFloor },
    /* 224 FoxAppearL */
    [nFTFoxStatusAppearL - nFTCommonStatusSpecialStart] = { { nFTFoxMotionAppearL, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonAppearProcUpdate, NULL,
        ftCommonAppearProcPhysics, mpCommonUpdateFighterProjectFloor },
    [nFTFoxStatusSpecialN - nFTCommonStatusSpecialStart] = { { nFTFoxMotionSpecialN, nFTMotionAttackIDSpecialN }, { { 0, FALSE, nMPKineticsGround, TRUE, nFTStatusAttackIDSpecialN } },
        ftFoxSpecialNProcUpdate, ftFoxSpecialNProcInterrupt, ftPhysicsApplyGroundVelFriction,
        mpCommonSetFighterFallOnEdgeBreak },
    [nFTFoxStatusSpecialAirN - nFTCommonStatusSpecialStart] = { { nFTFoxMotionSpecialAirN, nFTMotionAttackIDSpecialN }, { { 0, FALSE, nMPKineticsAir, TRUE, nFTStatusAttackIDSpecialN } },
        ftFoxSpecialNProcUpdate, ftFoxSpecialNProcInterrupt, ftPhysicsApplyAirVelDrift,
        mpCommonProcFighterWaitOrLanding },
    [nFTFoxStatusSpecialHiStart - nFTCommonStatusSpecialStart] = { { nFTFoxMotionSpecialHiStart, nFTMotionAttackIDSpecialHi }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDSpecialHi } },
        ftFoxSpecialHiStartProcUpdate, NULL, ftPhysicsApplyGroundVelFriction,
        ftFoxSpecialHiStartProcMap },
    [nFTFoxStatusSpecialAirHiStart - nFTCommonStatusSpecialStart] = { { nFTFoxMotionSpecialAirHiStart, nFTMotionAttackIDSpecialHi }, { { 0, FALSE, nMPKineticsAir, FALSE, nFTStatusAttackIDSpecialHi } },
        ftFoxSpecialAirHiStartProcUpdate, NULL, ftFoxSpecialAirHiStartProcPhysics,
        ftFoxSpecialAirHiStartProcMap },
    [nFTFoxStatusSpecialHiHold - nFTCommonStatusSpecialStart] = { { nFTFoxMotionSpecialHiHold, nFTMotionAttackIDSpecialHi }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDSpecialHi } },
        ftFoxSpecialHiHoldProcUpdate, NULL, ftPhysicsApplyGroundVelFriction,
        ftFoxSpecialHiHoldProcMap },
    [nFTFoxStatusSpecialAirHiHold - nFTCommonStatusSpecialStart] = { { nFTFoxMotionSpecialAirHiHold, nFTMotionAttackIDSpecialHi }, { { 0, FALSE, nMPKineticsAir, FALSE, nFTStatusAttackIDSpecialHi } },
        ftFoxSpecialHiHoldProcUpdate, NULL, ftFoxSpecialAirHiStartProcPhysics,
        ftFoxSpecialAirHiHoldProcMap },
    [nFTFoxStatusSpecialHi - nFTCommonStatusSpecialStart] = { { nFTFoxMotionSpecialHi, nFTMotionAttackIDSpecialHi }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDSpecialHi } },
        ftFoxSpecialHiProcUpdate, NULL, ftFoxSpecialHiProcPhysics,
        ftFoxSpecialHiProcMap },
    [nFTFoxStatusSpecialAirHi - nFTCommonStatusSpecialStart] = { { nFTFoxMotionSpecialAirHi, nFTMotionAttackIDSpecialHi }, { { 0, FALSE, nMPKineticsAir, FALSE, nFTStatusAttackIDSpecialHi } },
        ftFoxSpecialHiProcUpdate, NULL, ftFoxSpecialAirHiProcPhysics,
        ftFoxSpecialAirHiProcMap },
    [nFTFoxStatusSpecialHiEnd - nFTCommonStatusSpecialStart] = { { nFTFoxMotionSpecialHiEnd, nFTMotionAttackIDSpecialHi }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDSpecialHi } },
        ftAnimEndSetWait, NULL, ftFoxSpecialHiEndProcPhysics,
        mpCommonSetFighterFallOnGroundBreak },
    [nFTFoxStatusSpecialAirHiEnd - nFTCommonStatusSpecialStart] = { { nFTFoxMotionSpecialAirHiEnd, nFTMotionAttackIDSpecialHi }, { { 0, FALSE, nMPKineticsAir, FALSE, nFTStatusAttackIDSpecialHi } },
        ftFoxSpecialAirHiEndProcUpdate, NULL, ftPhysicsApplyAirVelDrift,
        ftFoxSpecialAirHiEndProcMap },
    [nFTFoxStatusSpecialAirHiBound - nFTCommonStatusSpecialStart] = { { nFTFoxMotionSpecialAirHiBound, nFTMotionAttackIDSpecialHi }, { { 0, FALSE, nMPKineticsAir, FALSE, nFTStatusAttackIDSpecialHi } },
        ftFoxSpecialAirHiBoundProcUpdate, NULL, ftFoxSpecialAirHiBoundProcPhysics,
        ftFoxSpecialAirHiBoundProcMap },
    [nFTFoxStatusSpecialLwStart - nFTCommonStatusSpecialStart] = { { nFTFoxMotionSpecialLwStart, nFTMotionAttackIDSpecialLw }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDSpecialLw } },
        ftFoxSpecialLwStartProcUpdate, NULL, ftPhysicsApplyGroundVelFriction,
        mpCommonSetFighterFallOnEdgeBreak },
    [nFTFoxStatusSpecialLwHit - nFTCommonStatusSpecialStart] = { { nFTFoxMotionSpecialLwHit, nFTMotionAttackIDSpecialLw }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDSpecialLw } },
        ftFoxSpecialLwHitProcUpdate, NULL, ftPhysicsApplyGroundVelFriction,
        mpCommonSetFighterFallOnEdgeBreak },
    [nFTFoxStatusSpecialLwEnd - nFTCommonStatusSpecialStart] = { { nFTFoxMotionSpecialLwEnd, nFTMotionAttackIDSpecialLw }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDSpecialLw } },
        ftFoxSpecialLwEndProcUpdate, NULL, ftPhysicsApplyGroundVelFriction,
        mpCommonSetFighterFallOnEdgeBreak },
    [nFTFoxStatusSpecialLwLoop - nFTCommonStatusSpecialStart] = { { nFTFoxMotionSpecialLwLoop, nFTMotionAttackIDSpecialLw }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDSpecialLw } },
        ftFoxSpecialLwLoopProcUpdate, ftFoxSpecialLwLoopProcInterrupt,
        ftPhysicsApplyGroundVelFriction, mpCommonSetFighterFallOnEdgeBreak },
    [nFTFoxStatusSpecialLwTurn - nFTCommonStatusSpecialStart] = { { nFTFoxMotionSpecialLwLoop, nFTMotionAttackIDSpecialLw }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDSpecialLw } },
        ftFoxSpecialLwTurnProcUpdate, NULL, ftPhysicsApplyGroundVelFriction,
        mpCommonSetFighterFallOnEdgeBreak },
    [nFTFoxStatusSpecialAirLwStart - nFTCommonStatusSpecialStart] = { { nFTFoxMotionSpecialAirLwStart, nFTMotionAttackIDSpecialLw }, { { 0, FALSE, nMPKineticsAir, FALSE, nFTStatusAttackIDSpecialLw } },
        ftFoxSpecialLwStartProcUpdate, NULL, ftFoxSpecialAirLwCommonProcPhysics,
        mpCommonProcFighterWaitOrLanding },
    [nFTFoxStatusSpecialAirLwHit - nFTCommonStatusSpecialStart] = { { nFTFoxMotionSpecialAirLwHit, nFTMotionAttackIDSpecialLw }, { { 0, FALSE, nMPKineticsAir, FALSE, nFTStatusAttackIDSpecialLw } },
        ftFoxSpecialLwHitProcUpdate, NULL, ftFoxSpecialAirLwCommonProcPhysics,
        mpCommonProcFighterWaitOrLanding },
    [nFTFoxStatusSpecialAirLwEnd - nFTCommonStatusSpecialStart] = { { nFTFoxMotionSpecialAirLwEnd, nFTMotionAttackIDSpecialLw }, { { 0, FALSE, nMPKineticsAir, FALSE, nFTStatusAttackIDSpecialLw } },
        ftFoxSpecialLwEndProcUpdate, NULL, ftFoxSpecialAirLwCommonProcPhysics,
        mpCommonProcFighterWaitOrLanding },
    [nFTFoxStatusSpecialAirLwLoop - nFTCommonStatusSpecialStart] = { { nFTFoxMotionSpecialAirLwLoop, nFTMotionAttackIDSpecialLw }, { { 0, FALSE, nMPKineticsAir, FALSE, nFTStatusAttackIDSpecialLw } },
        ftFoxSpecialLwLoopProcUpdate, ftFoxSpecialLwLoopProcInterrupt,
        ftFoxSpecialAirLwCommonProcPhysics, mpCommonProcFighterWaitOrLanding },
    [nFTFoxStatusSpecialAirLwTurn - nFTCommonStatusSpecialStart] = { { nFTFoxMotionSpecialAirLwLoop, nFTMotionAttackIDSpecialLw }, { { 0, FALSE, nMPKineticsAir, FALSE, nFTStatusAttackIDSpecialLw } },
        ftFoxSpecialLwTurnProcUpdate, NULL, ftFoxSpecialAirLwCommonProcPhysics,
        mpCommonProcFighterWaitOrLanding },
};

/* ---- ft/ftchar/ftsamus/ftsamusstatus.h dFTSamusSpecialStatusDescs,
 * statuses 227-230: Screw Attack (Samus's Up-B,) plus Bomb
 * (her Down-B,). ftsamusspecialhi.c's seven procs and
 * ftsamusspeciallw.c's thirteen are both decomp-verbatim. The Up-B pair
 * needs no new infrastructure; the Down-B pair needed
 * exactly one: efManagerSparkleWhiteMultiExplodeMakeEffect
 * (wp/wpsamus/wpsamusbomb.c's own hit/expire/absorb spark, ef/efmanager.c,
 * this step), the same plain-particle shape efManagerSparkleWhiteMakeEffect
 * already has. Every other callee -- ftPhysicsApplyGround-
 * VelFriction/GravityDefault/ClampAirVelXDec/ClampAirVelX, mpCommonSet-
 * FighterGround/Air, mpCommonProcFighterOnEdge/Landing, ftMainSetStatus/
 * PlayAnimEventsAll, gmCollisionGetFighterPartsWorldPosition -- was already
 * in the port. Rows 222-226 are the Charge Shot (her
 * Neutral-B, ftsamusspecialn.c's twenty-two procs plus wp/wpsamus/
 * wpsamuschargeshot.c, both decomp-verbatim -- Samus's whole special-move
 * set, the fifth fighter's): every one of the five rows is "is projectile"
 * TRUE like Bomb's, the Start/End pairs share ProcUpdate across ground and
 * air, and all five motions (StartingChargeShot, ChargingNeutralSpecial,
 * Shooting, StartingChargeShotAir, ShootingAir, ft/ftdata.c:2376-2380) are
 * FTANIM_FLAG_NONE, so no mock-motion entry. The charge is
 * passive_vars.samus.charge_level, kept across statuses and reset through
 * fp->proc_damage; the held shot is a weapon parented to Samus that the
 * Loop grows a level every FTSAMUS_CHARGE_INT tics, and the End either
 * fires the held one (mpCommonRunWeaponCollisionDefault first, so it
 * starts the tic already collided) or makes a released one on the spot.
 * Rows 220-221 (AppearR/L) are the only holes left. Neither Bomb motion
 * (BombGround/BombAir) carries the
 * TransN joint bit in ft/ftdata.c, unlike SpecialHi's ground half, so no
 * host mock-motion entry is needed for these two rows. Unlike Donkey's
 * pair, every one of Samus's four setters here targets its own status id
 * directly -- no same-tic quirk anywhere in this table. */
FTStatusDesc dFTSamusSpecialStatusDescs[nFTSamusStatusSpecialAirLw - nFTCommonStatusSpecialStart + 1] = {
    /* 220 SamusAppearR */
    [nFTSamusStatusAppearR - nFTCommonStatusSpecialStart] = { { nFTSamusMotionAppearR, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonAppearProcUpdate, NULL,
        ftCommonAppearProcPhysics, mpCommonUpdateFighterProjectFloor },
    /* 221 SamusAppearL */
    [nFTSamusStatusAppearL - nFTCommonStatusSpecialStart] = { { nFTSamusMotionAppearL, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonAppearProcUpdate, NULL,
        ftCommonAppearProcPhysics, mpCommonUpdateFighterProjectFloor },
    [nFTSamusStatusSpecialNStart - nFTCommonStatusSpecialStart] = { { nFTSamusMotionSpecialNStart, nFTMotionAttackIDSpecialN }, { { 0, FALSE, nMPKineticsGround, TRUE, nFTStatusAttackIDSpecialN } },
        ftSamusSpecialNStartProcUpdate, ftSamusSpecialNStartProcInterrupt,
        ftPhysicsApplyGroundVelFriction, ftSamusSpecialNStartProcMap },
    [nFTSamusStatusSpecialNLoop - nFTCommonStatusSpecialStart] = { { nFTSamusMotionSpecialNLoop, nFTMotionAttackIDSpecialN }, { { 0, FALSE, nMPKineticsGround, TRUE, nFTStatusAttackIDSpecialN } },
        ftSamusSpecialNLoopProcUpdate, ftSamusSpecialNLoopProcInterrupt,
        ftPhysicsApplyGroundVelFriction, ftSamusSpecialNLoopProcMap },
    [nFTSamusStatusSpecialNEnd - nFTCommonStatusSpecialStart] = { { nFTSamusMotionSpecialNEnd, nFTMotionAttackIDSpecialN }, { { 0, FALSE, nMPKineticsGround, TRUE, nFTStatusAttackIDSpecialN } },
        ftSamusSpecialNEndProcUpdate, NULL, ftPhysicsApplyGroundVelFriction,
        ftSamusSpecialNEndProcMap },
    [nFTSamusStatusSpecialAirNStart - nFTCommonStatusSpecialStart] = { { nFTSamusMotionSpecialAirNStart, nFTMotionAttackIDSpecialN }, { { 0, FALSE, nMPKineticsAir, TRUE, nFTStatusAttackIDSpecialN } },
        ftSamusSpecialNStartProcUpdate, NULL, ftPhysicsApplyAirVelDrift,
        ftSamusSpecialAirNStartProcMap },
    [nFTSamusStatusSpecialAirNEnd - nFTCommonStatusSpecialStart] = { { nFTSamusMotionSpecialAirNEnd, nFTMotionAttackIDSpecialN }, { { 0, FALSE, nMPKineticsAir, TRUE, nFTStatusAttackIDSpecialN } },
        ftSamusSpecialNEndProcUpdate, NULL, ftPhysicsApplyAirVelFriction,
        ftSamusSpecialAirNEndProcMap },
    [nFTSamusStatusSpecialHi - nFTCommonStatusSpecialStart] = { { nFTSamusMotionSpecialHi, nFTMotionAttackIDSpecialHi }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDSpecialHi } },
        ftSamusSpecialHiProcUpdate, NULL, ftSamusSpecialHiProcPhysics,
        ftSamusSpecialHiProcMap },
    [nFTSamusStatusSpecialAirHi - nFTCommonStatusSpecialStart] = { { nFTSamusMotionSpecialAirHi, nFTMotionAttackIDSpecialHi }, { { 0, FALSE, nMPKineticsAir, FALSE, nFTStatusAttackIDSpecialHi } },
        ftSamusSpecialHiProcUpdate, NULL, ftSamusSpecialAirHiProcPhysics,
        ftSamusSpecialHiProcMap },
    [nFTSamusStatusSpecialLw - nFTCommonStatusSpecialStart] = { { nFTSamusMotionSpecialLw, nFTMotionAttackIDSpecialLw }, { { 0, FALSE, nMPKineticsGround, TRUE, nFTStatusAttackIDSpecialLw } },
        ftSamusSpecialLwProcUpdate, NULL, ftSamusSpecialLwProcPhysics,
        ftSamusSpecialLwProcMap },
    [nFTSamusStatusSpecialAirLw - nFTCommonStatusSpecialStart] = { { nFTSamusMotionSpecialAirLw, nFTMotionAttackIDSpecialLw }, { { 0, FALSE, nMPKineticsAir, TRUE, nFTStatusAttackIDSpecialLw } },
        ftSamusSpecialAirLwProcUpdate, NULL, ftSamusSpecialAirLwProcPhysics,
        ftSamusSpecialAirLwProcMap },
};

/* ---- relocData/235_CaptainMainMotion.c:36-64 dCaptainMainMotion_0x0000:
 * Falcon Dive's grab offsets per victim FTKind, the one
 * piece of Captain's motion file the code reads directly.
 * ftCommonCaptureCaptainUpdatePositions (ft/ftcommon/ftcommoncapture-
 * captain.c:6-27, compiled unmodified) does it the game's way,
 * lbRelocGetFileData(Vec2h*, gFTDataCaptainMainMotion,
 * &llCaptainMainMotionSpecialHiVec2h), and the port never loads that file
 * (fighters come out of packs, src/dc/ftmanager.c). The table itself is
 * in the decomp as C, so it is copied here verbatim; the 27 rows are the
 * decomp's, in FTKind order.
 *
 * DIVERGES only in where the file base points: gFTDataCaptainMainMotion
 * (ft/ftchar/ftcaptain/ftcaptain.c:7, a file the port does not compile)
 * and the reloc offset are defined here, and ftCommonCaptureCaptainBind-
 * Offsets sets the base so that base + (intptr_t)&offset lands on the
 * table -- the same arithmetic the host tests use to put a WPAttributes
 * under a weapon's file base, done once per overlay load below because it
 * is not a constant expression. On the N64 the offset is 0x0 into the
 * file: this table is the first thing in it. */
Vec2h dCaptainMainMotion_0x0000[nFTKindEnumCount] = {
	{ 0x001E, 0x0046 }, /* Mario   */
	{ 0x0028, 0x0028 }, /* Fox     */
	{ 0x0064, 0x00FA }, /* Donkey  */
	{ 0x0050, 0x00D2 }, /* Samus   */
	{ 0x001E, 0x0046 }, /* Luigi   */
	{ 0x0064, 0x00A0 }, /* Link    */
	{ 0x0000, 0x006E }, /* Yoshi   */
	{ 0x0050, 0x00D2 }, /* Captain */
	{ 0x0014, 0x0064 }, /* Kirby   */
	{ -0x000A, 0x0050 }, /* Pikachu */
	{ 0x0014, 0x0064 }, /* Purin   */
	{ 0x001E, 0x0046 }, /* Ness    */
	{ 0x0000, 0x0000 }, /* Boss    */
	{ 0x001E, 0x0046 }, /* MMario  */
	{ 0x001E, 0x0046 }, /* NMario  */
	{ 0x0028, 0x0028 }, /* NFox    */
	{ 0x0078, 0x0104 }, /* NDonkey */
	{ 0x0050, 0x00D2 }, /* NSamus  */
	{ 0x001E, 0x0046 }, /* NLuigi  */
	{ 0x0064, 0x00A0 }, /* NLink   */
	{ 0x0000, 0x0000 }, /* NYoshi  */
	{ 0x0050, 0x00D2 }, /* NCaptain*/
	{ 0x0014, 0x0064 }, /* NKirby  */
	{ -0x000A, 0x0050 }, /* NPikachu*/
	{ 0x0014, 0x0064 }, /* NPurin  */
	{ 0x0032, 0x0096 }, /* NNess   */
	{ 0x0064, 0x00FA }, /* GDonkey */
};

void *gFTDataCaptainMainMotion;
int llCaptainMainMotionSpecialHiVec2h;

void ftCommonCaptureCaptainBindOffsets(void)
{
    gFTDataCaptainMainMotion =
        (void*)((char*)dCaptainMainMotion_0x0000 - (intptr_t)&llCaptainMainMotionSpecialHiVec2h);
}

/* ---- relocData/208_FoxMainMotion.c:1605-1614 dFoxMainMotion_LwReflector-
 * FTSpecialColl: the Reflector's hurt sphere, the one piece
 * of Fox's motion file the code reads directly. ftFoxSpecialLwStartInit-
 * StatusVars (ft/ftchar/ftfox/ftfoxspeciallw.c:284-294, compiled
 * unmodified) points fp->special_coll at gFTDataFoxMainMotion
 * &llFoxMainMotionLwReflectorFTSpecialColl, and ft/ftmain.c's reflect
 * dispatch reads its kind. The same shape as Captain's grab offsets
 * above: the decomp has the bytes as C, so they are copied here verbatim
 * (kind 0 is nFTSpecialCollKindFoxReflector), and the file base --
 * gFTDataFoxMainMotion is ftfox.c's own, a real global the port compiles
 * -- is bound so the unmodified read lands on the sphere.
 *
 * DIVERGES only in where the file base points, as above; the bind runs
 * at every overlay load because ftfox.o's bss is in the segment the
 * reload bzeroes (overlay 2), and this hook (overlay 3) runs after it. */
FTSpecialColl dFoxMainMotion_LwReflectorFTSpecialColl = {
	/* kind          */ 0,
	/* joint_id      */ 4,
	/* offset        */ { 0.0F, 60.0F, 0.0F },
	/* size          */ { 350.0F, 350.0F, 350.0F },
	/* damage_resist */ 50,
};

int llFoxMainMotionLwReflectorFTSpecialColl;

void ftFoxReflectorBindOffsets(void)
{
    gFTDataFoxMainMotion =
        (void*)((char*)&dFoxMainMotion_LwReflectorFTSpecialColl - (intptr_t)&llFoxMainMotionLwReflectorFTSpecialColl);
}

/* ---- relocData/228_KirbyMainMotion.c: the two tables of Kirby's motion
 * file the code reads directly.
 *
 * @0x0000 FTKirbyCopy[27], llKirbyMainMotionSpecialNFTKirbyCopy: which
 * model part a swallowed fighter gives Kirby and the star's
 * damage when he spits one out (ftkirbyspecialn.c:113 and :172,
 * ftcommoncapturekirby.c:478).
 * @0x1220 ftKirbyAttack100Effect[6], llKirbyMainMotionftKirbyAttack100-
 * Effect: where each rapid-jab spark appears and how it flies
 * (ftcommonattack100.c:91).
 *
 * Captain's and Fox's bindings above point the file base at one table,
 * which works because each of their files has one. This file has two, so
 * the file itself is laid out -- both tables at the offsets the N64 file
 * has them, zeroes between -- and the two ll symbols are those offsets as
 * absolute symbols, as wpattrs.ld makes the weapons'. Until this, both
 * were `int` stand-ins with the base unbound: every read was the .bss
 * after a 4-byte int, which tools/check/standin_check.py now refuses.
 *
 * DIVERGES only in where the file base points. The values are the
 * decomp's, verbatim; the offsets are reloc_data.us.h's, and
 * standin_check.py holds the `.set` lines below to it. */
typedef struct FTKirbyMainMotionImage
{
    FTKirbyCopy copy[27];
    u8 gap[0x1220 - (27 * 12)];
    f32 attack100_effect[36];

} FTKirbyMainMotionImage;

static FTKirbyMainMotionImage sFTKirbyMainMotionImage =
{
    {
        { nFTKindMario, 12, 1.5F, 17 },
        { nFTKindFox, 7, 1.5F, 17 },
        { nFTKindDonkey, 4, 2.0F, 30 },
        { nFTKindSamus, 8, 1.6F, 17 },
        { nFTKindLuigi, 11, 1.6F, 17 },
        { nFTKindLink, 10, 1.5F, 17 },
        { nFTKindYoshi, 5, 1.7F, 25 },
        { nFTKindCaptain, 9, 1.7F, 17 },
        { nFTKindKirby, 0, 1.6F, 17 },
        { nFTKindPikachu, 6, 1.5F, 17 },
        { nFTKindPurin, 3, 1.6F, 17 },
        { nFTKindNess, 13, 1.6F, 17 },
        { nFTKindKirby, 0, 1.0F, 17 },
        { nFTKindKirby, 0, 1.5F, 17 },
        { nFTKindKirby, 0, 1.5F, 17 },
        { nFTKindKirby, 0, 1.5F, 17 },
        { nFTKindKirby, 0, 2.0F, 30 },
        { nFTKindKirby, 0, 1.6F, 17 },
        { nFTKindKirby, 0, 1.6F, 17 },
        { nFTKindKirby, 0, 1.5F, 17 },
        { nFTKindKirby, 0, 1.7F, 17 },
        { nFTKindKirby, 0, 1.7F, 17 },
        { nFTKindKirby, 0, 1.6F, 17 },
        { nFTKindKirby, 0, 1.5F, 17 },
        { nFTKindKirby, 0, 1.6F, 17 },
        { nFTKindKirby, 0, 1.6F, 17 },
        { nFTKindDonkey, 4, 2.0F, 50 },
    },
    { 0 },
    {
        -30.0F, 200.0F, 300.0F, 0.0F, 180.0F, -30.0F,
        -30.0F, 240.0F, 270.0F, 19.0F, 180.0F, -30.0F,
        -30.0F, 160.0F, 300.0F, -13.0F, 180.0F, -30.0F,
        -30.0F, 240.0F, 300.0F, 10.0F, 180.0F, -30.0F,
        -30.0F, 160.0F, 270.0F, -10.0F, 180.0F, -30.0F,
        -30.0F, 200.0F, 300.0F, 5.0F, 180.0F, -30.0F,
    },
};

_Static_assert(sizeof(FTKirbyCopy) == 12, "FTKirbyCopy is 12 bytes in the file");
_Static_assert(sizeof(ftKirbyAttack100Effect) == 24,
               "ftKirbyAttack100Effect is 24 bytes in the file");
_Static_assert(offsetof(FTKirbyMainMotionImage, attack100_effect) == 0x1220,
               "the rapid-jab table sits at 0x1220 in the file");

#define FTCOMMON_ASM_STR_(x) #x
#define FTCOMMON_ASM_STR(x) FTCOMMON_ASM_STR_(x)
#define FTCOMMON_ASM_SYM(name) FTCOMMON_ASM_STR(__USER_LABEL_PREFIX__) #name

__asm__(".globl " FTCOMMON_ASM_SYM(llKirbyMainMotionSpecialNFTKirbyCopy) "\n"
        ".set " FTCOMMON_ASM_SYM(llKirbyMainMotionSpecialNFTKirbyCopy) ", 0x0\n"
        ".globl " FTCOMMON_ASM_SYM(llKirbyMainMotionftKirbyAttack100Effect) "\n"
        ".set " FTCOMMON_ASM_SYM(llKirbyMainMotionftKirbyAttack100Effect) ", 0x1220\n");

void ftKirbyMainMotionBindOffsets(void)
{
    gFTDataKirbyMainMotion = &sFTKirbyMainMotionImage;
}

/* ---- relocData 238_NessMainMotion.c: Ness's two FTSpecialColls ----
 *
 * @0x1114 dNessMainMotion_AttackS4ReflectorFTSpecialColl: the bat's
 * reflector (ftcommonattacks4.c:120, ftCommonAttackS4SetStatus).
 * @0x16D4 dNessMainMotion_LwAbsorbFTSpecialColl: PSI Magnet's absorb
 * sphere (ftnessspeciallw.c, ftNessSpecialLwStartSetStatus's
 * special_coll).
 *
 * The same two-table image as Kirby's above, for the same reason: one
 * file, two offsets. DIVERGES only in where the file base points; the
 * values are the decomp's (US), the offsets reloc_data.us.h's. */
typedef struct FTNessMainMotionImage
{
    u8 gap0[0x1114];
    FTSpecialColl attacks4_reflector;
    u8 gap1[0x16D4 - 0x1114 - sizeof(FTSpecialColl)];
    FTSpecialColl lw_absorb;

} FTNessMainMotionImage;

static FTNessMainMotionImage sFTNessMainMotionImage =
{
    { 0 },
    { 2, 0, { 0.0F, 150.0F, 0.0F }, { 300.0F, 300.0F, 300.0F }, 1000 },
    { 0 },
    { 1, 0, { 300.0F, 195.0F, 0.0F }, { 430.0F, 430.0F, 430.0F }, 0 },
};

_Static_assert(offsetof(FTNessMainMotionImage, attacks4_reflector) == 0x1114,
               "the bat's reflector sits at 0x1114 in the file");
_Static_assert(offsetof(FTNessMainMotionImage, lw_absorb) == 0x16D4,
               "PSI Magnet's absorb sits at 0x16D4 in the file");

__asm__(".globl " FTCOMMON_ASM_SYM(llNessMainMotionAttackS4ReflectorFTSpecialColl) "\n"
        ".set " FTCOMMON_ASM_SYM(llNessMainMotionAttackS4ReflectorFTSpecialColl) ", 0x1114\n"
        ".globl " FTCOMMON_ASM_SYM(llNessMainMotionLwAbsorbFTSpecialColl) "\n"
        ".set " FTCOMMON_ASM_SYM(llNessMainMotionLwAbsorbFTSpecialColl) ", 0x16d4\n");

void ftNessMainMotionBindOffsets(void)
{
    gFTNessFileMainMotion = &sFTNessMainMotionImage;
}

/* ---- ft/ftchar/ftcaptain/ftcaptainstatus.h dFTCaptainSpecialStatusDescs,
 * statuses 228-238: Falcon Punch (228-229,), Falcon Kick
 * (230-234,) and Falcon Dive (235-238,-- his
 * Up-B, a grab: Hi and AirHi share every proc column, Catch drags the
 * caught body in through ftCaptainSpecialHiCatchProcPhysics and the
 * caught side's row 179 above, Throw is three common leaves; the whole
 * of Captain's special-move set, the fourth fighter's after Mario's,
 * Purin's and Donkey's), Captain's Neutral-B, Down-B and Up-B --
 * dFTMainSpecialStatusDescs[nFTKindCaptain] was FT_SPECIAL_NONE before step
 * 45 (the Neutral-B demux's slot 7 has carried ftCaptainSpecialNSetStatus's
 * name in a comment since); dFTCommonSpecialLwStatusList's
 * Captain/NCaptain slots have carried ftCaptainSpecialLwSetStatus's the
 * same way since the Down-B demux itself was built, so filling rows
 * 230-234 alone reaches it. ftcaptainspecialn.c's eleven procs (228-229)
 * and ftcaptainspeciallw.c's nineteen (230-234) are both decomp-verbatim;
 * the two new leaves are efManagerCaptainFalconPunchMakeEffect (step 45)
 * and efManagerCaptainFalconKickMakeEffect (this step), each called from
 * inside a ProcPhysics rather than any table column -- unlike a
 * ProcInterrupt or ProcMap, an effect attach has no FTStatusDesc slot of
 * its own.
 *
 * Falcon Kick is five statuses, not two: SpecialLw (grounded), SpecialLwAir
 * (grounded kick that runs off an edge mid-move -- ProcMap alone differs
 * from SpecialLw, everything else shared), SpecialLwLanding (landing after
 * the aerial version, its own ProcPhysics and ProcMap), SpecialAirLw (the
 * genuine aerial input, its own ProcPhysics/ProcMap pair), SpecialLwBound
 * (bouncing off a wall mid-air, another ProcPhysics/ProcMap pair). None of
 * the five motions carry FTANIM_FLAG_XROTN_JOINT in ft/ftdata.c (unlike
 * Falcon Punch's ground half above), even though SpecialLw/SpecialLwAir's
 * shared ProcPhysics calls ftPhysicsApplyGroundVelTransN -- the first of
 * Captain's own table rows needing no mock-motion TransN entry at all. */
FTStatusDesc dFTCaptainSpecialStatusDescs[nFTCaptainStatusSpecialAirHi - nFTCommonStatusSpecialStart + 1] = {
    /* ftcaptainstatus.h status 220, the third jab. Without it the jab
 * combo stopped at the second hit, and Attack13's interrupt is the
 * only way into the rapid jab below. */
    [nFTCaptainStatusAttack13 - nFTCommonStatusSpecialStart] = { { nFTCaptainMotionAttack13, nFTMotionAttackIDAttack13 }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDAttack13 } },
        ftCommonAttack13ProcUpdate, ftCommonAttack13ProcInterrupt,
        ftPhysicsApplyGroundVelTransN, mpCommonSetFighterFallOnEdgeBreak },
    /* the rapid jab. Captain Falcon's is the one that is
 * not the others': his physics is ftPhysicsApplyGroundVelTransN
 * where the other four use the friction, because he walks forward
 * while he jabs. */
    /* Captain Attack100Start */
    [nFTCaptainStatusAttack100Start - nFTCommonStatusSpecialStart] = { { nFTCaptainMotionAttack100Start, nFTMotionAttackIDAttack100 }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDAttack100 } },
        ftCommonAttack100StartProcUpdate, NULL,
        ftPhysicsApplyGroundVelTransN, mpCommonSetFighterFallOnEdgeBreak },
    /* Captain Attack100Loop */
    [nFTCaptainStatusAttack100Loop - nFTCommonStatusSpecialStart] = { { nFTCaptainMotionAttack100Loop, nFTMotionAttackIDAttack100 }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDAttack100 } },
        ftCommonAttack100LoopProcUpdate, ftCommonAttack100LoopProcInterrupt,
        ftPhysicsApplyGroundVelTransN, mpCommonSetFighterFallOnEdgeBreak },
    /* Captain Attack100End */
    [nFTCaptainStatusAttack100End - nFTCommonStatusSpecialStart] = { { nFTCaptainMotionAttack100End, nFTMotionAttackIDAttack100 }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDAttack100 } },
        ftAnimEndSetWait, NULL,
        ftPhysicsApplyGroundVelTransN, mpCommonSetFighterFallOnEdgeBreak },

    /* 224 CaptainAppearRStart */
    [nFTCaptainStatusAppearRStart - nFTCommonStatusSpecialStart] = { { nFTCaptainMotionAppearRStart, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCaptainAppearStartProcUpdate, NULL,
        ftCommonAppearProcPhysics, mpCommonUpdateFighterProjectFloor },
    /* 225 CaptainAppearLStart */
    [nFTCaptainStatusAppearLStart - nFTCommonStatusSpecialStart] = { { nFTCaptainMotionAppearLStart, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCaptainAppearStartProcUpdate, NULL,
        ftCommonAppearProcPhysics, mpCommonUpdateFighterProjectFloor },
    /* 226 CaptainAppearREnd */
    [nFTCaptainStatusAppearREnd - nFTCommonStatusSpecialStart] = { { nFTCaptainMotionAppearREnd, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonAppearProcUpdate, NULL,
        ftCommonAppearProcPhysics, mpCommonUpdateFighterProjectFloor },
    /* 227 CaptainAppearLEnd */
    [nFTCaptainStatusAppearLEnd - nFTCommonStatusSpecialStart] = { { nFTCaptainMotionAppearLEnd, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonAppearProcUpdate, NULL,
        ftCommonAppearProcPhysics, mpCommonUpdateFighterProjectFloor },
    [nFTCaptainStatusSpecialN - nFTCommonStatusSpecialStart] = { { nFTCaptainMotionSpecialN, nFTMotionAttackIDSpecialN }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDSpecialN } },
        ftAnimEndSetWait, NULL, ftCaptainSpecialNProcPhysics,
        ftCaptainSpecialNProcMap },
    [nFTCaptainStatusSpecialAirN - nFTCommonStatusSpecialStart] = { { nFTCaptainMotionSpecialAirN, nFTMotionAttackIDSpecialN }, { { 0, FALSE, nMPKineticsAir, FALSE, nFTStatusAttackIDSpecialN } },
        ftAnimEndSetFall, NULL, ftCaptainSpecialAirNProcPhysics,
        ftCaptainSpecialAirNProcMap },
    [nFTCaptainStatusSpecialLw - nFTCommonStatusSpecialStart] = { { nFTCaptainMotionSpecialLw, nFTMotionAttackIDSpecialLw }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDSpecialLw } },
        ftCaptainSpecialLwProcUpdate, NULL, ftCaptainSpecialLwProcPhysics,
        ftCaptainSpecialLwProcMap },
    [nFTCaptainStatusSpecialLwAir - nFTCommonStatusSpecialStart] = { { nFTCaptainMotionSpecialLwAir, nFTMotionAttackIDSpecialLw }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDSpecialLw } },
        ftCaptainSpecialLwProcUpdate, NULL, ftCaptainSpecialLwProcPhysics,
        ftCaptainSpecialLwAirProcMap },
    [nFTCaptainStatusSpecialLwLanding - nFTCommonStatusSpecialStart] = { { nFTCaptainMotionSpecialLwLanding, nFTMotionAttackIDSpecialLw }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDSpecialLw } },
        ftAnimEndSetWait, NULL, ftCaptainSpecialLwLandingProcPhysics,
        mpCommonSetFighterFallOnEdgeBreak },
    [nFTCaptainStatusSpecialAirLw - nFTCommonStatusSpecialStart] = { { nFTCaptainMotionSpecialAirLw, nFTMotionAttackIDSpecialLw }, { { 0, FALSE, nMPKineticsAir, FALSE, nFTStatusAttackIDSpecialLw } },
        ftAnimEndSetFall, NULL, ftCaptainSpecialAirLwProcPhysics,
        ftCaptainSpecialAirLwProcMap },
    [nFTCaptainStatusSpecialLwBound - nFTCommonStatusSpecialStart] = { { nFTCaptainMotionSpecialLwBound, nFTMotionAttackIDSpecialLw }, { { 0, FALSE, nMPKineticsAir, FALSE, nFTStatusAttackIDSpecialLw } },
        ftAnimEndSetFall, NULL, ftCaptainSpecialLwBoundProcPhysics,
        mpCommonProcFighterWaitOrLanding },
    [nFTCaptainStatusSpecialHi - nFTCommonStatusSpecialStart] = { { nFTCaptainMotionSpecialHi, nFTMotionAttackIDSpecialHi }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDSpecialHi } },
        ftCaptainSpecialHiProcUpdate, ftCaptainSpecialHiProcInterrupt,
        ftCaptainSpecialHiProcPhysics, ftCaptainSpecialHiProcMap },
    [nFTCaptainStatusSpecialHiCatch - nFTCommonStatusSpecialStart] = { { nFTCaptainMotionSpecialHiCatch, nFTMotionAttackIDSpecialHi }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDSpecialHi } },
        ftCaptainSpecialHiCatchProcUpdate, NULL, ftCaptainSpecialHiCatchProcPhysics,
        mpCommonUpdateFighterProjectFloor },
    [nFTCaptainStatusSpecialHiThrow - nFTCommonStatusSpecialStart] = { { nFTCaptainMotionSpecialHiThrow, nFTMotionAttackIDSpecialHi }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDSpecialHi } },
        ftAnimEndSetFall, NULL, ftPhysicsApplyAirVelTransNAll,
        mpCommonProcFighterWaitOrLanding },
    [nFTCaptainStatusSpecialAirHi - nFTCommonStatusSpecialStart] = { { nFTCaptainMotionSpecialAirHi, nFTMotionAttackIDSpecialHi }, { { 0, FALSE, nMPKineticsAir, FALSE, nFTStatusAttackIDSpecialHi } },
        ftCaptainSpecialHiProcUpdate, ftCaptainSpecialHiProcInterrupt,
        ftCaptainSpecialHiProcPhysics, ftCaptainSpecialHiProcMap },
};

/* ---- ft/ftchar/ftyoshi/ftyoshistatus.h dFTYoshiSpecialStatusDescs,
 * statuses 224-227: Yoshi Bomb (Yoshi's Down-B), his
 * special-move table, dFTMainSpecialStatusDescs[nFTKindYoshi].
 * ftyoshispeciallw.c's ten procs and
 * wp/wpyoshi/wpyoshistar.c's nine (the two stars the landing throws, one
 * each way) are both decomp-verbatim, and the special needs no new leaf:
 * the star's own effects
 * (efManagerDustExpandSmallMakeEffect on expiry, efManagerSparkleWhite-
 * MakeEffect on a hit or a shield) are implemented. Every other callee --
 * ftPhysicsApplyAirVelTransNAll/GroundVel-
 * Friction/CheckClampAirVelXDecMax/ApplyAirVelXFriction, mpCommonCheck-
 * FighterCeilHeavy/CeilHeavyCliff, mpCommonSetFighterGround/Air/FallOn-
 * GroundBreak, ftCommonCliffCatchSetStatus/WaitSetStatus, ftAnimEndCheck-
 * SetStatus, ftMainSetStatus/PlayAnimEventsAll, gmCollisionGetFighter-
 * PartsWorldPosition -- was already in the port. The two Start motions (GroundPoundGroundStart and
 * GroundPoundAir, ft/ftdata.c:3858,3860) both carry the TransN joint bit
 * and their shared ProcPhysics reads it (ftPhysicsApplyAirVelTransNAll),
 * so both get a host mock-motion entry; Landing carries none, and the
 * Loop row has no motion at all: its Script ID is -1, which ftMainSetStatus
 * skips over verbatim, the caller (ftYoshiSpecialAirLwLoopSetStatus)
 * handing it the current anim_frame under FTSTATUS_PRESERVE_HIT so the
 * plunge keeps the hop's pose and hitboxes. Its kinetics column reads
 * nMPKineticsGround in the decomp even though the fighter is airborne
 * for the whole of it -- copied as it stands, not corrected. */
FTStatusDesc dFTYoshiSpecialStatusDescs[nFTYoshiStatusSpecialAirNRelease - nFTCommonStatusSpecialStart + 1] = {
    /* 220 YoshiAppearR */
    [nFTYoshiStatusAppearR - nFTCommonStatusSpecialStart] = { { nFTYoshiMotionAppearR, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonAppearProcUpdate, NULL,
        ftCommonAppearProcPhysics, mpCommonUpdateFighterProjectFloor },
    /* 221 YoshiAppearL */
    [nFTYoshiStatusAppearL - nFTCommonStatusSpecialStart] = { { nFTYoshiMotionAppearL, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonAppearProcUpdate, NULL,
        ftCommonAppearProcPhysics, mpCommonUpdateFighterProjectFloor },
    [nFTYoshiStatusSpecialLwStart - nFTCommonStatusSpecialStart] = { { nFTYoshiMotionSpecialLwStart, nFTMotionAttackIDSpecialLw }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDSpecialLw } },
        ftYoshiSpecialLwStartProcUpdate, NULL, ftPhysicsApplyAirVelTransNAll,
        ftYoshiSpecialLwStartProcMap },
    [nFTYoshiStatusSpecialLwLanding - nFTCommonStatusSpecialStart] = { { nFTYoshiMotionSpecialLwLanding, nFTMotionAttackIDSpecialLw }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDSpecialLw } },
        ftYoshiSpecialLwLandingProcUpdate, NULL, ftPhysicsApplyGroundVelFriction,
        mpCommonSetFighterFallOnGroundBreak },
    [nFTYoshiStatusSpecialAirLwStart - nFTCommonStatusSpecialStart] = { { nFTYoshiMotionSpecialAirLwStart, nFTMotionAttackIDSpecialLw }, { { 0, FALSE, nMPKineticsAir, FALSE, nFTStatusAttackIDSpecialLw } },
        ftYoshiSpecialLwStartProcUpdate, NULL, ftPhysicsApplyAirVelTransNAll,
        ftYoshiSpecialLwStartProcMap },
    [nFTYoshiStatusSpecialAirLwLoop - nFTCommonStatusSpecialStart] = { { -1, nFTMotionAttackIDSpecialLw }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDSpecialLw } },
        NULL, NULL, ftYoshiSpecialAirLwLoopProcPhysics,
        ftYoshiSpecialAirLwLoopProcMap },
    /* statuses 222-223: Egg Throw, Yoshi's Up-B -- the two
 * rows that finish his table, and the only pair of his that is
 * is_projectile TRUE (the thrown egg is the projectile, and the flag
 * is what ftMainProcSearch reads to let a reflector turn it). The
 * ground and air rows share nothing but that: each has its own
 * update, physics and map proc, because the ground throw and the air
 * throw are different moves -- the air one is Yoshi's only recovery.
 * Both procs are ftyoshispecialhi.c's, compiled unmodified. */
    [nFTYoshiStatusSpecialHi - nFTCommonStatusSpecialStart] = { { nFTYoshiMotionSpecialHi, nFTMotionAttackIDSpecialHi }, { { 0, FALSE, nMPKineticsGround, TRUE, nFTStatusAttackIDSpecialHi } },
        ftYoshiSpecialHiProcUpdate, NULL, ftYoshiSpecialHiProcPhysics,
        ftYoshiSpecialHiProcMap },
    [nFTYoshiStatusSpecialAirHi - nFTCommonStatusSpecialStart] = { { nFTYoshiMotionSpecialAirHi, nFTMotionAttackIDSpecialHi }, { { 0, FALSE, nMPKineticsAir, TRUE, nFTStatusAttackIDSpecialHi } },
        ftYoshiSpecialAirHiProcUpdate, NULL, ftYoshiSpecialAirHiProcPhysics,
        ftYoshiSpecialAirHiProcMap },
    /* statuses 228-233: Egg Lay, Yoshi's Neutral-B. Six
 * rows in two mirrored threes -- ground SpecialN/Catch/Release and
 * the air's, differing only in kinetics, in the air physics leaf,
 * and in the ground SpecialN ending into Wait where the air one ends
 * into Fall. Every proc is ftyoshispecialn.c's, compiled unmodified;
 * only Catch and Release have an update of their own, and the two
 * plain SpecialN rows use the animation-end leaves. All six carry
 * nFTMotionAttackIDSpecialN, which is what the tongue's hitbox is
 * staled against, and none is a smash. The caught fighter's two rows
 * (177 CaptureYoshi, 178 YoshiEgg) are in the common table below. */
    [nFTYoshiStatusSpecialN - nFTCommonStatusSpecialStart] = { { nFTYoshiMotionSpecialN, nFTMotionAttackIDSpecialN }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDSpecialN } },
        ftAnimEndSetWait, NULL, ftPhysicsApplyGroundVelFriction,
        ftYoshiSpecialNProcMap },
    [nFTYoshiStatusSpecialNCatch - nFTCommonStatusSpecialStart] = { { nFTYoshiMotionSpecialNCatch, nFTMotionAttackIDSpecialN }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDSpecialN } },
        ftYoshiSpecialNCatchProcUpdate, NULL, ftPhysicsApplyGroundVelFriction,
        ftYoshiSpecialNCatchProcMap },
    [nFTYoshiStatusSpecialNRelease - nFTCommonStatusSpecialStart] = { { nFTYoshiMotionSpecialNRelease, nFTMotionAttackIDSpecialN }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDSpecialN } },
        ftYoshiSpecialNReleaseProcUpdate, NULL, ftPhysicsApplyGroundVelFriction,
        ftYoshiSpecialNReleaseProcMap },
    [nFTYoshiStatusSpecialAirN - nFTCommonStatusSpecialStart] = { { nFTYoshiMotionSpecialAirN, nFTMotionAttackIDSpecialN }, { { 0, FALSE, nMPKineticsAir, FALSE, nFTStatusAttackIDSpecialN } },
        ftAnimEndSetFall, NULL, ftPhysicsApplyAirVelFriction,
        ftYoshiSpecialAirNProcMap },
    [nFTYoshiStatusSpecialAirNCatch - nFTCommonStatusSpecialStart] = { { nFTYoshiMotionSpecialAirNCatch, nFTMotionAttackIDSpecialN }, { { 0, FALSE, nMPKineticsAir, FALSE, nFTStatusAttackIDSpecialN } },
        ftYoshiSpecialAirNCatchProcUpdate, NULL, ftPhysicsApplyAirVelFriction,
        ftYoshiSpecialAirNCatchProcMap },
    [nFTYoshiStatusSpecialAirNRelease - nFTCommonStatusSpecialStart] = { { nFTYoshiMotionSpecialAirNRelease, nFTMotionAttackIDSpecialN }, { { 0, FALSE, nMPKineticsAir, FALSE, nFTStatusAttackIDSpecialN } },
        ftYoshiSpecialAirNReleaseProcUpdate, NULL, ftPhysicsApplyAirVelFriction,
        ftYoshiSpecialAirNReleaseProcMap },
};

/* ---- ft/ftchar/ftlink/ftlinkstatus.h dFTLinkSpecialStatusDescs,
 * statuses 220-234: Link's FIRST special-move table --
 * dFTMainSpecialStatusDescs[nFTKindLink] was FT_SPECIAL_NONE before this
 * step, and that was a LIVE BUG, not just a hole. ftCommonAttack13Set-
 * Status (above, ftcommonattack1.c:193) has named nFTLinkStatusAttack13
 * in its fkind switch since the jab was ported, so Link's third jab and
 * his rapid-jab already ran ftMainSetStatus with status 220 -- into a
 * fighter with no table at all, where ftMainGetStatusDesc returns NULL
 * and ftMainSetStatus aborts naming the number. Row 220 fixes that, and
 * it costs nothing: ftAnimEndSetWait and the two leaves beside it have
 * been in the port since. Ness's arm of that switch, which
 * names nFTNessStatusAttack13, has its row the same way.
 *
 * Rows 224-225 are AppearR/L, the entrance animation, on the common
 * appear procs; ftCommonAppearSetStatus picks them through
 * dFTCommonEntryAppearStatusIDs, filled for every playable kind.
 * Rows 229-234 are the Neutral-B: the boomerang throw and
 * its return, ground and air, each in three -- the throw (SpecialN), the
 * catch (SpecialNGet / SpecialAirNReturn) and the empty-handed throw
 * animation played when one is already out (SpecialNEmpty). All six are
 * is_projectile TRUE, which is what lets a reflector turn the boomerang.
 * Their procs are ft/ftchar/ftlink/ftlinkspecialn.c's, compiled
 * unmodified.
 *
 * Rows 226-228 (SpecialHi, SpecialHiEnd, SpecialAirHi) are the Spin
 * Attack, live since and 235-236 (SpecialLw, SpecialAirLw)
 * pull out the Bomb since: ftlinkspeciallw.c's procs, and
 * it/itfighter/itlinkbomb.c's item, both compiled unmodified. */
FTStatusDesc dFTLinkSpecialStatusDescs[nFTLinkStatusSpecialAirLw - nFTCommonStatusSpecialStart + 1] = {
    /* the rapid jab: three rows, the procs common and the
 * motion this fighter's own. */
    /* Link Attack100Start */
    [nFTLinkStatusAttack100Start - nFTCommonStatusSpecialStart] = { { nFTLinkMotionAttack100Start, nFTMotionAttackIDAttack100 }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDAttack100 } },
        ftCommonAttack100StartProcUpdate, NULL,
        ftPhysicsApplyGroundVelFriction, mpCommonSetFighterFallOnEdgeBreak },
    /* Link Attack100Loop */
    [nFTLinkStatusAttack100Loop - nFTCommonStatusSpecialStart] = { { nFTLinkMotionAttack100Loop, nFTMotionAttackIDAttack100 }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDAttack100 } },
        ftCommonAttack100LoopProcUpdate, ftCommonAttack100LoopProcInterrupt,
        ftPhysicsApplyGroundVelFriction, mpCommonSetFighterFallOnEdgeBreak },
    /* Link Attack100End */
    [nFTLinkStatusAttack100End - nFTCommonStatusSpecialStart] = { { nFTLinkMotionAttack100End, nFTMotionAttackIDAttack100 }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDAttack100 } },
        ftAnimEndSetWait, NULL,
        ftPhysicsApplyGroundVelFriction, mpCommonSetFighterFallOnEdgeBreak },

    /* 220 LinkAttack13 */
    [nFTLinkStatusAttack13 - nFTCommonStatusSpecialStart] = { { nFTLinkMotionAttack13, nFTMotionAttackIDAttack13 }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDAttack13 } },
        ftAnimEndSetWait, NULL,
        ftPhysicsApplyGroundVelFriction, mpCommonSetFighterFallOnEdgeBreak },
    /* 224 LinkAppearR */
    [nFTLinkStatusAppearR - nFTCommonStatusSpecialStart] = { { nFTLinkMotionAppearR, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonAppearProcUpdate, NULL,
        ftCommonAppearProcPhysics, mpCommonUpdateFighterProjectFloor },
    /* 225 LinkAppearL */
    [nFTLinkStatusAppearL - nFTCommonStatusSpecialStart] = { { nFTLinkMotionAppearL, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonAppearProcUpdate, NULL,
        ftCommonAppearProcPhysics, mpCommonUpdateFighterProjectFloor },
    /* 226 LinkSpecialHi */
    [nFTLinkStatusSpecialHi - nFTCommonStatusSpecialStart] = { { nFTLinkMotionSpecialHi, nFTMotionAttackIDSpecialHi }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDSpecialHi } },
        ftLinkSpecialHiProcUpdate, NULL,
        ftLinkSpecialHiProcPhysics, ftLinkSpecialHiProcMap },
    /* 227 LinkSpecialHiEnd */
    [nFTLinkStatusSpecialHiEnd - nFTCommonStatusSpecialStart] = { { nFTLinkMotionSpecialHiEnd, nFTMotionAttackIDSpecialHi }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDSpecialHi } },
        ftLinkSpecialHiEndProcUpdate, NULL,
        ftPhysicsApplyGroundVelFriction, ftLinkSpecialHiEndProcMap },
    /* 228 LinkSpecialAirHi */
    [nFTLinkStatusSpecialAirHi - nFTCommonStatusSpecialStart] = { { nFTLinkMotionSpecialAirHi, nFTMotionAttackIDSpecialHi }, { { 0, FALSE, nMPKineticsAir, FALSE, nFTStatusAttackIDSpecialHi } },
        ftLinkSpecialAirHiProcUpdate, NULL,
        ftLinkSpecialAirHiProcPhysics, ftLinkSpecialAirHiProcMap },
    /* 229 LinkSpecialN */
    [nFTLinkStatusSpecialN - nFTCommonStatusSpecialStart] = { { nFTLinkMotionSpecialN, nFTMotionAttackIDSpecialN }, { { 0, FALSE, nMPKineticsGround, TRUE, nFTStatusAttackIDSpecialN } },
        ftLinkSpecialNProcUpdate, NULL,
        ftPhysicsApplyGroundVelFriction, ftLinkSpecialNProcMap },
    /* 230 LinkSpecialNGet */
    [nFTLinkStatusSpecialNGet - nFTCommonStatusSpecialStart] = { { nFTLinkMotionSpecialNGet, nFTMotionAttackIDSpecialN }, { { 0, FALSE, nMPKineticsGround, TRUE, nFTStatusAttackIDSpecialN } },
        ftAnimEndSetWait, NULL,
        ftPhysicsApplyGroundVelFriction, mpCommonSetFighterFallOnGroundBreak },
    /* 231 LinkSpecialNEmpty */
    [nFTLinkStatusSpecialNEmpty - nFTCommonStatusSpecialStart] = { { nFTLinkMotionSpecialNEmpty, nFTMotionAttackIDSpecialN }, { { 0, FALSE, nMPKineticsGround, TRUE, nFTStatusAttackIDSpecialN } },
        ftAnimEndSetWait, NULL,
        ftPhysicsApplyGroundVelFriction, ftLinkSpecialNEmptyProcMap },
    /* 232 LinkSpecialAirN */
    [nFTLinkStatusSpecialAirN - nFTCommonStatusSpecialStart] = { { nFTLinkMotionSpecialAirN, nFTMotionAttackIDSpecialN }, { { 0, FALSE, nMPKineticsAir, TRUE, nFTStatusAttackIDSpecialN } },
        ftLinkSpecialAirNProcUpdate, NULL,
        ftPhysicsApplyAirVelFriction, ftLinkSpecialAirNProcMap },
    /* 233 LinkSpecialAirNReturn */
    [nFTLinkStatusSpecialAirNReturn - nFTCommonStatusSpecialStart] = { { nFTLinkMotionSpecialAirNReturn, nFTMotionAttackIDSpecialN }, { { 0, FALSE, nMPKineticsAir, TRUE, nFTStatusAttackIDSpecialN } },
        ftAnimEndSetFall, NULL,
        ftPhysicsApplyAirVelFriction, mpCommonProcFighterWaitOrLanding },
    /* 234 LinkSpecialAirNEmpty */
    [nFTLinkStatusSpecialAirNEmpty - nFTCommonStatusSpecialStart] = { { nFTLinkMotionSpecialAirNEmpty, nFTMotionAttackIDSpecialN }, { { 0, FALSE, nMPKineticsAir, TRUE, nFTStatusAttackIDSpecialN } },
        ftAnimEndSetFall, NULL,
        ftPhysicsApplyAirVelFriction, ftLinkSpecialAirNEmptyProcMap },
    /* 235 LinkSpecialLw */
    [nFTLinkStatusSpecialLw - nFTCommonStatusSpecialStart] = { { nFTLinkMotionSpecialLw, nFTMotionAttackIDSpecialLw }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDSpecialLw } },
        ftLinkSpecialLwProcUpdate, NULL,
        ftPhysicsApplyGroundVelFriction, ftLinkSpecialLwProcMap },
    /* 236 LinkSpecialAirLw */
    [nFTLinkStatusSpecialAirLw - nFTCommonStatusSpecialStart] = { { nFTLinkMotionSpecialAirLw, nFTMotionAttackIDSpecialLw }, { { 0, FALSE, nMPKineticsAir, FALSE, nFTStatusAttackIDSpecialLw } },
        ftLinkSpecialAirLwProcUpdate, NULL,
        ftPhysicsApplyAirVelFriction, ftLinkSpecialAirLwProcMap },
};

/* ---- ft/ftchar/ftluigi/ftluigistatus.h dFTLuigiSpecialStatusDescs,
 * statuses 220-228: Luigi's whole special table, and the
 * only one in the roster that needs no new code at all. He has no
 * ft/ftchar/ftluigi/ftluigispecial*.c: every proc below is Mario's, out
 * of the three files (ftmariospecialn.c, ftmariospecialhi.c,
 * ftmariospeciallw.c) are compiled unmodified, and his enum is Mario's
 * laid out row for row. What is his own
 * is the motion ids -- the animations his own pack carries -- and, for
 * the fireball, the WPDesc ftMarioSpecialNMakeFireball picks off
 * fp->fkind (wp/wpmario/wpmariofireball.c:30-46, gFTDataLuigiSpecial1,
 * which ftluigi.c has defined since).
 *
 * That is exactly why it was a LIVE ABORT and not a hole. Mario's four
 * setters do not branch: ftMarioSpecialNSetStatus sets the NUMBER 223
 * and lets dFTMainSpecialStatusDescs[fkind] decide whose table 223 means.
 * Luigi's six demux slots (SpecialN, SpecialHi, SpecialAirN, SpecialAirHi
 * and SpecialAirLw all hold Mario's setters) therefore depend on this
 * table being filled: with FT_SPECIAL_NONE ftMainGetStatusDesc returns
 * NULL and ftMainSetStatus prints the status id and aborts. The ground
 * Down-B slot is the one left NULL in dFTCommonSpecialLwStatusList above,
 * which names this table. The table and that slot together make all six
 * live.
 *
 * This table is IDENTICAL to Mario's, and not only in shape: parse both
 * out of the decomp and rename Mario to Luigi throughout and the two
 * agree line for line. The motion ids agree numerically too -- both
 * enums are the same nine names in the same order off
 * nFTCommonMotionSpecialStart -- so the port could point
 * dFTMainSpecialStatusDescs[nFTKindLuigi] at dFTMarioSpecialStatusDescs
 * and be numerically correct. It does not, because the game has two
 * arrays; every difference between the two fighters lives somewhere
 * else -- in Luigi's own motion FILE behind the same ids, in his
 * attributes, and in dWPMarioFireballWeaponAttributes[1], the row
 * ftMarioSpecialNProcAccessory picks off fkind (zero gravity, 80 tics,
 * a flat launch, against Mario's 1.2, 140 and -5 degrees).
 *
 * Copied as it stands, not corrected: the decomp writes the STATUS
 * attack id column of rows 221-228 with nFTMotionAttackID* spellings
 * rather than nFTStatusAttackID*, exactly as it does in Mario's rows
 * 221-228, where the port already recorded it. The two enums are
 * numbered differently -- which would matter -- except that the ids
 * these rows use happen to coincide (None 0, SpecialHi 17, SpecialN 18,
 * SpecialLw 30). The one id where they do NOT coincide is Attack13
 * (motion 3, status 15), and row 220 is the one row written with the
 * right enum. */
FTStatusDesc dFTLuigiSpecialStatusDescs[nFTLuigiStatusSpecialAirLw - nFTCommonStatusSpecialStart + 1] = {
    /* 220 LuigiAttack13 */
    [nFTLuigiStatusAttack13 - nFTCommonStatusSpecialStart] = { { nFTLuigiMotionAttack13, nFTMotionAttackIDAttack13 }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDAttack13 } },
        ftAnimEndSetWait, NULL,
        ftPhysicsApplyGroundVelFriction, mpCommonSetFighterFallOnEdgeBreak },
    /* 221 LuigiAppearR */
    [nFTLuigiStatusAppearR - nFTCommonStatusSpecialStart] = { { nFTLuigiMotionAppearR, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTMotionAttackIDNone } },
        ftCommonAppearProcUpdate, NULL,
        ftCommonAppearProcPhysics, mpCommonUpdateFighterProjectFloor },
    /* 222 LuigiAppearL */
    [nFTLuigiStatusAppearL - nFTCommonStatusSpecialStart] = { { nFTLuigiMotionAppearL, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTMotionAttackIDNone } },
        ftCommonAppearProcUpdate, NULL,
        ftCommonAppearProcPhysics, mpCommonUpdateFighterProjectFloor },
    /* 223 LuigiSpecialN */
    [nFTLuigiStatusSpecialN - nFTCommonStatusSpecialStart] = { { nFTLuigiMotionSpecialN, nFTMotionAttackIDSpecialN }, { { 0, FALSE, nMPKineticsGround, TRUE, nFTMotionAttackIDSpecialN } },
        ftMarioSpecialNProcUpdate, NULL,
        ftPhysicsApplyGroundVelFriction, ftMarioSpecialNProcMap },
    /* 224 LuigiSpecialAirN */
    [nFTLuigiStatusSpecialAirN - nFTCommonStatusSpecialStart] = { { nFTLuigiMotionSpecialAirN, nFTMotionAttackIDSpecialN }, { { 0, FALSE, nMPKineticsAir, TRUE, nFTMotionAttackIDSpecialN } },
        ftMarioSpecialNProcUpdate, NULL,
        ftPhysicsApplyAirVelDrift, ftMarioSpecialAirNProcMap },
    /* 225 LuigiSpecialHi */
    [nFTLuigiStatusSpecialHi - nFTCommonStatusSpecialStart] = { { nFTLuigiMotionSpecialHi, nFTMotionAttackIDSpecialHi }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTMotionAttackIDSpecialHi } },
        ftMarioSpecialHiProcUpdate, ftMarioSpecialHiProcInterrupt,
        ftMarioSpecialHiProcPhysics, ftMarioSpecialHiProcMap },
    /* 226 LuigiSpecialAirHi */
    [nFTLuigiStatusSpecialAirHi - nFTCommonStatusSpecialStart] = { { nFTLuigiMotionSpecialAirHi, nFTMotionAttackIDSpecialHi }, { { 0, FALSE, nMPKineticsAir, FALSE, nFTMotionAttackIDSpecialHi } },
        ftMarioSpecialHiProcUpdate, ftMarioSpecialHiProcInterrupt,
        ftMarioSpecialHiProcPhysics, ftMarioSpecialHiProcMap },
    /* 227 LuigiSpecialLw */
    [nFTLuigiStatusSpecialLw - nFTCommonStatusSpecialStart] = { { nFTLuigiMotionSpecialLw, nFTMotionAttackIDSpecialLw }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTMotionAttackIDSpecialLw } },
        ftMarioSpecialLwProcUpdate, NULL,
        ftMarioSpecialLwProcPhysics, ftMarioSpecialLwProcMap },
    /* 228 LuigiSpecialAirLw */
    [nFTLuigiStatusSpecialAirLw - nFTCommonStatusSpecialStart] = { { nFTLuigiMotionSpecialAirLw, nFTMotionAttackIDSpecialLw }, { { 0, FALSE, nMPKineticsAir, FALSE, nFTMotionAttackIDSpecialLw } },
        ftMarioSpecialAirLwProcUpdate, NULL,
        ftMarioSpecialAirLwProcPhysics, ftMarioSpecialAirLwProcMap },
};

/* ---- ft/ftchar/ftness/ftnessstatus.h dFTNessSpecialStatusDescs,
 * statuses 220-225: Ness's table, and with it the LAST
 * live abort of the kind steps 64 and 65 found. ftCommonAttack13Set-
 * Status's fkind switch has named nFTNessStatusAttack13 since the jab
 * was ported, into FT_SPECIAL_NONE, where ftMainGetStatusDesc returns
 * NULL and ftMainSetStatus prints the id and calls abort(). Row 220
 * closes it. Ness is the third and last kind whose common code reached
 * a special table it did not have; the one NONE row left is Boss's,
 * which nothing in this port spawns (Pikachu's became a table of his
 * own at below).
 *
 * Rows 221-225 are the entrance, and Ness's is the longest in the game:
 * five statuses where everyone but Captain Falcon has two, because he
 * arrives inside the PSI ring -- AppearRStart/LStart while it closes,
 * AppearWait held inside it, AppearREnd/LEnd as it opens. Three of the
 * five need procs of Ness's own (ftNessAppearStart/WaitProcUpdate,
 * above with the rest of ftcommonentry.c); the other two take the common
 * update. They were correct and INERT when this step wrote them, like
 * Link's pair since, because ftCommonAppearSetStatus reads
 * dFTCommonEntryAppearStatusIDs for the status id and that table then
 * had a row only for Mario. The table is now filled and Ness warps in.
 *
 * Statuses 226-244 are his specials: PK Fire (226-227),
 * PK Thunder (228-236) and PSI Magnet (237-244), the game's rows. Their
 * procs are ftnessspecial{n,hi,lw}.c, compiled unmodified, and so are
 * the weapons and the fire pillar behind them (wpnesspk{fire,thunder}.c,
 * it/itfighter/itnesspkfire.c). */
FTStatusDesc dFTNessSpecialStatusDescs[nFTNessStatusSpecialAirLwEnd - nFTCommonStatusSpecialStart + 1] = {
    /* 220 NessAttack13 */
    [nFTNessStatusAttack13 - nFTCommonStatusSpecialStart] = { { nFTNessMotionAttack13, nFTMotionAttackIDAttack13 }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDAttack13 } },
        ftAnimEndSetWait, NULL,
        ftPhysicsApplyGroundVelFriction, mpCommonSetFighterFallOnEdgeBreak },
    /* 221 NessAppearRStart */
    [nFTNessStatusAppearRStart - nFTCommonStatusSpecialStart] = { { nFTNessMotionAppearRStart, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftNessAppearStartProcUpdate, NULL,
        ftCommonAppearProcPhysics, mpCommonUpdateFighterProjectFloor },
    /* 222 NessAppearLStart */
    [nFTNessStatusAppearLStart - nFTCommonStatusSpecialStart] = { { nFTNessMotionAppearLStart, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftNessAppearStartProcUpdate, NULL,
        ftCommonAppearProcPhysics, mpCommonUpdateFighterProjectFloor },
    /* 223 NessAppearWait */
    [nFTNessStatusAppearWait - nFTCommonStatusSpecialStart] = { { nFTNessMotionAppearWait, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftNessAppearWaitProcUpdate, NULL,
        ftCommonAppearProcPhysics, mpCommonUpdateFighterProjectFloor },
    /* 224 NessAppearREnd */
    [nFTNessStatusAppearREnd - nFTCommonStatusSpecialStart] = { { nFTNessMotionAppearREnd, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonAppearProcUpdate, NULL,
        ftCommonAppearProcPhysics, mpCommonUpdateFighterProjectFloor },
    /* 225 NessAppearLEnd */
    [nFTNessStatusAppearLEnd - nFTCommonStatusSpecialStart] = { { nFTNessMotionAppearLEnd, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonAppearProcUpdate, NULL,
        ftCommonAppearProcPhysics, mpCommonUpdateFighterProjectFloor },
    /* 226 NessSpecialN */
    [nFTNessStatusSpecialN - nFTCommonStatusSpecialStart] = { { nFTNessMotionSpecialN, nFTMotionAttackIDSpecialN }, { { 0, FALSE, nMPKineticsGround, TRUE, nFTStatusAttackIDSpecialN } },
        ftAnimEndSetWait, NULL,
        ftPhysicsApplyGroundVelFriction, ftNessSpecialNProcMap },
    /* 227 NessSpecialAirN */
    [nFTNessStatusSpecialAirN - nFTCommonStatusSpecialStart] = { { nFTNessMotionSpecialAirN, nFTMotionAttackIDSpecialN }, { { 0, FALSE, nMPKineticsAir, TRUE, nFTStatusAttackIDSpecialN } },
        ftAnimEndSetFall, NULL,
        ftPhysicsApplyAirVelFriction, ftNessSpecialAirNProcMap },
    /* 228 NessSpecialHiStart */
    [nFTNessStatusSpecialHiStart - nFTCommonStatusSpecialStart] = { { nFTNessMotionSpecialHiStart, nFTMotionAttackIDSpecialHi }, { { 0, FALSE, nMPKineticsGround, TRUE, nFTStatusAttackIDSpecialHi } },
        ftNessSpecialHiStartProcUpdate, NULL,
        ftNessSpecialHiProcPhysics, ftNessSpecialHiStartProcMap },
    /* 229 NessSpecialHiHold */
    [nFTNessStatusSpecialHiHold - nFTCommonStatusSpecialStart] = { { nFTNessMotionSpecialHiHold, nFTMotionAttackIDSpecialHi }, { { 0, FALSE, nMPKineticsGround, TRUE, nFTStatusAttackIDSpecialHi } },
        ftNessSpecialHiHoldProcUpdate, NULL,
        ftPhysicsApplyGroundVelFriction, ftNessSpecialHiHoldProcMap },
    /* 230 NessSpecialHiEnd */
    [nFTNessStatusSpecialHiEnd - nFTCommonStatusSpecialStart] = { { nFTNessMotionSpecialHiEnd, nFTMotionAttackIDSpecialHi }, { { 0, FALSE, nMPKineticsGround, TRUE, nFTStatusAttackIDSpecialHi } },
        ftAnimEndSetWait, NULL,
        ftPhysicsApplyGroundVelFriction, ftNessSpecialHiEndProcMap },
    /* 231 NessSpecialHiJibaku */
    [nFTNessStatusSpecialHiJibaku - nFTCommonStatusSpecialStart] = { { nFTNessMotionSpecialHiJibaku, nFTMotionAttackIDSpecialHi }, { { 0, FALSE, nMPKineticsGround, TRUE, nFTStatusAttackIDSpecialHi } },
        ftNessSpecialHiJibakuProcUpdate, NULL,
        ftNessSpecialHiJibakuProcPhysics, ftNessSpecialHiJibakuProcMap },
    /* 232 NessSpecialAirHiStart */
    [nFTNessStatusSpecialAirHiStart - nFTCommonStatusSpecialStart] = { { nFTNessMotionSpecialAirHiStart, nFTMotionAttackIDSpecialHi }, { { 0, FALSE, nMPKineticsAir, TRUE, nFTStatusAttackIDSpecialHi } },
        ftNessSpecialAirHiStartProcUpdate, NULL,
        ftNessSpecialAirHiProcPhysics, ftNessSpecialAirHiStartProcMap },
    /* 233 NessSpecialAirHiHold */
    [nFTNessStatusSpecialAirHiHold - nFTCommonStatusSpecialStart] = { { nFTNessMotionSpecialAirHiHold, nFTMotionAttackIDSpecialHi }, { { 0, FALSE, nMPKineticsAir, TRUE, nFTStatusAttackIDSpecialHi } },
        ftNessSpecialAirHiHoldProcUpdate, NULL,
        ftNessSpecialAirHiProcPhysics, ftNessSpecialAirHiHoldProcMap },
    /* 234 NessSpecialAirHiEnd */
    [nFTNessStatusSpecialAirHiEnd - nFTCommonStatusSpecialStart] = { { nFTNessMotionSpecialAirHiEnd, nFTMotionAttackIDSpecialHi }, { { 0, FALSE, nMPKineticsAir, TRUE, nFTStatusAttackIDSpecialHi } },
        ftNessSpecialAirHiEndProcUpdate, NULL,
        ftNessSpecialAirHiProcPhysics, ftNessSpecialAirHiEndProcMap },
    /* 235 NessSpecialAirHiBound */
    [nFTNessStatusSpecialAirHiBound - nFTCommonStatusSpecialStart] = { { nFTNessMotionSpecialAirHiBound, nFTMotionAttackIDSpecialHi }, { { 0, FALSE, nMPKineticsAir, TRUE, nFTStatusAttackIDSpecialHi } },
        ftNessSpecialAirHiJibakuBoundProcUpdate, NULL,
        ftPhysicsApplyAirVelFriction, ftNessSpecialAirHiJibakuBoundProcMap },
    /* 236 NessSpecialAirHiJibaku */
    [nFTNessStatusSpecialAirHiJibaku - nFTCommonStatusSpecialStart] = { { nFTNessMotionSpecialAirHiJibaku, nFTMotionAttackIDSpecialHi }, { { 0, FALSE, nMPKineticsAir, TRUE, nFTStatusAttackIDSpecialHi } },
        ftNessSpecialAirHiJibakuProcUpdate, NULL,
        ftNessSpecialAirHiJibakuProcPhysics, ftNessSpecialAirHiJibakuProcMap },
    /* 237 NessSpecialLwScopeStart */
    [nFTNessStatusSpecialLwScopeStart - nFTCommonStatusSpecialStart] = { { nFTNessMotionSpecialLwStart, nFTMotionAttackIDSpecialLw }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDSpecialLw } },
        ftNessSpecialLwStartProcUpdate, NULL,
        ftNessSpecialLwProcPhysics, ftNessSpecialLwStartProcMap },
    /* 238 NessSpecialLwHold */
    [nFTNessStatusSpecialLwHold - nFTCommonStatusSpecialStart] = { { nFTNessMotionSpecialLwHold, nFTMotionAttackIDSpecialLw }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDSpecialLw } },
        ftNessSpecialLwHoldProcUpdate, NULL,
        ftPhysicsApplyGroundVelFriction, ftNessSpecialLwHoldProcMap },
    /* 239 NessSpecialLwHit */
    [nFTNessStatusSpecialLwHit - nFTCommonStatusSpecialStart] = { { nFTNessMotionSpecialLwHit, nFTMotionAttackIDSpecialLw }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDSpecialLw } },
        ftNessSpecialLwHitProcUpdate, NULL,
        ftPhysicsApplyGroundVelFriction, ftNessSpecialLwHitProcMap },
    /* 240 NessSpecialLwEnd */
    [nFTNessStatusSpecialLwEnd - nFTCommonStatusSpecialStart] = { { nFTNessMotionSpecialLwEnd, nFTMotionAttackIDSpecialLw }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDSpecialLw } },
        ftAnimEndSetWait, NULL,
        ftPhysicsApplyGroundVelFriction, ftNessSpecialLwEndProcMap },
    /* 241 NessSpecialAirLwStart */
    [nFTNessStatusSpecialAirLwStart - nFTCommonStatusSpecialStart] = { { nFTNessMotionSpecialAirLwStart, nFTMotionAttackIDSpecialLw }, { { 0, FALSE, nMPKineticsAir, FALSE, nFTStatusAttackIDSpecialLw } },
        ftNessSpecialAirLwStartProcUpdate, NULL,
        ftNessSpecialAirLwProcPhysics, ftNessSpecialAirLwStartProcMap },
    /* 242 NessSpecialAirLwHold */
    [nFTNessStatusSpecialAirLwHold - nFTCommonStatusSpecialStart] = { { nFTNessMotionSpecialAirLwHold, nFTMotionAttackIDSpecialLw }, { { 0, FALSE, nMPKineticsAir, FALSE, nFTStatusAttackIDSpecialLw } },
        ftNessSpecialAirLwHoldProcUpdate, NULL,
        ftNessSpecialAirLwProcPhysics, ftNessSpecialAirLwHoldProcMap },
    /* 243 NessSpecialAirLwHit */
    [nFTNessStatusSpecialAirLwHit - nFTCommonStatusSpecialStart] = { { nFTNessMotionSpecialAirLwHit, nFTMotionAttackIDSpecialLw }, { { 0, FALSE, nMPKineticsAir, FALSE, nFTStatusAttackIDSpecialLw } },
        ftNessSpecialAirLwHitProcUpdate, NULL,
        ftNessSpecialAirLwProcPhysics, ftNessSpecialAirLwHitProcMap },
    /* 244 NessSpecialAirLwEnd */
    [nFTNessStatusSpecialAirLwEnd - nFTCommonStatusSpecialStart] = { { nFTNessMotionSpecialAirLwEnd, nFTMotionAttackIDSpecialLw }, { { 0, FALSE, nMPKineticsAir, FALSE, nFTStatusAttackIDSpecialLw } },
        ftNessSpecialAirLwEndProcUpdate, NULL,
        ftNessSpecialAirLwProcPhysics, ftNessSpecialAirLwEndProcMap },
};

/* ---- ft/ftchar/ftpikachu/ftpikachustatus.h dFTPikachuSpecialStatusDescs,
 * statuses 220-223: the last fighter in the game to get a
 * table of his own, and the last kind whose dFTMainSpecialStatusDescs row
 * was FT_SPECIAL_NONE for any reason but being unspawnable.
 *
 * Four rows. 220-221 are the entrance -- the shape put at the
 * head of seven other tables, the common procs and nothing of Pikachu's
 * own -- and filling them is what finishes the roster's warp-in: every
 * playable kind now arrives the way the game has him arrive, and the one
 * empty row left in dFTCommonEntryAppearStatusIDs is Master Hand's.
 * Pikachu is the exact case ftCommonEntryAppearIsLive was written for
 * last step:'s bite 3 filled his entry-table row on purpose and
 * watched HasAppear flip while IsLive stayed FALSE, because the row
 * these four lines supply did not exist yet. It does now, and both
 * answer TRUE.
 *
 * 222-223 are the Thunder Jolt, his Neutral-B, grounded and airborne.
 * ftpikachuspecialn.c's seven procs compile unmodified; the two ProcMaps
 * named here are its own, and everything else in the pair is common --
 * ftAnimEndSetWait/SetFall for the update, the stock friction physics.
 * The work of the move is not in these rows at all but in the accessory
 * proc the setters install, which spawns the weapon at joint
 * FTPIKACHU_THUNDERJOLT_SPAWN_JOINT on a motion-script flag, and in
 * wp/wppikachu/wppikachuthunderjolt.c behind it.
 *
 * Statuses 224-237 -- both Thunders and both Agilities -- came at
 * steps 69 and 70, below. ---- */
FTStatusDesc dFTPikachuSpecialStatusDescs[nFTPikachuStatusSpecialAirHiEnd - nFTCommonStatusSpecialStart + 1] = {
    /* 220 PikachuAppearR */
    [nFTPikachuStatusAppearR - nFTCommonStatusSpecialStart] = { { nFTPikachuMotionAppearR, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonAppearProcUpdate, NULL,
        ftCommonAppearProcPhysics, mpCommonUpdateFighterProjectFloor },
    /* 221 PikachuAppearL */
    [nFTPikachuStatusAppearL - nFTCommonStatusSpecialStart] = { { nFTPikachuMotionAppearL, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonAppearProcUpdate, NULL,
        ftCommonAppearProcPhysics, mpCommonUpdateFighterProjectFloor },
    /* 222 PikachuSpecialN */
    [nFTPikachuStatusSpecialN - nFTCommonStatusSpecialStart] = { { nFTPikachuMotionSpecialN, nFTMotionAttackIDSpecialN }, { { 0, FALSE, nMPKineticsGround, TRUE, nFTStatusAttackIDSpecialN } },
        ftAnimEndSetWait, NULL,
        ftPhysicsApplyGroundVelFriction, ftPikachuSpecialNProcMap },
    /* 223 PikachuSpecialAirN */
    [nFTPikachuStatusSpecialAirN - nFTCommonStatusSpecialStart] = { { nFTPikachuMotionSpecialAirN, nFTMotionAttackIDSpecialN }, { { 0, FALSE, nMPKineticsAir, TRUE, nFTStatusAttackIDSpecialN } },
        ftAnimEndSetFall, NULL,
        ftPhysicsApplyAirVelFriction, ftPikachuSpecialAirNProcMap },
    /* ----  statuses 224-231, the Thunder, his Down-B. Four
 * statuses twice over, ground and air, and the pairing is the whole
 * shape of the move: Start calls down the bolt, Loop waits for it,
 * Hit is Pikachu being struck by his own lightning, End is the
 * recovery. Every one of the eight can switch to its opposite number
 * mid-move -- walking off a ledge or landing -- carrying the
 * animation's own frame across, which is why the file has eight
 * ProcMaps and eight switch functions rather than two.
 *
 * The attack ids are the decomp's, and they are WRONG in the game:
 * ft/ftchar/ftpikachu/ftpikachustatus.h says so at the top of the
 * table ("Scuffed attack IDs, SpecialHi and SpecialLw are swapped"),
 * and every one of these eight rows carries SpecialHi where the move
 * is the Down-B. Kept as the game has them, because the ids are what
 * ftMainSetStatus compares to decide whether to reload stat_flags,
 * and changing them would change when it does.
 *
 * All eight are is_projectile TRUE -- the bolt is a weapon, and the
 * flag is what makes the fighter's own hurtbox behave while it is
 * out. Only 230 needs physics of Pikachu's own
 * (ftPikachuSpecialAirLwHitProcPhysics: the hit knocks him downward
 * with a gravity of its own); the other seven take the common
 * friction. ---- */
    /* 224 PikachuSpecialLwStart */
    [nFTPikachuStatusSpecialLwStart - nFTCommonStatusSpecialStart] = { { nFTPikachuMotionSpecialLwStart, nFTMotionAttackIDSpecialHi }, { { 0, FALSE, nMPKineticsGround, TRUE, nFTStatusAttackIDSpecialHi } },
        ftPikachuSpecialLwStartProcUpdate, NULL,
        ftPhysicsApplyGroundVelFriction, ftPikachuSpecialLwStartProcMap },
    /* 225 PikachuSpecialLwLoop */
    [nFTPikachuStatusSpecialLwLoop - nFTCommonStatusSpecialStart] = { { nFTPikachuMotionSpecialLwLoop, nFTMotionAttackIDSpecialHi }, { { 0, FALSE, nMPKineticsGround, TRUE, nFTStatusAttackIDSpecialHi } },
        ftPikachuSpecialLwLoopProcUpdate, NULL,
        ftPhysicsApplyGroundVelFriction, ftPikachuSpecialLwLoopProcMap },
    /* 226 PikachuSpecialLwHit */
    [nFTPikachuStatusSpecialLwHit - nFTCommonStatusSpecialStart] = { { nFTPikachuMotionSpecialLwHit, nFTMotionAttackIDSpecialHi }, { { 0, FALSE, nMPKineticsGround, TRUE, nFTStatusAttackIDSpecialHi } },
        ftPikachuSpecialLwHitProcUpdate, NULL,
        ftPhysicsApplyGroundVelFriction, ftPikachuSpecialLwHitProcMap },
    /* 227 PikachuSpecialLwEnd */
    [nFTPikachuStatusSpecialLwEnd - nFTCommonStatusSpecialStart] = { { nFTPikachuMotionSpecialLwEnd, nFTMotionAttackIDSpecialHi }, { { 0, FALSE, nMPKineticsGround, TRUE, nFTStatusAttackIDSpecialHi } },
        ftAnimEndSetWait, NULL,
        ftPhysicsApplyGroundVelFriction, ftPikachuSpecialLwEndProcMap },
    /* 228 PikachuSpecialAirLwStart */
    [nFTPikachuStatusSpecialAirLwStart - nFTCommonStatusSpecialStart] = { { nFTPikachuMotionSpecialAirLwStart, nFTMotionAttackIDSpecialHi }, { { 0, FALSE, nMPKineticsAir, TRUE, nFTStatusAttackIDSpecialHi } },
        ftPikachuSpecialAirLwStartProcUpdate, NULL,
        ftPhysicsApplyAirVelFriction, ftPikachuSpecialAirLwStartProcMap },
    /* 229 PikachuSpecialAirLwLoop */
    [nFTPikachuStatusSpecialAirLwLoop - nFTCommonStatusSpecialStart] = { { nFTPikachuMotionSpecialAirLwLoop, nFTMotionAttackIDSpecialHi }, { { 0, FALSE, nMPKineticsAir, TRUE, nFTStatusAttackIDSpecialHi } },
        ftPikachuSpecialAirLwLoopProcUpdate, NULL,
        ftPhysicsApplyAirVelFriction, ftPikachuSpecialAirLwLoopProcMap },
    /* 230 PikachuSpecialAirLwHit */
    [nFTPikachuStatusSpecialAirLwHit - nFTCommonStatusSpecialStart] = { { nFTPikachuMotionSpecialAirLwHit, nFTMotionAttackIDSpecialHi }, { { 0, FALSE, nMPKineticsAir, TRUE, nFTStatusAttackIDSpecialHi } },
        ftPikachuSpecialAirLwHitProcUpdate, NULL,
        ftPikachuSpecialAirLwHitProcPhysics, ftPikachuSpecialAirLwHitProcMap },
    /* 231 PikachuSpecialAirLwEnd */
    [nFTPikachuStatusSpecialAirLwEnd - nFTCommonStatusSpecialStart] = { { nFTPikachuMotionSpecialAirLwEnd, nFTMotionAttackIDSpecialHi }, { { 0, FALSE, nMPKineticsAir, TRUE, nFTStatusAttackIDSpecialHi } },
        ftPikachuSpecialAirLwEndProcUpdate, NULL,
        ftPhysicsApplyAirVelFriction, ftPikachuSpecialAirLwEndProcMap },
    /* ----  statuses 232-237, the Agility (Quick Attack), his
 * Up-B. Six rows, Start/Zip/End ground and air.
 *
 * Three things in these six rows are the game's own oddities, all
 * kept:
 *
 * The two Start rows carry motion id -1. They play NO ANIMATION at
 * all: ftPikachuSpecialHiInitMiscVars freezes the animation with
 * gcSetAnimSpeed(0), makes him intangible and puts the crackle
 * colour on, and the Start proc counts FTPIKACHU_QUICKATTACK_START_TIME
 * frames down on a status var rather than on anim_frame. -1 fits
 * because FTMotionFlags.motion_id is a SIGNED ten-bit field
 * (ft/fttypes.h:184), so it stores as all-ones and reads back as -1.
 *
 * Row 235, the AIRBORNE Start, is marked nMPKineticsGround. The
 * decomp's own comment on it says "not marked as airborne". Its
 * ProcPhysics is airborne anyway (ftPikachuSpecialAirHiStartProcPhysics
 * applies gravity), so what the wrong flag changes is what reads
 * stat_flags.ga -- and the port keeps the game's answer.
 *
 * All six carry attack id SpecialLw on an Up-B, the same swap the
 * Thunder's eight carry the other way round.
 *
 * None is a projectile status: the Quick Attack is a teleport, not a
 * hitbox. ---- */
    /* 232 PikachuSpecialHiStart */
    [nFTPikachuStatusSpecialHiStart - nFTCommonStatusSpecialStart] = { { -1, nFTMotionAttackIDSpecialLw }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDSpecialLw } },
        ftPikachuSpecialHiStartProcUpdate, NULL,
        ftPhysicsApplyGroundVelFriction, ftPikachuSpecialHiStartProcMap },
    /* 233 PikachuSpecialHi */
    [nFTPikachuStatusSpecialHi - nFTCommonStatusSpecialStart] = { { nFTPikachuMotionSpecialHi, nFTMotionAttackIDSpecialLw }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDSpecialLw } },
        ftPikachuSpecialHiProcUpdate, NULL,
        ftPikachuSpecialHiProcPhysics, ftPikachuSpecialHiProcMap },
    /* 234 PikachuSpecialHiEnd */
    [nFTPikachuStatusSpecialHiEnd - nFTCommonStatusSpecialStart] = { { nFTPikachuMotionSpecialHiEnd, nFTMotionAttackIDSpecialLw }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDSpecialLw } },
        ftPikachuSpecialHiEndProcUpdate, NULL,
        ftPikachuSpecialHiEndProcPhysics, ftPikachuSpecialHiEndProcMap },
    /* 235 PikachuSpecialAirHiStart -- ga Ground, and the decomp says so */
    [nFTPikachuStatusSpecialAirHiStart - nFTCommonStatusSpecialStart] = { { -1, nFTMotionAttackIDSpecialLw }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDSpecialLw } },
        ftPikachuSpecialAirHiStartProcUpdate, NULL,
        ftPikachuSpecialAirHiStartProcPhysics, ftPikachuSpecialAirHiStartProcMap },
    /* 236 PikachuSpecialAirHi */
    [nFTPikachuStatusSpecialAirHi - nFTCommonStatusSpecialStart] = { { nFTPikachuMotionSpecialAirHi, nFTMotionAttackIDSpecialLw }, { { 0, FALSE, nMPKineticsAir, FALSE, nFTStatusAttackIDSpecialLw } },
        ftPikachuSpecialAirHiProcUpdate, NULL,
        ftPikachuSpecialAirHiProcPhysics, ftPikachuSpecialAirHiProcMap },
    /* 237 PikachuSpecialAirHiEnd */
    [nFTPikachuStatusSpecialAirHiEnd - nFTCommonStatusSpecialStart] = { { nFTPikachuMotionSpecialAirHiEnd, nFTMotionAttackIDSpecialLw }, { { 0, FALSE, nMPKineticsAir, FALSE, nFTStatusAttackIDSpecialLw } },
        ftPikachuSpecialAirHiEndProcUpdate, NULL,
        ftPikachuSpecialAirHiEndProcPhysics, ftPikachuSpecialAirHiEndProcMap },
};

/* ---- ft/ftcommon/ftcommonstatus.h dFTCommonActionStatusDescs, the
 * movement rows, indexed from nFTCommonStatusActionStart as the game's
 * is. Each row's motion and stat flags are the game's own (
 * checked every row against the decomp's table); the proc_map column is
 * what the collision port made real.
 *
 * SIX ROWS CARRIED STAND-IN CALLBACKS fromuntil,
 * and all six are the game's own now: JumpAerialF/B ran ftAnimEndSetFall
 * and ftCommonJumpProcInterrupt where the game runs
 * ftCommonJumpAerialProcUpdate/ProcInterrupt, Fall and FallAerial ran
 * ftCommonJumpProcInterrupt for ftCommonFallProcInterrupt, SquatWait had
 * no ProcUpdate at all, and SquatRv ran ftCommonLandingProcInterrupt for
 * ftCommonSquatRvProcInterrupt. What kept them stand-ins was that
 * ft/ftcommon/ftcommonjumpaerial.c, ftcommonsquat.c and ftcommonfall.c
 * were not in the build; they are, unmodified, and thirteen hand-copies
 * of their functions went out of this file when they came in. No row in
 * this table carries a stand-in any more. ------ */
FTStatusDesc dFTCommonActionStatusDescs[nFTCommonStatusLandingAirNull + 1 - nFTCommonStatusActionStart] = {
    [nFTCommonStatusEntryNull - nFTCommonStatusActionStart] = { { nFTCommonMotionWait, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonEntryNullProcUpdate, NULL, NULL, NULL },
    [nFTCommonStatusRebirthDown - nFTCommonStatusActionStart] = { { nFTCommonMotionRebirthDown, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonRebirthDownProcUpdate, NULL, NULL, ftCommonRebirthCommonProcMap },
    [nFTCommonStatusRebirthStand - nFTCommonStatusActionStart] = { { nFTCommonMotionRebirthStand, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonRebirthStandProcUpdate, NULL, NULL, ftCommonRebirthCommonProcMap },
    [nFTCommonStatusRebirthWait - nFTCommonStatusActionStart] = { { nFTCommonMotionRebirthWait, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonRebirthWaitProcUpdate, ftCommonRebirthWaitProcInterrupt, NULL, ftCommonRebirthCommonProcMap },
    [nFTCommonStatusWait - nFTCommonStatusActionStart] = { { nFTCommonMotionWait, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        NULL, ftCommonWaitProcInterrupt, ftPhysicsApplyGroundVelFriction,
        mpCommonProcFighterOnCliffEdge },
    [nFTCommonStatusWalkSlow - nFTCommonStatusActionStart] = { { nFTCommonMotionWalkSlow, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        NULL, ftCommonWalkProcInterrupt, ftCommonWalkProcPhysics,
        mpCommonProcFighterOnCliffEdge },
    [nFTCommonStatusWalkMiddle - nFTCommonStatusActionStart] = { { nFTCommonMotionWalkMiddle, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        NULL, ftCommonWalkProcInterrupt, ftCommonWalkProcPhysics,
        mpCommonProcFighterOnCliffEdge },
    [nFTCommonStatusWalkFast - nFTCommonStatusActionStart] = { { nFTCommonMotionWalkFast, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        NULL, ftCommonWalkProcInterrupt, ftCommonWalkProcPhysics,
        mpCommonProcFighterOnCliffEdge },
    /* ftcommonstatus.h status 189: the L-tap taunt. The
 * plain row -- ftAnimEndSetWait ends it, ground friction moves it,
 * the cliff-edge map keeps it on the stage. */
    [nFTCommonStatusAppeal - nFTCommonStatusActionStart] = { { nFTCommonMotionAppeal, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftAnimEndSetWait, ftCommonAppealProcInterrupt,
        ftPhysicsApplyGroundVelFriction, mpCommonProcFighterOnCliffEdge },
    /* ftcommonstatus.h:313-410, statuses 15 to 19. Run's
 * physics is the transfer-only one -- nothing decelerates a run, the
 * status change does -- and TurnRun's is the roll's, the animated
 * TransN, because the turn's stride is the animation's. */
    [nFTCommonStatusDash - nFTCommonStatusActionStart] = { { nFTCommonMotionDash, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonDashProcUpdate, ftCommonDashProcInterrupt,
        ftCommonDashProcPhysics, ftCommonDashProcMap },
    [nFTCommonStatusRun - nFTCommonStatusActionStart] = { { nFTCommonMotionRun, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        NULL, ftCommonRunProcInterrupt,
        ftPhysicsApplyGroundVelTransferAir, mpCommonSetFighterFallOnGroundBreak },
    [nFTCommonStatusRunBrake - nFTCommonStatusActionStart] = { { nFTCommonMotionRunBrake, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftAnimEndSetWait, ftCommonRunBrakeProcInterrupt,
        ftCommonRunBrakeProcPhysics, mpCommonProcFighterOnCliffEdge },
    [nFTCommonStatusTurn - nFTCommonStatusActionStart] = { { nFTCommonMotionTurn, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonTurnProcUpdate, ftCommonTurnProcInterrupt,
        ftPhysicsApplyGroundVelFriction, mpCommonSetFighterFallOnGroundBreak },
    [nFTCommonStatusTurnRun - nFTCommonStatusActionStart] = { { nFTCommonMotionTurnRun, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonTurnRunProcUpdate, ftCommonTurnRunProcInterrupt,
        ftPhysicsApplyGroundVelTransN, mpCommonSetFighterFallOnGroundBreak },
    [nFTCommonStatusKneeBend - nFTCommonStatusActionStart] = { { nFTCommonMotionKneeBend, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonKneeBendProcUpdate, ftCommonKneeBendProcInterrupt,
        ftPhysicsApplyGroundVelFriction, mpCommonSetFighterFallOnGroundBreak },
    [nFTCommonStatusJumpF - nFTCommonStatusActionStart] = { { nFTCommonMotionJumpF, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftAnimEndSetFall, ftCommonJumpProcInterrupt,
        ftPhysicsApplyAirVelDriftFastFall, mpCommonProcFighterCliffFloorCeil },
    [nFTCommonStatusJumpB - nFTCommonStatusActionStart] = { { nFTCommonMotionJumpB, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftAnimEndSetFall, ftCommonJumpProcInterrupt,
        ftPhysicsApplyAirVelDriftFastFall, mpCommonProcFighterCliffFloorCeil },
    [nFTCommonStatusJumpAerialF - nFTCommonStatusActionStart] = { { nFTCommonMotionJumpAerialF, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonJumpAerialProcUpdate, ftCommonJumpAerialProcInterrupt,
        ftPhysicsApplyAirVelDriftFastFall, mpCommonProcFighterCliffFloorCeil },
    [nFTCommonStatusJumpAerialB - nFTCommonStatusActionStart] = { { nFTCommonMotionJumpAerialB, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonJumpAerialProcUpdate, ftCommonJumpAerialProcInterrupt,
        ftPhysicsApplyAirVelDriftFastFall, mpCommonProcFighterCliffFloorCeil },
    [nFTCommonStatusFall - nFTCommonStatusActionStart] = { { nFTCommonMotionFall, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        NULL, ftCommonFallProcInterrupt,
        ftPhysicsApplyAirVelDriftFastFall, mpCommonProcFighterCliffFloorCeil },
    [nFTCommonStatusFallAerial - nFTCommonStatusActionStart] = { { nFTCommonMotionFallAerial, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        NULL, ftCommonFallProcInterrupt,
        ftPhysicsApplyAirVelDriftFastFall, mpCommonProcFighterCliffFloorCeil },
    [nFTCommonStatusSquat - nFTCommonStatusActionStart] = { { nFTCommonMotionSquat, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonSquatProcUpdate, ftCommonSquatProcInterrupt,
        ftPhysicsApplyGroundVelFriction, mpCommonSetFighterFallOnGroundBreak },
    [nFTCommonStatusSquatWait - nFTCommonStatusActionStart] = { { nFTCommonMotionSquatWait, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonSquatWaitProcUpdate, ftCommonSquatWaitProcInterrupt,
        ftPhysicsApplyGroundVelFriction, mpCommonSetFighterFallOnGroundBreak },
    [nFTCommonStatusSquatRv - nFTCommonStatusActionStart] = { { nFTCommonMotionSquatRv, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftAnimEndSetWait, ftCommonSquatRvProcInterrupt,
        ftPhysicsApplyGroundVelFriction, mpCommonSetFighterFallOnGroundBreak },
    [nFTCommonStatusLandingLight - nFTCommonStatusActionStart] = { { nFTCommonMotionLandingLight, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftAnimEndSetWait, ftCommonLandingProcInterrupt,
        ftPhysicsApplyGroundVelFriction, mpCommonProcFighterOnCliffEdge },
    [nFTCommonStatusLandingHeavy - nFTCommonStatusActionStart] = { { nFTCommonMotionLandingHeavy, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftAnimEndSetWait, ftCommonLandingProcInterrupt,
        ftPhysicsApplyGroundVelFriction, mpCommonProcFighterOnCliffEdge },
    [nFTCommonStatusPass - nFTCommonStatusActionStart] = { { nFTCommonMotionPass, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftAnimEndSetFall, ftCommonPassProcInterrupt,
        ftPhysicsApplyAirVelDriftFastFall, mpCommonProcFighterCliffFloorCeil },
    /* the shield's six (ftcommonstatus.h statuses 21, 34, 152-155):
 * the two guard-flavoured entries run KneeBend's and Pass's own procs. */
    [nFTCommonStatusGuardKneeBend - nFTCommonStatusActionStart] = { { nFTCommonMotionGuardKneeBend, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonKneeBendProcUpdate, ftCommonKneeBendProcInterrupt,
        ftPhysicsApplyGroundVelFriction, mpCommonSetFighterFallOnGroundBreak },
    [nFTCommonStatusGuardPass - nFTCommonStatusActionStart] = { { nFTCommonMotionGuardPass, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftAnimEndSetFall, ftCommonPassProcInterrupt,
        ftPhysicsApplyAirVelDriftFastFall, mpCommonProcFighterCliffFloorCeil },
    [nFTCommonStatusGuardOn - nFTCommonStatusActionStart] = { { nFTCommonMotionGuardOn, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonGuardOnProcUpdate, ftCommonGuardCommonProcInterrupt,
        ftPhysicsApplyGroundVelFriction, mpCommonSetFighterFallOnGroundBreak },
    [nFTCommonStatusGuard - nFTCommonStatusActionStart] = { { -1, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonGuardProcUpdate, ftCommonGuardCommonProcInterrupt,
        ftPhysicsApplyGroundVelFriction, mpCommonSetFighterFallOnGroundBreak },
    [nFTCommonStatusGuardOff - nFTCommonStatusActionStart] = { { nFTCommonMotionGuardOff, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonGuardOffProcUpdate, NULL,
        ftPhysicsApplyGroundVelFriction, mpCommonSetFighterFallOnGroundBreak },
    [nFTCommonStatusGuardSetOff - nFTCommonStatusActionStart] = { { -1, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonGuardSetOffProcUpdate, NULL,
        ftPhysicsApplyGroundVelFriction, mpCommonSetFighterFallOnGroundBreak },
    /* ftcommonstatus.h statuses 158-164: the shield break.
 * Fly and Fall are airborne (air-vel friction, landing goes to Down);
 * Down, Stand and FuraFura are grounded. Every proc is above. */
    [nFTCommonStatusShieldBreakFly - nFTCommonStatusActionStart] = { { nFTCommonMotionShieldBreakFly, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonShieldBreakFlyProcUpdate, NULL,
        ftPhysicsApplyAirVelFriction, ftCommonShieldBreakFlyProcMap },
    [nFTCommonStatusShieldBreakFall - nFTCommonStatusActionStart] = { { nFTCommonMotionShieldBreakFall, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        NULL, NULL,
        ftPhysicsApplyAirVelFriction, ftCommonShieldBreakFallProcMap },
    [nFTCommonStatusShieldBreakDownD - nFTCommonStatusActionStart] = { { nFTCommonMotionShieldBreakDownD, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonShieldBreakDownProcUpdate, NULL,
        ftPhysicsApplyGroundVelFriction, mpCommonSetFighterFallOnGroundBreak },
    [nFTCommonStatusShieldBreakDownU - nFTCommonStatusActionStart] = { { nFTCommonMotionShieldBreakDownU, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonShieldBreakDownProcUpdate, NULL,
        ftPhysicsApplyGroundVelFriction, mpCommonSetFighterFallOnGroundBreak },
    [nFTCommonStatusShieldBreakStandD - nFTCommonStatusActionStart] = { { nFTCommonMotionShieldBreakStandD, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonShieldBreakStandProcUpdate, NULL,
        ftPhysicsApplyGroundVelFriction, mpCommonSetFighterFallOnGroundBreak },
    [nFTCommonStatusShieldBreakStandU - nFTCommonStatusActionStart] = { { nFTCommonMotionShieldBreakStandU, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonShieldBreakStandProcUpdate, NULL,
        ftPhysicsApplyGroundVelFriction, mpCommonSetFighterFallOnGroundBreak },
    [nFTCommonStatusFuraFura - nFTCommonStatusActionStart] = { { nFTCommonMotionFuraFura, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonFuraFuraProcUpdate, NULL,
        ftPhysicsApplyGroundVelFriction, mpCommonSetFighterFallOnGroundBreak },
    /* ftcommonstatus.h:3333-3352, status 166: the grab, the reach
 *). The whiff ends into Wait; the connect is deferred (see
 * the grab section above). */
    [nFTCommonStatusCatch - nFTCommonStatusActionStart] = { { nFTCommonMotionCatch, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonCatchProcUpdate, NULL,
        ftPhysicsApplyGroundVelFriction, ftCommonCatchProcMap },
    /* ftcommonstatus.h:3353-3471, statuses 167/168/171/172: the connect.
 * CatchPull -> CatchWait is the catcher's; CapturePulled
 * -> CaptureWait is the caught fighter's, its physics proc snapping it
 * to the catcher's hand each frame. Scripts -2 on the two Wait rows
 * hold the last frame. */
    [nFTCommonStatusCatchPull - nFTCommonStatusActionStart] = { { nFTCommonMotionCatchPull, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonCatchPullProcUpdate, NULL,
        ftPhysicsApplyGroundVelFriction, ftCommonCatchProcMap },
    [nFTCommonStatusCatchWait - nFTCommonStatusActionStart] = { { -2, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        NULL, ftCommonCatchWaitProcInterrupt,
        ftPhysicsApplyGroundVelFriction, ftCommonCatchProcMap },
    /* ftcommonstatus.h:3393-3432, statuses 169/170: the throw,
 * on the catcher. ThrowProcUpdate reads the figatree's release event and
 * hands off knockback; the physics proc drives ground friction or the
 * animated TransN, the map proc is Catch's. */
    [nFTCommonStatusThrowF - nFTCommonStatusActionStart] = { { nFTCommonMotionThrowF, nFTMotionAttackIDThrowF }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDThrowF } },
        ftCommonThrowProcUpdate, NULL,
        ftPhysicsApplyGroundFrictionOrTransN, ftCommonCatchProcMap },
    [nFTCommonStatusThrowB - nFTCommonStatusActionStart] = { { nFTCommonMotionThrowB, nFTMotionAttackIDThrowB }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDThrowB } },
        ftCommonThrowProcUpdate, NULL,
        ftPhysicsApplyGroundFrictionOrTransN, ftCommonCatchProcMap },
    [nFTCommonStatusCapturePulled - nFTCommonStatusActionStart] = { { nFTCommonMotionCapturePulled, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        NULL, NULL,
        ftCommonCapturePulledProcPhysics, ftCommonCapturePulledProcMap },
    [nFTCommonStatusCaptureWait - nFTCommonStatusActionStart] = { { -2, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        NULL, NULL,
        ftCommonCapturePulledProcPhysics, ftCommonCaptureWaitProcMap },
    /* ftcommonstatus.h:3653-3752, statuses 182/185/186: the thrown fighter's
 * arc. Mario's forward throw is the one-stage ThrownCommon
 * (186); its back throw is the two-stage ThrownMarioBStart (182) ->
 * ThrownMarioB (185). All ride ftCommonThrownProc{Physics,Map}; 182/186
 * also run the ProcUpdate that flips to the queued status at anim end. */
    /* ftcommonstatus.h:3593-3611, status 179 CaptureCaptain:
 * the fighter Falcon Dive has hold of. ft/ftcommon/ftcommoncapture-
 * captain.c is compiled unmodified; its ProcCapture is what
 * ftCaptainSpecialHiSetCatchParams names for the caught side, and its
 * ProcPhysics drags the body to Captain's joint 29 through the Vec2h
 * table below (ftCommonCaptureCaptainUpdatePositions) until the throw
 * flag releases it. Motion CaptureCaptain (the FalconDivePulled anim)
 * is FTANIM_FLAG_NONE in every fighter's table. */
    [nFTCommonStatusCaptureCaptain - nFTCommonStatusActionStart] = { { nFTCommonMotionCaptureCaptain, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        NULL, NULL, ftCommonCaptureCaptainProcPhysics,
        mpCommonUpdateFighterProjectFloor },
    /* ftcommonstatus.h:3613-3812, statuses 180-188: every thrown and
 * carried row. DK's forward throw carries the caught fighter as
 * Shouldered (184, whose interrupt is the mash free) after 180/181,
 * and Fox's throws are 183/187/188. ThrownCommon (186) has no update
 * in the game: the thrower's release ends it. */
    [nFTCommonStatusThrownDonkeyUnk - nFTCommonStatusActionStart] = { { nFTCommonMotionThrownDonkeyUnk, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        NULL, NULL,
        NULL, NULL },
    [nFTCommonStatusThrownDonkeyF - nFTCommonStatusActionStart] = { { nFTCommonMotionThrownDonkeyF, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonThrownProcUpdate, NULL,
        ftCommonThrownProcPhysics, ftCommonThrownProcMap },
    [nFTCommonStatusThrownMarioBStart - nFTCommonStatusActionStart] = { { nFTCommonMotionThrownMarioB1, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonThrownProcUpdate, NULL,
        ftCommonThrownProcPhysics, ftCommonThrownProcMap },
    [nFTCommonStatusThrownFoxFStart - nFTCommonStatusActionStart] = { { nFTCommonMotionThrownUnk1, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonThrownProcUpdate, NULL,
        ftCommonThrownProcPhysics, ftCommonThrownProcMap },
    [nFTCommonStatusShouldered - nFTCommonStatusActionStart] = { { nFTCommonMotionShouldered, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        NULL, ftCommonCaptureShoulderedProcInterrupt,
        ftCommonThrownProcPhysics, ftCommonThrownProcMap },
    [nFTCommonStatusThrownMarioB - nFTCommonStatusActionStart] = { { nFTCommonMotionThrownMarioB2, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        NULL, NULL,
        ftCommonThrownProcPhysics, ftCommonThrownProcMap },
    [nFTCommonStatusThrownCommon - nFTCommonStatusActionStart] = { { nFTCommonMotionThrownCommon, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        NULL, NULL,
        ftCommonThrownProcPhysics, ftCommonThrownProcMap },
    [nFTCommonStatusThrownFoxF - nFTCommonStatusActionStart] = { { nFTCommonMotionThrownUnk2, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        NULL, NULL,
        ftCommonThrownProcPhysics, ftCommonThrownProcMap },
    [nFTCommonStatusThrownFoxB - nFTCommonStatusActionStart] = { { nFTCommonMotionThrownUnk3, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        NULL, NULL,
        ftCommonThrownProcPhysics, ftCommonThrownProcMap },
    /* ftcommonstatus.h:3133-3172, statuses 156 and 157: the roll. The
 * physics proc is the one that reads the animated TransN joint, so
 * the roll's distance and speed are the figatree's. */
    [nFTCommonStatusEscapeF - nFTCommonStatusActionStart] = { { nFTCommonMotionEscapeF, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonEscapeProcUpdate, ftCommonEscapeProcInterrupt,
        ftPhysicsApplyGroundVelTransN, mpCommonSetFighterFallOnEdgeBreak },
    [nFTCommonStatusEscapeB - nFTCommonStatusActionStart] = { { nFTCommonMotionEscapeB, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonEscapeProcUpdate, ftCommonEscapeProcInterrupt,
        ftPhysicsApplyGroundVelTransN, mpCommonSetFighterFallOnEdgeBreak },
    [nFTCommonStatusOttottoWait - nFTCommonStatusActionStart] = { { nFTCommonMotionOttottoWait, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        NULL, ftCommonOttottoProcInterrupt, NULL,
        ftCommonOttottoProcMap },
    [nFTCommonStatusOttotto - nFTCommonStatusActionStart] = { { nFTCommonMotionOttotto, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonOttottoProcUpdate, ftCommonOttottoProcInterrupt, NULL,
        ftCommonOttottoProcMap },
    [nFTCommonStatusStopCeil - nFTCommonStatusActionStart] = { { nFTCommonMotionStopCeil, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftAnimEndSetFall, NULL, NULL,
        mpCommonProcFighterCliffFloorCeil },
    /* ftcommonstatus.h status 37-49 (Hi/N/Lw x3, Air1-3, E1): every row
 * the ground-table Damage engine uses is the same four
 * functions, exactly as ftcommonstatus.h has it -- see the block
 * comment above ftCommonDamageGotoDamageStatus for what is and isn't
 * ported behind them. DamageE2, the Fly tiers and WallDamage (statuses
 * 50-57) follow just below. */
    [nFTCommonStatusDamageHi1 - nFTCommonStatusActionStart] = { { nFTCommonMotionDamageHi1, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonDamageCommonProcUpdate, ftCommonDamageCommonProcInterrupt,
        ftCommonDamageCommonProcPhysics, mpCommonUpdateFighterKinetics },
    [nFTCommonStatusDamageHi2 - nFTCommonStatusActionStart] = { { nFTCommonMotionDamageHi2, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonDamageCommonProcUpdate, ftCommonDamageCommonProcInterrupt,
        ftCommonDamageCommonProcPhysics, mpCommonUpdateFighterKinetics },
    [nFTCommonStatusDamageHi3 - nFTCommonStatusActionStart] = { { nFTCommonMotionDamageHi3, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonDamageCommonProcUpdate, ftCommonDamageCommonProcInterrupt,
        ftCommonDamageCommonProcPhysics, mpCommonUpdateFighterKinetics },
    [nFTCommonStatusDamageN1 - nFTCommonStatusActionStart] = { { nFTCommonMotionDamageN1, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonDamageCommonProcUpdate, ftCommonDamageCommonProcInterrupt,
        ftCommonDamageCommonProcPhysics, mpCommonUpdateFighterKinetics },
    [nFTCommonStatusDamageN2 - nFTCommonStatusActionStart] = { { nFTCommonMotionDamageN2, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonDamageCommonProcUpdate, ftCommonDamageCommonProcInterrupt,
        ftCommonDamageCommonProcPhysics, mpCommonUpdateFighterKinetics },
    [nFTCommonStatusDamageN3 - nFTCommonStatusActionStart] = { { nFTCommonMotionDamageN3, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonDamageCommonProcUpdate, ftCommonDamageCommonProcInterrupt,
        ftCommonDamageCommonProcPhysics, mpCommonUpdateFighterKinetics },
    [nFTCommonStatusDamageLw1 - nFTCommonStatusActionStart] = { { nFTCommonMotionDamageLw1, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonDamageCommonProcUpdate, ftCommonDamageCommonProcInterrupt,
        ftCommonDamageCommonProcPhysics, mpCommonUpdateFighterKinetics },
    [nFTCommonStatusDamageLw2 - nFTCommonStatusActionStart] = { { nFTCommonMotionDamageLw2, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonDamageCommonProcUpdate, ftCommonDamageCommonProcInterrupt,
        ftCommonDamageCommonProcPhysics, mpCommonUpdateFighterKinetics },
    [nFTCommonStatusDamageLw3 - nFTCommonStatusActionStart] = { { nFTCommonMotionDamageLw3, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonDamageCommonProcUpdate, ftCommonDamageCommonProcInterrupt,
        ftCommonDamageCommonProcPhysics, mpCommonUpdateFighterKinetics },
    [nFTCommonStatusDamageAir1 - nFTCommonStatusActionStart] = { { nFTCommonMotionDamageAir1, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonDamageCommonProcUpdate, ftCommonDamageCommonProcInterrupt,
        ftCommonDamageCommonProcPhysics, mpCommonUpdateFighterKinetics },
    [nFTCommonStatusDamageAir2 - nFTCommonStatusActionStart] = { { nFTCommonMotionDamageAir2, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonDamageCommonProcUpdate, ftCommonDamageCommonProcInterrupt,
        ftCommonDamageCommonProcPhysics, mpCommonUpdateFighterKinetics },
    [nFTCommonStatusDamageAir3 - nFTCommonStatusActionStart] = { { nFTCommonMotionDamageAir3, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonDamageCommonProcUpdate, ftCommonDamageCommonProcInterrupt,
        ftCommonDamageCommonProcPhysics, mpCommonUpdateFighterKinetics },
    [nFTCommonStatusDamageE1 - nFTCommonStatusActionStart] = { { nFTCommonMotionDamageE, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonDamageCommonProcUpdate, ftCommonDamageCommonProcInterrupt,
        ftCommonDamageCommonProcPhysics, mpCommonUpdateFighterKinetics },
    /* ftcommonstatus.h statuses 50-55 and 57, the Fly/tumble launch
 *). DamageE2 and the five DamageFly tiers share the Air-common
 * procs (their Proc Update drops into DamageFall when the flinch ends);
 * DamageFall is the tumble itself (NULL Proc Update -- it ends by map
 * or interrupt, not by a timer). WallDamage (status 56,)
 * sits between the Fly tiers and DamageFall: its Proc Update holds the
 * slam's reflected hitstun then drops into DamageFall. */
    [nFTCommonStatusDamageE2 - nFTCommonStatusActionStart] = { { nFTCommonMotionDamageE, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonDamageAirCommonProcUpdate, ftCommonDamageAirCommonProcInterrupt,
        ftCommonDamageCommonProcPhysics, ftCommonDamageAirCommonProcMap },
    [nFTCommonStatusDamageFlyHi - nFTCommonStatusActionStart] = { { nFTCommonMotionDamageFlyHi, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonDamageAirCommonProcUpdate, ftCommonDamageAirCommonProcInterrupt,
        ftCommonDamageCommonProcPhysics, ftCommonDamageAirCommonProcMap },
    [nFTCommonStatusDamageFlyN - nFTCommonStatusActionStart] = { { nFTCommonMotionDamageFlyN, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonDamageAirCommonProcUpdate, ftCommonDamageAirCommonProcInterrupt,
        ftCommonDamageCommonProcPhysics, ftCommonDamageAirCommonProcMap },
    [nFTCommonStatusDamageFlyLw - nFTCommonStatusActionStart] = { { nFTCommonMotionDamageFlyLw, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonDamageAirCommonProcUpdate, ftCommonDamageAirCommonProcInterrupt,
        ftCommonDamageCommonProcPhysics, ftCommonDamageAirCommonProcMap },
    [nFTCommonStatusDamageFlyTop - nFTCommonStatusActionStart] = { { nFTCommonMotionDamageFlyTop, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonDamageAirCommonProcUpdate, ftCommonDamageAirCommonProcInterrupt,
        ftCommonDamageCommonProcPhysics, ftCommonDamageAirCommonProcMap },
    [nFTCommonStatusDamageFlyRoll - nFTCommonStatusActionStart] = { { nFTCommonMotionDamageFlyRoll, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonDamageAirCommonProcUpdate, ftCommonDamageAirCommonProcInterrupt,
        ftCommonDamageCommonProcPhysics, ftCommonDamageAirCommonProcMap },
    /* ftcommonstatus.h status 56, the wall bounce. Its own
 * Proc Update holds the slammed fighter for the reflected hitstun then
 * drops it into DamageFall; it shares the tumble's interrupt, physics
 * and map procs (a wall bounce can chain into another wall the same
 * way). Reached from ftCommonDamageAirCommonProcMap via ftCommonWall
 * DamageCheckGoto, no longer the FALSE stub. */
    [nFTCommonStatusWallDamage - nFTCommonStatusActionStart] = { { nFTCommonMotionWallDamage, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonWallDamageProcUpdate, ftCommonDamageAirCommonProcInterrupt,
        ftCommonDamageCommonProcPhysics, ftCommonDamageAirCommonProcMap },
    [nFTCommonStatusDamageFall - nFTCommonStatusActionStart] = { { nFTCommonMotionDamageFall, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        NULL, ftCommonDamageFallProcInterrupt,
        ftPhysicsApplyAirVelDriftFastFall, ftCommonDamageFallProcMap },
    /* ftcommonstatus.h statuses 67-72, the knockdown floor.
 * DownBounce settles (anim end) into DownWait; DownWait counts the
 * auto-stand timer down (Proc Update) or takes the manual stand (Proc
 * Interrupt) into DownStand; DownStand ends its get-up anim (ftAnimEnd
 * SetWait) into Wait. All six share the ground friction physics and the
 * break-on-edge floor map. DownWaitD/U carry script id -2 (the pose is
 * held; no figatree plays while the fighter lies there). The getups
 * (75-80), the tech rolls (73-74, PassiveStand) and neutral tech (81,
 * Passive) follow below, all reached by their real CheckInterrupt arms. */
    [nFTCommonStatusDownBounceD - nFTCommonStatusActionStart] = { { nFTCommonMotionDownBounceD, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonDownBounceProcUpdate, NULL,
        ftPhysicsApplyGroundVelFriction, mpCommonSetFighterFallOnGroundBreak },
    [nFTCommonStatusDownBounceU - nFTCommonStatusActionStart] = { { nFTCommonMotionDownBounceU, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonDownBounceProcUpdate, NULL,
        ftPhysicsApplyGroundVelFriction, mpCommonSetFighterFallOnGroundBreak },
    [nFTCommonStatusDownWaitD - nFTCommonStatusActionStart] = { { -2, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonDownWaitProcUpdate, ftCommonDownWaitProcInterrupt,
        ftPhysicsApplyGroundVelFriction, mpCommonSetFighterFallOnGroundBreak },
    [nFTCommonStatusDownWaitU - nFTCommonStatusActionStart] = { { -2, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonDownWaitProcUpdate, ftCommonDownWaitProcInterrupt,
        ftPhysicsApplyGroundVelFriction, mpCommonSetFighterFallOnGroundBreak },
    [nFTCommonStatusDownStandD - nFTCommonStatusActionStart] = { { nFTCommonMotionDownStandD, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftAnimEndSetWait, ftCommonDownStandProcInterrupt,
        ftPhysicsApplyGroundVelFriction, mpCommonSetFighterFallOnGroundBreak },
    [nFTCommonStatusDownStandU - nFTCommonStatusActionStart] = { { nFTCommonMotionDownStandU, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftAnimEndSetWait, ftCommonDownStandProcInterrupt,
        ftPhysicsApplyGroundVelFriction, mpCommonSetFighterFallOnGroundBreak },
    /* ftcommonstatus.h statuses 73-74, the ground tech's roll recoveries:
 * a fighter that techs with the stick pushed forward or
 * back rolls that way (PassiveStandF/B) instead of standing in place.
 * Their entry is ftCommonPassiveStandCheckInterruptDamage, now real. Both
 * end the roll anim (ftAnimEndSetWait) into Wait, slide on the animated
 * TransN joint, and break off an edge -- the getups' physics/map. */
    [nFTCommonStatusPassiveStandF - nFTCommonStatusActionStart] = { { nFTCommonMotionPassiveStandF, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftAnimEndSetWait, NULL,
        ftPhysicsApplyGroundVelTransN, mpCommonSetFighterFallOnEdgeBreak },
    [nFTCommonStatusPassiveStandB - nFTCommonStatusActionStart] = { { nFTCommonMotionPassiveStandB, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftAnimEndSetWait, NULL,
        ftPhysicsApplyGroundVelTransN, mpCommonSetFighterFallOnEdgeBreak },
    /* ftcommonstatus.h statuses 75-80, the getups the knockdown floor breaks
 * into: DownForward/DownBack roll along the ground toward or
 * away from the fighter's facing, DownAttack is the get-up attack (its two
 * rows carry the attack ids the motion script's hitbox events read). All
 * six end their get-up anim (ftAnimEndSetWait) into Wait; the rolls slide
 * on the animated TransN joint (ftPhysicsApplyGroundVelTransN), the attack
 * on plain ground friction, and all break off an edge. Their entry checks
 * (ftCommonDownForwardOrBackCheckInterruptCommon, ftCommonDownAttackCheck
 * Interrupt{DownBounce,DownWait}) are the compiled-in decomp dependencies;
 * these rows are what give those checks somewhere to land. */
    [nFTCommonStatusDownForwardD - nFTCommonStatusActionStart] = { { nFTCommonMotionDownForwardD, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftAnimEndSetWait, NULL,
        ftPhysicsApplyGroundVelTransN, mpCommonSetFighterFallOnEdgeBreak },
    [nFTCommonStatusDownForwardU - nFTCommonStatusActionStart] = { { nFTCommonMotionDownForwardU, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftAnimEndSetWait, NULL,
        ftPhysicsApplyGroundVelTransN, mpCommonSetFighterFallOnEdgeBreak },
    [nFTCommonStatusDownBackD - nFTCommonStatusActionStart] = { { nFTCommonMotionDownBackD, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftAnimEndSetWait, NULL,
        ftPhysicsApplyGroundVelTransN, mpCommonSetFighterFallOnEdgeBreak },
    [nFTCommonStatusDownBackU - nFTCommonStatusActionStart] = { { nFTCommonMotionDownBackU, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftAnimEndSetWait, NULL,
        ftPhysicsApplyGroundVelTransN, mpCommonSetFighterFallOnEdgeBreak },
    [nFTCommonStatusDownAttackD - nFTCommonStatusActionStart] = { { nFTCommonMotionDownAttackD, nFTMotionAttackIDDownAttackD }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDDownAttackD } },
        ftAnimEndSetWait, NULL,
        ftPhysicsApplyGroundVelFriction, mpCommonSetFighterFallOnEdgeBreak },
    [nFTCommonStatusDownAttackU - nFTCommonStatusActionStart] = { { nFTCommonMotionDownAttackU, nFTMotionAttackIDDownAttackU }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDDownAttackU } },
        ftAnimEndSetWait, NULL,
        ftPhysicsApplyGroundVelFriction, mpCommonSetFighterFallOnEdgeBreak },
    /* ftcommonstatus.h status 81, the neutral ground tech: a
 * fighter that techs with the stick centred springs straight up in place
 * (Passive), entered by ftCommonPassiveCheckInterruptDamage. Ends the tech
 * anim (ftAnimEndSetWait) into Wait, ground friction, break on ground. */
    [nFTCommonStatusPassive - nFTCommonStatusActionStart] = { { nFTCommonMotionPassive, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftAnimEndSetWait, NULL,
        ftPhysicsApplyGroundVelFriction, mpCommonSetFighterFallOnGroundBreak },
    /* ftcommonstatus.h statuses 82-83, from ft/ftcommon/ftcommonrebound.c
 * (compiled unmodified,). ReboundWait's script id is -1:
 * the shield-poked fighter holds whatever pose it was in while it
 * slides, and Rebound plays the flinch at the speed ReboundWait
 * computed. ft/ftmain.c:2021 has been calling into a do-nothing stub
 * for these since; these two rows are what make that call
 * arrive somewhere. */
    [nFTCommonStatusReboundWait - nFTCommonStatusActionStart] = { { -1, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonReboundWaitProcUpdate, NULL, ftPhysicsApplyGroundVelFriction,
        mpCommonSetFighterFallOnGroundBreak },
    [nFTCommonStatusRebound - nFTCommonStatusActionStart] = { { nFTCommonMotionRebound, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonReboundProcUpdate, NULL, ftPhysicsApplyGroundVelFriction,
        mpCommonSetFighterFallOnGroundBreak },
    [nFTCommonStatusCliffCatch - nFTCommonStatusActionStart] = { { nFTCommonMotionCliffCatch, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonCliffCatchProcUpdate, NULL, ftCommonCliffCommonProcPhysics,
        ftCommonCliffCommonProcMap },
    [nFTCommonStatusCliffWait - nFTCommonStatusActionStart] = { { nFTCommonMotionCliffWait, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        NULL, ftCommonCliffWaitProcInterrupt, ftCommonCliffCommonProcPhysics,
        ftCommonCliffCommonProcMap },
    /* ftcommonstatus.h statuses 86-99. */
    [nFTCommonStatusCliffQuick - nFTCommonStatusActionStart] = { { nFTCommonMotionCliffQuick, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonCliffQuickProcUpdate, NULL, ftCommonCliffCommonProcPhysics,
        ftCommonCliffCommonProcMap },
    [nFTCommonStatusCliffClimbQuick1 - nFTCommonStatusActionStart] = { { nFTCommonMotionCliffClimbQuick1, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonCliffClimbQuick1ProcUpdate, NULL,
        ftCommonCliffCommonProcPhysics, ftCommonCliffCommonProcMap },
    [nFTCommonStatusCliffClimbQuick2 - nFTCommonStatusActionStart] = { { nFTCommonMotionCliffClimbQuick2, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonCliffCommon2ProcUpdate, NULL, ftCommonCliffCommon2ProcPhysics,
        ftCommonCliffClimbCommon2ProcMap },
    [nFTCommonStatusCliffSlow - nFTCommonStatusActionStart] = { { nFTCommonMotionCliffSlow, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonCliffSlowProcUpdate, NULL, ftCommonCliffCommonProcPhysics,
        ftCommonCliffCommonProcMap },
    [nFTCommonStatusCliffClimbSlow1 - nFTCommonStatusActionStart] = { { nFTCommonMotionCliffClimbSlow1, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonCliffClimbSlow1ProcUpdate, NULL,
        ftCommonCliffCommonProcPhysics, ftCommonCliffCommonProcMap },
    [nFTCommonStatusCliffClimbSlow2 - nFTCommonStatusActionStart] = { { nFTCommonMotionCliffClimbSlow2, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonCliffCommon2ProcUpdate, NULL, ftCommonCliffCommon2ProcPhysics,
        ftCommonCliffClimbCommon2ProcMap },
    /* ftcommonstatus.h statuses 92-95, from ft/ftcommon/ftcommoncliffattack.c
 * (compiled unmodified,). The ledge attack was the one
 * ledge release with no rows: its two hitboxes come from the motion
 * scripts the pack already carries, the same place the jab's and the
 * tilts' come from since so the only thing it was missing
 * was the four functions and these four rows. */
    [nFTCommonStatusCliffAttackQuick1 - nFTCommonStatusActionStart] = { { nFTCommonMotionCliffAttackQuick1, nFTMotionAttackIDCliffAttackQuick }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDCliffAttackQuick } },
        ftCommonCliffAttackQuick1ProcUpdate, NULL,
        ftCommonCliffCommonProcPhysics, ftCommonCliffCommonProcMap },
    [nFTCommonStatusCliffAttackQuick2 - nFTCommonStatusActionStart] = { { nFTCommonMotionCliffAttackQuick2, nFTMotionAttackIDCliffAttackQuick }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDCliffAttackQuick } },
        ftCommonCliffCommon2ProcUpdate, NULL, ftCommonCliffCommon2ProcPhysics,
        ftCommonCliffAttackEscape2ProcMap },
    [nFTCommonStatusCliffAttackSlow1 - nFTCommonStatusActionStart] = { { nFTCommonMotionCliffAttackSlow1, nFTMotionAttackIDCliffAttackSlow }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDCliffAttackSlow } },
        ftCommonCliffAttackSlow1ProcUpdate, NULL,
        ftCommonCliffCommonProcPhysics, ftCommonCliffCommonProcMap },
    [nFTCommonStatusCliffAttackSlow2 - nFTCommonStatusActionStart] = { { nFTCommonMotionCliffAttackSlow2, nFTMotionAttackIDCliffAttackSlow }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDCliffAttackSlow } },
        ftCommonCliffCommon2ProcUpdate, NULL, ftCommonCliffCommon2ProcPhysics,
        ftCommonCliffAttackEscape2ProcMap },
    [nFTCommonStatusCliffEscapeQuick1 - nFTCommonStatusActionStart] = { { nFTCommonMotionCliffEscapeQuick1, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonCliffEscapeQuick1ProcUpdate, NULL,
        ftCommonCliffCommonProcPhysics, ftCommonCliffCommonProcMap },
    [nFTCommonStatusCliffEscapeQuick2 - nFTCommonStatusActionStart] = { { nFTCommonMotionCliffEscapeQuick2, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonCliffCommon2ProcUpdate, NULL, ftCommonCliffCommon2ProcPhysics,
        ftCommonCliffAttackEscape2ProcMap },
    [nFTCommonStatusCliffEscapeSlow1 - nFTCommonStatusActionStart] = { { nFTCommonMotionCliffEscapeSlow1, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonCliffEscapeSlow1ProcUpdate, NULL,
        ftCommonCliffCommonProcPhysics, ftCommonCliffCommonProcMap },
    [nFTCommonStatusCliffEscapeSlow2 - nFTCommonStatusActionStart] = { { nFTCommonMotionCliffEscapeSlow2, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonCliffCommon2ProcUpdate, NULL, ftCommonCliffCommon2ProcPhysics,
        ftCommonCliffAttackEscape2ProcMap },
    /* ---- the item-use step: statuses 100-151, the whole fighter-side
 * item family, which ftcommonstatus.h has and this table did not.
 * Fifty-two rows in the decomp's own order, transcribed from
 * ft/ftcommon/ftcommonstatus.h:2013-2620 with every column as it
 * stands there.
 *
 * The procs are all real: this step compiles ftcommonget.c,
 * ftcommonitemthrow.c, ftcommonitemswing.c, ftcommonitemshoot.c,
 * ft/fthammer.c and the five ftcommonhammer*.c unmodified
 * (src/game/ssb64/Makefile), so every name below is the game's own
 * function rather than a hand-copy.
 *
 * Two oddities of the game's, both kept. LiftWait and LiftTurn carry
 * motion id -2, which plays no animation at all: the fighter holds
 * whatever pose HeavyGet left him in, and this port's motion loader
 * already ignores a negative id (ftMainSetStatus above). And five of
 * the six Hammer rows name nFTCommonMotionHammerWalk as their script
 * -- HammerTurn, HammerKneeBend, HammerFall and HammerLanding all
 * play the walk, because the Hammer's swing is a colour-animated
 * loop the fighter carries through every one of them. ---- */
    /* 100 LightGet */
    [nFTCommonStatusLightGet - nFTCommonStatusActionStart] = { { nFTCommonMotionLightGet, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonGetProcUpdate, NULL,
        ftPhysicsApplyGroundVelFriction, ftCommonLightGetProcMap },
    /* 101 HeavyGet */
    [nFTCommonStatusHeavyGet - nFTCommonStatusActionStart] = { { nFTCommonMotionHeavyGet, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonGetProcUpdate, NULL,
        ftPhysicsApplyGroundVelFriction, ftCommonHeavyGetProcMap },
    /* 102 LiftWait */
    [nFTCommonStatusLiftWait - nFTCommonStatusActionStart] = { { -2, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        NULL, ftCommonLiftWaitProcInterrupt,
        ftPhysicsApplyGroundVelFriction, ftCommonHeavyGetProcMap },
    /* 103 LiftTurn */
    [nFTCommonStatusLiftTurn - nFTCommonStatusActionStart] = { { -2, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonLiftTurnProcUpdate, ftCommonLiftTurnProcInterrupt,
        ftPhysicsApplyGroundVelFriction, ftCommonHeavyGetProcMap },
    /* 104 LightThrowDrop */
    [nFTCommonStatusLightThrowDrop - nFTCommonStatusActionStart] = { { nFTCommonMotionLightThrowDrop, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDItemThrow } },
        ftCommonItemThrowProcUpdate, NULL,
        ftPhysicsApplyGroundVelFriction, mpCommonSetFighterFallOnEdgeBreak },
    /* 105 LightThrowDash */
    [nFTCommonStatusLightThrowDash - nFTCommonStatusActionStart] = { { nFTCommonMotionLightThrowDash, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDItemThrow } },
        ftCommonItemThrowProcUpdate, NULL,
        ftPhysicsApplyGroundVelTransN, mpCommonSetFighterFallOnEdgeBreak },
    /* 106 LightThrowF */
    [nFTCommonStatusLightThrowF - nFTCommonStatusActionStart] = { { nFTCommonMotionLightThrowF, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDItemThrow } },
        ftCommonItemThrowProcUpdate, NULL,
        ftPhysicsApplyGroundVelFriction, mpCommonSetFighterFallOnEdgeBreak },
    /* 107 LightThrowB */
    [nFTCommonStatusLightThrowB - nFTCommonStatusActionStart] = { { nFTCommonMotionLightThrowB, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDItemThrow } },
        ftCommonItemThrowProcUpdate, NULL,
        ftPhysicsApplyGroundVelFriction, mpCommonSetFighterFallOnEdgeBreak },
    /* 108 LightThrowHi */
    [nFTCommonStatusLightThrowHi - nFTCommonStatusActionStart] = { { nFTCommonMotionLightThrowHi, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDItemThrow } },
        ftCommonItemThrowProcUpdate, NULL,
        ftPhysicsApplyGroundVelFriction, mpCommonSetFighterFallOnEdgeBreak },
    /* 109 LightThrowLw */
    [nFTCommonStatusLightThrowLw - nFTCommonStatusActionStart] = { { nFTCommonMotionLightThrowLw, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDItemThrow } },
        ftCommonItemThrowProcUpdate, NULL,
        ftPhysicsApplyGroundVelFriction, mpCommonSetFighterFallOnEdgeBreak },
    /* 110 LightThrowF4 */
    [nFTCommonStatusLightThrowF4 - nFTCommonStatusActionStart] = { { nFTCommonMotionLightThrowF4, nFTMotionAttackIDNone }, { { 0, TRUE, nMPKineticsGround, FALSE, nFTStatusAttackIDItemThrow } },
        ftCommonItemThrowProcUpdate, NULL,
        ftPhysicsApplyGroundVelFriction, mpCommonSetFighterFallOnEdgeBreak },
    /* 111 LightThrowB4 */
    [nFTCommonStatusLightThrowB4 - nFTCommonStatusActionStart] = { { nFTCommonMotionLightThrowB4, nFTMotionAttackIDNone }, { { 0, TRUE, nMPKineticsGround, FALSE, nFTStatusAttackIDItemThrow } },
        ftCommonItemThrowProcUpdate, NULL,
        ftPhysicsApplyGroundVelFriction, mpCommonSetFighterFallOnEdgeBreak },
    /* 112 LightThrowHi4 */
    [nFTCommonStatusLightThrowHi4 - nFTCommonStatusActionStart] = { { nFTCommonMotionLightThrowHi4, nFTMotionAttackIDNone }, { { 0, TRUE, nMPKineticsGround, FALSE, nFTStatusAttackIDItemThrow } },
        ftCommonItemThrowProcUpdate, NULL,
        ftPhysicsApplyGroundVelFriction, mpCommonSetFighterFallOnEdgeBreak },
    /* 113 LightThrowLw4 */
    [nFTCommonStatusLightThrowLw4 - nFTCommonStatusActionStart] = { { nFTCommonMotionLightThrowLw4, nFTMotionAttackIDNone }, { { 0, TRUE, nMPKineticsGround, FALSE, nFTStatusAttackIDItemThrow } },
        ftCommonItemThrowProcUpdate, NULL,
        ftPhysicsApplyGroundVelFriction, mpCommonSetFighterFallOnEdgeBreak },
    /* 114 LightThrowAirF */
    [nFTCommonStatusLightThrowAirF - nFTCommonStatusActionStart] = { { nFTCommonMotionLightThrowAirF, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsAir, FALSE, nFTStatusAttackIDItemThrow } },
        ftCommonItemThrowProcUpdate, NULL,
        ftPhysicsApplyAirVelDrift, mpCommonProcFighterWaitOrLanding },
    /* 115 LightThrowAirB */
    [nFTCommonStatusLightThrowAirB - nFTCommonStatusActionStart] = { { nFTCommonMotionLightThrowAirB, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsAir, FALSE, nFTStatusAttackIDItemThrow } },
        ftCommonItemThrowProcUpdate, NULL,
        ftPhysicsApplyAirVelDrift, mpCommonProcFighterWaitOrLanding },
    /* 116 LightThrowAirHi */
    [nFTCommonStatusLightThrowAirHi - nFTCommonStatusActionStart] = { { nFTCommonMotionLightThrowAirHi, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsAir, FALSE, nFTStatusAttackIDItemThrow } },
        ftCommonItemThrowProcUpdate, NULL,
        ftPhysicsApplyAirVelDrift, mpCommonProcFighterWaitOrLanding },
    /* 117 LightThrowAirLw */
    [nFTCommonStatusLightThrowAirLw - nFTCommonStatusActionStart] = { { nFTCommonMotionLightThrowAirLw, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsAir, FALSE, nFTStatusAttackIDItemThrow } },
        ftCommonItemThrowProcUpdate, NULL,
        ftPhysicsApplyAirVelDrift, mpCommonProcFighterWaitOrLanding },
    /* 118 LightThrowAirF4 */
    [nFTCommonStatusLightThrowAirF4 - nFTCommonStatusActionStart] = { { nFTCommonMotionLightThrowAirF4, nFTMotionAttackIDNone }, { { 0, TRUE, nMPKineticsAir, FALSE, nFTStatusAttackIDItemThrow } },
        ftCommonItemThrowProcUpdate, NULL,
        ftPhysicsApplyAirVelDrift, mpCommonProcFighterWaitOrLanding },
    /* 119 LightThrowAirB4 */
    [nFTCommonStatusLightThrowAirB4 - nFTCommonStatusActionStart] = { { nFTCommonMotionLightThrowAirB4, nFTMotionAttackIDNone }, { { 0, TRUE, nMPKineticsAir, FALSE, nFTStatusAttackIDItemThrow } },
        ftCommonItemThrowProcUpdate, NULL,
        ftPhysicsApplyAirVelDrift, mpCommonProcFighterWaitOrLanding },
    /* 120 LightThrowAirHi4 */
    [nFTCommonStatusLightThrowAirHi4 - nFTCommonStatusActionStart] = { { nFTCommonMotionLightThrowAirHi4, nFTMotionAttackIDNone }, { { 0, TRUE, nMPKineticsAir, FALSE, nFTStatusAttackIDItemThrow } },
        ftCommonItemThrowProcUpdate, NULL,
        ftPhysicsApplyAirVelDrift, mpCommonProcFighterWaitOrLanding },
    /* 121 LightThrowAirLw4 */
    [nFTCommonStatusLightThrowAirLw4 - nFTCommonStatusActionStart] = { { nFTCommonMotionLightThrowAirLw4, nFTMotionAttackIDNone }, { { 0, TRUE, nMPKineticsAir, FALSE, nFTStatusAttackIDItemThrow } },
        ftCommonItemThrowProcUpdate, NULL,
        ftPhysicsApplyAirVelDrift, mpCommonProcFighterWaitOrLanding },
    /* 122 HeavyThrowF */
    [nFTCommonStatusHeavyThrowF - nFTCommonStatusActionStart] = { { nFTCommonMotionHeavyThrowF, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDItemThrow } },
        ftCommonItemThrowProcUpdate, NULL,
        ftPhysicsApplyGroundVelFriction, ftCommonHeavyThrowProcMap },
    /* 123 HeavyThrowB */
    [nFTCommonStatusHeavyThrowB - nFTCommonStatusActionStart] = { { nFTCommonMotionHeavyThrowB, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDItemThrow } },
        ftCommonItemThrowProcUpdate, NULL,
        ftPhysicsApplyGroundVelFriction, ftCommonHeavyThrowProcMap },
    /* 124 HeavyThrowF4 */
    [nFTCommonStatusHeavyThrowF4 - nFTCommonStatusActionStart] = { { nFTCommonMotionHeavyThrowF4, nFTMotionAttackIDNone }, { { 0, TRUE, nMPKineticsGround, FALSE, nFTStatusAttackIDItemThrow } },
        ftCommonItemThrowProcUpdate, NULL,
        ftPhysicsApplyGroundVelFriction, ftCommonHeavyThrowProcMap },
    /* 125 HeavyThrowB4 */
    [nFTCommonStatusHeavyThrowB4 - nFTCommonStatusActionStart] = { { nFTCommonMotionHeavyThrowB4, nFTMotionAttackIDNone }, { { 0, TRUE, nMPKineticsGround, FALSE, nFTStatusAttackIDItemThrow } },
        ftCommonItemThrowProcUpdate, NULL,
        ftPhysicsApplyGroundVelFriction, ftCommonHeavyThrowProcMap },
    /* 126 SwordSwing1 */
    [nFTCommonStatusSwordSwing1 - nFTCommonStatusActionStart] = { { nFTCommonMotionSwordSwing1, nFTMotionAttackIDSwordSwing1 }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDSwordSwing1 } },
        ftAnimEndSetWait, NULL,
        ftPhysicsApplyGroundVelFriction, mpCommonSetFighterFallOnEdgeBreak },
    /* 127 SwordSwing3 */
    [nFTCommonStatusSwordSwing3 - nFTCommonStatusActionStart] = { { nFTCommonMotionSwordSwing3, nFTMotionAttackIDSwordSwing3 }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDSwordSwing3 } },
        ftAnimEndSetWait, NULL,
        ftPhysicsApplyGroundVelFriction, mpCommonSetFighterFallOnEdgeBreak },
    /* 128 SwordSwing4 */
    [nFTCommonStatusSwordSwing4 - nFTCommonStatusActionStart] = { { nFTCommonMotionSwordSwing4, nFTMotionAttackIDSwordSwing4 }, { { 0, TRUE, nMPKineticsGround, FALSE, nFTStatusAttackIDSwordSwing4 } },
        ftAnimEndSetWait, NULL,
        ftPhysicsApplyGroundFrictionOrTransN, mpCommonSetFighterFallOnEdgeBreak },
    /* 129 SwordSwingDash */
    [nFTCommonStatusSwordSwingDash - nFTCommonStatusActionStart] = { { nFTCommonMotionSwordSwingDash, nFTMotionAttackIDSwordSwingDash }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDSwordSwingDash } },
        ftAnimEndSetWait, NULL,
        ftPhysicsApplyGroundVelTransN, mpCommonSetFighterFallOnEdgeBreak },
    /* 130 BatSwing1 */
    [nFTCommonStatusBatSwing1 - nFTCommonStatusActionStart] = { { nFTCommonMotionBatSwing1, nFTMotionAttackIDBatSwing1 }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDBatSwing1 } },
        ftAnimEndSetWait, NULL,
        ftPhysicsApplyGroundVelFriction, mpCommonSetFighterFallOnEdgeBreak },
    /* 131 BatSwing3 */
    [nFTCommonStatusBatSwing3 - nFTCommonStatusActionStart] = { { nFTCommonMotionBatSwing3, nFTMotionAttackIDBatSwing3 }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDBatSwing3 } },
        ftAnimEndSetWait, NULL,
        ftPhysicsApplyGroundVelFriction, mpCommonSetFighterFallOnEdgeBreak },
    /* 132 BatSwing4 */
    [nFTCommonStatusBatSwing4 - nFTCommonStatusActionStart] = { { nFTCommonMotionBatSwing4, nFTMotionAttackIDBatSwing4 }, { { 0, TRUE, nMPKineticsGround, FALSE, nFTStatusAttackIDBatSwing4 } },
        ftAnimEndSetWait, NULL,
        ftPhysicsApplyGroundFrictionOrTransN, mpCommonSetFighterFallOnEdgeBreak },
    /* 133 BatSwingDash */
    [nFTCommonStatusBatSwingDash - nFTCommonStatusActionStart] = { { nFTCommonMotionBatSwingDash, nFTMotionAttackIDBatSwingDash }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDBatSwingDash } },
        ftAnimEndSetWait, NULL,
        ftPhysicsApplyGroundVelTransN, mpCommonSetFighterFallOnEdgeBreak },
    /* 134 HarisenSwing1 */
    [nFTCommonStatusHarisenSwing1 - nFTCommonStatusActionStart] = { { nFTCommonMotionHarisenSwing1, nFTMotionAttackIDHarisenSwing1 }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDHarisenSwing1 } },
        ftCommonHarisenSwingProcUpdate, NULL,
        ftPhysicsApplyGroundVelFriction, mpCommonSetFighterFallOnEdgeBreak },
    /* 135 HarisenSwing3 */
    [nFTCommonStatusHarisenSwing3 - nFTCommonStatusActionStart] = { { nFTCommonMotionHarisenSwing3, nFTMotionAttackIDHarisenSwing3 }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDHarisenSwing3 } },
        ftCommonHarisenSwingProcUpdate, NULL,
        ftPhysicsApplyGroundVelFriction, mpCommonSetFighterFallOnEdgeBreak },
    /* 136 HarisenSwing4 */
    [nFTCommonStatusHarisenSwing4 - nFTCommonStatusActionStart] = { { nFTCommonMotionHarisenSwing4, nFTMotionAttackIDHarisenSwing4 }, { { 0, TRUE, nMPKineticsGround, FALSE, nFTStatusAttackIDHarisenSwing4 } },
        ftCommonHarisenSwingProcUpdate, NULL,
        ftPhysicsApplyGroundFrictionOrTransN, mpCommonSetFighterFallOnEdgeBreak },
    /* 137 HarisenSwingDash */
    [nFTCommonStatusHarisenSwingDash - nFTCommonStatusActionStart] = { { nFTCommonMotionHarisenSwingDash, nFTMotionAttackIDHarisenSwingDash }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDHarisenSwingDash } },
        ftCommonHarisenSwingProcUpdate, NULL,
        ftPhysicsApplyGroundVelTransN, mpCommonSetFighterFallOnEdgeBreak },
    /* 138 StarRodSwing1 */
    [nFTCommonStatusStarRodSwing1 - nFTCommonStatusActionStart] = { { nFTCommonMotionStarRodSwing1, nFTMotionAttackIDStarRodSwing1 }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDStarRodSwing1 } },
        ftCommonStarRodSwingProcUpdate, NULL,
        ftPhysicsApplyGroundVelFriction, mpCommonSetFighterFallOnEdgeBreak },
    /* 139 StarRodSwing3 */
    [nFTCommonStatusStarRodSwing3 - nFTCommonStatusActionStart] = { { nFTCommonMotionStarRodSwing3, nFTMotionAttackIDStarRodSwing3 }, { { 0, FALSE, nMPKineticsGround, TRUE, nFTStatusAttackIDStarRodSwing3 } },
        ftCommonStarRodSwingProcUpdate, NULL,
        ftPhysicsApplyGroundVelFriction, mpCommonSetFighterFallOnEdgeBreak },
    /* 140 StarRodSwing4 */
    [nFTCommonStatusStarRodSwing4 - nFTCommonStatusActionStart] = { { nFTCommonMotionStarRodSwing4, nFTMotionAttackIDStarRodSwing4 }, { { 0, TRUE, nMPKineticsGround, TRUE, nFTStatusAttackIDStarRodSwing4 } },
        ftCommonStarRodSwingProcUpdate, NULL,
        ftPhysicsApplyGroundFrictionOrTransN, mpCommonSetFighterFallOnEdgeBreak },
    /* 141 StarRodSwingDash */
    [nFTCommonStatusStarRodSwingDash - nFTCommonStatusActionStart] = { { nFTCommonMotionStarRodSwingDash, nFTMotionAttackIDStarRodSwingDash }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDStarRodSwingDash } },
        ftCommonStarRodSwingProcUpdate, NULL,
        ftPhysicsApplyGroundVelTransN, mpCommonSetFighterFallOnEdgeBreak },
    /* 142 LGunShoot */
    [nFTCommonStatusLGunShoot - nFTCommonStatusActionStart] = { { nFTCommonMotionLGunShoot, nFTMotionAttackIDLGunShoot }, { { 0, FALSE, nMPKineticsGround, TRUE, nFTStatusAttackIDLGunShoot } },
        ftCommonLGunShootProcUpdate, NULL,
        ftPhysicsApplyGroundVelFriction, ftCommonLGunShootProcMap },
    /* 143 LGunShootAir */
    [nFTCommonStatusLGunShootAir - nFTCommonStatusActionStart] = { { nFTCommonMotionLGunShootAir, nFTMotionAttackIDLGunShoot }, { { 0, FALSE, nMPKineticsAir, TRUE, nFTStatusAttackIDLGunShoot } },
        ftCommonLGunShootProcUpdate, NULL,
        ftPhysicsApplyAirVelDrift, ftCommonLGunShootAirProcMap },
    /* 144 FireFlowerShoot */
    [nFTCommonStatusFireFlowerShoot - nFTCommonStatusActionStart] = { { nFTCommonMotionFireFlowerShoot, nFTMotionAttackIDFireFlowerShoot }, { { 0, FALSE, nMPKineticsGround, TRUE, nFTStatusAttackIDFireFlowerShoot } },
        ftCommonFireFlowerShootProcUpdate, NULL,
        ftPhysicsApplyGroundVelFriction, ftCommonFireFlowerShootProcMap },
    /* 145 FireFlowerShootAir */
    [nFTCommonStatusFireFlowerShootAir - nFTCommonStatusActionStart] = { { nFTCommonMotionFireFlowerShootAir, nFTMotionAttackIDFireFlowerShoot }, { { 0, FALSE, nMPKineticsAir, TRUE, nFTStatusAttackIDFireFlowerShoot } },
        ftCommonFireFlowerShootProcUpdate, NULL,
        ftPhysicsApplyAirVelDrift, ftCommonFireFlowerShootAirProcMap },
    /* 146 HammerWait */
    [nFTCommonStatusHammerWait - nFTCommonStatusActionStart] = { { nFTCommonMotionHammerWait, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDHammer } },
        NULL, ftHammerProcInterrupt,
        ftPhysicsApplyGroundVelFriction, ftHammerProcMap },
    /* 147 HammerWalk */
    [nFTCommonStatusHammerWalk - nFTCommonStatusActionStart] = { { nFTCommonMotionHammerWalk, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDHammer } },
        NULL, ftCommonHammerWalkProcInterrupt,
        ftCommonWalkProcPhysics, ftHammerProcMap },
    /* 148 HammerTurn */
    [nFTCommonStatusHammerTurn - nFTCommonStatusActionStart] = { { nFTCommonMotionHammerWalk, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDHammer } },
        ftCommonHammerTurnProcUpdate, ftCommonHammerTurnProcInterrupt,
        ftPhysicsApplyGroundVelFriction, ftHammerProcMap },
    /* 149 HammerKneeBend */
    [nFTCommonStatusHammerKneeBend - nFTCommonStatusActionStart] = { { nFTCommonMotionHammerWalk, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDHammer } },
        ftCommonHammerKneeBendProcUpdate, ftCommonHammerKneeBendProcInterrupt,
        ftPhysicsApplyGroundVelFriction, ftHammerProcMap },
    /* 150 HammerFall */
    [nFTCommonStatusHammerFall - nFTCommonStatusActionStart] = { { nFTCommonMotionHammerWalk, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDHammer } },
        NULL, ftCommonHammerFallProcInterrupt,
        ftPhysicsApplyAirVelDriftFastFall, ftCommonHammerFallProcMap },
    /* 151 HammerLanding */
    [nFTCommonStatusHammerLanding - nFTCommonStatusActionStart] = { { nFTCommonMotionHammerWalk, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDHammer } },
        ftCommonHammerLandingProcUpdate, NULL,
        ftPhysicsApplyGroundVelFriction, ftHammerProcMap },
    /* ftcommonstatus.h statuses 190-201 are the ground
 * attacks, of which the dash attack (192) is included
 * -- its row is below, in place. Rows checked
 * against the decomp's table as the movement rows were. */
    [nFTCommonStatusAttack11 - nFTCommonStatusActionStart] = { { nFTCommonMotionAttack11, nFTMotionAttackIDAttack11 }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDAttack11 } },
        ftCommonAttack11ProcUpdate, ftCommonAttack11ProcInterrupt,
        ftPhysicsApplyGroundVelFriction, mpCommonSetFighterFallOnEdgeBreak },
    [nFTCommonStatusAttack12 - nFTCommonStatusActionStart] = { { nFTCommonMotionAttack12, nFTMotionAttackIDAttack12 }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDAttack12 } },
        ftCommonAttack12ProcUpdate, ftCommonAttack12ProcInterrupt,
        ftPhysicsApplyGroundVelFriction, mpCommonSetFighterFallOnEdgeBreak },
    /* ftcommonstatus.h status 192, the dash attack. Its
 * physics column is the one that reads the animation's TransN joint
 * -- the lunge's distance is the animation's, not a velocity. */
    [nFTCommonStatusAttackDash - nFTCommonStatusActionStart] = { { nFTCommonMotionAttackDash, nFTMotionAttackIDAttackDash }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDAttackDash } },
        ftAnimEndSetWait, NULL,
        ftPhysicsApplyGroundVelTransN, mpCommonSetFighterFallOnEdgeBreak },
    [nFTCommonStatusAttackS3Hi - nFTCommonStatusActionStart] = { { nFTCommonMotionAttackS3Hi, nFTMotionAttackIDAttackS3 }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDAttackS3 } },
        ftAnimEndSetWait, NULL,
        ftPhysicsApplyGroundFrictionOrTransN, mpCommonSetFighterFallOnEdgeBreak },
    [nFTCommonStatusAttackS3HiS - nFTCommonStatusActionStart] = { { nFTCommonMotionAttackS3HiS, nFTMotionAttackIDAttackS3 }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDAttackS3 } },
        ftAnimEndSetWait, NULL,
        ftPhysicsApplyGroundFrictionOrTransN, mpCommonSetFighterFallOnEdgeBreak },
    [nFTCommonStatusAttackS3 - nFTCommonStatusActionStart] = { { nFTCommonMotionAttackS3, nFTMotionAttackIDAttackS3 }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDAttackS3 } },
        ftAnimEndSetWait, NULL,
        ftPhysicsApplyGroundFrictionOrTransN, mpCommonSetFighterFallOnEdgeBreak },
    [nFTCommonStatusAttackS3LwS - nFTCommonStatusActionStart] = { { nFTCommonMotionAttackS3LwS, nFTMotionAttackIDAttackS3 }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDAttackS3 } },
        ftAnimEndSetWait, NULL,
        ftPhysicsApplyGroundFrictionOrTransN, mpCommonSetFighterFallOnEdgeBreak },
    [nFTCommonStatusAttackS3Lw - nFTCommonStatusActionStart] = { { nFTCommonMotionAttackS3Lw, nFTMotionAttackIDAttackS3 }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDAttackS3 } },
        ftAnimEndSetWait, NULL,
        ftPhysicsApplyGroundFrictionOrTransN, mpCommonSetFighterFallOnEdgeBreak },
    [nFTCommonStatusAttackHi3F - nFTCommonStatusActionStart] = { { nFTCommonMotionAttackHi3F, nFTMotionAttackIDAttackHi3 }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDAttackHi3 } },
        ftAnimEndSetWait, NULL,
        ftPhysicsApplyGroundVelFriction, mpCommonSetFighterFallOnEdgeBreak },
    [nFTCommonStatusAttackHi3 - nFTCommonStatusActionStart] = { { nFTCommonMotionAttackHi3, nFTMotionAttackIDAttackHi3 }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDAttackHi3 } },
        ftAnimEndSetWait, NULL,
        ftPhysicsApplyGroundFrictionOrTransN, mpCommonSetFighterFallOnEdgeBreak },
    [nFTCommonStatusAttackHi3B - nFTCommonStatusActionStart] = { { nFTCommonMotionAttackHi3B, nFTMotionAttackIDAttackHi3 }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDAttackHi3 } },
        ftAnimEndSetWait, NULL,
        ftPhysicsApplyGroundVelFriction, mpCommonSetFighterFallOnEdgeBreak },
    [nFTCommonStatusAttackLw3 - nFTCommonStatusActionStart] = { { nFTCommonMotionAttackLw3, nFTMotionAttackIDAttackLw3 }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDAttackLw3 } },
        ftCommonAttackLw3ProcUpdate, ftCommonAttackLw3ProcInterrupt,
        ftPhysicsApplyGroundFrictionOrTransN, mpCommonSetFighterFallOnEdgeBreak },

    /* ---- ftcommonstatus.h statuses 202-208, the SMASH attacks
 * -- the seven rows had to grow past to reach
 * the aerials, and the last holes in this table. They are the only
 * rows in it whose "is smash attack" column is TRUE, which is what
 * the knockback formula reads to charge them. The five forward
 * smashes differ only in motion: one status per stick angle, all
 * five sharing ftCommonAttackS4ProcUpdate, all five with no
 * interrupt (a smash runs to its end). The up smash is the plain
 * ftAnimEndSetWait, and the down smash the same but for its
 * physics: it alone takes ftPhysicsApplyGroundVelFriction rather
 * than ftPhysicsApplyGroundFrictionOrTransN, the decomp's own
 * asymmetry, kept verbatim. ---------------------------------------- */
    [nFTCommonStatusAttackS4Hi - nFTCommonStatusActionStart] = { { nFTCommonMotionAttackS4Hi, nFTMotionAttackIDAttackS4 }, { { 0, TRUE, nMPKineticsGround, FALSE, nFTStatusAttackIDAttackS4 } },
        ftCommonAttackS4ProcUpdate, NULL,
        ftPhysicsApplyGroundFrictionOrTransN, mpCommonSetFighterFallOnEdgeBreak },
    [nFTCommonStatusAttackS4HiS - nFTCommonStatusActionStart] = { { nFTCommonMotionAttackS4HiS, nFTMotionAttackIDAttackS4 }, { { 0, TRUE, nMPKineticsGround, FALSE, nFTStatusAttackIDAttackS4 } },
        ftCommonAttackS4ProcUpdate, NULL,
        ftPhysicsApplyGroundFrictionOrTransN, mpCommonSetFighterFallOnEdgeBreak },
    [nFTCommonStatusAttackS4 - nFTCommonStatusActionStart] = { { nFTCommonMotionAttackS4, nFTMotionAttackIDAttackS4 }, { { 0, TRUE, nMPKineticsGround, FALSE, nFTStatusAttackIDAttackS4 } },
        ftCommonAttackS4ProcUpdate, NULL,
        ftPhysicsApplyGroundFrictionOrTransN, mpCommonSetFighterFallOnEdgeBreak },
    [nFTCommonStatusAttackS4LwS - nFTCommonStatusActionStart] = { { nFTCommonMotionAttackS4LwS, nFTMotionAttackIDAttackS4 }, { { 0, TRUE, nMPKineticsGround, FALSE, nFTStatusAttackIDAttackS4 } },
        ftCommonAttackS4ProcUpdate, NULL,
        ftPhysicsApplyGroundFrictionOrTransN, mpCommonSetFighterFallOnEdgeBreak },
    [nFTCommonStatusAttackS4Lw - nFTCommonStatusActionStart] = { { nFTCommonMotionAttackS4Lw, nFTMotionAttackIDAttackS4 }, { { 0, TRUE, nMPKineticsGround, FALSE, nFTStatusAttackIDAttackS4 } },
        ftCommonAttackS4ProcUpdate, NULL,
        ftPhysicsApplyGroundFrictionOrTransN, mpCommonSetFighterFallOnEdgeBreak },
    [nFTCommonStatusAttackHi4 - nFTCommonStatusActionStart] = { { nFTCommonMotionAttackHi4, nFTMotionAttackIDAttackHi4 }, { { 0, TRUE, nMPKineticsGround, FALSE, nFTStatusAttackIDAttackHi4 } },
        ftAnimEndSetWait, NULL,
        ftPhysicsApplyGroundFrictionOrTransN, mpCommonSetFighterFallOnEdgeBreak },
    [nFTCommonStatusAttackLw4 - nFTCommonStatusActionStart] = { { nFTCommonMotionAttackLw4, nFTMotionAttackIDAttackLw4 }, { { 0, TRUE, nMPKineticsGround, FALSE, nFTStatusAttackIDAttackLw4 } },
        ftAnimEndSetWait, NULL,
        ftPhysicsApplyGroundVelFriction, mpCommonSetFighterFallOnEdgeBreak },

    /* ---- ftcommonstatus.h statuses 209-213, the AERIALS.
 * Five rows that differ only in motion and attack id: the update
 * column ends the swing into Fall, there is no interrupt (an aerial
 * runs to its end), the physics is the plain air drift, and the map
 * column is ftCommonAttackAirProcMap, which is where the landing lag
 * lives. AttackAirLw is the one exception, and only for Link: its
 * update column carries his rehit timer. --------------------------- */
    [nFTCommonStatusAttackAirN - nFTCommonStatusActionStart] = { { nFTCommonMotionAttackAirN, nFTMotionAttackIDAttackAirN }, { { 0, FALSE, nMPKineticsAir, FALSE, nFTStatusAttackIDAttackAirN } },
        ftAnimEndSetFall, NULL, ftPhysicsApplyAirVelDrift,
        ftCommonAttackAirProcMap },
    [nFTCommonStatusAttackAirF - nFTCommonStatusActionStart] = { { nFTCommonMotionAttackAirF, nFTMotionAttackIDAttackAirF }, { { 0, FALSE, nMPKineticsAir, FALSE, nFTStatusAttackIDAttackAirF } },
        ftAnimEndSetFall, NULL, ftPhysicsApplyAirVelDrift,
        ftCommonAttackAirProcMap },
    [nFTCommonStatusAttackAirB - nFTCommonStatusActionStart] = { { nFTCommonMotionAttackAirB, nFTMotionAttackIDAttackAirB }, { { 0, FALSE, nMPKineticsAir, FALSE, nFTStatusAttackIDAttackAirB } },
        ftAnimEndSetFall, NULL, ftPhysicsApplyAirVelDrift,
        ftCommonAttackAirProcMap },
    [nFTCommonStatusAttackAirHi - nFTCommonStatusActionStart] = { { nFTCommonMotionAttackAirHi, nFTMotionAttackIDAttackAirHi }, { { 0, FALSE, nMPKineticsAir, FALSE, nFTStatusAttackIDAttackAirHi } },
        ftAnimEndSetFall, NULL, ftPhysicsApplyAirVelDrift,
        ftCommonAttackAirProcMap },
    [nFTCommonStatusAttackAirLw - nFTCommonStatusActionStart] = { { nFTCommonMotionAttackAirLw, nFTMotionAttackIDAttackAirLw }, { { 0, FALSE, nMPKineticsAir, FALSE, nFTStatusAttackIDAttackAirLw } },
        ftCommonAttackAirLwProcUpdate, NULL, ftPhysicsApplyAirVelDrift,
        ftCommonAttackAirProcMap },

    /* ---- ftcommonstatus.h statuses 214-219, the AERIALS' LANDINGS
 *). One per aerial, plus the generic Null the map column
 * falls back to when the fighter's pack has no landing animation for
 * the attack. All six are the same four columns -- end into Wait,
 * ground friction, the cliff-edge map -- and each keeps the attack id
 * of the aerial it lands, which is how a landing hitbox (Link's
 * down-air, Yoshi's) still knows what it belongs to.
 *
 * NOTE the kinetics column, kept verbatim: the five real landings say
 * nMPKineticsAir and only LandingAirNull says Ground. That is the
 * decomp's own asymmetry and it is harmless -- ftMainSetStatus does
 * not move a fighter between kinetics, and both setters
 * (ftCommonLandingAirSetStatus in ftcommonlandingair.c,
 * ftCommonLandingAirNullSetStatus above) call mpCommonSetFighterGround
 * before they set the status. The column is read for the physics
 * dispatch, and ftPhysicsApplyGroundVelFriction is a ground leaf in
 * all six. ---------------------------------------------------------- */
    [nFTCommonStatusLandingAirN - nFTCommonStatusActionStart] = { { nFTCommonMotionLandingAirN, nFTMotionAttackIDAttackAirN }, { { 0, FALSE, nMPKineticsAir, FALSE, nFTStatusAttackIDAttackAirN } },
        ftAnimEndSetWait, NULL, ftPhysicsApplyGroundVelFriction,
        mpCommonProcFighterOnCliffEdge },
    [nFTCommonStatusLandingAirF - nFTCommonStatusActionStart] = { { nFTCommonMotionLandingAirF, nFTMotionAttackIDAttackAirF }, { { 0, FALSE, nMPKineticsAir, FALSE, nFTStatusAttackIDAttackAirF } },
        ftAnimEndSetWait, NULL, ftPhysicsApplyGroundVelFriction,
        mpCommonProcFighterOnCliffEdge },
    [nFTCommonStatusLandingAirB - nFTCommonStatusActionStart] = { { nFTCommonMotionLandingAirB, nFTMotionAttackIDAttackAirB }, { { 0, FALSE, nMPKineticsAir, FALSE, nFTStatusAttackIDAttackAirB } },
        ftAnimEndSetWait, NULL, ftPhysicsApplyGroundVelFriction,
        mpCommonProcFighterOnCliffEdge },
    [nFTCommonStatusLandingAirHi - nFTCommonStatusActionStart] = { { nFTCommonMotionLandingAirHi, nFTMotionAttackIDAttackAirHi }, { { 0, FALSE, nMPKineticsAir, FALSE, nFTStatusAttackIDAttackAirHi } },
        ftAnimEndSetWait, NULL, ftPhysicsApplyGroundVelFriction,
        mpCommonProcFighterOnCliffEdge },
    [nFTCommonStatusLandingAirLw - nFTCommonStatusActionStart] = { { nFTCommonMotionLandingAirLw, nFTMotionAttackIDAttackAirLw }, { { 0, FALSE, nMPKineticsAir, FALSE, nFTStatusAttackIDAttackAirLw } },
        ftAnimEndSetWait, NULL, ftPhysicsApplyGroundVelFriction,
        mpCommonProcFighterOnCliffEdge },
    [nFTCommonStatusLandingAirNull - nFTCommonStatusActionStart] = { { nFTCommonMotionLandingAirNull, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftAnimEndSetWait, NULL, ftPhysicsApplyGroundVelFriction,
        mpCommonProcFighterOnCliffEdge },
    /* ---- the four singles: statuses 14, 58, 59 and 165.
 * These are the rows the step-57 census found still zeroed that
 * belong to no unported family -- everything each one calls was
 * already linked, so the hole was the row and nothing else.
 *
 * 14 WalkEnd is all-NULL in the decomp's table too, and no code in
 * the game sets it: the walk ends into Wait, not into this. The row
 * carries its motion id and nothing else, exactly as the game's
 * does, so the port's table stops disagreeing with the ROM's.
 *
 * 58 FallSpecial is the helpless fall every up-B drops into, and its
 * four procs come from ft/ftcommon/ftcommonfallspecial.c -- which
 * this build COMPILES UNMODIFIED
 * (FTCOMMON_STATUS_OBJS in the Makefile). Six ported up-Bs (Mario,
 * Fox, Samus, Captain, Donkey, Purin) call ftCommonFallSpecialSetStatus
 * and land in status 58, where the empty row meant no physics, no
 * interrupt and no map ran: a fighter left hanging where its up-B
 * ended. This row is the fix.
 *
 * 59 LandingFallSpecial is that fall's landing, and its setter
 * (ftCommonLandingFallSpecialSetStatus, above) is ported.
 * All four procs are
 * Landing's own.
 *
 * 165 FuraSleep's update is new this step (its section is above).
 *
 * The kinetics column of 58 is nMPKineticsGround in the decomp's
 * table even though the status is airborne, exactly as written; the
 * setter calls mpCommonSetFighterAir itself. Verbatim. ---------- */
    [nFTCommonStatusWalkEnd - nFTCommonStatusActionStart] = { { nFTCommonMotionWalkEnd, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        NULL, NULL, NULL, NULL },
    [nFTCommonStatusFallSpecial - nFTCommonStatusActionStart] = { { nFTCommonMotionFallSpecial, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        NULL, ftCommonFallSpecialProcInterrupt,
        ftCommonFallSpecialProcPhysics, ftCommonFallSpecialProcMap },
    [nFTCommonStatusLandingFallSpecial - nFTCommonStatusActionStart] = { { nFTCommonMotionLandingFallSpecial, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftAnimEndSetWait, ftCommonLandingProcInterrupt,
        ftPhysicsApplyGroundVelFriction, mpCommonProcFighterOnCliffEdge },
    [nFTCommonStatusFuraSleep - nFTCommonStatusActionStart] = { { nFTCommonMotionFuraSleep, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonFuraSleepProcUpdate, NULL,
        ftPhysicsApplyGroundVelFriction, mpCommonSetFighterFallOnGroundBreak },
    /* ---- status 60, Hyrule's tornado. The row the port's
 * own ftMainSearchHitHazard has been routing into since, whose
 * SetStatus was a no-op stub until this step -- src/dc/
 * ftcommontwister.c is the file, and its header says why. The
 * kinetics column is nMPKineticsGround in the decomp's table even
 * though the status is aerial, exactly as written; SetStatus calls
 * mpCommonSetFighterAir itself. The Map proc is
 * mpCommonProcFighterProject, which is what keeps the spinning
 * fighter off the floor. Verbatim. ---------- */
    [nFTCommonStatusTwister - nFTCommonStatusActionStart] = { { nFTCommonMotionTwister, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonTwisterProcUpdate, NULL,
        ftCommonTwisterProcPhysics, mpCommonProcFighterProject },
    /* ---- status 61, Kongo Jungle's barrel cannon. The
 * row ftMainSetHitHazard has been routing into since, whose
 * SetStatus was a no-op stub until this step -- src/dc/
 * ftcommontaru.c is the file. Two things the decomp's own table has
 * that the twister's does not: the script id is -1 (the captured
 * fighter borrows whatever motion it was in, and SetStatus calls
 * ftMainPlayAnimEventsAll over it), and there IS an interrupt --
 * grJungleTaruCannProcInterrupt's twin here, which lets the player
 * fire early. The kinetics column is nMPKineticsGround and the
 * status really is grounded, unlike the twister's: the cannon holds
 * the fighter and its own physics proc places it. Verbatim. ---------- */
    [nFTCommonStatusTaruCann - nFTCommonStatusActionStart] = { { -1, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonTaruCannProcUpdate, ftCommonTaruCannProcInterrupt,
        ftCommonTaruCannProcPhysics, mpCommonProcFighterProject },
    /* ---- statuses 62-65, Mushroom Kingdom's warp pipe:
 * ft/ftcommon/ftcommonstatus.h's rows, procs from
 * ft/ftcommon/ftcommondokan.c (compiled unmodified). DokanWait has no
 * motion: the fighter is invisible while the pipe carries it. Walk
 * shares End's update. Verbatim. ---------- */
    [nFTCommonStatusDokanStart - nFTCommonStatusActionStart] = { { nFTCommonMotionDokanStart, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonDokanStartProcUpdate, NULL,
        ftCommonDokanStartProcPhysics, mpCommonSetFighterFallOnEdgeBreak },
    [nFTCommonStatusDokanWait - nFTCommonStatusActionStart] = { { -1, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonDokanWaitProcUpdate, NULL,
        NULL, ftCommonDokanWaitProcMap },
    [nFTCommonStatusDokanEnd - nFTCommonStatusActionStart] = { { nFTCommonMotionDokanEnd, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonDokanEndProcUpdate, NULL,
        NULL, mpCommonSetFighterFallOnEdgeBreak },
    [nFTCommonStatusDokanWalk - nFTCommonStatusActionStart] = { { nFTCommonMotionDokanWalk, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonDokanEndProcUpdate, NULL,
        NULL, mpCommonUpdateFighterProjectFloor },
    /* ---- statuses 177 and 178, the caught side of Yoshi's Egg Lay,
 * the first two rows of the thrown/capture band 173-188. Both procs come from
 * ft/ftcommon/ftcommoncaptureyoshi.c, compiled unmodified.
 *
 * 177 CaptureYoshi is the swallow: the victim borrows
 * nFTCommonMotionCapturePulled, the same motion the ordinary pulled
 * grab uses, and has no update or interrupt at all -- its physics
 * proc is the whole status, dragging the victim to Yoshi's mouth
 * and stepping the three-stage counter that ends in the egg. Its
 * map proc is bare mpCommonUpdateFighterProjectFloor, which is a
 * projection and not a landing: a swallowed fighter does not land.
 *
 * 178 YoshiEgg is the egg, and it is the only status in the common
 * table whose fighter is invisible for the whole of it. Four procs,
 * all its own. ---------------------------------------------------- */
    [nFTCommonStatusCaptureYoshi - nFTCommonStatusActionStart] = { { nFTCommonMotionCapturePulled, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        NULL, NULL,
        ftCommonCaptureYoshiProcPhysics, mpCommonUpdateFighterProjectFloor },
    [nFTCommonStatusYoshiEgg - nFTCommonStatusActionStart] = { { nFTCommonMotionYoshiEgg, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonYoshiEggProcUpdate, ftCommonYoshiEggProcInterrupt,
        ftCommonYoshiEggProcPhysics, ftCommonYoshiEggProcMap },
    /* ---- Kirby's mouth: the caught fighter's four rows --
 * ft/ftcommon/ftcommonstatus.h statuses 173-176, procs from
 * ft/ftcommon/ftcommoncapturekirby.c (compiled unmodified). These
 * are what the SWALLOWED fighter is in, and together with 177/178
 * (Yoshi's,) they fill the low half of the thrown/capture
 * band contiguously, 173-178.
 *
 * 173 CaptureKirby is the reel-in: no update and no interrupt, just
 * ftCommonCaptureKirbyProcPhysics dragging the victim toward the
 * mouth, and a projection rather than a landing (same shape as
 * CaptureYoshi above -- a fighter being eaten does not land).
 * 174 CaptureWaitKirby is the wait inside the mouth, and it is the
 * only row in this whole table with a NULL PHYSICS proc: a
 * swallowed fighter has no physics of his own, Kirby carries him.
 * Its motion is -1 for the same reason CaptureWait's is -- the
 * victim is not drawn.
 * 175 ThrownKirbyStar and 176 ThrownCopyStar are the spit, the two
 * ways out. They share ftCommonThrownCommonStarProcMap and differ
 * only in whether Kirby kept the copy. ------------------------- */
    /* 173 CaptureKirby */
    [nFTCommonStatusCaptureKirby - nFTCommonStatusActionStart] = { { nFTCommonMotionDamageFall, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        NULL, NULL,
        ftCommonCaptureKirbyProcPhysics, mpCommonUpdateFighterProjectFloor },
    /* 174 CaptureWaitKirby */
    [nFTCommonStatusCaptureWaitKirby - nFTCommonStatusActionStart] = { { -1, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        NULL, ftCommonCaptureWaitKirbyProcInterrupt,
        NULL, ftCommonCaptureWaitKirbyProcMap },
    /* 175 ThrownKirbyStar */
    [nFTCommonStatusThrownKirbyStar - nFTCommonStatusActionStart] = { { nFTCommonMotionThrownKirbyStar, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonThrownKirbyStarProcUpdate, NULL,
        ftCommonThrownKirbyStarProcPhysics, ftCommonThrownCommonStarProcMap },
    /* 176 ThrownCopyStar */
    [nFTCommonStatusThrownCopyStar - nFTCommonStatusActionStart] = { { nFTCommonMotionThrownCopyStar, nFTMotionAttackIDNone }, { { 0, FALSE, nMPKineticsGround, FALSE, nFTStatusAttackIDNone } },
        ftCommonThrownCopyStarProcUpdate, NULL,
        ftCommonThrownCopyStarProcPhysics, ftCommonThrownCommonStarProcMap },
};

static const char *kStatusNames[nFTCommonStatusLandingAirNull + 1] = {
    [nFTCommonStatusDeadDown] = "DeadDown",
    [nFTCommonStatusDeadLeftRight] = "DeadLeftRight",
    [nFTCommonStatusDeadUpStar] = "DeadUpStar",
    [nFTCommonStatusDeadUpFall] = "DeadUpFall",
    [nFTCommonStatusSleep] = "Sleep",
    [nFTCommonStatusEntry] = "Entry",
    [nFTCommonStatusEntryNull] = "EntryNull",
    [nFTCommonStatusRebirthDown] = "RebirthDown",
    [nFTCommonStatusRebirthStand] = "RebirthStand",
    [nFTCommonStatusRebirthWait] = "RebirthWait",
    [nFTCommonStatusWait] = "Wait",
    [nFTCommonStatusWalkSlow] = "WalkSlow",
    [nFTCommonStatusWalkMiddle] = "WalkMiddle",
    [nFTCommonStatusWalkFast] = "WalkFast",
    [nFTCommonStatusWalkEnd] = "WalkEnd",
    [nFTCommonStatusDash] = "Dash",
    [nFTCommonStatusRun] = "Run",
    [nFTCommonStatusRunBrake] = "RunBrake",
    [nFTCommonStatusTurn] = "Turn",
    [nFTCommonStatusTurnRun] = "TurnRun",
    [nFTCommonStatusKneeBend] = "KneeBend",
    [nFTCommonStatusJumpF] = "JumpF",
    [nFTCommonStatusJumpB] = "JumpB",
    [nFTCommonStatusJumpAerialF] = "JumpAerialF",
    [nFTCommonStatusJumpAerialB] = "JumpAerialB",
    [nFTCommonStatusFall] = "Fall",
    [nFTCommonStatusFallAerial] = "FallAerial",
    [nFTCommonStatusSquat] = "Squat",
    [nFTCommonStatusGuardKneeBend] = "GuardKneeBend",
    [nFTCommonStatusGuardPass] = "GuardPass",
    [nFTCommonStatusGuardOn] = "GuardOn",
    [nFTCommonStatusGuard] = "Guard",
    [nFTCommonStatusGuardOff] = "GuardOff",
    [nFTCommonStatusGuardSetOff] = "GuardSetOff",
    [nFTCommonStatusShieldBreakFly] = "ShieldBreakFly",
    [nFTCommonStatusShieldBreakFall] = "ShieldBreakFall",
    [nFTCommonStatusShieldBreakDownD] = "ShieldBreakDownD",
    [nFTCommonStatusShieldBreakDownU] = "ShieldBreakDownU",
    [nFTCommonStatusShieldBreakStandD] = "ShieldBreakStandD",
    [nFTCommonStatusShieldBreakStandU] = "ShieldBreakStandU",
    [nFTCommonStatusFuraFura] = "FuraFura",
    [nFTCommonStatusFuraSleep] = "FuraSleep",
    [nFTCommonStatusTwister] = "Twister",
    [nFTCommonStatusTaruCann] = "TaruCann",
    [nFTCommonStatusCatch] = "Catch",
    [nFTCommonStatusCatchPull] = "CatchPull",
    [nFTCommonStatusCatchWait] = "CatchWait",
    [nFTCommonStatusThrowF] = "ThrowF",
    [nFTCommonStatusThrowB] = "ThrowB",
    [nFTCommonStatusCapturePulled] = "CapturePulled",
    [nFTCommonStatusCaptureWait] = "CaptureWait",
    [nFTCommonStatusThrownMarioBStart] = "ThrownMarioBStart",
    [nFTCommonStatusThrownMarioB] = "ThrownMarioB",
    [nFTCommonStatusThrownCommon] = "ThrownCommon",
    [nFTCommonStatusEscapeF] = "EscapeF",
    [nFTCommonStatusEscapeB] = "EscapeB",
    [nFTCommonStatusSquatWait] = "SquatWait",
    [nFTCommonStatusSquatRv] = "SquatRv",
    [nFTCommonStatusLandingLight] = "LandingLight",
    [nFTCommonStatusLandingHeavy] = "LandingHeavy",
    [nFTCommonStatusFallSpecial] = "FallSpecial",
    [nFTCommonStatusLandingFallSpecial] = "LandingFallSpecial",
    [nFTCommonStatusPass] = "Pass",
    [nFTCommonStatusOttottoWait] = "OttottoWait",
    [nFTCommonStatusOttotto] = "Ottotto",
    [nFTCommonStatusStopCeil] = "StopCeil",
    [nFTCommonStatusDamageHi1] = "DamageHi1",
    [nFTCommonStatusDamageHi2] = "DamageHi2",
    [nFTCommonStatusDamageHi3] = "DamageHi3",
    [nFTCommonStatusDamageN1] = "DamageN1",
    [nFTCommonStatusDamageN2] = "DamageN2",
    [nFTCommonStatusDamageN3] = "DamageN3",
    [nFTCommonStatusDamageLw1] = "DamageLw1",
    [nFTCommonStatusDamageLw2] = "DamageLw2",
    [nFTCommonStatusDamageLw3] = "DamageLw3",
    [nFTCommonStatusDamageAir1] = "DamageAir1",
    [nFTCommonStatusDamageAir2] = "DamageAir2",
    [nFTCommonStatusDamageAir3] = "DamageAir3",
    [nFTCommonStatusDamageE1] = "DamageE1",
    [nFTCommonStatusDamageE2] = "DamageE2",
    [nFTCommonStatusDamageFlyHi] = "DamageFlyHi",
    [nFTCommonStatusDamageFlyN] = "DamageFlyN",
    [nFTCommonStatusDamageFlyLw] = "DamageFlyLw",
    [nFTCommonStatusDamageFlyTop] = "DamageFlyTop",
    [nFTCommonStatusDamageFlyRoll] = "DamageFlyRoll",
    [nFTCommonStatusWallDamage] = "WallDamage",
    [nFTCommonStatusDamageFall] = "DamageFall",
    [nFTCommonStatusDownBounceD] = "DownBounceD",
    [nFTCommonStatusDownBounceU] = "DownBounceU",
    [nFTCommonStatusDownWaitD] = "DownWaitD",
    [nFTCommonStatusDownWaitU] = "DownWaitU",
    [nFTCommonStatusDownStandD] = "DownStandD",
    [nFTCommonStatusDownStandU] = "DownStandU",
    [nFTCommonStatusPassiveStandF] = "PassiveStandF",
    [nFTCommonStatusPassiveStandB] = "PassiveStandB",
    [nFTCommonStatusDownForwardD] = "DownForwardD",
    [nFTCommonStatusDownForwardU] = "DownForwardU",
    [nFTCommonStatusDownBackD] = "DownBackD",
    [nFTCommonStatusDownBackU] = "DownBackU",
    [nFTCommonStatusDownAttackD] = "DownAttackD",
    [nFTCommonStatusDownAttackU] = "DownAttackU",
    [nFTCommonStatusPassive] = "Passive",
    [nFTCommonStatusCliffCatch] = "CliffCatch",
    [nFTCommonStatusCliffWait] = "CliffWait",
    [nFTCommonStatusCliffQuick] = "CliffQuick",
    [nFTCommonStatusCliffClimbQuick1] = "CliffClimbQuick1",
    [nFTCommonStatusCliffClimbQuick2] = "CliffClimbQuick2",
    [nFTCommonStatusCliffSlow] = "CliffSlow",
    [nFTCommonStatusCliffClimbSlow1] = "CliffClimbSlow1",
    [nFTCommonStatusCliffClimbSlow2] = "CliffClimbSlow2",
    [nFTCommonStatusCliffEscapeQuick1] = "CliffEscapeQuick1",
    [nFTCommonStatusCliffEscapeQuick2] = "CliffEscapeQuick2",
    [nFTCommonStatusCliffEscapeSlow1] = "CliffEscapeSlow1",
    [nFTCommonStatusCliffEscapeSlow2] = "CliffEscapeSlow2",
    [nFTCommonStatusCliffAttackQuick1] = "CliffAttackQuick1",
    [nFTCommonStatusCliffAttackQuick2] = "CliffAttackQuick2",
    [nFTCommonStatusCliffAttackSlow1] = "CliffAttackSlow1",
    [nFTCommonStatusCliffAttackSlow2] = "CliffAttackSlow2",
    /* the item-use step's fifty-two, statuses 100-151 */
    [nFTCommonStatusLightGet] = "LightGet",
    [nFTCommonStatusHeavyGet] = "HeavyGet",
    [nFTCommonStatusLiftWait] = "LiftWait",
    [nFTCommonStatusLiftTurn] = "LiftTurn",
    [nFTCommonStatusLightThrowDrop] = "LightThrowDrop",
    [nFTCommonStatusLightThrowDash] = "LightThrowDash",
    [nFTCommonStatusLightThrowF] = "LightThrowF",
    [nFTCommonStatusLightThrowB] = "LightThrowB",
    [nFTCommonStatusLightThrowHi] = "LightThrowHi",
    [nFTCommonStatusLightThrowLw] = "LightThrowLw",
    [nFTCommonStatusLightThrowF4] = "LightThrowF4",
    [nFTCommonStatusLightThrowB4] = "LightThrowB4",
    [nFTCommonStatusLightThrowHi4] = "LightThrowHi4",
    [nFTCommonStatusLightThrowLw4] = "LightThrowLw4",
    [nFTCommonStatusLightThrowAirF] = "LightThrowAirF",
    [nFTCommonStatusLightThrowAirB] = "LightThrowAirB",
    [nFTCommonStatusLightThrowAirHi] = "LightThrowAirHi",
    [nFTCommonStatusLightThrowAirLw] = "LightThrowAirLw",
    [nFTCommonStatusLightThrowAirF4] = "LightThrowAirF4",
    [nFTCommonStatusLightThrowAirB4] = "LightThrowAirB4",
    [nFTCommonStatusLightThrowAirHi4] = "LightThrowAirHi4",
    [nFTCommonStatusLightThrowAirLw4] = "LightThrowAirLw4",
    [nFTCommonStatusHeavyThrowF] = "HeavyThrowF",
    [nFTCommonStatusHeavyThrowB] = "HeavyThrowB",
    [nFTCommonStatusHeavyThrowF4] = "HeavyThrowF4",
    [nFTCommonStatusHeavyThrowB4] = "HeavyThrowB4",
    [nFTCommonStatusSwordSwing1] = "SwordSwing1",
    [nFTCommonStatusSwordSwing3] = "SwordSwing3",
    [nFTCommonStatusSwordSwing4] = "SwordSwing4",
    [nFTCommonStatusSwordSwingDash] = "SwordSwingDash",
    [nFTCommonStatusBatSwing1] = "BatSwing1",
    [nFTCommonStatusBatSwing3] = "BatSwing3",
    [nFTCommonStatusBatSwing4] = "BatSwing4",
    [nFTCommonStatusBatSwingDash] = "BatSwingDash",
    [nFTCommonStatusHarisenSwing1] = "HarisenSwing1",
    [nFTCommonStatusHarisenSwing3] = "HarisenSwing3",
    [nFTCommonStatusHarisenSwing4] = "HarisenSwing4",
    [nFTCommonStatusHarisenSwingDash] = "HarisenSwingDash",
    [nFTCommonStatusStarRodSwing1] = "StarRodSwing1",
    [nFTCommonStatusStarRodSwing3] = "StarRodSwing3",
    [nFTCommonStatusStarRodSwing4] = "StarRodSwing4",
    [nFTCommonStatusStarRodSwingDash] = "StarRodSwingDash",
    [nFTCommonStatusLGunShoot] = "LGunShoot",
    [nFTCommonStatusLGunShootAir] = "LGunShootAir",
    [nFTCommonStatusFireFlowerShoot] = "FireFlowerShoot",
    [nFTCommonStatusFireFlowerShootAir] = "FireFlowerShootAir",
    [nFTCommonStatusHammerWait] = "HammerWait",
    [nFTCommonStatusHammerWalk] = "HammerWalk",
    [nFTCommonStatusHammerTurn] = "HammerTurn",
    [nFTCommonStatusHammerKneeBend] = "HammerKneeBend",
    [nFTCommonStatusHammerFall] = "HammerFall",
    [nFTCommonStatusHammerLanding] = "HammerLanding",
    [nFTCommonStatusReboundWait] = "ReboundWait",
    [nFTCommonStatusRebound] = "Rebound",
    [nFTCommonStatusAppeal] = "Appeal",
    [nFTCommonStatusAttack11] = "Attack11",
    [nFTCommonStatusAttack12] = "Attack12",
    [nFTCommonStatusAttackDash] = "AttackDash",
    [nFTCommonStatusAttackS3Hi] = "AttackS3Hi",
    [nFTCommonStatusAttackS3HiS] = "AttackS3HiS",
    [nFTCommonStatusAttackS3] = "AttackS3",
    [nFTCommonStatusAttackS3LwS] = "AttackS3LwS",
    [nFTCommonStatusAttackS3Lw] = "AttackS3Lw",
    [nFTCommonStatusAttackHi3F] = "AttackHi3F",
    [nFTCommonStatusAttackHi3] = "AttackHi3",
    [nFTCommonStatusAttackHi3B] = "AttackHi3B",
    [nFTCommonStatusAttackLw3] = "AttackLw3",
    [nFTCommonStatusAttackS4Hi] = "AttackS4Hi",
    [nFTCommonStatusAttackS4HiS] = "AttackS4HiS",
    [nFTCommonStatusAttackS4] = "AttackS4",
    [nFTCommonStatusAttackS4LwS] = "AttackS4LwS",
    [nFTCommonStatusAttackS4Lw] = "AttackS4Lw",
    [nFTCommonStatusAttackHi4] = "AttackHi4",
    [nFTCommonStatusAttackLw4] = "AttackLw4",
    [nFTCommonStatusAttackAirN] = "AttackAirN",
    [nFTCommonStatusAttackAirF] = "AttackAirF",
    [nFTCommonStatusAttackAirB] = "AttackAirB",
    [nFTCommonStatusAttackAirHi] = "AttackAirHi",
    [nFTCommonStatusAttackAirLw] = "AttackAirLw",
    [nFTCommonStatusLandingAirN] = "LandingAirN",
    [nFTCommonStatusLandingAirF] = "LandingAirF",
    [nFTCommonStatusLandingAirB] = "LandingAirB",
    [nFTCommonStatusLandingAirHi] = "LandingAirHi",
    [nFTCommonStatusLandingAirLw] = "LandingAirLw",
    [nFTCommonStatusLandingAirNull] = "LandingAirNull",
};

const char *ftMainStatusName(int status_id)
{
    /* Mario's rows; the name table is a debugging aid and only the
 * serial log reads it, so it does not grow a per-kind dimension */
    if (status_id == nFTMarioStatusAppearR)
        return "AppearR";
    if (status_id == nFTMarioStatusAppearL)
        return "AppearL";
    if (status_id == nFTMarioStatusAttack13)
        return "Attack13";
    if (status_id == nFTMarioStatusSpecialLw)
        return "SpecialLw";
    if (status_id == nFTMarioStatusSpecialAirLw)
        return "SpecialAirLw";
    if (status_id < 0 || status_id >= (int)ARRAY_COUNT(kStatusNames) ||
        kStatusNames[status_id] == NULL)
        return "?";
    return kStatusNames[status_id];
}

/* ---- ft/ftparam.c ---------------------------------------------------- */

/* ft/ftparam.c:158-183 ftParamInitPlayerBattleStats 0x800E7EE0, verbatim */
void ftParamInitPlayerBattleStats(s32 player, GObj *fighter_gobj)
{
    s32 i;

    gSCManagerBattleState->players[player].place = 0;
    gSCManagerBattleState->players[player].falls = gSCManagerBattleState->players[player].score = 0;

    for (i = 0; i < ARRAY_COUNT(gSCManagerBattleState->players); i++)
    {
        gSCManagerBattleState->players[player].total_kos_players[i] = 0;
        gSCManagerBattleState->players[player].total_damage_players[i] = 0;
    }
    gSCManagerBattleState->players[player].unk_pblock_0x28 = gSCManagerBattleState->players[player].unk_pblock_0x2C = gSCManagerBattleState->players[player].total_selfdestructs = 0;
    gSCManagerBattleState->players[player].total_damage_given = gSCManagerBattleState->players[player].total_damage_all = 0;
    gSCManagerBattleState->players[player].combo_damage_foe = gSCManagerBattleState->players[player].combo_count_foe = 0;

    gSCManagerBattleState->players[player].fighter_gobj = fighter_gobj;

    gSCManagerBattleState->players[player].stale_id = 0;

    for (i = 0; i < ARRAY_COUNT(gSCManagerBattleState->players[i].stale_info); i++)
    {
        gSCManagerBattleState->players[player].stale_info[i].attack_id = 0;
        gSCManagerBattleState->players[player].stale_info[i].motion_count = 0;
    }
}

/* ft/ftparam.c:204-215 ftParamLockPlayerControl 0x800E7F80. DIVERGES:
 * the cp (computer input) words are not carried. */
void ftParamLockPlayerControl(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    fp->input.pl.button_hold = fp->input.pl.button_tap = 0;

    fp->input.pl.stick_range.x = fp->input.pl.stick_range.y = fp->input.pl.stick_prev.x = fp->input.pl.stick_prev.y = 0;

    fp->tap_stick_x = fp->tap_stick_y = fp->hold_stick_x = fp->hold_stick_y = FTINPUT_STICKBUFFER_TICS_MAX;

    fp->is_control_disable = TRUE;
}

/* ft/ftparam.c:218-223 ftParamUnlockPlayerControl 0x800E7FD4, verbatim */
void ftParamUnlockPlayerControl(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    fp->is_control_disable = FALSE;
}

/* ft/ftkey.c:11-50 ftKeyProcessKeyEvents 0x80115B10 -- the scripted-input
 * interpreter: a Key/GameKey fighter's fp->key.script is a canned
 * FTKeyEvent list (button/stick/end opcodes, ftdef.h's FTKEY_EVENT_*
 * macros), and this walks it one instruction per elapsed input_wait tic,
 * writing the result into fp->input.cp exactly as a controller or
 * ftComputerProcessAll would -- ftMainProcUpdateInterrupt below folds
 * the same button/stick merge over it either way. ftkey.c is a
 * one-function decomp file with a single caller, ftMainProcUpdateInterrupt
 * (ft/ftmain.c) -- which the port already folds into this file rather
 * than a separate ftmain.c -- so ftKeyProcessKeyEvents is folded in
 * beside it here instead of getting its own ftkey.c/.h pair.
 *
 * DIVERGES: the decomp reads key->script->command.opcode/.param (a
 * u16 opcode:4, param:12 bitfield, ftdef.h's FTKeyEvent) and, for a
 * stick event, key->script[1] through a Vec2b reinterpret-cast
 * (ftKeyGetStickRange). Both read the halfword the way a big-endian
 * compiler lays the first-declared field at the MOST significant end
 * -- true on the N64's target, not on sh-elf (or the x86 this host
 * test runs on): GCC puts the first-declared bitfield member, and the
 * first byte of a small struct read off a scalar, at the LEAST
 * significant end on a little-endian target. Read that way,
 * command.opcode comes back as the low nibble of the 12-bit param
 * FTKEY_EVENT_INSTRUCTION packed into bits 0-11, not the opcode packed
 * into bits 12-15, and a stick event's x/y come back swapped. This
 * step's own host test caught it on the primitive's first real script
 * (dSCExplainKeyEvent0, src/dc/scexplain.c,): event 0 is
 * FTKEY_EVENT_STICK(0, 0, 0), command word 0x2000, whose low nibble is
 * 0 (nFTKeyEventEnd) instead of the 2 (nFTKeyEventStick) the high
 * nibble actually holds -- every Key/GameKey fighter's very first
 * script instruction ended the whole script on frame one, on the
 * target exactly as on the host, since sh-elf and x86 are both
 * little-endian. The fix below reads the same two halfwords with
 * explicit shifts instead, matching FTKEY_EVENT_INSTRUCTION/STICK's
 * own packing (ftdef.h) rather than either host's struct-layout rules
 * -- the same opcode/param/x/y on every endianness. ftKeyGetButtons is
 * untouched: a plain 16-bit read of a 16-bit write has no byte-order
 * question to begin with. */
void ftKeyProcessKeyEvents(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    FTComputerInput *cp = &fp->input.cp;
    FTKey *key = &fp->key;
    u16 word;
    s32 opcode;

    if (key->script != NULL)
    {
        if (key->input_wait != 0)
        {
            key->input_wait--;
        }
        while (TRUE)
        {
            if ((key->script == NULL) || (key->input_wait > 0))
            {
                break;
            }
            word = key->script->halfword;
            key->input_wait = word & 0xFFF;
            opcode = (word >> 12) & 0xF;

            switch (opcode)
            {
            case nFTKeyEventEnd:
                key->script = NULL;
                break;

            case nFTKeyEventButton:
                key->script++;

                cp->button_inputs = ftKeyGetButtons(key->script);

                key->script++;
                break;

            case nFTKeyEventStick:
                key->script++;

                word = key->script->halfword;
                cp->stick_range.x = (s8)((word >> 8) & 0xFF);
                cp->stick_range.y = (s8)(word & 0xFF);

                key->script++;
                break;
            }
        }
    }
}

/* ---- ft/ftmain.c: the fighter's GObjProcesses ------------------------ */

/* ft/ftmain.c:1214-1572 ftMainProcUpdateInterrupt 0x800E0940, the
 * fighter's first process of the frame. The input half is the game's
 * text for nFTPlayerKindMan; the buffers and the status calls are
 * verbatim. Com (line 1265,) and Key/GameKey (lines
 * 1271-1273,) are both ported now -- ftComputerProcessAll
 * and ftKeyProcessKeyEvents each write input.cp, and the merge below
 * (lines 1272-1298, shared with Man) reads it into input.pl the same
 * way, so every status/interrupt function downstream sees a CPU or
 * key-scripted fighter's decision exactly as it would a human's.
 * The jostle loop (lines 1512-1569), which pushes two grounded fighters
 * on the same floor line apart, is the game's since: its
 * jostle_x and jostle_width come from the pack (ftmanager.c). */
void ftMainProcUpdateInterrupt(GObj *fighter_gobj)
{
    FTStruct *this_fp = ftGetStruct(fighter_gobj);
    FTStruct *other_fp;
    FTAttributes *this_attr;
    FTAttributes *other_attr;
    FTPlayerInput *pl;
    FTComputerInput *cp;
    SYController *controller;
    GObj *other_gobj;
    f32 jostle_dist_x;
    f32 dist_z;
    sb32 is_check_self;
    sb32 is_jostle;
    u16 button_tap_mask;
    u16 button_hold;
    u16 button_hold_com;
    f32 this_jostle;

    if (!(this_fp->is_control_disable))
    {
        this_fp->status_total_tics++;
    }
    if (!(this_fp->is_control_disable))
    {
        pl = &this_fp->input.pl;

        pl->stick_prev.x = pl->stick_range.x;
        pl->stick_prev.y = pl->stick_range.y;

        switch (this_fp->pkind)
        {
        case nFTPlayerKindMan:
            controller = this_fp->input.controller;
            button_hold = controller->button_hold;

            if (button_hold & R_TRIG)
            {
                button_hold |= (A_BUTTON | Z_TRIG);
            }
            pl->stick_range.x = controller->stick_range.x;
            pl->stick_range.y = controller->stick_range.y;

            button_tap_mask = (button_hold ^ pl->button_hold) & button_hold;

            pl->button_tap = (this_fp->hitlag_tics != 0) ? pl->button_tap | button_tap_mask : button_tap_mask;

            button_tap_mask = (button_hold ^ pl->button_hold) & pl->button_hold;

            pl->button_release = (this_fp->hitlag_tics != 0) ? pl->button_release | button_tap_mask : button_tap_mask;

            pl->button_hold = button_hold;
            break;

        /* ft/ftmain.c:1265-1270 -- ftComputerProcessAll writes
 * input.cp, then falls into the same merge Man's controller
 * read shares below (ftmain.c:1272-1298, "next:" in the
 * decomp). Key/GameKey still fall to default. */
        case nFTPlayerKindCom:
            ftComputerProcessAll(fighter_gobj);

            cp = &this_fp->input.cp;
            button_hold_com = this_fp->input.cp.button_inputs;

            if (button_hold_com & R_TRIG)
            {
                button_hold_com |= (A_BUTTON | Z_TRIG);
            }
            pl->stick_range.x = cp->stick_range.x;
            pl->stick_range.y = cp->stick_range.y;

            button_tap_mask = (button_hold_com ^ pl->button_hold) & button_hold_com;

            pl->button_tap = (this_fp->hitlag_tics != 0) ? pl->button_tap | button_tap_mask : button_tap_mask;

            button_tap_mask = (button_hold_com ^ pl->button_hold) & pl->button_hold;

            pl->button_release = (this_fp->hitlag_tics != 0) ? pl->button_release | button_tap_mask : button_tap_mask;

            pl->button_hold = button_hold_com;
            break;

        /* ft/ftmain.c:1271-1298 -- ftKeyProcessKeyEvents writes
 * input.cp from the fighter's canned script, then falls into
 * the same merge Com/Man share below ("next:" in the decomp). */
        case nFTPlayerKindKey:
        case nFTPlayerKindGameKey:
            ftKeyProcessKeyEvents(fighter_gobj);

            cp = &this_fp->input.cp;
            button_hold_com = this_fp->input.cp.button_inputs;

            if (button_hold_com & R_TRIG)
            {
                button_hold_com |= (A_BUTTON | Z_TRIG);
            }
            pl->stick_range.x = cp->stick_range.x;
            pl->stick_range.y = cp->stick_range.y;

            button_tap_mask = (button_hold_com ^ pl->button_hold) & button_hold_com;

            pl->button_tap = (this_fp->hitlag_tics != 0) ? pl->button_tap | button_tap_mask : button_tap_mask;

            button_tap_mask = (button_hold_com ^ pl->button_hold) & pl->button_hold;

            pl->button_release = (this_fp->hitlag_tics != 0) ? pl->button_release | button_tap_mask : button_tap_mask;

            pl->button_hold = button_hold_com;
            break;

        default:
            break;
        }
        if (pl->stick_range.x > I_CONTROLLER_RANGE_MAX)
        {
            pl->stick_range.x = I_CONTROLLER_RANGE_MAX;
        }
        if (pl->stick_range.x < -I_CONTROLLER_RANGE_MAX)
        {
            pl->stick_range.x = -I_CONTROLLER_RANGE_MAX;
        }
        if (pl->stick_range.y > I_CONTROLLER_RANGE_MAX)
        {
            pl->stick_range.y = I_CONTROLLER_RANGE_MAX;
        }
        if (pl->stick_range.y < -I_CONTROLLER_RANGE_MAX)
        {
            pl->stick_range.y = -I_CONTROLLER_RANGE_MAX;
        }
        if (gSCManagerBackupData.error_flags & LBBACKUP_ERROR_HALFSTICKRANGE)
        {
            pl->stick_range.x *= 0.5F;
            pl->stick_range.y *= 0.5F;
        }
        if (pl->stick_range.x >= 20)
        {
            if (pl->stick_prev.x >= 20)
            {
                this_fp->tap_stick_x++, this_fp->hold_stick_x++;
            }
            else this_fp->tap_stick_x = this_fp->hold_stick_x = 1;
        }
        else if (pl->stick_range.x <= -20)
        {
            if (pl->stick_prev.x <= -20)
            {
                this_fp->tap_stick_x++, this_fp->hold_stick_x++;
            }
            else this_fp->tap_stick_x = this_fp->hold_stick_x = 1;
        }
        else this_fp->tap_stick_x = this_fp->hold_stick_x = FTINPUT_STICKBUFFER_TICS_MAX;

        if (this_fp->tap_stick_x > FTINPUT_STICKBUFFER_TICS_MAX)
        {
            this_fp->tap_stick_x = FTINPUT_STICKBUFFER_TICS_MAX;
        }
        if (this_fp->hold_stick_x > FTINPUT_STICKBUFFER_TICS_MAX)
        {
            this_fp->hold_stick_x = FTINPUT_STICKBUFFER_TICS_MAX;
        }
        if (pl->stick_range.y >= 20)
        {
            if (pl->stick_prev.y >= 20)
            {
                this_fp->tap_stick_y++, this_fp->hold_stick_y++;
            }
            else this_fp->tap_stick_y = this_fp->hold_stick_y = 1;
        }
        else if (pl->stick_range.y <= -20)
        {
            if (pl->stick_prev.y <= -20)
            {
                this_fp->tap_stick_y++, this_fp->hold_stick_y++;
            }
            else this_fp->tap_stick_y = this_fp->hold_stick_y = 1;
        }
        else this_fp->tap_stick_y = this_fp->hold_stick_y = FTINPUT_STICKBUFFER_TICS_MAX;

        if (this_fp->tap_stick_y > FTINPUT_STICKBUFFER_TICS_MAX)
        {
            this_fp->tap_stick_y = FTINPUT_STICKBUFFER_TICS_MAX;
        }
        if (this_fp->hold_stick_y > FTINPUT_STICKBUFFER_TICS_MAX)
        {
            this_fp->hold_stick_y = FTINPUT_STICKBUFFER_TICS_MAX;
        }
    }
    if (this_fp->tics_since_last_z < FTINPUT_ZTRIGLAST_TICS_MAX)
    {
        this_fp->tics_since_last_z++;
    }
    if (this_fp->input.pl.button_tap & this_fp->input.button_mask_z)
    {
        this_fp->tics_since_last_z = 0;
    }
    /* ft/ftmain.c:1382-1508 */
    if (this_fp->hitlag_tics != 0)
    {
        this_fp->hitlag_tics--;

        if (this_fp->hitlag_tics == 0)
        {
            this_fp->is_knockback_paused = FALSE;

            if (this_fp->proc_lagend != NULL)
            {
                this_fp->proc_lagend(fighter_gobj);
            }
        }
    }
    this_fp->is_events_forward = TRUE;

    if (this_fp->hitlag_tics == 0)
    {
        ftMainPlayAnimEventsAll(fighter_gobj);
    }
    ftMainRunUpdateColAnim(fighter_gobj);

    if (this_fp->intangible_tics != 0)
    {
        this_fp->intangible_tics--;

        if (this_fp->intangible_tics == 0)
        {
            this_fp->special_hitstatus = (this_fp->invincible_tics != FALSE) ? nGMHitStatusInvincible : nGMHitStatusNormal;

            if (this_fp->colanim.colanim_id == 0xA)
            {
                ftParamResetStatUpdateColAnim(fighter_gobj);
            }
        }
    }
    if (this_fp->invincible_tics != 0)
    {
        this_fp->invincible_tics--;

        if ((this_fp->invincible_tics == 0) && (this_fp->intangible_tics == 0))
        {
            this_fp->special_hitstatus = nGMHitStatusNormal;

            if (this_fp->colanim.colanim_id == 0xA)
            {
                ftParamResetStatUpdateColAnim(fighter_gobj);
            }
        }
    }
    if (this_fp->star_invincible_tics != 0)
    {
        this_fp->star_invincible_tics--;

        if (this_fp->star_invincible_tics == 0)
        {
            this_fp->star_hitstatus = nGMHitStatusNormal;

            if (this_fp->colanim.colanim_id == 0x4A)
            {
                ftParamResetStatUpdateColAnim(fighter_gobj);
            }
        }
        else if (this_fp->star_invincible_tics == ITSTAR_WARN_BEGIN_FRAME)
        {
            ftParamTryUpdateItemMusic();
        }
    }
    if (this_fp->damage_heal != 0)
    {
        this_fp->damage_heal--;

        if (this_fp->percent_damage != 0)
        {
            this_fp->percent_damage--;

            func_800269C0_275C0(nSYAudioFGMPlayerHeal);

            gSCManagerBattleState->players[this_fp->player].stock_damage_all = this_fp->percent_damage;
        }
        if (this_fp->percent_damage == 0)
        {
            this_fp->damage_heal = 0;
        }
        if ((this_fp->damage_heal == 0) && (this_fp->colanim.colanim_id == 0x9))
        {
            ftParamResetStatUpdateColAnim(fighter_gobj);
        }
    }
    if ((this_fp->item_gobj != NULL) && (this_fp->status_id != nFTCommonStatusLightGet) && (itGetStruct(this_fp->item_gobj)->kind == nITKindHammer))
    {
        ftHammerUpdateStats(fighter_gobj);
    }
    if (this_fp->shuffle_tics != 0)
    {
        this_fp->shuffle_tics--;

        this_fp->shuffle_frame_index++;

        if (this_fp->shuffle_frame_index == this_fp->shuffle_index_max)
        {
            this_fp->shuffle_frame_index = 0;
        }
    }
    if (this_fp->proc_passive != NULL)
    {
        this_fp->proc_passive(fighter_gobj);
    }
    if (this_fp->hitlag_tics == 0)
    {
        if ((this_fp->playertag_wait > 1) && !(this_fp->is_control_disable))
        {
            this_fp->playertag_wait--;
        }
        if (this_fp->proc_update != NULL)
        {
            this_fp->proc_update(fighter_gobj);
        }
        if (this_fp->proc_interrupt != NULL)
        {
            this_fp->proc_interrupt(fighter_gobj);
        }
        if (!(this_fp->is_hitstun))
        {
            gSCManagerBattleState->players[this_fp->player].combo_damage_foe = 0;
            gSCManagerBattleState->players[this_fp->player].combo_count_foe = 0;
        }
        is_jostle = FALSE;

        this_fp->physics.vel_jostle_x = this_fp->physics.vel_jostle_z = 0.0F;

        if ((this_fp->ga == nMPKineticsGround) && !(this_fp->is_jostle_ignore))
        {
            other_gobj = gGCCommonLinks[nGCCommonLinkIDFighter];

            is_check_self = FALSE;

            while (other_gobj != NULL)
            {
                other_fp = ftGetStruct(other_gobj);

                if ((fighter_gobj != other_gobj) && (other_fp->capture_gobj == NULL))
                {
                    if ((other_fp->ga == nMPKineticsGround) && (this_fp->coll_data.floor_line_id == other_fp->coll_data.floor_line_id))
                    {
                        this_attr = this_fp->attr;
                        other_attr = other_fp->attr;

                        this_jostle = this_fp->attr->jostle_width;

                        jostle_dist_x = (DObjGetStruct(fighter_gobj)->translate.vec.f.x + (this_attr->jostle_x * this_fp->lr)) - (DObjGetStruct(other_gobj)->translate.vec.f.x + (other_attr->jostle_x * other_fp->lr));

                        if (ABS(jostle_dist_x) < (this_jostle + other_attr->jostle_width))
                        {
                            is_jostle = TRUE;

                            if (jostle_dist_x == 0.0F)
                            {
                                this_fp->physics.vel_jostle_x += (6.75F * ((is_check_self != FALSE) ? -1 : 1));
                            }
                            else this_fp->physics.vel_jostle_x += (6.75F * ((jostle_dist_x < 0.0F) ? -1 : 1));

                            dist_z = DObjGetStruct(fighter_gobj)->translate.vec.f.z - DObjGetStruct(other_gobj)->translate.vec.f.z;

                            if (dist_z == 0.0F)
                            {
                                if (jostle_dist_x == 0.0F)
                                {
                                    this_fp->physics.vel_jostle_z += (3.0F * ((is_check_self != FALSE) ? -1 : 1));
                                }
                                else this_fp->physics.vel_jostle_z += (3.0F * ((jostle_dist_x < 0.0F) ? 1 : -1));
                            }
                            else this_fp->physics.vel_jostle_z += (3.0F * ((dist_z < 0.0F) ? -1 : 1));
                        }
                    }
                }
                else is_check_self = TRUE;

                other_gobj = other_gobj->link_next;
            }
            if ((is_jostle == FALSE) && (DObjGetStruct(fighter_gobj)->translate.vec.f.z != 0.0F))
            {
                this_fp->physics.vel_jostle_z = ((DObjGetStruct(fighter_gobj)->translate.vec.f.z < 0.0F) ? +1 : -1) * 3.0F;
            }
        }
    }
    this_fp->coll_data.vel_push.x = this_fp->coll_data.vel_push.y = this_fp->coll_data.vel_push.z = 0.0F;
}

/* ft/ftmain.c:1739-1916 ftMainProcPhysicsMap 0x800E2048. The blast-line
 * check (line 1815 ftCommonDeadCheckInterruptCommon) is live.
 * The rest, including Kirby's map star, the slope contour
 * behind proc_slope, the hitlag arms and the hitbox
 * position refresh at the end, is the game's. */
void ftMainProcPhysicsMap(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    Vec3f *topn_translate = &fp->joints[nFTPartsJointTopN]->translate.vec.f;
    Vec3f *coll_translate = &fp->coll_data.pos_prev;
    Vec3f *floor_angle = &fp->coll_data.floor_angle;
    Vec3f *vel_damage_air;
    f32 size_mul;
    f32 damage_angle;
    Vec2f vel_damage_new;
    s32 i;

 *coll_translate = *topn_translate;

    if (fp->hitlag_tics == 0)
    {
        if (fp->cliffcatch_wait != 0)
        {
            fp->cliffcatch_wait--;
        }
        if (fp->proc_physics != NULL)
        {
            fp->proc_physics(fighter_gobj);
        }
        vel_damage_air = &fp->physics.vel_damage_air;

        if ((vel_damage_air->x != 0.0F) || (vel_damage_air->y != 0.0F))
        {
            if (fp->ga == nMPKineticsAir)
            {
                damage_angle = syUtilsArcTan2(vel_damage_air->y, vel_damage_air->x);

                vel_damage_new.y = vel_damage_air->x;
                vel_damage_new.x = vel_damage_air->y;

                vel_damage_air->x -= (1.7F * __cosf(damage_angle));
                vel_damage_air->y -= (1.7F * __sinf(damage_angle));

                if (((vel_damage_air->x * vel_damage_new.y) < 0.0F) || ((vel_damage_air->y * vel_damage_new.x) < 0.0F))
                {
                    vel_damage_air->x = vel_damage_air->y = 0.0F;
                }
                fp->physics.vel_damage_ground = 0.0F;
            }
            else
            {
                if (fp->physics.vel_damage_ground == 0.0F)
                {
                    fp->physics.vel_damage_ground = fp->physics.vel_damage_air.x;
                }
                ftMainUpdateVelDamageGround(fp, dMPCollisionMaterialFrictions[fp->coll_data.floor_flags & MAP_VERTEX_MAT_MASK] * fp->attr->traction * 0.25F);

                vel_damage_air->x = (floor_angle->y * fp->physics.vel_damage_ground);
                vel_damage_air->y = (-floor_angle->x * fp->physics.vel_damage_ground);
            }
        }
        syVectorAdd3D(topn_translate, &fp->physics.vel_air);

        topn_translate->x += vel_damage_air->x;
        topn_translate->y += vel_damage_air->y;
    }
    if (fp->proc_lagupdate != NULL)
    {
        fp->proc_lagupdate(fighter_gobj);
    }
    syVectorDiff3D(&fp->coll_data.pos_diff, topn_translate, coll_translate);

    if ((fp->ga == nMPKineticsGround) && (fp->coll_data.floor_line_id != -1) && (fp->coll_data.floor_line_id != -2) && (mpCollisionCheckExistLineID(fp->coll_data.floor_line_id) != FALSE))
    {
        mpCollisionGetSpeedLineID(fp->coll_data.floor_line_id, &fp->coll_data.vel_speed);
        syVectorAdd3D(topn_translate, &fp->coll_data.vel_speed);
    }
    else fp->coll_data.vel_speed.x = fp->coll_data.vel_speed.y = fp->coll_data.vel_speed.z = 0.0F;

    ftCommonDeadCheckInterruptCommon(fighter_gobj);

    if ((fp->coll_data.pos_prev.y >= gMPCollisionGroundData->alt_warning) && (topn_translate->y < gMPCollisionGroundData->alt_warning) && (fp->fkind != nFTKindBoss))
    {
        func_800269C0_275C0(nSYAudioFGMAltitudeWarn);
    }
    if (fp->public_knockback != 0)
    {
        if ((fp->joints[nFTPartsJointTopN]->translate.vec.f.x > (gMPCollisionBounds.current.left + 450.0F)) && (fp->joints[nFTPartsJointTopN]->translate.vec.f.x < (gMPCollisionBounds.current.right - 450.0F)))
        {
            fp->public_knockback = 0.0F;
        }
    }
    if (fp->proc_map != NULL)
    {
        fp->coll_data.mask_prev = fp->coll_data.mask_curr;
        fp->coll_data.mask_curr = 0;
        fp->coll_data.is_coll_end = FALSE;
        fp->coll_data.mask_stat = 0;
        fp->coll_data.mask_unk = 0;

        fp->proc_map(fighter_gobj);

        if (fp->fkind == nFTKindKirby)
        {
            ftParamKirbyTryMakeMapStarEffect(fighter_gobj);
        }
    }
    if (fp->proc_slope != NULL)
    {
        fp->proc_slope(fighter_gobj);
    }
    ftParamsUpdateFighterPartsTransformAll(fp->joints[nFTPartsJointTopN]);

    if (fp->hitlag_tics == 0)
    {
        ftMainUpdateMotionEventsForwardEffect(fighter_gobj);
    }
    if (fp->hitlag_tics == 0)
    {
        if (fp->proc_accessory != NULL)
        {
            fp->proc_accessory(fighter_gobj);
        }
    }
    fp->is_events_forward = FALSE;

    for (i = 0; i < ARRAY_COUNT(fp->attack_colls); i++)
    {
        FTAttackColl *attack_coll = &fp->attack_colls[i];

        switch (attack_coll->attack_state)
        {
        case nGMAttackStateOff:
            break;

        case nGMAttackStateNew:
            attack_coll->pos_curr = attack_coll->offset;

            if (attack_coll->is_scale_pos)
            {
                size_mul = 1.0F / fp->attr->size;

                attack_coll->pos_curr.x *= size_mul;
                attack_coll->pos_curr.y *= size_mul;
                attack_coll->pos_curr.z *= size_mul;
            }
            gmCollisionGetFighterPartsWorldPosition(attack_coll->joint, &attack_coll->pos_curr);

            attack_coll->attack_state = nGMAttackStateTransfer;

            attack_coll->attack_matrix.unk_fthitmtx_0x0 = FALSE;
            attack_coll->attack_matrix.unk_fthitmtx_0x44 = 0.0F;
            break;

        case nGMAttackStateTransfer:
            attack_coll->attack_state = nGMAttackStateInterpolate;

            /* fallthrough */

        case nGMAttackStateInterpolate:
            attack_coll->pos_prev = attack_coll->pos_curr;
            attack_coll->pos_curr = attack_coll->offset;

            if (attack_coll->is_scale_pos)
            {
                size_mul = 1.0F / fp->attr->size;

                attack_coll->pos_curr.x *= size_mul;
                attack_coll->pos_curr.y *= size_mul;
                attack_coll->pos_curr.z *= size_mul;
            }
            gmCollisionGetFighterPartsWorldPosition(attack_coll->joint, &attack_coll->pos_curr);

            attack_coll->attack_matrix.unk_fthitmtx_0x0 = FALSE;
            attack_coll->attack_matrix.unk_fthitmtx_0x44 = 0.0F;

            break;
        }
    }
}

/* ft/ftmain.c:1918-1926 ftMainProcPhysicsMapDefault 0x800E2604: the
 * game runs the physics from this process unless the fighter is one
 * of a grabbed pair's second mover, when ftMainProcPhysicsMapCapture
 * (priority 3) runs it instead, after every first mover. */
void ftMainProcPhysicsMapDefault(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if ((fp->capture_gobj == NULL || fp->is_catch_or_capture) && (fp->catch_gobj == NULL || !(fp->is_catch_or_capture)))
    {
        ftMainProcPhysicsMap(fighter_gobj);
    }
}

#ifdef FT_HOSTTEST
/* Test hooks: the physics helpers stay static (one translation unit,
 * as in the decomp); the host cross-test reaches them through these. */
void ftTestSetGroundVelFriction(FTStruct *fp, float friction)
{ ftPhysicsSetGroundVelFriction(fp, friction); }
void ftTestSetGroundVelAbsStickRange(FTStruct *fp, float vel, float fric)
{ ftPhysicsSetGroundVelAbsStickRange(fp, vel, fric); }
void ftTestApplyGravityClampTVel(FTStruct *fp, float gravity, float tvel)
{ ftPhysicsApplyGravityClampTVel(fp, gravity, tvel); }
int ftTestCheckClampAirVelXDec(FTStruct *fp, float clamp)
{ return ftPhysicsCheckClampAirVelXDec(fp, clamp); }
void ftTestClampAirVelXStickRange(FTStruct *fp, int m, float v, float c)
{ ftPhysicsClampAirVelXStickRange(fp, m, v, c); }
void ftTestApplyAirVelXFriction(FTStruct *fp)
{ ftPhysicsApplyAirVelXFriction(fp, fp->attr); }
void ftTestJumpGetJumpForceButton(int sx, s32 *vx, s32 *vy, int hop)
{ ftCommonJumpGetJumpForceButton(sx, vx, vy, hop); }
#endif

/* The bzero arm of syDmaLoadOverlay for overlay 3, which is where the
 * ft/ftcommon/ statuses this file stands in for live (smashbrothers.us.
 * yaml; scManagerRunScene loads 2, 3 and 4 for the battle). Empty, like
 * ft/ftparam's and for the same reason: the cascade's state is all in the
 * FTStruct. The entry is what makes that a checked fact instead of a
 * true-for-now one -- see tools/check/overlay_check.py. */
void ftCommonOverlayLoad(void)
{
    /* the throw's damage-script scratch: a module static in
 * the noload segment, so the overlay reload must clear it the way the
 * N64's bzero of the overlay .bss would. */
    OVERLAY_CLEAR(sFTCommonThrownScriptID);
    /* the grab swirl's offset: all zero, so gcc puts it in
 * .bss, and the N64's overlay bzero is what gives it its zeros. */
    OVERLAY_CLEAR(dFTCommonCatchPullEffectOffset);
    /* Falcon Dive's grab-offset file base: cleared as the
 * N64's bzero would, then bound again to the table above -- the game
 * refills it from the ROM at every fighter load, the port from its
 * own copy. */
    OVERLAY_CLEAR(gFTDataCaptainMainMotion);
    OVERLAY_CLEAR(llCaptainMainMotionSpecialHiVec2h);
    /* and the Reflector's hurt sphere, the same way; its
 * base is ftfox.c's, already zeroed by overlay 2's reload */
    OVERLAY_CLEAR(llFoxMainMotionLwReflectorFTSpecialColl);
    ftCommonBindMainMotionFiles();
}

/* The fighter files the code reads directly, bound to the port's copies:
 * Captain's, Fox's, Kirby's and Ness's motion files, and every fighter
 * weapon's hitbox table (src/dc/wpattrs.c). The file pointers are
 * ft<char>.c's own, in overlay 2, so every scene that loads it zeroes
 * them. The game refills them at every fighter load
 * (ft/ftmanager.c:304 ftManagerSetupFilesKind); the port does it here,
 * from overlay 3's reload and from ftManagerSetupFilesAllKind, because
 * the character selects, the results screen and Characters load
 * overlay 2 and not 3. */
void ftCommonBindMainMotionFiles(void)
{
    ftCommonCaptureCaptainBindOffsets();
    ftFoxReflectorBindOffsets();
    /* Kirby's copy and rapid-jab tables: ftManagerMakeFighter reads the
 * copy table for every Kirby it makes, menus' too */
    ftKirbyMainMotionBindOffsets();
    ftNessMainMotionBindOffsets();
    wpAttrsBind();
}

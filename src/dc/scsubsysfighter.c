/* scsubsysfighter.c -- sc/scsubsys/scsubsysfighter.c, the fighter side
 * of the scene subsystem: the light the menus' fighters are lit by, the
 * one process a menu's fighter runs, and the status setter the menus
 * use. With it the per-kind scale table out of scsubsysdata.c. Every
 * function is the decomp's by name and body unless marked DIVERGES.
 *
 * scSubsysFighterOpeningProcUpdate is ported below, with
 * mvOpeningRoomMakeBoss: it holds the plucked trophy against the opening
 * room's Master Hand. Nothing of D_ovl1_80390BE8 is cut. */
#include "ftcommon.h"
#include "overlay.h"

#include <sc/scsubsys/scsubsys.h>
#include <sys/debug.h>

// // // // // // // // // // // //
//                               //
//   GLOBAL / STATIC VARIABLES   //
//                               //
// // // // // // // // // // // //

// 0x803929D8
static f32 sSCSubsysFighterLightAngleX;

// 0x803929DC
static f32 sSCSubsysFighterLightAngleY;

// 0x803929E0
static SYColorRGBA sSCSubsysLightsColor;

/* dSCSubsysFighterScales is in src/dc/scsubsysdata.c, where the decomp
 * keeps it. */

// // // // // // // // // // // //
//                               //
//           FUNCTIONS           //
//                               //
// // // // // // // // // // // //

// 0x803904E0
void scSubsysFighterSetLightParams(f32 light_angle_x, f32 light_angle_y, u8 r, u8 g, u8 b, u8 a)
{
    sSCSubsysFighterLightAngleX = light_angle_x;
    sSCSubsysFighterLightAngleY = light_angle_y;

    sSCSubsysLightsColor.r = r;
    sSCSubsysLightsColor.g = g;
    sSCSubsysLightsColor.b = b;
    sSCSubsysLightsColor.a = a;
}

// 0x8039051C
f32 scSubsysFighterGetLightAngleX(void)
{
    return sSCSubsysFighterLightAngleX;
}

// 0x80390528
f32 scSubsysFighterGetLightAngleY(void)
{
    return sSCSubsysFighterLightAngleY;
}

/* scsubsysfighter.c:51-57 0x80390534. DIVERGES: the
 * env colour it writes into `dls` has no display list to go in -- a
 * menu's light is white in every scene that sets it (the character
 * select and the results screen), and ftDisplayMainProcDisplay hands
 * the alpha it returns to the fighter's models itself
 * (Fighter.alpha_cut). `dls` is unused and may be NULL. */
u8 scSubsysFighterDrawLightColorGetAlpha(Gfx **dls)
{
    (void)dls;

    return sSCSubsysLightsColor.a;
}

// 0x80390584
void scSubsysFighterProcUpdate(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    ftMainPlayAnimEventsAll(fighter_gobj);
    ftMainRunUpdateColAnim(fighter_gobj);

    if (fp->proc_update != NULL)
    {
        fp->proc_update(fighter_gobj);
    }
}

/* scsubsysfighter.c:74-77 0x803905CC, verbatim.
 *
 * It stood on a DIVERGES for the port's whole life: every demo status
 * was turned into nFTCommonStatusWait here, so a fighter stood in its
 * idle animation wherever the game would have posed it. The stated
 * reason was that the pose "is in the fighter's SubMotion file, which
 * the pack does not carry", and that was wrong
 * twice over -- the poses are ordinary FT<Name>Anim* reloc files in the
 * packs. What it needed was the two tables: dFT<Kind>SubMotionDescs (in
 * every pack) and scsubsysdata.c's FTOpeningDescs (beside this file). */
void scSubsysFighterSetStatus(GObj *fighter_gobj, s32 status_id)
{
    ftMainSetStatus(fighter_gobj, status_id, FTSTATUS_PRESERVE_NONE, 1.0F, 0.0F);
}

/* scsubsysfighter.c:81-95 0x803905F4, the REGION_US arm, verbatim.
 * The proc row 8 of D_ovl1_80390BE8 carries, through
 * mvOpeningFighterProcUpdate: it holds the plucked trophy against the
 * opening room's Master Hand, once a frame, by taking his item-heavy
 * joint's matrix, reading its Euler angles into the trophy's rotate,
 * and carrying the negated child-joint offset through that matrix to a
 * world position. `this_gobj` is the hand and `other_gobj` the trophy.
 *
 * ftCommonCapturePulledRotateScale (src/dc/ftcommon.c) is the same
 * three calls in the same order for a caught fighter, and has been in
 * the port -- the one difference is that this one does not
 * scale the offset, because the trophy is already at scale 1.
 *
 * The JP arm is the one this port does not take: it reads the joint's
 * FTParts->mtx_translate through func_ovl2_800EDBA4 instead of building
 * a matrix, and the US build is what the port is. */
void scSubsysFighterOpeningProcUpdate(GObj *this_gobj, GObj *other_gobj)
{
    FTStruct *fp = ftGetStruct(this_gobj);
    DObj *child_dobj = DObjGetStruct(other_gobj)->child;
    Mtx44f mtx_f;

    func_ovl0_800C9A38(mtx_f, fp->joints[fp->attr->joint_itemheavy_id]);
    func_ovl2_800EDA0C(mtx_f, &DObjGetStruct(other_gobj)->rotate.vec.f);

    DObjGetStruct(other_gobj)->translate.vec.f.x = -child_dobj->translate.vec.f.x;
    DObjGetStruct(other_gobj)->translate.vec.f.y = -child_dobj->translate.vec.f.y;
    DObjGetStruct(other_gobj)->translate.vec.f.z = -child_dobj->translate.vec.f.z;

    gmCollisionGetWorldPosition(mtx_f, &DObjGetStruct(other_gobj)->translate.vec.f);
}

/* scsubsysfighter.c:118-125 0x8039069C, verbatim. The
 * proc rows 9 and 10 of D_ovl1_80390BE8 carry -- the figure drop and
 * the stand -- which folds what the animation moved TransN by into
 * TopN, so a demo fighter whose pose translates ends up where the
 * animation put it rather than where it was made. Nothing reached it
 * until scsubsysdata.c existed. */
void scSubsysFighterApplyVelTransN(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    DObj *transn_joint = fp->joints[nFTPartsJointTransN], *topn_joint = fp->joints[nFTPartsJointTopN];

    topn_joint->translate.vec.f.x += (transn_joint->translate.vec.f.x - fp->anim_vel.x);
    topn_joint->translate.vec.f.y += (transn_joint->translate.vec.f.y - fp->anim_vel.y);
    topn_joint->translate.vec.f.z += (transn_joint->translate.vec.f.z - fp->anim_vel.z);
}

/* The bzero arm of syDmaLoadOverlay for overlay 1, of which this
 * file is a part: sc/scmanager.c calls it on the way into every
 * scene that loads that overlay. src/dc/overlay.h says why the port
 * needs it written out. */
void scSubsysFighterOverlayLoad(void)
{
    OVERLAY_CLEAR(sSCSubsysFighterLightAngleX);
    OVERLAY_CLEAR(sSCSubsysFighterLightAngleY);
    OVERLAY_CLEAR(sSCSubsysLightsColor);
}

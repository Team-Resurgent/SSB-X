/* grsector.c -- gr/grcommon/grsector.c, the last of the nine stage-logic
 * files: Sector Z's Arwing.
 *
 * THE STAGE'S HAZARD IS ONE AIRCRAFT. An Arwing crosses the Great Fox's
 * deck every so often -- a flight pattern picked at random from eight --
 * flown by one of five pilots (whose animation is picked at random too,
 * and never the one the last Arwing had). While it is out it is a moving
 * WALL: `grSectorArwingUpdateCollisions` turns map-object collision 1 on
 * and drags it along the Arwing's own line, so a fighter standing in the
 * flight path is run into. It also shoots: two fixed lasers or one aimed
 * 3D bolt, depending on the pattern.
 *
 * The state machine is three statuses -- Sleep, Wait, Patrol -- over
 * `gGRCommonStruct.sector`, and its clock is the ARWING'S OWN ANIMATION:
 * `map_dobjs[0]->anim_wait` going to AOBJ_ANIM_NULL is what ends the
 * patrol, which is why every script in the pack matters -- a script whose
 * length is wrong is a patrol that lasts the wrong amount of time, and a
 * script that is missing is a patrol that never ends.
 *
 * DIVERGES, and the first is the one that shapes the file:
 *
 *  1. THE ARWING'S TREE COMES FROM THE EFFECT PACK, NOT FROM
 *     lbRelocGetForceStatusBufferFile. The game reaches into FOX's
 *     Special3 -- `map_file = lbRelocGetForceStatusBufferFile(
 *     &llFoxSpecial3FileID)` -- for both the tree and four of the
 *     scripts, and builds the tree with `grModelSetupGroundDObjs`, which
 *     fills `map_dobjs[12]` from a DObjDesc. This port has baked that
 *     same file as `romdisk/efarwing.mdl` (Fox's own
 *     Arwing, which his Special3 summons), and `dc_model_add_dobjs` takes
 *     the joint array as its fourth argument -- so the whole of that
 *     setup is one call, and the FOURTH divergence falls out of it: the
 *     per-joint transform kinds `dGRSectorArwingTransformKinds` names are
 *     the port's standing trade (the bake installs the RpyR equivalent on
 *     every joint; see src/dc/grinishie.c's second divergence), EXCEPT two
 *     things `grSectorArwingSetKinds` fixes up after the bake: the 0x2C
 *     XObjs the bake does not install and the Arwing's engine glow needs,
 *     and joint 0's own transform (matrix kind 0x53, `objdisplay.c`'s
 *     `case 0x53:`), which the bake's default triple cannot stand in for
 *     at all -- see that function's header for why.
 *
 *  2. EVERY SCRIPT COMES FROM THE PACK BY NAME. The game adds
 *     `&llGRSectorMap<X> + map_head` (or `&llFoxSpecial3_<X> + map_file`)
 *     to a file base this port has no pointer for. `stage_map_anim` names
 *     all 39 of them: the 30 flight-pattern scripts, the 5 pilot
 *     animations, the 2 out of Fox's file, and the 2 more the decomp
 *     MISCATALOGUES as Fox's (see divergence 3). See
 *     tools/export/ssb_stageexport.py's Sector entry for where each came from.
 *
 *  3. TWO OF THE FOUR SCRIPTS NAMED FOR FOX'S FILE ARE THIS STAGE'S. The
 *     decomp adds `llFoxSpecial3_1B84_AnimJoint` and `_1B34_` to
 *     `map_head` -- this stage's own file -- and not to `map_file`, which
 *     is Fox's. The symbol names are the decomp's and they are wrong:
 *     file 161 at 0x1B34 is a display list, while file 153's 0x1B34 is
 *     `dStageSectorFile3_Sub_0x1B34`, a SCAXYZ scale script. They are the
 *     two that make the Arwing's weapon parts appear -- the tree gives
 *     joints 2, 3, 4 and 5 a scale of 1e-5, and these scripts scale them
 *     up: `_1B84_` on joints 4 and 5 when a volley opens, `_1B34_` on
 *     joints 2 and 3 when the shot is fired. The host test pins both by
 *     name, and the export comment records the byte that settles which
 *     file they are in.
 *
 *  4. THE WEAPONS' ATTRIBUTES COME FROM THE ITEM PACK. Both WPDescs'
 *     `p_weapon` is `&gGRCommonStruct.sector.weapon_head` -- a second
 *     stage head beside `item_head`, the same arithmetic off
 *     gMPCollisionGroundData -- which this port has no file for, so it is
 *     `&gITManagerCommonData` and their `WPAttributes` ride in the pack's
 *     WEAPON ATTRIBUTES section under 0xBC and 0xF0. Same shape as the
 *     Gate's weapons (src/dc/itfushigibana.c).
 *
 *  5. NOTHING ELSE. `syInterpQuad` and `syInterpCubic` -- the two the
 *     path evaluator below calls through `aobj->interpolate` -- are the
 *     decomp's own (sys/interp.h:41, :40), and sys/interp.c is compiled
 *     into this port unmodified, so their three-argument forms are used
 *     as written.
 *
 * The two laser weapons' `data` is file 153's display list at 0x1C50
 * (`Gfx[27]`, "the ArwingLaser2D/3D weapon visual DL" in the file's own
 * header), a bare DL rather than a tree, baked by
 * `tools/export/ssb_effectexport.py --what arwinglaser` into wparwinglaser.mdl
 * (src/dc/wpmanager.c binds both descs to it).
 */
#include <gr/ground.h>
#include <gr/grcommon/grsector.h>  /* the file's own prototypes, so the two
                                     WPDescs can name procs defined below
                                     them */
#include <ft/fighter.h>
#include <wp/weapon.h>
#include <it/item.h>
#include <sc/scene.h>
#include <ef/effect.h>            /* efManagerDustExpandSmallMakeEffect,
                                     efManagerImpactShockMakeEffect,
                                     efManagerSparkleWhiteMultiExplodeMakeEffect */
#include <sys/objanim.h>          /* gcAddDObjAnimJoint, gcParseDObjAnimJoint,
                                     gcPlayDObjAnimJoint, gcPlayAnimAll */
#include <sys/interp.h>           /* syInterpQuad, syInterpCubic */

#include <stdlib.h>               /* malloc, for the Arwing's pack */

#ifdef FT_HOSTTEST
#include "hoststubs.h"
#else
#include <kos.h>
#endif

#include "overlay.h"              /* OVERLAY_CLEAR, for grSectorOverlayLoad */
#include "stage.h"                /* stage_map_anim */
#include "objmodel.h"             /* dc_model_add_dobjs */
#include "itempack.h"             /* itemPackWeaponAttr (via wpmanager) */
#include "ftcommon.h"             /* func_800269C0_275C0 */

/* // // // // // // // // // // // //
 *                                   //
 *          ENUMERATORS              //
 *                                   //
 * // // // // // // // // // // // // */

/* grsector.c:216-221, verbatim: the three statuses `grSectorProcUpdate`
 * switches on. */
enum grSectorArwingStatus
{
    nGRSectorArwingStatusSleep,
    nGRSectorArwingStatusWait,
    nGRSectorArwingStatusPatrol
};

/* // // // // // // // // // // // //
 *                                   //
 *       INITIALIZED DATA            //
 *                                   //
 * // // // // // // // // // // // // */

/* grsector.c:22-47 dGRSectorArwingSectorDescs and
 * dGRSectorArwingAnimJoints, as NAMES.
 *
 * DIVERGES: the decomp keeps two tables of LINKER SYMBOLS --
 * `intptr_t dGRSectorArwingSectorDescs[8]` naming the eight
 * `GRSectorDesc`s and `dGRSectorArwingAnimJoints[6]` naming the pilot
 * scripts -- and adds each to a file base at the point of use. The port's
 * pack lookup is BY NAME, so both tables carry the names the stage entry
 * gives those scripts, and each is resolved where it is used (no cached
 * pointers: a scene re-entry reloads the pack, and a cache would be the
 * one thing this file kept).
 *
 * The four names per pattern are the descriptor's own four fields, in its
 * own order (0x00, 0x1C, 0x24, 0x2C) -- the first of which is the script
 * whose END drives the patrol. `NULL` is a field the descriptor leaves
 * NULL: Arwing1 and Arwing4 have no 0x2C script. */
static const char *sGRSectorArwingPatternAnims[8][4] =
{
    { "Arwing0Path", "Arwing0Alt7", "Arwing0Alt9", "Arwing0Alt11" },
    { "Arwing1Path", "Arwing1Alt7", "Arwing1Alt9", NULL },
    { "Arwing2Path", "Arwing2Alt7", "Arwing2Alt9", "Arwing2Alt11" },
    { "Arwing3Path", "Arwing3Alt7", "Arwing3Alt9", "Arwing3Alt11" },
    { "Arwing4Path", "Arwing4Alt7", "Arwing4Alt9", NULL },
    { "Arwing5Path", "Arwing5Alt7", "Arwing5Alt9", "Arwing5Alt11" },
    { "Arwing6Path", "Arwing6Alt7", "Arwing6Alt9", "Arwing6Alt11" },
    { "Arwing7Path", "Arwing7Alt7", "Arwing7Alt9", "Arwing7Alt11" },
};

/* ... and the pilots. Entry 0 is `&llGRSectorMapArwing0AnimJoint`, which
 * is offset 0x0000 -- the map head ITSELF, never a script, because pilot
 * 0 means "no pilot" and `func_ovl2_80106DD8` only ever indexes this table
 * with a non-zero id (the `if (pilot_id != 0)` guard). See the stage
 * entry for all five. */
static const char *sGRSectorArwingPilotAnims[6] =
{
    NULL,
    "ArwingPilot1",
    "ArwingPilot2",
    "ArwingPilot3",
    "ArwingPilot4",
    "ArwingPilot5"
};

/* The four scripts this file plays that are not one of the 35 the flight
 * patterns and pilots own.
 *
 * The first two are Fox's, and they arrive through `map_file`. `_2E74_` is
 * the script the Arwing's OWN tree points past its DObjDesc sentinel --
 * `dFoxSpecial3_EntryArwing_post[0]` (161_FoxSpecial3.c:566) -- and it is
 * played on map_dobjs[10] once at init; its last word SetAnims back to
 * itself, so it pulses the hull's scale forever. `_2EB4_` is the pilot-5
 * transform on map_dobjs[8].
 *
 * The second two are not Fox's at all despite their names in the
 * decomp -- see this file's header, divergence 3. */
#define GRSECTOR_ANIM_DEFAULT   "FoxSpecial3_2E74"
#define GRSECTOR_ANIM_TRANSFORM "FoxSpecial3_2EB4"
#define GRSECTOR_ANIM_WINGSCALE "ArwingWingScale"
#define GRSECTOR_ANIM_GUNSCALE  "ArwingGunScale"

/* grsector.c:49-55 dGRSectorArwingMapPositionsX, verbatim: the three x
 * positions an Arwing is aimed at when a pattern asks for a targeted
 * flight, in the map's own units. */
s16 dGRSectorArwingMapPositionsX[/* */] =
{
    -3000,
        0,
     9000
};

/* grsector.c:57-66 dGRSectorArwingLaserCounts, verbatim: how many lasers a
 * pattern fires before it stops. 2 is the two fixed wing lasers, 4 is the
 * aimed bolt fired in pairs; 0 is a pattern that never shoots. */
u8 dGRSectorArwingLaserCounts[/* */] =
{
    0x02,
    0x02,
    0x02,
    0x02,
    0x02,
    0x00,
    0x00,
    0x00
};

/* grsector.c:68-126 dGRSectorArwingPilotIDs, verbatim: a flat table of
 * pilot ids the wait-timer pairs index into. */
u8 dGRSectorArwingPilotIDs[/* */] =
{
    0x01,
    0x01,
    0x02,
    0x03,
    0x04,
    0x04,
    0x05,
    0x00,
    0x00,
    0x00,
    0x00,
    0x02,
    0x03,
    0x04,
    0x04,
    0x05,
    0x00,
    0x00,
    0x00,
    0x00,
    0x01,
    0x01,
    0x03,
    0x04,
    0x04,
    0x05,
    0x00,
    0x00,
    0x00,
    0x00,
    0x01,
    0x01,
    0x02,
    0x04,
    0x04,
    0x05,
    0x00,
    0x00,
    0x00,
    0x00,
    0x01,
    0x01,
    0x02,
    0x03,
    0x05,
    0x00,
    0x00,
    0x00,
    0x00,
    0x01,
    0x01,
    0x02,
    0x03,
    0x04,
    0x04,
    0x00
};

/* grsector.c:128-136 dGRSectorArwingPilotWaitTimers, verbatim: per
 * PREVIOUS pilot, the (start, span) pair the next pilot's draw comes from
 * -- so the pilot who just flew is the one whose row is read, and the
 * table's rows are what keep a pilot from following itself. */
u8 dGRSectorArwingPilotWaitTimers[/* */][2] =
{
    {  0,  7 },
    {  7,  9 },
    { 16, 10 },
    { 26, 10 },
    { 36,  9 },
    { 45, 10 }
};

/* grsector.c:138-155 dGRSectorArwingTransformKinds, verbatim, REGION_US
 * arm. See grSectorArwingSetKinds for what the port takes from it and what
 * the bake already did. */
DObjTransformTypes dGRSectorArwingTransformKinds[/* */] =
{
    { 0x53, nGCMatrixKindNull, 0x00 },
    { nGCMatrixKindTraRotRpyR, nGCMatrixKindNull, 0x00 },
    { nGCMatrixKindTra, 0x2C, 0x01 },
    { nGCMatrixKindTra, 0x2C, 0x01 },
    { nGCMatrixKindTra, 0x2C, 0x00 },
    { nGCMatrixKindTra, 0x2C, 0x00 },
    { nGCMatrixKindTra, nGCMatrixKindNull, 0x01 },
    { nGCMatrixKindTra, nGCMatrixKindNull, 0x00 },
    { nGCMatrixKindTra, 0x2C, 0x00 },
    { nGCMatrixKindTra, nGCMatrixKindNull, 0x00 },
    { nGCMatrixKindTra, 0x2C, 0x01 },
    { nGCMatrixKindNull, nGCMatrixKindNull, 0x00 }
};

/* grsector.c:160-201 dGRSectorArwingWeaponLaser2DWeaponDesc and
 * dGRSectorArwingWeaponLaser3DWeaponDesc, verbatim but for `p_weapon` (see
 * this file's header).
 *
 * Note the two are the SAME KIND OF THING and differ only in their own
 * `o_attributes` -- 0xBC and 0xF0 into the same file's weapon-attribute
 * table, both of which the pack carries. Render flags 0 on both, so their
 * model is a bare display list rather than a DObjDesc tree (`WPAttributes
 * .data` names file 153's 0x1C50 -- see this file's header for the bake
 * that has not landed yet), and Proc Hit serves as Shield, Set-Off AND
 * Absorb on both. The 3D one's Proc Map and Proc Hit replace the whole
 * weapon with an EXPLODING one -- see
 * grSectorArwingWeaponLaserExplodeInitVars -- where the 2D one simply
 * dies. */
WPDesc dGRSectorArwingWeaponLaser2DWeaponDesc =
{
    0,                                          /* Render flags? */
    nWPKindArwingLaser2D,                       /* Weapon Kind */
    /* DIVERGES: the decomp's `&gGRCommonStruct.sector.weapon_head`. */
    &gITManagerCommonData,                      /* Pointer to character's loaded files? */
    (intptr_t)0xBC,                             /* Offset of weapon attributes in loaded files */

    /* DObj transformation struct */
    {
        nGCMatrixKindTraRotRpyR,                /* Main matrix transformations */
        nGCMatrixKindNull,                      /* Secondary matrix transformations? */
        0                                       /* ??? */
    },

    NULL,                                       /* Proc Update */
    grSectorArwingWeaponLaser2DProcMap,         /* Proc Map */
    grSectorArwingWeaponLaser2DProcHit,         /* Proc Hit */
    grSectorArwingWeaponLaser2DProcHit,         /* Proc Shield */
    grSectorArwingWeaponLaser2DProcHop,         /* Proc Hop */
    grSectorArwingWeaponLaser2DProcHit,         /* Proc Set-Off */
    grSectorArwingWeaponLaser2DProcReflector,   /* Proc Reflector */
    grSectorArwingWeaponLaser2DProcHit          /* Proc Absorb */
};

WPDesc dGRSectorArwingWeaponLaser3DWeaponDesc =
{
    0,                                          /* Render flags? */
    nWPKindArwingLaser3D,                       /* Weapon Kind */
    /* DIVERGES: the decomp's `&gGRCommonStruct.sector.weapon_head`. */
    &gITManagerCommonData,                      /* Pointer to character's loaded files? */
    (intptr_t)0xF0,                             /* Offset of weapon attributes in loaded files */

    /* DObj transformation struct */
    {
        nGCMatrixKindTraRotRpyR,                /* Main matrix transformations */
        nGCMatrixKindNull,                      /* Secondary matrix transformations? */
        0                                       /* ??? */
    },

    NULL,                                       /* Proc Update */
    grSectorArwingWeaponLaser3DProcMap,         /* Proc Map */
    grSectorArwingWeaponLaser3DProcHit,         /* Proc Hit */
    grSectorArwingWeaponLaser3DProcHit,         /* Proc Shield */
    grSectorArwingWeaponLaser3DProcHit,         /* Proc Hop */
    grSectorArwingWeaponLaser3DProcHit,         /* Proc Set-Off */
    grSectorArwingWeaponLaser3DProcHit,         /* Proc Reflector */
    grSectorArwingWeaponLaser3DProcAbsorb       /* Proc Absorb */
};

/* // // // // // // // // // // // //
 *                                   //
 *           FUNCTIONS               //
 *                                   //
 * // // // // // // // // // // // // */

/* The one place the port reads an AObj's interpolate value is the
 * decomp's `syInterpQuad` (sys/interp.h:41) -- the path evaluator's
 * quadratic spline, whose descriptor the pack carries as the scripts'
 * SetInterp operand. */

/* grsector.c:230-275 func_ovl2_80106730, verbatim.
 *
 * THE ARWING'S OWN FRAME. All three vectors come out of the CURRENT
 * animation's TraI track -- `gcGetAObjValue` reads the interpolation
 * value, clamped to 0..1, and `syInterpQuad` turns it into a point on the
 * spline -- so the aircraft's basis follows the path it is flying. And it
 * has a second source: while map_dobjs[11] is still animating AND the
 * weapon count is 0, the third vector is read from THAT joint's TraI
 * track through the CUBIC interpolator instead, and normalised -- which
 * is how the Arwing's orientation comes from a second, hidden joint while
 * it is deploying. The three vectors are then made mutually orthogonal the
 * cheap way: cross, cross back, normalise.
 *
 * `vlen` is declared outside the first loop and used in the second, which
 * is the decomp's own scoping; it is uninitialised on the path where the
 * first loop never assigns, and the port keeps that (the second loop only
 * runs when map_dobjs[11] is animating, which is the case the first loop
 * always covers). */
void func_ovl2_80106730(DObj *arg0, Vec3f *vec1, Vec3f *vec2, Vec3f *vec3)
{
    DObj *sp54 = gGRCommonStruct.sector.map_dobjs[11];
    AObj *aobj = arg0->aobj;
    f32 vlen;

    while (aobj != NULL)
    {
        if ((aobj->kind != nGCAnimKindNone) && !(arg0->parent_gobj->flags & GOBJ_FLAG_NOANIM) && (aobj->track == nGCAnimTrackTraI))
        {
            vlen = gcGetAObjValue(aobj);

            if (vlen < 0.0F)
            {
                vlen = 0.0F;
            }
            else if (vlen > 1.0F)
            {
                vlen = 1.0F;
            }
            syInterpQuad(vec1, aobj->interpolate, vlen);
        }
        aobj = aobj->next;
    }
    if ((sp54->anim_wait != AOBJ_ANIM_NULL) && (gGRCommonStruct.sector.arwing_laser_count == 0))
    {
        aobj = sp54->aobj;

        while (aobj != NULL)
        {
            if ((aobj->kind != nGCAnimKindNone) && !(arg0->parent_gobj->flags & GOBJ_FLAG_NOANIM) && (aobj->track == nGCAnimTrackTraI))
            {
                syInterpCubic(vec3, aobj->interpolate, vlen);
            }
            aobj = aobj->next;
        }
        syVectorNorm3D(vec3);
    }
    lbCommonCross3D(vec3, vec1, vec2);
    lbCommonCross3D(vec1, vec2, vec3);

    syVectorNorm3D(vec1);
    syVectorNorm3D(vec2);
    syVectorNorm3D(vec3);
}

/* grsector.c:277-328 grSectorArwingLaser3DFuncMatrix 0x80106904,
 * verbatim.
 *
 * THE NAME IS MISLEADING: this is joint 0's own matrix function -- the
 * Arwing's ROOT, not the 3D laser's. `dGRSectorArwingTransformKinds[0].tk1`
 * (US region) is 0x53, and 0x53 is what this reaches: entry 34 of
 * `dLBCommonFuncMatrixList`, which holds a proc_diff/proc_same PAIR per
 * kind, so flat index 34 is pair 17 -- `sGCMatrixFuncList` indexes pairs by
 * kind - 66, so 66 + 17 = 0x53. The two laser WPDescs use ordinary
 * `nGCMatrixKindTraRotRpyR` (see their tables above); nothing about this
 * function is laser-specific except that both share the same aiming
 * geometry, and the decomp's symbol name was written for that resemblance,
 * not this call site.
 *
 * THE ARWING'S ORIENTATION, and the reason the root needs a matrix function
 * at all: there is no rotation channel in its animation, so the ship's
 * facing is built fresh every tic from the flight path itself.
 * `func_ovl2_80106730`'s three vectors become the basis (a 4x3, in the
 * decomp's row-vector Mtx), with the translation taken from the DObj's own
 * plus `arwing_target_x` -- the Arwing's line offset, which the collision
 * line and the laser share.
 *
 * The `arwing_laser_count == 2` arm is the TWO-WING case: the flight
 * patterns that fire the Arwing's own fixed guns rather than banking
 * toward a target, so the basis is the identity's (z forward, y up, minus
 * x right) rather than the path's.
 *
 * DIVERGES: src/dc/objdisplay.c's `case 0x53:` does not call this function
 * -- it calls `grSectorArwingLaser3DFuncMatrixF` below, the same math
 * written straight into the port's float matrix layout, for the reason
 * 0x44/0x45 there are hand-transposed instead of called. This verbatim
 * version stays for fidelity and is not itself wired into the dispatch. */
sb32 grSectorArwingLaser3DFuncMatrix(Mtx *mtx, DObj *dobj, Gfx **dls)
{
    f32 sx;
    Vec3f sp80;
    Vec3f sp74;
    Vec3f sp68;
    Mtx44f f;
    f32 tx;
    f32 ty;
    f32 tz;

    tx = dobj->translate.vec.f.x;
    ty = dobj->translate.vec.f.y;
    tz = dobj->translate.vec.f.z;

    sp80.x = -1.0F;
    sp80.y = 0.0F;
    sp80.z = 0.0F;
    sp68.x = 0.0F;
    sp68.y = 1.0F;
    sp68.z = 0.0F;

    if (gGRCommonStruct.sector.arwing_laser_count == 2)
    {
        sp74.x = sp74.y = 0.0F;
        sp74.z = 1;
    }
    else func_ovl2_80106730(dobj, &sp80, &sp74, &sp68);

    f[0][0] = sp74.x; // sp28
    f[0][1] = sp74.y; // sp2C
    f[0][2] = sp74.z; // sp30
    f[1][0] = sp68.x; // sp38
    f[1][1] = sp68.y; // sp3C
    f[1][2] = sp68.z; // sp40
    f[2][0] = sp80.x; // sp48
    f[2][1] = sp80.y; // sp4C
    f[2][2] = sp80.z; // sp50

    f[0][3] = f[1][3] = f[2][3] = 0.0F;                     // sp34, sp44, sp54

    f[3][0] = tx + gGRCommonStruct.sector.arwing_target_x;  // sp58
    f[3][1] = ty;                                           // sp5C
    f[3][2] = tz;                                           // sp60

    f[3][3] = 1.0F;                                         // sp64

    guMtxF2L(f, mtx);

    return 0;
}

/* The same matrix as grSectorArwingLaser3DFuncMatrix above, straight into
 * the port's own float layout instead of the N64's fixed-point Mtx --
 * src/dc/objdisplay.c's `case 0x53:` calls this, not the verbatim function,
 * for the reason src/dc/objdisplay.c:489/497's 0x44/0x45 comment gives: a
 * whole local matrix has to be transposed into the port's column-vector
 * storage by hand, the same way those two are. Row r, column c of the
 * port's matrix is decomp row c's component r (or the translation, at
 * c == 3), which is what the four rows below write. */
void grSectorArwingLaser3DFuncMatrixF(float *m, DObj *dobj)
{
    Vec3f sp80;
    Vec3f sp74;
    Vec3f sp68;
    f32 tx;
    f32 ty;
    f32 tz;

    tx = dobj->translate.vec.f.x;
    ty = dobj->translate.vec.f.y;
    tz = dobj->translate.vec.f.z;

    sp80.x = -1.0F;
    sp80.y = 0.0F;
    sp80.z = 0.0F;
    sp68.x = 0.0F;
    sp68.y = 1.0F;
    sp68.z = 0.0F;

    if (gGRCommonStruct.sector.arwing_laser_count == 2)
    {
        sp74.x = sp74.y = 0.0F;
        sp74.z = 1;
    }
    else func_ovl2_80106730(dobj, &sp80, &sp74, &sp68);

    m[0] = sp74.x;
    m[1] = sp68.x;
    m[2] = sp80.x;
    m[3] = tx + gGRCommonStruct.sector.arwing_target_x;
    m[4] = sp74.y;
    m[5] = sp68.y;
    m[6] = sp80.y;
    m[7] = ty;
    m[8] = sp74.z;
    m[9] = sp68.z;
    m[10] = sp80.z;
    m[11] = tz;
    m[12] = m[13] = m[14] = 0.0F;
    m[15] = 1.0F;
}

/* grsector.c:330-347 grSectorArwingAddAnim 0x80106A40, verbatim but for
 * where the script comes from.
 *
 * TWO THINGS BEYOND gcAddDObjAnimJoint. `is_anim_root = FALSE` on both
 * arms, because the Arwing's joints are not animation roots -- the GObj's
 * animation clock is map_dobjs[0]'s -- and a script is PARSED AND PLAYED
 * ONCE here rather than waiting a tic for the process: that is what makes
 * a state transition take effect on the tic it happens.
 *
 * The else arm is reached on purpose by two of the eight flight patterns:
 * their descriptors' 0x2C field is NULL and no script is added at all,
 * which is the same thing the decomp's own NULL does there. */
void grSectorArwingAddAnim(DObj *dobj, AObjEvent32 *anim_joint, f32 unused)
{
    if (anim_joint != NULL)
    {
        gcAddDObjAnimJoint(dobj, anim_joint, 0.0F);

        dobj->is_anim_root = FALSE;

        gcParseDObjAnimJoint(dobj);
        gcPlayDObjAnimJoint(dobj);
    }
    else
    {
        dobj->anim_wait = AOBJ_ANIM_NULL;
        dobj->is_anim_root = FALSE;
    }
}

/* ... and the by-name form of it, which is what this file calls: the
 * decomp's `lbRelocGetFileData(AObjEvent32*, base, &symbol)`, with the
 * symbol replaced by the name the pack knows. A name the pack has not got
 * is a NULL, which the function above treats as "no animation" -- and a
 * NULL name is passed on purpose for the two descriptors whose 0x2C field
 * is NULL (Arwing1 and Arwing4; see the pattern table). */
static void grSectorArwingAddAnimName(DObj *dobj, const char *name)
{
    if (dobj == NULL)
    {
        return;
    }
    grSectorArwingAddAnim(dobj, (name != NULL)
                               ? (AObjEvent32 *)stage_map_anim(name)
                               : NULL, 0.0F);
}

/* grsector.c:349-356 grSectorArwingUpdateSleep 0x80106A98, verbatim: the
 * Arwing does not appear until the match is actually running. */
void grSectorArwingUpdateSleep(void)
{
    if (gSCManagerBattleState->game_status != nSCBattleGameStatusWait)
    {
        gGRCommonStruct.sector.arwing_status = nGRSectorArwingStatusWait;
    }
}

/* grsector.c:358-411 grSectorArwingUpdateWait 0x80106AC0, verbatim.
 *
 * THE DRAW. `arwing_appear_timer` counts down to zero and then a pattern
 * is picked. The first three appearances of a cycle (`arwing_type_cycle`)
 * are the plain ones and the fourth draws an x target from
 * `dGRSectorArwingMapPositionsX` ONE TIME IN FIVE, which is what makes an
 * Arwing fly at the player instead of across; after the cycle is spent the
 * draw changes shape entirely and `unk_sector_0x4C` picks between a fixed
 * pilot and none -- the "ghost" flight, which is why that field is `-1`
 * for one arm and an id for the other.
 *
 * Everything the last flight left behind is reset here, including the
 * collision line's three flags and the ammunition -- the Arwing is
 * re-armed, not resumed. */
void grSectorArwingUpdateWait(void)
{
    s32 random;

    if (gGRCommonStruct.sector.arwing_appear_timer != 0)
    {
        gGRCommonStruct.sector.arwing_appear_timer--;
    }
    else
    {
        gGRCommonStruct.sector.arwing_target_x = 0.0F;

        if (gGRCommonStruct.sector.arwing_type_cycle != 0)
        {
            random = syUtilsRandIntRange(5);

            if (random == 4)
            {
                gGRCommonStruct.sector.arwing_target_x = dGRSectorArwingMapPositionsX[syUtilsRandIntRange(ARRAY_COUNT(dGRSectorArwingMapPositionsX))];
            }
            gGRCommonStruct.sector.arwing_type_cycle--;
            gGRCommonStruct.sector.arwing_state_timer = syUtilsRandIntRange(540) + 180;
            gGRCommonStruct.sector.arwing_pilot_curr = -1;
        }
        else
        {
            random = syUtilsRandIntRange(3) + 5;

            gGRCommonStruct.sector.unk_sector_0x4C = ((syUtilsRandUShort() % 2) != 0) ? random - 5 : -1;

            gGRCommonStruct.sector.arwing_type_cycle = 3;
            gGRCommonStruct.sector.arwing_pilot_curr = -2;
        }
        gGRCommonStruct.sector.arwing_flight_pattern = random;
        gGRCommonStruct.sector.arwing_status = 2;
        gGRCommonStruct.sector.unk_sector_0x4E = 0x3C;

        gGRCommonStruct.sector.map_dobjs[1]->translate.vec.f.x =
        gGRCommonStruct.sector.map_dobjs[1]->translate.vec.f.y =
        gGRCommonStruct.sector.map_dobjs[1]->translate.vec.f.z = 0.0F;

        gGRCommonStruct.sector.map_dobjs[1]->rotate.vec.f.x =
        gGRCommonStruct.sector.map_dobjs[1]->rotate.vec.f.y =
        gGRCommonStruct.sector.map_dobjs[1]->rotate.vec.f.z = 0.0F;

        gGRCommonStruct.sector.is_arwing_line_collision = FALSE;
        gGRCommonStruct.sector.is_arwing_line_active = TRUE;
        gGRCommonStruct.sector.is_arwing_z_collision = FALSE;
        gGRCommonStruct.sector.arwing_laser_ammo = 0;

        func_800269C0_275C0(nSYAudioFGMSectorAmbient1);
    }
}

/* grsector.c:413-421 grSectorArwingDecideZNear 0x80106C28, verbatim: the
 * Arwing is "near" while its own z is inside 200 units of the deck line --
 * which is what lets its collision line be on. */
void grSectorArwingDecideZNear(void)
{
    if (ABSF(gGRCommonStruct.sector.map_dobjs[0]->translate.vec.f.z) < 200.0F)
    {
        gGRCommonStruct.sector.is_arwing_z_near = TRUE;
    }
    else gGRCommonStruct.sector.is_arwing_z_near = FALSE;
}

/* grsector.c:423-436 func_ovl2_80106C88 0x80106C28's neighbour, verbatim:
 * the collision line goes on and off at two points of pilot 1's flight. */
void func_ovl2_80106C88(void)
{
    switch (gGRCommonStruct.sector.arwing_state_timer)
    {
    case 0:
        gGRCommonStruct.sector.is_arwing_line_active = FALSE;
        break;

    case 88:
        gGRCommonStruct.sector.is_arwing_line_active = TRUE;
        break;
    }
}

/* grsector.c:438-451 func_ovl2_80106CC4, verbatim: pilot 4's, later in its
 * flight. */
void func_ovl2_80106CC4(void)
{
    switch (gGRCommonStruct.sector.arwing_state_timer)
    {
    case 0:
        gGRCommonStruct.sector.is_arwing_line_active = FALSE;
        break;

    case 178:
        gGRCommonStruct.sector.is_arwing_line_active = TRUE;
        break;
    }
}

/* grsector.c:453-474 func_ovl2_80106D00, verbatim but for the two
 * scripts (see this file's header).
 *
 * PILOT 5 IS THE TRANSFORM. At the start of its flight the wings'
 * animations are stopped and two joints' hidden flags swap, then
 * map_dobjs[8] plays the 2EB4 script -- an Arwing folding its wings, or
 * opening them. When that ends the flags swap back. And at the end, the
 * GObj's own animation is stopped by map_dobjs[1]'s, which is the chain
 * the patrol watches. */
void func_ovl2_80106D00(void)
{
    if (gGRCommonStruct.sector.arwing_state_timer == 0)
    {
        gGRCommonStruct.sector.map_dobjs[7]->anim_wait = AOBJ_ANIM_NULL;
        gGRCommonStruct.sector.map_dobjs[7]->flags = DOBJ_FLAG_NONE;
        gGRCommonStruct.sector.map_dobjs[9]->anim_wait = AOBJ_ANIM_NULL;
        gGRCommonStruct.sector.map_dobjs[9]->flags = DOBJ_FLAG_HIDDEN;

        grSectorArwingAddAnimName(gGRCommonStruct.sector.map_dobjs[8],
                                  GRSECTOR_ANIM_TRANSFORM);
    }
    else if (gGRCommonStruct.sector.map_dobjs[8]->anim_wait == AOBJ_ANIM_NULL)
    {
        gGRCommonStruct.sector.map_dobjs[7]->flags = DOBJ_FLAG_HIDDEN;
        gGRCommonStruct.sector.map_dobjs[9]->flags = DOBJ_FLAG_NONE;
    }
    if (gGRCommonStruct.sector.map_dobjs[1]->anim_wait == AOBJ_ANIM_NULL)
    {
        gGRCommonStruct.sector.map_dobjs[0]->anim_wait = AOBJ_ANIM_NULL;
    }
}

/* grsector.c:476-521 func_ovl2_80106DD8 0x80106CD8, verbatim.
 *
 * THE PILOT'S SPAN. A flight is in two halves and `arwing_pilot_curr`
 * says which: while it is non-negative the pilot's animation is playing,
 * the timer COUNTING UP, and the three per-pilot hooks above fire at
 * particular tics; when the animation ends the pilot is cleared and the
 * timer is set to 120. Then the timer counts DOWN, and at zero a new pilot
 * is drawn -- out of the row of `dGRSectorArwingPilotWaitTimers` that
 * belongs to the pilot who just flew, so a pilot never follows itself --
 * and if the draw comes back 0 there is no pilot at all, which is the
 * `-1`/`-2` states above. */
void func_ovl2_80106DD8(void)
{
    if (gGRCommonStruct.sector.arwing_pilot_curr != -2)
    {
        if (gGRCommonStruct.sector.arwing_pilot_curr >= 0)
        {
            switch (gGRCommonStruct.sector.arwing_pilot_curr)
            {
            case 1:
                func_ovl2_80106C88();
                break;

            case 4:
                func_ovl2_80106CC4();
                break;

            case 5:
                func_ovl2_80106D00();
                break;
            }
            if (gGRCommonStruct.sector.map_dobjs[1]->anim_wait == AOBJ_ANIM_NULL)
            {
                gGRCommonStruct.sector.arwing_pilot_curr = -1;
                gGRCommonStruct.sector.arwing_state_timer = 120;
            }
            else gGRCommonStruct.sector.arwing_state_timer++;
        }
        else
        {
            gGRCommonStruct.sector.arwing_state_timer--;

            if (gGRCommonStruct.sector.arwing_state_timer == 0)
            {
                u8 *random = &dGRSectorArwingPilotWaitTimers[gGRCommonStruct.sector.arwing_pilot_prev][0];
                s32 pilot_id = dGRSectorArwingPilotIDs[random[0] + syUtilsRandIntRange(random[1])];

                if (pilot_id != 0)
                {
                    grSectorArwingAddAnimName(gGRCommonStruct.sector.map_dobjs[1],
                                              sGRSectorArwingPilotAnims[pilot_id]);
                }
                gGRCommonStruct.sector.arwing_pilot_prev = gGRCommonStruct.sector.arwing_pilot_curr = pilot_id;
            }
        }
    }
}

/* grsector.c:523-531 grSectorArwingPrepareLaserCount 0x80106F2C,
 * verbatim. One in three is a pair; otherwise four -- which with the
 * two-shot-per-fire in the maker below is two volleys. */
s32 grSectorArwingPrepareLaserCount(void)
{
    if (syUtilsRandIntRange(3) >= 3)
    {
        return 2;
    }
    else return 4;
}

/* grsector.c:533-559 grSectorArwingGetLaserAmmoCount 0x80106F5C,
 * verbatim.
 *
 * THE AIM CHECK, and it is the TWO-WING case only: the 3D bolt is aimed
 * at whoever is picked, but the fixed wing lasers are only fired if a
 * fighter is actually in front of them -- x less than the Arwing's own
 * line, and y within +300/-500 of it. Any fighter satisfying that arms
 * the whole volley. */
s32 grSectorArwingGetLaserAmmoCount(void)
{
    GObj *fighter_gobj = gGCCommonLinks[nGCCommonLinkIDFighter];
    f32 pos_x = gGRCommonStruct.sector.map_dobjs[0]->translate.vec.f.x + gGRCommonStruct.sector.arwing_target_x;
    f32 pos_y = gGRCommonStruct.sector.map_dobjs[0]->translate.vec.f.y + gGRCommonStruct.sector.map_dobjs[1]->translate.vec.f.y;

    while (fighter_gobj != NULL)
    {
        FTStruct *fp = ftGetStruct(fighter_gobj);

        if (gGRCommonStruct.sector.arwing_laser_count == 2)
        {
            DObj *joint = fp->joints[nFTPartsJointTopN];

            if (joint->translate.vec.f.x < pos_x)
            {
                if ((joint->translate.vec.f.y < (pos_y + 300.0F)) && (joint->translate.vec.f.y > (pos_y + -500.0F)))
                {
                    return grSectorArwingPrepareLaserCount();
                }
            }
        }
        fighter_gobj = fighter_gobj->link_next;
    }
    return 0;
}

/* grsector.c:561-571 grSectorArwingWeaponLaser2DProcMap 0x80107030,
 * verbatim: the wing laser stops at the scenery with a small puff. */
sb32 grSectorArwingWeaponLaser2DProcMap(GObj *weapon_gobj)
{
    if (wpMapTestAllCheckCollEnd(weapon_gobj) != FALSE)
    {
        efManagerDustExpandSmallMakeEffect(&DObjGetStruct(weapon_gobj)->translate.vec.f, 1.0F);

        return TRUE;
    }
    else return FALSE;
}

/* grsector.c:573-581 grSectorArwingWeaponLaser2DProcHit 0x80107074,
 * verbatim: a shock, and the laser is CONSUMED. */
sb32 grSectorArwingWeaponLaser2DProcHit(GObj *weapon_gobj)
{
    WPStruct *wp = wpGetStruct(weapon_gobj);

    efManagerImpactShockMakeEffect(&DObjGetStruct(weapon_gobj)->translate.vec.f, wp->attack_coll.damage);

    return TRUE;
}

/* grsector.c:583-606 func_ovl2_801070A4 0x801070A4, verbatim.
 *
 * VELOCITY TO EULER. The special case is the one that matters for these
 * lasers: a velocity lying ON the z axis has no yaw to derive from
 * atan2(y, x) -- both are zero -- so the basis is built from the 90-degree
 * yaw instead, and only the pitch comes from the two components that are
 * not degenerate. Every other direction takes the ordinary atan2 chain. */
void func_ovl2_801070A4(Vec3f *rotate, Vec3f *direction, Vec3f *vec3, Vec3f *vec4)
{
    if ((vec3->z == -1.0F) || (vec3->z == 1.0F))
    {
        if (vec3->z == -1.0F)
        {
            rotate->y = F_CST_DTOR32(90.0F);
            rotate->x = syUtilsArcTan2(vec4->x, vec4->y);
        }
        else
        {
            rotate->y = F_CST_DTOR32(-90.0F);
            rotate->x = syUtilsArcTan2(-vec4->x, vec4->y);
        }
        rotate->z = 0.0F;
    }
    else
    {
        rotate->y = syUtilsArcSin(-vec3->z);
        rotate->x = syUtilsArcTan2(vec4->z, direction->z);
        rotate->z = syUtilsArcTan2(vec3->y, vec3->x);
    }
}

/* grsector.c:608-628 func_ovl2_8010719C 0x8010719C, verbatim.
 *
 * A velocity and a rotation into a rotation: the Arwing's own yaw plus 90
 * degrees gives the "up" the laser's frame is built around, two crosses
 * orthogonalise it against the velocity, and func_ovl2_801070A4 turns the
 * pair into Euler angles. So a laser is not aimed by a target -- its
 * orientation is derived from the direction it is already travelling. */
void func_ovl2_8010719C(Vec3f *vel, Vec3f *rotate)
{
    Vec3f sp2C;
    Vec3f sp20;
    f32 unused;
    f32 rot_z;

    sp20.x = 0.0F;

    rot_z = gGRCommonStruct.sector.map_dobjs[1]->rotate.vec.f.z + F_CST_DTOR32(90.0F);

    sp20.y = __sinf(rot_z);
    sp20.z = __cosf(rot_z);

    lbCommonCross3D(&sp20, vel, &sp2C);
    lbCommonCross3D(vel, &sp2C, &sp20);
    syVectorNorm3D(&sp2C);
    syVectorNorm3D(&sp20);
    func_ovl2_801070A4(rotate, vel, &sp2C, &sp20);
}

/* grsector.c:630-644 grSectorArwingWeaponLaser2DProcHop 0x80107238,
 * verbatim: a laser that bounces off a shield keeps flying, re-aimed. */
sb32 grSectorArwingWeaponLaser2DProcHop(GObj *weapon_gobj)
{
    WPStruct *wp = wpGetStruct(weapon_gobj);
    Vec3f vel;

    syVectorRotateAbout3D(&wp->physics.vel_air, &wp->shield_collide_dir, wp->shield_collide_angle * 2);

    vel = wp->physics.vel_air;

    syVectorNorm3D(&vel);
    func_ovl2_8010719C(&vel, &DObjGetStruct(weapon_gobj)->rotate.vec.f);

    return FALSE;
}

/* grsector.c:646-661 grSectorArwingWeaponLaser2DProcReflector 0x801072C0,
 * verbatim: the same re-aim, and the facing follows the new owner. */
sb32 grSectorArwingWeaponLaser2DProcReflector(GObj *weapon_gobj)
{
    WPStruct *wp = wpGetStruct(weapon_gobj);
    FTStruct *fp = ftGetStruct(wp->owner_gobj);
    Vec3f vel;

    wpMainReflectorSetLR(wp, fp);

    vel = wp->physics.vel_air;

    syVectorNorm3D(&vel);
    func_ovl2_8010719C(&vel, &DObjGetStruct(weapon_gobj)->rotate.vec.f);

    return FALSE;
}

/* grsector.c:663-720 grSectorArwingWeaponLaser2DMakeWeapon 0x80107330,
 * verbatim: TWO lasers, one per wing, and the second is made only if the
 * first was.
 *
 * The spawn positions are the Arwing's own frame applied to joints 2 and
 * 3, each rotated by the GObj's yaw and pushed 566 units forward -- the
 * wing tips. Both fly at -230 on x whatever the Arwing's attitude, because
 * the WING lasers are the fixed ones; only the 3D bolt is aimed. And the
 * second laser's rotation is the first's, so the pair are parallel. */
void grSectorArwingWeaponLaser2DMakeWeapon(void)
{
    GObj *weapon_gobj;
    WPStruct *wp;
    Vec3f sp54;
    Vec3f sp48;
    Vec3f pos;
    Vec3f rotate;
    Vec3f vel;
    f32 zero = 0.0F;

    sp54.x = gGRCommonStruct.sector.map_dobjs[0]->translate.vec.f.x + gGRCommonStruct.sector.arwing_target_x;
    sp54.y = gGRCommonStruct.sector.map_dobjs[0]->translate.vec.f.y + gGRCommonStruct.sector.map_dobjs[1]->translate.vec.f.y;

    sp48 = gGRCommonStruct.sector.map_dobjs[2]->translate.vec.f;

    syVectorRotate3D(&sp48, SYVECTOR_AXIS_Z, gGRCommonStruct.sector.map_dobjs[1]->rotate.vec.f.z);

    pos.x = (sp54.x - sp48.z) - 566.0F;
    pos.y = sp54.y + sp48.y;
    pos.z = zero + sp48.x;

    weapon_gobj = wpManagerMakeWeapon(NULL, &dGRSectorArwingWeaponLaser2DWeaponDesc, &pos, WEAPON_FLAG_PARENT_GROUND);

    if (weapon_gobj != NULL)
    {
        wp = wpGetStruct(weapon_gobj);

        wp->physics.vel_air.x = -230.0F;

        vel.y = vel.z = 0.0F;
        vel.x = -1.0F;

        func_ovl2_8010719C(&vel, &rotate);

        DObjGetStruct(weapon_gobj)->rotate.vec.f = rotate;

        sp48 = gGRCommonStruct.sector.map_dobjs[3]->translate.vec.f;

        syVectorRotate3D(&sp48, 4, gGRCommonStruct.sector.map_dobjs[1]->rotate.vec.f.z);

        pos.x = (sp54.x - sp48.z) - 566.0F;
        pos.y = sp54.y + sp48.y;
        pos.z = zero + sp48.x;

        weapon_gobj = wpManagerMakeWeapon(NULL, &dGRSectorArwingWeaponLaser2DWeaponDesc, &pos, WEAPON_FLAG_PARENT_GROUND);

        if (weapon_gobj != NULL)
        {
            wp = wpGetStruct(weapon_gobj);

            wp->physics.vel_air.x = -230.0F;

            DObjGetStruct(weapon_gobj)->rotate.vec.f = rotate;
        }
    }
}

/* grsector.c:722-730 grSectorArwingWeaponLaserExplodeProcUpdate
 * 0x80107518, verbatim: the exploding laser is a plain timer. */
sb32 grSectorArwingWeaponLaserExplodeProcUpdate(GObj *weapon_gobj)
{
    if (wpMainDecLifeCheckExpire(wpGetStruct(weapon_gobj)) != FALSE)
    {
        return TRUE;
    }
    else return FALSE;
}

/* grsector.c:732-760 grSectorArwingWeaponLaserExplodeInitVars 0x80107544,
 * verbatim.
 *
 * A LASER BECOMES A BOMB IN PLACE. The bolt's model is removed (dl NULL),
 * it stops dead, its hurtbox grows to 200, it stops being reflectable and
 * shieldable but CAN be absorbed, its attack record is cleared so it hits
 * everything again, and every proc but its own timer is cleared. Sixteen
 * tics later it is gone. The decomp's six assignments end with
 * `wp->proc_hop = NULL` written TWICE (where `proc_setoff` should be), and
 * the port keeps that: `proc_setoff` is left alone, which is harmless only
 * because the arm that could reach it is cleared too. */
void grSectorArwingWeaponLaserExplodeInitVars(GObj *weapon_gobj)
{
    WPStruct *wp = wpGetStruct(weapon_gobj);

    wp->lifetime = 16;

    wp->attack_coll.can_reflect = FALSE;
    wp->attack_coll.can_absorb = TRUE;
    wp->attack_coll.can_shield = FALSE;

    wp->physics.vel_air.x = wp->physics.vel_air.y = wp->physics.vel_air.z = 0.0F;

    wp->attack_coll.size = 200.0F;

    DObjGetStruct(weapon_gobj)->dl = NULL;

    wpMainClearAttackRecord(wp);

    wp->proc_update = grSectorArwingWeaponLaserExplodeProcUpdate;

    wp->proc_map        =
    wp->proc_hit        =
    wp->proc_shield     =
    wp->proc_hop        =
    wp->proc_setoff     =
    wp->proc_hop        =
    wp->proc_reflector  =  NULL;
}

/* grsector.c:762-777 grSectorArwingWeaponLaser3DProcMap 0x801075E0,
 * verbatim: the bolt only arms itself once it is within 1000 units of the
 * deck line, and the explosion is a multi-spark rather than a puff. */
sb32 grSectorArwingWeaponLaser3DProcMap(GObj *weapon_gobj)
{
    DObj *dobj = DObjGetStruct(weapon_gobj);

    if (ABSF(dobj->translate.vec.f.z) < 1000.0F)
    {
        if (wpMapTestAllCheckCollEnd(weapon_gobj) != FALSE)
        {
            func_800269C0_275C0(nSYAudioFGMExplodeS);
            efManagerSparkleWhiteMultiExplodeMakeEffect(&dobj->translate.vec.f);
            grSectorArwingWeaponLaserExplodeInitVars(weapon_gobj);
        }
    }
    return FALSE;
}

/* grsector.c:779-787 grSectorArwingWeaponLaser3DProcHit 0x80107670,
 * verbatim: hit anything and it becomes the bomb, in place. */
sb32 grSectorArwingWeaponLaser3DProcHit(GObj *weapon_gobj)
{
    func_800269C0_275C0(nSYAudioFGMExplodeS);
    efManagerSparkleWhiteMultiExplodeMakeEffect(&DObjGetStruct(weapon_gobj)->translate.vec.f);
    grSectorArwingWeaponLaserExplodeInitVars(weapon_gobj);

    return FALSE;
}

/* grsector.c:789-796 grSectorArwingWeaponLaser3DProcAbsorb 0x801076B0,
 * verbatim: absorbed means GONE, and the spark is the last of it. */
sb32 grSectorArwingWeaponLaser3DProcAbsorb(GObj *weapon_gobj)
{
    func_800269C0_275C0(nSYAudioFGMExplodeS);
    efManagerSparkleWhiteMultiExplodeMakeEffect(&DObjGetStruct(weapon_gobj)->translate.vec.f);

    return TRUE;
}

/* grsector.c:798-885 grSectorArwingWeaponLaser3DMakeWeapon 0x801076E8,
 * verbatim.
 *
 * THE AIMED SHOT. It starts from the Arwing's own frame -- the same
 * `func_ovl2_80106730` the matrix function uses -- with the muzzle 666
 * units along it, and its TARGET is a FIGHTER PICKED AT RANDOM from the
 * live ones (`syUtilsRandIntRange(pl_count + cp_count)` walked along the
 * fighter link). The direction is the normalised difference, and the
 * velocity is that times 230.
 *
 * A fighter in the air or off a ledge has no floor line, and then the
 * target position is the ORIGIN rather than a joint -- the decomp's own
 * arm, which makes an air-borne target's bolt fly at the middle of the
 * stage. */
void grSectorArwingWeaponLaser3DMakeWeapon(void)
{
    GObj *weapon_gobj;
    GObj *fighter_gobj;
    s32 random;
    s32 player;
    FTStruct *fp;
    WPStruct *wp;
    Vec3f wp_pos;
    Vec3f ft_pos;
    Vec3f sp94;
    Vec3f sp88;
    Vec3f sp7C;
    Vec3f wp_angle;
    Mtx44f mtx;
    DObj *dobj;

    dobj = gGRCommonStruct.sector.map_dobjs[0];

    func_ovl2_80106730(dobj, &sp94, &sp88, &sp7C);

    mtx[0][0] = sp88.x; // sp30
    mtx[0][1] = sp88.y; // sp34
    mtx[0][2] = sp88.z; // sp38

    mtx[1][0] = sp7C.x; // sp40
    mtx[1][1] = sp7C.y; // sp44
    mtx[1][2] = sp7C.z; // sp48

    mtx[2][0] = sp94.x; // sp50
    mtx[2][1] = sp94.y; // sp54
    mtx[2][2] = sp94.z; // sp58

    mtx[0][3] = mtx[1][3] = mtx[2][3] = 0.0F;// sp3C, sp4C, sp5C

    mtx[3][0] = dobj->translate.vec.f.x + gGRCommonStruct.sector.arwing_target_x; // sp60
    mtx[3][1] = dobj->translate.vec.f.y; // sp64
    mtx[3][2] = dobj->translate.vec.f.z;

    mtx[3][3] = 1.0F;

    wp_pos.y = 0.0F;
    wp_pos.x = 0;

    wp_pos.z = 666.0F;

    gmCollisionGetWorldPosition(mtx, &wp_pos);

    random = syUtilsRandIntRange(gSCManagerBattleState->pl_count + gSCManagerBattleState->cp_count);

    fighter_gobj = gGCCommonLinks[nGCCommonLinkIDFighter];

    for (player = 0; player < random; player++)
    {
        fighter_gobj = fighter_gobj->link_next;
    }
    fp = ftGetStruct(fighter_gobj);

    if ((fp->coll_data.floor_line_id == -1) || (fp->coll_data.floor_line_id == -2))
    {
        ft_pos.x = ft_pos.y = ft_pos.z = 0;
    }
    else
    {
        ft_pos = fp->joints[nFTPartsJointTopN]->translate.vec.f;

        ft_pos.y += fp->coll_data.floor_dist;
    }
    wp_angle.x = ft_pos.x - wp_pos.x;
    wp_angle.y = ft_pos.y - wp_pos.y;
    wp_angle.z = ft_pos.z - wp_pos.z;

    syVectorNorm3D(&wp_angle);

    weapon_gobj = wpManagerMakeWeapon(NULL, &dGRSectorArwingWeaponLaser3DWeaponDesc, &wp_pos, WEAPON_FLAG_PARENT_GROUND);

    if (weapon_gobj != NULL)
    {
        wp = wpGetStruct(weapon_gobj);

        wp->physics.vel_air.x = wp_angle.x * 230.0F;
        wp->physics.vel_air.y = wp_angle.y * 230.0F;
        wp->physics.vel_air.z = wp_angle.z * 230.0F;

        func_ovl2_8010719C(&wp_angle, &DObjGetStruct(weapon_gobj)->rotate.vec.f);
    }
}

/* grsector.c:887-897 func_ovl2_80107910 0x80107910, verbatim: which of the
 * two weapons a volley is, decided by the pattern's own count -- 2 is the
 * pair of wing lasers, anything else the aimed bolt. */
void func_ovl2_80107910(void)
{
    if (gGRCommonStruct.sector.arwing_laser_count == 2)
    {
        grSectorArwingWeaponLaser2DMakeWeapon();
    }
    else grSectorArwingWeaponLaser3DMakeWeapon();

    func_800269C0_275C0(nSYAudioFGMSectorArwingLaser);
}

/* grsector.c:899-978 func_ovl2_80107958 0x80107958, verbatim but for the
 * two Fox-special3 scripts (see this file's header, divergence 3).
 *
 * THE VOLLEY. With ammunition left, the muzzle parts SHOW first -- the
 * wing-scale script on joints 4 and 5, which scales them up out of the
 * 1e-5 the tree gives them -- and when that has ended the shot is fired
 * and the gun-scale script is played on joints 2 and 3, the joints the
 * two wing lasers actually spawn from. The timer is 30 tics, which with a
 * count of 4 is two volleys 30 apart.
 *
 * The `ammo == 0` half is the ARMING: a ghost flight (`pilot_curr == -2`)
 * arms at appear_timer 5, and a piloted one every 60 tics WHILE the
 * Arwing is near the deck line (`is_arwing_z_near`) -- so an Arwing that
 * has turned away stops firing. */
void func_ovl2_80107958(void)
{
    s32 ammo;

    if (gGRCommonStruct.sector.arwing_laser_ammo == 0)
    {
        ammo = 0;

        if (gGRCommonStruct.sector.arwing_pilot_curr == -2)
        {
            if
            (
                ((gGRCommonStruct.sector.unk_sector_0x4C == 0) && (gGRCommonStruct.sector.arwing_appear_timer == 5)) ||
                ((gGRCommonStruct.sector.unk_sector_0x4C == 1) && (gGRCommonStruct.sector.arwing_appear_timer == 5))
            )
            {
                ammo = grSectorArwingPrepareLaserCount();
            }
        }
        else if (gGRCommonStruct.sector.is_arwing_z_near == 0)
        {
            gGRCommonStruct.sector.unk_sector_0x4E = 60;
            gGRCommonStruct.sector.arwing_laser_ammo = 0;
        }
        else
        {
            gGRCommonStruct.sector.unk_sector_0x4E--;

            if (gGRCommonStruct.sector.unk_sector_0x4E == 0)
            {
                ammo = grSectorArwingGetLaserAmmoCount();

                gGRCommonStruct.sector.unk_sector_0x4E = 60;
            }
        }
        if (ammo != 0)
        {
            gGRCommonStruct.sector.arwing_laser_ammo = ammo;
            gGRCommonStruct.sector.arwing_laser_timer = 0;
            gGRCommonStruct.sector.unk_sector_0x52 = 0;
        }
    }
    else
    {
        if (gGRCommonStruct.sector.arwing_laser_timer == 0)
        {
            if (gGRCommonStruct.sector.unk_sector_0x52 == 0)
            {
                grSectorArwingAddAnimName(gGRCommonStruct.sector.map_dobjs[4],
                                          GRSECTOR_ANIM_WINGSCALE);
                grSectorArwingAddAnimName(gGRCommonStruct.sector.map_dobjs[5],
                                          GRSECTOR_ANIM_WINGSCALE);

                gGRCommonStruct.sector.unk_sector_0x52++;
            }
            else if (gGRCommonStruct.sector.map_dobjs[4]->anim_wait == AOBJ_ANIM_NULL)
            {
                func_ovl2_80107910();

                grSectorArwingAddAnimName(gGRCommonStruct.sector.map_dobjs[2],
                                          GRSECTOR_ANIM_GUNSCALE);
                grSectorArwingAddAnimName(gGRCommonStruct.sector.map_dobjs[3],
                                          GRSECTOR_ANIM_GUNSCALE);

                gGRCommonStruct.sector.arwing_laser_timer = 30;
                gGRCommonStruct.sector.arwing_laser_ammo--;
            }
        }
        if (gGRCommonStruct.sector.arwing_laser_timer)
        {
            gGRCommonStruct.sector.arwing_laser_timer--;
        }
        if (gGRCommonStruct.sector.arwing_laser_ammo == 0)
        {
            gGRCommonStruct.sector.unk_sector_0x4E = 240;
        }
    }
}

/* grsector.c:980-989 func_ovl2_80107B30 0x80107B30, verbatim but for the
 * script (see this file's header).
 *
 * The transform script, played from the OTHER direction: whenever joint 8
 * is idle and joint 7 is visible, it starts again -- which is how an
 * Arwing that transformed keeps cycling its wings while the joint's
 * hidden flag says so. */
void func_ovl2_80107B30(void)
{
    if ((gGRCommonStruct.sector.map_dobjs[8]->anim_wait == AOBJ_ANIM_NULL) && (gGRCommonStruct.sector.map_dobjs[7]->flags == DOBJ_FLAG_NONE))
    {
        grSectorArwingAddAnimName(gGRCommonStruct.sector.map_dobjs[8],
                                  GRSECTOR_ANIM_TRANSFORM);

        func_800269C0_275C0(nSYAudioFGMSectorAmbient2);
    }
}

/* grsector.c:991-1021 grSectorArwingUpdateCollisions 0x80107BA0,
 * verbatim.
 *
 * THE WALL. Map-object collision 1 is turned on and DRAGGED to the
 * Arwing's own line -- `map_dobjs[0]`'s position plus the pattern's x
 * target, and `map_dobjs[1]`'s y on top, which is the aircraft's own
 * altitude offset. It is only on while the line is active AND the Arwing
 * is near the deck, and turning it off needs BOTH of the previous tic's
 * flags, which is what the two `is_arwing_*_collision` fields remember.
 *
 * Note the double `mpCollisionSetYakumonoPosID(1, &pos)` in the first arm:
 * the decomp calls it inside the flag test and again after it, so a tic
 * that only just turned the line on moves it twice. Kept. */
void grSectorArwingUpdateCollisions(void)
{
    Vec3f pos;

    if (gGRCommonStruct.sector.arwing_pilot_curr != -2)
    {
        if ((gGRCommonStruct.sector.is_arwing_line_active) && (gGRCommonStruct.sector.is_arwing_z_near))
        {
            pos.x = gGRCommonStruct.sector.map_dobjs[0]->translate.vec.f.x + gGRCommonStruct.sector.arwing_target_x;
            pos.y = gGRCommonStruct.sector.map_dobjs[0]->translate.vec.f.y + gGRCommonStruct.sector.map_dobjs[1]->translate.vec.f.y;
            pos.z = 0.0F;

            if ((gGRCommonStruct.sector.is_arwing_z_collision == FALSE) || (gGRCommonStruct.sector.is_arwing_line_collision == FALSE))
            {
                mpCollisionSetYakumonoOnID(1);
                mpCollisionSetYakumonoPosID(1, &pos);
            }
            mpCollisionSetYakumonoPosID(1, &pos);
        }
        if (!(gGRCommonStruct.sector.is_arwing_line_active) || !(gGRCommonStruct.sector.is_arwing_z_near))
        {
            if ((gGRCommonStruct.sector.is_arwing_z_collision != FALSE) && (gGRCommonStruct.sector.is_arwing_line_collision != FALSE))
            {
                mpCollisionSetYakumonoOffID(1);
            }
        }
        gGRCommonStruct.sector.is_arwing_line_collision = gGRCommonStruct.sector.is_arwing_line_active;
        gGRCommonStruct.sector.is_arwing_z_collision = gGRCommonStruct.sector.is_arwing_z_near;
    }
}

/* grsector.c:1023-1042 grSectorArwingUpdatePatrol 0x80107CA0, verbatim.
 *
 * THE PATROL'S ONE TIC. The five sub-updates run in the decomp's order --
 * z-nearness, the pilot machine, the volley, the transform, the collision
 * line -- and then the Arwing's own animation is checked: when
 * `map_dobjs[0]`'s script ENDS the whole thing is over, the map GObj is
 * hidden, the next appearance is drawn from 960..2100 tics out, and the
 * collision line goes off. `appear_timer` counts UP while the Arwing is
 * out, so the next flight's delay is measured from the draw. */
void grSectorArwingUpdatePatrol(void)
{
    grSectorArwingDecideZNear();
    func_ovl2_80106DD8();
    func_ovl2_80107958();
    func_ovl2_80107B30();
    grSectorArwingUpdateCollisions();

    if (gGRCommonStruct.sector.map_dobjs[0]->anim_wait == AOBJ_ANIM_NULL)
    {
        gGRCommonStruct.sector.map_gobj->flags = GOBJ_FLAG_HIDDEN;

        gGRCommonStruct.sector.arwing_appear_timer = syUtilsRandIntRange(1140) + 960;
        gGRCommonStruct.sector.arwing_status = nGRSectorArwingStatusWait;

        mpCollisionSetYakumonoOffID(1);
    }
    else gGRCommonStruct.sector.arwing_appear_timer++;
}

/* grsector.c:1044-1065 func_ovl2_80107D50 0x80107D50, verbatim but for the
 * four scripts (see this file's header).
 *
 * THE PATTERN IS APPLIED HERE, one tic after it was drawn, and it is four
 * scripts at once: the path on map_dobjs[0] (whose end is the whole
 * flight's end), and three more on joints 7, 9 and 11. Then the pattern is
 * cleared and the map GObj is unhidden -- so the draw in
 * grSectorArwingUpdateWait and this are one state apart by design. */
void func_ovl2_80107D50(void)
{
    GObj *map_gobj;

    if (gGRCommonStruct.sector.arwing_flight_pattern != -1)
    {
        s32 pattern = gGRCommonStruct.sector.arwing_flight_pattern;

        map_gobj = gGRCommonStruct.sector.map_gobj;
        gGRCommonStruct.sector.arwing_laser_count = dGRSectorArwingLaserCounts[pattern];

        grSectorArwingAddAnimName(gGRCommonStruct.sector.map_dobjs[0],
                                  sGRSectorArwingPatternAnims[pattern][0]);
        grSectorArwingAddAnimName(gGRCommonStruct.sector.map_dobjs[7],
                                  sGRSectorArwingPatternAnims[pattern][1]);
        grSectorArwingAddAnimName(gGRCommonStruct.sector.map_dobjs[9],
                                  sGRSectorArwingPatternAnims[pattern][2]);
        grSectorArwingAddAnimName(gGRCommonStruct.sector.map_dobjs[11],
                                  sGRSectorArwingPatternAnims[pattern][3]);

        gGRCommonStruct.sector.arwing_flight_pattern = -1;
        map_gobj->flags = GOBJ_FLAG_NONE;
    }
}

/* grsector.c:1067-1085 grSectorProcUpdate 0x80107E08, verbatim. */
void grSectorProcUpdate(GObj *ground_gobj)
{
    switch (gGRCommonStruct.sector.arwing_status)
    {
    case nGRSectorArwingStatusSleep:
        grSectorArwingUpdateSleep();
        break;

    case nGRSectorArwingStatusWait:
        grSectorArwingUpdateWait();
        break;

    case nGRSectorArwingStatusPatrol:
        grSectorArwingUpdatePatrol();
        break;
    }
    func_ovl2_80107D50();
}

/* ---- the pack, and its lifetime (this file's header, divergence 1) ----
 *
 * The decomp needs no lifetime of its own here: its Arwing tree is a
 * pointer into `lbRelocGetForceStatusBufferFile(&llFoxSpecial3FileID)`, a
 * buffer the FIGHTER manager owns and the scene tears down with everything
 * else. This port's is a pack it loads itself -- Fox's Special3 is
 * `romdisk/efarwing.mdl`, baked for his own Arwing -- so
 * it owns one too, one load per scene that plays this stage.
 *
 * IT IS NOT KEPT IN `gGRCommonStruct.sector.map_file`, which is the
 * decomp's own field for exactly this pointer. That struct is cleared on
 * the way into every scene (src/dc/stage.c's grOverlayLoad, standing in
 * for overlay 2's bzero) while the memory a Fighter points at is not, so a
 * pointer kept there would be lost and the pack leaked once per battle.
 * src/dc/ftshadow.c's sFTShadowPack is the same shape and takes the same
 * clear; this is that, one file over.
 *
 * The cost is one duplicate: if Fox uses his Special3 on this stage, the
 * effect system's own row for `efarwing.mdl` (src/dc/efmanager.c's
 * sEFManagerModels) loads a second copy of the same file, because that
 * table is keyed by EFDesc and belongs to the effects. 34 KB, once, and
 * only when both are resident -- the alternative is a second ownership
 * path into the effect manager's table, which is worse. */
static Fighter sGRSectorArwingPack;
static sb32 sGRSectorArwingIsLoaded;

/* Called by src/dc/stage.c's grOverlayLoad, on the way into every scene:
 * give back the pack BEFORE the statics that name it go, the same order
 * ftShadowOverlayLoad uses. */
void grSectorOverlayLoad(void)
{
#ifndef FT_HOSTTEST
    fighter_release(&sGRSectorArwingPack);
    OVERLAY_CLEAR(sGRSectorArwingPack);
    OVERLAY_CLEAR(sGRSectorArwingIsLoaded);
#endif
}

/* efModelLoad's shape (src/dc/efmanager.c), one file over: one `Fighter`
 * per name, handed to fighter_load, and loaded at most once. */
static Fighter *grSectorArwingPackLoad(void)
{
    int pal_bank = 0;

    if ((sGRSectorArwingIsLoaded == FALSE) &&
        (fighter_load(&sGRSectorArwingPack, "efarwing.mdl", &pal_bank) != 0))
    {
        syDebugPrintf("grsector: efarwing.mdl did not load\n");

        return NULL;
    }
    sGRSectorArwingIsLoaded = TRUE;

    return &sGRSectorArwingPack;
}

/* The per-joint transform kinds -- what the port takes from
 * `dGRSectorArwingTransformKinds` beyond what the bake already installed.
 *
 * DIVERGES, and it is the port's standing trade (src/dc/grinishie.c's
 * second divergence, src/dc/gryoster.c's third) for every joint but one:
 * the bake installs the RpyR equivalent on every joint, so the Tra and
 * TraRotRpyR kinds the table names for joints 1-11 are already there and
 * are not re-applied. The 0x2C is NOT -- it is the RSP billboard, which
 * src/dc/objdisplay.c draws for the shield bubble and
 * Yoshi's egg, and which the Arwing's engine needs -- so it is added here,
 * on exactly the joints that ask for it (`grModelSetupGroundDObjs`'s real
 * semantics: tk2, where it is not Null, is a SECOND XObj of its own,
 * always at parameter 0 -- tk3 is tk1's parameter, not tk2's, which is why
 * it is not passed here).
 *
 * JOINT 0 IS THE EXCEPTION, and the reason this function exists rather
 * than being folded into the bake call. `dGRSectorArwingTransformKinds[0]`
 * is `{ 0x53, Null, 0x00 }` -- one whole-matrix XObj (see
 * grSectorArwingLaser3DFuncMatrixF) that replaces the joint's transform
 * outright rather than factoring into it, the same way 0x44/0x45 do
 * (src/dc/objdisplay.c). The bake's default triple has already given joint
 * 0 a Tra/RotRpyR/Sca it must not keep -- composing 0x53's whole matrix on
 * top of those would double the translation and fight the orientation --
 * so this clears `xobjs_num` back to 0 for that one joint before adding
 * the real kind. The three displaced XObjs are the pool's, not freed back;
 * this runs once per stage load, not per tic. */
static void grSectorArwingSetKinds(void)
{
    s32 i;
    DObj *root = gGRCommonStruct.sector.map_dobjs[0];

    if (root != NULL)
    {
        root->xobjs_num = 0;
        gcAddXObjForDObjFixed(root, dGRSectorArwingTransformKinds[0].tk1,
                              dGRSectorArwingTransformKinds[0].tk3);
    }

    for (i = 1; i < (s32)ARRAY_COUNT(dGRSectorArwingTransformKinds); i++)
    {
        DObj *dobj = gGRCommonStruct.sector.map_dobjs[i];

        if (dobj == NULL)
        {
            continue;
        }
        if (dGRSectorArwingTransformKinds[i].tk2 != nGCMatrixKindNull)
        {
            gcAddXObjForDObjFixed(dobj, dGRSectorArwingTransformKinds[i].tk2,
                                  0);
        }
    }
}

/* grsector.c:1087-1121 grSectorInitAll 0x80107E7C, verbatim but for the
 * tree and the scripts (see this file's header).
 *
 * `map_head` is still computed, and is still the decomp's arithmetic off
 * `map_nodes` -- but nothing in this port reads it, because every script
 * that used to be an offset from it is a name in the pack now. It is kept
 * so the struct's field means what the decomp's does. */
void grSectorInitAll(void)
{
    GObj *map_gobj;
    Fighter *pack;

    /* DIVERGES: the decomp's first line here is
     * `map_head = map_nodes - &llGRSectorMapMapHead`, and its last is
     * `weapon_head = gMPCollisionGroundData - &llGRSectorMapMapHeader` --
     * the two file bases every symbol in this file is an offset from. The
     * port has no pointer for either and never reads either field: the
     * scripts are names in the pack and the weapons' attributes are the
     * pack's own section, so both stay NULL. See src/dc/gryamabuki.c,
     * which made the same trade for `item_head`. */

    pack = grSectorArwingPackLoad();

    map_gobj = gcMakeGObjSPAfter(nGCCommonKindGround, NULL, nGCCommonLinkIDGround, GOBJ_PRIORITY_DEFAULT);

    gGRCommonStruct.sector.map_gobj = map_gobj;

    /* DIVERGES: dc_model_proc_display, not the decomp's
     * gcDrawDObjTreeDLLinksForGObj -- the same gap src/dc/itdisplay.c's
     * header describes for items; this GObj's tree is dc_model_add_dobjs's
     * below, and the raw walk alone never submits it to the PVR. */
    gcAddGObjDisplay(map_gobj, dc_model_proc_display, 6, GOBJ_PRIORITY_DEFAULT, ~0);

    if ((pack == NULL) ||
        (dc_model_add_dobjs(map_gobj, NULL, pack,
                            gGRCommonStruct.sector.map_dobjs) < 0))
    {
        /* No Arwing tree: the stage still plays, and the hazard is simply
         * not there. Loud rather than silent -- the same trade
         * src/dc/efmanager.c's own model path makes. */
        syDebugPrintf("grsector: no Arwing tree; the stage has no hazard\n");
    }
    else grSectorArwingSetKinds();

    gcAddGObjProcess(map_gobj, gcPlayAnimAll, nGCProcessKindFunc, 5);

    gGRCommonStruct.sector.arwing_status = 0;
    gGRCommonStruct.sector.arwing_flight_pattern = -1;
    gGRCommonStruct.sector.arwing_appear_timer = 600;
    gGRCommonStruct.sector.arwing_type_cycle = 3;
    gGRCommonStruct.sector.arwing_pilot_curr = -1;
    gGRCommonStruct.sector.arwing_pilot_prev = 0;
    gGRCommonStruct.sector.arwing_target_x = 0.0F;

    map_gobj->flags = GOBJ_FLAG_HIDDEN;

    /* The decomp's `gcAddDObjAnimJoint(map_dobjs[10], script, 0.0F)` --
     * NOT `grSectorArwingAddAnim`, which also clears the joint's
     * `is_anim_root` and plays the script a tic early. This one is the
     * tree's own default animation and `gcPlayAnimAll` below is what
     * starts it, exactly as the game does. The NULL guards are the
     * port's: a tree or a name that did not arrive leaves the joint
     * alone rather than dereferencing, which the game never has to
     * consider. */
    if ((gGRCommonStruct.sector.map_dobjs[10] != NULL) &&
        (stage_map_anim(GRSECTOR_ANIM_DEFAULT) != NULL))
    {
        gcAddDObjAnimJoint(gGRCommonStruct.sector.map_dobjs[10],
                           (AObjEvent32 *)stage_map_anim(GRSECTOR_ANIM_DEFAULT),
                           0.0F);
    }
    gcPlayAnimAll(map_gobj);
    mpCollisionSetYakumonoOffID(1);
}

/* grsector.c:1123-1131 grSectorMakeGround 0x80107FCC, verbatim. */
GObj* grSectorMakeGround(void)
{
    GObj *map_gobj = gcMakeGObjSPAfter(nGCCommonKindGround, NULL, nGCCommonLinkIDGround, GOBJ_PRIORITY_DEFAULT);

    grSectorInitAll();
    gcAddGObjProcess(map_gobj, grSectorProcUpdate, nGCProcessKindFunc, 4);

    return map_gobj;
}

/* grjungle.c -- gr/grcommon/grjungle.c, verbatim: Kongo Jungle's barrel
 * cannon, the port's second stage hazard.
 *
 * The cannon is a GObj on nGCCommonLinkIDGround with a two-state machine
 * (Move, Rotate) and no particle generator at all -- unlike Hyrule's
 * twister, this hazard IS a model, and it is the stage's own map file
 * that holds it. It waits 180-360 tics, then rolls 0.07 radians a tic
 * one way or the other for 90 tics, then waits again; a fighter it
 * touches is thrown by the game's own hazard path, not by anything here:
 * it registers grJungleTaruCannCheckGetDamageKind with
 * ftMainCheckAddGroundObstacle, ftMainSearchHitHazard calls it every tic
 * for every fighter, and the nGMHitEnvironmentTaruCann it answers with
 * lands in ftCommonTaruCannSetStatus (src/dc/ftcommontaru.c).
 *
 * The consumer half (ftCommonTaruCannSetStatus) is wired to the real
 * SetStatus. This file is the producer, so the hazard is whole.
 *
 * DIVERGES, two, and they are the reason this file is bigger than
 * grhyrule.c. The game's stage logic reaches its own objects through the map
 * file's address space:
 *
 *   map_head = gMPCollisionGroundData->map_nodes - &llGRJungleMapMapHead
 *   ... grModelSetupGroundDObjs(gobj, map_head + &llGRJungleMapMapHead,
 *                               NULL, dGRJungleTaruCannTransformKinds)
 *   ... gcAddAnimJointAll(gobj, map_head + &llGRJungleMapTaruCannDefault
 *                         AnimJoint, 0.0F)
 *
 * where every `llGRJungleMap*` is a linker symbol whose address is inside
 * the map data file the game loaded beside the map logic file. The port
 * has no such file: its stage is a pack (src/dc/stage.h), and the map
 * file is the MPK1 block in it -- the tree baked the way the stage's
 * visual layers are baked, and the AnimJoint scripts carried as the u32
 * words they are. So the two reads above become stage.h's
 * stage_bind_map_model and stage_map_anim, which is where the whole of
 * the divergence lives; everything else in this file is the decomp text.
 *
 * The second divergence is the anim-joint placement. The game's
 * gcAddAnimJointAll takes an ARRAY of scripts, one per joint in tree
 * order, and Kongo Jungle passes the map file's `mobjlink_0x0B1C` --
 * {NULL, TaruCannDefault, NULL} -- so the default animation lands on
 * JOINT 0, the cannon's root, and that is what moves the barrel across
 * the stage (the script is a 500-tic ping-pong between x = 3540 and
 * x = -3540). grJungleTaruCannGetPosition and ftCommonTaruCannProcPhysics
 * both read that same root, which is why the port's tree has to have the
 * map file's joint order. Fill and Shoot are separate: the decomp adds
 * them to `DObjGetStruct(gobj)->child`, joint 1, through
 * gcAddDObjAnimJoint. Both placements are the decomp's; only the way the
 * scripts are named has changed.
 *
 * dGRJungleTaruCannTransformKinds is ported verbatim and is dead today:
 * it is grModelSetupGroundDObjs' argument, and the port builds the tree
 * out of the baked pack instead. It is kept because it is the file's own
 * initialized data, and because the step that bakes a map object tree at
 * its transform kinds rather than at RotRpyR is the one that reads it.
 */

#include <gr/ground.h>
/* the decomp's grjungle.c reaches these through gr/ground.h ->
 * gr/grfunctions.h, which pulls every stage's own header; this port's
 * gr/ground.h is the collision files' shim and stops short of that, so
 * the one it needs is named here. */
#include <gr/grcommon/grjungle.h>
#include <ft/fighter.h>
#include <sc/scene.h>
#include <sys/taskman.h>
#include <sys/utils.h>

/* func_800269C0_275C0 -- not called by this file, but ftcommon.h is
 * where the port keeps the declaration it needs for the FGM calls the
 * consumer half makes, and grjungle.h itself drags in nothing that
 * declares nSYAudio*. Named for the same reason src/dc/grhyrule.c does. */
#include "ftcommon.h"

/* the port's own stage pack accessors: stage_map_anim and
 * stage_bind_map_model (see this file's header) */
#include "stage.h"

/* // // // // // // // // // // // //
 *                               //
 *       INITIALIZED DATA        //
 *                               //
 * // // // // // // // // // // // */

/* grjungle.c:12-16 0x8012EB50 dGRJungleTaruCannTransformKinds, verbatim:
 * the two XObj transform kinds grModelSetupGroundDObjs is given for this
 * tree. Dead in this port -- see this file's header. */
DObjTransformTypes dGRJungleTaruCannTransformKinds[] =
{
    { 0x28, nGCMatrixKindRotRpyR, 0x00 },
    { nGCMatrixKindTraRotRpyRSca, nGCMatrixKindNull, 0x00 }
};

/* // // // // // // // // // // // //
 *                               //
 *          ENUMERATORS          //
 *                               //
 * // // // // // // // // // // // */

enum grJungleTaruCannStatus
{
    nGRJungleTaruCannStatusMove,
    nGRJungleTaruCannStatusRotate
};

/* // // // // // // // // // // // //
 *                               //
 *           FUNCTIONS           //
 *                               //
 * // // // // // // // // // // // */

/* grjungle.c:37-44 0x80109CB0 grJungleTaruCannAddAnimOffset, verbatim but
 * for how the script is named: the game adds `map_head + offset` where
 * the offset is a linker symbol's address, and the port adds the script
 * stage_map_anim found by name. Everything after is the decomp's -- the
 * parse and the first play are what make the new script take effect on
 * the joint it was just attached to.
 *
 * A name the stage's map file has no script for adds nothing and plays
 * nothing, which is the same thing the game does with a NULL entry. */
void grJungleTaruCannAddAnimOffset(GObj *ground_gobj, const char *name)
{
#ifdef FT_HOSTTEST
    /* DIVERGES, and only in this build: see grJungleMakeTaruCann's own
     * note. The script is not attached, because the host cannot walk
     * one. */
    (void)ground_gobj;
    (void)name;
#else
    DObj *dobj = DObjGetStruct(ground_gobj)->child;
    AObjEvent32 *anim = stage_map_anim(name);

    if (anim == NULL)
    {
        return;
    }
    gcAddDObjAnimJoint(dobj, anim, 0.0F);
    gcParseDObjAnimJoint(dobj);
    gcPlayDObjAnimJoint(dobj);
#endif
}

/* grjungle.c:47-50 0x80109CFC grJungleTaruCannAddAnimFill, verbatim. The
 * fill is what the cannon plays when it has caught somebody: the barrel
 * squashes and stretches. Its one caller is
 * grJungleTaruCannCheckGetDamageKind below. */
void grJungleTaruCannAddAnimFill(GObj *ground_gobj)
{
    grJungleTaruCannAddAnimOffset(ground_gobj, "TaruCannFill");
}

/* grjungle.c:53-56 0x80109D20 grJungleTaruCannAddAnimShoot, verbatim. The
 * shoot is the cannon firing, and it has two callers, both in
 * ft/ftcommon/ftcommontarucann.c's ProcUpdate/ProcInterrupt. */
void grJungleTaruCannAddAnimShoot(GObj *ground_gobj)
{
    grJungleTaruCannAddAnimOffset(ground_gobj, "TaruCannShoot");
}

/* grjungle.c:59-71 0x80109D44 grJungleTaruCannUpdateMove, verbatim: the
 * wait between rolls, 180-360 tics, after which the cannon picks a
 * direction for the next one -- 0.07 radians a tic either way -- and a
 * fixed 90 tics to spend on it. The 90 is not the same as the wait, and
 * the roll does not start here: the status change is what
 * grJungleTaruCannUpdateRotate picks up next tic. */
void grJungleTaruCannUpdateMove(GObj *ground_gobj)
{
    gGRCommonStruct.jungle.tarucann_wait--;

    if (gGRCommonStruct.jungle.tarucann_wait == 0)
    {
        gGRCommonStruct.jungle.tarucann_status = nGRJungleTaruCannStatusRotate;

        gGRCommonStruct.jungle.tarucann_rotate_step = ((syUtilsRandUShort() % 2) != 0) ? 0.07F : -0.07F;

        gGRCommonStruct.jungle.tarucann_wait = 90;
    }
}

/* grjungle.c:74-89 0x80109DBC grJungleTaruCannUpdateRotate, verbatim. The
 * roll itself: the root DObj's z rotation steps every tic but the last,
 * and the last tic zeroes it and goes back to waiting.
 *
 * The zeroing is on the `wait == 0` arm and NOT on the step arm, so the
 * roll's total turn is 89 steps of 0.07 (6.23 radians, a little over a
 * full turn) and the cannon is level again the tic the status changes --
 * whatever angle it had accumulated, including the fraction of a step it
 * was on. That is what grJungleTaruCannGetRotate has to answer for
 * ftCommonTaruCannShootFighter's throw, so the small window where the
 * rotation is not the accumulated sum is a real one the target uses. */
void grJungleTaruCannUpdateRotate(GObj *ground_gobj)
{
    DObj *dobj = DObjGetStruct(ground_gobj);

    gGRCommonStruct.jungle.tarucann_wait--;

    if (gGRCommonStruct.jungle.tarucann_wait == 0)
    {
        gGRCommonStruct.jungle.tarucann_status = nGRJungleTaruCannStatusMove;

        gGRCommonStruct.jungle.tarucann_wait = syUtilsRandIntRange(180) + 180;

        dobj->rotate.vec.f.z = F_CST_DTOR32(0.0F);
    }
    else dobj->rotate.vec.f.z += gGRCommonStruct.jungle.tarucann_rotate_step;
}

/* grjungle.c:92-104 0x80109E34 grJungleTaruCannProcUpdate, verbatim: the
 * two-way dispatch, on the cannon GObj's own process at priority 4. */
void grJungleTaruCannProcUpdate(GObj *ground_gobj)
{
    switch (gGRCommonStruct.jungle.tarucann_status)
    {
    case nGRJungleTaruCannStatusMove:
        grJungleTaruCannUpdateMove(ground_gobj);
        break;

    case nGRJungleTaruCannStatusRotate:
        grJungleTaruCannUpdateRotate(ground_gobj);
        break;
    }
}

/* grjungle.c:107-131 0x80109E84 grJungleMakeTaruCann, verbatim but for
 * the two reads this file's header explains: the tree comes from
 * stage_bind_map_model instead of grModelSetupGroundDObjs over the map
 * file's DObjDesc, and the default animation from stage_map_anim instead
 * of map_head + a linker symbol.
 *
 * That second one needs the per-joint array the game passes. The game
 * hands gcAddAnimJointAll the address of the map file's
 * `mobjlink_0x0B1C` slot for TaruCannDefault, and what the walk reads
 * there is {TaruCannDefault, NULL} -- the script on JOINT 0, which is
 * the cannon's root and the DObj every position read in this file and in
 * ftcommontarucann.c goes through. Not joint 1, the quad the model
 * hangs off: an animation on that one would swing the picture while the
 * hazard and the fighter it captures stayed at the origin. This array is
 * that {script, NULL}, built from the name.
 *
 * What is the decomp's: the kind and link (nGCCommonKindGround, link 1 --
 * the same link Hyrule's twister and the stage's own ground GObj sit on),
 * the display on DL link 6, the gcPlayAnimAll process at priority 5 with
 * the state machine's at 4 (so the animation is parsed before the tic's
 * move is applied), the hazard registration, and the initial
 * state -- Move, a 180-360 tic wait and a zero rotate step, which is the
 * cannon level and waiting.
 *
 * `map_head` itself is gone: it existed only to be added to the linker
 * symbols, and stage_map_anim resolves by name instead. */
void grJungleMakeTaruCann(void)
{
    GObj *tarucann_gobj;

    gGRCommonStruct.jungle.tarucann_gobj = tarucann_gobj = gcMakeGObjSPAfter(nGCCommonKindGround, NULL, nGCCommonLinkIDGround, GOBJ_PRIORITY_DEFAULT);

    stage_bind_map_model(tarucann_gobj, 6);

#ifndef FT_HOSTTEST
    /* The port's one line, not the decomp's, and the same trade
     * src/dc/grhyrule.c's grHyruleTwisterInitVars makes: the target is
     * the only place the map file's own numbers can be checked at all,
     * and these are the numbers a serial log can hold it to. The tree is
     * the map file's DObjDesc (StageJungleFile3's, two joints), the
     * scripts are the three AnimJoints beside it in the map group
     * (TaruCannDefault/Fill/Shoot), and the weights are the map file's
     * MPItemWeights, which grJungleMap_item_weights declares as twenty
     * bytes beginning 0x50, 0x78, 0x32. Disc probe: `--serial`, grep
     * "tarucann:". */
    syDebugPrintf("tarucann: %d joint(s), %d script(s), %d weight(s)\n",
                  (int)stage_bound()->map_models[0].hd->joint_count,
                  (int)stage_bound()->map_anim_count,
                  (int)((stage_bound()->item_weights != NULL)
                        ? STAGE_ITEM_WEIGHTS_COUNT : 0));
#endif

#ifdef FT_HOSTTEST
    /* DIVERGES, and only in this build, for a reason this port has now
     * hit three times (src/dc/efmanager.c's efManagerLoadEffectBank,
     * the particle bank, and here): the game's DATA can be a
     * 32-bit machine's, not just its code.
     *
     * AObjEvent32 (sys/objtypes.h:94-108) is a union of f32, s32, u32
     * and -- the trouble -- void *p, and the interpreter walks a script
     * with AObjAnimAdvance, i.e. `p++`. On the N64, and on the SH-4,
     * that pointer is four bytes and the script's own words are four
     * bytes apart, so `p++` steps one word. On x86-64 the pointer makes
     * the union eight bytes and `p++` steps two, so gcParseDObjAnimJoint
     * reads every other word: a payload decodes as a command, the
     * `do { } while (anim_wait <= 0.0F)` never sees a positive wait, and
     * the parse spins forever. The scripts cannot be given to the host
     * interpreter at all.
     *
     * What the host build does instead is build the tree (above) and run
     * the state machine (below), and check the exported words through
     * stage_map_anim -- they are the ROM's, byte for byte, with their
     * one relocated pointer written in. What it cannot check is the
     * engine consuming them, which is the target's alone. */
#else
    {
        /* the map file's own joint array -- {TaruCannDefault, NULL} off
           the mobjlink slot the game passes -- has three entries; the
           tree has two, and the walk stops when the tree does */
        static AObjEvent32 *default_anim[2];

        default_anim[0] = stage_map_anim("TaruCannDefault");
        default_anim[1] = NULL;

        gcAddAnimJointAll(tarucann_gobj, default_anim, 0.0F);
        gcPlayAnimAll(tarucann_gobj);
    }
#endif

    gcAddGObjProcess(tarucann_gobj, grJungleTaruCannProcUpdate, nGCProcessKindFunc, 4);
    ftMainCheckAddGroundObstacle(tarucann_gobj, grJungleTaruCannCheckGetDamageKind);

    gGRCommonStruct.jungle.tarucann_status = nGRJungleTaruCannStatusMove;
    gGRCommonStruct.jungle.tarucann_wait = syUtilsRandIntRange(180) + 180;
    gGRCommonStruct.jungle.tarucann_rotate_step = F_CST_DTOR32(0.0F);
}

/* grjungle.c:134-139 0x80109FB4 grJungleMakeGround, verbatim: Kongo
 * Jungle's ground maker is one call and a NULL, because the cannon is
 * the whole of this stage's own ground logic -- there is no per-stage
 * ground GObj the way Hyrule has one. src/dc/stage.c's
 * grCommonSetupInitAll is what calls it, on the gkind test. */
GObj* grJungleMakeGround(void)
{
    grJungleMakeTaruCann();

    return NULL;
}

/* grjungle.c:142-190 0x80109FD8 grJungleTaruCannCheckGetDamageKind,
 * verbatim: the ground-hazard callback ftMainSearchHitHazard calls for
 * every fighter every tic. A fighter can be caught when it is off its own
 * cannon cooldown, is not already in one, and is not tarucann-immune --
 * and then only inside a 280-unit box around the cannon, measured
 * BOTH ways (unlike the twister's asymmetric one: the cannon is a small
 * object and its box is square).
 *
 * The extra test the twister does not have is the last one: only one
 * fighter may be in a given cannon at a time. It scans the fighter link
 * for anybody already in nFTCommonStatusTaruCann whose own remembered
 * cannon is THIS GObj -- so a second fighter can be caught by a
 * different cannon, and the same fighter cannot be caught by one it is
 * already inside. The captured fighter is remembered in
 * fp->status_vars.common.tarucann.tarucann_gobj, which is what makes
 * that comparison possible.
 *
 * The fill animation plays on the tic the catch is offered, not on the
 * tic the fighter actually enters: this function is what decides, and
 * ftCommonTaruCannSetStatus runs afterwards off the same tick's hazard
 * search. A fighter already in a hit status still triggers it, the same
 * way the twister's own box does. */
sb32 grJungleTaruCannCheckGetDamageKind(GObj *ground_gobj, GObj *fighter_gobj, s32 *kind)
{
    FTStruct *this_fp = ftGetStruct(fighter_gobj);
    f32 dist_x;
    f32 dist_y;

    if ((this_fp->tarucann_wait == 0) && (this_fp->status_id != nFTCommonStatusTaruCann) && !(this_fp->capture_immune_mask & FTCATCHKIND_MASK_TARUCANN))
    {
        DObj *gr_dobj = DObjGetStruct(ground_gobj);
        DObj *ft_dobj = DObjGetStruct(fighter_gobj);

        if (gr_dobj->translate.vec.f.x < ft_dobj->translate.vec.f.x)
        {
            dist_x = -(gr_dobj->translate.vec.f.x - ft_dobj->translate.vec.f.x);
        }
        else dist_x = gr_dobj->translate.vec.f.x - ft_dobj->translate.vec.f.x;

        if (gr_dobj->translate.vec.f.y < ft_dobj->translate.vec.f.y)
        {
            dist_y = -(gr_dobj->translate.vec.f.y - ft_dobj->translate.vec.f.y);
        }
        else dist_y = gr_dobj->translate.vec.f.y - ft_dobj->translate.vec.f.y;

        if ((dist_x < 280.0F) && (dist_y < 280.0F))
        {
            GObj *other_gobj = gGCCommonLinks[nGCCommonLinkIDFighter];

            while (other_gobj != NULL)
            {
                if (other_gobj != fighter_gobj)
                {
                    FTStruct *other_fp = ftGetStruct(other_gobj);

                    if ((other_fp->status_id == nFTCommonStatusTaruCann) && (ground_gobj == other_fp->status_vars.common.tarucann.tarucann_gobj))
                    {
                        return FALSE;
                    }
                }
                other_gobj = other_gobj->link_next;
            }
            *kind = nGMHitEnvironmentTaruCann;

            grJungleTaruCannAddAnimFill(ground_gobj);

            return TRUE;
        }
    }
    return FALSE;
}

/* grjungle.c:193-196 0x80110A104 grJungleTaruCannGetPosition, verbatim:
 * where the cannon is. Its one caller is ft/ftcomputer.c's hazard
 * avoidance (src/dc/ftcomputer.c). */
void grJungleTaruCannGetPosition(Vec3f *pos)
{
    *pos = DObjGetStruct(gGRCommonStruct.jungle.tarucann_gobj)->translate.vec.f;
}

/* grjungle.c:199-202 0x80110A12C grJungleTaruCannGetRotate, verbatim: how
 * far the cannon has rolled, which is the root DObj's z rotation, and
 * what ftCommonTaruCannShootFighter aims its throw with. */
f32 grJungleTaruCannGetRotate(void)
{
    return DObjGetStruct(gGRCommonStruct.jungle.tarucann_gobj)->rotate.vec.f.z;
}

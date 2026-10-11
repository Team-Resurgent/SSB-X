/* grbonus3.c -- gr/grbonus/grbonus3.c, the tenth stage-logic file and
 * the first that is not a VS stage at all: Race to the Finish.
 *
 * The course is a tunnel with a gate at the end. Three things run on
 * it, and all three are two dozen lines:
 *
 *  - THE BUMPERS. Four of them, standing still where the map file says,
 *    each following a script of its own. They are ordinary
 *    `nITKindGBumper` items -- the same kind Peach's Castle puts on the
 *    field -- and itGBumperMakeItem's `gkind == nGRKindCastle` test is
 *    what makes THESE ones the weaker object (src/dc/itgbumper.c:195).
 *
 *  - THE BARREL. One `nITKindTaruBomb` (src/dc/ittarubomb.c)
 *    every 180 tics, dropped at the map's one
 *    `nMPMapObjKind1PGameBonus3TaruBomb` position, which is at the top
 *    of the shaft the course runs under.
 *
 *  - THE GATE. A fighter STANDING on an `nMPMaterialDetect` floor is
 *    the whole win condition -- the same material Saffron City's gate
 *    pad is made of (src/dc/gryamabuki.c:147), read the same way, off
 *    the fighter's own floor flags. Reaching it raises COMPLETE! and
 *    queues nSYAudioFGMBonusComplete.
 *
 * WHAT ENDS THE COURSE IS THE BANNER'S OWN FREEZE, not a flag this file
 * sets. ifCommonAnnounceCompleteInitInterface installs
 * ifCommonBonusInterfaceProcUpdate, which calls
 * `gcFuncGObjAll(ifCommonBattleInterfacePauseGObj, 0)` -- and that stops
 * the ground GObj this file's two processes hang on along with
 * everything else. So grBonus3FinishProcUpdate needs no latch and has
 * none, in the decomp or here.
 *
 * DIVERGES, three:
 *
 *  1. grBonus3InitHeaders is NOT ported, for grInishieInitHeaders'
 *     reason exactly (src/dc/grinishie.c's second divergence): its two
 *     lines turn `gMPCollisionGroundData` into the `map_head` and
 *     `item_head` bases every other function in the decomp adds a
 *     linker symbol to, and the port has neither the pointer nor the
 *     symbols. Both fields stay NULL, and both readers are gone with
 *     it -- `map_head` was grBonus3MakeBumpers' two tables (divergence
 *     2 below) and `item_head` was the TaruBomb ITDesc's item list
 *     head, which diverges to
 *     `&gITManagerCommonData` (src/dc/ittarubomb.c's first divergence).
 *
 *  2. grBonus3MakeBumpers reads the PACK. The game walks a DObjDesc
 *     array and a parallel AnimJoint array at `map_head +
 *     &llGRBonus3MapBumpersDObjDesc` / `...BumpersAnimJoint` until the
 *     descs' `DOBJ_ARRAY_MAX` terminator; the port reads the four
 *     positions and four scripts the BMP1 block carries
 *     (tools/export/ssb_stageexport.py). The exporter
 *     skips the array's first entry exactly the way this function skips
 *     it -- the `anim_joint++, dobjdesc++` before the loop -- so
 *     `bumper_count` IS the game's own iteration count, and the order
 *     is the array's.
 *
 *     Note what does NOT come from the pack: the bumper's own model.
 *     `nITKindGBumper` has one already (romdisk/itgbumper.mdl), and the
 *     game does not read one here either -- it reads the desc for its
 *     `translate` and nothing else.
 *
 *  3. `gcAddDObjAnimJoint` is `#ifdef FT_HOSTTEST`-guarded, the same
 *     trade grJungleMakeTaruCann and grInishieScaleUpdateStep make:
 *     `union AObjEvent32` is eight bytes on x86-64, so the interpreter
 *     walks a real script twice as fast as it should and never
 *     terminates. The host checks the four scripts through the pack's
 *     own `bumper_anims`; the target is the only place a bumper
 *     actually moves -- and all four of this course's do. Each has a
 *     script of its own (`Bumpers1` through `Bumpers4`), each a TraY/
 *     TraX interpolation that loops back on itself, so the four are
 *     swinging obstacles and not scenery.
 *
 * NOTHING IS CUT. grBonus3MakeGround returns NULL the way the decomp's
 * does -- this is the one stage in the game with no ground GObj of its
 * own, because its two actors each make their own.
 */
#include <gr/ground.h>
#include <gr/grbonus/grbonus3.h>
#include <gr/grvars.h>            /* GRBonusGroundVarsBonus3 */
#include <ft/fighter.h>
#include <if/ifcommon.h>          /* ifCommonBattleEndAddSoundQueueID */
#include <it/item.h>              /* the bumpers and the barrel are ITEMS */
#include <mp/mpdef.h>             /* nMPMapObjKind*, nMPMaterialDetect */
#include <mp/mpcollision.h>
#include <sc/scene.h>
#include <sys/objanim.h>          /* gcAddDObjAnimJoint, gcPlayAnimAll */

#ifdef FT_HOSTTEST
#include "hoststubs.h"
#else
#include <kos.h>
#endif

#include "ifcommon.h"             /* ifCommonAnnounceCompleteInitInterface */
#include "stage.h"                /* stage_bound: the BMP1 block */

/* // // // // // // // // // // // //
 *                                   //
 *           FUNCTIONS               //
 *                                   //
 * // // // // // // // // // // // // */

/* grbonus3.c:8-12 grBonus3InitHeaders 0x8010B4D0 is NOT ported. See this
 * file's first divergence. */

/* grbonus3.c:15-40 grBonus3MakeBumpers 0x8010B508, the pack's positions
 * in place of the map file's DObjDesc walk. See the second and third
 * divergences.
 *
 * `ITEM_FLAG_PARENT_GROUND` is what makes these the STAGE's items rather
 * than a fighter's: itManagerMakeItemSetupCommon's NULL parent plus that
 * flag is the same pair grInishieMakePakkun and grYamabukiGateMakeMonster
 * pass. */
void grBonus3MakeBumpers(void)
{
    Stage *st = stage_bound();
    GObj *item_gobj;
    Vec3f vel;
    s32 i;

    vel.x = vel.y = vel.z = 0.0F;

    if (st == NULL)
    {
        return;
    }
    for (i = 0; i < (s32)st->bumper_count; i++)
    {
        AObjEvent32 *anim_joint = NULL;

        /* the decomp's `*anim_joint != NULL` test, one indirection
         * earlier: a still bumper is -1 here where it is a NULL entry in
         * the map file's parallel array. */
        if ((st->bumper_anim[i] >= 0) &&
            (st->bumper_anim[i] < (int8_t)st->bumper_anim_count))
        {
            anim_joint = st->bumper_anims[st->bumper_anim[i]];
        }
        item_gobj = itManagerMakeItemSetupCommon(NULL, nITKindGBumper, &st->bumpers[i], &vel, ITEM_FLAG_PARENT_GROUND);

        if ((anim_joint != NULL) && (item_gobj != NULL))
        {
#ifdef FT_HOSTTEST
            /* DIVERGES, and only in this build: NEITHER line runs. The
             * attach is out for the reason the third divergence gives,
             * and gcPlayAnimAll has to go with it -- its first act on
             * each joint is gcParseDObjAnimJoint/gcParseMObjMatAnim
             * Joint, which walk whatever script is already on the item's
             * own model, and the host cannot walk one either. This is
             * stage_bind_map_cloud's trade (hosttest_ft.c), reached from
             * the other side. */
            (void)anim_joint;
#else
            gcAddDObjAnimJoint(DObjGetStruct(item_gobj), anim_joint, 0.0F);
            gcPlayAnimAll(item_gobj);
#endif
        }
    }

#ifndef FT_HOSTTEST
    /* The port's own line, the same trade grinishie.c's `pakkun:` makes.
     * The four positions are the pack's and a bumper that failed to make
     * is silent otherwise -- there is no handle kept for any of them.
     * Disc probe: `--serial`, grep "bonus3:". */
    syDebugPrintf("bonus3: %d bumper(s), %d script(s)\n",
                  (int)st->bumper_count, (int)st->bumper_anim_count);
#endif
}

/* grbonus3.c:43-56 grBonus3TaruBombProcUpdate 0x8010B5F0, verbatim: one
 * barrel every 180 tics at the position grBonus3TaruBombMakeActor read
 * off the map, falling from rest. */
void grBonus3TaruBombProcUpdate(GObj *ground_gobj)
{
    Vec3f vel;

    if (gGRCommonStruct.bonus3.tarubomb_make_wait == 0)
    {
        vel.x = vel.y = vel.z = 0.0F;

        itManagerMakeItemSetupCommon(NULL, nITKindTaruBomb, &gGRCommonStruct.bonus3.tarubomb_make_pos, &vel, ITEM_FLAG_PARENT_GROUND);

        gGRCommonStruct.bonus3.tarubomb_make_wait = 180;
    }
    gGRCommonStruct.bonus3.tarubomb_make_wait--;
}

/* grbonus3.c:59-77 grBonus3TaruBombMakeActor 0x8010B660, verbatim,
 * error arm and all.
 *
 * The count is what decides whether the stage can run: a Race to the
 * Finish whose map carries anything other than exactly ONE
 * nMPMapObjKind1PGameBonus3TaruBomb object loops forever printing, which
 * is the DECOMP'S OWN arm and is kept rather than softened -- the same
 * call grInishieMakePowerBlock keeps. The stage as shipped has one. */
void grBonus3TaruBombMakeActor(void)
{
    s32 pos_ids;

    gcAddGObjProcess(gcMakeGObjSPAfter(nGCCommonKindGround, NULL, nGCCommonLinkIDGround, GOBJ_PRIORITY_DEFAULT), grBonus3TaruBombProcUpdate, nGCProcessKindFunc, 4);

    if (mpCollisionGetMapObjCountKind(nMPMapObjKind1PGameBonus3TaruBomb) != 1)
    {
        while (TRUE)
        {
            syDebugPrintf("Too many barrels!\n");
            scManagerRunPrintGObjStatus();
        }
    }
    mpCollisionGetMapObjIDsKind(nMPMapObjKind1PGameBonus3TaruBomb, &pos_ids);
    mpCollisionGetMapObjPositionID(pos_ids, &gGRCommonStruct.bonus3.tarubomb_make_pos);

    gGRCommonStruct.bonus3.tarubomb_make_wait = 180;

#ifndef FT_HOSTTEST
    /* The `bonus3:` line's other half: where the barrels come from. */
    syDebugPrintf("bonus3: barrel at map object %d, (%.0f, %.0f)\n",
                  (int)pos_ids,
                  gGRCommonStruct.bonus3.tarubomb_make_pos.x,
                  gGRCommonStruct.bonus3.tarubomb_make_pos.y);
#endif
}

/* grbonus3.c:80-89 grBonus3FinishProcUpdate 0x8010B700, verbatim but for
 * the NULL guard.
 *
 * THE WIN CONDITION. `gSCManagerSceneData.player` is the human's slot,
 * and that fighter standing on an `nMPMaterialDetect` floor is the gate
 * -- nothing here names a position, the same way
 * grYamabukiGateCheckPlayersNear names none. The guard is the port's:
 * this stage can be booted into a VS battle by DB_BOOT_STAGE with the
 * human's slot empty, which the game never has to consider. */
void grBonus3FinishProcUpdate(GObj *ground_gobj)
{
    GObj *fighter_gobj = gSCManagerBattleState->players[gSCManagerSceneData.player].fighter_gobj;
    FTStruct *fp;

    if (fighter_gobj == NULL)
    {
        return;
    }
    fp = ftGetStruct(fighter_gobj);

    if ((fp->ga == nMPKineticsGround) && ((fp->coll_data.floor_flags & MAP_VERTEX_MAT_MASK) == nMPMaterialDetect))
    {
        ifCommonAnnounceCompleteInitInterface(nSYAudioVoiceAnnounceComplete);
        ifCommonBattleEndAddSoundQueueID(nSYAudioFGMBonusComplete);
    }
}

/* grbonus3.c:92-95 grBonus3FinishMakeActor 0x8010B784, verbatim. The
 * decomp writes the process kind as the literal `1`, which is
 * nGCProcessKindFunc (sys/objdef.h:38). */
void grBonus3FinishMakeActor(void)
{
    gcAddGObjProcess(gcMakeGObjSPAfter(nGCCommonKindGround, NULL, nGCCommonLinkIDGround, GOBJ_PRIORITY_DEFAULT), grBonus3FinishProcUpdate, nGCProcessKindFunc, 4);
}

/* grbonus3.c:98-106 grBonus3MakeGround 0x8010B7C8, verbatim but for the
 * grBonus3InitHeaders call this file's first divergence removes.
 *
 * It returns NULL and means it: the two actors above each made a ground
 * GObj of their own, and there is no third for the stage itself. */
GObj* grBonus3MakeGround(void)
{
    grBonus3MakeBumpers();
    grBonus3TaruBombMakeActor();
    grBonus3FinishMakeActor();

    return NULL;
}

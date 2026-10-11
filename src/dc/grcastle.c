/* grcastle.c -- gr/grcommon/grcastle.c, verbatim but for the map reads:
 * Peach's Castle, the seventh stage hazard this port has ported.
 *
 * The stage's whole hazard is ONE BUMPER on a moving carriage. The
 * carriage is not a model: `grCastleInitAll` makes a ground GObj with a
 * single EMPTY DObj on it, hands that DObj the stage's own anim script
 * through `gcAddAnimJointAll(ground_gobj, gMPCollisionGroundData->
 * map_nodes, 0.0F)`, and grCastleBumperProcUpdate copies the swept x
 * onto the Bumper's own position every tic. The script -- the map file's
 * one script, `dStageCastleFile3_AnimJointRoot`'s entry, twelve words of
 * `TraX` over 2400 frames -- ramps 0 -> -1050 -> +1050 -> 0, so the
 * Bumper slides across the castle and back forever. Nothing about the
 * stage's picture moves.
 *
 * That is why this stage's map file has NO TREE to bake, and it is the
 * only one of the nine shaped that way: `map_nodes` is an `AObjEvent32 *`
 * ROOT rather than a DObjDesc array, so the port's pack for Castle
 * carries one script and zero objects (tools/export/ssb_stageexport.py's
 * `anims_only` arm, the `AnimJointRoot` name, and read_map_object's own
 * comment). The bumper itself is `it/itground/itgbumper.c`, ported
 * alongside this file, which supplies its animation script.
 *
 * DIVERGES, two:
 *
 *  - `map_head`, the field grCastleInitAll computes as
 *    `map_nodes - &llGRCastleMapMapHead`, stays NULL. The port has no
 *    linker symbols to subtract and no function that adds to them; the
 *    field is written in the decomp's own order and never read, here or
 *    there.
 *
 *  - `gcAddAnimJointAll(ground_gobj, map_nodes, 0.0F)` becomes
 *    `gcAddDObjAnimJoint(dobj, stage_map_anim("AnimJointRoot"), 0.0F)`,
 *    which is the same attachment for a one-joint tree and the shape
 *    grJungleTaruCannAddAnimOffset already uses. It is
 *    `#ifdef FT_HOSTTEST`-guarded for that function's own reason:
 *    `union AObjEvent32` is eight bytes on x86-64, so the anim
 *    interpreter walks a script twice as fast as it should and never
 *    terminates. The host checks the exported words instead; the target
 *    is the only place the Bumper actually slides.
 */
#include <gr/ground.h>
/* the decomp's grcastle.c reaches these through gr/ground.h ->
 * gr/grfunctions.h; this port's gr/ground.h is the collision files' shim
 * and stops short of that, so the ones it needs are named here, the same
 * way grpupupu.c and grinishie.c name their own. */
#include <gr/grcommon/grcastle.h>
#include <gr/grvars.h>            /* GRCommonGroundVarsCastle */
#include <it/item.h>              /* itManagerMakeItemSetupCommon */
#include <mp/mpdef.h>             /* nMPMapObjKindBumper */
#include <mp/mpcollision.h>
#include <sc/scene.h>
#include <sys/objanim.h>          /* gcAddDObjAnimJoint, gcPlayAnimAll */
#include <sys/debug.h>            /* syDebugPrintf, the castle: probe */

#include "stage.h"

/* // // // // // // // // // // // //
 *                               //
 *           FUNCTIONS           //
 *                               //
 * // // // // // // // // // // // */

/* grcastle.c:12-22 0x8010B340 grCastleBumperProcUpdate, verbatim. The
 * whole of the stage's motion: the Bumper's x is the carriage's swept x
 * PLUS the position the map object tagged, so a stage with its Bumper
 * somewhere other than the sweep's centre still gets the same travel.
 * The guard is the decomp's and it is load-bearing: the item can be gone
 * -- picked up, blown up, or not made at all on a stage whose map has no
 * Bumper object -- while this process keeps running. */
void grCastleBumperProcUpdate(GObj *ground_gobj)
{
    Vec3f *ground_pos = &DObjGetStruct(ground_gobj)->translate.vec.f;

    if (gGRCommonStruct.castle.bumper_gobj != NULL)
    {
        Vec3f *bumper_pos = &DObjGetStruct(gGRCommonStruct.castle.bumper_gobj)->translate.vec.f;

        bumper_pos->x = ground_pos->x + gGRCommonStruct.castle.bumper_pos.x;
    }
}

/* grcastle.c:25-58 0x8010B378 grCastleInitAll, verbatim but for the map
 * read above. Note the ORDER: the updater is installed before the DObj
 * exists, so it runs on a tree it did not build, and the sweep is
 * attached and played once before the Bumper is made -- which is what
 * gives the bumper its first position. */
void grCastleInitAll(void)
{
    void *map_head;
    GObj *ground_gobj;
    Vec3f yakumono_pos;
    Vec3f vel;
    s32 pos_id;
    DObj *dobj;

    /* DIVERGES: NULL, see this file's header. */
    gGRCommonStruct.castle.map_head = map_head = NULL;

    ground_gobj = gcMakeGObjSPAfter(nGCCommonKindGround, NULL, nGCCommonLinkIDGround, GOBJ_PRIORITY_DEFAULT);

    gcAddGObjProcess(ground_gobj, grCastleBumperProcUpdate, nGCProcessKindFunc, 4);

    dobj = gcAddDObjForGObj(ground_gobj, NULL);
    dobj->translate.vec.f.x = dobj->translate.vec.f.y = dobj->translate.vec.f.z = 0.0F;

    gcAddGObjProcess(ground_gobj, gcPlayAnimAll, nGCProcessKindFunc, 5);

    /* DIVERGES: the sweep, by name; host-guarded, see this file's
     * header. A pack with no such script attaches nothing, which is what
     * the game does with a NULL entry too. */
#ifdef FT_HOSTTEST
    (void)dobj;
#else
    {
        AObjEvent32 *anim = stage_map_anim("AnimJointRoot");

        if (anim != NULL)
        {
            gcAddDObjAnimJoint(dobj, anim, 0.0F);
        }
    }
#endif
    gcPlayAnimAll(ground_gobj);

    mpCollisionGetMapObjIDsKind(nMPMapObjKindBumper, &pos_id);
    mpCollisionGetMapObjPositionID(pos_id, &yakumono_pos);

    gGRCommonStruct.castle.bumper_pos = yakumono_pos;

    vel.x = 0.0F;
    vel.y = 0.0F;
    vel.z = 0.0F;

    gGRCommonStruct.castle.bumper_gobj = itManagerMakeItemSetupCommon(NULL, nITKindGBumper, &yakumono_pos, &vel, ITEM_FLAG_PARENT_GROUND);

#ifndef FT_HOSTTEST
    /* The port's own line, not the decomp's, and the same trade
     * src/dc/grjungle.c's `tarucann:` and src/dc/itmanager.c's `itspawn:`
     * make: this stage's output is a picture, and a serial log is the
     * only place its numbers can be checked at all. What it says is the
     * whole chain, in order, in one line: the sweep the pack carries
     * (its command count -- thirteen words for the decomp's twelve plus
     * the walker's own End), the carriage's own sweep at tic 0, the
     * position the map object tagged for the Bumper, and the item that
     * came out of it. Disc probe: `--serial`, grep "castle:".
     *
     * THE ONE THING THIS LINE CANNOT SHOW is the sweep actually
     * advancing: the script moves the carriage over 2400 frames and
     * nothing here has a counter to prove it, which would mean a static
     * this file has no other reason to keep. The host test holds the
     * other end -- grCastleBumperProcUpdate's arithmetic, and the
     * script's own words read back out of the pack. */
    {
        Stage *st = stage_bound();
        const u32 *w = (const u32 *)
            ((st != NULL) ? stage_map_anim("AnimJointRoot") : NULL);

        syDebugPrintf("castle: %d object(s), sweep %s (0x%08X), "
                      "bumper at (%.0f, %.0f), gjobj %p\n",
                      (int)st->map_object_count,
                      (w != NULL) ? "yes" : "NO",
                      (w != NULL) ? w[0] : 0u,
                      yakumono_pos.x, yakumono_pos.y,
                      (void *)gGRCommonStruct.castle.bumper_gobj);
    }
#endif
}

/* grcastle.c:61-66 0x8010B4AC grCastleMakeGround, verbatim: the stage's
 * entry point, and the one src/dc/stage.c calls when the bound stage is
 * nGRKindCastle.
 *
 * It returns NULL because the game's does -- dGRMainSetupProcMakeList's
 * entries are void-returning GObj* makers whose result grMainSetupMake-
 * Ground ignores, and this one's only GObj is the carriage, which it
 * keeps in gGRCommonStruct.castle rather than handing back. */
GObj* grCastleMakeGround(void)
{
    grCastleInitAll();

    return NULL;
}

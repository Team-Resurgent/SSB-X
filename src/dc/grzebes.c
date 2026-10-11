/* grzebes.c -- gr/grcommon/grzebes.c, verbatim: Planet Zebes' acid, the
 * port's third stage hazard and the second that is a model.
 *
 * The acid is a GObj on nGCCommonLinkIDGround with a four-state machine
 * (Wait, Normal, Shake, Rise). It waits a drawn number of tics, shakes
 * for eighteen of them while the screen quakes, rises over 240 to a new
 * level drawn from dGRZebesAcidAttributes, and settles -- then picks the
 * next attribute row and starts again. A fighter that falls below the
 * acid's own surface is handed to the fighter system as
 * nGMHitEnvironmentAcid with this stage's GRAttackColl, not as a status:
 * the acid damages and knocks back, it does not capture.
 *
 * That consumer half is in ft/ftmain.c:
 * ftMainSetDamageHitStats has the Acid arm (`fp->acid_wait
 * = 30` and the fire FGM), the damage-stats arm, and
 * ftMainCheckAddGroundHazard itself, whose callback shape is
 * `sb32 (*)(GObj *, GObj *, GRAttackColl **, s32 *)`.
 *
 * DIVERGES, and all of them are the same one Kongo Jungle has: the game reaches its map file through its
 * address space,
 *
 *   map_head = gMPCollisionGroundData->map_nodes - &llGRZebesMapAcidDObjDesc
 *   gcSetupCustomDObjs(gobj, map_head + &llGRZebesMapAcidDObjDesc, ...)
 *   gcAddMObjAll(gobj, lbRelocGetFileData(MObjSub***, map_head,
 *                                         &llGRZebesMapAcidMObjSub))
 *   gcAddAnimAll(gobj, lbRelocGetFileData(AObjEvent32**, map_head,
 *                                         &llGRZebesMapAcidAnimJoint), ...)
 *   ... = lbRelocGetFileData(GRAttackColl*, gMPCollisionGroundData -
 *                            &llGRZebesMapMapHeader,
 *                            &llGRZebesMapAcidGRAttackColl)
 *
 * and this port's stage is a pack (src/dc/stage.h) whose MPK1 block
 * carries the same data: the tree baked (with the object's MObjSub chain
 * folded into its batches, which is what makes the acid a rolling sea
 * rather than a flat plane), the AnimJoint as the u32 words it is, and
 * the GRAttackColl as its seven s32s. So the two gcSetup* calls and the
 * lbRelocGetFileData reads become stage_bind_map_model, stage_map_anim
 * and stage_bound()->attack_coll.
 *
 * The MatAnimJoint rides in the pack as the acid's one MObj `alt`
 * which stage_bind_map_model installs as the MObjs it
 * builds, so the script that scrolls the surface plays under the same
 * gcPlayAnimAll as the AnimJoint. dGRZebesAcidAttributes, the sixteen
 * behaviour rows, is initialized data and ported verbatim.
 */

#include <gr/ground.h>
/* the decomp's grzebes.c reaches these through gr/ground.h ->
 * gr/grfunctions.h; this port's gr/ground.h is the collision files'
 * shim and stops short of that, so the one it needs is named here. */
#include <gr/grcommon/grzebes.h>
#include <ft/fighter.h>
#include <ef/effect.h>
#include <sc/scene.h>
#include <sys/utils.h>

#include "ftcommon.h"
#include "stage.h"

/* // // // // // // // // // // // //
 *                               //
 *       INITIALIZED DATA        //
 *                               //
 * // // // // // // // // // // // */

/* grzebes.c:13-31 0x8012EA60 dGRZebesAcidAttributes, verbatim: sixteen
 * rows of { how long to wait, the random spread around it, how much more
 * the next wait may draw, and the level the acid rises to }. The level is
 * an absolute y; everything else is tics. */
GRZebesAcid dGRZebesAcidAttributes[] =
{
    { 1200, 60, 70, -3600.0F },
    {  180, 60, 70, -1000.0F },
    {   60, 60, 70,  -200.0F },
    {   60, 60, 70,  1800.0F },
    {   60, 60, 70, -3600.0F },
    {  120,  0,  0,  2600.0F },
    {   30,  0,  0, -1000.0F },
    { 1200, 60, 70,  -500.0F },
    {  600, 60, 70,  -400.0F },
    {  100, 60, 70,   800.0F },
    { 1200, 60, 70,  1200.0F },
    {   60,  0,  0, -1000.0F },
    {   60, 60, 70,     0.0F },
    {   60, 60, 70, -1000.0F },
    {  120, 60, 70,   500.0F },
    {  200, 60, 70, -3000.0F }
};

/* // // // // // // // // // // // //
 *                               //
 *          ENUMERATORS          //
 *                               //
 * // // // // // // // // // // // */

enum grZebesStatus
{
    nGRZebesAcidStatusWait,
    nGRZebesAcidStatusNormal,
    nGRZebesAcidStatusShake,
    nGRZebesAcidStatusRise
};

/* // // // // // // // // // // // //
 *                               //
 *           FUNCTIONS           //
 *                               //
 * // // // // // // // // // // // */

/* grzebes.c:54-58 0x80108020 grZebesAcidSetLevelStep, verbatim: how much
 * the acid moves each tic of its 240-tic rise. The target level is the
 * row's own plus a random 0-250, and the step is the whole distance over
 * 240 -- so a longer rise is a faster one, and a rise to a level already
 * reached is no rise at all. */
void grZebesAcidSetLevelStep(void)
{
    gGRCommonStruct.zebes.acid_level_step =
    ((dGRZebesAcidAttributes[gGRCommonStruct.zebes.acid_attr_id].acid_level + (syUtilsRandFloat() * 250.0F)) - gGRCommonStruct.zebes.acid_level_curr) / 240.0F;
}

/* grzebes.c:61-68 0x80108088 grZebesAcidSetRandomWait, verbatim: the
 * wait before the next shake, drawn as base + min + rand(max - min).
 * Note the row's second and third fields are both added, so a row's
 * "random spread" is a floor and its maximum is a further spread on top
 * -- row 0 waits 1260 to 1330, not 1200 to 1260. */
void grZebesAcidSetRandomWait(void)
{
    s32 index = gGRCommonStruct.zebes.acid_attr_id;

    gGRCommonStruct.zebes.acid_level_wait = dGRZebesAcidAttributes[index].acid_wait_base +
                                          dGRZebesAcidAttributes[index].acid_random_min +
                                          syUtilsRandIntRange(dGRZebesAcidAttributes[index].acid_random_max - dGRZebesAcidAttributes[index].acid_random_min);
}

/* grzebes.c:71-115 0x801080EC grZebesMakeAcid, verbatim but for the
 * map-file reads (see this file's header): the tree, its MObjSub
 * materials and its AnimJoint come out of the stage pack's MPK1 block
 * through stage_bind_map_model and stage_map_anim, and the GRAttackColl
 * off the bound stage instead of through four linker-symbol offsets off
 * map_head.
 *
 * What is the decomp's: the kind and link, the display on DL link 12,
 * the gcPlayAnimAll process at priority 5 (the state machine's, on the
 * ground GObj, is at 4), the initial state -- Wait, with the acid parked
 * at the LAST row's level and attr_id 0, so the first thing it does is
 * rise from the lowest level to whatever row 0 draws -- and the y write
 * that puts it there. */
GObj* grZebesMakeAcid(void)
{
    GObj *map_gobj;

    map_gobj = gcMakeGObjSPAfter(nGCCommonKindGround, NULL, nGCCommonLinkIDGround, GOBJ_PRIORITY_DEFAULT);
    gGRCommonStruct.zebes.map_gobj = map_gobj;

    stage_bind_map_model(map_gobj, 12);

#ifdef FT_HOSTTEST
    /* DIVERGES, and only in this build: the script is not attached,
     * because the host cannot walk one. `union AObjEvent32` is eight
     * bytes on x86-64, so gcParseDObjAnimJoint's `p++` steps two words
     * instead of one and the parse never terminates -- grJungleMakeTaruCann
     * has the full account. */
#else
    {
        /* The map file's own per-DObj table, as the array gcAddAnimAll
         * walks: one entry per joint of the acid's tree, NULL where the
         * group names nothing.
         *
         * NOT attaching this leaves the tree dead: the tree
         * was built, its materials installed and gcPlayAnimAll
         * registered, and no script was ever handed to it, so every
         * joint stayed at the transform the pack baked and the sea was a
         * static slab. Only the stage's own file can start one, which is
         * why it has to be here and not in stage.c.
         *
         * The MatAnimJoint the decomp passes as this call's third
         * argument is already on the MObjs stage_bind_map_model built --
         * see this file's header -- so NULL here adds nothing and
         * removes nothing. */
        AObjEvent32 *anim[STAGE_MAP_JOINTS_MAX];
        int i, attached = 0;

        stage_map_anim_array("Acid", anim);

        gcAddAnimAll(map_gobj, anim, NULL, 0.0F);
        gcPlayAnimAll(map_gobj);

        /* The port's one line, not the decomp's, in the same spirit as
         * the `acid:` line above and for the reason that line cannot
         * give: it counts what the PACK holds, and ledger row 28 was a
         * stage whose pack held a script nothing ever handed over. This
         * counts what the TREE holds after the attach -- a joint with an
         * empty anim_joint is a joint back at its bind pose. Disc probe:
         * `--serial`, grep "acid anim:". */
        for (i = 0; i < (int)stage_bound()->map_models[0].hd->joint_count; i++)
        {
            DObj *dobj = stage_map_joint(0, i);

            if (dobj != NULL && dobj->anim_joint.event32 != NULL)
            {
                attached++;
            }
        }
        syDebugPrintf("acid anim: %d of %d joint(s) carry a script\n",
                      attached,
                      (int)stage_bound()->map_models[0].hd->joint_count);
    }
#endif

    gGRCommonStruct.zebes.acid_status = nGRZebesAcidStatusWait;
    gGRCommonStruct.zebes.acid_level_curr = dGRZebesAcidAttributes[ARRAY_COUNT(dGRZebesAcidAttributes) - 1].acid_level;
    gGRCommonStruct.zebes.acid_attr_id = 0;

    gGRCommonStruct.zebes.attack_coll = stage_bound()->attack_coll;

#ifndef FT_HOSTTEST
    /* The port's one line, not the decomp's, and the same trade
     * src/dc/grhyrule.c and src/dc/grjungle.c make: the target is the
     * only place the map file's own numbers can be checked at all. The
     * tree is the group's `DObjDesc Acid` (two joints), the script is
     * `AnimJoint Acid`, and the descriptor is `GRAttackColl Acid`'s seven
     * s32s, whose first is nGMHitEnvironmentAcid and whose damage the
     * decomp's own source gives as 16. Disc probe: `--serial`, grep
     * "acid:". */
    syDebugPrintf("acid: %d joint(s), %d script(s), coll kind %d damage %d\n",
                  (int)stage_bound()->map_models[0].hd->joint_count,
                  (int)stage_bound()->map_anim_count,
                  (int)((gGRCommonStruct.zebes.attack_coll != NULL)
                        ? gGRCommonStruct.zebes.attack_coll->kind : -1),
                  (int)((gGRCommonStruct.zebes.attack_coll != NULL)
                        ? gGRCommonStruct.zebes.attack_coll->damage : -1));
#endif

    grZebesAcidSetRandomWait();

    DObjGetStruct(map_gobj)->translate.vec.f.y = gGRCommonStruct.zebes.acid_level_curr;

    return map_gobj;
}

/* grzebes.c:118-124 0x80108240 grZebesAcidUpdateWait, verbatim: the acid
 * does not move until the match leaves its Wait status -- the same gate
 * Hyrule's twister opens with. */
void grZebesAcidUpdateWait(void)
{
    if (gSCManagerBattleState->game_status != nSCBattleGameStatusWait)
    {
        gGRCommonStruct.zebes.acid_status = nGRZebesAcidStatusNormal;
    }
}

/* grzebes.c:127-136 0x80108268 grZebesAcidUpdateRumble, verbatim: a
 * screen quake every eighteen tics for as long as Shake and Rise run.
 *
 * Note the order -- the quake fires when the countdown IS zero and only
 * then is the countdown decremented, so the first quake is on the tic the
 * caller zeroes rumble_wait (Shake's own first tic) and the counter runs
 * 0, 17, 16, ... Rather than 18, 17, ... That is the decomp's, kept. */
void grZebesAcidUpdateRumble(void)
{
    if (gGRCommonStruct.zebes.rumble_wait == 0)
    {
        efManagerQuakeMakeEffect(0);

        gGRCommonStruct.zebes.rumble_wait = 18;
    }
    gGRCommonStruct.zebes.rumble_wait--;
}

/* grzebes.c:139-149 0x801082B4 grZebesAcidUpdateNormal, verbatim: the
 * settled wait. Its last tic goes to Shake with an eighteen-tic shake and
 * the rumble counter zeroed -- which is what makes the next Rumble call
 * fire immediately. */
void grZebesAcidUpdateNormal(void)
{
    gGRCommonStruct.zebes.acid_level_wait--;

    if (gGRCommonStruct.zebes.acid_level_wait == 0)
    {
        gGRCommonStruct.zebes.acid_status = nGRZebesAcidStatusShake;
        gGRCommonStruct.zebes.acid_level_wait = 18;
        gGRCommonStruct.zebes.rumble_wait = 0;
    }
}

/* grzebes.c:152-164 0x801082EC grZebesAcidUpdateShake, verbatim: eighteen
 * tics of quaking in place, then Rise -- 240 tics, with the step drawn
 * now rather than at the top. */
void grZebesAcidUpdateShake(void)
{
    gGRCommonStruct.zebes.acid_level_wait--;

    if (gGRCommonStruct.zebes.acid_level_wait == 0)
    {
        gGRCommonStruct.zebes.acid_status = nGRZebesAcidStatusRise;
        gGRCommonStruct.zebes.acid_level_wait = 240;

        grZebesAcidSetLevelStep();
    }
    grZebesAcidUpdateRumble();
}

/* grzebes.c:167-187 0x8010833C grZebesAcidUpdateRise, verbatim: the acid
 * climbs, the DObj's y following acid_level_curr, and at the end picks
 * the next attribute row -- wrapping to 0 after the sixteenth -- and
 * draws a fresh wait. The quake runs all the way through it. */
void grZebesAcidUpdateRise(void)
{
    gGRCommonStruct.zebes.acid_level_curr += gGRCommonStruct.zebes.acid_level_step;

    DObjGetStruct(gGRCommonStruct.zebes.map_gobj)->translate.vec.f.y = gGRCommonStruct.zebes.acid_level_curr;

    gGRCommonStruct.zebes.acid_level_wait--;

    if (gGRCommonStruct.zebes.acid_level_wait == 0)
    {
        gGRCommonStruct.zebes.acid_status = nGRZebesAcidStatusNormal;
        gGRCommonStruct.zebes.acid_attr_id++;

        if (gGRCommonStruct.zebes.acid_attr_id >= ARRAY_COUNT(dGRZebesAcidAttributes))
        {
            gGRCommonStruct.zebes.acid_attr_id = 0;
        }
        grZebesAcidSetRandomWait();
    }
    grZebesAcidUpdateRumble();
}

/* grzebes.c:190-210 0x801083C4 grZebesProcUpdate, verbatim: the four-way
 * dispatch, on the stage's own ground GObj at priority 4. */
void grZebesProcUpdate(GObj *ground_gobj)
{
    switch (gGRCommonStruct.zebes.acid_status)
    {
    case nGRZebesAcidStatusWait:
        grZebesAcidUpdateWait();
        break;

    case nGRZebesAcidStatusNormal:
        grZebesAcidUpdateNormal();
        break;

    case nGRZebesAcidStatusShake:
        grZebesAcidUpdateShake();
        break;

    case nGRZebesAcidStatusRise:
        grZebesAcidUpdateRise();
        break;
    }
}

/* grzebes.c:213-222 0x80108448 grZebesMakeGround, verbatim: unlike Kongo
 * Jungle's, this stage's ground maker returns a real GObj -- the one the
 * state machine hangs off, separate from the acid's own -- and registers
 * the acid as a ground HAZARD (ftMainCheckAddGroundHazard, the
 * GRAttackColl callback shape) rather than a ground obstacle. */
GObj* grZebesMakeGround(void)
{
    GObj *ground_gobj = gcMakeGObjSPAfter(nGCCommonKindGround, NULL, nGCCommonLinkIDGround, GOBJ_PRIORITY_DEFAULT);
    GObj *acid_gobj = grZebesMakeAcid();

    gcAddGObjProcess(ground_gobj, grZebesProcUpdate, nGCProcessKindFunc, 4);
    ftMainCheckAddGroundHazard(acid_gobj, grZebesAcidCheckGetDamageKind);

    return ground_gobj;
}

/* grzebes.c:225-242 0x801084AC grZebesAcidCheckGetDamageKind, verbatim:
 * the ground-hazard callback ftMainSearchHitHazard calls for every
 * fighter every tic. The test is not a box, as the twister's and the
 * cannon's are -- the acid is a surface, so a fighter is caught by being
 * BELOW it. The surface's own height is the acid GObj's y plus its
 * CHILD's y, because the root is the level the state machine writes and
 * the child is where the mesh actually sits relative to it.
 *
 * The gate is fp->acid_wait, the thirty-tic cooldown SetDamageHitStats
 * sets on every acid hit -- the same field, which is why this is the
 * first hazard the port has whose callback can be tested without a
 * second fighter. */
sb32 grZebesAcidCheckGetDamageKind(GObj *ground_gobj, GObj *fighter_gobj, GRAttackColl **gr_attack_coll, s32 *kind)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if (fp->acid_wait == 0)
    {
        DObj *dobj = DObjGetStruct(ground_gobj);

        if (DObjGetStruct(fighter_gobj)->translate.vec.f.y < (dobj->translate.vec.f.y + dobj->child->translate.vec.f.y))
        {
            *gr_attack_coll = gGRCommonStruct.zebes.attack_coll;
            *kind = nGMHitEnvironmentAcid;

            return TRUE;
        }
    }
    return FALSE;
}

/* grzebes.c:245-250 0x8010850C grZebesAcidGetLevelInfo, verbatim: where
 * the acid is and how fast it is moving, which is nothing while it is
 * settled. Its callers are the camera (gmCameraUpdateAcidZoom, which
 * keeps the acid in frame) and the CPU (ftcomputer.c's acid avoidance);
 * both are ported. */
void grZebesAcidGetLevelInfo(f32 *current, f32 *step)
{
    *current = gGRCommonStruct.zebes.acid_level_curr;

    *step = (gGRCommonStruct.zebes.acid_status == nGRZebesAcidStatusRise) ? gGRCommonStruct.zebes.acid_level_step : 0.0F;
}

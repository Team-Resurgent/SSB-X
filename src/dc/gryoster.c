/* gryoster.c -- gr/grcommon/gryoster.c, verbatim: Yoshi's Island's
 * clouds, the port's fourth stage hazard.
 *
 * Three clouds sit on the stage's own yakumono lines (line IDs 1, 2 and
 * 3). Each is a GObj whose tree comes from the map file's `MapHead`
 * SKELETON -- five DObjDesc entries, no display lists at all -- with the
 * cloud itself GRAFTED under each of the skeleton's three child joints:
 * a bare `DisplayList Cloud` (four vertices) plus the material chain
 * `MObjSub _4B8_`. The port carries both as the stage pack's MPK1 block:
 * the skeleton as the map object, the cloud as the GRAFT pack, attached
 * by src/dc/stage.c's stage_bind_map_cloud.
 *
 * The hazard is weight, not damage. A fighter standing on a cloud's line
 * (grYosterCheckFighterCloudStand) adds 5.0 of `pressure` a tic up to
 * 180, and the cloud's DObj sinks by exactly that much off its
 * `altitude`; a fighter who gets off frees it at the same 5.0 a tic. Hold
 * one down past its pressure timer and it EVAPORATES -- the line goes
 * off, a vapor puff plays at the cloud's own position, and 180 tics later
 * it comes back solid.
 *
 * DIVERGES, three, and the first two are the map-file one:
 *
 *  - The skeleton's own AnimJoint (`AnimJoint _1E0_`) is a PER-DOBJ
 *    TABLE of three scripts (`154_StageYosterFile3.c`:
 *    `AnimJoint_0x01E4[3] = { 0x1F0, 0x21C, 0x264 }`), each a scale
 *    pulse, 1.0 to 1.129 and back over 45 tics, looping. The port's MPK1
 *    block carries a block's scripts as a LIST, which
 *    `stage_map_anim_array` turns back into the `AObjEvent32 **` the
 *    game's own walk wants, and it is attached as the game attaches it.
 *    It shows nothing, in the game or here: the skeleton is built with
 *    the translate-only kind (the decomp's own comment says to make it
 *    TraRotRpyRSca "to see cloud scale animation"), and the cloud under
 *    each joint is a kind-48 billboard that keeps only the stack's
 *    position. Before the port built the game's kinds (grYosterSetKinds)
 *    its clouds pulsed in size and in place, which the game's never do.
 *
 *  - The cloud's material is animated rather than baked (the pack's
 *    FPackMObjs with two `alt`s), because grYosterUpdateCloudSolid gates
 *    on `dobj[0]->mobj->anim_wait`. That gate is REAL here -- the MObjs
 *    exist and their scripts run -- so this is not a divergence after
 *    all, but it is worth saying why the acid's material is not carried
 *    the same way: Zebes has no such gate.
 *
 *  - The cloud is the pack's own DObj (dc_model_add_dobjs), not the
 *    game's bare display-list child, so it is built with the port's
 *    Tra/RotRpyR/Sca set and grYosterSetKinds trades that for the game's
 *    `gcAddXObjForDObjFixed(cloud_dobj, nGCMatrixKindTra/48)` pair.
 *
 *    The graft also submits itself: its GObj's display proc is the
 *    game's, which only records matrices, and all nine clouds share the
 *    one Stage.graft_model -- so dc_model_graft_display makes each
 *    graft's DObj draw the pack the moment the walk reaches it (the
 *    effect arm's trick for the same reason). Without it the clouds
 *    record a matrix for a draw that never comes and the platforms are
 *    invisible.
 *
 * dGRYosterCloudLineIDs is the file's own initialized data. Its
 * companion, `dGRYosterCloudMatAnimJoints = { &Solid, &Evaporate }`, is
 * the pair of MatAnimJoints this file's own grYosterUpdateCloudAnim
 * picks between -- and the port's pack carries exactly that pair as its
 * two `alt`s, so the table is the two ALTERNATES' indices.
 */

#include <gr/ground.h>
/* the decomp's gryoster.c reaches these through gr/ground.h ->
 * gr/grfunctions.h; this port's gr/ground.h is the collision files'
 * shim and stops short of that, so the one it needs is named here. */
#include <gr/grcommon/gryoster.h>
#include <ft/fighter.h>
#include <lb/lbparticle.h>
#include <sc/scene.h>
#include <sys/utils.h>

#include "ftcommon.h"
#include "lbpartex.h"          /* lbpTexLoadBank */
#include "stage.h"

/* // // // // // // // // // // // //
 *                               //
 *       INITIALIZED DATA        //
 *                               //
 * // // // // // // // // // // // */

/* The status enum, which the decomp keeps below its data: moved up here
 * because dGRYosterCloudMatAnimJoints is initialized with it. */
enum grYosterCloudStatus
{
    nGRYosterCloudStatusSolid,
    nGRYosterCloudStatusEvaporate
};


/* gryoster.c:12 0x8012EB20 dGRYosterCloudMatAnimJoints, verbatim but for
 * what it holds: the game's two entries are `&llGRYosterMapCloudSolid
 * MatAnimJoint` and `&LLGRYosterMapCloudEvaporateMatAnimJoint`, and the
 * port's pack carries the same two scripts as its `alt`s -- so the table
 * is which alternate each state wants, in the decomp's own order. */
s32 dGRYosterCloudMatAnimJoints[] = { nGRYosterCloudStatusSolid, nGRYosterCloudStatusEvaporate };

/* gryoster.c:15 0x8012EB28 dGRYosterCloudLineIDs, verbatim: the three
 * yakumono lines the clouds ride. */
u8 dGRYosterCloudLineIDs[] = { 0x1, 0x2, 0x3 };

/* // // // // // // // // // // // //
 *                               //
 *          ENUMERATORS          //
 *                               //
 * // // // // // // // // // // // */

/* // // // // // // // // // // // //
 *                               //
 *           FUNCTIONS           //
 *                               //
 * // // // // // // // // // // // */

/* gryoster.c:36-47 0x80108550 grYosterCloudVaporMakeEffect, verbatim: the
 * puff a cloud leaves when it evaporates, one generator out of the
 * stage's own particle bank at the position the caller picked. */
LBGenerator* grYosterCloudVaporMakeEffect(Vec3f *pos)
{
    LBGenerator *gn = lbParticleMakeGenerator(gGRCommonStruct.yoster.particle_bank_id, 0);

    if (gn != NULL)
    {
        gn->pos.x = pos->x;
        gn->pos.y = pos->y;
        gn->pos.z = pos->z;
    }
    return gn;
}

/* gryoster.c:50-69 0x801085A8 grYosterCheckFighterCloudStand, verbatim:
 * is anybody standing on this cloud's own line? The walk is over the
 * fighter link, and the test is grounded AND the floor line's own
 * yakumono id -- mpCollisionSetDObjNoID, which is what asks a line which
 * yakumono owns it. A fighter in the air is not standing on anything. */
sb32 grYosterCheckFighterCloudStand(s32 cloud_id)
{
    GObj *fighter_gobj = gGCCommonLinks[nGCCommonLinkIDFighter];
    s32 line_id = dGRYosterCloudLineIDs[cloud_id];

    while (fighter_gobj != NULL)
    {
        FTStruct *fp = ftGetStruct(fighter_gobj);

        if (fp->ga == nMPKineticsGround)
        {
            if ((fp->coll_data.floor_line_id != -2) && (mpCollisionSetDObjNoID(fp->coll_data.floor_line_id) == line_id))
            {
                return TRUE;
            }
        }
        fighter_gobj = fighter_gobj->link_next;
    }
    return FALSE;
}

/* gryoster.c:72-135 0x80108634 grYosterUpdateCloudSolid, verbatim. The
 * first gate is the cloud's own material animation: while its first
 * DObj's MObj is still playing, none of the rest runs. Then the line is
 * turned on if it is off (this is what makes the cloud a floor), and the
 * pressure timer decides between evaporating and accumulating. A fighter
 * on the cloud pushes `pressure` up 5.0 a tic to 180 and holds a
 * 120-tic timer; one who is not lets it fall 5.0 a tic to 0 and clears
 * the timer. The DObj sinks by exactly `pressure` below its `altitude`,
 * and the yakumono line follows it -- so the cloud's collision IS its
 * position, which is why the line's id is enough to find standers. */
void grYosterUpdateCloudSolid(s32 cloud_id)
{
    Vec3f pos;
    DObj *dobj;

    if (gGRCommonStruct.yoster.clouds[cloud_id].dobj[0]->mobj->anim_wait == AOBJ_ANIM_NULL)
    {
        if (gGRCommonStruct.yoster.clouds[cloud_id].is_cloud_line_active == FALSE)
        {
            mpCollisionSetYakumonoOnID(dGRYosterCloudLineIDs[cloud_id]);

            gGRCommonStruct.yoster.clouds[cloud_id].is_cloud_line_active = TRUE;
        }
        if (gGRCommonStruct.yoster.clouds[cloud_id].pressure_timer == 0)
        {
            gGRCommonStruct.yoster.clouds[cloud_id].status = nGRYosterCloudStatusEvaporate;
            gGRCommonStruct.yoster.clouds[cloud_id].anim_id = nGRYosterCloudStatusEvaporate;
            gGRCommonStruct.yoster.clouds[cloud_id].evaporate_wait = 180;

            pos = DObjGetStruct(gGRCommonStruct.yoster.clouds[cloud_id].gobj)->translate.vec.f;

            pos.x += (-750.0F);
            pos.y += (-350.0F);

            grYosterCloudVaporMakeEffect(&pos);

            func_800269C0_275C0(nSYAudioFGMYosterCloudVapor);
        }
        else
        {
            if (grYosterCheckFighterCloudStand(cloud_id) != FALSE)
            {
                if (gGRCommonStruct.yoster.clouds[cloud_id].pressure_timer == -1)
                {
                    gGRCommonStruct.yoster.clouds[cloud_id].pressure_timer = 120;
                }
                gGRCommonStruct.yoster.clouds[cloud_id].pressure += 5.0F;

                if (gGRCommonStruct.yoster.clouds[cloud_id].pressure > 180.0F)
                {
                    gGRCommonStruct.yoster.clouds[cloud_id].pressure = 180.0F;
                }
            }
            else
            {
                gGRCommonStruct.yoster.clouds[cloud_id].pressure_timer = -1;
                gGRCommonStruct.yoster.clouds[cloud_id].pressure -= 5.0F;

                if (gGRCommonStruct.yoster.clouds[cloud_id].pressure < 0.0F)
                {
                    gGRCommonStruct.yoster.clouds[cloud_id].pressure = 0.0F;
                }
            }
            if (gGRCommonStruct.yoster.clouds[cloud_id].pressure_timer > 0)
            {
                gGRCommonStruct.yoster.clouds[cloud_id].pressure_timer--;
            }
        }
    }
    dobj = DObjGetStruct(gGRCommonStruct.yoster.clouds[cloud_id].gobj);
    dobj->translate.vec.f.y = gGRCommonStruct.yoster.clouds[cloud_id].altitude - gGRCommonStruct.yoster.clouds[cloud_id].pressure;

    mpCollisionSetYakumonoPosID(dGRYosterCloudLineIDs[cloud_id], &dobj->translate.vec.f);
}

/* gryoster.c:138-154 0x80108814 grYosterUpdateCloudEvaporate, verbatim:
 * the line goes OFF the tic the cloud starts evaporating (so nothing is
 * standing on it any more), and 180 tics later the cloud is solid again
 * with its pressure cleared to zero -- which puts it back at its own
 * altitude on the next Solid tic. */
void grYosterUpdateCloudEvaporate(s32 cloud_id)
{
    if (gGRCommonStruct.yoster.clouds[cloud_id].is_cloud_line_active != FALSE)
    {
        mpCollisionSetYakumonoOffID(dGRYosterCloudLineIDs[cloud_id]);

        gGRCommonStruct.yoster.clouds[cloud_id].is_cloud_line_active = FALSE;
    }
    if (gGRCommonStruct.yoster.clouds[cloud_id].evaporate_wait == 0)
    {
        gGRCommonStruct.yoster.clouds[cloud_id].status = nGRYosterCloudStatusSolid;
        gGRCommonStruct.yoster.clouds[cloud_id].anim_id = nGRYosterCloudStatusSolid;
        gGRCommonStruct.yoster.clouds[cloud_id].pressure_timer = -1;
        gGRCommonStruct.yoster.clouds[cloud_id].pressure = 0.0F;
    }
    else gGRCommonStruct.yoster.clouds[cloud_id].evaporate_wait--;
}

/* gryoster.c:157-175 0x80108890 grYosterUpdateCloudAnim, verbatim but
 * for how the script is reached: the game adds
 * `map_head + dGRYosterCloudMatAnimJoints[anim_id]` through
 * lbCommonAddTreeDObjsAnimAll, which is the map file's per-DObj table of
 * MatAnimJoints, and the port re-points the MObjs the graft already has
 * at the matching `alt` of the pack's own two. `anim_id` of -1 means
 * "nothing to change", which is what the initial state leaves it at
 * until the first transition. */
void grYosterUpdateCloudAnim(s32 cloud_id)
{
    s8 anim_id = gGRCommonStruct.yoster.clouds[cloud_id].anim_id;

    if (anim_id != -1)
    {
        s32 i;

        for (i = 0; i < ARRAY_COUNT(gGRCommonStruct.yoster.clouds[cloud_id].dobj); i++)
        {
            DObj *dobj = gGRCommonStruct.yoster.clouds[cloud_id].dobj[i];

            stage_cloud_set_anim(dobj, dGRYosterCloudMatAnimJoints[anim_id]);
        }
        gGRCommonStruct.yoster.clouds[cloud_id].anim_id = -1;
    }
}

/* gryoster.c:178-196 0x80108960 grYosterProcUpdate, verbatim: the two-way
 * dispatch per cloud, and the animation update for every cloud whether
 * its state changed or not (which is how a transition's script lands the
 * tic after the state does). */
void grYosterProcUpdate(GObj *ground_gobj)
{
    s32 i;

    for (i = 0; i < ARRAY_COUNT(gGRCommonStruct.yoster.clouds); i++)
    {
        switch (gGRCommonStruct.yoster.clouds[i].status)
        {
        case nGRYosterCloudStatusSolid:
            grYosterUpdateCloudSolid(i);
            break;

        case nGRYosterCloudStatusEvaporate:
            grYosterUpdateCloudEvaporate(i);
            break;
        }
        grYosterUpdateCloudAnim(i);
    }
}

/* The transform kinds the game builds the clouds with, on DObjs the port
 * built with its own Tra/RotRpyR/Sca set: gcSetupCustomDObjs(...,
 * nGCMatrixKindTra, Null, Null) for the skeleton and the Tra/48 pair
 * gcAddXObjForDObjFixed appends to each cloud. The port's set goes back
 * to the pool, as gcEjectDObj would return it, and adding a kind resets
 * the vectors it claims (sys/objman.c), so a skeleton joint gets its
 * bind vectors back the way gcSetupCustomDObjs writes them after its
 * kinds; the cloud keeps the defaults, as the game's does. */
static void grYosterSetKinds(DObj *dobj, u8 tk1, u8 tk2, sb32 is_keep_vectors)
{
    Vec3f translate = dobj->translate.vec.f;
    Vec3f rotate = dobj->rotate.vec.f;
    Vec3f scale = dobj->scale.vec.f;
    s32 i;

    for (i = 0; i < ARRAY_COUNT(dobj->xobjs); i++)
    {
        if (dobj->xobjs[i] != NULL)
        {
            gcSetXObjPrevAlloc(dobj->xobjs[i]);
            dobj->xobjs[i] = NULL;
        }
    }
    dobj->xobjs_num = 0;

    gcAddXObjForDObjFixed(dobj, tk1, 0);

    if (tk2 != nGCMatrixKindNull)
    {
        gcAddXObjForDObjFixed(dobj, tk2, 0);
    }
    if (is_keep_vectors != FALSE)
    {
        dobj->translate.vec.f = translate;
        dobj->rotate.vec.f = rotate;
        dobj->scale.vec.f = scale;
    }
}

/* gryoster.c:199-257 0x801089F4 grYosterInitAll, verbatim but for the map
 * reads (see this file's header): the skeleton comes from
 * stage_bind_map_model and the graft from stage_bind_map_cloud, each then
 * given the game's transform kinds by grYosterSetKinds. The particle
 * bank's load is the same
 * efParticleGetLoadBankID call Hyrule's twister and Kongo Jungle's cannon
 * make -- lGRYosterParticle{Script,Texture}Bank{Lo,Hi} are in
 * src/game/ssb64/particlebanks.ld.
 *
 * What is the decomp's: three GObs, one per cloud, each with a display
 * on DL link 6 and a gcPlayAnimAll process at priority 5; the root's
 * translate taken from the yakumono DObj its own line ID names, which is
 * what `altitude` is; the three cloud joints walked as siblings of the
 * skeleton's first child; the initial state (Solid, line on, pressure
 * zero, anim_id Solid so the first UpdateCloudAnim attaches a script);
 * and the bank load last. */
void grYosterInitAll(void)
{
    DObj *cloud_dobj;
    GObj *map_gobj;
    DObj *coll_dobj;
    s32 i, j;

    for (i = 0; i < ARRAY_COUNT(gGRCommonStruct.yoster.clouds); i++)
    {
        map_gobj = gcMakeGObjSPAfter(nGCCommonKindGround, NULL, nGCCommonLinkIDGround, GOBJ_PRIORITY_DEFAULT);

        gGRCommonStruct.yoster.clouds[i].gobj = map_gobj;

        stage_bind_map_model(map_gobj, 6);

        {
            DObj *dobj = DObjGetStruct(map_gobj);

            /* gcGetTreeDObjNext: every joint of the skeleton */
            while (dobj != NULL)
            {
                grYosterSetKinds(dobj, nGCMatrixKindTra, nGCMatrixKindNull, TRUE);

                dobj = gcGetTreeDObjNext(dobj);
            }
        }

#ifdef FT_HOSTTEST
        /* DIVERGES, and only in this build: the script is not attached,
         * because the host cannot walk one -- grJungleMakeTaruCann's note
         * on `union AObjEvent32` being eight bytes on x86-64 is the whole
         * reason. */
#else
        {
            /* The skeleton's own per-DObj AnimJoint, which is a divergence from
             * the game's one script table: `AnimJoint
             * _1E0_` is a TABLE of three scripts, one per cloud joint --
             * 154_StageYosterFile3.c's `AnimJoint_0x01E4[3]` -- and the
             * port's MPK1 block carried ONE script per named block, so
             * the table had nowhere to go and every cloud stood at its
             * bind size. The export carries a block's scripts
             * as a LIST and stage_map_anim_array turns it back into the
             * `AObjEvent32 **` the game's own walk wants, which is what
             * makes this attach possible at all.
             *
             * The decomp's own comment on the setup call says "Make this
             * nGCMatrixKindTraRotRpyRSca to see cloud scale animation" --
             * the pulse is a SCALE track, and the skeleton's kind is
             * translate-only (grYosterSetKinds above), so the script
             * runs and nothing on screen shows it, as in the game. */
            AObjEvent32 *anim[STAGE_MAP_JOINTS_MAX];
            int j, attached = 0;

            stage_map_anim_array("_1E0_", anim);

            gcAddAnimJointAll(map_gobj, anim, 0.0F);

            /* The port's one line, not the decomp's, and the only place
             * the array can be checked at all: the host build cannot walk
             * a script and this file's own array assertion is the one
             * that segfaults the suite. What it counts is what
             * the TREE holds after the attach -- a skeleton joint with an
             * empty anim_joint is a cloud back at its bind size. Disc
             * probe: `--serial`, grep "cloud anim:". */
            for (j = 0; j < (int)stage_bound()->map_models[0].hd->joint_count; j++)
            {
                DObj *dobj = stage_map_joint(0, j);

                if (dobj != NULL && dobj->anim_joint.event32 != NULL)
                {
                    attached++;
                }
            }
            syDebugPrintf("cloud anim: %d of %d skeleton joint(s) carry a "
                          "script\n", attached,
                          (int)stage_bound()->map_models[0].hd->joint_count);
        }
#endif

        coll_dobj = DObjGetStruct(map_gobj);
        coll_dobj->translate.vec.f = gMPCollisionYakumonoDObjs->dobjs[dGRYosterCloudLineIDs[i]]->translate.vec.f;

        gGRCommonStruct.yoster.clouds[i].altitude = coll_dobj->translate.vec.f.y;

        coll_dobj = coll_dobj->child;

        for (j = 0; j < ARRAY_COUNT(gGRCommonStruct.yoster.clouds[i].dobj); j++, coll_dobj = coll_dobj->sib_next)
        {
            cloud_dobj = stage_bind_map_cloud(map_gobj, coll_dobj, nGRYosterCloudStatusSolid);
            gGRCommonStruct.yoster.clouds[i].dobj[j] = cloud_dobj;

            if (cloud_dobj != NULL)
            {
                grYosterSetKinds(cloud_dobj, nGCMatrixKindTra, nGCMatrixKind48, FALSE);
            }
        }
        gcPlayAnimAll(map_gobj);

        gGRCommonStruct.yoster.clouds[i].status = nGRYosterCloudStatusSolid;
        gGRCommonStruct.yoster.clouds[i].anim_id = nGRYosterCloudStatusSolid;
        gGRCommonStruct.yoster.clouds[i].pressure_timer = -1;
        gGRCommonStruct.yoster.clouds[i].is_cloud_line_active = FALSE;
        gGRCommonStruct.yoster.clouds[i].pressure = 0.0F;

        mpCollisionSetYakumonoOnID(dGRYosterCloudLineIDs[i]);
    }
#ifdef FT_HOSTTEST
    /* DIVERGES, and only in this build, for the reason
     * grHyruleTwisterInitVars gives: lbParticleSetupBankID walks a bank
     * whose arrays are 32-bit pointers, so on x86-64 it reads and writes
     * at twice the stride. The host test builds bank 0 itself out of the
     * same two files -- host_load_particle_bank -- and leaves the id
     * here at zero, "a bank that at least exists". */
    gGRCommonStruct.yoster.particle_bank_id = 0;
#else
    gGRCommonStruct.yoster.particle_bank_id = efParticleGetLoadBankID(&lGRYosterParticleScriptBankLo, &lGRYosterParticleScriptBankHi, &lGRYosterParticleTextureBankLo, &lGRYosterParticleTextureBankHi);
    /* DIVERGES: the textures, bound by name as src/dc/mntitle.c binds
     * the title's. efParticleGetLoadBankID walks the bank's scripts and
     * its N64 textures, but the PVR draws from the pre-converted
     * gryoster.txp, and lbpdraw.c skips any particle whose image has no
     * bound PVR texture -- so without this a cloud's vapour was never
     * drawn at all. */
    if (lbpTexLoadBank(gGRCommonStruct.yoster.particle_bank_id, "gryoster") != 0)
    {
        syDebugPrintf("yoster: gryoster.txp did not load\n");
    }
#endif

#ifndef FT_HOSTTEST
    /* The port's one line, not the decomp's, and the same trade
     * src/dc/grzebes.c makes: the target is the only place the pack's own
     * numbers can be checked at all. The map tree is `DObjDesc MapHead`'s
     * four bare joints, the graft is `DisplayList Cloud` plus its MObjSub
     * chain, and the count is what proves the graft landed -- every
     * cloud's first DObj must exist AND carry the animated material
     * grYosterUpdateCloudSolid gates on, or the gate reads a NULL mobj.
     * Disc probe: `--serial`, grep "cloud:". */
    {
        s32 grafted = 0;

        for (i = 0; i < ARRAY_COUNT(gGRCommonStruct.yoster.clouds); i++)
        {
            if ((gGRCommonStruct.yoster.clouds[i].dobj[0] != NULL) &&
                (gGRCommonStruct.yoster.clouds[i].dobj[0]->mobj != NULL))
            {
                grafted++;
            }
        }
        syDebugPrintf("cloud: %d map joint(s), %d graft joint(s), %d/3 mobj(s), bank %d\n",
                      (int)stage_bound()->map_models[0].hd->joint_count,
                      (int)((stage_bound()->graft_model.hd != NULL)
                            ? stage_bound()->graft_model.hd->joint_count : -1),
                      (int)grafted,
                      (int)gGRCommonStruct.yoster.particle_bank_id);
    }
#endif
}

/* gryoster.c:260-268 0x80108C80 grYosterMakeGround, verbatim: the
 * stage's own ground GObj carries the cloud machine at priority 4, and
 * the clouds themselves are GObs of their own. */
GObj* grYosterMakeGround(void)
{
    GObj *ground_gobj = gcMakeGObjSPAfter(nGCCommonKindGround, NULL, nGCCommonLinkIDGround, GOBJ_PRIORITY_DEFAULT);

    grYosterInitAll();
    gcAddGObjProcess(ground_gobj, grYosterProcUpdate, nGCProcessKindFunc, 4);

    return ground_gobj;
}

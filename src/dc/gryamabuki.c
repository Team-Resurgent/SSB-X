/* gryamabuki.c -- gr/grcommon/gryamabuki.c, verbatim but for the map
 * reads: Saffron City, the eighth stage hazard this port has ported.
 *
 * The stage is one building with a gate in it. A fighter stands on the
 * pad in front (`nMPMaterialDetect`), the gate opens, and a POKEMON
 * comes out -- Chansey, Electrode, Charmander, Venusaur or Porygon,
 * picked at random and never the same one twice in a row
 * (`monster_id_prev`). The monster walks out across the roof and back;
 * the gate's own position follows it (`gate_pos.x` is the monster's
 * leading edge, clamped to the roof's two ends), and the collision line
 * at yakumono 3 is moved with it every tic, which is what makes the gate
 * a floor. When the monster is gone or destroyed, the gate closes and
 * the 1000-tick wait starts over.
 *
 * THE MONSTER IS AN ITEM, and this is the second place in the port that
 * makes one from a stage (Peach's Castle's Bumper is the first,
 * Mushroom Kingdom's `?` block the second). The kinds it picks from are
 * `nITKindGroundMonsterStart`..`End` -- the five Saffron City items --
 * which is a DIFFERENT range from the Poké Ball's, and it draws from the
 * same `dITManagerForceMonsterKind` debug global the Poké Ball does
 * (src/dc/itmball.c): a non-zero value inside the range
 * forces one kind here too. That shared global is the game's, not the
 * port's.
 *
 * DIVERGES, three:
 *
 *  - `map_head` and `item_head` are the decomp's `map_nodes -
 *    &llGRYamabukiMapMapHead` and `gMPCollisionGroundData -
 *    &llGRYamabukiMapItemHead` turned into offsets. The port has neither
 *    linker symbol; both fields stay NULL, and every read that used them
 *    is one of the two below.
 *
 *  - `grYamabukiMakeGate`'s `gcSetupCustomDObjs(gate_gobj, map_head +
 *    &llGRYamabukiMapMapHead, ...)` is `stage_bind_map_model`, the
 *    port's own way of building a GObj's tree out of the pack. The
 *    decomp asks for `gcDrawDObjTreeDLLinksForGObj` on display link 6;
 *    the port's bind uses gcDrawDObjTreeForGObj, which draws the pack's
 *    own batches -- the DL-links structure is what the EXPORTER reads
 *    (the stage entry's `dl_links`), so the picture is the same one.
 *
 *  - `grYamabukiGateAddAnimOffset` takes the decomp's linker OFFSET and
 *    is a NAME here: `gcAddAnimJointAll(gate_gobj, map_head + offset, 0)`
 *    becomes `stage_map_anim_array(name, arr)` and the same
 *    gcAddAnimJointAll over that array, which is the identical call for a
 *    per-joint table. It is `#ifdef FT_HOSTTEST`-guarded for grcastle.c's
 *    own reason (`union AObjEvent32` is eight bytes on x86-64, so the
 *    interpreter never terminates).
 *
 * dGRYamabukiMonsterAttackKind and dGRYamabukiMonsterMapObjKinds are the
 * file's own initialized data, ported verbatim. The first is not read
 * anywhere in the decomp -- `GRYAMABUKI_MONSTER_WEAPON_MAX` is a
 * sentinel for a weapon selector that never landed -- and it is kept
 * because it is the file's own data and the port has nowhere else to
 * put it.
 */
#include <gr/ground.h>
#include <gr/grcommon/gryamabuki.h>
#include <gr/grvars.h>            /* GRCommonGroundVarsYamabuki */
#include <ft/fighter.h>
#include <it/item.h>              /* the Gate's monster is an ITEM */
#include <mp/mpdef.h>             /* nMPMapObjKind* */
#include <mp/mpcollision.h>
#include <sc/scene.h>
#include <sys/objanim.h>          /* gcAddAnimJointAll, gcPlayAnimAll */
#include <sys/utils.h>            /* syUtilsRandIntRange */

#ifdef FT_HOSTTEST
#include "hoststubs.h"
#else
#include <kos.h>
#endif

#include "stage.h"                /* stage_bind_map_model, stage_map_anim* */
#include "ftcommon.h"             /* func_800269C0_275C0 */

/* // // // // // // // // // // // //
 *                                   //
 *       INITIALIZED DATA            //
 *                                   //
 * // // // // // // // // // // // // */

/* gryamabuki.c:14 0x8012EB60. A sentinel, never read -- see this file's
 * header. */
s32 dGRYamabukiMonsterAttackKind = GRYAMABUKI_MONSTER_WEAPON_MAX;

/* gryamabuki.c:17-25 0x8012EB64. The five map object kinds the Gate
 * looks for, and only the FIRST is a live kind: the four `Unused` ones
 * are how the decomp's own symbol names them, and the stage's map
 * carries one object of the first kind. `grYamabukiGateMakeMonster`
 * reads entry 0 alone. */
u16 dGRYamabukiMonsterMapObjKinds[/* */] =
{
    nMPMapObjKindMonster,
    nMPMapObjKindMonsterUnused2,
    nMPMapObjKindMonsterUnused3,
    nMPMapObjKindMonsterUnused4,
    nMPMapObjKindMonsterUnused1
};

/* // // // // // // // // // // // //
 *                                   //
 *          ENUMERATORS              //
 *                                   //
 * // // // // // // // // // // // // */

enum grYamabukiGateStatus
{
    nGRYamabukiGateStatusSleep,
    nGRYamabukiGateStatusWait,   /* The gates open + lights on state is
                                    still considered Gate_Wait */
    nGRYamabukiGateStatusOpen    /* Fully open when a Pokémon appears */
};

/* // // // // // // // // // // // //
 *                                   //
 *           FUNCTIONS               //
 *                                   //
 * // // // // // // // // // // // // */

/* gryamabuki.c:46-53 grYamabukiGateUpdateSleep 0x8010ACD0, verbatim:
 * the match's own start is what wakes the gate, and the first wait is
 * 1000 to 2000 tics. */
void grYamabukiGateUpdateSleep(void)
{
    if (gSCManagerBattleState->game_status != nSCBattleGameStatusWait)
    {
        gGRCommonStruct.yamabuki.gate_status = nGRYamabukiGateStatusWait;
        gGRCommonStruct.yamabuki.monster_wait = syUtilsRandIntRange(1000) + 1000;
    }
}

/* gryamabuki.c:56-72 grYamabukiGateCheckPlayersNear 0x8010AD18,
 * verbatim: a GROUNDED fighter standing on `nMPMaterialDetect` -- the
 * pad in front of the building -- is what opens the gate.
 *
 * The material is read off the fighter's own floor flags, so it is the
 * stage's geometry that decides where the pad is; nothing here names a
 * position. */
sb32 grYamabukiGateCheckPlayersNear(void)
{
    GObj *fighter_gobj = gGCCommonLinks[nGCCommonLinkIDFighter];

    while (fighter_gobj != NULL)
    {
        FTStruct *fp = ftGetStruct(fighter_gobj);

        if ((fp->ga == nMPKineticsGround) && ((fp->coll_data.floor_flags & MAP_VERTEX_MAT_MASK) == nMPMaterialDetect))
        {
            return TRUE;
        }
        else fighter_gobj = fighter_gobj->link_next;
    }
    return FALSE;
}

/* gryamabuki.c:74-103 grYamabukiGateMakeMonster 0x8010AD70, verbatim:
 * the Gate's payload.
 *
 * The kind is drawn from `nITKindGroundMonsterStart`'s own span -- the
 * five Saffron City items -- and never twice in a row: `monster_id_prev`
 * is nudged by one when the draw repeats, wrapping at the end.
 * `dITManagerForceMonsterKind` overrides the draw when it is inside the
 * range, which is the same debug global the Poké Ball reads.
 *
 * The monster is made with ITEM_FLAG_PARENT_GROUND at the map's own
 * `nMPMapObjKindMonster` position -- the mouth of the gate. */
void grYamabukiGateMakeMonster(void)
{
    Vec3f pos;
    Vec3f vel;
    s32 mapobj;
    s32 item_id;

    gGRCommonStruct.yamabuki.gate_status = nGRYamabukiGateStatusOpen;
    gGRCommonStruct.yamabuki.gate_noentry = FALSE;

    mpCollisionGetMapObjIDsKind(dGRYamabukiMonsterMapObjKinds[0], &mapobj);
    mpCollisionGetMapObjPositionID(mapobj, &pos);

    vel.x = vel.y = vel.z = 0.0F;

    if ((dITManagerForceMonsterKind == 0) || (dITManagerForceMonsterKind > (nITKindGroundMonsterEnd - nITKindGroundMonsterStart + 1)))
    {
        item_id = syUtilsRandIntRange(nITKindGroundMonsterEnd - nITKindGroundMonsterStart + 1);

        if (item_id == gGRCommonStruct.yamabuki.monster_id_prev)
        {
            item_id = (item_id == (nITKindGroundMonsterEnd - nITKindGroundMonsterStart)) ? 0 : item_id + 1;
        }
        gGRCommonStruct.yamabuki.monster_id_prev = item_id;
    }
    else item_id = dITManagerForceMonsterKind - 1;

    gGRCommonStruct.yamabuki.monster_gobj = itManagerMakeItemSetupCommon(NULL, item_id + nITKindGroundMonsterStart, &pos, &vel, ITEM_FLAG_PARENT_GROUND);

#ifndef FT_HOSTTEST
    /* The port's own line, the same trade `gate:` beside it makes: the
     * stage's whole point is a monster coming out of a door, and a
     * serial log is the only place it can be seen. Disc probe:
     * `--serial`, grep "gate:". */
    syDebugPrintf("gate: monster kind %d (index %d of %d), at (%.0f, "
                  "%.0f), gjobj %p\n",
                  (int)(item_id + nITKindGroundMonsterStart), (int)item_id,
                  (int)(nITKindGroundMonsterEnd - nITKindGroundMonsterStart + 1),
                  pos.x, pos.y,
                  (void *)gGRCommonStruct.yamabuki.monster_gobj);
#endif
}

/* gryamabuki.c:105-110 grYamabukiGateSetPositionFar 0x8010AE3C,
 * verbatim: the gate all the way open. Its Y is ALWAYS the yakumono
 * DObj's own -- the roof line -- and only X moves. */
void grYamabukiGateSetPositionFar(void)
{
    gGRCommonStruct.yamabuki.gate_pos.x = 1600.0F;
    gGRCommonStruct.yamabuki.gate_pos.y = gMPCollisionYakumonoDObjs->dobjs[3]->translate.vec.f.y;
}

/* gryamabuki.c:112-117 grYamabukiGateSetPositionNear 0x8010AE68,
 * verbatim: and closed. */
void grYamabukiGateSetPositionNear(void)
{
    gGRCommonStruct.yamabuki.gate_pos.x = 960.0F;
    gGRCommonStruct.yamabuki.gate_pos.y = gMPCollisionYakumonoDObjs->dobjs[3]->translate.vec.f.y;
}

/* gryamabuki.c:119-124 grYamabukiGateAddAnimOffset 0x8010AE94,
 * verbatim but for the script read. The decomp adds a linker offset to
 * `map_head`; the port names the pack's own table. See this file's
 * header. */
void grYamabukiGateAddAnimOffset(const char *name)
{
#ifdef FT_HOSTTEST
    /* the host cannot walk a script; see grcastle.c's own note */
    (void)name;
#else
    AObjEvent32 *arr[STAGE_MAP_JOINTS_MAX];

    stage_map_anim_array(name, arr);
    gcAddAnimJointAll(gGRCommonStruct.yamabuki.gate_gobj, arr, 0.0F);
    gcPlayAnimAll(gGRCommonStruct.yamabuki.gate_gobj);
#endif
}

/* gryamabuki.c:126-130 grYamabukiGateAddAnimOpen 0x8010AED8, verbatim
 * but for the name. */
void grYamabukiGateAddAnimOpen(void)
{
    grYamabukiGateAddAnimOffset("GateOpen");
}

/* gryamabuki.c:132-136 grYamabukiGateAddAnimClose 0x8010AEFC,
 * verbatim but for the name. */
void grYamabukiGateAddAnimClose(void)
{
    grYamabukiGateAddAnimOffset("GateClose");
}

/* gryamabuki.c:138-143 grYamabukiGateAddAnimOpenEntry 0x8010AF20,
 * verbatim: the gate opens all the way -- which is what lets a fighter
 * walk into the building -- and stays there. */
void grYamabukiGateAddAnimOpenEntry(void)
{
    grYamabukiGateAddAnimOpen();
    grYamabukiGateSetPositionFar();
}

/* gryamabuki.c:145-173 grYamabukiGateUpdateWait 0x8010AF48, verbatim.
 *
 * Three ways into a monster, and the order matters: a fighter on the pad
 * makes one immediately; the `gate_wait` countdown opening the gate
 * makes one; and the `monster_wait` countdown makes one whether the gate
 * is open or not (opening it first if it is not). The `monster_wait`
 * decrement happens on every path, which is why a gate that has just let
 * a fighter in does not immediately make another monster. */
void grYamabukiGateUpdateWait(void)
{
    if (gGRCommonStruct.yamabuki.gate_wait == 0)
    {
        if (grYamabukiGateCheckPlayersNear() != FALSE)
        {
            grYamabukiGateMakeMonster();

            return;
        }
    }
    else if (--gGRCommonStruct.yamabuki.gate_wait == 0)
    {
        grYamabukiGateAddAnimOpenEntry();
        func_800269C0_275C0(nSYAudioFGMYamabukiGate);
    }
    gGRCommonStruct.yamabuki.monster_wait--;

    if (gGRCommonStruct.yamabuki.monster_wait == 0)
    {
        if (gGRCommonStruct.yamabuki.gate_wait != 0)
        {
            grYamabukiGateAddAnimOpen();
            func_800269C0_275C0(nSYAudioFGMYamabukiGate);
        }
        grYamabukiGateMakeMonster();
    }
}

/* gryamabuki.c:175-200 grYamabukiGateUpdateOpen 0x8010AFF4, verbatim:
 * while a monster is out, the gate FOLLOWS it -- `gate_pos.x` is the
 * monster's own x minus its map collision width, i.e. the door's edge
 * rides against the thing that came through it. Clamped to the roof's
 * two ends, and once it has been clamped at the near end
 * (`gate_noentry`) it stops tracking, which is what keeps the door from
 * grinding backwards into the building. */
void grYamabukiGateUpdateOpen(void)
{
    if (gGRCommonStruct.yamabuki.monster_gobj == NULL)
    {
        grYamabukiGateSetClosedWait();
    }
    else if (gGRCommonStruct.yamabuki.gate_noentry == FALSE)
    {
        ITStruct *ip = itGetStruct(gGRCommonStruct.yamabuki.monster_gobj);

        gGRCommonStruct.yamabuki.gate_pos.x = DObjGetStruct(gGRCommonStruct.yamabuki.monster_gobj)->translate.vec.f.x - ip->coll_data.map_coll.width;
        gGRCommonStruct.yamabuki.gate_pos.y = gMPCollisionYakumonoDObjs->dobjs[3]->translate.vec.f.y;

        if (gGRCommonStruct.yamabuki.gate_pos.x < 960.0F)
        {
            gGRCommonStruct.yamabuki.gate_pos.x = 960.0F;

            gGRCommonStruct.yamabuki.gate_noentry = TRUE;
        }
        else if (gGRCommonStruct.yamabuki.gate_pos.x > 1600.0F)
        {
            gGRCommonStruct.yamabuki.gate_pos.x = 1600.0F;
        }
    }
}

/* gryamabuki.c:202-206 grYamabukiGateClearMonsterGObj 0x8010B0AC,
 * verbatim: the ITEM calls this when it is knocked out (see
 * src/dc/itglucky.c's damage proc), which is the Open state's own way of
 * learning the monster is gone. */
void grYamabukiGateClearMonsterGObj(void)
{
    gGRCommonStruct.yamabuki.monster_gobj = NULL;
}

/* gryamabuki.c:208-218 grYamabukiGateSetClosedWait 0x8010B0B8,
 * verbatim: closed, and both countdowns re-armed at random. The ITEM
 * calls this too -- a Chansey that finishes its walk is what closes the
 * gate behind it. */
void grYamabukiGateSetClosedWait(void)
{
    gGRCommonStruct.yamabuki.gate_status = nGRYamabukiGateStatusWait;
    gGRCommonStruct.yamabuki.gate_wait = 1000;

    gGRCommonStruct.yamabuki.monster_wait = syUtilsRandIntRange(1000) + 1000;

    grYamabukiGateSetPositionNear();
    grYamabukiGateAddAnimClose();
}

/* gryamabuki.c:220-223 grYamabukiGateUpdateYakumonoPos 0x8010B108,
 * verbatim: THE GATE IS A FLOOR. Yakumono 3 is the collision line
 * `grYamabukiInitGroundVars` turns on, and this writes the gate's own
 * position into it every tic -- which is what makes the roof a platform
 * a fighter can stand on and what makes the door's movement a moving
 * floor. */
void grYamabukiGateUpdateYakumonoPos(void)
{
    mpCollisionSetYakumonoPosID(3, &gGRCommonStruct.yamabuki.gate_pos);
}

/* gryamabuki.c:226-243 grYamabukiGateProcUpdate 0x8010B130, verbatim:
 * the stage's whole machine, three states. Sleep wakes into Wait; Wait
 * and Open both move the yakumono line, and neither moves it in Sleep,
 * which is the pre-match hold. */
void grYamabukiGateProcUpdate(GObj *ground_gobj)
{
    switch (gGRCommonStruct.yamabuki.gate_status)
    {
    case nGRYamabukiGateStatusSleep:
        grYamabukiGateUpdateSleep();
        break;

    case nGRYamabukiGateStatusWait:
        grYamabukiGateUpdateWait();
        grYamabukiGateUpdateYakumonoPos();
        break;

    case nGRYamabukiGateStatusOpen:
        grYamabukiGateUpdateOpen();
        grYamabukiGateUpdateYakumonoPos();
        break;
    }
}

/* gryamabuki.c:246-266 grYamabukiMakeGate 0x8010B19C, verbatim but for
 * the tree build. `map_head + &llGRYamabukiMapMapHead` is the pack's
 * object 0, and the decomp's own `gcAddGObjProcess(gcPlayAnimAll, ...)`
 * is not repeated because the port's bind registers one; its
 * `grYamabukiGateAddAnimClose` at the tail is, and that is what gives
 * the gate its first script. */
void grYamabukiMakeGate(void)
{
    GObj *gate_gobj;

    gGRCommonStruct.yamabuki.gate_gobj = gate_gobj = gcMakeGObjSPAfter(nGCCommonKindGround, NULL, nGCCommonLinkIDGround, GOBJ_PRIORITY_DEFAULT);

    stage_bind_map_model(gate_gobj, 6);

    grYamabukiGateAddAnimClose();
}

/* gryamabuki.c:268-288 grYamabukiInitGroundVars 0x8010B250, verbatim but
 * for the two offsets. `monster_id_prev` starts ONE PAST the last kind,
 * which is what makes the first draw always legal. */
void grYamabukiInitGroundVars(void)
{
    /* DIVERGES: NULL -- see this file's header. */
    gGRCommonStruct.yamabuki.map_head = NULL;

    mpCollisionSetYakumonoOnID(3);

    gGRCommonStruct.yamabuki.gate_wait = 1;
    /* DIVERGES: NULL -- see this file's header. */
    gGRCommonStruct.yamabuki.item_head = NULL;

    dGRYamabukiMonsterAttackKind = GRYAMABUKI_MONSTER_WEAPON_MAX;

    gGRCommonStruct.yamabuki.monster_id_prev = (nITKindGroundMonsterEnd - nITKindGroundMonsterStart) + 1;
    gGRCommonStruct.yamabuki.gate_pos.z = 0.0F;

    grYamabukiGateSetPositionNear();
    grYamabukiMakeGate();
    grYamabukiGateUpdateYakumonoPos();

    gGRCommonStruct.yamabuki.gate_status = 0;

#ifndef FT_HOSTTEST
    /* The port's own line, the same trade grcastle.c's `castle:` and
     * grinishie.c's two make. Saffron City's whole hazard is this one
     * machine, and a serial log is the only place its numbers can be
     * checked at all: the gate's tree and scripts came through the pack,
     * the yakumono line is on, and where the door starts. Disc probe:
     * `--serial`, grep "gate:". */
    {
        Stage *st = stage_bound();

        syDebugPrintf("gate: %d object(s), %d script(s), gate at "
                      "(%.0f, %.0f), status %d\n",
                      (st != NULL) ? (int)st->map_object_count : -1,
                      (st != NULL) ? (int)st->map_anim_count : -1,
                      gGRCommonStruct.yamabuki.gate_pos.x,
                      gGRCommonStruct.yamabuki.gate_pos.y,
                      (int)gGRCommonStruct.yamabuki.gate_status);
    }
#endif
}

/* gryamabuki.c:290-298 grYamabukiMakeGround 0x8010B2EC, verbatim: the
 * stage's entry point, and the one src/dc/stage.c calls when the bound
 * stage is nGRKindYamabuki. Note the order -- the process is installed
 * BEFORE the ground vars are initialised, so the first tic of the
 * machine runs on a state InitGroundVars has already set. */
GObj* grYamabukiMakeGround(void)
{
    GObj *ground_gobj = gcMakeGObjSPAfter(nGCCommonKindGround, NULL, nGCCommonLinkIDGround, GOBJ_PRIORITY_DEFAULT);

    gcAddGObjProcess(ground_gobj, grYamabukiGateProcUpdate, nGCProcessKindFunc, 4);
    grYamabukiInitGroundVars();

    return ground_gobj;
}

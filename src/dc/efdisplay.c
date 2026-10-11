/* efdisplay.c -- ef/efdisplay.c, verbatim.
 *
 * Seven functions and one of them matters: efDisplayInitAll, which makes
 * the four GObjs whose display procs are the only thing in the game that
 * draws a particle, and then loads the common effect bank. The other six
 * are the procs themselves -- a render mode, the renderer, and the
 * render mode put back.
 *
 * The GBI is the model src/dc/lbpdraw.h installs for lb/lbparticle.c's
 * renderer, and for the same reason: what these procs set is state the
 * renderer reads, and it has to be read out of the same place. See
 * src/dc/lbparticle.c.
 *
 * DIVERGES, three times and no more:
 *
 *   Each of the three ZPersp procs opens with a guard: the port's frame
 *   runs every camera once per PVR list (src/dc/taskman.c) and a
 *   particle is a translucent quad, so the other two passes have nothing
 *   to do and should not pay for a projection and a walk of the live
 *   list to find that out. src/dc/lbfade.c's display proc has the same
 *   line for the same reason.
 *
 *   efDisplayInitAll's four addresses are cast. They are the ROM's own,
 *   defined absolutely by src/game/ssb64/particlebanks.ld, and the four
 *   casts are gr/grcommon/grhyrule.c:413's -- the one caller of
 *   efParticleGetLoadBankID that spells them out; efdisplay.c:100 takes
 *   the implicit conversion instead.
 *
 *   The bank's textures are converted for the PVR and uploaded here,
 *   after the walk that turns the .txb's offsets into pointers, because
 *   that walk is what lbpTexLoadBank binds itself to (src/dc/lbpartex.h).
 *   The N64 had nothing to do: the RDP read the bank where it lay.
 */
#include <ssb_types.h>
#include <sys/obj.h>
#include <sys/taskman.h>
#include <lb/library.h>
#include <ef/efparticle.h>
#include <ef/effect.h>
#include <ef/efdisplay.h>

#include "lbpartex.h"
#include "objpvr.h"

#define LBPDRAW_MODEL 1
#include "lbpdraw.h"
#include "perf.h"

// 0x800FCCC0
void efDisplayCLDProcDisplay(GObj *effect_gobj)
{
    gDPPipeSync(gSYTaskmanDLHeads[1]++);
    gDPSetRenderMode(gSYTaskmanDLHeads[1]++, G_RM_CLD_SURF, G_RM_CLD_SURF2);
    gDPSetAlphaCompare(gSYTaskmanDLHeads[1]++, G_AC_THRESHOLD);
    gDPSetBlendColor(gSYTaskmanDLHeads[1]++, 0x00, 0x00, 0x00, 0x08);
    gSPClearGeometryMode(gSYTaskmanDLHeads[1]++, G_ZBUFFER);
}

// 0x800FCD64
void efDisplayXLUProcDisplay(GObj *effect_gobj)
{
    gDPPipeSync(gSYTaskmanDLHeads[1]++);
    gDPSetRenderMode(gSYTaskmanDLHeads[1]++, G_RM_AA_ZB_XLU_SURF, G_RM_AA_ZB_XLU_SURF2);
    gDPSetAlphaCompare(gSYTaskmanDLHeads[1]++, G_AC_NONE);
    gSPSetGeometryMode(gSYTaskmanDLHeads[1]++, G_ZBUFFER);
}

// 0x800FCDEC
void efDisplayMakeCLD(void)
{
    gcAddGObjDisplay(gcMakeGObjSPAfter(nGCCommonKindEffect, NULL, nGCCommonLinkIDEffect, GOBJ_PRIORITY_DEFAULT), efDisplayCLDProcDisplay, 15, 3, ~0);
    gcAddGObjDisplay(gcMakeGObjSPAfter(nGCCommonKindEffect, NULL, nGCCommonLinkIDEffect, GOBJ_PRIORITY_DEFAULT), efDisplayCLDProcDisplay, 18, 3, ~0);
}

// 0x800FCE6C
void efDisplayMakeXLU(void)
{
    gcAddGObjDisplay(gcMakeGObjSPAfter(nGCCommonKindEffect, NULL, nGCCommonLinkIDEffect, GOBJ_PRIORITY_DEFAULT), efDisplayXLUProcDisplay, 15, 0, ~0);
    gcAddGObjDisplay(gcMakeGObjSPAfter(nGCCommonKindEffect, NULL, nGCCommonLinkIDEffect, GOBJ_PRIORITY_DEFAULT), efDisplayXLUProcDisplay, 18, 0, ~0);
}

// 0x800FCEEC
void efDisplayZPerspXLUProcDisplay(GObj *effect_gobj)
{
    if (gcGetDrawList() != PVR_LIST_TR_POLY) return;    /* DIVERGES */

    gDPPipeSync(gSYTaskmanDLHeads[0]++);
    gDPSetRenderMode(gSYTaskmanDLHeads[0]++, G_RM_XLU_SURF, G_RM_XLU_SURF2);

    {
        DBPERF_BEGIN(DBP_PARTICLE);
        lbParticleDrawTextures(effect_gobj);
        DBPERF_END(DBP_PARTICLE);
    }

    gDPPipeSync(gSYTaskmanDLHeads[0]++);
    gDPSetTexturePersp(gSYTaskmanDLHeads[0]++, G_TP_PERSP);
    gDPSetDepthSource(gSYTaskmanDLHeads[0]++, G_ZS_PIXEL);
    gDPSetRenderMode(gSYTaskmanDLHeads[0]++, G_RM_AA_ZB_OPA_SURF, G_RM_AA_ZB_OPA_SURF2);
}

// 0x800FCFCC
void efDisplayZPerspCLDProcDisplay(GObj *effect_gobj)
{
    if (gcGetDrawList() != PVR_LIST_TR_POLY) return;    /* DIVERGES */

    gDPPipeSync(gSYTaskmanDLHeads[0]++);
    gDPSetRenderMode(gSYTaskmanDLHeads[0]++, G_RM_CLD_SURF, G_RM_CLD_SURF2);

    {
        DBPERF_BEGIN(DBP_PARTICLE);
        lbParticleDrawTextures(effect_gobj);
        DBPERF_END(DBP_PARTICLE);
    }

    gDPPipeSync(gSYTaskmanDLHeads[0]++);
    gDPSetTexturePersp(gSYTaskmanDLHeads[0]++, G_TP_PERSP);
    gDPSetDepthSource(gSYTaskmanDLHeads[0]++, G_ZS_PIXEL);
    gDPSetRenderMode(gSYTaskmanDLHeads[0]++, G_RM_AA_ZB_OPA_SURF, G_RM_AA_ZB_OPA_SURF2);
}

// 0x800FD0AC
void efDisplayZPerspAAXLUProcDisplay(GObj *effect_gobj)
{
    if (gcGetDrawList() != PVR_LIST_TR_POLY) return;    /* DIVERGES */

    gDPPipeSync(gSYTaskmanDLHeads[0]++);
    gDPSetRenderMode(gSYTaskmanDLHeads[0]++, G_RM_AA_ZB_XLU_SURF, G_RM_AA_ZB_XLU_SURF2);

    {
        DBPERF_BEGIN(DBP_PARTICLE);
        lbParticleDrawTextures(effect_gobj);
        DBPERF_END(DBP_PARTICLE);
    }

    gDPPipeSync(gSYTaskmanDLHeads[0]++);
    gDPSetTexturePersp(gSYTaskmanDLHeads[0]++, G_TP_PERSP);
    gDPSetDepthSource(gSYTaskmanDLHeads[0]++, G_ZS_PIXEL);
    gDPSetRenderMode(gSYTaskmanDLHeads[0]++, G_RM_AA_ZB_OPA_SURF, G_RM_AA_ZB_OPA_SURF2);
}

// 0x800FD18C
void efDisplayInitAll(void)
{
    GObj *gobj;

    gobj = gcMakeGObjSPAfter(nGCCommonKindEffect, NULL, nGCCommonLinkIDEffect, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, efDisplayZPerspCLDProcDisplay, 18, 1, ~0);
    gobj->camera_mask = COBJ_MASK_DLLINK(2) | COBJ_MASK_DLLINK(0);

    gobj = gcMakeGObjSPAfter(nGCCommonKindEffect, NULL, nGCCommonLinkIDEffect, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, efDisplayZPerspCLDProcDisplay, 15, 1, ~0);
    gobj->camera_mask = COBJ_MASK_DLLINK(1);

    gobj = gcMakeGObjSPAfter(nGCCommonKindInterface, NULL, nGCCommonLinkIDInterface, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, efDisplayZPerspXLUProcDisplay, 25, GOBJ_PRIORITY_DEFAULT, ~0);
    gobj->camera_mask = COBJ_MASK_DLLINK(3);

    gobj = gcMakeGObjSPAfter(nGCCommonKindEffect, NULL, nGCCommonLinkIDEffect, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, efDisplayZPerspAAXLUProcDisplay, 10, GOBJ_PRIORITY_DEFAULT, ~0);
    gobj->camera_mask = COBJ_MASK_DLLINK(4);

    gEFManagerParticleBankID = efParticleGetLoadBankID
    (
        (uintptr_t)&lEFCommonParticleScriptBankLo,
        (uintptr_t)&lEFCommonParticleScriptBankHi,
        (uintptr_t)&lEFCommonParticleTextureBankLo,
        (uintptr_t)&lEFCommonParticleTextureBankHi
    );

    lbpTexLoadBank(gEFManagerParticleBankID, "efcommon");
}

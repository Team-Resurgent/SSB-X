/* lbfade.c -- lb/lbfade.c, the fade between scenes. Every function is
 * the decomp's by name and body, and the six file-scope variables keep
 * the decomp's linkage (none of them is static there) so the host test
 * can read the timeline off them.
 *
 * How it works, for the port's sake: lbFadeMakeActor makes one GObj on
 * the transition link with two hooks. lbFadeProcUpdate is a process that
 * walks sLBFadeAlphaCurrent up to the fade length once a tic and ejects
 * the GObj two tics after it gets there; lbFadeProcDisplay is hung on
 * the GObj by func_80009F74, which puts it on the *camera* list (the last
 * DL link, objman.c:1939) at priority 10 -- below every real camera, so
 * it draws after all of them -- and gcDrawAll calls it once a frame as if
 * it were a camera. What it draws is one flat rectangle over the
 * 300x220 in the fade's colour at the alpha of the moment: the colour's
 * own alpha byte says the direction (0: from the colour to the scene,
 * 255: from the scene to the colour).
 *
 * DIVERGES once, in lbFadeProcDisplay: its six gDP* commands -- prim
 * colour, G_CC_PRIMITIVE, G_RM_CLD_SURF and a gDPFillRectangle -- are
 * lbCommonSpriteFillRect (src/dc/lbcommon.c), a flat quad in the PVR's
 * translucent list above every sprite. The frame runs each camera once
 * per PVR list (src/dc/taskman.c), so the proc draws on the translucent
 * one only. */
#include <lb/lbfade.h>
#include "lbcommon.h"
#include "objpvr.h"

#include <sys/obj.h>
#include <config.h>              /* GS_SCREEN_WIDTH_DEFAULT */

// 0x800D6460
s32 sLBFadeAlphaMax;

// 0x800D6464
s32 sLBFadeAlphaCurrent;

// 0x800D6468
s32 sLBFadeLength;

// 0x800D646C
SYColorRGBA sLBFadeColor;

// 0x800D6470
sb32* sLBFadeIsProceedScene;

// 0x800D6474
sb32 sLBFadeIsEjectGObj;

/* lbfade.c:31-53 0x800D3E80, verbatim. */
void lbFadeProcUpdate(GObj *gobj)
{
    if (sLBFadeAlphaCurrent < sLBFadeAlphaMax)
    {
        sLBFadeAlphaCurrent++;
    }
    if (sLBFadeLength != 0)
    {
        sLBFadeLength--;

        if (sLBFadeLength == 0)
        {
            if (sLBFadeIsProceedScene != NULL)
            {
                *sLBFadeIsProceedScene = TRUE;
            }
            if (sLBFadeIsEjectGObj != FALSE)
            {
                gcEjectGObj(gobj);
            }
        }
    }
}

/* lbfade.c:56-72 0x800D3F08. The alpha arithmetic is the decomp's; the
 * display list is the DIVERGES above. */
void lbFadeProcDisplay(GObj *gobj)
{
    s32 alpha = ((f32) sLBFadeAlphaCurrent / (f32) sLBFadeAlphaMax) * 255.0F;

    (void)gobj;

    if (sLBFadeColor.a == 0)
    {
        alpha = 0xFF - alpha;
    }
    if (gcGetDrawList() != PVR_LIST_TR_POLY)
    {
        return;
    }
    lbCommonSpriteFillRect(10, 10, GS_SCREEN_WIDTH_DEFAULT - 10, GS_SCREEN_HEIGHT_DEFAULT - 10,
                           sLBFadeColor.r, sLBFadeColor.g, sLBFadeColor.b, (u32)alpha);
}

/* lbfade.c:75-89 0x800D4060, verbatim. */
void lbFadeMakeActor(u32 id, s32 link, u32 link_priority, SYColorRGBA *color, s32 fade_length, sb32 is_eject_gobj, sb32 *is_proceed_scene)
{
    GObj *gobj = gcMakeGObjSPAfter(id, NULL, link, GOBJ_PRIORITY_DEFAULT);

    func_80009F74(gobj, lbFadeProcDisplay, link_priority, 0, ~0);
    gcAddGObjProcess(gobj, lbFadeProcUpdate, nGCProcessKindFunc, 0);

    sLBFadeColor = *color;
    sLBFadeAlphaMax = fade_length;
    sLBFadeAlphaCurrent = 0;
    sLBFadeLength = fade_length + 2;
    sLBFadeIsEjectGObj = is_eject_gobj;
    sLBFadeIsProceedScene = is_proceed_scene;
}

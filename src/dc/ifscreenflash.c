/* ifscreenflash.c -- if/ifscreenflash.c, the screen flash: the wash of
 * colour over the arena on every KO and on a hit that launches someone
 * past FTCOMMON_DAMAGE_KNOCKBACK_VERYHIGH -- white, or fire, electric
 * and ice coloured by the hit's element (ft/ftcommon/ftcommondamage.c
 * ftCommonDamageCheckMakeScreenFlash, ft/ftcommon/ftcommondead.c).
 *
 * The decomp's 74 lines, function for function -- the names, the
 * signatures, the two file globals, the interface GObj's link, DL link 22
 * and its process -- but for one body: ifScreenFlashProcDisplay's nine
 * GBI commands are a flat translucent rectangle over the game's 300x220,
 * and the port draws that through lbCommonSpriteFillRect
 * (src/dc/lbcommon.c), the same quad lb/lbfade.c's fade takes, in the
 * translucent list only. The DIVERGES there says the rest.
 *
 * The flash is a colour animation and nothing else: ifScreenFlashSetColAnimID
 * hands one of gm/gmcolscripts.c's five ScreenFlash scripts to
 * ftParamCheckSetColAnimID and ifScreenFlashProcUpdate steps it through
 * ftMainUpdateColAnim once a tic, the interpreter a fighter's own
 * colanim is stepped by (both in the link since this step: src/dc/ftparam.c
 * and src/dc/ftmain.c, the decomp's text). The scripts and their
 * priorities are the decomp's gm/gmcolscripts.c compiled unmodified
 * (src/game/ssb64/Makefile gmcolscripts.o).
 */
#include "ftcommon.h"
#include "lbcommon.h"
#include "objpvr.h"
#include "overlay.h"

#include <if/ifscreenflash.h>
#include <ft/ftparam.h>
#include <ft/ftmain.h>
#include <sc/scene.h>

// // // // // // // // // // // //
//                               //
//   GLOBAL / STATIC VARIABLES   //
//                               //
// // // // // // // // // // // //

// 0x80131A40
GMColAnim sIFScreenFlashColAnim; 

// 0x80131AA4
u8 sIFScreenFlashAlpha;

// // // // // // // // // // // //
//                               //
//           FUNCTIONS           //
//                               //
// // // // // // // // // // // //

// 0x80115BF0
void ifScreenFlashSetColAnimID(s32 colanim_id, s32 colanim_duration)
{
    ftParamCheckSetColAnimID(&sIFScreenFlashColAnim, colanim_id, colanim_duration);
}

/* 0x80115C20. DIVERGES: the decomp queues nine commands into the head
 * the screen-flash camera captures -- 1-cycle, G_CC_PRIMITIVE, the
 * AA_XLU render mode, the primitive colour, gDPFillRectangle(10, 10,
 * 310, 230), and the opaque Z-buffered mode back for whoever draws next.
 * That is a flat quad blended by its alpha over the game's whole
 * viewport, which lbCommonSpriteFillRect is (a header of its own, the
 * frame's next sprite depth, so it sits over the arena the battle camera
 * drew and under the HUD the interface camera draws after this one --
 * src/dc/gmcamera.c gmCameraScreenFlashMakeCamera on the order). The
 * frame runs every camera once per PVR list (src/dc/taskman.c) and a
 * translucent quad belongs to one of them, so the other two passes are
 * skipped whole, as lb/lbfade.c's fade skips them. The alpha is the
 * decomp's line: the script's, scaled by the alpha the interface was
 * made with. */
void ifScreenFlashProcDisplay(GObj *gobj)
{
    GMColAnim *ca = &sIFScreenFlashColAnim;
    s32 alpha;

    (void)gobj;

    if (ca->is_use_color1)
    {
        if (gcGetDrawList() != PVR_LIST_TR_POLY)
        {
            return;
        }
        alpha = (ca->color1.a * sIFScreenFlashAlpha) / 0xFF;

        lbCommonSpriteFillRect(10, 10, 310, 230, ca->color1.r, ca->color1.g, ca->color1.b, alpha);
    }
}

// 0x80115DA8
void ifScreenFlashProcUpdate(GObj *fighter_gobj)
{
    if (ftMainUpdateColAnim(&sIFScreenFlashColAnim, fighter_gobj, FALSE, FALSE) != FALSE)
    {
        ftParamResetColAnim(&sIFScreenFlashColAnim);
    }
}

// 0x80115DE8
void ifScreenFlashMakeInterface(u8 alpha)
{
    sIFScreenFlashAlpha = alpha;

    ftParamResetColAnim(&sIFScreenFlashColAnim);

    if (gSCManagerBackupData.is_allow_screenflash != FALSE)
    {
        GObj *interface_gobj = gcMakeGObjSPAfter(nGCCommonKindInterface, NULL, nGCCommonLinkIDInterface, GOBJ_PRIORITY_DEFAULT);

        gcAddGObjDisplay(interface_gobj, ifScreenFlashProcDisplay, 22, GOBJ_PRIORITY_DEFAULT, ~0);
        gcAddGObjProcess(interface_gobj, ifScreenFlashProcUpdate, nGCProcessKindFunc, 1);
    }
}

/* The port's: if/ifscreenflash is overlay 2's, and its two globals above
 * are the .bss syDmaLoadOverlay would zero (src/dc/overlay.h). */
void ifScreenFlashOverlayLoad(void)
{
    OVERLAY_CLEAR(sIFScreenFlashColAnim);
    OVERLAY_CLEAR(sIFScreenFlashAlpha);
}

/* mvending.h -- the ending diorama, mv/mvending/mvending.c ported.
 * Structurally this is a small diorama scene: one fighter
 * (the 1P-mode clear character) posed in a "room" set built from static
 * DObj props (desk, books, pencils, lamp, tissues, all from one shared
 * mvcommon relocData bank), two animated cameras driven off a canned
 * AObjEvent32 script, and a tic-counted fade/light sequence
 * (sMVEndingTotalTimeTics) that ends the scene at tic 660 by setting
 * scene_curr = nSCKindStaffroll and calling syTaskmanSetLoadScene() --
 * the ending and the staff roll are one continuous sequence, this file
 * only starts it.
 *
 * The room's six props (mvEndingMakeRoomBackground/Desk/Books/Pencils/
 * Lamp/Tissues) read a DObjDesc/MObjSub/MatAnimJoint scene-graph tree
 * out of the mvcommon relocData file. Nothing can read that
 * shape out of a non-fighter file, so the opening Room's props -- the
 * same llMVCommonRoom* symbols -- are baked into packs, and the ending
 * reads those packs (src/dc/mvending.c mvEndingSetupPropTree). No new export.
 *
 * mvEndingMakeRoomFadeIn/Light and their two cameras are ported
 * verbatim. Their display procs are fill quads in the translucent pass
 * instead of the raw gDPFillRectangle the port's write-only scratch
 * heads would swallow.
 *
 * The two main cameras' animated path is not part of that.
 * mvEndingSetupOperatorCamera's own
 * gcAddCObjCamAnimJoint/gcPlayCamAnim call is verbatim: the script is a bare AObjEvent32 stream with no tree around it,
 * so it needs none of the scene-graph machinery above -- just its words,
 * which tools/export/ssb_camanimexport.py cuts out of relocData file 76 into
 * romdisk/mvending.cam and camanim_get hands back by name
 * (src/dc/camanim.h). Both cameras move the way the game's do.
 *
 * mvEndingInitVars poses the run's own fighter, as the game does, out
 * of gSCManager1PGameBattleState (the 1P game's). A direct
 * boot has written nothing there, and a zeroed slot is fkind 0, which
 * is Mario in costume 0.
 *
 * Overlay 54 (ovl54_BSS_END, symbols/jp_wip_linker.txt), confirmed
 * free in the port's own OVERLAY_* numbering (src/dc/overlay.h).
 */
#ifndef SSB_DC_MVENDING_H
#define SSB_DC_MVENDING_H

#include <sc/scene.h>
#include <sys/obj.h>
#include <sys/taskman.h>

/* mvending.c:99-141 dMVEndingTaskmanSetup, DIVERGES the same shape
 * src/dc/scautodemo.c's own dSCAutoDemoTaskmanSetup documents: the
 * decomp's own pool counts are all zero (a placeholder, not a real
 * figure -- scvsbattle.c:24-64 names the same pattern), func_lights is
 * NULL (mvEndingFuncLights's whole body is ftDisplayLightsDrawReflect,
 * the same dropped reflection lighting every other scene's func_lights
 * drops), the DL buffers/graphics arena/RDP output are zeroed (no RSP/
 * RDP here), and the pool counts below are the port's own: this scene
 * poses one fighter with no battle interface, smaller than
 * scexplain.c's own two-GameKey-fighter figures, so half of those. */
extern SYTaskmanSetup dMVEndingTaskmanSetup;

void mvEndingMakeRoomBackground(void);
void mvEndingMakeRoomDesk(void);
void mvEndingMakeRoomBooks(void);
void mvEndingMakeRoomPencils(void);
void mvEndingMakeRoomLamp(void);
void mvEndingMakeRoomTissues(void);
void mvEndingMakeFighter(s32 fkind);
void mvEndingRoomFadeInProcDisplay(GObj *gobj);
void mvEndingMakeRoomFadeIn(void);
void mvEndingMakeRoomFadeInCamera(void);
void mvEndingRoomLightProcDisplay(GObj *gobj);
void mvEndingMakeRoomLight(void);
void mvEndingMakeRoomLightCamera(void);
void mvEndingEjectRoomGObjs(void);
void mvEndingSetupOperatorCamera(GObj *gobj);
void mvEndingMakeMainCameras(void);
void mvEndingInitVars(void);
void mvEndingFuncRun(GObj *gobj);
void mvEndingFuncStart(void);
void mvEndingStartScene(void);

/* The port's own bzero arm, src/dc/overlay.c's dSCManagerOverlays[54]. */
void mvEndingOverlayLoad(void);

#endif /* SSB_DC_MVENDING_H */

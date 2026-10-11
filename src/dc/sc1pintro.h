/* sc1pintro.h -- the 1P game's "<player> VS <opponent>" card,
 * sc/sc1pmode/sc1pintro.c. Overlay 24.
 *
 * The scene sc1PManager runs before every rung of the 1P ladder: a
 * black-bannered card with the route across the top, the two names
 * along the bottom, and between them the human's fighter and the
 * opponent standing on the same screen under their own cameras. It
 * lasts six seconds (sc1PIntroFuncRun's 360-tic cap) or until the human
 * presses A, B or Start, and then hands on to the battle.
 *
 * WHAT IT DRAWS, AND OUT OF WHAT.
 *
 *   - the sky, the two banners and the VS decal -- file 11, whose bank
 *     is romdisk/sc1pintro.spr;
 *   - the rung's number and its fourteen route markers, also file 11:
 *     sc1PIntroMakeFigures draws every marker from this rung onward, so
 *     the ones already beaten are simply absent;
 *   - the two names: the human's out of file 12 (characternames.spr,
 *     the same twelve plates the attract demo uses), the opponent's out
 *     of file 11's nine pre-drawn banners -- except on the Link and
 *     Pikachu rungs, whose opponent is a plain fighter and so takes a
 *     file 12 plate too. sc1PIntroSetNamePositions then centres the
 *     whole line by measuring the sprites it just made;
 *   - on the three bonus rungs, no VS decal and no names: a picture of
 *     the task (files 13 and 14) and its one line of text instead;
 *   - the fighters themselves, through ftManagerMakeFighter and the
 *     two status ids the card owns (0x1000D for the human's,
 *     0x1000E for the opponent's), each on its own camera and DL link.
 *
 * THE CAMERAS. This is the one scene that keeps TWO sets of camera
 * scripts in one relocData file and indexes them differently:
 * sc1PIntroMakeStageCamera picks among fourteen by rung, and
 * sc1PIntroMakeFighterCamera among twelve by the human's nFTKind. Both
 * sets are cut out of file 11 into romdisk/sc1pintro.cam and reached by
 * name (src/dc/camanim.h), since the port has no address to offset
 * from.
 *
 * WHAT THIS SCENE CANNOT SHOW YET. Three of the fourteen rungs are the
 * ones whose opponents the port has no fighter data for -- Metal Mario,
 * the Fighting Polygon Team and Master Hand (fourteen files of
 * gFTData* rows and no functions at all). Their
 * cameras, sprites, names and markers are all here and correct; the
 * ftManagerMakeFighter call on those rungs is what has nothing to make.
 * Nothing about that is this file's to fix.
 *
 * Every function is the decomp's by name and body, the REGION_US arms;
 * the line numbers are the decomp's. What is not here, and where it is
 * said:
 *   - the lighting pre-render (sc1PIntroFuncLights) and the display
 *     list it pushes -- dSC1PIntroTaskmanSetup;
 *   - syVideoInit, the z-buffer and the arena line -- sc1PIntroStartScene;
 *   - the clear camera (gcMakeDefaultCameraGObj) -- sc1PIntroFuncStart;
 *   - the reloc setup and its two status buffers -- sc1PIntroLoadFiles,
 *     which loads the four banks in their place.
 */
#ifndef SSB_DC_SC1PINTRO_H
#define SSB_DC_SC1PINTRO_H

#include <sc/scene.h>
#include <sys/obj.h>
#include <sys/taskman.h>
#include <ft/fighter.h>

extern SYTaskmanSetup dSC1PIntroTaskmanSetup;

void sc1PIntroMakeSky(void);
void sc1PIntroMakeBanners(void);
void func_ovl24_80131C58(GObj *gobj);
void func_ovl24_80131ECC(void);
void sc1PIntroMakeVSDecal(void);
sb32 sc1PIntroCheckNotBonusStage(s32 stage);
void sc1PIntroMakeLabels(s32 stage);
void sc1PIntroMakeFigures(s32 stage);
void sc1PIntroMakeBonusTasks(s32 stage);
void sc1PIntroMakeVSName(s32 stage);
s32 sc1PIntroGetAlliesNum(s32 stage);
void sc1PIntroMakeName(s32 stage);
s32 sc1PIntroGetPlayerNameOffsetX(s32 stage);
s32 sc1PIntroGetVSNameOffsetX(s32 stage);
s32 sc1PIntroGetTotalNameOffsetX(s32 stage);
void sc1PIntroSetNamePositions(s32 stage);
void sc1PIntroMakeNameAll(s32 stage);
void sc1PIntroMakeStageInfo(s32 stage);
f32 sc1PIntroGetFighterVelocityZ(s32 card_anim_frame_id);
void sc1PIntroFighterProcUpdate(GObj *fighter_gobj);
f32 sc1PIntroGetFighterPositionZ(s32 card_anim_frame_id);
void sc1PIntroMakeFighter(FTDemoDesc fighter, s32 card_anim_frame_id, void **figatree);
void sc1PIntroInitAllyTextParams(SObj *sobj);
void sc1PIntroMakeAllyText(s32 stage);
f32 sc1PIntroGetVSFighterVelocityZ(s32 stage, s32 fkind);
void sc1PIntroVSFighterProcUpdate(GObj *fighter_gobj);
void sc1PIntroSetKirbyTeamModelPartIDs(GObj *fighter_gobj, s32 fkind);
f32 sc1PIntroGetVSFighterPositionZ(s32 stage, s32 fkind);
void sc1PIntroVSFighterProcDisplay(GObj *fighter_gobj);
GObj* sc1PIntroMakeVSFighter(s32 fkind, s32 stage, s32 card_anim_frame_id, void **figatree, u8 dl_link);
void sc1PIntroMakeBonusPicture(s32 stage);
CObjDesc* sc1PIntroGetStageCObjDesc(CObjDesc *cobj_desc, s32 stage);
CObj* sc1PIntroMakeStageCamera(s32 stage, u32 dl_link);
sb32 sSC1PIntroCheckCostumeUsed(s32 stage, s32 fkind, s32 color);
void sc1PIntroInitVSFighters(s32 stage);
CObjDesc* sc1PIntroGetFighterCObjDesc(CObjDesc *cobj_desc, s32 fkind, s32 cobj_id);
void func_ovl24_80133F88(void);
void sc1PIntroMakeFighterCamera(s32 fkind, s32 cobj_id);
void sc1PIntroInitFighters(s32 stage);
void sc1PIntroMakeBannersCamera(void);
void sc1PIntroMakeDecalsCamera(void);
void sc1PIntroMakePicturesCamera(void);
s32 sc1PIntroGetFighterAllocsNum(s32 stage);
void sc1PIntroUpdateAnnounce(void);
void sc1PIntroInitVars(void);
void func_ovl24_801348EC(void);
void sc1PIntroFuncRun(GObj *gobj);
void sc1PIntroSetupFighterFiles(s32 stage);
void sc1PIntroFuncStart(void);
void sc1PIntroStartScene(void);

/* the port's own: the four banks in the reloc loader's place */
void sc1PIntroLoadFiles(void);

/* The port's own bzero arm, src/dc/overlay.c's dSCManagerOverlays[24]. */
void sc1PIntroOverlayLoad(void);

#endif /* SSB_DC_SC1PINTRO_H */

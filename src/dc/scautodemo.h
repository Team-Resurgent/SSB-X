/* scautodemo.h -- the auto-demo scene, sc/sccommon/scautodemo.c ported.
 * sc/sccommon/scautodemofiles.c is not: its one function,
 * scAutoDemoSetupFiles, is the reloc loader (lbRelocInitSetup +
 * lbRelocLoadFilesListed(dGMCommonFileIDs, gGMCommonFiles)) that
 * scvsbattle.c's own scVSBattleSetupFiles already replaced port-wide
 * with gmCommonLoadFiles() (src/dc/gmcommon.c); this file's own
 * scAutoDemoSetupFiles is that same one-line substitution, not a
 * second port of the loader.
 *
 * This is a real battle -- four CPU fighters, a stage cycled from an
 * eight-entry order, the same ftManagerMakeFighter/grCommonSetupInitAll
 * spine src/dc/scvsbattle.c already proved -- wearing a different
 * camera: an interface GObj (scAutoDemoFuncRun) walks a six-entry
 * dSCAutoDemoFuncList table, in order, each Nth tic zooming the camera
 * onto a different fighter (scAutoDemoSetFocusPlayer1/2), until the
 * demo fades out and hands scene_curr back to nSCKindStartup itself.
 *
 * DIVERGES once: scAutoDemoInitSObjs -- the two players' name-plate
 * sprites shown while each is in focus -- reads a reloc file the decomp
 * names llCharacterNamesFileID/llCharacterNames<Fighter>Sprite. The
 * twelve plates inside it are exported as characternames.spr; the reloc
 * load becomes one sprite_bank_load and each plate a sprite_bank_get,
 * the same one-bank substitution src/dc/scexplain.c and src/dc/scstaffroll.c
 * already make for their own scenes. A bank that fails to load still leaves
 * sSCAutoDemoFighterNameGObj NULL, and every read of it stays guarded on that.
 */
#ifndef SSB_DC_SCAUTODEMO_H
#define SSB_DC_SCAUTODEMO_H

#include <sc/scene.h>
#include <sys/obj.h>
#include <sys/taskman.h>

/* scautodemo.c:129-171 dSCAutoDemoTaskmanSetup, the scene's own setup
 * (see scautodemo.c for the same DIVERGES scvsbattle.c's own setup
 * documents: the display-list buffers, arena and RDP output are zeroed,
 * func_lights is NULL, the matrix function list is NULL, and the pool
 * counts are the port's own, not the decomp's zeroed placeholders). */
extern SYTaskmanSetup dSCAutoDemoTaskmanSetup;

/* scautodemo.c:21-127's own data tables, extern'd here (the decomp's
 * own scautodemo.h does not, since nothing outside the file reads them
 * on the N64) so the host test can drive the focus table and check the
 * stage cycle directly. */
extern u8 dSCAutoDemoGroundOrder[8];
extern s16 dSCAutoDemoMapObjKindList[8];
extern SCAutoDemoProc dSCAutoDemoFuncList[7];
extern f32 dSCAutoDemoZoomEyeX[6];
extern f32 dSCAutoDemoZoomEyeY[5];
extern SYColorRGBA dSCAutoDemoFadeColor;

/* scautodemo.c's own name-plate offset table, the sprite_bank_get keys
 * for characternames.spr in nFTKind order(see the header
 * note above). Extern'd for the host test, which is where a table of
 * transcribed literals belongs. */
extern const u32 dSCAutoDemoFighterNameSpriteOffsets[12];

void scAutoDemoSetupFiles(void);
void scAutoDemoFuncUpdate(void);
void scAutoDemoStartBattle(void);
void scAutoDemoDetectExit(void);
void scAutoDemoMakeFade(void);
sb32 scAutoDemoCheckStopFocusPlayer(FTStruct *fp);
void scAutoDemoSetCameraPlayerZoom(GObj *fighter_gobj);
void scAutoDemoSetFocusPlayer1(void);
void SCAutoDemoProcFocusPlayer1(void);
void scAutoDemoSetFocusPlayer2(void);
void SCAutoDemoProcFocusPlayer2(void);
void scAutoDemoResetFocusPlayerAll(void);
void scAutoDemoSetMagnifyDisplayOn(void);
void scAutoDemoExit(void);
void scAutoDemoChangeFocus(void);
void scAutoDemoUpdateFocus(void);
void scAutoDemoFuncRun(GObj *gobj);
GObj *scAutoDemoMakeFocusInterface(void);
void scAutoDemoGetPlayerStartPosition(s32 mapobj_kind, Vec3f *mapobj_pos);
s32 scAutoDemoGetFighterKindsNum(u16 flag);
s32 scAutoDemoGetShuffledFighterKind(u16 this_mask, u16 prev_mask, s32 random);
s32 scAutoDemoGetFighterKind(s32 player);
s32 scAutoDemoGetPlayerDamage(s32 player);
void scAutoDemoInitDemo(void);
void scAutoDemoInitSObjs(void);
void scAutoDemoFuncStart(void);
void scAutoDemoStartScene(void);

/* The port's own bzero arm, src/dc/overlay.c's dSCManagerOverlays[64]. */
void scAutoDemoOverlayLoad(void);

#endif /* SSB_DC_SCAUTODEMO_H */

/* mnbackupclear.h -- the Backup Clear screen, mn/mnoption/mnbackupclear.c.
 *
 * The second of Options' two children (after Screen Adjust) -- reached from
 * the BACKUP CLEAR tab (mnoption.c's own mnOptionFuncRun) and nothing else.
 * Six tabs -- Newcomers Unlocked, 1P High Score, VS Battle Records, Bonus
 * Stage Time, Prize Data, All Data -- each a yes/no confirmation over the
 * game's own six `lbBackupClear*` functions (lb/lbbackup.c, compiled
 * unmodified: see mnBackupClearApplyOptionID below); B, or a confirmed clear,
 * returns to Options with the cursor already on this tab (mnOptionInitVars
 * reads scene_prev the same way it reads nSCKindScreenAdjust for the other
 * child).
 *
 * Every function is the decomp's by name and body, the REGION_US arms;
 * the line numbers are the decomp's. What is not here, and where it is
 * said:
 *   - the lighting pre-render (mnBackupClearFuncLights) and its display
 *     list/Lights1 data -- dMNBackupClearTaskmanSetup, the same cut
 *     every menu scene's pre-render takes;
 *   - the black clear camera -- mnBackupClearFuncStart, the PVR clears
 *     its own frame;
 *   - syVideoInit, the zbuffer setup and the arena_size line --
 *     mnBackupClearStartScene;
 *   - the reloc setup: three sprite banks stand in for the scene's
 *     three files -- mnBackupClearLoadFiles;
 *   - mnBackupClearMakeUnused's whole REGION_JP body (a second frame
 *     sprite behind the JP subtitle text) -- mnBackupClearMakeUnused
 *     itself, the same cut mnoption.c's mnOptionMakeMenuGObj took: the
 *     US arm makes the tracking GObj (so gcEjectGObj has something to
 *     eject on a tab change) and hangs nothing on it;
 *   - func_ovl53_80132928, one more decomp function the file itself
 *     never calls (unlike func_ovl53_801325CC just above it, which the
 *     B-button path does reach despite its own "unused?" comment) --
 *     dropped outright, the same call mnoption.c's four dead statics
 *     made.
 *
 * A clear writes through the real backing store every other save-
 * touching screen in this port already uses (gSCManagerBackupData,
 * lbBackupWrite -- src/dc/vmusave.c), not a stand-in: this screen adds
 * no new mechanism, only the six calls into lb/lbbackup.c that were
 * already sitting there unused.
 */
#ifndef SSB_DC_MNBACKUPCLEAR_H
#define SSB_DC_MNBACKUPCLEAR_H

#include <sys/obj.h>
#include <sys/taskman.h>

/* mnbackupclear.c:856 */
extern SYTaskmanSetup dMNBackupClearTaskmanSetup;

/* mnbackupclear.c:39-44, the three files' real ROM ids: 0 (mncommon.spr,
 * unread on the US arm -- mnbackupclear.h below says why), 77
 * (mnbackupclear.spr), 78 (mnbackupclearheader.spr). */
extern u32 dMNBackupClearFileIDs[3];

/* mnbackupclear.c:141 sMNBackupClearFiles, the three files' bases: the
 * game's void*[3] with a SpriteBank* in each (sprite.h). Not static in
 * the decomp; the host test reads sprites out of them by offset.
 * [0] is mncommon.spr (loaded, unread on the US arm -- the decomp's
 * own file list, kept for shape parity since the ported code still
 * indexes [1] and [2] by the decomp's own numbers), [1] is
 * mnbackupclear.spr, [2] is mnbackupclearheader.spr. */
extern void *sMNBackupClearFiles[3];

/* mnbackupclear.c:99, 108, 111, 120-132 -- the state the host test
 * reads: which tab the cursor is on, the confirm dialog's kind/yes-no
 * pick/flash length, the wait counters, and the 5-minute idle-return
 * tic. */
extern s32 sMNBackupClearOption;
extern sb32 sMNBackupClearOptionConfirmYesOrNo;
extern s32 sMNBackupClearOptionMenuKind;
extern s32 sMNBackupClearOptionChangeWait;
extern s32 sMNBackupClearTotalTimeTics;
extern s32 sMNBackupClearUpdateWait;
extern s32 sMNBackupClearOptionConfirmAnimLength;
extern s32 sMNBackupClearReturnTic;

/* mnbackupclear.c:78-105, the six tab GObjs, in nMNBackupClearOption
 * order, plus the "unused" tracking GObj and the confirm dialog's. */
extern GObj *sMNBackupClearOptionNewcomersGObj;
extern GObj *sMNBackupClearOption1PHighScoreGObj;
extern GObj *sMNBackupClearOptionBonusStageTimeGObj;
extern GObj *sMNBackupClearOptionVSRecordGObj;
extern GObj *sMNBackupClearOptionPrizeGObj;
extern GObj *sMNBackupClearOptionAllDataClearGObj;
extern GObj *sMNBackupClearUnusedGObj;
extern GObj *sMNBackupClearOptionConfirmGObj;

/* mnbackupclear.c:147-908, in the decomp's order. */
void mnBackupClearMakeUnused(s32 option);
void mnBackupClearMakeHeaderSObjs(void);
void mnBackupClearUpdateOptionTabColors(GObj *gobj, s32 status);
void mnBackupClearSetOptionSpriteColors(void);
void mnBackupClearEjectOptionGObjs(void);
void mnBackupClearOptionConfirmProcDisplay(GObj *gobj);
void mnBackupClearEjectOptionConfirmGObj(void);
void mnBackupClearMakeOptionConfirm(sb32 confirm_kind, sb32 yes_or_no);
void mnBackupClearMakeMainCamera(void);
void mnBackupClearInitVars(void);
void mnBackupClearApplyOptionID(s32 option);
void mnBackupClearUpdateOptionMainMenu(void);
void mnBackupClearUpdateOptionConfirmMenu(sb32 confirm_kind);
void mnBackupClearFuncRun(GObj *gobj);
void mnBackupClearFuncStart(void);

/* mnbackupclear.c:900-908 0x80132E28: one task, and the scene is over
 * when it ends. */
void mnBackupClearStartScene(void);

/* The bzero arm of syDmaLoadOverlay for overlay 53 (src/dc/overlay.h). */
void mnBackupClearOverlayLoad(void);

#endif /* SSB_DC_MNBACKUPCLEAR_H */

/* mncharacters.h -- see mncharacters.c. The Character Data
 * tab, the third of DATA's three tabs (mndata.c, mnvsrecord.c,
 * mnsoundtest.c). Function-for-function port of
 * mn/mndata/mncharacters.c: every declaration is the decomp's by name.
 */
#ifndef SSB_DC_MNCHARACTERS_H
#define SSB_DC_MNCHARACTERS_H

#include <ssb_types.h>
#include <sys/objdef.h>
#include <sys/taskman.h>
#include <PR/gbi.h>
#include <mn/mndef.h>

/* mncharacters.c:2830 */
extern SYTaskmanSetup dMNCharactersTaskmanSetup;

/* mncharacters.c:1210-1216, the port's own copy of the file table. */
extern u32 dMNCharactersFileIDs[4];

/* mncharacters.c:1238-1329, the state the host test reads: which
 * fighter's page is showing, the five section GObjs, and the demo
 * fighter's own GObj (this file's header note on ftMainGetStatusDesc's
 * FTSTAT_CHARDATA_START fix -- ftGetStruct(sMNCharactersFighterGObj) is
 * how the test proves it resolved). Not static in the decomp. */
extern void *sMNCharactersFiles[4];
extern s32 sMNCharactersPage;
extern GObj *sMNCharactersEmblemGObj;
extern GObj *sMNCharactersNameGObj;
extern GObj *sMNCharactersStoryGObj;
extern GObj *sMNCharactersWorksGObj;
extern GObj *sMNCharactersFighterGObj;

s32 mnCharactersGetFighterKind(s32 page);
s32 mnCharactersGetPage(s32 fkind);
void mnCharactersMakeStory(s32 fkind);
void mnCharactersMakeDecals(void);
void mnCharactersMakeEmblem(s32 fkind);
void mnCharactersMakeName(s32 fkind);
void mnCharactersMakeWorksWallpaper(void);
void mnCharactersMakeWorks(s32 fkind);
void mnCharactersSetFighterScale(GObj *fighter_gobj, s32 fkind);
void mnCharactersSetFighterPosition(GObj *fighter_gobj, s32 fkind);
MNCharactersMotion *mnCharactersGetMotion(MNCharactersMotion *motion, s32 fkind, s32 motion_kind, s32 unused, s32 track);
sb32 mnCharactersCheckFighterAnimEnd(GObj *fighter_gobj);
void mnCharactersInitRecentMotionKinds(void);
sb32 mnCharactersCheckRecentMotionKind(s32 motion_kind);
s32 mnCharactersRandMotionKind(void);
MNCharactersMotion *mnCharactersSetMotion(MNCharactersMotion *motion, s32 motion_kind);
MNCharactersMotion *mnCharactersAdvanceTrack(MNCharactersMotion *motion, s32 unused);
void mnCharactersFighterProcUpdate(GObj *fighter_gobj);
void mnCharactersMakeFighter(s32 fkind);
s32 mnCharactersGetMotionKind(void);
void mnCharactersUpdateMotionName(GObj *gobj);
void mnCharactersMakeMotionName(void);
void mnCharactersMakeStoryCamera(void);
void mnCharactersMakeDecalsCamera(void);
void mnCharactersMakeEmblemCamera(void);
void mnCharactersMakeNameCamera(void);
void mnCharactersMakeWorksWallpaperCamera(void);
void mnCharactersMakeWorksCamera(void);
void mnCharactersMakeFighterCamera(void);
sb32 mnCharactersCheckHaveFighterKind(s32 fkind);
void mnCharactersInitVars(void);
void mnCharactersBackupFighterKind(void);
void mnCharactersChangeFighter(s32 fkind);
void mnCharactersMoveFighterCamera(CObj *cobj, f32 angle, s32 unused);
void mnCharactersResetFighterCamera(void);
void mnCharactersUpdateScene(void);
void mnCharactersUpdateSceneDemo(void);
void mnCharactersFuncRun(GObj *gobj);
void mnCharactersFuncStart(void);
void mnCharactersStartScene(void);
void mnCharactersOverlayLoad(void);      /* dSCManagerOverlays[33] */

#endif /* SSB_DC_MNCHARACTERS_H */

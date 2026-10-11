/* mndata.h -- the DATA menu, mn/mndata/mndata.c.
 *
 * The third option on the mode select, and the spine under the three
 * screens behind it: CHARACTERS, VS RECORD and SOUND TEST. The screen
 * itself is small -- the SMASH BROS. collage as a wallpaper, two paper
 * decals, a dark DATA icon, the logo and the word DATA in an orange
 * panel, and two or three option tabs stacked down the middle -- and
 * the stick moves between the tabs while A or START enters one.
 *
 * SOUND TEST is the third tab only when it has been unlocked
 * (gSCManagerBackupData.unlock_mask, which the unlock message writes to
 * the memory card -- src/dc/mnmessage.c). With it the three tabs sit
 * higher and closer together; without it there are two, lower and
 * further apart. That choice is made once in mnDataInitVars and every
 * maker below reads it, so it is the one piece of state the whole
 * screen's layout hangs on.
 *
 * Every function is the decomp's by name and body, the REGION_US arms;
 * the line numbers are the decomp's. What is not here, and where it is
 * said:
 *   - the lighting pre-render (mnDataFuncLights) and the clear camera
 *     -- dMNDataTaskmanSetup and mnDataFuncStart;
 *   - syVideoInit and the arena_size line -- mnDataStartScene;
 *   - the reloc setup: two sprite banks stand in for the two files --
 *     mnDataLoadFiles;
 *   - the RDP commands of mnDataLabelsProcDisplay, in the port's
 *     spelling (src/dc/lbcommon.h);
 *   - the JP arm of mnDataMakeMenuGObj, which is the whole function:
 *     the US build makes the GObj and hangs nothing on it, and
 *     mnDataSetSubtitleSpriteColors goes unused with it.
 *
 * The three screens this menu leads to are not ported. Each option
 * still sets gSCManagerSceneData.scene_curr as the game does, so the
 * scene manager's default arm bounces straight back here and
 * mnDataInitVars puts the cursor on the tab that was chosen -- which is
 * the game's own return route (src/dc/scmanager.c). Until mnvsrecord.c,
 * mncharacters.c and mnsoundtest.c land, that is what pressing A does.
 */
#ifndef SSB_DC_MNDATA_H
#define SSB_DC_MNDATA_H

#include <sys/obj.h>
#include <sys/taskman.h>

/* mndata.c:369 */
extern SYTaskmanSetup dMNDataTaskmanSetup;

/* mndata.c:110 sMNDataFiles, the two files' bases: the game's void*[2]
 * with a SpriteBank* in each (sprite.h). Not static in the decomp; the
 * host test reads sprites out of them by offset. */
extern void *sMNDataFiles[2];

/* mndata.c:60-97, the state the host test reads: which tab the cursor
 * is on, the two ends it moves between, and whether SOUND TEST is one
 * of them. */
extern s32 sMNDataOption;
extern s32 sMNDataFirstAvailableOption;
extern s32 sMNDataLastAvailableOption;
extern sb32 sMNDataIsHaveSoundTest;

/* mndata.c:56-68, the three option tabs' GObjs, in nMNDataOption
 * order. */
extern GObj *sMNDataOptionCharactersGObj;
extern GObj *sMNDataOptionVSRecordGObj;
extern GObj *sMNDataOptionSoundTestGObj;

/* mndata.c:121-868, in the decomp's order. */
sb32 mnDataCheckSoundTestUnlocked(void);
void mnDataSetOptionSpriteColors(GObj *gobj, s32 status);
void mnDataMakeOptionTab(GObj *gobj, f32 pos_x, f32 pos_y, s32 lrs);
void func_ovl61_80131D54(void);
void mnDataMakeCharacters(void);
void mnDataMakeVSRecord(void);
void mnDataMakeSoundTest(void);
void mnDataSetSubtitleSpriteColors(SObj *sobj);
void mnDataMakeMenuGObj(void);
void mnDataLabelsProcDisplay(GObj *gobj);
void mnDataMakeLabels(void);
void mnDataMakeDecals(void);
void mnDataMakeLink3Camera(void);
void mnDataMakeOptionsCamera(void);
void mnDataMakeLabelsCamera(void);
void mnDataMakeDecalsCamera(void);
void mnDataInitVars(void);
void mnDataFuncRun(GObj *gobj);
void mnDataFuncStart(void);

/* mndata.c:868 0x80132EC0: one task, and the scene is over when it
 * ends. */
void mnDataStartScene(void);

/* The bzero arm of syDmaLoadOverlay for overlay 61 (src/dc/overlay.h). */
void mnDataOverlayLoad(void);

#endif /* SSB_DC_MNDATA_H */

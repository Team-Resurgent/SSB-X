/* mnsoundtest.h -- the Sound Test screen, mn/mndata/mnsoundtest.c.
 *
 * The third of DATA's three tabs: three columns (Music, Sound, Voice) the player scrolls with
 * the D-pad or stick, each a raw index into one of this file's own ID
 * tables, with A playing the current selection, Z stopping it and START
 * fading the music out over two seconds. Every function is the decomp's
 * by name and body, the REGION_US arms; the line numbers are the
 * decomp's.
 *
 * What is not here, and where it is said:
 *   - the lighting pre-render (mnSoundTestFuncLights) and the clear
 *     camera -- dMNSoundTestTaskmanSetup and mnSoundTestFuncStart;
 *   - syVideoInit and the arena_size line -- mnSoundTestStartScene;
 *   - the reloc setup: five sprite banks stand in for the five files,
 *     three of them (mncommon, ifdamage, ifpause) reused from banks
 *     already on the disc for the menus and the battle HUD --
 *     mnSoundTestLoadFiles;
 *   - the three divider-line functions' raw RDP fill-rectangle pairs,
 *     in the port's spelling (src/dc/lbcommon.h), the same idiom
 *     src/dc/mnvsrecord.c's grid functions use, including
 *     syVideoGetFillColor's cut -- the port's most-applied divergence,
 *     dropped the same way syVideoInit is everywhere else.
 *
 * DIVERGES, and the reason is sound RAM, not this file: this screen is
 * built to audition nearly every sample and every music track the game
 * has (dMNSoundTestSoundIDs and dMNSoundTestVoiceIDs alone name 194 and
 * 285 IDs), which is the entire tier-A sound corpus src/dc/sndres.h says
 * will not fit in 2 MB at once. So this scene is not its own sndres
 * group -- src/dc/sndres.c gives it the same "menu" group as every other
 * menu scene, the small always-safe set of cues the scene manager
 * already carries between screens. Whatever the player scrolls to that
 * is not already in that set does not play, and fgm.c and the firmware log it
 * ("... is not resident") the same way any other not-yet-staged sound
 * anywhere in the game would -- fgm-audio-census (memory) says this is
 * the standing, already-documented shape of the constraint, not a new
 * one this screen introduces. Every A/Z/START call below is still made
 * exactly as the decomp has it; what plays is what the platform holds.
 */
#ifndef SSB_DC_MNSOUNDTEST_H
#define SSB_DC_MNSOUNDTEST_H

#include <sys/obj.h>
#include <sys/taskman.h>

/* mnsoundtest.c:638. */
extern SYTaskmanSetup dMNSoundTestTaskmanSetup;

/* mnsoundtest.c:588-595, the five files' bases: the game's void*[5] with
 * a SpriteBank* in each (sprite.h). Not static in the decomp; the host
 * test reads sprites out of them by offset. */
extern void *sMNSoundTestFiles[5];

/* mnsoundtest.c:692-722, the state the host test reads: which column is
 * selected, each column's own ID index, and the direction the D-pad or
 * stick last moved (mnSoundTestArrowsThreadUpdate's blink reset). */
extern s32 sMNSoundTestOption;
extern s32 sMNSoundTestOptionSelectID[/* nMNSoundTestOptionEnumCount */];
extern s32 sMNSoundTestDirectionInputKind;
extern s32 sMNSoundTestFadeOutWait;

/* mnsoundtest.c:737-1741, in the decomp's order. mnSoundTestFuncLights is
 * not here: it is cut with the rest of the pre-render (this header's
 * comment above says where). */
void mnSoundTestUpdateOptionColors(void);
void mnSoundTestUpdateControllerInputs(void);
void mnSoundTestUpdateFunctions(void);
void mnSoundTestFuncRun(GObj *gobj);
SObj *mnSoundTestMakeHeaderSObjs(void);
void mnSoundTestOptionThreadUpdate(GObj *gobj);
void mnSoundTestMusicProcDisplay(GObj *gobj);
SObj *mnSoundTestMakeMusicSObjs(void);
void mnSoundTestSoundProcDisplay(GObj *gobj);
SObj *mnSoundTestMakeSoundSObjs(void);
void mnSoundTestVoiceProcDisplay(GObj *gobj);
SObj *mnSoundTestMakeVoiceSObjs(void);
SObj *mnSoundTestMakeAButtonSObj(GObj *gobj);
SObj *mnSoundTestMakeBButtonSObj(GObj *gobj);
SObj *mnSoundTestMakeStartButtonSObj(GObj *gobj);
SObj *mnSoundTestMakeAFunctionSObj(GObj *gobj);
SObj *mnSoundTestMakeStartFunctionSObj(GObj *gobj);
SObj *mnSoundTestMakeBFunctionSObj(GObj *gobj);
void mnSoundTestMakeButtonSObjs(void);
void mnSoundTestMakeNumberSObj(GObj *gobj);
void mnSoundTestUpdateNumberPositions(GObj *gobj, f32 width);
void mnSoundTestUpdateNumberSprites(GObj *gobj);
void mnSoundTestSelectIDThreadUpdate(GObj *gobj);
void mnSoundTestMakeSelectIDGObjs(void);
void mnSoundTestArrowsThreadUpdate(GObj *gobj);
void mnSoundTestMakeArrowSObjs(void);
void mnSoundTestMakeAllSObjs(void);
void mnSoundTestMakeCameras(void);
void mnSoundTestInitVars(void);
void mnSoundTestFuncStart(void);

/* mnsoundtest.c:1734: one task, and the scene is over when it ends. */
void mnSoundTestStartScene(void);

/* The bzero arm of syDmaLoadOverlay for overlay 62 (src/dc/overlay.h). */
void mnSoundTestOverlayLoad(void);

#endif /* SSB_DC_MNSOUNDTEST_H */

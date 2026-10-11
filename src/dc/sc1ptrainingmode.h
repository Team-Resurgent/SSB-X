/* sc1ptrainingmode.h -- Training mode itself,
 * sc/sc1pmode/sc1ptrainingmode.c. Overlay 7.
 *
 * The back half of the one 1P mode that needs no boss, no Polygon team,
 * no ladder and no bonus map: src/dc/mnplayers1ptraining.c picks the
 * two fighters, this plays them. It is a battle scene -- so
 * src/dc/scvsbattle.c is its template, and every cut here is that
 * file's cut for the same reason -- with a pause menu and a HUD of its
 * own on top:
 *
 *   - a four-field stat line (DAMAGE, COMBO, ENEMY, SPEED) that reads
 *     the human's damage and hit count every tic;
 *   - a six-row menu (CP, ITEM, SPEED, VIEW, RESET, EXIT) the human
 *     opens with Start, whose CP row sets the dummy's
 *     FTComputerBehavior and whose SPEED row runs the whole scene at
 *     1/2, 1/4 or 2/3 by dropping tics (sc1PTrainingModeCheckLagTic);
 *   - the ITEM row, which spawns one of the 21 items on a timer;
 *   - RESET and EXIT, which leave sc1PTrainingModeFuncUpdate's loop
 *     through sSC1PTrainingModeMenu.exit_or_reset -- the scene's
 *     StartScene runs scManagerFuncUpdate in a do/while on it, so a
 *     reset is a fresh FuncStart and an exit falls out to
 *     nSCKindPlayers1PTraining.
 *
 * Every function is the decomp's by name and body, the REGION_US arms;
 * the line numbers are the decomp's. What is not here, and where it is
 * said:
 *   - the lighting pre-render (sc1PTrainingModeFuncLights) --
 *     dSC1PTrainingModeTaskmanSetup, as scvsbattle.c's;
 *   - syVideoInit, the z-buffer and the arena_size line --
 *     sc1PTrainingModeStartScene;
 *   - the reloc setup: one sprite bank stands in for file 254 and for
 *     the IFCommonItemNames file behind it -- sc1PTrainingModeLoadSprites
 *     (sc1PTrainingModeSetupFiles keeps its load, which is the HUD's);
 *   - the reloc heap the training wallpaper is forced into -- each of
 *     the three is a sprite bank (sc1PTrainingModeLoadWallpaper), and
 *     the stage draws it in place of its own (stage_set_wallpaper_override).
 */
#ifndef SSB_DC_SC1PTRAININGMODE_H
#define SSB_DC_SC1PTRAININGMODE_H

#include <sc/scene.h>
#include <sys/obj.h>
#include <sys/taskman.h>

extern SYTaskmanSetup dSC1PTrainingModeTaskmanSetup;

void sc1PTrainingModeSetMenuGObjFlags(u32 flags);
void sc1PTrainingModeCheckEnterMenu(void);
void sc1PTrainingModeCheckLeaveMenu(void);
void sc1PTrainingModeUpdateMenuInputs(void);
void sc1PTrainingModeUpdateScroll(void);
sb32 sc1PTrainingModeCheckUpdateOptionID(s32 *option, s32 option_min, s32 option_max);
sb32 sc1PTrainingModeUpdateCPOption(void);
s32 sc1PTrainingModeGetItemCount(void);
sb32 sc1PTrainingModeUpdateItemOption(void);
sb32 sc1PTrainingModeUpdateSpeedOption(void);
sb32 sc1PTrainingModeUpdateViewOption(void);
sb32 sc1PTrainingModeUpdateResetOption(void);
sb32 sc1PTrainingModeUpdateExitOption(void);
void sc1PTrainingModeUpdateMainOption(void);
void sc1PTrainingModeUpdateMenu(void);
sb32 sc1PTrainingModeCheckLagTic(void);
void sc1PTrainingModeUpdateAll(void);
void sc1PTrainingModeFuncUpdate(void);
void sc1PTrainingModeInitVars(void);
void sc1PTrainingModeLoadSprites(void);
void sc1PTrainingModeLoadWallpaper(void);
void sc1PTrainingModeInitDisplayVars(void);
SObj* sc1PTrainingModeMakeStatDisplay(GObj *interface_gobj, SC1PTrainingModeSprites *ts);
void sc1PTrainingModeMakeStatDisplayText(void);
void sc1PTrainingModeUpdateDamageDisplay(GObj *interface_gobj, s32 damage);
void sc1PTrainingModeDamageDisplayProcDisplay(GObj *interface_gobj);
void sc1PTrainingModeDamageDisplayProcUpdate(GObj *interface_gobj);
void sc1PTrainingModeInitStatDisplayCharacterSprites(void);
void sc1PTrainingModeInitSpriteEnvColors(SObj *sobj);
void sc1PTrainingModeMakeDamageDisplay(void);
void sc1PTrainingModeUpdateComboDisplay(GObj *interface_gobj, s32 combo);
void sc1PTrainingModeComboDisplayProcUpdate(GObj *interface_gobj);
void sc1PTrainingModeComboDisplayProcDisplay(GObj *interface_gobj);
void sc1PTrainingModeMakeComboDisplay(void);
void sc1PTrainingModeUpdateSpeedDisplaySprite(void);
void sc1PTrainingModeMakeSpeedDisplay(void);
void sc1PTrainingModeUpdateCPDisplaySprite(void);
void sc1PTrainingModeMakeCPDisplay(void);
void sc1PTrainingModeUpdateItemDisplaySprite(void);
void sc1PTrainingModeItemDisplayProcDisplay(GObj *interface_gobj);
void sc1PTrainingModeMakeItemDisplay(void);
void sc1PTrainingModeMakeStatDisplayAll(void);
void sc1PTrainingModeMakeMenuLabels(void);
void sc1PTrainingModeInitMenuOptionSpriteAttrs(void);
void sc1PTrainingModeMenuProcDisplay(GObj *interface_gobj);
void sc1PTrainingModeMakeMenu(void);
void sc1PTrainingModeInitCPOptionSpriteColors(void);
void sc1PTrainingModeUpdateCPOptionSprite(void);
void sc1PTrainingModeMakeCPOption(void);
void sc1PTrainingModeUpdateItemOptionSprite(void);
void sc1PTrainingModeInitItemOptionSpriteColors(void);
void sc1PTrainingModeMakeItemOption(void);
void sc1PTrainingModeInitSpeedOptionSpriteColors(void);
void sc1PTrainingModeUpdateSpeedOptionSprite(void);
void sc1PTrainingModeMakeSpeedOption(void);
void func_ovl7_8018F41C(void);
void sc1PTrainingModeUpdateViewOptionSprite(void);
void sc1PTrainingModeViewOptionProcUpdate(GObj *interface_gobj);
void sc1PTrainingModeMakeViewOption(void);
void sc1PTrainingModeSetHScrollOptionSObjs(void);
void sc1PTrainingModeInitOptionArrowSpriteColors(SObj *sobj);
void sc1PTrainingModeUpdateOptionArrows(void);
void sc1PTrainingModeMakeOptionArrows(void);
s32 sc1PTrainingModeGetOptionSpriteID(void);
void sc1PTrainingModeUpdateCursorPosition(void);
void sc1PTrainingModeMakeCursor(void);
void sc1PTrainingModeSetVScrollOptionSObjs(void);
void sc1PTrainingModeUnderlineProcDisplay(GObj *interface_gobj);
void sc1PTrainingModeUpdateUnderline(void);
void sc1PTrainingModeMakeUnderline(void);
void sc1PTrainingModeMakeMenuAll(void);
void sc1PTrainingModeSetPlayDefaultBGM(void);
void sc1PTrainingModeSetGameStatusGo(void);
void sc1PTrainingModeUpdateDummyBehavior(void);
void sc1PTrainingModeFuncStart(void);
void sc1PTrainingModeStartScene(void);
void sc1PTrainingModeSetupFiles(void);

/* The port's own bzero arm, src/dc/overlay.c's dSCManagerOverlays[7]. */
void sc1PTrainingModeOverlayLoad(void);

#endif /* SSB_DC_SC1PTRAININGMODE_H */

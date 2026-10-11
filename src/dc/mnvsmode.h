/* mnvsmode.h -- the VS mode menu, mn/mnvsmode/mnvsmode.c.
 *
 * The screen after VS MODE: the collage again, two torn-paper decals,
 * the dark console icon behind everything, and four option tabs down a
 * diagonal -- VS START, RULE, TIME/STOCK (which of the two the rule
 * decides), VS OPTIONS -- each a left cap, a tiled middle and a right
 * cap, coloured for whether the cursor is on it. RULE's value (TIME,
 * STOCK, TIME TEAM, STOCK TEAM) and the count beside TIME/STOCK are
 * sprites remade on every change, with orange arrows that blink beside
 * the option the cursor is on. The menu name -- the logo, VS, GAME MODE
 * -- sits on a paper-coloured rectangle at the bottom right.
 *
 * Up and down move the cursor round the four; left and right on RULE
 * step the rule, on TIME/STOCK the count (1-99 minutes then infinity,
 * 1-99 stocks); A or START on VS START saves the settings and goes to
 * the character select, on VS OPTIONS to the options; B saves and goes
 * back to the mode select; five idle minutes go to the title. The
 * settings are gSCManagerTransferBattleState's rule, time and stock
 * fields, read on the way in and written on the way out. Four sprite
 * cameras, one per DL link: the background on 0, the menu name on 1,
 * the tabs on 2, the values on 3.
 *
 * Every function is the decomp's by name and body; the REGION_JP arms
 * (the subtitles, which the US build has no sprites for) stay under
 * their #if as the decomp has them. What is not here, and where it is
 * said:
 *   - the clear camera (gcMakeDefaultCameraGObj) and the lighting
 *     pre-render (mnVSModeFuncLights), for 3D this scene has none of
 *     -- mnVSModeFuncStart, dMNVSModeTaskmanSetup;
 *   - syVideoInit and the arena_size line -- mnVSModeStartScene;
 *   - the reloc setup: two sprite banks stand in for the two files --
 *     mnVSModeLoadFiles;
 *   - the nine RDP commands that fill the menu name's rectangle are one
 *     fill quad -- mnVSModeRenderMenuName.
 *
 * Where VS START leads is the scene manager's business
 * (src/dc/scmanager.c): the scene asks for nSCKindPlayersVS as the
 * game's does, and the manager decides what the port has for it. */
#ifndef SSB_DC_MNVSMODE_H
#define SSB_DC_MNVSMODE_H

#include <sys/obj.h>
#include <sys/taskman.h>

/* mnvsmode.c:1655 */
extern SYTaskmanSetup dMNVSModeTaskmanSetup;

/* mnvsmode.c:112 sMNVSModeFiles, the two files' bases: the game's
 * void*[2] with a SpriteBank* in each (sprite.h). Not static in the
 * decomp; the host test reads sprites out of them by offset. */
extern void *sMNVSModeFiles[2];

s32 mnVSModePow(s32 num, s32 pow);
void mnVSModeSetTextureColors(SObj *sobj, u32 *colors);
s32 mnVSModeGetNumberOfDigits(s32 num, s32 maxDigits);
void mnVSModeMakeNumber(GObj *number_gobj, s32 num, f32 x, f32 y, u32 *colors, s32 maxDigits, sb32 pad);
void mnVSModeUpdateButton(GObj *button_gobj, s32 button_status);
void mnVSModeMakeButton(GObj *button_gobj, f32 x, f32 y, s32 arg3);
void mnVSModeMakeVSStartButton(void);
void mnVSModeMakeRuleValue(void);
SObj *mnVSModeGetArrowSObj(GObj *arrow_gobj, s32 direction);
void mnVSModeMakeLeftArrow(GObj *arrow_gobj, f32 x, f32 y);
void mnVSModeMakeRightArrow(GObj *arrow_gobj, f32 x, f32 y);
void mnVSModeAnimateRuleArrows(GObj *rule_arrows_gobj);
void mnVSModeMakeRuleArrows(void);
void mnVSModeAnimateTimeStockArrows(GObj *time_stock_arrows_gobj);
void mnVSModeMakeTimeStockArrows(void);
void mnVSModeMakeRuleButton(void);
sb32 mnVSModeIsTime(void);
s32 mnVSModeGetTimeStockValue(void);
void mnVSModeMakeTimeStockValue(void);
void mnVSModeMakeTimeStockButton(void);
void mnVSModeMakeVSOptionsButton(void);
void mnVSModeSetSubtitleSpriteColors(SObj *sobj);
void mnVSModeMakeSubtitle(void);
void mnVSModeRenderMenuName(GObj *menu_name_gobj);
void mnVSModeMakeMenuName(void);
void mnVSModeMakeBackground(void);
void mnVSModeMakeButtonValuegSYRdpViewport(void);
void mnVSModeMakeButtonViewport(void);
void mnVSModeMakeMenuNameViewport(void);
void mnVSModeMakeBackgroundViewport(void);
void mnVSModeFuncStartVars(void);
void mnVSModeSaveSettings(void);
s32 mnVSModeGetShade(s32 player);
s32 mnVSModeGetCostume(s32 fkind, s32 arg1);
void mnVSModeSetCostumesAndShades(void);
void mnVSModeMain(GObj *gobj);
void mnVSModeLoadFiles(void);
void mnVSModeFuncStart(void);
void mnVSModeStartScene(void);

#endif /* SSB_DC_MNVSMODE_H */

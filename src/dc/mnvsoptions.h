/* mnvsoptions.h -- the VS options screen, mn/mnvsmode/mnvsoptions.c.
 *
 * The screen behind VS OPTIONS on the VS mode menu: the collage under a
 * dark red tint, the console icon decal over it, VS OPTIONS in gold on
 * a grey bar, and four or five options down the left -- HANDICAP
 * (ON/AUTO/OFF), TEAM ATTACK (ON/OFF), STAGE SELECT (ON/OFF), DAMAGE
 * (50-200%), and ITEM SWITCH, which is there only once the save data
 * has unlocked it. Each option is a bubble and a label, orange where
 * the cursor is; the toggles are two or three words with a red one and
 * grey ones, underlined by a red bar the display proc fills.
 *
 * Up and down move the cursor between the first and last available
 * options, wrapping; left and right step the option under it -- the
 * handicap through its three settings, the two toggles between on and
 * off, the damage ratio round 50..200; A cycles the option under the
 * cursor instead, and on ITEM SWITCH opens the item switch screen. B
 * saves and goes back to the VS mode menu, and five idle minutes save
 * and go to the title. The settings are gSCManagerTransferBattleState's
 * handicap, is_team_attack, is_stage_select and damage_ratio, read on
 * the way in (mnVSOptionsInitVars) and written on every way out
 * (mnVSOptionsSetAllSettings) -- and turning the handicap off resets
 * every player's to the default, which is what makes this screen matter
 * to a battle.
 *
 * Five sprite cameras, one per DL link: the wallpaper on 0, the label
 * and underline on 1, the options on 2, the tint on 3, the decal on 4.
 *
 * Every function is the decomp's by name and body, REGION_US arms; the
 * REGION_JP subtitles stay under their #if as the decomp has them.
 * What is not here, and where it is said:
 *   - the clear camera (gcMakeDefaultCameraGObj) and the lighting
 *     pre-render (mnVSOptionsFuncLights), for 3D this scene has none of
 *     -- mnVSOptionsFuncStart, dMNVSOptionsTaskmanSetup;
 *   - syVideoInit and the arena_size line -- mnVSOptionsStartScene;
 *   - the reloc setup: two sprite banks stand in for the two files --
 *     mnVSOptionsLoadFiles;
 *   - the RDP commands of the two display procs are fill quads --
 *     mnVSOptionsLabelProcDisplay, mnVSOptionsUnderlineProcDisplay.
 *
 * ITEM SWITCH is locked on a fresh save (lb/lbbackup.c unlocks it after
 * a hundred item battles) and a fresh save has nothing unlocked, so
 * gSCManagerBackupData.unlock_mask is zero and the option is absent,
 * which is what the game does with the same save data. When it is
 * there, A on it asks for nSCKindVSItemSwitch, which the port does not
 * have: the scene manager's default arm comes back here with
 * scene_prev set to it, and mnVSOptionsInitVars puts the cursor back on
 * ITEM SWITCH, which is the game's own return route. */
#ifndef SSB_DC_MNVSOPTIONS_H
#define SSB_DC_MNVSOPTIONS_H

#include <sys/obj.h>
#include <sys/taskman.h>

/* mnvsoptions.c:1517 */
extern SYTaskmanSetup dMNVSOptionsTaskmanSetup;

/* mnvsoptions.c:110 sMNVSOptionsFiles, the two files' bases: the game's
 * void*[2] with a SpriteBank* in each (sprite.h). Not static in the
 * decomp; the host test reads sprites out of them by offset. */
extern void *sMNVSOptionsFiles[2];

sb32 mnVSOptionsCheckHaveItemSwitch(void);
s32 mnVSOptionsGetPowerOf(s32 base, s32 exp);
void mnVSOptionsSetDamageDigitSpriteColors(SObj *sobj, u32 *colors);
s32 mnVSOptionsGetDamageDigitCount(s32 damage, s32 digit_count_max);
void mnVSOptionsMakeDamageDigitSObjs(GObj *gobj, s32 damage, f32 pos_x, f32 pos_y, u32 *colors, s32 digit_count_max, sb32 is_fixed_digit_count);
void mnVSOptionsSetOptionSpriteColors(GObj *gobj, s32 status);
void mnVSOptionsSetToggleSpriteColors(GObj *gobj, s32 status);
void mnVSOptionsMakeOnOffToggle(GObj *gobj, f32 pos_x, f32 pos_y);
void mnVSOptionsSetHandicapSpriteColors(GObj *gobj, s32 setting);
void mnVSOptionsMakeHandicapToggle(GObj *gobj, f32 pos_x, f32 pos_y);
void mnVSOptionsMakeDamageDigits(void);
void mnVSOptionsMakeDamageOption(void);
void mnVSOptionsMakeItemSwitchOption(void);
void mnVSOptionsMakeStageSelectOption(void);
void mnVSOptionsMakeTeamAttackOption(void);
void mnVSOptionsMakeHandicapOption(void);
void mnVSOptionsLabelProcDisplay(GObj *gobj);
void mnVSOptionsMakeLabel(void);
void mnVSOptionsSetSubtitleSpriteColors(SObj *sobj);
void mnVSOptionsMakeSubtitle(void);
void mnVSOptionsMakeWallpaper(void);
void mnVSOptionsUnderlineProcDisplay(GObj *gobj);
void mnVSOptionsMakeUnderline(void);
void mnVSOptionsTintProcDisplay(GObj *gobj);
void mnVSOptionsMakeTint(void);
void mnVSOptionsMakeDecal(void);
void mnVSOptionsMakeTintCamera(void);
void mnVSOptionsMakeOptionCamera(void);
void mnVSOptionsMakeLabelUnderlineCamera(void);
void mnVSOptionsMakeWallpaperCamera(void);
void mnVSOptionsMakeDecalCamera(void);
void mnVSOptionsInitVars(void);
void mnVSOptionsSetAllSettings(void);
void mnVSOptionsSetHandicapSettings(void);
void mnVSOptionsFuncRun(GObj *gobj);
void mnVSOptionsLoadFiles(void);
void mnVSOptionsFuncStart(void);
void mnVSOptionsStartScene(void);

#endif /* SSB_DC_MNVSOPTIONS_H */

/* mnvsitemswitch.h -- the item switch, mn/mnvsmode/mnvsitemswitch.c.
 *
 * The screen behind the VS options' last row, unlocked after a hundred
 * VS matches queue an unlock message, which sets LBBACKUP_UNLOCK_MASK_ITEMSWITCH.
 * mnVSOptionsCheckHaveItemSwitch then grows the options screen a row that leads
 * here. Until that happens the row is not drawn and this scene is unreachable,
 * which is the game's behaviour and now the port's.
 *
 * What it is: a list of fifteen item names down the middle of the
 * screen with an ON/OFF toggle beside each, and above them the
 * appearance rate in six steps from NONE to VERY HIGH. A yellow cursor
 * picks a row; left and right (and A) work it. B saves and goes back.
 * Three sprite cameras draw it, on DL links 1 (the labels, the list,
 * the toggles and the rate), 3 (the cursor) and 4 (the button decal).
 *
 * The settings live on gSCManagerTransferBattleState:
 * `item_appearance_rate` is the rate as an nSCBattleItemSwitch*, and
 * `item_toggles` a bitmask over nITKind* -- so the scene's fifteen
 * toggles are fifteen bits of the same word ft/ and it/ read when a
 * match spawns items. mnVSItemSwitchSetItemToggles is where they go
 * back, and it is the one piece of this file with real logic in it:
 * every toggle off zeroes the mask outright, and any toggle on adds the
 * four containers (egg, capsule, barrel, crate) that have no switch of
 * their own. The green shell's toggle carries the red one.
 *
 * Nothing reads item_toggles yet -- it/ is not ported, and a match
 * spawns no items -- so what this scene changes is a battle state field
 * that is written and not yet used. That is the same order the port
 * took the rest of the VS options in, and the field is
 * checked where it is set rather than where it is read.
 *
 * Every function is the decomp's by name and body, the REGION_US arms.
 * What is not here, and where it is said:
 *   - the clear camera and the lighting pre-render
 *     (mnVSItemSwitchFuncLights) -- dMNVSItemSwitchTaskmanSetup;
 *   - syVideoInit and the arena_size line -- mnVSItemSwitchStartScene;
 *   - the reloc setup: one sprite bank stands in for the one file --
 *     mnVSItemSwitchLoadFiles;
 *   - the RDP commands of mnVSItemSwitchLabelsProcDisplay, in the
 *     port's spelling (src/dc/lbcommon.h);
 *   - the JP subtitle, which is the whole body of mnItemSwitchMakeSubtitle
 *     and twenty-three sprites of the file: the US build makes the GObj,
 *     puts nothing on it, and ejects and remakes it on every cursor
 *     move. Ported as it stands, because that GObj is real.
 */
#ifndef SSB_DC_MNVSITEMSWITCH_H
#define SSB_DC_MNVSITEMSWITCH_H

#include <sys/obj.h>
#include <sys/taskman.h>

/* mnvsitemswitch.c:865 */
extern SYTaskmanSetup dMNVSItemSwitchTaskmanSetup;

/* mnvsitemswitch.c:110 sMNVSItemSwitchFiles, the one file's base: the
 * game's void*[1] with a SpriteBank* in it (sprite.h). Not static in
 * the decomp; the host test reads sprites out of it by offset. */
extern void *sMNVSItemSwitchFiles[1];

/* mnvsitemswitch.c:80, 85, 88: which row the cursor is on, and the
 * sixteen rows' states -- [0] the appearance rate as an
 * nSCBattleItemSwitch*, [1..15] nMNOptionTabStatusOn/Off per item. The
 * host test drives the pad and reads all three. */
extern s32 sMNVSItemSwitchOptionSelectID;
extern sb32 sMNVSItemSwitchOptionStatuses[16];
extern GObj *sMNVSItemSwitchOptionGObjs[16];

/* mnvsitemswitch.c:37, the toggle rows' item kinds; [0] is the
 * appearance rate's row and is not an item. */
extern s32 dMNVSItemSwitchTogglesItemKinds[16];

/* mnvsitemswitch.c:658 and :686, the two the settings pass through:
 * every toggle off is an empty mask, anything on carries the four
 * containers with it. */
sb32 mnVSItemSwitchCheckAllTogglesOff(void);
void mnVSItemSwitchSetItemToggles(void);

/* mnvsitemswitch.c:590 and :606, the battle state read and written. */
void mnVSItemSwitchGetItemSettings(void);
void mnVSItemSwitchSetItemSettings(void);

/* mnvsitemswitch.c:889 0x801331B0. */
void mnVSItemSwitchStartScene(void);

/* The bzero arm of syDmaLoadOverlay for overlay 21 (src/dc/overlay.h). */
void mnVSItemSwitchOverlayLoad(void);

#endif /* SSB_DC_MNVSITEMSWITCH_H */

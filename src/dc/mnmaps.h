/* mnmaps.h -- the stage select, mn/mnmaps/mnmaps.c.
 *
 * The screen after the character select's START: a stone wall with ten
 * icons in two rows -- nine stages and RANDOM -- a red cursor over one
 * of them, and on the right a wooden plaque carrying the stage's series
 * emblem and its name over a plate. On the left, in a small window, the
 * stage previews itself: its own background image scaled down behind
 * its own model, spun a little by a camera that bobs its at-point
 * through a sine. A goes to the battle, B back to the character select,
 * five idle minutes to the title. Eight sprite cameras, one per DL link
 * 0-7, and one 3D camera for the preview.
 *
 * Every function is the decomp's by name and body, the REGION_US arms;
 * the JP subtitle arms stay under their #if as the decomp has them.
 * mnMapsMakeString -- the game's only text drawn glyph by glyph out of
 * 33_MNCommonFonts -- comes with them: the US build loads the font file
 * and draws nothing from it, because the two callers are the JP
 * subtitles. It is ported all the same, and the host test draws a
 * string with it, because it is the port's first font path.
 *
 * What is not here, and where it is said:
 *   - the clear camera and the lighting pre-render (mnMapsFuncLights) --
 *     mnMapsFuncStart, dMNMapsTaskmanSetup;
 *   - syVideoInit and the arena_size line -- mnMapsStartScene;
 *   - the reloc setup: four sprite banks stand in for four of the five
 *     files, and the fifth is training mode's -- mnMapsLoadFiles;
 *   - the two model heaps and the map file loads they page a stage's
 *     ground data into: the port's stages are packs resident from boot
 *     (gGRStages, stage.h), so a "load" is a table lookup --
 *     mnMapsAllocModelHeaps, mnMapsLoadMapFile;
 *   - four layer GObjs become one, because tools/export/ssb_stageexport.py
 *     bakes a stage's layers into one pack -- mnMapsMakeLayer,
 *     mnMapsMakeModel;
 *   - the training-mode preview background -- mnMapsMakePreviewWallpaper;
 *   - the RDP commands of the two display procs, in the port's spelling
 *     (src/dc/lbcommon.h) -- mnMapsLabelsProcDisplay,
 *     mnMapsPreviewWallpaperProcDisplay.
 *
 * An icon whose stage the port has no pack for still shows and still
 * picks; its preview window is the bare tile pattern and its battle
 * plays on Hyrule, said on the log (src/dc/scvsbattle.c), the same
 * answer the port gives a fighter with no pack. */
#ifndef SSB_DC_MNMAPS_H
#define SSB_DC_MNMAPS_H

#include <sys/obj.h>
#include <sys/taskman.h>
#include <mp/mpdef.h>
#include <mn/mndef.h>

#include "stage.h"

/* mnmaps.c:1670 */
extern SYTaskmanSetup dMNMapsTaskmanSetup;

/* mnmaps.c:130 sMNMapsFiles, the five files' bases: the game's void*[5]
 * with a SpriteBank* in each (sprite.h), NULL for training mode's
 * wallpaper file. Not static in the decomp; the host test reads sprites
 * out of them by offset. */
extern void *sMNMapsFiles[5];

/* mnmaps.c:82,86,106,114 -- the scene's state, not static in the decomp
 * either: which icon the cursor is on, the GObjs the cursor and the
 * name-and-emblem plaque are, and the stage the preview was built from.
 * The host test reads all four. */
extern s32 sMNMapsCursorSlot;
extern s32 sMNMapsScrollWait;
extern GObj *sMNMapsCursorGObj;
extern GObj *sMNMapsNameLogoGObj;
extern Stage *sMNMapsGroundInfo;

void mnMapsAllocModelHeaps(void);

/* The port's own: release the two previews' stages. mnMapsStartScene
 * calls it when the scene ends, where the game's two model heaps go with
 * the scene heap. */
void mnMapsReleaseMapFiles(void);
sb32 mnMapsCheckLocked(s32 gkind);
s32 mnMapsGetCharacterID(const char c);
f32 mnMapsGetCharacterSpacing(const char *str, s32 c);
void mnMapsMakeString(GObj *gobj, const char *str, f32 x, f32 y, u32 *color);
void mnMapsMakeWallpaper(void);
void mnMapsMakePlaque(void);
void mnMapsLabelsProcDisplay(GObj *gobj);
void mnMapsMakeLabels(void);
s32 mnMapsGetGroundKind(s32 slot);
s32 mnMapsGetSlot(s32 gkind);
void mnMapsMakeIcons(void);
void mnMapsSetNamePosition(SObj *sobj, s32 gkind);
void mnMapsMakeName(GObj *gobj, s32 gkind);
void mnMapsSetLogoPosition(GObj *gobj, s32 gkind);
void mnMapsMakeEmblem(GObj *gobj, s32 gkind);
void mnMapsMakeNameAndEmblem(s32 slot);
void mnMapsSetCursorPosition(GObj *gobj, s32 slot);
void mnMapsMakeCursor(void);
void mnMapsLoadMapFile(s32 gkind, void *heap);
void mnMapsLoadFiles(void);
void mnMapsPreviewWallpaperProcDisplay(GObj *gobj);
GObj *mnMapsMakePreviewWallpaper(s32 gkind);
void mnMapsModelPriProcDisplay(GObj *gobj);
void mnMapsModelSecProcDisplay(GObj *gobj);
GObj *mnMapsMakeLayer(s32 gkind, Stage *stage, s32 id);
void mnMapsMakeModel(s32 gkind, s32 heap_id);
void mnMapsDestroyPreview(s32 heap_id);
void mnMapsMakePreview(s32 gkind);
void mnMapsMakeWallpaperCamera(void);
void mnMapsMakePlaqueCamera(void);
void mnMapsMakePreviewWallpaperCamera(void);
void mnMapsMakeLabelsViewport(void);
void mnMapsMakeIconsCamera(void);
void mnMapsMakeNameAndEmblemCamera(void);
void mnMapsMakeCursorCamera(void);
void mnMapsSetPreviewCameraPosition(CObj *cobj, s32 gkind);
void mnMapsPreviewCameraThreadUpdate(GObj *gobj);
void mnMapsMakePreviewCamera(void);
void mnMapsSaveSceneData(void);
void mnMapsInitVars(void);
void mnMapsSaveSceneData2(void);
void mnMapsFuncRun(GObj *gobj);
void mnMapsFuncStart(void);
void mnMapsStartScene(void);

#endif /* SSB_DC_MNMAPS_H */

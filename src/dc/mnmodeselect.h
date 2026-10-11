/* mnmodeselect.h -- the mode select, mn/mncommon/mnmodeselect.c.
 *
 * The first menu: the collage behind a blue bar, MODE SELECT in the bar,
 * four options down a diagonal -- 1P GAME, VS MODE, OPTION, DATA -- with
 * the chosen one's icon lit and the other three dark, the labels in
 * red beside them, the logo in the corner. The stick or the pad moves
 * the choice up and down (wrapping, with a longer pause at the ends),
 * A or START takes it, B goes back to the title, and five idle minutes
 * do the same. Two sprite cameras: the decals on DL link 0, the labels
 * and options on link 1.
 *
 * Every function is the decomp's by name and body, REGION_US arms.
 * What is not here, and where it is said:
 *   - the clear camera (gcMakeDefaultCameraGObj) and the lighting
 *     pre-render (mnModeSelectFuncLights), both for 3D this scene has
 *     none of -- mnModeSelectFuncStart, dMNModeSelectTaskmanSetup;
 *   - syVideoInit and the arena_size line -- mnModeSelectStartScene;
 *   - the reloc setup: two sprite banks stand in for the two files --
 *     mnModeSelectLoadFiles.
 *
 * Where each option leads is the scene manager's business
 * (src/dc/scmanager.c): the scene names the game's own scene kinds and
 * the manager decides which of them the port has. */
#ifndef SSB_DC_MNMODESELECT_H
#define SSB_DC_MNMODESELECT_H

#include <sys/obj.h>
#include <sys/taskman.h>

/* mnmodeselect.c:52 */
extern SYTaskmanSetup dMNModeSelectTaskmanSetup;

/* mnmodeselect.c:127 sMNModeSelectFiles, the two files' bases: the
 * game's void*[2] with a SpriteBank* in each (sprite.h). Not static in
 * the decomp; the host test reads sprites out of them by offset. */
extern void *sMNModeSelectFiles[2];

void mnModeSelectMake1PMode(void);
void mnModeSelectMakeVSMode(void);
void mnModeSelectMakeOption(void);
void mnModeSelectMakeData(void);
void mnModeSelectMakeLabels(void);
void mnModeSelectMakeDecals(void);
void mnModeSelectMakeLabelsCamera(void);
void mnModeSelectMakeDecalsCamera(void);
void mnModeSelectMakeOptions(void);
void mnModeSelectEjectOptions(void);
void mnModeSelectInitVars(void);
void mnModeSelectFuncRun(GObj *gobj);
void mnModeSelectLoadFiles(void);
void mnModeSelectFuncStart(void);
void mnModeSelectStartScene(void);

#endif /* SSB_DC_MNMODESELECT_H */

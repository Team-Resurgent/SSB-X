/* camanim.h -- the camera-animation banks.
 *
 * WHAT THIS IS FOR.
 *
 * `gcAddCObjCamAnimJoint(cobj, script, frame)` hangs an AObjEvent32
 * script on a CObj, and the `gcPlayCamAnim` process runs it every tic:
 * gcParseCObjCamAnimJoint feeds ten camera tracks
 * (nGCAnimTrackEyeX..nGCAnimTrackFovY, sys/objdef.h:248-259) and
 * gcPlayCObjCamAnim writes their values into cobj->vec.eye/at/up and
 * cobj->projection.persp.fovy. That is how every in-engine cutscene in
 * this game moves its camera -- the openings, the ending diorama, the
 * 1P intros -- and there is no other mechanism.
 *
 * Both functions are the decomp's own, compiled unmodified out of
 * sys/objanim.c (src/game/ssb64/Makefile's objanim.o), so nothing about
 * the interpreter is ported or re-derived here. What was missing was the
 * DATA: the game reaches a script as `lbRelocGetFileData(AObjEvent32*,
 * sFiles[n], &ll<Name>CamAnimJoint)` -- a loaded relocData file's base
 * plus a linker offset -- and the port has neither the file nor the
 * linker symbol. src/dc/mvopeningroom.c and src/dc/mvending.c therefore
 * shipped with six such calls cut and six cameras
 * that did not move.
 *
 * A bank is what replaces the loaded file: tools/export/ssb_camanimexport.py
 * cuts the named scripts out of relocData and writes them to
 * romdisk/<scene>.cam, and camanim_get is the stand-in for
 * lbRelocGetFileData -- by name rather than by offset, because the port
 * has no address to offset from. It is the same substitution
 * stage_map_anim already makes for a stage's map AnimJoints, and the
 * same one sprite_bank_get makes for a Sprite.
 *
 * THE POINTER WORDS.
 *
 * An AnimJoint script's Jump, SetAnim and SetInterp opcodes are each
 * followed by a pointer word, and in the ROM that word is an absolute
 * N64 address. A script lifted out of its file by memcpy alone keeps
 * them, the parser follows one into whatever happens to live there,
 * never reaches an End, and loops forever -- inside the game's frame,
 * so it does not crash: it black-screens. src/dc/stage.c:230-257 carries
 * the same note and the same fix for the stage packs.
 *
 * So the bank stores each such word as a WORD INDEX into its own word
 * array, lists the indices of the words that hold one, and
 * camanim_bank_load turns each into an address after the bank is in RAM
 * -- exactly how a fighter pack's AnimJoint animations are relocated
 * (FPackAnim.off_reloc, src/dc/fighter.h). None of the thirteen scripts
 * shipped today has a pointer word; the machinery is here because the
 * failure it prevents is silent, not because today's data needs it.
 *
 * The scripts and their words are syTaskmanMalloc'd out of the scene
 * heap, so they vanish with the scene the way the game's loaded file
 * does. A scene that loads a bank must therefore OVERLAY_CLEAR its
 * CamAnimBank in its own OverlayLoad.
 */
#ifndef SSB_DC_CAMANIM_H
#define SSB_DC_CAMANIM_H

#include <sys/objtypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/* tools/export/ssb_camanimexport.py NAME_LEN */
#define CAMANIM_NAME_LEN 32

typedef struct CamAnimEntry
{
    char name[CAMANIM_NAME_LEN];
    u32 *words;                 /* scene heap; the script itself */
    u32 nwords;
} CamAnimEntry;

typedef struct CamAnimBank
{
    u32 count;
    CamAnimEntry *entries;      /* scene heap */
    u32 *words;                 /* scene heap; every script, end to end */
    u32 nwords;
    sb32 is_loaded;
} CamAnimBank;

/* Load a .cam into `bank`, by name ("mvopeningroom.cam") -- the asset
 * root decides which medium it comes off (src/dc/assetroot.h). Returns
 * 0, or -1 with the reason on the log, in which case the bank is zeroed
 * and every camanim_get on it answers NULL (so a scene whose bank failed
 * to load keeps the static cameras it had before this step rather than
 * taking a null-pointer fault). */
int camanim_bank_load(CamAnimBank *bank, const char *name);

/* lbRelocGetFileData(AObjEvent32*, file, &ll<Name>CamAnimJoint) for a
 * bank: the script the exporter wrote under `name`. NULL, with a log
 * line, when the bank has no such script -- a build error (the exporter
 * was asked for a different set), not a runtime condition.
 *
 * gcAddCObjCamAnimJoint takes NULL happily: gcParseCObjCamAnimJoint's
 * own first test of camanim_joint.event32 ends the animation, so the
 * camera simply holds still. */
AObjEvent32 *camanim_get(CamAnimBank *bank, const char *name);

#ifdef __cplusplus
}
#endif

#endif /* SSB_DC_CAMANIM_H */

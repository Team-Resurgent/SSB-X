/* scexplain.h -- the How to Play scene, sc/sccommon/scexplain.c ported.
 * sc/sccommon/scexplainfiles.c is not: its one function,
 * scExplainSetupFiles, is the same reloc-loader shape scvsbattle.c's
 * own scVSBattleSetupFiles already replaced port-wide with
 * gmCommonLoadFiles() (src/dc/gmcommon.c) -- this file's own
 * scExplainSetupFiles is that same one-line substitution.
 *
 * This is a real battle -- two GameKey fighters (Mario and Luigi) posed
 * by canned FTKeyEvent scripts, the ftKeyProcessKeyEvents/ftParamSetKey
 * primitive that was canned -- stepping
 * through a 23-phase table (scExplainUpdatePhase) that swaps a textbox
 * sprite and six phase-indicator icons (A/B/Z buttons, "HERE", two
 * "+" symbols) on a timer, spawns one real Fire Flower at phase 16
 * (scExplainTryMakeFireFlower), and exits to Character Data when the
 * table runs out or to Title on any A/B/START tap (scExplainDetectExit).
 *
 * Three DIVERGES, beyond the usual video/matrix-list/DL-buffer ones
 * every scene here already carries:
 *
 *  - nGRKindExplain has a real ground/map file in the decomp
 *    (relocData 267/115, dGRExplainMap_header) but no exported pack --
 *    tools/export/ssb_stageexport.py has no path for it and none of the port's
 *    nine VS stages is it. This scene falls back to Hyrule for its
 *    ground, collision and wallpaper, the same "no pack -> Hyrule"
 *    substitute src/dc/scvsbattle.c's own scVSBattleStartScene and
 *    src/dc/scautodemo.c's own scAutoDemoFuncStart already use --
 *    unconditionally here, since Explain's gkind never has a pack to
 *    try first. Without SOME bound stage, stage_bind_collision leaves
 *    gMPCollisionGeometry untouched (stale or NULL from whatever the
 *    previous scene bound, since taskman's scene-heap reset would have
 *    freed it) and mpCollisionGetPlayerMapObjPosition -- called for
 *    both fighters' spawn positions -- dereferences it unconditionally.
 *    Hyrule gives real, safe ground data as a side effect.
 *
 *  - relocData file 252 (SCExplainMain: both players' KeyEvent scripts
 *    and the 22-entry ExplainPhase table) is already decoded, readable
 *    C source in ssb-decomp-re -- not an opaque binary blob -- so this
 *    port compiles the US arm's data in directly (src/dc/scexplain.c's
 *    dSCExplainKeyEvent0/1 and dSCExplainPhase) rather than adding a
 *    new binary export format for one file. scExplainLoadExplainFiles
 *    therefore loads only file 198's sprite bank; sSCExplainMainFileHead
 *    and the lbRelocGetFileData indirection over it are gone, and
 *    SCExplainPhaseEntry's sprite field is a sprite_bank_get offset
 *    (u32) rather than the decomp's Sprite*, resolved through the same
 *    sprite.h API mncongra.c/mnsoundtest.c already use for their own
 *    banks.
 *
 *  - The control-stick diagram is real geometry, not sprites --
 *    relocData file 198's other eleven entries (MObjSub/DObjDesc/
 *    MatAnimJoint/DisplayList for the joystick, the tap-spark flash and
 *    the special-move RGB overlay), confirmed with
 *    tools/export/ssb_spriteexport.py --file 198 --list, which returns only
 *    the file's 26 real Sprite entries and silently skips these.
 *
 *    The three shapes are each a shipped exporter already reads;
 *    tools/export/ssb_explainexport.py bakes them into three packs --
 *    scstick.mdl (2 joints, one MObjSub over five 64x64 IA8 pictures,
 *    and the file's five Stick*MatAnimJoint tables side by side as five
 *    FPackMObjs alternates), scspark.mdl (one quad, three 32x32 I4
 *    pictures, one MatAnimJoint) and scrgb.mdl (one quad, one CI4
 *    16x48 tile and its TLUT). So scExplainMakeControlStickCamera,
 *    scExplainControlStickProcDisplay, scExplainProcUpdateControlStick
 *    Sprite, scExplainMakeControlStickInterface, scExplainUpdateTapSpark
 *    Effect, scExplainTapSparkProcUpdate, scExplainMakeTapSpark,
 *    scExplainMakeSpecialMoveRGB and func_ovl63_8018DDBC's whole body
 *    are all ported now.
 *
 *    What still DIVERGES about them is the port's ordinary model
 *    substitutions, all already documented at their own first sites:
 *    gcSetupCustomDObjs/gcAddDObjForGObj over relocData become
 *    dc_model_add_dobjs over a pack and gcAddMObjAll becomes
 *    dc_model_add_mobjs (src/dc/mnplayersvs.c's own spotlight);
 *    gcDrawDObjTreeDLLinksForGObj/gcDrawDObjDLHead1 and the two lines of
 *    render state around them become dc_model_draw_tree_layered over
 *    batches the exporter baked translucent and Z-less
 *    (tools/export/ssb_arrowexport.py's own note); and
 *    dSCExplainStickMatAnimJoints holds an alternate index rather than a
 *    file offset. A pack that fails to load leaves its GObj NULL, and
 *    every read of the three is still guarded on that -- the same shape
 *    scAutoDemoInitSObjs uses the same guard pattern.
 *
 *    NOT VISUALLY CONFIRMED, and the reason is not this code. The
 *    DB_BOOT_SCENE=nSCKindExplain boot produces a completely black
 *    frame -- eight captures 1.5s apart, every one mean 0, max 0, not one
 *    non-zero pixel, not even the damage/stock HUD sprites or
 *    scExplainWindowProcDisplay's own textbox fill, none of which this
 *    step touched.
 *
 *    -DDB_PVR_BUDGET on the same boot prints, once a second for the
 *    whole run, `tris op 0 pt 0 tr 0 ... vtx 0 of 524288 (0%)` (an
 *    occasional `tr 2`). NOTHING reaches the TA. That rules out
 *    the failure scstaffroll.h's own sixth DIVERGES documents -- geometry
 *    drawn and then covered by a full-screen opaque quad, which looks
 *    identical on screen but submits a full frame of triangles -- so
 *    there is no point hunting a stray fill here, and the three packs
 *    above cannot be the cause of, nor be cleared by, this screen.
 *
 *    Everything around the silence is healthy: the stage, Mario, Luigi,
 *    EFDokan, IFArrows, IFMagnfy, the efcommon particle bank and
 *    SCStick/SCSpark/SCRGB all load and report their joint and triangle
 *    counts; gmCameraMakeBattleCamera and the arrows/magnify/screen-flash/
 *    interface/effect cameras are all made; ftParamSetKey feeds both
 *    fighters; `db: game status Go at frame 0` prints; no assert, nothing
 *    `not resident`, no `DLBuffer over flow`, hang watchdog silent. The
 *    scene is loaded, built and posed and never drawn, which points at
 *    the draw path -- scManagerFuncDraw in dSCExplainTaskmanSetup and the
 *    camera links the manager walks -- and at nothing in this header.
 */
#ifndef SSB_DC_SCEXPLAIN_H
#define SSB_DC_SCEXPLAIN_H

#include <ft/fighter.h>
#include <sc/scene.h>
#include <sys/obj.h>
#include <sys/taskman.h>

/* scexplain.c's own port-local mirror of the decomp's SCExplainPhase
 * (sc/sctypes.h) -- same layout, but `sprite_offset` is a
 * sprite_bank_get offset into the exported scexplaingraphics.spr bank
 * rather than a Sprite* (see this file's header note above).
 * SCExplainArgs itself is the decomp's own type, unchanged, already
 * visible through <sc/scene.h>. */
typedef struct SCExplainPhaseEntry
{
    u16 phase_time;
    u16 unused;
    u8 textbox_pos_x;
    u8 textbox_pos_y;
    u32 sprite_offset;
    SCExplainArgs control_stick_args;
    SCExplainArgs phase_args0;
    SCExplainArgs phase_args1;
    SCExplainArgs phase_args2;
    SCExplainArgs phase_args3;
    SCExplainArgs phase_args4;
    SCExplainArgs rgb_overlay_args;
    SCExplainArgs phase_args5;

} SCExplainPhaseEntry;

/* scexplain.c:55-97 dSCExplainTaskmanSetup (see scautodemo.h's own note:
 * the DL buffers/graphics arena/RDP output are the port's real figures,
 * not the decomp's zeroed placeholders; func_lights runs the same
 * dropped-reflection-lighting shape every scene's does; the matrix
 * function list is NULL, the matrix kinds being a switch in objdisplay.c). */
extern SYTaskmanSetup dSCExplainTaskmanSetup;

/* scexplain.c's own data tables, extern'd for the host test the same
 * way scautodemo.h's are. */
extern SYColorRGBA dSCExplainFadeColor;
extern s32 dSCExplainRandomSeed1;
extern s32 dSCExplainRandomSeed2;
extern const SCExplainPhaseEntry dSCExplainPhase[22];
extern const FTKeyEvent dSCExplainKeyEvent0[1258];
extern const FTKeyEvent dSCExplainKeyEvent1[1300];

void scExplainLoadExplainFiles(void);
void scExplainSetBattleState(void);
void scExplainStartBattle(void);
void scExplainSetPlayerInterfacePositions(void);
void scExplainMakeWindowCamera(void);
GObj *scExplainMakeTextCamera(void);
GObj *scExplainMakeControlStickCamera(void);
void scExplainControlStickProcDisplay(GObj *gobj);
void scExplainProcUpdateControlStickSprite(GObj *gobj);
GObj *scExplainMakeControlStickInterface(void);
void scExplainUpdateTapSparkEffect(void);
void scExplainTapSparkProcUpdate(GObj *gobj);
GObj *scExplainMakeTapSpark(void);
void scExplainSpecialMoveRGBProcUpdate(void);
GObj *scExplainMakeSpecialMoveRGB(void);
void scExplainSetInterfaceGObjs(void);
void scExplainSetPhaseSObjs(void);
void scExplainUpdateTextBoxSprite(void);
void scExplainHideTapSpark(void);
void scExplainUpdateArgsSObj(SCExplainArgs *args, SObj *sobj);
void scExplainDetectExit(void);
void scExplainTryMakeFireFlower(void);
void scExplainUpdatePhase(void);
void scExplainSceneInterfaceProcUpdate(GObj *gobj);
GObj *scExplainMakeSceneInterface(void);
void scExplainFuncStart(void);
void scExplainFuncLights(Gfx **dls);
void scExplainFuncUpdate(void);
void scExplainFuncDraw(void);
void scExplainStartScene(void);

/* The port's own bzero arm, src/dc/overlay.c's dSCManagerOverlays[63]. */
void scExplainOverlayLoad(void);

#endif /* SSB_DC_SCEXPLAIN_H */

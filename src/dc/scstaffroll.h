/* scstaffroll.h -- the staff roll, sc/sccommon/scstaffroll.c ported, the second half of the ending sequence src/dc/mvending.c
 * hands off to at tic 660. 2,339 decomp lines, 48 functions, most of it
 * a scrolling job/name credits list with a name-shooting minigame: a
 * reticle (scStaffrollCrosshairThreadUpdate, real controller stick
 * input through scSubsysController*, the same subsystem every already-
 * ported scene uses) the player aims over the scrolling text to "shoot"
 * a name, which pops up that staff member's role and company in a side
 * textbox (scStaffrollMakeStaffRoleTextGObj/MakeCompanyTextGObj).
 *
 * Three DIVERGES, all one underlying gap:
 *
 * 1. The credit TEXT is real, compiled in, not cut. dSCStaffrollName/
 *    Job/StaffRole/CompanyCharacters (the encodeded glyph-index arrays)
 *    and their matching …TextInfo {start,count} tables are generated at
 *    build time by ssb-decomp-re's own tools/creditsTextConverter.py
 *    off its own readable src/credits .us.txt sources -- the same
 *    "already-decoded, not bytes to export" shape
 *    relocData/252_SCExplainMain.c was for scexplain.c's own KeyEvent
 *    data, just generated at build time instead of
 *    committed, because the decomp's own Makefile generates these too
 *    rather than checking in the .encoded/.metadata pair. src/game/
 *    ssb64/Makefile runs the same script against the same .txt files.
 *
 * 2. The STAFF-ROLE/COMPANY popup text (the side textbox a successful
 *    "shot" reveals) is real too: it draws through
 *    dSCStaffrollTextBoxSpriteInfo, a table of true libultra Sprite
 *    records inside relocData file 195 -- tools/export/ssb_spriteexport.py
 *    --file 195 catalogues and exports all 77 of them (the alphabet,
 *    digits and punctuation, the crosshair, the two textbox brackets)
 *    into scstaffrollgraphics.spr, exactly the tools/export/ssb_spriteexport.py
 *    --file 198 precedent scexplain.c's own sprite bank already set.
 *
 * 3. The MAIN SCROLLING job/name text -- the 3D quads
 *    scStaffrollInitNameAndJobDisplayLists builds out of
 *    dSCStaffrollNameAndJobSpriteInfo's own raw texture blocks, and the
 *    DObjDesc/AObjEvent32/SYInterpDesc "name plaque" skeleton
 *    (sSCStaffrollDObjDesc/sSCStaffrollNameAnimJoint/
 *    sSCStaffrollNameInterpolation, and the small rotating highlight
 *    backdrop func_ovl59_8013202C makes behind a shot name) -- is
 *    exported as packs, and the shape of that is worth writing
 *    down because the obvious premise was only half right.
 *
 *    The GLYPHS are not a DObjDesc tree at all. Measuring
 *    file 195 finds 56 raw Image blocks (12,880 B) and no
 *    display list whatsoever: the N64 BUILDS the quads at run time, in
 *    scStaffrollInitNameAndJobDisplayLists, out of nothing but each
 *    row's width and height and a gDPLoadTextureBlock_4b over the raw
 *    block. So the export is a texture export, not a mesh export, and
 *    tools/export/ssb_staffrollexport.py --what glyphs0/glyphs1 bakes one pack
 *    joint per glyph, each carrying that same quad (the same
 *    +-width/+-height corners, the same width x height texel span, the
 *    same 3,2,1 / 0,3,1 pair) over the same I4 tile at the same
 *    ((w + 15) / 16) * 16 stride. tools/export/ssb_shadowexport.py's blob is
 *    the precedent for the one-quad-from-two-numbers half.
 *
 *    The 56 glyphs ship as TWO packs, scglyphs0.mdl and scglyphs1.mdl,
 *    28 joints each (GLYPH_SPLIT in the exporter,
 *    SCSTAFFROLL_GLYPH_SPLIT here), because FIGHTER_MAX_JOINTS is 40 and
 *    a 56-joint blob is refused outright by fighter_init -- "fighter:
 *    blob is not a pack this build handles", which names no file, so the
 *    first disc probe of this step reported only that nothing loaded.
 *    Splitting is the right fix rather than raising the cap: the cap is
 *    a fighter-rig figure the whole port's matrix arrays are sized to,
 *    and a credit alphabet is not a rig. scStaffrollInitNameAndJob
 *    DisplayLists picks the pack with i / SCSTAFFROLL_GLYPH_SPLIT and
 *    the joint with i % SCSTAFFROLL_GLYPH_SPLIT; nothing else in the
 *    scene knows there are two.
 *
 *    What it is NOT is a pack of bare textures the run time turns into
 *    quads. There is no run-time quad builder on this target, and the
 *    obvious shortcut -- keeping the decomp's Gfx* and handing it to
 *    gcAddChildForDObj -- is the exact crash (5) below already
 *    documents. The geometry is baked; only the placement is live.
 *
 *    The PLAQUE half is a real DObjDesc, at file offset 0x78C0: two
 *    joints, four triangles, one tile -- smaller than the player arrows
 *    tools/export/ssb_arrowexport.py already ships. --what plaque bakes it as
 *    scplaque.mdl and func_ovl59_80131F34/8013202C are ported over it.
 *
 *    And the CURVE the whole roll rides -- the SYInterpDesc at 0x7304
 *    over eight Bezier control points, and the two-command AObjEvent32
 *    AnimJoint at 0x7338 that leans each line as it goes -- is compiled
 *    into scstaffroll.c directly rather than exported. It is plain
 *    floats and two script words with no pointer into the file to
 *    relocate, which makes it the same "already-decoded readable C, not
 *    bytes to export" call relocData/252 was for scexplain.c's own
 *    KeyEvent tables. This is the piece that actually MOVES a credit
 *    line: with it guarded away, every line sat at the origin.
 *
 *    What still diverges about all of it is the port's ordinary model
 *    substitutions, each documented at its own site in scstaffroll.c:
 *    a glyph DObj's payload is a DCDisplay over a pack joint rather
 *    than a Gfx*, and a GRAFT one (dc_model_graft_display) because a
 *    credit line is many DObjs over one shared pack and a repeated
 *    letter would otherwise draw once; the two ProcDisplays set the
 *    job/name primitive colour through fighter_set_prim_color on every
 *    call rather than once on their list head, since there is no
 *    standing RDP state here to inherit, and gate themselves on the
 *    translucent pass because the scene draw runs three times a frame;
 *    and gcSetupCustomDObjs over the plaque's DObjDesc becomes
 *    dc_model_add_dobjs over its pack. A pack that fails to load leaves
 *    its half of the scene as a bare scene -- the DObjs
 *    are still made and measured, the scroll still ends on time.
 *
 *    The hit-test math itself (func_ovl59_80131BB0/80131C88/
 *    80131D30/80131DD0/80131E70/8013330C, scStaffrollCheckCursor
 *    NameOverlap/HighlightPrompt) ports verbatim and runs for real --
 *    it reads cn->offset_x/unkgmcreditsstruct0x10 through an
 *    SCStaffrollMatrix* alias of the very same SCStaffrollName the
 *    decomp's own sctypes.h confirms share that struct's layout at
 *    those two offsets (0xC/0x10), and both fields are set by
 *    scStaffrollJobAndNameInitStruct's own arithmetic on the DObjs
 *    scStaffrollMakeJobDObjs makes, independent of the glyph children
 *    being attached -- so "shooting" a scrolling name works even
 *    without the glyph children, and its popup text (2) appears;
 *    func_ovl59_8013202C's own backdrop appears behind it too.
 *
 * A fourth, unrelated gap this step also closed for good: unlike
 * mvending.c's own gSCManager1PGameBattleState substitution,
 * scStaffrollInitVars needs no stand-in -- sSCStaffrollPlayer =
 * gSCManagerSceneData.player is a real, already-live field (see
 * mvending.h's own header note on the same struct), so this line ports
 * verbatim.
 *
 * scStaffrollTryHideUnlocks needs no new save-data either: it only
 * *reads* gSCManagerBackupData.unlock_mask (already real, already used
 * throughout the port -- e.g. src/dc/scvsresults.c's own unlock-message
 * logic) against the LBBACKUP_UNLOCK_MASK_* bits (already real decomp
 * macros, lb/lbdef.h) and question-marks the still-locked names in the
 * compiled-in dSCStaffrollStaffRoleCharacters table in place -- no
 * write path at all.
 *
 * A fifth DIVERGES a disc probe (not the host) caught:
 * scStaffrollMakeTextBoxGObj no longer builds a DObj out of
 * dSCStaffrollTextBoxDisplayList's own raw Gfx bytes (the side textbox's
 * flat-colour frame, four gDPFillRectangle strokes) -- gcAddDObjForGObj's
 * second argument becomes that DObj's `dv`, the same union member
 * gcSubmitDObj calls through as a DCDisplay*, and a raw Gfx* there
 * crashed real hardware on the very first frame (PC e7000004,
 * gsDPPipeSync's own first word, executed as an SH4 instruction) even
 * though the table has no relocData dependency to make unsafe -- the
 * host never actually calls through `dv`, so `./run.sh test host` could not
 * have caught this. scStaffrollTextBoxProcDisplay draws the same four
 * rectangles in the same colour through lbCommonSpriteFillRect instead,
 * the same substitution src/dc/ifscreenflash.c/lbfade.c already make for
 * their own flat quads.
 *
 * A sixth, and the one that actually kept the credits invisible once
 * every pack above loaded clean: scStaffrollFuncStart's own clear camera
 * no longer passes COBJ_FLAG_FILLCOLOR. On the N64 that flag is a
 * framebuffer clear, and a clear is behind everything drawn after it. In
 * this port src/dc/objdisplay.c:1703-1724 (gcPrepCameraViewport)
 * implements it as an lbCommonSpriteFillRect over the clamped viewport
 * in the TRANSLUCENT list at the frame's next sprite depth -- and its
 * own comment already predicted the consequence: "a 3D camera after a
 * fill would draw under it, because the opaque list's depths are all
 * behind the sprite depths -- no scene the port runs has one." This
 * scene is that scene: both of scStaffrollMakeCamera's 3D cameras run
 * after the clear camera, so every glyph in the roll was drawn and then
 * painted over by an opaque black full-screen quad. The symptom is
 * indistinguishable from "nothing rendered" -- triangle counts, texture
 * residency, DL budget and the hang watchdog were all clean throughout.
 * Dropping the flag is correct rather than a workaround, and it is what
 * objdisplay.c's own comment prescribes two sentences earlier: "every
 * other scene's clear camera is dropped for the PVR's own clear."
 * src/dc/mnvsoptions.c, mnmaps.c, mndata.c, mnsoundtest.c,
 * mnbackupclear.c and scautodemo.c all document the same drop. The
 * camera itself and its GPACK_RGBA8888 argument are kept (the colour
 * still lands in cobj->color, now inert) so the call site still reads
 * against the decomp line by line.
 *
 * Four more, found on 2026-09-24 when a player reported that the roll's
 * crosshair game did not work:
 * - the scene is 640x480 on the N64 (dSCStaffrollVideoSetup) and every
 *   2D position and viewport in it is in those pixels, so
 *   scStaffrollStartScene now sets that resolution for the scene
 *   (dcVideoSetResolution); at the port's usual scale of 2, the
 *   crosshair sat in the bottom-right corner and the textbox was off
 *   the screen;
 * - dSCStaffrollTextBoxSpriteInfo read file 195's interleaved upper/lower
 *   case run as capitals then small letters, which garbled the popup;
 * - the red lock-on box was raw GBI that nothing runs;
 * - scStaffrollFuncDraw was never the scene's draw function, so the
 *   credits never ended.
 *
 * Overlay 59 (ovl59_BSS_END, symbols/jp_wip_linker.txt), confirmed free.
 */
#ifndef SSB_DC_SCSTAFFROLL_H
#define SSB_DC_SCSTAFFROLL_H

#include <sc/scene.h>
#include <sys/interp.h>
#include <sys/obj.h>
#include <sys/taskman.h>

/* scstaffroll.c:2266-2308 dSCStaffrollTaskmanSetup. DIVERGES the same
 * shape every other scene's own setup documents: the DL buffers/
 * graphics arena/RDP output are zeroed (no RSP/RDP here), the matrix
 * function list is NULL (the matrix kinds are a switch in objdisplay.c), and arena_start is NULL
 * (keep the region syTaskmanMakeGeneralHeap already made) -- but unlike
 * mvending.c/scautodemo.c/scexplain.c, the decomp's own pool counts
 * here are NOT a zeroed placeholder (16 threads, 64 GObjs, 256 XObjs,
 * 1024 DObjs, 256 SObjs, 8 CObjs, real non-zero figures), so those port
 * verbatim rather than being re-estimated. */
extern SYTaskmanSetup dSCStaffrollTaskmanSetup;

/* relocData/195_SCStaffroll.c:3293-3345, compiled in rather than
 * exported (see (3) above): the curve every credit line rides and the
 * AObjEvent32 script that leans it. Extern'd for the host test the same
 * way scexplain.h's own compiled-in relocData 252 tables are -- these
 * are transcribed numbers, and a transcription is exactly the kind of
 * thing a test should hold still. */
extern Vec3f dSCStaffrollNamePoints[8];
extern f32 dSCStaffrollNameKeyframes[6];
extern f32 dSCStaffrollNameQuartics[25];
extern SYInterpDesc dSCStaffrollNameInterpolationDesc[1];
extern u32 dSCStaffrollNameAnimJointScript[5];

/* The popup textbox's font, one file-195 sprite per glyph (see the
 * table's comment in scstaffroll.c); extern'd for the host test. */
extern SCStaffrollSprite dSCStaffrollTextBoxSpriteInfo[74];

void func_ovl59_80131BB0(Mtx44f mtx, Vec3f *vec, f32 *width, f32 *height);
void func_ovl59_80131C88(CObj *cobj);
void func_ovl59_80131D30(DObj *dobj, Vec3f *vec, f32 *width, f32 *height);
void func_ovl59_80131DD0(GObj *gobj, SCStaffrollProjection *proj);
void func_ovl59_80131E70(Vec3f *arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4);
void scStaffrollPlaqueProcDisplay(GObj *gobj);
void func_ovl59_80131F34(GObj *arg0);
void func_ovl59_8013202C(GObj *arg0);
void func_ovl59_8013330C(void);
void scStaffrollSetupFiles(void);
void scStaffrollInitNameAndJobDisplayLists(void);
void scStaffrollTryHideUnlocks(void);
void scStaffrollSetTextQuetions(s32 *characters, s32 character_count);
void scStaffrollInitVars(void);
void scStaffrollMakeCrosshairGObj(void);
void scStaffrollCrosshairThreadUpdate(GObj *gobj);
sb32 scStaffrollCheckCursorNameOverlap(Vec3f *vec);
s32 scStaffrollGetLockOnPositionX(s32 pos_x);
s32 scStaffrollGetLockOnPositionY(s32 pos_y);
void scStaffrollHighlightProcDisplay(GObj *gobj);
void scStaffrollHighlightThreadUpdate(GObj *gobj);
void scStaffrollMakeHighlightGObj(GObj *gobj);
void scStaffrollMakeStaffRoleTextSObjs(GObj *text_gobj, GObj *staff_gobj);
void scStaffrollMakeStaffRoleTextGObj(GObj *staff_gobj);
void scStaffrollMakeCompanyTextSObjs(GObj *text_gobj, GObj *staff_gobj);
void scStaffrollMakeCompanyTextGObj(GObj *staff_gobj);
sb32 scStaffrollCheckCursorHighlightPrompt(GObj *gobj, SCStaffrollProjection *proj);
sb32 scStaffrollGetPauseStatusHighlight(void);
sb32 scStaffrollGetPauseStatusResume(void);
void scStaffrollFuncRun(GObj *gobj);
SCStaffrollName *SCStaffrollNameUpdateAlloc(GObj *gobj);
void SCStaffrollNameSetPrevAlloc(SCStaffrollName *cn);
void scStaffrollJobAndNameThreadUpdate(GObj *gobj);
void scStaffrollJobProcDisplay(GObj *gobj);
void scStaffrollNameProcDisplay(GObj *gobj);
void scStaffrollJobAndNameInitStruct(GObj *gobj, DObj *first_dobj, DObj *second_dobj, sb32 job_or_name);
SCStaffrollSetup *scStaffrollMakeJobDObjs(SCStaffrollSetup *name_setup, DObj *dobj, s32 name_id, f32 wbase);
GObj *scStaffrollMakeJobGObj(SCStaffrollJob *job);
GObj *scStaffrollMakeNameGObjAndDObjs(void);
void scStaffrollMakeTextBoxBracketSObjs(void);
void scStaffrollTextBoxProcDisplay(GObj *gobj);
void scStaffrollMakeTextBoxGObj(void);
void scStaffrollScrollThreadUpdate(GObj *gobj);
void scStaffrollMakeScrollGObj(void);
void scStaffrollUpdateCameraAt(GObj *gobj);
void scStaffrollMakeCamera(void);
void scStaffrollFuncStart(void);
void scStaffrollFuncDraw(void);
void scStaffrollStartScene(void);

/* The port's own bzero arm, src/dc/overlay.c's dSCManagerOverlays[59]. */
void scStaffrollOverlayLoad(void);

#endif /* SSB_DC_SCSTAFFROLL_H */

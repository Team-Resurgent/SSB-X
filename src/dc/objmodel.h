/* objmodel.h -- a .pack model as a GObj with a DObj tree.
 *
 * The bridge between the two halves of the port: src/dc/fighter.c holds
 * a whole model (joints, vertices, baked batches, compiled PVR headers)
 * and knows how to draw it; the object system holds a tree of DObjs, one
 * per joint, and knows what transform each one carries. This file makes
 * one out of the other, so that a model on screen is a GObj on a DL link
 * list like everything else the game draws.
 *
 * The mapping is one DObj per FPackJoint, in pack order, parented the way
 * the pack says. That is the same shape the game builds a fighter's parts
 * in (ft/ftparts.c walks a DObjDesc tree), and it is why the pack carries
 * joints at all: tools/export/ssb_packexport.py derives them from the very
 * DObjDesc the game would have read.
 */
#ifndef SSB_DC_OBJMODEL_H
#define SSB_DC_OBJMODEL_H

#include <sys/obj.h>

#include "fighter.h"

/* Hang a DObj tree for `model` off `gobj`, one DObj per pack joint with
 * the joint's bind translate/rotate/scale and the three XObjs
 * gcAddDObjMatrixSetsRpyR installs. The DObjs' payloads come out of the
 * scene heap and live as long as it does.
 *
 * A fighter pack (one with an FPackAttr) is built the way
 * lbCommonSetupFighterPartsDObjs builds the game's parts: only the
 * entries its setup_parts mask names get a DObj, and a masked-out entry
 * keeps its slot of `joints_out` (NULL) and its payload, which
 * dc_model_hidden_payload hands to ft/ftmain.c's hidden-part maker when
 * an animation asks for the entry. No masked-in entry
 * hangs off a masked-out one on any fighter (tools/check/joint_check.py), so
 * the pack's parent index is the game's tree. A pack without attributes
 * builds every joint.
 *
 * `parent` is what the pack's root joints hang under: NULL puts them on
 * the GObj itself (a stage, whose whole tree is the model), a DObj puts
 * them under it (a fighter, whose tree starts at the TopN the pack does
 * not carry -- see dc_model_add_root).
 *
 * `joints_out`, when not NULL, receives one DObj per pack joint in pack
 * order, NULL for an entry the mask leaves out. That array is the game's
 * FTStruct.joints from nFTPartsJointCommonStart on, and it is the reason
 * this returns anything at all: the animation walk (ft/ftparam.c:364
 * ftParamUpdateAnimKeys) indexes joints, it does not walk the tree.
 *
 * Returns the number of DObjs made, or -1 with the reason logged. The
 * caller still has to give the GObj a display proc: dc_model_proc_display
 * below is the one that matches this tree. */
int dc_model_add_dobjs(GObj *gobj, DObj *parent, Fighter *model,
                       DObj **joints_out);

/* The payload for pack joint `entry` of a tree dc_model_add_dobjs built,
 * for a DObj made outside that call: ft/ftmain.c's ftMainUpdateHiddenPartID
 * hands it to gcAddDObjForGObj where the game hands the entry's display
 * list. NULL where the entry bakes no geometry -- as the game's dl is NULL
 * for a DObjDesc without one -- so a bare transform stays bare and
 * modelpart_status reads it as -1. */
void *dc_model_hidden_payload(Fighter *model, int entry);

/* A payload for pack joint `joint` in storage the caller owns, for a DObj
 * whose model is switched to another pack's joint while it plays -- an
 * item's `dobj->dl = <display list>` (src/dc/itemmodel.c). Unlike
 * dc_model_hidden_payload's, which live in the scene heap with the tree
 * they came with, these last as long as the caller's storage. */
struct DCDisplay;
void dc_model_init_payload(struct DCDisplay *disp, Fighter *model, int joint);

/* The model parts (fighter.h FPackParts), for src/dc/ftparam.c's
 * setters. dc_model_has_parts is the game's
 * `modelparts_container->modelparts_desc[joint] != NULL`: whether pack
 * joint `joint` has any. dc_model_part_display is what the game reads out
 * of the joint's FTModelPart row: the payload to put on the DObj where the
 * game puts the row's display list -- NULL for a row with none -- and the
 * row's flags. A joint without parts gets its own payload and flags 0,
 * the commonparts row the port carries no flags of (ftmanager.c). Only
 * valid once dc_model_add_dobjs has cut the model's payloads. */
struct DCDisplay *dc_model_part_display(Fighter *model, int joint,
                                        int part_id, u8 *flags);
sb32 dc_model_has_parts(const Fighter *model, int joint);

/* The MObjs a model part of pack joint `joint` hangs with material scripts
 * of its own (FTModelPart.main_matanim_joints), in the two
 * shapes the row holds them for lbCommonAddMObjForFighterPartsDObj: a
 * NULL-terminated MObjSub * array and the scripts in lockstep with it.
 * Samus's grapple beam is the one VS part with any (fighter.h
 * FPackMObjs); every other joint gets NULL for both, the port's costume
 * MObjs being baked. Static storage, valid until the next call -- the
 * game's walk copies each MObjSub into its MObj, and the scripts are the
 * pack's. */
#define DC_MODEL_PART_MOBJS 8
void dc_model_part_mobjs(Fighter *model, int joint, MObjSub ***mobjsubs,
                         AObjEvent32 ***main_matanim_joints);

/* The distinct models a GObj's DObj tree draws, in tree order, at most
 * `max` of them: one for nearly everything, two when a DObj has been
 * switched to another pack's joint. dc_model_proc_display walks and draws
 * every one; a caller that sets a per-model colour (an item's colanim env)
 * sets it on every one. Returns the count. */
#define DC_MODEL_TREE_MODELS 4
int dc_model_models_of(GObj *gobj, Fighter **out, int max);

/* Hang the pack's MObjs off the DObj tree dc_model_add_dobjs just built,
 * and hand each one its MatAnimJoint script seeked to `anim_frame`.
 *
 * Two packs have any -- an emblem's (tools/export/ssb_emblemexport.py) and the
 * character select's spotlight (tools/export/ssb_spotexport.py); for every
 * other model this returns 0 and does nothing. The two walks it ends in
 * are the decomp's own -- gcAddMObjAll and gcAddMatAnimJointAll, both in
 * sys/objanim.c, both compiled unmodified -- because the pack keeps the
 * MObjs in the order and grouping the game's `MObjSub ***` did, and this
 * builds that shape back out of them.
 *
 * `anim_frame` is gcAddMatAnimJointAll's, and for the winner's emblem it
 * is the winning player's number: the script is five colour blocks a
 * frame apart, so seeking to frame N is choosing colour N
 * (mnvsresults.c:668).
 *
 * Returns how many MObjs were added, or 0 for a pack with none. */
int dc_model_add_mobjs(GObj *gobj, Fighter *model, f32 anim_frame);

/* The same, with which of the pack's MatAnimJoints to play. A pack
 * carries one (FPackMObjs.alt_count) unless the game swapped the
 * EFDesc's o_matanim_joint before making the effect, which only the dead
 * explosion does -- four blocks, one per player. Out of range plays the
 * first, as an unswapped descriptor would. */
int dc_model_add_mobjs_alt(GObj *gobj, Fighter *model, f32 anim_frame,
                           int alt);

/* dc_model_add_mobjs_alt's walk, rooted at an arbitrary DObj: a stage's
 * graft hangs off one DObj of a bigger tree, and gcAddMObjAll would walk
 * the whole thing. `joint` is which joint of the PACK `root` is, so the
 * caller picks the MObj list out of the pack's per-joint table itself --
 * a graft is joint 0, and a ground actor's animated MObj is on joint 1
 * (ef/efground.c's fifteen MatAnimJoints are all shaped that way). */
int dc_model_add_mobjs_dobj(DObj *root, Fighter *model, f32 anim_frame,
                            int alt, int joint);

/* Make one DObj of a GRAFT draw its pack as the walk reaches it, rather
 * than recording a matrix for a draw that never comes.
 *
 * A graft is a pack of its own whose DObj is a child of another model's
 * tree (src/dc/stage.c stage_bind_map_cloud, Yoshi's Island's clouds).
 * Its GObj's display proc is the game's gcDrawDObjTreeForGObj, which
 * only records matrices -- the port's submission step lives in
 * dc_model_proc_display, and a proc of the game's own never reaches it.
 * And a graft has no per-instance Fighter to record into even if it did:
 * Stage.graft_model is ONE pack shared by all nine clouds, so the last
 * instance's matrix would be the only one any later draw could use.
 *
 * So the graft's payload draws itself: the walk hands it its own matrix,
 * it transforms the pack and submits the batches of the open PVR list,
 * and the next instance overwrites what it used. That is the effect
 * arm's trick (dc_model_proc_display) for the same reason -- a shared
 * pack and per-instance placement -- and it is what a stage's graft
 * needs. A no-op in the host build, which has no renderer. */
void dc_model_graft_display(DObj *dobj);

/* A payload-carrying DObj on `gobj` that draws nothing itself: the stand
 * a model tree hangs from when the model is not the whole of what the
 * GObj is. The game's fighter TopN is exactly this -- gcAddDObjForGObj
 * with a NULL display list (ft/ftmanager.c:759), carrying the fighter's
 * position and facing above a parts tree that carries neither.
 *
 * It has to carry a payload because dc_model_proc_display finds the
 * model on the GObj's root DObj; its joint index is -1, so the matrix
 * walk hands it nothing. */
DObj *dc_model_add_root(GObj *gobj, Fighter *model);

/* The model a GObj's tree was built from -- what dc_model_proc_display
 * reads off the root DObj's payload, for anything else that needs the
 * pack behind a fighter's joints (the motion table, the animation bank,
 * the bind pose). NULL when the GObj has no root. This is how the fighter code reaches its pack: FTStruct is the game's and
 * carries no such pointer. */
Fighter *dc_model_of(GObj *gobj);

/* The view a GObj's models are transformed by: the camera's
 * (gcGetViewF), jogged in world space by the hitlag shuffle for a
 * fighter whose shuffle_tics is running (ft/ftdisplaymain.c:1205-1216). The pointer is to a static for the jogged case, good
 * until the next call. */
const float *dc_model_view_for(GObj *gobj);

/* proc_display for a GObj built that way. Composes the tree's matrices
 * into model->mtx[], folds in the camera the pass established, and
 * submits the model's batches for whichever PVR list is open.
 *
 * Under the magnify camera (gm/gmcamera.c) a fighter takes
 * a third path: the whole model into the translucent list at the
 * frame's next sprite depth, as the layered draw, spread by its own
 * clip z because the camera is ortho, and clipped to the sixteen
 * planes that stand in for the RDP's Z-buffer mask (src/dc/clip.h). */
void dc_model_proc_display(GObj *gobj);

/* The same, for a model a menu stands between two sprite passes: the
 * whole model goes into the PVR's translucent list at the frame's next
 * sprite depth, because the opaque list is drawn under every sprite.
 * The character select's fighters on their gates take it through their
 * player kind (src/dc/objmodel.c); the stage select's preview and the
 * spotlights under those same gates name it outright (src/dc/mnmaps.c,
 * src/dc/mnplayersvs.c). */
void dc_model_proc_display_layered(GObj *gobj);

/* The same in the backdrop band (src/dc/lbcommon.h), for a model a scene
 * draws Z-less before its battle camera: under every 3D pixel, over the
 * stage wallpaper. Final Destination's camera-tag-2 effects
 * (src/dc/sc1pgameboss.c). */
void dc_model_proc_display_backdrop(GObj *gobj);

/* One tree, walked and drawn, for a GObj whose display proc is the
 * game's own rather than one of the two above.
 *
 * `gcDrawDObjTreeForGObj` is the whole of such a proc on the N64 -- the
 * walk composes the matrices and runs each DObj's display list as it
 * goes. Here the walk only records matrices (dc_joint_submit) and the
 * model is submitted afterwards, so a ported proc's one line becomes
 * this one: reset the hide mask, walk, transform, and put the batches in
 * the translucent list at their own 1/w. The off-screen arrows of
 * if/ifcommon.c are the first caller and the reason for
 * the mask: their animation hides and unhides chevrons, and the walk is
 * what knows which. */
void dc_model_draw_tree(GObj *gobj);

/* The same for a tree that has to draw at the frame's next sprite depth
 * rather than at its own 1/w: the handle of the magnifying glass, which
 * the game draws Z-less over the fighter inside the glass and under the
 * HUD (if/ifcommon.c ifCommonPlayerMagnifyProcDisplay's
 * gcDrawDObjDLHead0). Translucent pass only. */
void dc_model_draw_tree_layered(GObj *gobj);

/* The light direction every model is shaded by, in world space, on the
 * scale the RSP reads a Light's direction bytes by: 0x7F is 1.0, so a
 * direction the game wrote as bytes is those bytes / 127, and one it
 * wrote through ftDisplayLightsDrawReflect is the unit vector * 100 /
 * 127. The magnitude is kept -- fighter_frame normalises the joint, not
 * the light -- because the N64 kept it too.
 *
 * A fighter never sees what a scene set here: dc_model_proc_display
 * resolves its light per draw the way ftDisplayMainProcDisplay does
 * (src/dc/objmodel.c fighter_light). A scene sets this for the models
 * that are not fighters -- the stage select's preview -- the way its
 * SYTaskmanSceneSetup.proc_lights would have. Defaults to the direction
 * H2 through H6 shaded everything by. */
void dc_model_set_light(float x, float y, float z);

/* ft/ftdisplaylights.c ftDisplayLightsDrawReflect: the two angles a
 * scene lights its fighters by (degrees), as the world-space direction
 * the RSP would have got, on the 0x7F == 1.0 scale the Light.dir bytes
 * use. ftshade.h has the formula. */
void dc_model_set_light_angles(float angle_x, float angle_y);

/* Triangles the model display procs have submitted since the last reset.
 * src/dc/db.c prints it; nothing in the game asks. */
unsigned dc_model_tris(void);
/* of those, the ones the magnifying glass drew: the fighter inside it,
 * clipped, and its handle */
unsigned dc_model_tris_magnify(void);
/* and the microseconds those took, walk and transform excluded: the
 * cost of the clip is in here and nowhere else */
unsigned dc_model_us_magnify(void);
void dc_model_tris_reset(void);

/* DIVERGES (port only): a scene whose Demo-kind fighters stand among 3D
 * props under cameras of one shared projection (the opening's Room: Master
 * Hand inside the toy box, the trophies on the desk) draws them in the
 * opaque list at their real depth, so the props occlude them as the N64's
 * Z buffer did. The layered draw (above) is for fighters between sprite
 * passes and under their own cameras. Cleared at every scene start
 * (src/dc/scmanager.c). */
void dc_model_set_demo_opaque(sb32 on);

/* On: the layered draw (a menu's Demo fighter) walks and transforms its
 * model again in the translucent pass, at the pose the joints hold
 * there, instead of drawing what the opaque pass left. For a scene that
 * re-poses one fighter within a frame and draws after each pose. Off by
 * default; sc1pmanager.c and scmanager.c clear it with the demo-opaque
 * flag. */
void dc_model_set_layered_rewalk(sb32 on);

/* A model the N64 drew with the Z buffer off, between cameras: the boss
 * wallpaper's trees (sc1pgameboss.c). `band` is where in the frame:
 * DC_MODEL_ZLESS_FRONT the frame's next sprite depth (the layered
 * draw's, each batch flat in display-list order --
 * fighter_draw_layered_paint), DC_MODEL_ZLESS_BACKDROP the backdrop 3D
 * stack's next range and DC_MODEL_ZLESS_BACKDROP_UNDER the under stack's
 * (lbcommon.h lbCommonBackdropStackTake), for what its camera drew into
 * an earlier display-list head -- both fighter_draw_layered_scaled, which
 * keeps the texture's perspective correction, as a backdrop filling the
 * screen needs.
 *
 * The re-walk is on for this call whatever dc_model_set_layered_rewalk
 * says, because such a GObj may share its pack with others drawn the
 * same frame (the 24 comets are one pack) and the opaque pass would
 * leave only the last one's pose. */
#define DC_MODEL_ZLESS_FRONT          1
#define DC_MODEL_ZLESS_BACKDROP       2
#define DC_MODEL_ZLESS_BACKDROP_UNDER 3
void dc_model_proc_display_zless(GObj *gobj, s32 band);

#endif /* SSB_DC_OBJMODEL_H */

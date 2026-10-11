/* stage.h -- a stage pack: the visual layers as a fighter pack (rendered
 * by the exact code that renders fighters), whose one animation is the
 * layers' own AnimJoints over the merged joints, plus the STG3 extras
 * the game logic needs: MPGeometryData's collision tables, the map
 * objects (spawn points), camera and blast bounds, and the BGM id.
 * tools/export/ssb_stageexport.py writes it.
 *
 * The collision tables are the game's own (mp/mptypes.h:16-80), stored
 * little-endian and pointed at in place, and handed straight to the
 * decomp's mpcollision.c through mpCollisionLoadGeometry (mpcommon.h).
 */
#ifndef SSB_DC_STAGE_H
#define SSB_DC_STAGE_H

#include <mp/mptypes.h>
#include <gr/grdef.h>
#include <gr/grtypes.h>
#include <sys/obj.h>
#include <PR/sp.h>

#include "efground.h"
#include "fighter.h"
#include "objmodel.h"
#include "objpvr.h"
#include "sprite.h"

/* The DL link the stage's geometry draws on. The game gives its four
 * layers 4, 6, 13 and 17 (gr/grdisplay.c:10-43 dGRDisplayDescs), and
 * draws 0, 2 and 3 with the z-buffer off: layer 0 is under everything,
 * and layers 2 and 3 paint over whatever came before them -- the fighters
 * (9) and items (11), and for layer 3 the weapons (14) and most effects
 * (15) too.
 *
 * DIVERGES: the port bakes all four into one pack, drawn on link 1 with
 * real depth (fighter.c gZBias keeps the painting order among coplanar
 * pieces). That gives the same picture wherever it matters, which was
 * Layer 0 covers no point of the z=0 play plane from the battle camera,
 * and every vertex of layers 2 and 3 is in front of it (z >= 204), so
 * they cover the fighters as the game's do. What differs is only what the
 * game draws AFTER a layer on top of it however deep it is: a link-18/20
 * model effect behind a layer-3 piece, or a weapon or link-15 effect
 * behind a layer-2 piece, is hidden here and shown there. Above the floor
 * line layer 3 covers at most 4.5% of the plane (Hyrule's edges) and
 * layer 2 under 1% (Castle alone). Particles are not affected: a CLD particle, which
 * the game draws with no depth compare, is PVR_DEPTHCMP_ALWAYS here
 * (src/dc/lbpdraw.c). */
#define STAGE_DLLINK 1

/* One stage builds several map objects off its one map file: Dream Land
 * makes four (the Whispy/eyes tree, the mouth, the flowers behind and the
 * flowers in front), each from its own DObjDesc block. Kongo Jungle, the
 * first stage with one, has the one. */
#define STAGE_MAP_OBJECTS_MAX 4

/* The map object's tree is small -- Kongo Jungle's is two joints -- but
 * it is the tree's own joints the scripts index, so the two counts are
 * what the exporter may write and the loader may read. The anims are
 * pooled across every object, because a stage's own code asks for a
 * script by name: Dream Land's eye and mouth tables are thirty-four
 * between them. */
#define STAGE_MAP_JOINTS_MAX 32
#define STAGE_MAP_ANIMS_MAX 128
/* The pack's own name field, and it has to hold the decomp's symbol names
 * whole: the longest is `WhispyMouthRightBlowTexture` at 27 characters,
 * and a name truncated to 16 makes every by-name lookup miss silently. */
#define STAGE_MAP_ANIM_NAME_MAX 32
#define STAGE_ITEM_WEIGHTS_COUNT 20

/* The graft is small -- Yoshi's Island's cloud is one DObj -- but it is
 * a DObj tree like any other, so its joints go through the same
 * dc_model_add_dobjs call.*/
#define STAGE_GRAFT_JOINTS_MAX 16

/* GRAttackColl (gr/grtypes.h:20-29) is seven s32s: kind, damage, angle,
 * knockback_scale, knockback_weight, knockback_base, element. The map
 * file carries one for an object whose ground logic hands the fighter
 * system a hit descriptor rather than a status -- Zebes' acid is the
 * first -- and the exporter writes it as those seven words. */
#define STAGE_ATTACK_COLL_S32S 7

/* Ground actors (ef/efground.c): background decorations that spawn,
 * drift across, and despawn at runtime -- Peach's Castle's Lakitu is the
 * first (Part B, Phase 1). Unlike a map object (bound once, always
 * present), a ground actor's DObj tree is built fresh every spawn off a
 * shared, stage-owned pack -- the graft's pattern, not map_models'.
 *
 * The four counts below are sized generously for every stage's known
 * need (efground.c's own per-stage EFGroundParam weight tables put the
 * largest around six to seven entries), not just Castle's two -- but
 * only Castle is actually populated as of Phase 1; verify a stage's real
 * counts against its own data before assuming these cover it. */
#define STAGE_GROUND_MODELS_MAX 8
#define STAGE_GROUND_DESCS_MAX 8
#define STAGE_GROUND_PARAMS_MAX 8

/* A ground actor's own tree is small -- Lakitu's is a handful of joints
 * -- but lbCommonAddTreeDObjsAnimAll wants an array exactly as long as
 * the tree it walks, the same requirement map_anim_joints has. */
#define STAGE_GROUND_JOINTS_MAX 8

/* BWP1 (Final Destination only): the trees sc/sc1pmode/sc1pgameboss.c
 * instances Master Hand's background from. Five packs, the largest
 * eight AnimJoint scripts (Effects1) -- but a boss effect's tree runs to
 * NINE joints, where a ground actor's is a handful, so this mask and the
 * anim row have to be wider than STAGE_GROUND_JOINTS_MAX. */
#define STAGE_BOSS_MODELS_MAX 8
#define STAGE_BOSS_JOINTS_MAX 16
#define STAGE_BOSS_ANIMS_MAX 16

/* BPL1 (every Board the Platforms course): the six trees relocData 136
 * (Bonus2Common) holds -- Small, Medium and Large, then the same three
 * boarded, which is the order dSC1PBonusStagePlatformDescs and
 * dSC1PBonusStageBoardedPlatformDescs index them in. Three joints each
 * and two scripts each, so the joint and anim bounds BWP1 already has
 * are more than these need and they share them. */
#define STAGE_PLATFORM_MODELS_MAX 6

/* BTG1 (the twelve Break the Targets courses only). Ten is
 * sc/scdef.h's SCBATTLE_BONUSGAME_TASK_MAX and the game hangs on any
 * other count; the anim bound is the worst course (Fox, three moving
 * targets) with room. */
#define STAGE_TARGETS_MAX 10
#define STAGE_TARGET_ANIMS_MAX 8

/* BMP1 (five of the twelve Board the Platforms courses). No count the
 * game insists on here, so these are the worst course with room: Fox
 * stands nine bumpers and eight of them move. The anim bound is much
 * larger than BTG1's because a bumper's script SetInterps to an
 * `SYInterpDesc` and a descriptor's four entries -- itself and its
 * three float arrays -- are entries in the same array:
 * Fox's eight scripts and four descriptors come to 23. */
#define STAGE_BUMPERS_MAX 10
#define STAGE_BUMPER_ANIMS_MAX 32

typedef struct
{
    void *blob;                 /* whole .stg file: the romdisk's own
                                   bytes where they can be mapped, a copy
                                   otherwise -- see blob_owned */
    int blob_owned;             /* the blob is this Stage's to free */
    Fighter model;              /* the visual layers */

    /* MPGroundData.map_geometry (mp/mptypes.h:71-80), tables in blob */
    MPGeometryData geo;
    uint16_t line_count;        /* sizes the game derives; here for
                                   bounds checks and the debug overlay */
    uint16_t vertex_id_count;
    uint16_t vertex_count;

    /* MPGroundData.camera_bound_* / map_bound_* / alt_warning / bgm_id
     * (mp/mptypes.h:190-203) */
    int16_t cam_top, cam_bottom, cam_right, cam_left;
    int16_t map_top, map_bottom, map_right, map_left;
    int16_t alt_warning;
    /* The COLLISION layer's place among the merged joints: layer 1's
     * first joint and how many it has. yakumono id k is layer-1 joint k
     * (gcSetupCustomDObjs fills gMPCollisionYakumonoDObjs->dobjs one per
     * DObjDesc in order), which is what lets grCommonSetupInitAll point
     * that array at real DObjs instead of mpcommon.c's stand-ins. 0/0
     * for a stage file written before they existed. */
    uint16_t layer1_jbase, layer1_joints;
    float light_angle[3];       /* MPGroundData.light_angle, radians */
    /* MPGroundData.emblem_colors, per player, and a fifth: the CPU's
     * (colour index 4), which the game reads one past the declared array,
     * out of MPGroundData.unused */
    uint8_t emblem[5][3];
    uint32_t bgm;
    uint32_t fog_rgb;
    /* MPGroundData's team bounds and zoom pair (mp/mptypes.h:201-210) */
    int16_t cam_team[4], map_team[4];
    int16_t zoom_start[3], zoom_end[3];

    pvr_ptr_t wp_txr;           /* wallpaper texture, or NULL if none */
    pvr_poly_hdr_t wp_hdr;
    uint16_t wp_texw, wp_texh;  /* power-of-two texture dims */
    uint16_t wp_w, wp_h;        /* image dims within it (300x220) */

    /* the same wallpaper as a Sprite, for the code that draws one
     * through the sprite renderer rather than as a camera's backdrop:
     * stage_wallpaper_sprite fills these on the first ask */
    Sprite wp_sprite;
    Bitmap wp_bitmap;
    DCSpriteTex wp_tex;

    /* ---- MPK1: the stage's map file (MPGroundData.map_nodes and
     * MPItemWeights, mp/mptypes.h:198-199) ---------------------------------
     *
     * The game reaches a stage's own map objects -- Kongo Jungle's
     * barrel cannon, Hyrule's twister positions -- through a pointer in
     * its map logic file into the map data file beside it, and reads
     * them as the N64's own DObjDesc arrays and AObjEvent32 scripts.
     * The port's stage pack is its own format, so the exporter bakes the
     * tree the way it bakes a visual layer and carries the scripts as
     * the u32 words they are; what survives is the shape the game
     * depends on, the tree's joint order and the scripts that address
     * them by index. */

    /* the map objects' DObjDesc trees, baked, in the order the stage's
     * own file builds them: map_models[0] is the tree
     * MPGroundData.map_nodes names, and the rest are the objects the
     * stage builds alongside it. map_models[i].hd is NULL past the last
     * one the pack carries; map_joints[i] holds the DObjs
     * dc_model_add_dobjs built for object i, in the map file's own joint
     * order. */
    Fighter map_models[STAGE_MAP_OBJECTS_MAX];
    int map_pal_banks[STAGE_MAP_OBJECTS_MAX];   /* fighter_init's out
                                                   param, unused */
    int map_object_count;

    /* The GRAFT: a second pack, of the object a stage's own code builds
     * at runtime under its map tree rather than off a DObjDesc of its
     * own -- Yoshi's Island's clouds are the case, one DObj carrying a
     * display list and a material, grafted onto each of the skeleton's
     * cloud joints. graft_model.hd is NULL for a stage with none. */
    Fighter graft_model;
    int graft_pal_bank;
    DObj *graft_joints[STAGE_GRAFT_JOINTS_MAX];
    DObj *map_joints[STAGE_MAP_OBJECTS_MAX][STAGE_MAP_JOINTS_MAX];

    /* the stage's AnimJoint scripts (gr/grcommon/<stage>.c plays them
     * through gcAddAnimJointAll / gcAddDObjAnimJoint, which take the
     * game's own AObjEvent32*). Each is the script's words with the one
     * relocated pointer -- SetAnim's, which is how a looping script
     * names itself -- written in place at load. */
    AObjEvent32 *map_anims[STAGE_MAP_ANIMS_MAX];
    const char *map_anim_names[STAGE_MAP_ANIMS_MAX];

    /* WHICH JOINT of its tree each script belongs on. The game's own
     * walk is a per-DObj array one entry per joint (sys/objanim.c:228-244
     * advances `anim_joints` once per gcGetTreeDObjNext), and a stage's
     * blocks are tables that put their script wherever the table says --
     * Dream Land's eyes carry theirs on joint 2 of 3, not joint 0. A
     * script attached to the wrong joint is a wrong animation; an array
     * SHORTER than the tree is a walk off its end. */
    int map_anim_joints[STAGE_MAP_ANIMS_MAX];
    int map_anim_count;

    /* MPGroundData.item_weights: one byte per common item kind, which
     * the randomizer reads to decide what drops. NULL for a stage whose
     * map file names none (Sector Z's is not a weight table at all). */
    uint8_t *item_weights;

    /* GRAttackColl from the map file, or NULL: what a stage's ground
     * logic hands ftMainCheckAddGroundHazard's callback to answer with
     * (gr/grtypes.h:20-29). Lives in map_owned with the scripts. */
    GRAttackColl *attack_coll;

    /* the script copies and the weights above, one allocation this
     * Stage owns: the scripts are not const -- each has a relocated
     * pointer written into it at load, and the pack's bytes are the
     * romdisk's own, which are not ours to write. */
    void *map_owned;

    /* ---- GRA1: the stage's ground actors (optional, Part B) ---------
     *
     * One baked pack per unique actor identity (Castle needs one: the
     * shared Lakitu tree, ground_models[0]) and one EFGroundDesc-
     * equivalent entry per spawnable variant (Castle needs two: Right-
     * and Left-facing Lakitu, both off ground_models[0]). ground_descs
     * and ground_params are the REAL decomp types (ef/eftypes.h) --
     * their scalar fields (alt_high, effect_weight, ...) are exactly what
     * ef/efground.c reads -- but effect_desc.file_head/o_dobjsetup/
     * o_mobjsub/o_anim_joint/o_matanim_joint are unused: the port's
     * rewritten efGroundMakeEffect reads ground_assets (efground.h)
     * instead, by the same index. */
    Fighter ground_models[STAGE_GROUND_MODELS_MAX];
    int ground_pal_banks[STAGE_GROUND_MODELS_MAX];
    int ground_model_count;

    EFGroundParam ground_params[STAGE_GROUND_PARAMS_MAX];
    int ground_param_count;

    EFGroundDesc ground_descs[STAGE_GROUND_DESCS_MAX];
    int ground_desc_count;

    /* efground.h's EFGroundActorAsset, one per ground_descs[] entry.
     * anim_joint (when not NULL) points into ground_anim_tables below. */
    EFGroundActorAsset ground_assets[STAGE_GROUND_DESCS_MAX];

    /* ground_assets[i].anim_joint's own storage: one row per
     * ground_descs[] entry, one AObjEvent32* per joint of that entry's
     * pack -- built once at load from the GRA1 block's named scripts,
     * the same expansion stage_map_anim_array does for a map object's. */
    AObjEvent32 *ground_anim_tables[STAGE_GROUND_DESCS_MAX][STAGE_GROUND_JOINTS_MAX];

    /* the ground scripts' word copies, one allocation this Stage owns
     * (map_owned's sibling, same reason: not the romdisk's to write). */
    void *ground_owned;

    /* ---- BWP1: the boss wallpaper's trees (Final Destination only) --
     *
     * GRA1's models + anims and nothing else. There is no desc array
     * here because a boss effect has no EFGroundDesc: what a desc would
     * carry (the plans, the spawn counts, the speeds, the row swaps) is
     * sc1pgameboss.c's own .data, which the port compiles rather than
     * exports -- so what the .stg has to carry is only what lives in the
     * ROM, and sc1pgameboss.c names the tree it wants by the same name
     * its own source does (stage_boss_pack). */
    Fighter boss_models[STAGE_BOSS_MODELS_MAX];
    int boss_pal_banks[STAGE_BOSS_MODELS_MAX];
    int boss_model_count;
    char boss_names[STAGE_BOSS_MODELS_MAX][STAGE_MAP_ANIM_NAME_MAX];

    /* bit k: pack joint k is a billboard joint (its DObjDesc id carries
     * 0xF000) -- sc1PGameBossSetupBackgroundDObjs' own flag, GRA1's
     * `billboard` widened to STAGE_BOSS_JOINTS_MAX. */
    uint16_t boss_billboard[STAGE_BOSS_MODELS_MAX];

    /* how many MatAnimJoints the pack carries as alts: a
     * SC1PGameBossAnim row picks one by index (Effects2_1 is the only
     * tree with two, Anims2_1 for wallpaper row 2 and Anims3_0 for
     * row 3). */
    uint8_t boss_matanim_count[STAGE_BOSS_MODELS_MAX];

    /* one AObjEvent32* per joint of each pack's tree, expanded once at
     * load from the block's named scripts -- ground_anim_tables' shape,
     * indexed by PACK rather than by desc. NULL where the tree's own
     * AnimJoint table gives that joint no script, and the whole row NULL
     * for a pack whose SC1PGameBossAnim rows carry no AnimJoint at all
     * (Effects2_1). */
    AObjEvent32 *boss_anim_tables[STAGE_BOSS_MODELS_MAX][STAGE_BOSS_JOINTS_MAX];

    /* the boss scripts' word copies (ground_owned's sibling). */
    void *boss_owned;

    /* ---- BTG1: a Break the Targets course's targets --
     *
     * Where the ten targets stand and, for the one to three of them that
     * move, the script they follow. sc1PBonusStageMakeTargets
     * (sc/sc1pmode/sc1pbonusstage.c:434) reads both out of the course's
     * layer file by reloc label, which the port has no runtime for, so
     * they ride in the pack.
     *
     * No models here, unlike GRA1 and BWP1: a target's geometry belongs
     * to the ITEM (nITKindTarget, relocData 253/150), loaded once and
     * shared by all ten, so there is nothing per-course to bake.
     *
     * target_count is 0 on every stage but the twelve courses. The game
     * hangs the console if a course has anything but ten
     * (SCBATTLE_BONUSGAME_TASK_MAX), so the exporter refuses to write
     * one that does and this stays a fact rather than a check. */
    Vec3f targets[STAGE_TARGETS_MAX];

    /* which of target_anims each target follows, or -1 for a still one.
     * Parallel to `targets`. */
    int8_t target_anim[STAGE_TARGETS_MAX];
    uint8_t target_count;
    uint8_t target_anim_count;

    /* the scripts themselves, shared: two targets may follow one. */
    AObjEvent32 *target_anims[STAGE_TARGET_ANIMS_MAX];

    /* the target scripts' word copies (boss_owned's sibling). */
    void *target_owned;

    /* ---- BMP1: a Board the Platforms course's bumpers --
     *
     * The same block one magic apart, read by the same loader, and here
     * for the same reason: sc1PBonusStageMakeBumpers
     * (sc/sc1pmode/sc1pbonusstage.c:700) is
     * sc1PBonusStageMakeTargets with nITKindGBumper in place of
     * nITKindTarget, and finds its two tables by the same reloc
     * arithmetic off gMPCollisionGroundData->map_nodes.
     *
     * Only five of the twelve courses have any (Fox 9, Kirby 5, Purin 5,
     * Samus 2, Ness 2); the other seven and every non-bonus stage leave
     * bumper_count 0. Unlike the targets there is no count the game
     * insists on -- the DObjDesc array's own terminator ends the walk --
     * so this one is read as it lies. */
    Vec3f bumpers[STAGE_BUMPERS_MAX];

    /* which of bumper_anims each bumper follows, or -1 for a still one.
     * Parallel to `bumpers`. Exactly one bumper per course is still. */
    int8_t bumper_anim[STAGE_BUMPERS_MAX];
    uint8_t bumper_count;
    uint8_t bumper_anim_count;

    /* the scripts themselves, shared the way the targets' are. */
    AObjEvent32 *bumper_anims[STAGE_BUMPER_ANIMS_MAX];

    /* the bumper scripts' word copies (target_owned's sibling). */
    void *bumper_owned;

    /* ---- BPL1: the shared platform trees ------------
     *
     * The same block BWP1 is, read by the same body, and held the same
     * way -- but it does NOT ride in the .stg. The six trees belong to
     * all twelve Board the Platforms courses at once, and baked they
     * come to 229 KB, so they are romdisk/bonus2plat.pak and every
     * course's Stage loads that one file (stage_load_platforms).
     *
     * Named, not indexed, for the reason the boss effects are: the
     * descs sc1pbonusstage.c reaches them by are pointer-to-pointer
     * into a file the port does not relocate, so the tree it wants is
     * found by the name its own source gives it. */
    Fighter platform_models[STAGE_PLATFORM_MODELS_MAX];
    int platform_pal_banks[STAGE_PLATFORM_MODELS_MAX];
    int platform_model_count;
    char platform_names[STAGE_PLATFORM_MODELS_MAX][STAGE_MAP_ANIM_NAME_MAX];
    uint16_t platform_billboard[STAGE_PLATFORM_MODELS_MAX];
    uint8_t platform_matanim_count[STAGE_PLATFORM_MODELS_MAX];
    AObjEvent32 *platform_anim_tables[STAGE_PLATFORM_MODELS_MAX]
                                     [STAGE_BOSS_JOINTS_MAX];

    /* the platform scripts' word copies, and the file they were read
     * out of: unlike every other block here the bytes are not the
     * .stg's, so this Stage owns them both. */
    void *platform_owned;
    void *platform_blob;
    int platform_blob_owned;
} Stage;

/* Which of this stage's BWP1 packs `name` is, or -1. sc1pgameboss.c
 * names the five by the names its own source gives them ("Effects0",
 * "Effects2_1", ...), so a config edit that reorders them cannot
 * silently repoint an effect at another tree. */
int stage_boss_pack(Stage *st, const char *name);

/* Which of the six shared platform trees `name` is, or -1. The names are
 * the decomp's own ("PlatformSmall", "BoardedPlatformLarge", ...), so
 * sc1pbonusstage.c asks for a tree the way its own source names it.
 * Only a Board the Platforms course has any; every other stage answers
 * -1 to all six. */
int stage_platform_pack(Stage *st, const char *name);

/* Load a .stg from romdisk/disc; textures upload, poly headers compile.
 * Returns 0, or -1 with the reason on dbglog. Collision and music are
 * not bound here: call stage_bind when the stage becomes the current
 * one, as the game's ground setup does. */
int stage_load(Stage *st, const char *name, int *pal_bank);

/* Make this stage the current one: publish its MPGroundData scalars and
 * remember it, so the three calls below can act on it. Nothing here comes
 * out of the scene heap, so it may be called before a scene starts --
 * which is where src/dc/db.c's boot calls it, standing in for the stage
 * select.
 *
 * The collision tables and the music are not bound here. Both are their
 * own line in the battle scene's sequence and src/dc/scvsbattle.c makes
 * them there; the tables have to be, because the scene heap they come
 * out of is emptied on the way into every scene. */
void stage_bind(Stage *st);

/* The port's mpCollisionInitGroundData (sc/sccommon/scvsbattle.c:155):
 * load the current stage's collision tables into the scene heap. Call it
 * from inside a scene -- see stage.c for why. */
void stage_bind_collision(void);

/* The two gr/ calls scVSBattleStartBattle makes that the port can
 * already answer, under the game's names -- see the comments on them in
 * stage.c for what each one diverges from. Both act on the stage
 * stage_bind made current. */
GObj *grCommonSetupInitAll(void);
void grWallpaperMakeDecideKind(void);

/* grwallpaper.c:335: let every wallpaper actor run again, what a 1P
 * match's ending freeze calls (src/dc/ifcommon.c's
 * ifCommon1PGameInterfaceProcSet). */
void grWallpaperResumeProcessAll(void);

/* The stages that are loaded *right now*, indexed by nGRKind*
 * (gr/grdef.h), NULL for a kind nobody is holding. The game's equivalent
 * is a relocData file id per stage in a scene's table (mn/mnmaps/mnmaps.c
 * dMNMapsFileInfos) and a DMA into a heap when the stage is wanted; this
 * table is what that DMA left behind, and the stage select reads it to
 * build a preview and the battle to bind the stage the player chose --
 * exactly as gFTManagerModels (ftcommon.h) holds the fighters'.
 *
 * The table is filled by grStageAcquire and emptied by grStageRelease,
 * which is what the game does with a map file.
 *
 * It was nGRKindInishie + 1 while every stage the port had was a VS
 * stage. Final Destination is the first that is not:
 * nGRKindLast sits past nGRKindBattleEnd, above the seven kinds the 1P
 * ladder and How to Play still want (Beta Dream Land, the Test Stage,
 * How to Play, Small Yoshi's Island, Meta Crystal, Duel Zone and Race
 * to the Finish), so the table is taken to the end of the common stages
 * in one move rather than raised a kind at a time. Those seven read
 * NULL here, which grStageAcquire already treats as "no such stage".
 * The table extends to the end of the bonus maps, for the twelve Break
 * the Targets courses. The kinds above common stages (Board the
 * Platforms) read NULL, which grStageAcquire already handles.
 *
 * DIVERGES from src/dc/sndres.h's SNDRES_STAGES, which stops at
 * nGRKindCommonEnd and stays there. That count is one sound set per
 * stage, and a bonus map needs none: every one of the twenty-four plays
 * nSYAudioBGM1PBonusStage and nothing else, so the bonus scene's own
 * group carries the one track and twenty-four near-empty sets would be
 * twenty-four wasted rows in sndsets.bin. The two bounds are not the
 * same question and no longer the same number.
 */
#define GR_STAGES_MAX (nGRKindBonusStageEnd + 1)
extern Stage *gGRStages[GR_STAGES_MAX];

/* The port's dMNMapsFileInfos (mn/mnmaps/mnmaps.c:28-39): the pack a
 * kind's stage is read from, NULL for a kind the port has no pack for
 * yet. What "does this stage exist" asks, without loading it. */
const char *grStageFileName(s32 gkind);

/* Load gkind's stage if nothing holds one already, and take a reference
 * on it; the loaded Stage, or NULL for a kind with no pack or a load
 * that failed. The reference count is the port's, not the game's: the
 * game's heaps make the same guarantee by construction (a map file is
 * force-loaded into a heap whose lifetime the caller owns) and the port
 * has one shared table instead, which the stage select and the battle
 * can both want the same entry of.
 *
 * Every acquire is matched by a release, and the last release frees the
 * blob and gives the textures back to VRAM. */
Stage *grStageAcquire(s32 gkind);
void grStageRelease(s32 gkind);

/* Everything a loaded stage holds: its blob, its model's VRAM and
 * headers, its wallpaper texture. Safe on a half-loaded stage (a
 * stage_load that failed part way) and on one that is currently bound,
 * which it unbinds. grStageRelease is how a caller normally reaches it;
 * this is public for a Stage a caller owns outright. */
void stage_release(Stage *st);

/* gcAddAnimAll for a stage's merged layers on `gobj`, whose tree
 * dc_model_add_dobjs built from st->model: the layers' MObjs with their
 * MatAnimJoint, and the layers' AnimJoint table (the one the battle's
 * grCommonSetupInitAll attaches). TRUE when anything was attached --
 * what mn/mnmaps/mnmaps.c mnMapsMakeLayer asks before it plays the tree
 * once. A no-op returning FALSE in the host build, which cannot walk a
 * 32-bit script. */
sb32 stage_add_layer_anims(Stage *st, GObj *gobj);

/* ---- the stage's map file (MPK1, above) ------------------------------ */

/* The stage a scene is playing on, or NULL before one is bound: what
 * src/dc/stage.c's own sStage is, for the port's stage logic to read the
 * map file's own numbers off (src/dc/grjungle.c's diagnostic line). */
Stage *stage_bound(void);

/* Build the bound stage's map object tree on `gobj` and wire its display
 * and per-frame anim, the way gr/grcommon/<stage>.c does with
 * grModelSetupGroundDObjs plus gcAddGObjDisplay and gcAddGObjProcess.
 * Returns the tree's root DObj, or NULL for a stage whose map file names
 * no tree. Call it once the scene has an object pool, i.e. from inside a
 * scene's own setup and not from stage_load. */
DObj *stage_bind_map_model(GObj *gobj, s32 dl_link);

/* The same, for the bound stage's map object number `index` -- what a
 * stage whose own file builds more than one object needs (Dream Land
 * makes four). index 0 is what stage_bind_map_model binds. Returns NULL
 * when the pack carries no such object. */
DObj *stage_bind_map_object(GObj *gobj, s32 index, s32 dl_link);

/* Graft the bound stage's graft object under `parent` -- one pack, one
 * DObj carrying a display list and a material, which the stage's own
 * code builds at runtime rather than off a DObjDesc (Yoshi's Island's
 * clouds). `alt` picks which of the pack's MatAnimJoints the material
 * starts on. Returns the graft's root DObj, or NULL for a stage with
 * none. Call it once the scene has an object pool. */
DObj *stage_bind_map_cloud(GObj *gobj, DObj *parent, int alt);

/* Re-point the graft's MObjs at another of the pack's MatAnimJoints:
 * what grYosterUpdateCloudAnim does when a cloud changes state. A no-op
 * when the bound stage has no graft. */
void stage_cloud_set_anim(DObj *dobj, int alt);

/* Re-point map object `index`'s MObjs at another of its pack's
 * MatAnimJoints: what grPupupuUpdateGObjAnims does when Whispy's eyes or
 * mouth change state. A no-op when the object or its pack has none --
 * every object whose materials are baked rather than animated. */
void stage_map_set_anim(DObj *dobj, int index, int alt);

/* One of the stage's map-file AnimJoint scripts by name -- the port's
 * stand-in for the decomp's `map_head + &llGR<Stage>Map<Name>AnimJoint`.
 * NULL when the bound stage's map file has no script by that name. */
AObjEvent32 *stage_map_anim(const char *name);

/* Which joint of its map object's tree that script belongs on, or -1 if
 * the bound stage's map file has no script by that name. */
int stage_map_anim_joint(const char *name);

/* EVERY script the bound stage's map file has under that name, as the
 * per-DObj array `gcAddAnimAll` / `gcAddAnimJointAll` actually want:
 * `out` is filled with a NULL at every joint and the stage's scripts at
 * theirs. A name is not one script -- it is a TABLE, one entry per joint
 * of the tree it belongs to -- and the array MUST be as long as that
 * tree, because the game walks one entry per joint (sys/objanim.c:228).
 * `out` must have STAGE_MAP_JOINTS_MAX entries. */
void stage_map_anim_array(const char *name, AObjEvent32 **out);

/* The bound stage's map object `obj`'s joints, in the map file's own
 * order: what the port uses where the game's own code says
 * DObjGetStruct(ground_gobj)->child. NULL out of range. */
DObj *stage_map_joint(int obj, int index);

/* The stage's background as a Sprite the sprite renderer can draw: what
 * MPGroundData.wallpaper is on the N64 (mp/mptypes.h:182), which the
 * stage select hands straight to lbCommonMakeSObjForGObj. The pack
 * stores the image as one PVR texture rather than a strip list, so this
 * is one Bitmap over it. Owned by the Stage; NULL when it has none. */
Sprite *stage_wallpaper_sprite(Stage *st);

/* Training mode's flat background over the stage's own, drawn at
 * grWallpaperMakeStatic's placement (sc1PTrainingModeLoadWallpaper).
 * NULL, or a texture that is not ARGB1555, keeps the stage's own.
 * Forgotten at every scene change (grOverlayLoad). */
void stage_set_wallpaper_override(const DCSpriteTex *tex);

/* Submit the stage background quad to the open OP list, positioned the
 * way the game positions stage `gkind`'s wallpaper sprite for this camera
 * (src/dc/grwallpaper.h). No-op when the stage has no wallpaper. */
void stage_draw_wallpaper(Stage *st, s32 gkind, float eye_x, float eye_y,
                          float eye_z, float at_x, float at_y,
                          float at_z);

#endif /* SSB_DC_STAGE_H */

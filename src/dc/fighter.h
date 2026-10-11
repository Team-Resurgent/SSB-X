/* A fighter at runtime: the transpiled model plus its whole animation set,
 * loaded from a .pack written by tools/export/ssb_packexport.py.
 *
 * The pack *is* the in-memory format -- little-endian, 4-byte-aligned
 * sections, laid out as the structs below -- so loading is: read the file,
 * check the header, turn eight offsets into pointers. No byte-swapping, no
 * parsing, and one relocation: the pointer words of an AnimJoint animation
 * (FPackAnim.kind) become addresses once, at load, the way the game's own
 * reloc walk makes them. The one thing that leaves the blob is texture
 * data, which is uploaded to PVR memory and its pages freed conceptually
 * (the blob is kept whole; textures are a few hundred bytes of it).
 */
#ifndef DC_FIGHTER_H
#define DC_FIGHTER_H

#include <dc/pvr.h>
#include <stdint.h>

#include "mtx.h"

#define FPACK_MAGIC "SSBPACKA"
#define FPACK_ANIM_NAME_LEN 40
/* FPackAttr.hiddenparts rows; the widest table on the roster is Samus's 13 */
#define FPACK_HIDDENPART_MAX 16
/* FPackAttr.thrown_status rows: one per nFTKind* (ft/ftdef.h
 * nFTKindEnumCount), the same width on every fighter */
#define FPACK_THROWN_STATUS_NUM 27
/* FPackAttr.translate_scales rows: one per entry of the game's joints
 * array (ft/ftdef.h FTPARTS_JOINT_NUM_MAX; src/dc/ftmanager.c holds the
 * two equal) */
#define FPACK_JOINT_NUM_MAX 37
/* FPackAttr.anm_tier_end: the .anm's tiers short of the whole file */
#define FPACK_ANM_TIERS 2

/* The viewport clip space maps onto, in framebuffer pixels: centre and
 * half-size. x = cx + ndc_x * hw, y = cy - ndc_y * hh, which is the
 * RSP's viewport transform (vscale, vtrans) in float. The camera pass
 * sets it from the camera's CObj.viewport (src/dc/objdisplay.c
 * gcPrepCameraViewport, through objpvr.h); a draw with no camera leaves
 * it at the whole 640x480. */
typedef struct DCViewport
{
    float cx, cy, hw, hh;
} DCViewport;

extern DCViewport gDCViewport;

typedef struct
{
    char magic[8];
    uint32_t joint_count, vert_count, tri_count, batch_count;
    uint32_t tex_count, pal_count, anim_count, texdata_size;
    uint32_t off_joints, off_verts, off_tris, off_batches;
    uint32_t off_texs, off_pals, off_texdata, off_anims;
    float center[3], radius;
    char name[8];
    uint32_t off_attr;          /* 0 when the pack has no attributes */
    uint32_t off_motion, motion_count;
    /* The fighter's MainMotion file (ssb-decomp-re/src/relocData/
     * NNN_<F>MainMotion.c): the ftMotionCommand scripts FPackMotion.script
     * points into, and the throw tables beside them, as u32 words in
     * host order, followed by a copy of the common moveset file
     * (201_FTCommonMoveset.c: the item swings and thrown-damage scripts
     * the fighter's own scripts call). The words listed by index at
     * off_script_reloc hold an offset into these words that fighter_init
     * turns into a pointer, the way lbRelocLoadAndRelocFile walks both
     * files' intern chains and MainMotion's extern chain for the game.
     * 0/0 for a pack with no scripts (a stage). */
    uint32_t off_script, script_words;
    uint32_t off_script_reloc, script_reloc_count;
    /* Pack offset of the FPackMObjs below, or 0 for a pack whose
     * materials are entirely baked. The emblems
     * (tools/export/ssb_emblemexport.py) set it, whose MObjs are recoloured per
     * player; so do the character select's spotlight
     * (tools/export/ssb_spotexport.py), whose one MObj carries the primitive
     * colour its display list multiplies the mask by, the stage objects
     * whose materials animate, and Samus's pack, whose grapple beam's
     * part MObjs play their own flicker (tools/export/ssb_packexport.py
     * fighter_mobjs). A fighter's are hung by the part
     * setters (src/dc/ftparam.c ftParamGetPartMObjs), not at spawn. */
    uint32_t off_mobjs;
} FPackHeader;

/* ---- animated materials ----
 *
 * A pack's batch carries the colours its display list and MObj chain
 * settled on at bake time. That is the whole story for a model whose
 * materials never change; an emblem's MObjs are handed a MatAnimJoint that recolours them
 * per player, so the pack has to carry the MObjs themselves and let the
 * object system's own parser run. The spotlight carries them for the
 * lesser reason: its MObj is constant, but it is where the primitive
 * colour comes from, and reaching it through the game's own
 * gcAddMObjAll is the same code either way.
 *
 * What the pack carries is what a relocData file carries, minus the two
 * shapes that are pointers: the `MObjSub **` per DObj and the
 * `AObjEvent32 **` beside it are rebuilt at load (src/dc/objmodel.c
 * dc_model_add_mobjs) and handed to the decomp's gcAddMObjAll and
 * gcAddMatAnimJointAll unchanged.
 */
/* The MObjSub.flags bits a pack's material carries, by the values
 * sys/objtypes.h:49-56 gives them. Spelled apart from the decomp's
 * MOBJ_FLAG_* so that a file reaching both headers has no redefinition:
 * these are the same four bits under the port's own name. The other
 * twelve -- the ones that load a TLUT or recompute tile size -- are
 * gcDrawMObjForDObj's and are not ported; ssb_emblemexport.py and
 * ssb_spotexport.py both refuse a pack that sets one.
 *
 * FPACK_MOBJ_ALPHA is the one that picks a texture out of the MObj's
 * sprite array (objdisplay.c:1288's `flags & 0x11`). The port carries
 * that array as a run of textures in the pack's own table -- tex_first
 * to tex_first + tex_count -- because the picture is the one thing a
 * MatAnimJoint can change that a poly header baked at load cannot
 * follow. The damage slash (tools/export/ssb_effectexport.py --what slash) is
 * the first model in the port to have one. */
#define FPACK_MOBJ_ALPHA   (1 << 0)
#define FPACK_MOBJ_PRIM    (1 << 9)
/* The fourth, and the newest: efManagerDeadExplodeMakeEffect writes a
 * player's colour into two of the explosion's three MObjs and sets this
 * bit on them (ef/efmanager.c:4825-4835). Only a FPACK_ENVLERP batch
 * reads it. */
#define FPACK_MOBJ_ENV     (1 << 10)
#define FPACK_MOBJ_LIGHT1  (1 << 12)
#define FPACK_MOBJ_LIGHT2  (1 << 13)

typedef struct
{
    uint16_t flags;             /* MObjSub.flags, the game's own bits */
    uint8_t prim_l, pad;
    uint32_t primcolor, envcolor, blendcolor, light1color, light2color;
    /* The MObj's sprite array, as pack texture indices. tex_count is 1
     * and tex_first is the batch's own texture where nothing animates
     * the picture, which is every MObj in the port but the slash's two;
     * where it is more, frame N is texture tex_first + N and the loader
     * has compiled one poly header per frame. */
    int16_t tex_first, tex_count;
    /* The MObj's SECOND tile's texture coordinates, for a batch whose
     * combiner cross-fades two of them (FPACK_TEXLERP). gcDrawMObjForDObj
     * moves that tile's origin with the material animation's scrollu and
     * scrollv tracks (objdisplay.c:1385-1397); the port cannot re-load a
     * tile, so it translates the UVs instead, by
     *
     *     (live track - uv_base) * uv_scale
     *
     * where uv_base is the value the bake froze into the vertices and
     * uv_scale is the normalised distance one unit of the track moves the
     * origin -- which works out to 1/scau and 1/scav, the tile's own
     * repeat counts, so one whole unit of the track is exactly one repeat
     * and the loop is seamless. Zero for every MObj that does not animate
     * its UV, and then the offset is identically zero. */
    float uv_base[2], uv_scale[2];
    /* The same pair for the FIRST tile, against its own trau/trav tracks
     * (objdisplay.c:1353-1382): uv0_base is the value the bake froze in,
     * so the baked coordinates are tile 0 at rest. Every batch the MObj
     * draws is translated by it, cross-fade or not -- Final
     * Destination's fog and vortex, Meta Crystal's shimmer, Zebes' acid
     * and Dream Land's clouds scroll this way. */
    float uv0_base[2], uv0_scale[2];
    /* MOBJ_FLAG_PALETTE dynamic recolour: dpal_count > 1
     * for a MObj whose script steps palette_id through more than one
     * value (Run's crash, Clash's wallpaper quadrants). tex_count stays
     * 1 for these -- the pixels never change, only the 16-colour TLUT
     * does -- so baking one texture and one PVR bank per step the way
     * tex_first/tex_count do for a sprite array would cost a bank PER
     * STEP (FPACK_PAL_BANKS 64, shared by every pack resident at once;
     * Run's crash alone needs 130 after every dedupe). Instead dpal_bank
     * names the ONE bank every batch this MObj draws was baked against,
     * and dpal_first/dpal_count locate dpal_count raw 16-entry frames at
     * Fighter.dpals[dpal_first], in the pack's own colour order
     * (tools/lib/ssb_assets.py read_palette) -- src/dc/objmodel.c
     * dc_joint_material rewrites that bank's entries from the current
     * one in place whenever palette_id changes, the same DMA the N64
     * does on a TLUT swap. (0, 0, -1) -- dpal_count 0 -- for every MObj
     * that does not animate this way, which is every one but those two
     * packs' four MObjs each. */
    int16_t dpal_first, dpal_count, dpal_bank, dpal_pad;
} FPackMObjSub;

/* What one MObj is carrying at draw time. gcDrawMObjForDObj fills these
 * in from the DObj's live MObj chain just before the model is submitted,
 * exactly where the N64 emits the gSPLightColor pair (objdisplay.c:1212),
 * and draw_batches reads them instead of the batch's baked colours. */
typedef struct DCMObjColor
{
    uint32_t prim, light1, light2;
    uint32_t env;               /* 0xRRGGBBAA as the game packs it; only
                                 * a FPACK_ENVLERP batch reads it, and
                                 * only its top three bytes */
    uint16_t flags;             /* MObjSub.flags: which of the four the
                                 * MObj actually drives */
    uint16_t live;              /* 0 until a display walk has seen it */
    uint16_t tex;               /* MObj.texture_id_curr: which frame of
                                 * the sprite array the MatAnimJoint has
                                 * the MObj on now */
    uint16_t tex2;              /* MObj.texture_id_next: the second tile a
                                 * FPACK_TEXLERP batch cross-fades to */
    float lfrac;                /* MObj.lfrac, the cross-fade itself --
                                 * what objdisplay.c:1243 hands the RDP as
                                 * the primitive colour's LOD byte, and
                                 * what PRIM_LOD_FRAC reads there */
    float du, dv;               /* the second tile's UV translation, from
                                 * FPackMObjSub.uv_base/uv_scale */
    float du0, dv0;             /* the first tile's, from uv0_base and
                                 * uv0_scale */
    int16_t dpal_frame;          /* which frame of FPackMObjSub.dpal_count
                                 * this MObj's bank last had rewritten
                                 * into it; calloc's 0
                                 * matches the bank's own load-time
                                 * content, which IS frame 0, so the
                                 * first real palette_id of 0 correctly
                                 * finds nothing to do */
} DCMObjColor;

typedef struct
{
    uint32_t mobj_count;        /* MObjSubs in the pack, all joints */
    uint32_t off_subs;          /* FPackMObjSub[mobj_count] */
    uint32_t off_joint;         /* joint_count pairs of int16: the joint's
                                 * first MObj and how many it has */
    uint32_t off_batch;         /* batch_count int16: which MObj drew the
                                 * batch, -1 where none did */
    uint32_t off_entry;         /* alt_count * mobj_count int32: the MObj's
                                 * MatAnimJoint script as a word index, -1
                                 * for none, alternate by alternate */
    uint32_t off_words, words;  /* the scripts themselves, host order */
    uint32_t off_reloc, reloc_count;    /* word indices holding a pointer,
                                         * as FPackAnim's do */
    uint32_t alt_count;         /* how many whole MatAnimJoints the pack
                                 * carries, one after another in off_entry.
                                 * 1 for every pack but the dead
                                 * explosion's, which has one per player:
                                 * the game swaps the EFDesc's
                                 * o_matanim_joint before it makes the
                                 * effect (ef/efmanager.c:4808) and the
                                 * port picks the alternate instead
                                 * (src/dc/objmodel.c
                                 * dc_model_add_mobjs_alt) */
    uint32_t off_dpal;           /* FPackDPal[dpal_count] -- raw dynamic
                                 * palette frames, tools/lib/ssb_assets.py
                                 * read_palette's own 16-entry ARGB1555
                                 * shape, indexed by an animating
                                 * FPackMObjSub's own dpal_first/
                                 * dpal_count */
    uint32_t dpal_count;
} FPackMObjs;

/* One dynamic-palette frame: 16 entries, PVR_PAL_ARGB1555, host order --
 * the same layout Fighter.pals already carries, just rewritable at
 * runtime instead of written once at load. */
typedef struct
{
    uint16_t c[16];
} FPackDPal;

/* The physics half of FTAttributes (fttypes.h), read off the ROM at pack
 * time. Field names keep the decomp's spelling. */
typedef struct
{
    float size;
    float walkslow_anim_length, walkmiddle_anim_length;
    float walkfast_anim_length, rebound_anim_length;
    float walk_speed_mul, traction, dash_speed, dash_decel, run_speed;
    float kneebend_anim_length;
    float jump_vel_x, jump_height_mul, jump_height_base;
    float jumpaerial_vel_x, jumpaerial_height;
    float air_accel, air_speed_max_x, air_friction;
    float gravity, tvel_base, tvel_fast;
    float weight, attack1_followup_frames, dash_to_run;
    float shield_size;          /* fttypes.h:901; the shield's YRotN scale
                                 * (ftCommonGuardUpdateShieldCollision) */
    float shield_break_vel_y;   /* fttypes.h:902; the shield break's upward
                                 * launch (ftCommonShieldBreakFlySetStatus, -- 70 for Mario */
    float shadow_size;          /* fttypes.h:903; the half-width of the
                                 * blob shadow (ftShadowProcDisplay, -- 200 for all but
                                 * Donkey Kong, whose is 350 */
    /* fttypes.h:904-910: the jostle box and the camera's per-fighter
     * numbers (gm/gmcamera.c) */
    float jostle_width, jostle_x;
    float cam_offset_y, closeup_camera_zoom, camera_zoom, camera_zoom_base;
    /* MPObjectColl map_coll (fttypes.h:911), the collision diamond the
     * mp walk carries: top, center, bottom, width */
    float map_coll_top, map_coll_center, map_coll_bottom, map_coll_width;
    /* Vec2f cliffcatch_coll (fttypes.h:912), the ledge-grab probe */
    float cliffcatch_coll_x, cliffcatch_coll_y;
    /* fttypes.h:921, the rebirth halo's scale */
    float halo_size;
    /* fttypes.h:876-878, the cargo walk's animation lengths
     * (ftDonkeyThrowFWalk) */
    float throw_walkslow_anim_length, throw_walkmiddle_anim_length;
    float throw_walkfast_anim_length;
    int32_t jumps_max;
    /* fttypes.h:950 s32 effect_joint_ids[5]: the five joints
     * ftParamGetEffectJointPosition cycles through, one per call, so a
     * fighter on fire spreads the flames over his body rather than
     * trailing them from one bone. Every arm of ftParamMakeEffect that
     * opens with that call reads it. */
    int32_t effect_joint_ids[5];
    /* fttypes.h:951 sb32 cliff_status_ga[5]: whether each cliff state is
     * grounded or airborne, indexed by ftCommonCliffStatusKind
     * (ftcommoncliffclimb.c:237). That enum has six members, so the game
     * reads one word past the array for EscapeSlow; the sixth entry here
     * is that word (fttypes.h:952 unused_0x2CC), so the port reads the
     * same value the N64 does instead of running off its own array. */
    int32_t cliff_status_ga[6];
    /* The hit-detection half. fttypes.h:904 is_metallic;
     * :924-945 the twenty-two is_have_* bits as the ROM's one word, MSB
     * first (is_have_attack11 is bit 31 -- ftManagerSetupAttributes
     * unpacks them into the bit-fields by name, since this ABI packs
     * them the other way); :947 hit_detect_range; :949 the two animlock
     * words the pointer led to; :946 the eleven hurtbox descriptions,
     * joint_id -1 ending the list. */
    int32_t is_metallic;
    uint32_t is_have;
    float hit_detect_range[3];
    uint32_t animlock[2];
    struct
    {
        int32_t joint_id, placement, is_grabbable;
        float offset[3], size[3];
    } damage_coll_descs[11];
    /* The joint tables. fttypes.h:948
     * setup_parts: the two words lbCommonSetupFighterPartsDObjs tests MSB
     * first, one bit per DObjDesc entry, to decide which entries become
     * DObjs at ftManagerMakeFighter -- a clear bit's entry still takes
     * its slot of FTStruct.joints (NULL) and its pack joint (no DObj).
     * fttypes.h:953 hiddenparts: FTHiddenPart rows verbatim,
     * {root_joint_id, parent_joint_id, partindex_0x8, joint_kind}, one
     * per FTAnimDesc bit from 31 down (ft/ftmain.c:4636-4652): the DObj
     * ftMainUpdateHiddenPartID makes for the root and links under the
     * parent while an animation asks for it. Rows 0..2 are XRotN, TransN
     * and YRotN on every fighter; the rest are a marker in front of the
     * hip, Kirby's copy hats, Samus's arm cannon, Link's and Yoshi's
     * extras. ftManagerSetupAttributes points FTAttributes at both.
     * off_slots: pack offset of anim_count FPackAnimSlots, each
     * animation's figatree table raw (below). */
    uint32_t setup_parts[2];
    int32_t hiddenpart_count;
    int32_t hiddenparts[FPACK_HIDDENPART_MAX][4];
    /* fttypes.h:969 FTAttributes.thrown_status: a pointer in the game,
     * into the fighter's own 27-row table (one row per nFTKind*,
     * forward-throw then back-throw: FTThrownStatus{status1,status2}) --
     * what status a completed grab's release puts the CAUGHT fighter
     * into, indexed by the caught fighter's own fkind
     * (ft/ftcommon/ftcommonthrow.c ftCommonThrowSetStatus). Every roster
     * file is the identical 432-byte shape (dNameMain_thrown_status[54]
     * FTThrownStatus rows, read in pairs), so the pack carries it inline
     * like hiddenparts rather than by offset; ftManagerSetupAttributes
     * points FTAttributes at it. Left NULL,
     * ftCommonThrowSetStatus's dereference of it would land near address
     * zero, read back garbage, and abort on the resulting
     * nonsense status id. */
    int32_t thrown_status[FPACK_THROWN_STATUS_NUM][2][2];
    /* The item half fttypes.h:917-920 and
     * :968/:970. item_pickup is FTItemPickup flattened -- offset then
     * range, light then heavy, Vec2f each: the rectangle
     * ftCommonGetFindItem needs an item to be inside before A picks it
     * up, centred on the fighter's position plus lr*offset and widened by
     * the item's own collision diamond. itemthrow_* are per-fighter
     * percentages ftCommonItemThrowProcUpdate multiplies into a thrown
     * item's speed and damage; heavyget_sfx is the grunt
     * itMainSetFighterHold plays lifting a heavy one
     * (nSYAudioFGMVoiceEnd = none), and the seven after it are the voice
     * half, fttypes.h:913-916 : the two KO sounds, the
     * star-KO scream, the hurt voice and the three smash-attack voices.
     * Left uncarried, FTAttributes would read 0 for all seven --
     * nSYAudioFGMExplodeS -- and every one of those voices would be a small
     * explosion. joint_item*_id say which joint the
     * held item hangs off -- itMainSetFighterHold points the item's new
     * parent DObj at fp->joints[id] and itMainSetFighterRelease reads the
     * same joint's world position to place the item on release.
     *
     * src/dc/ftcomputer.c has a DIVERGES paragraph on the item family. */
    float item_pickup[8];
    uint16_t itemthrow_vel_scale, itemthrow_damage_scale;
    uint16_t heavyget_sfx;
    uint16_t dead_fgm_ids[2], deadup_sfx, damage_sfx, smash_sfx[3];
    int32_t joint_itemheavy_id, joint_itemlight_id;
    /* fttypes.h:957-963, the slope contour: each foot
     * joint and its leg's lower bone length, then unk_0x31C (how far
     * below its parent a foot may rise) and unk_0x320 (the steepest
     * angle it may drop below the root), all read by
     * mpCommonUpdateFighterSlopeContour (src/dc/mpcommon.c). */
    int32_t joint_rfoot_id;
    float joint_rfoot_rotate;
    int32_t joint_lfoot_id;
    float joint_lfoot_rotate;
    float unk_0x31C, unk_0x320;
    /* fttypes.h:964 translate_scales: a per-joint scale on
     * the translation tracks, so Mario's animations fit Luigi's taller
     * limbs. Only Luigi has one (relocData/221_LuigiMain.c:194); the
     * rest carry is_have_translate_scales 0 and identity rows.
     * ftManagerSetupAttributes points FTAttributes.translate_scales here
     * or leaves it NULL. One row per joint of the game's joints array. */
    int32_t is_have_translate_scales;
    float translate_scales[FPACK_JOINT_NUM_MAX][3];
    /* fttypes.h:922-923 SYColorRGBA shade_color[3] and fog_color, as
     * RGBA bytes: the team shade and the colour
     * animation's tint scale ft/ftdisplaymain.c's fog functions read */
    uint8_t shade_color[12];
    uint8_t fog_color[4];
    uint32_t off_slots;
    uint32_t off_shield;        /* FPackShieldPose */
    uint32_t off_parts;         /* FPackParts */
    uint32_t off_costumes;      /* FPackCostumes */
    uint32_t off_texparts;      /* FPackTexParts, 0 for a fighter without
                                 * a textureparts_container */
    /* fttypes.h:967 textureparts_container, the game's six bytes
     * (FTTexturePart[2]), which ftManagerSetupAttributes points the
     * attribute at when is_have_textureparts says the fighter has one */
    uint8_t textureparts[6];
    uint8_t is_have_textureparts, pad_textureparts;
    /* fttypes.h:966 accesspart: an FTAccessPart with the ROM's joint_id
     * and NULL pointers, 32 bytes so the struct fits at the host's
     * pointer width as well; ftManagerSetupAttributes points the
     * attribute here when is_have_accesspart. The accessory itself is a
     * part of the pack (FPackParts.accessory_tag). */
    uint8_t accesspart[32];
    uint32_t is_have_accesspart;
    /* fttypes.h:972 skeleton: FTSkeleton *[3] in the game,
     * word 0 a joint id ftDisplayMainDrawAll checks and words 1 and 2 one
     * table per skeleton id. The tables are the pack's skeleton tags
     * (FPackPartTag.skeleton); this is the joint and a mask of the ids
     * the fighter has (bit n for id n), 0 for no skeleton.
     * ftManagerSetupAttributes builds the attribute's three words. */
    int32_t skeleton_joint;
    uint32_t skeleton_ids;
    /* sc/scsubsys/scsubsysdata<f>.c dFT<F>SubMotionDescs:
     * FPackMotion[submotion_count], the table a DEMO status indexes.
     * ftMainSetStatus sorts a status id into four bands and the two
     * above FTSTAT_OPENING2_START read an FTOpeningDesc whose motion id
     * counts here rather than in off_motion's table -- the character
     * select's selected pose, the results screen's win and lose poses,
     * the 1P stage cards' two sides, and every opening movie's trophies,
     * runners, clashes and slides.     *
     * It is here and not in FPackHeader because the header is exactly
     * 128 bytes with nothing spare and eleven exporters write it, while
     * only a fighter has a SubMotion table -- the same reason off_shield
     * and off_parts are here. Every row's `script` is 0: CUT, see
     * ssb_packexport.py fighter_submotion_table. */
    uint32_t off_submotion, submotion_count;
    /* The animations' words, which are not in the pack but in <name>.anm
     * beside it (FPackAnm): the id both files carry and the words'
     * length. 0/0 for a pack whose FPackAnim.off_words count from the
     * pack's own start, as every pack without attributes does. */
    uint32_t anm_id, anm_size;
    /* The .anm in tiers (tools/export/anm_tiers.tsv): the words are laid
     * out tier by tier, and tier t's animations are all within the
     * first anm_tier_end[t] bytes of them, so a scene that plays only
     * those reads that much of the file (fighter_load_tier). Tier 0 is
     * what the character selects, the results screen and the 1P card
     * play; tier 1 adds the Characters screen's. */
    uint32_t anm_tier_end[FPACK_ANM_TIERS];
} FPackAttr;

/* <name>.anm, a fighter's animation words (tools/export/ssb_packexport.py):
 * this header, then every FPackAnim's words, each 4-aligned, tier by tier
 * (FPackAttr.anm_tier_end). A fighter's
 * FPackAnim.off_words count from the start of this file rather than the
 * pack's, so a scene can hold a fighter's model without its 300-500 KB
 * of animations. `id` is a CRC of the words, and FPackAttr.anm_id the same
 * one: a pack and an .anm from two exports do not load together. */
#define FPACK_ANM_MAGIC "SSBANIM1"
typedef struct
{
    char magic[8];
    uint32_t id;
    uint32_t size;              /* of the words after this header */
} FPackAnm;

/* The texture parts: FTAttributes.textureparts_container,
 * the faces. ftParamSetTexturePartID sets the texture_id_curr of the MObj
 * a part names, which picks the sprite gcDrawMObjForDObj loads -- open
 * eyes, closed, a wince. The pack has no MObjs, so the exporter bakes a
 * tile per frame beside each batch drawn under that MObj, for every
 * frame a motion script sets, per costume; Fighter.texpart_frame picks.
 *
 * The container itself is FPackAttr.textureparts. frame_count is per part,
 * 0 for one no script sets. A batch's FPackTexPartBatch names its part
 * (0xFF for none) and its first entry in the frame table, which runs
 * costume_count * frame_count long, costume-major; an entry is a texture
 * index, -1 where the game's sprite array has a hole there. */
typedef struct
{
    uint8_t part, pad;
    uint16_t frame_first;
} FPackTexPartBatch;

typedef struct
{
    uint8_t frame_count[2];
    uint16_t pad;
    uint32_t frame_total;
    uint32_t off_batches;       /* FPackTexPartBatch[batch_count] */
    uint32_t off_frames;        /* int16_t[frame_total] */
} FPackTexParts;

/* The costumes. The game plays each MObj's costume script
 * to the frame FTStruct.costume names when it adds the MObj
 * (lbCommonAddMObjForFighterPartsDObj), which moves the MObj's colours
 * and palette id. Both are baked here: the exporter bakes the model once
 * per costume the game can deal (dFTParamCostumeIDs), checks that only
 * the batches' textures and colours differ, and carries those per
 * costume per batch. The texture table holds every costume's; costume 0
 * is the batches' own. Fighter.costume picks the row
 * (fighter_set_costume). */
typedef struct
{
    int16_t tex;
    uint16_t pad;
    uint32_t prim, light1, light2, env;
} FPackCostumeMat;

typedef struct
{
    uint32_t costume_count;
    uint32_t off_mats;          /* FPackCostumeMat[costume_count *
                                 * batch_count], costume-major */
} FPackCostumes;

/* The model parts: FTAttributes.modelparts_container
 * (ft/fttypes.h:129-159), the display lists ftParamSetModelPartID swaps
 * onto a joint -- Mario's open and closed fists, Link's shield hand and
 * sheathed sword, Kirby's fifteen copy hats.
 *
 * The pack has no display lists to swap, so the exporter bakes every part
 * a joint can wear as batches of its own and tags them (FPackBatch.part,
 * 1..tag_count). A joint then draws its untagged batches and those of the
 * one part it has on, which the display walk records per joint from the
 * DObj's payload (Fighter.part_cur, src/dc/objmodel.c). A tag's vertices
 * are one run the transform skips while the part is off
 * (FPackPartTag); Kirby's hats are 1700 of his 1860.
 *
 * FPackPartJoint, one per pack joint: the part ids the joint has, as a
 * run of FPackPartID (tag and FTModelPart.flags per id, tag
 * FPACK_PART_NONE for a part with no display list), and `tree`, the tag
 * of the tree entry's own display list -- 0 for a joint with no parts,
 * FPACK_PART_NONE where the entry has none, so the joint starts with no
 * part on (ftmanager.c's modelpart_id_base -1). tools/export/ssb_packexport.py
 * writes it; the ids are the high-detail row of each FTModelPart pair,
 * the tree the pack is baked from. */
#define FPACK_PART_NONE 0xFE

/* ft/ftdef.h FTPARTS_FLAG_NOFOG, the one part flag the pack's draw reads
 * (Fighter.part_flags). Ness's entry silhouette is the only VS part that
 * sets it; FTPARTS_FLAG_TOGGLEFOG is set by no fighter's data. */
#define FIGHTER_PART_NOFOG 0x40

typedef struct
{
    uint16_t vert_first, vert_count;    /* tags run in vertex order */
    uint8_t joint;
    /* The electric skeleton id this tag draws for, 0 for
     * a model part or the accessory. A skeleton tag is one joint's
     * FTSkeleton display lists, drawn in place of every other batch while
     * Fighter.skeleton_id names its id. */
    uint8_t skeleton;
    uint8_t pad[2];
} FPackPartTag;

typedef struct
{
    uint16_t id_first;
    uint8_t id_count;
    uint8_t tree;
} FPackPartJoint;

typedef struct
{
    uint8_t tag, flags;
} FPackPartID;

typedef struct
{
    uint32_t tag_count, id_count;
    uint32_t off_tags;          /* FPackPartTag[tag_count], tag 1 first */
    uint32_t off_joints;        /* FPackPartJoint[joint_count] */
    uint32_t off_ids;           /* FPackPartID[id_count] */
    /* The costume accessory (Pikachu's hat, Jigglypuff's bow,
     * FTAttributes.accesspart) as one more tag, 0 for none. Its batches
     * are drawn beside the joint's own while Fighter.accessory_on --
     * which ftDisplayMainDrawAccessory sets for a fighter whose joint
     * carries the accessory GObj, costume 0 never does -- not in place
     * of them. */
    uint32_t accessory_tag;
} FPackParts;

/* The shield pose: FTAttributes.dobj_lookup and
 * shield_anim_joints[8] (ft/fttypes.h:955-956) out of the fighter's
 * ShieldPose file, the two tables ft/ftcommon/ftcommonguard1.c reads
 * and the whole of what that file holds (tools/lib/ssb_meshexport.py
 * read_shieldpose). The file's words are carried whole, host order:
 * `lookup` is the word index of `lookup_count` DObjDesc rows (eleven
 * words each: id, dl zeroed, translate, rotate, scale -- FPackDObjDesc,
 * the game's struct); `table[k]` the word index of direction k's
 * AObjEvent32 *[table_count] dispatch table, each entry a word index
 * into the same words or 0 for NULL, which the loader makes pointers
 * (Fighter.shield_joints). The scripts' own pointer words, if any, are
 * listed at off_reloc as an AnimJoint animation's are and made
 * addresses the same way; no fighter's has one. */
typedef struct
{
    uint32_t off_words, nwords;
    uint32_t off_reloc, nreloc;
    uint32_t lookup, lookup_count;
    uint32_t table[8];
    uint32_t table_count;
} FPackShieldPose;

/* sys/objtypes.h:376 DObjDesc, member for member, so the rows the loader
 * builds are the game's type on either ABI; ftmanager.c casts. */
typedef struct
{
    int32_t id;
    void *dl;
    float translate[3], rotate[3], scale[3];
} FPackDObjDesc;

/* One animation's figatree table as the file has it: `count` int32_t word
 * indices into the animation's words (FPackAnim.off_words, in its word
 * size) at pack offset `off`, -1 where the table's pointer is NULL.
 * lbCommonAddFighterPartsFigatree (lb/lbcommon.c:806) deals slot k to
 * the k-th DObj of the tree walk from TopN's child, hidden parts
 * included, so `count` runs past the skeleton for an animation that
 * links one (22 to 33 across the roster) and the table cannot be
 * indexed by pack joint. FPackAnim.off_entries is this same table dealt
 * to the skeleton alone -- slot k to the k-th instantiated entry --
 * which is right while no hidden part is linked and is what the pose
 * player reads. */
typedef struct
{
    uint32_t off;
    uint32_t count;
} FPackAnimSlots;

/* One dFT<F>MotionDescs row: which pack animation a motion id plays.
 * The flags keep the decomp's FTANIM_FLAG_* encoding verbatim. */
typedef struct
{
    int16_t anim;               /* index into the pack's animation bank */
    int16_t pad;
    uint32_t flags;             /* raw FTANIM_FLAG_* word */
    uint32_t script;            /* pack offset of the row's ftMotionCommand
                                 * script (FTMotionDesc.offset laid onto the
                                 * file), 0 when the row has none */
} FPackMotion;

typedef struct
{
    int32_t parent;             /* -1 at the root */
    float t[3], r[3], s[3];     /* bind translate, rotate (rpy), scale */
} FPackJoint;

typedef struct
{
    float x, y, z;
    float nx, ny, nz;           /* unit, joint-local */
    float u, v;
    uint8_t alpha, joint;
    uint16_t pad;
} FPackVtx;

/* FPackBatch.bucket: bits[1:0] pick the PVR list, bit2 turns the z-test
 * off. The exporter reads the list out of the RDP render mode the batch's
 * triangles were emitted under (tools/lib/ssb_assets.py rendermode_list):
 * FORCE_BL is a real blend and goes translucent; CVG_X_ALPHA without it is
 * the RDP's alpha cutout, whose analogue here is punch-through -- alpha
 * tested, z-written, no per-pixel sort, which is what a hard-edged texture
 * wants and what blending gets wrong; anything else is opaque. */
#define FPACK_LIST_OP  0
#define FPACK_LIST_PT  1
#define FPACK_LIST_TR  2
#define FPACK_LIST_MASK 3
#define FPACK_NOZ      4
/* The RDP's texture filter, out of other-mode H's G_MDSFT_TEXTFILT field.
 * sys/rdp.c's sSYRdpResetDisplayList -- the display list every N64 frame
 * starts with -- programs that word whole and puts G_TF_BILERP in it, so
 * bilinear is the game's standing state and point sampling is what a
 * display list has to ask for. Across every pack this port bakes, none
 * does: the bit is here so that the exporter's answer, and not the
 * renderer's default, is what the PVR is told. G_TF_AVERAGE, the RDP's
 * box filter, is filtering too and does not set it -- only G_TF_POINT. */
#define FPACK_FILT_POINT 8
/* The batch's colour combiner is lerp(ENV, PRIM, TEXEL0) --
 * (PRIM - ENV) * TEXEL0 + ENV, the texel's intensity choosing between two
 * flat colours rather than tinting one -- and FPackBatch.env holds the ENV
 * it lerps from, 0x00RRGGBB. The PVR draws it exactly: base colour
 * PRIM - ENV modulated by the texel, ENV as the offset colour, which is
 * what src/dc/lbcommon.c has done for the sprite renderer's
 * nLBCommonCombineIAPrimEnv Without the bit the batch reads no
 * ENV at all, which is every batch this port bakes but a dead
 * explosion's. */
#define FPACK_ENVLERP  16

/* The batch's colour combiner is lerp(TEXEL0, TEXEL1, PRIM_LOD_FRAC) --
 * (TEXEL1 - TEXEL0) * PRIM_LOD_FRAC + TEXEL0, two tiles cross-faded by a
 * factor the RDP takes from the primitive colour's LOD byte, which
 * objdisplay.c:1243 writes as `mobj->lfrac * 255`. Dream Land's clouds are
 * the only thing in the game that asks for it, and they are why the port
 * has it: the two tiles are the same cloud a moment apart, one of them
 * scrolling, and without the fade the background is a still picture.
 *
 * The PVR samples one texture per polygon, so the port draws the batch
 * TWICE -- tile A, then tile B over it at alpha lfrac, which composites to
 * A*(1 - lfrac) + B*lfrac. The N64's alpha for this combiner is TEXEL1's
 * alone (times the primitive's); the two-pass form uses each tile's own,
 * which differs where the two frames' alpha differs and does not where
 * they agree. On the one model that uses it they agree closely -- it is
 * the same cloud -- and the alternative is no cross-fade at all. */
#define FPACK_TEXLERP  32

/* The N64 geometry mode's G_CULL_BACK/G_CULL_FRONT bits at the moment
 * this batch's triangles were emitted (tools/lib/ssb_assets.py's DL replay,
 * default cull_back=1 -- sys/rdp.c's sSYRdpResetDisplayList clears
 * G_CULL_BOTH then sets G_CULL_BACK every frame, so a display list that
 * never mentions culling is still asking for it). compile_batch
 * (fighter.c) reads these to choose the PVR's cull mode instead of
 * always drawing both sides. */
#define FPACK_CULL_BACK  64
#define FPACK_CULL_FRONT 128

/* The geometry mode's G_TEXTURE_GEN bit: the RSP makes this batch's
 * texture coordinates out of each vertex's NORMAL, against the two
 * gSPLookAt vectors (gm/gmcamera.c:1021-1024 sends the camera's own
 * right and up, syMatrixLookAtReflectF), rather than reading the pair in
 * the Vtx. A sphere map, and in this game Metal Mario is all of it:
 * fourteen drawn joints, one chrome texture, every triangle. The UVs
 * baked into the pack mean nothing for such a batch -- draw_batch
 * recomputes them per vertex from Fighter.texgen. */
#define FPACK_TEXGEN     256

/* The batch steps its OWN run of pictures under its MObj: frame k is
 * texture FPackBatch.tex + k, not FPackMObjSub.tex_first + k. A MObj
 * normally has one picture however many batches it covers, so its
 * run is theirs. Final Destination's boss wall (Effects2_1) is the
 * exception: four joints, one MObj apiece, each drawing five stacked
 * slices of one image under the MObj's one palette_id, so each slice
 * needs the palette frames over ITS texels. Without this every slice
 * drew the first slice. */
#define FPACK_OWNRUN     512
/* The batch was drawn with the RDP's Z compare off (tools/lib/
 * ssb_assets.py FPACK_ZALWAYS, baked for the weapons, which
 * wpDisplayDrawNormal draws Z-less): it is compiled PVR_DEPTHCMP_ALWAYS
 * with the depth write off, in whichever list it is in, so what the
 * frame drew before it does not hide it, and what it draws after still
 * covers it -- the RDP's painter's order. The translucent list's
 * per-pixel sort still orders it among the other translucent polygons;
 * the opaque and punch-through lists take polygons in submission order,
 * which is the game's. Master Hand's finger bullet leaving his fingertip
 * was hidden inside the finger for its first frames without it, and a
 * fireball by a platform's edge. */
#define FPACK_ZALWAYS    1024

/* FPackBatch.alpha_src: which sources the RDP's *alpha* combiner cycle
 * reads. It is programmed separately from the colour cycle and routinely
 * disagrees with it -- across the seven exported stages 37 display lists
 * compute alpha as TEXEL0 * SHADE, 23 as TEXEL0 alone, 5 as SHADE alone --
 * so `shaded`, which is the colour cycle's answer, cannot speak for alpha
 * too. Reading a vertex alpha the RDP never reads makes geometry vanish the
 * moment it leaves the opaque list, where alpha is ignored: Kongo Jungle's
 * railings carry a vertex alpha of 0 and 64 in display lists whose alpha is
 * TEXEL0 alone. */
#define FPACK_ALPHA_TEX   1     /* the alpha cycle reads the texel */
#define FPACK_ALPHA_SHADE 2     /* the alpha cycle reads the vertex */
/* PRIM is two registers to the combiner: the colour cycle reads its RGB,
 * the alpha cycle its A, and a display list routinely programs one and
 * not the other -- the opening Room's spotlight shades with SHADE alone
 * and fades with TEXEL0 * PRIMITIVE. `prim` below is baked with each
 * half white where its cycle does not read it (tools/lib/ssb_assets.py
 * MeshBaker._material); these two say which half a LIVE prim -- a
 * costume's, a display proc's, an animated MObj's -- may replace, as the
 * RDP would have read it. */
#define FPACK_ALPHA_PRIM  4     /* the alpha cycle reads PRIM's alpha */
#define FPACK_COLOR_PRIM  8     /* the colour cycle reads PRIM's RGB */

typedef struct
{
    uint16_t tri_first, tri_count;
    int16_t tex;                /* index into textures, -1 = none */
    uint8_t shaded, joint;      /* shaded: 0 flat, 1 N.L, 2 vertex colour */
    uint16_t bucket;            /* FPACK_LIST_* | FPACK_NOZ |
                                 * FPACK_FILT_POINT | FPACK_CULL_BACK |
                                 * FPACK_CULL_FRONT */
    uint8_t alpha_src;          /* FPACK_ALPHA_* */
    uint8_t part;               /* the model part the batch draws, 0 for
                                 * none: see FPackParts */
    uint32_t prim, light1, light2;
    uint32_t env;               /* 0x00RRGGBB, and 0 unless the bucket
                                 * says FPACK_ENVLERP */
} FPackBatch;

/* The PVR's palette RAM is 1024 entries, which is 64 banks of 16 for the
 * 4bpp formats. Every pack resident at once shares them (fighter.c). */
#define FPACK_PAL_BANKS 64

#ifdef DB_PVR_BUDGET
/* src/dc/fighter.c: the frame loop's TA-budget probe. DB_PVR_OPB_POOL is
 * how many 16-word object pointer blocks dc_pvr_init's three overflow
 * sets hold for the whole frame, which is what a dense tile draws on
 * once its own one block is full. */
#define DB_PVR_TW 20
#define DB_PVR_TH 15
#define DB_PVR_OPB_POOL (DB_PVR_TW * DB_PVR_TH * 3 * 3)
void db_pvr_budget_frame(unsigned *worst_tile, unsigned *worst_bytes,
                         unsigned *tris, unsigned *pool);
#endif

/* FPackTex.fmt: how the exporter encoded the baked texels. A CI4 tile stays
 * paletted, which is a quarter of the VRAM and what every fighter model is;
 * the other eight N64 formats are resolved to direct colour at build time
 * (tools/lib/ssb_assets.py pvr_encoding), ARGB1555 where the alpha is already
 * on/off and ARGB4444 where it is graded. */
#define FPACK_TEX_PAL4     0
#define FPACK_TEX_ARGB1555 1
#define FPACK_TEX_ARGB4444 2
/* The pack carries no texels for this one: the runtime supplies both the
 * PVR texture and its format through fighter_set_extern_texture below.
 * `off`/`size` are 0 and `w`/`h` are the *picture's* size, which is what
 * the exporter normalised the UVs against. Only the transition wipes use
 * it (tools/export/ssb_transexport.py): their display lists texture from
 * segment 1, which lb/lbtransition.c fills with a copy of the last frame
 * drawn. */
#define FPACK_TEX_EXTERN   3

typedef struct
{
    uint32_t off, size;         /* into the texdata section */
    uint16_t w, h;
    uint8_t pal, clamp;         /* clamp: PVR_UVCLAMP_* */
    uint8_t fmt;                /* FPACK_TEX_* */
    uint8_t pad;
} FPackTex;

/* FPackAnim.kind: which of the game's two joint-script languages the
 * animation is written in. A figatree is the AObjEvent16 language only
 * fighters use (ft/ftanim.c ftAnimParseDObjFigatree); an AnimJoint
 * animation is the AObjEvent32 language everything else in the game
 * animates with (sys/objanim.c gcParseDObjAnimJoint). FTMotionDesc's
 * FTANIM_FLAG_ANIMJOINT says which a motion plays (ft/fttypes.h:59), and
 * the exporter carries that here so the file is self-describing: every
 * Appear is an AnimJoint animation, and so are a handful of others. */
#define FPACK_ANIM_FIGATREE 0
#define FPACK_ANIM_ANIMJOINT 1

typedef struct
{
    char name[FPACK_ANIM_NAME_LEN];
    uint32_t off_words;         /* from pack start; the whole animation
                                 * file as native words: uint16_t for a
                                 * figatree, uint32_t (4-aligned) for an
                                 * AnimJoint animation */
    uint32_t nwords;            /* in that word size */
    uint32_t off_entries;       /* from pack start; int32_t per joint, a
                                 * word index or -1 */
    uint32_t kind;              /* FPACK_ANIM_* */
    uint32_t off_reloc;         /* from pack start; nreloc uint32_t word
                                 * indices of the AnimJoint words that hold
                                 * a pointer (Jump, SetAnim, SetInterp
                                 * targets), or of a figatree's spline-table
                                 * pointers (the low word of each u16
                                 * pair). Each holds its target's word
                                 * index until fighter_init makes it an
                                 * address. */
    uint32_t nreloc;
} FPackAnim;

/* ---- runtime instance ---- */

#define FIGHTER_MAX_JOINTS 40

typedef struct Fighter
{
    void *blob;                 /* the whole pack */
    uint32_t blob_size;
    int blob_owned;             /* free() it in fighter_release: true when
                                 * fighter_load had to read the file rather
                                 * than map it, false for a mapped romdisk
                                 * pack or a blob the caller owns */
    const FPackHeader *hd;
    const FPackJoint *joints;
    const FPackVtx *verts;
    const uint16_t (*tris)[3];
    const FPackBatch *batches;
    const FPackTex *texs;
    const uint16_t (*pals)[16];
    const FPackAnim *anims;
    /* The words FPackAnim.off_words counts into when the pack's are in
     * an .anm (FPackAttr.anm_size != 0): that file as read, NULL until
     * fighter_attach_anm. fighter_anim_words is how to reach them. */
    void *anm;
    uint32_t anm_size;          /* bytes of it held: the whole file, or a
                                 * tier's prefix (fighter_load_tier) */
    int anm_owned;              /* free() it in fighter_release */
    void *anm_retired;          /* the prefix a miss replaced, which a
                                 * playing clone may still read: freed
                                 * in fighter_release */
    char anm_name[32];          /* where it came from, for the rest */
    /* A clone's pack (fighter_clone), NULL for a pack: the .anm is the
     * pack's, and one that grows mid-scene grows for every clone. */
    struct Fighter *anm_src;
    const FPackAttr *attr;      /* NULL for stage packs */
    const FPackAnimSlots *slots; /* anim_count of them; NULL without attr */
    const FPackShieldPose *shield;  /* NULL without attr */
    void **shield_joints[8];    /* AObjEvent32 *[table_count] per stick
                                 * sector, built from the pack's tables:
                                 * FTAttributes.shield_anim_joints */
    FPackDObjDesc *shield_lookup;   /* lookup_count rows:
                                 * FTAttributes.dobj_lookup */
    const FPackMotion *motions;
    uint32_t motion_count;
    /* dFT<F>SubMotionDescs (FPackAttr.off_submotion), the
     * table a demo status indexes. NULL/0 for a pack with no attributes
     * or none written. */
    const FPackMotion *submotions;
    uint32_t submotion_count;

    /* The animated-material half, NULL/0 for a pack with no MObjs. */
    const FPackMObjs *mobjs;
    const FPackMObjSub *mobj_subs;
    const int16_t *mobj_joint;  /* 2 per joint: first, count */
    const int16_t *mobj_batch;  /* 1 per batch */
    const FPackDPal *dpals;     /* mobjs->dpal_count of them, or NULL --
                                 * FPackMObjSub.dpal_first/dpal_count/
                                 * dpal_bank index into this */
    /* What the last display walk left here for each MObj: the colours the
     * object system's parser has the MObj carrying now. draw_batches
     * reads them through FPackBatch.mobj instead of the batch's own baked
     * ones. NULL where the pack has no MObjs. */
    struct DCMObjColor *mobj_color;

    /* The costumes (FPackCostumes), NULL for a pack without them, and
     * the one this instance wears. hdr and hdr_tr then hold a header per
     * costume per batch, costume-major. */
    const FPackCostumes *costumes;
    const FPackCostumeMat *costume_mats;
    int costume;

    /* The texture parts (FPackTexParts), NULL for a pack without them;
     * the frame each part is on, and a header pair per frame table entry
     * ([entry * 2 + pass], pass 1 the translucent list). */
    const FPackTexParts *texparts;
    const FPackTexPartBatch *texpart_batches;
    const int16_t *texpart_frames;
    uint8_t texpart_frame[2];
    pvr_poly_hdr_t *hdr_frames;

    /* The model parts (FPackParts), NULL/0 for a pack without them. */
    const FPackParts *parts;
    const FPackPartTag *part_tags;
    const FPackPartJoint *part_joints;
    const FPackPartID *part_ids;
    /* The part each joint has on, as a tag (FPackBatch.part): what the
     * last display walk found in the joint's DObj payload
     * (dc_joint_submit), and the tree's own until a walk has run. A
     * batch tagged with another part is not drawn, and its vertices not
     * transformed. */
    uint8_t part_cur[FIGHTER_MAX_JOINTS];
    /* Whether the accessory part draws this frame (FPackParts). */
    uint8_t accessory_on;
    /* The electric skeleton the fighter is drawn as this frame, 0 for its
     * own model (ft/ftdisplaymain.c ftDisplayMainDrawAll):
     * only the tags FPackPartTag.skeleton names for it draw. */
    uint8_t skeleton_id;
    /* Each joint's FTParts.flags as the last display found them
     * (ft/ftdisplaymain.c ftDisplayMainDecideFogDraw):
     * FIGHTER_PART_NOFOG draws the joint's batches out of the fog
     * (Fighter.fog_live), as its G_RM_PASS render mode does. */
    uint8_t part_flags[FIGHTER_MAX_JOINTS];

    /* THE LARGEST PACK DECIDES THIS, and for six milestones it was a
     * FIGHTER: Link's pack has 25 textures and 32 was "enough". Saffron
     * City's STAGE pack has 35 -- its layers draw more distinct images
     * than any character -- so `fighter_init`'s own
     * `hd->tex_count > sizeof(f->txr)/sizeof(f->txr[0])` refused the
     * stage outright, and the only symptom was "fighter: blob is not a
     * pack this build handles" with no stage named. Nothing else in the
     * build catches it: the pack is written by the exporter, and every
     * stage pack is loaded through this one call. Yamabuki was the only
     * stage over the line (the other eight are 17 to 26).
     *
     * 40 went to 64 when the layers' MatAnimJoints landed: an MObj that
     * flips its picture carries one baked tile per frame, as a contiguous
     * run (FPackMObjSub.tex_first), so Brinstar's ten animated MObjs took
     * it from 17 to 48 and Saffron City's two from 35 to 41. Brinstar is
     * the largest and 64 is the round number above it; tools/stgtex_check
     * .py refuses a stage pack that creeps past this, so the next one to
     * cross the line fails the build instead of the boot.
     *
     * 64 went to 128 when the fighters' costumes landed (FPackCostumes):
     * a pack carries every costume's palettes baked out, and Captain
     * Falcon's six come to 99.
     *
     * 128 went to 256 for Final Destination, which is by
     * some way the most animated background in the game: its one layer
     * hangs THIRTEEN MatAnimJoint MObjs off four joints, and thirteen
     * flipbooks come to 234 baked tiles and 242 textures in all --
     * five times Brinstar's. This time tools/check/stgtex_check.py named the
     * pack and the margin before the boot did, which is what it was
     * written for; the probe that would have said only "blob is not a
     * pack this build handles" ran first because the check is in
     * `./run.sh test` and not in `./run.sh build`. */
    pvr_ptr_t txr[256];
    pvr_poly_hdr_t *hdr;        /* one compiled header per batch (per
                                 * costume, FPackCostumes) */
    pvr_poly_hdr_t *hdr_tr;     /* the same for the translucent list, for
                                 * fighter_draw_layered */
    /* One pair of headers per frame of the sprite array, for a batch
     * whose MObj animates the picture; NULL for every other batch, which
     * is every batch of every pack but the slash's. The pair is
     * [frame * 2 + pass], pass 0 the batch's own list and pass 1 the
     * translucent one, exactly as hdr and hdr_tr are. */
    pvr_poly_hdr_t **hdr_mobj;
    /* An FPACK_ENVLERP batch's pair again, over a copy of its texture
     * with the RGB inverted: lerp(ENV, PRIM, T) is also
     * (ENV - PRIM) * (1 - T) + PRIM, which the PVR draws exactly when ENV
     * is the brighter in every channel -- the case the plain form clamps
     * to flat ENV (material_of). [batch * 2 + pass] as hdr_mobj's pairs;
     * has_inv says which batches have one. NULL when no batch does, and
     * always NULL for a batch with costumes, texture parts or a sprite
     * array, whose headers are chosen elsewhere. txr_inv is per
     * texture, NULL where none was made. */
    pvr_poly_hdr_t *hdr_inv;
    uint8_t *has_inv;
    pvr_ptr_t *txr_inv;
    int pal_base;               /* first PVR 16-colour palette bank */

    /* Joints the last display walk did not reach, one bit each: the
     * batches under them are skipped. gcDrawDObjTree refuses a DObj with
     * DOBJ_FLAG_HIDDEN and everything below it (src/dc/objdisplay.c), so
     * "not reached" is the walk's own answer to "hidden", and the mask is
     * set to all ones before a walk and cleared a bit at a time by
     * dc_joint_submit. It matters for exactly one model so far: the
     * off-screen arrows, whose whole animation is three scripts that set
     * and clear that flag to march the chevrons outward.
     * A model drawn without a walk leaves it zero and draws whole. */
    uint64_t joint_hide;
    /* Which PVR lists this model has a batch for, bit FPACK_LIST_*, and
     * whether that has been worked out (fighter_has_list). A batch
     * cross-fading two textures draws its second tile in the translucent
     * list whatever its own. */
    uint8_t list_mask, list_mask_ok;

    /* A primitive colour set from outside, over every batch's baked one,
     * while prim_live_on: the port of a gDPSetPrimColor the game issues
     * just before it draws a model whose display list reads PRIMITIVE
     * and sets none of its own. The magnifying glass's handle is the
     * case (if/ifcommon.c ifCommonPlayerMagnifyProcDisplay: the player's
     * colour on a triangle every player shares), and it is per instance
     * because each player's handle is a clone of the one pack. */
    uint32_t prim_live;
    int prim_live_on;

    /* The same for the environment colour: a gDPSetEnvColor issued
     * before the draw, over the batch's baked one, read only by a batch
     * whose combiner lerps ENV and PRIM by the texel (FPACK_ENVLERP).
     * The shield bubble is the case (ef/efmanager.c
     * efManagerShieldProcDisplay: PRIM white and ENV the player's colour
     * on a disc every player shares). Packed RGBA, as the
     * game's SYColorRGBA is. */
    uint32_t env_live;
    int env_live_on;

    /* The fog colour, packed RGBA: the port of the gDPSetFogColor
     * ft/ftdisplaymain.c issues before a fighter's draw under
     * G_RM_FOG_PRIM_A, which blends every pixel toward the colour by its
     * alpha -- the hit flash, the invincibility flash and the team shade
     * -- material_of folds it into the vertex and offset
     * colours; alpha 0 is no fog. */
    uint32_t fog_live;

    /* 0xFF less the alpha the whole fighter is drawn at
     * (sFTDisplayMainSkyFogAlpha, ft/ftdisplaymain.c:1180-1196, which
     * switches the render mode to G_RM_AA_ZB_XLU_SURF2 below 0xFF): the
     * results screen's podium fade-in. Kept as the cut so
     * a zeroed Fighter draws opaque. draw_batch scales every vertex's
     * alpha by it; only a translucent-list draw shows it. */
    uint32_t alpha_cut;

    /* pose + skeleton state, sized by the real joint count */
    float pose[FIGHTER_MAX_JOINTS][9];      /* tx ty tz rx ry rz sx sy sz */
    mtx4_t mtx[FIGHTER_MAX_JOINTS];         /* joint -> model */
    float light[FIGHTER_MAX_JOINTS][3];     /* light dir, joint space */
    /* The two gSPLookAt vectors in joint space, the same way `light` is
     * the light in joint space: [joint][0] is the camera's right and
     * [joint][1] its up, so a vertex's sphere-map coordinate is just two
     * dot products with its own normal. Written by fighter_frame only
     * when the pack has an FPACK_TEXGEN batch (Fighter.has_texgen);
     * Metal Mario is the only model in the game that does. */
    float texgen[FIGHTER_MAX_JOINTS][2][3];
    uint8_t has_texgen;
    /* fighter_set_extern_window's u0, v0, su, sv as the pack loaded;
     * read only for a batch on a FPACK_TEX_EXTERN texture */
    float ext_win[4];

    /* Every vertex in clip space, rebuilt by fighter_frame. A vertex's
     * clip position depends only on itself and its owning joint, never on
     * the batch, but draw walks triangles: Mario's 320 tris reference his
     * 233 verts 960 times, so transforming per corner did 4.12x the work.
     * Sized vert_count; fighter_frame must run before either draw. */
    float (*vclip)[4];
    /* The DObj payloads dc_model_add_dobjs made for this model, one per
     * pack joint whether or not the joint was built; NULL until a tree
     * is hung. dc_model_hidden_payload reads it. Every tree hung from
     * the same model shares them -- a payload is the model and a joint
     * index, nothing of the instance -- while disp_epoch still names the
     * scene heap they were cut from (syTaskmanGeneralHeapEpoch). */
    struct DCDisplay *disp;
    uint32_t disp_epoch;
} Fighter;

/* Pack joint k is DObjDesc entry k, which the game builds as
 * FTStruct.joints[nFTPartsJointCommonStart + k] -- entry 0 is the hip,
 * the game's joints[4]. The pack carries the model's DObjDesc tree and
 * nothing above it: TopN is a DObj the fighter manager makes itself, and
 * TransN, XRotN and YRotN are hidden parts (FPackAttr.hiddenparts) the
 * game makes and links above the hip for the animations that ask, and
 * ejects for the rest. The fighter proper reads its joints through
 * FTStruct.joints and deals the raw table down the game's tree walk
 * (src/dc/ftcommon.c). */

/* Load a pack by name ("mario.pack"), upload its textures, compile its
 * poly headers. Which medium the name resolves against -- the romdisk or
 * the disc -- is src/dc/assetroot.h's business, and a name that already
 * contains a '/' is taken as the path it is. `*pal_bank` is the next free
 * 16-colour PVR palette bank and is advanced past the banks this fighter
 * takes. Returns 0, or -1 with the reason on dbglog. */
int fighter_load(Fighter *f, const char *name, int *pal_bank);

/* fighter_load reading only tier `tier` of the pack's .anm
 * (FPackAttr.anm_tier_end), or the whole of it for FIGHTER_ANM_FULL: a
 * scene that plays a handful of each fighter's motions -- the character
 * selects load all twelve -- holds those and not 4 MB of the rest. An
 * animation outside the tier is read when it is asked for (below). */
#define FIGHTER_ANM_FULL FPACK_ANM_TIERS
int fighter_load_tier(Fighter *f, const char *name, int *pal_bank, int tier);

/* The pack's .anm to tier `tier` or the whole of it, if it holds less:
 * a pack kept from a menu into the battle. The prefix it held is freed
 * when `retire` is 0 -- nothing plays from it -- and kept to the
 * release otherwise. 0, or -1 with the pack left as it was. */
int fighter_anm_grow(Fighter *f, int tier, int retire);

/* fighter_load for a pack that lives as long as one scene and no longer:
 * the opening movie's models, a scene's own props. The pack is malloc'd
 * and pvr_mem_malloc'd and belongs to no heap, so nothing frees it when
 * the scene heap resets; a scene that released it only in its own
 * OverlayLoad -- which runs when the scene is next ENTERED, not when it is
 * left -- kept its textures in VRAM through everything after it, and the
 * nineteen-scene movie left the auto-demo battle behind it without room
 * for a texture (pvr_mem_malloc failed). This one is released, through
 * the heap-reset hook, at the next scene change. The Fighter is zeroed by
 * the release, so the scene's own OverlayLoad release becomes a no-op. */
int fighter_load_scene(Fighter *f, const char *name, int *pal_bank);

/* Same, from a pack already in memory (a stage embeds one); `blob` is
 * owned by the caller and must outlive the Fighter. */
int fighter_init(Fighter *f, void *blob, uint32_t size, int *pal_bank);

/* fighter_init for a blob whose offsets a previous load already made
 * pointers, which is what a romdisk pack is on its second scene
 * (fighter_load keeps the list): everything but the relocation. */
int fighter_init_ex(Fighter *f, void *blob, uint32_t size, int *pal_bank,
                    int relocated);

/* Hand a pack whose animations are in an .anm (FPackAttr.anm_size) that
 * file as read: checked against the pack, its script pointers made
 * addresses, and kept -- and freed in fighter_release when `owned`.
 * fighter_load does this itself; a caller that fighter_init's a pack
 * does it after. Returns 0, or -1 with the reason on dbglog. */
int fighter_attach_anm(Fighter *f, void *anm, uint32_t size, int owned);

/* Animation `a`'s words' length in bytes. */
static inline uint32_t fighter_anim_bytes(const FPackAnim *a)
{
    return a->nwords * (a->kind == FPACK_ANIM_FIGATREE ? 2u : 4u);
}

/* The first word of animation `a` of `f`: in the pack, or in the .anm
 * attached to it (a clone's pack's) -- NULL when that has not been
 * attached, or holds a tier that `a` is not in. */
static inline const void *fighter_anim_words(const Fighter *f,
                                             const FPackAnim *a)
{
    const Fighter *o = (f->anm_src != NULL) ? f->anm_src : f;

    if (o->attr != NULL && o->attr->anm_size != 0)
    {
        return (o->anm != NULL &&
                a->off_words + fighter_anim_bytes(a) <= o->anm_size)
                   ? (const uint8_t *)o->anm + a->off_words : NULL;
    }
    return (o->blob != NULL) ? (const uint8_t *)o->blob + a->off_words
                             : NULL;
}

/* The same, reading the rest of the .anm when `a` is not in the tier it
 * holds -- which says so on the log, once per pack: the tier lists
 * (tools/export/anm_tiers.tsv) missed an animation a scene plays. */
const void *fighter_anim_words_need(Fighter *f, const FPackAnim *a);

/* Where a FPACK_TEX_EXTERN texture comes from. `txr` is a PVR texture the
 * caller made and owns, `txrfmt` its PVR_TXRFMT_*, and `w`/`h` its real
 * dimensions -- which are not the picture's: the UVs run 0..1 over the
 * whole texture, so a picture that does not fill it is stretched to.
 *
 * There is one of these because the game has one: sLBTransitionPhotoHeap.
 * It has to be set before the pack that reads it is loaded, which is the
 * order lbTransitionSetupTransition and lbTransitionMakeTransition already
 * run in (lbtransition.c:3362 in mnVSResultsFuncStart). */
void fighter_set_extern_texture(pvr_ptr_t txr, uint32_t txrfmt,
                                int w, int h);

/* And where the picture sits in that texture, for one it does not fill:
 * a batch on the extern texture draws with u' = u0 + u * su and
 * v' = v0 + v * sv. fighter_set_extern_texture resets it to the whole
 * texture; set it after that and before the pack loads, which copies it
 * (Fighter.ext_win). The transition wipes' picture is the frame loop's
 * photo (taskman.h), whose 300x220 middle is a window of its 1024x512. */
void fighter_set_extern_window(float u0, float v0, float su, float sv);

/* Give back everything fighter_init and fighter_load took: the compiled
 * headers, the clip-space pool, the PVR textures the pack owns (not an
 * extern one, which belongs to whoever bound it) and the blob if this
 * Fighter owns it. `f` is left zeroed and can be loaded into again.
 *
 * Every other pack the port loads is resident for the run, so
 * nothing calls for it; a transition wipe is the first model that lives
 * for one scene (src/dc/lbtransition.c). Never call it on a clone, whose
 * blob, headers and textures are somebody else's.
 */
void fighter_release(Fighter *f);

/* -DDB_ANIM_CHECK: -1, and a log line naming every released pack that
 * held the address, when `script` lies in no loaded pack */
int fighter_animcheck(const void *script, const char *what);
/* and a buffer that is not a pack but holds scripts (the item pack) */
void fighter_animcheck_add(const void *lo, uint32_t size, const char *name);
void fighter_animcheck_remove(const void *lo);

/* The live colours of the MObj that drew batch `batch`, or NULL where the
 * batch has none -- which is every batch of every pack without an
 * FPackMObjs section. draw_batches folds them over the batch's baked
 * ones. */
const DCMObjColor *fighter_batch_mobj(const Fighter *f, int batch);

/* Where the display walk writes what it found on the MObj chain: entry
 * `index` of joint `joint`'s MObjs, or NULL when the pack has no such
 * MObj. src/dc/objdisplay.c's gcDrawMObjForDObj is the only caller. */
DCMObjColor *fighter_mobj_color(Fighter *f, int joint, int index);

/* A second instance of a loaded pack: `dst` shares `src`'s blob, textures
 * and compiled headers and gets its own pose, skeleton, light and clip
 * state, so two fighters built from one pack can each be drawn from
 * their own DObj tree (ft/ftmanager.c makes N fighters from one
 * FTData). `vclip` is `src->hd->vert_count` float[4]s the caller owns
 * -- the scene heap, on a fighter -- and must outlive `dst`. Neither
 * blob nor vclip is freed for a clone. */
void fighter_clone(Fighter *dst, const Fighter *src, float (*vclip)[4]);

/* Fold camera+projection into every joint, pull the light back into each
 * joint's space, and transform every vertex into clip space. `mv`/`proj`
 * are row-major column-vector float[16].
 *
 * Both draws read the clip positions this leaves behind, so it must run
 * each frame before either of them, and again if the camera moves. */
void fighter_frame(Fighter *f, const float *mv, const float *proj,
                   float lx, float ly, float lz);

/* Submit the opaque batches to the current PVR list, which must be
 * PVR_LIST_OP_POLY. Returns triangles emitted. */
/* Whether a draw into FPACK_LIST_* can put anything on the PVR for this
 * model. False means the pass can skip it whole: no walk, no transform,
 * no batch loop. Worked out once from the batches. */
int fighter_has_list(Fighter *f, int fpack_list);

uint32_t fighter_draw(Fighter *f);

/* Submit only the punch-through batches -- call inside PVR_LIST_PT_POLY. */
uint32_t fighter_draw_pt(Fighter *f);

/* Submit only the translucent batches -- call inside PVR_LIST_TR_POLY.
 * Between them the three cover every batch exactly once. */
uint32_t fighter_draw_tr(Fighter *f);

/* Every batch, opaque ones too, into the translucent list at `depth`
 * plus a hair of the real 1/w: for a fighter that stands between two
 * sprite passes (a menu's, on the character select's gate cards and
 * under its pucks), where the opaque list -- drawn first, under every
 * sprite -- cannot put it. The PVR sorts the translucent list per pixel,
 * so the fighter's own polygons still resolve by depth among themselves.
 * The transform is fighter_frame's, as for the other two. */
uint32_t fighter_draw_layered(Fighter *f, float depth);

/* fighter_draw_layered into a slot `width` deep rather than half a
 * sprite step: for a model in lbcommon.h's backdrop band, whose steps
 * are far narrower than the front band's. */
uint32_t fighter_draw_layered_band(Fighter *f, float depth, float width);

/* fighter_draw_layered_band for a model the N64 drew with the Z buffer
 * off, in display-list order: no batch's depth comes from its 1/w, each
 * takes a flat one in the slot, later batches above earlier ones, so the
 * PVR's per-pixel sort reproduces the painter's order. The boss
 * wallpaper's two counter-rotating vortex cones are the case: the same
 * cone twice, which at real depth swapped pink and cyan facet by facet. */
uint32_t fighter_draw_layered_paint(Fighter *f, float depth, float width);

/* Every batch into the translucent list at its real 1/w times one scale,
 * which puts the model's farthest vertex at `base` (and batch i a further
 * 1 + i * 1e-4, nearer in display-list order). For a Z-less model drawn
 * ahead of the battle camera, `base` taken from lbcommon.h's backdrop 3D
 * stack with fighter_layered_ratio's ratio (src/dc/objmodel.c): under
 * every 3D pixel, over the stage wallpaper and everything the stack took
 * before it, and textured with perspective correction intact, which
 * neither the band draw (base + scale * 1/w) nor the paint draw (flat)
 * can be -- the boss stage's fog and vortex showed every big triangle's
 * seam under both. */
uint32_t fighter_draw_layered_scaled(Fighter *f, float base);

/* The range fighter_draw_layered_scaled will need from `base` up, as a
 * ratio: the model's nearest 1/w this frame over its farthest, with the
 * batches' nudges on top. 0 when nothing of it is in front of the eye. */
float fighter_layered_ratio(const Fighter *f);

/* Every model drawn from here on has its colours, base and offset,
 * scaled by k / 256 (256, the default, is none; alpha is untouched) --
 * a translucent black quad of alpha a laid over them afterwards, without
 * the quad. The difference is what else the quad would have covered:
 * the opening Room's close-up dimmer darkens the room but not the
 * trophy and its spotlight, whose camera draws after the dimmer yet
 * shares the room's Z buffer, which a sprite-depth quad cannot do
 * (src/dc/mvopeningroom.c). Blending keeps the identity: a translucent
 * batch over dimmed colour, itself dimmed, is the dimmed blend. */
void fighter_set_dim(unsigned k);

/* Every batch into the translucent list at `depth`, for a model drawn
 * under an ortho camera, clipped to `planes` (clip.h): the fighter
 * inside the magnifying glass, and the glass's own handle with no
 * planes. Under an ortho projection 1/w is the same for every vertex,
 * so fighter_draw_layered's spread would flatten the model; this one
 * maps the model's own range of clip z onto the sprite step below
 * `depth` instead, nearest highest, so its parts still resolve among
 * themselves. The transform is fighter_frame's. */
uint32_t fighter_draw_layered_ortho(Fighter *f, float depth,
                                    const float (*planes)[3], int nplanes,
                                    float inradius);

/* The live primitive colour, see Fighter.prim_live. */
static inline void fighter_set_prim_color(Fighter *f, uint32_t argb)
{
    f->prim_live = argb;
    f->prim_live_on = 1;
}

/* The live environment colour, see Fighter.env_live: RGBA packed as the
 * game packs it. */
static inline void fighter_set_env_color(Fighter *f, uint32_t rgba)
{
    f->env_live = rgba;
    f->env_live_on = 1;
}

/* The live fog colour, see Fighter.fog_live. */
/* The costume this instance wears (FPackCostumes): 0 for a pack
 * without them, and clamped to the pack's last. */
/* Whether a batch or vertex run tagged `tag` on `joint` draws: untagged,
 * the part the joint has on, or the accessory while it is on. */
static inline int fighter_part_on(const Fighter *f, int tag, int joint)
{
    if (f->skeleton_id != 0)
    {
        return (tag != 0) && (f->parts != NULL) &&
               (tag <= (int)f->parts->tag_count) &&
               (f->part_tags[tag - 1].skeleton == f->skeleton_id);
    }
    return (tag == 0) || (tag == f->part_cur[joint]) ||
           ((f->parts != NULL) && (tag == (int)f->parts->accessory_tag) &&
            f->accessory_on);
}

static inline void fighter_set_costume(Fighter *f, int costume)
{
    int n = (f->costumes != NULL) ? (int)f->costumes->costume_count : 1;

    f->costume = (costume < 0) ? 0 : (costume >= n) ? n - 1 : costume;
}

/* Put texture part `part` on `frame` (FPackTexParts). FALSE where the
 * pack has no frames for the part -- the game's "no MObj at that detail",
 * which leaves everything as it was. */
static inline int fighter_set_texture_frame(Fighter *f, int part, int frame)
{
    if (f->texparts == NULL || part < 0 || part > 1 ||
        f->texparts->frame_count[part] == 0)
    {
        return 0;
    }
    f->texpart_frame[part] = (uint8_t)frame;

    return 1;
}

static inline void fighter_set_fog_color(Fighter *f, uint32_t rgba)
{
    f->fog_live = rgba;
}

static inline void fighter_set_alpha(Fighter *f, uint8_t alpha)
{
    f->alpha_cut = 0xFFu - alpha;
}


#endif /* DC_FIGHTER_H */

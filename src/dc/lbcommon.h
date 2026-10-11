/* lbcommon.h -- see lbcommon.c: lb/lbcommon.c's table trigonometry,
 * its positioned sound effect, and its sprite renderer. */
#ifndef SSB_DC_LBCOMMON_H
#define SSB_DC_LBCOMMON_H

#include <ssb_types.h>
#include <macros.h>
#include <PR/gbi.h>              /* Gfx, for the display-list heads the
                                    sprite functions take and ignore */
#include <PR/sp.h>               /* Sprite, Bitmap */
#include <sys/obj.h>             /* DObj, for the tree helpers below */
#include <gm/gmsound.h>          /* alSoundEffect */

/* lbcommon.c:18 */
extern f32 dLBCommonSinLookup[/* */];

/* lbcommon.c:321, 340, 359: sine, cosine and tangent of an angle in
 * radians, through the game's 4096-step table */
f32 lbCommonSin(f32 angle);
f32 lbCommonCos(f32 angle);
f32 lbCommonTan(f32 angle);

/* lb/lbcommon.c:725-748: a sound effect panned by where on the stage
 * it happens; the hit sounds go through it (ft/ftmain.c:2107). */
alSoundEffect *lbCommonMakePositionFGM(u16 fgm, f32 pos);

/* ---- lb/lbcommon.c:2218-3007: the sprite renderer --------
 *
 * The game's 2D: an SObj holds a libultra Sprite, a GObj's proc_display
 * (lbCommonDrawSObjAttr) walks the GObj's SObjs and draws each as one
 * texture rectangle per Bitmap strip, and a camera whose proc_display is
 * lbCommonDrawSprite sets the scissor and captures the GObjs on its DL
 * links. Every function below is the decomp's, name and body; what
 * changed is the last step of lbCommonDrawSObjBitmap, where the RDP got
 * a texture load and a rectangle and the PVR gets a polygon header and
 * a quad. The comment block at that point in lbcommon.c says how the
 * RDP's state is carried across and what does not survive.
 *
 * The sprites themselves come from a SpriteBank (sprite.h); a Bitmap's
 * buf points at its DCSpriteTex. */
struct GObj;
struct SObj;
struct DCSpriteTex;

void lbCommonDrawSObjBitmap(Gfx **dls, struct SObj *sobj, Sprite *sprite,
                            Bitmap *bitmap, s32 x, s32 y, s32 xx, s32 yy,
                            s32 fs, s32 ft, s32 sx, s32 sy);
void lbCommonPrepSObjAttr(Gfx **dls, struct SObj *sobj);
void lbCommonPrepSObjDraw(Gfx **dls, struct SObj *sobj);
void lbCommonClearExternSpriteParams(void);
void lbCommonSetExternSpriteParams(Sprite *sprite);
void lbCommonDrawSObjAttr(struct GObj *gobj);
void lbCommonDrawSObjNoAttr(struct GObj *gobj);
struct SObj *lbCommonMakeSObjForGObj(struct GObj *gobj, Sprite *sprite);
struct GObj *lbCommonMakeSpriteGObj(u32 id, void (*func_run)(struct GObj *),
                                    s32 link, u32 link_priority,
                                    void (*proc_display)(struct GObj *),
                                    s32 dl_link, u32 dl_link_priority,
                                    u32 camera_tag, Sprite *sprite,
                                    u8 gobjproc_kind,
                                    void (*proc)(struct GObj *),
                                    u32 gobjproc_priority);
void lbCommonStartSprite(Gfx **dls);

/* ---- the fighter tree's helpers, verbatim from lb/lbcommon.c:
 * the DObj after `a` in draw order under root `b` (750), a figatree table
 * dealt down that walk (807), and a DObj given its transforms (893). The
 * fighter's status setter and hidden parts (src/dc/ftcommon.c) call them
 * where the game does. */
DObj *lbCommonGetTreeDObjNextFromRoot(DObj *a, DObj *b);
void lbCommonAddFighterPartsFigatree(DObj *root_dobj, void **figatree, f32 anim_frame);
void lbCommonInitDObj(DObj *dobj, u8 tk1, u8 tk2, u8 tk3, u8 arg4);
void lbCommonAddDObjAnimJointAll(DObj *root_dobj, AObjEvent32 **anim_joints, f32 anim_frame);
/* lb/lbcommon.c:1112-1156 lbCommonAddTreeDObjsAnimAll and :1169-1191
 * lbCommonAddMObjForTreeDObjs(ef/efground.c's engine): the
 * tree-walk counterpart of lbCommonAddDObjAnimJointAll above, one table
 * entry per DObj of the walk rather than per fighter-parts joint, plus
 * the MObj material-anim and MObjSub halves the ground actors need and
 * the fighter tree does not. */
void lbCommonAddTreeDObjsAnimAll(DObj *root_dobj, AObjEvent32 **anim_joints, AObjEvent32 ***p_matanim_joints, f32 anim_frame);
void lbCommonAddMObjForTreeDObjs(DObj *root_dobj, MObjSub ***p_mobjsubs);
/* lb/lbcommon.c:1194 lbCommonPlayTreeDObjsAnim: start
 * every script the walk above attached. gcPlayAnimAll's counterpart for
 * a tree that hangs off somebody else's DObj rather than off a GObj --
 * a Board the Platforms platform, on one of the map's yakumono DObjs. */
void lbCommonPlayTreeDObjsAnim(DObj *root_dobj);
/* lb/lbcommon.c:986-995 lbCommonInitDObj3Transforms 0x800C89BC, verbatim:
 * lbCommonInitDObj's cousin, gcAddDObj3TransformsKind's three kinds in
 * one call rather than gcAddXObjForDObjFixed one at a time -- the ground
 * actors' own DObjs use it (ef/efground.c efGroundMakeEffect); the
 * fighter tree still uses lbCommonInitDObj above. */
void lbCommonInitDObj3Transforms(DObj *dobj, u8 tk1, u8 tk2, u8 tk3);
void lbCommonPlayTranslateScaledDObjAnim(DObj *dobj, Vec3f *scale);
void lbCommonSetSpriteScissor(s32 xmin, s32 xmax, s32 ymin, s32 ymax);
void lbCommonFinishSprite(Gfx **dls);
void lbCommonDrawSprite(struct GObj *camera_gobj);

/* DIVERGES (port only): lbCommonDrawSprite for a camera whose sprites are
 * a backdrop -- drawn first on the N64 by a higher dl_link_priority (the
 * openings' wallpaper cameras, mv/mvopening/mvopeningcliff.c:408 and
 * kin), or placed at the far end of the Z buffer outright
 * (mvopeningroom.c:586 gDPSetPrimDepth + G_ZS_PRIM). The PVR's
 * translucent list is depth-tested against the opaque one, and the front
 * band's 1.0 beats every 3D pixel, so a wallpaper drawn there covers the
 * props behind the fighters. This proc runs the same capture with the
 * frame's depth counter switched to LB_SPRITE_Z_BACKDROP_BASE, under
 * every 3D camera's far plane: the quads show where nothing opaque was
 * drawn and lose everywhere else, which is the N64's result. */
void lbCommonDrawSpriteBackdrop(struct GObj *camera_gobj);

/* lbcommon.c:2113: eject every GObj on a link, tail first. The pause
 * menu's two GObjs go this way (src/dc/ifcommon.c). */
void lbCommonEjectGObjLinkedList(struct GObj *gobj);

/* The colour combiner programs the sprite code sets, as the port models
 * them (lbcommon.c lbCommonRDPSetCombineLERP):
 *   IPrim       RGB = PRIM, A = TEXEL0            -- I sprites
 *   IPrimAlpha  RGB = PRIM, A = TEXEL0 * PRIM.a   -- mnTitleLogoProcDisplay
 *   IAPrimEnv   RGB = lerp(ENV, PRIM, TEXEL0), A = TEXEL0 * PRIM.a
 *   TexPrimEnv  RGB = lerp(PRIM, TEXEL0, ENV), A = TEXEL0
 *                                               -- mnPlayersVSPuckProcDisplay,
 *                                                  the puck glowing to white
 *   Decal       RGBA = TEXEL0                     -- RGBA and CI sprites, and
 *                                                  the noise lerp of
 *                                                  mnPlayersVSPortraitProcDisplay
 *                                                  (DIVERGES: no noise)
 *   TexPrimAlpha RGB = TEXEL0, A = TEXEL0 * PRIM.a -- mnTitleFireProcDisplay,
 *                                                  the title's flames
 *   TexPrim     RGB = TEXEL0 * PRIM, A = PRIM     -- G_CC_MODULATEI_PRIM,
 *                                                  Stage Clear's wallpaper
 *                                                  at half brightness */
enum
{
    nLBCommonCombineDecal,
    nLBCommonCombineIPrim,
    nLBCommonCombineIPrimAlpha,
    nLBCommonCombineIAPrimEnv,
    nLBCommonCombineTexPrimEnv,
    nLBCommonCombineTexPrimAlpha,
    nLBCommonCombineTexPrim
};

/* The port's spelling of the three RDP commands a scene's own display
 * proc puts between lbCommonPrepSObjAttr and lbCommonDrawSObjNoAttr
 * (mn/mncommon/mntitle.c:1019-1023 mnTitleLogoProcDisplay:
 * gDPSetPrimColor and gDPSetCombineLERP on gSYTaskmanDLHeads[0];
 * mn/mnplayers/mnplayersvs.c:3723 mnPlayersVSPuckProcDisplay adds
 * gDPSetEnvColor). */
void lbCommonSpriteSetPrimColor(u32 r, u32 g, u32 b, u32 a);
void lbCommonSpriteSetEnvColor(u32 r, u32 g, u32 b, u32 a);
void lbCommonSpriteSetCombine(s32 combine);

/* gSPTextureRectangle's arguments, and the quad they become. Kept as
 * types so the host test can check the one piece of arithmetic that is
 * the port's rather than the decomp's. Screen coordinates are 10.2 in
 * the game's 320x240, s/t 10.5 texels, the steps 5.10; t0 is the
 * strip's first row in the stitched texture (Bitmap.t). */
typedef struct LBCommonSpriteRect
{
    s32 rxh, ryh, rxl, ryl;
    s32 rs, rt;
    s32 sx, sy;
    s32 t0;
    sb32 copy;
} LBCommonSpriteRect;

typedef struct LBCommonSpriteQuad
{
    f32 x0, y0, x1, y1;         /* framebuffer pixels */
    f32 u0, v0, u1, v1;         /* texture space, 0..1 */
    f32 z;
    u32 argb, oargb;
    LBCommonSpriteRect rect;    /* what it was made from (host log) */
} LBCommonSpriteQuad;

void lbCommonSpriteQuadOf(const LBCommonSpriteRect *r,
                          const struct DCSpriteTex *tex,
                          LBCommonSpriteQuad *q);

/* The game's pixels onto the port's 640x480 framebuffer: 2 for the
 * game's 320x240, 1 for the staff roll's 640x480 (src/dc/sysshim.c
 * dcVideoSetResolution). */
extern f32 gDCScreenScale;
#define LB_SPRITE_SCREEN_SCALE gDCScreenScale

/* Sprite depth: above anything a 3D camera draws (every camera's near
 * plane keeps real geometry's 1/w no bigger than 0.01 -- sys/objman.c
 * dGCPerspDefault's near=100 -- so 1.0 clears it 100x over), and
 * stepped per rectangle so later ones win the PVR's per-pixel sort the
 * way later ones win the RDP's draw order.
 *
 * Kept close to that floor rather than far above it, because this base
 * is also fighter_draw_layered's (fighter.c) zero for a menu model's
 * own ~1e-3 depth spread: everything summed onto it shares one
 * float32's precision at this magnitude. The old 8.0 put that spread
 * within 1-3 orders of magnitude of its own ULP -- coarse enough for
 * near-coplanar geometry (a shield's trim over its base) to tie and
 * flicker. 1.0 buys back roughly a decimal digit of precision there
 * while keeping ~25x margin over a frame's sprite-depth counter ever
 * running past a couple hundred -- see lbCommonSpriteNextDepth's
 * DB_SPRITE_DEPTH_BUDGET guard, which checks that assumption instead
 * of leaving it one. */
#define LB_SPRITE_Z_BASE 1.0F
#define LB_SPRITE_Z_STEP (1.0F / 256.0F)

/* The bottom of the depth space, nearest first: the backdrop band below,
 * the backdrop 3D stack (lbCommonBackdropStackTake), the stage wallpaper
 * quad at LB_Z_WALLPAPER (src/dc/stage.c) and the PVR background plane at
 * LB_Z_ZCLIP (src/game/ssb64/main.c pvr_set_zclip), each under the one
 * before. No camera's far plane reaches
 * past 30000 units, so nothing real draws down there either way. Not
 * lower than 1e-10: the PVR maps depth as log2(1 + w) / 34 and drops
 * anything past w = 2^34 (1/w under 6e-11) -- the wallpaper at 1e-12
 * vanished there. */
#define LB_Z_ZCLIP 1.0e-10F
#define LB_Z_WALLPAPER 2.0e-10F

/* The backdrop band, for lbCommonDrawSpriteBackdrop's cameras: below
 * every 3D pixel and above the empty screen. Its bounds are the backdrop
 * 3D stack and the stage wallpaper (above) below, and the
 * nearest far plane's 1/w above -- 1/30000 = 3.3e-5 (the openings'
 * sector scene; every other opening camera's far is 16384 or nearer).
 * 2e-5 sits between with 256 steps of 2e-8 reaching 2.5e-5, and float32
 * resolves 2e-8 at this magnitude a thousand times over. */
#define LB_SPRITE_Z_BACKDROP_BASE 2.0e-5F
#define LB_SPRITE_Z_BACKDROP_STEP 2.0e-8F

/* The next step of the frame's depth counter: what every sprite
 * rectangle and fill quad takes, and what a menu's fighter takes to
 * stand between two sprite passes (src/dc/objmodel.c). */
f32 lbCommonSpriteNextDepth(void);

/* The next step of the backdrop band's own count, from a caller that is
 * not a backdrop camera: a 3D model a scene draws Z-less before its
 * battle camera, which must lose to every 3D pixel and still cover the
 * stage's wallpaper (src/dc/sc1pgameboss.c's camera-tag-2 effects). */
f32 lbCommonSpriteNextBackdropDepth(void);

/* The backdrop 3D stack: where a Z-less 3D model drawn before the
 * battle camera (sc1pgameboss.c's camera-tag-2 trees) takes its depths.
 *
 * The PVR has one 1/w per vertex, and it is BOTH the depth the list is
 * sorted by AND the 1/w the texture is perspective-corrected with. A band
 * that maps 1/w as base + scale * 1/w (or flattens it) puts a model where
 * it wants it and textures every triangle affine -- each big triangle of
 * the boss stage's fog and vortex showed its own seams. Only a uniform
 * SCALE of 1/w leaves the correction exact, so each model takes a range
 * [base, base * ratio] of its own -- ratio its own nearest over farthest
 * 1/w -- and draws at 1/w * base / farthest: the models one after
 * another, each wholly over the one before (the N64's draw order, these
 * draws being Z-less), each textured as the N64 textures it.
 *
 * Two stacks, `under` for what one camera drew into an EARLIER display-
 * list head than the rest: a camera's heads splice 0, 2, 1, 3 (sys/
 * taskman.c syTaskmanUpdateDLBuffers), so the boss's head-0 picture at
 * the end of the vortex draws under the head-1 vortex whichever came
 * first. Counted per frame. Returns the range's base; a stack that runs
 * out reuses its top (and says so under -DDB_SPRITE_DEPTH_BUDGET). */
#define LB_Z_STACK_UNDER_LO 3.0e-10F
#define LB_Z_STACK_UNDER_HI 9.0e-10F
#define LB_Z_STACK_LO 1.0e-9F
#define LB_Z_STACK_HI 1.9e-5F
f32 lbCommonBackdropStackTake(sb32 under, f32 ratio);

/* A screen rectangle in one flat colour, blended by its alpha: the
 * gDPFillRectangle under G_CC_PRIMITIVE that lb/lbfade.c's fade and
 * mn/mncommon/mnmessage.c's tint draw, from display procs that run as
 * cameras of their own (outside any sprite pass). Game pixels,
 * lower-right exclusive. On the host it lands in the quad log with
 * rect.copy == -1. */
void lbCommonSpriteFillRect(s32 ulx, s32 uly, s32 lrx, s32 lry,
                            u32 r, u32 g, u32 b, u32 a);

#ifdef SSB_NO_DRAW
/* Every quad the host build would have submitted since the count was
 * zeroed, in order. */
#define LB_SPRITE_QUAD_LOG_MAX 128
extern LBCommonSpriteQuad gLBCommonSpriteQuadLog[LB_SPRITE_QUAD_LOG_MAX];
extern s32 gLBCommonSpriteQuadLogCount;
#endif

#endif /* SSB_DC_LBCOMMON_H */

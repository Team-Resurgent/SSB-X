/* ftshadow.h -- ft/ftshadow.h, plus the model src/dc/ftshadow.c's copy
 * of ftShadowProcDisplay draws through.
 *
 * The three declarations are the decomp's own (ssb-decomp-re/src/ft/
 * ftshadow.h). The rest is the port's: the log of what each shadow's
 * arithmetic produced, which is what hosttest_ft.c checks and what
 * src/dc/db.c prints, and the frame reset src/dc/taskman.c calls.
 */
#ifndef SSB_DC_FTSHADOW_H
#define SSB_DC_FTSHADOW_H

#include <ssb_types.h>
#include <sys/objdef.h>
#include <ft/ftdef.h>
#include <PR/gbi.h>

extern f32 ftShadowGetAltitude(Vec3f *a, Vec3f *b, f32 f);
extern void ftShadowProcDisplay(GObj *shadow_gobj);
extern GObj* ftShadowMakeShadow(GObj *fighter_gobj);

/* ft/ftdef.h:7 FTDISPLAY_DLLINK_DEFAULT is 9; ftShadowMakeShadow's own
 * gcAddGObjDisplay names 7, so a shadow draws before the fighter over
 * it. src/dc/scvsbattle.c puts it in the battle camera's mask. */
#define FTSHADOW_DLLINK 7

/* One shadow's draw, as ftShadowProcDisplay built it: the primitive
 * colour gDPSetPrimColor set, the four, six or eight vertices gSPVertex
 * handed over, and the two, four or six triangles gSP2Triangles indexed
 * them with. The Vtx are the game's, so ob[] is the world position in
 * s16 and tc[] the texture coordinate in S10.5. */
typedef struct FTShadowDraw
{
    u8 prim[4];                 /* r, g, b, a */
    s32 vtx_num;
    Vtx vtx[8];
    s32 tri_num;
    u8 tri[6][3];
} FTShadowDraw;

#define FTSHADOW_LOG_MAX 4

extern FTShadowDraw gFTShadowLog[FTSHADOW_LOG_MAX];
extern s32 gFTShadowLogCount;

void ftShadowEjectShadow(GObj *fighter_gobj);
void ftShadowResetFrame(void);
void ftShadowOverlayLoad(void);

#endif

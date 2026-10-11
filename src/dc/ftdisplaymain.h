/* ftdisplaymain.h -- ft/ftdisplaymain.c, the fighter's display proc.
 *
 * One function of that file's 1400 is ported, and it is the outer shape
 * of the proc rather than the drawing inside it: the block that decides,
 * once a frame under the battle camera, whether a fighter has left the
 * viewport -- and if it has, where on the edge to point at it. That is
 * what raises gIFCommonPlayerInterface.arrows_flags for the off-screen
 * arrows and writes FTStruct.magnify_pos for the magnifying glasses
 * -- and, under the magnify camera, the arms that draw the
 * fighter again inside its glass and the handle over it.
 *
 * The fog colour a fighter is drawn under is ported too:
 * the tint and team shade become Fighter.fog_live on its models.
 * Everything else in the decomp's proc is the GBI emitter for a
 * fighter's parts -- the reflection lights, the shadow, the
 * hit-collision debug shapes -- and the port draws a fighter from its
 * baked pack instead (src/dc/objmodel.c dc_model_proc_display), so this
 * calls that where the decomp calls ftDisplayMainDrawAll.
 * ftdisplaymain.c's own name is kept because it is the file the block
 * comes from and because ft/ftmanager.c names this function as every
 * fighter's proc_display.
 */
#ifndef SSB_DC_FTDISPLAYMAIN_H
#define SSB_DC_FTDISPLAYMAIN_H

#include <sys/obj.h>
#include <ft/fttypes.h>

/* ftdisplaymain.c:1068-1160 (the head) then src/dc/objmodel.c: the
 * display proc every FTDesc names (src/dc/ftmanager.c:215). */
void ftDisplayMainProcDisplay(GObj *fighter_gobj);

/* ftdisplaymain.c:929-947: which electric skeleton ftDisplayMainDrawAll
 * would draw the fighter as, 0 for its own model */
s32 ftDisplayMainGetSkeletonID(FTStruct *fp);

/* ftdisplaymain.c:599-684: the fog colour, its setter and the shade-only
 * choice */
void ftDisplayMainCalcFogColor(FTStruct *fp);
void ftDisplayMainSetFogColor(FTStruct *fp);
void ftDisplayMainDecideFogColor(FTStruct *fp);
void ftDisplayMainDrawAccessory(FTStruct *fp, DObj *dobj, FTParts *parts);

/* ftdisplaymain.c:397-596: the swing trail, and the record
 * of what it last drew -- vertices in world space and how many triangles
 * over them, for the host tests. */
#define FTDISPLAYMAIN_AFTERIMAGE_VTX_MAX 48
void ftDisplayMainDrawAfterImage(FTStruct *fp);
extern Vtx gFTDisplayMainAfterImageVtx[FTDISPLAYMAIN_AFTERIMAGE_VTX_MAX];
extern s32 gFTDisplayMainAfterImageVtxCount;
extern s32 gFTDisplayMainAfterImageTriCount;

#endif /* SSB_DC_FTDISPLAYMAIN_H */

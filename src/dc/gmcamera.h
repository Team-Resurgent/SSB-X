/* gmcamera.h -- gm/gmcamera.c, the battle camera, ported (see gmcamera.c
 * for what is and is not). Names, signatures and the globals are the
 * decomp's (gm/gmcamera.h). */
#ifndef SSB_DC_GMCAMERA_H
#define SSB_DC_GMCAMERA_H

#include <gm/generic.h>         /* GMCamera and, through gm/gmcamera.h, the
                                 * camera's own declarations */

/* The port's: gGMCameraMatrix from the camera's CObj, at the end of
 * gmCameraRunFuncCamera (gmcamera.c on why there). */
void gmCameraUpdateMatrix(GObj *camera_gobj);

/* gmcamera.c:1422 0x8010E2D4, the off-screen arrows' ortho camera.
 * gm/gmcamera.h declares its display proc and every other camera's maker,
 * but not this one -- so it is here rather than there. */
void gmCameraMakePlayerArrowsCamera(void);

/* The port's, for the magnify camera: install the
 * (view, projection) pair its matrix function built for `xobj` -- what
 * gSPMatrix(&xobj->mtx, G_MTX_PROJECTION) does from a fighter's display
 * proc on the N64 (if/ifcommon.c ifCommonPlayerMagnifyUpdateViewport,
 * ifCommonPlayerMagnifyProcDisplay). Kinds 0x4D and 0x4E; any other is
 * refused loudly. */
void gmCameraLoadXObjMatrix(XObj *xobj);

/* Every DL link the game's battle camera draws: the union of the six
 * capture passes of gm/gmcamera.c gmCameraDefaultProcDisplay ({1,2},
 * {4}, {6,7,9,10,11,12}, {13,14,15}, {16,17,18}, {19,20}), which the
 * port's gmCameraDefaultProcDisplay collapses into one ascending walk.
 * The scene sets it (src/dc/scvsbattle.c). It is the game's set and
 * not a census of what the port registers today: twice now a link the
 * mask left out was taken up later and its GObjs silently never drew
 * (the stage hazards, then the items on 11 and the weapons on 14). */
#define GMCAMERA_BATTLE_DLLINK_MASK \
    (COBJ_MASK_DLLINK(1)  | COBJ_MASK_DLLINK(2)  | COBJ_MASK_DLLINK(4)  | \
     COBJ_MASK_DLLINK(6)  | COBJ_MASK_DLLINK(7)  | COBJ_MASK_DLLINK(9)  | \
     COBJ_MASK_DLLINK(10) | COBJ_MASK_DLLINK(11) | COBJ_MASK_DLLINK(12) | \
     COBJ_MASK_DLLINK(13) | COBJ_MASK_DLLINK(14) | COBJ_MASK_DLLINK(15) | \
     COBJ_MASK_DLLINK(16) | COBJ_MASK_DLLINK(17) | COBJ_MASK_DLLINK(18) | \
     COBJ_MASK_DLLINK(19) | COBJ_MASK_DLLINK(20))

#endif /* SSB_DC_GMCAMERA_H */

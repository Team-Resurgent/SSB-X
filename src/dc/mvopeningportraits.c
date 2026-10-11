/* mvopeningportraits.c -- see mvopeningportraits.h. Function-for-function
 * from ssb-decomp-re/src/mv/mvopening/mvopeningportraits.c; every
 * function names its line range. The cuts (FILLCOLOR camera,
 * Lights1/FuncLights, the reloc-load substitution, and the dropped padding
 * field) are explained once, with the busy-wait's history, in
 * mvopeningportraits.h's own header comment -- not repeated at every
 * site.
 */
#include "mvopeningportraits.h"
#include "overlay.h"

#include "lbcommon.h"   /* lbCommonMakeSObjForGObj, lbCommonDrawSprite */
#include "objpvr.h"     /* gcEjectGObjIfMade */
#include "scmanager.h"
#include "sprite.h"
#include "taskman.h"

#include <sys/controller.h>
#include <sys/debug.h>
#include <sys/rdp.h>
#include <sc/scsubsys/scsubsys.h> /* scSubsysControllerGetPlayerTapButtons */
#include <macros.h>

/* The game reaches a sprite as a file's base plus the offset its link
 * label took; the port's file is a bank and the label's value is the
 * offset (src/dc/mvopeningroom.c does the same). Two banks this time --
 * dMVOpeningPortraitsFileIDs lists both -- so both need the redefinition,
 * not just one. */
#undef lbRelocGetFileData
#define lbRelocGetFileData(type, file, offset) \
    ((type)sprite_bank_get((SpriteBank *)(file), (u32)(offset)))

/* relocData files 53 (MVOpeningPortraitsSet1) and 54 (Set2), matching
 * the decomp's own dMVOpeningPortraitsFileIDs order. Offsets are the
 * decomp's own link labels' values, src/dc/decomp/reloc_data.us.h. */
#define llMVOpeningPortraitsSet1SamusSprite    0x9960
#define llMVOpeningPortraitsSet1MarioSprite    0x13310
#define llMVOpeningPortraitsSet1FoxSprite      0x1ccc0
#define llMVOpeningPortraitsSet1PikachuSprite  0x26670
#define llMVOpeningPortraitsSet1CoverSprite    0x2b2d0

#define llMVOpeningPortraitsSet2LinkSprite     0x9960
#define llMVOpeningPortraitsSet2KirbySprite    0x13310
#define llMVOpeningPortraitsSet2DonkeySprite   0x1ccc0
#define llMVOpeningPortraitsSet2YoshiSprite    0x26670

// // // // // // // // // // // //
//                               //
//       INITIALIZED DATA        //
//                               //
// // // // // // // // // // // //

/* mvopeningportraits.c:17 (0x801328A0) */
u32 dMVOpeningPortraitsFileIDs[] = { 53, 54 };

/* mvopeningportraits.c:20, 23 dMVOpeningPortraitsLights11/12: dropped
 * with mvOpeningPortraitsFuncLights below, the standing rule every
 * Lights1/FuncLights pair takes. */

// // // // // // // // // // // //
//                               //
//   GLOBAL / STATIC VARIABLES   //
//                               //
// // // // // // // // // // // //

/* mvopeningportraits.c:35 sMVOpeningPortraitsPad0x801329E0[2]: N64
 * struct-layout padding, read and written by nothing in this file --
 * dropped (mvopeningportraits.h). */

static s32 sMVOpeningPortraitsTotalTimeTics;
static s32 sMVOpeningPortraitsRow;
static GObj *sMVOpeningPortraitsGObj;
static s32 sMVOpeningPortraitsUnused0x801329F4;

/* mvopeningportraits.c:47, 50 sMVOpeningPortraitsStatusBuffer[48] and
 * sMVOpeningPortraitsForceStatusBuffer[7]: the reloc loader's per-file
 * status records, dropped with lbRelocInitSetup (mvOpeningPortraitsLoadFiles
 * below). */

/* Not static: the host test's scene_sprite_find reads this the same way
 * it already reads src/dc/mnmodeselect.c's sMNModeSelectFiles, both
 * exported through their own header with the "s" name the decomp gives
 * them despite the dropped keyword. */
void *sMVOpeningPortraitsFiles[ARRAY_COUNT(dMVOpeningPortraitsFileIDs)];
static SpriteBank sMVOpeningPortraitsBanks[ARRAY_COUNT(dMVOpeningPortraitsFileIDs)];

// // // // // // // // // // // //
//                               //
//           FUNCTIONS           //
//                               //
// // // // // // // // // // // //

/* mvopeningportraits.c:71-79, verbatim. */
void mvOpeningPortraitsMakeSet1(void)
{
    GObj *gobj;
    SObj *sobj;
    s32 i;

    intptr_t offsets[] =
    {
        llMVOpeningPortraitsSet1SamusSprite,
        llMVOpeningPortraitsSet1MarioSprite,
        llMVOpeningPortraitsSet1FoxSprite,
        llMVOpeningPortraitsSet1PikachuSprite
    };
    Vec2f pos[] =
    {
        { 10.0F,  10.0F },
        { 10.0F,  65.0F },
        { 10.0F, 120.0F },
        { 10.0F, 175.0F }
    };

    sMVOpeningPortraitsGObj = gobj = gcMakeGObjSPAfter(0, NULL, 17, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 27, GOBJ_PRIORITY_DEFAULT, ~0);

    for (i = 0; i < (s32)ARRAY_COUNT(offsets); i++)
    {
        sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMVOpeningPortraitsFiles[0], offsets[i]));

        sobj->sprite.attr &= ~SP_FASTCOPY;

        sobj->pos.x = pos[i].x;
        sobj->pos.y = pos[i].y;
    }
}

/* mvopeningportraits.c:97-105, verbatim. */
void mvOpeningPortraitsMakeSet2(void)
{
    GObj *gobj;
    SObj *sobj;
    s32 i;

    intptr_t offsets[] =
    {
        llMVOpeningPortraitsSet2LinkSprite,
        llMVOpeningPortraitsSet2KirbySprite,
        llMVOpeningPortraitsSet2DonkeySprite,
        llMVOpeningPortraitsSet2YoshiSprite
    };
    Vec2f pos[] =
    {
        { 10.0F,  10.0F },
        { 10.0F,  65.0F },
        { 10.0F, 120.0F },
        { 10.0F, 175.0F }
    };

    sMVOpeningPortraitsGObj = gobj = gcMakeGObjSPAfter(0, NULL, 17, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 27, GOBJ_PRIORITY_DEFAULT, ~0);

    for (i = 0; i < (s32)ARRAY_COUNT(offsets); i++)
    {
        sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMVOpeningPortraitsFiles[1], offsets[i]));

        sobj->sprite.attr &= ~SP_FASTCOPY;

        sobj->pos.x = pos[i].x;
        sobj->pos.y = pos[i].y;
    }
}

/* mvopeningportraits.c:122-149. DIVERGES: the four black rows are
 * gDPFillRectangles under the G_CC_PRIMITIVE the cover proc below sets,
 * and raw GBI lands in the write-only scratch heads (src/dc/wpmanager.c)
 * -- the earlier claim here that they "run against whatever the DL head
 * holds" was wrong, nothing reads those heads. Each is the fill quad
 * src/dc/ifscreenflash.c draws, black and opaque. The cover proc runs
 * under a sprite camera (lbCommonDrawSprite, translucent pass only), so
 * every quad here takes the frame's next sprite depth in the RDP's own
 * order: over the portraits drawn before it, under the cover's own
 * sprite drawn after. */
static void mvOpeningPortraitsBlockRect(s32 ulx, s32 uly, s32 lrx, s32 lry)
{
    lbCommonSpriteFillRect(ulx, uly, lrx, lry, 0x00, 0x00, 0x00, 0xFF);
}

void mvOpeningPortraitsBlockRow0(void)
{
    mvOpeningPortraitsBlockRect(10, 10, 310, 65);
}

void mvOpeningPortraitsBlockRow1(void)
{
    mvOpeningPortraitsBlockRect(10, 65, 310, 120);
}

void mvOpeningPortraitsBlockRow2(void)
{
    mvOpeningPortraitsBlockRect(10, 120, 310, 175);
}

void mvOpeningPortraitsBlockRow3(void)
{
    mvOpeningPortraitsBlockRect(10, 175, 310, 230);
}

/* mvopeningportraits.c:160-176, the same substitution. */
void mvOpeningPortraitsBlockPartialRow(s32 row, s32 pos_x)
{
    s32 uly = 10 + row * 55;
    s32 lry = 65 + row * 55;

    if (pos_x > 0)
    {
        mvOpeningPortraitsBlockRect(0, uly, pos_x, lry);
    }
    if ((pos_x + 656) < 0)
    {
        mvOpeningPortraitsBlockRect(0, uly, 320, lry);
    }
    if ((pos_x + 656) < 320)
    {
        mvOpeningPortraitsBlockRect(pos_x + 656, uly, 320, lry);
    }
}

/* mvopeningportraits.c:180-224: the black wipe-cover sprite's own display
 * proc, drawing whichever three rows are NOT the row it is currently
 * covering (plus the sliding partial-row strip) as fill quads, then
 * handing off to the portraits set's own sprite draw. The RDP mode
 * changes around them are the fill quad's own header now. */
void mvOpeningPortraitsCoverProcDisplay(GObj *gobj)
{

    switch (sMVOpeningPortraitsRow)
    {
    case 0:
        mvOpeningPortraitsBlockRow1();
        mvOpeningPortraitsBlockRow2();
        mvOpeningPortraitsBlockRow3();
        mvOpeningPortraitsBlockPartialRow(0, (s32)SObjGetStruct(gobj)->pos.x);
        break;

    case 1:
        mvOpeningPortraitsBlockRow0();
        mvOpeningPortraitsBlockRow2();
        mvOpeningPortraitsBlockRow3();
        mvOpeningPortraitsBlockPartialRow(1, (s32)SObjGetStruct(gobj)->pos.x);
        break;

    case 2:
        mvOpeningPortraitsBlockRow0();
        mvOpeningPortraitsBlockRow1();
        mvOpeningPortraitsBlockRow3();
        mvOpeningPortraitsBlockPartialRow(2, (s32)SObjGetStruct(gobj)->pos.x);
        break;

    case 3:
        mvOpeningPortraitsBlockRow0();
        mvOpeningPortraitsBlockRow1();
        mvOpeningPortraitsBlockRow2();
        mvOpeningPortraitsBlockPartialRow(3, (s32)SObjGetStruct(gobj)->pos.x);
        break;
    }

    lbCommonClearExternSpriteParams();
    lbCommonDrawSObjAttr(gobj);
}

/* mvopeningportraits.c:228-260, verbatim: the cover's own scroll, one row
 * every 30/15 tics, sweeping in from the right at tics <75 and out to the
 * left from 75 on. */
void mvOpeningPortraitsCoverProcUpdate(GObj *gobj)
{
    SObj *sobj = SObjGetStruct(gobj);

    if (sMVOpeningPortraitsTotalTimeTics == 75)
    {
        sobj->pos.x = -656.0F;
    }
    if (sMVOpeningPortraitsTotalTimeTics < 75)
    {
        if (sobj->pos.x < 656.0F)
        {
            sobj->pos.x += 93.0F;

            if (sobj->pos.x > 656.0F)
            {
                sobj->pos.x = 656.0F;
            }
        }
    }
    else if (sobj->pos.x > -656.0F)
    {
        sobj->pos.x -= 93.0F;

        if (sobj->pos.x < -656.0F)
        {
            sobj->pos.x = -656.0F;
        }
    }
    switch (sMVOpeningPortraitsTotalTimeTics)
    {
    case 15:
        sobj->pos.x = -656.0F;
        sobj->pos.y = 10.0F;
        sMVOpeningPortraitsRow = 0;
        break;

    case 45:
        sobj->pos.x = -656.0F;
        sobj->pos.y = 65.0F;
        sMVOpeningPortraitsRow = 1;
        break;

    case 30:
        sobj->pos.x = -656.0F;
        sobj->pos.y = 120.0F;
        sMVOpeningPortraitsRow = 2;
        break;

    case 60:
        sobj->pos.x = -656.0F;
        sobj->pos.y = 175.0F;
        sMVOpeningPortraitsRow = 3;
        break;

    case 105:
        sobj->pos.x = 656.0F;
        sobj->pos.y = 10.0F;
        sMVOpeningPortraitsRow = 0;
        break;

    case 135:
        sobj->pos.x = 656.0F;
        sobj->pos.y = 65.0F;
        sMVOpeningPortraitsRow = 1;
        break;

    case 90:
        sobj->pos.x = 656.0F;
        sobj->pos.y = 120.0F;
        sMVOpeningPortraitsRow = 2;
        break;

    case 120:
        sobj->pos.x = 656.0F;
        sobj->pos.y = 175.0F;
        sMVOpeningPortraitsRow = 3;
        break;
    }
}

/* mvopeningportraits.c:264-284, verbatim. */
void mvOpeningPortraitsMakeCover(void)
{
    GObj *gobj;
    SObj *sobj;

    gobj = gcMakeGObjSPAfter(0, NULL, 18, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, mvOpeningPortraitsCoverProcDisplay, 28, GOBJ_PRIORITY_DEFAULT, ~0);

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMVOpeningPortraitsFiles[0], llMVOpeningPortraitsSet1CoverSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->sprite.red = 0x00;
    sobj->sprite.green = 0x00;
    sobj->sprite.blue = 0x00;

    sobj->pos.x = 656.0F;
    sobj->pos.y = 10.0F;

    gcAddGObjProcess(gobj, mvOpeningPortraitsCoverProcUpdate, nGCProcessKindFunc, 1);
}

/* mvopeningportraits.c:288-306, verbatim: the portraits' own sprite
 * camera, dl-link 27 (mvOpeningPortraitsMakeSet1/Set2's own
 * gcAddGObjDisplay link). */
void mvOpeningPortraitsMakePortraitsCamera(void)
{
    CObj *cobj = CObjGetStruct
    (
        gcMakeCameraGObj
        (
            nGCCommonKindSceneCamera,
            NULL,
            16,
            GOBJ_PRIORITY_DEFAULT,
            lbCommonDrawSprite,
            80,
            COBJ_MASK_DLLINK(27),
            ~0,
            FALSE,
            nGCProcessKindFunc,
            NULL,
            1,
            FALSE
        )
    );
    syRdpSetViewport(&cobj->viewport, 10.0F, 10.0F, 310.0F, 230.0F);
}

/* mvopeningportraits.c:310-328, verbatim: the cover's own camera,
 * dl-link 28, priority 60 -- BELOW the portraits camera (80), so the
 * cover draws first and the portraits' own sprites composite over it
 * where the cover has scrolled clear. */
void mvOpeningPortraitsMakeCoverCamera(void)
{
    CObj *cobj = CObjGetStruct
    (
        gcMakeCameraGObj
        (
            nGCCommonKindSceneCamera,
            NULL,
            16,
            GOBJ_PRIORITY_DEFAULT,
            lbCommonDrawSprite,
            60,
            COBJ_MASK_DLLINK(28),
            ~0,
            FALSE,
            nGCProcessKindFunc,
            NULL,
            1,
            FALSE
        )
    );
    syRdpSetViewport(&cobj->viewport, 10.0F, 10.0F, 310.0F, 230.0F);
}

/* mvopeningportraits.c:332-336, verbatim. */
void mvOpeningPortraitsInitVars(void)
{
    sMVOpeningPortraitsTotalTimeTics = 0;
    sMVOpeningPortraitsRow = 0;
}

/* mvopeningportraits.c:340-378, verbatim: the scene clock, the three
 * exits (skip to title, set-1 -> set-2 at tic 75, hand off to
 * nSCKindOpeningMario at tic 150), and sMVOpeningPortraitsUnused0x801329F4's
 * dead-but-executed decrement/reset (mvopeningportraits.h's closing
 * note). The gcEjectGObj call on sMVOpeningPortraitsGObj is
 * gcEjectGObjIfMade below, the same standing preference every other
 * ported opening scene's own eject calls take even where -- as here --
 * NULL is provably impossible (mvOpeningPortraitsMakeSet1 always runs
 * first, in FuncStart). */
void mvOpeningPortraitsFuncRun(GObj *gobj)
{
    (void)gobj;

    sMVOpeningPortraitsTotalTimeTics++;

    if (sMVOpeningPortraitsTotalTimeTics >= 10)
    {
        if (sMVOpeningPortraitsUnused0x801329F4 != 0)
        {
            sMVOpeningPortraitsUnused0x801329F4--;
        }
        if ((scSubsysControllerGetPlayerStickInRangeLR(-15, 15) != FALSE) &&
            (scSubsysControllerGetPlayerStickInRangeUD(-15, 15) != FALSE))
        {
            sMVOpeningPortraitsUnused0x801329F4 = 0;
        }
        if (scSubsysControllerGetPlayerTapButtons(A_BUTTON | B_BUTTON | START_BUTTON))
        {
            gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
            gSCManagerSceneData.scene_curr = nSCKindTitle;

            syTaskmanSetLoadScene();
        }
        if (sMVOpeningPortraitsTotalTimeTics == 75)
        {
            gcEjectGObjIfMade(sMVOpeningPortraitsGObj);
            mvOpeningPortraitsMakeSet2();
        }
        if (sMVOpeningPortraitsTotalTimeTics == 150)
        {
            gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
            gSCManagerSceneData.scene_curr = nSCKindOpeningMario;

            syTaskmanSetLoadScene();
        }
    }
}

/* Replaces mvopeningportraits.c's LBRelocSetup block and its
 * lbRelocLoadFilesListed call: sprite_bank_load over both entries of
 * dMVOpeningPortraitsFileIDs, the same substitution
 * src/dc/mncharacters.c's own mnCharactersLoadFiles already makes for a
 * multi-file scene. */
static void mvOpeningPortraitsLoadFiles(void)
{
    static const char *const paths[ARRAY_COUNT(dMVOpeningPortraitsFileIDs)] =
    {
        "mvopeningportraitsset1.spr",
        "mvopeningportraitsset2.spr",
    };
    s32 i;

    for (i = 0; i < (s32)ARRAY_COUNT(dMVOpeningPortraitsFileIDs); i++)
    {
        if (sprite_bank_load(&sMVOpeningPortraitsBanks[i], paths[i]) < 0)
        {
            syDebugPrintf("mvOpeningPortraits: no bank for file %d (%s)\n",
                          (int)dMVOpeningPortraitsFileIDs[i], paths[i]);
            continue;
        }
        sMVOpeningPortraitsFiles[i] = &sMVOpeningPortraitsBanks[i];
    }
}

/* mvopeningportraits.c:404-425 mvOpeningPortraitsFuncStart. DIVERGES:
 * the LBRelocSetup block is mvOpeningPortraitsLoadFiles above, and
 * gcMakeDefaultCameraGObj is gone -- mvopeningportraits.h says why this
 * one specifically would have painted over the whole scene, not just
 * confirmed the general rule. The trailing tic-sync busy-wait is
 * sySchedulerWaitTicCount(1335): kept verbatim it would hang, because
 * this port's tic counter only advances inside the per-frame loop
 * FuncStart runs before, so the wait is on the retrace count instead
 * (mvopeningportraits.h gives the reason at length). */
void mvOpeningPortraitsFuncStart(void)
{
    mvOpeningPortraitsLoadFiles();

    gcMakeGObjSPAfter(0, mvOpeningPortraitsFuncRun, 0, GOBJ_PRIORITY_DEFAULT);

    mvOpeningPortraitsInitVars();
    mvOpeningPortraitsMakePortraitsCamera();
    mvOpeningPortraitsMakeCoverCamera();
    mvOpeningPortraitsMakeSet1();
    mvOpeningPortraitsMakeCover();

    /* mvopening*.c: the decomp spins here until the retrace count reaches
     * 1335 (since the Room's BGM start); see sySchedulerWaitTicCount. */
    sySchedulerWaitTicCount(1335);
}

/* mvopeningportraits.c:428-431 mvOpeningPortraitsFuncLights: gSPDisplayList
 * of this file's own two-command lighting list. Dropped with the list
 * itself -- there is no RSP here. */

/* mvopeningportraits.c:436-478 (0x80132954). DIVERGES as every other
 * ported scene's: the arena is the port's region (NULL here, see
 * src/dc/taskman.c), the draw is the scene manager's (scManagerFuncDraw
 * is gcDrawAll, src/dc/mntitle.c's own note -- the decomp's literal
 * gcDrawAll here is the same function under its usual port-side name),
 * the four DL buffer sizes/Graphics Heap Size/RDP Output Buffer Size are
 * zero rather than the decomp's own RSP display-list numbers, the
 * Pre-render function is NULL rather than mvOpeningPortraitsFuncLights,
 * and the Matrix function list is NULL rather than
 * dLBCommonFuncMatrixList -- the port's matrix kinds are a switch in
 * objdisplay.c, not a function list, the same NULL every other scene's
 * setup carries. The pool counts (8 GObjThreads, 128 procs/GObjs, 256
 * XObjs, 512 AObjs, 160 MObjs, 256 DObjs, 128 SObjs, 16 CObjs) are the
 * decomp's own non-placeholder numbers, kept verbatim -- unlike
 * src/dc/mvopeningroom.c's dMVOpeningRoomTaskmanSetup, whose decomp
 * counts really are all zero (a placeholder, mvopeningroom.c's own pools
 * note), this scene's own figures are real and this file makes no
 * gcAddGObjProcess(..., nGCProcessKindThread, ...) call at all -- the
 * eight reserved GObjThreads simply sit unused, the same safe
 * over-provisioning src/dc/mnstartup.h's GObjThreads note already
 * established works in the other direction (a pool count is a size, not
 * a ceiling, either way). */
SYTaskmanSetup dMVOpeningPortraitsTaskmanSetup =
{
    {
        0,                              /* flags */
        gcRunAll,                       /* update function */
        scManagerFuncDraw,              /* frame draw function */
        NULL,                           /* allocatable memory pool start */
        0,                              /* allocatable memory pool size */
        1,                              /* ??? */
        2,                              /* number of contexts? */
        0, 0, 0, 0,                     /* the four DL buffer sizes */
        0,                              /* graphics heap size */
        2,                              /* ??? */
        0,                              /* RDP output buffer size */
        NULL,                           /* pre-render function */
        syControllerFuncRead,           /* controller I/O function */
    },

    8,                                  /* number of GObjThreads */
    sizeof(u64) * 192,                  /* thread stack size */
    8,                                  /* number of thread stacks */
    0,                                  /* ??? */
    128,                                /* number of GObjProcesses */
    128,                                /* number of GObjs */
    sizeof(GObj),                       /* GObj size */
    256,                                /* number of XObjs */
    NULL,                               /* matrix function list */
    NULL,                               /* DObjVec eject function */
    512,                                /* number of AObjs */
    160,                                /* number of MObjs */
    256,                                /* number of DObjs */
    sizeof(DObj),                       /* DObj size */
    128,                                /* number of SObjs */
    sizeof(SObj),                       /* SObj size */
    16,                                 /* number of CObjs */
    sizeof(CObj),                       /* CObj size */

    mvOpeningPortraitsFuncStart         /* task start function */
};

/* The port's own bzero arm for dSCManagerOverlays[35] (src/dc/overlay.c),
 * the same role mvOpeningRoomOverlayLoad plays for overlay 34. */
void mvOpeningPortraitsOverlayLoad(void)
{
    OVERLAY_CLEAR(sMVOpeningPortraitsTotalTimeTics);
    OVERLAY_CLEAR(sMVOpeningPortraitsRow);
    OVERLAY_CLEAR(sMVOpeningPortraitsGObj);
    OVERLAY_CLEAR(sMVOpeningPortraitsUnused0x801329F4);
    OVERLAY_CLEAR(sMVOpeningPortraitsFiles);
    OVERLAY_CLEAR(sMVOpeningPortraitsBanks);
}

/* mvopeningportraits.c:481-485 mvOpeningPortraitsStartScene. DIVERGES:
 * syVideoInit, the zbuffer setup and the arena_size line are the N64's
 * video mode and its link map, set once at boot in this port
 * (src/game/ssb64/main.c) rather than per scene. */
void mvOpeningPortraitsStartScene(void)
{
    syTaskmanStartTask(&dMVOpeningPortraitsTaskmanSetup);
}

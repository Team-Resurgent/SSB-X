/* scstaffroll.c -- see scstaffroll.h. Function-for-function from
 * ssb-decomp-re/src/sc/sccommon/scstaffroll.c, REGION_US arms; every
 * function names its line range. The DIVERGES (the credit text and the
 * staff-role/company popup text are real; the scrolling name/job
 * glyphs and the rotating hit backdrop are the one guarded cut) are
 * explained once, in scstaffroll.h's own header comment -- not
 * repeated at every site.
 */
#include "scstaffroll.h"
#include "overlay.h"

#include "fighter.h" /* fighter_load, fighter_release, fighter_set_prim_color */
#include "ftcommon.h" /* func_800269C0_275C0 */
#include "gmcamera.h"
#include "objmodel.h" /* dc_model_add_dobjs, dc_model_init_payload, dc_model_graft_display */
#include "lbcommon.h" /* lbCommonDrawSObjAttr, lbCommonMakeSObjForGObj, lbCommonSpriteFillRect */
#include "objpvr.h" /* gcGetDrawList, PVR_LIST_TR_POLY */
#include "scmanager.h"
#include "sprite.h"
#include "lbpdraw.h"
#include "taskman.h"

#include <gm/gmdef.h>
#include <gm/gmsound.h>
#include <sys/controller.h>
#include <sys/interp.h>
#include <sys/objanim.h> /* gcAddDObjAnimJoint, gcPlayAnimAll */
#include <sys/objdef.h> /* aobjEvent32SetValBlock, AOBJ_FLAG_ROTZ */
#include <sys/rdp.h>
#include <lb/lbdef.h>

/* scstaffroll.c:15-19 dSCStaffrollNameCharacters, scstaffroll.c:21-25
 * dSCStaffrollNameTextInfo, and the three matching (Job/StaffRole/
 * Company) pairs below: real, compiled-in credit text, generated at
 * build time by ssb-decomp-re's own tools/creditsTextConverter.py off
 * its own src/credits/*.us.txt -- see scstaffroll.h's header note. */
s32 dSCStaffrollNameCharacters[] =
{
    #include "credits/staff.credits.encoded"
};

SCStaffrollText dSCStaffrollNameTextInfo[] =
{
    #include "credits/staff.credits.metadata"
};

/* scstaffroll.c:39-182 dSCStaffrollJobDescriptions, verbatim (REGION_US
 * arm): the job-title order the scroll walks (scStaffrollScrollThread
 * Update), each entry naming how many staff members roll under it
 * before the next job heading shows. */
SCStaffrollJob dSCStaffrollJobDescriptions[] =
{
    /* Director */
    { -1, 0, 1 },

    /* Chief Programmer */
    { 1, 16, 2 },

    /* Programmers */
    { -1, 2, 7 },

    /* Chief Designer */
    { 1, 17, 8 },

    /* Designers */
    { -1, 3, 27 },

    /* Sound Composer */
    { -1, 4, 28 },

    /* Support */
    { -1, 5, 29 },

    /* Voice Actors */
    { -1, 6, 38 },

    /* Original Game Staff */
    { -1, 8, 56 },

    /* NOA Staff */
    { -1, 15, 59 },

    /* Special Thanks */
    { -1, 7, 75 },

    /* Project Manager */
    { 9, 10, 76 },

    /* Progress Manager */
    { 11, 10, 77 },

    /* Producer */
    { -1, 12, 81 },

    /* Executive Producer */
    { 13, 12, 82 },

    /* Presents */
    { -1, 14, -1 }
};

s32 dSCStaffrollJobCharacters[] =
{
    #include "credits/titles.credits.encoded"
};

SCStaffrollText dSCStaffrollJobTextInfo[] =
{
    #include "credits/titles.credits.metadata"
};

s32 dSCStaffrollStaffRoleCharacters[] =
{
    #include "credits/info.credits.encoded"
};

SCStaffrollText dSCStaffrollStaffRoleTextInfo[] =
{
    #include "credits/info.credits.metadata"
};

s32 dSCStaffrollCompanyCharacters[] =
{
    #include "credits/companies.credits.encoded"
};

SCStaffrollText dSCStaffrollCompanyTextInfo[] =
{
    #include "credits/companies.credits.metadata"
};

/* scstaffroll.c:228-326 dSCStaffrollCompanyIDs, verbatim (REGION_US
 * arm): one SCStaffrollCompany per staff-role entry, read by
 * scStaffrollMakeCompanyTextSObjs to decide whether (and which)
 * company name to print under a staff member's role. */
SCStaffrollCompany dSCStaffrollCompanyIDs[] =
{
    nSCStaffrollCompanyHAL, nSCStaffrollCompanyHAL, nSCStaffrollCompanyHAL,
    nSCStaffrollCompanyHAL, nSCStaffrollCompanyHAL, nSCStaffrollCompanyHAL,
    nSCStaffrollCompanyHAL, nSCStaffrollCompanyHAL, nSCStaffrollCompanyHAL,
    nSCStaffrollCompanyHAL, nSCStaffrollCompanyHAL, nSCStaffrollCompanyHAL,
    nSCStaffrollCompanyHAL, nSCStaffrollCompanyHAL, nSCStaffrollCompanyHAL,
    nSCStaffrollCompanyHAL, nSCStaffrollCompanyHAL, nSCStaffrollCompanyHAL,
    nSCStaffrollCompanyHAL,
    nSCStaffrollCompanyNull, nSCStaffrollCompanyNull, nSCStaffrollCompanyNull,
    nSCStaffrollCompanyNull, nSCStaffrollCompanyNull,
    nSCStaffrollCompanyCreatures, nSCStaffrollCompanyCreatures, nSCStaffrollCompanyCreatures,
    nSCStaffrollCompanyHAL, nSCStaffrollCompanyHAL,
    nSCStaffrollCompanyNull,
    nSCStaffrollCompanyARTSVISION,
    nSCStaffrollCompanyNINTENDO,
    nSCStaffrollCompanyAONIProd, nSCStaffrollCompanyAONIProd,
    nSCStaffrollCompanyEZAKIProd,
    nSCStaffrollCompanyAONIProd,
    nSCStaffrollCompanyNull,               /* REGION_US: KENProd (JP) */
    nSCStaffrollCompanyMickeys,
    nSCStaffrollCompanyNINTENDO, nSCStaffrollCompanyNINTENDO,
    nSCStaffrollCompanyNINTENDO, nSCStaffrollCompanyNINTENDO,
    nSCStaffrollCompanyNull,               /* REGION_US: Rare (JP) */
    nSCStaffrollCompanyNINTENDO, nSCStaffrollCompanyNINTENDO,
    nSCStaffrollCompanyNINTENDO,
    nSCStaffrollCompanyHAL,
    nSCStaffrollCompanyNINTENDO, nSCStaffrollCompanyNINTENDO,
    nSCStaffrollCompanyGAMEFREAK,
    nSCStaffrollCompanyCreatures,
    nSCStaffrollCompanyNINTENDO,
    nSCStaffrollCompanyHAL,
    nSCStaffrollCompanyNINTENDO,
    nSCStaffrollCompanyCreatures,
    nSCStaffrollCompanyNull,
    nSCStaffrollCompanyNOA, nSCStaffrollCompanyNOA, nSCStaffrollCompanyNOA,
    nSCStaffrollCompanyHAL, nSCStaffrollCompanyHAL, nSCStaffrollCompanyHAL,
    nSCStaffrollCompanyHAL, nSCStaffrollCompanyHAL, nSCStaffrollCompanyHAL,
    nSCStaffrollCompanyHAL,
    nSCStaffrollCompanyNINTENDO, nSCStaffrollCompanyNINTENDO,
    nSCStaffrollCompanyCreatures,
    nSCStaffrollCompanyNINTENDO, nSCStaffrollCompanyNINTENDO, nSCStaffrollCompanyNINTENDO,
    nSCStaffrollCompanyNull, nSCStaffrollCompanyNull,
    nSCStaffrollCompanyNull,               /* REGION_US-only extra entry */
    nSCStaffrollCompanyHAL,
    nSCStaffrollCompanyNINTENDO,
    nSCStaffrollCompanyHAL, nSCStaffrollCompanyHAL,
    nSCStaffrollCompanyNINTENDO, nSCStaffrollCompanyNINTENDO, nSCStaffrollCompanyNINTENDO,
    nSCStaffrollCompanyNull, nSCStaffrollCompanyNull
};

/* scstaffroll.c:332-392 dSCStaffrollNameAndJobSpriteInfo, DIVERGES: the
 * width/height are verbatim (they drive scStaffrollMakeJobDObjs/
 * MakeNameGObjAndDObjs's own layout arithmetic, and now also the quads
 * tools/export/ssb_staffrollexport.py bakes, which are the same four Vtx from
 * the same two numbers). The offset stays 0 for every entry: these are
 * not libultra Sprite records (ssb_spriteexport.py's own --file 195
 * --list does not catalogue them, confirmed), they are raw I4 texture
 * blocks, and the exporter reads them out of the ROM by the decomp's
 * own &llSCStaffrollXxx link labels at build time -- so no run-time
 * code dereferences this field. The ROW INDEX is what matters here and
 * is what the port carries instead: glyph i of this table is joint i of
 * scglyphs.mdl, which is the same identity the decomp's own
 * sSCStaffrollNameAndJobDisplayLists[i] has. */
SCStaffrollSprite dSCStaffrollNameAndJobSpriteInfo[] =
{
    { 20, 22, 0 }, { 15, 22, 0 }, { 15, 22, 0 }, { 18, 22, 0 }, { 13, 22, 0 },
    { 13, 22, 0 }, { 19, 22, 0 }, { 18, 22, 0 }, {  7, 22, 0 }, { 11, 22, 0 },
    { 18, 22, 0 }, { 13, 22, 0 }, { 23, 22, 0 }, { 19, 22, 0 }, { 22, 22, 0 },
    { 15, 22, 0 }, { 23, 22, 0 }, { 16, 22, 0 }, { 15, 22, 0 }, { 15, 22, 0 },
    { 16, 22, 0 }, { 18, 22, 0 }, { 25, 22, 0 }, { 19, 22, 0 }, { 16, 22, 0 },
    { 19, 22, 0 },
    { 14, 18, 0 }, { 16, 22, 0 }, { 14, 18, 0 }, { 16, 22, 0 }, { 16, 18, 0 },
    { 11, 22, 0 }, { 15, 22, 0 }, { 15, 22, 0 }, {  6, 22, 0 }, {  8, 26, 0 },
    { 16, 22, 0 }, {  6, 22, 0 }, { 20, 18, 0 }, { 15, 18, 0 }, { 18, 18, 0 },
    { 16, 22, 0 }, { 16, 22, 0 }, { 11, 18, 0 }, { 13, 18, 0 }, { 11, 22, 0 },
    { 15, 18, 0 }, { 14, 18, 0 }, { 21, 18, 0 }, { 15, 18, 0 }, { 16, 22, 0 },
    { 14, 17, 0 },
    {  7,  7, 0 }, {  8, 11, 0 }, {  8, 11, 0 },
    { 16, 22, 0 }                          /* REGION_US: '4' */
};

/* scstaffroll.c:395-473 dSCStaffrollTextBoxSpriteInfo, DIVERGES only in
 * how the offset is written: the decomp's own &llSCStaffrollTextBoxXxx
 * Sprite link labels are relocData 195's own linker symbols, which
 * this port does not link (it exports the file's sprites into a bank
 * instead) -- so each entry's offset here is the literal number
 * tools/export/ssb_spriteexport.py --file 195 --list itself reports for that
 * same symbol, the key scstaffrollgraphics.spr's own sprite_bank_get
 * looks a sprite up by (the tool's own docstring: "it is also the key
 * the port's sprite_bank_get looks a sprite up by"). Width/height are
 * verbatim. The file keeps each letter's upper and lower case side by
 * side (AUpper 0x03258, ALower 0x03310, BUpper 0x033e8, ...); until
 * 2026-09-24 this table read them as 26 capitals and then 26 small
 * letters, so every small letter, digit, the colon and the period in
 * the popup drew another glyph. Each sprite's own width in the file
 * matches the width beside it here but capital Z's, which the game's
 * table advances by 14 over a sprite 12 wide (hosttest/text.c holds
 * that). */
SCStaffrollSprite dSCStaffrollTextBoxSpriteInfo[] =
{
    /* A-Z */
    { 12, 14, 0x03258 }, { 12, 14, 0x033e8 }, { 12, 14, 0x03588 }, { 12, 14, 0x03718 },
    { 12, 14, 0x038b8 }, { 12, 14, 0x03a48 }, { 12, 14, 0x03be8 }, { 12, 14, 0x03d78 },
    {  5, 14, 0x03f18 }, { 12, 14, 0x040b8 }, { 12, 14, 0x04258 }, { 12, 14, 0x043f8 },
    { 14, 14, 0x04598 }, { 12, 14, 0x04728 }, { 12, 14, 0x048b8 }, { 12, 14, 0x04a48 },
    { 13, 14, 0x04bd8 }, { 12, 14, 0x04d68 }, { 12, 14, 0x04ef8 }, { 13, 14, 0x05088 },
    { 12, 14, 0x05228 }, { 14, 14, 0x053b8 }, { 14, 14, 0x05548 }, { 12, 14, 0x056d8 },
    { 13, 14, 0x05868 }, { 14, 14, 0x059f8 },
    /* a-z */
    { 10, 11, 0x03310 }, { 10, 13, 0x034b0 }, { 10, 11, 0x03640 }, { 10, 13, 0x037e0 },
    { 10, 11, 0x03970 }, {  9, 13, 0x03b10 }, { 10, 12, 0x03ca8 }, { 10, 13, 0x03e40 },
    {  4, 13, 0x03fe0 }, {  6, 14, 0x04188 }, { 10, 13, 0x04320 }, {  4, 13, 0x044c0 },
    { 12, 11, 0x04650 }, { 10, 11, 0x047e0 }, { 10, 11, 0x04970 }, { 10, 12, 0x04b08 },
    { 10, 12, 0x04c98 }, {  9, 11, 0x04e20 }, { 10, 11, 0x04fb0 }, {  9, 13, 0x05150 },
    { 10, 11, 0x052e0 }, { 10, 11, 0x05470 }, { 12, 11, 0x05600 }, { 12, 11, 0x05790 },
    { 10, 12, 0x05928 }, { 10, 11, 0x05ab0 },
    /* colon */
    {  5, 11, 0x05b70 },
    /* 9 down to 0, the decomp's order */
    { 13, 14, 0x06468 }, { 13, 14, 0x06398 }, { 13, 14, 0x062c8 }, { 13, 14, 0x061f8 },
    { 13, 14, 0x06128 }, { 13, 14, 0x06058 }, { 13, 14, 0x05f88 }, { 13, 14, 0x05eb8 },
    {  9, 14, 0x05de8 }, { 13, 14, 0x06538 },
    /* . - , & " / ' ? ( ) */
    {  5,  5, 0x05c90 }, {  9,  4, 0x05d18 }, {  5,  5, 0x05c00 }, { 16, 14, 0x06698 },
    {  5,  5, 0x065c0 }, {  6, 12, 0x06758 }, {  5,  5, 0x067e0 }, { 12, 14, 0x068b8 },
    {  7, 14, 0x06988 }, {  7, 14, 0x06a58 },
    { 10, 13, 0x06b20 }                    /* REGION_US: e-accent */
};

/* scstaffroll.c:476-487 dSCStaffrollTextBoxDisplayList: DIVERGES. The
 * decomp's four gDPFillRectangle strokes (the side textbox's own frame)
 * have no relocData dependency, so a verbatim Gfx table looked safe --
 * it is not. gcAddDObjForGObj's second argument becomes the DObj's own
 * `dv` (objman.c:1389, `new_dobj->dv = dvar;`), the same union member
 * gcSubmitDObj calls through as a DCDisplay* (standin_check.py's own
 * "payload" rule); a raw Gfx* there is not a DCDisplay* on this target
 * no matter how the bytes inside it were made -- confirmed on a real
 * disc probe: PC e7000004, gsDPPipeSync's own first word, executed as
 * an SH4 instruction from inside gcDrawDObjTree. itemModelSetDisplayList
 * (src/dc/itemmodel.c) draws the same lesson: `dobj->dl = dl` is a
 * host-only, FT_HOSTTEST-guarded arm, never the target's. The target
 * arm here is what ifScreenFlashProcDisplay/lbFadeProcDisplay
 * (src/dc/ifscreenflash.c, src/dc/lbfade.c) already do for a flat
 * fill-rectangle quad -- lbCommonSpriteFillRect from a plain ProcDisplay,
 * no DObj at all -- so scStaffrollTextBoxProcDisplay below replaces this
 * table with four calls to it at the same four rectangles and the same
 * fill colour (0x42, 0x3A, 0x31), full alpha for the 5551 fill's own
 * alpha bit. */

/* scstaffroll.c:2204-2212 dSCStaffrollLights1/dSCStaffrollDisplayList,
 * the lighting the pre-render function would set for the 3D this scene
 * never draws (the same reasoning mnoption.c's own dMNOptionTaskmanSetup
 * and nine other menu-shaped files across this port already give for
 * dropping this exact table -- none of them keep a static Gfx array
 * built with gsSPSetLights1, because on this port's own host build
 * (test-host, plain gcc, no MIPS/SH-4 linker relocations for an
 * address packed into a bitfield of a 64-bit static initializer)
 * "initializer element is not constant" is a hard error, not just a
 * target/host difference to shrug off). Here it is doubly moot: this
 * step's own real cut (scstaffroll.h) means no DObj this file makes
 * ever carries real XObj geometry, so no lit polygon is ever drawn
 * regardless. Dropped with scStaffrollFuncLights; func_lights is NULL
 * below, the same as dMNOptionTaskmanSetup's own. */

/* relocData/195_SCStaffroll.c:3293-3345, the curve every credit line
 * rides and the tilt it rides it at: dSCStaffroll_Points_0x7228 (eight
 * Bezier control points), _Keyframes_0x7288, _Quartics_0x72A0, the
 * SYInterpDesc at 0x7304 over them and the two-command AnimJoint at
 * 0x7338. They are compiled in
 * here rather than read from file 195's generic reloc data, the same "already-decoded readable C, not bytes to
 * export" call scexplain.c's own relocData 252 KeyEvent tables make
 * (scexplain.h). Nothing in them is a pointer into the file -- the
 * three arrays are plain floats and the script's two values are plain
 * f32 words -- so there is no relocation to carry and no reason for a
 * pack: tools/export/ssb_staffrollexport.py bakes the GEOMETRY of file 195,
 * and this is not geometry.
 *
 * The script's own words are written with the decomp's own
 * aobjEvent32* macros (sys/objdef.h), so they come out bit-for-bit what
 * the ROM holds at 0x7338 -- the same words sys/objanim.c already
 * parses out of every pack this port ships (src/dc/stage.c's
 * stage_map_anim, and grsector.c's own Arwing scripts through it). */
Vec3f dSCStaffrollNamePoints[8] =
{
    { -280.82037F,  -159.58678F,  605.7593F   },
    {  -98.54599F,  -126.8449F,   327.375F    },
    {   83.7284F,    -94.10303F,   48.990753F },
    {   58.747913F,    2.629765F, -212.3352F  },
    { -178.79285F,    72.95225F,  -405.5F     },
    { -458.7232F,    139.82056F,  -524.78125F },
    { -871.74115F,   205.81422F,  -641.71875F },
    { -1284.759F,    271.8079F,   -758.65625F }
};

f32 dSCStaffrollNameKeyframes[6] =
{ 0.0F, 0.194292F, 0.362257F, 0.543994F, 0.746575F, 1.0F };

f32 dSCStaffrollNameQuartics[25] =
{
    184.92339F,  -0.000017F, -631.7313F,   -0.000222F, 1746.7778F,
     42.234528F, -59.586906F, 426.25696F, -523.7697F,  1299.9694F,
    115.30385F, -568.55865F,  450.089F,    318.92166F, 1185.1042F,
     52.151268F, -22.654272F, 657.61707F,  -25.361145F, 1500.8602F,
     69.2132F,  -276.85266F, -438.4054F,  1430.5151F,  2162.613F
};

SYInterpDesc dSCStaffrollNameInterpolationDesc[1] =
{
    {
        nSYInterpKindBezier, 6, 0.0F, dSCStaffrollNamePoints, 203.93144F,
        dSCStaffrollNameKeyframes, dSCStaffrollNameQuartics
    }
};

/* SetValBlock(ROTZ) -0.190F at frame 0, +0.4189F at frame 99, End --
 * the lean a line takes on as it rides the curve away from the camera.
 * A u32 array cast to AObjEvent32 * where it is handed over, which is
 * exactly how a pack's own script words reach the parser today
 * (src/dc/objmodel.c, src/dc/stage.c), and which keeps the two f32
 * values as the decomp's own words rather than as whatever this
 * compiler's decimal parse of the printed constant would be. */
u32 dSCStaffrollNameAnimJointScript[5] =
{
    aobjEvent32SetValBlock(AOBJ_FLAG_ROTZ, 0),  0xBE427301, /* -0.190F */
    aobjEvent32SetValBlock(AOBJ_FLAG_ROTZ, 99), 0x3ED67750, /*  0.4189F */
    aobjEvent32End()
};

/* ---- state -------------------------------------------------------- */

/* The port's own sprite bank for relocData 195,
 * tools/export/ssb_spriteexport.py --file 195 -- see scstaffroll.h's
 * header note. The one call site that needs a Sprite* out of it (below)
 * reads through this local getter instead of a redefined
 * lbRelocGetFileData, since -- unlike scexplain.c's own file 198 -- 195
 * also carries the DObjDesc/AnimJoint/Interpolation "name plaque" data
 * this step does not port (a blanket lbRelocGetFileData redefinition
 * would be wrong for those, if they were ever added later). */
static SpriteBank sSCStaffrollGraphicsBank;
static SpriteBank *sSCStaffrollGraphicsFileHead;

#define STAFFROLL_SPRITE(offset) \
    ((Sprite *) sprite_bank_get(sSCStaffrollGraphicsFileHead, (u32)(offset)))

/* the packs tools/export/ssb_staffrollexport.py bakes out of
 * relocData 195, and the payloads over them.
 *
 * scglyphs0/1.mdl are the 56 credit glyphs, one pack JOINT each: the
 * same four Vtx and two triangles scStaffrollInitNameAndJobDisplayLists
 * builds at run time on the N64, from the same width/height, with the
 * file's own raw I4 block as the tile. They come in two files of 28
 * because src/dc/fighter.h's own FIGHTER_MAX_JOINTS is 40 and
 * fighter_init refuses a pack over it outright -- the failure a first
 * disc probe of this step hit, one line of serial ("blob is not a pack
 * this build handles") and nothing drawn. Glyph i lives at joint
 * i % SCSTAFFROLL_GLYPH_SPLIT of pack i / SCSTAFFROLL_GLYPH_SPLIT, and
 * nothing else in the file has to know: the payload table below is
 * still one entry per glyph in the decomp's own order.
 *
 * scplaque.mdl is the two-joint DObjDesc at file offset 0x78C0 -- the
 * backdrop func_ovl59_8013202C spreads behind a shot name.
 *
 * sSCStaffrollNameAndJobDisplayLists keeps the decomp's own name and
 * its own one-per-glyph identity; what it holds is a DCDisplay per
 * glyph rather than a Gfx *, since that is what a DObj's `dv` is on
 * this target (scstaffroll.h's own fifth DIVERGES, the crash a raw
 * Gfx * there already caused once). dc_model_init_payload cuts them,
 * and src/dc/itemmodel.c's own alt->disp is the same "a payload in
 * storage the caller owns" call. */
#define SCSTAFFROLL_GLYPH_SPLIT 28
#define SCSTAFFROLL_GLYPH_PACKS 2

static Fighter sSCStaffrollGlyphPack[SCSTAFFROLL_GLYPH_PACKS];
static Fighter sSCStaffrollPlaquePack;
static Fighter sSCStaffrollPlaqueModel;
static sb32 sSCStaffrollGlyphsLoaded;
static sb32 sSCStaffrollPlaqueLoaded;
static DCDisplay sSCStaffrollNameAndJobDisplayLists[ARRAY_COUNT(dSCStaffrollNameAndJobSpriteInfo)];
static s32 sSCStaffrollNameID;
static f32 sSCStaffrollRollSpeed;
static s32 sSCStaffrollStatus;
static SCStaffrollName *sSCStaffrollNameAllocFree;
static GObj *sSCStaffrollScrollGObj;
static GObj *sSCStaffrollCrosshairGObj;
static sb32 sSCStaffrollIsPaused;
static f32 sSCStaffrollCrosshairPositionX;
static f32 sSCStaffrollCrosshairPositionY;
static AObjEvent32 *sSCStaffrollNameAnimJoint;
static SYInterpDesc *sSCStaffrollNameInterpolation;
static CObj *sSCStaffrollCamera;
static s32 sSCStaffrollHighlightSize;
static f32 sSCStaffrollHighlightPositionX;
static f32 sSCStaffrollHighlightPositionY;
static GObj *sSCStaffrollStaffRoleTextGObj;
static GObj *sSCStaffrollCompanyTextGObj;
static s32 sSCStaffrollRollBeginWait;
static u8 sSCStaffrollPlayer;
static s32 sSCStaffrollRollEndWait;
static Mtx44f sSCStaffrollMatrix;

// // // // // // // // // // // //
//                               //
//           FUNCTIONS           //
//                               //
// // // // // // // // // // // //

/* scstaffroll.c:584-617 scStaffrollGetPauseStatusResume, verbatim: real
 * controller input (gSYControllerDevices, the same subsystem every
 * already-ported scene's own input reads). */
sb32 scStaffrollGetPauseStatusResume(void)
{
    sb32 is_paused = TRUE;
    u16 button_tap = gSYControllerDevices[sSCStaffrollPlayer].button_tap;

    if (button_tap & (A_BUTTON | B_BUTTON | Z_TRIG | START_BUTTON))
    {
        GObj *name_gobj;
        GObj *job_gobj;

        if (sSCStaffrollScrollGObj != NULL)
        {
            gcResumeGObjProcessAll(sSCStaffrollScrollGObj);
        }
        name_gobj = gGCCommonLinks[3];

        while (name_gobj != NULL)
        {
            gcResumeGObjProcessAll(name_gobj);

            name_gobj = name_gobj->link_next;
        }
        job_gobj = gGCCommonLinks[4];

        while (job_gobj != NULL)
        {
            gcResumeGObjProcessAll(job_gobj);

            job_gobj = job_gobj->link_next;
        }
        is_paused = FALSE;
    }
    return is_paused;
}

/* scstaffroll.c:620-634 func_ovl59_80131BB0, verbatim: project a local
 * point through sSCStaffrollMatrix to screen space. Real -- see
 * scstaffroll.h's header note on why this whole hit-test chain runs
 * without the glyph rendering it would otherwise sit behind. */
void func_ovl59_80131BB0(Mtx44f mtx, Vec3f *vec, f32 *width, f32 *height)
{
    f32 x = vec->x;
    f32 y = vec->y;
    f32 z = vec->z;

    f32 w = (mtx[0][0] * x) + (mtx[1][0] * y) + (mtx[2][0] * z) + mtx[3][0];
    f32 h = (mtx[0][1] * x) + (mtx[1][1] * y) + (mtx[2][1] * z) + mtx[3][1];
    f32 i = (mtx[0][3] * x) + (mtx[1][3] * y) + (mtx[2][3] * z) + mtx[3][3];

    i = (1.0F / i);

    *width = w * i * 640.0F * 0.5F;
    *height = h * i * 480.0F * 0.5F;
}

/* scstaffroll.c:637-644 func_ovl59_80131C88, verbatim: the credits
 * camera's own view*persp matrix, cached for the hit test below. */
void func_ovl59_80131C88(CObj *cobj)
{
    Mtx44f m, n;

    syMatrixPerspFastF(n, &cobj->projection.persp.norm, cobj->projection.persp.fovy, cobj->projection.persp.aspect, cobj->projection.persp.near, cobj->projection.persp.far, cobj->projection.persp.scale);
    syMatrixLookAtF(&m, cobj->vec.eye.x, cobj->vec.eye.y, cobj->vec.eye.z, cobj->vec.at.x, cobj->vec.at.y, cobj->vec.at.z, cobj->vec.up.x, cobj->vec.up.y, cobj->vec.up.z);
    guMtxCatF(m, n, sSCStaffrollMatrix);
}

/* scstaffroll.c:647-666 func_ovl59_80131D30, verbatim: a name/job
 * entry's own DObj transform composed with the credits matrix. */
void func_ovl59_80131D30(DObj *dobj, Vec3f *vec, f32 *width, f32 *height)
{
    Mtx44f m, r;

    syMatrixTraRotRpyRScaF
    (
        &m,
        dobj->translate.vec.f.x,
        dobj->translate.vec.f.y,
        dobj->translate.vec.f.z,
        dobj->rotate.vec.f.x,
        dobj->rotate.vec.f.y,
        dobj->rotate.vec.f.z,
        dobj->scale.vec.f.x,
        dobj->scale.vec.f.y,
        dobj->scale.vec.f.z
    );
    guMtxCatF(m, sSCStaffrollMatrix, r);
    func_ovl59_80131BB0(r, vec, width, height);
}

/* scstaffroll.c:669-680 func_ovl59_80131DD0, verbatim, unchanged
 * struct-aliasing and all: SCStaffrollMatrix and SCStaffrollName share
 * one field layout at offsets 0xC/0x10 (sc/sctypes.h -- offset_x and
 * unkgmcreditsstruct0x10 on the one side, unk_gmcreditsmtx_0xC/0x10 on
 * the other), which scStaffrollJobAndNameInitStruct below sets whether
 * or not the glyph children are attached -- so this reads real data. */
void func_ovl59_80131DD0(GObj *gobj, SCStaffrollProjection *proj)
{
    SCStaffrollMatrix *credits = gobj->user_data.p;

    proj->pv0.z = proj->pv1.z = proj->pv2.z = proj->pv3.z = 0.0F;
    proj->pv0.y = proj->pv2.y = 28.0F;

    proj->pv1.y = proj->pv3.y = -(credits->unk_gmcreditsmtx_0x10 + 4.0F);
    proj->pv0.x = proj->pv1.x = -22.0F;

    proj->pv2.x = proj->pv3.x = (ABS(credits->unk_gmcreditsmtx_0xC) * 2) + 18.0F;
}

/* scstaffroll.c:683-688 func_ovl59_80131E70, verbatim. */
void func_ovl59_80131E70(Vec3f *arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4)
{
    arg0->x = arg2 - arg4;
    arg0->y = arg3 - arg1;
    arg0->z = (arg1 * arg4) - (arg3 * arg2);
}

/* scstaffroll.c:691-704 scStaffrollCheckCursorNameOverlap, verbatim:
 * the crosshair's own real SObj position (scStaffrollCrosshairThread
 * Update below writes it every frame) against one edge of a projected
 * quad. */
sb32 scStaffrollCheckCursorNameOverlap(Vec3f *vec)
{
    SObj *sobj = SObjGetStruct(sSCStaffrollCrosshairGObj);
    f32 x = (sobj->pos.x + 29.0F) - 320.0F;
    f32 y = 240.0F - (sobj->pos.y + 29.0F);
    f32 v = (vec->x * x) + (vec->y * y) + vec->z;
    sb32 ret = TRUE;

    if (v < 0.0F)
    {
        ret = FALSE;
    }
    return ret;
}

/* The plaque's own proc_display. The decomp hands gcAddGObjDisplay
 * gcDrawDObjTreeForGObj, whose one line here is dc_model_draw_tree --
 * the walk records matrices and the pack is submitted after it, the
 * same one-line substitution src/dc/ifcommon.c's own off-screen arrows
 * make (objmodel.h). It gates itself on the translucent pass. */
void scStaffrollPlaqueProcDisplay(GObj *gobj)
{
#ifndef FT_HOSTTEST
    dc_model_draw_tree(gobj);
#else
    (void)gobj;
#endif
}

/* scstaffroll.c:707-727 func_ovl59_80131F34, verbatim. */
void func_ovl59_80131F34(GObj *arg0)
{
    GObj *ugobj = arg0->user_data.p;
    SCStaffrollMatrix *credits = ugobj->user_data.p;

    if ((credits->unk_gmcreditsmtx_0x14 + sSCStaffrollRollSpeed) >= 1.0F)
    {
        gcEjectGObj(NULL);
        gcEjectGObj(sSCStaffrollStaffRoleTextGObj);
        gcEjectGObj(sSCStaffrollCompanyTextGObj);
    }
    else
    {
        DObj *dobj = DObjGetStruct(arg0)->child;

        DObjGetStruct(arg0)->translate.vec.f = DObjGetStruct(ugobj)->translate.vec.f;
        DObjGetStruct(arg0)->rotate.vec.f = DObjGetStruct(ugobj)->rotate.vec.f;

        dobj->translate.vec.f.x = (ABS(credits->unk_gmcreditsmtx_0xC) * 2) + 18.0F;
    }
}

/* scstaffroll.c:730-750 func_ovl59_8013202C -- the 3D backdrop behind a
 * shot name. DIVERGES only in where its two DObjs come from:
 * gcSetupCustomDObjs over sSCStaffrollDObjDesc (relocData 195's own
 * DObjDesc at file offset 0x78C0) becomes dc_model_add_dobjs over
 * scplaque.mdl, the tree/matrix-kind substitution src/dc/mnplayersvs.c's
 * own spotlight first made and scexplain.c's own control stick repeats.
 * The tree it makes is the same two joints in the same parent/child
 * order, which is what func_ovl59_80131F34 above walks
 * (DObjGetStruct(arg0)->child). A pack that failed to load makes no
 * GObj at all and the highlight simply has no backdrop, the same NULL
 * guard every other pack read carries. */
void func_ovl59_8013202C(GObj *arg0)
{
    GObj *gobj = gGCCommonLinks[nGCCommonLinkID02];
    GObj *ugobj = arg0->user_data.p;

    if (gobj == NULL)
    {
#ifndef FT_HOSTTEST
        if (!sSCStaffrollPlaqueLoaded)
        {
            return;
        }
#endif
        gobj = gcMakeGObjSPAfter(8, NULL, nGCCommonLinkID02, GOBJ_PRIORITY_DEFAULT);

        gcAddGObjDisplay(gobj, scStaffrollPlaqueProcDisplay, 3, GOBJ_PRIORITY_DEFAULT, ~0);

#ifndef FT_HOSTTEST
        if (dc_model_add_dobjs(gobj, NULL, &sSCStaffrollPlaqueModel, NULL) <= 0)
        {
            gcEjectGObj(gobj);

            return;
        }
#else
        /* As scexplain.c's own control stick: the host cross-test links
         * the scene and not the renderer, and func_ovl59_80131F34 above
         * still needs a root DObj and one child to place. */
        gcAddDObjRpyR(gobj, NULL);
        gcAddDObjRpyR(DObjGetStruct(gobj), NULL);
#endif
        gcAddGObjProcess(gobj, func_ovl59_80131F34, nGCProcessKindFunc, 1);

        gobj->user_data.p = arg0;
        ugobj->unk_gobj_0x1C = gobj;
    }
    else
    {
        gobj->user_data.p = arg0;
        ugobj->unk_gobj_0x1C = gobj;
    }
}

/* scstaffroll.c:752-766 scStaffrollGetLockOnPositionX, verbatim. */
s32 scStaffrollGetLockOnPositionX(s32 pos_x)
{
    s32 bound_x = pos_x;

    if (pos_x < 20)
    {
        bound_x = 20;
    }
    if (pos_x > 620)
    {
        bound_x = 620;
    }
    return bound_x;
}

/* scstaffroll.c:768-782 scStaffrollGetLockOnPositionY, verbatim. */
s32 scStaffrollGetLockOnPositionY(s32 pos_y)
{
    s32 bound_y = pos_y;

    if (pos_y < 20)
    {
        bound_y = 20;
    }
    if (pos_y > 460)
    {
        bound_y = 460;
    }
    return bound_y;
}

/* scstaffroll.c:784-825 scStaffrollHighlightProcDisplay: the red
 * lock-on box that closes in on a shot name. DIVERGES: the decomp writes
 * four gDPFillRectangle strokes into gSYTaskmanDLHeads[0], and nothing
 * on this target ever runs that buffer, so the box never showed. The
 * same four rectangles in the same colour go through
 * lbCommonSpriteFillRect instead, as scStaffrollTextBoxProcDisplay's
 * do. G_CYC_FILL rectangles include their lower-right edge and the
 * helper's exclude it, hence the + 1 on each. The helper draws in the
 * translucent pass only. */
static void scStaffrollHighlightFill(s32 ulx, s32 uly, s32 lrx, s32 lry)
{
    lbCommonSpriteFillRect
    (
        scStaffrollGetLockOnPositionX(ulx),
        scStaffrollGetLockOnPositionY(uly),
        scStaffrollGetLockOnPositionX(lrx) + 1,
        scStaffrollGetLockOnPositionY(lry) + 1,
        0x80, 0x00, 0x00, 0xFF
    );
}

void scStaffrollHighlightProcDisplay(GObj *gobj)
{
    scStaffrollHighlightFill
    (
        (sSCStaffrollHighlightSize * -30) + sSCStaffrollHighlightPositionX,
        (sSCStaffrollHighlightSize * -25) + sSCStaffrollHighlightPositionY,
        ((sSCStaffrollHighlightSize * -30) + 2) + sSCStaffrollHighlightPositionX,
        ((sSCStaffrollHighlightSize * 45) + 2) + sSCStaffrollHighlightPositionY
    );
    scStaffrollHighlightFill
    (
        (sSCStaffrollHighlightSize * -30) + sSCStaffrollHighlightPositionX,
        (sSCStaffrollHighlightSize * -25) + sSCStaffrollHighlightPositionY,
        ((sSCStaffrollHighlightSize * 65) + 2) + sSCStaffrollHighlightPositionX,
        ((sSCStaffrollHighlightSize * -25) + 2) + sSCStaffrollHighlightPositionY
    );
    scStaffrollHighlightFill
    (
        (sSCStaffrollHighlightSize * -30) + sSCStaffrollHighlightPositionX,
        (sSCStaffrollHighlightSize * 45) + sSCStaffrollHighlightPositionY,
        ((sSCStaffrollHighlightSize * 65) + 2) + sSCStaffrollHighlightPositionX,
        ((sSCStaffrollHighlightSize * 45) + 2) + sSCStaffrollHighlightPositionY
    );
    scStaffrollHighlightFill
    (
        (sSCStaffrollHighlightSize * 65) + sSCStaffrollHighlightPositionX,
        (sSCStaffrollHighlightSize * -25) + sSCStaffrollHighlightPositionY,
        ((sSCStaffrollHighlightSize * 65) + 2) + sSCStaffrollHighlightPositionX,
        ((sSCStaffrollHighlightSize * 45) + 2) + sSCStaffrollHighlightPositionY
    );
}

/* scstaffroll.c:828-844 scStaffrollHighlightThreadUpdate, verbatim. */
void scStaffrollHighlightThreadUpdate(GObj *gobj)
{
    s32 i;

    for (i = 0; i < 3; i++)
    {
        sSCStaffrollHighlightSize = 6;

        while (sSCStaffrollHighlightSize != 0)
        {
            sSCStaffrollHighlightSize--;
            gcSleepCurrentGObjThread(1);
        }
    }
    gcEjectGObj(NULL);
    gcSleepCurrentGObjThread(1);
}

/* scstaffroll.c:847-863 scStaffrollMakeHighlightGObj, verbatim. */
void scStaffrollMakeHighlightGObj(GObj *gobj)
{
    GObj *highlight_gobj = gGCCommonLinks[nGCCommonLinkIDHighlight];
    SObj *sobj = SObjGetStruct(sSCStaffrollCrosshairGObj);

    if (highlight_gobj == NULL)
    {
        highlight_gobj = gcMakeGObjSPAfter(9, NULL, 9, GOBJ_PRIORITY_DEFAULT);

        gcAddGObjDisplay(highlight_gobj, scStaffrollHighlightProcDisplay, 8, GOBJ_PRIORITY_DEFAULT, ~0);
        gcAddGObjProcess(highlight_gobj, scStaffrollHighlightThreadUpdate, nGCProcessKindThread, 1);

        sSCStaffrollHighlightPositionX = sobj->pos.x + 8.0F;
        sSCStaffrollHighlightPositionY = sobj->pos.y + 20.0F;
    }
}

/* scstaffroll.c:866-900 scStaffrollSetTextQuetions, verbatim: replaces
 * a run of characters in the compiled-in dSCStaffrollStaffRoleCharacters
 * table with question marks -- pure data, no asset dependency. */
void scStaffrollSetTextQuetions(s32 *characters, s32 character_count)
{
    s32 i, j, k;
    s32 *cadd, *cbase = &dSCStaffrollStaffRoleCharacters[0];

    for (i = 0; i < (s32) ARRAY_COUNT(dSCStaffrollStaffRoleTextInfo) - 1; i++)
    {
        for (cadd = cbase, j = 0, k = 0; j < dSCStaffrollStaffRoleTextInfo[i].character_count; j++, cbase++)
        {
            if (*cbase == characters[k])
            {
                if (k == 0)
                {
                    cadd = cbase;
                }
                k++;
            }
            else k = 0;

            if (k == character_count)
            {
                while (k != 0)
                {
                    *cadd++ = GMSTAFFROLL_QUESTION_MARK_PARA_FONT_INDEX;

                    k--;

                    continue;
                }
                cadd = cbase;
                k = 0;
            }
        }
    }
}

/* scstaffroll.c:903-1039 scStaffrollTryHideUnlocks, verbatim (REGION_US
 * arm): reads gSCManagerBackupData.unlock_mask (real, already used
 * throughout the port) against the real LBBACKUP_UNLOCK_MASK_* bits --
 * no new save-data field, no write path, see scstaffroll.h. */
void scStaffrollTryHideUnlocks(void)
{
    static s32 luigi[] =
    {
        GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('L'),
        GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('u'),
        GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('i'),
        GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('g'),
        GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('i')
    };

    static s32 purin[] =
    {
        GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('J'),
        GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('i'),
        GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('g'),
        GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('g'),
        GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('l'),
        GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('y'),
        GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('p'),
        GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('u'),
        GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('f'),
        GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('f')
    };

    static s32 captain[] =
    {
        GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('C'),
        GMSTAFFROLL_PERIOD_PARA_FONT_INDEX,
        GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('F'),
        GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('a'),
        GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('l'),
        GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('c'),
        GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('o'),
        GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('n')
    };

    static s32 ness[] =
    {
        GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('N'),
        GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('e'),
        GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('s'),
        GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('s')
    };

    static s32 earthbound[] =
    {
        GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('E'),
        GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('a'),
        GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('r'),
        GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('t'),
        GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('h'),
        GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('B'),
        GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('o'),
        GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('u'),
        GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('n'),
        GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('d')
    };

    static s32 fzero[] =
    {
        GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('F'),
        GMSTAFFROLL_DASH_PARA_FONT_INDEX,
        GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('Z'),
        GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('E'),
        GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('R'),
        GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('O'),
        GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX(' '),
        GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('X')
    };

    static s32 classicmario[] =
    {
        GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('C'),
        GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('l'),
        GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('a'),
        GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('s'),
        GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('s'),
        GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('i'),
        GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('c'),
        GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('M'),
        GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('a'),
        GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('r'),
        GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('i'),
        GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('o')
    };

    if (!(gSCManagerBackupData.unlock_mask & LBBACKUP_UNLOCK_MASK_LUIGI))
    {
        scStaffrollSetTextQuetions(luigi, ARRAY_COUNT(luigi));
    }
    if (!(gSCManagerBackupData.unlock_mask & LBBACKUP_UNLOCK_MASK_PURIN))
    {
        scStaffrollSetTextQuetions(purin, ARRAY_COUNT(purin));
    }
    if (!(gSCManagerBackupData.unlock_mask & LBBACKUP_UNLOCK_MASK_CAPTAIN))
    {
        scStaffrollSetTextQuetions(captain, ARRAY_COUNT(captain));
        scStaffrollSetTextQuetions(fzero, ARRAY_COUNT(fzero));
    }
    if (!(gSCManagerBackupData.unlock_mask & LBBACKUP_UNLOCK_MASK_NESS))
    {
        scStaffrollSetTextQuetions(ness, ARRAY_COUNT(ness));
        scStaffrollSetTextQuetions(earthbound, ARRAY_COUNT(earthbound));
    }
    if (!(gSCManagerBackupData.unlock_mask & LBBACKUP_UNLOCK_MASK_INISHIE))
    {
        scStaffrollSetTextQuetions(classicmario, ARRAY_COUNT(classicmario));
    }
}

/* scstaffroll.c:1042-1143 scStaffrollMakeStaffRoleTextSObjs, verbatim:
 * the popup textbox's own role-text SObjs, real Sprite gets through
 * STAFFROLL_SPRITE (scstaffrollgraphics.spr). */
void scStaffrollMakeStaffRoleTextSObjs(GObj *text_gobj, GObj *staff_gobj)
{
    s32 character_id;
    SObj *sobj;
    s32 i;
    f32 hvar;
    s32 character_count;
    f32 wbase;
    f32 hbase;
    SCStaffrollName *staff = staff_gobj->user_data.p;

    wbase = 350.0F;
    hbase = 40.0F;

    character_count = dSCStaffrollStaffRoleTextInfo[staff->name_id].character_count;

    for (i = 0, character_id = dSCStaffrollStaffRoleTextInfo[staff->name_id].character_start; i < character_count; i++, character_id++)
    {
        if
        (
            (dSCStaffrollStaffRoleCharacters[character_id] != GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX(' ')) &&
            (dSCStaffrollStaffRoleCharacters[character_id] != GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('\n'))
        )
        {
            hvar = 0.0F;

            sobj = lbCommonMakeSObjForGObj(text_gobj, STAFFROLL_SPRITE(dSCStaffrollTextBoxSpriteInfo[dSCStaffrollStaffRoleCharacters[character_id]].offset));

            sobj->sprite.attr = SP_TRANSPARENT;

            sobj->sprite.red   = 0xB7;
            sobj->sprite.green = 0xBC;
            sobj->sprite.blue  = 0xEC;

            sobj->pos.x = wbase;

            sobj->sprite.scalex = sobj->sprite.scaley = 1;

            if (dSCStaffrollStaffRoleCharacters[character_id] >= GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('a'))
            {
                hvar = 3.0F;

                if
                (
                    dSCStaffrollStaffRoleCharacters[character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('b')      ||
                    dSCStaffrollStaffRoleCharacters[character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('d')      ||
                    dSCStaffrollStaffRoleCharacters[character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('f')      ||
                    dSCStaffrollStaffRoleCharacters[character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('h')      ||
                    dSCStaffrollStaffRoleCharacters[character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('i')      ||
                    dSCStaffrollStaffRoleCharacters[character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('j')      ||
                    dSCStaffrollStaffRoleCharacters[character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('k')      ||
                    dSCStaffrollStaffRoleCharacters[character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('l')      ||
                    dSCStaffrollStaffRoleCharacters[character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('t')      ||
                    dSCStaffrollStaffRoleCharacters[character_id] == GMSTAFFROLL_COLON_PARA_FONT_INDEX                ||
                    dSCStaffrollStaffRoleCharacters[character_id] == GMSTAFFROLL_ASCII_NUMBER_TO_PARA_FONT_INDEX('9') ||
                    dSCStaffrollStaffRoleCharacters[character_id] == GMSTAFFROLL_ASCII_NUMBER_TO_PARA_FONT_INDEX('8') ||
                    dSCStaffrollStaffRoleCharacters[character_id] == GMSTAFFROLL_ASCII_NUMBER_TO_PARA_FONT_INDEX('7') ||
                    dSCStaffrollStaffRoleCharacters[character_id] == GMSTAFFROLL_ASCII_NUMBER_TO_PARA_FONT_INDEX('6') ||
                    dSCStaffrollStaffRoleCharacters[character_id] == GMSTAFFROLL_ASCII_NUMBER_TO_PARA_FONT_INDEX('5') ||
                    dSCStaffrollStaffRoleCharacters[character_id] == GMSTAFFROLL_ASCII_NUMBER_TO_PARA_FONT_INDEX('4') ||
                    dSCStaffrollStaffRoleCharacters[character_id] == GMSTAFFROLL_ASCII_NUMBER_TO_PARA_FONT_INDEX('3') ||
                    dSCStaffrollStaffRoleCharacters[character_id] == GMSTAFFROLL_ASCII_NUMBER_TO_PARA_FONT_INDEX('2') ||
                    dSCStaffrollStaffRoleCharacters[character_id] == GMSTAFFROLL_ASCII_NUMBER_TO_PARA_FONT_INDEX('1') ||
                    dSCStaffrollStaffRoleCharacters[character_id] == GMSTAFFROLL_ASCII_NUMBER_TO_PARA_FONT_INDEX('0') ||
                    dSCStaffrollStaffRoleCharacters[character_id] == GMSTAFFROLL_AMPERSAND_PARA_FONT_INDEX            ||
                    dSCStaffrollStaffRoleCharacters[character_id] == GMSTAFFROLL_QUESTION_MARK_PARA_FONT_INDEX        ||
                    dSCStaffrollStaffRoleCharacters[character_id] == GMSTAFFROLL_E_ACCENT_PARA_FONT_INDEX             ||
                    dSCStaffrollStaffRoleCharacters[character_id] == GMSTAFFROLL_DOUBLE_QUOTES_PARA_FONT_INDEX
                )
                {
                    hvar = 1.0F;
                }
            }
            if (dSCStaffrollStaffRoleCharacters[character_id] == GMSTAFFROLL_PERIOD_PARA_FONT_INDEX)
            {
                hvar += 6.0F;
            }
            if (dSCStaffrollStaffRoleCharacters[character_id] == GMSTAFFROLL_DASH_PARA_FONT_INDEX)
            {
                hvar += 2.0F;
            }
            if (dSCStaffrollStaffRoleCharacters[character_id] == GMSTAFFROLL_COMMA_PARA_FONT_INDEX)
            {
                hvar += 7.0F;
            }
            sobj->pos.y = hbase + hvar;

            wbase += dSCStaffrollTextBoxSpriteInfo[dSCStaffrollStaffRoleCharacters[character_id]].width;
        }
        else if (dSCStaffrollStaffRoleCharacters[character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX(' '))
        {
            wbase += 3.0F;
        }
        else
        {
            wbase = 350.0F;
            hbase += 20.0F;
        }
    }
}

/* scstaffroll.c:1146-1161 scStaffrollMakeStaffRoleTextGObj, verbatim. */
void scStaffrollMakeStaffRoleTextGObj(GObj *staff_gobj)
{
    GObj *text_gobj;

    if (gGCCommonLinks[10] != NULL)
    {
        gcEjectGObj(gGCCommonLinks[10]);
    }
    text_gobj = gcMakeGObjSPAfter(6, NULL, 10, GOBJ_PRIORITY_DEFAULT);

    gcAddGObjDisplay(text_gobj, lbCommonDrawSObjAttr, 5, GOBJ_PRIORITY_DEFAULT, ~0);

    sSCStaffrollStaffRoleTextGObj = text_gobj;

    scStaffrollMakeStaffRoleTextSObjs(text_gobj, staff_gobj);
}

/* scstaffroll.c:1164-1247 scStaffrollMakeCompanyTextSObjs, verbatim. */
void scStaffrollMakeCompanyTextSObjs(GObj *text_gobj, GObj *staff_gobj)
{
    SObj *sobj;
    f32 hvar;
    f32 wbase;
    s32 character_id;
    s32 character_count;
    SCStaffrollName *staff = staff_gobj->user_data.p;
    s32 i;

    if (dSCStaffrollCompanyIDs[staff->name_id] != nSCStaffrollCompanyNull)
    {
        wbase = 350.0F;

        character_count = dSCStaffrollCompanyTextInfo[dSCStaffrollCompanyIDs[staff->name_id]].character_count;
        character_id = dSCStaffrollCompanyTextInfo[dSCStaffrollCompanyIDs[staff->name_id]].character_start;

        for (i = 0; i < character_count; i++, character_id++)
        {
            if (dSCStaffrollCompanyCharacters[character_id] != GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX(' '))
            {
                sobj = lbCommonMakeSObjForGObj(text_gobj, STAFFROLL_SPRITE(dSCStaffrollTextBoxSpriteInfo[dSCStaffrollCompanyCharacters[character_id]].offset));

                hvar = 0.0F;

                sobj->sprite.attr = SP_TRANSPARENT;

                sobj->sprite.scalex = sobj->sprite.scaley = 1;

                sobj->sprite.red   = 0x80;
                sobj->sprite.green = 0x40;
                sobj->sprite.blue  = 0x80;

                sobj->pos.x = wbase;

                if (dSCStaffrollCompanyCharacters[character_id] >= GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('a'))
                {
                    hvar = 3.0F;

                    if
                    (
                        (dSCStaffrollCompanyCharacters[character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('b')) ||
                        (dSCStaffrollCompanyCharacters[character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('d')) ||
                        (dSCStaffrollCompanyCharacters[character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('f')) ||
                        (dSCStaffrollCompanyCharacters[character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('h')) ||
                        (dSCStaffrollCompanyCharacters[character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('i')) ||
                        (dSCStaffrollCompanyCharacters[character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('j')) ||
                        (dSCStaffrollCompanyCharacters[character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('k')) ||
                        (dSCStaffrollCompanyCharacters[character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('l')) ||
                        (dSCStaffrollCompanyCharacters[character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('t')) ||
                        (dSCStaffrollCompanyCharacters[character_id] == GMSTAFFROLL_COLON_PARA_FONT_INDEX) ||
                        (dSCStaffrollCompanyCharacters[character_id] == GMSTAFFROLL_AMPERSAND_PARA_FONT_INDEX) ||
                        (dSCStaffrollCompanyCharacters[character_id] == GMSTAFFROLL_DOUBLE_QUOTES_PARA_FONT_INDEX)
                    )
                    {
                        hvar = 1.0F;
                    }
                }
                if ((dSCStaffrollCompanyCharacters[character_id] == GMSTAFFROLL_OPEN_PARENTHESIS_PARA_FONT_INDEX) ||
                    (dSCStaffrollCompanyCharacters[character_id] == GMSTAFFROLL_CLOSE_PARENTHESIS_PARA_FONT_INDEX))
                {
                    hvar = 0.0F;
                }
                if (dSCStaffrollCompanyCharacters[character_id] == GMSTAFFROLL_PERIOD_PARA_FONT_INDEX)
                {
                    hvar += 6.0F;
                }
                if (dSCStaffrollCompanyCharacters[character_id] == GMSTAFFROLL_DASH_PARA_FONT_INDEX)
                {
                    hvar += 2.0F;
                }
                if (dSCStaffrollCompanyCharacters[character_id] == GMSTAFFROLL_COMMA_PARA_FONT_INDEX)
                {
                    hvar += 7.0F;
                }
                sobj->pos.y = 140.0F + hvar;

                wbase += dSCStaffrollTextBoxSpriteInfo[dSCStaffrollCompanyCharacters[character_id]].width;
            }
            else wbase += 3.0F;
        }
    }
}

/* scstaffroll.c:1250-1265 scStaffrollMakeCompanyTextGObj, verbatim. */
void scStaffrollMakeCompanyTextGObj(GObj *staff_gobj)
{
    GObj *text_gobj;

    if (gGCCommonLinks[11] != NULL)
    {
        gcEjectGObj(gGCCommonLinks[11]);
    }
    text_gobj = gcMakeGObjSPAfter(7, NULL, 0xB, GOBJ_PRIORITY_DEFAULT);

    gcAddGObjDisplay(text_gobj, lbCommonDrawSObjAttr, 6, GOBJ_PRIORITY_DEFAULT, ~0);

    sSCStaffrollCompanyTextGObj = text_gobj;

    scStaffrollMakeCompanyTextSObjs(text_gobj, staff_gobj);
}

/* scstaffroll.c:1268-1302 scStaffrollCheckCursorHighlightPrompt,
 * verbatim, with func_ovl59_8013202C's own backdrop
 * (see it above). */
sb32 scStaffrollCheckCursorHighlightPrompt(GObj *gobj, SCStaffrollProjection *proj)
{
    sb32 b;
    Vec3f sp4C;
    Vec3f sp40;
    Vec3f sp34;
    Vec3f sp28;

    b = TRUE;

    func_ovl59_80131E70(&sp4C, proj->px0, proj->py0, proj->px2, proj->py2);
    func_ovl59_80131E70(&sp40, proj->px1, proj->py1, proj->px3, proj->py3);
    func_ovl59_80131E70(&sp34, proj->px0, proj->py0, proj->px1, proj->py1);
    func_ovl59_80131E70(&sp28, proj->px2, proj->py2, proj->px3, proj->py3);

    if
    (
        (scStaffrollCheckCursorNameOverlap(&sp4C) == FALSE) &&
        (scStaffrollCheckCursorNameOverlap(&sp40) != FALSE) &&
        (scStaffrollCheckCursorNameOverlap(&sp34) != FALSE) &&
        (scStaffrollCheckCursorNameOverlap(&sp28) == FALSE)
    )
    {
        func_800269C0_275C0(nSYAudioFGMTrainingSel);

        b = FALSE;

        func_ovl59_8013202C(gobj);
        scStaffrollMakeHighlightGObj(gobj);
        scStaffrollMakeStaffRoleTextGObj(gobj);
        scStaffrollMakeCompanyTextGObj(gobj);
    }
    return b;
}

/* scstaffroll.c:1305-1334 func_ovl59_8013330C, verbatim. */
void func_ovl59_8013330C(void)
{
    GObj *gobj;
    DObj *dobj;
    SCStaffrollProjection proj;
    sb32 b;

    func_ovl59_80131C88(sSCStaffrollCamera);

    gobj = gGCCommonLinks[3];

    if (gobj != NULL)
    {
        do
        {
            dobj = DObjGetStruct(gobj);

            func_ovl59_80131DD0(gobj, &proj);
            func_ovl59_80131D30(dobj, &proj.pv0, &proj.px0, &proj.py0);
            func_ovl59_80131D30(dobj, &proj.pv1, &proj.px1, &proj.py1);
            func_ovl59_80131D30(dobj, &proj.pv2, &proj.px2, &proj.py2);
            func_ovl59_80131D30(dobj, &proj.pv3, &proj.px3, &proj.py3);

            b = scStaffrollCheckCursorHighlightPrompt(gobj, &proj);

            gobj = gobj->link_next;
        }
        while ((gobj != NULL) && (b != FALSE));
    }
}

/* scstaffroll.c:1336-1373 scStaffrollGetPauseStatusHighlight, verbatim. */
sb32 scStaffrollGetPauseStatusHighlight(void)
{
    GObj *gobj;
    u16 button_tap = gSYControllerDevices[sSCStaffrollPlayer].button_tap;
    sb32 is_paused = FALSE;

    if (button_tap & (A_BUTTON | B_BUTTON))
    {
        func_ovl59_8013330C();

        if (button_tap & B_BUTTON)
        {
            if (sSCStaffrollScrollGObj != NULL)
            {
                gcPauseGObjProcessAll(sSCStaffrollScrollGObj);
            }
            gobj = gGCCommonLinks[3];

            while (gobj != NULL)
            {
                gcPauseGObjProcessAll(gobj);

                gobj = gobj->link_next;
            }
            gobj = gGCCommonLinks[4];

            while (gobj != NULL)
            {
                gcPauseGObjProcessAll(gobj);

                gobj = gobj->link_next;
            }
            is_paused = TRUE;
        }
    }
    return is_paused;
}

/* scstaffroll.c:1375-1419 scStaffrollFuncRun, verbatim. */
void scStaffrollFuncRun(GObj *gobj)
{
    sb32 is_paused;
    u16 button_tap;

    if ((sSCStaffrollRollEndWait == 0) || (sSCStaffrollStatus != -1) && (sSCStaffrollStatus != -2))
    {
        button_tap = gSYControllerDevices[sSCStaffrollPlayer].button_tap;

        if (sSCStaffrollStatus == 1)
        {
            if (sSCStaffrollRollBeginWait < 120)
            {
                sSCStaffrollRollBeginWait++;
            }
            else
            {
                scStaffrollMakeTextBoxBracketSObjs();
                scStaffrollMakeTextBoxGObj();
                sSCStaffrollStatus = 0;
            }
        }
        is_paused = sSCStaffrollIsPaused;

        if (sSCStaffrollIsPaused == FALSE)
        {
            is_paused = scStaffrollGetPauseStatusHighlight();
        }
        if (sSCStaffrollIsPaused == TRUE)
        {
            is_paused = scStaffrollGetPauseStatusResume();
        }
        sSCStaffrollIsPaused = is_paused;

        if (button_tap & START_BUTTON)
        {
            if (sSCStaffrollRollSpeed == 0.0037500001F)
            {
                sSCStaffrollRollSpeed = 0.049999997F;
            }
            else sSCStaffrollRollSpeed = 0.0037500001F;
        }
    }
}

/* scstaffroll.c:1422-1440 SCStaffrollNameUpdateAlloc, verbatim. */
SCStaffrollName *SCStaffrollNameUpdateAlloc(GObj *gobj)
{
    SCStaffrollName *cn;

    if (sSCStaffrollNameAllocFree == NULL)
    {
        cn = syTaskmanMalloc(sizeof(SCStaffrollName), 0x4);
    }
    else
    {
        cn = sSCStaffrollNameAllocFree;
        sSCStaffrollNameAllocFree = cn->next;
    }
    cn->offset_x = cn->unkgmcreditsstruct0x10 = cn->interpolation = cn->status = 0;

    gobj->user_data.p = cn;

    return cn;
}

/* scstaffroll.c:1443-1447 SCStaffrollNameSetPrevAlloc, verbatim. */
void SCStaffrollNameSetPrevAlloc(SCStaffrollName *cn)
{
    cn->next = sSCStaffrollNameAllocFree;
    sSCStaffrollNameAllocFree = cn;
}

/* scstaffroll.c:1450-1496 scStaffrollJobAndNameThreadUpdate, verbatim:
 * the gcAddDObjAnimJoint/syInterpCubic pair runs over the compiled-in script and curve
 * at the top of this file, and is what carries a credit line up the
 * screen and away from the camera. */
void scStaffrollJobAndNameThreadUpdate(GObj *gobj)
{
    SCStaffrollName *cn;
    DObj *dobj;

    cn = gobj->user_data.p;
    dobj = gobj->obj;

    cn->interpolation = 0.0F;

    gobj->flags = GOBJ_FLAG_HIDDEN;

    while (sSCStaffrollStatus != 0)
    {
        gcSleepCurrentGObjThread(1);
    }
    gobj->flags = GOBJ_FLAG_NONE;

    while (cn->interpolation != 1.0F)
    {
        {
            Vec3f pos;

            gcAddDObjAnimJoint(dobj, sSCStaffrollNameAnimJoint, cn->interpolation * 99.0F);

            syInterpCubic(&pos, sSCStaffrollNameInterpolation, cn->interpolation);

            dobj->translate.vec.f.x = pos.x + cn->offset_x;
            dobj->translate.vec.f.y = pos.y + 12.0F;
            dobj->translate.vec.f.z = pos.z;
        }
        cn->interpolation += sSCStaffrollRollSpeed;

        if (cn->interpolation > 1.0F)
        {
            cn->interpolation = 1.0F;
        }
        gcPlayAnimAll(gobj);
        gcSleepCurrentGObjThread(1);
    }
    if (cn->status == -1)
    {
        sSCStaffrollStatus = -1;
    }
    SCStaffrollNameSetPrevAlloc(cn);
    gcEjectGObj(NULL);
    gcSleepCurrentGObjThread(1);
}

/* scstaffroll.c:1499-1524 scStaffrollJobProcDisplay and
 * scStaffrollNameProcDisplay. DIVERGES, and in the same one way twice.
 *
 * The decomp's five GBI commands are RDP state, so it only has to emit
 * them ONCE per list: the head of the job list (gGCCommonLinks[4]) and
 * the head of the name list (gGCCommonLinks[3]) each set their own
 * primitive colour, and every GObj drawn after it in that list inherits
 * it. There is no such standing state here. The texture-on, the
 * Z-clear, the render mode and the combiner are all baked into the
 * glyph pack's own batches by tools/export/ssb_staffrollexport.py (translucent
 * bucket, FPACK_NOZ, alpha from the tile and colour from PRIM), and the
 * colour itself is a live per-instance override on the pack
 * (fighter_set_prim_color, src/dc/wpdisplay.c's own precedent) -- but
 * the pack is ONE pack shared by every line on screen, so the override
 * has to be put on immediately ahead of the walk that consumes it, on
 * every call rather than on the list head's. Same two colours, same two
 * lists; only the "once" becomes "each".
 *
 * And the whole thing is gated on the translucent pass. The scene draw
 * runs three times a frame, once per PVR list (src/dc/taskman.c), and
 * each glyph DObj below submits itself as the walk reaches it
 * (dc_model_graft_display) -- a walk in the opaque or punch-through
 * pass would transform the pack once per glyph for batches that go
 * nowhere. scStaffrollTextBoxProcDisplay already gates the same way. */
void scStaffrollJobProcDisplay(GObj *gobj)
{
#ifndef FT_HOSTTEST
    if (gcGetDrawList() != PVR_LIST_TR_POLY)
    {
        return;
    }
    fighter_set_prim_color(&sSCStaffrollGlyphPack[0], 0x7F7F89FFU);
    fighter_set_prim_color(&sSCStaffrollGlyphPack[1], 0x7F7F89FFU);
#endif
    gcDrawDObjTreeForGObj(gobj);
}

void scStaffrollNameProcDisplay(GObj *gobj)
{
#ifndef FT_HOSTTEST
    if (gcGetDrawList() != PVR_LIST_TR_POLY)
    {
        return;
    }
    fighter_set_prim_color(&sSCStaffrollGlyphPack[0], 0x8893FFFFU);
    fighter_set_prim_color(&sSCStaffrollGlyphPack[1], 0x8893FFFFU);
#endif
    gcDrawDObjTreeForGObj(gobj);
}

/* scstaffroll.c:1527-1537 scStaffrollJobAndNameInitStruct, verbatim:
 * offset_x/unkgmcreditsstruct0x10 -- the two fields the hit-test chain
 * above reads back through its SCStaffrollMatrix* alias -- are set from
 * real DObj positions regardless of whether the glyph children are
 * attached to those DObjs. */
void scStaffrollJobAndNameInitStruct(GObj *gobj, DObj *first_dobj, DObj *second_dobj, sb32 job_or_name)
{
    SCStaffrollName *cn = SCStaffrollNameUpdateAlloc(gobj);

    cn->offset_x = (first_dobj->translate.vec.f.x - second_dobj->translate.vec.f.x) * 0.5F;

    cn->unkgmcreditsstruct0x10 = 26.0F;

    cn->name_id = sSCStaffrollNameID;
    cn->job_or_name = job_or_name;
}

/* One glyph DObj, the port's shape of the decomp's own
 * `gcAddChildForDObj(dobj, sSCStaffrollNameAndJobDisplayLists[id])`.
 *
 * Two things beyond swapping the payload. The DObj gets a GRAFT
 * payload, not a plain one: a credit line is many DObjs over ONE pack,
 * several of them often the same joint (every repeated letter in a
 * name), so a payload that only recorded a matrix would leave the pack
 * holding the last one and draw that glyph once. dc_model_graft_display
 * makes each DObj transform and submit its own joint as the walk
 * reaches it and lets the next overwrite what it used -- the same
 * reason src/dc/stage.c's Yoshi's Island clouds are grafts
 * (objmodel.h). And the payload lives in this file's own static array
 * rather than the pack's per-joint set, because the pack's is one
 * DCDisplay per joint and these DObjs outnumber the joints.
 *
 * A pack that failed to load makes no child at all, which is exactly
 * what every glyph does. */
static DObj *scStaffrollAddGlyphDObj(DObj *parent, s32 glyph_id)
{
#ifndef FT_HOSTTEST
    DObj *new_dobj;

    if (!sSCStaffrollGlyphsLoaded)
    {
        return NULL;
    }
    new_dobj = gcAddChildForDObj(parent,
                                 &sSCStaffrollNameAndJobDisplayLists[glyph_id]);

    if (new_dobj != NULL)
    {
        dc_model_graft_display(new_dobj);
    }
    return new_dobj;
#else
    /* The host cross-test links the scene and not the renderer, so
     * there is no pack -- but the DObjs, and the positions
     * scStaffrollJobAndNameInitStruct measures off the last of them,
     * are the thing it checks, so they are made with no payload. */
    (void)glyph_id;

    return gcAddChildForDObj(parent, NULL);
#endif
}

/* scstaffroll.c:1540-1678 scStaffrollMakeJobDObjs, verbatim
 * but for where the glyph's payload comes from (see
 * scStaffrollAddGlyphDObj above): the layout arithmetic
 * (wbase/width/height/kerning and the five baseline fixups after it)
 * places real DObjs. A glyph the
 * pack could not supply leaves new_dobj alone, so name_setup->dobj
 * falls back to the parent. */
SCStaffrollSetup *scStaffrollMakeJobDObjs(SCStaffrollSetup *name_setup, DObj *dobj, s32 name_id, f32 wbase)
{
    SCStaffrollSetup local_setup;
    DObj *new_dobj = dobj;
    f32 width;
    f32 height;
    s32 job_character_id;
    s32 character_id;
    s32 i;

    for
    (
        i = 0, character_id = dSCStaffrollJobTextInfo[name_id].character_start, job_character_id = -1;
        i < dSCStaffrollJobTextInfo[name_id].character_count;
        job_character_id = character_id, i++, character_id++
    )
    {
        if (dSCStaffrollJobCharacters[character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX(' '))
        {
            wbase += 16.0F;

            continue;
        }
        else
        {
            DObj *glyph_dobj;

            width = dSCStaffrollNameAndJobSpriteInfo[dSCStaffrollJobCharacters[character_id]].width;
            height = dSCStaffrollNameAndJobSpriteInfo[dSCStaffrollJobCharacters[character_id]].height;

            glyph_dobj = scStaffrollAddGlyphDObj(dobj, dSCStaffrollJobCharacters[character_id]);

            if (glyph_dobj != NULL)
            {
                new_dobj = glyph_dobj;

                gcAddXObjForDObjFixed(new_dobj, nGCMatrixKindTra, 1);
            }
            if (job_character_id != -1)
            {
                if
                (
                    (
                        (dSCStaffrollJobCharacters[job_character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('K')) ||
                        (dSCStaffrollJobCharacters[job_character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('T')) ||
                        (dSCStaffrollJobCharacters[job_character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('V')) ||
                        (dSCStaffrollJobCharacters[job_character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('W')) ||
                        (dSCStaffrollJobCharacters[job_character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('Y'))
                    )
                    &&
                    (
                        (dSCStaffrollJobCharacters[character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('a')) ||
                        (dSCStaffrollJobCharacters[character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('c')) ||
                        (dSCStaffrollJobCharacters[character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('e')) ||
                        (dSCStaffrollJobCharacters[character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('g')) ||
                        (dSCStaffrollJobCharacters[character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('m')) ||
                        (dSCStaffrollJobCharacters[character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('n')) ||
                        (dSCStaffrollJobCharacters[character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('o')) ||
                        (dSCStaffrollJobCharacters[character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('p')) ||
                        (dSCStaffrollJobCharacters[character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('q')) ||
                        (dSCStaffrollJobCharacters[character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('r')) ||
                        (dSCStaffrollJobCharacters[character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('s')) ||
                        (dSCStaffrollJobCharacters[character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('u')) ||
                        (dSCStaffrollJobCharacters[character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('v')) ||
                        (dSCStaffrollJobCharacters[character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('w')) ||
                        (dSCStaffrollJobCharacters[character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('x')) ||
                        (dSCStaffrollJobCharacters[character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('y')) ||
                        (dSCStaffrollJobCharacters[character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('z'))
                    )
                )
                {
                    wbase -= 6.0F;
                }
                else if
                (
                    (
                        (dSCStaffrollJobCharacters[job_character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('k')) ||
                        (dSCStaffrollJobCharacters[job_character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('r')) ||
                        (dSCStaffrollJobCharacters[job_character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('y'))
                    )
                    &&
                    (
                        (dSCStaffrollJobCharacters[character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('a')) ||
                        (dSCStaffrollJobCharacters[character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('e')) ||
                        (dSCStaffrollJobCharacters[character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('o'))
                    )
                )
                {
                    wbase -= 6.0F;
                }
                else if
                (
                    (dSCStaffrollJobCharacters[job_character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('o')) &&
                    (dSCStaffrollJobCharacters[character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('s'))
                )
                {
                    wbase -= 4.0F;
                }
                else if
                (
                    (dSCStaffrollJobCharacters[job_character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('S')) &&
                    (dSCStaffrollJobCharacters[character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('u'))
                )
                {
                    wbase -= 4.0F;
                }
            }
            /* The decomp writes the placement straight into the DObj and
             * then reads wbase back out of it; a glyph the pack could
             * not supply has no DObj to write, so the two numbers are
             * held here and stored at the end. Same arithmetic, same
             * order, same five baseline fixups. */
            {
                f32 pos_x = wbase + width;
                f32 pos_y = height - 22.0F;

                wbase = pos_x + width;

                if (dSCStaffrollJobCharacters[character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('z'))
                {
                    pos_y += 1.0F;
                }
                if (dSCStaffrollJobCharacters[character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('j'))
                {
                    pos_y = 22.0F - height;
                }
                if (dSCStaffrollJobCharacters[character_id] == GMSTAFFROLL_ASCII_NUMBER_TO_PARA_FONT_INDEX('8'))
                {
                    pos_y += 22.0F;
                }
                if
                (
                    (dSCStaffrollJobCharacters[character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('g')) ||
                    (dSCStaffrollJobCharacters[character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('p')) ||
                    (dSCStaffrollJobCharacters[character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('q')) ||
                    (dSCStaffrollJobCharacters[character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('y'))
                )
                {
                    pos_y = -8.0F;
                }
                if (glyph_dobj != NULL)
                {
                    glyph_dobj->translate.vec.f.x = pos_x;
                    glyph_dobj->translate.vec.f.y = pos_y;
                }
            }
        }
    }
    local_setup.spacing = wbase;
    local_setup.dobj = new_dobj;

    *name_setup = local_setup;

    return name_setup;
}

/* scstaffroll.c:1681-1709 scStaffrollMakeJobGObj, verbatim: the parent
 * GObj/DObj are real (no relocData dependency, dl is NULL); only the
 * glyph children scStaffrollMakeJobDObjs above would attach are cut. */
GObj *scStaffrollMakeJobGObj(SCStaffrollJob *job)
{
    SCStaffrollSetup job_setup;
    GObj *gobj;
    DObj *dobj;
    f32 wbase;

    wbase = 0.0F;

    gobj = gcMakeGObjSPAfter(1, NULL, 4, GOBJ_PRIORITY_DEFAULT);

    gcAddGObjDisplay(gobj, scStaffrollJobProcDisplay, 2, GOBJ_PRIORITY_DEFAULT, ~0);

    dobj = gcAddDObjForGObj(gobj, NULL);

    gcAddXObjForDObjFixed(dobj, nGCMatrixKindTraRotRpyRSca, 0);

    if (job->prefix_id != -1)
    {
        scStaffrollMakeJobDObjs(&job_setup, dobj, job->prefix_id, 0.0F);
        wbase = 16.0F + job_setup.spacing;
    }
    scStaffrollMakeJobDObjs(&job_setup, dobj, job->job_id, wbase);
    scStaffrollJobAndNameInitStruct(gobj, dobj, job_setup.dobj, 0);
    gcAddGObjProcess(gobj, scStaffrollJobAndNameThreadUpdate, nGCProcessKindThread, 1);

    return gobj;
}

/* scstaffroll.c:1712-1865 scStaffrollMakeNameGObjAndDObjs, the same
 * loop over the same table for the name half, and the same
 * glyph handling scStaffrollMakeJobDObjs above has (its own note). The
 * one difference the decomp has here is a sixth baseline fixup, the
 * para-font '9'. */
GObj *scStaffrollMakeNameGObjAndDObjs(void)
{
    GObj *gobj;
    DObj *dobj;
    DObj *new_dobj;
    f32 width;
    f32 height;
    f32 wbase;
    s32 name_character_id;
    s32 character_id;
    s32 i;

    name_character_id = -1;

    gobj = gcMakeGObjSPAfter(1, NULL, 3, GOBJ_PRIORITY_DEFAULT);

    gcAddGObjDisplay(gobj, scStaffrollNameProcDisplay, 1, GOBJ_PRIORITY_DEFAULT, ~0);

    new_dobj = dobj = gcAddDObjForGObj(gobj, NULL);

    gcAddXObjForDObjFixed(dobj, nGCMatrixKindTraRotRpyRSca, 0);

    wbase = 0.0F;

    for
    (
        i = 0, character_id = dSCStaffrollNameTextInfo[sSCStaffrollNameID].character_start;
        i < dSCStaffrollNameTextInfo[sSCStaffrollNameID].character_count;
        name_character_id = character_id, i++, character_id++
    )
    {
        if (dSCStaffrollNameCharacters[character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX(' '))
        {
            wbase += 16.0F;

            continue;
        }
        else
        {
            DObj *glyph_dobj;

            width = dSCStaffrollNameAndJobSpriteInfo[dSCStaffrollNameCharacters[character_id]].width;
            height = dSCStaffrollNameAndJobSpriteInfo[dSCStaffrollNameCharacters[character_id]].height;

            glyph_dobj = scStaffrollAddGlyphDObj(dobj, dSCStaffrollNameCharacters[character_id]);

            if (glyph_dobj != NULL)
            {
                new_dobj = glyph_dobj;

                gcAddXObjForDObjFixed(new_dobj, nGCMatrixKindTra, 1);
            }
            if (name_character_id != -1)
            {
                if
                (
                    (
                        (dSCStaffrollNameCharacters[name_character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('K')) ||
                        (dSCStaffrollNameCharacters[name_character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('T')) ||
                        (dSCStaffrollNameCharacters[name_character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('V')) ||
                        (dSCStaffrollNameCharacters[name_character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('W')) ||
                        (dSCStaffrollNameCharacters[name_character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('Y'))
                    )
                    &&
                    (
                        (dSCStaffrollNameCharacters[character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('a')) ||
                        (dSCStaffrollNameCharacters[character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('c')) ||
                        (dSCStaffrollNameCharacters[character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('e')) ||
                        (dSCStaffrollNameCharacters[character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('g')) ||
                        (dSCStaffrollNameCharacters[character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('m')) ||
                        (dSCStaffrollNameCharacters[character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('n')) ||
                        (dSCStaffrollNameCharacters[character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('o')) ||
                        (dSCStaffrollNameCharacters[character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('p')) ||
                        (dSCStaffrollNameCharacters[character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('q')) ||
                        (dSCStaffrollNameCharacters[character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('r')) ||
                        (dSCStaffrollNameCharacters[character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('s')) ||
                        (dSCStaffrollNameCharacters[character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('u')) ||
                        (dSCStaffrollNameCharacters[character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('v')) ||
                        (dSCStaffrollNameCharacters[character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('w')) ||
                        (dSCStaffrollNameCharacters[character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('x')) ||
                        (dSCStaffrollNameCharacters[character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('y')) ||
                        (dSCStaffrollNameCharacters[character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('z'))
                    )
                )
                {
                    wbase -= 6.0F;
                }
                else if
                (
                    (
                        (dSCStaffrollNameCharacters[name_character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('k')) ||
                        (dSCStaffrollNameCharacters[name_character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('r')) ||
                        (dSCStaffrollNameCharacters[name_character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('y'))
                    )
                    &&
                    (
                        (dSCStaffrollNameCharacters[character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('a')) ||
                        (dSCStaffrollNameCharacters[character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('e')) ||
                        (dSCStaffrollNameCharacters[character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('o'))
                    )
                )
                {
                    wbase -= 6.0F;
                }
                else if
                (
                    (dSCStaffrollNameCharacters[name_character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('o')) &&
                    (dSCStaffrollNameCharacters[character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('s'))
                )
                {
                    wbase -= 4.0F;
                }
                else if
                (
                    (dSCStaffrollNameCharacters[name_character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('S')) &&
                    (dSCStaffrollNameCharacters[character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('u'))
                )
                {
                    wbase -= 4.0F;
                }
            }
            {
                f32 pos_x = wbase + width;
                f32 pos_y = height - 22.0F;

                wbase = pos_x + width;

                if (dSCStaffrollNameCharacters[character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('z'))
                {
                    pos_y += 1.0F;
                }
                if (dSCStaffrollNameCharacters[character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('j'))
                {
                    pos_y = 22.0F - height;
                }
                if
                (
                    (dSCStaffrollNameCharacters[character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('g')) ||
                    (dSCStaffrollNameCharacters[character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('p')) ||
                    (dSCStaffrollNameCharacters[character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('q')) ||
                    (dSCStaffrollNameCharacters[character_id] == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('y'))
                )
                {
                    pos_y = -8.0F;
                }
                if (dSCStaffrollNameCharacters[character_id] == GMSTAFFROLL_ASCII_NUMBER_TO_PARA_FONT_INDEX('9'))
                {
                    pos_y -= 4.0F;
                }
                if (glyph_dobj != NULL)
                {
                    glyph_dobj->translate.vec.f.x = pos_x;
                    glyph_dobj->translate.vec.f.y = pos_y;
                }
            }
        }
    }
    scStaffrollJobAndNameInitStruct(gobj, dobj, new_dobj, 1);
    gcAddGObjProcess(gobj, scStaffrollJobAndNameThreadUpdate, 0, 1);

    return gobj;
}

/* scstaffroll.c:1868-1905 scStaffrollCrosshairThreadUpdate, verbatim:
 * real controller stick input (gSYControllerDevices), the same
 * mvending.c's own scSubsysControllerGet* calls already use through a
 * different wrapper. */
void scStaffrollCrosshairThreadUpdate(GObj *gobj)
{
    SObj *sobj = SObjGetStruct(gobj);
    s32 crosshair_center_wait = 19;

    sobj->pos.x = 291.0F;
    sobj->pos.y = 0.0F;

    do
    {
        sobj->pos.y += 10.5F;

        gcSleepCurrentGObjThread(1);
    }
    while (crosshair_center_wait--);

    sSCStaffrollStatus = 1;

    while (TRUE)
    {
        s32 stick_x = gSYControllerDevices[sSCStaffrollPlayer].stick_range.x;
        s32 stick_y = gSYControllerDevices[sSCStaffrollPlayer].stick_range.y;

        f32 base_x = sobj->pos.x;
        f32 base_y = sobj->pos.y;

        sobj->pos.x += (ABS(stick_x) > 16) ? stick_x * 0.125F : 0.0F;
        sobj->pos.y -= (ABS(stick_y) > 16) ? stick_y * 0.125F : 0.0F;

        sobj->pos.x = (sobj->pos.x < 32.0F) ? 32.0F : (sobj->pos.x > 540.0F) ? 540.0F : sobj->pos.x;
        sobj->pos.y = (sobj->pos.y < 36.0F) ? 36.0F : (sobj->pos.y > 400.0F) ? 400.0F : sobj->pos.y;

        sSCStaffrollCrosshairPositionX = sobj->pos.x - base_x;
        sSCStaffrollCrosshairPositionY = sobj->pos.y - base_y;

        gcSleepCurrentGObjThread(1);
    }
}

/* scstaffroll.c:1908-1929 scStaffrollMakeCrosshairGObj, verbatim: real
 * Sprite get through STAFFROLL_SPRITE. */
void scStaffrollMakeCrosshairGObj(void)
{
    GObj *gobj;
    SObj *sobj;

    gobj = gcMakeGObjSPAfter(3, NULL, 6, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 4, GOBJ_PRIORITY_DEFAULT, ~0);
    gcAddGObjProcess(gobj, scStaffrollCrosshairThreadUpdate, nGCProcessKindThread, 1);

    sobj = lbCommonMakeSObjForGObj(gobj, STAFFROLL_SPRITE(0x06d58));

    sSCStaffrollCrosshairGObj = gobj;

    sobj->sprite.attr = SP_TRANSPARENT;

    sobj->sprite.red   = 0xFF;
    sobj->sprite.green = 0x00;
    sobj->sprite.blue  = 0x00;

    sobj->sprite.scalex = 2.0F;
    sobj->sprite.scaley = 2.0F;
}

/* scstaffroll.c:1932-1963 scStaffrollMakeTextBoxBracketSObjs, verbatim:
 * real Sprite gets. */
void scStaffrollMakeTextBoxBracketSObjs(void)
{
    GObj *gobj;
    SObj *left_sobj;
    SObj *right_sobj;

    gobj = gcMakeGObjSPAfter(3, NULL, 8, GOBJ_PRIORITY_DEFAULT);

    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 7, GOBJ_PRIORITY_DEFAULT, ~0);

    left_sobj = lbCommonMakeSObjForGObj(gobj, STAFFROLL_SPRITE(0x06f98));

    gobj = gcMakeGObjSPAfter(3, NULL, 8, GOBJ_PRIORITY_DEFAULT);

    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 7, GOBJ_PRIORITY_DEFAULT, ~0);

    right_sobj = lbCommonMakeSObjForGObj(gobj, STAFFROLL_SPRITE(0x071d8));

    left_sobj->sprite.attr = right_sobj->sprite.attr = SP_TRANSPARENT;

    left_sobj->sprite.scalex = right_sobj->sprite.scalex = 2.0F;
    left_sobj->sprite.scaley = right_sobj->sprite.scaley = 2.4F;

    left_sobj->sprite.red   = right_sobj->sprite.red   = 0x78;
    left_sobj->sprite.green = right_sobj->sprite.green = 0x6E;
    left_sobj->sprite.blue  = right_sobj->sprite.blue  = 0x40;

    left_sobj->pos.y = right_sobj->pos.y = 30.0F;

    left_sobj->pos.x = 328.0F;
    right_sobj->pos.x = 588.0F;
}

/* Not decomp: see dSCStaffrollTextBoxDisplayList's own comment above.
 * The four rectangles and the fill colour are the same table's own
 * values, just drawn through lbCommonSpriteFillRect instead of a raw
 * Gfx DObj (+ 1 because G_CYC_FILL includes the lower-right edge); the
 * draw-list guard matches ifScreenFlashProcDisplay's. */
void scStaffrollTextBoxProcDisplay(GObj *gobj)
{
    (void)gobj;

    if (gcGetDrawList() != PVR_LIST_TR_POLY)
    {
        return;
    }

    lbCommonSpriteFillRect(346,  35, 348 + 1, 164 + 1, 0x42, 0x3A, 0x31, 0xFF);
    lbCommonSpriteFillRect(346,  35, 584 + 1,  37 + 1, 0x42, 0x3A, 0x31, 0xFF);
    lbCommonSpriteFillRect(582,  35, 584 + 1, 164 + 1, 0x42, 0x3A, 0x31, 0xFF);
    lbCommonSpriteFillRect(346, 162, 584 + 1, 164 + 1, 0x42, 0x3A, 0x31, 0xFF);
}

/* scstaffroll.c:1966-1972 scStaffrollMakeTextBoxGObj. DIVERGES: draws
 * through scStaffrollTextBoxProcDisplay above instead of a DObj built
 * out of dSCStaffrollTextBoxDisplayList's raw Gfx bytes. */
void scStaffrollMakeTextBoxGObj(void)
{
    GObj *gobj = gcMakeGObjSPAfter(4, NULL, 7, GOBJ_PRIORITY_DEFAULT);

    gcAddGObjDisplay(gobj, scStaffrollTextBoxProcDisplay, 9, GOBJ_PRIORITY_DEFAULT, ~0);
}

/* scstaffroll.c:1975-2022 scStaffrollScrollThreadUpdate, verbatim: the
 * scroll's own real timeline -- sSCStaffrollNameID advances for real
 * (scStaffrollJobAndNameInitStruct runs for real above), so this walks
 * the whole real job/staff table and reaches its own natural end. */
void scStaffrollScrollThreadUpdate(GObj *gobj)
{
    GObj *name_gobj = NULL;
    SCStaffrollJob *job;
    SCStaffrollName *name;
    sb32 is_queued_name;
    f32 interpolation;

    is_queued_name = TRUE;
    job = dSCStaffrollJobDescriptions;
    name = scStaffrollMakeJobGObj(job)->user_data.p;

    while (sSCStaffrollNameID < (s32) ARRAY_COUNT(dSCStaffrollStaffRoleTextInfo))
    {
        interpolation = (is_queued_name != FALSE) ? 0.15F : 0.3F;

        if (name->interpolation > interpolation)
        {
            if (is_queued_name != FALSE)
            {
                name_gobj = scStaffrollMakeNameGObjAndDObjs();

                name = name_gobj->user_data.p;

                if (++sSCStaffrollNameID == job->staff_count)
                {
                    is_queued_name = FALSE;
                }
            }
            else
            {
                job++;

                name = scStaffrollMakeJobGObj(job)->user_data.p;

                is_queued_name = TRUE;
            }
        }
        gcSleepCurrentGObjThread(1);
    }
    name = name_gobj->user_data.p;
    name->status = -1;

    sSCStaffrollScrollGObj = NULL;

    gcEjectGObj(NULL);
    gcSleepCurrentGObjThread(1);
}

/* scstaffroll.c:2025-2032 scStaffrollMakeScrollGObj, verbatim. */
void scStaffrollMakeScrollGObj(void)
{
    GObj *gobj = gcMakeGObjSPAfter(0, NULL, 1, GOBJ_PRIORITY_DEFAULT);

    gcAddGObjProcess(gobj, scStaffrollScrollThreadUpdate, nGCProcessKindThread, 1);

    sSCStaffrollScrollGObj = gobj;
}

/* scstaffroll.c:2035-2050 scStaffrollSetupFiles. DIVERGES: the reloc
 * loader is gone, replaced with the port's own sprite bank load (see
 * scstaffroll.h and this file's own "state" comment above
 * STAFFROLL_SPRITE) -- the same one-bank-load shape scexplain.c's own
 * scExplainLoadExplainFiles is, except this scene needs no gmCommonLoadFiles
 * call first (it is not battle-shaped -- no fighters, no shared HUD). */
void scStaffrollSetupFiles(void)
{
    if (sprite_bank_load(&sSCStaffrollGraphicsBank, "scstaffrollgraphics.spr") < 0)
    {
        sSCStaffrollGraphicsFileHead = NULL;
    }
    else
    {
        sSCStaffrollGraphicsFileHead = &sSCStaffrollGraphicsBank;
    }
#ifndef FT_HOSTTEST
    {
        static const char *const files[SCSTAFFROLL_GLYPH_PACKS] =
        { "scglyphs0.mdl", "scglyphs1.mdl" };
        int pal_bank = 0;
        s32 i;

        sSCStaffrollGlyphsLoaded = TRUE;

        for (i = 0; i < SCSTAFFROLL_GLYPH_PACKS; i++)
        {
            pal_bank = 0;

            if (fighter_load_scene(&sSCStaffrollGlyphPack[i], files[i],
                             &pal_bank) != 0)
            {
                sSCStaffrollGlyphsLoaded = FALSE;
            }
        }
        pal_bank = 0;

        sSCStaffrollPlaqueLoaded =
            (fighter_load_scene(&sSCStaffrollPlaquePack, "scplaque.mdl", &pal_bank) == 0);

        if (sSCStaffrollPlaqueLoaded)
        {
            fighter_clone(&sSCStaffrollPlaqueModel, &sSCStaffrollPlaquePack,
                          syTaskmanMalloc(sizeof(float[4]) *
                                          sSCStaffrollPlaquePack.hd->vert_count,
                                          0x8));
        }
    }
#endif
}

/* scstaffroll.c:2053-2102 scStaffrollInitNameAndJobDisplayLists.
 * DIVERGES: the whole body is a build-time step now.
 *
 * What the decomp does here is make each glyph's four Vtx and its
 * twelve-command display list out of two numbers -- the row's width and
 * height -- and a raw I4 texture block reached by
 * lbRelocGetFileData. tools/export/ssb_staffrollexport.py does exactly that,
 * off the same rows and the same blocks, at build time: joint i of
 * scglyphs.mdl is glyph i's quad, with the same four object-space
 * corners (+-width, +-height), the same texture coordinates (a full
 * width x height span of the tile) and the same two triangles
 * (3,2,1 / 0,3,1). The tile is baked from the ROM as I4 at the same
 * ((width + 15) / 16) * 16 stride and the same height the
 * gDPLoadTextureBlock_4b call above names, clamped both ways with the
 * same mask of 5.
 *
 * So all that is left at run time is cutting the payloads -- the same
 * "a pack with no run-time geometry step" shape
 * tools/export/ssb_shadowexport.py set for the blob shadow -- and the function
 * keeps its name and its place in scStaffrollFuncStart because that is still
 * what it does. */
void scStaffrollInitNameAndJobDisplayLists(void)
{
#ifndef FT_HOSTTEST
    s32 i;

    if (!sSCStaffrollGlyphsLoaded)
    {
        return;
    }
    for (i = 0; i < (s32)ARRAY_COUNT(sSCStaffrollNameAndJobDisplayLists); i++)
    {
        dc_model_init_payload(&sSCStaffrollNameAndJobDisplayLists[i],
                              &sSCStaffrollGlyphPack[i / SCSTAFFROLL_GLYPH_SPLIT],
                              i % SCSTAFFROLL_GLYPH_SPLIT);
    }
#endif
}

/* scstaffroll.c:2105-2118 scStaffrollInitVars. DIVERGES only in where
 * the three relocData reads land: the AnimJoint and the SYInterpDesc
 * are the compiled-in copies at the top of this file rather than
 * lbRelocGetFileData over sSCStaffrollFiles[0], and the DObjDesc has
 * become a pack loaded in scStaffrollSetupFiles above, so it has no
 * line here at all. sSCStaffrollPlayer is real and verbatim -- see
 * scstaffroll.h's header note. */
void scStaffrollInitVars(void)
{
    sSCStaffrollStatus = 2;
    sSCStaffrollNameID = 0;
    sSCStaffrollRollSpeed = 0.0037500001F;
    sSCStaffrollNameAllocFree = NULL;
    sSCStaffrollIsPaused = FALSE;
    sSCStaffrollNameInterpolation = dSCStaffrollNameInterpolationDesc;
    sSCStaffrollNameAnimJoint = (AObjEvent32 *)dSCStaffrollNameAnimJointScript;
    sSCStaffrollRollBeginWait = 0;
    sSCStaffrollPlayer = gSCManagerSceneData.player;
    sSCStaffrollRollEndWait = 60;
}

/* scstaffroll.c:2121-2127 scStaffrollUpdateCameraAt, verbatim. */
void scStaffrollUpdateCameraAt(GObj *gobj)
{
    CObj *cobj = CObjGetStruct(gobj);

    cobj->vec.at.x += (sSCStaffrollCrosshairPositionX * 0.25F);
    cobj->vec.at.y -= (sSCStaffrollCrosshairPositionY * 0.25F);
}

/* scstaffroll.c:2130-2183 scStaffrollMakeCamera, verbatim: no relocData
 * dependency (fixed eye/at/fovy), so this scene's own camera is real
 * and does track the crosshair. */
void scStaffrollMakeCamera(void)
{
    CObj *cobj = CObjGetStruct
    (
        gcMakeCameraGObj
        (
            5,
            NULL,
            12,
            GOBJ_PRIORITY_DEFAULT,
            lbCommonDrawSprite,
            30,
            COBJ_MASK_DLLINK(7) | COBJ_MASK_DLLINK(6) |
            COBJ_MASK_DLLINK(5) | COBJ_MASK_DLLINK(4),
            -1,
            0,
            1,
            NULL,
            1,
            0
        )
    );
    syRdpSetViewport(&cobj->viewport, 20.0F, 20.0F, 620.0F, 460.0F);

    sSCStaffrollCamera = cobj = CObjGetStruct
    (
        gcMakeCameraGObj
        (
            5,
            NULL,
            12,
            GOBJ_PRIORITY_DEFAULT,
            func_80017EC0,
            50,
            COBJ_MASK_DLLINK(9) | COBJ_MASK_DLLINK(8) |
            COBJ_MASK_DLLINK(3) | COBJ_MASK_DLLINK(2) |
            COBJ_MASK_DLLINK(1),
            -1,
            1,
            1,
            scStaffrollUpdateCameraAt,
            1,
            0
        )
    );
    syRdpSetViewport(&cobj->viewport, 20.0F, 20.0F, 620.0F, 460.0F);

    cobj->vec.eye.y = cobj->vec.at.x = cobj->vec.at.y = cobj->vec.at.z = 0.0F;

    cobj->vec.eye.x = 0.0F;
    cobj->vec.eye.z = 580.0F;

    cobj->projection.persp.fovy = 50.0F;
}

/* scstaffroll.c:2186-2201 scStaffrollFuncStart. DIVERGES in one word:
 * the clear camera keeps its GObj and
 * loses COBJ_FLAG_FILLCOLOR.
 *
 * src/dc/objdisplay.c's gcPrepCameraViewport says what that flag does
 * here and predicts this exactly: "a 3D camera after a fill would draw
 * under it, because the opaque list's depths are all behind the sprite
 * depths -- no scene the port runs has one". This scene is that scene.
 * On the RDP the fill is a framebuffer write that later cameras paint
 * over; here it is a quad at the frame's next sprite depth in the
 * translucent list, and both of scStaffrollMakeCamera's cameras run
 * AFTER it -- so every credit glyph, the name plaque and the side
 * textbox were drawn behind an opaque black rectangle, and the only
 * thing on screen was the crosshair, whose own sprite depth is later
 * still. That is what a disc probe of this step showed before the flag
 * came off: the packs load, the roll runs, fighter_draw_tr returns 26
 * to 34 triangles a line, and the screen never changes.
 *
 * Dropping it costs nothing, and it is the substitution the rest of the
 * port already makes: gcPrepCameraViewport's same comment says "every
 * other scene's clear camera is dropped for the PVR's own clear", and
 * src/dc/mnvsoptions.c, mnmaps.c, mndata.c, mnsoundtest.c,
 * mnbackupclear.c and scautodemo.c each document the drop at their own
 * call site (scexplain.c drops the whole camera). Only mntitle.c's two
 * cameras and mvopeningroom.c/mvending.c's still want the fill, and
 * none of those has a 3D camera behind it. The GObj and the colour are
 * kept so the call still reads against the decomp line by line. */
void scStaffrollFuncStart(void)
{
    gcMakeGObjSPAfter(0, scStaffrollFuncRun, 1, GOBJ_PRIORITY_DEFAULT);
    gcMakeDefaultCameraGObj(12, GOBJ_PRIORITY_DEFAULT, 100, 0, GPACK_RGBA8888(0x00, 0x00, 0x00, 0xFF));

    scStaffrollSetupFiles();
    scStaffrollInitNameAndJobDisplayLists();
    scStaffrollTryHideUnlocks();
    scStaffrollInitVars();
    scStaffrollMakeCrosshairGObj();
    scStaffrollMakeScrollGObj();
    scStaffrollMakeCamera();

    syAudioStopBGMAll();
    syAudioPlayBGM(0, nSYAudioBGMStaffroll);
}

/* scstaffroll.c:2215-2218 scStaffrollFuncLights: not ported, see this
 * file's own dSCStaffrollDisplayList comment above. dSCStaffrollTaskmanSetup's
 * own func_lights is NULL, the same as dMNOptionTaskmanSetup's. */

/* scstaffroll.c:2221-2249 scStaffrollFuncDraw (REGION_US arm): the
 * credits' own end -- the last name's roll sets status -1, which hands
 * scene_curr to nSCKindStartup, and sSCStaffrollRollEndWait's 60 frames
 * later the scene ends. It was defined here but never hooked up
 * (dSCStaffrollTaskmanSetup named scManagerFuncDraw), so the roll ran
 * to its end and the scene never left. DIVERGES:
 * syVideoSetFlags(SYVIDEO_FLAG_BLACKOUT) has no video mode to set here;
 * not drawing once it would have been set gives the same black second,
 * since the PVR clears every frame. */
void scStaffrollFuncDraw(void)
{
    if (sSCStaffrollStatus != -2)
    {
        gcDrawAll();
    }

    if (sSCStaffrollRollEndWait != 0)
    {
        if ((sSCStaffrollStatus == -1) || (sSCStaffrollStatus == -2))
        {
            sSCStaffrollRollEndWait--;
        }
    }
    if (sSCStaffrollRollEndWait == 0)
    {
        syTaskmanSetLoadScene();
    }
    if (sSCStaffrollStatus == -1)
    {
        gSCManagerSceneData.scene_curr = nSCKindStartup;

        syAudioStopBGMAll();

        sSCStaffrollStatus = -2;
    }
}

/* scstaffroll.c:2311-2338 scStaffrollStartScene. DIVERGES: syVideoInit
 * and the framebuffer-zeroing loops are the PVR's business; what
 * survives of dSCStaffrollVideoSetup is its 640x480, the one scene in
 * the game that is not 320x240. Every 2D position here (the crosshair's
 * 32..540 clamp, the textbox at x 328..588) and both cameras' 20..620
 * viewport are in those pixels, so dcVideoSetResolution sets the port's
 * scale to 1 for the scene and back to 2 after it. At 2 the crosshair
 * sat in the bottom-right corner and the textbox was off the screen.
 * scManagerFuncUpdate runs the task directly instead of
 * syTaskmanStartTask, the same substitution scexplain.c/scautodemo.c
 * already use; this scene sets scene_curr itself (scStaffrollFuncDraw
 * above), so this does not set it again either. */
void scStaffrollStartScene(void)
{
    dcVideoSetResolution(640, 480);

    dSCStaffrollTaskmanSetup.func_start = scStaffrollFuncStart;

    scManagerFuncUpdate(&dSCStaffrollTaskmanSetup);

    /* SYVIDEO_SETUP_DEFAULT's, which every other scene's syVideoInit
     * would put back on the N64 */
    dcVideoSetResolution(320, 240);

    gmRumbleInitPlayers();
}

/* scstaffroll.c:2266-2308 dSCStaffrollTaskmanSetup -- see scstaffroll.h's
 * header note. */
SYTaskmanSetup dSCStaffrollTaskmanSetup =
{
    {
        0,                              /* flags */
        gcRunAll,                       /* update function */
        scStaffrollFuncDraw,            /* frame draw function */
        NULL,                           /* allocatable memory pool start */
        0,                              /* allocatable memory pool size */
        1,                              /* ??? */
        2,                              /* number of contexts? */
        0, 0, 0, 0,                     /* the four DL buffer sizes */
        0,                              /* graphics heap size */
        2,                              /* ??? */
        0,                              /* RDP output buffer size */
        NULL,                           /* pre-render function: not ported, see dSCStaffrollLights1's comment */
        syControllerFuncRead,           /* controller I/O function */
    },

    16,                                 /* number of GObjThreads */
    sizeof(u64) * 192,                  /* thread stack size */
    16,                                 /* number of thread stacks */
    0,                                  /* ??? */
    64,                                 /* number of GObjProcesses */
    64,             sizeof(GObj),       /* number of GObjs */
    256,                                /* number of XObjs */
    NULL,                               /* matrix function list */
    NULL,                               /* DObjVec eject function */
    32,                                 /* number of AObjs */
    16,                                 /* number of MObjs */
    1024,           sizeof(DObj),       /* number of DObjs */
    256,            sizeof(SObj),       /* number of SObjs */
    8,              sizeof(CObj),       /* number of CObjs */

    scStaffrollFuncStart                /* task start function */
};

/* The port's own bzero arm for dSCManagerOverlays[59] (src/dc/overlay.c). */
void scStaffrollOverlayLoad(void)
{
#ifndef FT_HOSTTEST
    /* The two packs' own blobs go back before the clear, the same order
     * src/dc/mnplayersvs.c's own fighter_release/OVERLAY_CLEAR pair
     * uses: OVERLAY_CLEAR would drop the pointer and leak it. */
    if (sSCStaffrollGlyphsLoaded)
    {
        fighter_release(&sSCStaffrollGlyphPack[0]);
        fighter_release(&sSCStaffrollGlyphPack[1]);
    }
    if (sSCStaffrollPlaqueLoaded)
    {
        fighter_release(&sSCStaffrollPlaquePack);
    }
#endif
    OVERLAY_CLEAR(sSCStaffrollGlyphPack);
    OVERLAY_CLEAR(sSCStaffrollPlaquePack);
    OVERLAY_CLEAR(sSCStaffrollPlaqueModel);
    OVERLAY_CLEAR(sSCStaffrollGlyphsLoaded);
    OVERLAY_CLEAR(sSCStaffrollPlaqueLoaded);
    OVERLAY_CLEAR(sSCStaffrollGraphicsBank);
    OVERLAY_CLEAR(sSCStaffrollGraphicsFileHead);
    OVERLAY_CLEAR(sSCStaffrollNameAndJobDisplayLists);
    OVERLAY_CLEAR(sSCStaffrollNameID);
    OVERLAY_CLEAR(sSCStaffrollRollSpeed);
    OVERLAY_CLEAR(sSCStaffrollStatus);
    OVERLAY_CLEAR(sSCStaffrollNameAllocFree);
    OVERLAY_CLEAR(sSCStaffrollScrollGObj);
    OVERLAY_CLEAR(sSCStaffrollCrosshairGObj);
    OVERLAY_CLEAR(sSCStaffrollIsPaused);
    OVERLAY_CLEAR(sSCStaffrollCrosshairPositionX);
    OVERLAY_CLEAR(sSCStaffrollCrosshairPositionY);
    OVERLAY_CLEAR(sSCStaffrollNameAnimJoint);
    OVERLAY_CLEAR(sSCStaffrollNameInterpolation);
    OVERLAY_CLEAR(sSCStaffrollCamera);
    OVERLAY_CLEAR(sSCStaffrollHighlightSize);
    OVERLAY_CLEAR(sSCStaffrollHighlightPositionX);
    OVERLAY_CLEAR(sSCStaffrollHighlightPositionY);
    OVERLAY_CLEAR(sSCStaffrollStaffRoleTextGObj);
    OVERLAY_CLEAR(sSCStaffrollCompanyTextGObj);
    OVERLAY_CLEAR(sSCStaffrollRollBeginWait);
    OVERLAY_CLEAR(sSCStaffrollPlayer);
    OVERLAY_CLEAR(sSCStaffrollRollEndWait);
    OVERLAY_CLEAR(sSCStaffrollMatrix);
}

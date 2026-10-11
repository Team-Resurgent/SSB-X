/* scsubsysdata.c -- sc/scsubsys/scsubsysdata.c, the scene subsystem's
 * data: the two FTOpeningDesc tables a DEMO status is read out of, and
 * the per-kind scale a fighter stands at on a menu. Every table is the
 * decomp's, value for value, under the decomp's names.
 *
 * ftMainSetStatus sorts a status id into four bands (ft/ftmain.c:4550-
 * 4577). The two above FTSTAT_OPENING2_START land here rather than in a
 * status table: D_ovl1_80390BE8 for the fifteen common demo statuses
 * (nFTDemoStatusNull..IntroR) and D_ovl1_80390D20[fkind] for the
 * character-specific ones the opening movie adds on top. A row is a
 * motion id -- counted in the fighter's SubMotion table, NOT the
 * MainMotion one (src/dc/fighter.h FPackAttr.off_submotion) -- and the
 * one proc a demo fighter runs.
 *
 * This is what poses a fighter on the character select, on the results
 * screen, on the 1P game's stage cards, in the ending and on the
 * Continue screen, and in every opening movie. The poses
 * themselves are in the packs.
 *
 * D_ovl1_80390C60 is the sentinel a kind with no opening statuses of
 * its own gets -- its one row's motion id is 0xFFFFFFFF, which is not a
 * motion. ftMainGetOpeningDesc refuses it rather than indexing a table
 * with it. */
/* The decomp's own two includes, and deliberately not "ftcommon.h":
 * that pulls <ft/fighter.h>, which declares D_ovl1_80390BE8 as a plain
 * FTOpeningDesc for ft/ftmain.c's `&D_ovl1_80390BE8` while this file
 * defines it as the array it is. The decomp has the same mismatch and
 * keeps the two apart the same way. */
#include <ft/fttypes.h>
#include <sc/scsubsys/scsubsys.h>

/* Row 8's proc, the one thing in this file that is another scene's.
 * It declares no D_ovl1_* and so does not reopen the
 * mismatch above. */
#include "mvopeningroom.h"

/* scsubsysdata.c:6 D_ovl1_80390BE0, two unused words, is not carried. */

/* scsubsysdata.c:8-25, verbatim. The fifteen common demo statuses, in
 * nFTDemoStatus order, each naming the motion of the same number: row 0
 * is nFTDemoStatusNull at motion 0, row 14 is nFTDemoStatusIntroR at
 * motion 14. Three rows carry a proc.
 *
 * All three are the decomp's. Row 8 is
 * mvOpeningFighterProcUpdate, which forwards to
 * scSubsysFighterOpeningProcUpdate(sMVOpeningRoomBossGObj, ...) -- the
 * opening room's Master Hand holding the plucked trophy. Rows 9 and 10 are the figure
 * drop and the stand.
 *
 * This is the one row that reaches OUT of overlay 1: the proc lives in
 * mvopeningroom.c, overlay 34. That is the decomp's own arrangement --
 * the table is only ever read while the scene that owns the proc is the
 * running one -- and in this port every object is linked at once, so
 * the reference costs nothing but the include at the top of this
 * file. */
FTOpeningDesc D_ovl1_80390BE8[15] =
{
    { 0x00010000, NULL },
    { 0x00010001, NULL },
    { 0x00010002, NULL },
    { 0x00010003, NULL },
    { 0x00010004, NULL },
    { 0x00010005, NULL },
    { 0x00010006, NULL },
    { 0x00010007, NULL },
    { 0x00010008, mvOpeningFighterProcUpdate },
    { 0x00010009, scSubsysFighterApplyVelTransN },
    { 0x0001000A, scSubsysFighterApplyVelTransN },
    { 0x0001000B, NULL },
    { 0x0001000C, NULL },
    { 0x0001000D, NULL },
    { 0x0001000E, NULL }
};

/* scsubsysdata.c:27-30, the sentinel: a kind with no opening statuses of
 * its own. 0xFFFFFFFF is not a motion id and nothing may index it. */
FTOpeningDesc D_ovl1_80390C60[1] =
{
    { 0xFFFFFFFF, NULL }
};

/* scsubsysdata.c:32-79, the character-specific opening statuses, one
 * table per kind that has any: Mario 4, Fox 9, Link 1, Yoshi 4, Kirby 1,
 * Pikachu 1, Master Hand 3. Their motion ids carry on from the common
 * band's fourteen, so 0x1000F is SubMotion row 15 -- which for Master
 * Hand is the first of the three of Yoshi's animations his table
 * borrows. */
FTOpeningDesc D_ovl1_80390C68[4] =
{
    { 0x0001000F, NULL },
    { 0x00010010, NULL },
    { 0x00010011, NULL },
    { 0x00010012, NULL }
};

FTOpeningDesc D_ovl1_80390C88[9] =
{
    { 0x0001000F, NULL },
    { 0x00010010, NULL },
    { 0x00010011, NULL },
    { 0x00010012, NULL },
    { 0x00010013, NULL },
    { 0x00010014, NULL },
    { 0x00010015, NULL },
    { 0x00010016, NULL },
    { 0x00010017, NULL }
};

FTOpeningDesc D_ovl1_80390CD0[1] =
{
    { 0x0001000F, NULL }
};

FTOpeningDesc D_ovl1_80390CD8[1] =
{
    { 0x0001000F, NULL }
};

FTOpeningDesc D_ovl1_80390CE0[4] =
{
    { 0x0001000F, NULL },
    { 0x00010010, NULL },
    { 0x00010011, NULL },
    { 0x00010012, NULL }
};

FTOpeningDesc D_ovl1_80390D00[1] =
{
    { 0x0001000F, NULL }
};

FTOpeningDesc D_ovl1_80390D08[3] =
{
    { 0x0001000F, NULL },
    { 0x00010010, NULL },
    { 0x00010011, NULL }
};

/* scsubsysdata.c:83-112, verbatim: the table per FTKind, the sentinel
 * for the twenty kinds that have none of their own, and the NULL that
 * ends it. Twenty-eight entries for twenty-seven kinds. */
FTOpeningDesc *D_ovl1_80390D20[] =
{
    D_ovl1_80390C68,    /* Mario */
    D_ovl1_80390C88,    /* Fox */
    D_ovl1_80390C60,    /* Donkey */
    D_ovl1_80390C60,    /* Samus */
    D_ovl1_80390C60,    /* Luigi */
    D_ovl1_80390CD8,    /* Link */
    D_ovl1_80390CE0,    /* Yoshi */
    D_ovl1_80390C60,    /* Captain */
    D_ovl1_80390CD0,    /* Kirby */
    D_ovl1_80390D00,    /* Pikachu */
    D_ovl1_80390C60,    /* Purin */
    D_ovl1_80390C60,    /* Ness */
    D_ovl1_80390D08,    /* Boss */
    D_ovl1_80390C60,    /* MMario */
    D_ovl1_80390C60,
    D_ovl1_80390C60,
    D_ovl1_80390C60,
    D_ovl1_80390C60,
    D_ovl1_80390C60,
    D_ovl1_80390C60,
    D_ovl1_80390C60,
    D_ovl1_80390C60,
    D_ovl1_80390C60,
    D_ovl1_80390C60,
    D_ovl1_80390C60,
    D_ovl1_80390C60,
    D_ovl1_80390C60,
    NULL,
};

/* scsubsysdata.c:115-128 dSCSubsysFighterScales, verbatim: how large
 * each fighter stands on a menu (the character select's gates, the
 * results screen). */
f32 dSCSubsysFighterScales[] =
{
    1.25,
    1.15,
    1.00,
    1.03,
    1.21,
    1.33,
    1.05,
    1.07,
    1.22,
    1.20,
    1.26,
    1.30
};

/* No overlay loader: syDmaLoadOverlay's bzero arm clears .bss, and
 * every object in this file is initialised data. src/dc/overlay.h. */

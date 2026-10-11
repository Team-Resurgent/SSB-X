/* overlay.c -- the port's syDmaLoadOverlay (sys/dma.c:94) and the table
 * it reads (sc/scmanager.c:63 dSCManagerOverlays).
 *
 * src/dc/overlay.h has the argument. The short of it: entering a scene
 * bzeroes the noload segment of every overlay that scene loads, so a
 * module's statics are zero on every entry. The port has to do that
 * itself, and it has two halves to do it with -- a written-out clear
 * for each port file, because a port file's boundaries are not a decomp
 * file's, and a linker-bracketed address range for the decomp files the
 * port compiles unmodified, whose statics have no names outside them.
 */
#include "overlay.h"
#include "ftshadow.h"

#include <macros.h>
#include <sys/debug.h>

/* The game's Makefile compiles each of those decomp objects with one
 * .bss and renames it to ovlN_noload, which makes it an orphan output
 * section -- and ld synthesizes __start_NAME/__stop_NAME for exactly
 * those. (A linker script cannot do this job; the Makefile says why.)
 *
 * The references are deliberately not weak. ld only synthesizes the pair
 * when something has a strong undefined reference to it, and a build
 * that dropped an overlay's section should fail to link rather than
 * quietly get an empty range and skip the bzero -- which is the fault
 * this whole file exists to prevent. An overlay the port has no decomp
 * file in has no section and no symbols here at all: its entry in the
 * table below carries NULL, and syDmaLoadOverlay skips it the way the
 * game skips an overlay with an empty noload segment (sys/dma.c:109). */
extern char __start_ovl1_noload[], __stop_ovl1_noload[];
extern char __start_ovl2_noload[], __stop_ovl2_noload[];
extern char __start_ovl3_noload[], __stop_ovl3_noload[];

/* One arm of the port's dSCManagerOverlays. Two of SYOverlay's nine
 * fields (sys/dma.h:18-19) and one the N64 has no use for:
 *
 * DIVERGES: reload is the port's. On the N64 every file in an overlay
 * is inside the one address range, so one bzero covers the lot; here
 * the port's own files are ordinary objects in the always-resident
 * image, and each one clears itself. */
typedef struct DCOverlay
{
    void *ram_noload_start;
    void *ram_noload_end;
    void (*reload)(void);

} DCOverlay;

/* dSCManagerOverlays[1]: sc/scsubsys. scsubsysunused is not ported;
 * scsubsyscontroller is compiled unmodified and is in the segment. */
static void overlayReloadSubsys(void)
{
    scSubsysFighterOverlayLoad();
}

/* dSCManagerOverlays[2]: the battle's bulk. smashbrothers.us.yaml lists
 * forty-odd files in it; these are the ones the port has. The decomp
 * files it compiles unmodified out of this overlay -- mp/mpcollision,
 * mp/mpprocess, gm/gmcollision, ft/ftcommondata, ft/ftanim -- are in
 * the segment instead. */
static void overlayReloadBattle(void)
{
    /* smashbrothers.us.yaml:402 puts sc/sc1pmode/sc1pmanager at the
     * front of this overlay, which is where the 1P run's carried totals
     * live. Nothing inside a run fires this -- the router never reloads
     * its own overlay -- so what this clear really does is hand the next
     * run a fresh zero. src/dc/sc1pmanager.h has the argument. */
    sc1PManagerOverlayLoad();
    ftManagerOverlayLoad();
    ftMainOverlayLoad();
    ftParamOverlayLoad();
    mpCommonOverlayLoad();
    gmCommonOverlayLoad();
    gmCameraOverlayLoad();
    ifCommonOverlayLoad();
    ifScreenFlashOverlayLoad();
    grOverlayLoad();
}

/* dSCManagerOverlays[3]: ft/ftcommon, ft/ftcomputer, ft/ftshadow, wp/
 * and it/. src/dc/ftcommon.c is the port's stand-in for the first of
 * those. Only four of the overlay's 245 files have a .bss at all --
 * ft/ftcommon/ftcommonthrown2, ft/ftpublic, wp/wpmanager, it/itmanager
 * -- and ft/ftpublic is the one the port compiles, so the segment this
 * arm bzeroes is that file's eighteen statics: the audience's reaction
 * timer, the chant it is in the middle of, and the announcer's queue. */
static void overlayReloadFighting(void)
{
    ftCommonOverlayLoad();
    ftShadowOverlayLoad();
}

static const DCOverlay dSCManagerOverlays[] =
{
    /* [0] lb/lbcommon, lb/lbreloc, lb/lbfade, lb/lbtransition:
     * scManagerRunLoop loads it once at boot (scmanager.c:826) and no
     * scene reloads it, so its statics keep their state on purpose. */
    [OVERLAY_SUBSYS]     = { __start_ovl1_noload, __stop_ovl1_noload,
                             overlayReloadSubsys },
    [OVERLAY_BATTLE]     = { __start_ovl2_noload, __stop_ovl2_noload,
                             overlayReloadBattle },
    [OVERLAY_FIGHTING]   = { __start_ovl3_noload, __stop_ovl3_noload,
                             overlayReloadFighting },
    [OVERLAY_VSBATTLE]   = { NULL, NULL, scVSBattleOverlayLoad },
    [OVERLAY_1PBONUSSTAGE] = { NULL, NULL, sc1PBonusStageOverlayLoad },
    [OVERLAY_TRAININGMODE] = { NULL, NULL, sc1PTrainingModeOverlayLoad },
    [OVERLAY_TITLE]      = { NULL, NULL, mnTitleOverlayLoad },
    [OVERLAY_NOCONTROLLER] = { NULL, NULL, mnNoControllerOverlayLoad },
    [OVERLAY_MODESELECT] = { NULL, NULL, mnModeSelectOverlayLoad },
    [OVERLAY_1PMODE]     = { NULL, NULL, mn1PModeOverlayLoad },
    [OVERLAY_1PGAME]     = { NULL, NULL, mnPlayers1PGameOverlayLoad },
    [OVERLAY_1PTRAINING] = { NULL, NULL, mnPlayers1PTrainingOverlayLoad },
    [OVERLAY_1PBONUS]    = { NULL, NULL, mnPlayers1PBonusOverlayLoad },
    [OVERLAY_VSMODE]     = { NULL, NULL, mnVSModeOverlayLoad },
    [OVERLAY_VSOPTIONS]  = { NULL, NULL, mnVSOptionsOverlayLoad },
    [OVERLAY_ITEMSWITCH] = { NULL, NULL, mnVSItemSwitchOverlayLoad },
    [OVERLAY_MESSAGE]    = { NULL, NULL, mnMessageOverlayLoad },
    [OVERLAY_1PCHALLENGER] = { NULL, NULL, sc1PChallengerOverlayLoad },
    [OVERLAY_1PINTRO]    = { NULL, NULL, sc1PIntroOverlayLoad },
    [OVERLAY_1PGAMEPLAY] = { NULL, NULL, sc1PGameOverlayLoad },
    [OVERLAY_1PSTAGECLEAR] = { NULL, NULL, sc1PStageClearOverlayLoad },
    [OVERLAY_1PCONTINUE] = { NULL, NULL, mnPlayers1PGameContinueOverlayLoad },
    [OVERLAY_SCREENADJUST] = { NULL, NULL, mnScreenAdjustOverlayLoad },
    [OVERLAY_PLAYERSVS]  = { NULL, NULL, mnPlayersVSOverlayLoad },
    [OVERLAY_MAPS]       = { NULL, NULL, mnMapsOverlayLoad },
    [OVERLAY_VSRESULTS]  = { NULL, NULL, mnVSResultsOverlayLoad },
    [OVERLAY_VSRECORD]   = { NULL, NULL, mnVSRecordOverlayLoad },
    [OVERLAY_CHARACTERS] = { NULL, NULL, mnCharactersOverlayLoad },
    [OVERLAY_BACKUPCLEAR] = { NULL, NULL, mnBackupClearOverlayLoad },
    [OVERLAY_OPENINGROOM] = { NULL, NULL, mvOpeningRoomOverlayLoad },
    [OVERLAY_OPENINGPORTRAITS] = { NULL, NULL, mvOpeningPortraitsOverlayLoad },
    [OVERLAY_OPENINGMARIO] = { NULL, NULL, mvOpeningMarioOverlayLoad },
    [OVERLAY_OPENINGDONKEY] = { NULL, NULL, mvOpeningDonkeyOverlayLoad },
    [OVERLAY_OPENINGSAMUS] = { NULL, NULL, mvOpeningSamusOverlayLoad },
    [OVERLAY_OPENINGLINK] = { NULL, NULL, mvOpeningLinkOverlayLoad },
    [OVERLAY_OPENINGYOSHI] = { NULL, NULL, mvOpeningYoshiOverlayLoad },
    [OVERLAY_OPENINGKIRBY] = { NULL, NULL, mvOpeningKirbyOverlayLoad },
    [OVERLAY_OPENINGFOX] = { NULL, NULL, mvOpeningFoxOverlayLoad },
    [OVERLAY_OPENINGPIKACHU] = { NULL, NULL, mvOpeningPikachuOverlayLoad },
    [OVERLAY_OPENINGRUN] = { NULL, NULL, mvOpeningRunOverlayLoad },
    [OVERLAY_OPENINGCLIFF] = { NULL, NULL, mvOpeningCliffOverlayLoad },
    [OVERLAY_OPENINGYAMABUKI] = { NULL, NULL, mvOpeningYamabukiOverlayLoad },
    [OVERLAY_OPENINGJUNGLE] = { NULL, NULL, mvOpeningJungleOverlayLoad },
    [OVERLAY_OPENINGYOSTER] = { NULL, NULL, mvOpeningYosterOverlayLoad },
    [OVERLAY_OPENINGSECTOR] = { NULL, NULL, mvOpeningSectorOverlayLoad },
    [OVERLAY_OPENINGSTANDOFF] = { NULL, NULL, mvOpeningStandoffOverlayLoad },
    [OVERLAY_OPENINGCLASH] = { NULL, NULL, mvOpeningClashOverlayLoad },
    [OVERLAY_OPENINGNEWCOMERS] = { NULL, NULL, mvOpeningNewcomersOverlayLoad },
    [OVERLAY_ENDING]     = { NULL, NULL, mvEndingOverlayLoad },
    [OVERLAY_CONGRA]     = { NULL, NULL, mnCongraOverlayLoad },
    [OVERLAY_STARTUP]    = { NULL, NULL, mnStartupOverlayLoad },
    [OVERLAY_STAFFROLL]  = { NULL, NULL, scStaffrollOverlayLoad },
    [OVERLAY_OPTION]     = { NULL, NULL, mnOptionOverlayLoad },
    [OVERLAY_DATA]       = { NULL, NULL, mnDataOverlayLoad },
    [OVERLAY_SOUNDTEST]  = { NULL, NULL, mnSoundTestOverlayLoad },
    [OVERLAY_EXPLAIN]    = { NULL, NULL, scExplainOverlayLoad },
    [OVERLAY_AUTODEMO]   = { NULL, NULL, scAutoDemoOverlayLoad },
};

/* sys/dma.c:94. The DMA and the two cache invalidations have nothing to
 * do here -- the overlays are linked in, and the SH4's caches are not
 * holding a copy of a ROM the port does not read -- so the bzero is all
 * of syDmaLoadOverlay that survives, in the same order and under the
 * same guard.
 *
 * DIVERGES: an index the port has no arm for is said on the log and
 * ignored, rather than following a garbage SYOverlay. The game cannot
 * reach that case: its table has all sixty-six. */
void syDmaLoadOverlay(s32 index)
{
    const DCOverlay *ovl;

    if (index < 0 || index >= (s32) ARRAY_COUNT(dSCManagerOverlays))
    {
        syDebugPrintf("syDmaLoadOverlay: overlay %d is out of range\n",
                      (int) index);
        return;
    }

    ovl = &dSCManagerOverlays[index];

    if (ovl->reload != NULL)
    {
        ovl->reload();
    }
    if ((size_t) ((char *) ovl->ram_noload_end -
                  (char *) ovl->ram_noload_start) != 0)
    {
        memset(ovl->ram_noload_start, 0,
               (char *) ovl->ram_noload_end - (char *) ovl->ram_noload_start);
    }
}

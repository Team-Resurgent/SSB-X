/* overlay.h -- the port's stand-in for syDmaLoadOverlay.
 *
 * On the N64 the cartridge is large and the RAM is not, so most of the
 * game's code sits in ROM and is DMAed in a scene at a time, into a
 * region later scenes overwrite. Each such piece is an overlay.
 * sc/scmanager.c's loop (scmanager.c:867-1080) names the overlays a
 * scene needs and calls syDmaLoadOverlay on each one before starting
 * it, and that function ends with
 *
 *     if ((ovl->ram_noload_end - ovl->ram_noload_start) != 0)
 *         bzero((void*) ovl->ram_noload_start,
 *               ovl->ram_noload_end - ovl->ram_noload_start);
 *
 * (sys/dma.c:109-112). So an overlay's .data comes back off the ROM and
 * its .bss is cleared *on every entry to the scene*: a scene module's
 * statics start at zero every time, not just the first time.
 *
 * The port links the overlays in instead of loading them, so nothing
 * clears them, and a scene entered twice reads what it left behind the
 * first time. That is not a slow drift -- the scene heap is reset
 * between scenes (src/dc/taskman.c), so every GObj*, SObj* and CObj* a
 * module kept is a pointer into memory the next scene has already
 * handed out to something else. mnPlayersVSMakeGameRuleSelector
 * (src/dc/mnplayersvs.c:1426-1428) is the one that found this: it
 * ejects sMNPlayersVSGameRuleGObj before remaking it, and on the second
 * visit to the character select that eject reads a GObj out of sprite
 * data and writes its link_next into gGCCommonLinks[25]. The next
 * gcRunAll walks that list into an SObj and calls its 13x90 sprite
 * dimensions as a function pointer.
 *
 * So the port has to write that bzero out, and it has two ways to,
 * because it has two kinds of file in an overlay:
 *
 *  - A module the port wrote clears its own statics, in an
 *    xxxOverlayLoad() built out of OVERLAY_CLEAR. It has to be written
 *    out because a port file's boundaries are not a decomp file's --
 *    src/dc/stage.c stands in for nine gr/ files *and* holds the
 *    boot-loaded stage table, and only one of those two is
 *    the overlay's.
 *  - A decomp .c the port compiles unmodified cannot be reached that
 *    way at all: its statics are file-scope, and there is no name for
 *    them outside the file. The N64 did not use names either. It
 *    bzeroed an address range the linker gave it, and so does the port:
 *    src/game/ssb64/overlay.ld puts each overlay's objects in
 *    their own output section, and dSCManagerOverlays carries the
 *    bracketing symbols as ram_noload_start/ram_noload_end.
 *
 * DIVERGES: the d-prefixed tables are the overlay's .data, which the
 * DMA restores from ROM unchanged and the game never writes, so nothing
 * here clears them. Two of them are .bss in the port because it fills
 * them at runtime from the romdisk where the N64 had them in the
 * image -- gFTManagerModels (the port's dFTManagerDataFiles) and
 * gGRStages (the port's map-file table); tools/check/overlay_check.py carries
 * both in its EXCLUDE list with that reason, and it re-derives
 * everything else from the source and from smashbrothers.us.yaml, so a
 * module that grows a variable the reload does not clear fails the
 * build.
 */
#ifndef SSB_DC_OVERLAY_H
#define SSB_DC_OVERLAY_H

#include <ssb_types.h>
#include <string.h>

/* one variable of an overlay's noload segment */
#define OVERLAY_CLEAR(v) memset(&(v), 0, sizeof(v))

/* sys/dma.c:94 syDmaLoadOverlay, reduced to its last step: with nothing
 * to DMA and no caches to invalidate, the bzero is all of it that means
 * anything here. The index is the game's own -- scManagerRunScene says
 * syDmaLoadOverlay(&dSCManagerOverlays[2]) and so does src/dc/scmanager.c.
 *
 * DIVERGES: the game passes the SYOverlay by pointer, out of a table
 * that is itself in the always-resident segment; the port takes the
 * index, because its table is private to src/dc/overlay.c and holds a
 * reload hook the N64's has no need for. */
void syDmaLoadOverlay(s32 index);

/* The overlays the port has files in. dSCManagerOverlays' own numbering
 * (sc/scmanager.c:63); the names are what smashbrothers.us.yaml puts in
 * each. Overlay 0 (lb/lbcommon, lb/lbreloc, lb/lbfade, lb/lbtransition)
 * is loaded once by scManagerRunLoop at boot and no scene reloads it,
 * so it keeps its state on purpose and is not here. */
#define OVERLAY_SUBSYS      1   /* sc/scsubsys */
#define OVERLAY_BATTLE      2   /* ft, mp, gm, gr, if, ef -- the bulk */
#define OVERLAY_FIGHTING    3   /* ft/ftcommon, ft/ftcomputer, wp, it */
#define OVERLAY_VSBATTLE    4   /* sc/sccommon/scvsbattle */
#define OVERLAY_1PBONUSSTAGE 6  /* sc/sc1pmode/sc1pbonusstage */
#define OVERLAY_TRAININGMODE 7  /* sc/sc1pmode/sc1ptrainingmode */
#define OVERLAY_TITLE      10   /* mn/mncommon/mntitle */
#define OVERLAY_NOCONTROLLER 11 /* mn/mncommon/mnnocontroller */
#define OVERLAY_MODESELECT 17   /* mn/mncommon/mnmodeselect */
#define OVERLAY_1PMODE     18   /* mn/mn1pmode/mn1pmode */
#define OVERLAY_VSMODE     19   /* mn/mnvsmode/mnvsmode */
#define OVERLAY_VSOPTIONS  20   /* mn/mnvsmode/mnvsoptions */
#define OVERLAY_ITEMSWITCH 21   /* mn/mnvsmode/mnvsitemswitch */
#define OVERLAY_MESSAGE    22   /* mn/mncommon/mnmessage */
#define OVERLAY_1PCHALLENGER 23 /* sc/sc1pmode/sc1pchallenger */
#define OVERLAY_1PINTRO    24   /* sc/sc1pmode/sc1pintro */
#define OVERLAY_1PGAMEPLAY 65   /* sc/sc1pmode/sc1pgame */
#define OVERLAY_1PCONTINUE 55   /* mn/mn1pmode/mn1pcontinue */
#define OVERLAY_1PSTAGECLEAR 56 /* sc/sc1pmode/sc1pstageclear */
#define OVERLAY_SCREENADJUST 25 /* mn/mnoption/mnscreenadjust */
#define OVERLAY_PLAYERSVS  26   /* mn/mnplayers/mnplayersvs */
#define OVERLAY_1PGAME     27   /* mn/mnplayers/mnplayers1pgame */
#define OVERLAY_1PTRAINING 28   /* mn/mnplayers/mnplayers1ptraining */
#define OVERLAY_1PBONUS    29   /* mn/mnplayers/mnplayers1pbonus */
#define OVERLAY_MAPS       30   /* mn/mnmaps/mnmaps */
#define OVERLAY_VSRESULTS  31   /* mn/mnvsmode/mnvsresults */
#define OVERLAY_VSRECORD   32   /* mn/mndata/mnvsrecord */
#define OVERLAY_CHARACTERS 33   /* mn/mndata/mncharacters */
#define OVERLAY_OPENINGROOM 34  /* mv/mvopening/mvopeningroom */
#define OVERLAY_OPENINGPORTRAITS 35 /* mv/mvopening/mvopeningportraits */
#define OVERLAY_OPENINGMARIO 36 /* mv/mvopening/mvopeningmario */
#define OVERLAY_OPENINGDONKEY 37 /* mv/mvopening/mvopeningdonkey */
#define OVERLAY_OPENINGSAMUS 38 /* mv/mvopening/mvopeningsamus */
#define OVERLAY_OPENINGLINK 40  /* mv/mvopening/mvopeninglink */
#define OVERLAY_OPENINGYOSHI 41 /* mv/mvopening/mvopeningyoshi */
#define OVERLAY_OPENINGKIRBY 43 /* mv/mvopening/mvopeningkirby */
#define OVERLAY_OPENINGFOX 39 /* mv/mvopening/mvopeningfox */
#define OVERLAY_OPENINGPIKACHU 42 /* mv/mvopening/mvopeningpikachu */
#define OVERLAY_OPENINGRUN 44 /* mv/mvopening/mvopeningrun */
#define OVERLAY_OPENINGCLIFF 46 /* mv/mvopening/mvopeningcliff */
#define OVERLAY_OPENINGYAMABUKI 48 /* mv/mvopening/mvopeningyamabuki */
#define OVERLAY_OPENINGJUNGLE 51 /* mv/mvopening/mvopeningjungle */
#define OVERLAY_OPENINGYOSTER 45 /* mv/mvopening/mvopeningyoster */
#define OVERLAY_OPENINGSECTOR 50 /* mv/mvopening/mvopeningsector */
#define OVERLAY_OPENINGSTANDOFF 47 /* mv/mvopening/mvopeningstandoff */
#define OVERLAY_OPENINGCLASH 49 /* mv/mvopening/mvopeningclash */
#define OVERLAY_OPENINGNEWCOMERS 52 /* mv/mvopening/mvopeningnewcomers */
#define OVERLAY_BACKUPCLEAR 53  /* mn/mnoption/mnbackupclear */
#define OVERLAY_ENDING      54  /* mv/mvending/mvending */
#define OVERLAY_CONGRA      57  /* mn/mncommon/mncongra */
#define OVERLAY_STARTUP     58  /* mn/mncommon/mnstartup */
#define OVERLAY_STAFFROLL   59  /* sc/sccommon/scstaffroll */
#define OVERLAY_OPTION     60   /* mn/mnoption/mnoption */
#define OVERLAY_DATA       61   /* mn/mndata/mndata */
#define OVERLAY_SOUNDTEST  62   /* mn/mndata/mnsoundtest */
#define OVERLAY_EXPLAIN    63   /* sc/sccommon/scexplain (REGION_US) */
#define OVERLAY_AUTODEMO   64   /* sc/sccommon/scautodemo (REGION_US) */

/* The per-module halves, one per port file that stands in for a decomp
 * file in an overlay. Only src/dc/overlay.c calls these -- a scene
 * calls syDmaLoadOverlay, the way the game does -- but they are the
 * unit tools/check/overlay_check.py checks, and the comment beside each names
 * the overlay it is part of. */
void scSubsysFighterOverlayLoad(void);  /* dSCManagerOverlays[1] */
void sc1PManagerOverlayLoad(void);      /* dSCManagerOverlays[2] */
void ftManagerOverlayLoad(void);        /* dSCManagerOverlays[2] */
void ftMainOverlayLoad(void);           /* dSCManagerOverlays[2] */
void ftParamOverlayLoad(void);          /* dSCManagerOverlays[2] */
void mpCommonOverlayLoad(void);         /* dSCManagerOverlays[2] */
void gmCommonOverlayLoad(void);         /* dSCManagerOverlays[2] */
void gmCameraOverlayLoad(void);         /* dSCManagerOverlays[2] */
void ifCommonOverlayLoad(void);         /* dSCManagerOverlays[2] */
void ifScreenFlashOverlayLoad(void);    /* dSCManagerOverlays[2] */
void grOverlayLoad(void);               /* dSCManagerOverlays[2] */
void ftCommonOverlayLoad(void);         /* dSCManagerOverlays[3] */
void scVSBattleOverlayLoad(void);       /* dSCManagerOverlays[4] */
void sc1PBonusStageOverlayLoad(void);   /* dSCManagerOverlays[6] */
void mnTitleOverlayLoad(void);          /* dSCManagerOverlays[10] */
void mnNoControllerOverlayLoad(void);   /* dSCManagerOverlays[11] */
void mnModeSelectOverlayLoad(void);     /* dSCManagerOverlays[17] */
void mn1PModeOverlayLoad(void);         /* dSCManagerOverlays[18] */
void mnPlayers1PTrainingOverlayLoad(void);
void sc1PTrainingModeOverlayLoad(void);
void mnPlayers1PGameOverlayLoad(void); /* dSCManagerOverlays[28] */
void mnPlayers1PBonusOverlayLoad(void); /* dSCManagerOverlays[29] */
void mnVSModeOverlayLoad(void);         /* dSCManagerOverlays[19] */
void mnVSOptionsOverlayLoad(void);      /* dSCManagerOverlays[20] */
void mnVSItemSwitchOverlayLoad(void);    /* dSCManagerOverlays[21] */
void mnMessageOverlayLoad(void);        /* dSCManagerOverlays[22] */
void sc1PChallengerOverlayLoad(void);   /* dSCManagerOverlays[23] */
void sc1PIntroOverlayLoad(void);        /* dSCManagerOverlays[24] */
void sc1PGameOverlayLoad(void);         /* dSCManagerOverlays[65] */
void sc1PStageClearOverlayLoad(void);   /* dSCManagerOverlays[56] */
void mnPlayers1PGameContinueOverlayLoad(void); /* dSCManagerOverlays[55] */
void mnScreenAdjustOverlayLoad(void);   /* dSCManagerOverlays[25] */
void mnPlayersVSOverlayLoad(void);      /* dSCManagerOverlays[26] */
void mnMapsOverlayLoad(void);           /* dSCManagerOverlays[30] */
void mnVSResultsOverlayLoad(void);      /* dSCManagerOverlays[31] */
void mnVSRecordOverlayLoad(void);       /* dSCManagerOverlays[32] */
void mnCharactersOverlayLoad(void);     /* dSCManagerOverlays[33] */
void mvOpeningRoomOverlayLoad(void);    /* dSCManagerOverlays[34] */
void mvOpeningPortraitsOverlayLoad(void); /* dSCManagerOverlays[35] */
void mvOpeningMarioOverlayLoad(void);   /* dSCManagerOverlays[36] */
void mvOpeningDonkeyOverlayLoad(void);  /* dSCManagerOverlays[37] */
void mvOpeningSamusOverlayLoad(void);   /* dSCManagerOverlays[38] */
void mvOpeningLinkOverlayLoad(void);    /* dSCManagerOverlays[40] */
void mvOpeningYoshiOverlayLoad(void);   /* dSCManagerOverlays[41] */
void mvOpeningKirbyOverlayLoad(void);   /* dSCManagerOverlays[43] */
void mvOpeningFoxOverlayLoad(void);     /* dSCManagerOverlays[39] */
void mvOpeningPikachuOverlayLoad(void); /* dSCManagerOverlays[42] */
void mvOpeningRunOverlayLoad(void);     /* dSCManagerOverlays[44] */
void mvOpeningCliffOverlayLoad(void);   /* dSCManagerOverlays[46] */
void mvOpeningYamabukiOverlayLoad(void); /* dSCManagerOverlays[48] */
void mvOpeningJungleOverlayLoad(void);  /* dSCManagerOverlays[51] */
void mvOpeningYosterOverlayLoad(void);  /* dSCManagerOverlays[45] */
void mvOpeningSectorOverlayLoad(void);  /* dSCManagerOverlays[50] */
void mvOpeningStandoffOverlayLoad(void); /* dSCManagerOverlays[47] */
void mvOpeningClashOverlayLoad(void);   /* dSCManagerOverlays[49] */
void mvOpeningNewcomersOverlayLoad(void); /* dSCManagerOverlays[52] */
void mnBackupClearOverlayLoad(void);    /* dSCManagerOverlays[53] */
void mvEndingOverlayLoad(void);         /* dSCManagerOverlays[54] */
void mnCongraOverlayLoad(void);         /* dSCManagerOverlays[57] */
void mnStartupOverlayLoad(void);        /* dSCManagerOverlays[58] */
void scStaffrollOverlayLoad(void);      /* dSCManagerOverlays[59] */
void mnOptionOverlayLoad(void);         /* dSCManagerOverlays[60] */
void mnDataOverlayLoad(void);           /* dSCManagerOverlays[61] */
void mnSoundTestOverlayLoad(void);      /* dSCManagerOverlays[62] */
void scExplainOverlayLoad(void);        /* dSCManagerOverlays[63] */
void scAutoDemoOverlayLoad(void);       /* dSCManagerOverlays[64] */

#endif /* SSB_DC_OVERLAY_H */

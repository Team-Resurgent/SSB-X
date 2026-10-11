/* Shadows ssb-decomp-re/src/sc/scene.h, the scene module's umbrella
 * (sctypes.h, scoverlay.h, scfunctions.h). Two of the three are the
 * overlay table and the whole scene-function surface, which the port
 * does not have; the first is the scene *types*, and the port wants the
 * real ones: SCBattleState and SCPlayerData
 * are what ftManagerMakeFighter and ftParamInitPlayerBattleStats fill
 * in (ft/ftmanager.c:691, ft/ftparam.c:158), and a hardcoded battle
 * state is how the port boots straight into a match. So sctypes.h comes in
 * whole, ahead of it the two enum headers it reaches for and its own
 * includes do not supply (gm/gmdef.h through the gm/generic.h shim,
 * lb/lbdef.h for nLBBackupUnlockEnumCount).
 *
 * Eight symbols are declared here that the real scfunctions.h and
 * sc/scmanager.c would:
 *   gSCManagerBattleState        sc/scmanager.c:42, defined in mpshim.c
 *   the six scene-manager globals below, which lb/lbbackup.c reaches
 *        for and src/dc/scmanager.c defines
 *   scManagerRunPrintGObjStatus  the game's "print every GObj and spin
 *        forever" debug halt that every `while (TRUE) { syDebugPrintf
 *        (...); ... }` block in mpcollision.c calls. mpshim.c aborts
 *        instead of spinning. */
#ifndef _SCENE_H_
#define _SCENE_H_

#include <ssb_types.h>
#include <lb/lbdef.h>
#include <lb/lbtypes.h>          /* LBBackupData, for the extern below */
#include <sc/sctypes.h>

extern SCBattleState *gSCManagerBattleState;

/* sc/scmanager.c:27, 31, 36 and the three d-prefixed defaults at :135,
 * :487 and :564. The real sc/scmanager.h declares all six; the port's
 * src/dc/scmanager.c defines them, and they are declared here because
 * lb/lbbackup.c -- which the port compiles unmodified -- reaches for
 * every one of them.
 *
 * gSCManagerBackupData is the save data. It is what lbBackupIsSramValid leaves
 * behind, which on a store with nothing in it is
 * dSCManagerDefaultBackupData, the same as a fresh cartridge. */
extern LBBackupData gSCManagerBackupData, dSCManagerDefaultBackupData;
extern SCCommonData gSCManagerSceneData, dSCManagerDefaultSceneData;
extern SCBattleState gSCManagerTransferBattleState, dSCManagerDefaultBattleState;

extern void scManagerRunPrintGObjStatus(void);

/* sc/sc1pmode/sc1pgame.h:38-39, reached in the game through this same
 * umbrella (sc/scene.h -> scfunctions.h). The 1P game's bonus tallies for
 * food eaten, which ft/ftcommon/ftcommonget.c's ftCommonLightGetProcDamage
 * bumps when the 1P player takes a Maxim Tomato or a Heart Container --
 * and that file is compiled unmodified, so the
 * declaration has to be somewhere it sees. src/dc/itmain.c defines them,
 * beside gSC1PGameBonusMewCatcher, which arrived the same way. */
extern u8 gSC1PGameBonusTomatoCount;
extern u8 gSC1PGameBonusHeartCount;

/* sc/sc1pmode/sc1pgame.h:31, :34 and :37, for ft/ftmain.c's hit stats
 * (src/dc/ftmain.c), which count a Star taken and flag a 20-damage clash
 * or hit on the 1P player, and ftCommonShieldBreakFlyCommonSetStatus
 * (src/dc/ftcommon.c), which flags a shield the 1P player broke.
 * src/dc/sc1pgame.c defines all three. */
extern u8 gSC1PGameBonusStarCount;
extern ub8 gSC1PGameBonusShieldBreaker;
extern ub8 gSC1PGameBonusGiantImpact;

/* Not sc/'s at all, and here because this is the header lb/lbbackup.c
 * includes that the port owns. That file calls syDmaReadSram,
 * syDmaWriteSram (sys/dma.h) and syAudioSetQuality (sys/audio.h) with
 * none of the three declared: the umbrella it reaches them through is
 * sc/scene.h -> scfunctions.h, which the port does not have, and the
 * decomp's own build lets the implicit declarations stand because it is
 * matching a binary and they cost it nothing. They would cost the port
 * something. An implicitly declared syDmaReadSram returns int and takes
 * whatever it is handed, so the call is correct only for as long as
 * uintptr_t, size_t and a pointer are all one register wide -- true on
 * both the N64 and the SH-4, and not a thing to leave uncheckable.
 *
 * sys/dma.h itself cannot come in -- its other three declarations are
 * about OSPiHandle, which is libultra's DMA descriptor and one of the
 * things the port does not have -- so the two lines the port needs are
 * copied here from sys/dma.h:40-41. sys/audio.h comes in whole. */
extern void syDmaReadSram(uintptr_t rom_src, void *ram_dst, size_t size);
extern void syDmaWriteSram(void *ram_src, uintptr_t rom_dst, size_t size);

#include <sys/audio.h>

#endif /* _SCENE_H_ */

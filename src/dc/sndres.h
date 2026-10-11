/* sndres.h -- which samples are in sound RAM, scene by scene.
 *
 * DIVERGES: on the N64 both sample banks are resident for the life of
 * the cartridge -- B1_sounds2's 322 sound effects for the FGM engine and
 * B1_sounds1's 117 instrument waves for the sequencer. The Dreamcast has
 * 2 MB of AICA sound RAM for both, and the VS game alone reaches about
 * 1.97 MB of sound effects and 0.89 MB of music as ADPCM. So the port
 * keeps a scene's samples instead: tools/export/ssb_sndsets.py measures, out of
 * the decomp and the ROM, which samples each group of game code can
 * reach (sndsets.bin), and sndres_enter_scene composes the next scene's
 * need out of those groups and uploads it -- only when it is not already
 * resident, so the menus, and the character select and the stage select,
 * share one upload. The same tool fails the build if any scene's worst
 * case (the four most expensive fighters on the most expensive stage)
 * does not fit.
 *
 * What plays does not change, with one exception at the seams: a swap
 * stops every sound and the music the moment the next scene starts, so
 * a cue still ringing out of the scene before (the stage select's
 * confirm, say) is cut where on the N64 it would finish over the load.
 * Only the swaps do this -- menu to menu, and the character select to
 * the stage select and back, share a set and never swap. A sample a
 * scene can reach and the census did not find is silent, as an unstaged
 * one always was, and logged by the engine that asked for it (fgm.c,
 * aica/seqsyn.c), once per swap.
 */
#ifndef DC_SNDRES_H
#define DC_SNDRES_H

#include <stdint.h>

#include <ssb_types.h>
#include <gr/grdef.h>       /* nGRKindCommonEnd, for SNDRES_STAGES */

/* sndsets.bin's groups, in tools/export/ssb_sndsets.py GROUPS order. */
#define SNDRES_GROUP_MENU        0
#define SNDRES_GROUP_CORE        1
#define SNDRES_GROUP_RESULTS     2
#define SNDRES_GROUP_FIGHTER     3      /* + nFTKind, 12 of them */
#define SNDRES_FIGHTERS          12
/* + nGRKind, to the end of the common stages. It was the nine VS stages
 * while those were the only ones the port had a pack for; Final
 * Destination is the first past nGRKindBattleEnd, and the
 * seven kinds between it and Mushroom Kingdom -- Beta Dream Land, the
 * Test Stage, How to Play, Small Yoshi's Island, Meta Crystal, Duel
 * Zone and Race to the Finish --
 * are held open here rather than renumbered in one at a time, because a
 * group index is an index into sndsets.bin and moving one moves every
 * group after it. A kind with no pack has an EMPTY group, which costs
 * nothing to stage and is what a gkind the port cannot play already
 * got. src/dc/stage.h's GR_STAGES_MAX is taken to the same end for the
 * same reason. */
#define SNDRES_GROUP_STAGE       15
#define SNDRES_STAGES            (nGRKindCommonEnd + 1)
/* The ending diorama and the staff roll: a fixed, never-
 * player-chosen composition -- one hardcoded fighter (mvending.h's own
 * header note) and no stage -- so it is its own group rather than
 * folded into SNDRES_GROUP_MENU: nSYAudioBGMEnding and
 * nSYAudioBGMStaffroll together are real weight (tools/export/ssb_sndsets.py's
 * own scene_worst prints it), and MENU already has to fit alongside
 * every fighter at once for the character select screen -- a co-
 * occurrence CREDITS never has. */
#define SNDRES_GROUP_CREDITS     (SNDRES_GROUP_STAGE + SNDRES_STAGES)
/* The openings: the same reasoning again, and the same
 * answer. All nineteen opening scenes share one BGM track,
 * nSYAudioBGMOpening, started once by mvOpeningRoomFuncStart and left
 * running across the whole chain (no other opening file calls
 * syAudioPlayBGM at all), plus a handful of FGM stingers the later
 * scenes fire. The openings never co-occur with the menus, the results
 * screen or a full roster, so OPENING is its own group rather than more
 * weight on MENU. */
#define SNDRES_GROUP_OPENING     (SNDRES_GROUP_CREDITS + 1)
/* The 1P ladder's VS card: the same reasoning a third
 * time. sc1pintro.c starts one of two tracks -- nSYAudioBGM1PIntro on
 * twelve of the fourteen rungs, nSYAudioBGMBossStage on the last -- and
 * the card never co-occurs with a menu, a battle or the results screen,
 * since the manager runs it between them. Its own group therefore,
 * rather than two more tracks on CORE, which every battle would then
 * carry. The card's FIGHTERS come from its compose arm
 * (src/dc/sndres.c), not from here: which two they are is the ladder's
 * to say. */
#define SNDRES_GROUP_1PINTRO     (SNDRES_GROUP_OPENING + 1)
/* A rung of the ladder itself. Unlike the four groups
 * above this one is taken ON TOP OF CORE -- a rung IS a battle, and
 * every sound a VS battle can play a rung can play too. What it holds
 * is the ladder's own: sc1pgame.c's cues, and the one BGM track the
 * scene names, nSYAudioBGMLast (Final Destination), which
 * sc1PGameWaitStageBossUpdate hands to gMPCollisionBGMDefault because
 * the Master Hand rung's map has no pack to carry it. Measuring those
 * into CORE instead put an ordinary VS battle 131 KB over the budget,
 * which is what decided this; tools/export/ssb_sndsets.py's "1p rung" row is
 * where that is checked, rung by rung. */
#define SNDRES_GROUP_1PGAME      (SNDRES_GROUP_1PINTRO + 1)
/* "CHALLENGER APPROACHING!", and the lightest group here.
 * sc1pchallenger.c starts exactly one track, nSYAudioBGM1PChallenger,
 * which no other scene in the port names, and it stands exactly one
 * fighter -- the challenger, whose own group its compose arm adds. It
 * is substitutive like SNDRES_GROUP_1PINTRO above and not additive like
 * SNDRES_GROUP_1PGAME: the card is not a battle, so it takes no CORE.
 * The one FGM it plays, nSYAudioFGMDeadUpStar, is CORE's everywhere
 * else (ft/ftcommon/ftcommondead.c), so this group holds a copy of that
 * one sample rather than drag the whole of CORE in behind it -- which
 * is what tools/export/ssb_sndsets.py's classify() arm for sc1pchallenger.c
 * is for. */
#define SNDRES_GROUP_1PCHALLENGER (SNDRES_GROUP_1PGAME + 1)
/* The score screen after a rung, substitutive like the
 * two cards above it and for the same reason -- it is not a battle,
 * and it stands no fighter at all, so this is the one group in this
 * file that is a whole scene's set on its own. It carries four BGM
 * tracks (the stage-clear and game-clear jingles and the bonus
 * stage's pass and fail) and the three cues the score counts up
 * with, none of which any other ported scene names. */
#define SNDRES_GROUP_1PSTAGECLEAR (SNDRES_GROUP_1PCHALLENGER + 1)
/* The "CONTINUE?" screen, substitutive like the two cards
 * and the score screen -- it is not a battle either. Two BGM tracks
 * (the choice jingle and the game-over one), the announcer saying both
 * words, and two cues: the continue stinger and the menu scroll. The
 * scroll is CORE's everywhere else, so this group holds a copy of it
 * for the same reason SNDRES_GROUP_1PCHALLENGER holds the star. */
#define SNDRES_GROUP_1PCONTINUE  (SNDRES_GROUP_1PSTAGECLEAR + 1)
/* The three BONUS STAGES' own set: gr/grbonus/'s files and
 * the two items only they make, the target and the Race to the Finish
 * barrel. It went in one step ahead of anything that composes it, because
 * the port already COMPILED a file naming its sounds
 * (src/dc/ittarubomb.c) and an id a shipped file can play with no group
 * holding it is exactly the "not resident" silence this file was built to
 * make impossible. nSCKind1PBonusStage composes it --
 * additively, like SNDRES_GROUP_1PGAME, since a bonus stage IS a battle
 * and takes SNDRES_GROUP_CORE and its course's stage group with it. See
 * tools/export/ssb_sndsets.py's own note on GROUPS for why it is last in the
 * list. */
#define SNDRES_GROUP_1PBONUS     (SNDRES_GROUP_1PCONTINUE + 1)
/* The attract loop's character showcase: one track,
 * nSYAudioBGMExplain, which mnTitleProceedDemoNext starts on its way from
 * How to Play to nSCKindCharacters and nothing else in the port plays
 * (the How to Play battle itself plays its course's BGM through
 * mpCollisionSetPlayBGM, as any battle does). The same reasoning as
 * CREDITS and OPENING, with a twist: the showcase reached that way poses
 * only the two demo fighters the title picked (mncharacters.c's
 * sMNCharactersIsDemo arm), never the whole roster, so its compose arm
 * takes MENU, these two and this group -- whereas on MENU the track would
 * have tipped the character select screen's worst case (MENU plus all
 * twelve) 29 KB over the budget. */
#define SNDRES_GROUP_ATTRACT     (SNDRES_GROUP_1PBONUS + 1)
#define SNDRES_GROUPS            (SNDRES_GROUP_ATTRACT + 1)

#define SNDRES_FGM_BITS          512    /* bank-2 sound indices, fgm.h */
#define SNDRES_BGM_BITS          256    /* bank-1 waves; the bank has 117 */

typedef struct
{
    uint32_t fgm[SNDRES_FGM_BITS / 32];
    uint32_t bgm[SNDRES_BGM_BITS / 32];
} SndResSet;

/* Boot, after bgm_defer_samples and syAudioMakeBGMPlayers: read
 * sndsets.bin and the sound-effect pack's directory, and bring the FGM
 * engine up on its tables with nothing resident. `fgm_chn_base` is
 * fgm_init's. 0, or -1 if a file is missing (the game is then silent,
 * as a missing bank always made it). */
int sndres_init(int fgm_chn_base);

/* The scene manager's, before each scene starts: make the samples
 * `scene` (an nSCKind) can reach resident, composing its players and
 * stage out of gSCManagerTransferBattleState and gSCManagerSceneData. A
 * scene the port has no set for leaves sound RAM as it is. */
void sndres_enter_scene(s32 scene);

/* XBOX: the auto-demo's, from scAutoDemoFuncStart once scAutoDemoInitDemo
 * has filled gSCManagerBattleState: SNDRES_GROUP_CORE, the four fighters
 * and the stage, the shape of nSCKindVSBattle's set. Its players 2 and 3
 * are drawn inside the scene, after the scene manager's own call, which
 * is why the demo has no arm in sndres_compose and stages here. */
void sndres_enter_autodemo(void);

/* What sndres_enter_scene would make resident, without doing it. 0 and
 * `set` cleared for a scene with no set. */
int sndres_compose(s32 scene, SndResSet *set);

/* For the host test and the log: how many uploads have happened, and
 * whether a sample is resident now. */
uint32_t sndres_swaps(void);
/* sound RAM the resident set fills, for -DDB_LOAD_CENSUS */
uint32_t sndres_resident_bytes(void);
int sndres_fgm_resident(unsigned bank_index);
int sndres_bgm_resident(unsigned wave);

/* One group's lists, straight out of sndsets.bin. NULL if not loaded. */
const uint16_t *sndres_group_fgm(unsigned group, unsigned *count);
const uint16_t *sndres_group_bgm(unsigned group, unsigned *count);

#endif /* DC_SNDRES_H */

#ifndef _DC_SC1PGAMEBOSS_H_
#define _DC_SC1PGAMEBOSS_H_

/* sc/sc1pmode/sc1pgameboss.c (overlay 65): Final Destination's animated
 * background -- the comets, the rings and the closing flash Master Hand
 * is fought against. Four SC1PGameBossWallpaper rows, swapped as he
 * takes damage or on a timer, each spawning up to `loop_count` GObjs at
 * once off one of five shared trees.
 *
 * The trees come out of the stage pack's BWP1 block (src/dc/stage.h,
 * tools/export/ssb_stageexport.py); everything else here -- the rows, the
 * plans, the speeds, the spawn counts -- is this file's own .data,
 * compiled rather than exported, as the decomp has it.
 *
 * Entry point is grWallpaperMakeDecideKind's nGRKindLast arm
 * (gr/grwallpaper.c:294, src/dc/stage.c here).
 */

#include <ssb_types.h>
#include <sys/objdef.h>
#include <sc/scdef.h>

/* gr/grwallpaper.c:294: called for nGRKindLast before the common
 * wallpaper. Makes the wallpaper GObj, its two cameras and the boss
 * player lookup, and arms row 0. A no-op on a stage whose pack carries
 * no BWP1 block, so a debug boot of Final Destination without one
 * simply has no background. */
extern void sc1PGameBossInitWallpaper(void);

/* sc1pgame.c:1986 / :2008, the two the 1P scene calls. */
extern void func_ovl65_801910B0(void);
extern void sc1PGameBossSetChangeWallpaper(void);

extern void sc1PGameBossMakeCamera(void);
extern void SC1PGameBossWallpaper0ProcDisplay(GObj *gobj);
extern void SC1PGameBossWallpaper1ProcDisplay(GObj *gobj);
extern void SC1PGameBossWallpaper2ProcDisplay(GObj *gobj);
extern void SC1PGameBossWallpaper3ProcDisplay0(GObj *gobj);
extern void sc1PGameBossProcDisplayFadeAlpha(GObj *gobj);
extern void sc1PGameBossProcDisplayFadeColor(GObj *gobj);
extern void sc1PGameBossUpdateWallpaperColorID(void);
extern void SC1PGameBossWallpaper3ProcUpdate0(GObj *gobj);
extern void func_ovl65_80191B44(GObj *gobj);
extern void SC1PGameBossWallpaper0ProcUpdate(GObj *gobj);
extern void SC1PGameBossWallpaper1ProcUpdate(GObj *gobj);
extern void SC1PGameBossWallpaper2ProcUpdate0(GObj *gobj);
extern void SC1PGameBossWallpaper2ProcUpdate1(GObj *gobj);
extern void SC1PGameBossWallpaper3ProcUpdate1(GObj *gobj);
extern void sc1PGameBossSetWallpaperTranslate(GObj *gobj, s32 plan_id);
extern GObj *sc1PGameBossMakeWallpaperEffect(s32 effect_id, s32 anim_id,
                                             s32 plan_id);
extern void sc1PGameBossAdvanceWallpaper(void);
extern void sc1PGameBossWallpaperProcUpdate(GObj *gobj);
extern void sc1PGameBossSetBossPlayer(void);

#endif

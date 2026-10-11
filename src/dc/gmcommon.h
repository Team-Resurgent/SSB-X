/* gmcommon.h -- gm/gmcommon.c, the files every battle scene loads: the
 * HUD's sprites. The game lists eight relocData files in
 * dGMCommonFileIDs and lbRelocLoadFilesListed fills gGMCommonFiles with
 * their bases; if/ifcommon.c then reaches each sprite as
 * lbRelocGetFileData(Sprite*, gGMCommonFiles[n], offset).
 *
 * The port keeps both names and both shapes (gm/generic.h declares
 * them): dGMCommonFileIDs is the same eight numbers, and gGMCommonFiles
 * holds a SpriteBank* where the game held a file base, so the one
 * macro that dereferences it (src/dc/ifcommon.c redefines
 * lbRelocGetFileData) is the whole difference. A file with no bank yet
 * -- the timer's, the pause menu's, the announcer's letters, the
 * arrows' models -- is NULL, which sprite_bank_get refuses with a log
 * line rather than a crash. */
#ifndef SSB_DC_GMCOMMON_H
#define SSB_DC_GMCOMMON_H

#include <gm/generic.h>

/* gm/gmcommon.c:11-21 dGMCommonFileIDs, by index */
enum
{
    nGMCommonFileIFPlayer,          /* 166: the arrows and the magnifier */
    nGMCommonFileIFGameStatus,      /* 82: GO!, GAME SET, TIME UP, the countdown */
    nGMCommonFileIFPlayerDamage,    /* 164: the damage digits */
    nGMCommonFileIFTimer,           /* 165: the match clock's digits */
    nGMCommonFileIFDigits,          /* 36: the stock count past six */
    nGMCommonFileIFBattlePause,     /* 197: the pause menu */
    nGMCommonFileIFPlayerTags,      /* 38: 1P 2P 3P 4P CP and the ally heart */
    nGMCommonFileIFAnnounceCommon   /* 37: SUDDEN DEATH, COMPLETE, FAILURE */
};

/* sc/sccommon/scvsbattlefiles.c:37 lbRelocLoadFilesListed(dGMCommonFileIDs,
 * gGMCommonFiles): load the banks the port has into the scene heap and
 * VRAM. Returns how many of the eight loaded. */
s32 gmCommonLoadFiles(void);

#endif /* SSB_DC_GMCOMMON_H */

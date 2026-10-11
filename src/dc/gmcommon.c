/* gmcommon.c -- see gmcommon.h. Function-for-function from
 * ssb-decomp-re/src/gm/gmcommon.c, which is a table and an array. */
#include "gmcommon.h"

#include "sprite.h"

#include <sys/debug.h>
#include "overlay.h"

/* gmcommon.c:11-21, verbatim but for the link labels, which are these
 * numbers (tools/relocFileDescriptions.us.txt) */
u32 dGMCommonFileIDs[/* */] =
{
    166,                            /* &llIFCommonPlayerFileID */
    82,                             /* &llIFCommonGameStatusFileID */
    164,                            /* &llIFCommonPlayerDamageFileID */
    165,                            /* &llIFCommonTimerFileID */
    36,                             /* &llIFCommonDigitsFileID */
    197,                            /* &llIFCommonBattlePauseFileID */
    38,                             /* &llIFCommonPlayerTagsFileID */
    37                              /* &llIFCommonAnnounceCommonFileID */
};

/* gmcommon.c:30 */
void *gGMCommonFiles[ARRAY_COUNT(dGMCommonFileIDs)];

/* The port's: which of the eight has a bank on the romdisk, and its
 * name there. The rest stay NULL. */
static const char *const sGMCommonBankPaths[ARRAY_COUNT(dGMCommonFileIDs)] =
{
    NULL,
    "ifstatus.spr",
    "ifdamage.spr",
    "iftimer.spr",
    "ifdigits.spr",
    "ifpause.spr",
    "iftags.spr",
    "ifannounce.spr"
};

static SpriteBank sGMCommonBanks[ARRAY_COUNT(dGMCommonFileIDs)];

s32 gmCommonLoadFiles(void)
{
    s32 i;
    s32 loaded = 0;

    for (i = 0; i < (s32)ARRAY_COUNT(dGMCommonFileIDs); i++)
    {
        gGMCommonFiles[i] = NULL;

        if (sGMCommonBankPaths[i] == NULL)
        {
            continue;
        }
        if (sprite_bank_load(&sGMCommonBanks[i], sGMCommonBankPaths[i]) == 0)
        {
            if (sGMCommonBanks[i].file_id != dGMCommonFileIDs[i])
            {
                syDebugPrintf("gmcommon: %s is file %lu, wanted %lu\n",
                              sGMCommonBankPaths[i],
                              (unsigned long)sGMCommonBanks[i].file_id,
                              (unsigned long)dGMCommonFileIDs[i]);
            }
            gGMCommonFiles[i] = &sGMCommonBanks[i];
            loaded++;
        }
    }
    return loaded;
}

/* The bzero arm of syDmaLoadOverlay for overlay 2, of which gm/gmcommon
 * is a part (smashbrothers.us.yaml). Both are the scene's sprite files:
 * gmCommonLoadFiles fills them out of the scene heap on the way in, so
 * what is here on the way out is a bank in memory the next scene owns. */
void gmCommonOverlayLoad(void)
{
    OVERLAY_CLEAR(gGMCommonFiles);
    OVERLAY_CLEAR(sGMCommonBanks);
}

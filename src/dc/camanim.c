/* camanim.c -- see camanim.h. The file format is
 * tools/export/ssb_camanimexport.py's; the two records below are its HEADER_FMT
 * and DIR_FMT, little-endian, and the static asserts hold the C to them.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "assetroot.h"
#include "camanim.h"
#include "taskman.h"

#include <sys/debug.h>

typedef struct CamHeader
{
    char magic[8];              /* "SSBCAM1\0" */
    u32 count;
    u32 nwords;
    u32 nreloc;
    u32 off_dir;
    u32 off_words;
    u32 off_reloc;
} CamHeader;

typedef struct CamDir
{
    char name[CAMANIM_NAME_LEN];
    u32 word_first;
    u32 nwords;
} CamDir;

#define CAMANIM_STATIC_ASSERT(c, n) typedef char n[(c) ? 1 : -1]
CAMANIM_STATIC_ASSERT(sizeof(CamHeader) == 32, camanim_header_is_32);
CAMANIM_STATIC_ASSERT(sizeof(CamDir) == CAMANIM_NAME_LEN + 8,
                      camanim_dir_is_40);

static int camanim_fail(CamAnimBank *bank, const char *name, const char *why)
{
    syDebugPrintf("camanim: %s: %s\n", name, why);
    memset(bank, 0, sizeof(*bank));
    return -1;
}

int camanim_bank_load(CamAnimBank *bank, const char *name)
{
    long size;
    u8 *blob;
    const CamHeader *hd;
    const CamDir *dir;
    const u32 *src, *rl;
    u32 i;

    if (bank == NULL)
    {
        return -1;
    }
    memset(bank, 0, sizeof(*bank));

    blob = asset_read_whole(name, &size);
    if (blob == NULL)
    {
        syDebugPrintf("camanim: cannot read %s\n", name);
        return -1;
    }
    hd = (const CamHeader *)blob;

    if (size < (long)sizeof(*hd) || memcmp(hd->magic, "SSBCAM1", 8) != 0)
    {
        free(blob);
        return camanim_fail(bank, name, "bad magic");
    }
    /* Every span the header names has to be inside the file before any of
     * it is read: a truncated .cam is a build accident, and the words it
     * would otherwise hand the parser are whatever followed the buffer. */
    if ((long)(hd->off_dir + (u32)sizeof(CamDir) * hd->count) > size ||
        (long)(hd->off_words + 4 * hd->nwords) > size ||
        (long)(hd->off_reloc + 4 * hd->nreloc) > size)
    {
        free(blob);
        return camanim_fail(bank, name, "truncated");
    }
    dir = (const CamDir *)(blob + hd->off_dir);
    src = (const u32 *)(blob + hd->off_words);
    rl = (const u32 *)(blob + hd->off_reloc);

    /* The scene heap, the way sprite.c's records take it: the scripts have
     * to outlive this call and die with the scene, which is what the
     * game's own loaded relocData file does. */
    bank->words = syTaskmanMalloc(4 * hd->nwords, 0x4);
    bank->entries = syTaskmanMalloc(sizeof(CamAnimEntry) * hd->count, 0x4);
    if (bank->words == NULL || bank->entries == NULL)
    {
        free(blob);
        return camanim_fail(bank, name, "scene heap exhausted");
    }
    memcpy(bank->words, src, 4 * hd->nwords);
    bank->nwords = hd->nwords;
    bank->count = hd->count;

    for (i = 0; i < hd->count; i++)
    {
        if (dir[i].word_first > hd->nwords ||
            dir[i].nwords > hd->nwords - dir[i].word_first)
        {
            free(blob);
            return camanim_fail(bank, name, "directory entry out of range");
        }
        memcpy(bank->entries[i].name, dir[i].name, CAMANIM_NAME_LEN);
        bank->entries[i].name[CAMANIM_NAME_LEN - 1] = '\0';
        bank->entries[i].words = bank->words + dir[i].word_first;
        bank->entries[i].nwords = dir[i].nwords;
    }

    /* THE RELOCATION, and the whole reason this is a bank rather than a
     * memcpy (camanim.h). Each listed word holds its target's word index
     * in this bank; it becomes that word's address now that the bank has
     * one. Done last, so a script that jumps into another one finds the
     * other's copy already in place -- src/dc/stage.c:230-257 does the
     * same for the stage packs' map AnimJoints, for the same reason.
     *
     * An index out of range is refused rather than written: the pointer
     * it would make is the exact failure this machinery exists to stop,
     * and it does not crash -- gcParseCObjCamAnimJoint follows it, never
     * finds an End and spins inside the frame, which looks like a hang. */
    for (i = 0; i < hd->nreloc; i++)
    {
        u32 w = rl[i];

        if (w >= hd->nwords || bank->words[w] >= hd->nwords)
        {
            free(blob);
            return camanim_fail(bank, name, "relocation out of range");
        }
        bank->words[w] = (u32)(uintptr_t)(bank->words + bank->words[w]);
    }
    i = hd->nreloc;
    free(blob);
    bank->is_loaded = TRUE;
    syDebugPrintf("camanim: %s: %d script(s), %d words, %d reloc\n",
                  name, (int)bank->count, (int)bank->nwords, (int)i);
    return 0;
}

AObjEvent32 *camanim_get(CamAnimBank *bank, const char *name)
{
    u32 i;

    if (bank == NULL || !bank->is_loaded || name == NULL)
    {
        return NULL;
    }
    for (i = 0; i < bank->count; i++)
    {
        if (strcmp(bank->entries[i].name, name) == 0)
        {
            return (AObjEvent32 *)bank->entries[i].words;
        }
    }
    syDebugPrintf("camanim: no script named %s in this bank\n", name);
    return NULL;
}

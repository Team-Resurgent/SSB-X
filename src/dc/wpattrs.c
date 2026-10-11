/* wpattrs.c -- the fighters' weapon hitbox tables, bound on target. See
 * wpattrs.h for the whole argument; the two halves here are the record
 * decoder the item pack shares, and the blob loader + bind. */
#include <stdlib.h>
#include <string.h>

#include <sys/debug.h>            /* syDebugPrintf, both builds */

#include <it/ittypes.h>         /* ITAttackEvent */

#include "assetroot.h"
#include "wpattrs.h"

#define WPATTR_MAGIC     "WPAT"
#define WPATTR_VERSION   2
#define WPATTR_HEADER    8        /* magic, u16 version, u16 file_count */
#define WPATTR_FILE_HDR  8        /* u16 file_id, u16 records, u16 blob_size, u16 raw tables */
#define WPATTR_RAW_HDR   8        /* u32 ROM offset, u16 kind, u16 count */
#define WPATTR_RAW_ATTACKEVENT 1  /* u8 timer, pad, s16 angle, u8 damage, pad, u16 size */
#define WPATTR_RAW_F32         2  /* f32, little-endian */
#define WPATTR_ATTACKEVENT_SIZE 8

/* The host fields go into a host ITAttackEvent, but the file offsets that
 * follow one are the N64's, eight bytes apart. */
_Static_assert(sizeof(ITAttackEvent) == WPATTR_ATTACKEVENT_SIZE,
               "ITAttackEvent is the N64's eight bytes");
#define WPATTR_ROM_SIZE  0x34     /* sizeof(WPAttributes) on the N64 */
#define WPATTR_FILES_MAX 16

#ifndef FT_HOSTTEST
/* The ll* offsets are the N64's, and the blob lays each record at its
 * offset -- so the SH4's WPAttributes has to be the N64's size, or the
 * second table of a file (Yoshi's Star at 0x40 after the egg at 0xC)
 * would land inside the first. Four 4-byte pointers, twelve bytes of
 * Vec3h, eight of s16 and four bitfield words, on both. */
_Static_assert(sizeof(WPAttributes) == WPATTR_ROM_SIZE,
               "WPAttributes is not the N64's 0x34 bytes on this target");
#endif

/* The blobs. sWPAttrsFile[] and the bytes they point at persist across
 * overlay reloads the way src/dc/itempack.c's sItemPack does: this is a
 * port module the overlay map (tools/check/overlay_check.py) does not clear,
 * and the file is read once per boot. */
typedef struct WPAttrsFile
{
    int file_id;
    u8 *blob;
    u32 size;
} WPAttrsFile;

static WPAttrsFile sWPAttrsFile[WPATTR_FILES_MAX];
static int sWPAttrsFileCount;
static int sWPAttrsLoaded;      /* 0 never tried, 1 loaded, -1 failed */

/* What a fighter blob's non-NULL pointer fields point at. Nothing reads
 * through it; wpManagerIsModelLess only asks whether `data` is NULL. */
static u8 sWPAttrsMarker;

/* Which gFTData* holds which file. The globals are the decomp's own
 * (ft/ftchar/<char>/ft<char>.c, compiled unmodified); Mario's is named
 * differently from every other fighter's. File 203 (MarioMain) is not
 * here: wpMarioFireballMakeWeapon rewrites its WPDesc's file/offset pair
 * to Special1's before any read (wp/wpmario/wpmariofireball.c:167), so
 * gFTMarioFileMain is never used as a weapon-table base. */
extern void *gFTMarioFileSpecial1;
extern void *gFTDataLuigiSpecial1;
extern void *gFTDataFoxSpecial1;
extern void *gFTDataSamusMain;
extern void *gFTDataSamusSpecial1;
extern void *gFTDataYoshiMain;
extern void *gFTDataKirbyMain;
extern void *gFTDataLinkMain;
extern void *gFTDataLinkSpecial1;
extern void *gFTDataPikachuMain;
extern void *gFTDataPikachuSpecial1;
extern void *gFTNessFileMain;
extern void *gFTNessFileSpecial1;
extern void *gFTDataBossMainMotion;

/* wp/wppikachu/wppikachuthunderjolt.c wpPikachuThunderJoltGroundAddAnim
 * hands gcAddAnimAll lbRelocGetFileData(gFTDataPikachuSpecial3,
 * &llPikachuSpecial3ThunderJoltB{,Mat}AnimJoint): the jolt's second
 * animation, re-added each time the crawler turns onto a new floor or
 * wall. Nothing loads Special3 in this port -- the pointer is NULL for
 * the life of the run -- so what that expression yields is the address
 * of the offset symbol itself, and gcAddAnimAll reads one entry per DObj
 * from there: eight joints' AnimJoints and eight joints' MatAnimJoint
 * rows.
 *
 * They were `int`s in src/dc/wpmanager.c, four bytes each, so seven of
 * the eight AnimJoint reads landed on the weapon manager's neighbouring
 * .bss -- its group counter and struct free list -- and the parser
 * walked those as event scripts until it hung
 * (gcParseDObjAnimJoint's `while (anim_wait <= 0)`): a four-CPU Saffron
 * City match froze at tic 1358 the first time a Thunder Jolt climbed a
 * building. As arrays of NULLs they are the decomp's own "no animation
 * for this joint" (sys/objanim.c gcAddAnimAll's `*anim_joints == NULL`
 * arm), which is what the jolt played until wpAttrsBindJoltB below.
 *
 * ThunderJoltB is not a second animation: the jolt's own attributes name
 * the same two tables (Special1 0x34's anim_joints and matanim), so it is
 * the pack's animation 0 and MatAnimJoint, which wppikachugjolt.mdl
 * already carries. wpManagerAddModel therefore binds these arrays to the
 * pack's tables at each spawn, and the replay on a turn is the
 * decomp's.
 *
 * Defined here and not in wpmanager.c because that file includes
 * reloc_data.h, whose `extern int` would conflict. 64 entries: the
 * jolt's tree is 8 joints, and a larger tree would still read NULLs. */
#define WPATTRS_NULL_ANIM_ROWS 64

void *llPikachuSpecial3ThunderJoltBAnimJoint[WPATTRS_NULL_ANIM_ROWS];
void *llPikachuSpecial3ThunderJoltBMatAnimJoint[WPATTRS_NULL_ANIM_ROWS];

/* Fill the two with the loaded jolt pack's own tables, `n` joints of them
 * (the rest stay NULL). wpManagerAddModel calls it each time a ground jolt
 * is made, so the tables always point into the pack that is resident. */
void wpAttrsBindJoltB(void **anim_joints, void ***matanim_joints, unsigned n)
{
    unsigned i;

    for (i = 0; i < WPATTRS_NULL_ANIM_ROWS; i++)
    {
        llPikachuSpecial3ThunderJoltBAnimJoint[i] =
            ((anim_joints != NULL) && (i < n)) ? anim_joints[i] : NULL;
        llPikachuSpecial3ThunderJoltBMatAnimJoint[i] =
            ((matanim_joints != NULL) && (i < n)) ? (void *)matanim_joints[i] : NULL;
    }
}

static const struct
{
    void **file_ptr;
    int file_id;
} kWPAttrsBinds[] =
{
    { &gFTMarioFileSpecial1,    204 },
    { &gFTDataLuigiSpecial1,    222 },
    { &gFTDataFoxSpecial1,      210 },
    { &gFTDataSamusMain,        217 },
    { &gFTDataSamusSpecial1,    218 },
    { &gFTDataYoshiMain,        247 },
    { &gFTDataKirbyMain,        229 },
    { &gFTDataLinkMain,         225 },
    { &gFTDataLinkSpecial1,     226 },
    { &gFTDataPikachuMain,      243 },
    { &gFTDataPikachuSpecial1,  244 },
    { &gFTNessFileMain,         239 },
    { &gFTNessFileSpecial1,     240 },
    /* Master Hand's finger rocket, whose two records live in his
     * MainMotion (wp/wpboss/wpbossbullet.c). Without a row here the
     * pointer stays NULL and every bullet reads its attributes from
     * address 0x774 -- BIOS bytes: no model, no hitbox, gone at once. */
    { &gFTDataBossMainMotion,   249 },
};

static u16 rd16(const u8 *p)
{
    u16 v;
    memcpy(&v, p, 2);
    return v;
}

static s16 rd16s(const u8 *p)
{
    s16 v;
    memcpy(&v, p, 2);
    return v;
}

static u32 rd32(const u8 *p)
{
    u32 v;
    memcpy(&v, p, 4);
    return v;
}

static void *rdptr(const u8 *p, const u8 *ptr_base)
{
    u32 off = rd32(p);
    return (off == WPATTR_NO_PTR) ? NULL : (void *)(ptr_base + off);
}

void wpAttrsSetupRecord(WPAttributes *attr, const u8 *c, const u8 *ptr_base)
{
    u16 flags;
    s32 i;

    memset(attr, 0, sizeof(*attr));

    attr->data = rdptr(c + 0, ptr_base);
    attr->p_mobjsubs = (MObjSub ***)rdptr(c + 4, ptr_base);
    attr->anim_joints = (AObjEvent32 **)rdptr(c + 8, ptr_base);
    attr->p_matanim_joints = (AObjEvent32 ***)rdptr(c + 12, ptr_base);

    for (i = 0; i < 2; i++)
    {
        attr->attack_offsets[i].x = rd16s(c + 16 + i * 6);
        attr->attack_offsets[i].y = rd16s(c + 18 + i * 6);
        attr->attack_offsets[i].z = rd16s(c + 20 + i * 6);
    }
    attr->map_coll_top = rd16s(c + 28);
    attr->map_coll_center = rd16s(c + 30);
    attr->map_coll_bottom = rd16s(c + 32);
    attr->map_coll_width = rd16s(c + 34);
    attr->size = rd16(c + 36);
    attr->angle = rd16s(c + 38);
    attr->knockback_scale = rd16(c + 40);
    attr->damage = c[42];
    attr->element = c[43];
    attr->knockback_weight = rd16(c + 44);
    attr->shield_damage = (s8)c[46];     /* `s32 : 8`; Yoshi's Star is -3 */
    attr->attack_count = c[47];
    flags = rd16(c + 48);
    attr->can_setoff        = (flags >> 0) & 1;
    attr->can_rehit_item    = (flags >> 1) & 1;
    attr->can_rehit_fighter = (flags >> 2) & 1;
    attr->can_hop           = (flags >> 3) & 1;
    attr->can_reflect       = (flags >> 4) & 1;
    attr->can_absorb        = (flags >> 5) & 1;
    attr->can_shield        = (flags >> 6) & 1;
    attr->unused_0x2F_b6    = (flags >> 7) & 1;
    attr->unused_0x2F_b7    = (flags >> 8) & 1;
    attr->sfx = rd16(c + 50);
    attr->priority = c[52];
    /* 53, not 54: the record is BYTE-TIGHT -- the writer's fields come to
     * 55 bytes and its own check says so -- so the one-byte `priority`
     * leaves `knockback_base` straddling the odd offset. Reading it at 54
     * took one byte of knockback_base and one of the padding, which is
     * zero either way, so every weapon whose base was 0 read correctly and
     * the first one with a real base (Sector Z's lasers) is
     * what found it. */
    attr->knockback_base = rd16(c + 53);
}

int wpAttrsLoad(void)
{
    u8 *p;
    long size = 0;
    u32 pos, i, n;

    if (sWPAttrsLoaded != 0)
    {
        return (sWPAttrsLoaded > 0) ? 0 : -1;
    }
    sWPAttrsLoaded = -1;

    p = asset_read_whole("wpattrs.bin", &size);

    if (p == NULL)
    {
        syDebugPrintf("wpattrs: wpattrs.bin is not in the romdisk\n");
        return -1;
    }
    if (size < WPATTR_HEADER || memcmp(p, WPATTR_MAGIC, 4) != 0 ||
        rd16(p + 4) != WPATTR_VERSION)
    {
        syDebugPrintf("wpattrs: wpattrs.bin is not a WPAT v%d file\n",
                      WPATTR_VERSION);
        free(p);
        return -1;
    }
    n = rd16(p + 6);

    if (n > WPATTR_FILES_MAX)
    {
        syDebugPrintf("wpattrs: %u files, more than %d\n", n, WPATTR_FILES_MAX);
        free(p);
        return -1;
    }
    pos = WPATTR_HEADER;

    for (i = 0; i < n; i++)
    {
        WPAttrsFile *f = &sWPAttrsFile[i];
        u32 records, r, raws, rec_pos, raw_pos, need;

        if (pos + WPATTR_FILE_HDR > (u32)size)
        {
            goto truncated;
        }
        f->file_id = rd16(p + pos);
        records = rd16(p + pos + 2);
        f->size = rd16(p + pos + 4);
        raws = rd16(p + pos + 6);
        pos += WPATTR_FILE_HDR;

        if (f->size < WPATTR_ROM_SIZE ||
            pos + records * (4 + WPATTR_RECORD_SIZE) > (u32)size)
        {
            goto truncated;
        }
        /* The records sit at their ROM offsets, spread apart by however
         * much wider this build's WPAttributes is than the N64's -- zero
         * on the target (the assert above), 0x14 per record on a 64-bit
         * host -- in the offset order the file lists them, which is the
         * rule the fragment's offsets follow (tools/export/ssb_wpattrexport.py
         * mapped_offsets). */
        f->size += records * (u32)(sizeof(WPAttributes) - WPATTR_ROM_SIZE);

        /* The RAW tables past the records: data an item
         * reads straight out of the same file -- Link's Bomb's attack
         * events and bloat scales in LinkMain, which it/itfighter/
         * itlinkbomb.c indexes off gFTDataLinkMain. Each sits at its ROM
         * offset, pushed along by the excess of every record before it,
         * the same rule; the blob grows to hold the last one. */
        rec_pos = pos;
        raw_pos = pos + records * (4 + WPATTR_RECORD_SIZE);

        for (r = 0; r < raws; r++)
        {
            u32 off, kind, count, k, before = 0;

            if (raw_pos + WPATTR_RAW_HDR > (u32)size)
            {
                goto truncated;
            }
            off = rd32(p + raw_pos);
            kind = rd16(p + raw_pos + 4);
            count = rd16(p + raw_pos + 6);

            for (k = 0; k < records; k++)
            {
                if (rd32(p + rec_pos + k * (4 + WPATTR_RECORD_SIZE)) < off)
                {
                    before++;
                }
            }
            off += before * (u32)(sizeof(WPAttributes) - WPATTR_ROM_SIZE);
            need = off + count * ((kind == WPATTR_RAW_ATTACKEVENT) ?
                                  (u32)sizeof(ITAttackEvent) : (u32)sizeof(f32));
            if (need > f->size)
            {
                f->size = need;
            }
            raw_pos += WPATTR_RAW_HDR + count * ((kind == WPATTR_RAW_ATTACKEVENT) ?
                                                 WPATTR_ATTACKEVENT_SIZE : 4);
        }
        if (raw_pos > (u32)size)
        {
            goto truncated;
        }
        f->blob = calloc(1, f->size);

        if (f->blob == NULL)
        {
            syDebugPrintf("wpattrs: no room for file %d's %u bytes\n",
                          f->file_id, f->size);
            free(p);
            return -1;
        }
        for (r = 0; r < records; r++)
        {
            u32 off = rd32(p + pos) +
                      r * (u32)(sizeof(WPAttributes) - WPATTR_ROM_SIZE);

            if (off + sizeof(WPAttributes) > f->size)
            {
                syDebugPrintf("wpattrs: file %d: record at 0x%x is past "
                              "its %u-byte blob\n", f->file_id, off, f->size);
                free(p);
                return -1;
            }
            wpAttrsSetupRecord((WPAttributes *)(f->blob + off), p + pos + 4,
                               &sWPAttrsMarker);
            pos += 4 + WPATTR_RECORD_SIZE;
        }
        for (r = 0; r < raws; r++)
        {
            u32 off = rd32(p + pos), kind = rd16(p + pos + 4);
            u32 count = rd16(p + pos + 6), k, before = 0;

            for (k = 0; k < records; k++)
            {
                if (rd32(p + rec_pos + k * (4 + WPATTR_RECORD_SIZE)) < off)
                {
                    before++;
                }
            }
            off += before * (u32)(sizeof(WPAttributes) - WPATTR_ROM_SIZE);
            pos += WPATTR_RAW_HDR;

            for (k = 0; k < count; k++)
            {
                if (kind == WPATTR_RAW_ATTACKEVENT)
                {
                    ITAttackEvent ev;
                    const u8 *c = p + pos;

                    memset(&ev, 0, sizeof(ev));
                    ev.timer = c[0];
                    ev.angle = rd16s(c + 2);
                    ev.damage = c[4];
                    ev.size = rd16(c + 6);
                    memcpy(f->blob + off + k * sizeof(ev), &ev, sizeof(ev));
                    pos += WPATTR_ATTACKEVENT_SIZE;
                }
                else if (kind == WPATTR_RAW_F32)
                {
                    memcpy(f->blob + off + k * sizeof(f32), p + pos, sizeof(f32));
                    pos += 4;
                }
                else
                {
                    syDebugPrintf("wpattrs: file %d: raw table kind %u\n",
                                  f->file_id, kind);
                    free(p);
                    return -1;
                }
            }
        }
        sWPAttrsFileCount = i + 1;
    }
    free(p);
    sWPAttrsLoaded = 1;
    syDebugPrintf("wpattrs: %d weapon files bound\n", sWPAttrsFileCount);
    return 0;

truncated:
    syDebugPrintf("wpattrs: wpattrs.bin is truncated at byte %u\n", pos);
    free(p);
    return -1;
}

const void *wpAttrsFileBlob(int file_id)
{
    int i;

    for (i = 0; i < sWPAttrsFileCount; i++)
    {
        if (sWPAttrsFile[i].file_id == file_id)
        {
            return sWPAttrsFile[i].blob;
        }
    }
    return NULL;
}

void wpAttrsBind(void)
{
    u32 i;

    if (wpAttrsLoad() != 0)
    {
        return;
    }
    for (i = 0; i < sizeof(kWPAttrsBinds) / sizeof(kWPAttrsBinds[0]); i++)
    {
        const void *blob = wpAttrsFileBlob(kWPAttrsBinds[i].file_id);

        if (blob == NULL)
        {
            syDebugPrintf("wpattrs: no blob for file %d\n",
                          kWPAttrsBinds[i].file_id);
            continue;
        }
        *kWPAttrsBinds[i].file_ptr = (void *)blob;
    }
    wpAttrsUnboundFiles();      /* names any file left unbound */
}

/* How many of the loaded blobs no row of kWPAttrsBinds names: a file the
 * exporter ships whose gFTData* would stay NULL. The host test holds it
 * at zero. */
int wpAttrsUnboundFiles(void)
{
    int i, count = 0;
    u32 k;

    for (i = 0; i < sWPAttrsFileCount; i++)
    {
        for (k = 0; k < sizeof(kWPAttrsBinds) / sizeof(kWPAttrsBinds[0]); k++)
        {
            if (kWPAttrsBinds[k].file_id == sWPAttrsFile[i].file_id)
            {
                break;
            }
        }
        if (k == sizeof(kWPAttrsBinds) / sizeof(kWPAttrsBinds[0]))
        {
            syDebugPrintf("wpattrs: file %d has no binding\n",
                          sWPAttrsFile[i].file_id);
            count++;
        }
    }
    return count;
}

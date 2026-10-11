/* itempack.c -- see itempack.h for what this is and why the regions are
 * split the way they are. This file is the reader.
 *
 * The pack is little-endian and so is everything that reads it (the SH-4
 * and the host both), so every field is a plain memcpy -- the same read
 * src/dc/stage.c makes of the MPK1 block, and for the same reason: the
 * pack's own layout decides where a field sits, never a cast.
 */
#include <stdio.h>                /* snprintf */
#include <stdlib.h>
#include <string.h>

#include <sys/debug.h>            /* syDebugPrintf, both builds */

#include "assetroot.h"
#include "itempack.h"
#include "wpattrs.h"           /* wpAttrsSetupRecord */
#if defined(DB_ANIM_CHECK) && !defined(FT_HOSTTEST)
#include "fighter.h"           /* fighter_animcheck_add */
#endif

/* tools/export/ssb_itemexport.py's header: magic, version, two region sizes,
 * eight counts, eight offsets, and the model region's reloc table. Header
 * size is 80; the loader checks the file against this before reading any
 * regions. */
#define ITEM_PACK_HEADER   80
#define ITEM_PACK_VERSION   2
#define ITEM_PACK_BLOCK    40     /* char name[32]; u32 region; u32 offset */
#define ITEM_PACK_FIXUP     8     /* u32 site; u32 target */
#define ITEM_PACK_NAME_MAX 32

/* itmanager.c: the file every item's ITDesc points its p_file at. The
 * loader publishes region 0 there, which is what makes
 * `lbRelocGetFileData(ITAttributes*, gITManagerCommonData, off)` resolve
 * the way the game's does. */
extern void *gITManagerCommonData;

static u8 *sItemPack;
static u32 sItemPackSize;
static u32 sItemDataSize, sItemModelSize;
static u32 sItemBlockCount, sItemAttrCount;
static u32 sItemOffEvents, sItemEventCount;
static u32 sItemOffAEvents, sItemAEventCount;
static u32 sItemOffWAttrs, sItemWAttrCount;
static u32 sItemOffData, sItemOffModels, sItemOffBlocks, sItemOffAttrs;

int itemPackLoaded(void)
{
    return sItemPack != NULL;
}

static void item_pack_u32(u32 *out, u32 off)
{
    memcpy(out, sItemPack + off, 4);
}

/* One name-table entry by index: its name, region and offset. */
static void item_pack_entry(u32 i, char *name, u32 *region, u32 *off)
{
    u32 at = sItemOffBlocks + i * ITEM_PACK_BLOCK;

    memcpy(name, sItemPack + at, ITEM_PACK_NAME_MAX);
    name[ITEM_PACK_NAME_MAX - 1] = '\0';
    item_pack_u32(region, at + ITEM_PACK_NAME_MAX);
    item_pack_u32(off, at + ITEM_PACK_NAME_MAX + 4);
}

/* Where a region starts and how long it is. */
static const u8 *item_pack_region(int region, u32 *size)
{
    switch (region)
    {
    case ITEM_PACK_REGION_DATA:
        *size = sItemDataSize;
        return sItemPack + sItemOffData;

    case ITEM_PACK_REGION_MODELS:
        *size = sItemModelSize;
        return sItemPack + sItemOffModels;

    case ITEM_PACK_REGION_ATTRS:
        *size = sItemAttrCount * ITEM_PACK_RECORD_SIZE;
        return sItemPack + sItemOffAttrs;
    }
    return NULL;
}

/* The index of the block named `name` in region `region`, or -1.
 *
 * A name is not unique across regions on its own -- the descriptions list
 * `DataStart Shell` and `MatAnimJoint Shell` -- so a caller that wants a
 * table names the region it lives in, which is what the region parameter
 * on the public calls is for. */
static int item_pack_find(int region, const char *name)
{
    char nm[ITEM_PACK_NAME_MAX];
    u32 r, off, i;

    /* An unloaded pack has no blocks, and asking it for one is a question
     * whose answer is "no" rather than a walk off a NULL base. The count
     * alone is not enough: it is stale the moment the blob is given back
     * (test_item_pack asks an unloaded pack precisely to pin this). */
    if (sItemPack == NULL)
    {
        return -1;
    }
    for (i = 0; i < sItemBlockCount; i++)
    {
        item_pack_entry(i, nm, &r, &off);

        if (r == (u32)region && strcmp(nm, name) == 0)
        {
            return (int)i;
        }
    }
    return -1;
}

const void *itemPackBlock(int region, const char *name, u32 *size_out)
{
    int i = item_pack_find(region, name);
    char nm[ITEM_PACK_NAME_MAX];
    const u8 *base;
    u32 r, off, size, here, j, next = 0;
    int found = 0;

    if (i < 0)
    {
        return NULL;
    }
    base = item_pack_region(region, &size);

    if (base == NULL)
    {
        return NULL;
    }
    item_pack_entry((u32)i, nm, &r, &here);

    if (size_out != NULL)
    {
        /* The next block in this region, in table order -- the exporter
         * writes them in file order, so the gap is the block's extent. */
        for (j = 0; j < sItemBlockCount; j++)
        {
            item_pack_entry(j, nm, &r, &off);

            if (r == (u32)region && off > here && (!found || off < next))
            {
                next = off;
                found = 1;
            }
        }
        *size_out = found ? (next - here) : (size - here);
    }
    return base + here;
}

static void item_pack_read_attr(const u8 *c, ITItemAttr *out);

int itemPackGetAttr(const char *name, ITItemAttr *out)
{
    /* The block table carries the name the descriptions spell, kind
     * first. An attribute table's kind is always `ItemAttributes`, so a
     * caller names the item and this prefixes it -- the port's stand-in
     * for the decomp's `&llITCommonData<Name>ItemAttributes`. */
    char full[ITEM_PACK_NAME_MAX];
    const void *p;

    snprintf(full, sizeof(full), "ItemAttributes %s", name);
    p = itemPackBlock(ITEM_PACK_REGION_ATTRS, full, NULL);

    if (p == NULL || out == NULL)
    {
        return -1;
    }
    item_pack_read_attr(p, out);
    return 0;
}

/* One record's fields, read out of the pack's own layout. The field order
 * tools/export/ssb_itemexport.py's encode_item_attr writes, read back in the
 * same order. Not a cast: a u8 before a u16 in the reader would take padding
 * the writer never wrote. */
static void item_pack_read_attr(const u8 *c, ITItemAttr *out)
{
    memcpy(&out->data_off, c, 4);            c += 4;
    memcpy(&out->mobjsubs_off, c, 4);        c += 4;
    memcpy(&out->animjoints_off, c, 4);      c += 4;
    memcpy(&out->matanimjoints_off, c, 4);   c += 4;
    memcpy(&out->flags, c, 2);               c += 2;
    memcpy(out->atk0, c, 6);                 c += 6;
    memcpy(out->atk1, c, 6);                 c += 6;
    memcpy(out->dcoll_off, c, 6);            c += 6;
    memcpy(out->dcoll_size, c, 6);           c += 6;
    memcpy(&out->map_top, c, 2);             c += 2;
    memcpy(&out->map_center, c, 2);          c += 2;
    memcpy(&out->map_bottom, c, 2);          c += 2;
    memcpy(&out->map_width, c, 2);           c += 2;
    memcpy(&out->size, c, 2);                c += 2;
    memcpy(&out->angle, c, 2);               c += 2;
    memcpy(&out->knockback_scale, c, 2);     c += 2;
    out->damage = c[0];
    out->element = c[1];                     c += 2;
    memcpy(&out->knockback_weight, c, 2);    c += 2;
    out->shield_damage = c[0];
    out->attack_count = c[1];                c += 2;
    memcpy(&out->flags2, c, 2);              c += 2;
    memcpy(&out->hit_sfx, c, 2);             c += 2;
    out->priority = c[0];                    c += 1;
    memcpy(&out->knockback_base, c, 2);      c += 2;
    out->type = c[0];
    out->hitstatus = c[1];                   c += 2;
    memcpy(&out->drop_sfx, c, 2);            c += 2;
    memcpy(&out->throw_sfx, c, 2);           c += 2;
    memcpy(&out->smash_sfx, c, 2);           c += 2;
    memcpy(&out->vel_scale, c, 2);           c += 2;
    memcpy(&out->spin_speed, c, 2);          c += 2;
}

/* ---- the 34 tables, in host layout -------------------------------
 *
 * `itManagerMakeItem` reads `attr = lbRelocGetFileData(ITAttributes*,
 * *item_desc->p_file, item_desc->o_attributes)` and then keeps the pointer
 * for the item's whole life (`ip->attr = attr`). The offset maps into the
 * pack's common data, but that layout differs from the N64 decomp: `ITAttributes`
 * opens with four POINTERS, so the same offset addresses different fields here.
 * Each table is built once, at load, into an array that lives as long as the pack.
 *
 * BUILT BY NAME, not cast. `ITAttributes` mixes four pointers with two
 * long bitfield runs and a u8/u16 tail, so its layout is the compiler's
 * business; every field is written through its own member and nothing
 * reads the pack's record as a struct.
 *
 * THE FOUR POINTERS CARRY THE MODEL REGION'S BASE. What a record stores is
 * the offset in ITCommonObject -- the same number the decomp names by
 * `&llITCommonDataShellDataStart`, and the same number the FIXUP table's
 * `site` side carries. (The fixup table's other side is the ABSOLUTE pack
 * offset, because a raw ITAttributes read out of region 0 must be followed
 * without a base; see the note in itemPackLoad. The two are different on
 * purpose and this is the one that needed saying.) So resolving them is
 * `sItemPack + sItemOffModels + off`, exactly the arithmetic
 * `itemPackBlock(ITEM_PACK_REGION_MODELS, ...)` does. */

/* A record's key: the block's own offset in region 0, which is the number
 * an ITDesc's o_attributes carries and the decomp names by
 * `&llITCommonData<Name>ItemAttributes`. */
#define ITEM_PACK_RECORD_KEY (ITEM_PACK_RECORD_SIZE - 4)

static ITAttributes *sItemAttrs;
static WPAttributes *sItemWAttrs;

static u32 item_pack_wattr_key(u32 i)
{
    u32 key;

    memcpy(&key, sItemPack + sItemOffWAttrs + i * ITEM_PACK_WATTR_SIZE +
           ITEM_WATTR_KEY, 4);
    return key;
}

static u32 item_pack_record_key(u32 i)
{
    u32 key;

    memcpy(&key, sItemPack + sItemOffAttrs + i * ITEM_PACK_RECORD_SIZE + ITEM_PACK_RECORD_KEY, 4);
    return key;
}

static void item_pack_setup_attr(ITAttributes *attr, const ITItemAttr *ia)
{
    memset(attr, 0, sizeof(*attr));

    attr->data = (ia->data_off == ITEM_PACK_NO_PTR) ? NULL :
        (void *)(sItemPack + sItemOffModels + ia->data_off);
    attr->p_mobjsubs = (ia->mobjsubs_off == ITEM_PACK_NO_PTR) ? NULL :
        (MObjSub ***)(sItemPack + sItemOffModels + ia->mobjsubs_off);
    attr->anim_joints = (ia->animjoints_off == ITEM_PACK_NO_PTR) ? NULL :
        (AObjEvent32 **)(sItemPack + sItemOffModels + ia->animjoints_off);
    attr->p_matanim_joints = (ia->matanimjoints_off == ITEM_PACK_NO_PTR) ? NULL :
        (AObjEvent32 ***)(sItemPack + sItemOffModels + ia->matanimjoints_off);

    attr->is_display_xlu = (ia->flags >> 0) & 1;
    attr->is_item_dobjs = (ia->flags >> 1) & 1;
    attr->is_display_colanim = (ia->flags >> 2) & 1;
    attr->is_give_hitlag = (ia->flags >> 3) & 1;
    attr->weight = (ia->flags >> 4) & 1;

    attr->attack_offset0_x = ia->atk0[0];
    attr->attack_offset0_y = ia->atk0[1];
    attr->attack_offset0_z = ia->atk0[2];
    attr->attack_offset1_x = ia->atk1[0];
    attr->attack_offset1_y = ia->atk1[1];
    attr->attack_offset1_z = ia->atk1[2];

    attr->damage_coll_offset.x = ia->dcoll_off[0];
    attr->damage_coll_offset.y = ia->dcoll_off[1];
    attr->damage_coll_offset.z = ia->dcoll_off[2];
    attr->damage_coll_size.x = ia->dcoll_size[0];
    attr->damage_coll_size.y = ia->dcoll_size[1];
    attr->damage_coll_size.z = ia->dcoll_size[2];

    attr->map_coll_top = ia->map_top;
    attr->map_coll_center = ia->map_center;
    attr->map_coll_bottom = ia->map_bottom;
    attr->map_coll_width = ia->map_width;

    attr->size = ia->size;
    attr->angle = ia->angle;
    attr->knockback_scale = ia->knockback_scale;
    attr->damage = ia->damage;
    attr->element = ia->element;
    attr->knockback_weight = ia->knockback_weight;
    attr->shield_damage = ia->shield_damage;
    attr->attack_count = ia->attack_count;

    attr->can_setoff = (ia->flags2 >> 0) & 1;
    attr->can_rehit_item = (ia->flags2 >> 1) & 1;
    attr->can_rehit_fighter = (ia->flags2 >> 2) & 1;
    attr->can_hop = (ia->flags2 >> 3) & 1;
    attr->can_reflect = (ia->flags2 >> 4) & 1;
    attr->can_shield = (ia->flags2 >> 5) & 1;
    /* unk_atca_0x3C_b6/b7 are the two bits the pack does not carry: the
     * decomp marks both unused and nothing reads them */

    attr->hit_sfx = ia->hit_sfx;
    attr->priority = ia->priority;
    attr->knockback_base = ia->knockback_base;
    attr->type = ia->type;
    attr->hitstatus = ia->hitstatus;
    attr->drop_sfx = ia->drop_sfx;
    attr->throw_sfx = ia->throw_sfx;
    attr->smash_sfx = ia->smash_sfx;
    attr->vel_scale = ia->vel_scale;
    attr->spin_speed = ia->spin_speed;
}

/* The weapon-attributes record is decoded by src/dc/wpattrs.c's
 * wpAttrsSetupRecord -- one reader for the item pack's stage weapons and
 * the fighters' baked tables alike, so the two cannot drift. The item
 * pack's pointer fields are offsets into its own model region. */
static void item_pack_setup_wattr(WPAttributes *attr, const u8 *c)
{
    wpAttrsSetupRecord(attr, c, sItemPack + sItemOffModels);
}

/* `wpManagerMakeWeapon`'s attribute lookup, itemPackAttr's twin -- see the
 * header for why a stage weapon needs one and a fighter's does not. */
WPAttributes *itemPackWeaponAttr(void *file, intptr_t offset)
{
    u32 i;

    if (sItemPack == NULL || sItemWAttrs == NULL || file == NULL)
    {
        return NULL;
    }
    if (file != (void *)(sItemPack + sItemOffData))
    {
        return NULL;
    }
    for (i = 0; i < sItemWAttrCount; i++)
    {
        if (item_pack_wattr_key(i) == (u32)offset)
        {
            return &sItemWAttrs[i];
        }
    }
    return NULL;
}


/* itemPackMonsterEvent: one event of the table keyed by `o_attributes`,
 * or -1. A TABLE's events are CONTIGUOUS and all carry the same key, so
 * the `index`-th is the one `index` records on -- and if the key there is
 * not the same, the index ran past the end, which is a caller bug and is
 * reported rather than read as the next table's first event. */
int itemPackMonsterEvent(intptr_t o_attributes, u32 index, ITItemEvent *out)
{
    u32 i;

    if (sItemPack == NULL || out == NULL)
    {
        return -1;
    }
    for (i = 0; i < sItemEventCount; i++)
    {
        const u8 *p = sItemPack + sItemOffEvents + i * ITEM_PACK_EVENT_SIZE;
        u32 key;

        memcpy(&key, p + ITEM_EVENT_KEY, 4);

        if ((intptr_t)key != o_attributes)
        {
            continue;
        }
        if ((i + index) >= sItemEventCount)
        {
            return -1;
        }
        p = sItemPack + sItemOffEvents + (i + index) * ITEM_PACK_EVENT_SIZE;
        memcpy(&key, p + ITEM_EVENT_KEY, 4);

        if ((intptr_t)key != o_attributes)
        {
            return -1;
        }
        memcpy(out, p, sizeof(*out));

        return 0;
    }
    return -1;
}

/* The attack-events reader, `itemPackMonsterEvent`'s twin one size down:
 * the same walk (find the table's first record by key, then step forward
 * `index` records re-checking the key, which is what detects an index past
 * the end), the same key (`o_attributes`), a four-field record. */
int itemPackAttackEvent(intptr_t o_attributes, u32 index,
                        ITItemAttackEvent *out)
{
    u32 i;

    if (sItemPack == NULL || out == NULL)
    {
        return -1;
    }
    for (i = 0; i < sItemAEventCount; i++)
    {
        const u8 *p = sItemPack + sItemOffAEvents + i * ITEM_PACK_AEVENT_SIZE;
        u32 key;

        memcpy(&key, p + ITEM_AEVENT_KEY, 4);

        if ((intptr_t)key != o_attributes)
        {
            continue;
        }
        if ((i + index) >= sItemAEventCount)
        {
            return -1;
        }
        p = sItemPack + sItemOffAEvents + (i + index) * ITEM_PACK_AEVENT_SIZE;
        memcpy(&key, p + ITEM_AEVENT_KEY, 4);

        if ((intptr_t)key != o_attributes)
        {
            return -1;
        }
        memcpy(out, p, sizeof(*out));

        return 0;
    }
    return -1;
}

/* PK Fire's pillar: its ITDesc names &gFTNessFileSpecial1 and
 * &llNessSpecial1PKFireItemAttributes, the table's offset in that file.
 * The offset is this absolute symbol, as wpattrs.ld makes the weapons';
 * the table is a region-2 record under that key (tools/export/ssb_itemexport.py
 * FIGHTER_ITEMS), whose `data` is the stage items' non-NULL marker: the tree is
 * itnesspkfire.mdl, keyed by 0x34 in src/dc/itemmodel.c. gFTNessFileSpecial1
 * is bound to the spark's attribute blob by src/dc/wpattrs.c. */
#define ITEMPACK_ASM_STR_(x) #x
#define ITEMPACK_ASM_STR(x) ITEMPACK_ASM_STR_(x)
#define ITEMPACK_ASM_SYM(name) ITEMPACK_ASM_STR(__USER_LABEL_PREFIX__) #name

__asm__(".globl " ITEMPACK_ASM_SYM(llNessSpecial1PKFireItemAttributes) "\n"
        ".set " ITEMPACK_ASM_SYM(llNessSpecial1PKFireItemAttributes) ", 0x34\n");

/* Link's Bomb: its table is 0x40 into LinkMain, whose gFTDataLinkMain
 * wpattrs.c binds to the Spin Attack's blob -- which also carries the
 * Bomb's attack events and bloat scales, read raw (tools/export/ssb_wpattrexport.py
 * RAW_TABLES). */
__asm__(".globl " ITEMPACK_ASM_SYM(llLinkMainBombItemAttributes) "\n"
        ".set " ITEMPACK_ASM_SYM(llLinkMainBombItemAttributes) ", 0x40\n");

extern void *gFTNessFileSpecial1;
extern void *gFTDataLinkMain;

ITAttributes *itemPackAttr(void *file, intptr_t offset)
{
    u32 i;

    if (sItemPack == NULL || sItemAttrs == NULL || file == NULL)
    {
        return NULL;
    }
    /* The item pack's own region 0 is the only file this can answer for,
     * but for the fighter-owned items' files above; anything else is a
     * caller with a file of its own. DIVERGES: the decomp reads those
     * tables straight out of the fighter's file (itmanager.c). */
    if ((file != (void *)(sItemPack + sItemOffData)) && (file != gFTNessFileSpecial1) &&
        (file != gFTDataLinkMain))
    {
        return NULL;
    }
    for (i = 0; i < sItemAttrCount; i++)
    {
        if (item_pack_record_key(i) == (u32)offset)
        {
            return &sItemAttrs[i];
        }
    }
    return NULL;
}

void itemPackRelease(void)
{
    if (sItemPack == NULL)
    {
        return;
    }
    gITManagerCommonData = NULL;
#if defined(DB_ANIM_CHECK) && !defined(FT_HOSTTEST)
    fighter_animcheck_remove(sItemPack);
#endif
    free(sItemPack);                /* asset_read_whole's, see assetroot.h */
    sItemPack = NULL;
    sItemPackSize = 0;
    free(sItemAttrs);
    sItemAttrs = NULL;
    free(sItemWAttrs);
    sItemWAttrs = NULL;
    /* every count and offset too: item_pack_region multiplies the count
     * by the record size, and a stale one describes a pack that is gone */
    sItemDataSize = sItemModelSize = 0;
    sItemBlockCount = sItemAttrCount = 0;
    sItemOffData = sItemOffModels = sItemOffBlocks = sItemOffAttrs = 0;
    sItemEventCount = sItemAEventCount = sItemWAttrCount = 0;
    sItemOffEvents = sItemOffAEvents = sItemOffWAttrs = 0;
}

int itemPackLoad(const char *name)
{
    u8 *p;
    long size = 0;
    u32 fields[10];
    u32 i, fixup_count, off_fixups;
    u32 off_events, event_count;
    u32 off_aevents, aevent_count;
    u32 off_wattrs, wattr_count;
    u32 off_ifixups, ifixup_count;

    if (sItemPack != NULL)
    {
        return 0;
    }
    p = asset_read_whole(name, &size);

    if (p == NULL)
    {
        syDebugPrintf("itempack: %s is not in the romdisk\n", name);
        return -1;
    }
    if (size < ITEM_PACK_HEADER || memcmp(p, "ITCD", 4) != 0)
    {
        syDebugPrintf("itempack: %s is not an ITCD pack\n", name);
        free(p);
        return -1;
    }
    sItemPack = p;
    sItemPackSize = (u32)size;

    /* version, data_size, model_size, block_count, fixup_count, off_data,
     * off_models, off_blocks, off_fixups, off_attrs */
    for (i = 0; i < 10; i++)
    {
        memcpy(&fields[i], p + 4 + i * 4, 4);
    }
    sItemDataSize = fields[1];
    sItemModelSize = fields[2];
    sItemBlockCount = fields[3];
    fixup_count = fields[4];
    sItemOffData = fields[5];
    sItemOffModels = fields[6];
    sItemOffBlocks = fields[7];
    off_fixups = fields[8];
    sItemOffAttrs = fields[9];
    memcpy(&sItemAttrCount, p + 44, 4);
    memcpy(&off_events, p + 48, 4);
    memcpy(&event_count, p + 52, 4);
    memcpy(&off_aevents, p + 56, 4);
    memcpy(&aevent_count, p + 60, 4);
    memcpy(&off_wattrs, p + 64, 4);
    memcpy(&wattr_count, p + 68, 4);
    memcpy(&off_ifixups, p + 72, 4);
    memcpy(&ifixup_count, p + 76, 4);

    if (fields[0] != ITEM_PACK_VERSION)
    {
        syDebugPrintf("itempack: %s is version %u, this build reads %u\n",
                      name, (unsigned)fields[0], (unsigned)ITEM_PACK_VERSION);
        itemPackRelease();
        return -1;
    }

    /* Every section inside the file, before anything is read through a
     * pointer taken from it. */
    if (sItemOffData + sItemDataSize > sItemPackSize ||
        sItemOffModels + sItemModelSize > sItemPackSize ||
        sItemOffBlocks + sItemBlockCount * ITEM_PACK_BLOCK > sItemPackSize ||
        off_fixups + fixup_count * ITEM_PACK_FIXUP > sItemPackSize ||
        sItemOffAttrs + sItemAttrCount * ITEM_PACK_RECORD_SIZE > sItemPackSize ||
        off_events + event_count * ITEM_PACK_EVENT_SIZE > sItemPackSize ||
        off_aevents + aevent_count * ITEM_PACK_AEVENT_SIZE > sItemPackSize ||
        off_wattrs + wattr_count * ITEM_PACK_WATTR_SIZE > sItemPackSize ||
        off_ifixups + ifixup_count * ITEM_PACK_FIXUP > sItemPackSize)
    {
        syDebugPrintf("itempack: %s has a section past its end\n", name);
        itemPackRelease();
        return -1;
    }

    /* The fixups: the (site, target) pairs the exporter leaves for whoever
     * reads an `ITAttributes` off region 0 without going through region 2.
     * Each is bounds-checked here on both architectures, and applied only
     * where the slot can actually hold the result -- see the note in the
     * loop. Read-only on the host, which is the honest state: a truncated
     * pointer is not a pointer. */
    for (i = 0; i < fixup_count; i++)
    {
        u32 site, target;
        void *addr;

        memcpy(&site, p + off_fixups + i * ITEM_PACK_FIXUP, 4);
        memcpy(&target, p + off_fixups + i * ITEM_PACK_FIXUP + 4, 4);

        /* The target the exporter writes is an ABSOLUTE offset in the
         * pack -- `off_models + <offset in file 86>` -- not a region-1
         * one, because the fixup is applied by the loader and the loader
         * is the side that knows where region 1 landed. So the check is
         * against the model region's EXTENT, not its size. */
        if (site + 4 > sItemDataSize ||
            target < sItemOffModels ||
            target + 4 > sItemOffModels + sItemModelSize)
        {
            syDebugPrintf("itempack: %s: fixup %u out of range\n", name, i);
            itemPackRelease();
            return -1;
        }
        /* The slot is the file's own FOUR bytes, and on the target a
         * pointer is four bytes, so writing the resolved address there is
         * what a raw ITAttributes read off region 0 would follow.
         *
         * On a 64-bit host it is not: the slot cannot hold the pointer and
         * this would store its low half -- a value that LOOKS like an
         * address and is not, which is a worse trap than the file-86
         * number it replaced. So the host skips the write and the slots
         * keep the numbers the ROM put there; its attribute reads go
         * through region 2, which is what that region is for (see the
         * ITDesc note in src/dc/itstar.c). The table is still walked and
         * bounds-checked either way, so a malformed pack is still caught
         * on both. */
        if (sizeof(void *) > 4)
        {
            continue;
        }
        addr = p + target;
        memcpy(p + sItemOffData + site, &addr, 4);
    }

    /* The model region's own pointers. The loop above is
     * ITCommonData reaching into ITCommonObject; this is ITCommonObject
     * reaching into itself, the file's own intern reloc chain, which the
     * exporter used to drop. On the N64 lbRelocLoadAndRelocFile walks
     * both chains and writes an address into every site, so a word that
     * is not fixed up here is not a pointer the game can follow: it is
     * still the ROM's reloc descriptor, a {u16 next, u16 target/4} pair.
     *
     * What that cost: `MatAnimJoint BombHeiWalk` carries eight of these,
     * and gcParseMObjMatAnimJoint parses commands in a
     * `do { ... } while (anim_wait <= 0.0F)` loop -- so a Bob-omb that
     * walked long enough to reach one read a descriptor as its next
     * command, never set a wait, and the game thread stopped there. It
     * took a four-minute match to reach and it was deterministic;
     * -DDB_ITEM_RAIN on any stage is what reproduces it.
     *
     * Both sides are absolute pack offsets, as the other table's target
     * is, and the site is a word in the model region rather than region
     * 0 -- which is why this is a table of its own and not more rows in
     * that one. The host arm is the same trade for the same reason: a
     * four-byte slot cannot hold a 64-bit pointer, and writing its low
     * half would make a number that looks like an address. */
    for (i = 0; i < ifixup_count; i++)
    {
        u32 site, target;
        void *addr;

        memcpy(&site, p + off_ifixups + i * ITEM_PACK_FIXUP, 4);
        memcpy(&target, p + off_ifixups + i * ITEM_PACK_FIXUP + 4, 4);

        if (site < sItemOffModels ||
            site + 4 > sItemOffModels + sItemModelSize ||
            target < sItemOffModels ||
            target + 4 > sItemOffModels + sItemModelSize)
        {
            syDebugPrintf("itempack: %s: model reloc %u out of range\n",
                          name, i);
            itemPackRelease();
            return -1;
        }
        if (sizeof(void *) > 4)
        {
            continue;
        }
        addr = p + target;
        memcpy(p + site, &addr, 4);
    }

    /* The 34 tables, in host layout, once. `ip->attr` keeps the pointer
     * for the item's whole life, so these cannot be built per lookup --
     * see the section above. Nothing reads through the pack's own record
     * after this except itemPackGetAttr, which wants the fields. */
    sItemOffEvents = off_events;
    sItemEventCount = event_count;
    sItemOffAEvents = off_aevents;
    sItemAEventCount = aevent_count;
    sItemOffWAttrs = off_wattrs;
    sItemWAttrCount = wattr_count;

    sItemAttrs = malloc(sItemAttrCount * sizeof(*sItemAttrs));

    if (sItemAttrs == NULL)
    {
        syDebugPrintf("itempack: %s: no room for %u attribute tables\n",
                      name, sItemAttrCount);
        itemPackRelease();
        return -1;
    }
    for (i = 0; i < sItemAttrCount; i++)
    {
        ITItemAttr ia;

        item_pack_read_attr(p + sItemOffAttrs + i * ITEM_PACK_RECORD_SIZE, &ia);
        item_pack_setup_attr(&sItemAttrs[i], &ia);
    }

    sItemWAttrs = malloc(sItemWAttrCount * sizeof(*sItemWAttrs));

    if (sItemWAttrs == NULL)
    {
        syDebugPrintf("itempack: %s: no room for %u weapon tables\n",
                      name, sItemWAttrCount);
        itemPackRelease();
        return -1;
    }
    for (i = 0; i < sItemWAttrCount; i++)
    {
        item_pack_setup_wattr(&sItemWAttrs[i],
                              p + sItemOffWAttrs + i * ITEM_PACK_WATTR_SIZE);
    }

    gITManagerCommonData = p + sItemOffData;
#if defined(DB_ANIM_CHECK) && !defined(FT_HOSTTEST)
    /* ITCommonData's MatAnimJoints are scripts the castle bumper runs */
    fighter_animcheck_add(sItemPack, (uint32_t)sItemPackSize, name);
#endif

    syDebugPrintf("itempack: %s: %u blocks (%u attrs), %u fixups, "
           "%u model relocs, %u data + %u models\n", name, sItemBlockCount,
           sItemAttrCount, fixup_count, ifixup_count, sItemDataSize,
           sItemModelSize);
    return 0;
}

/* itempack.h -- the item pack (ITCD), written by
 * tools/export/ssb_itemexport.py.
 *
 * `ITCommonData` (reloc file 0xFB) is what every item's `o_attributes`
 * reads through, and `ITCommonObject` (0x56) is where those tables'
 * pointers land. Every ported item file LEFT OUT its itXxxMakeItem wrapper --
 * the tail needs ifCommonItemArrowMakeInterface and this file.
 *
 * THREE REGIONS, and the split is the whole design:
 *
 *   r0  ITCommonData's own bytes. Still carried because some of its
 *       blocks ARE read as bytes -- `VelocitiesY Container` is
 *       pointer-free s16 rows, `AttackEvents` is a row of hit events.
 *   r1  ITCommonObject's bytes, the item models. A block's offset here
 *       is an offset into that file.
 *   r2  the 34 `ItemAttributes` tables, PARSED. These are the reason the
 *       pack is not just two blobs: `ITAttributes` opens with four
 *       POINTERS, so on the N64 it is 16 bytes of them and on a 64-bit
 *       host it is 32, and reading the raw bytes as a host struct puts
 *       every field an item reads sixteen bytes out. So each table is
 *       parsed from the decomp's typed initializer (where the pointer
 *       fields name symbols) and written here in HOST layout, with the
 *       pointers as pack-absolute offsets. Each record ends with the
 *       table's own offset in region 0 -- the number an ITDesc carries --
 *       which is the key itemPackAttr looks it up by.
 *
 * That is the port's answer to this class of bug, and this is its third
 * appearance: `union AObjEvent32` is eight bytes on x86-64, which is why
 * every script attach is `#ifdef FT_HOSTTEST`-guarded, and
 * `MPYakumonoDObj`'s flexible-array member is the second. A fighter's
 * attributes are the model for the fix -- tools/export/ssb_packexport.py ships
 * `FPackAttr`, not `FTAttributes`.
 *
 * NOTHING HERE IS CAST. The record is 81 bytes of fields padded to 84,
 * then the 4-byte key, and a C struct with a `u8` before a `u16` picks up
 * padding the writer knows nothing about -- so every field is read with
 * its own memcpy and the pack's own layout is the only thing that decides
 * where it sits. itemPackAttr goes one further and writes the fields into
 * an `ITAttributes` BY NAME, because that struct mixes four pointers with
 * two long bitfield runs and its layout is the compiler's business.
 */
#ifndef SSB_DC_ITEMPACK_H
#define SSB_DC_ITEMPACK_H

#include <ssb_types.h>
#include <it/ittypes.h>        /* ITAttributes, the struct this hands back */
#include <wp/wptypes.h>        /* WPAttributes, the weapon one */

#define ITEM_PACK_REGION_DATA   0
#define ITEM_PACK_REGION_MODELS 1
#define ITEM_PACK_REGION_ATTRS  2

/* The four pointer fields' "there is none" -- the same value the
 * exporter writes for a NULL in the decomp's initializer. */
#define ITEM_PACK_NO_PTR 0xFFFFFFFFu

#define ITEM_PACK_RECORD_SIZE 88

/* The MONSTER EVENTS: one record per `ITMonsterEvent`, the
 * table Porygon's and Venusaur's own update steps through. Written the
 * same way the attribute records are -- parsed out of the decomp's typed
 * initializer and put down as plain host fields -- because the ROM's
 * struct is BITFIELDS (`s32 angle : 10; u32 damage : 8; ub32 can_setoff :
 * 1`) and IDO and GCC need not lay those out alike. The key is the
 * `o_attributes` of the item the table belongs to. */
#define ITEM_PACK_EVENT_SIZE 48

/* where the record's own key sits, past the eleven fields */
#define ITEM_EVENT_KEY 44

typedef struct ITItemEvent
{
    u32 timer;
    u32 angle;
    u32 damage;
    u32 size;
    u32 knockback_scale;
    u32 knockback_weight;
    u32 knockback_base;
    s32 element;
    u32 can_setoff;
    u32 shield_damage;
    u32 fgm_id;
} ITItemEvent;

int itemPackMonsterEvent(intptr_t o_attributes, u32 index, ITItemEvent *out);

/* An `ITAttackEvent`, in host layout -- the four-field hitbox script an
 * explosion runs (Marumine's, and the base items', which ride in region
 * 0's own bytes). Same reason for the shape as ITItemEvent above: the
 * ROM's struct is BITFIELDS (`s32 angle : 10; u32 damage : 8`). */
#define ITEM_PACK_AEVENT_SIZE 20
#define ITEM_AEVENT_KEY 16

/* One `WPAttributes` record, in host layout; see itemPackWeaponAttr. */
#define ITEM_PACK_WATTR_SIZE 64
#define ITEM_WATTR_KEY 60

typedef struct ITItemAttackEvent
{
    u32 timer;
    u32 angle;
    u32 damage;
    u32 size;
} ITItemAttackEvent;

/* One event out of the table the item's `o_attributes` names. Returns 0,
 * or -1 when the pack has no such table or the index runs past its end. */
int itemPackAttackEvent(intptr_t o_attributes, u32 index,
                        ITItemAttackEvent *out);

/* The `WPAttributes` a stage weapon's WPDesc names -- `*wp_desc->p_weapon`
 * is the STAGE's map file, which this port has not got, so Saffron City's
 * Charmander and Venusaur carry their flame and razor tables here instead,
 * keyed by the desc's `o_attributes` and materialised into a real
 * `WPAttributes` the way itemPackAttr materialises an ITAttributes.
 * ITCommonData's own twelve weapon tables ride here too: the pack's
 * region 0 holds them as the ROM's big-endian bitfields, which read as
 * garbage through the SH4's (the Fire Flower's flame did 0%).
 *
 * NULL when the pack is not loaded, when `file` is not the item pack's own
 * region 0, or when `offset` names no table -- and a wpManagerMakeWeapon
 * that gets NULL falls back to the byte overlay, which is what every
 * FIGHTER's own weapon still does (and which is correct for them: four
 * pointers are 16 bytes on the SH4 as on the N64).
 *
 * The returned struct lives as long as the pack, as itemPackAttr's does. */
WPAttributes *itemPackWeaponAttr(void *file, intptr_t offset);

/* One `ItemAttributes` table, in host layout. The four offsets are into
 * region 1, or ITEM_PACK_NO_PTR. The scalars are the decomp's own field
 * order, and the two flag words pack the bitfields that order declares:
 * `flags` is xlu, item_dobjs, colanim, give_hitlag, weight (bits 0-4) and
 * `flags2` is setoff, rehit_item, rehit_fighter, hop, reflect, shield
 * (bits 0-5). */
typedef struct ITItemAttr
{
    u32 data_off, mobjsubs_off, animjoints_off, matanimjoints_off;
    u32 flags;
    s16 atk0[3], atk1[3];
    s16 dcoll_off[3], dcoll_size[3];
    s16 map_top, map_center, map_bottom, map_width;
    u16 size;
    s16 angle;
    u16 knockback_scale;
    u8 damage, element;
    u16 knockback_weight;
    u8 shield_damage, attack_count;
    u32 flags2;
    u16 hit_sfx;
    u8 priority;
    u16 knockback_base;
    u8 type, hitstatus;
    u16 drop_sfx, throw_sfx, smash_sfx;
    u16 vel_scale, spin_speed;
} ITItemAttr;

/* Load the pack and publish it: points gITManagerCommonData at region 0,
 * so `lbRelocGetFileData(ITAttributes*, gITManagerCommonData, off)`
 * resolves the way the game's does. Returns 0, or -1 with the reason on
 * dbglog. Safe to call twice -- the second is a no-op. */
int itemPackLoad(const char *name);

/* Give the blob back. Safe on an unloaded pack. */
void itemPackRelease(void);

/* One `ItemAttributes` table by name, as the descriptions spell it -- the
 * port's stand-in for `&llITCommonData<Name>ItemAttributes`. `out` is
 * filled; returns 0, or -1 for a name the pack has not got. */
int itemPackGetAttr(const char *name, ITItemAttr *out);

/* The `ITAttributes` an `ITDesc` names: `file` is `*item_desc->p_file` and
 * `offset` is `item_desc->o_attributes`, the two things
 * `lbRelocGetFileData(ITAttributes*, file, offset)` is handed. The decomp
 * reads the bytes at that offset as the struct; the port cannot, because
 * `ITAttributes` opens with four POINTERS -- 16 bytes on the N64 and 32
 * here -- so the same offset addresses different fields. The pack ships
 * the 34 tables parsed, and this is the lookup that finds one and the
 * materialiser that hands it back in host layout.
 *
 * NULL when the pack is not loaded, when `file` is not the item pack's own
 * region 0, or when `offset` names no table. A caller with a file of its
 * own falls back to the byte arithmetic on NULL, which is what it was doing anyway.
 *
 * The returned struct lives as long as the pack (ip->attr keeps the
 * pointer for the item's whole life), so it is built once at load and
 * never freed but by itemPackRelease. */
ITAttributes *itemPackAttr(void *file, intptr_t offset);

/* A region's block by name: its address and, when `size_out` is not NULL,
 * the offset of the NEXT block in that region minus this one -- which is
 * a block's extent only because the exporter writes them in file order,
 * and is what a caller walking one wants. NULL for a name the pack has
 * not got. */
const void *itemPackBlock(int region, const char *name, u32 *size_out);

/* The pack's load state, for a test that wants to know without asking
 * dbglog. */
int itemPackLoaded(void);

#endif /* SSB_DC_ITEMPACK_H */

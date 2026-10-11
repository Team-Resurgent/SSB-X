/* wpattrs.h -- the fighters' weapon hitbox tables (WPAttributes) on target.
 *
 * A fighter's projectile reads its table through the decomp's own
 * arithmetic in wpManagerMakeWeapon (src/dc/wpmanager.c, verbatim):
 *
 *   lbRelocGetFileData(WPAttributes*, *wp_desc->p_weapon, wp_desc->o_attributes)
 *   == *gFTData<Char><File> + (intptr_t)&ll<Char><File><Weapon>WeaponAttributes
 *
 * The game has the fighter's relocData file loaded at gFTData<Char><File>
 * and the ll* symbol is an absolute linker symbol, the table's byte
 * offset in that file. The port has neither, so:
 * tools/export/ssb_wpattrexport.py bakes each file's tables into
 * romdisk/wpattrs.bin, one blob per file with every record laid at its ROM
 * offset, and generates wpattrs.ld, which gives the ll* symbols those offsets
 * at link time (the particlebanks.ld mechanism). wpAttrsBind then points each
 * gFTData* at its blob, and the verbatim arithmetic lands on the table.
 *
 * The record is the item pack's own 64-byte WATTR record
 * (tools/export/ssb_itemexport.py encode_weapon_attr); wpAttrsSetupRecord is
 * the one reader of it, shared with src/dc/itempack.c so the two cannot
 * drift. */
#ifndef SSB_DC_WPATTRS_H
#define SSB_DC_WPATTRS_H

#include <ssb_types.h>
#include <wp/wptypes.h>        /* WPAttributes */

#define WPATTR_RECORD_SIZE 64
#define WPATTR_NO_PTR      0xFFFFFFFFu

/* One WATTR record into a WPAttributes, field by field (the record is
 * host fields, byte-tight, never a struct cast). A pointer field that is
 * WPATTR_NO_PTR reads NULL; any other value reads `ptr_base + value` --
 * the item pack passes its model region so its stage weapons' pointers
 * are real, the fighter blobs pass a marker so `data` is merely non-NULL
 * (the only pointer field anything reads on target, wpManagerIsModelLess). */
void wpAttrsSetupRecord(WPAttributes *attr, const u8 *rec, const u8 *ptr_base);

/* Read romdisk/wpattrs.bin once (later calls return 0 at once) and build
 * the per-file blobs. -1 if the file is missing or malformed; then no
 * bind happens and every fighter weapon spawns model-less as before. */
int wpAttrsLoad(void);

/* Point every fighter weapon file's gFTData* global at its blob. Called
 * from ftCommonOverlayLoad on every overlay-3 reload: the globals are the
 * decomp's own, in ft<char>.o's ovl2_noload segment, which overlay 2's
 * reload has just bzeroed. Loads the file first if it is not yet. */
void wpAttrsBind(void);

/* Loaded blobs no bind row names (0 when every file is bound). */
int wpAttrsUnboundFiles(void);

/* Point wppikachuthunderjolt.c's two ThunderJoltB tables at the loaded
 * jolt pack's animation and MatAnimJoint (src/dc/wpattrs.c). */
void wpAttrsBindJoltB(void **anim_joints, void ***matanim_joints, unsigned n);

/* The blob a file id was given, or NULL: the host test's oracle. */
const void *wpAttrsFileBlob(int file_id);

/* src/dc/wpmanager.c: load every weapon's model pack now rather than when
 * the weapon is first made, so a match reads nothing off the disc. Kept
 * for the run. Declared here because the port has no header of its own
 * beside wp/wpmanager.h, and this is its weapon-side one. */
void wpManagerPreloadModels(void);
/* and give them back (src/dc/scmanager.c, the movie's start) */
void wpManagerReleaseModels(void);

#endif /* SSB_DC_WPATTRS_H */

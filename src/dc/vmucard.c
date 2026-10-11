/* vmucard.c -- see vmucard.h.
 *
 * Three parts. The first is common to the console and the host: the
 * save's pictures, the file's size, and the game's own test of a
 * payload. The second is the console's: the maple bus and vmufs. The
 * third is the host's fake of the second, a row of cards in RAM.
 *
 * ---- what a damaged file is -------------------------------------------
 *
 * Until this file there was no such thing. The save was read through
 * fs_vmu, which hands back the file's data whether its CRC holds or not,
 * so a damaged file went into the store as it stood; the game then found
 * neither copy valid, installed its defaults and wrote them over the
 * file. The outcome was right and the path was an accident. Now the file
 * is judged before any of it is loaded, and a file that fails -- its VMS
 * CRC, its app id, its length, or both of the game's copies -- is left on
 * the card and out of the store. What the game sees is the same: a store
 * of zeroes is a cartridge never written. What the port gains is knowing,
 * what the boot check uses to judge the save.
 *
 * One copy bad and the other good is not damage. That is the cartridge
 * lb/lbbackup.c already repairs (lbBackupIsSramValid's second read), and
 * the repair is the game's to make.
 */
#include "vmucard.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/obj.h>
#include <sys/debug.h>
#include <lb/lbtypes.h>
#include <lb/lbbackup.h>

#include "assetroot.h"

/* The two copies' offsets and the store's extent, in lbBackupWrite's own
 * arithmetic (lb/lbbackup.c:39-40); src/dc/vmusave.c spells it the same
 * way and says why. */
#define CARD_COPY2 (((sizeof(LBBackupData) + 0xFu) & ~0xFu))
#define CARD_PAYLOAD (CARD_COPY2 + sizeof(LBBackupData))

/* The VMS header (KOS's vmu_hdr_t) and the eyecatch kind the art uses. */
#define CARD_HDR_SIZE 128
#define CARD_EC_16COL 3

/* ---- the save's pictures ----------------------------------------------
 *
 * The icon the BIOS's file manager shows, three 32x32 frames at four bits
 * a pixel; the 72x56 sixteen-colour eyecatch; and the VMU LCD image,
 * which is rendered separately. The N64 has none of
 * them -- a cartridge's save has no pictures -- so they are the port's,
 * made at build time out of the game's own emblem and wordmark by
 * tools/export/ssb_vmuart.py, whose docstring has the layout read here.
 * Read once, at vmucard_init, and kept: 3820 bytes. */
#define VMUART_MAGIC   0x41554D56u /* "VMUA", little-endian */
#define VMUART_VERSION 1
#define VMUART_ICONS   3
#define VMUART_EC_SIZE (32 + (72 * 56 / 2))
#define VMUART_LCD     (48 * 32 / 8)

typedef struct VMUArtHeader
{
    uint32_t magic;
    uint16_t version;
    uint16_t icon_cnt;
    uint16_t icon_speed;
    uint16_t eyecatch_type;
    uint16_t icon_pal[16];
} VMUArtHeader;

_Static_assert(sizeof(VMUArtHeader) == 44, "vmuart.bin header");

static int sVMUCardReady;
static const VMUArtHeader *sVMUArt;
static uint8_t *sVMUArtIcons;
static uint8_t *sVMUArtEyecatch;
static uint8_t *sVMUArtLCD;

static void vmuart_load(void)
{
    long size;
    uint8_t *blob;
    const VMUArtHeader *h;
    long want;

    if ((blob = asset_read_whole("vmuart.bin", &size)) == NULL)
    {
        syDebugPrintf("save: no vmuart.bin; the save gets the plain icon\n");
        return;
    }
    h = (const VMUArtHeader *)blob;
    want = (long)sizeof(*h) + (512 * VMUART_ICONS) + VMUART_EC_SIZE +
           VMUART_LCD;
    if ((size != want) || (h->magic != VMUART_MAGIC) ||
        (h->version != VMUART_VERSION) || (h->icon_cnt != VMUART_ICONS) ||
        (h->eyecatch_type != CARD_EC_16COL))
    {
        syDebugPrintf("save: vmuart.bin is not the file this build wants "
                      "(%ld bytes); the save gets the plain icon\n", size);
        free(blob);
        return;
    }
    sVMUArt = h;
    sVMUArtIcons = blob + sizeof(*h);
    sVMUArtEyecatch = sVMUArtIcons + (512 * VMUART_ICONS);
    sVMUArtLCD = sVMUArtEyecatch + VMUART_EC_SIZE;
}

/* The fallback when vmuart.bin is missing: a mark drawn here, 32x32 out
 * of a sixteen-entry ARGB4444 palette of which it uses three. */
static uint8_t sSaveIcon[512];

static void save_icon_build(void)
{
    int x, y;

    memset(sSaveIcon, 0, sizeof(sSaveIcon));

    for (y = 0; y < 32; y++)
    {
        for (x = 0; x < 32; x++)
        {
            /* doubled coordinates, so the centre falls between pixels
             * and the mark comes out symmetric on both axes */
            int dx = 2 * x - 31;
            int dy = 2 * y - 31;
            int idx;

            if ((dx * dx) + (dy * dy) > (30 * 30))
            {
                continue; /* outside the ball: transparent */
            }
            idx = ((dx > -6) && (dx < 6)) || ((dy > -6) && (dy < 6)) ? 1 : 2;
            sSaveIcon[(y * 16) + (x / 2)] |=
                (uint8_t)(idx << (((x & 1) != 0) ? 0 : 4));
        }
    }
}

void vmucard_init(void)
{
    if (sVMUCardReady != 0)
    {
        return;
    }
    sVMUCardReady = 1;
    save_icon_build();
    vmuart_load();
}

int vmucard_file_blocks(int len)
{
    int bytes = CARD_HDR_SIZE + len;

    bytes += (sVMUArt != NULL) ? (512 * VMUART_ICONS) + VMUART_EC_SIZE : 512;

    return (bytes + 511) / 512;
}

/* lbBackupIsChecksumValid (lb/lbbackup.c:26), asked of a copy that is not
 * gSCManagerBackupData: the same sum, over the same bytes, and the same
 * signature. */
static int payload_copy_ok(const uint8_t *copy)
{
    LBBackupData backup;

    memcpy(&backup, copy, sizeof(backup));

    return (lbBackupCreateChecksum(&backup) == backup.checksum) &&
           (backup.signature == 666);
}

int vmucard_payload_ok(const void *payload, int len)
{
    const uint8_t *p = payload;

    if (len < (int)CARD_PAYLOAD)
    {
        return 0;
    }
    return payload_copy_ok(p) || payload_copy_ok(p + CARD_COPY2);
}

const char *vmucard_error_name(int rv)
{
    switch (rv)
    {
    case nVMUCardOK:
        return "ok";
    case nVMUCardErrGone:
        return "no card there any more";
    case nVMUCardErrFull:
        return "not enough free blocks";
    case nVMUCardErrMemory:
        return "no memory to build the file";
    case nVMUCardErrBlank:
        return "the card is not formatted (format it in the Dreamcast's file manager)";
    default:
        return "the card refused the write";
    }
}

static void info_clear(VMUCardInfo *info, int port, int unit)
{
    memset(info, 0, sizeof(*info));
    info->port = port;
    info->unit = unit;
    info->state = nVMUCardNoSave;
    info->free_blocks = -1;
}

#ifndef FT_HOSTTEST

static int bcd(uint8_t v)
{
    return ((v >> 4) * 10) + (v & 0xF);
}

#include <kos.h>
#include <dc/maple.h>
#include <dc/vmufs.h>
#include <dc/vmu_pkg.h>

_Static_assert(VMUPKG_EC_16COL == CARD_EC_16COL, "eyecatch kind");
_Static_assert(sizeof(vmu_hdr_t) == CARD_HDR_SIZE, "VMS header");

static maple_device_t *card_dev(int port, int unit)
{
    maple_device_t *dev;

    if ((port < 0) || (port >= MAPLE_PORT_COUNT) || (unit < 0) ||
        (unit >= MAPLE_UNIT_COUNT))
    {
        return NULL;
    }
    dev = maple_enum_dev(port, unit);

    return ((dev != NULL) && ((dev->info.functions & MAPLE_FUNC_MEMCARD) != 0))
               ? dev
               : NULL;
}

/* Whether the card at dev has a filesystem KOS can use, before vmufs is
 * asked to. vmufs_setup trusts the root block: on a card that was never
 * formatted (erased flash reads 0xFF) it takes 65535 for the FAT's size
 * and asks malloc for 32 MB, twice, and every later call refuses. The
 * root is block 255 and opens with sixteen 0x55 bytes.
 *
 * A read that times out is the other trap: KOS's vmu_block_read marks the
 * frame vacant while it is still on the maple queue, so the next use of
 * that card's frame trips an assert in maple_frame_init and the console
 * aborts. So a card whose root does not answer is never touched again
 * this session (sDead). Looked at on every call otherwise, so a card
 * formatted or swapped while the game runs is noticed. Never logs: the
 * writer thread calls it. */
enum { CARD_OK = 0, CARD_BLANK = 1, CARD_DEAD = 2 };
static uint8_t sDead[MAPLE_PORT_COUNT][MAPLE_UNIT_COUNT];

static int card_root_state(maple_device_t *dev)
{
    static uint8_t root[512] __attribute__((aligned(32)));
    int i;

    if (sDead[dev->port][dev->unit])
    {
        return CARD_DEAD;
    }
    if (vmu_block_read(dev, 255, root) != MAPLE_EOK)
    {
        sDead[dev->port][dev->unit] = 1;
        return CARD_DEAD;
    }
    for (i = 0; i < 16; i++)
    {
        if (root[i] != 0x55)
        {
            return CARD_BLANK;
        }
    }
    return CARD_OK;
}

int vmucard_present(int port, int unit)
{
    return card_dev(port, unit) != NULL;
}

const char *vmucard_where(int port, int unit)
{
    static char where[32];

    snprintf(where, sizeof(where), "/vmu/%c%d/%s", 'a' + port, unit,
             VMUCARD_NAME);
    return where;
}

/* vmu_pkg_parse keeps the field's padding. vmu_pkg_build pads app_id
 * with NULs (a strcpy into a zeroed header) and desc_* with spaces, and a
 * file another tool has copied may have either, so both are padding. */
static int card_app_id_is(const char *field, const char *want)
{
    size_t n = strlen(want);
    size_t i;

    if (strncmp(field, want, n) != 0)
    {
        return 0;
    }
    for (i = n; field[i] != '\0'; i++)
    {
        if (field[i] != ' ')
        {
            return 0;
        }
    }
    return 1;
}

/* Judge a whole file as vmufs_read hands it back: the VMS header and its
 * CRC, which is KOS's to check (vmu_pkg_parse), then ours. */
static const char *card_judge(uint8_t *file, int size, int len,
                              const uint8_t **payload)
{
    vmu_pkg_t pkg;

    if (size < CARD_HDR_SIZE)
    {
        return "short";
    }
    if (vmu_pkg_parse(file, size, &pkg) < 0)
    {
        return "crc";
    }
    if (card_app_id_is(pkg.app_id, "SSB64-DC") == 0)
    {
        return "app id";
    }
    if (pkg.data_len < len)
    {
        return "short";
    }
    if (vmucard_payload_ok(pkg.data, len) == 0)
    {
        return "copies";
    }
    *payload = pkg.data;

    return NULL;
}

static int card_read(int port, int unit, void *payload, int len,
                     VMUCardInfo *info)
{
    maple_device_t *dev;
    vmu_dir_t *dir;
    void *file;
    int count, size, i, rv;

    info_clear(info, port, unit);

    if ((dev = card_dev(port, unit)) == NULL)
    {
        return -1;
    }
    if ((rv = card_root_state(dev)) != CARD_OK)
    {
        info->why = (rv == CARD_BLANK) ? "not formatted" : "no answer";
        return info->state;
    }
    if ((rv = vmufs_free_blocks(dev)) >= 0)
    {
        info->free_blocks = rv;
    }
    if (vmufs_readdir(dev, &dir, &count) < 0)
    {
        return info->state;
    }
    for (i = 0; i < count; i++)
    {
        const uint8_t *data = NULL;

        /* vmufs_dir_find's own comparison */
        if (strncmp(VMUCARD_NAME, dir[i].filename, 12) != 0)
        {
            continue;
        }
        info->file_blocks = dir[i].filesize;
        /* The century is 0x19 or not: KOS's vmufs_dir_fill_time adds
         * 0x19 to it in binary rather than BCD, so every file it has
         * stamped since 2000 says 0x1a where the BIOS would say 0x20. */
        info->year = ((dir[i].timestamp.cent == 0x19) ? 1900 : 2000) +
                     bcd(dir[i].timestamp.year);
        info->month = bcd(dir[i].timestamp.month);
        info->day = bcd(dir[i].timestamp.day);
        info->hour = bcd(dir[i].timestamp.hour);
        info->min = bcd(dir[i].timestamp.min);

        if (vmufs_read_dirent(dev, &dir[i], &file, &size) < 0)
        {
            info->state = nVMUCardCorrupt;
            info->why = "read";
            break;
        }
        if ((info->why = card_judge(file, size, len, &data)) != NULL)
        {
            info->state = nVMUCardCorrupt;
        }
        else
        {
            info->state = nVMUCardValid;
            memcpy(payload, data, len);
        }
        free(file);
        break;
    }
    free(dir);

    return info->state;
}

int vmucard_read(int port, int unit, void *payload, int len,
                 VMUCardInfo *info)
{
    int took = asset_io_card_begin(), rv;

    rv = card_read(port, unit, payload, len, info);
    asset_io_card_end(took);

    return rv;
}

int vmucard_scan(VMUCardInfo *out, int max)
{
    static uint8_t scratch[CARD_PAYLOAD];
    maple_device_t *dev;
    int n = 0;

    while ((n < max) && ((dev = maple_enum_type(n, MAPLE_FUNC_MEMCARD)) != NULL))
    {
        vmucard_read(dev->port, dev->unit, scratch, CARD_PAYLOAD, &out[n]);
        n++;
    }
    return n;
}

static void card_pkg(vmu_pkg_t *pkg, const void *payload, int len)
{
    memset(pkg, 0, sizeof(*pkg));

    strcpy(pkg->desc_short, "SUPER SMASH BROS");
    strcpy(pkg->desc_long, "Save data -- records and options");
    strcpy(pkg->app_id, "SSB64-DC");

    if (sVMUArt != NULL)
    {
        pkg->icon_cnt = sVMUArt->icon_cnt;
        pkg->icon_anim_speed = sVMUArt->icon_speed;
        pkg->eyecatch_type = sVMUArt->eyecatch_type;
        memcpy(pkg->icon_pal, sVMUArt->icon_pal, sizeof(pkg->icon_pal));
        pkg->icon_data = sVMUArtIcons;
        pkg->eyecatch_data = sVMUArtEyecatch;
    }
    else
    {
        pkg->icon_cnt = 1;
        pkg->icon_anim_speed = 0;
        pkg->eyecatch_type = VMUPKG_EC_NONE;
        pkg->icon_pal[0] = 0x0000; /* transparent */
        pkg->icon_pal[1] = 0xFFFF; /* white */
        pkg->icon_pal[2] = 0xFF80; /* orange */
        pkg->icon_data = sSaveIcon;
    }
    pkg->data = payload;
    pkg->data_len = len;
}

/* The blocks the save file already holds on dev, which an overwrite gives
 * back before it takes its own; 0 when there is none, -1 when the
 * directory cannot be read. */
static int card_old_blocks(maple_device_t *dev)
{
    vmu_dir_t *dir;
    int count, i, blocks = 0;

    if (vmufs_readdir(dev, &dir, &count) < 0)
    {
        return -1;
    }
    for (i = 0; i < count; i++)
    {
        if (strncmp(VMUCARD_NAME, dir[i].filename, 12) == 0)
        {
            blocks = dir[i].filesize;
            break;
        }
    }
    free(dir);

    return blocks;
}

static int card_write(int port, int unit, const void *payload, int len)
{
    vmu_pkg_t pkg;
    maple_device_t *dev;
    uint8_t *built, *file;
    int built_len, file_len, free_blocks, old_blocks, rv;

    if ((dev = card_dev(port, unit)) == NULL)
    {
        return nVMUCardErrGone;
    }
    if ((rv = card_root_state(dev)) != CARD_OK)
    {
        return (rv == CARD_BLANK) ? nVMUCardErrBlank : nVMUCardErrWrite;
    }
    /* Room first, so vmufs_write never reaches its own "not enough
     * space" path: that one deletes the old file from its copy of the
     * directory before it finds out, and it logs, which this thread must
     * not do (vmucard.h). */
    if (((free_blocks = vmufs_free_blocks(dev)) < 0) ||
        ((old_blocks = card_old_blocks(dev)) < 0))
    {
        return nVMUCardErrWrite;
    }
    if (vmucard_file_blocks(len) > free_blocks + old_blocks)
    {
        return nVMUCardErrFull;
    }
    /* vmu_pkg_build and vmufs_write, not fs_vmu. fs_vmu keeps its own
     * copy of the header, and that copy (kernel/arch/dreamcast/fs/
     * fs_vmu.c vmu_pkg_dup) sizes a 16-colour eyecatch without its
     * 32-byte palette, so vmu_pkg_build then reads 32 bytes past the end
     * of it into the file. Built here, the eyecatch is ours and whole.
     * vmufs_write rounds the size up to a block and reads the rounding
     * out of the buffer too, so the buffer is padded to one first. */
    card_pkg(&pkg, payload, len);
    if (vmu_pkg_build(&pkg, &built, &built_len) < 0)
    {
        return nVMUCardErrMemory;
    }
    file_len = (built_len + 511) & ~511;
    if ((file = realloc(built, file_len)) == NULL)
    {
        free(built);
        return nVMUCardErrMemory;
    }
    memset(file + built_len, 0, file_len - built_len);
    rv = vmufs_write(dev, VMUCARD_NAME, file, file_len, VMUFS_OVERWRITE);
    free(file);

    return (rv < 0) ? nVMUCardErrWrite : nVMUCardOK;
}

int vmucard_write(int port, int unit, const void *payload, int len)
{
    int took = asset_io_card_begin(), rv;

    rv = card_write(port, unit, payload, len);
    asset_io_card_end(took);

    return rv;
}

#else /* FT_HOSTTEST */

/* The host has no maple bus and no VMU. Each card is a slot of RAM that
 * holds a payload and a claimed free-block count; the fake runs the same
 * judgement the console does (vmucard_payload_ok) and fails a write the
 * way vmufs does when the card is too full for it. What it does not
 * model is the VMS container, so a CRC failure is a flag the test sets
 * (vmucard_host_put's bad_crc). The first slot can instead be a plain
 * file, sy_sram_set_host_path's card. */
#define HOST_BLOCKS 200

typedef struct HostCard
{
    int present;
    int free_blocks;
    int has_file;
    int bad_crc;
    int fail_writes;
    int file_blocks;
    int stamp;
    uint8_t data[CARD_PAYLOAD];
} HostCard;

static HostCard sHostCard[VMUCARD_MAX];
static const char *sHostFile;
static int sHostClock;

static int host_slot(int port, int unit)
{
    if ((port < 0) || (port > 3) || (unit < 1) || (unit > 2))
    {
        return -1;
    }
    return (port * 2) + (unit - 1);
}

static HostCard *host_card(int port, int unit)
{
    int slot = host_slot(port, unit);

    if ((slot < 0) || ((slot == 0) && (sHostFile != NULL)))
    {
        return NULL;
    }
    return (sHostCard[slot].present != 0) ? &sHostCard[slot] : NULL;
}

void vmucard_host_reset(void)
{
    memset(sHostCard, 0, sizeof(sHostCard));
    sHostFile = NULL;
    sHostClock = 0;
}

void vmucard_host_set_file(const char *path)
{
    sHostFile = path;
}

void vmucard_host_insert(int port, int unit, int free_blocks)
{
    int slot = host_slot(port, unit);

    if (slot >= 0)
    {
        memset(&sHostCard[slot], 0, sizeof(sHostCard[slot]));
        sHostCard[slot].present = 1;
        sHostCard[slot].free_blocks = free_blocks;
    }
}

void vmucard_host_remove(int port, int unit)
{
    int slot = host_slot(port, unit);

    if (slot >= 0)
    {
        sHostCard[slot].present = 0;
    }
}

void vmucard_host_put(int port, int unit, const void *payload, int len,
                      int bad_crc)
{
    HostCard *c = host_card(port, unit);

    if ((c == NULL) || (len > (int)sizeof(c->data)))
    {
        return;
    }
    memset(c->data, 0, sizeof(c->data));
    memcpy(c->data, payload, len);
    c->has_file = 1;
    c->bad_crc = bad_crc;
    c->file_blocks = vmucard_file_blocks(CARD_PAYLOAD);
    c->stamp = ++sHostClock;
}

int vmucard_host_get(int port, int unit, void *payload, int len)
{
    HostCard *c = host_card(port, unit);

    if ((c == NULL) || (c->has_file == 0) || (len > (int)sizeof(c->data)))
    {
        return -1;
    }
    memcpy(payload, c->data, len);

    return 0;
}

void vmucard_host_fail_writes(int port, int unit, int fail)
{
    HostCard *c = host_card(port, unit);

    if (c != NULL)
    {
        c->fail_writes = fail;
    }
}

int vmucard_present(int port, int unit)
{
    return ((host_slot(port, unit) == 0) && (sHostFile != NULL)) ||
           (host_card(port, unit) != NULL);
}

const char *vmucard_where(int port, int unit)
{
    static char where[32];

    if ((host_slot(port, unit) == 0) && (sHostFile != NULL))
    {
        return sHostFile;
    }
    snprintf(where, sizeof(where), "host:%c%d", 'a' + port, unit);

    return where;
}

/* The file card: the bare payload, so the judgement is its length and
 * the game's copies. */
static int host_file_read(void *payload, int len, VMUCardInfo *info)
{
    static uint8_t buf[CARD_PAYLOAD];
    FILE *f;
    size_t got;

    info->free_blocks = HOST_BLOCKS;
    if ((f = fopen(sHostFile, "rb")) == NULL)
    {
        return info->state;
    }
    got = fread(buf, 1, sizeof(buf), f);
    fclose(f);
    info->file_blocks = vmucard_file_blocks(CARD_PAYLOAD);
    info->free_blocks -= info->file_blocks;
    if ((int)got < len)
    {
        info->state = nVMUCardCorrupt;
        info->why = "short";
    }
    else if (vmucard_payload_ok(buf, len) == 0)
    {
        info->state = nVMUCardCorrupt;
        info->why = "copies";
    }
    else
    {
        info->state = nVMUCardValid;
        memcpy(payload, buf, len);
    }
    return info->state;
}

int vmucard_read(int port, int unit, void *payload, int len,
                 VMUCardInfo *info)
{
    HostCard *c;

    info_clear(info, port, unit);

    if ((host_slot(port, unit) == 0) && (sHostFile != NULL))
    {
        return host_file_read(payload, len, info);
    }
    if ((c = host_card(port, unit)) == NULL)
    {
        return -1;
    }
    info->free_blocks = c->free_blocks;
    if (c->has_file == 0)
    {
        return info->state;
    }
    info->file_blocks = c->file_blocks;
    info->year = 1999;
    info->month = 9;
    info->day = 9;
    info->hour = c->stamp / 60;
    info->min = c->stamp % 60;
    if (c->bad_crc != 0)
    {
        info->state = nVMUCardCorrupt;
        info->why = "crc";
    }
    else if (vmucard_payload_ok(c->data, len) == 0)
    {
        info->state = nVMUCardCorrupt;
        info->why = "copies";
    }
    else
    {
        info->state = nVMUCardValid;
        memcpy(payload, c->data, len);
    }
    return info->state;
}

int vmucard_scan(VMUCardInfo *out, int max)
{
    static uint8_t scratch[CARD_PAYLOAD];
    int slot, n = 0;

    for (slot = 0; (slot < VMUCARD_MAX) && (n < max); slot++)
    {
        int port = slot / 2, unit = (slot % 2) + 1;

        if (vmucard_present(port, unit) != 0)
        {
            vmucard_read(port, unit, scratch, CARD_PAYLOAD, &out[n++]);
        }
    }
    return n;
}

int vmucard_write(int port, int unit, const void *payload, int len)
{
    HostCard *c;
    int need;

    if ((host_slot(port, unit) == 0) && (sHostFile != NULL))
    {
        FILE *f;

        if ((f = fopen(sHostFile, "wb")) == NULL)
        {
            return nVMUCardErrWrite;
        }
        fwrite(payload, 1, len, f);
        fclose(f);

        return nVMUCardOK;
    }
    if ((c = host_card(port, unit)) == NULL)
    {
        return nVMUCardErrGone;
    }
    /* vmufs_write's overwrite frees the old file's blocks first */
    need = vmucard_file_blocks(len);
    if (need > c->free_blocks + c->file_blocks)
    {
        return nVMUCardErrFull;
    }
    if ((c->fail_writes != 0) || (len > (int)sizeof(c->data)))
    {
        return nVMUCardErrWrite;
    }
    c->free_blocks += c->file_blocks - need;
    c->file_blocks = need;
    c->has_file = 1;
    c->bad_crc = 0;
    c->stamp = ++sHostClock;
    memcpy(c->data, payload, len);

    return nVMUCardOK;
}

#endif /* FT_HOSTTEST */

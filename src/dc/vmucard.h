#ifndef SSB64DC_VMUCARD_H
#define SSB64DC_VMUCARD_H

/* vmucard.h -- the memory cards themselves.
 *
 * src/dc/vmusave.c is the game's save store: the byte array sys/dma.c's
 * two SRAM bodies read and write. This is what sits under it -- the VMUs
 * on the maple bus, the save file on each, and the file's pictures -- so
 * that the store never sees a KOS type and the host test can put any
 * number of cards in the machine.
 *
 * The N64 has nothing here: a cartridge's SRAM is always there, is
 * always the right size and never belongs to another game. Everything in
 * this file is the port's.
 *
 * It calls vmufs_* and vmu_pkg_*, never fs_* on /vmu, and it never takes
 * the disc's I/O lock (src/dc/assetroot.h): vmufs keeps its own mutex and
 * never touches fs.c's file table, so a card write does not stand in
 * front of a disc read (`./run.sh test io`
 * holds both halves). The one disc read it makes -- vmuart.bin, at
 * vmucard_init -- goes through asset_*, like every other.
 *
 * Every call here blocks for the maple traffic it makes: a read is a
 * dozen block transfers, a write that and the erases.
 */

#include <stdint.h>

/* A VMU directory entry holds twelve characters; this uses eleven. */
#define VMUCARD_NAME "SSB64DC.SAV"

/* Four ports, two expansion slots each. */
#define VMUCARD_MAX 8

/* What a card holds, as far as this game is concerned. */
enum
{
    nVMUCardNoSave,  /* a card with no SSB64DC.SAV on it */
    nVMUCardValid,   /* a file whose payload the game will accept */
    nVMUCardCorrupt  /* a file the game will not: see VMUCardInfo.why */
};

typedef struct VMUCardInfo
{
    int port;        /* 0-3, A-D */
    int unit;        /* 1-2, the controller's slot */
    int state;       /* nVMUCard* */
    const char *why; /* nVMUCardCorrupt: "crc", "app id", "short",
                      * "copies" or "read"; NULL otherwise */
    int free_blocks; /* the card's, or -1 when it could not be read */
    int file_blocks; /* the save file's, 0 when there is none */
    /* the file's directory time, BCD decoded; zero when there is none */
    int year, month, day, hour, min;
} VMUCardInfo;

/* Load the save's pictures (vmuart.bin) and build the fallback icon.
 * Once, at boot, before anything below; later calls do nothing. */
void vmucard_init(void);

/* Look at every card: fill out[0..n) in maple order and return n. The
 * payload is read and judged but not kept. */
int vmucard_scan(VMUCardInfo *out, int max);

/* Read the save on the card at port/unit into payload (len bytes, the
 * store's extent), and describe the card into *info. Returns info->state;
 * payload is written only when that is nVMUCardValid. No card there is
 * -1, with info->free_blocks -1. */
int vmucard_read(int port, int unit, void *payload, int len,
                 VMUCardInfo *info);

/* vmucard_write's answers. */
enum
{
    nVMUCardOK = 0,
    nVMUCardErrGone = -1,   /* no memory card at port/unit */
    nVMUCardErrFull = -2,   /* fewer free blocks than the file needs */
    nVMUCardErrMemory = -3, /* the file could not be built in RAM */
    nVMUCardErrWrite = -4,  /* the card failed a read or a write */
    nVMUCardErrBlank = -5   /* the card has no filesystem: never formatted */
};

/* Write payload as the save file on port/unit, replacing the one there.
 * nVMUCardOK or an nVMUCardErr*. It never logs: it runs on the save's
 * writer thread (src/dc/vmusave.c), and printf is not safe off the main
 * thread (src/dc/fgm.c's deferred log says why) -- the caller logs the
 * answer. It checks the room itself first, so KOS's vmufs_write does not
 * reach its own full-card path, which logs; a card failing mid-write can
 * still make KOS log from the thread, and nothing here can stop that. */
int vmucard_write(int port, int unit, const void *payload, int len);

/* What an nVMUCard* answer means, for the log. */
const char *vmucard_error_name(int rv);

/* Whether a memory card is at port/unit now. */
int vmucard_present(int port, int unit);

/* The save file's size in blocks, for a payload of len bytes: the header,
 * the icon frames and the eyecatch, which are fixed once vmucard_init has
 * run, and the payload. */
int vmucard_file_blocks(int len);

/* The game's own test of a payload: TRUE when either of lb/lbbackup.c's
 * two copies passes lbBackupCreateChecksum and the signature, which is
 * exactly when lbBackupIsSramValid will keep it. len is the store's
 * extent. */
int vmucard_payload_ok(const void *payload, int len);

/* Where the save is, for the log: "/vmu/a1/SSB64DC.SAV" on the console,
 * the backing file or "host:a1" on the host. A static buffer. */
const char *vmucard_where(int port, int unit);

#ifdef FT_HOSTTEST
/* The host's cards. Each is a slot of RAM; the first can instead be a
 * plain file (vmucard_host_set_file), which is sy_sram_set_host_path's
 * one-card machine and the only one that outlives the process. The file
 * holds the bare payload, with no VMS header, so it can have no CRC to
 * fail. */
void vmucard_host_reset(void);
void vmucard_host_set_file(const char *path);
void vmucard_host_insert(int port, int unit, int free_blocks);
void vmucard_host_remove(int port, int unit);
/* Put a save file on a card that is in: len bytes of payload, and whether
 * its VMS CRC is to fail. */
void vmucard_host_put(int port, int unit, const void *payload, int len,
                      int bad_crc);
/* The payload of the file on port/unit, or -1 when there is none. */
int vmucard_host_get(int port, int unit, void *payload, int len);
/* Make every write to port/unit fail (1) or land (0). */
void vmucard_host_fail_writes(int port, int unit, int fail);
#endif

#endif /* SSB64DC_VMUCARD_H */

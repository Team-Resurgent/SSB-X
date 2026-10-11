/* dma.h -- syDmaReadRom, the one N64 service that reads the cartridge.
 *
 * sys/dma.c:116 is four lines: take the DMA semaphore, hand the PI a
 * physical address, a RAM address and a length, wait for the message.
 * The game calls it with addresses its own linker wrote into the
 * executable -- `&lEFCommonParticleScriptBankLo` is 0x00AC7340 and
 * nothing else -- so a caller never names a file, only a range.
 *
 * There is no cartridge here, so the port keeps the interface and moves
 * the medium: the ranges the game asks for were cut out of the ROM at
 * build time (tools/export/ssb_particleexport.py) and lie on the romdisk or the
 * disc under names of their own, and this turns an address back into
 * <file, offset>. The addresses themselves come from
 * src/game/ssb64/particlebanks.ld, which the same tool generates from the
 * same table, so the two halves cannot drift.
 *
 * Only the eighteen particle-bank segments are here. The rest of the
 * game's ROM reads go through lb/lbreloc.c, which the port replaced
 * outright with its packs and banks rather than emulating a ROM under, and
 * through sys/main.c's RSP boot code, which has no counterpart at all.
 * An address in none of the ranges is a programming error, not a bad
 * disc. It is logged with the address, and the destination is zeroed --
 * a bank of zeroes is a bank of no scripts, which the game's own loops
 * handle, where a block of last scene's heap is a pointer table into
 * nothing.
 */
#ifndef SSB_DC_DMA_H
#define SSB_DC_DMA_H

#include <stddef.h>
#include <stdint.h>

/* sys/dma.h:37, the game's own signature. src/dc/dma.c includes that
 * header too, so a drift is a compile error. */
void syDmaReadRom(uintptr_t rom_src, void *ram_dst, size_t size);

/* Reads served and bytes moved since boot, for load accounting.
 * asset_io_* already counts the medium; this counts the game's side of
 * it, which is the number a load screen is argued about in. */
uint32_t sy_dma_reads(uint32_t *bytes);

#endif /* SSB_DC_DMA_H */

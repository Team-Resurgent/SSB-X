/* primdump.h -- -DDB_PRIM_DUMP=<tic>: the PVR's own input for one frame of a
 * 1P team card's crowd, on the serial line.
 *
 * What tools/bake/ (the in-build baker) draws from is the stream
 * the port hands the TA: polygon headers, vertices, the textures and the
 * palette those headers name. This records that stream from a real run so the
 * rasteriser can be checked against the frame the console drew from it, before
 * the same stream is produced on the host with no console at all.
 *
 * sc1pintro.c calls primdump_track(tic) from each crowd fighter's display
 * proc; on update <tic> every pvr_prim (linker --wrap) is copied, and on the
 * next update the lot is written out:
 *   primdump: begin <tic> <units>            units of 32 bytes
 *   pd <unit> <4 units, hex>                 headers and vertices in order
 *   pdt <vram offset> <len> <hex>            a texture, 64 bytes a line
 *   pdp <first entry> <8 entries, hex>       the palette RAM, then
 *   primdump: pal cfg <n>                    its entry format
 *   primdump: end <tic>
 * tools/check/primdump.py reads it back. Needs
 * EXTRA_LDFLAGS=-Wl,--wrap=pvr_prim.
 */
#ifndef DC_PRIMDUMP_H
#define DC_PRIMDUMP_H

#ifdef FT_BAKER
#include <stddef.h>
#include <stdio.h>
/* The host baker's: pvr_prim (hoststubs/bakepvr.c) hands every submission to
 * primdump_record; between start and finish they are kept, and finish writes
 * them, the textures they name and the palette RAM to gPrimDumpOut in the
 * lines above. */
extern FILE *gPrimDumpOut;
void primdump_record(const void *data, size_t size);
void primdump_start(void);
void primdump_finish(int tic);
#define primdump_track(tic) ((void)0)
#elif defined(DB_PRIM_DUMP)
void primdump_track(int tic);
#else
#define primdump_track(tic) ((void)0)
#endif

#endif

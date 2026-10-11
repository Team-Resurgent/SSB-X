/* lbpartex.h -- the particle banks' textures, in the PVR's own memory.
 *
 * A particle bank is two files. The .scb and the .txb are the ROM's own
 * bytes and belong to lb/lbparticle.c: the interpreter reads a texture's
 * count, fmt, siz, width, height and flags out of the .txb and never
 * touches a texel. The renderer does, and the texels there are the N64's
 * -- CI4 against a TLUT, I4, IA8, RGBA5551, RGBA8888 -- of which the PVR
 * reads none.
 *
 * So the port ships a third file per bank, a .txp, holding the same
 * images twiddled in ARGB1555 or ARGB4444
 * (tools/export/ssb_particletexexport.py, checked against the decomp's own
 * extractParticleTextures.py by tools/check/particletex_check.py). This is what
 * loads one, and it is indexed exactly the way lbParticleDrawTextures indexes
 * the .txb: [bank_id][texture_id][frame_id], the same three numbers.
 *
 * DIVERGES, wholly: there is no such file and no such loader in the
 * game. The N64 pointed the RDP at the .txb's own bytes and let the
 * texture unit read them where they lay, once per rectangle. The PVR
 * samples out of its own 8 MB and nothing else, so a bank's images have
 * to be converted and uploaded before anything can draw one, and that is
 * a load-time cost the N64 did not pay: efcommon is 492 KB of VRAM.
 *
 * The file is streamed rather than slurped -- one frame at a time
 * through a staging buffer the size of the largest -- because the whole
 * pack is two thirds of the scene heap and none of it needs to be in
 * main RAM once the PVR has it.
 */
#ifndef SSB_DC_LBPARTEX_H
#define SSB_DC_LBPARTEX_H

#include <stdint.h>

#include <dc/pvr.h>

/* One frame of one texture, ready to bind. */
typedef struct LBPTex
{
    pvr_ptr_t txr;
    const void *img;            /* the .txb frame this was converted from,
                                 * LBTexture.data[frame_id] in the bank the
                                 * game pointerized -- see lbpTexForImage */
    uint16_t  w, h;             /* the PVR texture's, a power of two >= 8 */
    uint16_t  src_w, src_h;     /* the .txb's own, which the RDP's dsdx and
                                 * dtdy step across (lb/lbparticle.c:1823) */
    /* PVR_TXRFMT_ARGB1555 or _ARGB4444, which KOS spells in bits 27-28:
     * a u16 here once kept the low half of ARGB4444, which is zero and
     * is ARGB1555, and 43 of the 65 particle textures drew as 1555 --
     * white graded-alpha smoke came out cyan with a 1-bit edge.
     * lbpartex.c holds the width to the constant at compile time. */
    uint32_t  fmt;
} LBPTex;

/* Load bank `bank_id`'s textures from "<name>.txp". Loading the same name
 * over the same id again is free; loading a different one frees the first.
 * 0 on success, -1 if the file is missing or malformed or VRAM is full. */
int lbpTexLoadBank(int bank_id, const char *name);

/* Every bank given back to the PVR. Called where the scene's other VRAM
 * is (src/dc/scvsbattle.c). */
void lbpTexFreeAll(void);

/* One frame, or NULL if this bank/texture/frame is not loaded. */
const LBPTex *lbpTexGet(int bank_id, int texture_id, int frame_id);

/* The frame whose .txb image is at `image`, over every loaded bank, or
 * NULL. This is the lookup the renderer needs and the one the game does
 * not: lb/lbparticle.c hands the RDP the bank's own bytes and the
 * address is the texture, so by the time src/dc/lbpdraw.c is asked to
 * draw a rectangle the address is all that is left of the three numbers
 * (lbparticle.c:1806-1815) that chose it. lbpTexLoadBank walks the
 * pointerized bank once and writes them down, which also says -- on the
 * target, where the walk is real -- that the pack and the ROM's bank
 * agree on how many textures there are and how many frames each has. */
const LBPTex *lbpTexForImage(const void *image);

/* How much VRAM the loaded banks hold, and how many frames it is. */
uint32_t lbpTexBytes(int *frames_out);

#endif /* SSB_DC_LBPARTEX_H */

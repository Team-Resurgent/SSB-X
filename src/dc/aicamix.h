/* Mix headroom, shared by both audio engines.
 *
 * libultra gives each voice a gain relative to full scale: a single voice
 * at maximum fills the output on its own, and the RSP's envmixer summed
 * the voices into a saturating 16-bit buffer. The AICA sums its channels
 * the same way, so the arithmetic is the original's -- what the port
 * lacks is anywhere for the sum to go.
 *
 * DIVERGES: measured with the game's own sequences through the ported
 * player at its own pools (24 voices, 64 events -- sys/audio.c:91), the
 * live voices' volumes sum past full scale on 46 of the ROM's 47 tracks;
 * Kongo Jungle reaches 6.6x with ten voices live, and only Hammer, which
 * never plays more than one, stays under. That is the worst case, since
 * voices rarely peak in phase -- mixing the actual waveforms offline puts
 * Kongo Jungle's true peak at 1.16x and the loudest track's at 3.26x --
 * but both are over unity and the AICA hard-clips there, which is what
 * distortion on the dense passages of a busy track sounds like.
 *
 * So every voice is attenuated by a fixed amount before it reaches a
 * channel. It is applied per channel rather than at the AICA's own master
 * volume (MVOL, common register 0x2800, which the ARM driver sets to full
 * in aica_init) deliberately: MVOL scales the mix *after* the summing
 * stage, so if that stage is where the saturation happens, a lower master
 * volume buys quieter distortion and nothing else. Attenuating before the
 * sum is correct wherever the clipping is.
 *
 * Both engines share the one constant so the balance the game strikes
 * between its music and its sound effects is preserved exactly: the whole
 * output moves down together.
 */
#ifndef DC_AICAMIX_H
#define DC_AICAMIX_H

/* Halvings of every voice's gain: 2 is 12 dB, which clears the 3.26x
 * measured peak with margin. A power of two so it is exact in the log
 * domain KOS's calc_aica_vol maps into -- the dB step between any two
 * volumes is unchanged, there are simply fewer of them. */
#define AICA_MIX_HEADROOM_SHIFT 2

#endif /* DC_AICAMIX_H */

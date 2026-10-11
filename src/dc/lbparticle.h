/* lbparticle.h -- the port's half of lb/lbparticle.c.
 *
 * The game's particle library is 2,954 lines and 41 functions in three
 * layers: the free lists a scene allocates once, the bytecode
 * interpreter that runs a particle a frame at a time, and
 * lbParticleDrawTextures, which is 669 lines of display list. Forty of
 * the 41 are here, in src/dc/lbparticle.c, copied line for line; the
 * renderer is the one that is not, and it is the next step.
 *
 * So a particle made now is made the way the game makes it, runs the
 * bytecode the game runs, ages, spawns its children, drags its
 * generator and dies on the frame the game kills it -- and is not
 * drawn. tools/check/lbparticle_check.py is what says the copy is the
 * decomp's file: it diffs the text line by line and then runs both
 * builds over all 119 scripts of the efcommon bank and compares the
 * traces. src/dc/db.c's -DDB_PARTICLE_SCRIPT runs one of them on the
 * Dreamcast for the same comparison across machines.
 *
 * A copy rather than a direct dependency because of that one function.
 * The decomp's file does compile whole -- one more warning flag for two
 * pointer types IDO allowed -- but lbParticleDrawTextures cannot then be
 * taken back out of the object: it is the game's name for the renderer,
 * ef/efdisplay.c and mn/mncommon/mntitle.c call it by that name, and the
 * port has to be what answers. Removing it after the fact fails on its
 * own switch: -ffunction-sections puts the body in a section of its own,
 * but the jump table stays in .rodata, which -fno-data-sections keeps
 * whole for the overlay reload's sake, and objcopy will not drop a
 * section .rodata still points into. Leaving it in costs the ELF the
 * body plus every symbol it reaches -- the 4x4 library, libultra's gu,
 * a display-list head that must exist and must never be used -- to hold
 * code that is dead by construction. The copy costs 2,200 lines that a
 * test compares against the original on every run.
 */
#ifndef SSB_DC_LBPARTICLE_H
#define SSB_DC_LBPARTICLE_H

#include <ssb_types.h>
#include <lb/lbtypes.h>

/* The pointerized banks, for the log. lbParticleSetupBankID walks a
 * freshly loaded bank turning every file-relative offset in it into a
 * pointer into itself, in place, and the game keeps the result in four
 * file-scope arrays. The port keeps them where the game keeps them and
 * adds these four observers, because the *target* is the only place
 * that walk can be checked: the bank's arrays are the ROM's 32-bit
 * pointers and the host the cross-tests run on is 64-bit, so the walk
 * cannot run there at all (src/dc/scvsbattle.c scVSBattleLoadEffectBank).
 * So scVSBattleStartBattle prints the two counts it arrived at, and a
 * serial log says whether they are the 119 and 47 the decomp's own
 * efcommon_scb.c and efcommon_txb.c declare. */
s32 lb_particle_scripts(s32 bank_id);
s32 lb_particle_textures(s32 bank_id);

#endif /* SSB_DC_LBPARTICLE_H */

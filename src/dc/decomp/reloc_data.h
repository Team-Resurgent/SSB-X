/* Shadows ssb-decomp-re/include/reloc_data.h, whose whole body is a
 * REGION_US/REGION_JP switch onto reloc_data.<version>.h -- the generated
 * declaration list for every ROM file and every block inside one. The port
 * builds the US ROM, so it needs one arm, and it generates that arm the
 * same way the decomp does: tools/export/gen_reloc_header.sh runs the decomp's
 * own tools/genRelocSymbols.py into reloc_data.us.h beside this file. The
 * script carries the argument for why; src/game/ssb64/Makefile has the
 * rule; .gitignore keeps the output out of the tree.
 *
 * Until 2026-09-05 this file declared 82 symbols by hand -- the stage map
 * ids and map headers that mp/mpcollision.c's dMPCollisionGroundFileInfos
 * (mpcollision.c:45-88) initialises itself with -- and nothing else. That
 * single omission was the largest thing standing between the decomp's
 * source and this build: of the 209 decomp sources that would not compile
 * against the port, 151 failed on an undeclared ll* symbol and nothing
 * else. The 82 are a subset of what the generator emits, so they are no
 * longer restated; src/dc/mpshim.c still defines them, now as the `int`
 * the generator declares.
 *
 * These are absolute linker symbols: the ADDRESS of llGRCastleMapFileID is
 * the file id, which is why the game writes &llGRCastleMapFileID. The
 * port declares them; it does not yet give them their values (the
 * generator's companion linker script does that, and nothing the port
 * compiles reads one). mpshim.c's zeroed definitions stand in for the 82,
 * and the port never indexes them -- stage geometry comes from the packs.
 */
#ifndef _RELOC_DATA_H_
#define _RELOC_DATA_H_

#include <stdint.h>

/* The stage file table's own list, kept because src/dc/mpshim.c defines
 * these 82 and needs to name them. The declarations come from the
 * generated header below, not from here. */
#define MPSHIM_GROUND_FILES(X) \
    X(Castle) X(Sector) X(Jungle) X(Zebes) X(Hyrule) X(Yoster) X(Pupupu) \
    X(Yamabuki) X(Inishie) X(PupupuSmall) X(PupupuTest) X(Explain) \
    X(YosterSmall) X(Metal) X(Zako) X(Bonus3) X(Last) \
    X(Bonus1Mario) X(Bonus1Fox) X(Bonus1Donkey) X(Bonus1Samus) \
    X(Bonus1Luigi) X(Bonus1Link) X(Bonus1Yoshi) X(Bonus1Captain) \
    X(Bonus1Kirby) X(Bonus1Pikachu) X(Bonus1Purin) X(Bonus1Ness) \
    X(Bonus2Mario) X(Bonus2Fox) X(Bonus2Donkey) X(Bonus2Samus) \
    X(Bonus2Luigi) X(Bonus2Link) X(Bonus2Yoshi) X(Bonus2Captain) \
    X(Bonus2Kirby) X(Bonus2Pikachu) X(Bonus2Purin) X(Bonus2Ness)

#include "reloc_data.us.h"

#endif /* _RELOC_DATA_H_ */

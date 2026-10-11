/* hosttest_gmcolscripts.c -- gm/gmcolscripts.c for the host cross-test.
 *
 * The colour-animation scripts are u32 arrays with the address of another
 * script written into a word wherever a command jumps (gmColCommandGoto,
 * Subroutine and Parallel: gm/gmdef.h casts the array to uintptr_t).
 * That is a 32-bit word holding a 32-bit address on the N64 and on the
 * Dreamcast, where the target build compiles the decomp's file
 * unmodified (src/game/ssb64/Makefile gmcolscripts.o). This host is
 * 64-bit, and gcc will not fold a 64-bit address into a 32-bit word at
 * load time.
 *
 * So the host takes the same file textually with the three address words
 * zeroed. The layout is untouched -- a jump is still two words -- and
 * every command that carries no address is the decomp's bit for bit,
 * which is every command the five ScreenFlash scripts use. A script that
 * jumps cannot be run here, the stance hosttest_ft.c takes for the
 * fighters' motion scripts too (its MOCK_SCRIPT comment); the target runs
 * those.
 */
#include <gm/gmdef.h>

#undef gmColCommandGotoS2
#undef gmColCommandSubroutineS2
#undef gmColCommandParallelS2
#define gmColCommandGotoS2(addr) 0
#define gmColCommandSubroutineS2(addr) 0
#define gmColCommandParallelS2(addr) 0

#include <gm/gmcolscripts.c>

/* Shadows ssb-decomp-re/src/sys/utils.h to move two names out of the way,
 * and passes the rest of it through untouched.
 *
 * utils.h:14-15 declare `extern f32 __sinf(f32);` and the same for __cosf.
 * glibc's math.h declares `float __sinf(float)`. On the target, and in the
 * float half of tools/check/figatree_check.py, those agree. In its double half
 * f32 is double (see PR/ultratypes.h) and they collide, so every object
 * system translation unit stops on a conflicting declaration -- because
 * sys/objtypes.h:13 includes this header, so every one of them reaches it.
 *
 * Both users in the decomp are sys/utils.c and sys/matrix.c, and both are
 * in the ordinary build. The pair is
 * libultra's own -- ssb-decomp-re/src/libultra/gu's sinf.c and cosf.c,
 * written for the port by tools/export/ssb_trigexport.py -- and not
 * libm's at all. Neither user is
 * in the *double* build, which is tools/check/figatree_check.py's alone, so the
 * rename below still stands: a call that appears there fails to link,
 * loudly, instead of silently reaching libm at the wrong width. Pulling
 * math.h in first is what keeps the rename off glibc's own declarations,
 * which are float either way.
 */
#ifndef SSB_DC_SYS_UTILS_H
#define SSB_DC_SYS_UTILS_H

#ifdef SSB_F32_IS_F64
#include <math.h>
#define __sinf ssb_dc_decomp_sinf
#define __cosf ssb_dc_decomp_cosf
#endif

#include_next <sys/utils.h>

#endif /* SSB_DC_SYS_UTILS_H */

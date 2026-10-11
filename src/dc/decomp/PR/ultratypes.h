/* Shadows ssb-decomp-re/include/PR/ultratypes.h (lines 33-63). The
 * original spells u32/s32 as `unsigned long`/`long`, which is 64 bits on
 * the x86-64 host the cross-tests run on and would change every s32
 * expression in the collision code; stdint keeps them 32-bit on both
 * the SH-4 and the host. Its `size_t` typedef is dropped for stddef's. */
#ifndef _ULTRATYPES_H_
#define _ULTRATYPES_H_

#include <stddef.h>
#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

/* tools/check/figatree_check.py builds the animation engine twice: once as the
 * target runs it, and once in double, where any difference from the Python
 * reference is a porting mistake rather than rounding. The decomp writes
 * every scalar as f32, so the knob has to sit here. Nothing else defines
 * it, and no build that links against the target's structs may. */
#ifdef SSB_F32_IS_F64
typedef double f32;
#else
typedef float f32;
#endif
typedef double f64;

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif
#ifndef NULL
#define NULL ((void *)0)
#endif

#endif /* _ULTRATYPES_H_ */

/* lbptrace.h -- one live particle, written down the same way twice.
 *
 * The particle interpreter is checked in two places that cannot see each
 * other: tools/check/lbparticle_check.py runs the port's src/dc/lbparticle.c
 * and the decomp's lb/lbparticle.c side by side on this machine, and
 * src/dc/db.c's -DDB_PARTICLE_SCRIPT runs the same script on the
 * Dreamcast. The host pair is compared record for record; the target can
 * only send back a number down a serial line. This header is the record
 * and the number, so both ends mean the same thing by them.
 *
 * Every field an LBParticle carries that the interpreter writes is here,
 * and none that it does not: the four pointers in one are addresses, and
 * two processes -- let alone two machines -- have no reason to agree
 * about those. The live list is described by its length and its order
 * instead, which is what `index` and `structs_used` are for. Both
 * machines are little-endian and both floats are IEEE 754 binary32, so
 * the bytes of a record mean the same on each; the digest is over those
 * bytes, so a float that came out one ulp different says so.
 *
 * The struct is laid out to have no padding: four s32, ten u16, eight
 * u8, ten f32, sixteen u8 -- 100 bytes, every field naturally aligned.
 * tools/check/lbparticle_check.py's REC unpacks exactly that.
 *
 * Every float goes in through lbpTraceF32, which folds three things the
 * machines can disagree about and nothing else:
 *
 *   - a negative zero becomes a positive one. This is the next rule seen
 *     from the other side: when the SH-4 flushes a subnormal it keeps its
 *     sign, so a product that underflows lands on -0.0 there and on a
 *     subnormal here. The two machines computed the same number -- zero
 *     -- and writing it down two ways would be the record's fault, not
 *     theirs.
 *   - a subnormal becomes zero. x86 computes with those; the SH-4 does
 *     not, because KOS runs with FPSCR.DN set, and neither did the N64,
 *     whose VR4300 traps every subnormal operand into libultra's handler
 *     and gets zero back. The host is the odd one out here and the target
 *     is the faithful one, and comparing them means saying so.
 *   - a NaN becomes one NaN. A NaN's payload bits are the FPU's business,
 *     not the program's.
 *
 * Infinities are left alone: both machines make the same ones.
 *
 * None of the three fires on the efcommon bank: lbParticleReadFloatBigEnd
 * reads the bytecode's floats big-endian (see the divergence note in
 * src/dc/lbparticle.c), so the bank's 9,519-record replay has no subnormal,
 * no signed zero and no NaN in it at all, and the whole-run digest is the
 * same -- 0x1be1ed2c -- whether these rules are applied or not. They stay
 * because the machines really do differ this way and another bank, or a
 * fighter's own effects, can still reach it; they do not paper over the reader.
 */
#ifndef SSB_DC_LBPTRACE_H
#define SSB_DC_LBPTRACE_H

#include <string.h>

#include <ssb_types.h>
#include <lb/lbtypes.h>

typedef struct LBPTraceRec
{
    s32 script_id;
    s32 frame;
    s32 index;              /* position in the live list */
    s32 structs_used;
    u16 generator_id, flags;
    u16 bytecode_csr, return_ptr, loop_ptr, lifetime, bytecode_timer;
    u16 size_target_length, primcolor_target_length, envcolor_target_length;
    u8 bank_id, loop_count, texture_id, frame_id;
    u8 has_gn, has_xf, pad[2];
    f32 pos[3], vel[3];
    f32 gravity, friction, size, size_target;
    u8 primcolor[4], target_primcolor[4], envcolor[4], target_envcolor[4];
} LBPTraceRec;

static inline f32 lbpTraceF32(f32 v)
{
    u32 b;

    memcpy(&b, &v, sizeof(b));

    if ((b & 0x7FFFFFFFU) == 0)
    {
        b = 0;                          /* -0.0 */
    }
    else if ((b & 0x7F800000U) == 0)
    {
        b = 0;                          /* subnormal */
    }
    else if ((b & 0x7F800000U) == 0x7F800000U && (b & 0x007FFFFFU) != 0)
    {
        b = 0x7FC00000U;                /* NaN */
    }
    memcpy(&v, &b, sizeof(v));

    return v;
}

static inline void lbpTraceRec(LBPTraceRec *r, const LBParticle *pc,
                               s32 script_id, s32 frame, s32 index,
                               s32 structs_used)
{
    memset(r, 0, sizeof(*r));

    r->script_id = script_id;
    r->frame = frame;
    r->index = index;
    r->structs_used = structs_used;
    r->generator_id = pc->generator_id;
    r->flags = pc->flags;
    r->bytecode_csr = pc->bytecode_csr;
    r->return_ptr = pc->return_ptr;
    r->loop_ptr = pc->loop_ptr;
    r->lifetime = pc->lifetime;
    r->bytecode_timer = pc->bytecode_timer;
    r->size_target_length = pc->size_target_length;
    r->primcolor_target_length = pc->primcolor_target_length;
    r->envcolor_target_length = pc->envcolor_target_length;
    r->bank_id = pc->bank_id;
    r->loop_count = pc->loop_count;
    r->texture_id = pc->texture_id;
    r->frame_id = pc->frame_id;
    r->has_gn = (pc->gn != NULL);
    r->has_xf = (pc->xf != NULL);
    r->pos[0] = lbpTraceF32(pc->pos.x);
    r->pos[1] = lbpTraceF32(pc->pos.y);
    r->pos[2] = lbpTraceF32(pc->pos.z);
    r->vel[0] = lbpTraceF32(pc->vel.x);
    r->vel[1] = lbpTraceF32(pc->vel.y);
    r->vel[2] = lbpTraceF32(pc->vel.z);
    r->gravity = lbpTraceF32(pc->gravity);
    r->friction = lbpTraceF32(pc->friction);
    r->size = lbpTraceF32(pc->size);
    r->size_target = lbpTraceF32(pc->size_target);
    memcpy(r->primcolor, &pc->primcolor, 4);
    memcpy(r->target_primcolor, &pc->target_primcolor, 4);
    memcpy(r->envcolor, &pc->envcolor, 4);
    memcpy(r->target_envcolor, &pc->target_envcolor, 4);
}

/* The record's own words, added up. Wrapping is the point: 32 bits of
 * sum is enough for a serial line to carry and enough to fail on one
 * changed byte, and it needs no order beyond the struct's own. */
static inline u32 lbpTraceDigest(u32 acc, const LBPTraceRec *r)
{
    u32 words[sizeof(LBPTraceRec) / sizeof(u32)];
    size_t i;

    memcpy(words, r, sizeof(words));

    for (i = 0; i < sizeof(words) / sizeof(words[0]); i++)
    {
        acc += words[i];
    }
    return acc;
}

#endif /* SSB_DC_LBPTRACE_H */

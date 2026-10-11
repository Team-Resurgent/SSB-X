/* Host driver for the *decomp's* Figatree interpreter, used by
 * tools/check/figatree_check.py to diff it against ssb_assets.Figatree.
 *
 * What runs underneath is the game's own code, unmodified: the object
 * system out of ssb-decomp-re/src/sys (objman, objanim, objhelper,
 * objscript, interp, malloc) with the port's src/dc/taskman.c under it for
 * the scene heap, and the two joint-script parsers a fighter runs --
 * ft/ftanim.c's ftAnimParseDObjFigatree for a figatree, sys/objanim.c's
 * gcParseDObjAnimJoint for an AnimJoint animation (ft/ftparam.c:412 picks
 * by FTAnimDesc.is_anim_joint) -- driving real DObjs and AObjs. The
 * animation is attached the way the game attaches one (gcAddDObjAnimJoint,
 * with .event16 in place of .event32 for a figatree), and stepped the way
 * ft/ftanim.c:397 func_ovl2_800ECCA4 steps one.
 *
 * Reads one animation on stdin -- {u32 words, u32 joints, u32 frames,
 * u32 kind, u32 relocs, u32 floats}, then the words (u16 for kind 0, u32
 * for kind 1), a word offset per joint, the reloc list and the float list
 * -- steps every joint in lockstep, and writes {u32 live mask, u32 error,
 * f32 track[10]} per joint per frame on stdout. Native byte order both
 * ways; this never runs on the Dreamcast.
 *
 * An AnimJoint script is expanded into an AObjEvent32 array rather than
 * read in place, because on this host the union holds a pointer and is
 * wider than the 32-bit stream word: each word goes into .u, the words the
 * reloc list names -- Jump, SetAnim and SetInterp targets, stored as word
 * indices -- become addresses in .p exactly as the game's reloc walk makes
 * them, and the words the float list names are widened into .f, which
 * matters for the double build (f32 there is a double, and the parser
 * reads values as ->f). On the Dreamcast the union is four bytes and the
 * pack is read in place; see fighter_init.
 *
 * Three things here are the oracle's, not the game's. All three are about
 * surviving animation data that the parser walks into and has no answer
 * for.
 *
 * 1. GOBJ_FLAG_NOANIM is set on the GObj, so gcPlayDObjAnimJoint advances
 *    every live AObj's length and retires the animation exactly as it
 *    always does, but does not write the values into the DObj -- and the
 *    values are read back with the decomp's own gcGetAObjValue instead.
 *    The reason is the TraI track: gcPlayDObjAnimJoint routes it through
 *    syInterpCubic(&dobj->translate.vec.f, aobj->interpolate, value), and
 *    aobj->interpolate is NULL until a SetTranslateInterp command installs
 *    a spline. Three of the ROM's 1775 animations run that command; three
 *    dozen drive a TraI track without it, and in the game get away with it
 *    because the AObj -- and its spline -- outlives the animation that made
 *    it. Started cold, one joint at a time, they would dereference NULL.
 *    gcGetAObjValue evaluates the identical expression gcPlayDObjAnimJoint
 *    inlines (objanim.c:636-656 against objanim.c:733-751, the same
 *    products in a different order, which in IEEE is the same bits), so
 *    nothing about the arithmetic under test changes.
 *
 * 2. The word array is padded with zeros on both sides and the script
 *    pointer is checked against the real range after every parse. A word of
 *    zero decodes as End, so a script that runs off its file -- or a Loop
 *    that jumps out of it -- stops on the first padding word instead of
 *    reading the heap, and the frame is reported as an error. The game has
 *    no such guard; ssb_assets.Figatree does, and this is what keeps the
 *    two comparable on the four animations where it fires.
 *
 * 3. Each parse runs under an alarm, and a parse that does not come back is
 *    abandoned with the joint marked runaway. Both parsers' switches end in
 *    `default: break;` -- which does not advance the script pointer, so an
 *    opcode neither has a case for puts the `do { ... } while (anim_wait
 *    <= 0.0F)` into a loop that never ends. Every slot in the ROM that
 *    used to trip this was an AnimJoint script being read as a figatree;
 *    with the kind carried in, none
 *    does, and the alarm is what keeps a future misreading a reported
 *    failure rather than a hung check. The reference stops at the same
 *    word for the same reason -- see the opcode guards in
 *    ssb_assets.Figatree and ssb_assets.AnimJoint.
 *
 *    A whole animation's 300 frames run in single-digit milliseconds, and a
 *    joint that trips this is marked and skipped for the rest of the run,
 *    so the timer fires at most once per bad joint and never on a good one.
 */
#include <setjmp.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/obj.h>
#include <sys/objanim.h>
#include <sys/objdef.h>
#include <ft/ftanim.h>

#include "taskman.h"
/* src/dc/fighter.c is the PVR renderer and is not in this build, but
 * src/dc/objdisplay.c is (it is where the camera walk lives) and its
 * viewport publication writes this. Defined here as the host cross-test
 * defines it (src/game/ssb64/hosttest_ft.c), with the same
 * 320x240 the game's default camera sets. It is four plain floats
 * whatever f32 is, so it is the same struct in both builds. */
typedef struct DCViewport { float cx, cy, hw, hh; } DCViewport;
DCViewport gDCViewport = { 320.0f, 240.0f, 320.0f, 240.0f };

/* src/dc/objdisplay.c's roll look-at (gcMtxModLookAt, camera matrix
 * kinds 8-11/14-17) rotates with sys/vector.c's syVectorRotateAbout3D.
 * No camera is ever drawn here, and vector.c drags in sys/utils.c,
 * which the double build cannot compile (its sqrtf prototype is f32's),
 * so this is the one symbol, and reaching it is a bug in the oracle. */
Vec3f *syVectorRotateAbout3D(Vec3f *dst, Vec3f *dir, f32 angle)
{
    (void)dir;
    (void)angle;
    fprintf(stderr, "objanim_oracle: syVectorRotateAbout3D reached\n");
    abort();
    return dst;
}

#define MAX_JOINTS 64
#define TRACK_COUNT (nGCAnimTrackJointEnd - nGCAnimTrackJointStart + 1)

/* Room either side for the widest jump a Loop can encode: its delta is an
 * s16 of bytes, so at most 0x8000/2 words. */
#define PAD_WORDS 32768

enum { ERR_NONE = 0, ERR_RUNAWAY = 2, ERR_OVERRUN = 3 };

/* See (3) in the file comment. */
#define PARSE_SECONDS 2

static sigjmp_buf sParseJmp;
static volatile sig_atomic_t sInParse;

static void oracle_alarm(int sig)
{
    (void)sig;

    if (sInParse)
    {
        siglongjmp(sParseJmp, 1);
    }
    alarm(PARSE_SECONDS);       /* nothing to interrupt; keep watching */
}

static AObjEvent16 *sWords;     /* the real script words (kind 0) */
static AObjEvent32 *sWords32;   /* the expanded script words (kind 1) */
static u32 sNumWords;
static u32 sKind;
static DObj *sJoints[MAX_JOINTS];
static u8 sFailed[MAX_JOINTS];
static u8 sDriven[MAX_JOINTS];
static u32 sNumJoints;

static void *xread(size_t n)
{
    void *p = malloc(n ? n : 1);

    if (p == NULL || (n != 0 && fread(p, 1, n, stdin) != n))
    {
        fprintf(stderr, "objanim_oracle: short read\n");
        exit(1);
    }
    return p;
}

/* gcAddDObjAnimJoint (objanim.c:137-149) with .event16 in place of
 * .event32 -- the figatree half of the same three lines. */
static void oracle_add_figatree(DObj *dobj, AObjEvent16 *anim_joint,
                                f32 anim_frame)
{
    AObj *aobj = dobj->aobj;

    while (aobj != NULL)
    {
        aobj->kind = nGCAnimKindNone;
        aobj = aobj->next;
    }
    dobj->anim_joint.event16 = anim_joint;
    dobj->anim_wait = AOBJ_ANIM_CHANGED;
    dobj->anim_frame = anim_frame;
}

/* The scene's start function: one GObj carrying one DObj per joint. */
static void oracle_scene_start(void)
{
    GObj *gobj = gcMakeGObjSPAfter(0, NULL, 1, GOBJ_PRIORITY_DEFAULT);
    u32 j;

    /* See (1) in the file comment: the lengths still advance, the values
     * are read from the AObjs instead of the DObj. */
    gobj->flags |= GOBJ_FLAG_NOANIM;

    for (j = 0; j < sNumJoints; j++)
    {
        sJoints[j] = gcAddDObjForGObj(gobj, NULL);
    }
}

static void oracle_scene_update(void) {}
static void oracle_scene_draw(void) {}

static SYTaskmanSetup sOracleSetup =
{
    {
        0, oracle_scene_update, oracle_scene_draw,
        NULL, 0,                        /* keep the heap made below */
        1, 2, 0, 0, 0, 0, 0, 2, 0,      /* N64 display-list plumbing */
        NULL, NULL                      /* lights, controller */
    },

    0, 0, 0, 0,                         /* GObjThreads: none */
    4,                                  /* GObjProcesses */
    4, sizeof(GObj),
    1,                                  /* XObjs: nothing is drawn */
    NULL, NULL,
    MAX_JOINTS * TRACK_COUNT,           /* AObjs: every track of every joint */
    1,                                  /* MObjs */
    MAX_JOINTS, sizeof(DObj),
    1, sizeof(SObj),
    1, sizeof(CObj),

    oracle_scene_start
};

int main(void)
{
    u32 hdr[6];
    s32 *entries;
    u32 f, j;

    if (fread(hdr, sizeof(hdr), 1, stdin) != 1)
    {
        return fprintf(stderr, "objanim_oracle: no header\n"), 1;
    }
    if (hdr[1] > MAX_JOINTS)
    {
        return fprintf(stderr, "objanim_oracle: too many joints\n"), 1;
    }
    sNumWords = hdr[0];
    sNumJoints = hdr[1];
    sKind = hdr[3];

    if (sKind == 0)
    {
        u16 *raw = xread((size_t)sNumWords * sizeof(u16));
        AObjEvent16 *padded;

        entries = xread((size_t)sNumJoints * sizeof(s32));
        free(xread((size_t)hdr[4] * sizeof(u32)));
        free(xread((size_t)hdr[5] * sizeof(u32)));

        /* See (2) in the file comment. */
        padded = calloc((size_t)sNumWords + 2 * PAD_WORDS,
                        sizeof(AObjEvent16));
        if (padded == NULL)
        {
            return fprintf(stderr, "objanim_oracle: out of memory\n"), 1;
        }
        sWords = padded + PAD_WORDS;
        memcpy(sWords, raw, (size_t)sNumWords * sizeof(u16));
    }
    else
    {
        u32 *raw = xread((size_t)sNumWords * sizeof(u32));
        u32 *relocs, *floats;
        AObjEvent32 *padded;
        u32 k;

        entries = xread((size_t)sNumJoints * sizeof(s32));
        relocs = xread((size_t)hdr[4] * sizeof(u32));
        floats = xread((size_t)hdr[5] * sizeof(u32));

        /* The expansion the file comment describes; padded as for a
         * figatree, and a zero word is End in this language too. */
        padded = calloc((size_t)sNumWords + 2 * PAD_WORDS,
                        sizeof(AObjEvent32));
        if (padded == NULL)
        {
            return fprintf(stderr, "objanim_oracle: out of memory\n"), 1;
        }
        sWords32 = padded + PAD_WORDS;
        for (k = 0; k < sNumWords; k++)
        {
            sWords32[k].u = raw[k];
        }
        for (k = 0; k < hdr[5]; k++)
        {
            union { u32 u; float f; } bits;

            if (floats[k] >= sNumWords)
            {
                return fprintf(stderr, "objanim_oracle: float word %u of "
                               "%u\n", floats[k], sNumWords), 1;
            }
            bits.u = raw[floats[k]];
            sWords32[floats[k]].f = bits.f;
        }
        for (k = 0; k < hdr[4]; k++)
        {
            if (relocs[k] >= sNumWords || raw[relocs[k]] >= sNumWords)
            {
                return fprintf(stderr, "objanim_oracle: reloc word %u -> %u "
                               "of %u\n", relocs[k], raw[relocs[k]],
                               sNumWords), 1;
            }
            sWords32[relocs[k]].p = sWords32 + raw[relocs[k]];
        }
    }

    {
        /* SA_NODEFER so SIGALRM is not blocked while the handler runs:
         * siglongjmp leaves it by jumping, not returning, and a blocked
         * SIGALRM would never be delivered again. */
        struct sigaction sa;

        memset(&sa, 0, sizeof(sa));
        sa.sa_handler = oracle_alarm;
        sa.sa_flags = SA_NODEFER;
        sigaction(SIGALRM, &sa, NULL);
        alarm(PARSE_SECONDS);
    }

    if (syTaskmanMakeGeneralHeap(1024 * 1024) < 0)
    {
        return 1;
    }

    /* The pools and the scene's start function, but not the frame loop.
     * syTaskmanStartTask ends in syTaskmanRunTask and does not return
     * (src/dc/taskman.c), and this driver runs its own tics:
     * it steps one joint at a time and reads the AObjs back between
     * parses, which no scene_update could do. syTaskmanSetupPools is the
     * half of syTaskmanStartTask the port split out for exactly this --
     * the host cross-test drives the object system the same way
     * (src/game/ssb64/hosttest_ft.c). */
    syTaskmanSetupPools(&sOracleSetup);
    sOracleSetup.func_start();

    for (j = 0; j < sNumJoints; j++)
    {
        /* A negative entry is the table's own "this animation does not
         * drive this joint"; one past the end of the file is data the
         * reference refuses to read, and so does this. Either way the
         * DObj keeps the AOBJ_ANIM_NULL gcInitDObj left it. */
        if (entries[j] < 0)
        {
            continue;
        }
        if ((u32)entries[j] >= sNumWords)
        {
            sFailed[j] = ERR_OVERRUN;
            continue;
        }
        if (sKind == 0)
        {
            oracle_add_figatree(sJoints[j], sWords + entries[j], 0.0f);
        }
        else
        {
            gcAddDObjAnimJoint(sJoints[j], sWords32 + entries[j], 0.0f);
        }
        sDriven[j] = 1;
    }

    for (f = 0; f < hdr[2]; f++)
    {
        for (j = 0; j < sNumJoints; j++)
        {
            DObj *dobj = sJoints[j];
            u32 rec[2];
            f32 v[TRACK_COUNT];
            AObj *aobj;
            /* volatile: written inside the sigsetjmp region and read
             * after it. */
            volatile sb32 live = FALSE;

            if (sFailed[j] == ERR_NONE && sDriven[j] != 0)
            {
                if (sigsetjmp(sParseJmp, 0) == 0)
                {
                    sb32 ended;
                    ptrdiff_t pc;

                    sInParse = 1;
                    if (sKind == 0)
                    {
                        ftAnimParseDObjFigatree(dobj);
                    }
                    else
                    {
                        gcParseDObjAnimJoint(dobj);
                    }
                    sInParse = 0;

                    /* Where the script pointer stopped says whether the
                     * parse stayed inside the file. One past the last word
                     * is fine on its own -- a command can end there -- but
                     * reading from it is not, and a read either advances
                     * past it or, for the End the padding decodes as,
                     * retires the animation while sitting on it. */
                    ended = (dobj->anim_wait == AOBJ_ANIM_END) ? TRUE : FALSE;
                    pc = (sKind == 0) ? dobj->anim_joint.event16 - sWords
                                      : dobj->anim_joint.event32 - sWords32;

                    if (pc < 0 || (u32)pc > sNumWords ||
                        ((u32)pc == sNumWords && ended != FALSE))
                    {
                        sFailed[j] = ERR_OVERRUN;
                    }
                    /* Sampled here, not after: gcPlayDObjAnimJoint retires
                     * the animation on the frame the End lands, and that
                     * frame still has values to report -- the AObjs are
                     * aged and evaluated first, and only then does
                     * anim_wait go from AOBJ_ANIM_END to AOBJ_ANIM_NULL. */
                    live = (dobj->anim_wait != AOBJ_ANIM_NULL) ? TRUE : FALSE;
                    gcPlayDObjAnimJoint(dobj);
                }
                else
                {
                    /* See (3) in the file comment: the parse never came
                     * back, so the joint is done and the timer is re-armed
                     * for whatever comes next. */
                    sInParse = 0;
                    alarm(PARSE_SECONDS);
                    sFailed[j] = ERR_RUNAWAY;
                    live = FALSE;
                }
            }
            memset(v, 0, sizeof(v));
            rec[0] = 0;
            rec[1] = sFailed[j];

            if (rec[1] == ERR_NONE && live != FALSE)
            {
                for (aobj = dobj->aobj; aobj != NULL; aobj = aobj->next)
                {
                    s32 t = (s32)aobj->track - nGCAnimTrackJointStart;

                    if (aobj->kind == nGCAnimKindNone || t < 0 ||
                        t >= TRACK_COUNT)
                    {
                        continue;
                    }
                    rec[0] |= 1u << t;
                    v[t] = gcGetAObjValue(aobj);
                }
            }
            fwrite(rec, sizeof(rec), 1, stdout);
            fwrite(v, sizeof(v), 1, stdout);
        }
    }
    return 0;
}

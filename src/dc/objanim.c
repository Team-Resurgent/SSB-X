/* ssb64-dc: sys/objanim.c, the decomp's own, with its one big-endian
 * line of arithmetic answered.
 *
 * The file below is still compiled unmodified -- it is #included, not
 * copied -- under two renames, and this one stands in front of the two
 * functions they move aside.
 *
 * THE FAULT: gcPlayMObjMatAnim's LINEAR colour track
 * (objanim.c:1351-1388) lerps two packed RGBA words two channels at a
 * time. It writes R and A of an endpoint into the `g` and `a` BYTES of a
 * scratch SYColorPack, multiplies the scratch's 32-BIT WORD by a 0..256
 * fraction, and reads the answers back out of the `r` and `b` bytes: on
 * the N64 a multiply by 256 carries byte 1 into byte 0 and byte 3 into
 * byte 2. On a little-endian CPU it carries them the other way. The
 * first channel of each pair lands in the wrong byte and the second is
 * shifted out of the word, and the endpoints are indexed from the wrong
 * end besides. What comes out for an endpoint of RR GG BB AA is
 * GG 00 AA 00 as the port reads a pack: a white material fading its
 * alpha in is FF 00 xx 00, MAGENTA, which is the opening Room's
 * spotlight as the user saw it, and no green or alpha in any material
 * colour anywhere that a script moves over a duration. tools/
 * ssb_assets.py's _AObj.value has the right arithmetic, so the offline checks never saw it; the STEP kind is a word
 * copy and was always right.
 *
 * The port's convention for a pack is the WORD: 0xRRGGBBAA, which is
 * what the exporters write for a script's colour and an MObjSub's
 * (src/dc/objmodel.c) and what src/dc/fighter.c's material_of shifts
 * apart. So the answer is the same lerp on the word's bit fields.
 *
 * gcPlayAnimAll is here only because it calls gcPlayMObjMatAnim from
 * inside the same file, where a rename reaches the call too; its body
 * is objanim.c:1428-1471 verbatim. */
#define gcPlayMObjMatAnim gcPlayMObjMatAnimBE
#define gcPlayAnimAll     gcPlayAnimAllBE
#if defined(DB_ANIM_CHECK) && !defined(FT_HOSTTEST)
#define gcParseMObjMatAnimJoint gcParseMObjMatAnimJointBE
#endif
#include <sys/objanim.c>
#undef gcPlayMObjMatAnim
#undef gcPlayAnimAll
#if defined(DB_ANIM_CHECK) && !defined(FT_HOSTTEST)
#undef gcParseMObjMatAnimJoint
void gcParseMObjMatAnimJoint(MObj *mobj);
#endif

#include <string.h>

#if defined(DB_ANIM_CHECK) && !defined(FT_HOSTTEST)
#include <kos/dbglog.h>
int fighter_animcheck(const void *script, const char *what);    /* fighter.h */

/* which live GObj the MObj hangs off, for the stale-script line */
static void animcheck_owner(MObj *target)
{
    u32 link;

    for (link = 0; link < ARRAY_COUNT(gGCCommonLinks); link++)
    {
        GObj *gobj;

        for (gobj = gGCCommonLinks[link]; gobj != NULL; gobj = gobj->link_next)
        {
            DObj *dobj;

            if (gobj->obj_kind != nGCCommonAppendDObj)
                continue;
            dobj = DObjGetStruct(gobj);
            while (dobj != NULL)
            {
                MObj *mobj;

                for (mobj = dobj->mobj; mobj != NULL; mobj = mobj->next)
                {
                    if (mobj == target)
                    {
                        dbglog(DBG_ERROR, "animcheck: owner GObj %p id %d "
                               "link %u, user_data %p\n", (void *)gobj,
                               (int)gobj->id, (unsigned)link,
                               gobj->user_data.p);
                        return;
                    }
                }
                if (dobj->child != NULL)
                    dobj = dobj->child;
                else if (dobj->sib_next != NULL)
                    dobj = dobj->sib_next;
                else
                {
                    while (dobj != NULL && dobj->parent != DOBJ_PARENT_NULL &&
                           dobj->parent->sib_next == NULL)
                        dobj = dobj->parent;
                    dobj = (dobj == NULL || dobj->parent == DOBJ_PARENT_NULL)
                         ? NULL : dobj->parent->sib_next;
                }
            }
        }
    }
    dbglog(DBG_ERROR, "animcheck: owner not in any GObj link\n");
}
#endif

void gcPlayMObjMatAnim(MObj *mobj);
void gcPlayAnimAll(GObj *gobj);

static u32 objAnimLerpColor(const AObj *aobj)
{
    u32 base, target, out = 0;
    s32 interp = (aobj->length * aobj->length_invert * 256.0F);
    s32 shift;

    /* the endpoints are colour words a script left in float slots
     * (gcParseMObjMatAnimJoint's `event32->f`): bits, not values */
    memcpy(&base, &aobj->value_base, sizeof(base));
    memcpy(&target, &aobj->value_target, sizeof(target));

    if (interp < 0)
    {
        interp = 0;
    }
    if (interp > 256)
    {
        interp = 256;
    }
    /* A channel's product is at most 255 * 256, so the pairs the N64
     * packs into one multiply never carry into each other and one
     * channel at a time is the same number. */
    for (shift = 24; shift >= 0; shift -= 8)
    {
        u32 b = (base >> shift) & 0xFF, t = (target >> shift) & 0xFF;

        out |= ((((256 - interp) * b + t * interp) >> 8) & 0xFF) << shift;
    }
    return out;
}

#if defined(DB_ANIM_CHECK) && !defined(FT_HOSTTEST)
/* Every MObj script the port runs is in a loaded pack
 * (objmodel.c dc_model_add_mobjs): one that is not is named, with its
 * owner, and stopped, before the parser takes whatever is there for
 * commands -- an unknown one it never steps past (the `default:` of its
 * do-while), which is a spin with no other trace. */
void gcParseMObjMatAnimJoint(MObj *mobj)
{
    if ((mobj->anim_wait != AOBJ_ANIM_NULL) &&
        (fighter_animcheck(mobj->matanim_joint.event32, "MObj") < 0))
    {
        animcheck_owner(mobj);
        mobj->matanim_joint.event32 = NULL;
        mobj->anim_wait = AOBJ_ANIM_NULL;
        return;
    }
    {
        AObjEvent32 *before = mobj->matanim_joint.event32;

        gcParseMObjMatAnimJointBE(mobj);

        /* the parse itself stepped off: name the commands it read */
        if ((mobj->anim_wait != AOBJ_ANIM_NULL) &&
            (fighter_animcheck(mobj->matanim_joint.event32, "MObj parse") < 0))
        {
            const u32 *w = (const u32 *)before;

            dbglog(DBG_ERROR, "animcheck: the parse from %p read "
                   "%08lx %08lx %08lx %08lx %08lx %08lx %08lx %08lx\n",
                   (void *)before,
                   (unsigned long)w[0], (unsigned long)w[1],
                   (unsigned long)w[2], (unsigned long)w[3],
                   (unsigned long)w[4], (unsigned long)w[5],
                   (unsigned long)w[6], (unsigned long)w[7]);
            animcheck_owner(mobj);
        }
    }
}
#endif

void gcPlayMObjMatAnim(MObj *mobj)
{
    /* read before the call: the last frame of a script turns
     * AOBJ_ANIM_END into AOBJ_ANIM_NULL on its way out */
    sb32 is_playing = (mobj->anim_wait != AOBJ_ANIM_NULL);
    AObj *aobj;

    gcPlayMObjMatAnimBE(mobj);

    if (!is_playing)
    {
        return;
    }
    /* `length` was advanced by the call and is what it lerped with */
    for (aobj = mobj->aobj; aobj != NULL; aobj = aobj->next)
    {
        if (aobj->kind != nGCAnimKindLinear)
        {
            continue;
        }
        switch (aobj->track)
        {
        case nGCAnimTrackPrimColor:
            mobj->sub.primcolor.pack = objAnimLerpColor(aobj);
            break;

        case nGCAnimTrackEnvColor:
            mobj->sub.envcolor.pack = objAnimLerpColor(aobj);
            break;

        case nGCAnimTrackBlendColor:
            mobj->sub.blendcolor.pack = objAnimLerpColor(aobj);
            break;

        case nGCAnimTrackLight1Color:
            mobj->sub.light1color.pack = objAnimLerpColor(aobj);
            break;

        case nGCAnimTrackLight2Color:
            mobj->sub.light2color.pack = objAnimLerpColor(aobj);
            break;

        default:
            break;
        }
    }
}

void gcPlayAnimAll(GObj *gobj)
{
    DObj *dobj = DObjGetStruct(gobj);
    MObj *mobj;

    while (dobj != NULL)
    {
        gcParseDObjAnimJoint(dobj);
        gcPlayDObjAnimJoint(dobj);

        mobj = dobj->mobj;

        while (mobj != NULL)
        {
            gcParseMObjMatAnimJoint(mobj);
            gcPlayMObjMatAnim(mobj);

            mobj = mobj->next;
        }
        if (dobj->child != NULL)
        {
            dobj = dobj->child;
        }
        else if (dobj->sib_next != NULL)
        {
            dobj = dobj->sib_next;
        }
        else while (TRUE)
        {
            if (dobj->parent == DOBJ_PARENT_NULL)
            {
                dobj = NULL;
                break;
            }
            else if (dobj->parent->sib_next != NULL)
            {
                dobj = dobj->parent->sib_next;
                break;
            }
            else dobj = dobj->parent;
        }
    }
}

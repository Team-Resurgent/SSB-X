/* efground.h -- the port-own half of ef/efground.c: what Part B (Phase
 * 1, background ground actors) needs that ef/efground.h (the decomp's,
 * pulled in transitively through ef/effect.h -- effunctions.h ->
 * efground.h -- which every efGround* prototype's fixed signature comes
 * from) has no room for: EFGroundDesc/EFDesc carry no field for a baked
 * pack or a resolved script, and dEFGroundDatas itself (the decomp's own
 * global, defined in efground.c) is declared nowhere -- not even there;
 * src/game/ssb64/hosttest_ft.c already writes its own extern for the
 * same reason. This is that missing declaration plus the two genuinely
 * new ones stage.c needs, so they live in one place rather than each
 * being repeated where used.
 */
#ifndef SSB_DC_EFGROUND_H
#define SSB_DC_EFGROUND_H

#include <ssb_types.h>
#include <sys/obj.h>
#include <ef/effect.h>

#include "fighter.h"

/* The baked pack + resolved scripts behind one EFGroundDesc entry, same
 * index as dEFGroundDatas[gkind].effect_descs[i]. anim_joint is already
 * the per-joint table lbCommonAddTreeDObjsAnimAll wants -- one
 * AObjEvent32* per joint of `pack`'s tree, in pack order, built once when
 * the stage loads (src/dc/stage.c) -- not a name to resolve per spawn.
 * pack NULL means "no entry" (padding past the gkind's real count, or a
 * gkind with none at all).
 *
 * The MATERIAL scripts do not ride here. lbCommonAddTreeDObjsAnimAll's
 * third argument is an `AObjEvent32 ***` -- a table per joint per MObj --
 * and the port's packs already carry exactly that, as the MObjs section's
 * words plus one entry index per MObj (fighter.h FPackMObjs), because a
 * baked pack has to know which of its textures each MObj's script steps
 * through anyway. So dcGroundSetupEffectDObjs attaches them from the pack
 * as it hangs the MObjs on (dc_model_add_mobjs_dobj) and matanim_joint
 * stays NULL. What the asset does carry is `matanim_alt`: which of the
 * pack's MatAnimJoints this desc plays, since a pack may hold more than
 * one and two descs share it -- Dream Land's Bronto, left-facing and
 * right-facing, is the only one in the game. */
typedef struct
{
    Fighter *pack;
    AObjEvent32 **anim_joint;
    AObjEvent32 ***matanim_joint;
    int matanim_alt;
    /* bit k set: pack joint k's DObjDesc id had 0xF000 set, which makes
     * it a billboard joint in efGroundSetupEffectDObjs (ef/efground.c:
     * 1328-1343). The pack keeps no ids, so the stage block carries this. */
    uint32_t billboard;
} EFGroundActorAsset;

/* ef/efground.c's own per-gkind table (decomp type, this port's global).
 * stage.c is this port's second reader/writer, to fill a gkind's slot in
 * from its own loaded Stage instead of leaving it NULL
 * (efGroundMakeAppearActor's guard, and efground.c's own file header
 * note, cover why it starts that way). */
extern EFGroundData dEFGroundDatas[];

/* Hand efground.c gkind's baked packs/scripts, one entry per
 * dEFGroundDatas[gkind].effect_descs[i], `count` long. NULL clears
 * gkind's table (stage release) -- efGroundMakeAppearActor's own guard
 * (dEFGroundDatas[gkind].effect_params != NULL) already keeps a cleared
 * gkind from spawning anything, so this only has to outlive the Stage
 * that owns the packs/scripts it points at. */
void efGroundSetActorAssets(s32 gkind, const EFGroundActorAsset *assets,
                             int count);

#endif /* SSB_DC_EFGROUND_H */

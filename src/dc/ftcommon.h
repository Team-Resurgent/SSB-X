/* ftcommon.h -- the fighter action-state machine, ported from
 * ssb-decomp-re's ft/ (this is a PORT: functions keep the decomp's
 * names, signatures and field names; only the N64 services around them
 * are replaced). Every function in ftcommon.c names its source file and
 * line range; deviations are marked DIVERGES where they happen.
 *
 * The types are the game's own. FTStruct, FTAttributes, FTDesc,
 * FTStatusDesc and everything under them come from <ft/fighter.h>, the
 * decomp's umbrella header, unmodified (two layout-neutral patches in
 * docker/patches/ let GCC parse it: the per-character headers moved
 * below the types they declare arrays of, and FTAnimDesc's bit-fields
 * following the ABI). So the fighter's position is what it is in the
 * game -- the TopN joint's translate, DObjGetStruct(fighter_gobj)->
 * translate.vec.f -- and every field a game function reads is there to
 * be read.
 *
 * Deviations from the decomp, and why:
 *  - What the game reaches through fp->data (the fighter's FTData: the
 *    ROM files, the motion table, the animation bank) the port keeps in
 *    the pack (src/dc/fighter.h), one Fighter instance per fighter,
 *    reached through dc_model_of(fighter_gobj) -- it hangs off the TopN
 *    DObj's payload, exactly where the display walk finds it, so FTStruct
 *    needs no field the game's lacks. fp->data stays NULL.
 *  - fp->attr is a real FTAttributes, but filled from the pack's
 *    FPackAttr (ftManagerSetupAttributes) rather than read off a ROM file
 *    in the game's layout: the pack carries the physics, the collision
 *    diamond, the ledge-grab box, the jostle box and the camera numbers,
 *    and every other field is zero. Exporting the whole struct is the
 *    day the pack format changes for it.
 *  - Fighters are made the game's way (ft/ftmanager.c ftManagerMakeFighter
 *    from an FTDesc, src/dc/ftmanager.c) and updated by the game's own
 *    six GObjProcesses: update/interrupt, physics/map, the capture
 *    variant of it, the catch search, the hit search and the params
 *    (damage) process. Man and Com fighters are driven (Com through
 *    src/dc/ftcomputer.c); Key and GameKey, the demo kinds, are not. The
 *    jostle between fighters on one floor is the game's.
 *  - Hit detection is the game's: gm/gmcollision.c compiled unmodified
 *    over the FTParts every joint carries, ft/ftmain.c's fighter, weapon,
 *    item and catch searches and hit stats (src/dc/ftmain.c), the
 *    knockback and hitlag numbers of ft/ftparam.c. What a landed hit
 *    becomes is ft/ftcommon/ftcommondamage.c (src/dc/ftcommon.c): the
 *    Damage statuses, the Fly/tumble launch and its landings, shield
 *    stun, shield break, rebound and the reflectors.
 *  - Anim locks (FTAttributes.animlock, ftParamSetAnimLocks) are set as
 *    the game sets them, and both the hit matrices and the display walk
 *    read them: fighter joints draw through matrix kind 0x4B
 *    (src/dc/objdisplay.c gcDObjLocalMatrix), which holds a locked joint
 *    still and, under is_use_animlocks, keeps a stretched joint's scale
 *    off its children.
 *  - Per-fighter figatree heaps (FTDesc.figatree_heap) are NULL: the
 *    game copies each animation file into one before playing it, the
 *    port plays the pack in place, relocated once at load.
 *  - Animation control is the game's -- ftAnimParseDObjFigatree or
 *    gcParseDObjAnimJoint by the motion's anim_desc.flags.is_anim_joint,
 *    then gcPlayDObjAnimJoint, over the fighter's own DObjs -- but which
 *    animation a status plays comes from the pack's motion table
 *    (FPackMotion) rather than from FTData's FTMotionDesc array. The
 *    motion-command scripts beside it ARE run (src/dc/ftmain.c, the
 *    game's interpreter verbatim, over the fighter's MainMotion file
 *    carried whole in the pack, with the common moveset file it calls
 *    copied in behind it: tools/export/ssb_packexport.py fighter_scripts).
 *  - Collision IS the game's: the diamond walk over the stage's real
 *    line tables, the yakumono platforms bound to the stage's collision
 *    layer (src/dc/stage.c), foot IK on slopes and the
 *    shared-ledge test (src/dc/mpcommon.c).
 *  - The KO is the game's (ft/ftmain.c:1815's blast-line check into
 *    ft/ftcommon/ftcommondead.c, then ftcommonsleep.c or
 *    ftcommonrebirth.c's platform, a real floor at line id -2), with its
 *    effects and HUD calls, the 1P-game arms (the map_bound_team_*
 *    box, sc1PGame*), included.
 * The status tables are the game's, row for row; tools/check/status_check.py
 * holds them to it.
 */
#ifndef SSB_DC_FTCOMMON_H
#define SSB_DC_FTCOMMON_H

#include <ft/fighter.h>         /* the fighter system's types and declarations */
#include <sc/scene.h>           /* SCBattleState, through the shim */

#include "fighter.h"
#include "input.h"

/* ft/fttypes.h:1230 FTStruct.joints[FTPARTS_JOINT_NUM_MAX].
 *
 * The game's joint ids (ft/ftdef.h:1071-1077) are TopN, TransN, XRotN,
 * YRotN, then the common parts from nFTPartsJointCommonStart (4): the
 * parts setup fills joints[4 + k] from DObjDesc entry k
 * (lb/lbcommon.c:1004 with the base &fp->joints[CommonStart], which the
 * ROM's own stores confirm), and an entry the setup_parts mask leaves
 * out keeps its slot, NULL. Pack joint k is entry k, so the port fills
 * the same slots from the same base (ftmanager.c, objmodel.c). Slots
 * 1 to 3 are hidden parts: FTAttributes.hiddenparts rows the status
 * setter makes, links and ejects per animation (ftcommon.c
 * ftMainUpdateHiddenPartID and the loop in ftMainSetStatus), so
 * joints[TransN] exists exactly while a motion's is_use_transn_joint
 * bit is set, which is when the game reads it. The figatree's slots are
 * dealt down the tree walk from TopN's child, hidden parts first
 * (lbCommonAddFighterPartsFigatree).
 *
 * tools/check/joint_check.py is the oracle for the table's base.
 *
 * FTPARTS_JOINT_NUM_MAX is the decomp's own (ft/ftdef.h:10, 37), so a
 * pack with more than 33 entries is refused rather than silently
 * truncated; the widest fighter on the roster, Samus, has exactly 33.
 */

/* ---- ft/ftmanager.c (src/dc/ftmanager.c): what the port adds --------
 * The manager's own interface (ftManagerAllocFighter, ftManagerMakeFighter,
 * ftManagerDestroyFighter, ftManagerAllocFigatreeHeapKind,
 * dFTManagerDefaultFighterDesc) is declared by <ft/ftmanager.h>. */

/* The port's dFTManagerDataFiles (ft/ftdata.c): the loaded pack per
 * fighter kind, NULL where none is loaded. ftManagerSetupFilesAllKind
 * fills a kind's slot when the scene asks for the kind, as the game
 * loads a kind's files, and ftManagerReleaseFilesAll empties the table
 * where the scene's files go; ftManagerMakeFighter refuses a kind with
 * no pack. */
extern Fighter *gFTManagerModels[nFTKindEnumCount];

/* The most vertices a pack may have. A fighter's clip buffer is its own
 * pack's length rather than this one (src/dc/ftmanager.c's pool block),
 * so the ceiling is the loader's and the exporter's agreement about what
 * the port will carry at all, not a buffer size.
 * tools/export/ssb_packexport.py VCLIP_MAX mirrors it and
 * refuses a larger pack; Kirby, the largest, has 2107, fifteen copy hats
 * of which the transform only ever runs one and his two
 * electric skeletons (V14). */
#define FTMANAGER_VCLIP_MAX 2304

/* Whether a kind has a pack for ftManagerSetupFilesAllKind to load: the
 * twelve playables. */
sb32 ftManagerKindHasPack(s32 fkind);

/* Whether a kind's warp-in statuses are ported (src/dc/ftcommon.c
 * dFTCommonEntryAppearStatusIDs has its row). Every kind whose special table
 * exists has one, so the only kind this answers FALSE for is Master Hand --
 * the one dFTMainSpecialStatusDescs has no table for, and one nothing in this
 * port spawns. A kind without them appears on the spot at the countdown. */
sb32 ftCommonEntryHasAppear(s32 fkind);

/* The two facts at once: the entry table names a status for this kind
 * AND that status has a real row in the kind's own status table.
 * entry_id is 0 facing right, 1 facing left. */
sb32 ftCommonEntryAppearIsLive(s32 fkind, s32 entry_id);

/* Give back every pack ftManagerSetupFilesAllKind loaded this scene,
 * except the kinds ftManagerKeepFilesForScene has marked. Called where
 * the scene's files go: by the scene manager between scenes and by the
 * heap reset hook (src/dc/taskman.h). */
void ftManagerReleaseFilesAll(void);

/* The port's own, and a deviation from the game rather than a stand-in
 * for it (src/dc/ftmanager.c says why): mark the packs
 * the scene about to start, or a later one on its path, will ask for,
 * so the release above leaves them where they are instead of making the
 * disc read them again. Two paths: into a battle, and the opening movie
 * from Portraits through Clash. The
 * scene manager calls it with the scene it is about to start, once, on
 * the way out of every scene; the set it computes is the whole set, so
 * there is nothing to un-mark. On the N64 the equivalent is free: a
 * fighter's files are re-DMA'd out of the cartridge into the new
 * scene's heap. */
void ftManagerKeepFilesForScene(s32 scene);

/* The opening movie's scenes after the Room, through Clash: the path
 * ftManagerKeepFilesForScene keeps packs along. The scene manager asks
 * too, for the files it holds in RAM across the movie. */
sb32 ftManagerSceneIsOpeningChain(s32 scene);
sb32 ftManagerSceneIs1PLadder(s32 scene);
/* The port's own: how much of each fighter's .anm the scene's
 * ftManagerSetupFilesAllKind calls read (src/dc/fighter.h
 * fighter_load_tier). A menu that plays a few motions of every kind --
 * the character selects, the results screen, the 1P card -- asks for
 * FIGHTER_ANM_TIER_MENU before its loads, and the Characters screen for
 * FIGHTER_ANM_TIER_CHARACTERS; every scene change puts it back to the
 * whole file, which a pack kept from a menu into the battle then reads.
 * An animation outside the tier is still read when it is bound, and the
 * log says so. */
#define FIGHTER_ANM_TIER_MENU 0
#define FIGHTER_ANM_TIER_CHARACTERS 1
void ftManagerSetAnmTier(s32 tier);

/* Points the fighter file pointers the code reads directly at the port's
 * copies (src/dc/ftcommon.c): overlay 3's reload and every
 * ftManagerSetupFilesAllKind, as the game's own fighter load does. */
void ftCommonBindMainMotionFiles(void);

/* the kind's file stem ("mario": mario.pack, ftmario.spr), or NULL */
const char *ftManagerKindName(s32 fkind);

/* Whether a kind's pack survives the next release. For the instrument
 * and the tests; no game code asks. */
sb32 ftManagerKindIsKept(s32 fkind);

/* The port's own (src/dc/ftmanager.c): the kind's FTSprites, the stock
 * icon and emblem ftManagerSetupFilesAllKind loaded into this scene,
 * without a fighter to read them off. NULL when the kind has no bank.
 * The VS results screen draws its header with it. */
FTSkeleton **ftManagerGetSkeletonWords(s32 joint, u32 ids);
FTSprites *ftManagerGetKindSprites(s32 fkind);

/* The pack's attributes into the game's FTAttributes (see the header
 * comment). The manager runs it per spawn; the host test runs it once
 * for its reference values. */
void ftManagerSetupAttributes(FTAttributes *attr, const FPackAttr *pa);
void ftManagerSetupShieldPose(FTAttributes *attr, const Fighter *model);

/* ft/ftmanager.c:418-668 ftManagerInitFighter, which no decomp header
 * declares by name (ftmanager.h has it as func_ovl2_800D79F0).
 * ftCommonRebirthDownSetStatus calls it to rebuild a fighter on the
 * respawn platform. */
void ftManagerInitFighter(GObj *fighter_gobj, FTDesc *desc);

/* Port-only: teleport to (x, y), settled the way the spawn was, in Wait
 * or Fall. A debug and test helper, not the respawn -- the blast line goes
 * through the game's own Dead and Rebirth statuses. It reacquires the
 * collision line, which writing translate by hand does not. */
void ftMainRespawn(GObj *fighter_gobj, float x, float y);

/* ---- ft/ftmain.c ------------------------------------------------------
 * The fighter's two GObjProcesses and ftMainSetStatus are declared by
 * <ft/ftmain.h>; the status setters other modules call
 * (ftCommonWaitSetStatus, ftCommonFallSetStatus, ftCommonEntrySetStatus,
 * ftCommonAppearSetStatus, ...) by <ft/ftcommon/ftcommonfunctions.h>. */

/* ftcommonottotto.c:100 and ftcommonstopceil.c:10: mp/mpcommon.c calls
 * both and no decomp header declares either (IDO took the implicit
 * declaration). */
void ftCommonOttottoSetStatus(GObj *fighter_gobj);
void ftCommonStopCeilSetStatus(GObj *fighter_gobj);

/* The two n_audio calls ft/ftparam.c and the script interpreter make by
 * ROM address -- start an FGM voice script and get its handle, stop one
 * -- which no decomp header declares. src/dc/sysshim.c defines them over
 * the port's FGM engine (src/dc/fgm.c); the handle is NULL there, so the
 * stops are no-ops and every "store info" field reads 0. */
alSoundEffect *func_800269C0_275C0(u32 fgm_id);
void func_80026738_27338(alSoundEffect *sfx);

/* n_env.c:5128, the third of them: stop every FGM at once. The game
 * calls it on the way out of a scene and on the way into the next, so
 * a battle's last sound does not follow the player to the title. */
void func_800266A0_272A0(void);

/* ef/efmanager.c's heavy-hit spark, which ft/ftmain.c calls and no
 * decomp header declares (the game takes the implicit declaration).
 * src/dc/ftparam.c defines it, a stub with the rest of the effects. */
void efManagerDamageNormalHeavyMakeEffect(Vec3f *pos, s32 player, s32 size);

/* Port-only: the status's name, for src/dc/db.c's log lines. */
const char *ftMainStatusName(int status_id);

/* The two stage hazards' takeover statuses, whose rows in
 * dFTCommonActionStatusDescs (src/dc/ftcommon.c) name them. The decomp
 * declares them in ft/ftcommon/ftcommonstatus.h, which this port's
 * ftcommon.c does not include -- it keeps the table and drops the
 * header -- so they are declared here instead. Hyrule's tornado is
 * src/dc/ftcommontwister.c, Kongo Jungle's barrel cannon
 * src/dc/ftcommontaru.c. Every one of these would otherwise be
 * an implicit `int` declaration at the table that names it. */
extern void ftCommonTwisterProcUpdate(GObj *fighter_gobj);
extern void ftCommonTwisterProcPhysics(GObj *fighter_gobj);
extern void ftCommonTwisterSetStatus(GObj *fighter_gobj, GObj *tornado_gobj);
extern void ftCommonTwisterShootFighter(GObj *fighter_gobj);

extern void ftCommonTaruCannProcUpdate(GObj *fighter_gobj);
extern void ftCommonTaruCannProcInterrupt(GObj *fighter_gobj);
extern void ftCommonTaruCannProcPhysics(GObj *fighter_gobj);
extern void ftCommonTaruCannSetStatus(GObj *fighter_gobj, GObj *tarucann_gobj);
extern void ftCommonTaruCannShootFighter(GObj *fighter_gobj);

#endif /* SSB_DC_FTCOMMON_H */

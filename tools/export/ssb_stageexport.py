#!/usr/bin/env python3
"""ssb64-dc: stage -> pack, converted at build time.

A stage is four geometry layers, each a DObjDesc tree in the stage model
file, named by the map header (MPGroundData) in the map logic file. Layers
whose layer_mask bit is set store DObjDLLink arrays -- {DL head, DL} pairs
walked by gcDrawDObjDLLinks -- instead of a single DL; layers 0 and 2 draw
with the z-buffer off (backgrounds). Stage DLs run with G_LIGHTING cleared:
their Vtx colours ride in the normal bytes and the baked batches carry
shaded=2 (vertex colour).

Which PVR list a batch lands in comes from the RDP render mode in force
where its triangles were emitted: the layer's display proc seeds one per DL
head (LAYER_RENDERMODE, quoted from gr/grdisplay.c) and the display lists
change it as they go. FORCE_BL is a blend and goes translucent, CVG_X_ALPHA
without it is the RDP's alpha cutout and goes punch-through, and the rest is
opaque -- see ssb_assets.rendermode_list. layer_mask does not enter into it,
which is the point: three of the seven exported stages set it to 0, and
before this every one of their batches was opaque by default.

The visual layers are merged into one fighter pack (the target renders a
stage with the exact code that renders a fighter), carrying the layers'
own AnimJoint tables as the pack's single animation -- see
read_layer_anims, and LAYER_ANIM_LAYERS for the one layer left out of it
on purpose. That pack is followed by a
STG3 section: MPGeometryData's four collision tables (line_info,
vertex_links, vertex_id, vertex positions -- mp/mptypes.h:16-80) copied
value for value and byte-swapped, the map objects (spawn points), the
camera/blast bounds and the BGM id. The target hands the tables to the
decomp's own mpcollision.c/mpprocess.c, compiled unmodified, so the
layout is the game's: no segment soup, line ids are the game's line ids.
Every scalar the map header supplies is asserted against the decomp's
typed initializer for the stage before anything is written.

Usage: python3 tools/export/ssb_stageexport.py --out romdisk/hyrule.stg
                                        [--rom <rom.z64>] [--stage Hyrule]
"""
import os
import struct
import sys

sys.path[:0] = [os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), d)
                for d in ("lib", "export", "check")]
import ssb_extract               # noqa: E402
import ssb_assets as A           # noqa: E402
import ssb_meshexport as M       # noqa: E402
import ssb_logicexport as L      # noqa: E402
import ssb_spriteexport as SP    # noqa: E402  (unshuffle_row, SP_TEXSHUF)
from ssb_packexport import model_sections  # noqa: E402
# The fighter pack's animation directory, shared verbatim: a stage's
# layer animation is one more FPackAnim in the same section a fighter's
# motions live in, so it is read by the same fighter_init.
from ssb_packexport import ANIM_NAME_LEN as SP_ANIM_NAME_LEN  # noqa: E402
from ssb_packexport import ANIM_DIR_ENTRY as SP_ANIM_DIR_ENTRY  # noqa: E402

# fighter.h FPACK_ANIM_ANIMJOINT: the AObjEvent32 language everything but
# a fighter's figatrees is written in.
FPACK_ANIM_ANIMJOINT = 1

# MPGroundData within the map logic file. It was a constant 0x14 while
# every stage here was a VS stage, because a VS stage's logic file opens
# with the eight `HitDesc`/`ItemAttributes`/`WeaponAttributes` words the
# stage's own code needs and puts the header after them. Final
# Destination has no stage code at all -- there is no gr/grcommon/
# grlast.c -- so its file is the header and nothing else, at 0x0. The
# descriptions file says which, per group, on its `MapHeader` line, and
# that is the same source of truth read_map_blocks already reads.
def ground_off(fid):
    for kind, name, off in read_map_blocks(fid):
        if kind == "MapHeader":
            return off
    raise AssertionError("no MapHeader line in descriptions group [%d]"
                         % fid)

# The decomp checkout, for the one file the ROM cannot answer: which
# blocks a stage's map group is made of, and what they are called. The
# ROM holds the bytes; only tools/relocFileDescriptions.us.txt says that
# the word at 0xB20 is an AnimJoint called TaruCannDefault. Same lookup
# every other tool here uses.
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
DECOMP = os.environ.get("SSB_DECOMP_DIR", "/opt/ssb-decomp-re")
if not os.path.isdir(DECOMP):
    DECOMP = os.path.join(ROOT, "ssb-decomp-re")
DESC = os.path.join(DECOMP, "tools", "relocFileDescriptions.us.txt")

# AObjEvent32's per-opcode payload length, in words AFTER the command
# word -- the rule ssb-decomp-re/src/sys/objanim.c's dispatcher follows,
# and the same table its own tools/decodeAObjEvent32.py carries. Needed
# here only to find where a script ends: a payload word can hold 0.0f,
# which decodes as opcode 0 (End), so a script cannot be walked by
# looking for zeros.
def aobj_payload_words(opcode, flags):
    """Words this command consumes after its own, or None if unknown."""
    pc = bin(flags).count("1")
    table = {
        0x00: 0, 0x01: 1, 0x02: 0, 0x03: pc, 0x04: pc, 0x05: 2 * pc,
        0x06: 2 * pc, 0x07: pc, 0x08: pc, 0x09: pc, 0x0A: pc, 0x0B: pc,
        0x0C: 0, 0x0D: 1, 0x0E: 1, 0x0F: 0, 0x10: 0,
        0x11: bin((flags >> 4) & 0x3FF).count("1"),
        0x12: pc, 0x13: pc, 0x14: pc, 0x15: pc, 0x16: 0, 0x17: 0,
    }
    return table.get(opcode)


# stage name -> (map logic file, expected scalars from the decomp's typed
# MPGroundData initializer: cam bounds t/b/r/l, map bounds t/b/r/l,
# alt_warning, bgm id)
STAGES = {
    "Castle": {
        "map_file": 259,
        "cam": (4800, -1300, 4000, -4000),
        "map": (9500, -4000, 9000, -9000),
        "alt_warning": -1900,
        "bgm": 6,
        # dGRCastleMap_item_weights (relocData/Castle)
        # MPGroundData.fog_color (relocData/259_GRCastleMap.c:39), checked against
        # the ROM the same way item_weights is: the magnifying
        # glass draws its disc in this colour.
        "fog": (0xB0, 0xC2, 0xE0),
        "item_weights": (0x50, 0x1E, 0x78, 0x00, 0x0E, 0x07,
                          0x0C, 0x0A, 0x05, 0x0F, 0x0A, 0x08,
                          0x13, 0x08, 0x10, 0x16, 0x0A, 0x0A,
                          0x0A, 0x14),
        # Peach's Castle's `map_nodes` is NOT a tree, and this is the
        # only stage of the nine whose is not: it is
        # `dStageCastleFile3_AnimJointRoot` (relocData file 156), a
        # ONE-SLOT `AObjEvent32 *` table whose entry is the stage's one
        # script, and the game hands the table itself to
        # `gcAddAnimJointAll(ground_gobj, map_nodes, 0.0F)`
        # (gr/grcommon/grcastle.c:45) -- on a ground GObj whose only DObj
        # is an empty one. That script is `TraX` over 2400 frames: the
        # invisible driver the bumper hangs off, swept -1050 then back
        # +1050, which grCastleBumperProcUpdate copies onto the Bumper's
        # own x every tic. So this entry carries SCRIPTS and no object.
        # See read_map_object's anims_only arm for where they are found.
        "map_object": {
            "anims_only": True,
            "anims": ["AnimJointRoot"],
        },
        # Part B (Phase 1): the Lakitu flying over the castle --
        # ef/efground.c's dEFGroundCastleEffectDescs/dEFGroundCastleParams,
        # transcribed by hand (this data is the executable's own .data,
        # compiled from efground.c, not a ROM asset the reloc-chain
        # machinery can read -- see read_ground_actors). One pack (the
        # DObjDesc block relocFileDescriptions.us.txt's [259] group calls
        # `DObjDesc Lakitu` at 0x4118), two spawnable variants sharing it
        # -- Right-facing (`AnimJoint LakituR` 0x4220) and Left-facing
        # (`AnimJoint LakituL` 0x4370) -- no MObjSub, no MatAnimJoint:
        # Lakitu's material is baked, not animated.
        "ground_actors": {
            "packs": {
                "Lakitu": {"desc": "Lakitu"},
            },
            "descs": [
                {"pack": "Lakitu", "anim": "LakituR", "dl_link": 4,
                 "update_kind": "common", "alt_high": 4000.0,
                 "alt_low": -1000.0, "pos_z": -2000.0, "scale": 1.3,
                 "effect_status": -1},
                {"pack": "Lakitu", "anim": "LakituL", "dl_link": 4,
                 "update_kind": "common", "alt_high": 4000.0,
                 "alt_low": -1000.0, "pos_z": -1000.0, "scale": 1.0,
                 "effect_status": -1},
            ],
            "params": [
                {"effect_id": 0, "make_queue": 0, "lr": 0, "weight": 2},
                {"effect_id": 1, "make_queue": 0, "lr": 0, "weight": 1},
            ],
        },
    },
    "Sector": {
        "map_file": 262,
        "cam": (7500, -1800, 10100, -9800),
        "map": (11000, -6500, 14000, -14000),
        "alt_warning": -2300,
        "bgm": 4,
        # the US bytes of relocData/262_GRSectorMap.c's item weights, which
        # the decomp names dGRSectorMap_Arwing0_AnimJoint; without this row
        # the stage handed the randomizer no weights and no item dropped
        # MPGroundData.fog_color (relocData/262_GRSectorMap.c:58), checked against
        # the ROM the same way item_weights is: the magnifying
        # glass draws its disc in this colour.
        "fog": (0x00, 0x00, 0x32),
        "item_weights": (0x3C, 0x64, 0x96, 0x00, 0x06, 0x03,
                          0x08, 0x14, 0x05, 0x08, 0x05, 0x0F,
                          0x0B, 0x07, 0x15, 0x0C, 0x0A, 0x0A,
                          0x04, 0x12),
        # Sector Z has NO map object, so this is the no-map-object arm: its
        # Arwing is built by grSectorInitAll out of FOX's Special3 file
        # through lbRelocGetForceStatusBufferFile, not out of `map_nodes`.
        # What the stage owns is the eight FLIGHT PATTERNS' scripts, four
        # per pattern, named after the descriptor field each fills: `Path`
        # is `anim_joint_0x0` (whose end drives the whole patrol),
        # `Alt7`/`Alt9`/`Alt11` are the 0x1C/0x24/0x2C fields and hang on
        # map_dobjs[7]/[9]/[11]. Two patterns have no fourth script
        # (Arwing1 and Arwing4 are NULL at 0x2C), which is why there are 30
        # and not 32.
        #
        # NOTHING ELSE IS LISTED, and that is the point of the arm: every
        # one of these scripts opens with `aobjEvent32SetInterp`, whose
        # operand is an `SYInterpDesc` rather than another script -- data
        # the pack could not carry before. Those descriptors
        # (0xE8, 0x210, 0x338, 0x510, 0x698, 0x9B0, 0xCD8, 0xDF8, 0x1AF4
        # and the ones inside the descriptors themselves) are found by
        # add_interp_descs from the fixups, so listing them here would name
        # data as if it were a script -- which is exactly the mistake that
        # cost the first two attempts at this.
        "stage_anims": [
            ("AnimJoint", "Arwing0Path",  (153, 0x0100), 0),
            ("AnimJoint", "Arwing0Alt7",  (153, 0x0140), 0),
            ("AnimJoint", "Arwing0Alt9",  (153, 0x014C), 0),
            ("AnimJoint", "Arwing0Alt11", (153, 0x0228), 0),
            ("AnimJoint", "Arwing1Path",  (153, 0x0350), 0),
            ("AnimJoint", "Arwing1Alt7",  (153, 0x03B8), 0),
            ("AnimJoint", "Arwing1Alt9",  (153, 0x03C8), 0),
            ("AnimJoint", "Arwing2Path",  (153, 0x09C8), 0),
            ("AnimJoint", "Arwing2Alt7",  (153, 0x0A08), 0),
            ("AnimJoint", "Arwing2Alt9",  (153, 0x0A18), 0),
            ("AnimJoint", "Arwing2Alt11", (153, 0x0CF0), 0),
            ("AnimJoint", "Arwing3Path",  (153, 0x0528), 0),
            ("AnimJoint", "Arwing3Alt7",  (153, 0x0568), 0),
            ("AnimJoint", "Arwing3Alt9",  (153, 0x0580), 0),
            ("AnimJoint", "Arwing3Alt11", (153, 0x06B0), 0),
            ("AnimJoint", "Arwing4Path",  (153, 0x0E10), 0),
            ("AnimJoint", "Arwing4Alt7",  (153, 0x0E98), 0),
            ("AnimJoint", "Arwing4Alt9",  (153, 0x0EA0), 0),
            ("AnimJoint", "Arwing5Path",  (153, 0x101C), 0),
            ("AnimJoint", "Arwing5Alt7",  (153, 0x1044), 0),
            ("AnimJoint", "Arwing5Alt9",  (153, 0x1054), 0),
            ("AnimJoint", "Arwing5Alt11", (153, 0x11A0), 0),
            ("AnimJoint", "Arwing6Path",  (153, 0x1808), 0),
            ("AnimJoint", "Arwing6Alt7",  (153, 0x1824), 0),
            ("AnimJoint", "Arwing6Alt9",  (153, 0x1834), 0),
            ("AnimJoint", "Arwing6Alt11", (153, 0x1B0C), 0),
            ("AnimJoint", "Arwing7Path",  (153, 0x133C), 0),
            ("AnimJoint", "Arwing7Alt7",  (153, 0x1360), 0),
            ("AnimJoint", "Arwing7Alt9",  (153, 0x1370), 0),
            ("AnimJoint", "Arwing7Alt11", (153, 0x14E0), 0),
            # ... and the five PILOT animations, which are a different table
            # again: `dGRSectorArwingAnimJoints[6]` is indexed by
            # `arwing_pilot_curr` and its entry 0 is 0x0000 -- the
            # descriptor the map head itself points at, never a script,
            # because pilot 0 means "no pilot". Pilots 1 to 5 are these.
            ("AnimJoint", "ArwingPilot1", (153, 0x1D34), 0),
            ("AnimJoint", "ArwingPilot2", (153, 0x1DA4), 0),
            ("AnimJoint", "ArwingPilot3", (153, 0x1DC4), 0),
            ("AnimJoint", "ArwingPilot4", (153, 0x1D54), 0),
            ("AnimJoint", "ArwingPilot5", (153, 0x1DE4), 0),
            # ... and the two scripts out of FOX's Special3 that the
            # Arwing's own transitions play -- `grSectorInitAll` on
            # map_dobjs[10] and the pilot-5 case on map_dobjs[8]. Both are
            # reached through `map_file`, which IS Fox's file.
            ("AnimJoint", "FoxSpecial3_2E74", (161, 0x2E74), 0),
            ("AnimJoint", "FoxSpecial3_2EB4", (161, 0x2EB4), 0),
            # ... and two more that the decomp reaches through symbols
            # NAMED for Fox's file but adds to `map_head`, which is THIS
            # stage's file, and that is where they really are. `_1B84_` is
            # played on map_dobjs[4]/[5] when a volley opens and `_1B34_`
            # on map_dobjs[2]/[3] when the shot is fired; the four joints
            # are born at scale 1e-5 in the Arwing's tree, so these two
            # SCAXYZ scripts are what make those parts appear.
            #
            # The file is checked and not assumed, because the symbol name
            # says otherwise: file 161 at 0x1B34 is a display list
            # (`0xFF110000`), while file 153's is the script
            # `dStageSectorFile3_Sub_0x1B34`. And `map_head` is file 153's
            # base -- `map_head + 0x0` is `llGRSectorMapArwing0SectorDesc`,
            # whose four pointer slots relocate to this file's
            # 0x100/0x140/0x14C/0x228, which is how the `Arwing0*` names
            # above were found in the first place.
            ("AnimJoint", "ArwingGunScale",  (153, 0x1B34), 0),
            ("AnimJoint", "ArwingWingScale", (153, 0x1B84), 0),
        ],
        # Part B: the Great Fox and the rockets that fly past behind the
        # battle -- dEFGroundSectorEffectDescs (ef/efground.c:152-295),
        # seven params over five descs and two identities. The Rocket is
        # the only actor in the game with a spawn-time
        # `proc_groundeffect`: desc 0 takes `efGroundSetStepPositions`,
        # which sizes a per-tic `scale_step` so the rocket GROWS as it
        # crosses, and pairs with the `steps` update proc that applies it.
        # The other two rocket descs are the same tree at other depths
        # with the plain update.
        "ground_actors": {
            "packs": {
                "Rocket": {"desc": "Rocket"},
                "Ship": {"desc": "Ship", "mobjsub": "Ship",
                         "matanim": "Ship"},
            },
            "descs": [
                {"pack": "Rocket", "anim": "Rocket", "dl_link": 4,
                 "update_kind": "steps", "setup_kind": "steps",
                 "alt_high": 5000.0, "alt_low": 0.0, "pos_z": -10000.0,
                 "scale": 6.0, "effect_status": -1},
                {"pack": "Rocket", "anim": "Rocket", "dl_link": 4,
                 "update_kind": "common", "alt_high": 5000.0,
                 "alt_low": 0.0, "pos_z": -3000.0, "scale": 6.0,
                 "effect_status": -1},
                {"pack": "Rocket", "anim": "Rocket", "dl_link": 4,
                 "update_kind": "common", "alt_high": 5000.0,
                 "alt_low": 0.0, "pos_z": -10000.0, "scale": 6.0,
                 "effect_status": -1},
                {"pack": "Ship", "anim": "ShipL", "dl_link": 4,
                 "update_kind": "common", "alt_high": 4000.0,
                 "alt_low": 0.0, "pos_z": -8000.0, "scale": 6.0,
                 "effect_status": -1},
                {"pack": "Ship", "anim": "ShipR", "dl_link": 4,
                 "update_kind": "common", "alt_high": 4000.0,
                 "alt_low": 0.0, "pos_z": -8000.0, "scale": 6.0,
                 "effect_status": -1},
            ],
            # dEFGroundSectorParams (efground.c:1081-1090)
            "params": [
                {"effect_id": 3, "make_queue": 0, "lr": -1, "weight": 3},
                {"effect_id": 4, "make_queue": 0, "lr": +1, "weight": 3},
                {"effect_id": 4, "make_queue": 1, "lr": +1, "weight": 4},
                {"effect_id": 0, "make_queue": 0, "lr": -1, "weight": 3},
                {"effect_id": 1, "make_queue": 0, "lr": -1, "weight": 2},
                {"effect_id": 2, "make_queue": 0, "lr": -1, "weight": 2},
                {"effect_id": 2, "make_queue": 0, "lr": +1, "weight": 1},
            ],
        },
    },
    "Jungle": {
        "map_file": 261,
        "cam": (4000, -2000, 3700, -3700),
        "map": (8000, -4700, 8100, -8100),
        "alt_warning": -1900,
        "bgm": 5,
        # The stage's map object (MPGroundData.map_nodes): Kongo
        # Jungle's barrel cannon, whose DObjDesc tree and AnimJoint
        # scripts come out of the map data file beside the collision
        # tables. Only stages that ask get the export; the others are
        # their own steps. See read_map_object for what each field names.
        "map_object": {
            "desc": "MapHead",
            "anims": ["TaruCannDefault", "TaruCannFill", "TaruCannShoot"],
        },
        # dGRJungleMap_item_weights (relocData/261_GRJungleMap.c:18)
        # MPGroundData.fog_color (relocData/261_GRJungleMap.c:43), checked against
        # the ROM the same way item_weights is: the magnifying
        # glass draws its disc in this colour.
        "fog": (0x5A, 0x0F, 0x00),
        "item_weights": (0x50, 0x78, 0x32, 0x00, 0x0A, 0x05, 0x06, 0x0C,
                         0x05, 0x08, 0x0A, 0x05, 0x12, 0x0D, 0x0A, 0x0A,
                         0x0E, 0x03, 0x07, 0x14),
        # Part B: the birds (dEFGroundJungleEffectDescs,
        # ef/efground.c:109-149). One actor identity (unlike Castle's
        # two): the group's own MObjSub/DObjDesc/AnimJoint/MatAnimJoint
        # all named plain "Bird", so one entry names all four.
        "ground_actors": {
            "packs": {
                "Bird": {"desc": "Bird", "mobjsub": "Bird",
                         "matanim": "Bird"},
            },
            "descs": [
                {"pack": "Bird", "anim": "Bird", "dl_link": 4,
                 "update_kind": "common", "alt_high": 4000.0,
                 "alt_low": 3000.0, "pos_z": -12000.0, "scale": 1.4,
                 "effect_status": -1},
            ],
            # dEFGroundJungleParams (efground.c:76-81): both entries pick
            # the same (only) desc -- effect_id 0 -- and differ in
            # make_queue, one bird (weight 2) or a queued pair (weight 1).
            "params": [
                {"effect_id": 0, "make_queue": 0, "lr": 0, "weight": 2},
                {"effect_id": 0, "make_queue": 1, "lr": 0, "weight": 1},
            ],
        },
    },
    "Zebes": {
        "map_file": 257,
        "cam": (4700, -2400, 4500, -4500),
        "map": (9000, -4200, 9500, -9500),
        "alt_warning": -2900,
        "bgm": 1,
        # The acid: its tree, its material chain (MObjSub, and the
        # MatAnimJoint that scrolls it), its one AnimJoint, and the attack
        # descriptor grZebesAcidCheckGetDamageKind hands the fighter
        # system. The group also names Ridley and Ship; they are other
        # objects in other files, for the steps that port them.
        "map_object": {
            "desc": "Acid",
            "anims": ["Acid"],
            "mobjsub": "Acid",
            # the script that scrolls the acid's surface
            # (grzebes.c:95-101's gcAddAnimAll third argument)
            "matanim": "Acid",
            "attack_coll": "Acid",
            # the acid's tree is drawn through DObjDLLink arrays
            # (gcDrawDObjTreeDLLinksForGObj), so each DObjDesc's dl is a
            # {list_id, Gfx *} list rather than a display list
            "dl_links": True,
        },
        # dGRZebesMap_item_weights (relocData/Zebes)
        # MPGroundData.fog_color (relocData/257_GRZebesMap.c:39), checked against
        # the ROM the same way item_weights is: the magnifying
        # glass draws its disc in this colour.
        "fog": (0x00, 0x00, 0x00),
        "item_weights": (0x14, 0x08, 0xC8, 0x00, 0x0A, 0x05,
                          0x05, 0x14, 0x05, 0x08, 0x0C, 0x1E,
                          0x0F, 0x08, 0x16, 0x0C, 0x0E, 0x05,
                          0x07, 0x10),
        # Part B: Samus' gunship and Ridley, the two the map_object entry
        # above already says are "for the steps that port them" --
        # dEFGroundZebesEffectDescs (ef/efground.c:351-431). One desc per
        # identity; the params spawn the ship alone, the ship queued, or
        # Ridley.
        "ground_actors": {
            "packs": {
                "Ship": {"desc": "Ship", "mobjsub": "Ship",
                         "matanim": "Ship"},
                "Ridley": {"desc": "Ridley", "mobjsub": "Ridley",
                           "matanim": "Ridley"},
            },
            "descs": [
                {"pack": "Ship", "anim": "Ship", "dl_link": 4,
                 "update_kind": "common", "alt_high": 4000.0,
                 "alt_low": 1000.0, "pos_z": -5000.0, "scale": 2.0,
                 "effect_status": -1},
                {"pack": "Ridley", "anim": "Ridley", "dl_link": 4,
                 "update_kind": "common", "alt_high": 6000.0,
                 "alt_low": 2000.0, "pos_z": -10000.0, "scale": 3.0,
                 "effect_status": -1},
            ],
            # dEFGroundZebesParams (efground.c:1073-1079)
            "params": [
                {"effect_id": 0, "make_queue": 0, "lr": 0, "weight": 1},
                {"effect_id": 0, "make_queue": 1, "lr": 0, "weight": 1},
                {"effect_id": 1, "make_queue": 0, "lr": 0, "weight": 1},
            ],
        },
    },
    "Hyrule": {
        "map_file": 265,
        "cam": (6000, -2100, 6000, -6000),
        "map": (9000, -5000, 12000, -12000),
        "alt_warning": -2600,
        "bgm": 9,
        # dGRHyruleMap_item_weights (relocData/Hyrule)
        # MPGroundData.fog_color (relocData/265_GRHyruleMap.c:46), checked against
        # the ROM the same way item_weights is: the magnifying
        # glass draws its disc in this colour.
        "fog": (0xE1, 0xC8, 0xFF),
        "item_weights": (0x50, 0x3C, 0x50, 0x00, 0x00, 0x14,
                          0x06, 0x0F, 0x08, 0x08, 0x0A, 0x08,
                          0x0A, 0x0A, 0x0D, 0x0A, 0x0A, 0x0A,
                          0x05, 0x12),
    },
    "Yoster": {
        "map_file": 263,
        "cam": (4300, -2000, 7000, -4300),
        "map": (8200, -4000, 10500, -7800),
        "alt_warning": -2500,
        "bgm": 8,
        # The clouds. The group names `MapHead` (the tree, shared by all
        # three clouds), `AnimJoint _1E0_`, `MObjSub _4B8_`, `DisplayList
        # Cloud` and the two MatAnimJoints; the cloud's own material is
        # ANIMATED rather than baked (FPackMObjs), because
        # grYosterUpdateCloudSolid gates on dobj[0]->mobj->anim_wait and
        # the evaporate script is what makes a cloud look like it is
        # evaporating.
        "map_object": {
            "desc": "MapHead",
            "anims": ["_1E0_"],
            "mobjsub": "_4B8_",
            # the cloud itself: a bare DisplayList grafted onto the
            # skeleton's cloud joints, with its material's two states as
            # the pack's two MatAnimJoint `alt`s
            "graft": {
                "dl": "Cloud",
                "mobjsub": "_4B8_",
                "matanim": ["CloudSolid", "CloudEvaporate"],
            },
        },
        # dGRYosterMap_item_weights (relocData/Yoster)
        # MPGroundData.fog_color (relocData/263_GRYosterMap.c:42), checked against
        # the ROM the same way item_weights is: the magnifying
        # glass draws its disc in this colour.
        "fog": (0xF3, 0xC7, 0xA5),
        "item_weights": (0x3C, 0x28, 0x00, 0x96, 0x0E, 0x07,
                          0x08, 0x0A, 0x06, 0x0E, 0x0F, 0x08,
                          0x0C, 0x0A, 0x0C, 0x0D, 0x08, 0x08,
                          0x0A, 0x14),
        # Part B: the bird and the four Shy Guys --
        # dEFGroundYosterEffectDescs (ef/efground.c:433-669), the largest
        # table in the game at six descs. `HeihoFruitSlow` is the one
        # identity with TWO DObjDesc blocks, L and R, sharing one MObjSub
        # and one AnimJoint between them -- so two packs off two trees,
        # not one pack spawned twice, and they are the two descs that take
        # the `yaw` update proc (efGroundUpdateEffectYaw).
        #
        # Their params are also the only ones in the game whose `lr` is
        # outside -1/+1: 3*-1 and 3*+1. That is a FLAG riding in the sign
        # field, not a bigger direction -- efGroundMakeEffect sets
        # `lr_bool` from `lr == +-3` (efground.c:1398), which makes
        # dcGroundSetupEffectDObjs give a marked DObj XObj kind 0x2E
        # rather than 0x48, and then the spawn normalises the value back
        # to +-1 (efground.c:1482-1490). Carried through as the signed
        # number it is; both readers are the decomp's own code.
        "ground_actors": {
            "packs": {
                "Bird": {"desc": "Bird", "mobjsub": "Bird",
                         "matanim": "Bird"},
                "HeihoFruitSlowL": {"desc": "HeihoFruitSlowL",
                                    "mobjsub": "HeihoFruitSlow",
                                    "matanim": "HeihoFruitSlow"},
                "HeihoFruitSlowR": {"desc": "HeihoFruitSlowR",
                                    "mobjsub": "HeihoFruitSlow",
                                    "matanim": "HeihoFruitSlow"},
                "HeihoSlow": {"desc": "HeihoSlow", "mobjsub": "HeihoSlow",
                              "matanim": "HeihoSlow"},
                "HeihoFruitFast": {"desc": "HeihoFruitFast",
                                   "mobjsub": "HeihoFruitFast",
                                   "matanim": "HeihoFruitFast"},
                "HeihoFast": {"desc": "HeihoFast", "mobjsub": "HeihoFast",
                              "matanim": "HeihoFast"},
            },
            "descs": [
                {"pack": "Bird", "anim": "Bird", "dl_link": 4,
                 "update_kind": "common", "alt_high": 3000.0,
                 "alt_low": 500.0, "pos_z": -6000.0, "scale": 3.0,
                 "effect_status": -1},
                {"pack": "HeihoFruitSlowL", "anim": "HeihoFruitSlow",
                 "dl_link": 4, "update_kind": "yaw", "alt_high": 3000.0,
                 "alt_low": 500.0, "pos_z": -3000.0, "scale": 2.0,
                 "effect_status": -1},
                {"pack": "HeihoFruitSlowR", "anim": "HeihoFruitSlow",
                 "dl_link": 4, "update_kind": "yaw", "alt_high": 3000.0,
                 "alt_low": 500.0, "pos_z": -3000.0, "scale": 2.0,
                 "effect_status": -1},
                {"pack": "HeihoSlow", "anim": "HeihoSlow", "dl_link": 4,
                 "update_kind": "common", "alt_high": 3000.0,
                 "alt_low": 500.0, "pos_z": -3000.0, "scale": 2.0,
                 "effect_status": -1},
                {"pack": "HeihoFruitFast", "anim": "HeihoFruitFast",
                 "dl_link": 4, "update_kind": "common", "alt_high": 3000.0,
                 "alt_low": 500.0, "pos_z": -2000.0, "scale": 2.0,
                 "effect_status": -1},
                {"pack": "HeihoFast", "anim": "HeihoFast", "dl_link": 4,
                 "update_kind": "common", "alt_high": 3000.0,
                 "alt_low": 500.0, "pos_z": -3000.0, "scale": 2.0,
                 "effect_status": -1},
            ],
            # dEFGroundYosterParams (efground.c)
            "params": [
                {"effect_id": 3, "make_queue": 0, "lr": 0, "weight": 2},
                {"effect_id": 3, "make_queue": 1, "lr": 0, "weight": 1},
                {"effect_id": 1, "make_queue": 0, "lr": -3, "weight": 1},
                {"effect_id": 2, "make_queue": 0, "lr": +3, "weight": 1},
                {"effect_id": 4, "make_queue": 0, "lr": 0, "weight": 1},
                {"effect_id": 5, "make_queue": 0, "lr": 0, "weight": 1},
                {"effect_id": 0, "make_queue": 0, "lr": 0, "weight": 4},
            ],
        },
    },
    "Pupupu": {
        "map_file": 255,
        "cam": (4000, -2000, 3900, -3900),
        "map": (8300, -3500, 9000, -9000),
        "alt_warning": -2900,
        "bgm": 0,
        # Whispy Woods. FOUR objects, and this is why map_object is a
        # list: grPupupuInitAll builds one GObj per DObjDesc block of the
        # one map file -- the Whispy/eyes tree (`MapHead`, which is also
        # what map_nodes names), the mouth, the flowers behind and the
        # flowers in front -- on two different display layers (0 and 3).
        # The eyes' and the mouth's materials are ANIMATED, so each
        # carries its MatAnimJoints as `alt`s and the stage's own state
        # machine picks one with stage_map_set_anim: two for the eyes
        # (left/right turn), eight for the mouth (left/right x
        # stretch/turn/open/close). The flowers carry no material at all.
        #
        # The `Texture` entries are the decomp's own oddity: the mouth's
        # and eyes' texture swaps are `Texture` blocks fed to
        # gcAddAnimJointAll, and two of them share a name with an
        # AnimJoint in the same group, so they are written as a pair and
        # carried as `name + kind` -- the decomp's own symbol.
        "map_object": [
            {
                "desc": "WhispyEyesTransformKinds",
                "mobjsub": "WhispyEyesTransformKinds",
                "matanim": ["WhispyEyesLeftTurn", "WhispyEyesRightTurn"],
                "anims": ["WhispyEyesLeftTurn", "WhispyEyesLeftBlink",
                          "WhispyEyesRightTurn", "WhispyEyesRightBlink"],
            },
            {
                "desc": "WhispyMouthTransformKinds",
                "mobjsub": "WhispyMouthTransformKinds",
                "matanim": ["WhispyMouthLeftStretch",
                            "WhispyMouthLeftTurn",
                            "WhispyMouthLeftOpen",
                            "WhispyMouthLeftClose",
                            "WhispyMouthRightStretch",
                            "WhispyMouthRightTurn",
                            "WhispyMouthRightOpen",
                            "WhispyMouthRightClose"],
                "anims": ["WhispyMouthLeftStretch", "WhispyMouthLeftTurn",
                          "WhispyMouthLeftOpen", "WhispyMouthLeftClose",
                          "WhispyMouthRightStretch", "WhispyMouthRightTurn",
                          "WhispyMouthRightOpen", "WhispyMouthRightClose"],
            },
            {
                "desc": "FlowersBackTransformKinds",
                "anims": [["Texture", "WhispyMouthLeftOpen"],
                          ["Texture", "WhispyMouthLeftBlow"],
                          ["Texture", "WhispyMouthLeftClose"],
                          ["Texture", "WhispyMouthRightOpen"],
                          ["Texture", "WhispyMouthRightBlow"],
                          ["Texture", "WhispyMouthRightClose"]],
            },
            {
                "desc": "FlowersFrontTransformKinds",
                "anims": [["Texture", "WhispyEyesLeft0"],
                          ["Texture", "WhispyEyesLeft1"],
                          ["Texture", "WhispyEyesLeft2"],
                          ["Texture", "WhispyEyesRight0"],
                          ["Texture", "WhispyEyesRight1"],
                          ["Texture", "WhispyEyesRight2"]],
            },
        ],
        # dGRPupupuMap_item_weights, which read_item_weights checks
        # against the ROM's own copy. The decomp's file splits this table
        # on REGION_JP (relocData/255_GRPupupuMap.c:20 is the JP one);
        # these are the `#else` arm's values, which is what the US ROM
        # carries -- the check is what caught the first draft using the
        # JP table.
        # MPGroundData.fog_color (relocData/255_GRPupupuMap.c:37), checked against
        # the ROM the same way item_weights is: the magnifying
        # glass draws its disc in this colour.
        "fog": (0x6E, 0xD2, 0xFF),
        "item_weights": (0x46, 0x28, 0x78, 0x00, 0x14, 0x04,
                         0x06, 0x0E, 0x07, 0x0C, 0x16, 0x07,
                         0x0A, 0x07, 0x0A, 0x09, 0x0E, 0x05,
                         0x05, 0x14),
        # Part B: the Bronto Burt and King Dedede that cross behind Whispy
        # -- dEFGroundPupupuEffectDescs (ef/efground.c:671-829). Each
        # identity has ONE DObjDesc and two descs off it, differing only
        # in the AnimJoint they play: Bronto L/R (which also have a
        # MatAnimJoint each, not carried -- see the GRA1 gap) and Dedede
        # Far/Near, which have neither MObjSub nor MatAnimJoint and so
        # bake exactly like Castle's Lakitu.
        "ground_actors": {
            "packs": {
                # the one actor in the game with TWO MatAnimJoints over
                # one tree -- a left-facing and a right-facing chew --
                # which is what the pack's alt_count is for, and the two
                # descs below name which each plays.
                "Bronto": {"desc": "Bronto", "mobjsub": "Bronto",
                           "matanim": ["BrontoL", "BrontoR"]},
                "Dedede": {"desc": "Dedede"},
            },
            "descs": [
                {"pack": "Bronto", "anim": "BrontoL", "matanim": "BrontoL",
                 "dl_link": 4,
                 "update_kind": "common", "alt_high": 2000.0,
                 "alt_low": -300.0, "pos_z": -4000.0, "scale": 1.4,
                 "effect_status": -1},
                {"pack": "Bronto", "anim": "BrontoR", "matanim": "BrontoR",
                 "dl_link": 4,
                 "update_kind": "common", "alt_high": 1500.0,
                 "alt_low": -300.0, "pos_z": -3000.0, "scale": 1.4,
                 "effect_status": -1},
                {"pack": "Dedede", "anim": "DededeFar", "dl_link": 4,
                 "update_kind": "common", "alt_high": 3000.0,
                 "alt_low": 1000.0, "pos_z": -5000.0, "scale": 2.0,
                 "effect_status": -1},
                {"pack": "Dedede", "anim": "DededeNear", "dl_link": 4,
                 "update_kind": "common", "alt_high": 3000.0,
                 "alt_low": 1000.0, "pos_z": -5000.0, "scale": 3.0,
                 "effect_status": -1},
            ],
            # dEFGroundPupupuParams (efground.c)
            "params": [
                {"effect_id": 0, "make_queue": 0, "lr": 0, "weight": 2},
                {"effect_id": 0, "make_queue": 1, "lr": 0, "weight": 1},
                {"effect_id": 1, "make_queue": 0, "lr": 0, "weight": 2},
                {"effect_id": 1, "make_queue": 1, "lr": 0, "weight": 1},
                {"effect_id": 2, "make_queue": 0, "lr": 0, "weight": 1},
                {"effect_id": 3, "make_queue": 0, "lr": 0, "weight": 1},
            ],
        },
    },
    "Yamabuki": {
        "map_file": 264,
        "cam": (5000, -2500, 5700, -5700),
        "map": (9000, -6000, 10000, -10000),
        "alt_warning": -2900,
        "bgm": 7,
        # Saffron City's GATE: the building the Pokémon come out of.
        # `MPGroundData.map_nodes` is `MapHead` (0x8A0 of reloc file 160,
        # StageYamabukiFile4), a real DObjDesc tree, and the game builds
        # it with gcSetupCustomDObjs and draws it through
        # gcDrawDObjTreeDLLinksForGObj -- so the tree's display lists are
        # DL LINKS, the shape Kongo Jungle's cannon does not have and
        # Mushroom Kingdom's platforms do (grinishie.c's own `dl`
        # object). Its two scripts are the door opening and closing.
        "map_object": {
            "desc": "MapHead",
            "dl_links": True,
            "anims": ["GateOpen", "GateClose"],
        },
        # The Chansies, Charmanders and the rest are the GATE's, three of
        # them reach their animations by offset with no name in the map
        # group, and the Gate's own scripts are stage-level: none of them
        # belongs to an object the pack bakes. See add_anims.
        # ... and the file each offset is in, which is NOT the Gate's
        # (160): `attr->anim_joints` relocates into StageYamabukiFile3,
        # the same file the monsters' models come from.
        "stage_anims": [("AnimJoint", "GLuckyAppear", (159, 0x03F8), 1),
                        ("AnimJoint", "PorygonAppear", (159, 0x0F38), 1),
                        ("AnimJoint", "MarumineAppear", (159, 0x0828), 1),
                        ("AnimJoint", "HitokageAppear", (159, 0x1A28), 1),
                        ("AnimJoint", "FushigibanaAppear", (159, 0x23D8), 1)],
        # dGRYamabukiMap_item_weights (relocData/264_GRYamabukiMap.c:36,
        # the `#else` arm -- read_item_weights asserts it against the ROM).
        # MPGroundData.fog_color (relocData/264_GRYamabukiMap.c:63), checked against
        # the ROM the same way item_weights is: the magnifying
        # glass draws its disc in this colour.
        "fog": (0xCD, 0xE6, 0xFF),
        "item_weights": (0x64, 0x28, 0x50, 0x00, 0x0C, 0x06, 0x08, 0x0A,
                         0x08, 0x0C, 0x09, 0x0D, 0x0A, 0x0A, 0x11, 0x0A,
                         0x12, 0x04, 0x08, 0x28),
        # Part B: the Pokemon that fly past Silph Co. --
        # dEFGroundYamabukiEffectDescs (ef/efground.c:831-989). Four
        # identities, one desc each, every one with its own MObjSub (the
        # MatAnimJoints beside them are the GRA1 gap). Note `Fire` is a
        # Pokemon (Charizard's flight) and not a hazard: it goes through
        # the same common update as the other three, at weight 1 against
        # Onidrill's 10.
        "ground_actors": {
            "packs": {
                "Butterfree": {"desc": "Butterfree",
                               "mobjsub": "Butterfree",
                               "matanim": "Butterfree"},
                "Fire": {"desc": "Fire", "mobjsub": "Fire",
                         "matanim": "Fire"},
                "Onidrill": {"desc": "Onidrill", "mobjsub": "Onidrill",
                             "matanim": "Onidrill"},
                "Poppo": {"desc": "Poppo", "mobjsub": "Poppo",
                          "matanim": "Poppo"},
            },
            "descs": [
                {"pack": "Butterfree", "anim": "Butterfree", "dl_link": 4,
                 "update_kind": "common", "alt_high": 2000.0,
                 "alt_low": -300.0, "pos_z": -4000.0, "scale": 1.4,
                 "effect_status": -1},
                {"pack": "Fire", "anim": "Fire", "dl_link": 4,
                 "update_kind": "common", "alt_high": 3000.0,
                 "alt_low": 1000.0, "pos_z": -5000.0, "scale": 2.0,
                 "effect_status": -1},
                {"pack": "Onidrill", "anim": "Onidrill", "dl_link": 4,
                 "update_kind": "common", "alt_high": 2000.0,
                 "alt_low": -300.0, "pos_z": -4000.0, "scale": 1.4,
                 "effect_status": -1},
                {"pack": "Poppo", "anim": "Poppo", "dl_link": 4,
                 "update_kind": "common", "alt_high": 3000.0,
                 "alt_low": 1000.0, "pos_z": -5000.0, "scale": 2.0,
                 "effect_status": -1},
            ],
            # dEFGroundYamabukiParams (efground.c)
            "params": [
                {"effect_id": 0, "make_queue": 0, "lr": 0, "weight": 6},
                {"effect_id": 0, "make_queue": 1, "lr": 0, "weight": 6},
                {"effect_id": 3, "make_queue": 0, "lr": 0, "weight": 8},
                {"effect_id": 3, "make_queue": 1, "lr": 0, "weight": 8},
                {"effect_id": 2, "make_queue": 0, "lr": 0, "weight": 10},
                {"effect_id": 1, "make_queue": 0, "lr": 0, "weight": 1},
            ],
        },
    },
    "Inishie": {
        "map_file": 260,
        "cam": (3000, -1000, 3400, -3400),
        "map": (9000, -4350, 7400, -7400),
        "alt_warning": -2200,
        "bgm": 2,
        # Mushroom Kingdom: the scales. TWO objects, and the pair is the
        # reason the port needs a third object shape.
        #
        # `MPGroundData.map_nodes` points at `MapHead` (0x5F0) and that
        # block is a DISPLAY LIST, not a DObjDesc tree -- its first words
        # are `E7000000 D9FDFFFF FC121824`, RDP pipe-sync and
        # SetTextureImage. The game uses it exactly that way:
        # `gcAddDObjForGObj(gobj, map_head + &llGRInishieMapMapHead)`
        # (grinishie.c:372) is objman.c:1389, which sets
        # `new_dobj->dv = dvar` -- an empty DObj carrying a DL, one per
        # platform. So `dl` is a map object with no tree of its own, and
        # object 0 has to be it, because object 0 is what map_nodes names.
        #
        # The scale's own tree is beside it (`DObjDesc Scale`, 0x380), a
        # real five-joint DObjDesc tree with no display list at all --
        # grinishie.c:354 reads it only for its joints' translates, which
        # are the two strings' lengths. It is the object the game builds
        # FIRST and this table builds second, which is the one place the
        # two orders disagree; the port's own calls name their indices, so
        # nothing reads object 0 as "the tree".
        #
        # The retract script is the PLATFORM's, not the tree's:
        # grinishie.c:260 is `gcAddDObjAnimJoint(scale[i].platform_dobj,
        # map_head + &llGRInishieMapScaleRetractAnimJoint, 0.0F)`.
        "map_object": [
            {"dl": "MapHead", "anims": ["ScaleRetract"]},
            {"desc": "Scale"},
        ],
        # dGRInishieMap_item_weights. The decomp's file splits this table
        # on REGION_JP (relocData/260_GRInishieMap.c:24 is the JP one);
        # these are the `#else` arm's, which is what read_item_weights'
        # assert against the ROM confirms.
        # MPGroundData.fog_color (relocData/260_GRInishieMap.c:52), checked against
        # the ROM the same way item_weights is: the magnifying
        # glass draws its disc in this colour.
        "fog": (0x71, 0x88, 0xB8),
        "item_weights": (0x50, 0x28, 0x78, 0x00, 0x0A, 0x05,
                         0x0A, 0x0A, 0x05, 0x08, 0x08, 0x07,
                         0x14, 0x12, 0x08, 0x0E, 0x06, 0x19,
                         0x0A, 0x12),
        # The `?` block's own script, which belongs to no map object: its
        # TREE is the stage model file's (StageInishieFile3's DObjDesc at
        # 0x11F8, which is what `DataStart PowerBlock` names), and the
        # item's own code reaches the script by an offset arithmetic --
        # `itGetPData(ip, &llGRInishieMapPowerBlockDataStart,
        # &llGRInishieMapPowerBlockAnimJoint)` -- that the port replaces
        # with a name: stage_map_anim("PowerBlock"). See add_anims'
        # comment.
        # ... and the PIRANHA PLANT's three, which are the `?` block's
        # shape twice over: an appear AnimJoint the item attaches with
        # gcAddDObjAnimJoint, and two MatAnimJoints it hangs on its own
        # MObj with gcAddMObjMatAnimJoint -- the undamaged and damaged
        # palettes. A pair names them `name + kind`, which is what
        # separates `AnimJoint PakkunAppear` from `MatAnimJoint
        # PakkunAppear`: two blocks, one name, and the pack's lookup is
        # by name.
        "stage_anims": ["PowerBlock",
                        ("AnimJoint", "PowerBlockIdle", 0x13B8),
                        ("AnimJoint", "PakkunAppear"),
                        ("MatAnimJoint", "PakkunAppear"),
                        ("MatAnimJoint", "PakkunDamaged")],
        # `GRAttackColl PowerBlock` (0xBC), the descriptor the `?` block
        # hands ftMainCheckAddGroundHazard's callback when it registers
        # itself as a hazard -- `Stage.attack_coll`, which
        # grInishieMakePowerBlock reads in place of the decomp's
        # `map_head + &llGRInishieMapPowerBlockGRAttackColl`.
        "attack_coll": "PowerBlock",
    },
    # Final Destination, the Master Hand rung's stage, and the first of
    # the port's stages that is not a VS stage (nGRKindLast is past
    # nGRKindBattleEnd). It is also the plainest map in the game: no
    # item weights -- the map's own pointer is NULL, the rung allows no
    # items -- no map_nodes, no ground actors, no stage logic at all.
    # There is no gr/grcommon/grlast.c; the whole of Final Destination
    # that is not this map is sc/sc1pmode/sc1pgameboss.c's background,
    # which lives in the SAME reloc file (114) and is a separate step.
    # Break the Targets, one course per fighter. The
    # twelve are the first entries here that no menu can reach: only the
    # 1P ladder's two bonus rungs and the bonus-practice select play
    # them, the way Final Destination above is the Master Hand rung's
    # alone.
    #
    # They are the simplest stages in the game and all twelve are the
    # same shape, which is why they go in as one step: a MapHeader at
    # 0x0 (no stage logic file in front of it -- there is no
    # gr/grcommon/grbonus1.c, the same reason "Last" sits at 0x0), two
    # layers, one AnimJoint on layer 1, no map_nodes, no item weights
    # and no ground actors. Every one shares nSYAudioBGM1PBonusStage and
    # the same wallpaper, relocData file 119 Bonus1CommonBackground.
    #
    # alt_warning is -32768 on all of them, which is the game's "never":
    # a course has no blast line to warn about, you simply fall off it.
    #
    # The scalars below were read off the ROM and then checked line for
    # line against ssb-decomp-re/src/relocData/271_GRBonus1MarioMap.c and
    # its eleven neighbours, which is the same contract every stage above
    # has -- read_ground raises if the ROM and these disagree.
    #
    # What is NOT here yet: the targets. Each course's Targets DObjDesc
    # and AnimJoint live in its map file and sc1pbonusstage.c reaches
    # them by reloc label (dSC1PBonusStageTargetDescs), so they need a
    # block of their own in the pack the way Final Destination's
    # boss_wallpaper does. That is not done here; this entry is the
    # course itself, which a VS battle can stand a fighter on and prove.
    "Bonus1Mario": {
        "map_file": 271,
        "cam": (5000, -5000, 5000, -5000),
        "map": (9600, -9600, 9600, -9600),
        "alt_warning": -32768,
        "bgm": 26,
        "fog": (0x4B, 0xC2, 0xF4),
        "targets": {"desc": "Targets", "anim": "Targets"},
    },
    "Bonus1Fox": {
        "map_file": 272,
        "cam": (4700, -5000, 4700, -4760),
        "map": (9300, -9600, 9300, -9340),
        "alt_warning": -32768,
        "bgm": 26,
        "fog": (0x4B, 0xC2, 0xF4),
        "targets": {"desc": "Targets", "anim": "Targets"},
    },
    "Bonus1Donkey": {
        "map_file": 273,
        "cam": (6100, -1700, 2510, -2600),
        "map": (10000, -6300, 7100, -7300),
        "alt_warning": -32768,
        "bgm": 26,
        "fog": (0x4B, 0xC2, 0xF4),
        "targets": {"desc": "Targets", "anim": "Targets"},
    },
    "Bonus1Samus": {
        "map_file": 274,
        "cam": (4790, -3800, 3680, -3680),
        "map": (9600, -8000, 8800, -8800),
        "alt_warning": -32768,
        "bgm": 26,
        "fog": (0x4B, 0xC2, 0xF4),
        "targets": {"desc": "Targets", "anim": "Targets"},
    },
    "Bonus1Luigi": {
        "map_file": 275,
        "cam": (5000, -4000, 3800, -3800),
        "map": (9600, -8600, 8400, -8400),
        "alt_warning": -32768,
        "bgm": 26,
        "fog": (0x4B, 0xC2, 0xF4),
        "targets": {"desc": "Targets", "anim": "Targets"},
    },
    "Bonus1Link": {
        "map_file": 276,
        "cam": (5000, -5000, 2600, -2900),
        "map": (9600, -9600, 7200, -7500),
        "alt_warning": -32768,
        "bgm": 26,
        "fog": (0x4B, 0xC2, 0xF4),
        "targets": {"desc": "Targets", "anim": "Targets"},
    },
    "Bonus1Yoshi": {
        "map_file": 277,
        "cam": (5000, -5000, 5600, -5000),
        "map": (9600, -9600, 10200, -9600),
        "alt_warning": -32768,
        "bgm": 26,
        "fog": (0x4B, 0xC2, 0xF4),
        "targets": {"desc": "Targets", "anim": "Targets"},
    },
    "Bonus1Captain": {
        "map_file": 278,
        "cam": (5000, -5000, 5000, -5000),
        "map": (9600, -9600, 9600, -9600),
        "alt_warning": -32768,
        "bgm": 26,
        "fog": (0x4B, 0xC2, 0xF4),
        "targets": {"desc": "Targets", "anim": "Targets"},
    },
    "Bonus1Kirby": {
        "map_file": 279,
        "cam": (4730, -3700, 5610, -4790),
        "map": (9600, -8300, 8210, -9390),
        "alt_warning": -32768,
        "bgm": 26,
        "fog": (0x4B, 0xC2, 0xF4),
        "targets": {"desc": "Targets", "anim": "Targets"},
    },
    "Bonus1Pikachu": {
        "map_file": 280,
        "cam": (4670, -4820, 4670, -4760),
        "map": (8670, -9420, 9270, -9360),
        "alt_warning": -32768,
        "bgm": 26,
        "fog": (0x4B, 0xC2, 0xF4),
        "targets": {"desc": "Targets", "anim": "Targets"},
    },
    "Bonus1Purin": {
        "map_file": 281,
        "cam": (4940, -4700, 4640, -4850),
        "map": (9540, -9300, 9240, -9450),
        "alt_warning": -32768,
        "bgm": 26,
        "fog": (0x4B, 0xC2, 0xF4),
        "targets": {"desc": "Targets", "anim": "Targets"},
    },
    "Bonus1Ness": {
        "map_file": 282,
        "cam": (4550, -4400, 4350, -4330),
        "map": (9150, -9000, 9450, -9420),
        "alt_warning": -32768,
        "bgm": 26,
        "fog": (0x4B, 0xC2, 0xF4),
        "targets": {"desc": "Targets", "anim": "Targets"},
    },
    # Board the Platforms, one course per fighter. The
    # same shape and the same step as the twelve Break the Targets
    # courses above -- header at 0x0, two layers, one AnimJoint on layer
    # 1, no item weights, no ground actors, one shared wallpaper
    # (relocData 136 Bonus2Common) and nSYAudioBGM1PBonusStage -- and
    # read and cross-checked the same way, against
    # ssb-decomp-re/src/relocData/283_GRBonus2MarioMap.c and its eleven
    # neighbours.
    #
    # Five of the twelve also carry a Bumpers DObjDesc + AnimJoint of
    # their own (Fox, Samus, Kirby, Purin, Ness), which sc1pbonusstage.c
    # reaches by reloc label the way it reaches a course's targets, and
    # they ship as BMP1 -- the same block BTG1 is, one magic apart.
    #
    # What is NOT here, and what makes this the courses and not the whole
    # bonus stage: the PLATFORMS, which are not per-course at all.
    # dSC1PBonusStagePlatformDescs is four columns of pointer-to-pointer
    # into the shared Bonus2Common file (relocData 136), a different
    # shape again and a step of its own; this one is the ground a
    # fighter stands on.
    "Bonus2Mario": {
        "map_file": 283,
        "cam": (5000, -5000, 5000, -5000),
        "map": (9600, -9600, 9600, -9600),
        "alt_warning": -32768,
        "bgm": 26,
        "fog": (0x4B, 0xC2, 0xF4),
    },
    "Bonus2Fox": {
        "map_file": 284,
        "cam": (7000, -5000, 5000, -5000),
        "map": (11600, -9600, 9600, -9600),
        "alt_warning": -32768,
        "bgm": 26,
        "fog": (0x4B, 0xC2, 0xF4),
        "bumpers": {"desc": "Bumpers", "anim": "Bumpers"},
    },
    "Bonus2Donkey": {
        "map_file": 285,
        "cam": (5000, -5000, 5000, -5000),
        "map": (9600, -9600, 9600, -9600),
        "alt_warning": -32768,
        "bgm": 26,
        "fog": (0x4B, 0xC2, 0xF4),
    },
    "Bonus2Samus": {
        "map_file": 286,
        "cam": (5000, -4400, 4700, -4325),
        "map": (9600, -9000, 9300, -8925),
        "alt_warning": -32768,
        "bgm": 26,
        "fog": (0x4B, 0xC2, 0xF4),
        "bumpers": {"desc": "Bumpers", "anim": "Bumpers"},
    },
    "Bonus2Luigi": {
        "map_file": 287,
        "cam": (5000, -5000, 5000, -5000),
        "map": (11000, -9600, 9600, -9600),
        "alt_warning": -32768,
        "bgm": 26,
        "fog": (0x4B, 0xC2, 0xF4),
    },
    "Bonus2Link": {
        "map_file": 288,
        "cam": (5000, -4450, 5000, -5000),
        "map": (9600, -9150, 9600, -9600),
        "alt_warning": -32768,
        "bgm": 26,
        "fog": (0x4B, 0xC2, 0xF4),
    },
    "Bonus2Yoshi": {
        "map_file": 289,
        "cam": (5000, -5000, 5000, -5000),
        "map": (9600, -9600, 9600, -9600),
        "alt_warning": -32768,
        "bgm": 26,
        "fog": (0x4B, 0xC2, 0xF4),
    },
    "Bonus2Captain": {
        "map_file": 290,
        "cam": (5000, -5000, 5000, -5000),
        "map": (11000, -9600, 9600, -9600),
        "alt_warning": -32768,
        "bgm": 26,
        "fog": (0x4B, 0xC2, 0xF4),
    },
    "Bonus2Kirby": {
        "map_file": 291,
        "cam": (5000, -6400, 5000, -5000),
        "map": (9600, -11000, 9600, -9600),
        "alt_warning": -32768,
        "bgm": 26,
        "fog": (0x4B, 0xC2, 0xF4),
        "bumpers": {"desc": "Bumpers", "anim": "Bumpers"},
    },
    "Bonus2Pikachu": {
        "map_file": 292,
        "cam": (5000, -5000, 5000, -5000),
        "map": (9600, -9600, 9600, -9600),
        "alt_warning": -32768,
        "bgm": 26,
        "fog": (0x4B, 0xC2, 0xF4),
    },
    "Bonus2Purin": {
        "map_file": 293,
        "cam": (5000, -5000, 5000, -5000),
        "map": (9600, -9600, 9600, -9600),
        "alt_warning": -32768,
        "bgm": 26,
        "fog": (0x4B, 0xC2, 0xF4),
        "bumpers": {"desc": "Bumpers", "anim": "Bumpers"},
    },
    "Bonus2Ness": {
        "map_file": 294,
        "cam": (7000, -5000, 5000, -5000),
        "map": (11600, -9600, 9600, -9600),
        "alt_warning": -32768,
        "bgm": 26,
        "fog": (0x4B, 0xC2, 0xF4),
        "bumpers": {"desc": "Bumpers", "anim": "Bumpers"},
    },
    # Race to the Finish, the third bonus stage and the
    # only one that is ONE course rather than twelve. No wallpaper at
    # all (MPGroundData.wallpaper is NULL and fog_alpha 0, which no
    # other stage in the game is) and only layer 1, so the pack is the
    # ground and nothing else. Read off the ROM and checked against
    # ssb-decomp-re/src/relocData/295_GRBonus3Map.c.
    #
    # Its bumpers are NOT in the layer file the other courses' are:
    # grBonus3MakeBumpers reaches them off `map_head`, which is
    # `map_nodes - llGRBonus3MapMapHead` and so is relocData 162
    # (GRBonus3File3), not 149. "file" says so.
    "Bonus3": {
        "map_file": 295,
        "cam": (7500, -7500, 7500, -7500),
        "map": (8000, -8000, 8000, -8000),
        "alt_warning": -32768,
        "bgm": 26,
        "fog": (0x00, 0x00, 0x00),
        "bumpers": {"desc": "Bumpers", "anim": "Bumpers", "file": 162},
    },
    # The three stages only the 1P ladder plays, the last
    # of the game's stages the port had not exported. Like "Last" above
    # they are unreachable from the stage select on purpose, and like it
    # they are ordinary stages in every other way: one map file, one
    # wallpaper, the layers the map header names. None of the three has
    # a gr/grcommon/gr*.c of its own -- no stage logic, no moving ground
    # -- so these entries are the whole of each one but the background.
    #
    # Every scalar was read off the ROM and then checked against
    # relocData/268_GRZakoMap.c, 269_GRMetalMap.c and
    # 270_GRYosterSmallMap.c, which is the contract read_ground enforces.
    #
    # The Duel Zone, the Fighting Polygon Team's rung (12). Its map header
    # names a DObjDesc on layer 1 alone and NULL map_nodes; the spawn
    # positions sc1PGameGetStartPosition asks for -- map object kind 38
    # -- are in the collision geometry, not in map_nodes.
    "Zako": {
        "map_file": 268,
        "cam": (3000, -2000, 4000, -4000),
        "map": (9000, -4200, 9000, -9000),
        "alt_warning": -2900,
        "bgm": 36,                      # nSYAudioBGMZako
        "fog": (0x41, 0x00, 0x69),
        # dGRZakoMap_item_weights (relocData/268_GRZakoMap.c:16), the
        # US row -- read_item_weights checks it against the ROM.
        "item_weights": (0x46, 0x28, 0x78, 0x00, 0x1B, 0x09,
                         0x07, 0x0A, 0x05, 0x0C, 0x16, 0x08,
                         0x0A, 0x07, 0x0A, 0x0C, 0x0A, 0x03,
                         0x08, 0x1E),
    },
    # Meta Crystal, the Metal Mario rung (10). Four layers, each with an
    # MObjSub and a MatAnimJoint.
    "Metal": {
        "map_file": 269,
        "cam": (5000, -2400, 5000, -5000),
        "map": (9000, -4200, 10000, -10000),
        "alt_warning": -2900,
        "bgm": 37,                      # nSYAudioBGMMetal
        "fog": (0x00, 0x00, 0x0A),
        # dGRMetalMap_item_weights (relocData/269_GRMetalMap.c:25)
        "item_weights": (0x3C, 0x28, 0x78, 0x00, 0x01, 0x00,
                         0x06, 0x0A, 0x05, 0x0C, 0x16, 0x08,
                         0x0A, 0x07, 0x0A, 0x0A, 0x0A, 0x05,
                         0x05, 0x12),
    },
    # Yoshi's Island, small, the Yoshi Team rung (1). The same wallpaper
    # and bgm as the VS Yoster above, a map of its own, and an AnimJoint
    # on layer 0.
    "YosterSmall": {
        "map_file": 270,
        "cam": (4100, -2000, 5500, -5500),
        "map": (8000, -4000, 7000, -7000),
        "alt_warning": -2500,
        "bgm": 8,                       # nSYAudioBGMYoster
        "fog": (0xF3, 0xC7, 0xA5),
        # dGRYosterSmallMap_item_weights (relocData/270_GRYosterSmall-
        # Map.c:20) -- the same twenty bytes as the VS Yoster above.
        "item_weights": (0x3C, 0x28, 0x00, 0x96, 0x0E, 0x07,
                         0x08, 0x0A, 0x06, 0x0E, 0x0F, 0x08,
                         0x0C, 0x0A, 0x0C, 0x0D, 0x08, 0x08,
                         0x0A, 0x14),
    },
    "Last": {
        "map_file": 266,
        "cam": (5000, -2400, 5500, -5500),
        "map": (9000, -4200, 11000, -11000),
        "alt_warning": -2900,
        # nSYAudioBGMBossEntry, "Master Hand Appears" (gm/gmsound.h:56).
        # The rung's OTHER track, nSYAudioBGMLast, is handed to
        # gMPCollisionBGMDefault by sc1pgame.c -- see src/dc/sndres.h's
        # SNDRES_GROUP_1PGAME note.
        "bgm": 24,
        # MPGroundData.fog_color (relocData/266_GRLastMap.c:27).
        "fog": (0x00, 0x00, 0x32),
        # sc1pgameboss.c's dSC1PGameBossEffects0..3 / dSC1PGameBossAnims0..3
        # (lines 25-240), which pair a DObjDesc+MObjSub with an
        # AnimJoint+MatAnimJoint. Effects3_0 is not listed: it is the SAME
        # DObjDesc and MObjSub as Effects2_1, so the two rows share one
        # pack and Anims3_0 is just its second material alternate.
        "boss_wallpaper": {
            "Effects0": {"desc": "Effects0", "mobjsub": "Effects0",
                         "anim": "Anims0", "matanim": "Anims0",
                         "dl_links": True},
            "Effects1": {"desc": "Effects1", "mobjsub": "Effects1",
                         "anim": "Anims1", "matanim": "Anims1",
                         "dl_links": True, "two_cycle": True},
            "Effects2_0": {"desc": "Effects2_0", "mobjsub": "Effects2_0",
                           "anim": "Anims2_0", "matanim": "Anims2_0",
                           "dl_links": True},
            "Effects2_1": {"desc": "Effects2_1", "mobjsub": "Effects2_1",
                           "matanim": ["Anims2_1", "Anims3_0"]},
            "Effects3_1": {"desc": "Effects3_1", "anim": "Anims3_1",
                           "dl_links": True},
        },
    },
}

LINE_KINDS = ("floor", "ceil", "rwall", "lwall")

# The other-mode L word each layer's display proc leaves in each display-list
# head before it walks the tree (gr/grdisplay.c grDisplayLayer<N><Pri|Sec>-
# ProcDisplay). A joint DL that sets no render mode of its own inherits this,
# which is how three of the four layers get a bucket at all: only layers whose
# layer_mask bit is set reach the Sec proc and its second head. Indexed
# [layer][head]; heads 2 and 3 are never written by a ground display proc.
#
# G_AC_THRESHOLD rides along in bits 0-1 for the modes the DLs pair it with,
# but the seeds are the render mode alone -- the procs set no alpha compare,
# so it starts at G_AC_NONE.
LAYER_RENDERMODE = {
    False: {       # pri proc: gcDrawDObjTree, everything into head 0
        0: [A.G_RM_AA_OPA_SURF, None, None, None],
        1: [A.G_RM_AA_ZB_OPA_SURF, A.G_RM_AA_ZB_XLU_SURF, None, None],
        2: [A.G_RM_AA_OPA_SURF, None, None, None],
        3: [A.G_RM_AA_OPA_SURF, None, None, None],
    },
    True: {        # sec proc: gcDrawDObjTreeDLLinks, head per link list_id
        0: [A.G_RM_AA_OPA_SURF, A.G_RM_AA_XLU_SURF, None, None],
        1: [A.G_RM_AA_ZB_OPA_SURF, A.G_RM_AA_ZB_XLU_SURF, None, None],
        2: [A.G_RM_AA_OPA_SURF, A.G_RM_AA_XLU_SURF, None, None],
        3: [A.G_RM_AA_OPA_SURF, A.G_RM_AA_XLU_SURF, None, None],
    },
}


def be16s(f, o):
    return struct.unpack_from(">h", f, o)[0]


def be16u(f, o):
    return struct.unpack_from(">H", f, o)[0]


def be32u(f, o):
    return struct.unpack_from(">I", f, o)[0]


def read_ground(rom, stage):
    """Map header scalars + resolved layer/geometry pointers."""
    cfg = STAGES[stage]
    fid = cfg["map_file"]
    f, e, intern, sites, ids = L.file_info(rom, fid)
    ext = {}
    for (site, toff), tfid in zip(sites, ids):
        ext[site] = (tfid, toff)

    def ptr(site):
        if site in ext:
            return ext[site]
        if site in intern:
            return (fid, intern[site])
        return None

    g = ground_off(fid)
    out = {
        "layers": [ptr(g + i * 16) for i in range(4)],
        # MPGroundDesc.p_mobjsubs (mp/mptypes.h:173), the MObj chain each
        # layer's parts get from mnMapsMakeLayer's gcAddMObjAll /
        # grMainMakeLayer's: without it a layer whose display list
        # branches into segment 0xE (a texture the MObj carries) cannot
        # be baked
        "mobjsubs": [ptr(g + i * 16 + 8) for i in range(4)],
        # MPGroundDesc.anim_joints (+4) and .p_matanim_joints (+12),
        # mp/mptypes.h:169-175 -- the other two pointers of the same
        # 16-byte desc the two lines above read. grDisplayMakeGeometryLayer
        # (gr/grdisplay.c:206-215) attaches gcAddAnimAll + gcPlayAnimAll to
        # a layer iff either is non-NULL, and this function read neither
        # until now, so every stage shipped with its background frozen at
        # the bind pose. Eight of the nine have at least one animated
        # layer; Hyrule, the stage the port's DIVERGES note was written
        # against (src/dc/stage.c), is the only one that does not.
        "layer_anims": [ptr(g + i * 16 + 4) for i in range(4)],
        "layer_matanims": [ptr(g + i * 16 + 12) for i in range(4)],
        "layer_mask": f[g + 0x44],
        "fog": (f[g + 0x4C], f[g + 0x4D], f[g + 0x4E]),
        "geometry": ptr(g + 0x40),
        "cam": tuple(be16s(f, g + 0x6C + i * 2) for i in range(4)),
        "map": tuple(be16s(f, g + 0x74 + i * 2) for i in range(4)),
        "bgm": be32u(f, g + 0x7C),
        "alt_warning": be16s(f, g + 0x88),
        # SYColorRGB emblem_colors[4] (mp/mptypes.h:185): the tint the
        # HUD gives each player's emblem beside their damage
        # (if/ifcommon.c ifCommonPlayerDamageInitInterface)
        "emblem": tuple(f[g + 0x50 + i] for i in range(12)),
        # ...and a fifth, which the struct does not declare: the CPU's
        # colour is index 4 (sc1pgame.c nSCBattlePlayerColorCP,
        # mnplayersvs.c GMCOMMON_PLAYERS_MAX), so the emblem of every
        # computer player reads the first three bytes of the `unused`
        # word after the array (0xDCDCDC00 on most stages: grey)
        "emblem_cp": tuple(f[g + 0x5C + i] for i in range(3)),
        # Vec3f light_angle (mp/mptypes.h:187): the stage's light, which
        # gm/gmcamera.c's gmCameraGetAdjustAtAngle adds to the camera's
        # pitch
        "light_angle": tuple(struct.unpack_from(">3f", f, g + 0x60)),
        # MPGroundData.map_nodes (mp/mptypes.h:198): the stage's own map
        # objects -- the barrels, tornadoes and platforms its gr/ logic
        # makes GObjs out of. A pointer into whichever map file holds
        # them, which is the map logic file's neighbour and not itself.
        "map_nodes": ptr(g + 0x80),
        # MPGroundData.item_weights (mp/mptypes.h:199): the item
        # randomizer's per-kind weights, the one table that says which
        # item drops where.
        "item_weights": ptr(g + 0x84),
        # The 1P game's team-rung bounds (mp/mptypes.h:201-208): the
        # camera box and the blast box gm/gmcamera.c and
        # ftCommonDeadCheckInterruptCommon use for an is_spgame_enemy
        # fighter -- the Yoshi, Kirby and Polygon teams, penned into a
        # tighter stage than the one the 1P player can use. Then the two
        # Vec3h the bonus pause zooms between (if/ifcommon.c:2928).
        "cam_team": tuple(be16s(f, g + 0x8A + i * 2) for i in range(4)),
        "map_team": tuple(be16s(f, g + 0x92 + i * 2) for i in range(4)),
        "zoom_start": tuple(be16s(f, g + 0x9A + i * 2) for i in range(3)),
        "zoom_end": tuple(be16s(f, g + 0xA0 + i * 2) for i in range(3)),
    }
    for key in ("cam", "map", "alt_warning", "bgm"):
        if out[key] != cfg[key]:
            raise AssertionError("%s %s: read %r, decomp says %r"
                                 % (stage, key, out[key], cfg[key]))
    check_ground_typed(fid, out)
    return out


def check_ground_typed(fid, out):
    """Hold the team bounds and the zoom pair to the decomp's own typed
    MPGroundData initializer (relocData/<fid>_*Map.c), whose lines carry
    each field's name in a comment. Skipped when the decomp is not
    checked out; the ROM read above is the value either way."""
    import glob
    import re
    import ssb_paths
    hits = glob.glob(os.path.join(ssb_paths.DECOMP_DIR, "src", "relocData",
                                  "%d_*.c" % fid))
    if not hits:
        return
    text = open(hits[0]).read()

    def field(name):
        m = re.search(r"(-?\d+|\{[^}]*\}),\s*/\*\s*%s\s*\*/" % name, text)
        if m is None:
            raise AssertionError("%s: no typed %s" % (hits[0], name))
        return tuple(int(v) for v in re.findall(r"-?\d+", m.group(1)))

    want = {
        "cam_team": sum((field("camera_bound_team_" + s)
                         for s in ("top", "bottom", "right", "left")), ()),
        "map_team": sum((field("map_bound_team_" + s)
                         for s in ("top", "bottom", "right", "left")), ()),
        "zoom_start": field("zoom_start"),
        "zoom_end": field("zoom_end"),
    }
    for key, val in want.items():
        if out[key] != val:
            raise AssertionError("%s %s: read %r, decomp says %r"
                                 % (hits[0], key, out[key], val))


def read_map_blocks(fid):
    """The block entries the reloc descriptions list for a map file's
    group, in file order: (kind, name, offset).

    A stage's map file is not one file. The game links the stage's map
    logic file and its map data file into one address space, and the
    descriptions' group for the logic file lists blocks from both: the
    MPGroundData itself (MapHeader), then everything the logic file
    points at, by its address. That is why Kongo Jungle's group is
    [261] and still names a MapHead at 0xA98, which is file 158 --
    map_nodes says so, and this function's caller checks that it does.
    """
    want = "[%d]" % fid
    out, inside = [], False
    with open(DESC) as fp:
        for line in fp:
            line = line.strip()
            if line.startswith("["):
                inside = (line == want)
                continue
            if not inside or not line:
                continue
            parts = line.split()
            out.append((parts[0], parts[1], int(parts[2], 16)))
    if not out:
        raise AssertionError("no descriptions group [%d]" % fid)
    return out


def read_anim_script(f, intern, off):
    """One AObjEvent32 script, as u32 words, with the word indices that
    hold a relocated pointer and where each points.

    The words are the ROM's own, in host order: the script is opcodes and
    payloads, and the only pointer in one is SetAnim's, which is how a
    looping script names itself. The port cannot carry that value -- it
    is an address in the game's link, and a 64-bit build has no such
    address -- so it is left out of the words and recorded as a fixup
    instead, for the loader to write once the words are in memory.
    """
    words, fixups = [], []
    while True:
        if off + 4 * len(words) >= len(f):
            raise AssertionError("AnimJoint 0x%X: runs past the file after "
                                 "%d words" % (off, len(words)))
        w = be32u(f, off + 4 * len(words))
        words.append(w)
        opcode = (w >> 25) & 0x7F
        if opcode == 0x00:
            break
        n = aobj_payload_words(opcode, (w >> 15) & 0x3FF)
        if n is None:
            raise AssertionError("AnimJoint 0x%X: opcode 0x%02X at word %d"
                                 % (off, opcode, len(words) - 1))
        for i in range(n):
            p = be32u(f, off + 4 * len(words))

            if opcode in (0x01, 0x0D, 0x0E) and i == 0:
                # Jump / SetInterp / SetAnim: the word is a relocated
                # pointer, so it names another script (or, for the
                # self-loop every one of these loops with, this one).
                #
                # A NULL target -- "jump nowhere", which is how Dream
                # Land's texture scripts end -- is written two ways in
                # the ROM: a word that is not a relocated slot at all, and
                # a slot whose chain-encoded target is offset 0 (the map
                # file's own header, never a script). Both mean nothing to
                # write, so neither is recorded as a fixup -- which is what
                # lets the caller's own check mean what it says.
                t = intern.get(off + 4 * len(words))

                if t:
                    # The OPCODE rides along, because the three are not the
                    # same kind of pointer: Jump (0x01) and SetAnim (0x0E)
                    # name another SCRIPT, while SetInterp (0x0D) names an
                    # `SYInterpDesc` -- a spline's control data, which is
                    # not a script at all. The remap folds
                    # this back to a pair, so nothing downstream sees it.
                    fixups.append((len(words), t, opcode))
                p = 0
            words.append(p)
        if opcode in (0x01, 0x0E):
            # Jump and SetAnim are unconditional, so this word and its
            # target are the last of the script: whatever the ROM put
            # behind them is somebody else's and the runtime never
            # reaches it. The walk did not say so and
            # kept going, which an audit of every stage the port ships
            # put at 51 scripts: 29 swallowed one trailing End word, and
            # 22 swallowed the BLOCK BEHIND THEM whole, up to 855 words
            # of a neighbour's floats and scripts, until some word in it
            # happened to decode as End. All of it was dead weight -- the
            # runtime stops here -- but it is why the packs got smaller
            # the day this landed. Two courses' bumpers are what forced
            # the issue, being the first whose trailing data does not
            # decode at all (Purin's loop at 0x5290 is followed by the
            # floats of the block below it), and 13b's own EOF special
            # case was the same walk running off the end of
            # Bonus1Yoshi's file rather than into a neighbour. Stopping
            # here is what the runtime does and it subsumes both.
            break
    return words, fixups


def remap_anim_fixups(stage, anims, at):
    """Point every script's pointer words at the exported set's INDICES.

    A Jump/SetInterp/SetAnim word names another object by its FILE offset;
    the pack addresses entries by their index in its anim array. The key is
    (file, offset) and not the offset alone, because the target is an
    offset in the SAME file as the script that names it and two files'
    offsets can collide.
    """
    for a in anims:
        remapped = []

        for (w, t, _op) in a["fixups"]:
            if t is None or (a["fid"], t) not in at:
                raise AssertionError(
                    "%s: %s (joint %d) word %d -> 0x%X (in file %d) points "
                    "outside the exported set (have %s)"
                    % (stage, a["name"], a["joint"], w,
                       (t if t is not None else -1), a["fid"],
                       sorted(o for (fi, o) in at if fi == a["fid"])))
            remapped.append((w, at[(a["fid"], t)]))
        a["fixups"] = remapped


# `SYInterpDesc` (sys/interp.h:8), as the ROM writes it: a u8 kind, an s16
# point count, then four floats-or-pointers. The three POINTERS name float
# arrays whose lengths the point count gives -- `points` is points_num
# Vec3f, `keyframes` and `quartics` are points_num f32 each -- so the
# descriptor is self-describing and none of the three needs a length of
# its own. (The game reads exactly those three spans; bytes after them in
# the ROM belong to whatever is next.)
INTERP_POINTS_NUM = 2       # the s16 at +0x02
INTERP_PTRS = (0x08, 0x10, 0x14)     # points, keyframes, quartics
INTERP_DESC_WORDS = 6       # the 24-byte struct, as words


def read_interp_desc(f, intern, off, name):
    """One `SYInterpDesc` as PACK ENTRIES: the three float arrays, then the
    descriptor itself with a fixup per pointer.

    This is what a SetInterp word needs and what the pack could not carry
    before. The descriptor is not a script -- walking it with
    `read_anim_script` fails on its first word -- so it is read as data:
    the struct's own words, with its three pointers recorded as fixups to
    the arrays' entries, which the loader fills in exactly as it fills a
    script's Jump. The runtime then gets a real `SYInterpDesc*` at
    `aobj->interpolate`, which is what `syInterpCubic` and its kin read.

    The arrays go FIRST so the descriptor's fixups can name their indices.
    """
    kind = f[off]
    points_num = struct.unpack_from(">h", f, off + INTERP_POINTS_NUM)[0]

    if points_num < 0 or points_num > 256:
        raise AssertionError("%s at 0x%X: %d points" % (name, off, points_num))

    entries = []

    for (field_off, span, label) in ((INTERP_PTRS[0], 12, "Points"),
                                     (INTERP_PTRS[1], 4, "Keyframes"),
                                     (INTERP_PTRS[2], 4, "Quartics")):
        tgt = intern.get(off + field_off)

        if tgt is None:
            # A NULL array is the KIND saying it does not need that one,
            # not a hole: an nSYInterpKindLinear descriptor has no
            # quartic coefficients, because there are none to a straight
            # line, and four of the five Board the Platforms courses'
            # bumpers ride one. Nothing is exported for
            # it and no fixup is recorded, so the descriptor's own word
            # -- which is 0 in the ROM -- stays 0 in the pack, which is
            # what syInterpLinear reads. Only a non-zero word that is
            # not relocated is still wrong.
            if be32u(f, off + field_off):
                raise AssertionError("%s at 0x%X: %s holds 0x%08X, which is "
                                     "not a relocation"
                                     % (name, off, label,
                                        be32u(f, off + field_off)))
            continue

        nbytes = points_num * span

        if tgt + nbytes > len(f):
            raise AssertionError("%s at 0x%X: %s runs past the file"
                                 % (name, off, label))
        words = [struct.unpack_from(">I", f, tgt + 4 * i)[0]
                 for i in range(nbytes // 4)]
        entries.append({"name": "%s%s" % (name, label), "words": words,
                        "fixups": [], "off": tgt, "fid": None})

    desc_words = [struct.unpack_from(">I", f, off + 4 * i)[0]
                  for i in range(INTERP_DESC_WORDS)]
    # The fixup's middle field is the TARGET, not the site -- that is what
    # the remap looks an entry up by -- so these come from the intern table
    # the same way a script's do.
    fixups = [(INTERP_PTRS[i] // 4, intern.get(off + INTERP_PTRS[i]), 0x0D)
              for i in range(3) if intern.get(off + INTERP_PTRS[i]) is not None]
    entries.append({"name": name, "words": desc_words, "fixups": fixups,
                    "off": off, "fid": None, "kind": kind,
                    "points_num": points_num})

    return entries


def add_interp_descs(stage, anims, at, fid, f, intern):
    """Export every `SYInterpDesc` this file's scripts SetInterp to.

    A descriptor is only ever reached through a `SetInterp` word, so this
    runs AFTER the stage's own scripts are in `anims` and BEFORE the remap
    -- the fixups are still 3-tuples here and carry their opcode. The
    descriptor's own entries are added with `fid` so the remap finds them,
    and the loop repeats until nothing new appears, because one descriptor
    may be named by several scripts and its arrays are shared.
    """
    done = set()

    while True:
        want = []

        for a in anims:
            for (_w, t, op) in a["fixups"]:
                if (op == 0x0D) and (t is not None) and (t not in done):
                    want.append(t)

        if not want:
            return
        for t in want:
            done.add(t)

            if (fid, t) in at:
                continue

            # Named by its own offset in its file, because the pack's name
            # table is what a dump and stage_map_anim read and two
            # descriptors may share nothing else. Nothing looks these up at
            # RUN time -- a SetInterp word reaches its descriptor by the
            # fixup's index -- so the names are for the reader, not the game.
            name = "Interp%d_%04X" % (fid, t)

            for ent in read_interp_desc(f, intern, t, name):
                e = dict(ent)
                e["fid"] = fid

                if (fid, e["off"]) in at:
                    continue
                at[(fid, e["off"])] = len(anims)
                e["joint"] = 0
                anims.append(e)


def intern_target(off, intern):
    """A map-file block offset resolved through the file's own
    relocation: a slot gives what it points at, a block gives itself."""
    return intern.get(off, off)


def read_map_object(rom, stage, ground):
    """The stage's own map object: the tree MPGroundData.map_nodes names,
    the AnimJoint scripts and MObjSub materials the descriptions list
    beside it, and the attack descriptor its ground logic hands the
    fighter system.

    A map group names its blocks by object -- `DObjDesc Acid`,
    `MObjSub Acid`, `AnimJoint Acid`, `MatAnimJoint Acid` -- so the
    stage table's `map_object` entry names them by that, and each is
    found by name rather than by guessing which of the group's offsets
    belong together. Kongo Jungle is the exception the table has to spell
    out: its tree is the group's unnamed `MapHead` and its three scripts
    are named `TaruCann*` after the object, not `TaruCann` for all of
    them.

    Each entry's offset is resolved through the map data file's own
    relocation: an entry naming a pointer slot (Kongo Jungle's
    TaruCannDefault at 0xB20, Zebes' MObjSub at 0x8C0) gives what it
    points at, and an entry naming the block itself (TaruCannFill at
    0xB68) gives itself.
    """
    cfg = STAGES[stage].get("map_object")
    stage_anims = STAGES[stage].get("stage_anims", ())

    if cfg is None:
        # A stage with NO map object of its own can still carry scripts.
        # Sector Z's Arwing is not built out of `map_nodes`
        # at all: `grSectorInitAll` takes the tree from FOX's Special3 file
        # through `lbRelocGetForceStatusBufferFile`, and the flight
        # patterns' scripts are in the sector's own MODEL file. So there is
        # no map group here to look a name up in, and this arm REFUSES a
        # bare name rather than pretending: every entry names its own
        # `(file, offset)`, which is the only form accepted.
        if not stage_anims:
            return None

        anims = []
        at = {}
        joined = []

        for entry in stage_anims:
            if not (isinstance(entry, tuple) and len(entry) in (3, 4) and
                    isinstance(entry[2], tuple)):
                raise AssertionError(
                    "%s: a stage with no map object must name each script "
                    "as (kind, name, (file, offset), joint); got %r"
                    % (stage, entry))

            kind, name, (src_fid, raw) = entry[0], entry[1], entry[2]
            joint = entry[3] if len(entry) == 4 else 0
            pname = name + kind if kind != "AnimJoint" else name

            if pname in joined:
                continue
            joined.append(pname)

            sf, _, sintern, _, _ = L.file_info(rom, src_fid)

            if raw + 4 > len(sf):
                raise AssertionError("%s: %s at 0x%X is outside file %d "
                                     "(%d bytes)"
                                     % (stage, pname, raw, src_fid, len(sf)))
            words, fixups = read_anim_script(sf, sintern, raw)
            at[(src_fid, raw)] = len(anims)
            anims.append({"name": pname, "words": words, "fixups": fixups,
                          "off": raw, "fid": src_fid, "joint": joint})

        # ... and the `SYInterpDesc`s those scripts SetInterp to, which are
        # data rather than scripts and are read as their own entries.
        add_interp_descs(stage, anims, at,
                         stage_anims[0][2][0],
                         *L.file_info(rom, stage_anims[0][2][0])[0:3:2])

        remap_anim_fixups(stage, anims, at)

        return {"fid": None, "off": None, "anims": anims, "skipped": [],
                "objects": [], "attack": None, "graft": None}

    # One stage may build several map GObojs off its one map file: Dream
    # Land makes four (the Whispy/eyes tree, the mouth, the flowers behind
    # and the flowers in front), each from its own DObjDesc block, with its
    # own MObjSub and its own display link. `map_object` is therefore a
    # list, and a bare dict -- which is every stage before Dream Land --
    # means one object.
    cfgs = cfg if isinstance(cfg, list) else [cfg]

    target = ground["map_nodes"]
    if target is None:
        raise AssertionError("%s: map_object asked for, but map_nodes is "
                             "NULL" % stage)
    fid, off = target

    # An ANIMS-ONLY entry: the stage's `map_nodes` is an AObjEvent32*
    # ROOT rather than a DObjDesc tree, so there is nothing to bake and
    # nothing to name -- only the scripts the root points at. Peach's
    # Castle is the one stage of the nine shaped this way (see its STAGES
    # entry): the game reads `map_nodes[0]` as a single script through
    # gcAddAnimJointAll, on a GObj whose tree is one empty DObj.
    #
    # The root's SLOT is what says where the script is, and it is an
    # intern relocation -- file 156's slot 0 relocates to offset 4, the
    # script's first word, and the script's own SetAnim word relocates
    # back to 4. So this reads the same two things read_anim_block does
    # for a named block, from a table that is not in any map group: file
    # 156 has no descriptions entry at all, which is exactly why the
    # entry names the anim rather than a block.
    if cfgs[0].get("anims_only"):
        f, e, intern, sites, ids = L.file_info(rom, fid)
        anims = []
        at = {}

        for entry in cfgs[0]["anims"]:
            joint, name = (0, entry) if isinstance(entry, str) \
                else (entry[0], entry[1])
            soff = intern.get(off, off)

            if soff + 4 > len(f):
                raise AssertionError(
                    "%s: %s at 0x%X is outside file %d (%d bytes)"
                    % (stage, name, soff, fid, len(f)))
            words, fixups = read_anim_script(f, intern, soff)
            at[soff] = len(anims)
            anims.append({"name": name, "words": words,
                          "fixups": fixups, "off": soff,
                          "joint": joint})

        # The same by-script remap the named-block path does at its own
        # tail: a SetAnim word names another script by its FILE offset,
        # and the pack addresses scripts by their index in its anim
        # array. A self-loop -- which is what this stage's script is --
        # resolves to index 0.
        for a in anims:
            remapped = []

            for (w, t, _op) in a["fixups"]:
                if t is None or t not in at:
                    raise AssertionError(
                        "%s: %s (joint %d) word %d -> 0x%X points outside "
                        "the exported set (have %s)"
                        % (stage, a["name"], a["joint"], w,
                           (t if t is not None else -1), sorted(at)))
                remapped.append((w, at[t]))
            a["fixups"] = remapped

        return {"fid": fid, "off": off, "anims": anims, "skipped": [],
                "objects": [], "attack": None, "graft": None}

    f, e, intern, sites, ids = L.file_info(rom, fid)
    lf, _, _, _, _ = L.file_info(rom, STAGES[stage]["map_file"])
    blocks = read_map_blocks(STAGES[stage]["map_file"])

    def block(kind, name):
        hits = [b[2] for b in blocks if b[0] == kind and b[1] == name]

        if len(hits) != 1:
            raise AssertionError("%s: %d %s blocks named %s in the map "
                                 "group" % (stage, len(hits), kind, name))
        return hits[0]

    def find_head(desc):
        # `desc` names the block two ways, because the descriptions do:
        # Zebes lists `DObjDesc Acid` (the block's own kind and the
        # object's name), Kongo Jungle lists its tree as a bare `MapHead`
        # entry with no name at all. Try the named DObjDesc first, then a
        # standalone kind.
        try:
            return block("DObjDesc", desc)
        except AssertionError:
            hits = [b[2] for b in blocks if b[0] == desc]

            if len(hits) != 1:
                raise AssertionError("%s: %d blocks of kind %s in the map "
                                     "group" % (stage, len(hits), desc))
            return hits[0]

    def find_dl(name):
        # An object that is only a DISPLAY LIST, with no DObjDesc of its
        # own. Yoshi's Island's cloud is listed as `DisplayList Cloud`,
        # by kind and name; Mushroom Kingdom's is a bare `MapHead` entry
        # -- the same listing shape Kongo Jungle's tree has -- and there
        # the two words swap places, because MapHead is the block's KIND
        # and the name is `-`. Try the documented one first, then the
        # bare kind, which is find_head's own two-way resolution one
        # shape over.
        try:
            return block("DisplayList", name)
        except AssertionError:
            return block(name, "-")

    def head_of(oc):
        return find_dl(oc["dl"]) if oc.get("dl") else find_head(oc["desc"])

    # The FIRST object is the one MPGroundData.map_nodes points at. It is
    # usually the tree the game's own ground setup builds before any
    # stage logic runs, and for Mushroom Kingdom it is a display list --
    # `map_nodes` names `MapHead`, one Gfx, and the stage's own code
    # allocates an empty DObj for it per platform. Either shape answers
    # the same question, and this assertion is what proves the
    # descriptions' offsets are the *symbols'* addresses: it compares the
    # description's offset with the one the ROM's map header carries.
    if head_of(cfgs[0]) != off:
        raise AssertionError("%s: map_nodes points at 0x%X, the "
                             "descriptions put %s at 0x%X"
                             % (stage, off, cfgs[0].get("desc")
                                or cfgs[0]["dl"], head_of(cfgs[0])))

    # A stage's map group spans more than one file: Kongo Jungle names
    # four AnimJoints (the fourth, `Bird`, an object past the end of this
    # one) and Zebes three whole objects (Acid, Ridley, Ship). Only the
    # tree's own file is exported; the rest are other objects in other
    # files, for the steps that port them.
    anims = []
    at = {}
    skipped = []
    objects = []

    def add_anims(names, joined):
        for entry in names:
            # An entry is a name (an `AnimJoint` block) or a [kind, name]
            # pair. The pair exists because a description may use one name
            # for two blocks -- Dream Land has `AnimJoint
            # WhispyMouthLeftOpen` at 0x1E80 and `Texture
            # WhispyMouthLeftOpen` at 0x2BE0, and its own code feeds BOTH
            # to gcAddAnimJointAll -- and the pack's names are a by-name
            # lookup, so they have to differ. A pair is carried under
            # `name + kind`, which is exactly the decomp's own symbol for
            # it (`llGRPupupuMapWhispyMouthLeftOpenTexture`).
            # ... OR a (kind, name, offset) triple, when the script is
            # not a NAMED block at all: Mushroom Kingdom's `?` block
            # carries two, and only the damage one is in the map group's
            # list (`AnimJoint PowerBlock`, 0x1288). Its idle script is
            # reached in the game only through `attr->anim_joints`, an
            # array at 0x13B0 whose second entry is file 155's 0x13B8 --
            # an offset with no name to look up. The triple gives it one,
            # and the offset is read as the script itself rather than as
            # a pointer slot.
            if isinstance(entry, tuple) and len(entry) in (3, 4):
                kind, name, raw = entry[0], entry[1], entry[2]
                # The JOINT is the tuple's fourth field, default 0. It
                # matters even for one script: Saffron City's Chansey
                # reaches its appear script through an anim_joints array
                # whose entry 0 is NULL and whose entry 1 is the script,
                # and `stage_map_anim_array` builds the per-joint array
                # from this field -- a script on the wrong joint is a
                # wrong animation, or none.
                joint = entry[3] if len(entry) == 4 else 0
                # ... AND THE FILE, which is the one thing this arm cannot
                # check for itself. A bare offset is read in the stage's
                # MAP-OBJECT file, which is right for Mushroom Kingdom's
                # `?` block (its idle script is in the map model file the
                # map object already comes from) and WRONG for Saffron
                # City's monsters: `attr->anim_joints` is a reloc pointer
                # into StageYamabukiFile3 (159), while the Gate's tree
                # comes from StageYamabukiFile4 (160). An offset read in
                # the wrong file is not an error -- it is whatever bytes
                # are there, and for Chansey 160:0x03F8 is a DISPLAY LIST
                # that decodes cleanly as a one-word script, so the pack
                # "succeeded" with a garbage animation
                # unless the name is paired with its file. A pair names the file.
                if isinstance(raw, tuple):
                    src_fid, raw = raw
                else:
                    src_fid = fid
                if src_fid == fid:
                    sf, sintern = f, intern
                else:
                    _sf, _, sintern, _, _ = L.file_info(rom, src_fid)
                    sf = _sf
                pname = name + kind if kind != "AnimJoint" else name

                if pname in joined:
                    continue
                joined.append(pname)
                if raw + 4 > len(sf):
                    raise AssertionError("%s: %s at 0x%X is outside file %d "
                                         "(%d bytes)"
                                         % (stage, pname, raw, src_fid,
                                            len(sf)))
                words, fixups = read_anim_script(sf, sintern, raw)
                at[(src_fid, raw)] = len(anims)
                anims.append({"name": pname, "words": words,
                              "fixups": fixups, "off": raw, "fid": src_fid,
                              "joint": joint})
                continue

            kind, name = ("AnimJoint", entry) if isinstance(entry, str) \
                else (entry[0], entry[1])
            pname = name if isinstance(entry, str) else name + kind

            if pname in joined:
                continue
            joined.append(pname)
            boff = block(kind, name)
            # One record per (name, joint): a name may cover several
            # scripts, one per joint of the tree the table belongs to, and
            # the port builds its per-DObj array from all of them. Two
            # records sharing a script is fine -- a table can name the
            # same block on two joints -- so the by-script dedup is keyed
            # on the offset alone.
            for joint, soff in read_anim_block(f, intern, boff):
                if soff + 4 > len(f):
                    skipped.append("%s@0x%X" % (name, soff))
                    continue
                if (fid, soff) in at:
                    continue
                words, fixups = read_anim_script(f, intern, soff)
                at[(fid, soff)] = len(anims)
                anims.append({"name": pname, "words": words,
                              "fixups": fixups, "off": soff, "fid": fid,
                              "joint": joint})

    # The stage's OWN scripts, which belong to no object this pack carries.
    # Mushroom Kingdom's Piranha Plant and `?` block are
    # the case: their trees and their animations are the stage MODEL
    # file's, so nothing here bakes a tree for them -- but their scripts
    # are named blocks in the map group (`AnimJoint PowerBlock`,
    # `AnimJoint PakkunAppear`) and the item's own code reaches one by an
    # offset arithmetic (`itGetPData(ip, &DataStart, &AnimJoint)`) that
    # the port replaces with a name, stage_map_anim("PowerBlock").
    #
    # They go into the SAME anim section the objects' scripts do, which is
    # already a by-name lookup -- so this costs no format change at all,
    # only a longer list. The `joined` set is fresh, because it exists to
    # dedup within one object's own table.
    add_anims(STAGES[stage].get("stage_anims", ()), [])

    for oc in cfgs:
        joined = []

        add_anims(oc.get("anims", ()), joined)

        mobjsub = None
        if oc.get("mobjsub"):
            mo = block("MObjSub", oc["mobjsub"])
            mo = intern.get(mo, mo)
            if mo + 4 > len(f):
                raise AssertionError("%s: MObjSub %s at 0x%X is outside the "
                                     "map file" % (stage, oc["mobjsub"], mo))
            mobjsub = mo

        # The object's own material scripts, when its materials are
        # ANIMATED rather than baked: one MatAnimJoint per state, which
        # becomes the pack's `alt`s and is what the stage's own code
        # switches between (Yoshi's cloud, Dream Land's mouth). A stage
        # naming one leaves the object with a single alt, which is the
        # acid's shape and the reason the count is carried at all.
        matanims = []
        mc = oc.get("matanim")

        for name in ((mc,) if isinstance(mc, str) else (mc or ())):
            matanims.append(matanim_table(intern, block("MatAnimJoint",
                                                        name)))

        obj_off = head_of(oc)

        objects.append({"desc": oc.get("desc") or oc["dl"], "off": obj_off,
                        "dl": (obj_off if oc.get("dl") else None),
                        "mobjsub": mobjsub, "matanims": matanims,
                        "dl_link": oc.get("dl_link"),
                        "dl_links": bool(oc.get("dl_links"))})

    for a in anims:
        remapped = []

        for (w, t, opcode) in a["fixups"]:
            # The target is an offset in the SAME file as the script that
            # names it -- a Jump to a sibling block -- so the index is
            # looked up under (file, offset), not the offset alone.
            #
            # `opcode` is dropped here: once the target's INDEX is what the
            # pack writes, a SetInterp and a Jump are the same kind of
            # record -- a word the loader fills with the entry's address.
            # What the opcode decided was how the target was READ, which
            # happened before this ran (see add_interp_descs).
            if t is None or (a["fid"], t) not in at:
                raise AssertionError(
                    "%s: %s (joint %d) word %d -> 0x%X (in file %d) points "
                    "outside the exported set (have %s)"
                    % (stage, a["name"], a["joint"], w,
                       (t if t is not None else -1), a["fid"],
                       sorted(o for (fi, o) in at if fi == a["fid"])))
            remapped.append((w, at[(a["fid"], t)]))
        a["fixups"] = remapped

    # The graft, the stage-level MatAnimJoint and the attack descriptor are
    # the FIRST object's: one per stage, not one per object.
    cfg = cfgs[0]

    # The grafted object, when the stage's own code builds one at runtime
    # off a bare DisplayList rather than off a DObjDesc of its own.
    graft = None
    if cfg.get("graft"):
        g = cfg["graft"]
        # the RAW block offsets, not what they point at: they are the
        # symbols the game adds to map_head, and each one's own content is
        # the relocated pointer that read_mobjsubs/read_mobj_scripts
        # follow.
        graft = {
            "dl": block("DisplayList", g["dl"]),
            "mobjsub": block("MObjSub", g["mobjsub"]),
            "matanim": [block("MatAnimJoint", n) for n in g["matanim"]],
        }

    # NB: the MatAnimJoint is NOT here -- it is per OBJECT (see `matanims`
    # above), because each of Dream Land's four trees animates its own.


    # The attack descriptor is in the map LOGIC file, not the data file:
    # the game reaches it as gMPCollisionGroundData - &llGR<Stage>Map
    # MapHeader + the offset, i.e. straight off the header's own base.
    attack = None
    # It is the STAGE's, and the table names it on
    # whatever `map_object` entry the stage has -- which for a one-object
    # stage IS the stage's, and for a four-object stage is nobody's. Both
    # spellings are accepted: the object's first (Zebes, where it has
    # always been), the stage's after (Inishie's `?` block, whose four
    # objects are two and none of them is the descriptor's owner).
    ac_name = cfg.get("attack_coll") or STAGES[stage].get("attack_coll")
    if ac_name:
        ac = block("GRAttackColl", ac_name)
        if ac + 28 > len(lf):
            raise AssertionError("%s: GRAttackColl %s at 0x%X is outside the "
                                 "map logic file"
                                 % (stage, cfg["attack_coll"], ac))
        attack = struct.unpack_from(">7i", lf, ac)

    return {"fid": fid, "off": off, "anims": anims, "skipped": skipped,
            "objects": objects, "attack": attack, "graft": graft}


def check_fog(stage, ground):
    """MPGroundData.fog_color, against the decomp's own literal.

    The ONE thing in the game that reads this colour is the off-screen
    magnifying glass, whose disc is drawn in it
    (if/ifcommon.c ifCommonPlayerMagnifyUpdateRender). The port carried
    the bytes and never bound them, so every glass came up black inside;
    a wrong READ here would put every glass back to the wrong colour with
    nothing to say so, which is why the value is pinned. Zebes' really is
    0,0,0 -- black there is the game's own answer."""
    cfg = STAGES[stage].get("fog")

    if cfg is None:
        raise AssertionError("%s: STAGES has no fog row" % stage)
    if tuple(ground["fog"]) != tuple(cfg):
        raise AssertionError("%s fog_color: read %s, decomp says %s"
                             % (stage, ["0x%02X" % c for c in ground["fog"]],
                                ["0x%02X" % c for c in cfg]))
    return tuple(ground["fog"])


def read_item_weights(rom, stage, ground):
    """MPItemWeights for the stage: one byte per common item kind, which
    the randomizer reads to decide what drops where. In the map logic
    file, beside the header that points at it."""
    cfg = STAGES[stage].get("item_weights")
    if ground["item_weights"] is None:
        return None
    if cfg is None:
        # A stage whose map names weights must carry them: without the
        # row the randomizer's sum is 0 and no item ever drops (Sector Z
        # until its row was added).
        raise AssertionError("%s: the map points at item weights but "
                             "STAGES has no item_weights row" % stage)
    wfid, woff = ground["item_weights"]
    wf, _, _, _, _ = L.file_info(rom, wfid)
    if woff + ITEM_WEIGHTS_COUNT > len(wf):
        raise AssertionError("item weights at 0x%X run past file %d"
                             % (woff, wfid))
    weights = bytes(wf[woff:woff + ITEM_WEIGHTS_COUNT])
    if tuple(weights) != tuple(cfg):
        raise AssertionError("%s item weights: read %s, decomp says %s"
                             % (stage, list(weights), list(cfg)))
    return weights


# MPItemWeights.values (mp/mptypes.h:166): one byte per common item kind,
# nITKindCommonStart..nITKindCommonEnd (it/itdef.h:93-122, twenty of them).
ITEM_WEIGHTS_COUNT = 20


# ef/efground.c's own update_kind switch (EFGroundDesc.effect_desc.
# proc_update): stage_load_ground in src/dc/stage.c picks the same three
# functions by this same number. THE TWO PARSERS MOVE TOGETHER.
GROUND_UPDATE_KINDS = {"common": 0, "yaw": 1, "steps": 2}

# EFGroundDesc.proc_groundeffect, the hook efGroundUpdatePhysics runs ONCE
# at spawn after it has placed the actor (efground.c:1285). Every desc in
# the game leaves it NULL except Sector Z's first rocket, which takes
# efGroundSetStepPositions -- so this is a two-value table, and the same
# switch lives in stage_load_ground. THE TWO PARSERS MOVE TOGETHER.
GROUND_SETUP_KINDS = {"none": 0, "steps": 1}


def read_ground_actors(rom, stage):
    """dEFGroundDatas[stage] (Part B): the background actors
    ef/efground.c spawns and despawns at runtime -- Peach's Castle's
    Lakitu is the only one exported so far -- baked from the stage's own
    map group the same way read_map_object bakes its tree.

    Unlike a map object, an EFGroundDesc/EFGroundParam's scalars
    (alt_high, pos_z, scale, the weight table, ...) are the executable's
    own .data, compiled from efground.c's dEFGround<Stage>EffectDescs/
    Params arrays -- not a ROM asset there is a block to read -- so they
    are transcribed by hand into STAGES[stage]["ground_actors"] and
    carried through untouched, the same way item_weights already are.
    """
    cfg = STAGES[stage].get("ground_actors")
    if cfg is None:
        return None

    # The descriptions GROUP is named by the map LOGIC file (259 for
    # Castle) -- read_map_blocks' own docstring already flags this as a
    # group that lists blocks belonging to a DIFFERENT physical file --
    # but the offsets it gives are addresses in whichever file the game
    # links at the same base. For a ground actor that is NOT the map
    # object's own file (`map_nodes`, which for Castle is a small,
    # unrelated bumper script): it is gr_desc[1].dobjdesc's file, i.e.
    # the stage's own GROUND LAYERS -- efGroundMakeEffectID's decomp
    # `file_head` is derived from exactly that pointer
    # (gMPCollisionGroundData->gr_desc[1].dobjdesc - o_data), which is
    # this same read_ground()["layers"][1].
    ground = read_ground(rom, stage)
    fid = next(t[0] for t in ground["layers"] if t)
    f, e, intern, sites, ids = L.file_info(rom, fid)
    blocks = read_map_blocks(STAGES[stage]["map_file"])

    def block(kind, name):
        hits = [b[2] for b in blocks if b[0] == kind and b[1] == name]

        if len(hits) != 1:
            raise AssertionError("%s: %d %s blocks named %s in the map "
                                 "group" % (stage, len(hits), kind, name))
        return hits[0]

    # One baked pack per unique actor IDENTITY, not per spawnable variant
    # -- Castle needs exactly one, the Lakitu tree both the right- and
    # left-facing variants spawn from.
    pack_at = {}
    packs = []
    for name, pc in cfg["packs"].items():
        off = block("DObjDesc", pc["desc"])
        mobjsub = None
        if pc.get("mobjsub"):
            mo = block("MObjSub", pc["mobjsub"])
            mobjsub = intern.get(mo, mo)
        # The actor's material scripts, when it has any: one MatAnimJoint
        # per STATE, laid out as the pack's `alt`s and picked per desc
        # below. Dream Land's Bronto is the only actor with two (a
        # left-facing and a right-facing script over one tree); every
        # other animated actor has exactly one, and eight have none.
        mc = pc.get("matanim")
        o = {"off": off, "dl": None, "mobjsub": mobjsub, "name": name,
             "ground": True, "dl_links": bool(pc.get("dl_links")),
             "matanims": [block("MatAnimJoint", n)
                          for n in ((mc,) if isinstance(mc, str)
                                    else (mc or ()))]}
        (onodes, overts, otris, obatches, otexs, opals, omobj,
         omobj_count, omobj_alts) = bake_map_object(rom, fid, o)
        pack_at[name] = len(packs)
        packs.append({
            "name": name,
            "pack": object_pack(onodes, overts, otris, obatches, otexs,
                                opals, "%sG%d" % (stage, len(packs)),
                                omobj, omobj_count, omobj_alts),
            "joint_count": len(onodes),
            "matanim": [n for n in ((mc,) if isinstance(mc, str)
                                    else (mc or ()))],
            "frames": o.get("frames", 1),
            "billboard": sum(1 << k for k in o["billboard"]),
        })
        if len(onodes) > 8:
            raise AssertionError("%s: ground actor %s has %d joints; GRA1's "
                                 "billboard mask holds 8 (stage.h "
                                 "STAGE_GROUND_JOINTS_MAX)"
                                 % (stage, name, len(onodes)))

    # Every spawnable variant's own anim script -- one AnimJoint table,
    # read the same way a map object's own scripts are (read_map_object's
    # add_anims), because a name may cover more than one joint of the
    # tree it walks. Shared by name across descs, the same dedup add_anims
    # itself does within one object's table.
    anims = []
    at = {}
    seen = set()
    descs = []

    for dc in cfg["descs"]:
        if dc.get("anim") and dc["anim"] not in seen:
            seen.add(dc["anim"])
            boff = block("AnimJoint", dc["anim"])

            for joint, soff in read_anim_block(f, intern, boff):
                if (fid, soff) in at:
                    continue
                words, fixups = read_anim_script(f, intern, soff)
                at[(fid, soff)] = len(anims)
                anims.append({"name": dc["anim"], "words": words,
                              "fixups": fixups, "off": soff, "fid": fid,
                              "joint": joint})

        # Which of the pack's MatAnimJoints this desc plays. Named rather
        # than indexed, so the config cannot silently drift out of step
        # with the pack's own list; a pack with one is unambiguous and a
        # desc may leave it out.
        mats = packs[pack_at[dc["pack"]]]["matanim"]
        alt = 0
        if dc.get("matanim"):
            if dc["matanim"] not in mats:
                raise AssertionError(
                    "%s: desc %d asks for MatAnimJoint %s, but pack %s "
                    "carries %s" % (stage, len(descs), dc["matanim"],
                                    dc["pack"], mats))
            alt = mats.index(dc["matanim"])
        elif len(mats) > 1:
            raise AssertionError(
                "%s: desc %d's pack %s carries %s and the desc does not say "
                "which" % (stage, len(descs), dc["pack"], mats))

        descs.append({
            "pack": pack_at[dc["pack"]],
            "anim": dc.get("anim") or "",
            "matanim_alt": alt,
            "billboard": packs[pack_at[dc["pack"]]]["billboard"],
            "dl_link": dc.get("dl_link", 0),
            "update_kind": dc.get("update_kind", "common"),
            "setup_kind": dc.get("setup_kind", "none"),
            "alt_high": dc["alt_high"], "alt_low": dc["alt_low"],
            "pos_z": dc["pos_z"], "scale": dc["scale"],
            "effect_status": dc.get("effect_status", -1),
        })

    remap_anim_fixups(stage, anims, at)

    params = [{"effect_id": p["effect_id"],
               "make_queue": p.get("make_queue", 0), "lr": p.get("lr", 0),
               "weight": p["weight"]}
              for p in cfg["params"]]

    return {"packs": packs, "descs": descs, "anims": anims, "params": params}


def ground_actors_section(ga):
    """GRA1: the stage's dEFGroundDatas[], as one packed block between the
    optional WLP1 (wallpaper) and MPK1 (map objects) blocks in a .stg
    file's extras. THE TWO PARSERS MOVE TOGETHER: this one and
    stage_load_ground in src/dc/stage.c (which spells out every field's
    byte offset in its own header comment).

    `ga` is None for a stage with no ground-actor table -- the block
    is then simply absent, `dEFGroundDatas[gkind]` stays NULL, and
    efGroundMakeAppearActor's own no-op guard keeps that stage exactly as
    silent as it is today.
    """
    if ga is None:
        return b""

    pk = b""
    mt = b""
    for p in ga["packs"]:
        mt += struct.pack("<2I", len(pk), len(p["pack"]))
        pk += align(p["pack"])
    mt = align(mt)

    ds = b""
    for d in ga["descs"]:
        # The setup kind, the material alternate and the billboard joint
        # mask ride in the three pad bytes that followed dl_link, so the
        # record stays 60 bytes: "no hook" (every stage but Sector Z), "the
        # pack's first MatAnimJoint" (every desc but Dream Land's
        # right-facing Bronto), and bit k for pack joint k whose DObjDesc
        # id carries 0xF000 (efground.c:1328-1343's billboard joints).
        ds += struct.pack(
            "<ffffHHIBBBB32s", d["alt_high"], d["alt_low"], d["pos_z"],
            d["scale"], d["effect_status"] & 0xFFFF,
            GROUND_UPDATE_KINDS[d["update_kind"]], d["pack"],
            d["dl_link"], GROUND_SETUP_KINDS[d["setup_kind"]],
            d["matanim_alt"], d["billboard"], d["anim"].encode()[:32])
    ds = align(ds)

    pm = b""
    for p in ga["params"]:
        pm += struct.pack("<HHiB3x", p["effect_id"], p["make_queue"],
                          p["lr"], p["weight"])
    pm = align(pm)

    # Same shape as MPK1's own anim/fixup tables (main(), below) -- a 40-
    # byte header (word count, joint, name[32]) then the words, and a
    # fixup naming (this script's own index, the word, the target
    # script's index).
    an = b""
    for a in ga["anims"]:
        an += struct.pack("<II32s", len(a["words"]), a["joint"],
                          a["name"].encode()[:32])
        an += b"".join(struct.pack("<I", w) for w in a["words"])
    an = align(an)

    fx = b"".join(struct.pack("<3I", i, w, t)
                  for (i, a) in enumerate(ga["anims"])
                  for (w, t) in a["fixups"])
    fx = align(fx)

    header_size = 56
    parts = (pk, mt, ds, pm, an, fx)
    offs = []
    cur = header_size
    for x in parts:
        offs.append(cur)
        cur += len(x)
    off_pack, off_models, off_descs, off_params, off_anims, off_fixups = offs
    block_size = cur

    header = struct.pack(
        "<4s13I", b"GRA1", len(ga["packs"]), len(ga["descs"]),
        len(ga["params"]), len(ga["anims"]),
        sum(len(a["fixups"]) for a in ga["anims"]),
        off_pack, len(pk), off_models, off_descs, off_params, off_anims,
        off_fixups, block_size)
    return header + b"".join(parts)


# BWP1's per-pack billboard mask is 16 bits wide, because two of Final
# Destination's five boss-wallpaper trees are nine joints and GRA1's
# eight-joint mask (STAGE_GROUND_JOINTS_MAX) will not hold them. Mirrored
# by STAGE_BOSS_JOINTS_MAX in src/dc/stage.h.
BOSS_JOINTS_MAX = 16


def read_boss_wallpaper(rom, stage):
    """BWP1: the five trees Final Destination's animated background is
    instanced from -- sc/sc1pmode/sc1pgameboss.c's `bosseffect` models.

    sc1PGameBossMakeWallpaperEffect (sc1pgameboss.c:852) makes a fresh
    GObj per spawn and builds its tree with
    sc1PGameBossSetupBackgroundDObjs off a DObjDesc array, then hangs the
    chosen AnimJoint/MatAnimJoint pair on it with
    lbCommonAddTreeDObjsAnimAll -- the SAME shape a ground actor is
    spawned in (ef/efground.c:1443 is the identical call), so the packs
    are baked exactly as read_ground_actors bakes its own.

    The rows, speeds, plans and spawn counts around them are NOT read
    here: they are sc1pgameboss.c's own .data (SC1PGameBossWallpaper /
    Effect / Anim / Plan), which the port compiles verbatim, so this
    block carries only what lives in the ROM -- the trees, their
    materials and their scripts.

    A separate block rather than GRA1: a boss effect has no EFGroundDesc,
    and `Stage.ground_anim_tables` is indexed by that desc, so putting
    these in GRA1 would mean re-indexing a structure nine shipping stages
    read. Nothing existing changes by adding one.

    All of it lives in reloc file 114, the stage's own layer file, not in
    the map group 266 that merely NAMES the blocks: sSC1PGameBossMain.
    file_head is `gr_desc[1].dobjdesc - llGRLastMapFileHead`, which is
    that file's base, and every llGRLastMap* symbol is an offset into it.
    """
    cfg = STAGES[stage].get("boss_wallpaper")
    if cfg is None:
        return None

    ground = read_ground(rom, stage)
    fid = next(t[0] for t in ground["layers"] if t)
    f, e, intern, sites, ids = L.file_info(rom, fid)
    blocks = read_map_blocks(STAGES[stage]["map_file"])

    def block(kind, name):
        hits = {b[2] for b in blocks if b[0] == kind and b[1] == name}

        if len(hits) != 1:
            raise AssertionError("%s: %d distinct %s blocks named %s in the "
                                 "map group" % (stage, len(hits), kind, name))
        return hits.pop()

    packs, anims, at = [], [], {}

    for name, pc in cfg.items():
        # Which of the two tree walks this effect's display proc uses, and
        # what render mode it leaves the head in. sc1pgameboss.c:461-545:
        # Wallpaper0/1/2ProcDisplay set head 1 to G_RM_AA_XLU_SURF and
        # call gcDrawDObjTreeDLLinksForGObj (so the DObjDesc's `dl` is a
        # DObjDLLink array, not commands); Wallpaper3ProcDisplay0 sets
        # head 0 to the same mode and calls gcDrawDObjTreeForGObj, whose
        # `dl` is one plain display list.
        links = bool(pc.get("dl_links"))
        seed = ([A.G_RM_AA_OPA_SURF, A.G_RM_AA_XLU_SURF, None, None] if links
                else [A.G_RM_AA_XLU_SURF, None, None, None])
        mc = pc.get("matanim") or ()
        mc = (mc,) if isinstance(mc, str) else tuple(mc)
        # Wallpaper1ProcDisplay (sc1pgameboss.c:490-492) also puts head 1
        # in G_CYC_2CYCLE, and its list's second combiner cycle is where
        # the fog's purple and its fade are (MeshBaker._fold_cycle2).
        omh = None
        if pc.get("two_cycle"):
            omh = [A.OTHERMODE_H_RESET] * 4
            omh[1] |= A.G_CYC_2CYCLE << A.G_MDSFT_CYCLETYPE
        o = {"off": block("DObjDesc", pc["desc"]), "dl": None, "name": name,
             "ground": True, "dl_links": links, "rendermode": seed,
             "othermode_h": omh,
             "mobjsub": (block("MObjSub", pc["mobjsub"])
                         if pc.get("mobjsub") else None),
             "matanims": [block("MatAnimJoint", n) for n in mc]}
        (onodes, overts, otris, obatches, otexs, opals, omobj,
         omobj_count, omobj_alts) = bake_map_object(rom, fid, o)

        if len(onodes) > BOSS_JOINTS_MAX:
            raise AssertionError("%s: boss effect %s has %d joints; BWP1's "
                                 "billboard mask holds %d (stage.h "
                                 "STAGE_BOSS_JOINTS_MAX)"
                                 % (stage, name, len(onodes),
                                    BOSS_JOINTS_MAX))
        # The tree's own AnimJoint table, read the way a ground actor's
        # is: the block names a TABLE, and what matters is each script's
        # joint. An effect whose SC1PGameBossAnim row carries a NULL
        # AnimJoint offset (Anims2_1, Anims3_0) simply has none.
        first = len(anims)
        if pc.get("anim"):
            for joint, soff in read_anim_block(f, intern,
                                               block("AnimJoint",
                                                     pc["anim"])):
                words, fixups = read_anim_script(f, intern, soff)
                at[soff] = len(anims)
                anims.append({"name": pc["anim"], "words": words,
                              "fixups": fixups, "joint": joint,
                              "off": soff})
        packs.append({
            "name": name,
            "pack": object_pack(onodes, overts, otris, obatches, otexs,
                                opals, "%sB%d" % (stage, len(packs)),
                                omobj, omobj_count, omobj_alts),
            "anim_first": first,
            "anim_count": len(anims) - first,
            "matanim_count": len(mc),
            "billboard": sum(1 << k for k in o["billboard"]),
        })

    # The same by-script remap every other anim table gets: a Jump or
    # SetAnim word names another script by its FILE offset, and the pack
    # addresses scripts by their index in its own anim array. Every one of
    # these scripts is a self-loop, so each resolves to itself. Only one
    # file is in play here, so the index is keyed on the offset alone.
    for a in anims:
        remapped = []

        for (w, t, _op) in a["fixups"]:
            if t is None or t not in at:
                raise AssertionError(
                    "%s: %s (joint %d) word %d -> 0x%X points outside the "
                    "exported set (have %s)"
                    % (stage, a["name"], a["joint"], w,
                       (t if t is not None else -1), sorted(at)))
            remapped.append((w, at[t]))
        a["fixups"] = remapped

    return {"packs": packs, "anims": anims}


def tree_pack_section(bw, magic=b"BWP1"):
    """BWP1 or BPL1, as one packed block. THE TWO PARSERS MOVE TOGETHER:
    this one and stage_load_packs in src/dc/stage.c, which reads both
    magics with one body.

    BWP1 rides in Final Destination's .stg extras; BPL1 is a file of its
    own (romdisk/bonus2plat.pak) and is the whole of it, because the six
    trees it carries belong to all twelve Board the Platforms courses
    rather than to one -- 229 KB that would otherwise be written to the
    disc twelve times over.

    GRA1's `packs` + `anims` + `fixups` sections and nothing else -- the
    descs and params GRA1 also carries are efground.c's .data, and a boss
    effect's equivalent (SC1PGameBossPlan) is sc1pgameboss.c's, which the
    port compiles rather than exports. Each pack is named so that
    sc1pgameboss.c looks its tree up the way its own source names it.
    """
    if bw is None:
        return b""

    pk = b""
    mt = b""
    for p in bw["packs"]:
        mt += struct.pack("<6I32s", len(pk), len(p["pack"]),
                          p["anim_first"], p["anim_count"],
                          p["billboard"], p["matanim_count"],
                          p["name"].encode()[:32])
        pk += align(p["pack"])
    mt = align(mt)

    an = b""
    for a in bw["anims"]:
        an += struct.pack("<II32s", len(a["words"]), a["joint"],
                          a["name"].encode()[:32])
        an += b"".join(struct.pack("<I", w) for w in a["words"])
    an = align(an)

    fx = b"".join(struct.pack("<3I", i, w, t)
                  for (i, a) in enumerate(bw["anims"])
                  for (w, t) in a["fixups"])
    fx = align(fx)

    header_size = 40
    parts = (pk, mt, an, fx)
    offs = []
    cur = header_size
    for x in parts:
        offs.append(cur)
        cur += len(x)
    off_pack, off_models, off_anims, off_fixups = offs
    block_size = cur

    header = struct.pack(
        "<4s9I", magic, len(bw["packs"]), len(bw["anims"]),
        sum(len(a["fixups"]) for a in bw["anims"]),
        off_pack, len(pk), off_models, off_anims, off_fixups, block_size)
    return header + b"".join(parts)


# Board the Platforms' six shared trees, in the order
# dSC1PBonusStagePlatformDescs and dSC1PBonusStageBoardedPlatformDescs
# index them: Small, Medium, Large, then the same three boarded. A name
# is what src/dc/stage.c looks a tree up by, so these are the decomp's
# own (relocFileDescriptions.us.txt group [136]).
BONUS2_COMMON_FILE = 136
BONUS2_PLATFORMS = ["PlatformSmall", "PlatformMedium", "PlatformLarge",
                    "BoardedPlatformSmall", "BoardedPlatformMedium",
                    "BoardedPlatformLarge"]


def read_bonus_platforms(rom):
    """BPL1: the six trees a Board the Platforms course's platforms are
    built from, out of the file all twelve share.

    sc1PBonusStageInitPlatforms (sc/sc1pmode/sc1pbonusstage.c:539) walks
    the course's floor lines, and for each whose material is
    nMPMaterialDetect builds a tree on that line's YAKUMONO DObj --
    lbCommonSetupTreeDObjs off a DObjDesc, then
    lbCommonAddMObjForTreeDObjs and lbCommonAddTreeDObjsAnimAll for the
    materials and the two anim tables. That is the same four-part shape
    a ground actor and a boss effect are built in, so the trees are
    baked exactly as read_boss_wallpaper bakes its own and ship in the
    same block.

    Where they differ from BWP1 is WHOSE they are.
    dSC1PBonusStagePlatformDescs is four columns of pointer-to-pointer
    into relocData 136 (Bonus2Common), which every one of the twelve
    courses loads -- so this reads no stage at all, and what it writes
    is a file rather than a block in one. Baked, the six come to 229 KB;
    put in each course's pack, as the wallpaper they also share already
    is, they would be 2.7 MB of a 13 MB disc.

    The three unboarded trees carry an MObjSub and a MatAnimJoint and
    the three boarded ones do not, which is the table's own shape: a
    [4] row against a [2] row.
    """
    import ssb_assets as A

    fid = BONUS2_COMMON_FILE
    f, e, intern, sites, ids = L.file_info(rom, fid)
    blocks = read_map_blocks(fid)

    def block(kind, name):
        hits = {b[2] for b in blocks if b[0] == kind and b[1] == name}

        if len(hits) != 1:
            raise AssertionError("Bonus2Common: %d distinct %s blocks named "
                                 "%s" % (len(hits), kind, name))
        return hits.pop()

    packs, anims, at = [], [], {}

    for name in BONUS2_PLATFORMS:
        # A platform hangs off a yakumono DObj, which the map GObj draws
        # with gcDrawDObjTreeDLLinksForGObj -- so the DObjDesc's `dl` is
        # a DObjDLLink array and not commands, the same reading three of
        # the five boss effects get. Reading it as a display list walks
        # off the end of the file, which is what said so.
        mat = not name.startswith("Boarded")
        o = {"off": block("DObjDesc", name), "dl": None, "name": name,
             "ground": True, "dl_links": True,
             "rendermode": [A.G_RM_AA_OPA_SURF, A.G_RM_AA_XLU_SURF,
                            None, None],
             "mobjsub": (block("MObjSub", name) if mat else None),
             "matanims": ([block("MatAnimJoint", name)] if mat else [])}
        (onodes, overts, otris, obatches, otexs, opals, omobj,
         omobj_count, omobj_alts) = bake_map_object(rom, fid, o)

        if len(onodes) > BOSS_JOINTS_MAX:
            raise AssertionError("Bonus2Common: %s has %d joints; the block's "
                                 "billboard mask holds %d (stage.h "
                                 "STAGE_BOSS_JOINTS_MAX)"
                                 % (name, len(onodes), BOSS_JOINTS_MAX))
        first = len(anims)

        for joint, soff in read_anim_block(f, intern,
                                           block("AnimJoint", name)):
            words, fixups = read_anim_script(f, intern, soff)
            at[soff] = len(anims)
            anims.append({"name": name, "words": words, "fixups": fixups,
                          "joint": joint, "off": soff})
        packs.append({
            "name": name,
            "pack": object_pack(onodes, overts, otris, obatches, otexs,
                                opals, "B2C%d" % len(packs),
                                omobj, omobj_count, omobj_alts),
            "anim_first": first,
            "anim_count": len(anims) - first,
            "matanim_count": len(o["matanims"]),
            "billboard": sum(1 << k for k in o["billboard"]),
        })
        print("Bonus2Common: %s: %d joints, %d verts, %d tris, %d batches, "
              "%d texs, %d mobj alt(s), %d script(s), %d bytes"
              % (name, len(onodes), len(overts), len(otris), len(obatches),
                 len(otexs), omobj_alts, len(anims) - first,
                 len(packs[-1]["pack"])))

    # Every script here is a self-loop, the way a boss effect's are, and
    # only one file is in play, so the index is keyed on the offset
    # alone.
    for a in anims:
        remapped = []

        for (w, t, _op) in a["fixups"]:
            if t is None or t not in at:
                raise AssertionError(
                    "Bonus2Common: %s (joint %d) word %d -> 0x%X points "
                    "outside the exported set (have %s)"
                    % (a["name"], a["joint"], w,
                       (t if t is not None else -1), sorted(at)))
            remapped.append((w, at[t]))
        a["fixups"] = remapped

    return {"packs": packs, "anims": anims}


# One DObjDesc, sys/objtypes.h:376: s32 id, void *dl, then three Vec3f.
BONUS_DESC_SIZE = 0x2C
BONUS_DESC_TRANSLATE = 0x08
# sys/objdef.h DOBJ_ARRAY_MAX, the id that ends a DObjDesc array.
DOBJ_ARRAY_MAX = 0x12
# sc/scdef.h SCBATTLE_BONUSGAME_TASK_MAX. sc1PBonusStageMakeTargets
# hangs the console if a course does not have exactly this many, so the
# export refuses to ship one that would.
BONUS_TASK_MAX = 10
# src/dc/stage.h's STAGE_TARGET_ANIMS_MAX and STAGE_BUMPER_ANIMS_MAX. The
# loader refuses a block with more entries than its array holds, and it
# is cheaper to hear about that here than off a disc probe.
BONUS_ANIMS_MAX = {b"BTG1": 8, b"BMP1": 32}


def read_bonus_placements(rom, stage, key, exact=None):
    """BTG1 and BMP1: where a bonus course's items stand, and the scripts
    the moving ones follow.

    sc1PBonusStageMakeTargets (sc/sc1pmode/sc1pbonusstage.c:434) walks a
    DObjDesc array and an AObjEvent32* table side by side, making one
    nITKindTarget item per desc at its `translate` and hanging the
    matching script on it with gcAddDObjAnimJoint.
    sc1PBonusStageMakeBumpers (:700) is the same eleven lines with
    nITKindGBumper in place of nITKindTarget, so `key` picks which of a
    course's two tables to read and both come out the same shape. Both
    live in the course's LAYER file -- the same file
    read_ground()["layers"][1] names -- and the game finds them by
    subtracting a known offset from a pointer it already has (the layer
    for targets, gMPCollisionGroundData->map_nodes for bumpers), which
    is the reloc trick the port has no runtime for. So the tables are
    read here and shipped as data.

    Only the translate is read, because only the translate is used: the
    desc's rotate and scale go nowhere -- itManagerMakeItemSetupCommon
    takes a position and a velocity and nothing else. The array's first
    entry is skipped the way the game skips it (`dobjdesc++` before the
    loop): it is the parent the rest hang off, not one of them.

    A block of its own rather than GRA1 or BWP1 for the reason BWP1 is
    its own: these carry no model at all. A target's geometry is an
    ITEM's -- relocData 253/150, ITBonus1Object -- loaded once by
    sc1PBonusStageBonus1LoadFile and shared by all ten, and a bumper's
    is nITKindGBumper's out of ITCommonObject, which every stage already
    has. There is nothing here to bake.

    `exact` is how many placements the game will not survive being wrong
    about; None means the array's own terminator decides.
    """
    cfg = STAGES[stage].get(key)
    if cfg is None:
        return None

    ground = read_ground(rom, stage)
    # The twelve Bonus1 and Bonus2 courses keep these tables in their
    # layer file, which is what the game's own reloc arithmetic reaches
    # (`gr_desc[1].dobjdesc - target->start`). Race to the Finish does
    # not: grBonus3MakeBumpers works off `map_head`, a different file
    # again, so its config names one.
    fid = cfg.get("file") or ground["layers"][1][0]
    f, e, intern, sites, ids = L.file_info(rom, fid)
    ext = {site: (tfid, toff) for (site, toff), tfid in zip(sites, ids)}
    blocks = read_map_blocks(STAGES[stage]["map_file"])

    def block(kind, name):
        hits = {b[2] for b in blocks if b[0] == kind and b[1] == name}
        if len(hits) != 1:
            raise AssertionError("%s: %d distinct %s blocks named %s"
                                 % (stage, len(hits), kind, name))
        return hits.pop()

    doff = block("DObjDesc", cfg["desc"])
    aoff = block("AnimJoint", cfg["anim"])

    anims, at, targets = [], {}, []
    i = 0
    while True:
        o = doff + i * BONUS_DESC_SIZE
        if o + BONUS_DESC_SIZE > len(f):
            raise AssertionError("%s: %s descs run past file %d"
                                 % (stage, key, fid))
        if be32u(f, o) == DOBJ_ARRAY_MAX:
            break
        if i > 0:
            tr = struct.unpack_from(">3f", f, o + BONUS_DESC_TRANSLATE)
            # The script pointer for THIS desc, one word per desc beside
            # the array. A cross-file one would mean a course animates a
            # target from another course's file, which none does; saying
            # so here keeps a silent wrong read from shipping.
            site = aoff + i * 4
            if site in ext:
                raise AssertionError("%s: %s %d's script is in file %d, "
                                     "not the course's own"
                                     % (stage, key, i, ext[site][0]))
            soff = intern.get(site)
            idx = -1
            if soff is not None:
                if (fid, soff) not in at:
                    words, fixups = read_anim_script(f, intern, soff)
                    at[(fid, soff)] = len(anims)
                    anims.append({"name": "%s%d" % (cfg["anim"], i),
                                  "words": words, "fixups": fixups,
                                  "off": soff, "fid": fid, "joint": 0})
                idx = at[(fid, soff)]
            targets.append((tr[0], tr[1], tr[2], idx))
        i += 1
        if i > BONUS_TASK_MAX + 8:
            raise AssertionError("%s: %s descs do not end" % (stage, key))

    if exact is not None and len(targets) != exact:
        raise AssertionError("%s: %d %s, and sc1PBonusStageMakeTargets "
                             "hangs on anything but %d"
                             % (stage, len(targets), key, exact))
    # Four of the five courses with bumpers move them along a spline, so
    # their scripts SetInterp to an `SYInterpDesc` -- data, not a script
    # -- exactly as a map object's do, and it ships the same way. No Break the
    # Targets course has one, but nothing here
    # needs to know that.
    add_interp_descs(stage, anims, at, fid, f, intern)
    remap_anim_fixups(stage, anims, at)
    return {"targets": targets, "anims": anims}


def bonus_placements_section(bt, magic):
    """BTG1 or BMP1, as one packed block in a .stg file's extras. THE TWO
    PARSERS MOVE TOGETHER: this one and stage_load_placements in
    src/dc/stage.c, which reads both magics with one body.

    No pack section, unlike GRA1 and BWP1 -- see read_bonus_placements on
    why these carry no model. Just the placements and whatever scripts
    they name, in the same anim + fixup shape the other two use, so the
    loader's script fixing is the same code path.
    """
    if bt is None:
        return b""

    if len(bt["anims"]) > BONUS_ANIMS_MAX[magic]:
        raise AssertionError("%s: %d anim entries, and src/dc/stage.h holds "
                             "%d" % (magic.decode(), len(bt["anims"]),
                                     BONUS_ANIMS_MAX[magic]))

    tg = b"".join(struct.pack("<3fi", *t) for t in bt["targets"])
    tg = align(tg)

    an = b""
    for a in bt["anims"]:
        an += struct.pack("<I32s", len(a["words"]), a["name"].encode()[:32])
        an += b"".join(struct.pack("<I", w) for w in a["words"])
    an = align(an)

    fx = b"".join(struct.pack("<3I", i, w, t)
                  for (i, a) in enumerate(bt["anims"])
                  for (w, t) in a["fixups"])
    fx = align(fx)

    header_size = 32
    parts = (tg, an, fx)
    offs, cur = [], header_size
    for x in parts:
        offs.append(cur)
        cur += len(x)
    header = struct.pack("<4s7I", magic, len(bt["targets"]),
                         len(bt["anims"]),
                         sum(len(a["fixups"]) for a in bt["anims"]),
                         offs[0], offs[1], offs[2], cur)
    return header + b"".join(parts)


def read_anim_block(f, intern, boff):
    """[(joint, script offset), ...] for one `AnimJoint` / `Texture` block.

    Every one of these blocks is the same thing to the game: an
    `AObjEvent32 **` -- a PER-DOBJ ARRAY of scripts, one entry per joint of
    the tree it is attached to, walked by sys/objanim.c:228-244's
    `anim_joints++` beside `gcGetTreeDObjNext`. So a name does not name
    one script, it names a TABLE, and what matters is each script's joint.

    What the descriptions call the block, and what the table below it
    looks like, are different things, and all three shapes are in the
    tree:

      * the block's own offset is a slot -- Kongo Jungle's
        `TaruCannDefault` at 0xB20. One entry, joint 0.
      * the block is a table of slots at a 4-byte stride over a run of
        NULLs -- Dream Land's `WhispyEyesLeftTurn` at 0x11A0 has slots at
        +4 and +8, so its scripts are on joints 1 and 2 of the eyes'
        three. `WhispyEyesLeft0` at 0x33E0 has one slot at +0x18, joint 6
        of the flowers-front tree's ten.
      * the block is the script itself -- Kongo Jungle's `TaruCannFill` at
        0xB68 -- joint 0.

    Reading a table as if it were a single script is silent and
    expensive: the first word is NULL, which decodes as AObjEvent32's End,
    so the export "succeeds" with a one-word script for every name. That
    is what it did to Dream Land the first time. Reading only the
    FIRST slot of a table is the second, quieter half of the same mistake:
    the script's own Jump names a sibling in the same table, and the
    export fails with "points outside the exported set" because the
    sibling was never exported.

    The NULL run is what tells a table from a script: a script's leading
    words are commands (non-zero), a table's leading words before its
    first slot are NULL by construction.
    """
    out = []

    for k in range(32):
        off = boff + k * 4
        t = intern.get(off)

        if t is not None:
            out.append((k, t))
        elif struct.unpack_from(">I", f, off)[0] != 0:
            # A non-zero word that is not a relocated slot is a COMMAND,
            # so this is not a table's entry: either the table ended, or
            # the block was never a table (a script's first word is one).
            # Table entries between slots are ZERO and are skipped rather
            # than ending the scan -- `WhispyEyesRight1` at 0x35C0 has
            # slots at word 3 AND word 9, and stopping at the first gap
            # after word 3 loses the eyes' second texture script, whose
            # own Jump is what then "points outside the exported set".
            break

    if not out:
        return [(0, boff)]          # the block IS the script

    return out


# Which geometry layers' animation this export carries: all four.
#
# Layer 1 was left out of the first pass and is in now. It is the
# COLLISION layer -- `gcSetupCustomDObjs` fills
# gMPCollisionYakumonoDObjs->dobjs one entry per layer-1 DObjDesc in
# order (objanim.c:2363-2366), so YAKUMONO ID k IS LAYER-1 JOINT k, and
# every collision query offsets its line by that DObj's live translate.
# The port stood those DObjs in with zeroed ones (src/dc/mpcommon.c),
# which is why animating the layer had to wait: a platform's picture
# would have moved out from under its floor. src/dc/stage.c now points
# the array at the pack's own layer-1 joints, so the two move together
# and the drawn platform IS the floor, as it is in the game.
LAYER_ANIM_LAYERS = (0, 1, 2, 3)

def read_layer_anims(stage, f, intern, ground, spans, nodes):
    """The stage's geometry-layer AnimJoints, as ONE pack animation.

    MPGroundDesc.anim_joints is an `AObjEvent32 **`: one slot per joint of
    the layer's own DObjDesc tree, which the game walks beside
    gcGetTreeDObjNext (sys/objanim.c:228-244). The port merges the four
    layers into one pack, so the four tables become one table over the
    merged joints -- `entries[j]`, a word index into a shared pool or -1,
    which is the shape fighter.h's FPackAnim already has and
    efmanager.c's efModelAnimJoint already turns back into pointers.

    THE POOL IS THE FILE'S OWN SPAN, NOT A COPY OF EACH SCRIPT. An
    AnimJoint's Jump/SetAnim pointers are absolute, so lifting a script
    out of the file breaks every pointer into its neighbours --
    ssb_meshexport.read_anim_ex says exactly this about a fighter's
    animation file and carries the file whole for it. A stage's model file
    is far too big to carry whole, so what is carried is the contiguous
    span the scripts actually occupy: every inter-script pointer stays a
    pointer within the span, and the only thing the exporter rewrites is
    the span-relative word index each one holds until fighter_init turns
    it into an address (src/dc/fighter.c:481-503).

    ssb_assets.animjoint_walk is what finds that span, and it is also the
    validation: it follows every Jump and SetAnim, stops when it revisits
    a word, and raises on an undefined opcode, an unrelocated pointer or a
    read past the file. Those are the three things the game's parser has
    no case for and spins on, so the guard belongs here, where the data is
    made, and not in a parser the port runs unmodified.

    Returns None for a stage with no animated layer -- Hyrule.
    """
    heads = []                  # (merged joint, script byte offset)
    seen = set()                # every word the scripts reach
    per_layer = []

    for (layer, jbase, njoints) in spans:
        tbl = ground["layer_anims"][layer]
        if tbl is None or layer not in LAYER_ANIM_LAYERS:
            continue
        n = 0
        for k in range(njoints):
            slot = tbl[1] + k * 4
            t = intern.get(slot)
            if t is None:
                # Every slot of a per-DObj table is either a relocated
                # pointer or NULL. A non-zero word that is not relocated
                # would mean the table is shorter than the tree and this
                # is somebody else's data -- read_anim_block's own rule.
                raw = be32u(f, slot)
                if raw:
                    raise AssertionError(
                        "%s layer %d: table slot %d (0x%X) holds 0x%08X, "
                        "which is not a relocated pointer"
                        % (stage, layer, k, slot, raw))
                continue
            try:
                cmds, floats = A.animjoint_walk(f, intern, t)
            except ValueError as e:
                raise AssertionError("%s layer %d joint %d: script 0x%X: %s"
                                     % (stage, layer, k, t, e))
            seen |= cmds | floats
            heads.append((jbase + k, t))
            n += 1
        if n:
            per_layer.append((layer, n, njoints))

    if not heads:
        return None

    # A SetInterp (0x0D) payload names an `SYInterpDesc`, not a script, so
    # animjoint_walk does not follow it -- it is spline control data, not
    # code. The span below is min..max of what the walk reached, and until
    # every stage's layer scripts kept their descriptors
    # inside that range by luck of layout. Bonus2Luigi's does not: its
    # descriptor and the three float arrays it points at sit BELOW the
    # first script word, and the reloc pass further down raised on the
    # pointer that left the span.
    #
    # Pulling them into `seen` widens the span to cover them, which is all
    # they need: the words between are copied verbatim, and the desc's own
    # three pointers then fall INSIDE the span, so the same reloc pass
    # rebases them with no special case. (This is the layer path's version
    # of what add_interp_descs does for a map object's scripts; a layer's
    # scripts are one flat span rather than per-script
    # entries, so the answer is a wider span rather than more entries.)
    for slot in sorted(x for x in intern if x in seen):
        if slot < 4 or ((be32u(f, slot - 4) >> 25) & 0x7F) != 0x0D:
            continue
        d = intern[slot]
        points_num = struct.unpack_from(">h", f, d + INTERP_POINTS_NUM)[0]

        if points_num < 0 or points_num > 256:
            raise AssertionError("%s: SetInterp at 0x%X names a descriptor "
                                 "at 0x%X with %d points"
                                 % (stage, slot, d, points_num))
        seen |= set(range(d, d + INTERP_DESC_WORDS * 4, 4))

        for (field_off, span) in zip(INTERP_PTRS, (12, 4, 4)):
            a = intern.get(d + field_off)

            if a is None:
                # A NULL array is not a hole, it is the kind saying it
                # does not need that array: every nSYInterpKindLinear
                # descriptor in the twelve Bonus2 layers carries points
                # and keyframes and a NULL at +0x14, because there are
                # no quartic coefficients to a straight line. Only a
                # non-zero word that is not relocated is wrong, and that
                # is the same rule the per-DObj table slots use above.
                if be32u(f, d + field_off):
                    raise AssertionError(
                        "%s: descriptor 0x%X: the pointer at +0x%02X holds "
                        "0x%08X, which is not a relocation"
                        % (stage, d, field_off, be32u(f, d + field_off)))
                continue
            seen |= set(range(a, a + points_num * span, 4))

    start = min(seen) & ~3
    end = max(seen) + 4
    words = list(struct.unpack_from(">%dI" % ((end - start) // 4), f, start))

    entries = [-1] * len(nodes)
    for (j, t) in heads:
        entries[j] = (t - start) // 4

    # Every relocated slot INSIDE the span, as the span-relative word
    # index of its target. A slot the walk actually reached must point
    # back into the span or the script would leave it; one it never
    # reached is somebody else's pointer that happens to lie between two
    # scripts, and it is left holding its ROM word because nothing ever
    # reads it.
    relocs = []
    for slot, target in sorted(intern.items()):
        if not (start <= slot < end):
            continue
        if start <= target < end:
            words[(slot - start) // 4] = (target - start) // 4
            relocs.append((slot - start) // 4)
        elif slot in seen:
            raise AssertionError(
                "%s: layer script pointer at 0x%X names 0x%X, outside the "
                "span 0x%X..0x%X the scripts occupy"
                % (stage, slot, target, start, end))

    return {"words": words, "entries": entries, "relocs": relocs,
            "scripts": len(heads), "per_layer": per_layer,
            "span": (start, end)}


def read_layer_matanims(stage, f, intern, ground, spans, layer_subs):
    """The geometry layers' MatAnimJoints -- read_layer_anims' twin.

    MPGroundDesc.p_matanim_joints (+12) is an `AObjEvent32 ***`: one slot
    per joint of the layer's tree, each either NULL or a per-MOBJ table of
    scripts. gcAddMatAnimJointAll (sys/objanim.c:190-217) walks the tree
    beside it and steps the inner table once per `mobj->next`, so **THE
    JOINT'S MOBJ COUNT IS THE INNER TABLE'S LENGTH** -- nothing terminates
    it, and the tables sit end to end, so a NULL-terminated read of one
    runs straight into the next joint's (Brinstar's first joint reads 18
    scripts where it has 3).

    THE POOL IS THE FILE'S OWN SPAN, for the reason read_layer_anims gives
    at length: an AObjEvent32's Jump/SetAnim pointers are absolute and
    lifting a script out of the file breaks every pointer into its
    neighbours. Measured, the scripts of a layer occupy their span with no
    foreign words in it at all on all five stages that have one, so the
    span costs nothing over copying and is safe where copying is not.

    `layer_subs` is {layer: read_mobjsubs' per-joint lists}, which is what
    says how many MObjs each joint has. Returns None for a stage with no
    layer MatAnimJoint table (four of the nine), else the pack's MObj
    script block plus, per (layer, joint, mobj), the script's word index.
    """
    heads = []                  # ((layer, joint, mobj), script byte offset)
    seen = set()
    per_layer = []

    for (layer, jbase, njoints) in spans:
        tbl = ground["layer_matanims"][layer]
        if tbl is None or layer not in LAYER_ANIM_LAYERS:
            continue
        subs = layer_subs.get(layer) or []
        n = nj = 0
        for k in range(njoints):
            slot = tbl[1] + k * 4
            t = intern.get(slot)
            if t is None:
                # read_layer_anims' rule, for the same reason: every slot
                # of a per-DObj table is a relocated pointer or NULL, and a
                # non-zero raw word means the table is shorter than the
                # tree and this is somebody else's data.
                raw = be32u(f, slot)
                if raw:
                    raise AssertionError(
                        "%s layer %d: MatAnimJoint slot %d (0x%X) holds "
                        "0x%08X, which is not a relocated pointer"
                        % (stage, layer, k, slot, raw))
                continue
            nmobj = len(subs[k]) if k < len(subs) else 0
            if nmobj == 0:
                raise AssertionError(
                    "%s layer %d joint %d has a MatAnimJoint table at 0x%X "
                    "but no MObj to hang it on" % (stage, layer, k, t))
            for mi, s in enumerate(A._ptr_list(intern, t)[:nmobj]):
                try:
                    cmds, floats = A.animjoint_walk(f, intern, s, mat=True)
                except ValueError as e:
                    raise AssertionError(
                        "%s layer %d joint %d MObj %d: MatAnimJoint 0x%X: %s"
                        % (stage, layer, k, mi, s, e))
                seen |= cmds | floats
                heads.append(((layer, k, mi), s))
                n += 1
            nj += 1
        if n:
            per_layer.append((layer, nj, njoints, n))

    if not heads:
        return None

    start = min(seen) & ~3
    end = max(seen) + 4
    words = list(struct.unpack_from(">%dI" % ((end - start) // 4), f, start))

    entry_of = {key: (t - start) // 4 for (key, t) in heads}

    relocs = []
    for slot, target in sorted(intern.items()):
        if not (start <= slot < end):
            continue
        if start <= target < end:
            words[(slot - start) // 4] = (target - start) // 4
            relocs.append((slot - start) // 4)
        elif slot in seen:
            raise AssertionError(
                "%s: MatAnimJoint pointer at 0x%X names 0x%X, outside the "
                "span 0x%X..0x%X the scripts occupy"
                % (stage, slot, target, start, end))

    return {"words": words, "entry_of": entry_of, "relocs": relocs,
            "scripts": len(heads), "per_layer": per_layer,
            "span": (start, end),
            "coverage": 4.0 * len(seen) / (end - start)}


def check_layer_anim_joints(stage, f, intern, ground, spans, nodes):
    """Evidence that each script is on the joint it belongs to.

    Merging four layers renumbers their joints, and the failure that would
    be silent is a table landing off by one: every joint still gets A
    script, just its neighbour's, and the stage animates plausibly and
    wrongly. The alignment itself is already structural -- MeshBaker.bake
    asserts its draw order IS DObjDesc array order, and the decomp asserts
    array order is the animation walk's order by stepping `dobjdesc++`
    beside lbCommonGetTreeDObjNextFromRoot (grdisplay.c:158-179). This is
    the independent second opinion, from the data.

    A layer script OFTEN opens with a zero-duration SetVal writing the
    joint's own bind pose, which is what makes the comparison possible --
    but a DObjDesc's pose is only the artist's snapshot, and a script is
    free to start its cycle somewhere else. Yoshi's Island matches 21 of
    21 and Kongo Jungle 4 of 4; **Peach's Castle matches 0 of 6**, because
    its platform slides between x=+1800 and x=-1800 and was left parked at
    842. So "how many match" cannot be a threshold: absence of the signal
    is not evidence of misalignment.

    What IS evidence is another alignment fitting BETTER. This scores the
    same comparison at shifts -2..+2 and fails when some other shift beats
    shift 0 by a margin -- which is the shape a real off-by-one makes
    (Yoshi's Island's 21 of 21 would move wholesale to shift -1) and which
    no stage whose scripts simply start elsewhere can produce, since their
    misses miss at every shift.

    Two things keep that from crying wolf. A track whose bind value is
    ZERO is not compared at all: bind poses are full of zeros, so a zero
    matching a zero one joint over is coincidence and nothing else --
    Peach's Castle's spinner opens at RotZ 0.0 and every joint around it
    is unrotated. And the margin is 2, not 1, so a single lucky hit is
    not a verdict.

    Returns (matched, compared) at shift 0, over individual TRACKS.
    """
    SHIFTS = (-2, -1, 0, 1, 2)
    score = {s: 0 for s in SHIFTS}
    compared = 0

    for (layer, jbase, njoints) in spans:
        tbl = ground["layer_anims"][layer]
        if tbl is None or layer not in LAYER_ANIM_LAYERS:
            continue
        for k in range(njoints):
            t = intern.get(tbl[1] + k * 4)
            if t is None:
                continue
            w = be32u(f, t)
            op, flags, dur = (w >> 25) & 0x7F, (w >> 15) & 0x3FF, w & 0x7FFF
            # SetValBlock (3) / SetVal (4) with no duration: the value IS
            # the pose, one float per set track bit. (The enum is not the
            # obvious order -- sys/objdef.h:189: 3 is the BLOCKING one.)
            if op not in (0x03, 0x04) or dur != 0:
                continue
            vals = [struct.unpack_from(">f", f, t + 4 * (1 + i))[0]
                    for i in range(bin(flags).count("1"))]
            for shift in SHIFTS:
                j = jbase + k + shift
                if not (jbase <= j < jbase + njoints):
                    continue
                _parent, tra, rot, sca = nodes[j]
                # AObj joint track order (sys/objdef.h AOBJ_FLAG_*): RotX,
                # RotY, RotZ, TraI, TraX, TraY, TraZ, ScaX, ScaY, ScaZ.
                # TraI is None because it is not a component of the bind
                # pose -- it is the translate INTERPOLATION track -- so it
                # is the one bit there is nothing to compare against.
                bind = list(rot) + [None] + list(tra) + list(sca)
                i = 0
                for bit in range(10):
                    if not (flags & (1 << bit)):
                        continue
                    want, got = bind[bit], vals[i]
                    i += 1
                    if want is None or want == 0.0:
                        continue
                    if shift == 0:
                        compared += 1
                    if abs(want - got) <= abs(want) * 1e-4:
                        score[shift] += 1

    best = max(score.values())
    if best >= score[0] + 2:
        raise AssertionError(
            "%s: the layer animation tables fit the merged joints better "
            "at shift %s (%d of %d opening poses) than where they were put "
            "(%d) -- the tables are not aligned with the joints"
            % (stage, [s for s in SHIFTS if score[s] == best], best,
               compared, score[0]))
    return score[0], compared


def matanim_table(intern, mo):
    """The per-DObj table a `MatAnimJoint <name>` block names.

    The descriptions point at these blocks at different relative
    positions, and both shapes are in the tree: Yoshi's Island's
    `MatAnimJoint CloudSolid 0x670` IS the table's own third word (the
    table starts 8 bytes earlier, at `mobjlink_0x0668`), while Dream
    Land's `MatAnimJoint WhispyEyesLeftTurn 0x11E0` is the START of such
    a table and its slot is the word at 0x11E8. Dream Land's mouth
    blocks are a word longer again (the slot at +0x0C). So the block is
    resolved by finding the chain slot in its first words rather than at
    one fixed offset: the slot is the only word in that run that is a
    relocated pointer.
    """
    for k in range(4):
        t = intern.get(mo + k * 4)

        if t is not None:
            return t
    return mo


def read_mobj_scripts(f, reloc, table_off):
    """A MatAnimJoint's scripts, as ONE word array for the pack's MObjs
    section: the words, the word index each script starts at, and the
    indices that hold a pointer.

    The game hands lbCommonAddTreeDObjsAnimAll an `AObjEvent32 ***` -- one
    PER-MOBJ SCRIPT TABLE PER DObj of the tree it is being attached to --
    so the symbol the stage adds to `map_head` is the address of that
    per-DObj table, and its first entry is the per-DObj table's own list
    of scripts, one per MObj. A graft is one DObj with one MObj, so its
    entry 0 is the whole of it. src/relocData/154_StageYosterFile3.c
    spells the nesting out for Yoshi's Island: `mobjlink_0x0668[3]` is
    the per-DObj table (the descriptions' `MatAnimJoint CloudSolid
    0x670` is its third word), `mobjlink_0x0688[2]` is the per-MObj one,
    and `AnimJoint_0x0674` is the script.

    FPackMObjs carries the scripts the way FPackAnim does -- host-order
    words with every relocated pointer rewritten as a word index -- since
    gcAddMObjMatAnimJoint walks them. A script's only pointer is the loop
    back to itself or sideways to a sibling.
    """
    dobs = A._ptr_list(reloc, table_off)
    if not dobs:
        raise AssertionError("MatAnimJoint 0x%X has no per-DObj table"
                             % table_off)
    scripts = A._ptr_list(reloc, dobs[0])
    if not scripts:
        # One level shallower, and Dream Land is the case: its per-DObj
        # tables hold the scripts directly (the eyes' is
        # `{NULL, NULL, script}`, the table at 0x11E0's slot leading to
        # 0x1214) where Yoshi's Island's hold a per-MObj table
        # (`mobjlink_0x0688[2] = { AnimJoint_0x0674, NULL }`). An entry
        # that is not itself a list of pointers IS a script, so the outer
        # list is the script list.
        scripts = dobs
    parsed = [(s, read_matanim_script(f, reloc, s)) for s in scripts]
    start = {}
    words = []
    for s, (w, _fx) in parsed:
        start[s] = len(words)
        words.extend(w)
    relocs = []
    for s, (_w, fx) in parsed:
        base = start[s]
        for (wi, target, _op) in fx:
            if target not in start:
                raise AssertionError("MatAnimJoint script 0x%X word %d "
                                     "points at 0x%X, which is not one of "
                                     "the table's own scripts"
                                     % (s, wi, target if target else 0))
            words[base + wi] = start[target]
            relocs.append(base + wi)
    return words, [start[s] for s, _ in parsed], relocs


def read_matanim_script(f, intern, off):
    """One MatAnimJoint script, as read_anim_script's (words, fixups).

    Two things make this its own reader rather than read_anim_script with a
    flag. The opcode set is the MATERIAL one -- the stream
    gcParseMObjMatAnimJoint reads, where the SetExtVal opcodes are defined
    and SetInterp and the FuncAnims are not (ssb_assets.animjoint_walk says
    the same thing, and why the script itself cannot tell you which it is).
    And the scripts END differently: a joint script is a block that stops
    at AOBJ_END, while every one of ef/efground.c's fifteen ground-actor
    material scripts runs its frames and then LOOPS, with a Jump back to
    its own first word and no terminator behind it. read_anim_script walks
    straight past that Jump and off the end of the block, which is what
    `opcode 0x7F at word 47` was.

    So: linear like read_anim_script (the pack stores a script as a
    contiguous word run and the runtime parser walks it), stopping at
    AOBJ_END or at the Jump/SetAnim that takes control elsewhere -- either
    way the last word emitted is the last word of this script.
    """
    words, fixups = [], []
    while True:
        w = be32u(f, off + 4 * len(words))
        op, flags, _ = A._aj_decode(w)
        words.append(w)

        if op == A.AJ_END:
            break
        if op in (A.AJ_JUMP, A.AJ_SETANIM):
            # the target (this script itself, for the self-loop all
            # fifteen end with) is carried as an index, the way
            # read_anim_script carries one
            t = intern.get(off + 4 * len(words))
            if t:
                fixups.append((len(words), t, op))
            words.append(0)
            break
        if op in (A.AJ_WAIT, A.AJ_ADDLENGTH):
            continue
        if op not in A._AJ_TRACK_OPS and op not in A._AJ_MAT_EXT_OPS:
            raise AssertionError("MatAnimJoint 0x%X: opcode 0x%02X at word "
                                 "%d is not a MObj AObjEvent32Kind"
                                 % (off, op, len(words) - 1))
        per_bit = 2 if op in (A.AJ_SETVALRATE_BLOCK, A.AJ_SETVALRATE) else 1
        for _ in range(per_bit * bin(flags).count("1")):
            words.append(be32u(f, off + 4 * len(words)))
    return words, fixups


def read_ground_matanim(f, reloc, mo, subs, where):
    """A GROUND ACTOR's MatAnimJoint, as read_mobj_scripts' (words,
    entries, relocs) -- but read per JOINT, because a ground actor's
    animated MObj is not on joint 0.

    Every one of the fifteen tables in the game has the same shape, and
    build/scratch/ga_matjoint.py dumped it: the symbol IS the per-DObj
    table, one slot per joint of the actor's (two-joint) tree, slot 0 NULL
    and slot 1 pointing at a one-entry per-MObj table whose entry is the
    script. read_mobj_scripts cannot be used for it -- it takes `dobs[0]`
    and would read the NULL slot as "no per-DObj level", collapsing joint
    1's script onto joint 0.

    The shape is asserted rather than assumed: the joints that carry a
    matanim slot have to be exactly the joints that carry MObjs, which is
    also what says the block really does start at the symbol (a table read
    one slot early would put the pointer on joint 2 and fail here).

    The per-MObj table is bounded by the JOINT'S OWN MObj CHAIN, not by a
    NULL terminator: lbCommonAddTreeDObjsAnimAll's inner loop is
    `while (mobj != NULL) { ...; mobj = mobj->next; matanim_joints++; }`
    (lb/lbcommon.c:865-875), so it stops when the DObj runs out of MObjs
    and never looks at the slot after. The nine shipping ground actors
    cannot tell the two readings apart -- their tables end in a NULL at
    exactly the MObj count -- but Final Destination's boss-wallpaper
    Effects2_1 can: its four one-MObj joints point at 0x115A4, 0x115A8,
    0x115AC and 0x115B0, four overlapping windows onto ONE run of four
    script pointers, so a NULL-terminated read hands joint 2 all four.

    `entries` is flat over the pack's MObjs in the order mobj_pack_section
    flattens them -- joint-major, the pack's own numbering -- with -1 for a
    MObj the table leaves alone.
    """
    flat = [(j, mi) for j, lst in enumerate(subs) for mi in range(len(lst))]
    have = {}

    slots = set()
    for j in range(len(subs)):
        t = reloc.get(mo + j * 4)

        if t is None:
            continue
        slots.add(j)
        for mi in range(len(subs[j])):
            s = reloc.get(t + mi * 4)
            if s is None:
                raise AssertionError(
                    "%s: the MatAnimJoint at 0x%X gives joint %d only %d of "
                    "its %d MObjs a script" % (where, mo, j, mi,
                                               len(subs[j])))
            have[(j, mi)] = s

    if set(have) != set(flat) or slots != {j for j, _mi in flat}:
        raise AssertionError(
            "%s: the MatAnimJoint at 0x%X drives joints %s / MObjs %s but "
            "the MObjSub chain has %s" % (where, mo, sorted(slots),
                                          sorted(have), sorted(flat)))

    parsed = [(s, read_matanim_script(f, reloc, s)) for s in have.values()]
    start = {}
    words = []
    for s, (w, _fx) in parsed:
        start[s] = len(words)
        words.extend(w)
    relocs = []
    for s, (_w, fx) in parsed:
        base = start[s]
        for (wi, target, _op) in fx:
            if target not in start:
                raise AssertionError(
                    "%s: MatAnimJoint script 0x%X word %d points at 0x%X, "
                    "which is not one of the table's own scripts"
                    % (where, s, wi, target if target else 0))
            words[base + wi] = start[target]
            relocs.append(base + wi)
    return words, [start[have[k]] for k in flat], relocs


def mobj_pack_section(baker, mobjsubs, scripts, tex=None, uv=None,
                      uv0=None):
    """The pack's MObjs section (fighter.h FPackMObjs), as bytes.

    The recipe is tools/export/ssb_emblemexport.py's, which is the only other
    exporter that writes one -- and the only two packs that need one are
    the ones whose materials are *animated* rather than baked: an emblem
    recoloured per player, and (here) a stage object whose material
    script switches it between states. `baker` must have been built with
    `mobj_batches=True`, so each batch remembers which MObj drew it.

    `scripts` is (words, entries, relocs) from read_mobj_scripts, for the
    one MatAnimJoint (alt_count 1) -- every pack but the dead
    explosion's carries one.

    `tex` is one (tex_first, tex_count) per MObj when the MObj animates
    its PICTURE and the caller has already appended the run to the
    baker's texture table (bake_map_object, for a ground actor); None
    leaves every MObj on the single frame its own batch drew, which is
    what a material script that only moves colours or UVs needs.

    `uv` and `uv0` are map_object_scroll's: the two tiles' scroll pairs,
    by flat MObj index, for the MObjs whose script moves a tile.
    """
    joint_first = []
    mobjs = []

    for lst in mobjsubs:
        joint_first.append((len(mobjs), len(lst)))
        mobjs.extend(lst)

    batch_mobj = []
    for b in baker.batches:
        m = b.get("mobj")

        if m is None or m[0] < 0 or m[1] >= len(mobjsubs[m[0]]):
            batch_mobj.append(-1)
        else:
            batch_mobj.append(joint_first[m[0]][0] + m[1])

    # which texture each MObj's batch draws: with no sprite array to step
    # through that is the whole of the MObj's, so tex_count below is 1
    # and this is tex_first. -1 where the batch is untextured.
    batch_tex = [-1] * len(mobjs)
    for i, b in enumerate(baker.batches):
        if batch_mobj[i] >= 0 and batch_tex[batch_mobj[i]] < 0:
            batch_tex[batch_mobj[i]] = b["mat"][0]

    # One (words, entries, relocs) per whole MatAnimJoint -- the pack's
    # `alt`s. The word array is all of them concatenated and each alt's
    # own indices are rebased into it, so an alt is a contiguous run.
    #
    # An alt's `entries` is per-MObj in the pack's own flat order, short
    # (read_mobj_scripts, which numbers its scripts as it finds them) or
    # full-length with -1 holes (read_ground_matanim, which knows which
    # MObj each script belongs to). Both are padded to one row of
    # mobj_count here, so the blob below is simply the rows concatenated.
    words, relocs, base = [], [], 0
    all_entries = []
    for (w, entries, rl) in scripts:
        w = list(w)
        # a pointer word is a word index into the one array the loader
        # relocates, so it moves with its alt exactly as the entries do;
        # an alt at base 0 (every pack with one alt) is unchanged
        for i in rl:
            w[i] += base
        words.extend(w)
        for i in rl:
            relocs.append(base + i)
        row = list(entries) + [-1] * (len(mobjs) - len(entries))
        all_entries.extend([(base + e) if e >= 0 else -1
                            for e in row[:len(mobjs)]])
        base += len(w)

    subs_blob = b"".join(
        # tex_first/tex_count: the sprite array a MatAnimJoint steps
        # through (fighter.h FPackMObjSub). One frame, the batch's own
        # texture, unless the caller filled batch_tex or `tex` in.
        A.pack_mobjsub(sub, *(tex[k] if tex else (batch_tex[k], 1)),
                       uv=(uv or {}).get(k), uv0=(uv0 or {}).get(k))
        for k, sub in enumerate(mobjs))
    joint_blob = struct.pack("<%dh" % (2 * len(mobjsubs)),
                             *[v for pair in joint_first for v in pair])
    batch_blob = struct.pack("<%dh" % len(batch_mobj), *batch_mobj)
    # the MObj's script as a word index, -1 for none, alternate by
    # alternate: alt_count * mobj_count of them, alt-major
    entry_blob = struct.pack("<%di" % len(all_entries), *all_entries)
    words_blob = struct.pack("<%dI" % len(words), *words) if words else b""
    reloc_blob = (struct.pack("<%dI" % len(relocs), *relocs) if relocs
                  else b"")
    return (subs_blob, joint_blob, batch_blob, entry_blob, words_blob,
            reloc_blob)


def align(b):
    return b + b"\0" * (-len(b) % 4)

def bake_map_object(rom, fid, o):
    """The map object's own DObjDesc tree, baked into one pack.

    The game builds these GObjs with gr/grmodelsetup.c's
    grModelSetupGroundDObjs, straight off the map file's DObjDesc and its
    N64 display lists. The port draws nothing from an N64 DL, so the tree
    is baked here instead -- the same reading the stage's four visual
    layers get, one tree, z-buffered and opaque, at the transform kinds
    the stage's own file names. What the port keeps from the game is the
    shape: the tree's own joints, in the map file's own order, because
    the stage's AnimJoint scripts address them by index.
    """
    import pygfxd

    f, _, reloc, _, _ = L.file_info(rom, fid)

    if o.get("dl") is not None:
        # An object with no DObjDesc of its own: ONE DObj carrying a
        # display list. Mushroom Kingdom's platforms are the case -- the
        # game gives each its own GObj and then
        # `gcAddDObjForGObj(gobj, map_head + &llGRInishieMapMapHead)`,
        # which is objman.c:1389, and that sets `new_dobj->dv = dvar`:
        # the tree is one joint and the block map_nodes names is the list
        # it draws. The bake is the graft's shape, an identity root the
        # tree walk needs and one node under it; the root is not a joint
        # (DObjNode index -1, which baker.bake drops) so the pack carries
        # the one.
        root = A.DObjNode(-1, -1, None, (0.0, 0.0, 0.0), (0.0, 0.0, 0.0),
                          (1.0, 1.0, 1.0))
        nodes = [A.DObjNode(0, 0, o["dl"], (0.0, 0.0, 0.0),
                            (0.0, 0.0, 0.0), (1.0, 1.0, 1.0))]
        nodes[0].parent = root
        root.children.append(nodes[0])
    else:
        root, nodes = A.read_dobj_tree(f, reloc, o["off"],
                                       dl_links=bool(o.get("dl_links")))
    # The object's material chain, the same read the four visual layers
    # get: entry i of the array is the MObj list DObjDesc[i]'s DObj
    # carries, and the baker folds each into the batches that joint drew
    # (MeshBaker._apply_mobj) unless the object's materials are ANIMATED,
    # in which case each MObj is kept so the script can move it --
    # mobj_batches, which is what Zebes' acid and Dream Land's mouth both
    # need.
    subs = []
    if o.get("mobjsub") is not None:
        subs = A.read_mobjsubs(f, reloc, o["mobjsub"], len(nodes))
    matanims = o.get("matanims") or ()
    animated = len(matanims) > 0
    ground = bool(o.get("ground"))
    # The render mode each DL head is ALREADY in when the tree is drawn.
    # A map object and a ground actor are drawn by the stage's own display
    # proc, which leaves the z-buffered modes gr/grdisplay.c set; a tree
    # whose own display proc sets something else says so (Final
    # Destination's boss wallpaper -- see read_boss_wallpaper).
    seed = o.get("rendermode") or [A.G_RM_AA_ZB_OPA_SURF,
                                   A.G_RM_AA_ZB_XLU_SURF, None, None]

    # The other-mode H word per head, likewise, where the tree's display
    # proc sets one (the boss fog's G_CYC_2CYCLE -- read_boss_wallpaper).
    state_seed = ({"othermode_h": list(o["othermode_h"])}
                  if o.get("othermode_h") else None)

    def rebake():
        b = A.MeshBaker(f, pygfxd, mobjsubs=subs, mobj_batches=animated,
                        rendermode=seed, seed=state_seed,
                        texture_files=texture_files(rom, fid))
        b.bake(root)
        return b

    baker = rebake()

    out_nodes, verts, tris, batches, texs, palettes = baker_to_pack(baker)
    # ef/efground.c:1328-1343: a ground actor's DObjDesc id with any of
    # 0xF000 set is a billboard joint. Pack joint k is DObjDesc k.
    for k, n in enumerate(baker.nodes):
        assert n.index == k, (fid, o.get("name"), k, n.index)
    o["billboard"] = [k for k, n in enumerate(baker.nodes) if n.flags]
    mobjsec = None
    mobj_count = 0

    if animated:
        # One MatAnimJoint per state: mobj_pack_section lays them out as
        # the pack's `alt`s, and the stage's own code picks one by index.
        # A ground actor's table is read per JOINT (its animated MObj is
        # on joint 1, not joint 0) and is the only kind whose script
        # steps the MObj's PICTURE, so only it grows a texture run.
        alts = []

        for mo in matanims:
            if ground:
                scripts = read_ground_matanim(f, reloc, mo, subs,
                                              "%s %s" % (fid, o.get("name")))
            else:
                scripts = read_mobj_scripts(f, reloc, mo)
            if not scripts[0]:
                raise AssertionError("%s: the MatAnimJoint table at 0x%X has "
                                     "no scripts" % (fid, mo))
            alts.append(scripts)
        mobj_count = len([s for lst in subs for s in lst])
        tex = ground_mobj_frames(f, reloc, subs, matanims, baker, rebake,
                                 texs, batches, o, fid) if ground else None
        uv, uv0 = map_object_scroll(
            f, reloc, subs, matanims, baker, ground,
            "%s %s" % (fid, o.get("name") or "map object 0x%X"
                       % (o.get("off") or 0)))
        mobjsec = mobj_pack_section(baker, subs, alts, tex, uv, uv0)
        # for the build log: how many pictures the busiest MObj carries
        o["frames"] = max([c for (_t, c) in tex], default=1) if tex else 1

    return (out_nodes, verts, tris, batches, texs, palettes, mobjsec,
            mobj_count, len(matanims) if animated else 1)


def map_object_scroll(f, reloc, subs, matanims, baker, ground, owner):
    """merge_layers' tile scroll (mobj_scroll) for one map object's MObjs:
    ({flat MObj index: uv}, {flat MObj index: uv0}).

    Each MObj's scripts are found the way the object's own reader found
    them: a ground actor's table is per joint, then per MObj
    (read_ground_matanim, and ground_mobj_frames' own lookup), and any
    other object's is one flat script list, one per MObj in the pack's
    order (read_mobj_scripts).
    """
    flat = [(j, mi) for j, lst in enumerate(subs) for mi in range(len(lst))]
    batch_of = {}
    for i, b in enumerate(baker.batches):
        m = b.get("mobj")
        if m is not None and m[0] == b["node"].index and m not in batch_of:
            batch_of[m] = i
    uv, uv0 = {}, {}
    for k, (j, mi) in enumerate(flat):
        scripts = []
        for mo in matanims:
            if ground:
                lst, at = A._ptr_list(reloc, reloc[mo + j * 4]), mi
            else:
                dobs = A._ptr_list(reloc, mo)
                lst, at = A._ptr_list(reloc, dobs[0]) or dobs, k
            scripts.append(lst[at] if at < len(lst) else None)
        a, b, note = mobj_scroll(f, reloc, baker, subs[j][mi],
                                 batch_of.get((j, mi)), scripts,
                                 (owner, j, mi))
        if a is not None:
            uv[k] = a
            print("%s joint %d MObj %d: second tile scrolls %+.3f,%+.3f per "
                  "unit" % (owner, j, mi, a[2], a[3]))
        if b is not None:
            uv0[k] = b
            print("%s joint %d MObj %d: first tile scrolls %+.3f,%+.3f per "
                  "unit" % (owner, j, mi, b[2], b[3]))
        if note is not None:
            print(note)
    return uv, uv0


def ground_mobj_frames(f, reloc, subs, matanims, baker, rebake, texs,
                       batches, o, fid):
    """The extra pictures a ground actor's MatAnimJoint steps through, as
    one contiguous (tex_first, tex_count) run per MObj.

    merge_layers' recipe (which the four frame-cycle STAGE LAYERS already
    use) on one tree instead of four merged ones: the script says how many
    frames of the MObj's sprite array it ever reaches
    (ssb_assets.matanim_frame_count -- the array cannot answer, see its own
    note), and each frame is the same tree baked again with the MObjSub
    wound forward, which is all MeshBaker._apply_mobj needs to pick the
    next entry as the tile. Only the picture is supposed to change, and the
    re-bake is checked to prove it.

    The run is appended WHOLE, frame 0 included even though the batch
    already interned it: FPackMObjSub.tex_first/tex_count is a run and
    fighter.c compiles one poly header per entry of it (fighter.c:610-628),
    so frame N has to be tex_first + N.

    Returns one (first, count) per MObj in the pack's flat order -- the
    MObj's own single tile where its script does not step the picture,
    because that is what the field means.
    """
    flat = [(j, mi) for j, lst in enumerate(subs) for mi in range(len(lst))]
    want = {}

    for j, mi in flat:
        # read_ground_matanim has already checked that every MObj has a
        # slot in every alt, so this indexes rather than searches. An alt
        # is a whole separate state of the same MObj (Dream Land's two
        # Brontos), so the run has to be as long as the longest of them.
        scripts = [A._ptr_list(reloc, reloc[mo + j * 4])[mi]
                   for mo in matanims]
        # A MObj that flips `texture_id_curr` indexes `sprites`; one that
        # flips `palette_id` indexes `palettes`. layer_mobj_frames asks
        # the same two questions of a stage LAYER's MObjs and says why
        # texture_id_next counts into the sprite run. No ground actor in
        # the game winds a palette (build/scratch/ga_mat_tracks.py) --
        # Final Destination's boss-wallpaper Effects2_1 is the first tree
        # read through here that does, and it winds nothing else: its
        # MObjs are flags=MOBJ_FLAG_PALETTE with an EMPTY sprite array,
        # so `palettes` is their picture array outright (_apply_mobj
        # takes the entry as `timg`, not as a TLUT).
        for bits, name in (((A.MATANIM_BIT_TEXID,
                             A.MATANIM_BIT_TEXID_NEXT), "sprites"),
                           ((A.MATANIM_BIT_PALETTEID,), "palettes")):
            n = max(A.matanim_frame_count(f, reloc, s, b) for s in scripts
                    for b in bits)
            if n <= 1:
                continue
            if (j, mi) in want:
                raise AssertionError(
                    "%s %s joint %d MObj %d drives both texture_id_curr "
                    "and palette_id; the pack has one picture array per "
                    "MObj" % (fid, o.get("name"), j, mi))
            have = len(subs[j][mi][name] or ())
            if have < n:
                raise AssertionError(
                    "%s %s joint %d MObj %d: the MatAnimJoint reaches "
                    "%s[%d] but the array reads %d entries"
                    % (fid, o.get("name"), j, mi, name, n - 1, have))
            want[(j, mi)] = (n, name)

    # which batch each animated MObj drew -- `mobj` is the last segment
    # 0xE branch the walk saw and carries across display lists, so it is
    # this batch's own MObj only when its joint agrees
    batch_of = {}
    for i, b in enumerate(baker.batches):
        m = b.get("mobj")
        if m is not None and m[0] == b["node"].index and m not in batch_of:
            batch_of[m] = i

    frames = {}
    for key in sorted(want):
        if key not in batch_of:
            raise AssertionError(
                "%s %s: joint %d MObj %d has a MatAnimJoint but drew no "
                "batch of its own" % ((fid, o.get("name")) + key))
        if baker.batches[batch_of[key]]["mat"][0] < 0:
            raise AssertionError(
                "%s %s: joint %d MObj %d steps a picture but its batch "
                "samples no texture" % ((fid, o.get("name")) + key))
        frames[key] = [texs[baker.batches[batch_of[key]]["mat"][0]]]

    # A MObj whose batches draw DIFFERENT pictures steps each of them: the
    # MObj picks the TLUT and the display list's own tile loads decide the
    # picture. Final Destination's Effects2_1 is four one-MObj joints each
    # drawing five stacked slices of one image under the one palette_id.
    # The pack's run is the MObj's, so without this every slice drew the
    # FIRST batch's picture (the wall showed one slice five times, its
    # lines meeting nowhere). Such a batch carries its own run
    # (FPACK_OWNRUN: frame k is its tex + k); a MObj whose batches all
    # draw one picture is untouched.
    xframes = {}
    for key in sorted(want):
        first = baker.batches[batch_of[key]]["mat"][0]
        for i, b in enumerate(baker.batches):
            m = b.get("mobj")
            if m == key and m[0] == b["node"].index and i != batch_of[key] \
                    and b["mat"][0] >= 0 and b["mat"][0] != first:
                xframes[i] = (key, [texs[b["mat"][0]]])
    ownrun_keys = {k for (k, _l) in xframes.values()}

    for fr in range(1, max([n for (n, _w) in want.values()], default=0)):
        for (j, mi), (n, which) in want.items():
            field = ("texture_id_curr" if which == "sprites"
                     else "palette_id")
            subs[j][mi][field] = min(fr, n - 1)
        b2 = rebake()
        if (len(b2.verts), len(b2.tris), len(b2.batches), b2.palettes) != \
                (len(baker.verts), len(baker.tris), len(baker.batches),
                 baker.palettes):
            raise AssertionError(
                "%s %s: frame %d bakes to different geometry; only the "
                "picture is supposed to change" % (fid, o.get("name"), fr))
        for key, (n, _which) in want.items():
            if fr < n:
                frames[key].append(b2.textures[
                    b2.batches[batch_of[key]]["mat"][0]])
        for i, (key, lst) in xframes.items():
            if fr < want[key][0]:
                lst.append(b2.textures[b2.batches[i]["mat"][0]])
    for (j, mi), (_n, which) in want.items():
        subs[j][mi]["texture_id_curr" if which == "sprites"
                    else "palette_id"] = 0

    # Two MObjs stepping the SAME pictures share one run. The run has to
    # be contiguous (fighter.c compiles one poly header per entry of
    # tex_first/tex_count), so the pictures cannot simply be interned one
    # at a time the way the baker interns a tile -- but a whole run can
    # be, and identical runs are the rule rather than the exception here:
    # Final Destination's boss wallpaper draws its Effects2_1 tree with
    # four MObjs that step the same eighteen TLUTs over the same tile,
    # and its Effects1 tree with eight that step the same two, so the
    # four copies of an 18 x 256x32 run alone are 950 KB. A run is
    # identified by its frames' TexKeys, which are the baker's own intern
    # identity and carry the TLUT, so two runs share only when every
    # frame would have baked to the same texels.
    for i, (key, lst) in xframes.items():
        frames[key + (i,)] = lst
    tex_first = {}
    run_at = {}
    for key in sorted(frames):
        sig = tuple(t["key"] for t in frames[key])
        if sig in run_at:
            tex_first[key] = run_at[sig]
            continue
        run_at[sig] = tex_first[key] = len(texs)
        texs.extend(dict(t) for t in frames[key])
    for key, i in batch_of.items():
        if key in tex_first:
            b = batches[i]
            batches[i] = (b[0], b[1], (tex_first[key],) + tuple(b[2][1:]),
                          b[3], b[4] | (A.FPACK_OWNRUN
                                        if key in ownrun_keys else 0))
    for i, (key, _lst) in xframes.items():
        b = batches[i]
        batches[i] = (b[0], b[1], (tex_first[key + (i,)],) + tuple(b[2][1:]),
                      b[3], b[4] | A.FPACK_OWNRUN)

    own = {}
    for key, i in batch_of.items():
        own[key] = batches[i][2][0]
    return [(tex_first[k], len(frames[k])) if k in tex_first
            else (own.get(k, -1), 1) for k in flat]


def baker_to_pack(baker):
    """A MeshBaker's result as model_sections' four arrays: the joints,
    the vertices, the triangles and the batches."""
    nodes = []
    for n in baker.nodes:
        parent = n.parent.index if n.parent.index >= 0 else -1
        nodes.append((parent, n.translate, n.rotate, n.scale))
    verts = [(x, y, z, u, v, nx, ny, nz, alpha, joint)
             for (x, y, z, u, v, nx, ny, nz, alpha, joint) in baker.verts]
    tris = [(a, b, c) for (a, b, c) in baker.tris]
    batches = []
    for b in baker.batches:
        (tex, prim, l1, l2, uses_shade, lit,
         a_tex, a_shade, env) = b["mat"][:9]
        # The eleventh element, "the colour cycle cross-fades two tiles"
        # (model_sections' texlerp), rides along: without it Zebes's acid
        # and Final Destination's cloud batches drew tile A alone.
        # Elements 12 and 13 are which halves of PRIM the combiner reads
        # (fighter.h FPACK_COLOR_PRIM, FPACK_ALPHA_PRIM). The shorter tuple
        # this used to hand over meant "PRIM whole", both bits set on every
        # stage batch; no live PRIM ever reaches a stage pack, so that drew
        # the same, but the bits now say what the combiner does.
        mat = (tex, prim, l1, l2, uses_shade, lit, a_tex, a_shade, env)
        if len(b["mat"]) > 13:
            mat += (b["mat"][9], b["mat"][10], None,
                    b["mat"][12], b["mat"][13])
        elif len(b["mat"]) > 10:
            mat += (b["mat"][9], b["mat"][10])
        batches.append((b["tri_first"], b["tri_count"], mat,
                        b["node"].index, b["bucket"]))
    return nodes, verts, tris, batches, baker.textures, baker.palettes


def bake_graft_object(rom, obj):
    """The object a stage's own code GRAFTS under its map tree, as its own
    pack: ONE DObj carrying a display list and a material, built straight
    off the map file's `DisplayList` block with no DObjDesc of its own.

    Yoshi's Island is the case. grYosterInitAll builds each cloud's GObj
    tree from the map's `MapHead` skeleton -- which is a bare skeleton,
    five DObjDesc entries and no display lists at all -- and then, per
    cloud joint, `gcAddChildForDObj(coll_dobj, map_head +
    &llGRYosterMapCloudDisplayList)` and
    `lbCommonAddMObjForTreeDObjs(cloud_dobj, map_head +
    &llGRYosterMap_4B8_MObjSub)`. The port's whole substitute for that
    pair is this pack plus dc_model_add_dobjs' own `parent` argument.

    Its material is ANIMATED rather than baked, and the pack carries
    BOTH of the object's MatAnimJoints as its two `alt`s -- because
    `dGRYosterCloudMatAnimJoints[2]` is exactly that pair, the stage's
    state machine names which is current, and the port picks with
    dc_model_add_mobjs_alt.
    """
    cfg = obj.get("graft")
    if cfg is None:
        return None
    import pygfxd

    f, _, reloc, _, _ = L.file_info(rom, obj["fid"])
    root = A.DObjNode(-1, -1, None, (0.0, 0.0, 0.0), (0.0, 0.0, 0.0),
                      (1.0, 1.0, 1.0))
    node = A.DObjNode(0, 0, cfg["dl"], (0.0, 0.0, 0.0), (0.0, 0.0, 0.0),
                      (1.0, 1.0, 1.0))
    node.parent = root
    root.children.append(node)

    subs = A.read_mobjsubs(f, reloc, cfg["mobjsub"], 1)
    baker = A.MeshBaker(f, pygfxd, mobjsubs=subs, mobj_batches=True,
                        rendermode=[A.G_RM_AA_ZB_OPA_SURF,
                                    A.G_RM_AA_ZB_XLU_SURF, None, None],
                        texture_files=texture_files(rom, obj["fid"]))
    baker.bake(root)

    scripts = [read_mobj_scripts(f, reloc, t) for t in cfg["matanim"]]
    mobjsec = mobj_pack_section(baker, subs, scripts)
    n = len([sub for lst in subs for sub in lst])
    return baker_to_pack(baker) + (mobjsec, n, len(scripts))


def object_pack(nodes, verts, tris, batches, textures, palettes, name,
                mobjsec=None, mobj_count=0, mobj_alts=1):
    """One SSBPACKA blob (src/dc/fighter.h:40): the same layout
    ssb_packexport.py writes for a fighter, without attributes, motion or
    scripts -- a stage's map object animates through the game's own
    AObjEvent32 scripts, not through the port's motion format.

    `mobjsec` is mobj_pack_section's six blobs, when the object's
    materials are animated rather than baked: they go after the model's
    own sections and the header's off_mobjs points at the first of them
    (the FPackMObjs itself). 0 leaves the pack with no MObjs, which is
    every stage layer and every map object but Zebes' acid and Yoshi's
    clouds."""
    secs = model_sections(nodes, verts, tris, batches, textures, palettes)

    off = 128
    offsets = {}
    body = b""
    for key in ("joints", "verts", "tris", "batches", "texs", "pals",
                "texdata"):
        sec = align(secs[key])
        offsets[key] = off
        body += sec
        off += len(sec)
    offsets["anims"] = off
    # the MObjs section: its own header (FPackMObjs's twelve words), then the
    # six arrays it names. `alt_count` is 1 -- one MatAnimJoint, which is
    # every pack but the dead explosion's.
    mobjs_off = 0
    alts = mobj_alts
    if mobjsec is not None:
        subs_blob, joint_blob, batch_blob, entry_blob, words_blob, \
            reloc_blob = (align(x) for x in mobjsec)
        alts = mobj_alts
        mobjs_off = off
        mhdr = off + 48
        mhdr = struct.pack("<12I", mobj_count, mhdr,
                           mhdr + len(subs_blob),
                           mhdr + len(subs_blob) + len(joint_blob),
                           mhdr + len(subs_blob) + len(joint_blob) +
                               len(batch_blob),
                           mhdr + len(subs_blob) + len(joint_blob) +
                               len(batch_blob) + len(entry_blob),
                           len(words_blob) // 4,
                           mhdr + len(subs_blob) + len(joint_blob) +
                               len(batch_blob) + len(entry_blob) +
                               len(words_blob),
                           len(reloc_blob) // 4, alts, 0, 0)
        body += mhdr + subs_blob + joint_blob + batch_blob + entry_blob + \
            words_blob + reloc_blob
    return struct.pack("<8s8I8I3ff8s8I", b"SSBPACKA",
                       len(nodes), len(verts), len(tris), len(batches),
                       len(textures), len(palettes), 0,
                       len(secs["texdata"]),
                       offsets["joints"], offsets["verts"], offsets["tris"],
                       offsets["batches"], offsets["texs"], offsets["pals"],
                       offsets["texdata"], offsets["anims"],
                       0.0, 0.0, 0.0, 0.0, name.encode()[:8],
                       0, 0, 0, 0, 0, 0, 0, mobjs_off) + body


def read_wallpaper(rom, stage):
    """The stage background: a libultra Sprite (MPGroundData.wallpaper).

    The map header points into the sprite file: a Sprite struct whose
    bitmap array is horizontal strips (each Bitmap: 16 bytes, buf +8,
    bmheight rows used of actualHeight stored). Hyrule's is 44 strips of
    300x5 RGBA16. Returns (texw, texh, imgw, imgh, argb1555_le_bytes)
    padded to power-of-two with the last row/column replicated so PVR
    bilinear sampling at the image edge stays clean, or None when the
    stage has no wallpaper.

    A Sprite's rows are STORED SHUFFLED, and this is the one thing that
    separates a sprite's bitmap from a model's texture. Every stage
    wallpaper sets SP_TEXSHUF (PR/sp.h:155, Sprite.attr bit 9) -- attr is
    0x0240 on all nine -- which says the odd rows already have the RDP's
    TMEM word swap applied to them in ROM, so the sprite library's load
    puts them back the right way round. Reading them linearly, which is
    what this once did, leaves every odd row's
    texels permuted in pairs and draws the backgrounds with a fine comb
    over them. ssb_spriteexport.unshuffle_row is the same undo the HUD
    sprites have always used; a model's texture is NOT shuffled and must
    not go through it (ssb_assets.n64_texel's docstring says why, and
    tools/check/texshuf_check.py measures both).
    """
    cfg = STAGES[stage]
    f, e, intern, sites, ids = L.file_info(rom, cfg["map_file"])
    ext = {s: (t, o) for (s, o), t in zip(sites, ids)}
    site = ground_off(cfg["map_file"]) + 0x48
    if site not in ext:
        return None
    sfid, soff = ext[site]
    sf, _, sintern, ssites, sids = L.file_info(rom, sfid)
    sext = {s: (t, o) for (s, o), t in zip(ssites, sids)}

    def ptr(o):
        if o in sext:
            fid2, off2 = sext[o]
            assert fid2 == sfid, "wallpaper bitmap crosses files"
            return off2
        return sintern[o]

    imgw, imgh = struct.unpack_from(">2h", sf, soff + 4)
    attr = be16u(sf, soff + 20)
    nbitmaps = be16s(sf, soff + 40)
    bmheight = be16s(sf, soff + 44)
    bmfmt, bmsiz = sf[soff + 48], sf[soff + 49]
    assert (bmfmt, bmsiz) == (0, 2), \
        "wallpaper fmt/siz %d/%d: only RGBA16 handled" % (bmfmt, bmsiz)
    assert attr & SP.SP_TEXSHUF, \
        "wallpaper attr 0x%04X has no SP_TEXSHUF: its rows are not " \
        "pre-shuffled and unshuffle_row below would comb them" % attr
    bitmaps = ptr(soff + 52)

    rows = []
    for i in range(nbitmaps):
        bo = bitmaps + i * 16
        # Bitmap.width is the drawn width, Bitmap.width_img (+2) the
        # stored one: the stride, and what the shuffle's 8-byte unit is
        # counted along. They are both 300 on every stage, but the load
        # is width_img's and so is this.
        bwi = be16s(sf, bo + 2)
        buf = ptr(bo + 8)
        for r in range(bmheight):
            # r is the row within the strip, which is what the parity is
            # on: each Bitmap is its own load.
            o = buf + r * bwi * 2
            raw = SP.unshuffle_row(sf[o:o + bwi * 2], r)
            rows.append(struct.unpack_from(">%dH" % bwi, raw, 0))
            if len(rows) == imgh:
                break
    assert len(rows) == imgh, "wallpaper: %d rows, want %d" % (len(rows),
                                                              imgh)

    def pvr(t):     # N64 RGBA5551 (bit0 alpha) -> PVR ARGB1555
        return ((t & 1) << 15) | (t >> 1)

    texw = 1
    while texw < imgw:
        texw *= 2
    texh = 1
    while texh < imgh:
        texh *= 2
    out = bytearray(texw * texh * 2)
    for y in range(min(imgh + 1, texh)):
        row = rows[min(y, imgh - 1)]
        line = [pvr(t) for t in row]
        line.append(line[-1])            # replicate edge column
        struct.pack_into("<%dH" % len(line), out, y * texw * 2, *line)
    return texw, texh, imgw, imgh, bytes(out)


def read_collision(f, reloc, geo_off):
    """MPGeometryData (mp/mptypes.h:71-80) -> its tables, as the game binds
    them.

    mp/mpcollision.c:3961-3995 mpCollisionInitGroundData points the
    gMPCollision* globals at vertex_data / vertex_id / vertex_links /
    line_info / mapobjs; func_ovl2_800FB04C (3436-3492) then walks
    line_info group by group and kind by kind and writes vertex_info[l++],
    i.e. it assumes every group's (group_id, line_count) ranges tile the
    line ids 0..N-1 in exactly that walk order. That is asserted here so a
    stage that breaks the assumption fails at export, not on the console.

    Returns a dict: yak_count; line_info [(yakumono_id, [(group_id,
    line_count)] * 4)]; links [(vertex1, vertex2)] per line id (first
    index into vids, vertex count); vids [u16] indexing vpos; vpos
    [(x, y, flags)] (upper 8 flag bits collision, lower 8 material);
    mapobjs [(kind, x, y)]; segments (endpoint pairs, for the summary).
    """
    yak_count = be16u(f, geo_off)
    vpos_off = reloc[geo_off + 0x04]
    vids_off = reloc[geo_off + 0x08]
    links_off = reloc[geo_off + 0x0C]
    line_info_off = reloc[geo_off + 0x10]
    mapobj_count = be16u(f, geo_off + 0x14)
    mapobjs_off = reloc[geo_off + 0x18]

    line_info = []
    expect = 0
    for grp in range(yak_count):
        info = line_info_off + grp * 18
        yak_id = be16u(f, info)
        kinds = []
        for kind in range(4):
            group_id = be16u(f, info + 2 + kind * 4)
            count = be16u(f, info + 4 + kind * 4)
            if count and group_id != expect:
                raise AssertionError(
                    "line ids not contiguous: group %d %s starts at %d, "
                    "func_ovl2_800FB04C expects %d"
                    % (grp, LINE_KINDS[kind], group_id, expect))
            expect += count
            kinds.append((group_id, count))
        line_info.append((yak_id, kinds))
    n_lines = expect

    links = [(be16u(f, links_off + i * 4), be16u(f, links_off + i * 4 + 2))
             for i in range(n_lines)]
    n_vids = max([first + count for first, count in links] or [0])
    vids = [be16u(f, vids_off + i * 2) for i in range(n_vids)]
    n_vpos = max(vids) + 1 if vids else 0
    vpos = [(be16s(f, vpos_off + v * 6), be16s(f, vpos_off + v * 6 + 2),
             be16u(f, vpos_off + v * 6 + 4)) for v in range(n_vpos)]
    mapobjs = []
    for i in range(mapobj_count):
        o = mapobjs_off + i * 6
        mapobjs.append((be16u(f, o), be16s(f, o + 2), be16s(f, o + 4)))

    # mpCollisionCheck{Floor,Ceil,LWall,RWall}LineCollisionSame each sweep
    # their kind's lines in table order and bail out of the whole group
    # (the `goto l_break`) at the first line that lies wholly past the
    # swept span, so the table order is load-bearing: floors and right
    # walls must run far-to-near (coll_pos_next, the line's max, never
    # rising), ceilings and left walls near-to-far (coll_pos_prev, its
    # min, never falling). coll_pos_* is measured on y for floors and
    # ceilings, on x for the walls (func_ovl2_800FB04C,
    # mp/mpcollision.c:3466-3467). A stage out of order would silently
    # lose collisions on the console, so it fails here instead.
    for yak_id, kinds in line_info:
        for kind, (group_id, count) in enumerate(kinds):
            axis = 1 if kind < 2 else 0
            bound, order = ((1, "descending") if kind in (0, 2)
                            else (0, "ascending"))
            prev_key = None
            for line_id in range(group_id, group_id + count):
                first, nvert = links[line_id]
                vals = [vpos[vids[first + k]][axis] for k in range(nvert)]
                key = max(vals) if bound else min(vals)
                if prev_key is not None and (
                        (bound and key > prev_key) or
                        (not bound and key < prev_key)):
                    raise AssertionError(
                        "group %d %s lines are not %s: line %d's %s is %d "
                        "after %d; the collision sweep would stop early"
                        % (yak_id, LINE_KINDS[kind], order, line_id,
                           "max" if bound else "min", key, prev_key))
                prev_key = key

    segments = []
    for yak_id, kinds in line_info:
        for kind, (group_id, count) in enumerate(kinds):
            for line_id in range(group_id, group_id + count):
                first, nvert = links[line_id]
                pts = [vpos[vids[first + k]] for k in range(nvert)]
                for a, b in zip(pts, pts[1:]):
                    segments.append((a[0], a[1], b[0], b[1], a[2], b[2],
                                     kind, yak_id))
    return {"yak_count": yak_count, "line_info": line_info, "links": links,
            "vids": vids, "vpos": vpos, "mapobjs": mapobjs,
            "segments": segments}


def alpha_in_opaque(batches, textures, palettes):
    """Batches the render mode put in the opaque list whose texture carries
    alpha anyway: [(batch index, texture index, why)].

    Not an error, and deliberately not corrected here. The RDP consults a
    texel's alpha only where the render mode says to -- ALPHA_CVG_SEL
    without CVG_X_ALPHA (G_RM_AA_*_OPA_SURF) leaves coverage untouched by
    it -- so a stage that draws an alpha-carrying tile through OPA_SURF
    really does draw it solid, and Yoshi's Island does exactly that, in a
    display list that resets G_AC_NONE and G_RM_AA_OPA_SURF by hand
    (dStageYosterFile2_DL_0x5A08). Forcing those to punch-through would
    punch holes the N64 does not have.

    What the list is for is the opposite failure: if the render-mode read
    ever stops working, every stage collapses to opaque and this set grows
    to swallow the cutout geometry. tools/check/rendermode_check.py watches it
    against gfxdis for that.
    """
    out = []
    for i, (tri_first, tri_count, mat, joint, bucket) in enumerate(batches):
        tex = mat[0]
        if (bucket & 3) != A.FPACK_LIST_OP or tex < 0:
            continue
        t = textures[tex]
        if t["fmt"] == A.PVRTEX_ARGB4444:
            out.append((i, tex, "ARGB4444 (graded alpha)"))
        elif t["fmt"] == A.PVRTEX_ARGB1555 and \
                any(not (x & 0x8000) for x in t["texels"]):
            out.append((i, tex, "ARGB1555 with clear texels"))
        elif t["fmt"] == A.PVRTEX_PAL4 and t["pal"] >= 0 and \
                any(not (x & 0x8000) for x in palettes[t["pal"]]):
            out.append((i, tex, "CI4 with a clear palette entry"))
    return out


def texture_files(rom, fid):
    """Where a file's images are: {byte offset: the bytes it lives in}.

    `ssb_assets.extern_texture_files` is the rule and the explanation; this
    is it by file id, for the stage code that has the id rather than the
    bytes. A MeshBaker handed a RelocFile applies the same rule itself, so
    passing this is belt and braces rather than the only thing keeping
    Yoshi's Island's layers (file 111's display lists, file 110's images)
    reading out of the right file.
    """
    return A.extern_texture_files(A.get_file(rom, fid, ssb_extract))


def layer_mobj_frames(f_model, reloc, subs, matanim_target, layer_nodes,
                      stage, layer):
    """How many pictures each of a layer's MObjs needs, by (joint, mobj).

    The layer's MatAnimJoint table says which MObjs animate; each script
    says how many frames of its own array it ever asks for
    (ssb_assets.matanim_frame_count, which also explains why the array
    itself cannot answer). A MObj that flips `texture_id_curr` indexes
    `sprites`; one that flips `palette_id` indexes `palettes`; Brinstar is
    the only stage doing the second and does nothing else.

    `texture_id_next` indexes `sprites` as well -- it is the SECOND tile of
    a cross-fade, drawn out of the same array (objdisplay.c:1330-1352) --
    so it counts into the same run, which has to be as long as the further
    of the two tracks reaches. Dream Land's two clouds are the only MObjs
    in the game that drive it.

    Returns {(joint, mobj): (frame_count, which_array)} for the MObjs that
    need more than one picture, and {} for a layer with no table.
    """
    out = {}
    if matanim_target is None:
        return out
    for k in range(len(layer_nodes)):
        t = reloc.get(matanim_target[1] + k * 4)
        if t is None:
            continue
        nmobj = len(subs[k]) if k < len(subs) else 0
        for mi, s in enumerate(A._ptr_list(reloc, t)[:nmobj]):
            for bits, name in (((A.MATANIM_BIT_TEXID,
                                 A.MATANIM_BIT_TEXID_NEXT), "sprites"),
                               ((A.MATANIM_BIT_PALETTEID,), "palettes")):
                n = max(A.matanim_frame_count(f_model, reloc, s, b)
                        for b in bits)
                if n <= 1:
                    continue
                if (k, mi) in out:
                    raise AssertionError(
                        "%s layer %d joint %d MObj %d drives both "
                        "texture_id_curr and palette_id; the pack has one "
                        "picture array per MObj" % (stage, layer, k, mi))
                have = len(subs[k][mi][name])
                if have < n:
                    raise AssertionError(
                        "%s layer %d joint %d MObj %d: MatAnimJoint 0x%X "
                        "reaches %s[%d] but the array reads %d entries"
                        % (stage, layer, k, mi, s, name, n - 1, have))
                out[(k, mi)] = (n, name)
    return out


def texlerp_uv(baker, sub, batch, where):
    """FPackMObjSub.uv_base/uv_scale for one cross-fading MObj.

    The N64 gives the second tile its own origin and moves it with the
    material animation's scrollu/scrollv tracks (objdisplay.c:1383-1397):

        uls1 = ((tile1_w * scrollu) + unk0A) / scau          [texels]
        ult1 = ((((1 - scav) - scrollv) * tile1_h) + unk0A) / scav

    The port cannot re-load a tile, so it draws the SAME baked picture with
    the coordinates translated. A baked u is `(s * scale_s - uls0) / tex_w`
    (MeshBaker._finish) and the same s through tile 1's origin is
    `(s * scale_s - uls1) / tex_w`, so the offset is `(uls0 - uls1) / tex_w`
    -- linear in the track, which is what the two packed pairs are:

        du = (scrollu - uv_base[0]) * uv_scale[0]

    with uv_scale the derivative and uv_base the track value at which tile
    1 lands exactly on the tile 0 the bake froze in. That is solved for,
    not assumed: Dream Land's clouds start with the two together, but
    Final Destination's fog starts them 32 texels apart in t (one whole
    repeat of its 32-texel picture), and with base = the MObjSub's own
    value the port would slide a picture that is already in the wrong
    place.

    Note the scale is NOT 1/scau: the baker normalises against the tile's
    baked extent, which is the repeat period and not always tile1_w. Dream
    Land's first cloud bakes 128 texels of tile into a 64-texel period, so
    one unit of scrollu is one whole repeat there (1.0) and 1/scau would
    have been half of it.
    """
    tex = baker.textures[baker.batches[batch]["mat"][0]]
    st = {"tilesize": (0.0, 0.0, 0.0, 0.0), "scale_s": 1.0, "scale_t": 1.0,
          "texture_on": False}
    baker._mobj_tile(sub, sub["flags"], st)
    uls0, ult0 = st["tilesize"][0], st["tilesize"][1]
    scau, scav, w1, h1 = (sub["scau"], sub["scav"],
                          sub["tile1_w"], sub["tile1_h"])
    eps = 1.0 / 65535.0
    if not (sub["flags"] & A.MOBJ_FLAG_TILESIZE1) or \
            abs(scau) <= eps or abs(scav) <= eps or w1 == 0 or h1 == 0:
        raise AssertionError(
            "%s joint %d MObj %d cross-fades two tiles and does "
            "not give the second one a size" % where)
    # objdisplay.c:1173-1177 halves scau and re-bases scrollu for a split
    # tile; no cross-fading MObj in the game is one, and the solve below
    # is only written for the ordinary case.
    if sub["unk10"] != 0:
        raise AssertionError(
            "%s joint %d MObj %d cross-fades a kind-%d tile"
            % (where + (sub["unk10"],)))
    base_u = (uls0 * scau - sub["unk0A"]) / w1
    base_v = (1.0 - scav) - (ult0 * scav - sub["unk0A"]) / h1
    return (base_u, base_v,
            -(w1 / scau) / tex["w"], (h1 / scav) / tex["h"])


def tile0_uv(baker, sub, batch, tracks, where, lowest=0.0):
    """FPackMObjSub.uv0_base/uv0_scale: the FIRST tile's scroll, or None.

    gcDrawMObjForDObj moves tile 0's origin by the trau/trav tracks
    (objdisplay.c:1353-1382) whenever the MObj asks for a tile size:

        uls0 = ((tile_w * trau) + unk0A) / scau
        ult0 = ((((1 - scav) - trav) * tile_h) + unk0A) / scav

    texlerp_uv's translation, against the tile's own tracks: the baked
    coordinates ARE tile 0 at the MObjSub's own trau/trav, so that is the
    base, and the scale is the derivative. A split tile (kind 1) halves
    scau and trau together, so its slope is the ordinary one. A kind-2
    tile measures from zero rather than from the far edge, so its t slope
    has the other sign, and it clamps its origin at zero (objdisplay.c:
    1355-1368), which no translation reproduces -- Meta Crystal's are the
    only ones that scroll, and every value their scripts set is a small
    positive shimmer, so `lowest` (the least value any script sets) is
    checked to keep the clamp out of reach.

    `tracks` is every MAT_TRACKS name the MObj's scripts write. A script
    that also winds scau/scav changes the tile's SCALE as well as its
    origin (Saffron's screen), which a translation cannot draw either;
    that MObj keeps its frozen tile and the build log names it.
    """
    if not (tracks & {"trau", "trav"}) or \
            not (sub["flags"] & A.MOBJ_FLAG_TILESIZE):
        return None
    if tracks & {"scau", "scav"}:
        return None
    if sub["unk10"] == 2 and lowest < 0.0:
        raise AssertionError(
            "%s joint %d MObj %d scrolls a kind-2 tile to %g, where its "
            "origin clamps at zero" % (where + (lowest,)))
    tex = baker.textures[baker.batches[batch]["mat"][0]]
    eps = 1.0 / 65535.0
    scau, scav = sub["scau"], sub["scav"]
    su = (-(sub["tile_w"] / scau) / tex["w"]
          if "trau" in tracks and abs(scau) > eps else 0.0)
    sv = ((sub["tile_h"] / scav) / tex["h"]
          if "trav" in tracks and abs(scav) > eps else 0.0)
    if sub["unk10"] == 2:
        sv = -sv
    return (sub["trau"], sub["trav"], su, sv)


def mobj_scroll(f, reloc, baker, sub, batch, scripts, where):
    """(uv, uv0, note) for one MObj: its two tiles' scroll pairs, each
    None where that tile does not move, and a build-log note for a scroll
    the port leaves frozen. `scripts` is every script the MObj has, one
    per alt; `batch` the batch it drew, or None."""
    tracks = set()
    for s in scripts:
        if s is not None:
            tracks |= A.matanim_tracks(f, reloc, s)
    if not (tracks & {"trau", "trav", "scrollu", "scrollv"}):
        return None, None, None
    if batch is None or baker.batches[batch]["mat"][0] < 0:
        return None, None, None
    uv = (texlerp_uv(baker, sub, batch, where)
          if baker.batches[batch]["mat"][10] else None)
    lowest = min([v for s in scripts if s is not None
                  for bit in (1, 2)             # MAT_TRACKS trau, trav
                  for v in A.matanim_frame_ids(f, reloc, s, bit)]
                 + [sub["trau"], sub["trav"]])
    uv0 = tile0_uv(baker, sub, batch, tracks, where, lowest)
    note = None
    if tracks & {"trau", "trav"} and uv0 is None and \
            sub["flags"] & A.MOBJ_FLAG_TILESIZE:
        note = ("%s joint %d MObj %d scales its tile as it scrolls; the "
                "port keeps it still" % where)
    return uv, uv0, note


def merge_layers(f_model, reloc, layers, layer_mask, mobjsubs=None,
                 trace=None, tex_files=None, spans=None, matanims=None,
                 extern=None, mobjs=None, stage="?"):
    """Bake each visual layer and merge into one model's worth of data.

    `trace`, when given, collects one (layer, dl_off, head, othermode_l)
    per emitted batch -- what tools/check/rendermode_check.py checks the buckets
    against. `tex_files` is texture_files' answer for the file `f_model`
    holds, for a file whose images live in another (see MeshBaker).

    `spans`, when given, is filled with one (layer, jbase, njoints) per
    baked layer. Merging four layers into one pack renumbers their joints,
    and a layer's animation table is indexed by the joint's number WITHIN
    ITS OWN LAYER (sys/objanim.c:228 advances one entry per
    gcGetTreeDObjNext), so read_layer_anims cannot place a script without
    knowing where the layer landed.

    `matanims` is ground["layer_matanims"]. A layer that has one is baked
    with `mobj_batches=True` so that each batch remembers which MObj drew
    it -- which only stops two different MObjs' batches from MERGING;
    MeshBaker._apply_mobj still folds the MObj's frame-0 material either
    way, so a layer's frame 0 bakes exactly as it did before -- and is
    re-baked once per extra frame with that MObj wound forward, the recipe
    ssb_effectexport.py's read_tree() uses for the fireball's two palettes.
    `mobjs`, when given, is filled with what the pack's MObjs section
    needs. `extern` reaches sprite/palette arrays in another file.
    """
    nodes = []          # (parent, t, r, s)
    verts = []
    tris = []
    batches = []        # (tri_first, tri_count, mat, joint, bucket)
    textures = []
    palettes = []
    tex_ix = {}
    pal_ix = {}
    # the MObjs section, accumulated across layers in merged joint order
    m_subs = []             # one MObjSub dict per MObj, tree order
    m_joint = {}            # merged joint -> (first, count)
    m_batch = []            # one entry per merged batch: MObj index or -1
    m_frames = {}           # MObj index -> [tile dicts], frame 0 first
    m_index = {}            # (layer, layer joint, mobj) -> MObj index, which
                            # is how read_layer_matanims names its scripts
    m_uv = {}               # MObj index -> texlerp_uv, for the MObjs whose
                            # combiner cross-fades two tiles
    m_uv0 = {}              # MObj index -> tile0_uv, for the MObjs whose
                            # script scrolls the first tile
    m_notes = []            # mobj_scroll's notes, for the build log
    import pygfxd

    for layer, target in enumerate(layers):
        if target is None:
            continue
        links = bool(layer_mask & (1 << layer))
        noz = A.FPACK_NOZ if layer in (0, 2) else 0
        root, layer_nodes = A.read_dobj_tree(f_model, reloc, target[1],
                                             dl_links=links)
        subs = []
        sub_target = (mobjsubs or [None] * 4)[layer]
        if sub_target is not None:
            subs = A.read_mobjsubs(f_model, reloc, sub_target[1],
                                   len(layer_nodes), extern)
        mat_target = (matanims or [None] * 4)[layer]
        if mat_target is not None and layer not in LAYER_ANIM_LAYERS:
            mat_target = None
        animated = layer_mobj_frames(f_model, reloc, subs, mat_target,
                                     layer_nodes, stage, layer)
        seed = LAYER_RENDERMODE[links][layer]
        baker = A.bake_retry(
            lambda force: A.MeshBaker(f_model, pygfxd, mobjsubs=subs,
                                      force_extent=force, rendermode=seed,
                                      texture_files=tex_files,
                                      mobj_batches=mat_target is not None),
            lambda b: b.bake(root))

        jbase, vbase = len(nodes), len(verts)
        if spans is not None:
            spans.append((layer, jbase, len(baker.nodes)))

        # -- this layer's MObjs, and the extra pictures they animate over --
        #
        # gcAddMObjAll walks the tree and hangs the MObjSub list of joint j
        # on joint j, so the pack keeps them per joint and in tree order
        # (see dc_model_add_mobjs_alt). Merging four layers renumbers the
        # joints, so the per-joint index is built here, against jbase.
        mobj_of = {}            # (layer joint, mobj) -> merged MObj index
        if mat_target is not None:
            for k in range(len(baker.nodes)):
                lst = subs[k] if k < len(subs) else []
                m_joint[jbase + k] = (len(m_subs), len(lst))
                for mi, sub in enumerate(lst):
                    mobj_of[(k, mi)] = m_index[(layer, k, mi)] = len(m_subs)
                    m_subs.append(sub)
        # Which batch each animated MObj drew. `mobj` is the last segment
        # 0xE branch the walk saw and it carries across display lists, so
        # it is only this batch's own MObj when its joint agrees -- the
        # guard ssb_effectexport.py's pack_weapon makes for the same reason.
        batch_of = {}
        layer_frames = {}       # merged MObj index -> tiles, in THIS layer's
                                # palette numbering; remapped below
        for i, b in enumerate(baker.batches):
            m = b.get("mobj")
            if m is not None and m[0] == b["node"].index and m not in batch_of:
                batch_of[m] = i
        for key in sorted(animated):
            if key not in batch_of:
                raise AssertionError(
                    "%s layer %d: joint %d MObj %d has a MatAnimJoint but "
                    "drew no batch of its own" % ((stage, layer) + key))
            if baker.batches[batch_of[key]]["mat"][0] < 0:
                # A MObj whose batch samples no texture. Brinstar's joints
                # 24 and 27 are the case and the only one: their display
                # lists leave gsSPTexture OFF and their combiner is
                # (0 - 0) * 0 + SHADE, so the palette their script winds is
                # a TLUT nothing reads -- on the N64 as much as here. The
                # script is still attached below, because the game attaches
                # it; what it does not get is frames it cannot show.
                del animated[key]
                continue
            layer_frames[mobj_of[key]] = [baker.textures[
                baker.batches[batch_of[key]]["mat"][0]]]

        # Every scripted MObj's tile scroll: the two tiles' origins move
        # with the script's trau/trav and scrollu/scrollv, which the port
        # draws as a translation of the baked coordinates (mobj_scroll).
        for key in sorted(mobj_of):
            t = reloc.get(mat_target[1] + key[0] * 4)
            lst = A._ptr_list(reloc, t) if t is not None else []
            if key[1] >= len(lst):
                continue
            uv, uv0, note = mobj_scroll(
                f_model, reloc, baker, subs[key[0]][key[1]],
                batch_of.get(key), [lst[key[1]]],
                ("%s layer %d" % (stage, layer),) + key)
            if uv is not None:
                m_uv[mobj_of[key]] = uv
            if uv0 is not None:
                m_uv0[mobj_of[key]] = uv0
            if note is not None:
                m_notes.append(note)

        # Frames 1..n-1: the same tree baked again with that MObj wound
        # forward, which is all _apply_mobj needs to pick the next entry of
        # the sprite or palette array as the tile. Only the picture is
        # supposed to change, and the re-bake is checked to prove it.
        for fr in range(1, max([n for (n, _w) in animated.values()],
                               default=0)):
            for (k, mi), (n, which) in animated.items():
                field = ("texture_id_curr" if which == "sprites"
                         else "palette_id")
                subs[k][mi][field] = min(fr, n - 1)
            b2 = A.bake_retry(
                lambda force: A.MeshBaker(f_model, pygfxd, mobjsubs=subs,
                                          force_extent=force, rendermode=seed,
                                          texture_files=tex_files,
                                          mobj_batches=True),
                lambda b: b.bake(root))
            if (len(b2.verts), len(b2.tris), len(b2.batches),
                    b2.palettes) != (len(baker.verts), len(baker.tris),
                                     len(baker.batches), baker.palettes):
                raise AssertionError(
                    "%s layer %d: frame %d bakes to different geometry; only "
                    "the picture is supposed to change" % (stage, layer, fr))
            for key, (n, _which) in animated.items():
                if fr < n:
                    layer_frames[mobj_of[key]].append(
                        b2.textures[b2.batches[batch_of[key]]["mat"][0]])
        for (k, mi) in animated:
            subs[k][mi]["texture_id_curr"] = 0
            subs[k][mi]["palette_id"] = 0

        for n in baker.nodes:
            parent = n.parent.index + jbase if n.parent.index >= 0 else -1
            nodes.append((parent, n.translate, n.rotate, n.scale))
        for (x, y, z, u, v, nx, ny, nz, alpha, joint) in baker.verts:
            verts.append((x, y, z, u, v, nx, ny, nz, alpha, joint + jbase))
        for (a, b, c) in baker.tris:
            tris.append((a + vbase, b + vbase, c + vbase))

        pal_map = {}
        for i, pal in enumerate(baker.palettes):
            key = tuple(pal)
            if key not in pal_ix:
                pal_ix[key] = len(palettes)
                palettes.append(pal)
            pal_map[i] = pal_ix[key]
        tex_map = {}
        for i, t in enumerate(baker.textures):
            # A direct-colour tile has no palette bank to remap (pal -1).
            pal = pal_map[t["pal"]] if t["pal"] >= 0 else -1
            key = (tuple(t["texels"]), t["w"], t["h"], pal, t["fmt"],
                   t["clamp_u"], t["clamp_v"])
            if key not in tex_ix:
                tex_ix[key] = len(textures)
                t = dict(t)
                t["pal"] = pal
                textures.append(t)
            tex_map[i] = tex_ix[key]

        # The animated frames were collected before this layer's palettes
        # were merged, so their `pal` is still this layer's numbering.
        for mi, tiles in layer_frames.items():
            m_frames[mi] = [dict(t, pal=(pal_map[t["pal"]] if t["pal"] >= 0
                                         else -1)) for t in tiles]

        tribase = batches and None
        for b in baker.batches:
            # The key's tenth element is which MObj drew the batch, and
            # a stage never merges on it: it is replaced by None here so
            # that merging layers keys on the material alone, as it always
            # has. The eleventh -- the two-tile cross-fade -- is the
            # material and stays.
            (tex, prim, l1, l2, uses_shade, lit,
             a_tex, a_shade, env) = b["mat"][:9]
            mat = (tex_map[tex] if tex >= 0 else -1, prim, l1, l2,
                   uses_shade, lit, a_tex, a_shade, env, None, b["mat"][10],
                   None, b["mat"][12], b["mat"][13])
            batches.append((b["tri_first"] + (len(tris) - len(baker.tris)),
                            b["tri_count"], mat,
                            b["node"].index + jbase,
                            b["bucket"] | noz))
            m = b.get("mobj")
            m_batch.append(mobj_of.get(m, -1)
                           if (m is not None and m[0] == b["node"].index)
                           else -1)
            if trace is not None:
                trace.append((layer, b["dl"], b["head"], b["rendermode"]))

    # -- the animated MObjs' pictures, as one contiguous run each ----------
    #
    # FPackMObjSub.tex_first/tex_count is a RUN in the pack's texture table
    # and fighter.c compiles one poly header per entry of it, so frame N
    # has to be tex_first + N. That cannot go through the interning above,
    # which would coalesce two frames that happen to be the same tile and
    # break the run, so the runs are appended whole -- frame 0 included,
    # even though it is already interned as the batch's own. The duplicate
    # is the trade ssb_effectexport.py's pack_weapon already makes and
    # names: romdisk bytes for an index the runtime can just add to.
    tex_first = {}
    for mi in sorted(m_frames):
        tex_first[mi] = len(textures)
        textures.extend(dict(t) for t in m_frames[mi])
    for i, mi in enumerate(m_batch):
        if mi in tex_first:
            b = batches[i]
            batches[i] = (b[0], b[1], (tex_first[mi],) + tuple(b[2][1:]),
                          b[3], b[4])

    if mobjs is not None and m_subs:
        # A MObj that does not animate its picture still gets a tex_first,
        # its batch's own texture, because that is what the field means --
        # fighter.c compiles no extra header for a tex_count of 1, so
        # nothing reads it, but -1 would be a lie about a real batch.
        own = [-1] * len(m_subs)
        for i, mi in enumerate(m_batch):
            if mi >= 0 and own[mi] < 0:
                own[mi] = batches[i][2][0]
        mobjs.update(subs=m_subs, joint=m_joint, batch=m_batch,
                     index=m_index, uv=m_uv, uv0=m_uv0, notes=m_notes,
                     tex=[(tex_first[k], len(m_frames[k])) if k in m_frames
                          else (own[k], 1) for k in range(len(m_subs))])
    return nodes, verts, tris, batches, textures, palettes


def layer_mobj_section(mobjs, joint_count, scripts):
    """The merged layers' MObjs section (fighter.h FPackMObjs), as bytes.

    mobj_pack_section's sibling: same six blobs in the same order, built
    from merge_layers' merged bookkeeping instead of from one baker, and
    with tex_first/tex_count carrying real runs rather than the single
    frame that one hard-wires.

    `scripts` is read_layer_matanims' answer -- ONE whole MatAnimJoint over
    every MObj of the pack (alt_count 1), keyed by (layer, joint, mobj) --
    or None, which leaves every entry -1 and makes this a pack that has
    MObjs and no material animation, the shape the spotlight already has.
    """
    subs, joint, batch, tex = (mobjs["subs"], mobjs["joint"],
                               mobjs["batch"], mobjs["tex"])
    subs_blob = b"".join(
        A.pack_mobjsub(s, tex[k][0], tex[k][1], mobjs["uv"].get(k),
                       uv0=mobjs["uv0"].get(k))
        for k, s in enumerate(subs))
    # One pair per joint of the WHOLE pack, not per joint that has an MObj:
    # dc_model_add_mobjs_alt indexes it by joint number straight across.
    joint_blob = b"".join(struct.pack("<2h", *joint.get(j, (0, 0)))
                          for j in range(joint_count))
    batch_blob = struct.pack("<%dh" % len(batch), *batch)

    words = scripts["words"] if scripts else []
    relocs = scripts["relocs"] if scripts else []
    entries = [-1] * len(subs)
    if scripts:
        for (layer, k, mi), w in scripts["entry_of"].items():
            entries[mobjs["index"][(layer, k, mi)]] = w
    entry_blob = struct.pack("<%di" % len(entries), *entries)
    words_blob = struct.pack("<%dI" % len(words), *words) if words else b""
    reloc_blob = (struct.pack("<%dI" % len(relocs), *relocs) if relocs
                  else b"")
    return (subs_blob, joint_blob, batch_blob, entry_blob, words_blob,
            reloc_blob)


def main():
    argv = sys.argv[1:]
    rom_path = M.ROM_DEFAULT
    out = None
    stage = "Hyrule"
    if "--rom" in argv:
        rom_path = argv[argv.index("--rom") + 1]
    if "--out" in argv:
        out = argv[argv.index("--out") + 1]
    if "--stage" in argv:
        stage = argv[argv.index("--stage") + 1]
    if not out:
        sys.exit("--out is required")
    rom = open(rom_path, "rb").read()

    # --platforms writes a FILE rather than a stage: the one BPL1 block
    # the twelve Board the Platforms courses share. It
    # takes no --stage, because the trees are relocData 136's and not
    # any course's.
    if "--platforms" in argv:
        bp = read_bonus_platforms(rom)
        blob = tree_pack_section(bp, b"BPL1")
        open(out, "wb").write(blob)
        print("wrote %s (%d bytes: %d pack(s), %d anim(s), %d fixup(s))"
              % (out, len(blob), len(bp["packs"]), len(bp["anims"]),
                 sum(len(a["fixups"]) for a in bp["anims"])))
        return

    ground = read_ground(rom, stage)
    model_fid = next(t[0] for t in ground["layers"] if t)
    for t in ground["layers"]:
        if t and t[0] != model_fid:
            raise AssertionError("layers span model files %d and %d"
                                 % (model_fid, t[0]))
    geo_fid, geo_off = ground["geometry"]
    if geo_fid != model_fid:
        raise AssertionError("geometry in file %d, model in %d"
                             % (geo_fid, model_fid))

    f_model, _, m_reloc, m_sites, _ = L.file_info(rom, model_fid)
    reloc = m_reloc
    # The model file's EXTERNAL relocation chain, as {site: target offset}.
    # Yoshi's Island's two animated MObjSubs hold their sprite arrays here
    # and not in the intern dict, which is why those arrays read as empty
    # until now (ssb_assets._ptr_list).
    m_extern = dict(m_sites)
    spans = []
    mobjs = {}
    nodes, verts, tris, batches, textures, palettes = \
        merge_layers(f_model, reloc, ground["layers"], ground["layer_mask"],
                     ground["mobjsubs"],
                     tex_files=texture_files(rom, model_fid), spans=spans,
                     matanims=ground["layer_matanims"], extern=m_extern,
                     mobjs=mobjs, stage=stage)
    # src/dc/mnmaps.c mnMapsMakeModel finds the layers its preview fix-ups
    # act on by where they sit among the merged roots: Yoshi's Island's
    # layer 0 is the first root and holds the 15th and 17th DObjs it
    # hides, and Saffron's layer 3 is the last root, one root deep enough
    # for a grandchild (mn/mnmaps/mnmaps.c:1039-1057).
    def layer_roots(layer):
        return [jb + k for (l, jb, n) in spans if l == layer
                for k in range(n) if nodes[jb + k][0] < 0]
    if stage == "Yoster":
        if not (spans and spans[0][:2] == (0, 0) and spans[0][2] >= 17 and
                layer_roots(0) == [0]):
            raise AssertionError("Yoster: layer 0 is not the first root with "
                                 "17 joints: %r" % spans)
    if stage == "Yamabuki":
        last = spans[-1] if spans else None
        roots = [j for j in range(len(nodes)) if nodes[j][0] < 0]
        if not (last and last[0] == 3 and layer_roots(3) == [roots[-1]] and
                last[2] >= 3 and nodes[last[1] + 1][0] == last[1] and
                nodes[last[1] + 2][0] == last[1] + 1):
            raise AssertionError("Yamabuki: layer 3 is not the last root "
                                 "with a grandchild: %r" % spans)
    layer_subs = {}
    for (layer, _jb, njoints) in spans:
        t = ground["mobjsubs"][layer]
        layer_subs[layer] = (A.read_mobjsubs(f_model, reloc, t[1], njoints,
                                             m_extern) if t else [])
    mat = read_layer_matanims(stage, f_model, reloc, ground, spans,
                              layer_subs)
    lay = read_layer_anims(stage, f_model, reloc, ground, spans, nodes)
    bind_ok, bind_n = check_layer_anim_joints(stage, f_model, reloc, ground,
                                              spans, nodes)
    geo = read_collision(f_model, reloc, geo_off)
    segments, spawns = geo["segments"], geo["mapobjs"]
    wallpaper = read_wallpaper(rom, stage)
    ga = read_ground_actors(rom, stage)
    bw = read_boss_wallpaper(rom, stage)
    bt = read_bonus_placements(rom, stage, "targets", BONUS_TASK_MAX)
    bm = read_bonus_placements(rom, stage, "bumpers")

    obj = read_map_object(rom, stage, ground)
    weights = read_item_weights(rom, stage, ground)
    check_fog(stage, ground)
    mpk = None
    if obj is not None or weights is not None:
        mpk = dict(obj or {})
        mpk["weights"] = weights
        mpk["anims"] = mpk.get("anims", [])
        mpk["attack"] = mpk.get("attack")
        mpk["packs"] = []
        if obj is not None:
            # One baked pack per object. A stage with one object -- every
            # stage before Dream Land -- gets exactly the pack it always
            # did; the object's name goes on the pack so a dump can tell
            # a four-object stage's four apart.
            for i, o in enumerate(obj["objects"]):
                (onodes, overts, otris, obatches, otexs, opals, omobj,
                 omobj_count, omobj_alts) = bake_map_object(rom, obj["fid"],
                                                            o)
                mpk["packs"].append({
                    "desc": o["desc"],
                    "pack": object_pack(onodes, overts, otris, obatches,
                                        otexs, opals,
                                        stage if i == 0
                                        else "%s%d" % (stage, i),
                                        omobj, omobj_count, omobj_alts),
                    "joint_count": len(onodes), "vert_count": len(overts),
                    "tri_count": len(otris), "batch_count": len(obatches),
                    "mobj_count": omobj_count, "mobj_alts": omobj_alts,
                })
            # An ANIMS-ONLY stage (Peach's Castle) reaches here with no
            # packs at all: the totals a dump prints are the first
            # object's, and there is no first object. Zero is the honest
            # answer, and it is the same one the `else` arm below gives a
            # stage with no map block.
            if mpk["packs"]:
                mpk.update(joint_count=mpk["packs"][0]["joint_count"],
                           vert_count=mpk["packs"][0]["vert_count"],
                           tri_count=mpk["packs"][0]["tri_count"],
                           batch_count=mpk["packs"][0]["batch_count"])
            else:
                mpk.update(joint_count=0, vert_count=0, tri_count=0,
                           batch_count=0)
            graft = bake_graft_object(rom, obj)
            if graft is not None:
                gnodes, gverts, gtris, gbatches, gtexs, gpals, gmobj, \
                    gmobj_count, gmobj_alts = graft
                mpk.update(graft_pack=object_pack(
                    gnodes, gverts, gtris, gbatches, gtexs, gpals,
                    stage + "C", gmobj, gmobj_count, gmobj_alts),
                    graft_nodes=len(gnodes), graft_verts=len(gverts),
                    graft_tris=len(gtris), graft_batches=len(gbatches))
        else:
            mpk.update(joint_count=0, vert_count=0, tri_count=0,
                       batch_count=0)

    floors = sum(1 for s in segments if s[6] == 0)
    print("%s: file %d: %d nodes, %d verts, %d tris, %d batches, "
          "%d texs, %d pals; %d lines in %d groups (%d segments, %d "
          "floors), %d vertices, %d spawns, bgm %d"
          % (stage, model_fid, len(nodes), len(verts), len(tris),
             len(batches), len(textures), len(palettes), len(geo["links"]),
             geo["yak_count"], len(segments), floors, len(geo["vpos"]),
             len(spawns), ground["bgm"]))
    if wallpaper:
        print("%s: wallpaper %dx%d in %dx%d ARGB1555"
              % (stage, wallpaper[2], wallpaper[3], wallpaper[0],
                 wallpaper[1]))
    if lay is not None:
        print("%s: layer anim: %s; %d script(s) in 0x%04X..0x%04X, %d "
              "words, %d fixup(s); %d of %d opening pose(s) match the "
              "joint's bind pose"
              % (stage, ", ".join("layer %d %d/%d joints" % p
                                  for p in lay["per_layer"]),
                 lay["scripts"], lay["span"][0], lay["span"][1],
                 len(lay["words"]), len(lay["relocs"]), bind_ok, bind_n))
    # The material half of the same desc. Carried now: the merged pack
    # keeps the layers' MObjs (merge_layers' `mobjs`) and their
    # MatAnimJoints, and a MObj whose script flips texture_id_curr or
    # palette_id carries one baked picture per frame it ever asks for.
    if mat is not None:
        frames = sum(n for (_f, n) in mobjs["tex"] if n > 1)
        print("%s: layer matanim: %s; %d script(s) in 0x%04X..0x%04X, %d "
              "words, %d fixup(s), %.0f%% covered; %d MObj(s), %d animated "
              "picture(s)%s"
              % (stage, ", ".join("layer %d %d/%d joints, %d MObj(s)" % p
                                  for p in mat["per_layer"]),
                 mat["scripts"], mat["span"][0], mat["span"][1],
                 len(mat["words"]), len(mat["relocs"]),
                 100.0 * mat["coverage"], len(mobjs["subs"]), frames,
                 "".join(
                     ", MObj %d cross-fades two tiles (%+.3f,%+.3f per "
                     "unit of scroll)" % (k, uv[2], uv[3])
                     for k, uv in sorted(mobjs["uv"].items())) +
                 "".join(
                     ", MObj %d scrolls its first tile (%+.3f,%+.3f per "
                     "unit)" % (k, uv[2], uv[3])
                     for k, uv in sorted(mobjs["uv0"].items())) +
                 "".join("; " + n for n in mobjs["notes"])))

    # -- model pack (0 anims), same layout fighter_load reads ---------------
    secs = model_sections(nodes, verts, tris, batches, textures, palettes)

    xs = [v[0] for v in verts] or [0.0]
    ys = [v[1] for v in verts] or [0.0]
    zs = [v[2] for v in verts] or [0.0]
    cx, cy, cz = ((min(xs) + max(xs)) / 2, (min(ys) + max(ys)) / 2,
                  (min(zs) + max(zs)) / 2)
    import math
    radius = max(math.sqrt((x - cx) ** 2 + (y - cy) ** 2 + (z - cz) ** 2)
                 for x, y, z in zip(xs, ys, zs))

    def align(b):
        return b + b"\0" * (-len(b) % 4)

    header_size = 128
    off = header_size
    offsets = {}
    body = b""
    for key in ("joints", "verts", "tris", "batches", "texs", "pals",
                "texdata"):
        sec = align(secs[key])
        offsets[key] = off
        body += sec
        off += len(sec)
    # -- the pack's animation section: the geometry layers' AnimJoints ------
    #
    # One FPackAnim, named "layers", in the same directory+words+entries
    # +reloc layout tools/export/ssb_packexport.py writes for a fighter's
    # motions -- so fighter_init's own reloc walk (src/dc/fighter.c:481)
    # turns the pointer words into addresses with no stage-specific code,
    # and stage.c's attach is the same three lines efmanager.c's
    # efModelAnimJoint already is. Empty (anim_count 0) for Hyrule, which
    # is the one stage with nothing to animate.
    offsets["anims"] = off
    anim_blob = b""
    if lay is not None:
        words_off = off + SP_ANIM_DIR_ENTRY
        w_blob = struct.pack("<%dI" % len(lay["words"]), *lay["words"])
        ent_off = words_off + len(w_blob)
        e_blob = struct.pack("<%di" % len(lay["entries"]), *lay["entries"])
        rel_off = ent_off + len(e_blob)
        r_blob = struct.pack("<%dI" % len(lay["relocs"]), *lay["relocs"])
        anim_blob = struct.pack(
            "<%dsIIIIII" % SP_ANIM_NAME_LEN, b"layers", words_off,
            len(lay["words"]), ent_off, FPACK_ANIM_ANIMJOINT,
            rel_off, len(lay["relocs"])) + w_blob + e_blob + r_blob
        assert len(anim_blob) % 4 == 0
    body += anim_blob
    off += len(anim_blob)

    # -- the pack's MObjs section: the layers' animated materials ----------
    #
    # object_pack's layout, for the same reason and read by the same code
    # (src/dc/objmodel.c dc_model_add_mobjs). A stage with no layer
    # MatAnimJoint table writes 0 here and keeps the pack it always had.
    mobjs_off = 0
    if mat is not None:
        subs_b, joint_b, batch_b, entry_b, words_b, reloc_b = (
            align(x) for x in layer_mobj_section(mobjs, len(nodes), mat))
        mobjs_off = off
        mh = off + 48
        body += struct.pack(
            "<12I", len(mobjs["subs"]), mh, mh + len(subs_b),
            mh + len(subs_b) + len(joint_b),
            mh + len(subs_b) + len(joint_b) + len(batch_b),
            mh + len(subs_b) + len(joint_b) + len(batch_b) + len(entry_b),
            len(words_b) // 4,
            mh + len(subs_b) + len(joint_b) + len(batch_b) + len(entry_b) +
            len(words_b),
            len(reloc_b) // 4, 1, 0, 0)
        body += subs_b + joint_b + batch_b + entry_b + words_b + reloc_b
        off = len(body) + header_size

    pack = struct.pack("<8s8I8I3ff8s8I", b"SSBPACKA",
                       len(nodes), len(verts), len(tris), len(batches),
                       len(textures), len(palettes), 0 if lay is None else 1,
                       len(secs["texdata"]),
                       offsets["joints"], offsets["verts"], offsets["tris"],
                       offsets["batches"], offsets["texs"], offsets["pals"],
                       offsets["texdata"], offsets["anims"],
                       cx, cy, cz, radius, stage.encode()[:8],
                       0, 0, 0, 0, 0, 0, 0, mobjs_off) + body

    # -- STG2 extras: header, then the four MPGeometryData tables and the
    # map objects, each element little-endian in the decomp's struct
    # layout (MPLineInfo 18 bytes, MPVertexLinks 4, vertex_id u16,
    # MPVertexData 6, MPMapObjData 6). stage.c points MPGeometryData at
    # them in place.
    # Where layer 1 -- the COLLISION layer -- landed among the merged
    # joints. yakumono id k is layer-1 joint k (gcSetupCustomDObjs fills
    # gMPCollisionYakumonoDObjs->dobjs one per DObjDesc in order), so
    # these two are all src/dc/stage.c needs to point that array at the
    # pack's own DObjs instead of mpcommon.c's zeroed stand-ins. They go
    # in StgExtra's two unused pad fields, so the block stays 68 bytes and
    # the magic stays STG4; a stage built before this reads 0/0, which
    # means "no layer 1" and leaves the stand-ins in place.
    l1_base, l1_joints = 0, 0
    for (layer, jbase, njoints) in spans:
        if layer == 1:
            l1_base, l1_joints = jbase, njoints
    # The port's own yakumono count (src/dc/mpcommon.c): the highest id
    # any line group names, plus one. Asserted against the layer's tree
    # here so a stage whose ids outrun it fails at export rather than
    # indexing past the pack's joints on the console.
    yaks = max((y for y, _k in geo["line_info"]), default=-1) + 1
    if yaks > l1_joints:
        raise AssertionError(
            "%s: %d yakumono id(s) but layer 1 has %d joint(s) -- the "
            "collision layer cannot answer for every line group"
            % (stage, yaks, l1_joints))

    extra = struct.pack("<4s6H", b"STG6", geo["yak_count"],
                        len(geo["links"]), len(geo["vids"]),
                        len(geo["vpos"]), len(geo["mapobjs"]), l1_base)
    extra += struct.pack("<4h", *ground["cam"])
    extra += struct.pack("<4h", *ground["map"])
    extra += struct.pack("<hH", ground["alt_warning"], l1_joints)
    extra += struct.pack("<I", ground["bgm"])
    extra += struct.pack("<I", (ground["fog"][0] << 16) |
                         (ground["fog"][1] << 8) | ground["fog"][2])
    extra += struct.pack("<3f", *ground["light_angle"])
    extra += struct.pack("<12B", *ground["emblem"])
    # STG5: the team-rung bounds and the bonus pause's zoom pair, after
    # STG4's 68 bytes (src/dc/stage.c StgExtra)
    extra += struct.pack("<4h", *ground["cam_team"])
    extra += struct.pack("<4h", *ground["map_team"])
    extra += struct.pack("<3h", *ground["zoom_start"])
    extra += struct.pack("<3h", *ground["zoom_end"])
    # STG6: the CPU's emblem colour, emblem_colors[4], after STG5's 96
    extra += struct.pack("<3Bx", *ground["emblem_cp"])
    assert len(extra) == 100
    for yak_id, kinds in geo["line_info"]:
        extra += struct.pack("<H", yak_id)
        for group_id, count in kinds:
            extra += struct.pack("<2H", group_id, count)
    for first, count in geo["links"]:
        extra += struct.pack("<2H", first, count)
    extra += struct.pack("<%dH" % len(geo["vids"]), *geo["vids"])
    for x, y, flags in geo["vpos"]:
        extra += struct.pack("<2hH", x, y, flags)
    for kind, x, y in geo["mapobjs"]:
        extra += struct.pack("<H2h", kind, x, y)
    extra += b"\0" * (-len(extra) % 4)
    if wallpaper:
        texw, texh, imgw, imgh, data = wallpaper
        extra += struct.pack("<4s4H", b"WLP1", texw, texh, imgw, imgh)
        extra += data

    # -- GRA1 (Part B): the stage's background ground actors, between the
    # optional WLP1 block and MPK1 -- stage_load reads it right after WLP1
    # and before stage_load_map. Absent only for Hyrule.
    extra += ground_actors_section(ga)
    # -- BTG1: a Break the Targets course's ten target placements, after
    # GRA1 and before BWP1. Only the twelve bonus1 courses have one.
    if bt is not None:
        extra += bonus_placements_section(bt, b"BTG1")
        print("%s: targets: %d placement(s), %d script(s), %d word(s)"
              % (stage, len(bt["targets"]), len(bt["anims"]),
                 sum(len(a["words"]) for a in bt["anims"])))
    # -- BMP1: the bumpers five of the twelve Board the Platforms courses
    # stand on their own, the same shape and the same place in the file.
    # A course has one block or the other, never both.
    if bm is not None:
        extra += bonus_placements_section(bm, b"BMP1")
        print("%s: bumpers: %d placement(s), %d script(s), %d word(s)"
              % (stage, len(bm["targets"]), len(bm["anims"]),
                 sum(len(a["words"]) for a in bm["anims"])))
    if bw is not None:
        # -- BWP1: Final Destination's boss-wallpaper trees, after GRA1.
        # Only this stage has one; every other stage's .stg has no such
        # block and stage_load_boss leaves Stage.boss NULL.
        extra += tree_pack_section(bw)
        print("%s: boss wallpaper: %d pack(s), %d anim(s), %d bytes"
              % (stage, len(bw["packs"]), len(bw["anims"]),
                 sum(len(p["pack"]) for p in bw["packs"])))
    if ga is not None:
        print("%s: ground actors: %d pack(s), %d variant(s), %d anim(s), "
              "%d param(s)"
              % (stage, len(ga["packs"]), len(ga["descs"]), len(ga["anims"]),
                 len(ga["params"])))
        for p in ga["packs"]:
            if p["matanim"]:
                print("%s:   %s: MatAnimJoint %s, %d picture(s)"
                      % (stage, p["name"], "+".join(p["matanim"]),
                         p["frames"]))

    # -- MPK1: the stage's own map objects, the game's map file for the
    # stage. Its own FPack (baked the same way the layers are), the
    # AObjEvent32 scripts its ground logic plays, and the item weights.
    # Optional: a stage whose map file names no tree has no block.
    if mpk is not None:
        an = b""
        for a in mpk["anims"]:
            # Each script is its word count, THE JOINT of the tree it
            # belongs on, its name, and its words: the name is how the
            # port's stage logic asks for it (the decomp reaches the same
            # script through a linker symbol whose name is all it has,
            # `llGRJungleMapTaruCannFillAnimJoint`) and the joint is where
            # the game's own per-DObj walk puts it. The name is 32 bytes
            # because the decomp's own symbol names run to 27
            # (`WhispyMouthRightBlowTexture`), and a name truncated to 16
            # is silent on both sides of the wire.
            an += struct.pack("<II32s", len(a["words"]), a["joint"],
                              a["name"].encode()[:32])
            an += b"".join(struct.pack("<I", w) for w in a["words"])
        an = align(an)
        fx = b"".join(struct.pack("<3I", i, w, t)
                      for (i, a) in enumerate(mpk["anims"])
                      for (w, t) in a["fixups"])
        wt = mpk["weights"] or b""
        wt += b"\0" * (-len(wt) % 4)
        fx += b"\0" * (-len(fx) % 4)
        an += b"\0" * (-len(an) % 4)

        # The header is 52 bytes: 4s12I. Its last two fields are the
        # GRAFT pack (the object a stage's own code grafts under its map
        # tree) and its size; 0/0 for the stages that have none. THE
        # THREE PARSERS MOVE TOGETHER: this one, stage_load_map in
        # src/dc/stage.c, and host_load_map_file in hosttest_ft.c.
        at = b""
        if mpk.get("attack") is not None:
            at = struct.pack("<7i", *mpk["attack"])
        at += b"\0" * (-len(at) % 4)
        gp = mpk.get("graft_pack") or b""
        gp += b"\0" * (-len(gp) % 4)

        # The object packs, one after another, and the table naming each
        # one's place among them: (offset, size) per object. Each becomes
        # its own fighter_init and its own entry in Stage.map_models,
        # because Dream Land builds four GObojs off four DObjDesc blocks
        # of one map file. THE THREE PARSERS MOVE TOGETHER: this one,
        # stage_load_map in src/dc/stage.c, and host_load_map_file in
        # hosttest_ft.c.
        pk = b""
        ob = b""
        for o in mpk["packs"]:
            ob += struct.pack("<2I", len(pk), len(o["pack"]))
            pk += align(o["pack"])
        ob = align(ob)

        parts = (pk, an, fx, wt, at, gp, ob)
        offs = []
        cur = 60
        for x in parts:
            offs.append(cur)
            cur += len(x)
        fixups = sum(len(a["fixups"]) for a in mpk["anims"])
        extra += struct.pack("<4s14I", b"MPK1", len(pk),
                             len(mpk["anims"]), fixups,
                             len(mpk["weights"] or b""),
                             len(mpk["attack"] or ()), len(gp),
                             len(mpk["packs"]),
                             offs[0], offs[1], offs[2], offs[3],
                             offs[4], offs[5], offs[6])
        extra += b"".join(parts)
        if mpk.get("graft_pack"):
            print("%s: graft: %d joints, %d verts, %d tris, %d batches"
                  % (stage, mpk["graft_nodes"], mpk["graft_verts"],
                     mpk["graft_tris"], mpk["graft_batches"]))
        print("%s: map file: %d item weights%s%s"
              % (stage, len(mpk["weights"] or b""),
                 (", %d object(s): %s, %d anims (%s), %d fixups"
                  % (len(mpk["packs"]),
                     ", ".join("%s %d joints, %d verts, %d tris, %d "
                               "batches%s"
                               % (o["desc"], o["joint_count"],
                                  o["vert_count"], o["tri_count"],
                                  o["batch_count"],
                                  (", %d mobj alts" % o["mobj_alts"])
                                  if o["mobj_count"] else "")
                               for o in mpk["packs"]),
                     len(mpk["anims"]),
                     ", ".join(a["name"] for a in mpk["anims"]), fixups))
                 if mpk["packs"] or mpk["anims"] else "",
                 (", other file: " + ", ".join(obj.get("skipped", [])))
                 if obj and obj.get("skipped") else ""))

    pack = align(pack)
    blob = struct.pack("<8s2I", b"SSBSTAG1", len(pack), 16 + len(pack)) + \
        pack + extra
    with open(out, "wb") as fp:
        fp.write(blob)
    print("wrote %s (%d bytes: %d model, %d extras)"
          % (out, len(blob), len(pack), len(extra)))


if __name__ == "__main__":
    main()

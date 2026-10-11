#!/usr/bin/env python3
"""disc_layout.py -- where each file goes on the GD-ROM.

mkisofs writes files in the order it walks the tree, which is alphabetical:
`aeroplane.tra` first and `zelda.mdl` last. That is the worst order there
is, because it interleaves every scene's files with every other scene's.
`-sort <file>` overrides it -- one `<path> <weight>` line per file, highest
weight nearest the start of the data track -- and this is what writes that
file.

The order is the layout table, in code, so the plan and
the image cannot drift apart without one of them failing: a file in the
tree that MANIFEST does not name is an error here, and tools/check/disc_check.py
reads the finished ISO back and asserts the extents really ascend in this
order.

What it is and is not for, from the same plan: the whole game is ~50 MB,
about 7% of a CD, so every file is within a narrow radial span of every
other and seek *time* is near the drive's minimum whatever we do. The
order is not a seek-avoidance trick. It is here so that each scene's read
is one ascending run -- the drive streams forward instead of stepping back
and forth over the same few millimetres -- and so that a scene's files
sit together for the read-ahead. The levers that decide a load screen are
still how many bytes a scene reads (the `io:` log) and when it
reads them (file lifetimes).

Where the run sits is the pad's job (PAD below). The GD-ROM drive spins a
CD at constant angular velocity, so the outer edge passes the laser about
twice as fast as the inner edge, where an unpadded image puts every
byte. A zero-filled file of the right size, first on the track, pushes
the whole game out to the end of an 80-minute disc.
"""
import argparse
import os
import sys

# nFTKind order, which is the order scvsresults.c:942 and every other
# per-fighter table in the port is written in.
FIGHTERS = [
    "mario", "fox", "donkey", "samus", "luigi", "link",
    "yoshi", "captain", "kirby", "pikachu", "purin", "ness",
]

# Three fighters carry a particle bank of their own, named from
# dFTManagerDataFiles rather than by symbol (ft/ftdata.c:3891, 4957, 6500).
# ftManagerSetupFilesAllKind reads a bank BETWEEN its owner's pack and its
# owner's stock icons, so that is where the bank sits on the disc: a
# -DDB_IO_TRACE run reads yoshi.pack, particles_unk2.*, ftyoshi.spr with no
# seek between them, where before the three banks lived in the battle group
# a megabyte away and the select's read jumped out and back three times.
FIGHTER_BANKS = {
    "yoshi": "particles_unk2",
    "kirby": "particles_unk0",
    "ness":  "particles_unk1",
}

# Each pack, then its owner's particle bank if it has one, then its stock
# icon bank: exactly the order ftManagerLoadFighterFiles asks for them,
# twelve times, and that is the one 5.9 MB run in the game.
SELECT_PACKS = [
    f
    for k in FIGHTERS
    for f in ([k + ".pack"]
              + ([FIGHTER_BANKS[k] + e for e in (".scb", ".txb", ".txp")]
                 if k in FIGHTER_BANKS else [])
              + ["ft" + k + ".spr"])
]

# src/dc/efmanager.c's dEFManagerModels, in the order efDisplayInitAll
# walks it: every effect model is read at the battle's load, not on the
# first spawn, so this is the order the disc is asked for them in.
EF_MODELS = [
    "ef%s.mdl" % n for n in (
        "halo mballrays swirl orbs slash spark mdust dexp shield yegg "
        "shock firespark impactwave dokan taru point star wave beam "
        "entryegg arwing car egglay cutup cutdown cutdraw cuttrail spin "
        "vulcan psimagnet pkwave pktrail reflectbreak catchswirl "
        "grapplebeam reflector falconkick falconpunch sing mballthrown "
        "joltspark thundertrail kirbystar thundershock quake".split())
]

# src/dc/wpmanager.c's dWPManagerModels in the same way -- the fighters'
# weapons first, in nFTKind order, then the seven Poke Ball monsters'
# weapons out of ITCommonObject, then Sector Z's Arwing bolt and the Ray
# Gun's shot. FIVE more of that file's weapon tables have no model and
# tools/export/ssb_itemmodelexport.py's KNOWN_UNBAKED says which and why:
# Charizard's flame has no data pointer, the Fire Flower's does not decode,
# and the Ray Gun's ammo and the Star Rod's stars never were trees (they
# are one-quad weapons, wplgunammo.mdl and Yoshi's wpyoshistar.mdl).
WP_MODELS = [
    "wp%s.mdl" % n for n in (
        "mariofireball foxblaster samuschargeshot pikachujolt yoshistar "
        "yoshieggthrow samusbomb linkboomerang kirbycutter pikachugjolt "
        "pikachuthunder linkspin nesspkfire nesspkthunder nesspktrail "
        "nyarscoin dogassmog warkrock starmieswift spearswarm pippiswarm "
        "kamexhydro fushigibanarazor arwinglaser lgunammo "
        # Master Hand's finger rocket, last because he is
        # rung 13's and no other scene spawns him. One pack for both the
        # Normal and the Hard bullet, and the only weapon model in this
        # list baked out of a FIGHTER's model file under attributes that
        # live in his MainMotion.
        "bossbullet".split())
]

# src/dc/itemmodel.c's table, in its own order (the exporter's --list
# prints what each bakes; re-run `python3 tools/export/ssb_itemmodelexport.py
# --emit-table` after changing it). All 34 of ITCommonObject's items
# export -- Heart's and Sword's `dl` is a DObjDLLink array rather than a
# display list, and reading it as one is what had them baking to nothing
# for a step -- then the seven STAGE items, whose models
# come out of the stage model files; then PK Fire's pillar
# and Link's bomb (G05); then the Container smash debris, which is not an
# item and is the one block in ITCommonObject that is a bare display list
# rather than a DObjDesc tree (a Crate and a Barrel both play it when they
# break); then the display lists five items swap in
# (ALT_DISPLAY_LISTS in tools/export/ssb_itemmodelexport.py --alts).
ITEM_MODELS = [
    "it%s.mdl" % n for n in (
        "capsule tomato heart star sword bat harisen lgun fflower hammer "
        "msbomb bombhei starrod gshell rshell box taru mball wark kabigon "
        "tosakinto mew nyars lizardon spear kamex mlucky egg starmie "
        "sawamura dogas pippi gbumper nbumper "
        "powerblock pakkun glucky marumine hitokage fushigibana porygon "
        "nesspkfire linkbomb boxsmash tarubomb tarubombsmash "
        "target".split())
] + [
    "italtbombheiwalkl.mdl", "italtbombheiwalkr.mdl", "italtnbumperwait.mdl",
    "italtkamex.mdl", "italtsawamura.mdl", "italtwark.mdl",
]

# The three lists above, read back to back by the battle's preloads, are
# one file on the disc: tools/export/ssb_bundle.py packs them into models.bnd in
# exactly this order and src/dc/assetroot.c serves each by name from inside
# it. The manifest names the bundle, not the 124 files; the romdisk build
# keeps them loose. NOT YET TESTED ON REAL HARDWARE (2026-09-22).
BUNDLE = "models.bnd"
BUNDLE_MODELS = EF_MODELS + WP_MODELS + ITEM_MODELS

# lbtransition.c:83, in nLBTransitionKind order.
TRANSITIONS = [
    "aeroplane.tra", "check.tra", "gakubuthi.tra", "kannon.tra", "star.tra",
    "sudare1.tra", "sudare2.tra", "camera.tra", "block.tra", "rotscale.tra",
    "curtain.tra",
]

# scvsresults.c:942 with the duplicates dropped (Mario and Luigi share an
# emblem, so do Pikachu and Jigglypuff), first use keeping its place.
EMBLEMS = [
    "mario.mdl", "fox.mdl", "donkey.mdl", "metroid.mdl", "zelda.mdl",
    "yoshi.mdl", "fzero.mdl", "kirby.mdl", "pmonsters.mdl", "mother.mdl",
]

# The pad (scripts/make_cdi.sh, SSB_DISC_PAD): a file of zeros that is
# first on the data track, sized so the image ends PAD_MARGIN short of the
# furthest point an 80-minute blank allows. The volume descriptors, path
# tables and directory records stay in front of it: mkisofs always writes
# those first. That costs one inner-to-outer seek at boot and no more,
# because KOS keeps the directory's sectors (9 today) in its 16-block
# icache and only drops them when the tray opens (fs_iso9660.c iso_vblank).
PAD = "pad.bin"
SECTOR = 2048
SESSION_LBA = 11702     # make_cdi.sh's -C 0,11702: sector 0 of the image
# 79:59:74, the last lead-out start an 80-minute CD-R's ATIP allows, as
# an LBA (MSF minus the 150-sector pregap).
CD80_LEADOUT_LBA = 79 * 60 * 75 + 59 * 75 + 74 - 150
# cdi4dc's two GAP sectors after the data track (common.c
# write_gap_end_tracks), which the lead-out follows.
POSTGAP = 2
# 20 seconds. Blanks that stop at 79:59:71 and burners that round do not
# get to decide whether the disc fits.
PAD_MARGIN = 20 * 75
# The image's length, in sectors, once padded.
PAD_TARGET_SECTORS = CD80_LEADOUT_LBA - PAD_MARGIN - POSTGAP - SESSION_LBA


def pad_bytes(unpadded_sectors):
    """The pad's size, given the image's length (mkisofs -print-size)
    with the pad present but empty. An empty file takes no sector, and
    every byte of pad takes its own, so the padded image comes out at
    PAD_TARGET_SECTORS exactly."""
    spare = PAD_TARGET_SECTORS - unpadded_sectors
    if spare < 0:
        raise SystemExit("disc_layout: the image is %d sectors, %d past "
                         "what an 80-minute disc holds with the pad's "
                         "margin" % (unpadded_sectors, -spare))
    return spare * SECTOR


# The scene chain, top of the disc first, in the order a -DDB_IO_TRACE run
# actually opens them (tools/check/disc_order_check.py scores a log against this
# list and prints every file opened behind the one before it). The group
# names are the ones the `io:` lines use, so a layout group and a
# measured window can be read against each other.
MANIFEST = [
    # Nothing reads it. It is first so that everything below lands at the
    # outer edge; PAD above.
    ("pad", [
        PAD,
    ]),
    # The bootstrap reads 1ST_READ.BIN before anything else exists, and
    # assetroot.c probes disc.id before the first load.
    ("boot", [
        "1ST_READ.BIN", "disc.id",
    ]),
    # The tables are read once, at boot. The two sample packs are read
    # again at every scene change that needs a different sound set
    # (src/dc/sndres.c), a sample at a time -- and THAT read is what the
    # order serves: sndres_enter_scene takes fgm_sounds.pak before
    # bgm.pak, so the pair sits that way round and a scene change walks
    # forward through both. The boot's own bgm_bank_load reads bgm.pak
    # first and is the one step backwards here; it happens once.
    ("audio", [
        "sndsets.bin", "fgm.ucd", "fgm.tbl", "fgm.unk", "fgm_sounds.pak",
        "bgm.pak",
    ]),
    # The port's own boot reads: the save file's pictures
    # (src/dc/vmucard.c), read by sy_sram_init inside scManagerInitData,
    # and the resident font (src/dc/dctext.c), read by main() just after
    # -- both after the audio banks and before the first scene.
    ("port", [
        "vmuart.bin", "dcfont.spr",
    ]),
    # The N64 logo (194, src/dc/mnstartup.c): one sprite, and the first
    # scene asset a cold boot reads. nSCKindStartup is the US build's
    # default boot scene (src/dc/scmanagerdata.c:385-393), so this sits
    # ahead of the title rather than after it -- the only group whose
    # position is fixed by the game's own boot order rather than by a
    # measured window.
    ("startup", [
        "n64logo.spr",
    ]),
    ("title", [
        "mntitle.spr",
        # the flames' frames (file 168) and the labels' and PRESS START's
        # animation trees (file 167), read as the scene starts
        "mntitlefire.spr", "mntitlelabels.tra", "mntitlepress.tra",
        # the title after the opening movie: the animated
        # logo's tree, the logo fire's tree and the slash model, read as
        # the scene starts on that visit only
        "mntitlelogo.tra", "mntitlefiretree.tra", "mntitleslash.mdl",
        # mntitle.c:1469 loads the title's own particle bank;
        # every bank is three files, the .txp being the same images
        # twiddled for the PVR (src/dc/lbpartex.c)
        "mntitle.scb", "mntitle.txb", "mntitle.txp",
    ]),
    # In the order the menus come up: mode select, VS mode, VS options.
    # mngamemodes.spr is NOT here although it is a menu sprite -- the only
    # scene that reads it is the character select, so it sits there.
    # mndata.spr is last of them although DATA sits on the mode select
    # beside VS MODE: the VS chain is the path every match takes and the
    # DATA menu is a detour, so the four files between them stay
    # contiguous and the detour pays the one backward reach.
    #
    # mnoption.spr is OPTION's own file, placed right before
    # mndata.spr for the same reason: OPTION sits on the mode select
    # between VS MODE and DATA (mnmodeselect.c's own
    # nMNModeSelectOption{1PMode,VSMode,Option,Data} order), and is the
    # same kind of detour off the VS chain DATA is, so it pays the same
    # one backward reach rather than splitting the contiguous VS files.
    #
    # mnscreenadjust.spr is SCREEN ADJUST's own file,
    # Options' first child, placed right after mnoption.spr: one reach
    # further into the same detour, the same way the Records tab's own
    # file sits right after mndata.spr below.
    #
    # mnbackupclear.spr and mnbackupclearheader.spr are BACKUP CLEAR's
    # own two files, Options' second child, placed right
    # after mnscreenadjust.spr: one reach further still into the same
    # detour.
    #
    # mnvsrecord.spr and mndatacommon.spr are the Records tab's own
    # files and sit right after mndata.spr, one reach
    # further into the same detour. mnportraits.spr and mnfonts.spr are
    # the Records tab's other two files; both are already placed
    # earlier (the "select" and "stages" groups below) and are not
    # repeated here -- tools/check/disc_order_check.py's module comment says
    # why reopening them is cheaper than a second copy.
    #
    # mnsoundtest.spr is the Sound Test tab's own file, one
    # reach further still. Its other four files (ifpause.spr,
    # ifdamage.spr, mncommon.spr, mndatacommon.spr) are all already
    # placed -- the first two in the "battle" group below, reopened the
    # same way the Records tab's reused banks are.
    #
    # mncharacters.spr is the Character Data tab's own file,
    # last of the DATA detour. Its other files are all already placed:
    # mndatacommon.spr right above, ftemblems.spr in the "select" group
    # below (the winner's-emblem sprites), and the winner's-emblem
    # models themselves -- the ten EMBLEMS .mdl packs, already placed
    # for the results screen (below) and reopened here the same way the
    # Records tab's reused banks are.
    ("menus", [
        "mncommon.spr", "mnmain.spr", "mn1p.spr", "mn1pselect.spr",
        # the 1P game select's own two: the five difficulty
        # names the LEVEL slider scrolls, and the grey stock head
        "mn1pdiff.spr", "ftstockszako.spr",
        "mnvsmode.spr",
        "mnvsoptions.spr",
        "mnitemswitch.spr", "mnmessage.spr", "mnoption.spr",
        "mnscreenadjust.spr", "mnbackupclear.spr", "mnbackupclearheader.spr",
        "mndata.spr",
        "mnvsrecord.spr", "mndatacommon.spr", "mnsoundtest.spr",
        "mncharacters.spr",
        # The ladder's VS card (src/dc/sc1pintro.c): the card
        # itself (file 11) with the twenty-three camera scripts cut out
        # of the same file beside it, and one bonus-stage picture each
        # from files 13 and 14. characternames.spr, its second file, is
        # already placed with the attract demo's.
        "sc1pintro.spr", "sc1pintro.cam",
        # The three team cards' baked crowds (tools/export/ssb_introcrowd.py,
        # src/dc/introcrowd.h): optional, baked on the host, and
        # the card draws its models when a file is missing.
        "introcrowd_yoshi.bin", "introcrowd_kirby.bin", "introcrowd_poly.bin",
        "bonuspicture.spr", "bonuspictureplatform.spr",
        # "CHALLENGER APPROACHING!" (src/dc/sc1pchallenger.c):
        # one file, four decals, and it plays in the same part of the
        # ladder as the card above it.
        "sc1pchallenger.spr",
        # The boss rung's approach and defeat camera scripts, cut out of
        # Final Destination's layer file (src/dc/sc1pgame.c).
        "sc1pgameboss.cam",
        # The score screen after a rung (src/dc/sc1pstageclear.c): its three own files. The three
        # digit sets it also reads are the battle HUD's and are placed
        # with those; its wallpaper is the framebuffer, not a file.
        "sc1pstageclear1.spr", "sc1pstageclear2.spr",
        "sc1pstageclear3.spr",
        # The Continue prompt (src/dc/mn1pcontinue.c): the
        # other end of the same rung, and its one own file. Its other
        # four reloc files are the score screen's two just above, the
        # announcer's letters and the HUD's damage digits, all already
        # placed.
        "mn1pcontinue.spr",
    ]),
    # The character select, in mnPlayersVS's own load order. efcommon.*
    # is the common effect bank, read here by efDisplayInitAll and read
    # again by the battle; 818 KB, the largest single read either scene
    # makes, and it belongs to whichever scene reads it FIRST.
    ("select", [
        "mnplayers.spr", "ftemblems.spr", "mnselect.spr", "mngamemodes.spr",
        "mnportraits.spr",
        "efcommon.scb", "efcommon.txb", "efcommon.txp",
    ] + SELECT_PACKS + [
        "spotlite.mdl",
    ]),
    # The stage select. mnfonts.spr is a select sprite and sits here
    # because this is the scene that reads it, after mnmaps.spr.
    ("stages", [
        "mnmaps.spr", "mnfonts.spr",
        # all nine VS stages -- pupupu, yamabuki and
        # inishie were the three the Makefile said could not export, and
        # all three do. The last two have no gr/ logic file yet (Saffron's
        # Gate monster and Mushroom Kingdom's `?` block both want the
        # unported item roster), so their packs are selectable and their
        # stage is inert.
        #
        # In mnMapsGetGroundKind's order, which is the icon grid left to
        # right: the preview is loaded on every cursor move, so a player
        # walking the grid walks the disc. Which stages a given run reads,
        # and in what order, is the player's; this is the only order that
        # is the game's.
        "castle.stg", "jungle.stg", "hyrule.stg", "zebes.stg",
        "inishie.stg", "yoster.stg", "pupupu.stg", "sector.stg",
        "yamabuki.stg",
        # the three stages with hazards of their own load a particle bank
        # in their grXInit (grhyrule.c:411, grpupupu.c:689, gryoster.c:256),
        # at the battle's load rather than the select's
        "grhyrule.scb", "grhyrule.txb", "grhyrule.txp",
        "gryoster.scb", "gryoster.txb", "gryoster.txp",
        "grpupupu.scb", "grpupupu.txb", "grpupupu.txp",
    ]),
    # The battle, in scVSBattle's load order. The chosen stage's .stg and
    # its gr* bank are read from the stages group above, which is the one
    # reach backwards this window cannot avoid: which stage it is is not
    # known until the player picks it.
    ("battle", [
        # Final Destination. Every other stage pack is in
        # the "stages" group above, because the stage select's preview
        # reads it before the battle does; this one no select can reach.
        # The 1P ladder's last rung asks for it and nothing else ever
        # does, so it belongs to the battle's window and not to the
        # select's.
        "last.stg",
        # The Duel Zone, Meta Crystal and the small Yoshi's Island, rungs 12,
        # 10 and 1, here for exactly the reason
        # last.stg is: no select can reach any of them, only the ladder.
        "zako.stg", "metal.stg", "yostersmall.stg",
        # Break the Targets, for the same reason as
        # last.stg above and one step further: no select can reach these
        # either, only the ladder's bonus rungs and the bonus-practice
        # select, and a run plays exactly one of the twelve -- the
        # human's own fighter's. They sit together so that one is a
        # short seek from the battle's other reads whichever it turns
        # out to be.
        "bonus1mario.stg", "bonus1fox.stg", "bonus1donkey.stg",
        "bonus1samus.stg", "bonus1luigi.stg", "bonus1link.stg",
        "bonus1yoshi.stg", "bonus1captain.stg", "bonus1kirby.stg",
        "bonus1pikachu.stg", "bonus1purin.stg", "bonus1ness.stg",
        # Board the Platforms, beside them for
        # the same reason.
        "bonus2mario.stg", "bonus2fox.stg", "bonus2donkey.stg",
        "bonus2samus.stg", "bonus2luigi.stg", "bonus2link.stg",
        "bonus2yoshi.stg", "bonus2captain.stg", "bonus2kirby.stg",
        "bonus2pikachu.stg", "bonus2purin.stg", "bonus2ness.stg",
        # and the six trees all twelve of them build their
        # platforms from, which is one file for that reason
        # and is read right after the course.
        "bonus2plat.pak",
        # Race to the Finish, the third bonus
        # stage, beside the other two.
        "bonus3.stg",
        # Giant Donkey Kong, the ladder's seventh rung and
        # the first of the 1P game's own fighter kinds. Here and not in
        # the "select" group beside the twelve: no select can reach him,
        # only that rung, so he belongs to the battle's window for the
        # same reason last.stg and the bonus courses above do. His pack
        # is Donkey Kong's geometry and animations under his own
        # attributes, so it is a full 1.5 MB read; the bank beside it is
        # his stock icon and emblem, which are Donkey Kong's own file.
        "gdonkey.pack", "ftgdonkey.spr",
        # Metal Mario, rung 10, here for the same
        # reason. His pack is Mario's geometry re-textured and
        # both their motion files, so it is the smaller read.
        "mmario.pack", "ftmmario.spr",
        # The twelve Fighting Polygons, rung 12. All
        # twelve are resident at once: the rung deals thirty enemies
        # out of twelve distinct kinds (sc1pgame.c:1256-1269), so this
        # is the largest set of packs any scene in the game asks for.
        # Each is its base fighter's animations under a flat-shaded
        # model of its own, so they read big and draw cheap. Each bank
        # beside its pack is Master Hand's emblem alone: their stocks
        # are ftstockszako.spr's grey head.
        "nmario.pack", "ftnmario.spr",
        "nfox.pack", "ftnfox.spr",
        "ndonkey.pack", "ftndonkey.spr",
        "nsamus.pack", "ftnsamus.spr",
        "nluigi.pack", "ftnluigi.spr",
        "nlink.pack", "ftnlink.spr",
        "nyoshi.pack", "ftnyoshi.spr",
        "ncaptain.pack", "ftncaptain.spr",
        "nkirby.pack", "ftnkirby.spr",
        "npikachu.pack", "ftnpikachu.spr",
        "npurin.pack", "ftnpurin.spr",
        "nness.pack", "ftnness.spr",
        # Master Hand, rung 13, and the last of the 1P
        # game's own fighter kinds. Here for the same reason as the rung
        # kinds above: no select reaches him. His pack is the smallest
        # of them all -- 18 joints, 19 batches, 474 triangles and not one
        # texture, because he is flat-shaded white -- and it is the only
        # pack whose animations outweigh its geometry four to one. His
        # bank is 345_MasterHandIcon.c's stock icon and emblem.
        "boss.pack", "ftboss.spr",
        # the fighters' weapon hitbox tables (WPAttributes), one blob per
        # ROM file, bound to the gFTData* file pointers by wpAttrsBind
        # from ftCommonOverlayLoad on every overlay-3 reload; read once
        # per boot by wpAttrsLoad (src/dc/wpattrs.c).
        "wpattrs.bin",
        "ifstatus.spr", "ifdamage.spr", "iftimer.spr", "ifdigits.spr",
        "ifpause.spr", "iftags.spr", "ifannounce.spr",
        # the item pack: ITCommonData (reloc 0xFB) and ITCommonObject
        # (0x56), the two files every it/*.c reads its model and its
        # attributes out of. Read once by itemPackLoad, which is the
        # port's replacement for the two lbRelocGetFileData the game's
        # itManagerInitItems would have made -- then the
        # item manager's own particle bank, at itmanager.c:150.
        "itcommon.itp", "itcommon.scb", "itcommon.txb", "itcommon.txp",
        # the red pointer a dropped item shows while it can be picked up
        # (IFCommonItem, relocData file 87 -- one sprite and nothing
        # else). Loaded on the first itManagerInitItems by
        # ifCommonItemArrowSetAttr.
        "ifcommonitem.spr",
        # Training mode's HUD and pause-menu text: the 84 sprites of
        # relocData file 29 IFCommonItemNames that file 254's four
        # layout tables name (src/dc/sc1ptrainingmode.c).
        "sc1ptrain.spr",
        # Training mode's three flat backgrounds (relocData 26-28),
        # one read per match by sc1PTrainingModeLoadWallpaper and per
        # cursor move by the stage select's training preview.
        "grwptrainingblack.spr", "grwptrainingyellow.spr",
        "grwptrainingblue.spr",
        BUNDLE,
    ] + [
        # the blob under every fighter, read once by the first
        # ftShadowMakeShadow of the battle
        "ftshadow.mdl",
        "ifmagnify.mdl", "ifarrows.mdl",
    ] + TRANSITIONS),
    ("results", [
        "mnresults.spr",
    ] + EMBLEMS),
    # The congratulations screen's own 24 files (src/dc/
    # mncongra.c): one bottom/top pair per fighter, in FTKind's own
    # playable order (the same order $(FIGHTERS) walks in the Makefile).
    # Reached only from 1P mode or Debug Battle, neither ported yet,
    # so there is no real play-order constraint
    # yet to place these against -- they sit last, after the group a
    # completed match actually ends on.
    ("congra", [
        "mncongramariobottom.spr", "mncongramariotop.spr",
        "mncongrafoxbottom.spr", "mncongrafoxtop.spr",
        "mncongradonkeybottom.spr", "mncongradonkeytop.spr",
        "mncongrasamusbottom.spr", "mncongrasamustop.spr",
        "mncongraluigibottom.spr", "mncongraluigitop.spr",
        "mncongralinkbottom.spr", "mncongralinktop.spr",
        "mncongrayoshibottom.spr", "mncongrayoshitop.spr",
        "mncongracaptainbottom.spr", "mncongracaptaintop.spr",
        "mncongrakirbybottom.spr", "mncongrakirbytop.spr",
        "mncongrapikachubottom.spr", "mncongrapikachutop.spr",
        "mncongrapurinbottom.spr", "mncongrapurintop.spr",
        "mncongranessbottom.spr", "mncongranesstop.spr",
    ]),
    # No Controller's own file (src/dc/mnnocontroller.c): one
    # sprite. Reached only by a direct DB_BOOT_SCENE boot -- not even 1P
    # mode or Debug Battle reach it, the way they reach congra above --
    # since the port's own scManagerInitData never carries the
    # gSYControllerConnectedNum == 0 check the decomp gates it behind
    # (src/dc/scmanager.c's own note). No play-order constraint at all,
    # so it sits last, after even congra.
    ("nocontroller", [
        "mnnocontroller.spr",
    ]),
    # How to Play's own file (src/dc/scexplain.c): the
    # textbox illustration and the six phase-indicator icons. Reached
    # today only by a direct DB_BOOT_SCENE boot -- the real entry point
    # (mnTitleProceedDemoNext's own title-idle-demo cycle) is not ported,
    # the same gap as
    # auto-demo -- so, like nocontroller above, there is no real
    # play-order constraint yet and it sits last.
    # adds the three packs the file's non-sprite half bakes
    # to (tools/export/ssb_explainexport.py): the control-stick diagram, its
    # tap-spark flash and the special-move RGB overlay. All four are read
    # back to back in scExplainLoadExplainFiles, in this order.
    ("explain", [
        "scexplaingraphics.spr",
        "scstick.mdl",
        "scspark.mdl",
        "scrgb.mdl",
    ]),
    # The ending diorama (src/dc/mvending.c) adds no romdisk
    # asset of its own: it poses a fighter through ftManagerSetupFilesAllKind,
    # the same fighter pack every battle already carries, and its room
    # props are the opening Room's own packs, read again from the
    # "openings" group below (each file is on the disc once). The staff roll
    # (src/dc/scstaffroll.c) that follows it has one, the same shape
    # explain's own file above is: reached only by mvEndingFuncRun's own
    # hand-off (tic 660) or a direct DB_BOOT_SCENE boot, so it sits last.
    # The ending diorama's camera path (src/dc/camanim.c):
    # 1,272 bytes, read once in mvEndingFuncStart before the first camera
    # is made, so it sits with the scene that reads it rather than with
    # the openings' banks below.
    ("ending", [
        "mvending.cam",
    ]),
    # adds the packs the file's non-sprite half bakes to
    # (tools/export/ssb_staffrollexport.py): the 56 credit glyphs, in two halves
    # of 28 because fighter.h's own FIGHTER_MAX_JOINTS is 40, and the
    # name plaque. All three are read in scStaffrollSetupFiles, in this
    # order, after the bank.
    ("staffroll", [
        "scstaffrollgraphics.spr",
        "scglyphs0.mdl",
        "scglyphs1.mdl",
        "scplaque.mdl",
    ]),
    # The openings' first scene (src/dc/mvopeningroom.c):
    # one sprite, the room wallpaper the wipe transition reveals. Every
    # other asset this scene wants is a relocData scene-graph tree the
    # port has no loader for (src/dc/mvopeningroom.h), and its two
    # trophy fighters come from the ordinary fighter packs. It is the
    # game's own default boot scene, so in a finished port it would sit
    # first -- but nothing reaches it today except a direct
    # DB_BOOT_SCENE boot (the title's own idle-demo cycle,
    # mnTitleProceedDemoNext, is still not ported), so like explain and
    # staffroll above it has no real play-order constraint yet and sits
    # last.
    # mvopeningroom.cam is this scene's second file: the
    # four camera paths, read in FuncStart right after the wallpaper.
    # mvopeningcommon.cam is the eight per-fighter opening scenes' shared
    # camera file, exported once and shipped ahead of them -- nothing
    # reads it yet, so it goes last of the two.
    #
    # mvopeningportraitsset1/2.spr are the second scene's own two files
    # (src/dc/mvopeningportraits.c) -- pure sprite content,
    # tier 1 of the still-unported eighteen. Reached the same way the
    # rest of this group is (a direct DB_BOOT_SCENE boot, or by playing
    # through from the room once nSCKindStartup's own chain is followed),
    # so no play-order constraint either; appended after the room's own
    # two files rather than interleaved with them.
    ("openings", [
        "mvopeningroomwallpaper.spr",
        "mvopeningroom.cam",
        # The room's four plain props and its five lone
        # display lists: read in FuncStart right after the
        # camera, in the order the makers run, which is the order
        # mvOpeningRoomFuncStart calls them. The pencils are the one
        # exception and stay where the props are -- their maker runs from
        # mvOpeningRoomFuncUpdate at tic 560, not from FuncStart.
        "mvopeningroomoutside.mdl",
        "mvopeningroomhaze.mdl",
        "mvopeningroombackground0.mdl",
        "mvopeningroombackground1.mdl",
        "mvopeningroomsunlight.mdl",
        "mvopeningroomdesk.mdl",
        "mvopeningroombooks.mdl",
        "mvopeningroomlamp.mdl",
        "mvopeningroomlogo.mdl",
        "mvopeningroomtissues.mdl",
        "mvopeningroombossshadow.mdl",
        "mvopeningroompencils.mdl",
        # and the four the scene makes as it runs, in tic order: the
        # spotlight at 500, the camera snap at 860, the desk's ground at
        # the wipe (1040) and the close-up effect's two halves at 19 s.
        "mvopeningroomspotlight.mdl",
        "mvopeningroomsnap.mdl",
        # the wipe's two stars (relocData 63), read at 1040 just before
        # the desk's ground, by mvOpeningRoomMakeTransition
        "mvroomwipe.bin",
        "mvopeningroomdeskground.mdl",
        "mvopeningroomcloseupair.mdl",
        "mvopeningroomcloseupground.mdl",
        "mvopeningcommon.cam",
        "mvopeningportraitsset1.spr",
        "mvopeningportraitsset2.spr",
        # mvopeningrun.c: the eight fighters' and the
        # camera's scripts, and the scrolling wallpaper.
        "mvopeningrun.cam",
        "mvopeningrun.spr",
        # mvopeningcliff.c
        "mvopeningcliff.cam",
        "mvopeningcliff.spr",
        "mvopeningcliffhills.mdl",
        "mvopeningcliffocarina.mdl",
        # mvopeningyamabuki.c
        "mvopeningyamabuki.cam",
        "mvopeningyamabuki.spr",
        # mvopeningjungle.c
        "mvopeningjungle.cam",
        # mvopeningyoster.c
        "mvopeningyoster.cam",
        "mvopeningyoster.spr",
        "mvopeningyosternest.mdl",
        "mvopeningyosternestrest.mdl",
        "mvopeningyosterground.mdl",
        # mvopeningsector.c
        "mvopeningsector.cam",
        "mvopeningsector.spr",
        "mvopeningsectorwallpaper.spr",
        "mvopeningsectorgreatfox.mdl",
        "mvopeningsectorarwing.mdl",
        # mvopeningstandoff.c; its wallpaper is Cliff's
        "mvopeningstandoff.cam",
        "mvopeningstandoffground.mdl",
        "mvopeningstandofflightning.mdl",
        # mvopeningclash.c
        "mvopeningclash.cam",
        "mvopeningclashwallll.mdl",
        "mvopeningclashwalllr.mdl",
        "mvopeningclashwallul.mdl",
        "mvopeningclashwallur.mdl",
        # mvopeningrun.c's crash
        "mvopeningruncrash.mdl",
        # mvopeningnewcomers.c
        "mvopeningncpurin.mdl",
        "mvopeningncpurinhide.mdl",
        "mvopeningnccaptain.mdl",
        "mvopeningnccaptainhide.mdl",
        "mvopeningncluigi.mdl",
        "mvopeningncluigihide.mdl",
        "mvopeningncness.mdl",
        "mvopeningncnesshide.mdl",
        "mvopeningyamabukilegs.mdl",
        "mvopeningyamabukilegsshadow.mdl",
        "mvopeningyamabukimball.mdl",
    ]),
    # The attract demo's fighter name plates (src/dc/scautodemo.c
    # scAutoDemoInitSObjs): twelve sprites in one
    # bank, read once in scAutoDemoFuncStart. The demo itself is reached
    # only by a direct DB_BOOT_SCENE boot -- mnTitleProceedDemoNext's own
    # idle cycle is still not ported, the same reason explain, staffroll
    # and the openings above sit at the end -- so this has no real
    # play-order constraint either and goes last.
    ("autodemo", [
        "characternames.spr",
    ]),
]


def group_files(files, bundled=True):
    """A group's files, with the bundle standing for its entries when the
    disc carries them loose (scripts/make_cdi.sh SSB_DISC_NO_BUNDLE=1).
    A fighter's .anm (src/dc/fighter.h FPackAnm) sits right after its
    .pack, which is the order fighter_load reads the two in."""
    out = []
    for f in files:
        if f == BUNDLE and not bundled:
            out.extend(BUNDLE_MODELS)
        elif f.endswith(".pack"):
            out.extend([f, f[:-len(".pack")] + ".anm"])
        else:
            out.append(f)
    return out


def manifest_order(bundled=True):
    """Every name the manifest knows, in disc order. bundled=False puts the
    bundle's entries where the bundle is, which is also the order a read
    of the bundled disc visits them in (tools/check/disc_order_check.py)."""
    names = []
    for _group, files in MANIFEST:
        names.extend(group_files(files, bundled))
    return names


def manifest_groups():
    """name -> group, for the reports; a bundled name is its bundle's."""
    return {f: g for g, files in MANIFEST for f in group_files(files, False)
            } | {BUNDLE: g for g, files in MANIFEST if BUNDLE in files}


def check_duplicates():
    names = manifest_order(bundled=False)
    dupes = sorted({n for n in names if names.count(n) > 1})
    if dupes:
        raise SystemExit("disc_layout: MANIFEST names a file twice: "
                         + ", ".join(dupes))


def tree_files(tree):
    """Every file in the staged tree, as a path relative to it."""
    out = []
    for root, _dirs, files in os.walk(tree):
        rel = os.path.relpath(root, tree)
        for name in files:
            out.append(name if rel == "." else os.path.join(rel, name))
    return sorted(out)


# The load census's order:
# tools/check/loadcensus.py layout writes the files the census runs read,
# in the order that makes their trips cheapest. They go right after the
# pad and the boot files; everything else keeps the manifest's order
# after them. The manifest is still what says which files exist.
ORDER_FILE = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                          "disc_order.txt")


# The census's pack list: files
# the census runs always read back to back, which tools/export/ssb_bundle.py
# puts in models.bnd beside the models, so that a chain of them is one
# stream on one handle instead of an open and a stream start each.
# Written by tools/check/loadcensus.py pack.
PACK_FILE = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                         "bundle_census.txt")


def bundle_names():
    """models.bnd's entries in their order: the census's packed files and
    the models, in the census order (ORDER_FILE) where it places them --
    the models as one run where the order names the bundle -- and the rest
    after, the models in BUNDLE_MODELS order."""
    if not os.path.exists(PACK_FILE):
        return list(BUNDLE_MODELS)
    extra = [ln.strip() for ln in open(PACK_FILE)
             if ln.strip() and not ln.startswith("#")]
    extra = [f for f in dict.fromkeys(extra) if f not in BUNDLE_MODELS]
    want = set(extra)
    order = []
    if os.path.exists(ORDER_FILE):
        order = [ln.strip() for ln in open(ORDER_FILE)
                 if ln.strip() and not ln.startswith("#")]
    out = []
    for f in order:
        if f == BUNDLE and not set(BUNDLE_MODELS) & set(out):
            out.extend(BUNDLE_MODELS)
        elif f in want and f not in out:
            out.append(f)
    if not set(BUNDLE_MODELS) & set(out):
        out.extend(BUNDLE_MODELS)
    out.extend(f for f in extra if f not in out)
    return out


def census_order(order):
    if not os.path.exists(ORDER_FILE):
        return order
    known = set(order)
    hot = [ln.strip() for ln in open(ORDER_FILE)
           if ln.strip() and not ln.startswith("#")]
    hot = [f for f in dict.fromkeys(hot) if f in known]
    first = [f for g, files in MANIFEST if g in ("pad", "boot")
             for f in group_files(files)]
    lead = [f for f in order if f in first]
    rest = [f for f in order if f not in first and f not in set(hot)]
    return lead + [f for f in hot if f not in first] + rest


def sort_lines(tree):
    """The mkisofs sort file for the staged ISO tree at `tree`.

    A manifest entry the tree does not have is skipped without comment.
    A file the tree has and the manifest does not is the error,
    because that is the drift this exists to catch: a new asset would
    otherwise land wherever the alphabet put it and nobody would know.
    """
    check_duplicates()
    have = set(tree_files(tree))
    order = manifest_order(bundled=BUNDLE in have)
    unknown = sorted(have - set(order))
    if unknown:
        raise SystemExit(
            "disc_layout: %d file(s) in %s that the layout table does not "
            "name:\n  %s\nAdd them to MANIFEST in the scene that reads them."
            % (len(unknown), tree, "\n  ".join(unknown)))

    order = census_order(order)
    present = [n for n in order if n in have]
    # Descending, so the first manifest entry gets the highest weight and
    # lands nearest the start of the track. Positive throughout, because
    # mkisofs gives a file no sort line mentions weight 0 and everything
    # the manifest names has to come before those.
    top = len(present)
    return [(os.path.join(tree, name), top - i)
            for i, name in enumerate(present)]


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("tree", nargs="?",
                    help="the staged ISO tree (build-dc/disc/<h>/iso)")
    ap.add_argument("-o", "--out", help="sort file to write (default stdout)")
    ap.add_argument("--list", action="store_true",
                    help="print the manifest order and stop")
    ap.add_argument("--pad-bytes", type=int, metavar="SECTORS",
                    help="print the pad's size for an image of SECTORS "
                         "(mkisofs -print-size, pad empty) and stop")
    args = ap.parse_args()

    if args.pad_bytes is not None:
        print(pad_bytes(args.pad_bytes))
        return 0

    if args.list:
        check_duplicates()
        for group, files in MANIFEST:
            print("%-8s %s" % (group, " ".join(files)))
        return 0
    if args.tree is None:
        ap.error("a tree is required unless --list")

    lines = ["%s %d\n" % (path, weight)
             for path, weight in sort_lines(args.tree)]
    if args.out:
        with open(args.out, "w") as fp:
            fp.writelines(lines)
        print("disc_layout: %d files -> %s" % (len(lines), args.out))
    else:
        sys.stdout.writelines(lines)
    return 0


if __name__ == "__main__":
    sys.exit(main())

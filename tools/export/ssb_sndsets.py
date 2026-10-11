#!/usr/bin/env python3
"""ssb64-dc: which samples each scene needs in sound RAM, and whether it fits.

On the N64 the whole sound-effect bank (B1_sounds2) and the whole music
bank (B1_sounds1) are resident for the life of the cartridge. The
Dreamcast has 2 MB of sound RAM for both, and the VS game alone reaches
about 1.97 MB of sound effects and 0.89 MB of music as AICA ADPCM -- so
src/dc/sndres.c keeps only what the running scene can reach, and this is
where "can reach" is decided.

A sound set is a group of samples, both kinds:

  menu        the menu scenes' FGMs (title through stage select, the
              unlock message and the results screen's announcer and
              crowd) and the music those scenes' port files name
  core        every FGM a VS battle can play whoever is fighting: the
              common moveset, items and Pokemon, projectiles, effects,
              the HUD and announcer, and all nine stages -- plus the
              battle's own non-stage music (Star, Hammer, the Mushroom
              Kingdom hurry)
  results     the results screen's music: every win fanfare and Results
  fighter:K   one fighter's own FGMs: ft/ftchar/ft<k>/ and its
              relocData Main/MainMotion files, whose motion scripts and
              FTAttributes voices name them
  stage:G     one stage's own music track

and src/dc/sndres.c composes a scene's need out of them at runtime,
because only then is it known who is fighting where.

What counts as reachable is measured, not listed. An FGM id is reachable
from a group when a decomp source file in that group names it
(nSYAudioFGM*/nSYAudioVoice*, code and relocData alike); its samples are
the voice script's articulations, fork closure included, and each
articulation's trigger args (tools/export/ssb_fgmexport.py's walkers). A music
track's waves are the ones its notes actually select: the sequence is
walked with the port's own channel rules (src/dc/seq/seqcore.c: every channel
starts on the bank's first valid program, channel 9 on the percussion
instrument, program changes skip empty slots) and its own sound lookup
(bgmbank.c bgm_bank_lookup_sound's binary search, which with overlapping
key ranges does not pick what a scan would), and each track's loop body
is replayed until the set stops growing, since a program change inside a
loop body applies to the notes before it on the next pass.

  --check       rebuild everything from the decomp and the ROM, print the
                per-scene budget table, and fail if any tier-A id is
                unaccounted for or any scene's worst case exceeds the
                budget (./run.sh test sndsets)
  --out FILE    also write sndsets.bin for the target

sndsets.bin, all little-endian:
  char magic[8] "SSBSETS1"
  u32  n_groups, budget_bytes
  n_groups x { u32 off; u16 n_fgm; u16 n_bgm }
  per group at off: u16 fgm_bank_index[n_fgm], u16 bgm_wave_index[n_bgm]
Group order is fixed and src/dc/sndres.h names it: menu, core, results,
fighter 0..11 in nFTKind order, stage 0..8 in nGRKind order.

Usage: python3 tools/export/ssb_sndsets.py [--rom <rom.z64>] [--check] [--out F]
"""
import collections
import glob
import itertools
import os
import re
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path[:0] = [os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), d)
                for d in ("lib", "export", "check")]
import ssb_paths as P        # noqa: E402
P.require_decomp()
sys.path.insert(0, P.DECOMP_TOOLS)
import cseq_to_mid as CSQ    # noqa: E402
import ssb_bgmpack as BP     # noqa: E402
import ssb_fgmexport as FGM  # noqa: E402
import ssb_meshexport as M   # noqa: E402
import ssb_sfxexport as SFX  # noqa: E402

REPO = os.path.dirname(os.path.dirname(HERE))
DECOMP_SRC = os.path.join(P.DECOMP_DIR, "src")
PORT_SRC = os.path.join(REPO, "src", "dc")

MAGIC = b"SSBSETS1"

# Sound RAM the sets may use. 1,900,532 bytes is what snd_mem_available()
# reported at boot before the port loaded anything (the music bank's
# 910,548 plus the 989,984 left after it, from a boot log); the rest of
# the 2 MB is the ARM driver and KOS's stream buffers. The margin covers
# the allocator's own headers and anything KOS takes later.
SOUND_RAM_BYTES = 1900532
BUDGET_MARGIN = 64 * 1024
BUDGET = SOUND_RAM_BYTES - BUDGET_MARGIN

# ft/ftdef.h nFTKind and gr/grdef.h nGRKind, checked against the headers
# below rather than trusted.
FIGHTERS = ["Mario", "Fox", "Donkey", "Samus", "Luigi", "Link", "Yoshi",
            "Captain", "Kirby", "Pikachu", "Purin", "Ness"]
STAGES = ["Castle", "Sector", "Jungle", "Zebes", "Hyrule", "Yoster",
          "Pupupu", "Yamabuki", "Inishie"]
# The stage groups are indexed by nGRKind, not by position in STAGES
# above -- src/dc/sndres.c writes SNDRES_GROUP_STAGE + gkind -- so the
# group list runs to nGRKindCommonEnd and holds a slot for every kind,
# named or not. The three unnamed ones (Beta Dream Land, the Test Stage
# and How to Play) have no pack and get an empty group. The four 1P-only
# maps -- Small Yoshi's Island, Meta Crystal, Duel Zone and Race to the
# Finish -- were unnamed too until their packs were exported, and
# a rung on one of them played with its music not resident.
# src/dc/sndres.h's SNDRES_STAGES note says why the holes are held open
# rather than renumbered away. STAGES itself stays the VS nine, because
# that is what the battle and stage-select worst cases are taken over.
#
# The length of the hole is CHECKED against gr/grdef.h below rather than
# trusted: it was written as six first, which made this list one short
# of the SNDRES_STAGES the C computes, and the only symptom was
# sndres_init's "boot: no sound sets; the game is silent" -- a whole
# silent game for an off-by-one in a comment's worth of enum counting.
STAGE_KINDS = (STAGES + [None] * 3 +
               ["YosterSmall", "Metal", "Zako", "Bonus3", "Last"])
# "credits" (src/dc/sndres.h's own header note): the ending
# diorama and the staff roll, kept out of "menu" on purpose -- their two
# BGM tracks together are real weight, and "menu" already has to fit
# alongside every fighter at once for the character select screen, a
# co-occurrence these two scenes never have (nSCKindEnding poses one
# fighter, the run's own; nSCKindStaffroll poses none).
# "opening" (src/dc/sndres.h's own header note): the
# nineteen attract-movie scenes. They share one BGM track, started once
# by mvOpeningRoomFuncStart and left running across the whole chain, and
# they never co-occur with the menus, the results screen or a full
# roster -- so, like "credits" above, its own group rather than more
# weight on "menu".
#
# "1pbonus" is the three BONUS STAGES' own set: gr/grbonus/'s
# four files and the two items only they make, the target and the Race to
# the Finish barrel. It is LAST in this list on purpose -- a group appended
# here leaves every earlier group's index alone, which is what lets
# src/dc/sndres.h name them by number.
#
# It went in one step ahead of anything composing it, because the port
# already compiled a file naming its sounds (src/dc/ittarubomb.c) and an
# id no group holds is exactly the "not resident" silence sndsets exists
# to prevent. sc/sc1pmode/sc1pbonusstage.c is ported,
# so the group carries that scene's own announcer lines and the
# twelve Break the Targets courses' BGM as well, and scene_worst has a
# row for it.
#
# "attract" (src/dc/sndres.h's own header note): the one
# track mntitle.c's mnTitleProceedDemoNext starts on its way from How to
# Play to the character showcase, nSYAudioBGMExplain. mntitle.c is a
# MENU_SCENE_FILE, so the scan below would put the track on "menu" -- and
# there it tips the character select screen's worst case (menu plus all
# twelve fighters) 29 KB over the budget. The showcase reached that way
# poses only the two demo fighters the title picked, so the track rides
# its own group, composed with those two, and is taken out of "menu".
# Appended last, as "1pbonus" was, for the same reason.
GROUPS = (["menu", "core", "results"] +
          ["fighter:" + f for f in FIGHTERS] +
          ["stage:" + (s or "?%d" % i)
           for i, s in enumerate(STAGE_KINDS)] +
          ["credits", "opening", "1pintro", "1pgame", "1pchallenger",
           "1pstageclear", "1pcontinue", "1pbonus", "attract"])
ATTRACT_TRACKS = ["Explain"]          # bgm_names() drops the nSYAudioBGM

# The port's scene files, by the set whose music they name. Music is read
# out of the port rather than the decomp because the scene composition is
# the port's: these are the files whose syAudioPlayBGM calls run.
# The port's own scenes (src/dc/scport.h): no group of their own, so
# every FGM id they name must be one "menu" stages.
PORT_SCENE_FILES = ["dcmemcard.c"]

MENU_SCENE_FILES = ["mntitle.c", "mnmodeselect.c", "mnvsmode.c",
                    "mnvsoptions.c", "mnvsitemswitch.c", "mnplayersvs.c",
                    "mnmaps.c", "mnmessage.c", "mndata.c", "mnvsrecord.c",
                    "mn1pmode.c", "mnplayers1ptraining.c",
                    "mnplayers1pgame.c", "mnplayers1pbonus.c"]
RESULTS_SCENE_FILES = ["scvsresults.c"]
BATTLE_MUSIC_FILES = ["scvsbattle.c", "sc1ptrainingmode.c", "ftparam.c",
                      "ftmain.c", "ifcommon.c",
                      "ftcommon.c", "itmain.c", "mpshim.c"]
CREDITS_SCENE_FILES = ["mvending.c", "scstaffroll.c"]
# Only the Room calls syAudioPlayBGM; the other eighteen opening
# scenes inherit the track it starts. Add each one here as it lands,
# so a scene that ever does start its own is not missed.
OPENING_SCENE_FILES = ["mvopeningroom.c"]
# The 1P ladder's VS card. Its own group rather than more
# weight on "core": src/dc/sndres.h's note on SNDRES_GROUP_1PINTRO says
# why. It names both of the tracks it can start, nSYAudioBGM1PIntro and
# nSYAudioBGMBossStage.
INTRO_SCENE_FILES = ["sc1pintro.c"]
# A rung of the ladder. Its own group on top of "core" for
# the reason classify() gives below: the one track it names is Final
# Destination's, which no VS battle can ever reach.
GAME_SCENE_FILES = ["sc1pgame.c"]
# "CHALLENGER APPROACHING!". One track, nSYAudioBGM1PChallenger,
# which no other ported scene names, and one fighter. Its own group and
# not weight on "1pintro": the two cards never co-occur.
CHALLENGER_SCENE_FILES = ["sc1pchallenger.c"]
# The score screen after a rung. Four tracks -- the
# stage-clear and game-clear jingles and the bonus stage's pass and
# fail -- and no other ported scene names any of them.
STAGECLEAR_SCENE_FILES = ["sc1pstageclear.c"]
# The Continue prompt after a lost rung. Two tracks --
# nSYAudioBGM1PGameOver and nSYAudioBGM1PGameEndChoice -- and no other
# ported scene names either. Its own group and not weight on
# "1pstageclear": the score screen and the Continue prompt are the two
# ways a rung can end, and never both.
CONTINUE_SCENE_FILES = ["mn1pcontinue.c"]

# dSC1PGameStageDesc (src/dc/sc1pgame.c:378), the part of each row that
# costs sound RAM: the stage and who stands on it. A rung's cast is not
# the player's to choose the way a VS battle's is -- only the human is,
# and the allies the Mario Bros. and Giant DK rungs give are copies of
# the human's own fighter -- so the worst case is taken over the twelve
# humans against each row, not over every four-fighter combination.
#
# None for a stage is a stage the port ships no pack for, which is also
# a stage with no group: the five 1P-only maps (Small Yoshi's Island,
# Meta Crystal, Duel Zone, Race to the Finish, Final Destination). Their
# music is not staged today and src/dc/sndres.c's own arm says so.
# A kind the port has no pack for (Giant DK, Metal Mario, the Polygons,
# Master Hand) is named by the fighter it is built from where there is
# one, which is the conservative reading.
RUNGS = [
    ("Link",          "Hyrule",   ["Link"]),
    ("Yoshi Team",    "YosterSmall", ["Yoshi"]),
    ("Fox",           "Sector",   ["Fox"]),
    ("Break Targets", "Castle",   []),
    ("Mario Bros.",   "Castle",   ["Mario", "Luigi"]),
    ("Pikachu",       "Yamabuki", ["Pikachu"]),
    ("Giant DK",      "Jungle",   ["Donkey"]),
    ("Board Plats",   "Castle",   []),
    ("Kirby Team",    "Pupupu",   ["Kirby"]),
    ("Samus",         "Zebes",    ["Samus"]),
    ("Metal Mario",   "Metal",    ["Mario"]),
    ("Race Finish",   "Bonus3",   []),
    ("Polygon Team",  "Zako",     []),
    ("Master Hand",   "Last",     []),
    ("Luigi",         "Castle",   ["Luigi"]),
    ("Ness",          "Pupupu",   ["Ness"]),
    ("Jigglypuff",    "Yamabuki", ["Purin"]),
    ("Captain",       "Zebes",    ["Captain"]),
]

# A sample is uploaded in whole 32-byte store-queue lines (spu_memload_sq).
SQ_LINE = 32
AICA_MAX_SAMPLES = 65535


def classify(rel):
    """A decomp source file (path under src/) to its group, "excluded"
    for the tiers past alpha, or None for a file nobody has placed --
    which --check refuses, so a new file cannot go silently missing."""
    b = os.path.basename(rel)
    if rel.startswith("relocData/"):
        name = re.match(r"\d+_(\w+)\.c$", b).group(1)
        for f in FIGHTERS:
            if name in (f + "Main", f + "MainMotion", f + "Special1"):
                return "fighter:" + f
        if name in ("FTCommonMoveset", "ITCommonData"):
            return "core"
        # A map file names no FGM or Voice id at all -- its one sound is
        # its bgm_id, and BGM ids do not come through this function
        # (group_tracks below is where a track is placed). What this arm
        # really says is that a VS stage's map file is tier A and a
        # 1P-only one is not.
        if name.startswith("GR") and name.endswith("Map") and \
                "Bonus" not in name:
            return "core"
        # N*/MMario/GDonkey/Boss movesets, bonus-stage objects: 1P only
        return "excluded"
    # mvending.c: its own door-close stinger,
    # nSYAudioFGMDoorClose, is named only here and by mnsoundtest.c
    # (excluded on purpose -- see that exemption further down), so the
    # blanket mv/ exclusion just below would leave the port's own
    # mvEndingFuncRun call to it stray -- "credits" rather than "menu"
    # the way mnbackupclear.c/mncongra.c/scexplain.c below take it: see
    # GROUPS's own comment on why.
    if rel == "mv/mvending/mvending.c":
        return "credits"
    # mvopeningsector.c: its ambient loop,
    # nSYAudioFGMOpeningSectorAmbient, is named by no other file, so the
    # blanket mv/ exclusion would leave it stray -- "opening" stages it.
    if rel in ("mv/mvopening/mvopeningsector.c",
               "mv/mvopening/mvopeningclash.c",
               "mv/mvopening/mvopeningnewcomers.c"):
        return "opening"
    if rel.startswith(("db/", "sc/scsubsys/", "mv/", "audio/")):
        return "excluded"
    if rel.startswith("ft/ftchar/"):
        kind = rel.split("/")[2][2:]
        for f in FIGHTERS:
            if f.lower() == kind:
                return "fighter:" + f
        return None
    if rel.startswith(("ft/", "gm/", "ef/", "if/")):
        return "core"
    if rel.startswith("it/"):
        # The targets and the Race to the Finish barrels are the BONUS
        # stages' own items, and they were "excluded" until the barrel
        # was ported: its two cues, nSYAudioFGMTaruBombHit
        # and nSYAudioFGMTaruBombMap, are now named by a file the port
        # compiles, so they need a group that stages them. See "1pbonus"
        # in GROUPS above.
        return "1pbonus" if b in ("ittarget.c", "ittarubomb.c") else "core"
    if rel.startswith("wp/"):
        return "excluded" if rel.startswith("wp/wpboss/") else "core"
    if rel.startswith("gr/"):
        return "1pbonus" if rel.startswith("gr/grbonus/") else "core"
    # Training mode is a battle scene, so its sounds are the battle's;
    # the blanket sc/sc1pmode/ exclusion below would otherwise take it.
    if b in ("scvsbattle.c", "sc1ptrainingmode.c"):
        return "core"
    # The ladder's VS card is the same shape and the same
    # exemption, but it is not a battle -- it has a group of its own, so
    # the two tracks it starts are not weight on every battle's set.
    if b == "sc1pintro.c":
        return "1pintro"
    # A rung of the ladder IS a battle, and it takes
    # "core" on top of this -- but its own weight is not every battle's.
    # It is the one scene that names Final Destination's music (the
    # Polygon and Master Hand rungs have no exported stage pack, so
    # sc1PGameSetupStageAll sets gMPCollisionBGMDefault by hand), and
    # measuring it into "core" put a VS battle 131 KB over budget.
    if b == "sc1pgame.c":
        return "1pgame"
    # The bonus stages ARE battles and take "core" on
    # top of this, like sc1pgame.c above -- but their own weight is not
    # every battle's. The announcer's COMPLETE!, FAILURE and NEW RECORD
    # lines and the target's break cue belong with gr/grbonus/'s files
    # and the two bonus items, which "1pbonus" already holds.
    if b == "sc1pbonusstage.c":
        return "1pbonus"
    # "CHALLENGER APPROACHING!" is the VS card's shape
    # again and takes the same exemption, for one id: the star-KO
    # stinger it plays once, nSYAudioFGMDeadUpStar. That id is "core"
    # everywhere else (ft/ftcommon/ftcommondead.c names it too), and
    # this scene takes no "core" -- so it is staged here, one sample,
    # rather than dragging the whole battle set onto a picture.
    if b == "sc1pchallenger.c":
        return "1pchallenger"
    # The score screen, the same exemption once more. Its
    # three score-counting cues are named by no other decomp file, and
    # it takes no "core" to borrow them from.
    if b == "sc1pstageclear.c":
        return "1pstageclear"
    # The Continue prompt, and it has to come before the
    # blanket mn/mn1pmode/ exclusion below or it would be dropped. Three
    # of its four ids -- nSYAudioFGM1PGameContinue and the two announcer
    # words -- are named by no decomp file but this one and mnsoundtest.c
    # (excluded on purpose), so the exclusion would leave the port's own
    # calls stray. The fourth, nSYAudioFGMMenuScroll1, is "menu" through
    # mn/mnvsmode/, and this scene takes no "menu" -- so the group holds
    # its own copy, one sample, the way "1pchallenger" holds the star-KO
    # stinger above.
    if b == "mn1pcontinue.c":
        return "1pcontinue"
    if b in ("mntitle.c", "mnmodeselect.c", "mnmessage.c", "mnmaps.c",
             "mnplayersvs.c", "mn1pmode.c",
             "mnplayers1ptraining.c",
             "mnplayers1pgame.c",
             # the bonus stages' practice select, one
             # scene for both bonus games. Its two announcer voices,
             # "Break the Targets!" and "Board the Platforms!", are
             # also named by sc1pintro.c and so are in "1pintro"
             # already; every other id it names is a common menu sound.
             "mnplayers1pbonus.c") or rel.startswith("mn/mnvsmode/"):
        return "menu"
    # the DATA menu and its Records tab are ported (src/dc/mndata.c,
    # src/dc/mnvsrecord.c); the other two screens behind DATA are not.
    # mnbackupclear.c is the same shape: its own confirm-
    # applied jingle, nSYAudioFGMOptionBackupClear, is not named by any
    # other decomp file, so the whole mn/mnoption/ exclusion below would
    # leave it stray -- unlike mnoption.c and mnscreenadjust.c (whose own FGM
    # ids are all common menu sounds tier A already
    # covers through other files, so neither needed this. mncongra.c
    # is the same again: nSYAudioVoiceAnnounceCongra and
    # nSYAudioVoiceAnnounceIncredible are its own and no other decomp
    # file names them (gm/gmsound.h only declares the enum). scexplain.c
    # is the same shape again, down to one id: the "How to
    # Play!" announcer voice, nSYAudioVoiceAnnounceHowToPlay, is named
    # only here and by mnsoundtest.c (excluded below, on purpose -- see
    # its own exemption further down), so the excluded shape this file
    # would otherwise take (it names nothing else) would leave the port's
    # own scExplainFuncStart call to it stray.
    if rel in ("mn/mndata/mndata.c", "mn/mndata/mnvsrecord.c",
              "mn/mnoption/mnbackupclear.c", "mn/mncommon/mncongra.c",
              "sc/sccommon/scexplain.c"):
        return "menu"
    # scstaffroll.c is the same shape again: the
    # name-shooting minigame's "hit" cue, nSYAudioFGMTrainingSel, is
    # named only here and by mnsoundtest.c -- unlike scautodemo.c just
    # below, whose own lone id (nSYAudioVoicePublicExcited) is already
    # covered by scvsbattle.c's own "core" group, so it needs no
    # exemption of its own. "credits" rather than "menu" the way
    # mvending.c's own exemption above is: see GROUPS's own comment.
    if rel == "sc/sccommon/scstaffroll.c":
        return "credits"
    # XBOX: scautodemo.c stays out of the census on purpose but is staged
    # all the same: src/dc/sndres.c's sndres_enter_autodemo gives it the
    # VS battle's shape (core, four fighters, a VS stage), which "battle"
    # in scene_worst already holds to the budget.
    if rel.startswith(("mn/mndata/", "mn/mnoption/", "mn/mn1pmode/",
                       "sc/sc1pmode/")) or \
            b.startswith("mnplayers1p") or \
            b in ("scautodemo.c",):
        return "excluded"
    return None


def strip_comments(text):
    return re.sub(r"/\*.*?\*/|//[^\n]*", "", text, flags=re.S)


ID_RE = re.compile(r"\b(nSYAudio(?:FGM|Voice)\w+)")
BGM_RE = re.compile(r"\bnSYAudioBGM(\w+)")


def check_enum(header, enum, names, prefix):
    """names[i] must evaluate to i in the enum, so the group numbers
    src/dc/sndres.h uses are nFTKind/nGRKind values. A None entry is a
    kind the port has no name for and only holds a place; its index is
    still counted. Returns the enum, for a caller with more to check."""
    body = re.search(r"typedef enum %s\s*\{(.*?)\}" % enum,
                     open(header).read(), re.S).group(1)
    value = {}
    nxt = 0
    for line in strip_comments(body).splitlines():
        # case-blind: gr/grdef.h spells one of its aliases nGRkind
        m = re.match(r"\s*%s(\w+)\s*(?:=\s*(\w+))?\s*,?\s*$" % prefix,
                     line, re.I)
        if not m:
            continue
        rhs = m.group(2)
        if rhs is None:
            v = nxt
        elif rhs.lower().startswith(prefix.lower()):
            v = value[rhs[len(prefix):].lower()]
        else:
            v = int(rhs, 0)
        value[m.group(1).lower()] = v
        nxt = v + 1
    for i, n in enumerate(names):
        if n is None:
            continue
        if value.get(n.lower()) != i:
            raise SystemExit("%s: %s%s is %r, tools/export/ssb_sndsets.py expects "
                             "%d" % (header, prefix, n, value.get(n.lower()),
                                     i))
    return value


# ---- FGM -----------------------------------------------------------------

class FgmBank:
    def __init__(self, rom):
        self.blobs = {n: rom[o:o + s] for n, (o, s) in FGM.FGM_FILES.items()}
        self.ids = FGM.fgm_names()
        self.ucd = FGM.package_entries(self.blobs["fgm.ucd"])
        self.tbl = FGM.package_entries(self.blobs["fgm.tbl"])
        r = SFX.BANKS[2]
        self.sounds = SFX.parse_bank(rom[r["ctl"][0]:r["ctl"][1]])
        self._closure = {}

    def closure(self, vid):
        """Bank-2 sample indices voice script `vid` can trigger."""
        if vid not in self._closure:
            artics, forks = set(), {vid}
            queue = [self.ucd[vid]]
            while queue:
                off = queue.pop()
                for fork in FGM.walk_ucd(self.blobs["fgm.ucd"], off, artics,
                                         forks):
                    queue.append(self.ucd[fork])
            out = set()
            for a in artics:
                out |= FGM.walk_tbl(self.blobs["fgm.tbl"], self.tbl[a])
            self._closure[vid] = out
        return self._closure[vid]

    def sample_bytes(self, idx):
        """What ssb_fgmexport writes for bank sound `idx`, in sound RAM."""
        return adpcm_bytes(self.sounds[idx]["len"])


def adpcm_bytes(vadpcm_len):
    n = vadpcm_len // 9 * 16             # 9-byte frames of 16 samples
    while n > AICA_MAX_SAMPLES:          # ssb_fgmexport/ssb_bgmpack halve()
        n //= 2
    n += n & 1
    return ((n // 2) + SQ_LINE - 1) & ~(SQ_LINE - 1)


# ---- music ---------------------------------------------------------------

class MusicBank:
    def __init__(self, rom):
        r = SFX.BANKS[1]
        bank = BP.parse_bank(rom[r["ctl"][0]:r["ctl"][1]])
        (self.programs, self.perc, self.sounds, self.waves,
         self.prog_rows) = BP.layout(bank)
        self.n_insts = len(self.prog_rows)
        self.first_valid = next(i for i, row in enumerate(self.prog_rows)
                                if row is not None)
        self.seqs = BP.read_sbk(rom)
        from ssb_bgmexport import bgm_names   # numpy-free at import time
        self.names = bgm_names()

    def lookup(self, prog, key, vel):
        """bgmbank.c bgm_bank_lookup_sound, line for line."""
        row = self.prog_rows[prog]
        if row is None:
            return None
        _inst, first, count = row
        lo, hi = 1, count
        while hi >= lo:
            i = (lo + hi) // 2
            s = self.sounds[first + i - 1]
            if s["key_min"] <= key <= s["key_max"] and \
                    s["vel_min"] <= vel <= s["vel_max"]:
                return s["wave_index"]
            elif key < s["key_min"] or \
                    (vel < s["vel_min"] and key <= s["key_max"]):
                hi = i - 1
            else:
                lo = i + 1
        return None

    def track_waves(self, seq_index):
        seq = self.seqs[seq_index]
        offs = struct.unpack_from(">16I", seq, 0)
        events = []
        loops = {}
        for t, off in enumerate(offs):
            if not off:
                continue
            evs = CSQ.parse_track_to_events(seq, off, len(seq))
            ls = [tk for tk, _, e in evs
                  if e[0] == "marker" and e[1] == "loopstart"]
            le = [tk for tk, _, e in evs
                  if e[0] == "marker" and e[1].startswith("loopend")]
            if ls and le:
                loops[t] = (min(ls), max(le))
            events += [(tk, k, t, e) for tk, k, e in evs]
        events.sort(key=lambda e: (e[0], e[1]))

        chan = [self.first_valid] * 16
        if self.perc >= 0:
            chan[9] = self.perc
        waves = set()

        def run(evs):
            for _tk, _k, _t, e in evs:
                if e[0] == "midi" and (e[1] & 0xF0) == 0xC0:
                    prog = e[2]
                    if prog < self.n_insts and \
                            self.prog_rows[prog] is not None:
                        chan[e[1] & 0xF] = prog
                elif e[0] == "note_on":
                    w = self.lookup(chan[e[1]], e[2], e[3])
                    if w is not None:
                        waves.add(w)

        run(events)
        body = [e for e in events
                if e[2] in loops and loops[e[2]][0] <= e[0] < loops[e[2]][1]]
        for _ in range(8):
            before = len(waves), tuple(chan)
            run(body)
            if (len(waves), tuple(chan)) == before:
                break
        return waves

    def wave_bytes(self, w):
        return adpcm_bytes(self.waves[w]["len"])

    def index(self, name):
        return self.names.index(name)


# ---- the census ----------------------------------------------------------

def build(rom):
    check_enum(os.path.join(DECOMP_SRC, "ft", "ftdef.h"), "FTKind", FIGHTERS,
               "nFTKind")
    gr = check_enum(os.path.join(DECOMP_SRC, "gr", "grdef.h"), "GRKind",
                    STAGE_KINDS, "nGRKind")
    # src/dc/sndres.h computes SNDRES_STAGES as nGRKindCommonEnd + 1 and
    # sndres_init refuses a sndsets.bin whose group count does not match
    # SNDRES_GROUPS -- silently, from the game's point of view: "boot: no
    # sound sets; the game is silent" and nothing else. So the length of
    # STAGE_KINDS is not a thing to count by hand.
    if len(STAGE_KINDS) != gr["commonend"] + 1:
        raise SystemExit("gr/grdef.h: nGRKindCommonEnd is %d, so "
                         "SNDRES_STAGES is %d, but STAGE_KINDS has %d "
                         "slot(s)" % (gr["commonend"], gr["commonend"] + 1,
                                      len(STAGE_KINDS)))
    fgm = FgmBank(rom)
    mus = MusicBank(rom)

    group_ids = collections.defaultdict(set)
    unplaced = []
    excluded_ids = set()
    for path in sorted(glob.glob(os.path.join(DECOMP_SRC, "**", "*.c"),
                                 recursive=True)):
        # XBOX: "/" on every host -- classify() matches on "mv/", "it/" ...,
        # and a Windows relpath is spelt with backslashes
        rel = os.path.relpath(path, DECOMP_SRC).replace(os.sep, "/")
        names = set(ID_RE.findall(strip_comments(open(path,
                                                     errors="replace").read())))
        names.discard("nSYAudioFGMVoiceEnd")
        if not names:
            continue
        g = classify(rel)
        if g is None:
            unplaced.append(rel)
            continue
        for n in names:
            if n not in fgm.ids:
                raise SystemExit("%s names %s, which gmsound.h does not"
                                 % (rel, n))
            (excluded_ids if g == "excluded" else group_ids[g]).add(
                fgm.ids[n])

    # The port's own copies must not name an id the decomp's tier-A files
    # do not: that would be a sound the census never staged. Two files are
    # exempt because both deliberately reach past tier A on purpose rather
    # than by a stale census: db.c's DB_BOOT_* probes name IDs to boot
    # straight into any scene, and mnsoundtest.c is the Sound Test screen,
    # whose whole job is browsing nearly the entire corpus (mnsoundtest.h's
    # own DIVERGES note says why it still has no sndres group of its own).
    tier_a = set().union(*group_ids.values())
    stray = collections.defaultdict(set)
    for path in sorted(glob.glob(os.path.join(PORT_SRC, "*.c"))):
        if os.path.basename(path) in ("db.c", "mnsoundtest.c"):
            continue
        # The port's own scenes stage nothing of their own
        # (src/dc/sndres.c's nSCKindDCMemCard arm keeps what is resident),
        # so the memory card's page may name only what the menus stage.
        allowed = group_ids["menu"] \
            if os.path.basename(path) in PORT_SCENE_FILES else tier_a
        for n in ID_RE.findall(strip_comments(open(path).read())):
            if n != "nSYAudioFGMVoiceEnd" and fgm.ids[n] not in allowed:
                stray[os.path.basename(path)].add(n)

    def port_bgm(files):
        out = set()
        for f in files:
            text = strip_comments(open(os.path.join(PORT_SRC, f)).read())
            out |= {mus.index(n) for n in BGM_RE.findall(text)
                    if n in mus.names}
        return out

    import ssb_stageexport as STG
    stage_track = {s: STG.read_ground(rom, s)["bgm"]
                   for s in STAGE_KINDS if s}
    attract = {mus.index(n) for n in ATTRACT_TRACKS}
    group_tracks = {
        "menu": port_bgm(MENU_SCENE_FILES) - attract,
        "attract": attract,
        "results": port_bgm(RESULTS_SCENE_FILES),
        "core": port_bgm(BATTLE_MUSIC_FILES) - set(stage_track.values()),
        "credits": port_bgm(CREDITS_SCENE_FILES),
        "opening": port_bgm(OPENING_SCENE_FILES),
        "1pintro": port_bgm(INTRO_SCENE_FILES),
        # ...plus the VS card's 1PIntro, which the rung inherits: the
        # Metal Mario and Polygon Team rungs only arm their stage track
        # (mpCollisionSetBGM) and start it at GO, so the card's track
        # plays on through the countdown. Every other rung replaces it
        # in its FuncStart.
        "1pgame": (port_bgm(GAME_SCENE_FILES) - set(stage_track.values()))
                  | {mus.index("1PIntro")},
        "1pchallenger": port_bgm(CHALLENGER_SCENE_FILES),
        "1pstageclear": port_bgm(STAGECLEAR_SCENE_FILES),
        "1pcontinue": port_bgm(CONTINUE_SCENE_FILES),
        # The bonus stages. Their track is not named by
        # any port file: sc1PBonusStageFuncStart calls
        # mpCollisionSetPlayBGM, which plays the COURSE's own bgm_id --
        # a stage track, exactly like the nine in `stage_track` above,
        # except that the bonus kinds are past nGRKindCommonEnd and so
        # have no "stage:" group to ride in. So it is read the same way
        # and put here. All twenty-four courses -- twelve Break the
        # Targets and twelve Board the Platforms --
        # name the same one (nSYAudioBGM1PBonusStage); the union is taken
        # rather than assumed, because a course that ever disagreed would
        # otherwise go silent with nothing to say why.
        "1pbonus": {STG.read_ground(rom, g + f)["bgm"]
                    for g in ("Bonus1", "Bonus2") for f in FIGHTERS},
    }
    for i, s in enumerate(STAGE_KINDS):
        group_tracks["stage:" + (s or "?%d" % i)] = (
            {stage_track[s]} if s else set())
    track_waves = {}
    for tracks in group_tracks.values():
        for t in tracks:
            if t not in track_waves:
                track_waves[t] = mus.track_waves(t)

    sets = {}
    for g in GROUPS:
        samples = set()
        for vid in group_ids.get(g, ()):
            samples |= fgm.closure(vid)
        waves = set()
        for t in group_tracks.get(g, ()):
            waves |= track_waves[t]
        sets[g] = (samples, waves)

    return {"fgm": fgm, "mus": mus, "sets": sets, "group_ids": group_ids,
            "group_tracks": group_tracks, "unplaced": unplaced,
            "stray": stray, "excluded_ids": excluded_ids}


def set_bytes(c, groups):
    samples, waves = set(), set()
    for g in groups:
        samples |= c["sets"][g][0]
        waves |= c["sets"][g][1]
    fb = sum(c["fgm"].sample_bytes(s) for s in samples)
    bb = sum(c["mus"].wave_bytes(w) for w in waves)
    return fb, bb


def scene_worst(c):
    """Each scene's worst composition, the way src/dc/sndres.c composes
    them: (name, groups, fgm bytes, bgm bytes)."""
    fighters = ["fighter:" + f for f in FIGHTERS]
    rows = []
    rows.append(("menus", ["menu"]) + set_bytes(c, ["menu"]))
    g = ["menu"] + fighters
    rows.append(("character select", g) + set_bytes(c, g))
    # The attract loop's character showcase (src/dc/sndres.c's
    # nSCKindCharacters arm): "menu", the Explain track's own group, and
    # the two demo fighters the title picked -- any two, so a best-of-N.
    best = None
    for two in itertools.combinations(fighters, 2):
        g = ["menu", "attract"] + list(two)
        fb, bb = set_bytes(c, g)
        if best is None or fb + bb > best[2] + best[3]:
            best = ("character showcase", g, fb, bb)
    rows.append(best)
    best = None
    for four in itertools.combinations(fighters, 4):
        for s in STAGES:
            g = ["core", "stage:" + s] + list(four)
            fb, bb = set_bytes(c, g)
            if best is None or fb + bb > best[2] + best[3]:
                best = ("battle", g, fb, bb)
    rows.append(best)
    best = None
    for four in itertools.combinations(fighters, 4):
        g = ["menu", "results"] + list(four)
        fb, bb = set_bytes(c, g)
        if best is None or fb + bb > best[2] + best[3]:
            best = ("results", g, fb, bb)
    rows.append(best)
    # The ending diorama and the staff roll. nSCKindEnding
    # poses the run's own fighter (mvEndingInitVars), which is the
    # player's to choose, so it is a best-of-N over the twelve;
    # nSCKindStaffroll poses none, so the ending is the worse of the two.
    best = None
    for f in fighters:
        g = ["credits", f]
        fb, bb = set_bytes(c, g)
        if best is None or fb + bb > best[2] + best[3]:
            best = ("credits", g, fb, bb)
    rows.append(best)
    # The openings: "opening" alone. The two trophy
    # fighters the Room poses never make a sound -- their demo statuses
    # have no SubMotion in the pack and fall back to a silent Wait
    # (src/dc/mvopeningroom.h) -- so sndres_compose stages no fighter
    # group for them and there is nothing to take a worst case over.
    g = ["opening"]
    rows.append(("openings", g) + set_bytes(c, g))
    # XBOX: the title straight after the movie (src/dc/sndres.c's
    # nSCKindTitle arm) keeps the opening set beside the menu's, because
    # the movie's music and the announcer's "Super Smash Bros." play on
    # into it, as mntitle.c's mnTitleInitVars lets them on the N64.
    g = ["opening", "menu"]
    rows.append(("title after movie", g) + set_bytes(c, g))
    # The ladder's VS card (src/dc/sndres.c's nSCKind1PIntro
    # arm): its own two tracks and announcer lines, plus the fighters it
    # stands on screen -- at most four (the human, the rung's opponent
    # and, on the Giant DK rung, two allies). No stage, since the card
    # has none, and no "core": the card is not a battle, and that arm's
    # own comment records that this row is what decided it. A best-of-N
    # like battle and results above, for the same reason: which four is
    # the player's to decide.
    best = None
    for four in itertools.combinations(fighters, 4):
        g = ["1pintro"] + list(four)
        fb, bb = set_bytes(c, g)
        if best is None or fb + bb > best[2] + best[3]:
            best = ("1p intro card", g, fb, bb)
    rows.append(best)
    # A rung of the ladder (src/dc/sndres.c's nSCKind1PGame
    # arm): a battle, so "core" and a stage, plus this scene's own
    # group. Four fighters is the most a rung can stand at once (the
    # Mario Bros. rung: the human, Mario, Luigi and one ally), and which
    # ones is the player's to decide, so it is a best-of-N like battle
    # above. The stage is the rung's, not the player's -- the loop runs
    # over the exported stages because most of them are rungs' stages
    # too. Every rung now names its stage; the four 1P-only maps had
    # none until they were exported. Final Destination carries
    # nSYAudioBGMBossEntry on top of "core" and "1pgame".
    best = None
    for label, st, enemies in RUNGS:
        for human in FIGHTERS:
            g = ["core", "1pgame"] + (["stage:" + st] if st else [])
            g += ["fighter:" + f
                  for f in dict.fromkeys([human] + enemies)]
            fb, bb = set_bytes(c, g)
            if best is None or fb + bb > best[2] + best[3]:
                best = ("1p rung (%s)" % label, g, fb, bb)
    rows.append(best)
    # "CHALLENGER APPROACHING!" (src/dc/sndres.c's
    # nSCKind1PChallenger arm): its own group and the one fighter it
    # stands. The ladder only ever challenges with Luigi, Ness,
    # Jigglypuff or Captain Falcon, but src/dc/db.c's boot arm can name
    # any of the twelve, so the worst case is taken over all of them.
    best = None
    for f in fighters:
        g = ["1pchallenger", f]
        fb, bb = set_bytes(c, g)
        if best is None or fb + bb > best[2] + best[3]:
            best = ("1p challenger", g, fb, bb)
    rows.append(best)
    # The score screen (src/dc/sndres.c's nSCKind1PStageClear
    # arm): its own group alone, and nothing to take a worst case over --
    # the scene stands no fighter and plays on no stage.
    g = ["1pstageclear"]
    rows.append(("1p score", g) + set_bytes(c, g))
    # The Continue prompt (src/dc/sndres.c's
    # nSCKind1PContinue arm): its own group and the one fighter it
    # stands -- the human's, slumped in the spotlight. No stage: the
    # scene's room is three sprites. A best-of-N over all twelve, like
    # the challenger card above, because the human is the player's to
    # choose.
    best = None
    for f in fighters:
        g = ["1pcontinue", f]
        fb, bb = set_bytes(c, g)
        if best is None or fb + bb > best[2] + best[3]:
            best = ("1p continue", g, fb, bb)
    rows.append(best)
    # A bonus stage (src/dc/sndres.c's nSCKind1PBonusStage
    # arm): a battle with ONE fighter, so "core" and this scene's own
    # group and nothing else -- the twelve courses are past
    # STAGE_KINDS's end, so no "stage:" group is staged for them and
    # their BGM rides in "1pbonus" instead (see classify above). A
    # best-of-N over all twelve fighters, because the course IS the
    # fighter: which one is the player's to choose.
    best = None
    for f in fighters:
        g = ["core", "1pbonus", f]
        fb, bb = set_bytes(c, g)
        if best is None or fb + bb > best[2] + best[3]:
            best = ("1p bonus stage", g, fb, bb)
    rows.append(best)
    return rows


def tier_a_samples(rom):
    """Every bank-2 sample any group reaches: what fgm_sounds.pak holds."""
    c = build(rom)
    return sorted(set().union(*(s for s, _w in c["sets"].values())))


def serialize(c):
    head = struct.pack("<8sII", MAGIC, len(GROUPS), BUDGET)
    dir_size = len(GROUPS) * 8
    body = b""
    directory = b""
    for g in GROUPS:
        samples, waves = c["sets"][g]
        directory += struct.pack("<IHH", len(head) + dir_size + len(body),
                                 len(samples), len(waves))
        body += struct.pack("<%dH" % len(samples), *sorted(samples))
        body += struct.pack("<%dH" % len(waves), *sorted(waves))
    return head + directory + body


def main():
    argv = sys.argv[1:]
    rom_path = M.ROM_DEFAULT
    if "--rom" in argv:
        rom_path = argv[argv.index("--rom") + 1]
    out = argv[argv.index("--out") + 1] if "--out" in argv else None
    check = "--check" in argv
    rom = open(rom_path, "rb").read()
    c = build(rom)

    kb = lambda b: b / 1024.0
    print("%-8s %6s %6s %9s %9s" % ("group", "fgm", "waves", "fgm KB",
                                   "music KB"))
    for g in GROUPS:
        fb, bb = set_bytes(c, [g])
        print("%-16s %4d %6d %9.0f %9.0f" % (g, len(c["sets"][g][0]),
                                             len(c["sets"][g][1]), kb(fb),
                                             kb(bb)))
    all_samples = set().union(*(s for s, _w in c["sets"].values()))
    print("tier A: %d FGM ids -> %d of %d bank-2 samples"
          % (len(set().union(*c["group_ids"].values())), len(all_samples),
             len(c["fgm"].sounds)))
    failed = False
    print("scene worst cases against %d bytes (%d of sound RAM less a "
          "%d KB margin):" % (BUDGET, SOUND_RAM_BYTES, BUDGET_MARGIN // 1024))
    for name, groups, fb, bb in scene_worst(c):
        over = fb + bb > BUDGET
        failed |= over
        detail = ", ".join(g.split(":")[-1] for g in groups
                           if g.startswith(("fighter:", "stage:")))
        print("  %-17s %6.0f KB fgm + %5.0f KB music = %6.0f KB, %5.0f KB "
              "spare%s%s" % (name, kb(fb), kb(bb), kb(fb + bb),
                             kb(BUDGET - fb - bb),
                             " (%s)" % detail if detail else "",
                             "  OVER BUDGET" if over else ""))
    if c["unplaced"]:
        failed = True
        print("unplaced decomp files naming FGM ids (classify() needs a "
              "rule):\n  " + "\n  ".join(c["unplaced"]))
    if c["stray"]:
        failed = True
        for f, names in sorted(c["stray"].items()):
            print("%s names FGM ids no %s stages: %s"
                  % (f, "menu group" if f in PORT_SCENE_FILES
                     else "tier-A group", ", ".join(sorted(names))))

    if out:
        d = os.path.dirname(out)
        if d:
            os.makedirs(d, exist_ok=True)
        with open(out, "wb") as fp:
            fp.write(serialize(c))
        print("%s: %d groups" % (out, len(GROUPS)))
    if failed:
        sys.exit("ssb_sndsets: the sound sets do not hold")
    if check:
        print("ssb_sndsets: every tier-A FGM id is in a group, and every "
              "scene fits sound RAM")


if __name__ == "__main__":
    main()

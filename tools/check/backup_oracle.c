/* backup_oracle.c -- what the port compiled for sc/scmanager.c's three
 * .data tables, as a field list tools/check/backup_check.py can hold against
 * the ROM's own bytes.
 *
 * The check this feeds is the whole reason src/dc/scmanagerdata.c is its
 * own file: those 660 lines are a hand copy of an initializer, and a hand
 * copy of an initializer is exactly the kind of thing that is wrong in one
 * place and nowhere else. The ROM has the answer -- dSCManagerDefaultBackupData
 * is at 0x800A3994 in the always-resident segment -- so the port's copy can
 * be held against it field for field.
 *
 * Output is one line per scalar leaf:
 *
 *     F <struct> <field> <offset> <size> <is_signed> <value>
 *     S <struct> <sizeof>
 *     B <struct> <field> <value>          (a one-bit field; see below)
 *
 * The offsets are the port's own, from offsetof. That is deliberate: if
 * the SH-4 laid a field out somewhere the MIPS did not, the check would
 * read the ROM at the wrong place and the values would stop matching,
 * which is the failure it is looking for.
 *
 * One correction, and one only: this runs on the machine that builds the
 * port, where a pointer is eight bytes, and both machines it is talking
 * about have four. SCPlayerData holds one -- GObj *fighter_gobj -- so
 * every field of a player at or after it, the stride between players and
 * the size of the whole battle state are reported four bytes per pointer
 * lower than offsetof gives here. PTRFIX is zero on a 32-bit host, so
 * this is a no-op wherever it does not apply, and the correction is not
 * taken on trust: with it, the battle state has to come out at the 496
 * bytes the ROM's own symbol spans, and 466 fields have to land on the
 * ROM's values. LBBackupData and SCCommonData hold no pointer at all and
 * take no correction.
 *
 * The two ub32 : 1 fields of SCBattleState are emitted as values without
 * an offset, because offsetof cannot name a bit field and because where a
 * one-bit field sits inside its word is the two ABIs' business rather than
 * this table's -- big-endian MIPS allocates from the top of the word,
 * little-endian SH-4 from the bottom. backup_check.py checks the ROM's
 * container word against the MIPS rule and the values against these.
 */
#include <stdio.h>
#include <stddef.h>
#include <string.h>

#include <stdlib.h>

#include <sc/scene.h>
#include <lb/lbbackup.h>

/* lb/lbbackup.c is linked in for one function -- lbBackupCreateChecksum,
 * whose loop bound is sizeof(LBBackupData) - sizeof(...checksum) and is
 * the subtle line in the file. These are what the rest of that file needs
 * to link, and nothing here calls anything that touches them. */
LBBackupData gSCManagerBackupData;
SCCommonData gSCManagerSceneData;
SCBattleState gSCManagerTransferBattleState;

void syDmaReadSram(uintptr_t a, void *b, size_t c) { (void)a, (void)b, (void)c; }
void syDmaWriteSram(void *a, uintptr_t b, size_t c) { (void)a, (void)b, (void)c; }
void syAudioSetQuality(s32 q) { (void)q; }
void syVideoSetCenterOffsets(s16 l, s16 r, s16 t, s16 b)
{ (void)l, (void)r, (void)t, (void)b; }

/* The host's pointer, less the four bytes the N64 and the Dreamcast
 * both spend on one. See the note at the top. */
#define PTRFIX (sizeof(GObj *) - 4)

static const char *sStruct;

static void F(const char *name, size_t off, size_t size, int is_signed,
              long long val)
{
    printf("F %s %s %u %u %d %lld\n", sStruct, name, (unsigned)off,
           (unsigned)size, is_signed, val);
}

static void S(size_t size)
{
    printf("S %s %u\n", sStruct, (unsigned)size);
}

/* ---- LBBackupData (lb/lbtypes.h:267-297) ---------------------------- */

#define VS_OFF(f) (base + offsetof(LBBackupVSRecord, f))

static void emit_vsrecord(size_t base, const LBBackupVSRecord *r, int i)
{
    char n[64];
    int k;

    for (k = 0; k < GMCOMMON_FIGHTERS_PLAYABLE_NUM; k++)
    {
        sprintf(n, "vs_records[%d].ko_count[%d]", i, k);
        F(n, VS_OFF(ko_count) + 2 * k, 2, 0, r->ko_count[k]);
    }
    sprintf(n, "vs_records[%d].time_used", i);
    F(n, VS_OFF(time_used), 4, 0, r->time_used);
    sprintf(n, "vs_records[%d].damage_given", i);
    F(n, VS_OFF(damage_given), 4, 0, r->damage_given);
    sprintf(n, "vs_records[%d].damage_taken", i);
    F(n, VS_OFF(damage_taken), 4, 0, r->damage_taken);
    sprintf(n, "vs_records[%d].unk", i);
    F(n, VS_OFF(unk), 2, 0, r->unk);
    sprintf(n, "vs_records[%d].selfdestructs", i);
    F(n, VS_OFF(selfdestructs), 2, 0, r->selfdestructs);
    sprintf(n, "vs_records[%d].games_played", i);
    F(n, VS_OFF(games_played), 2, 0, r->games_played);
    sprintf(n, "vs_records[%d].player_count_tally", i);
    F(n, VS_OFF(player_count_tally), 2, 0, r->player_count_tally);
    for (k = 0; k < GMCOMMON_FIGHTERS_PLAYABLE_NUM; k++)
    {
        sprintf(n, "vs_records[%d].player_count_tallies[%d]", i, k);
        F(n, VS_OFF(player_count_tallies) + 2 * k, 2, 0,
          r->player_count_tallies[k]);
    }
    for (k = 0; k < GMCOMMON_FIGHTERS_PLAYABLE_NUM; k++)
    {
        sprintf(n, "vs_records[%d].played_against[%d]", i, k);
        F(n, VS_OFF(played_against) + 2 * k, 2, 0, r->played_against[k]);
    }
}

#define SP_OFF(f) (base + offsetof(LBBackup1PRecord, f))

static void emit_1precord(size_t base, const LBBackup1PRecord *r, int i)
{
    char n[64];

    sprintf(n, "spgame_records[%d].spgame_hiscore", i);
    F(n, SP_OFF(spgame_hiscore), 4, 0, r->spgame_hiscore);
    sprintf(n, "spgame_records[%d].spgame_continues", i);
    F(n, SP_OFF(spgame_continues), 4, 0, r->spgame_continues);
    sprintf(n, "spgame_records[%d].spgame_total_bonuses", i);
    F(n, SP_OFF(spgame_total_bonuses), 4, 0, r->spgame_total_bonuses);
    sprintf(n, "spgame_records[%d].spgame_best_difficulty", i);
    F(n, SP_OFF(spgame_best_difficulty), 1, 0, r->spgame_best_difficulty);
    sprintf(n, "spgame_records[%d].bonus1_time", i);
    F(n, SP_OFF(bonus1_time), 4, 0, r->bonus1_time);
    sprintf(n, "spgame_records[%d].bonus1_task_count", i);
    F(n, SP_OFF(bonus1_task_count), 1, 0, r->bonus1_task_count);
    sprintf(n, "spgame_records[%d].bonus2_time", i);
    F(n, SP_OFF(bonus2_time), 4, 0, r->bonus2_time);
    sprintf(n, "spgame_records[%d].bonus2_task_count", i);
    F(n, SP_OFF(bonus2_task_count), 1, 0, r->bonus2_task_count);
    sprintf(n, "spgame_records[%d].is_spgame_complete", i);
    F(n, SP_OFF(is_spgame_complete), 1, 0, r->is_spgame_complete);
}

#define BK(f, sz, sg) F(#f, offsetof(LBBackupData, f), sz, sg, b->f)

static void emit_backup(const LBBackupData *b)
{
    int i;

    sStruct = "dSCManagerDefaultBackupData";
    for (i = 0; i < GMCOMMON_FIGHTERS_PLAYABLE_NUM; i++)
    {
        emit_vsrecord(offsetof(LBBackupData, vs_records) +
                      i * sizeof(LBBackupVSRecord), &b->vs_records[i], i);
    }
    BK(is_allow_screenflash, 1, 0);
    BK(sound_mono_or_stereo, 1, 0);
    BK(screen_adjust_h, 2, 1);
    BK(screen_adjust_v, 2, 1);
    BK(characters_fkind, 1, 0);
    BK(unlock_mask, 1, 0);
    BK(fighter_mask, 2, 0);
    BK(spgame_difficulty, 1, 0);
    BK(spgame_stock_count, 1, 0);
    for (i = 0; i < GMCOMMON_FIGHTERS_PLAYABLE_NUM; i++)
    {
        emit_1precord(offsetof(LBBackupData, spgame_records) +
                      i * sizeof(LBBackup1PRecord), &b->spgame_records[i], i);
    }
    BK(ground_mask, 2, 0);
    BK(vs_itemswitch_battles, 1, 0);
    BK(vs_total_battles, 2, 0);
    BK(error_flags, 1, 0);
    BK(boot, 1, 0);
    BK(signature, 2, 0);
    BK(checksum, 4, 1);
    S(sizeof(LBBackupData));
}

/* ---- SCCommonData (sc/sctypes.h:378-414) ---------------------------- */

#define CD(f, sz, sg) F(#f, offsetof(SCCommonData, f), sz, sg, c->f)

static void emit_scene(const SCCommonData *c)
{
    char n[64];
    int i;

    sStruct = "dSCManagerDefaultSceneData";
    CD(scene_curr, 1, 0);
    CD(scene_prev, 1, 0);
    for (i = 0; i < nLBBackupUnlockEnumCount; i++)
    {
        sprintf(n, "unlock_messages[%d]", i);
        F(n, offsetof(SCCommonData, unlock_messages) + i, 1, 0,
          c->unlock_messages[i]);
    }
    CD(challenger_fkind, 1, 0);
    CD(demo_mask_prev, 2, 0);
    CD(demo_first_fkind, 1, 0);
    for (i = 0; i < 2; i++)
    {
        sprintf(n, "demo_fkind[%d]", i);
        F(n, offsetof(SCCommonData, demo_fkind) + i, 1, 0, c->demo_fkind[i]);
    }
    CD(gkind, 1, 0);
    CD(is_suddendeath, 1, 0);
    CD(is_continue, 1, 0);
    CD(is_reset, 1, 0);
    CD(player, 1, 0);
    CD(fkind, 1, 0);
    CD(costume, 1, 0);
    CD(spgame_time_limit, 1, 0);
    CD(spgame_stage, 1, 0);
    for (i = 0; i < 2; i++)
    {
        sprintf(n, "ally_players[%d]", i);
        F(n, offsetof(SCCommonData, ally_players) + i, 1, 0,
          c->ally_players[i]);
    }
    CD(spgame_time_remain, 4, 0);
    CD(spgame_score, 4, 0);
    CD(continues_used, 4, 0);
    CD(bonus_count, 4, 0);
    for (i = 0; i < 3; i++)
    {
        sprintf(n, "bonus_get_mask[%d]", i);
        F(n, offsetof(SCCommonData, bonus_get_mask) + 4 * i, 4, 0,
          c->bonus_get_mask[i]);
    }
    CD(bonus_tasks_complete, 1, 0);
    CD(bonus_fkind, 1, 0);
    CD(bonus_costume, 1, 0);
    CD(training_man_fkind, 1, 0);
    CD(training_man_costume, 1, 0);
    CD(training_com_fkind, 1, 0);
    CD(training_com_costume, 1, 0);
    CD(is_extend_demo_wait, 1, 0);
    CD(demo_gkind_order, 1, 0);
    CD(maps_vsmode_gkind, 1, 0);
    CD(maps_training_gkind, 1, 0);
    CD(challenger_level_drop, 1, 0);
    CD(is_title_anim_viewed, 1, 0);
    S(sizeof(SCCommonData));
}

/* ---- SCBattleState (sc/sctypes.h:355-376) --------------------------- */

#define PL_OFF(f) (base + offsetof(SCPlayerData, f))

static void emit_player(size_t base, const SCPlayerData *p, int i)
{
    char n[64];
    int k;

#define PL(f, sz, sg)                               \
    do {                                            \
        sprintf(n, "players[%d]." #f, i);           \
        F(n, PL_OFF(f), sz, sg, p->f);              \
    } while (0)

    PL(level, 1, 0);
    PL(handicap, 1, 0);
    PL(pkind, 1, 0);
    PL(fkind, 1, 0);
    PL(team, 1, 0);
    PL(player, 1, 0);
    PL(costume, 1, 0);
    PL(shade, 1, 0);
    PL(color, 1, 0);
    PL(is_single_stockicon, 1, 0);
    PL(tag, 1, 0);
    PL(stock_count, 1, 1);
    PL(is_spgame_enemy, 1, 0);
    PL(place, 1, 0);
    PL(falls, 4, 1);
    PL(score, 4, 1);
    for (k = 0; k < GMCOMMON_PLAYERS_MAX; k++)
    {
        sprintf(n, "players[%d].total_kos_players[%d]", i, k);
        F(n, PL_OFF(total_kos_players) + 4 * k, 4, 1, p->total_kos_players[k]);
    }
    PL(unk_pblock_0x28, 4, 1);
    PL(unk_pblock_0x2C, 4, 1);
    PL(total_selfdestructs, 4, 1);
    PL(total_damage_given, 4, 1);
    PL(total_damage_all, 4, 1);
    for (k = 0; k < GMCOMMON_PLAYERS_MAX; k++)
    {
        sprintf(n, "players[%d].total_damage_players[%d]", i, k);
        F(n, PL_OFF(total_damage_players) + 4 * k, 4, 1,
          p->total_damage_players[k]);
    }
    PL(stock_damage_all, 4, 1);
    PL(combo_damage_foe, 4, 1);
    PL(combo_count_foe, 4, 1);
    /* the GObj pointer, and everything the host puts after it: four
     * bytes on both machines that matter, and NULL in every default */
    sprintf(n, "players[%d].fighter_gobj", i);
    F(n, PL_OFF(fighter_gobj), 4, 0, (long long)(size_t)p->fighter_gobj);
    sprintf(n, "players[%d].stale_id", i);
    F(n, PL_OFF(stale_id) - PTRFIX, 4, 0, p->stale_id);
    for (k = 0; k < 5; k++)
    {
        sprintf(n, "players[%d].stale_info[%d].attack_id", i, k);
        F(n, PL_OFF(stale_info) - PTRFIX + k * 4, 2, 0,
          p->stale_info[k].attack_id);
        sprintf(n, "players[%d].stale_info[%d].motion_count", i, k);
        F(n, PL_OFF(stale_info) - PTRFIX + k * 4 + 2, 2, 0,
          p->stale_info[k].motion_count);
    }
#undef PL
}

#define BS(f, sz, sg) F(#f, offsetof(SCBattleState, f), sz, sg, s->f)

static void emit_battle(const SCBattleState *s)
{
    int i;

    sStruct = "dSCManagerDefaultBattleState";
    BS(game_type, 1, 0);
    BS(gkind, 1, 0);
    BS(is_team_battle, 1, 0);
    BS(game_rules, 1, 0);
    BS(pl_count, 1, 0);
    BS(cp_count, 1, 0);
    BS(time_limit, 1, 0);
    BS(stocks, 1, 0);
    BS(handicap, 1, 0);
    BS(is_team_attack, 1, 0);
    BS(is_stage_select, 1, 0);
    BS(damage_ratio, 1, 0);
    BS(item_toggles, 4, 0);
    BS(is_reset_players, 1, 0);
    BS(game_status, 1, 0);
    BS(time_remain, 4, 0);
    BS(time_passed, 4, 0);
    BS(item_appearance_rate, 1, 0);
    printf("B %s is_show_score %d\n", sStruct, (int)s->is_show_score);
    printf("B %s is_not_teamshadows %d\n", sStruct, (int)s->is_not_teamshadows);
    for (i = 0; i < GMCOMMON_PLAYERS_MAX; i++)
    {
        emit_player(offsetof(SCBattleState, players) +
                    i * (sizeof(SCPlayerData) - PTRFIX), &s->players[i], i);
    }
    S(sizeof(SCBattleState) - GMCOMMON_PLAYERS_MAX * PTRFIX);
}

/* Each argv is a file of raw LBBackupData bytes -- the ROM's own copy of
 * the defaults, as backup_check.py cut them out, and then the same bytes
 * with the checksum field filled in the way lbBackupWrite would leave it.
 * Running the game's checksum over them says what the loop really sums: a
 * byte-wise weighted sum has no byte order of its own, so the port's
 * answer over the N64's bytes is a number the check can compute for
 * itself. The second file is why there are two -- the defaults carry a
 * checksum of zero, so a loop that ran four bytes too far would sum four
 * zeroes and give the same answer, and only a filled-in checksum makes
 * the loop's bound visible at all. */
int main(int argc, char **argv)
{
    emit_backup(&dSCManagerDefaultBackupData);
    emit_scene(&dSCManagerDefaultSceneData);
    emit_battle(&dSCManagerDefaultBattleState);

    {
        int a;

        for (a = 1; a < argc; a++)
        {
            LBBackupData rom;
            FILE *f = fopen(argv[a], "rb");

            if (f == NULL || fread(&rom, 1, sizeof(rom), f) != sizeof(rom))
            {
                fprintf(stderr, "backup_oracle: cannot read %s\n", argv[a]);
                return 1;
            }
            fclose(f);
            printf("C %d\n", (int)lbBackupCreateChecksum(&rom));
        }
    }
    return 0;
}

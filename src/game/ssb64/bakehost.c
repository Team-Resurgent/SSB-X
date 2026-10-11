/* bakehost.c -- the in-build baker's driver, included by
 * hosttest_ft.c under BAKER=1 in place of the tests.
 *
 *   bake_host <stage> <fkind> <costume> <hat> <tic> <out.txt>
 *
 * Boots the 1P VS card for ladder rung <stage> the way db.c's
 * -DDB_BOOT_SCENE=nSCKind1PIntro does, runs <tic> updates, then draws one
 * frame's three lists with every pvr_prim kept, and writes the stream in
 * the lines src/dc/primdump.h describes. tools/lib/pvrsoft.py draws it.
 * Run from src/game/ssb64 (the romdisk is read from there). <hat> < 0 leaves
 * the Kirby team's copy hat as the game rolled it.
 */
#include "sc1pintro.h"
#include "sc1pmanager.h"
#include "primdump.h"

int main(int argc, char **argv)
{
    static const int lists[3] = { PVR_LIST_OP_POLY, PVR_LIST_PT_POLY,
                                  PVR_LIST_TR_POLY };
    int stage, fkind, costume, hat, tic, i, pass;

    if (argc != 7)
    {
        fprintf(stderr, "usage: bake_host stage fkind costume hat tic out\n");
        return 2;
    }
    stage = atoi(argv[1]);
    fkind = atoi(argv[2]);
    costume = atoi(argv[3]);
    hat = atoi(argv[4]);
    tic = atoi(argv[5]);

    gSCManagerSceneData.scene_curr = nSCKind1PIntro;
    gSCManagerSceneData.scene_prev = nSCKind1PGamePlayers;
    gSCManagerSceneData.spgame_stage = stage;
    gSCManagerSceneData.player = 0;
    gSCManagerSceneData.fkind = fkind;
    gSCManagerSceneData.costume = costume;
    gSCManagerSceneData.ally_players[0] = 1;
    gSCManagerSceneData.ally_players[1] = 2;
    gSCManager1PGameBattleState.players[0].fkind = fkind;
    gSCManager1PGameBattleState.players[0].costume = costume;
    gSCManager1PGameBattleState.players[0].shade = 0;
    gSCManager1PGameBattleState.players[0].pkind = nFTPlayerKindMan;
    gSCManager1PGameBattleState.players[0].handicap = FTCOMMON_HANDICAP_DEFAULT;
    gSCManager1PGameBattleState.players[0].team = 0;
    gSCManager1PGameBattleState.players[0].color = 0;
    gSCManager1PGameBattleState.players[0].tag = 0;
    gSCManager1PGameBattleState.players[0].is_spgame_enemy = FALSE;

    if (hat >= 0)
    {
        gSC1PManagerKirbyTeamModelPartID = (u8)hat;
    }
    if (syTaskmanMakeGeneralHeap(4 * 1024 * 1024) < 0)
    {
        return 2;
    }
    syTaskmanSetupPools(&dSC1PIntroTaskmanSetup);
    sc1PIntroFuncStart();
    /* the crowd is on DL link 32 (sc1PIntroMakeVSFighter's last argument);
     * the human's own fighter and the allies stand on 29-31 and are not
     * part of the picture */
    {
        GObj *g;

        for (g = gGCCommonLinks[nGCCommonLinkIDFighter]; g != NULL; g = g->link_next)
        {
            if (g->dl_link_id != 32)
            {
                g->flags |= GOBJ_FLAG_HIDDEN;
            }
        }
    }
    for (i = 0; i < tic; i++)
    {
        gcRunAll();
    }
    gPrimDumpOut = fopen(argv[6], "w");
    if (gPrimDumpOut == NULL)
    {
        perror(argv[6]);
        return 2;
    }
    primdump_start();
    for (pass = 0; pass < 3; pass++)
    {
        gcSetDrawList(lists[pass]);
        gcDrawAll();
    }
    primdump_finish(tic);
    fclose(gPrimDumpOut);
    return 0;
}

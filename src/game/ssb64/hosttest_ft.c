/* hosttest_ft.c -- headless cross-test of the port against the
 * decompiled N64 code (use-ssb-decomp-re: verify the
 * Dreamcast code behaves identically to the N64 logic).
 *
 * The collision itself needs no oracle: mp/mpcollision.c and
 * mp/mpprocess.c are compiled into this binary straight out of
 * $SSB_DECOMP_DIR, unmodified (see src/dc/decomp/README.md), so there
 * is nothing to diverge. What is tested here is everything around them.
 *
 * It began as three layers, which hosttest/physics.c still holds:
 *  1. Reference physics copied VERBATIM from ssb-decomp-re's
 *     ft/ftphysics.c and ft/ftcommon/ftcommonjump.c (only FTStruct
 *     field spelling maps onto a bare struct), fuzzed against the port
 *     over the full input range -- any float divergence fails.
 *  2. Scenario tests through the fighter's two GObjProcesses on
 *     Mario's real attribute values, read off the ROM by the pack
 *     exporter and restated here: walk tiers, stick and button jumps,
 *     the short-hop window, aerial jump budget, fast-fall landing
 *     weight.
 *  3. Collision scenarios on a hand-built stage in the game's own table
 *     layout (MPGeometryData, mp/mptypes.h:71-80): standing, walking
 *     off an edge into the teeter, dropping through a pass-through
 *     platform, landing, riding a slope, bonking a ceiling, grabbing a
 *     ledge and climbing or dropping off it. These assert the port's
 *     glue -- what it feeds the collision walk and what it does with
 *     the answer.
 *
 * One translation unit in seventeen files: this one holds the includes
 * and main(), and includes the parts in hosttest/ in the order below,
 * each part seeing everything the parts before it define.
 * hosttest/common.c is the scaffolding (spawn, frame, the CHECK macros,
 * the hand-built stage); the rest are the tests, grouped by subsystem.
 *
 * Build: see the Makefile's `hosttest` target.
 */
#include <assert.h>
#include "overlay.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <stddef.h>

#include "ftcommon.h"
#include "ftshadow.h"
#include "ftdisplaymain.h"
#include "vmusave.h"
#include "vmucard.h"
#include "dctext.h"
#include "vmunotice.h"
#include "dcmemcard.h"
#include "scport.h"
#include "dcstrings.h"
#include "gmcamera.h"
#include <gm/gmcollision.h>
#include <gm/generic.h>
#include <gr/ground.h>            /* GRStruct gGRCommonStruct */
#include <ft/ftcommon.h>          /* FTCOMMON_TORNADO_* */
#include "ifcommon.h"
#include "fgm.h"             /* the boss defeat's sound count */
#include "scmanager.h"
#include "scvsbattle.h"
#include "scvsresults.h"
#include "scautodemo.h"
#include "scexplain.h"
#include "mvending.h"
#include "mvopeningroom.h"
#include "mvopeningportraits.h"
#include "mvopeningmario.h"
#include "mvopeningdonkey.h"
#include "mvopeningsamus.h"
#include "mvopeninglink.h"
#include "mvopeningyoshi.h"
#include "mvopeningkirby.h"
#include "mvopeningfox.h"
#include "mvopeningpikachu.h"
#include "mvopeningrun.h"
#include "mvopeningcliff.h"
#include "mvopeningyamabuki.h"
#include "mvopeningjungle.h"
#include "mvopeningyoster.h"
#include "mvopeningsector.h"
#include "mvopeningstandoff.h"
#include "mvopeningclash.h"
#include "mvopeningnewcomers.h"
#include "camanim.h"              /* the camera-anim banks */
#include "scstaffroll.h"
#include "lbcommon.h"
#include "lbparticle.h"           /* LBScript/LBTexture */
#include "grwallpaper.h"          /* stage_wallpaper_place */
#include <lb/library.h>           /* G_IM_FMT_CI, as src/dc/lbparticle.c has it */
#include <lb/lbbackup.h>
#include "syaudio.h"
#include <sys/audio.h>
#include <sys/interp.h>
#include <sys/objdef.h>          /* aobjEvent32End, AOBJ_FLAG_ROTZ */
#include <gm/gmsound.h>
#include "mntitle.h"
#include "mnmodeselect.h"
#include "mnvsmode.h"
#include "mnplayersvs.h"
#include "mnmaps.h"
#include "mnmessage.h"
#include "mnvsitemswitch.h"
#include "mndata.h"
#include "mnvsrecord.h"
#include "mncharacters.h"
#include "mnsoundtest.h"
#include "mnoption.h"
#include "mnscreenadjust.h"
#include "mnbackupclear.h"
#include "mncongra.h"
#include "mnnocontroller.h"
#include "mnstartup.h"
#include "mn1pmode.h"
#include "mnplayers1ptraining.h"
#include "sc1ptrainingmode.h"
#include "mpcommon.h"
#include <wp/weapon.h>
#include <wp/wpmanager.h>
#include <it/item.h>
#include "clip.h"
#include "dma.h"
#include "lbparticle.h"
#include "sndres.h"
#include "lbpartex.h"
#include "lbpdraw.h"
#include "assetroot.h"
#include "objmodel.h"
#include "objpvr.h"
#include "sprite.h"
#include "taskman.h"

/* gr/grcommon/grjungle.h, for the barrel cannon's own eleven functions.
 * The decomp's header would do; this port's shim is the
 * one with grJungleTaruCannAddAnimOffset's changed signature. Without
 * it every one of them is an implicit `int` declaration, which is exactly
 * the trap the twister's own test avoided by declaring the two functions
 * it needed by hand here. */
#include <gr/grcommon/grjungle.h>
#include <gr/grcommon/grhyrule.h>
#include <gr/grcommon/grzebes.h>
#include <gr/grcommon/gryoster.h>
#include <gr/grcommon/grpupupu.h>
#include <gr/grcommon/grinishie.h>
#include <gr/grcommon/grsector.h>
#include <gr/grcommon/gryamabuki.h>
#include <gr/grvars.h>              /* GRPUPUPU_WHISPY_* */

/* src/dc/itempack.h: the item pack (ITCD). The port's own header, not the
 * decomp's -- the pack is the port's format -- and without it every one
 * of its functions is an implicit `int` declaration, the trap the gr/
 * includes above are here to avoid. */
#include "itempack.h"
#include "wpattrs.h"           /* the fighters' weapon tables, test_wp_attrs */
#include "itemoffsets.h"          /* llITCommonData* offsets */
#include "itemmodel.h"             /* the baked item models */
#include "itmonster.h"             /* itGetMonsterAnimNode */

#include <sys/utils.h>
#include <sc/sc1pmode/sc1pgame.h> /* the 1P bonus counters, test_1pgame_bonus_stats */
/* sys/utils.c:143, which sys/utils.h does not declare */
void syUtilsSetRandomSeed(s32 seed);
/* src/dc/grcastle.c: the bumper carriage's update, which the decomp's
 * grcastle.h does not declare */
void grCastleBumperProcUpdate(GObj *ground_gobj);
/* src/dc/wpmanager.c: the port's own question, no header of the
 * decomp's declares it */
sb32 wpManagerIsModelLess(WPAttributes *attr);

/* the game's own thresholds (ssb-decomp-re/src/ft/ftcommon.h) */
#include <ft/ftcommon.h>
#include <sys/rdp.h>            /* syRdpSetViewport, for test_screen_flash */
#include <ft/ftpublic.h>
#include <ft/ftparam.h>
#include <if/ifcommon.h>
#include <if/interface.h>
#include <lb/lbfade.h>
#include <ef/efdisplay.h>
#include <ef/efparticle.h>
#include <ef/effect.h>          /* ef/efground.h's twelve */
#include "efground.h"            /* EFGroundActorAsset, Part B Phase 1 */
#include <sc/scsubsys/scsubsys.h>
#include <mn/mntypes.h>

/* lb/lbfade.c's timeline, file-scope in the decomp and in src/dc/lbfade.c */
extern s32 sLBFadeAlphaMax;
extern s32 sLBFadeAlphaCurrent;
extern s32 sLBFadeLength;
extern SYColorRGBA sLBFadeColor;

/* the port's test hooks (ftcommon.c, FT_HOSTTEST) */
void ftTestSetGroundVelFriction(FTStruct *fp, float friction);
void ftTestSetGroundVelAbsStickRange(FTStruct *fp, float vel, float fric);
void ftTestApplyGravityClampTVel(FTStruct *fp, float gravity, float tvel);
int ftTestCheckClampAirVelXDec(FTStruct *fp, float clamp);
void ftTestClampAirVelXStickRange(FTStruct *fp, int m, float v, float c);
void ftTestApplyAirVelXFriction(FTStruct *fp);
void ftTestJumpGetJumpForceButton(int sx, s32 *vx, s32 *vy, int hop);

#define FT_JUMP_BUTTONS_TEST (N64_C_UP | N64_C_DOWN | N64_C_LEFT | \
                              N64_C_RIGHT)

#include "hosttest/common.c"
#ifdef FT_BAKER
/* the in-build baker is this link with the tests left out and a
 * main of its own */
#include "bakehost.c"
#else
#include "hosttest/physics.c"
#include "hosttest/fighters.c"
#include "hosttest/items.c"
#include "hosttest/stages.c"
#include "hosttest/itemcore.c"
#include "hosttest/weapons.c"
#include "hosttest/scripts.c"
#include "hosttest/presentation.c"
#include "hosttest/combat.c"
#include "hosttest/match.c"
#include "hosttest/roster.c"
#include "hosttest/computer.c"
#include "hosttest/frontend.c"
#include "hosttest/openings.c"
#include "hosttest/save.c"
#include "hosttest/text.c"
#include "hosttest/memcard.c"
#include "hosttest/effects.c"

int main(void)
{
    int pass;

    /* The save store's pump at the end of every tic, as the boot installs
     * it (src/game/ssb64/main.c): test_title_screen's scene runs the
     * real frame loop and reads the card back after it. */
    syTaskmanSetFrameEndHook(sy_sram_pump);

    /* First of all, because it asks what the store holds before anything
     * has written to it. */
    test_save_data();
    /* and then what is under the store, which needs the same clean slate */
    test_save_card();
    test_save_cards();
    test_save_writer();
    test_dctext();
    test_vmunotice();

    /* Then, while the host voice pool is untouched: every scene below
     * plays sounds and the double never reaps them. */
    test_sound_players();
    test_crowd_reacts();

    /* Before load_stage(), which remakes the heap this one fills. */
    test_particle_bank();
    test_particle_texpack();
    test_particle_draw();

    ftManagerSetupAttributes(&kMarioAttr, &kMario);
    test_attr_joint_tables();
    test_pack_attr_item_half();
    test_pack_attr_voice_half();
    test_sndres_scene_sets();

    load_stage();

    /* gm/gmrumble.c's own link-chain setup, the same thing
     * scVSBattleStartBattle's own two real call sites do before a real
     * battle's first frame (src/dc/scvsbattle.c) -- needed here too,
     * once, for the whole rest of this file's run: ftParamMakeRumble
     * (already real, already called from all over the ported fighter
     * damage/death/capture/thrown/item-shoot code below) reaches
     * gmRumbleSetPlayerRumbleParams -> gmRumbleGetEventPriorityRelink,
     * which dereferences sGMRumblePlayers[player].rlink unconditionally
     * -- NULL, and a crash, without this. */
    gmRumbleMakeActor();

    fuzz_physics();

    test_walk_tiers();
    test_button_jump_long_and_short();
    test_stick_jump_and_aerial();
    test_land_light_and_heavy();
    test_turn();
    test_script_attack_coll();
    test_script_loop();
    test_standin_rows();
    test_pass_through();
    test_script_waits();

    test_spawn_settles_on_floor();
    test_walkoff_teeters();
    test_walkoff_without_cliffedge_falls();
    test_dropthrough();
    test_jump_through_platform();
    test_slope();
    test_ceiling_bonk();
    test_wall_blocks();
    test_ledge_grab_and_climb();
    test_ledge_shared();
    test_ledge_climb_root_motion();
    test_ledge_roll();
    test_ledge_climb_slow();
    test_ledge_attack();
    test_shield_rebound();
    test_downb_tornado();
    test_downb_dispatch();
    test_v18_effect_makers();
    test_billboard_scale_fold();
    test_attacks4_pikachu_ness();
    test_landing_walk_timing();
    test_fighter_colanim();
    test_specialn_demux();
    test_specialhi_demux();
    test_specialair_demux();
    test_attack_air();
    test_smash_attacks();
    test_attack_dash();
    test_singles_rows();
    test_fox_firefox();
    test_captain_falcon_dive();
    test_yoshi_egg_lay();
    test_yoshi_egg_throw();
    test_kirby_final_cutter();
    test_kirby_inhale();
    test_kirby_copy();
    test_link_boomerang();
    test_luigi_table();
    test_ness_table();
    test_roster_entrance();
    test_pikachu_jolt();
    test_pikachu_thunder();
    test_pikachu_agility();
    test_avoid_ub();
    test_weapon_quads();
    test_weapon_pool_roundtrip();
    test_tree_weapon_packs();
    test_entry_vehicles();
    test_ledge_climb_airborne();
    test_ledge_drop();
    test_ledge_timeout();
    test_tumble();
    test_wall_bounce();
    test_wp_pool();
    test_it_pool();
    test_it_main();
    test_it_process();
    test_it_destroy();
    test_it_display();
    test_it_manager_make_item();
    test_it_manager_set_spawn_wait();
    test_it_map();
    test_it_star();
    test_it_tomato();
    test_it_heart();
    test_it_sword();
    test_it_bat();
    test_it_hammer();
    test_it_harisen();
    test_it_fflower();
    test_it_lgun();
    test_it_starrod();
    test_it_gshell();
    test_it_rshell();
    test_it_nbumper();
    test_it_gbumper();
    test_it_powerblock();
    test_it_pakkun();
    test_it_glucky();
    test_it_porygon();
    test_it_marumine();
    test_it_hitokage();
    test_it_fushigibana();
    test_it_tarubomb();
    test_it_msbomb();
    test_it_bombhei();
    test_gm_rumble();
    test_rumble_safety_nets();
    test_gr_hyrule();
    test_gr_jungle();
    test_gr_zebes();
    test_gr_yoster();
    test_gr_pupupu();
    test_gr_inishie();
    test_gr_castle();
    test_gr_yamabuki();
    test_gr_sector();
    test_gr_bonus3();
    test_sc1p_bonus_stage();
    test_collision_line_existence();
    test_item_pack();
    test_it_init_items();
    test_it_make_item_arrow();
    test_item_pack_attrs();
    test_it_model();
    test_it_container();
    test_it_box_effect();
    test_v01_dropped_effects();
    test_v02_hud_particles();
    test_it_taru_roll();
    test_it_monster();
    test_wp_main();
    test_wp_process();
    test_wp_display();
    test_wp_map();
    test_wp_make();
    test_wp_mario_fireball();
    test_wp_fox_blaster();
    test_wp_attrs();
    test_wp_yoshi_star();
    test_wp_samus_charge_shot();
    test_wp_link_boomerang();
    test_ness_specials();
    test_link_bomb();
    test_kirby_spawn_copy();
    test_capsule_status_table();
    test_hit_knocks_item_loose();
    test_dokan_pipe();
    test_luigi_translate_scales();
    test_samus_roll_spline();
    test_anm_tiers();
    test_donkey_cargo_walk_lengths();
    test_fighter_fog();
    test_ft_mario_specialn_accessory();
    test_respawn();
    test_entry_and_appear();
    test_gobj_threads();
    test_entry_sequence();
    test_battle_camera();
    test_battle_camera_v03();
    test_wallpaper_placement();
    test_autodemo();
    test_autodemo_names();
    test_explain();
    test_1pgame_bonus_stats();
    test_bonus_pause_retry();
    test_boss_defeat_end();
    test_training_wallpaper();
    test_mod_lookat();
    test_asset_bundle();
    test_asset_hold();
    test_asset_prefetch();
    test_room_wipe();
    test_ending();
    test_staffroll();
    test_openingroom();
    test_camanim();

    test_parts_world_position();
    test_guard();
    test_roll();
    test_dash_run();
    test_appeal();
    test_shield_break();
    test_grab();
    test_jostle();
    test_attack_statuses();
    test_hit_lands();
    test_hit_misses();
    test_weapon_hits_fighter();
    test_item_hits_fighter();
    test_item_pickup_find();
    test_item_pickup_hold();
    test_item_throw();
    test_item_consume();
    test_hit_stales();

    test_ko_and_rebirth();
    test_ko_1pgame_team_box();
    test_ko_credits_the_attacker();
    /* last: it ends the scene, and syTaskmanCheckBreakLoop stays TRUE
     * until a scene is loaded, which nothing here does */
    test_stock_match_ends();

    /* last: they re-cut the object pools, so nothing any earlier test
     * made survives them.
     *
     * Twice, and that is the point of the second pass: on the N64 every
     * one of these scenes is an overlay, and sc/scmanager.c reloads it
     * -- .data off the ROM, .bss bzeroed -- before starting it, so a
     * scene's statics are zero on entry however many times it has run
     * (src/dc/overlay.h). The port links the overlays in, so the
     * xxxOverlayLoad each test calls above is what stands in for that
     * bzero, and a second pass is what proves it: without it the
     * character select ejects the GObj it kept from the first pass and
     * walks gGCCommonLinks into a sprite. Every assertion below is the
     * first pass's, unchanged -- a scene that comes up differently the
     * second time fails the same CHECK it passed the first. */
    for (pass = 0; pass < 2; pass++)
    {
        test_scene_runs_a_match();
        test_results_ranks_the_match();
        test_time_match();
        test_time_rankings();
        test_auto_handicap();
        test_sudden_death();
        test_sudden_death_rankings();
        test_pause_menu();
        test_pause_reset_no_contest();
        test_offscreen_arrows();
        test_magnify_glass();
        test_three_fighters_jab();
        test_fighter_files_per_scene();
        test_fighter_files_kept();
        test_every_fighter_stands();
        test_computer_dispatch();
        test_computer_follow_objective_walk();
        test_computer_track_item();
        test_computer_evade_target();
        test_computer_counter_attack();
        test_computer_edge_target();
        test_computer_recover();
        test_computer_target_search();
        test_computer_charge_specialn();
        test_computer_idle_patrol();
        test_computer_engage_target();
        test_computer_lunge_attack();
        test_computer_escape_roll();
        test_computer_detect_target();
        test_computer_use_item();
        test_computer_objective_engage();
        test_computer_objective_rush();
        test_computer_objective_status();
        test_computer_damage_detect_size();
        test_computer_find_item();
        test_computer_predict_attack();
        test_computer_proc_default();
        test_computer_proc_stand_walk_jump();
        test_effect_joint_cycle();
        test_slope_contour();
        test_model_parts();
        test_electric_skeleton();
        test_costumes();
        test_texture_parts();
        test_accessory();
        test_demo_status();
        test_part_fog();
        test_part_mobjs();
        test_afterimage();
        test_fighter_shadow();
        test_ground_actors();
        test_matrix_kinds();
        test_effect_manager();
        test_dead_explode();
        test_matanim_color_lerp();
        test_screen_flash();
        test_results_confetti();
        test_sprite_rects();
        test_title_screen();
        test_title_after_opening();
        test_mode_select();
        test_1pmode_menu();
        test_vs_mode();
        test_players_vs();
        test_1ptraining_select();
        test_1pbonus_select();
        test_1ptrainingmode_menu();
        test_maps();
        test_vs_mode_back();
        test_vs_options();
        test_results_screen();
        test_message_scene();
        test_item_switch_row();
        test_item_switch();
        test_data_menu();
        test_vsrecord_menu();
        test_characters_menu();
        test_soundtest_menu();
        test_option_menu();
        test_screenadjust_menu();
        test_backupclear_menu();
        test_congra_menu();
        test_nocontroller_menu();
        test_dcmemcard();
        test_startup_scene();
        test_openingportraits_scene();
        test_openingmario_scene();
        test_openingdonkey_scene();
        test_openinglink_scene();
        test_openingsamus_scene();
        test_openingyoshi_scene();
        test_openingkirby_scene();
        test_openingfox_scene();
        test_openingpikachu_scene();
        test_openingrun_scene();
        test_openingcliff_scene();
        test_openingyamabuki_scene();
        test_openingjungle_scene();
        test_openingyoster_scene();
        test_openingsector_scene();
        test_openingstandoff_scene();
        test_openingclash_scene();
        test_openingnewcomers_scene();
    }

    if (failures)
    {
        printf("hosttest_ft: %d FAILURES\n", failures);
        return 1;
    }
    printf("hosttest_ft: all tests passed\n");
    return 0;
}
#endif /* FT_BAKER */

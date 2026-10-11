/* hosttest/frontend.c -- part of hosttest_ft.c: the sprite renderer, the attribute and sound tables, the title and
 * every menu scene, the item switch and the unlock message.
 *
 * Not compiled on its own: hosttest_ft.c includes every part in
 * order, so this file sees everything the parts before it define. */

/* ---- the sprite renderer and the title ------------------ */

/* lb/lbcommon.c lbCommonPrepSObjDraw and lbCommonDrawSObjBitmap on a
 * sprite built by hand to the title's Cutout (208x90 I4 in three strips
 * of bmheight 30, stored 31/31/30 rows), placed where mnTitleSetPosition
 * puts it. The rectangles are worked by hand from the decomp's text --
 * they are the N64's own numbers, rounding quirks included -- and the
 * quads from the port's stated mapping (lbcommon.h). */

static void test_sprite_rects(void)
{
    static Bitmap bitmaps[3];
    static DCSpriteTex tex;
    Sprite sprite;
    GObj *gobj;
    SObj *sobj;
    int i;
    const LBCommonSpriteQuad *q;

    memset(&sprite, 0, sizeof(sprite));
    sprite.width = 208;
    sprite.height = 90;
    sprite.scalex = sprite.scaley = 1.0F;
    sprite.attr = SP_TRANSPARENT;
    sprite.red = sprite.green = sprite.blue = sprite.alpha = 255;
    sprite.istep = 1;
    sprite.nbitmaps = 3;
    sprite.bmheight = 30;
    sprite.bmHreal = 31;
    sprite.bmfmt = G_IM_FMT_I;
    sprite.bmsiz = G_IM_SIZ_4b;
    sprite.bitmap = bitmaps;
    tex.texw = 256;
    tex.texh = 128;
    tex.imgw = 208;
    tex.imgh = 90;
    tex.fmt = nDCSpriteTexFmtARGB4444;
    for (i = 0; i < 3; i++)
    {
        bitmaps[i].width = bitmaps[i].width_img = 208;
        bitmaps[i].t = i * 30;
        bitmaps[i].buf = &tex;
        bitmaps[i].actualHeight = (i == 2) ? 30 : 31;
    }

    gobj = gcMakeGObjSPAfter(8, NULL, 8, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 1, GOBJ_PRIORITY_DEFAULT, ~0);
    sobj = lbCommonMakeSObjForGObj(gobj, &sprite);
    sobj->sprite.attr = SP_TRANSPARENT;
    /* mnTitleSetPosition for kind 0, {157, 94}: the centre less half the
     * size */
    sobj->pos.x = 157 - 208 * 0.5F;
    sobj->pos.y = 94 - 90 * 0.5F;
    sobj->sprite.red = sobj->sprite.green = sobj->sprite.blue = 0;

    /* what lbCommonDrawSprite does around the capture, for the title's
     * viewport (10, 10, 310, 230) */
    gcSetDrawList(PVR_LIST_TR_POLY);
    lbCommonStartSprite(NULL);
    lbCommonSetSpriteScissor(10, 310, 10, 230);
    gLBCommonSpriteQuadLogCount = 0;
    lbCommonDrawSObjAttr(gobj);
    lbCommonFinishSprite(NULL);

    CHECK(gLBCommonSpriteQuadLogCount == 3);
    q = gLBCommonSpriteQuadLog;

    /* strip 0: posx 53, posy 49; the rectangle runs to x 261 (53 + 208
     * + 0.9999 truncated) and y 79 (49 + 30); s and t start at 0 and step
     * one texel a pixel (0x400 in 5.10) */
    CHECK(q[0].rect.rxh == 53 * 4 && q[0].rect.ryh == 49 * 4);
    CHECK(q[0].rect.rxl == 261 * 4 && q[0].rect.ryl == 79 * 4);
    CHECK(q[0].rect.rs == 0 && q[0].rect.rt == 0);
    CHECK(q[0].rect.sx == 0x400 && q[0].rect.sy == 0x400);
    CHECK(q[0].rect.t0 == 0 && !q[0].rect.copy);
    /* strip 1: y 79 to 110 (79 + bmHreal 31, the game's own step) */
    CHECK(q[1].rect.ryh == 79 * 4 && q[1].rect.ryl == 110 * 4);
    CHECK(q[1].rect.rt == 0 && q[1].rect.t0 == 30);
    /* strip 2: the running edge is 109 but the last rectangle drawn
     * ended at 110, so it starts at 110 with t shifted by 31/32 of a
     * texel to compensate, and runs to 139 (30 * 1 + 109 + 0.9999) */
    CHECK(q[2].rect.ryh == 110 * 4 && q[2].rect.ryl == 139 * 4);
    CHECK(q[2].rect.rt == 31 && q[2].rect.t0 == 60);

    /* the quads: the game's 320x240 doubled, texels over the 256x128
     * texture */
    CHECK(q[0].x0 == 106.0F && q[0].y0 == 98.0F);
    CHECK(q[0].x1 == 522.0F && q[0].y1 == 158.0F);
    CHECK(q[0].u0 == 0.0F && q[0].v0 == 0.0F);
    CHECK(fabsf(q[0].u1 - 208.0F / 256.0F) < 1e-6F);
    CHECK(fabsf(q[0].v1 - 30.0F / 128.0F) < 1e-6F);
    CHECK(fabsf(q[2].v0 - (60.0F + 31.0F / 32.0F) / 128.0F) < 1e-6F);
    CHECK(fabsf(q[2].v1 - (60.0F + 31.0F / 32.0F + 29.0F) / 128.0F) < 1e-6F);
    /* the depth steps up per rectangle */
    CHECK(q[1].z > q[0].z && q[2].z > q[1].z);
    /* an I sprite: RGB from the prim colour, alpha from the texel */
    CHECK(q[0].argb == 0xFF000000 && q[0].oargb == 0);

    /* scissored: the same sprite with its top above the border draws
     * from y 10 with t advanced by the rows cut off */
    sobj->pos.y = -2.0F;
    gLBCommonSpriteQuadLogCount = 0;
    lbCommonStartSprite(NULL);
    lbCommonDrawSObjAttr(gobj);
    CHECK(gLBCommonSpriteQuadLogCount == 3);
    CHECK(q[0].rect.ryh == 10 * 4);
    CHECK(q[0].rect.rt == ((10 - (-2)) * 0x400) >> 5);

    gcEjectGObj(gobj);
}

/* the title's own numbers, seen from inside the scene */
static int gTitleSeen[4];

static void title_tic(void)
{
    u32 tic = dSYTaskmanUpdateCount;
    GObj *gobj;
    SObj *sobj;

    /* tic 60: the clock stands at 229 -- past 170 (labels shown) and
     * 220 (end layout pinned), before 280 (PRESS START) */
    if (tic == 60)
    {
        int n = 0;

        for (gobj = gGCCommonLinks[8]; gobj != NULL; gobj = gobj->link_next)
        {
            CHECK(gobj->flags == GOBJ_FLAG_NONE);
            if (gobj->id == 8)
            {
                /* mntitle.c dMNTitleCommonSpriteDescs and
                 * mnTitleSetColors, kinds 0..5 */
                sobj = SObjGetStruct(gobj);
                CHECK(sobj->pos.x == 157 - 104.0F && sobj->pos.y == 94 - 45.0F);
                CHECK(sobj->sprite.red == 0 && sobj->sprite.blue == 0);
                sobj = sobj->next;                          /* Smash */
                CHECK(sobj->pos.x == 161 - 86.0F && sobj->pos.y == 88 - 31.0F);
                CHECK(sobj->sprite.red == 0xFF && sobj->sprite.green == 0xFE &&
                      sobj->sprite.blue == 0x2A);
                sobj = sobj->next;                          /* Super */
                CHECK(sobj->pos.x == 55 - 32.0F && sobj->pos.y == 96 - 25.0F);
                sobj = sobj->next;                          /* Bros */
                CHECK(sobj->pos.x == 268 - 28.0F && sobj->pos.y == 96 - 26.0F);
                sobj = sobj->next;                          /* TMUnk */
                CHECK(sobj->pos.x == 270 - 16.0F && sobj->pos.y == 132 - 6.0F);
                CHECK(sobj->next == NULL);
                CHECK(sobj->sprite.scalex == 1.0F);
                n++;
            }
            if (gobj->id == 9)
            {
                /* the footer and the header, kinds 5 and 6 */
                sobj = SObjGetStruct(gobj);                 /* Copyright */
                CHECK(sobj->pos.x == 160 - 150.0F && sobj->pos.y == 208 - 22.0F);
                CHECK(sobj->sprite.red == 0xB7 && sobj->envcolor.r == 0x14);
                sobj = sobj->next;                          /* BorderUpper */
                CHECK(sobj->pos.x == 160 - 150.0F && sobj->pos.y == 15 - 5.0F);
                CHECK(sobj->sprite.red == 0x14 && sobj->sprite.blue == 0x06);
                CHECK(sobj->next == NULL);
                n++;
            }
        }
        CHECK(n == 2);
        /* PRESS START waits on link 9 */
        CHECK(gGCCommonLinks[9] != NULL && gGCCommonLinks[9]->flags == GOBJ_FLAG_HIDDEN);
        /* the logo silhouette on link 10, at its end position */
        sobj = SObjGetStruct(gGCCommonLinks[10]);
        CHECK(sobj->pos.x == 260 - 64.0F && sobj->pos.y == 60 - 62.0F);
        CHECK(sobj->sprite.red == 0xFF && sobj->sprite.green == 0);
        CHECK(gSCManagerSceneData.is_title_anim_viewed == FALSE);

        /* the two animation trees on link 8 (id 10, never hidden), the
         * labels' root and seven joints and PRESS START's root and one;
         * mnTitleSetPosition put each joint where its sprite belongs, in
         * the tree's centred coordinates */
        n = 0;
        for (gobj = gGCCommonLinks[8]; gobj != NULL; gobj = gobj->link_next)
        {
            if (gobj->id == 10)
            {
                DObj *dobj = DObjGetStruct(gobj)->child;
                int joints = 0;

                if (n == 0)
                {
                    /* Cutout {157, 94} and the border {160, 15} */
                    CHECK(dobj->translate.vec.f.x == 157 - 160.0F &&
                          dobj->translate.vec.f.y == -(94 - 120.0F));
                }
                else
                {
                    /* PRESS START {162, 177} */
                    CHECK(dobj->translate.vec.f.x == 162 - 160.0F &&
                          dobj->translate.vec.f.y == -(177 - 120.0F));
                }
                for (; dobj != NULL; dobj = dobj->sib_next)
                {
                    joints++;
                }
                CHECK(joints == ((n == 0) ? 7 : 1));
                n++;
            }
        }
        CHECK(n == 2);

        /* the flames: one GObj (id 5) on link 6, shown at once because
         * the title did not come from the opening, two sprites twelve
         * frames apart and stretched (mntitle.c:934-994), still twelve
         * apart after a second of cycling round the thirty */
        gobj = gGCCommonLinks[6];
        CHECK(gobj != NULL && gobj->id == 5 && gobj->link_next == NULL);
        CHECK(gobj->flags == GOBJ_FLAG_NONE);
        sobj = SObjGetStruct(gobj);
        CHECK(sobj->pos.x == -32.0F && sobj->pos.y == -16.0F);
        CHECK(sobj->sprite.scalex == 12.0F && sobj->sprite.scaley == 8.5F);
        CHECK(sobj->user_data.s >= 0 && sobj->user_data.s < 30);
        CHECK(sobj->user_data.s == (sobj->next->user_data.s + 12) % 30);
        sobj = sobj->next;
        CHECK(sobj->pos.x == 8.0F && sobj->pos.y == 8.0F);
        CHECK(sobj->sprite.scalex == 9.5F && sobj->sprite.scaley == 7.0F);
        CHECK(sobj->next == NULL);

        /* the fire camera: the first camera on link 2, a fill */
        gobj = gGCCommonLinks[2];
        CHECK(gobj != NULL && CObjGetStruct(gobj)->flags == (COBJ_FLAG_FILLCOLOR | COBJ_FLAG_ZBUFFER));
        CHECK((CObjGetStruct(gobj)->color & 0xFF) == 0xFF);
        gTitleSeen[0] = 1;
    }
    /* one frame's worth of quads: the draw ran after tic 60's update.
     * The fire camera's fill (1) and the two flames (1 each) first, then
     * 4 (logo), 3 (Cutout) + 21 (Smash) + 1 + 1 + 1 on the first GObj, 4
     * (Copyright) + 1 (border) on the second = 39; the draw runs twice,
     * once per list, and only the translucent pass draws sprites */
    if (tic == 61)
    {
        const LBCommonSpriteQuad *q = gLBCommonSpriteQuadLog;
        GObj *camera = gGCCommonLinks[2];
        u32 color = CObjGetStruct(camera)->color;

        CHECK(gLBCommonSpriteQuadLogCount == 39);
        /* the fill: the whole default viewport, in the camera's colour,
         * under everything else */
        CHECK(q[0].rect.copy == -1);
        CHECK(q[0].x0 == 0.0F && q[0].y0 == 0.0F && q[0].x1 == 640.0F && q[0].y1 == 480.0F);
        CHECK(q[0].argb == (0xFF000000 | (color >> 8)));
        CHECK(q[0].z < q[1].z);
        /* the flames: the texel's colour, alpha 0xFF, clipped to the
         * sprite scissor */
        CHECK(q[1].rect.copy == 0 && q[1].argb == 0xFFFFFFFF && q[1].oargb == 0);
        CHECK(q[2].argb == 0xFFFFFFFF);
        CHECK(q[1].x0 == 20.0F && q[1].y0 == 20.0F && q[1].x1 == 620.0F && q[1].y1 == 460.0F);
        gTitleSeen[1] = 1;
    }
    if (tic == 60 || tic == 100)
    {
        gLBCommonSpriteQuadLogCount = 0;
    }
    /* tic 111: the clock reached 280 last tic -- PRESS START is up and
     * a press may proceed */
    if (tic == 111)
    {
        CHECK(gGCCommonLinks[9]->flags == GOBJ_FLAG_NONE);
        CHECK(gSCManagerSceneData.is_title_anim_viewed == TRUE);
        gTitleSeen[2] = 1;
    }
    /* tic 116: the press on 115 made the black camera, drawn last
     * (DL link priority 0) over everything for the three tics left */
    if (tic == 116)
    {
        int fills = 0;

        for (gobj = gGCCommonLinks[2]; gobj != NULL; gobj = gobj->link_next)
        {
            CObj *cobj = CObjGetStruct(gobj);

            if (cobj->flags == COBJ_FLAG_FILLCOLOR)
            {
                CHECK(cobj->color == 0x000000FF);
                fills++;
            }
        }
        CHECK(fills == 1);
        gTitleSeen[3] = 1;
    }
    /* press from tic 115 on. The stub computed this tic's tap before
     * calling here, so the flag goes up on 114; every odd tic is then an
     * edge, so tic 115 is the press, and the three-tic wait leaves after
     * tic 118 */
    if (tic >= 114)
    {
        gSceneFeedStart = 1;
    }
}

/* The title on the game's clock: its sprites appear at 170, pin at 220,
 * PRESS START shows at 280, and a press after that leaves for the
 * battle three tics later. The sprite bank is the real one the game's
 * build makes (romdisk/mntitle.spr), so the numbers the sprites carry --
 * sizes, strip counts -- are the ROM's. Last, because the scene re-cuts
 * the pools and re-inits the heap. */
/* src/dc/syaudio.c's host arm counts the game's stop-every-FGM calls:
 * the engine itself is AICA code and is not in this build, but which
 * scenes owe the call is game logic and belongs here. Before.
 * the port made none of them, and the last sound of a battle went on
 * sounding through every screen after it. */
extern int gHostFGMStopAllCount;

/* sys/audio.c's sound players, (src/dc/syaudio.c).
 * The engine under them is AICA code and is not in this build, so that
 * file's host arm stands a pool of 24 alSoundEffects in its place: no
 * voice script runs, so a host voice lives until something stops it,
 * which is exactly what makes the table's own rules visible. What is
 * under test here is all game logic -- who gets which slot, what frees
 * one, and what lbCommonMakePositionFGM writes into a handle. Without
 * the table, syAudioPlayFGM returns 0 or -1, and every one of these
 * paths takes its NULL branch.
 */
/* ftManagerSetupAttributes carries the pack's joint tables across as the
 * pointers the game's code reads them through (fttypes.h:948, :953): the
 * setup mask the parts setup tests and the hidden-part rows
 * ftMainUpdateHiddenPartID indexes by FTAnimDesc bit. They are
 * in the pack. */
static void test_attr_joint_tables(void)
{
    FTAttributes attr;
    FPackAttr pa = kMario;

    pa.hiddenpart_count = 4;
    pa.hiddenparts[0][0] = 2; pa.hiddenparts[0][1] = 0;
    pa.hiddenparts[0][2] = 1; pa.hiddenparts[0][3] = 3;
    pa.hiddenparts[1][0] = 1; pa.hiddenparts[1][1] = 0;
    pa.hiddenparts[1][2] = 0; pa.hiddenparts[1][3] = 3;
    pa.hiddenparts[2][0] = 3; pa.hiddenparts[2][1] = 2;
    pa.hiddenparts[2][2] = 1; pa.hiddenparts[2][3] = 0;
    pa.hiddenparts[3][0] = 28; pa.hiddenparts[3][1] = 4;
    pa.hiddenparts[3][2] = 1; pa.hiddenparts[3][3] = 0;
    ftManagerSetupAttributes(&attr, &pa);

    CHECK(attr.setup_parts == pa.setup_parts);
    CHECK(attr.setup_parts[0] == 0xE0000000u && attr.setup_parts[1] == 0u);
    CHECK((void *)attr.hiddenparts == (void *)pa.hiddenparts);
    /* FTHiddenPart is the row's four words in the row's order */
    CHECK(sizeof(FTHiddenPart) == sizeof(pa.hiddenparts[0]));
    CHECK(attr.hiddenparts[1].root_joint_id == nFTPartsJointTransN);
    CHECK(attr.hiddenparts[1].parent_joint_id == nFTPartsJointTopN);
    CHECK(attr.hiddenparts[1].joint_kind == 3);
    CHECK(attr.hiddenparts[3].root_joint_id == 28 &&
          attr.hiddenparts[3].parent_joint_id == 4 &&
          attr.hiddenparts[3].partindex_0x8 == 1 &&
          attr.hiddenparts[3].joint_kind == 0);
    printf("attr joint tables: setup mask and %d hidden-part rows reach "
           "FTAttributes as the game's pointers\n",
           (int)pa.hiddenpart_count);
}

/* The item half of the attributes across the same copy:
 * fttypes.h:917-920 item_pickup / itemthrow_vel_scale /
 * itemthrow_damage_scale / heavyget_sfx, and :968/:970 the two hand
 * joints. The values are Mario's own ROM row
 * (relocData/203_MarioMain.c:280-283, :348, :350) rather than the
 * TopN-flattened hand joints kMario carries for the three-DObj mock's
 * sake, so this is the one place that checks the real pair makes it
 * across. Everything upstream of the copy -- reading them off the ROM at
 * the right offset -- is the exporter's own per-fighter assertion
 * against the decomp initializer, which runs for all twelve every build.
 *
 * The flat float[8] is FTItemPickup's four Vec2f in struct order:
 * offset light, range light, offset heavy, range heavy. Getting that
 * order wrong would put the heavy window on a light item and nothing
 * else would notice, so each of the eight is named here. */
static void test_pack_attr_item_half(void)
{
    FTAttributes attr;
    FPackAttr pa = kMario;

    pa.item_pickup[0] = 105.0f; pa.item_pickup[1] = 0.0f;
    pa.item_pickup[2] = 378.0f; pa.item_pickup[3] = 200.0f;
    pa.item_pickup[4] = 75.0f;  pa.item_pickup[5] = 0.0f;
    pa.item_pickup[6] = 150.0f; pa.item_pickup[7] = 150.0f;
    pa.itemthrow_vel_scale = 100;
    pa.itemthrow_damage_scale = 100;
    pa.heavyget_sfx = nSYAudioVoiceMarioHeavyGet;
    pa.joint_itemheavy_id = 28;
    pa.joint_itemlight_id = 17;

    ftManagerSetupAttributes(&attr, &pa);

    CHECK(attr.item_pickup.pickup_offset_light.x == 105.0f);
    CHECK(attr.item_pickup.pickup_offset_light.y == 0.0f);
    CHECK(attr.item_pickup.pickup_range_light.x == 378.0f);
    CHECK(attr.item_pickup.pickup_range_light.y == 200.0f);
    CHECK(attr.item_pickup.pickup_offset_heavy.x == 75.0f);
    CHECK(attr.item_pickup.pickup_offset_heavy.y == 0.0f);
    CHECK(attr.item_pickup.pickup_range_heavy.x == 150.0f);
    CHECK(attr.item_pickup.pickup_range_heavy.y == 150.0f);
    CHECK(attr.itemthrow_vel_scale == 100);
    CHECK(attr.itemthrow_damage_scale == 100);
    CHECK(attr.heavyget_sfx == nSYAudioVoiceMarioHeavyGet);
    CHECK(attr.joint_itemheavy_id == 28);
    CHECK(attr.joint_itemlight_id == 17);

    printf("attr item half: Mario's pickup rectangle, throw scales and "
           "hands (%d light, %d heavy) reach FTAttributes\n",
           (int)attr.joint_itemlight_id, (int)attr.joint_itemheavy_id);
}

/* The voice half across the same copy: fttypes.h:913-916,
 * Mario's own row (relocData/203_MarioMain.c:276-279). Before the pack
 * carried these, ftManagerSetupAttributes' memset left all seven 0 --
 * nSYAudioFGMExplodeS, a real sound, so every "!= nSYAudioFGMVoiceEnd"
 * guard in front of them passed and each voice played an explosion. So
 * the check is not only that the values arrive but that none of them is
 * that 0. Smash3 before Smash1 is deliberate: Fox's and Captain Falcon's
 * rows are not in 1-2-3 order, and a copy that sorted or shifted the
 * array would pass on an in-order row. */
/* The sound RAM sets (src/dc/sndres.c), composed out of
 * the real sndsets.bin tools/export/ssb_sndsets.py writes -- the host cannot
 * upload anything, so what it checks is the part that decides: which
 * groups a scene takes, and when a scene change has to swap. The menus
 * share one upload; the character select adds all twelve fighters and
 * the stage select, which it leads to and returns from, reuses them; a
 * battle takes the common set, its own fighters and its own stage and
 * leaves the others out; the results screen swaps again. */
static int sndres_group_resident(unsigned group)
{
    const uint16_t *list;
    unsigned i, n;

    list = sndres_group_fgm(group, &n);
    for (i = 0; i < n; i++)
        if (!sndres_fgm_resident(list[i]))
            return 0;
    list = sndres_group_bgm(group, &n);
    for (i = 0; i < n; i++)
        if (!sndres_bgm_resident(list[i]))
            return 0;
    return 1;
}

/* A sample in `group` that none of `others` has, or -1. */
static int sndres_only_in(unsigned group, const unsigned *others, int n_others)
{
    const uint16_t *list, *other;
    unsigned i, j, n, m;
    int k;

    list = sndres_group_fgm(group, &n);
    for (i = 0; i < n; i++)
    {
        int shared = 0;

        for (k = 0; k < n_others && !shared; k++)
        {
            other = sndres_group_fgm(others[k], &m);
            for (j = 0; j < m; j++)
                if (other[j] == list[i])
                    shared = 1;
        }
        if (!shared)
            return list[i];
    }
    return -1;
}

static void test_sndres_scene_sets(void)
{
    static const unsigned kBattleGroups[] = {
        SNDRES_GROUP_CORE, SNDRES_GROUP_FIGHTER + nFTKindMario,
        SNDRES_GROUP_FIGHTER + nFTKindFox, SNDRES_GROUP_MENU,
        SNDRES_GROUP_RESULTS,
    };
    SCBattleState saved_transfer = gSCManagerTransferBattleState;
    u8 saved_gkind = gSCManagerSceneData.gkind;
    unsigned n;
    uint32_t swaps;
    int ness_only;
    s32 i;

    CHECK(sndres_init(32) == 0);
    sndres_group_fgm(SNDRES_GROUP_FIGHTER + nFTKindNess, &n);
    CHECK(n > 0);
    ness_only = sndres_only_in(SNDRES_GROUP_FIGHTER + nFTKindNess,
                               kBattleGroups, 5);
    CHECK(ness_only >= 0);
    swaps = sndres_swaps();

    sndres_enter_scene(nSCKindTitle);
    CHECK(sndres_swaps() == swaps + 1);
    CHECK(sndres_group_resident(SNDRES_GROUP_MENU));
    CHECK(!sndres_fgm_resident((unsigned)ness_only));
    sndres_enter_scene(nSCKindModeSelect);
    sndres_enter_scene(nSCKindVSMode);
    sndres_enter_scene(nSCKindVSOptions);
    sndres_enter_scene(nSCKindVSItemSwitch);
    CHECK(sndres_swaps() == swaps + 1);

    sndres_enter_scene(nSCKindPlayersVS);
    CHECK(sndres_swaps() == swaps + 2);
    CHECK(sndres_group_resident(SNDRES_GROUP_MENU));
    for (i = 0; i < SNDRES_FIGHTERS; i++)
        CHECK(sndres_group_resident(SNDRES_GROUP_FIGHTER + i));
    sndres_enter_scene(nSCKindMaps);
    sndres_enter_scene(nSCKindPlayersVS);
    sndres_enter_scene(nSCKindVSMode);
    CHECK(sndres_swaps() == swaps + 2);

    for (i = 0; i < GMCOMMON_PLAYERS_MAX; i++)
        gSCManagerTransferBattleState.players[i].pkind = nFTPlayerKindNot;
    gSCManagerTransferBattleState.players[0].pkind = nFTPlayerKindMan;
    gSCManagerTransferBattleState.players[0].fkind = nFTKindMario;
    gSCManagerTransferBattleState.players[1].pkind = nFTPlayerKindCom;
    gSCManagerTransferBattleState.players[1].fkind = nFTKindFox;
    gSCManagerSceneData.gkind = nGRKindCastle;

    sndres_enter_scene(nSCKindVSBattle);
    CHECK(sndres_swaps() == swaps + 3);
    CHECK(sndres_group_resident(SNDRES_GROUP_CORE));
    CHECK(sndres_group_resident(SNDRES_GROUP_FIGHTER + nFTKindMario));
    CHECK(sndres_group_resident(SNDRES_GROUP_FIGHTER + nFTKindFox));
    CHECK(sndres_group_resident(SNDRES_GROUP_STAGE + nGRKindCastle));
    CHECK(!sndres_group_resident(SNDRES_GROUP_STAGE + nGRKindYamabuki));
    CHECK(!sndres_fgm_resident((unsigned)ness_only));
    /* sudden death is the same scene run again: no swap */
    sndres_enter_scene(nSCKindVSBattle);
    CHECK(sndres_swaps() == swaps + 3);

    sndres_enter_scene(nSCKindVSResults);
    CHECK(sndres_swaps() == swaps + 4);
    CHECK(sndres_group_resident(SNDRES_GROUP_RESULTS));
    CHECK(sndres_group_resident(SNDRES_GROUP_MENU));
    CHECK(sndres_group_resident(SNDRES_GROUP_FIGHTER + nFTKindMario));
    CHECK(!sndres_fgm_resident((unsigned)ness_only));
    /* the unlock message after it is a menu scene, and the results set
     * holds the menus' */
    sndres_enter_scene(nSCKindMessage);
    CHECK(sndres_swaps() == swaps + 4);

    gSCManagerTransferBattleState = saved_transfer;
    gSCManagerSceneData.gkind = saved_gkind;
    printf("sndres: menus share one set, the character select adds the "
           "twelve fighters, a battle holds its own two and its stage and "
           "not Ness's, results swap again (%u swaps)\n",
           (unsigned)(sndres_swaps() - swaps));
}

static void test_pack_attr_voice_half(void)
{
    FTAttributes attr;
    FPackAttr pa = kMario;
    s32 i;

    pa.dead_fgm_ids[0] = nSYAudioVoiceMarioDead;
    pa.dead_fgm_ids[1] = nSYAudioFGMMarioDeadSlam;
    pa.deadup_sfx = nSYAudioVoiceMarioDeadUp;
    pa.damage_sfx = nSYAudioVoiceMarioDamage;
    pa.smash_sfx[0] = nSYAudioVoiceMarioSmash3;
    pa.smash_sfx[1] = nSYAudioVoiceMarioSmash1;
    pa.smash_sfx[2] = nSYAudioVoiceMarioSmash2;

    ftManagerSetupAttributes(&attr, &pa);

    CHECK(attr.dead_fgm_ids[0] == nSYAudioVoiceMarioDead);
    CHECK(attr.dead_fgm_ids[1] == nSYAudioFGMMarioDeadSlam);
    CHECK(attr.deadup_sfx == nSYAudioVoiceMarioDeadUp);
    CHECK(attr.damage_sfx == nSYAudioVoiceMarioDamage);
    CHECK(attr.smash_sfx[0] == nSYAudioVoiceMarioSmash3);
    CHECK(attr.smash_sfx[1] == nSYAudioVoiceMarioSmash1);
    CHECK(attr.smash_sfx[2] == nSYAudioVoiceMarioSmash2);
    CHECK(attr.dead_fgm_ids[0] != nSYAudioFGMExplodeS);
    CHECK(attr.deadup_sfx != nSYAudioFGMExplodeS);
    CHECK(attr.damage_sfx != nSYAudioFGMExplodeS);
    for (i = 0; i < 3; i++)
        CHECK(attr.smash_sfx[i] != nSYAudioFGMExplodeS);

    printf("attr voice half: Mario's KO, star-KO, hurt and three smash "
           "voices reach FTAttributes (damage %d)\n",
           (int)attr.damage_sfx);
}

static void test_sound_players(void)
{
    int stops = gHostFGMStopAllCount;
    alSoundEffect *sfx;
    s32 i;

    /* sys/audio.c:1371: the lowest free slot, and its index is the
     * handle the rest of sys/audio.c indexes back with. */
    for (i = 0; i < SYAUDIO_SNDPLAYERS_NUM; i++)
    {
        CHECK(syAudioPlayFGM(nSYAudioFGMMenuSelect) == i);
    }
    /* The table is the same 24 as the engine's voice pool, so full is
     * full: no slot, no sound, and the caller is told. */
    CHECK(syAudioPlayFGM(nSYAudioFGMMenuSelect) == -1);

    /* sys/audio.c:1458 syAudioStopFGM frees its slot there and then,
     * without waiting for the sweep, so the next sound takes it. */
    syAudioStopFGM(7);
    CHECK(syAudioPlayFGM(nSYAudioFGMMenuSelect) == 7);

    /* n_env.c:5128 ends every voice, but the slots are the audio
     * thread's to release: nothing is free until the sweep runs
     * (sys/audio.c:1092-1098), and then everything is. */
    func_800266A0_272A0();
    CHECK(gHostFGMStopAllCount - stops == 1);
    CHECK(syAudioPlayFGM(nSYAudioFGMMenuSelect) == -1);
    syAudioSweepFGMPlayers();
    CHECK(syAudioPlayFGM(nSYAudioFGMMenuSelect) == 0);

    func_800266A0_272A0();
    syAudioSweepFGMPlayers();

    /* A handle carries a serial (0x26, gm/gmsound.h's sfx_id) that a
     * stop clears, and that is the whole of the test ft/ftmain.c:1209
     * and ft/ftparam.c:84 make before they stop a sound they started:
     * the voice they are holding is still the voice they started. */
    sfx = func_800269C0_275C0(nSYAudioFGMMenuSelect);
    CHECK(sfx != NULL);
    CHECK(sfx->sfx_id != 0);
    func_80026738_27338(sfx);
    CHECK(sfx->sfx_id == 0);

    /* lb/lbcommon.c:725 lbCommonMakePositionFGM turns a world x into
     * the voice's balance: centre 64, and hard over at 8000 units
     * either way, clamped past that. This is the call the whole handle
     * exists for -- a hit that sounds from where it happened. */
    {
        static const struct { f32 pos; u8 balance; } kPan[] = {
            {      0.0F,  64 },
            {   8000.0F,   4 },
            {  -8000.0F, 124 },
            {  99999.0F,   4 },
            { -99999.0F, 124 },
            {   4000.0F,  34 },
        };
        size_t k;

        for (k = 0; k < ARRAY_COUNT(kPan); k++)
        {
            sfx = lbCommonMakePositionFGM(nSYAudioFGMMenuSelect, kPan[k].pos);
            CHECK(sfx != NULL);
            CHECK(sfx->balance == kPan[k].balance);
        }
    }

    func_800266A0_272A0();
    syAudioSweepFGMPlayers();
}

/* ft/ftpublic.c, compiled unmodified: the crowd and
 * the announcer. It is the port's first file out of overlay 3, and its
 * eighteen statics are that overlay's whole noload segment
 * (src/dc/overlay.c).
 *
 * Everything the file does is a sequence of voice ids, so that sequence
 * is what is checked here, off the log src/dc/syaudio.c's host double
 * keeps. The tiers can be driven without a battle standing because the
 * one thing they do with a fighter is ftParamGetPlayerNumGObj, which
 * walks an empty link list and hands back NULL.
 *
 * ftPublicCommonCheck and ftPublicDefeatedAddID in src/dc/ftcommon.c and
 * src/dc/ftparam.c are real: a hit reaches the audience, and the
 * "<player> defeated" queue has a reader. */
static void test_crowd_reacts(void)
{
    /* ftpublic.c:190-230 ftPublicDecideCommon, the knockback tiers. Each
     * call names a different player from the last, so each takes the
     * plain tier arm rather than the "same player, still inside sixty
     * tics" one above it. */
    crowd_reset();
    ftPublicDecideCommon(NULL, 1, 170.0F, FALSE);
    ftPublicDecideCommon(NULL, 2, 140.0F, FALSE);
    ftPublicDecideCommon(NULL, 3, 110.0F, FALSE);
    CHECK(gHostFGMStartedCount == 3);
    CHECK(gHostFGMStartedLog[0] == nSYAudioVoicePublicDamageL);
    CHECK(gHostFGMStartedLog[1] == nSYAudioVoicePublicDamageM);
    CHECK(gHostFGMStartedLog[2] == nSYAudioVoicePublicDamageS);

    /* and the arm that reads what the last call left behind: the same
     * player again, inside sixty tics, is a continuation of the same
     * reaction and goes through ftPublicDecideCall, which at 170 cheers
     * rather than groans. It only reads that way because
     * sFTPublicCommonPlayerNum is still 3 from the call above. */
    crowd_reset();
    ftPublicDecideCommon(NULL, 3, 170.0F, FALSE);
    CHECK(gHostFGMStartedCount == 1);
    CHECK(gHostFGMStartedLog[0] == nSYAudioVoicePublicCheer);

    /* which is what makes this an overlay test. ft/ftpublic.c's statics
     * are a decomp file's: no xxxOverlayLoad can name them, and the N64
     * did not name them either -- syDmaLoadOverlay bzeroes an address
     * range (sys/dma.c:109). Here that range is the one ld brackets the
     * renamed .bss with, and with it cleared the identical call is a
     * first reaction again. */
    syDmaLoadOverlay(OVERLAY_FIGHTING);
    crowd_reset();
    ftPublicDecideCommon(NULL, 3, 170.0F, FALSE);
    CHECK(gHostFGMStartedCount == 1);
    CHECK(gHostFGMStartedLog[0] == nSYAudioVoicePublicDamageL);

    /* ftpublic.c:286-297 and :310-350: the announcer. if/ifcommon.c
     * queues two voices on a KO that ends a player's run without ending
     * the match (ifcommon.c:1782-1791) -- "Player 2", then "defeated" --
     * and the crowd's own update is what says them, one at a time,
     * waiting for each to finish. Without ft/ftpublic.c the queue has no
     * reader at all: ftPublicDefeatedAddID is a stub in
     * src/dc/ftparam.c that dropped what it was given.
     *
     * ftPublicProcUpdate takes a GObj it never dereferences, and its
     * fighter walk is over an empty link list here, so it can be ticked
     * by hand. What it does start beside the queue is the crowd's chant,
     * with the id a bzeroed sFTPublicCallID leaves at zero -- noise the
     * scans below step over, and the reason they scan rather than count.
     * (In the game ftPublicMakeActor sets that counter past its limit
     * before a tic ever runs.) */
    crowd_reset();
    ftPublicDefeatedAddID(dIFCommonAnnounceDefeatedVoiceIDs[1]);
    ftPublicDefeatedAddID(nSYAudioVoiceAnnounceDefeated);

    ftPublicProcUpdate(NULL);
    CHECK(crowd_said(dIFCommonAnnounceDefeatedVoiceIDs[1]));
    CHECK(!crowd_said(nSYAudioVoiceAnnounceDefeated));

    /* the second waits: the voice it follows is still going */
    ftPublicProcUpdate(NULL);
    CHECK(!crowd_said(nSYAudioVoiceAnnounceDefeated));

    /* and follows it once it has ended, which for the double is the
     * stop-all inside crowd_reset */
    crowd_reset();
    ftPublicProcUpdate(NULL);
    CHECK(crowd_said(nSYAudioVoiceAnnounceDefeated));

    /* the queue is empty now, and stays quiet */
    crowd_reset();
    ftPublicProcUpdate(NULL);
    CHECK(!crowd_said(nSYAudioVoiceAnnounceDefeated));

    /* leave nothing behind for the scene tests: the reload a battle
     * makes anyway */
    syDmaLoadOverlay(OVERLAY_FIGHTING);
}

/* The host's stand-in for a memory card (src/dc/vmusave.c); what it
 * is a fake of, and why, is with test_save_card below. */
static const char kSaveFile[] = "hosttest_save.bin";

static long save_file_size(const char *path)
{
    FILE *f = fopen(path, "rb");
    long n;

    if (f == NULL)
    {
        return -1;
    }
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fclose(f);
    return n;
}

static void test_title_screen(void)
{
    int stops = gHostFGMStopAllCount;
    u8 boot = gSCManagerBackupData.boot;

    gSCManagerSceneData.scene_prev = nSCKindVSResults;
    gSCManagerSceneData.scene_curr = nSCKindTitle;
    gSCManagerSceneData.is_title_anim_viewed = FALSE;
    memset(gTitleSeen, 0, sizeof(gTitleSeen));

    /* a card in the slot for the length of this scene, so the boot count
     * below can be read back off it rather than out of the store */
    remove(kSaveFile);
    sy_sram_set_host_path(kSaveFile);

    gSceneTicHook = title_tic;
    syDmaLoadOverlay(OVERLAY_BATTLE);
    syDmaLoadOverlay(OVERLAY_TITLE);
    mnTitleStartScene();
    gSceneTicHook = NULL;
    gSceneFeedStart = 0;

    CHECK(gTitleSeen[0] && gTitleSeen[1] && gTitleSeen[2] && gTitleSeen[3]);
    /* pressed at tic 115, mnTitleProceedModeSelect the same tic, the
     * wait of 3 counts down on 116, 117, 118 and syTaskmanSetLoadScene
     * fires on 118; the counter is bumped at the top of each tic */
    CHECK((s32)dSYTaskmanUpdateCount == 119);
    CHECK(gSCManagerSceneData.scene_prev == nSCKindTitle);
    CHECK(gSCManagerSceneData.scene_curr == nSCKindModeSelect);
    /* mntitle.c:353 as the scene comes up and :500 as it hands over */
    CHECK(gHostFGMStopAllCount - stops == 2);

    /* mntitle.c:1552-1556, ported because without a store that
     * outlives the session there was nothing for a boot count to mean:
     * the first title of a session counts the boot and writes it. The
     * flag it is guarded by is the game's own, and it is TRUE now, so a
     * second title this session will not count again. */
    CHECK(gSCManagerBackupData.boot == (u8)(boot + 1));
    CHECK(gSCManagerSceneData.is_title_anim_viewed != FALSE);
    {
        /* And the whole way down: the scene's own frame loop found the
         * store dirty and carried it to the card while the title was
         * still on screen (src/dc/taskman.c), so what a power cycle
         * reads back is the count this scene made. This is the one
         * place in the suite where the entire path runs -- game logic,
         * lbBackupWrite, the store, the frame loop, the file -- with
         * nothing driven by hand. */
        LBBackupData ram = gSCManagerBackupData;

        CHECK(sy_sram_is_dirty() == 0);
        CHECK(save_file_size(kSaveFile) == 3036);

        sy_sram_init();
        CHECK(lbBackupIsSramValid() != FALSE);
        CHECK(memcmp(&gSCManagerBackupData, &ram, sizeof(ram)) == 0);
        CHECK(gSCManagerBackupData.boot == (u8)(boot + 1));
    }
    sy_sram_set_host_path(NULL);
    remove(kSaveFile);
}

/* ---- the title after the opening movie -------------------
 *
 * The else arms of mntitle.c, reached with scene_prev at Newcomers: the
 * animated logo tree and the three cutouts riding it, the full logo
 * fading over them, the logo fire's display GObj and tree, the two extra
 * cameras; the hand-over to the fire layout at tic 111; the extended
 * demo wait cleared at 170; and, with no pad touched, the idle
 * demonstration at 650 -- mnTitleProceedDemoNext picks the next two demo
 * fighters and leaves for How to Play. The slash and the particle bank
 * are target-only (the host links no renderer for either), so link 14
 * stays empty here and the tic-111 ejects take their NULL arms. The
 * trees come from the packs the game's build makes (romdisk/mntitlelogo.tra,
 * romdisk/mntitlefiretree.tra); the host plays no animation on them, so
 * every joint keeps its desc scale and the translate the maker zeroed. */
static int gTitleOpeningSeen[5];

static void title_opening_tic(void)
{
    u32 tic = dSYTaskmanUpdateCount;
    GObj *gobj;
    SObj *sobj;
    DObj *dobj;
    int i;

    if (tic == 2)
    {
        int n6 = 0, n7 = 0, cams = 0, ortho = 0, persp = 0, limbs = 0;

        /* link 7: the tree (id 7), a root and four children, and the
         * cutouts (id 6), three black sprites on one GObj */
        for (gobj = gGCCommonLinks[7]; gobj != NULL; gobj = gobj->link_next)
        {
            if (gobj->id == 7)
            {
                int joints = 0;

                for (dobj = DObjGetStruct(gobj)->child; dobj != NULL; dobj = dobj->sib_next)
                {
                    CHECK(dobj->translate.vec.f.x == 0.0F && dobj->translate.vec.f.y == 0.0F);
                    joints++;
                }
                CHECK(joints == 4);
                n7++;
            }
            if (gobj->id == 6)
            {
                int k = 0;

                CHECK(gobj->flags == GOBJ_FLAG_NONE);
                for (sobj = SObjGetStruct(gobj); sobj != NULL; sobj = sobj->next)
                {
                    CHECK(sobj->sprite.red == 0 && sobj->sprite.green == 0 && sobj->sprite.blue == 0);
                    CHECK(sobj->sprite.attr == SP_TRANSPARENT);
                    k++;
                }
                CHECK(k == 3);
                n6++;
            }
        }
        CHECK(n7 == 1 && n6 == 1);

        /* the full logo on link 10, red, following the fourth joint: at
         * that joint's scale of 1 and translate of 0 it sits centred */
        gobj = gGCCommonLinks[10];
        CHECK(gobj != NULL && gobj->id == 11 && gobj->link_next == NULL);
        sobj = SObjGetStruct(gobj);
        CHECK(sobj->sprite.red == 0xFF && sobj->sprite.green == 0 && sobj->sprite.blue == 0);
        CHECK(sobj->sprite.scalex == 1.0F && sobj->sprite.scaley == 1.0F);
        CHECK(sobj->pos.x == 160.0F - sobj->sprite.width * 0.5F);
        CHECK(sobj->pos.y == 120.0F - sobj->sprite.height * 0.5F);

        /* the flames wait, hidden, for tic 111 */
        gobj = gGCCommonLinks[6];
        CHECK(gobj != NULL && gobj->id == 5 && gobj->flags == GOBJ_FLAG_HIDDEN);

        /* the logo fire's display GObj on link 4, its tree on link 5 --
         * three limbs of two joints -- and no slash in this build */
        CHECK(gGCCommonLinks[4] != NULL && gGCCommonLinks[4]->id == 15);
        gobj = gGCCommonLinks[5];
        CHECK(gobj != NULL && gobj->id == 14 && gobj->link_next == NULL);
        for (dobj = DObjGetStruct(gobj)->child; dobj != NULL; dobj = dobj->sib_next)
        {
            CHECK(dobj->child != NULL && dobj->child->sib_next == NULL && dobj->child->child == NULL);
            limbs++;
        }
        CHECK(limbs == 3);
        CHECK(gGCCommonLinks[14] == NULL);

        /* four cameras: the fill on link 2, and on link 3 the sprite
         * camera, the slash's ortho at z 2000 and the particles'
         * perspective at z 1000 with a 30-degree fovy */
        for (i = 2; i <= 3; i++)
        {
            for (gobj = gGCCommonLinks[i]; gobj != NULL; gobj = gobj->link_next)
            {
                CObj *cobj = CObjGetStruct(gobj);
                s32 k;

                cams++;
                for (k = 0; k < cobj->xobjs_num; k++)
                {
                    if (cobj->xobjs[k] == NULL)
                    {
                        continue;
                    }
                    if (cobj->xobjs[k]->kind == nGCMatrixKindOrtho)
                    {
                        CHECK(cobj->vec.eye.z == 2000.0F);
                        ortho++;
                    }
                    if (cobj->xobjs[k]->kind == nGCMatrixKindPerspFastF || cobj->xobjs[k]->kind == nGCMatrixKindPerspF)
                    {
                        CHECK(cobj->vec.eye.z == 1000.0F && cobj->projection.persp.fovy == 30.0F);
                        persp++;
                    }
                }
            }
        }
        CHECK(cams == 4 && ortho == 1 && persp == 1);
        gTitleOpeningSeen[0] = 1;
    }
    /* the logo's quads in the translucent pass -- one per strip of the
     * I4 sprite, four of them, as the plain title's tic 61 counts: red
     * under nLBCommonCombineIPrimAlpha with sMNTitleLogoAlpha for
     * alpha, four less a tic from 0xFF, and pinned at 0x4C once there */
    if (tic == 3 || tic == 100)
    {
        const LBCommonSpriteQuad *q = gLBCommonSpriteQuadLog;
        int logos = 0;

        for (i = 0; i < gLBCommonSpriteQuadLogCount; i++)
        {
            if ((q[i].argb & 0x00FFFFFF) == 0x00FF0000 && q[i].rect.copy == 0)
            {
                u32 alpha = q[i].argb >> 24;

                CHECK((tic == 3) ? (alpha > 0x4C && alpha < 0xFF) : (alpha == 0x4C));
                logos++;
            }
        }
        CHECK(logos == 4);
        gTitleOpeningSeen[1] |= (tic == 3) ? 1 : 2;
    }
    if (tic == 2 || tic == 99)
    {
        gLBCommonSpriteQuadLogCount = 0;
    }
    /* tic 111: mnTitleTransitionFromFireLogo -- the cutouts hide, the
     * logo fire's display GObj goes, the flames show */
    if (tic == 112)
    {
        for (gobj = gGCCommonLinks[7]; gobj != NULL; gobj = gobj->link_next)
        {
            if (gobj->id == 6)
            {
                CHECK(gobj->flags == GOBJ_FLAG_HIDDEN);
            }
        }
        CHECK(gGCCommonLinks[4] == NULL);
        CHECK(gGCCommonLinks[6] != NULL && gGCCommonLinks[6]->flags == GOBJ_FLAG_NONE);
        gTitleOpeningSeen[2] = 1;
    }
    /* tic 170: the layout leaves Opening and the extended demo wait
     * with it, so it is the 650 arm that fires below, not 1190 */
    if (tic == 171)
    {
        CHECK(gSCManagerSceneData.is_extend_demo_wait == FALSE);
        gTitleOpeningSeen[3] = 1;
    }
    /* tic 650: mnTitleProceedDemoNext. Two demo fighters out of the
     * unlocked set, both new to demo_mask_prev, the black fill camera
     * drawn last, and How to Play next (the default arm: the scene
     * before this one was the movie, not Explain or ModeSelect) */
    if (tic == 651)
    {
        u16 unlocked = gSCManagerBackupData.fighter_mask | LBBACKUP_CHARACTER_MASK_STARTER;
        int fills = 0;

        CHECK(gSCManagerSceneData.scene_prev == nSCKindTitle);
        CHECK(gSCManagerSceneData.scene_curr == nSCKindExplain);
        CHECK(gSCManagerSceneData.is_extend_demo_wait == TRUE);
        CHECK(gSCManagerSceneData.demo_fkind[0] != gSCManagerSceneData.demo_fkind[1]);
        CHECK(unlocked & LBBACKUP_MASK_FIGHTER(gSCManagerSceneData.demo_fkind[0]));
        CHECK(unlocked & LBBACKUP_MASK_FIGHTER(gSCManagerSceneData.demo_fkind[1]));
        CHECK(gSCManagerSceneData.demo_mask_prev == (LBBACKUP_MASK_FIGHTER(gSCManagerSceneData.demo_fkind[0]) | LBBACKUP_MASK_FIGHTER(gSCManagerSceneData.demo_fkind[1])));
        CHECK(gSCManagerSceneData.demo_first_fkind == gSCManagerSceneData.demo_fkind[0]);
        for (gobj = gGCCommonLinks[2]; gobj != NULL; gobj = gobj->link_next)
        {
            if (CObjGetStruct(gobj)->flags == COBJ_FLAG_FILLCOLOR)
            {
                CHECK(CObjGetStruct(gobj)->color == 0x000000FF);
                fills++;
            }
        }
        CHECK(fills == 1);
        gTitleOpeningSeen[4] = 1;
    }
}

static void test_title_after_opening(void)
{
    int stops = gHostFGMStopAllCount;

    gSCManagerSceneData.scene_prev = nSCKindOpeningNewcomers;
    gSCManagerSceneData.scene_curr = nSCKindTitle;
    gSCManagerSceneData.is_extend_demo_wait = FALSE;
    gSCManagerSceneData.demo_mask_prev = 0;
    memset(gTitleOpeningSeen, 0, sizeof(gTitleOpeningSeen));

    gSceneTicHook = title_opening_tic;
    syDmaLoadOverlay(OVERLAY_BATTLE);
    syDmaLoadOverlay(OVERLAY_TITLE);
    mnTitleStartScene();
    gSceneTicHook = NULL;

    CHECK(gTitleOpeningSeen[0] && gTitleOpeningSeen[1] == 3 && gTitleOpeningSeen[2] &&
          gTitleOpeningSeen[3] && gTitleOpeningSeen[4]);
    /* mnTitleProceedDemoNext on the 650th update, the wait of 3 counts
     * down on the 651st, 652nd and 653rd and syTaskmanSetLoadScene fires
     * on the last; the counter is bumped after each update, so it reads
     * 653 (one less than test_title_screen's, whose press lands on the
     * pad read before an update rather than inside one) */
    CHECK((s32)dSYTaskmanUpdateCount == 653);
    CHECK(gSCManagerSceneData.scene_prev == nSCKindTitle);
    CHECK(gSCManagerSceneData.scene_curr == nSCKindExplain);
    /* mntitle.c:353 as the scene comes up and :461 as it hands over */
    CHECK(gHostFGMStopAllCount - stops == 2);

    /* the mode select test below reads the hand-off test_title_screen
     * left, title -> mode select; put it back */
    gSCManagerSceneData.scene_prev = nSCKindTitle;
    gSCManagerSceneData.scene_curr = nSCKindModeSelect;
}

/* ---- the mode select --------------------------------------
 *
 * mn/mncommon/mnmodeselect.c driven by a scripted pad: the layout the
 * scene builds, the ten tics it ignores the pad for, the stick moving
 * the cursor both ways and round both ends, and A taking VS MODE. The
 * sprites are found by which of the bank's sprites they are -- an SObj
 * copies the Sprite it is made from, and the bitmap pointer is the
 * sprite's own -- because the option GObjs are remade on every move and
 * their order on the link changes. */
static SObj *scene_sprite_find(void **files, s32 link, int file, u32 offset)
{
    Sprite *sp = sprite_bank_get(files[file], offset);
    GObj *gobj;

    for (gobj = gGCCommonLinks[link]; gobj != NULL; gobj = gobj->link_next)
    {
        SObj *sobj = (gobj->obj_kind == 2) ? SObjGetStruct(gobj) : NULL;

        for (; sobj != NULL; sobj = sobj->next)
        {
            if (sp != NULL && sobj->sprite.bitmap == sp->bitmap)
            {
                return sobj;
            }
        }
    }
    return NULL;
}

static SObj *mode_select_find(s32 link, int file, u32 offset)
{
    return scene_sprite_find(sMNModeSelectFiles, link, file, offset);
}

/* the option icons' offsets in file 1, lit and dark, and where each sits
 * (mnmodeselect.c mnModeSelectMake1PMode .. MakeData) */
static const struct
{
    u32 lit, dark;
    f32 x, y;
} kModeSelectIcons[4] =
{
    { 0x1990, 0x6048, 169.0f,  27.0f },      /* 1P GAME: the controller */
    { 0x2520, 0x6708, 128.0f,  64.0f },      /* VS MODE: the console */
    { 0x4C80, 0x82F8,  87.0f, 101.0f },      /* OPTION: the settings */
    { 0x30B0, 0x6DC8,  46.0f, 138.0f }       /* DATA */
};

/* the cursor is on option `lit`: that icon's bright sprite is up at its
 * position with prim white over env black, the other three are their
 * dark sprites at 0x96 grey, and none of the four other sprites is up */
static void mode_select_check_cursor(int lit)
{
    int i;

    for (i = 0; i < 4; i++)
    {
        SObj *up = mode_select_find(3, 1, kModeSelectIcons[i].lit);
        SObj *dark = mode_select_find(3, 1, kModeSelectIcons[i].dark);
        SObj *sobj = (i == lit) ? up : dark;

        CHECK((i == lit) ? (up != NULL && dark == NULL) : (up == NULL && dark != NULL));
        if (sobj == NULL)
        {
            continue;
        }
        CHECK_EQF(sobj->pos.x, kModeSelectIcons[i].x);
        CHECK_EQF(sobj->pos.y, kModeSelectIcons[i].y);
        CHECK((sobj->sprite.attr & (SP_FASTCOPY | SP_TRANSPARENT)) == SP_TRANSPARENT);
        if (i == lit)
        {
            CHECK(sobj->sprite.red == 0xFF && sobj->sprite.green == 0xFF && sobj->sprite.blue == 0xFF);
            CHECK(sobj->envcolor.r == 0 && sobj->envcolor.g == 0 && sobj->envcolor.b == 0);
        }
        else
        {
            CHECK(sobj->sprite.red == 0x96 && sobj->sprite.green == 0x96 && sobj->sprite.blue == 0x96);
        }
    }
}

static int gModeSelectSeen[6];
static s32 gModeSelectQuads;

static void mode_select_tic(void)
{
    s32 tic = (s32)dSYTaskmanUpdateCount;
    SYController *pad = &gSYControllerDevices[0];
    SObj *sobj;
    GObj *gobj;

    pad->button_tap = 0;
    pad->button_hold = 0;
    pad->stick_range.x = 0;
    pad->stick_range.y = 0;

    /* the layout, a few tics in: mnModeSelectMakeDecals on link 2,
     * mnModeSelectMakeOptions and MakeLabels on link 3, the cursor on
     * 1P GAME because the title is not one of the four modes */
    if (tic == 3)
    {
        int n = 0;

        sobj = mode_select_find(2, 0, 0x18000);                 /* the collage */
        CHECK(sobj != NULL);
        if (sobj != NULL)
        {
            CHECK_EQF(sobj->pos.x, 10.0f);
            CHECK_EQF(sobj->pos.y, 10.0f);
            CHECK(!(sobj->sprite.attr & SP_TRANSPARENT));
        }
        sobj = mode_select_find(2, 1, 0x7C38);                  /* the bar's middle, tiled */
        CHECK(sobj != NULL);
        if (sobj != NULL)
        {
            CHECK_EQF(sobj->pos.x, 0.0f);
            CHECK_EQF(sobj->pos.y, 37.0f);
            CHECK(sobj->cms == 0 && sobj->masks == 4 && sobj->lrs == 96 && sobj->lrt == 38);
            CHECK(sobj->sprite.red == 0x08 && sobj->sprite.green == 0x33 && sobj->sprite.blue == 0x65);
            /* what the tile needs of the bank: the sprite is 8 wide
             * but its strip is stored 16 (Bitmap.width_img, an I4 row
             * padded to the word), the mask says 16, and the RDP tiles
             * the stored row -- so the texture is the stored row, 16
             * wide, and the s coordinate wraps where the mask would */
            CHECK(sobj->sprite.width == 8);
            CHECK(sobj->sprite.bitmap->width_img == 16);
            CHECK(((const DCSpriteTex *)sobj->sprite.bitmap->buf)->texw == 16);
        }
        sobj = mode_select_find(2, 1, 0x72E8);                  /* the bar's edge */
        CHECK(sobj != NULL && sobj->pos.x == 96.0f && sobj->pos.y == 37.0f);
        sobj = mode_select_find(2, 1, 0x40F0);                  /* MODE SELECT */
        CHECK(sobj != NULL);
        if (sobj != NULL)
        {
            CHECK_EQF(sobj->pos.x, 28.0f);
            CHECK_EQF(sobj->pos.y, 27.0f);
            CHECK(sobj->sprite.red == 0x3C && sobj->sprite.green == 0x73 && sobj->sprite.blue == 0xB4);
            CHECK(sobj->envcolor.r == 0 && sobj->envcolor.b == 0);
        }
        sobj = mode_select_find(2, 1, 0x7AA8);                  /* the logo */
        CHECK(sobj != NULL && sobj->pos.x == 226.0f && sobj->pos.y == 137.0f);

        /* the four labels, red, in one GObj (mnModeSelectMakeLabels) */
        sobj = mode_select_find(3, 1, 0x5570);                  /* 1P GAME */
        CHECK(sobj != NULL && sobj->pos.x == 224.0f && sobj->pos.y == 52.0f);
        CHECK(sobj != NULL && sobj->sprite.red == 0xFF && sobj->sprite.green == 0 && sobj->sprite.blue == 0);
        sobj = mode_select_find(3, 1, 0x57E0);                  /* VS MODE */
        CHECK(sobj != NULL && sobj->pos.x == 183.0f && sobj->pos.y == 89.0f);
        sobj = mode_select_find(3, 1, 0x84F8);                  /* OPTION */
        CHECK(sobj != NULL && sobj->pos.x == 142.0f && sobj->pos.y == 126.0f);
        sobj = mode_select_find(3, 1, 0x5980);                  /* DATA */
        CHECK(sobj != NULL && sobj->pos.x == 102.0f && sobj->pos.y == 163.0f);

        mode_select_check_cursor(0);

        /* one frame's quads: every strip of every SObj on the two
         * links, once, since each sprite here is one rectangle per
         * strip (the tiled bar too: one strip, drawn 96 wide) */
        for (gobj = gGCCommonLinks[2]; gobj != NULL; gobj = gobj->link_next)
        {
            for (sobj = SObjGetStruct(gobj); sobj != NULL; sobj = sobj->next)
            {
                n += sobj->sprite.nbitmaps;
            }
        }
        for (gobj = gGCCommonLinks[3]; gobj != NULL; gobj = gobj->link_next)
        {
            for (sobj = SObjGetStruct(gobj); sobj != NULL; sobj = sobj->next)
            {
                n += sobj->sprite.nbitmaps;
            }
        }
        gModeSelectQuads = n;
        CHECK(n > 8);
        gLBCommonSpriteQuadLogCount = 0;
        gModeSelectSeen[0] = 1;
    }
    if (tic == 4)
    {
        CHECK(gLBCommonSpriteQuadLogCount == gModeSelectQuads);
        /* the bar's middle: its one quad spans 96 game pixels and six
         * tiles of s */
        if (gModeSelectQuads <= LB_SPRITE_QUAD_LOG_MAX)
        {
            int i, found = 0;

            for (i = 0; i < gLBCommonSpriteQuadLogCount; i++)
            {
                const LBCommonSpriteQuad *q = &gLBCommonSpriteQuadLog[i];

                if (q->rect.rxh == 10 * 4 && q->rect.ryh == 37 * 4 && q->rect.rxl == 96 * 4)
                {
                    found++;
                    CHECK_NEAR(q->u1 - q->u0, (96.0f - 10.0f) / 16.0f, 1e-4f);
                }
            }
            CHECK(found == 1);
        }
        /* the first ten tics are deaf: this A goes nowhere */
        pad->button_tap = A_BUTTON;
        gModeSelectSeen[1] = 1;
    }
    /* still here, cursor unmoved */
    if (tic == 11)
    {
        mode_select_check_cursor(0);
        gModeSelectSeen[2] = 1;
    }
    /* the stick, one tic each way with a neutral tic between (which
     * resets the change wait): down to VS MODE; up back to 1P GAME; up
     * again round the top to DATA; down round the bottom to 1P GAME;
     * down to VS MODE; then A */
    if (tic == 12 || tic == 18 || tic == 20)
    {
        pad->stick_range.y = -80;
    }
    if (tic == 14 || tic == 16)
    {
        pad->stick_range.y = 80;
    }
    if (tic == 13)
    {
        mode_select_check_cursor(1);
        gModeSelectSeen[3] = 1;
    }
    if (tic == 15)
    {
        mode_select_check_cursor(0);
    }
    if (tic == 17)
    {
        mode_select_check_cursor(3);
        gModeSelectSeen[4] = 1;
    }
    if (tic == 19)
    {
        mode_select_check_cursor(0);
    }
    if (tic == 21)
    {
        mode_select_check_cursor(1);
        gModeSelectSeen[5] = 1;
    }
    if (tic == 22)
    {
        pad->button_tap = A_BUTTON;
    }
}

static void test_mode_select(void)
{
    /* from the title, as the loop arrives */
    CHECK(gSCManagerSceneData.scene_prev == nSCKindTitle);
    CHECK(gSCManagerSceneData.scene_curr == nSCKindModeSelect);
    memset(gModeSelectSeen, 0, sizeof(gModeSelectSeen));

    gSceneTicHook = mode_select_tic;
    syDmaLoadOverlay(OVERLAY_SUBSYS);
    syDmaLoadOverlay(OVERLAY_MODESELECT);
    mnModeSelectStartScene();
    gSceneTicHook = NULL;

    CHECK(gModeSelectSeen[0] && gModeSelectSeen[1] && gModeSelectSeen[2]);
    CHECK(gModeSelectSeen[3] && gModeSelectSeen[4] && gModeSelectSeen[5]);
    /* A on tic 22 took VS MODE the same tic; the counter reads one past
     * the tic the scene left on (test_title_screen) */
    CHECK((s32)dSYTaskmanUpdateCount == 23);
    CHECK(gSCManagerSceneData.scene_prev == nSCKindModeSelect);
    CHECK(gSCManagerSceneData.scene_curr == nSCKindVSMode);
}

/* ---- the 1P submenu --------------------------------------
 *
 * mn/mn1pmode/mn1pmode.c driven by a scripted pad: the layout on its
 * four DL links, the ten deaf tics, the cursor round the four options
 * both ways (including the wrap at each end), and A on 1P GAME asking
 * for the 1P character select. The sprites are found the mode select's
 * way; file 0 is mncommon.spr and file 1 is mn1p.spr.
 */
extern s32 sMN1PModeOption;
extern void *sMN1PModeFiles[];

static SObj *one_p_mode_find(s32 link, int file, u32 offset)
{
    return scene_sprite_find(sMN1PModeFiles, link, file, offset);
}

/* Each option's tab: how many sprites carry its status colours (the two
 * full-size tabs are three pieces, the two bonus ones are one), and the
 * GObj the maker parked in sMN1PModeOptionGObjs. */
extern GObj *sMN1PModeOptionGObjs[];

static void one_p_mode_check_tab(int option, s32 status)
{
    static const u8 kEnv[3][3] = { { 0, 0, 0 }, { 0x82, 0x00, 0x28 }, { 0, 0, 0 } };
    static const u8 kPrim[3][3] = { { 0x82, 0x82, 0xAA }, { 0xFF, 0x00, 0x28 }, { 0xFF, 0xFF, 0xFF } };
    int count = (option == nMN1PModeOption1PGame || option == nMN1PModeOptionTrainingMode) ? 3 : 1;
    SObj *sobj = SObjGetStruct(sMN1PModeOptionGObjs[option]);
    int i;

    for (i = 0; i < count; i++)
    {
        CHECK(sobj != NULL);
        if (sobj == NULL)
        {
            return;
        }
        CHECK(sobj->envcolor.r == kEnv[status][0] && sobj->envcolor.g == kEnv[status][1] && sobj->envcolor.b == kEnv[status][2]);
        CHECK(sobj->sprite.red == kPrim[status][0] && sobj->sprite.green == kPrim[status][1] && sobj->sprite.blue == kPrim[status][2]);
        sobj = sobj->next;
    }
}

/* the cursor is on `lit`: that tab highlighted, the other three not */
static void one_p_mode_check_cursor(int lit)
{
    int i;

    CHECK(sMN1PModeOption == lit);
    for (i = 0; i < nMN1PModeOptionEnumCount; i++)
    {
        one_p_mode_check_tab(i, (i == lit) ? nMNOptionTabStatusHighlight : nMNOptionTabStatusNot);
    }
}

static int gOnePModeSeen[6];

/* press a jpad direction on this tic: tap and hold together, and the
 * next tic clears both, which is what lets mn1PModeFuncRun's own
 * "stick centred and nothing held" branch zero the change-wait so the
 * following press is heard */
static void one_p_mode_press(SYController *pad, u32 mask)
{
    pad->button_tap = mask;
    pad->button_hold = mask;
}

static void one_p_mode_tic(void)
{
    s32 tic = (s32)dSYTaskmanUpdateCount;
    SYController *pad = &gSYControllerDevices[0];
    SObj *sobj;

    pad->button_tap = 0;
    pad->button_hold = 0;
    pad->stick_range.x = 0;
    pad->stick_range.y = 0;

    /* the layout, a few tics in: the decals on link 2, the labels on
     * link 3, the four option tabs on link 4, and the cursor on 1P GAME
     * because the mode select is not one of the four selects */
    if (tic == 3)
    {
        sobj = one_p_mode_find(2, 0, 0x18000);                  /* the collage */
        CHECK(sobj != NULL);
        if (sobj != NULL)
        {
            CHECK_EQF(sobj->pos.x, 10.0f);
            CHECK_EQF(sobj->pos.y, 10.0f);
            CHECK(!(sobj->sprite.attr & SP_TRANSPARENT));
        }
        sobj = one_p_mode_find(2, 1, 0x50f8);                   /* the dark controller icon */
        CHECK(sobj != NULL);
        if (sobj != NULL)
        {
            CHECK_EQF(sobj->pos.x, 10.0f);
            CHECK_EQF(sobj->pos.y, 10.0f);
            CHECK(sobj->sprite.red == 0x99 && sobj->sprite.green == 0x99 && sobj->sprite.blue == 0x99);
        }
        sobj = one_p_mode_find(3, 0, 0x31f8);                   /* the logo, in the orange panel */
        CHECK(sobj != NULL && sobj->pos.x == 235.0f && sobj->pos.y == 158.0f);
        sobj = one_p_mode_find(3, 1, 0x5338);                   /* 1P */
        CHECK(sobj != NULL && sobj->pos.x == 161.0f && sobj->pos.y == 194.0f);
        sobj = one_p_mode_find(3, 0, 0xd240);                   /* GAME MODE */
        CHECK(sobj != NULL && sobj->pos.x == 188.0f && sobj->pos.y == 88.0f);

        /* the four option labels, black, one per tab GObj on link 4 */
        sobj = one_p_mode_find(4, 1, 0x2a28);                   /* 1P GAME */
        CHECK(sobj != NULL && sobj->pos.x == 161.0f && sobj->pos.y == 46.0f);
        CHECK(sobj != NULL && sobj->sprite.red == 0 && sobj->sprite.green == 0 && sobj->sprite.blue == 0);
        sobj = one_p_mode_find(4, 1, 0x5ac8);                   /* TRAINING MODE */
        CHECK(sobj != NULL && sobj->pos.x == 107.0f && sobj->pos.y == 87.0f);
        sobj = one_p_mode_find(4, 1, 0x5f28);                   /* BONUS 1 PRACTICE */
        CHECK(sobj != NULL && sobj->pos.x == 97.0f && sobj->pos.y == 127.0f);
        sobj = one_p_mode_find(4, 1, 0x6388);                   /* BONUS 2 PRACTICE */
        CHECK(sobj != NULL && sobj->pos.x == 86.0f && sobj->pos.y == 149.0f);

        one_p_mode_check_cursor(nMN1PModeOption1PGame);
        gOnePModeSeen[0] = 1;
    }
    /* the first ten tics are deaf: A here does nothing */
    if (tic == 5)
    {
        one_p_mode_press(pad, A_BUTTON);
    }
    if (tic == 6)
    {
        CHECK(gSCManagerSceneData.scene_curr == nSCKind1PMode);
        one_p_mode_check_cursor(nMN1PModeOption1PGame);
        gOnePModeSeen[1] = 1;
    }
    /* down steps 1P GAME -> TRAINING MODE */
    if (tic == 12)
    {
        one_p_mode_press(pad, D_JPAD);
    }
    if (tic == 14)
    {
        one_p_mode_check_cursor(nMN1PModeOptionTrainingMode);
        gOnePModeSeen[2] = 1;
    }
    /* up steps back, and up again wraps to the bottom */
    if (tic == 16)
    {
        one_p_mode_press(pad, U_JPAD);
    }
    if (tic == 18)
    {
        one_p_mode_check_cursor(nMN1PModeOption1PGame);
        one_p_mode_press(pad, U_JPAD);
    }
    if (tic == 20)
    {
        one_p_mode_check_cursor(nMN1PModeOptionBonus2Practice);
        gOnePModeSeen[3] = 1;
    }
    /* down from the bottom wraps back to the top */
    if (tic == 22)
    {
        one_p_mode_press(pad, D_JPAD);
    }
    if (tic == 24)
    {
        one_p_mode_check_cursor(nMN1PModeOption1PGame);
        gOnePModeSeen[4] = 1;
    }
    /* A on 1P GAME: the tab goes SELECTED on this tic and the scene is
     * asked for, but the load is deferred to the next tic */
    if (tic == 26)
    {
        one_p_mode_press(pad, A_BUTTON);
    }
    if (tic == 27)
    {
        one_p_mode_check_tab(nMN1PModeOption1PGame, nMNOptionTabStatusSelected);
        CHECK(gSCManagerSceneData.scene_prev == nSCKind1PMode);
        CHECK(gSCManagerSceneData.scene_curr == nSCKind1PGamePlayers);
        CHECK(gSCManagerSceneData.player == 0);
        gOnePModeSeen[5] = 1;
    }
}

static void test_1pmode_menu(void)
{
    /* The scenes above run as a chain, each arriving in the state the
     * one before it left; the 1P submenu is not on that chain (it is
     * the mode select's other door), so this is a side excursion and it
     * puts the pair back the way the mode select left it before the VS
     * mode test reads them. */
    s32 keep_prev = gSCManagerSceneData.scene_prev;
    s32 keep_curr = gSCManagerSceneData.scene_curr;

    gSCManagerSceneData.scene_prev = nSCKindModeSelect;
    gSCManagerSceneData.scene_curr = nSCKind1PMode;
    memset(gOnePModeSeen, 0, sizeof(gOnePModeSeen));

    gSceneTicHook = one_p_mode_tic;
    syDmaLoadOverlay(OVERLAY_SUBSYS);
    syDmaLoadOverlay(OVERLAY_1PMODE);
    mn1PModeStartScene();
    gSceneTicHook = NULL;

    CHECK(gOnePModeSeen[0] && gOnePModeSeen[1] && gOnePModeSeen[2]);
    CHECK(gOnePModeSeen[3] && gOnePModeSeen[4] && gOnePModeSeen[5]);
    CHECK(gSCManagerSceneData.scene_prev == nSCKind1PMode);
    CHECK(gSCManagerSceneData.scene_curr == nSCKind1PGamePlayers);

    gSCManagerSceneData.scene_prev = keep_prev;
    gSCManagerSceneData.scene_curr = keep_curr;
}

/* ---- the VS mode -----------------------------------------
 *
 * mn/mnvsmode/mnvsmode.c driven by a scripted pad: the layout it builds
 * from the battle state, the ten deaf tics, the cursor round the four
 * tabs both ways, the rule stepped to a team rule and back (which
 * re-deals the two Marios' costumes), the stock count stepped up, down
 * and round the bottom, and A on VS START saving the settings and
 * asking for the character select. The sprites are found as the mode
 * select's are; the scene's own state is read straight off its
 * globals, which the decomp keeps at file scope. */
extern s32 sMNVSModeCursorIndex;
extern s32 sMNVSModeRule;
extern s32 sMNVSModeTime;
extern s32 sMNVSModeStock;
extern s32 sMNVSModeChangeWait;
extern GObj *sMNVSModeRuleArrowsGObj;
extern GObj *sMNVSModeTimeStockArrowsGObj;
extern GObj *sMNVSModeButtonGObjVSStart;
extern GObj *sMNVSModeButtonGObjRule;
extern GObj *sMNVSModeButtonGObjTimeStock;
extern GObj *sMNVSModeButtonGObjVSOptions;

static SObj *vs_mode_find(s32 link, int file, u32 offset)
{
    return scene_sprite_find(sMNVSModeFiles, link, file, offset);
}

/* a tab's three pieces carry the status colours mnVSModeUpdateButton
 * deals: env and prim per status */
static void vs_mode_check_tab(GObj *gobj, s32 status)
{
    static const u8 kEnv[3][3] = { { 0, 0, 0 }, { 0x82, 0x00, 0x28 }, { 0, 0, 0 } };
    static const u8 kPrim[3][3] = { { 0x82, 0x82, 0xAA }, { 0xFF, 0x00, 0x28 }, { 0xFF, 0xFF, 0xFF } };
    SObj *sobj = SObjGetStruct(gobj);
    int i;

    for (i = 0; i < 3; i++)
    {
        CHECK(sobj != NULL);
        if (sobj == NULL)
        {
            return;
        }
        CHECK(sobj->envcolor.r == kEnv[status][0] && sobj->envcolor.g == kEnv[status][1] && sobj->envcolor.b == kEnv[status][2]);
        CHECK(sobj->sprite.red == kPrim[status][0] && sobj->sprite.green == kPrim[status][1] && sobj->sprite.blue == kPrim[status][2]);
        sobj = sobj->next;
    }
}

/* the cursor is on tab `lit`: that tab highlighted, the other three not */
static void vs_mode_check_cursor(int lit)
{
    GObj *tabs[4];
    int i;

    tabs[0] = sMNVSModeButtonGObjVSStart;
    tabs[1] = sMNVSModeButtonGObjRule;
    tabs[2] = sMNVSModeButtonGObjTimeStock;
    tabs[3] = sMNVSModeButtonGObjVSOptions;
    CHECK(sMNVSModeCursorIndex == lit);
    for (i = 0; i < 4; i++)
    {
        vs_mode_check_tab(tabs[i], (i == lit) ? nMNOptionTabStatusHighlight : nMNOptionTabStatusNot);
    }
}

/* the count beside TIME/STOCK: its digits right to left from x - 11
 * (mnVSModeMakeNumber), white: mnVSModeMakeTimeStockValue's colours are
 * 0x000000FF each, and Sprite.red is a byte */
static void vs_mode_check_number(s32 value, int is_time)
{
    static const u32 kDigits[10] = { 0xD310, 0xD3E0, 0xD4B0, 0xD580, 0xD650, 0xD720, 0xD7F0, 0xD8C0, 0xD990, 0xDA60 };
    s32 x = is_time ? ((value < 10) ? 185 : 190) : ((value < 10) ? 210 : 215);
    s32 v = value;
    int place = 0;

    do
    {
        SObj *sobj = vs_mode_find(5, 0, kDigits[v % 10]);
        int found = 0;

        /* two of a digit (99) are two SObjs on the one GObj */
        for (; sobj != NULL; sobj = sobj->next)
        {
            if (sobj->sprite.bitmap == sprite_bank_get(sMNVSModeFiles[0], kDigits[v % 10])->bitmap &&
                sobj->pos.x == (f32)(x - 11 * (place + 1)) && sobj->pos.y == 116.0F)
            {
                found = 1;
                CHECK(sobj->sprite.red == 0xFF && sobj->sprite.green == 0xFF && sobj->sprite.blue == 0xFF);
            }
        }
        CHECK(found);
        v /= 10;
        place++;
    }
    while (v != 0);
}

static int gVSModeSeen[10];
static s32 gVSModeQuads;

static void vs_mode_tic(void)
{
    s32 tic = (s32)dSYTaskmanUpdateCount;
    SYController *pad = &gSYControllerDevices[0];
    SObj *sobj;
    GObj *gobj;

    pad->button_tap = 0;
    pad->button_hold = 0;
    pad->stick_range.x = 0;
    pad->stick_range.y = 0;

    /* the layout, a few tics in: the background on link 2, the menu
     * name on 3, the tabs on 4, the values on 5 */
    if (tic == 3)
    {
        s32 nbitmaps = 0;

        sobj = vs_mode_find(2, 0, 0x18000);                     /* the collage */
        CHECK(sobj != NULL && sobj->pos.x == 10.0F && sobj->pos.y == 10.0F);
        sobj = vs_mode_find(2, 0, 0x2A30);                      /* the paper, twice */
        CHECK(sobj != NULL && sobj->sprite.red == 0xA0 && sobj->sprite.green == 0x78 && sobj->sprite.blue == 0x14);
        sobj = vs_mode_find(2, 1, 0x5EB0);                      /* the dark console */
        CHECK(sobj != NULL && sobj->sprite.red == 0x99 && sobj->pos.x == 10.0F);

        sobj = vs_mode_find(3, 0, 0x31F8);                      /* the logo */
        CHECK(sobj != NULL && sobj->pos.x == 235.0F && sobj->pos.y == 158.0F && sobj->sprite.red == 0);
        sobj = vs_mode_find(3, 1, 0x6118);                      /* VS */
        CHECK(sobj != NULL && sobj->pos.x == 158.0F && sobj->pos.y == 192.0F);
        sobj = vs_mode_find(3, 0, 0xD240);                      /* GAME MODE */
        CHECK(sobj != NULL && sobj->pos.x == 189.0F && sobj->pos.y == 87.0F);

        sobj = vs_mode_find(4, 1, 0x24C8);                      /* VS START */
        CHECK(sobj != NULL && sobj->pos.x == 153.0F && sobj->pos.y == 36.0F);
        sobj = vs_mode_find(4, 1, 0x2748);                      /* RULE. */
        CHECK(sobj != NULL && sobj->pos.x == 108.0F && sobj->pos.y == 75.0F);
        sobj = vs_mode_find(4, 1, 0x3248);                      /* STOCK., the rule being stock */
        CHECK(sobj != NULL && sobj->pos.x == 106.0F && sobj->pos.y == 114.0F);
        CHECK(vs_mode_find(4, 1, 0x2EC8) == NULL);              /* no TIME. */
        sobj = vs_mode_find(4, 1, 0x3828);                      /* VS OPTIONS */
        CHECK(sobj != NULL && sobj->pos.x == 71.0F && sobj->pos.y == 151.0F);
        /* a tab's middle is tiled sixteen wide to 136 (mnVSModeMakeButton) */
        sobj = vs_mode_find(4, 0, 0x330);
        CHECK(sobj != NULL && sobj->cms == 0 && sobj->masks == 4 && sobj->maskt == 0 && sobj->lrs == 136 && sobj->lrt == 0x1D);
        CHECK(sobj != NULL && sobj->pos.x == 120.0F + 16.0F);   /* VS START's, at x + 16 */

        sobj = vs_mode_find(5, 1, 0x2A80);                      /* STOCK, the value */
        CHECK(sobj != NULL && sobj->pos.x == 183.0F && sobj->pos.y == 78.0F && sobj->sprite.red == 0xFF);
        CHECK(sMNVSModeRule == nMNVSModeRuleStock && sMNVSModeStock == 0 && sMNVSModeTime == 2);
        vs_mode_check_number(1, 0);
        vs_mode_check_cursor(0);

        /* the arrows: RULE's GObj is empty until the cursor reaches it,
         * TIME/STOCK's holds both, hidden */
        CHECK(sMNVSModeRuleArrowsGObj != NULL && SObjGetStruct(sMNVSModeRuleArrowsGObj) == NULL);
        CHECK(sMNVSModeTimeStockArrowsGObj != NULL && sMNVSModeTimeStockArrowsGObj->flags == GOBJ_FLAG_HIDDEN);
        sobj = SObjGetStruct(sMNVSModeTimeStockArrowsGObj);
        CHECK(sobj != NULL && sobj->pos.x == 165.0F && sobj->pos.y == 109.0F && sobj->user_data.s == 0);
        CHECK(sobj != NULL && sobj->next != NULL && sobj->next->pos.x == 230.0F && sobj->next->user_data.s == 1);

        /* what the frame draws: every visible SObj's strips, plus the
         * one fill quad */
        for (gobj = gGCCommonLinks[2]; gobj != NULL; gobj = gobj->link_next)
        {
            if (gobj->flags == GOBJ_FLAG_HIDDEN) continue;
            for (sobj = SObjGetStruct(gobj); sobj != NULL; sobj = sobj->next) nbitmaps += sobj->sprite.nbitmaps;
        }
        for (gobj = gGCCommonLinks[3]; gobj != NULL; gobj = gobj->link_next)
        {
            if (gobj->flags == GOBJ_FLAG_HIDDEN) continue;
            for (sobj = SObjGetStruct(gobj); sobj != NULL; sobj = sobj->next) nbitmaps += sobj->sprite.nbitmaps;
        }
        for (gobj = gGCCommonLinks[4]; gobj != NULL; gobj = gobj->link_next)
        {
            if (gobj->flags == GOBJ_FLAG_HIDDEN) continue;
            for (sobj = SObjGetStruct(gobj); sobj != NULL; sobj = sobj->next) nbitmaps += sobj->sprite.nbitmaps;
        }
        for (gobj = gGCCommonLinks[5]; gobj != NULL; gobj = gobj->link_next)
        {
            if (gobj->flags == GOBJ_FLAG_HIDDEN) continue;
            for (sobj = SObjGetStruct(gobj); sobj != NULL; sobj = sobj->next) nbitmaps += sobj->sprite.nbitmaps;
        }
        gVSModeQuads = nbitmaps + 1;
        gLBCommonSpriteQuadLogCount = 0;
        gVSModeSeen[0] = 1;
    }
    if (tic == 4)
    {
        int i, fills = 0;

        CHECK(gLBCommonSpriteQuadLogCount == gVSModeQuads);
        for (i = 0; i < gLBCommonSpriteQuadLogCount && i < LB_SPRITE_QUAD_LOG_MAX; i++)
        {
            const LBCommonSpriteQuad *q = &gLBCommonSpriteQuadLog[i];

            /* every quad a step past the one before, across all four
             * cameras: the frame's one depth counter */
            CHECK(i == 0 || q->z > gLBCommonSpriteQuadLog[i - 1].z);
            if (q->rect.copy != -1)
            {
                continue;
            }
            /* the paper rectangle behind the menu name
             * (mnVSModeRenderMenuName): after the background's quads,
             * before the menu name's three sprites */
            fills++;
            CHECK_EQF(q->x0, 450.0f);
            CHECK_EQF(q->y0, 286.0f);
            CHECK_EQF(q->x1, 620.0f);
            CHECK_EQF(q->y1, 460.0f);
            CHECK(q->argb == 0xE6A07814);
            CHECK(i > 0 && i < gLBCommonSpriteQuadLogCount - 3);
        }
        CHECK(fills == 1);
        /* the first ten tics are deaf: this A goes nowhere */
        pad->button_tap = A_BUTTON;
        gVSModeSeen[1] = 1;
    }
    if (tic == 11)
    {
        vs_mode_check_cursor(0);
        gVSModeSeen[2] = 1;
    }
    /* down to RULE */
    if (tic == 12)
    {
        pad->stick_range.y = -80;
    }
    if (tic == 13)
    {
        vs_mode_check_cursor(1);
        /* the wait: (160 - 80) / 7 (the 8 more is for leaving
         * TIME/STOCK downward, not for landing on RULE); the neutral
         * stick this tic will clear it */
        CHECK(sMNVSModeChangeWait == 11);
        /* the arrows' thread has put both arrows up by now */
        CHECK(sMNVSModeRuleArrowsGObj->flags == GOBJ_FLAG_NONE);
        sobj = SObjGetStruct(sMNVSModeRuleArrowsGObj);
        CHECK(sobj != NULL && sobj->next != NULL);
        gVSModeSeen[3] = 1;
    }
    /* right: stock -> time team, which deals the two Marios their team
     * colours (mnVSModeSetCostumesAndShades) */
    if (tic == 14)
    {
        pad->stick_range.x = 80;
    }
    if (tic == 15)
    {
        CHECK(sMNVSModeRule == nMNVSModeRuleTimeTeam);
        sobj = vs_mode_find(5, 1, 0x28E0);                      /* TIME */
        CHECK(sobj != NULL && sobj->pos.x == 168.0F);
        sobj = vs_mode_find(5, 1, 0x2C20);                      /* TEAM */
        CHECK(sobj != NULL && sobj->pos.x == 212.0F);
        sobj = vs_mode_find(4, 1, 0x2EC8);                      /* TIME. */
        CHECK(sobj != NULL && sobj->pos.x == 97.0F && sobj->pos.y == 113.0F);
        sobj = vs_mode_find(4, 1, 0x2FC8);                      /* MIN */
        CHECK(sobj != NULL && sobj->pos.x == 197.0F && sobj->pos.y == 120.0F);
        CHECK(vs_mode_find(4, 1, 0x3248) == NULL);
        vs_mode_check_number(2, 1);
        /* P1 on team 0 keeps Mario's team-0 costume, P2 on team 1 takes
         * team[1] = 3; a team rule gives no shades */
        CHECK(gSCManagerTransferBattleState.players[0].costume == 0);
        CHECK(gSCManagerTransferBattleState.players[1].costume == 3);
        CHECK(gSCManagerTransferBattleState.players[0].shade == 0);
        CHECK(gSCManagerTransferBattleState.players[1].shade == 0);
        /* the arrows: rule time team is neither end, both stay */
        sobj = SObjGetStruct(sMNVSModeRuleArrowsGObj);
        CHECK(sobj != NULL && sobj->next != NULL);
        gVSModeSeen[4] = 1;
    }
    /* left: back to stock, and the free-for-all deal: P1 keeps royal 0,
     * P2 gets the next free royal, 1 */
    if (tic == 16)
    {
        pad->stick_range.x = -80;
    }
    if (tic == 17)
    {
        CHECK(sMNVSModeRule == nMNVSModeRuleStock);
        CHECK(vs_mode_find(4, 1, 0x3248) != NULL);
        CHECK(vs_mode_find(4, 1, 0x2EC8) == NULL);
        vs_mode_check_number(1, 0);
        CHECK(gSCManagerTransferBattleState.players[0].costume == 0);
        CHECK(gSCManagerTransferBattleState.players[1].costume == 1);
        gVSModeSeen[5] = 1;
    }
    /* down to TIME/STOCK; its arrows show */
    if (tic == 18)
    {
        pad->stick_range.y = -80;
    }
    if (tic == 19)
    {
        vs_mode_check_cursor(2);
        CHECK(sMNVSModeTimeStockArrowsGObj->flags == GOBJ_FLAG_NONE);
        CHECK(sMNVSModeRuleArrowsGObj->flags == GOBJ_FLAG_HIDDEN);
    }
    /* right: one more stock; left: back; left: round the bottom to 99;
     * right: round the top to 1 */
    if (tic == 20 || tic == 26)
    {
        pad->stick_range.x = 80;
    }
    if (tic == 22 || tic == 24)
    {
        pad->stick_range.x = -80;
    }
    if (tic == 21)
    {
        CHECK(sMNVSModeStock == 1);
        vs_mode_check_number(2, 0);
    }
    if (tic == 23)
    {
        CHECK(sMNVSModeStock == 0);
        vs_mode_check_number(1, 0);
    }
    if (tic == 25)
    {
        CHECK(sMNVSModeStock == 98);
        vs_mode_check_number(99, 0);
        gVSModeSeen[6] = 1;
    }
    if (tic == 27)
    {
        CHECK(sMNVSModeStock == 0);
        vs_mode_check_number(1, 0);
    }
    /* up, up to VS START; up again round the top to VS OPTIONS; down
     * round the bottom to VS START */
    if (tic == 28 || tic == 30 || tic == 32)
    {
        pad->stick_range.y = 80;
    }
    if (tic == 34)
    {
        pad->stick_range.y = -80;
    }
    if (tic == 29)
    {
        vs_mode_check_cursor(1);
    }
    if (tic == 31)
    {
        vs_mode_check_cursor(0);
    }
    if (tic == 33)
    {
        vs_mode_check_cursor(3);
        gVSModeSeen[7] = 1;
    }
    if (tic == 35)
    {
        vs_mode_check_cursor(0);
        gVSModeSeen[8] = 1;
    }
    /* A on VS START */
    if (tic == 36)
    {
        pad->button_tap = A_BUTTON;
    }
    if (tic == 37)
    {
        /* taken: the tab's selected colours, the settings saved, the
         * scene asked for; the load fires this tic */
        vs_mode_check_tab(sMNVSModeButtonGObjVSStart, nMNOptionTabStatusSelected);
        CHECK(gSCManagerSceneData.scene_curr == nSCKindPlayersVS);
        gVSModeSeen[9] = 1;
    }
}

static void test_vs_mode(void)
{
    int i;

    /* from the mode select, as the loop arrives; the battle state is
     * what the last match left, with the game's saved defaults for
     * what the menu shows: a stock match, one stock (the field is one
     * less than the count), two minutes */
    CHECK(gSCManagerSceneData.scene_prev == nSCKindModeSelect);
    CHECK(gSCManagerSceneData.scene_curr == nSCKindVSMode);
    gSCManagerTransferBattleState.game_rules = SCBATTLE_GAMERULE_STOCK;
    gSCManagerTransferBattleState.is_team_battle = FALSE;
    gSCManagerTransferBattleState.stocks = 0;
    gSCManagerTransferBattleState.time_limit = 2;
    for (i = 0; i < GMCOMMON_PLAYERS_MAX; i++)
    {
        gSCManagerTransferBattleState.players[i].costume = 0;
        gSCManagerTransferBattleState.players[i].shade = 1;
    }
    memset(gVSModeSeen, 0, sizeof(gVSModeSeen));
    /* what the scene manager does between scenes (src/dc/scmanager.c):
     * the banks the earlier scene tests loaded go, or the table of
     * eight overflows */
    sprite_bank_release_all();

    gSceneTicHook = vs_mode_tic;
    syDmaLoadOverlay(OVERLAY_SUBSYS);
    syDmaLoadOverlay(OVERLAY_VSMODE);
    mnVSModeStartScene();
    gSceneTicHook = NULL;

    for (i = 0; i < 10; i++)
    {
        CHECK(gVSModeSeen[i]);
    }
    /* A on tic 36 set the exit interrupt; tic 37 fired the load */
    CHECK((s32)dSYTaskmanUpdateCount == 38);
    CHECK(gSCManagerSceneData.scene_prev == nSCKindVSMode);
    CHECK(gSCManagerSceneData.scene_curr == nSCKindPlayersVS);
    /* mnVSModeSaveSettings: what the stick left */
    CHECK(gSCManagerTransferBattleState.game_rules == SCBATTLE_GAMERULE_STOCK);
    CHECK(gSCManagerTransferBattleState.is_team_battle == FALSE);
    CHECK(gSCManagerTransferBattleState.stocks == 0);
    CHECK(gSCManagerTransferBattleState.time_limit == 2);
}

/* back from the VS options (which the scene manager would have bounced
 * straight back, src/dc/scmanager.c): the cursor on VS OPTIONS, and B
 * at the first live tic saving and going back to the mode select */
static void vs_mode_back_tic(void)
{
    s32 tic = (s32)dSYTaskmanUpdateCount;
    SYController *pad = &gSYControllerDevices[0];

    pad->button_tap = 0;
    pad->button_hold = 0;
    pad->stick_range.x = 0;
    pad->stick_range.y = 0;
    if (tic == 3)
    {
        vs_mode_check_cursor(3);
        gVSModeSeen[0] = 1;
    }
    if (tic == 10)
    {
        pad->button_tap = B_BUTTON;
    }
}

static void test_vs_mode_back(void)
{
    gSCManagerSceneData.scene_prev = nSCKindVSOptions;
    gSCManagerSceneData.scene_curr = nSCKindVSMode;
    memset(gVSModeSeen, 0, sizeof(gVSModeSeen));
    sprite_bank_release_all();

    gSceneTicHook = vs_mode_back_tic;
    syDmaLoadOverlay(OVERLAY_SUBSYS);
    syDmaLoadOverlay(OVERLAY_VSMODE);
    mnVSModeStartScene();
    gSceneTicHook = NULL;

    CHECK(gVSModeSeen[0]);
    /* B on tic 10, the first the scene listens on, fires the load at once */
    CHECK((s32)dSYTaskmanUpdateCount == 11);
    CHECK(gSCManagerSceneData.scene_prev == nSCKindVSMode);
    CHECK(gSCManagerSceneData.scene_curr == nSCKindModeSelect);
}

/* ---- the character select --------------------------------
 *
 * mn/mnplayers/mnplayersvs.c driven by pad 0: the layout it builds from
 * two Marios already picked (the wall, the labels, the eight portraits
 * and four locked shadows, the four gates in their palettes, the two
 * badges, names and emblems, the two cursors, the pucks on Mario's
 * portrait, the fighter on the gate, READY TO FIGHT up); then P1's B
 * recalling its puck (the banner goes, START is refused), the puck
 * flying to the cursor and the cursor closing on it, the stick carrying
 * it up onto Mario's portrait, A dropping it (P1 picked again), and
 * START leaving for the stage select thirty tics on with the battle
 * state written. Two pads are plugged in for the scene, as on the
 * target. */
extern MNPlayersSlotVS sMNPlayersVSSlots[GMCOMMON_PLAYERS_MAX];
extern sb32 sMNPlayersVSIsStart;
extern s32 sMNPlayersVSStockValue;

static SObj *players_vs_find(s32 link, int file, u32 offset)
{
    return scene_sprite_find(sMNPlayersVSFiles, link, file, offset);
}

/* the SObj on `link` whose sprite is `offset` of file 0 and whose x is
 * `x`: the four gates are one sprite four times */
static SObj *players_vs_find_at(s32 link, int file, u32 offset, f32 x)
{
    Sprite *sp = sprite_bank_get(sMNPlayersVSFiles[file], offset);
    GObj *gobj;

    for (gobj = gGCCommonLinks[link]; gobj != NULL; gobj = gobj->link_next)
    {
        SObj *sobj = (gobj->obj_kind == 2) ? SObjGetStruct(gobj) : NULL;

        for (; sobj != NULL; sobj = sobj->next)
        {
            if (sp != NULL && sobj->sprite.bitmap == sp->bitmap && sobj->pos.x == x)
            {
                return sobj;
            }
        }
    }
    return NULL;
}

static GObj *players_vs_gobj_of(s32 link, int file, u32 offset)
{
    Sprite *sp = sprite_bank_get(sMNPlayersVSFiles[file], offset);
    GObj *gobj;

    for (gobj = gGCCommonLinks[link]; gobj != NULL; gobj = gobj->link_next)
    {
        SObj *sobj = (gobj->obj_kind == 2) ? SObjGetStruct(gobj) : NULL;

        for (; sobj != NULL; sobj = sobj->next)
        {
            if (sp != NULL && sobj->sprite.bitmap == sp->bitmap)
            {
                return gobj;
            }
        }
    }
    return NULL;
}

static int gPlayersVSSeen[12];

/* The spotlight standing under gate `i`. All four are on SP link 21 with
 * id 0 and nothing else in the scene is (mnPlayersVSMakeSpotlight), so
 * the index the maker left in user_data is what tells them apart -- and
 * it is the same field mnPlayersVSSpotlightProcUpdate reads to find its
 * slot. */
static GObj *spotlight_of(s32 i)
{
    GObj *g;

    for (g = gGCCommonLinks[21]; g != NULL; g = g->link_next)
    {
        if (g->id == 0 && g->user_data.s == i)
        {
            return g;
        }
    }
    return NULL;
}

static s32 gSpotlightFlags;

static void players_vs_tic(void)
{
    s32 tic = (s32)dSYTaskmanUpdateCount;
    SYController *pad = &gSYControllerDevices[0];
    SObj *sobj;
    GObj *gobj;

    pad->button_tap = 0;
    pad->button_hold = 0;
    pad->stick_range.x = 0;
    pad->stick_range.y = 0;

    if (tic == 3)
    {
        static const u32 kGateLUTs[4] = { 0x103F8, 0x10420, 0x113F0, 0x113C8 };
        int i;

        /* the scene made its own particle pools
         * (efParticleInitAll's two GObjs, ~0x5 and ~0x6), so a pose
         * script's effect does not allocate out of the last scene's.
         * The effect pool itself is target-only (efManagerLoadEffectBank
         * returns before it on the host). */
        CHECK(gcFindGObjByID(~0x5) != NULL);
        CHECK(gcFindGObjByID(~0x6) != NULL);

        /* the wall, tiled 64x32 over the 300x220 (mnPlayersVSMakeWallpaper) */
        sobj = players_vs_find(17, 3, 0x440);
        CHECK(sobj != NULL && sobj->pos.x == 10.0F && sobj->pos.y == 10.0F);
        CHECK(sobj != NULL && sobj->cms == G_TX_WRAP && sobj->cmt == G_TX_WRAP && sobj->masks == 6 && sobj->maskt == 5);
        CHECK(sobj != NULL && sobj->lrs == 300 && sobj->lrt == 220);
        /* FREE FOR ALL in gold, BACK, the stock selector (mnPlayersVSMakeLabels) */
        sobj = players_vs_find(25, 4, 0x280);
        CHECK(sobj != NULL && sobj->pos.x == 27.0F && sobj->pos.y == 24.0F);
        CHECK(sobj != NULL && sobj->sprite.red == 0xE3 && sobj->sprite.green == 0xAC && sobj->sprite.blue == 0x04);
        sobj = players_vs_find(25, 0, 0x115C8);
        CHECK(sobj != NULL && sobj->pos.x == 244.0F && sobj->pos.y == 23.0F);
        CHECK(players_vs_find(25, 4, 0x4E0) == NULL);           /* not TEAM BATTLE */
        CHECK(sMNPlayersVSStockValue == 0);
        /* the portraits: Mario's on link 18, the four locked ones as
         * shadows with question marks (mnPlayersVSMakePortrait) */
        CHECK(players_vs_find(18, 5, 0x4728) != NULL);           /* Mario */
        CHECK(players_vs_find(18, 5, 0x6978) == NULL);           /* no Luigi */
        CHECK(players_vs_find(18, 5, 0x20538) != NULL);          /* Luigi's shadow */
        CHECK(players_vs_find(18, 5, 0x1E2E8) != NULL);          /* Captain's shadow */
        CHECK(players_vs_find(18, 5, 0x249D8) != NULL);          /* Purin's shadow */
        CHECK(players_vs_find(18, 5, 0x22788) != NULL);          /* Ness's shadow */
        /* the four gates: one card sprite, each in its player's palette
         * -- P1 and P2 as humans, P3 and P4 empty in the CPU palette
         * (mnPlayersVSSetGateLUT through sprite_bank_lut) */
        for (i = 0; i < 4; i++)
        {
            sobj = players_vs_find_at(22, 0, 0x104B0, (f32)(i * 69 + 22));
            CHECK(sobj != NULL && sobj->pos.y == 126.0F);
            CHECK(sobj != NULL && sobj->sprite.LUT != NULL);
            CHECK(sobj != NULL && sobj->sprite.LUT == sprite_bank_lut(sMNPlayersVSFiles[0], kGateLUTs[i]));
        }
        /* the badges: HMN for P1 and P2, NA for P3 and P4 */
        CHECK(players_vs_find_at(24, 0, 0x6048, 64.0F) != NULL);
        CHECK(players_vs_find_at(24, 0, 0x6048, 133.0F) != NULL);
        CHECK(players_vs_find_at(24, 0, 0x6748, 202.0F) != NULL);
        CHECK(players_vs_find_at(24, 0, 0x6748, 271.0F) != NULL);
        /* the names and emblems (mnPlayersVSMakeNameAndEmblem) */
        sobj = players_vs_find_at(22, 0, 0x1838, 22.0F);
        CHECK(sobj != NULL && sobj->pos.y == 201.0F);
        CHECK(players_vs_find_at(22, 0, 0x1838, 91.0F) != NULL);
        sobj = players_vs_find_at(22, 2, 0x618, 24.0F);
        CHECK(sobj != NULL && sobj->pos.y == 143.0F && sobj->sprite.red == 0x1E);
        /* the cursors: two pads, two hands, pointing from below the
         * gates (mnPlayersVSMakeCursor, then UpdateCursorNoRecall) */
        CHECK(sMNPlayersVSSlots[0].cursor != NULL && sMNPlayersVSSlots[1].cursor != NULL);
        CHECK(sMNPlayersVSSlots[2].cursor == NULL && sMNPlayersVSSlots[3].cursor == NULL);
        sobj = SObjGetStruct(sMNPlayersVSSlots[0].cursor);
        CHECK(sobj != NULL && sobj->pos.x == 40.0F && sobj->pos.y == 170.0F);
        CHECK(sobj != NULL && sobj->sprite.bitmap == sprite_bank_get(sMNPlayersVSFiles[0], 0x6F88)->bitmap);
        CHECK(sobj != NULL && sobj->next != NULL && sobj->next->pos.x == 47.0F && sobj->next->pos.y == 185.0F);
        CHECK(sobj != NULL && sobj->next != NULL && sobj->next->sprite.red == 0xE0 && sobj->next->envcolor.r == 0x5B);
        CHECK(sMNPlayersVSSlots[0].cursor_status == nMNPlayersCursorStatusPointer);
        /* the pucks, both inside Mario's portrait -- centred there by
         * mnPlayersVSCenterPuckInPortrait, then nudged apart by
         * mnPlayersVSPuckAdjustOverlap (a random step, so no exact
         * place) and kept off its edge by PuckAdjustPortraitEdge --
         * and hidden for the first thirty tics (PuckProcUpdate) */
        CHECK(sMNPlayersVSSlots[0].puck != NULL && sMNPlayersVSSlots[0].puck->flags == GOBJ_FLAG_HIDDEN);
        for (i = 0; i < 2; i++)
        {
            sobj = SObjGetStruct(sMNPlayersVSSlots[i].puck);
            CHECK(sobj != NULL && sobj->pos.x + 13.0F >= 70.0F && sobj->pos.x + 13.0F <= 110.0F);
            CHECK(sobj != NULL && sobj->pos.y + 12.0F >= 41.0F && sobj->pos.y + 12.0F <= 74.0F);
        }
        CHECK(sMNPlayersVSSlots[0].is_selected && sMNPlayersVSSlots[0].is_fighter_selected);
        CHECK(sMNPlayersVSSlots[0].fkind == nFTKindMario && sMNPlayersVSSlots[1].fkind == nFTKindMario);
        CHECK(sMNPlayersVSSlots[2].pkind == nFTPlayerKindNot && sMNPlayersVSSlots[3].pkind == nFTPlayerKindNot);
        /* the fighters on the gates: Mario's pack is loaded here, so
         * both stand, at the game's places and scale (mnPlayersVSMakeFighter) */
        CHECK(sMNPlayersVSSlots[0].player != NULL && sMNPlayersVSSlots[1].player != NULL);
        CHECK(sMNPlayersVSSlots[2].player == NULL && sMNPlayersVSSlots[3].player == NULL);
        if (sMNPlayersVSSlots[1].player != NULL)
        {
            DObj *dobj = DObjGetStruct(sMNPlayersVSSlots[1].player);

            CHECK_EQF(dobj->translate.vec.f.x, -410.0f);
            CHECK_EQF(dobj->translate.vec.f.y, -850.0f);
            CHECK_EQF(dobj->scale.vec.f.x, 1.25f);
            CHECK(ftGetStruct(sMNPlayersVSSlots[1].player)->pkind == nFTPlayerKindDemo);
        }
        /* READY TO FIGHT is up (mnPlayersVSReadyProcUpdate) */
        gobj = players_vs_gobj_of(32, 0, 0xF530);
        CHECK(gobj != NULL && gobj->flags == GOBJ_FLAG_NONE);
        /* the four spotlights, at the game's places
         * (mnPlayersVSMakeSpotlight). Both picked slots are locked in
         * and both empty ones have no kind, so every one of them is
         * hidden this tic (mnPlayersVSSpotlightProcUpdate). */
        for (i = 0; i < 4; i++)
        {
            GObj *spot = spotlight_of(i);
            DObj *dobj = (spot != NULL) ? DObjGetStruct(spot) : NULL;

            CHECK(dobj != NULL);
            if (dobj != NULL)
            {
                CHECK_EQF(dobj->translate.vec.f.x, -1250.0f + 840.0f * i);
                CHECK_EQF(dobj->translate.vec.f.y, -850.0f);
                CHECK_EQF(dobj->translate.vec.f.z, 0.0f);
                CHECK(spot->flags == GOBJ_FLAG_HIDDEN);
            }
        }
        gPlayersVSSeen[0] = 1;
    }
    /* B: P1 recalls its puck */
    if (tic == 40)
    {
        pad->button_tap = B_BUTTON;
    }
    if (tic == 41)
    {
        CHECK(sMNPlayersVSSlots[0].is_recalling && !sMNPlayersVSSlots[0].is_fighter_selected);
        CHECK(sMNPlayersVSSlots[0].is_selected == FALSE);
        gPlayersVSSeen[1] = 1;
    }
    if (tic == 42)
    {
        gobj = players_vs_gobj_of(32, 0, 0xF530);
        CHECK(gobj != NULL && gobj->flags == GOBJ_FLAG_HIDDEN);
        gPlayersVSSeen[2] = 1;
    }
    /* START with one fighter placed: refused */
    if (tic == 50)
    {
        pad->button_tap = START_BUTTON;
    }
    if (tic == 51)
    {
        CHECK(sMNPlayersVSIsStart == FALSE);
        gPlayersVSSeen[3] = 1;
    }
    /* the recall done (30 tics): the cursor holds the puck -- pointing,
     * since it is still below the gates (mnPlayersVSUpdateCursorNoRecall)
     * -- and the puck rides it at (+11, -14) (mnPlayersVSPuckProcUpdate) */
    if (tic == 72)
    {
        SObj *puck;

        CHECK(sMNPlayersVSSlots[0].is_recalling == FALSE);
        CHECK(sMNPlayersVSSlots[0].cursor_status == nMNPlayersCursorStatusPointer);
        CHECK(sMNPlayersVSSlots[0].held_player == 0 && sMNPlayersVSSlots[0].holder_player == 0);
        CHECK(sMNPlayersVSSlots[0].is_cursor_adjusting == FALSE);
        sobj = SObjGetStruct(sMNPlayersVSSlots[0].cursor);
        puck = SObjGetStruct(sMNPlayersVSSlots[0].puck);
        CHECK(sobj != NULL && puck != NULL && puck->pos.x == sobj->pos.x + 11.0F && puck->pos.y == sobj->pos.y - 14.0F);
        CHECK(sMNPlayersVSSlots[0].fkind == nFTKindNull);
        gPlayersVSSeen[4] = 1;
    }
    /* the stick up: four pixels a tic, 26 tics, onto Mario's row */
    if (tic >= 75 && tic < 101)
    {
        pad->stick_range.y = 80;
    }
    if (tic == 102)
    {
        sobj = SObjGetStruct(sMNPlayersVSSlots[0].cursor);
        CHECK(sobj != NULL && sobj->pos.y >= 38.0F && sobj->pos.y <= 80.0F);
        CHECK(sobj != NULL && sobj->pos.x >= 46.0F && sobj->pos.x <= 90.0F);
        /* the puck rides the cursor and reads Mario's portrait under it */
        CHECK(sMNPlayersVSSlots[0].fkind == nFTKindMario);
        /* P1's puck is over Mario and not yet dropped, which is the one
         * state the spotlight is for: it blinks, at Mario's size
         * (mnPlayersVSSpotlightProcUpdate's sizes[]). */
        gobj = spotlight_of(0);
        CHECK(gobj != NULL);
        if (gobj != NULL)
        {
            CHECK_EQF(DObjGetStruct(gobj)->scale.vec.f.x, 1.5f);
            CHECK_EQF(DObjGetStruct(gobj)->scale.vec.f.y, 1.5f);
            gSpotlightFlags = (s32)gobj->flags;
        }
        /* and the three that are not: two empty slots and P2, locked in */
        CHECK(spotlight_of(1) != NULL && spotlight_of(1)->flags == GOBJ_FLAG_HIDDEN);
        CHECK(spotlight_of(2) != NULL && spotlight_of(2)->flags == GOBJ_FLAG_HIDDEN);
        gPlayersVSSeen[5] = 1;
    }
    /* the blink is a flag flip every tic, so the next one disagrees */
    if (tic == 103)
    {
        gobj = spotlight_of(0);
        CHECK(gobj != NULL && (s32)gobj->flags != gSpotlightFlags);
        gPlayersVSSeen[9] = 1;
    }
    /* A: dropped */
    if (tic == 105)
    {
        pad->button_tap = A_BUTTON;
    }
    if (tic == 106)
    {
        CHECK(sMNPlayersVSSlots[0].is_selected && sMNPlayersVSSlots[0].is_fighter_selected);
        CHECK(sMNPlayersVSSlots[0].held_player == -1 && sMNPlayersVSSlots[0].holder_player == GMCOMMON_PLAYERS_MAX);
        CHECK(sMNPlayersVSSlots[0].cursor_status == nMNPlayersCursorStatusHover);
        CHECK(sMNPlayersVSSlots[0].flash != NULL);
        gPlayersVSSeen[6] = 1;
    }
    /* START: on, thirty tics to go */
    if (tic == 120)
    {
        pad->button_tap = START_BUTTON;
    }
    if (tic == 121)
    {
        CHECK(sMNPlayersVSIsStart == TRUE);
        gPlayersVSSeen[7] = 1;
    }
    if (tic == 149)
    {
        CHECK(gSCManagerSceneData.scene_curr == nSCKindPlayersVS);
        gPlayersVSSeen[8] = 1;
    }
}

/* ---- Training mode's character select ---------------------
 *
 * mn/mnplayers/mnplayers1ptraining.c, the VS select's twin: one human
 * slot and one CPU slot rather than four free ones. The scene is driven
 * to its layout and then backed out with B, which is
 * mnPlayers1PTrainingBackTo1PMode -- one of the five functions this file
 * has that the VS select does not.
 */
extern s32 sMNPlayers1PTrainingManPlayer;
extern s32 sMNPlayers1PTrainingComPlayer;
extern u16 sMNPlayers1PTrainingFighterMask;
extern void *sMNPlayers1PTrainingFiles[];

static int gOnePTrainingSeen[4];

static void one_p_training_tic(void)
{
    s32 tic = (s32)dSYTaskmanUpdateCount;
    SYController *pad = &gSYControllerDevices[0];

    pad->button_tap = 0;
    pad->button_hold = 0;
    pad->stick_range.x = 0;
    pad->stick_range.y = 0;

    /* mnPlayers1PTrainingInitVars: the human slot is the player the 1P
     * submenu recorded, the CPU slot is the other of the first two, and
     * the roster is the save data's */
    if (tic == 2)
    {
        CHECK(sMNPlayers1PTrainingManPlayer == 0);
        CHECK(sMNPlayers1PTrainingComPlayer == 1);
        CHECK(sMNPlayers1PTrainingFighterMask == gSCManagerBackupData.fighter_mask);
        gOnePTrainingSeen[0] = 1;
    }
    /* the seven banks are up and the eighth file -- the spotlight model
     * -- is deliberately NULL, as the VS select leaves its own */
    if (tic == 3)
    {
        int i, loaded = 0;

        for (i = 0; i < 7; i++)
        {
            if (sMNPlayers1PTrainingFiles[i] != NULL)
            {
                loaded++;
            }
        }
        CHECK(loaded == 7);
        CHECK(sMNPlayers1PTrainingFiles[7] == NULL);
        gOnePTrainingSeen[1] = 1;
    }
    /* B is deaf for the first ten tics, as the 1P submenu's own is */
    if (tic == 5)
    {
        pad->button_tap = B_BUTTON;
    }
    if (tic == 7)
    {
        CHECK(gSCManagerSceneData.scene_curr == nSCKindPlayers1PTraining);
        gOnePTrainingSeen[2] = 1;
    }
    /* and heard after them -- but B has two jobs here and the nearer
     * one wins: with the human's puck already on a portrait, B pulls it
     * back off (mnPlayers1PTrainingRecallPuck) and
     * mnPlayers1PTrainingDetectBack is skipped for as long as the slot
     * is recalling. Whether the puck starts placed depends on what the
     * scene before this one left in gSCManagerTransferBattleState, and
     * main() runs the suite twice, so it is placed on one pass and
     * not the other. Press twice, far enough apart for the recall to
     * finish, and both passes leave.
     *
     * The leaving itself is checked after mnPlayers1PTrainingStartScene
     * returns rather than from a later tic: the B that is heard ends the
     * scene inside its own tic and scManagerRunLoop is gone before the
     * hook would run again. */
    if (tic == 20 || tic == 80)
    {
        pad->button_tap = B_BUTTON;
        gOnePTrainingSeen[3] = 1;
    }
}

/* ---- Training mode itself, src/dc/sc1ptrainingmode.c.
 *
 * The scene is a battle, and the battle scenes above already prove the
 * shape a battle runs in; what this test adds that nothing else has is
 * the pause menu's own state machine -- six rows, each with its own
 * wrap, and a speed setting that throttles the whole scene by dropping
 * tics. Those are pure functions over sSC1PTrainingModeMenu, so they
 * are exercised directly rather than through a scene, which is also
 * what makes the asymmetry below visible.
 */
extern SC1PTrainingModeMenu sSC1PTrainingModeMenu;
extern u8 dSC1PTrainingModeLagIntervals[][2];

static void test_1ptrainingmode_menu(void)
{
    s32 option;
    s32 i;
    s32 ran;

    memset(&sSC1PTrainingModeMenu, 0, sizeof(sSC1PTrainingModeMenu));

    /* ---- sc1PTrainingModeCheckUpdateOptionID: sc1ptrainingmode.c:314-334.
     * A row's left/right scroll, and the two directions are NOT
     * symmetric in the decomp -- left wraps to option_max - 1, right
     * wraps to option_min. With a range that starts at 0 the two happen
     * to agree; they are checked separately anyway, because the port
     * keeps the body and a "tidied" symmetric version would pass a
     * round-trip test and still be wrong for a range starting above 0. */
    option = nSC1PTrainingModeMenuCPStand;
    sSC1PTrainingModeMenu.button_queue = 0;
    CHECK(sc1PTrainingModeCheckUpdateOptionID(&option,
              nSC1PTrainingModeMenuCPEnumStart,
              nSC1PTrainingModeMenuCPEnumCount) == FALSE);
    CHECK(option == nSC1PTrainingModeMenuCPStand);   /* no press, no move */

    sSC1PTrainingModeMenu.button_queue = R_JPAD;
    CHECK(sc1PTrainingModeCheckUpdateOptionID(&option,
              nSC1PTrainingModeMenuCPEnumStart,
              nSC1PTrainingModeMenuCPEnumCount) != FALSE);
    CHECK(option == nSC1PTrainingModeMenuCPWalk);

    /* right off the end wraps to the first */
    option = nSC1PTrainingModeMenuCPEnumCount - 1;
    CHECK(sc1PTrainingModeCheckUpdateOptionID(&option,
              nSC1PTrainingModeMenuCPEnumStart,
              nSC1PTrainingModeMenuCPEnumCount) != FALSE);
    CHECK(option == nSC1PTrainingModeMenuCPEnumStart);

    /* left off the front wraps to the last */
    sSC1PTrainingModeMenu.button_queue = L_JPAD;
    option = nSC1PTrainingModeMenuCPEnumStart;
    CHECK(sc1PTrainingModeCheckUpdateOptionID(&option,
              nSC1PTrainingModeMenuCPEnumStart,
              nSC1PTrainingModeMenuCPEnumCount) != FALSE);
    CHECK(option == nSC1PTrainingModeMenuCPEnumCount - 1);

    /* both at once is a left, because the L_JPAD arm is tested first */
    sSC1PTrainingModeMenu.button_queue = L_JPAD | R_JPAD;
    option = nSC1PTrainingModeMenuCPEvade;
    CHECK(sc1PTrainingModeCheckUpdateOptionID(&option,
              nSC1PTrainingModeMenuCPEnumStart,
              nSC1PTrainingModeMenuCPEnumCount) != FALSE);
    CHECK(option == nSC1PTrainingModeMenuCPWalk);

    /* the ITEM row is the long one: seventeen entries, and it wraps at
     * its own end rather than the CP row's */
    sSC1PTrainingModeMenu.button_queue = L_JPAD;
    option = nSC1PTrainingModeMenuItemEnumStart;
    CHECK(sc1PTrainingModeCheckUpdateOptionID(&option,
              nSC1PTrainingModeMenuItemEnumStart,
              nSC1PTrainingModeMenuItemEnumCount) != FALSE);
    CHECK(option == nSC1PTrainingModeMenuItemPokeBall);

    /* sc1PTrainingModeUpdateMainOption does the same wrap vertically
     * over the six rows, but it is not called here: after moving the
     * option it drives sc1PTrainingModeUpdateCursorPosition and
     * sc1PTrainingModeUpdateScroll, which walk the cursor and the
     * option rows' SObjs, and outside a real scene those are NULL.
     * The wrap itself is the same shape as the one above.
     *
     */
    /* ---- sc1PTrainingModeCheckLagTic: sc1ptrainingmode.c:526-549, the
     * SPEED row. dSC1PTrainingModeLagIntervals is { hold, advance } per
     * setting: FULL is { 0, 0 } and never skips a tic, and the three
     * slow settings skip on the ratio their names claim. TRUE means
     * "this tic does not run".
     *
     * FULL: every tic runs. */
    sSC1PTrainingModeMenu.speed_menu_option = nSC1PTrainingModeMenuSpeedFull;
    sSC1PTrainingModeMenu.lagtic_wait = 0;
    sSC1PTrainingModeMenu.frameadvance_wait = 0;
    for (i = 0; i < 60; i++)
    {
        CHECK(sc1PTrainingModeCheckLagTic() == FALSE);
    }

    /* HALF is { 0, 1 }: one tic runs, the next is skipped, 30 of 60. */
    sSC1PTrainingModeMenu.speed_menu_option = nSC1PTrainingModeMenuSpeedHalf;
    sSC1PTrainingModeMenu.lagtic_wait = 0;
    sSC1PTrainingModeMenu.frameadvance_wait = 0;
    for (i = 0, ran = 0; i < 60; i++)
    {
        if (sc1PTrainingModeCheckLagTic() == FALSE)
        {
            ran++;
        }
    }
    CHECK(ran == 30);

    /* QUARTER is { 0, 3 }: one in four, 15 of 60. */
    sSC1PTrainingModeMenu.speed_menu_option = nSC1PTrainingModeMenuSpeedQuarter;
    sSC1PTrainingModeMenu.lagtic_wait = 0;
    sSC1PTrainingModeMenu.frameadvance_wait = 0;
    for (i = 0, ran = 0; i < 60; i++)
    {
        if (sc1PTrainingModeCheckLagTic() == FALSE)
        {
            ran++;
        }
    }
    CHECK(ran == 15);

    /* 2/3 is { 1, 1 }: the one setting with a hold as well as an
     * advance, two of every three. */
    sSC1PTrainingModeMenu.speed_menu_option = nSC1PTrainingModeMenuSpeed2Thirds;
    sSC1PTrainingModeMenu.lagtic_wait = 0;
    sSC1PTrainingModeMenu.frameadvance_wait = 0;
    for (i = 0, ran = 0; i < 60; i++)
    {
        if (sc1PTrainingModeCheckLagTic() == FALSE)
        {
            ran++;
        }
    }
    CHECK(ran == 40);

    /* the table itself, so a re-export that reordered it is caught here
     * and not only in the counts above */
    CHECK(dSC1PTrainingModeLagIntervals[nSC1PTrainingModeMenuSpeedFull][0] == 0);
    CHECK(dSC1PTrainingModeLagIntervals[nSC1PTrainingModeMenuSpeedFull][1] == 0);
    CHECK(dSC1PTrainingModeLagIntervals[nSC1PTrainingModeMenuSpeed2Thirds][0] == 1);
    CHECK(dSC1PTrainingModeLagIntervals[nSC1PTrainingModeMenuSpeedQuarter][1] == 3);

    /* ---- sc1PTrainingModeUpdateExitOption: sc1ptrainingmode.c:478-490.
     * A on EXIT ends the scene; anything else leaves it alone. It calls
     * syTaskmanSetLoadScene, which the tests around this one have to
     * undo by hand (test_autodemo's own note says why). */
    memset(&gSYControllerDevices, 0, sizeof(gSYControllerDevices));
    gSCManagerSceneData.player = 0;
    CHECK(sc1PTrainingModeUpdateExitOption() == FALSE);

    gSYControllerDevices[0].button_tap = A_BUTTON;
    CHECK(sc1PTrainingModeUpdateExitOption() != FALSE);
    syTaskmanResetBreakLoop();

    /* RESET is the same press with one more effect: exit_or_reset goes
     * to 1, which is what sc1PTrainingModeStartScene's do/while reads to
     * tell a reset from an exit. */
    sSC1PTrainingModeMenu.exit_or_reset = 0;
    CHECK(sc1PTrainingModeUpdateResetOption() != FALSE);
    CHECK(sSC1PTrainingModeMenu.exit_or_reset == 1);
    syTaskmanResetBreakLoop();
    memset(&gSYControllerDevices, 0, sizeof(gSYControllerDevices));
}

static void test_1ptraining_select(void)
{
    /* off the VS chain, as src/dc/mn1pmode.c's own test is: put the pair
     * back before the next test reads them */
    s32 keep_prev = gSCManagerSceneData.scene_prev;
    s32 keep_curr = gSCManagerSceneData.scene_curr;
    s32 keep_player = gSCManagerSceneData.player;
    int i;

    gSCManagerSceneData.scene_prev = nSCKind1PMode;
    gSCManagerSceneData.scene_curr = nSCKindPlayers1PTraining;
    gSCManagerSceneData.player = 0;
    gSCManagerBackupData.fighter_mask = dSCManagerDefaultBackupData.fighter_mask;
    memset(gOnePTrainingSeen, 0, sizeof(gOnePTrainingSeen));
    sprite_bank_release_all();

    gSceneTicHook = one_p_training_tic;
    syDmaLoadOverlay(OVERLAY_BATTLE);
    syDmaLoadOverlay(OVERLAY_SUBSYS);
    syDmaLoadOverlay(OVERLAY_1PTRAINING);
    mnPlayers1PTrainingStartScene();
    gSceneTicHook = NULL;

    for (i = 0; i < 4; i++)
    {
        CHECK(gOnePTrainingSeen[i]);
    }
    /* the B at tic 20 left the scene the way it came in */
    CHECK(gSCManagerSceneData.scene_prev == nSCKindPlayers1PTraining);
    CHECK(gSCManagerSceneData.scene_curr == nSCKind1PMode);

    gSCManagerSceneData.scene_prev = keep_prev;
    gSCManagerSceneData.scene_curr = keep_curr;
    gSCManagerSceneData.player = keep_player;
}

/* ---- the bonus stages' practice select ------------------
 *
 * mn/mnplayers/mnplayers1pbonus.c, the 1P game select's twin with the
 * difficulty and stock rows swapped for a GAME MODE row and a records
 * panel. One human slot, no CPU slot at all. What it has that no other
 * select does is that ONE scene serves both bonus games: the kind that
 * sent it -- nSCKind1PBonus1Players or nSCKind1PBonus2Players -- is the
 * only thing that says which, and every panel that reads a record reads
 * it through sMNPlayers1PBonusBonusKind. So the scene is run twice, once
 * per kind, and what is checked is that the split lands.
 */
extern s32 sMNPlayers1PBonusManPlayer;
extern s32 sMNPlayers1PBonusBonusKind;
extern u16 sMNPlayers1PBonusFighterMask;
extern void *sMNPlayers1PBonusFiles[];
extern sb32 mnPlayers1PBonusCheckBonusComplete(s32 fkind);
extern void mnPlayers1PBonusStartScene(void);

static int gOnePBonusSeen[4];
static s32 gOnePBonusWantKind;

static void one_p_bonus_tic(void)
{
    s32 tic = (s32)dSYTaskmanUpdateCount;
    SYController *pad = &gSYControllerDevices[0];

    pad->button_tap = 0;
    pad->button_hold = 0;
    pad->stick_range.x = 0;
    pad->stick_range.y = 0;

    /* mnPlayers1PBonusInitVars: the human slot is the player the 1P
     * submenu recorded, the roster is the save data's, and the game is
     * read off the scene kind -- 0 for Break the Targets, 1 for Board
     * the Platforms */
    if (tic == 2)
    {
        CHECK(sMNPlayers1PBonusManPlayer == 0);
        CHECK(sMNPlayers1PBonusFighterMask == gSCManagerBackupData.fighter_mask);
        CHECK(sMNPlayers1PBonusBonusKind == gOnePBonusWantKind);
        gOnePBonusSeen[0] = 1;
    }
    /* the ten banks are up and the eleventh file -- the spotlight model
     * -- is deliberately NULL, as both of the other 1P selects leave
     * their own */
    if (tic == 3)
    {
        int i, loaded = 0;

        for (i = 0; i < 10; i++)
        {
            if (sMNPlayers1PBonusFiles[i] != NULL)
            {
                loaded++;
            }
        }
        CHECK(loaded == 10);
        CHECK(sMNPlayers1PBonusFiles[10] == NULL);
        gOnePBonusSeen[1] = 1;
    }
    /* the records panel reads the half of the save record the game
     * picks, and a default save has neither game finished by anybody */
    if (tic == 4)
    {
        s32 f;

        for (f = nFTKindPlayableStart; f <= nFTKindPlayableEnd; f++)
        {
            CHECK(mnPlayers1PBonusCheckBonusComplete(f) == FALSE);
        }
        gSCManagerBackupData.spgame_records[nFTKindMario].bonus1_task_count =
            SCBATTLE_BONUSGAME_TASK_MAX;
        CHECK(mnPlayers1PBonusCheckBonusComplete(nFTKindMario) ==
              (gOnePBonusWantKind == 0));
        gSCManagerBackupData.spgame_records[nFTKindMario].bonus1_task_count = 0;
        gOnePBonusSeen[2] = 1;
    }
    /* B is deaf for the first ten tics, as the 1P submenu's own is
     * (mnPlayers1PBonusDetectBack), and heard after them. As in the
     * training select, the human's puck may already be on a portrait,
     * in which case the first B only recalls it -- so press twice, far
     * enough apart for the recall to finish. */
    if (tic == 5)
    {
        pad->button_tap = B_BUTTON;
    }
    if (tic == 7)
    {
        CHECK(gSCManagerSceneData.scene_curr ==
              ((gOnePBonusWantKind == 0) ? nSCKind1PBonus1Players
                                         : nSCKind1PBonus2Players));
        gOnePBonusSeen[3] = 1;
    }
    if (tic == 20 || tic == 80)
    {
        pad->button_tap = B_BUTTON;
    }
}

static void one_p_bonus_run(s32 scene, s32 want_kind)
{
    int i;

    gSCManagerSceneData.scene_prev = nSCKind1PMode;
    gSCManagerSceneData.scene_curr = scene;
    gSCManagerSceneData.player = 0;
    gSCManagerBackupData.fighter_mask = dSCManagerDefaultBackupData.fighter_mask;
    memset(gOnePBonusSeen, 0, sizeof(gOnePBonusSeen));
    gOnePBonusWantKind = want_kind;
    sprite_bank_release_all();

    gSceneTicHook = one_p_bonus_tic;
    syDmaLoadOverlay(OVERLAY_SUBSYS);
    syDmaLoadOverlay(OVERLAY_BATTLE);
    syDmaLoadOverlay(OVERLAY_1PBONUS);
    mnPlayers1PBonusStartScene();
    gSceneTicHook = NULL;

    for (i = 0; i < 4; i++)
    {
        CHECK(gOnePBonusSeen[i]);
    }
    /* the B at tic 20 left the scene the way it came in, and
     * mnPlayers1PBonusBackTo1PMode names the kind it was serving */
    CHECK(gSCManagerSceneData.scene_prev == scene);
    CHECK(gSCManagerSceneData.scene_curr == nSCKind1PMode);
}

static void test_1pbonus_select(void)
{
    /* off the VS chain, as the training select's own test is: put the
     * three back before the next test reads them */
    s32 keep_prev = gSCManagerSceneData.scene_prev;
    s32 keep_curr = gSCManagerSceneData.scene_curr;
    s32 keep_player = gSCManagerSceneData.player;

    one_p_bonus_run(nSCKind1PBonus1Players, 0);
    one_p_bonus_run(nSCKind1PBonus2Players, 1);

    gSCManagerSceneData.scene_prev = keep_prev;
    gSCManagerSceneData.scene_curr = keep_curr;
    gSCManagerSceneData.player = keep_player;
}

static void test_players_vs(void)
{
    int i;

    /* from the VS mode, as the loop arrives, with the VS options' stage
     * select on; two pads plugged in */
    gSCManagerSceneData.scene_prev = nSCKindVSMode;
    gSCManagerSceneData.scene_curr = nSCKindPlayersVS;
    gSCManagerTransferBattleState.is_stage_select = TRUE;
    /* the second visit's state, not the first's: the game boots this
     * TRUE and mnPlayersVSInitVars then clears every slot and clears
     * the flag, which is a player picking their fighter rather than
     * finding the last one still on the gate. The scripted pad below
     * drives one puck, so it wants the picks that are already there. */
    gSCManagerTransferBattleState.is_reset_players = FALSE;
    /* the absent players have no kind, as the scene itself leaves them
     * (mnPlayersVSSetSceneData); a kind on an empty slot reads as a pick */
    gSCManagerTransferBattleState.players[2].fkind = nFTKindNull;
    gSCManagerTransferBattleState.players[3].fkind = nFTKindNull;
    /* who is on the roster is the save data's, and the
     * assertions below are about the four who are not: the defaults'
     * masks, pinned here because the unlock message really does unlock
     * Luigi (test_message_scene) and this suite is one
     * cartridge. */
    gSCManagerBackupData.fighter_mask = dSCManagerDefaultBackupData.fighter_mask;
    gSCManagerBackupData.unlock_mask = dSCManagerDefaultBackupData.unlock_mask;
    gSCManagerBackupData.characters_fkind = dSCManagerDefaultBackupData.characters_fkind;
    gSYControllerDeviceStatuses[0] = 0;
    gSYControllerDeviceStatuses[1] = 1;
    memset(gPlayersVSSeen, 0, sizeof(gPlayersVSSeen));
    sprite_bank_release_all();

    gSceneTicHook = players_vs_tic;
    syDmaLoadOverlay(OVERLAY_BATTLE);
    syDmaLoadOverlay(OVERLAY_SUBSYS);
    syDmaLoadOverlay(OVERLAY_PLAYERSVS);
    mnPlayersVSStartScene();
    gSceneTicHook = NULL;
    gSYControllerDeviceStatuses[1] = -1;

    for (i = 0; i < 10; i++)
    {
        CHECK(gPlayersVSSeen[i]);
    }
    /* START on tic 120, the load on tic 150 */
    CHECK((s32)dSYTaskmanUpdateCount == 151);
    CHECK(gSCManagerSceneData.scene_prev == nSCKindPlayersVS);
    CHECK(gSCManagerSceneData.scene_curr == nSCKindMaps);
    /* mnPlayersVSSetSceneData: the two humans, their tags and colours,
     * the counts */
    CHECK(gSCManagerTransferBattleState.pl_count == 2 && gSCManagerTransferBattleState.cp_count == 0);
    CHECK(gSCManagerTransferBattleState.players[0].fkind == nFTKindMario);
    CHECK(gSCManagerTransferBattleState.players[1].fkind == nFTKindMario);
    CHECK(gSCManagerTransferBattleState.players[2].pkind == nFTPlayerKindNot);
    CHECK(gSCManagerTransferBattleState.players[0].tag == 0 && gSCManagerTransferBattleState.players[1].tag == 1);
    CHECK(gSCManagerTransferBattleState.players[0].color == 0 && gSCManagerTransferBattleState.players[1].color == 1);
    CHECK(gSCManagerTransferBattleState.players[2].tag == GMCOMMON_PLAYERS_MAX);
    CHECK(gSCManagerTransferBattleState.players[0].is_single_stockicon == FALSE);
    CHECK(gSCManagerTransferBattleState.stocks == 0 && gSCManagerTransferBattleState.game_rules == SCBATTLE_GAMERULE_STOCK);
    /* P2 keeps royal costume 1 from the VS mode's deal; P1's re-pick
     * took the free one, 0 */
    CHECK(gSCManagerTransferBattleState.players[0].costume == 0);
    CHECK(gSCManagerTransferBattleState.players[1].costume == 1);
}

/* ---- the stage select -------------------------------------------------
 *
 * mn/mnmaps/mnmaps.c through src/dc/mnmaps.c: the scene the character
 * select's START asks for. What it has to get right is the grid (ten
 * icons, two rows of five, the tenth RANDOM), the cursor's walk over it
 * with the game's wraps, the name and emblem remade per stage, the
 * preview built from the stage's own pack, and the battle state it
 * leaves -- gSCManagerSceneData.gkind, which is the stage the battle
 * then binds.
 */
static int gMapsSeen[10];

/* two mock stages in the table the scene previews from: Hyrule, where
 * the cursor starts, and Congo Jungle, one step left of it. Only the
 * fields the scene reads are filled -- the model it hangs a DObj tree
 * off and the wallpaper it draws in the preview window. */
static Stage gMockStages[2];

static void maps_mock_stages(void)
{
    memset(gMockStages, 0, sizeof(gMockStages));
    memset(gGRStages, 0, sizeof(gGRStages));
    stage_holds_reset();

    gMockStages[0].model = mock_model;
    gMockStages[0].wp_txr = (pvr_ptr_t)(uintptr_t)1;   /* not NULL: it has one */
    gMockStages[0].wp_texw = 512;
    gMockStages[0].wp_texh = 256;
    gMockStages[0].wp_w = 300;
    gMockStages[0].wp_h = 220;
    gGRStages[nGRKindHyrule] = &gMockStages[0];

    gMockStages[1] = gMockStages[0];
    gMockStages[1].wp_sprite.bitmap = NULL;            /* its own, unmade */
    gGRStages[nGRKindJungle] = &gMockStages[1];
}

/* mnMapsMakeModel's two fix-ups over a made-up merged
 * tree of two roots: the first (layer 0) with a grandchild under joint 1
 * and fifteen more children, so its depth-first 15th and 17th DObjs are
 * joints 14 and 16; the second (Saffron's layer 3) a chain of three. */
#define MAPS_FIX_JOINTS 22
static FPackJoint gMapsFixJoints[MAPS_FIX_JOINTS];
static FPackHeader gMapsFixHd;
static Stage gMapsFixStage;

extern GObj *sMNMapsHeap0LayerGObjs[4];

static void maps_check_fixups(void)
{
    Stage *saved = sMNMapsGroundInfo;
    GObj *saved_layer = sMNMapsHeap0LayerGObjs[0];
    DObj *dobj;
    int k, hidden;

    memset(&gMapsFixStage, 0, sizeof(gMapsFixStage));
    memset(&gMapsFixHd, 0, sizeof(gMapsFixHd));
    for (k = 0; k < MAPS_FIX_JOINTS; k++)
    {
        gMapsFixJoints[k].parent = 0;
        gMapsFixJoints[k].s[0] = gMapsFixJoints[k].s[1] = gMapsFixJoints[k].s[2] = 1.0F;
    }
    gMapsFixJoints[0].parent = -1;
    gMapsFixJoints[2].parent = 1;
    gMapsFixJoints[19].parent = -1;
    gMapsFixJoints[20].parent = 19;
    gMapsFixJoints[21].parent = 20;
    gMapsFixHd.joint_count = MAPS_FIX_JOINTS;
    gMapsFixStage.model.hd = &gMapsFixHd;
    gMapsFixStage.model.joints = gMapsFixJoints;
    sMNMapsGroundInfo = &gMapsFixStage;

    /* Yoshi's Island: depth first from the first root, 1-based, the
     * 15th and 17th are joints 14 and 16 -- j0, j1, j2, j3 ... */
    mnMapsMakeModel(nGRKindYoster, 1);
    dobj = DObjGetStruct(sMNMapsHeap0LayerGObjs[0]);
    for (k = 0, hidden = 0; dobj != NULL; dobj = lbCommonGetTreeDObjNextFromRoot(dobj, DObjGetStruct(sMNMapsHeap0LayerGObjs[0])), k++)
    {
        if (dobj->flags == DOBJ_FLAG_HIDDEN)
        {
            CHECK(k == 14 || k == 16);
            hidden++;
        }
    }
    CHECK(k == 19 && hidden == 2);
    /* and nothing of the second root */
    dobj = DObjGetStruct(sMNMapsHeap0LayerGObjs[0])->sib_next;
    CHECK(dobj != NULL && dobj->flags == 0 && dobj->child->flags == 0 &&
          dobj->child->child->flags == 0);
    gcEjectGObj(sMNMapsHeap0LayerGObjs[0]);
    sMNMapsHeap0LayerGObjs[0] = NULL;

    /* Saffron: the last root's grandchild, and only that */
    mnMapsMakeModel(nGRKindYamabuki, 1);
    dobj = DObjGetStruct(sMNMapsHeap0LayerGObjs[0]);
    CHECK(dobj->flags == 0 && dobj->child->flags == 0 &&
          dobj->child->child->flags == 0);
    dobj = dobj->sib_next;
    CHECK(dobj->flags == 0 && dobj->child->flags == 0 &&
          dobj->child->child->flags == DOBJ_FLAG_HIDDEN);
    gcEjectGObj(sMNMapsHeap0LayerGObjs[0]);
    sMNMapsHeap0LayerGObjs[0] = NULL;

    /* any other stage: nothing hidden */
    mnMapsMakeModel(nGRKindHyrule, 1);
    for (dobj = DObjGetStruct(sMNMapsHeap0LayerGObjs[0]), hidden = 0; dobj != NULL; dobj = dobj->sib_next)
    {
        DObj *d;

        for (d = dobj; d != NULL; d = lbCommonGetTreeDObjNextFromRoot(d, dobj))
        {
            hidden += (d->flags != 0);
        }
    }
    CHECK(hidden == 0);
    gcEjectGObj(sMNMapsHeap0LayerGObjs[0]);
    sMNMapsHeap0LayerGObjs[0] = NULL;

    sMNMapsGroundInfo = saved;
    sMNMapsHeap0LayerGObjs[0] = saved_layer;
}

/* the SObj a GObj's Nth is, for reading the plaque's two sprites */
static SObj *maps_sobj(GObj *gobj, int n)
{
    SObj *sobj = (gobj != NULL) ? SObjGetStruct(gobj) : NULL;

    while (sobj != NULL && n-- > 0)
    {
        sobj = sobj->next;
    }
    return sobj;
}

static void maps_tic(void)
{
    SYController *pad = &gSYControllerDevices[0];
    s32 tic = (s32)dSYTaskmanUpdateCount;

    pad->button_tap = 0;
    pad->button_hold = 0;
    pad->stick_range.x = pad->stick_range.y = 0;

    /* the scene comes up on the stage the VS chain last used
     * (mnMapsInitVars reads maps_vsmode_gkind), which is Hyrule -- slot
     * 2 of the top row. Its icon, name and emblem are the plaque's, and
     * the preview is built from Hyrule's pack. */
    if (tic == 2)
    {
        CHECK(sMNMapsCursorSlot == 2);
        CHECK(mnMapsGetGroundKind(2) == nGRKindHyrule);
        CHECK(sMNMapsGroundInfo == &gMockStages[0]);
        /* mnMapsSetCursorPosition, the top row: 50 a slot, +23 */
        CHECK(SObjGetStruct(sMNMapsCursorGObj)->pos.x == 2 * 50 + 23.0F);
        CHECK(SObjGetStruct(sMNMapsCursorGObj)->pos.y == 23.0F);
        /* the plaque: the emblem first (mnMapsMakeEmblem), then the name
         * (mnMapsMakeName). Hyrule's emblem is Zelda's, its logo offset
         * (3, 17) off the plaque's (189, 124), and the US name sits at
         * (183, 196) whatever the stage. */
        CHECK(maps_sobj(sMNMapsNameLogoGObj, 0)->pos.x == 189.0F + 3.0F);
        CHECK(maps_sobj(sMNMapsNameLogoGObj, 0)->pos.y == 124.0F + 17.0F);
        CHECK(maps_sobj(sMNMapsNameLogoGObj, 1)->pos.x == 183.0F);
        CHECK(maps_sobj(sMNMapsNameLogoGObj, 1)->pos.y == 196.0F);
        gMapsSeen[0] = 1;
    }
    /* the input is deaf for ten tics (mnMapsFuncRun) */
    if (tic == 3)
    {
        maps_check_fixups();
        gMapsSeen[9] = 1;
    }
    if (tic == 5)
    {
        pad->button_tap = A_BUTTON;
    }
    if (tic == 7)
    {
        CHECK(gSCManagerSceneData.scene_curr == nSCKindMaps);
        gMapsSeen[1] = 1;
    }
    /* the stick left, held: slot 2 -> 1, Congo Jungle. The name and
     * emblem are remade, the cursor steps 50 left, and the preview is
     * rebuilt from Congo Jungle's pack -- which is the whole point of
     * the scene. Held for six tics, it moves once: mnMapsFuncRun sets
     * sMNMapsScrollWait to (160 - stick) / 7, 11 tics here, and only
     * counts it down while the stick is off centre -- centring the
     * stick clears the wait outright, so the window bounds a held
     * stick and never a tap. */
    if (tic >= 20 && tic <= 25)
    {
        pad->stick_range.x = -80;
    }
    if (tic == 21)
    {
        CHECK(sMNMapsCursorSlot == 1);
        CHECK(mnMapsGetGroundKind(1) == nGRKindJungle);
        CHECK(sMNMapsGroundInfo == &gMockStages[1]);
        CHECK(SObjGetStruct(sMNMapsCursorGObj)->pos.x == 1 * 50 + 23.0F);
        /* Congo Jungle's emblem is Donkey's, offset (3, 20) */
        CHECK(maps_sobj(sMNMapsNameLogoGObj, 0)->pos.y == 124.0F + 20.0F);
        CHECK(sMNMapsScrollWait == (160 - 80) / 7);
        gMapsSeen[2] = 1;
    }
    if (tic == 26)
    {
        CHECK(sMNMapsCursorSlot == 1);
        gMapsSeen[3] = 1;
    }
    /* down: slot 1 -> 6, Dream Land, which the port has no pack for.
     * The scene shows it all the same, as the game does, and the
     * preview window is the bare tiles. */
    if (tic == 40)
    {
        pad->stick_range.y = -80;
    }
    if (tic == 41)
    {
        CHECK(sMNMapsCursorSlot == 6);
        CHECK(mnMapsGetGroundKind(6) == nGRKindPupupu);
        CHECK(sMNMapsGroundInfo == NULL);
        /* the bottom row: 50 a slot less 250, +23, at y 61 */
        CHECK(SObjGetStruct(sMNMapsCursorGObj)->pos.x == 6 * 50 - 250 + 23.0F);
        CHECK(SObjGetStruct(sMNMapsCursorGObj)->pos.y == 61.0F);
        gMapsSeen[4] = 1;
    }
    /* left from slot 5 wraps to 9, RANDOM: the question mark at
     * (223, 144) and no name (mnMapsMakeNameAndEmblem skips it) */
    if (tic == 60)
    {
        pad->stick_range.x = -80;       /* 6 -> 5 */
    }
    if (tic == 80)
    {
        pad->stick_range.x = -80;       /* 5 -> 9 */
    }
    if (tic == 81)
    {
        CHECK(sMNMapsCursorSlot == 9);
        CHECK(mnMapsGetGroundKind(9) == 0xDE);
        CHECK(maps_sobj(sMNMapsNameLogoGObj, 0)->pos.x == 223.0F);
        CHECK(maps_sobj(sMNMapsNameLogoGObj, 0)->pos.y == 144.0F);
        CHECK(maps_sobj(sMNMapsNameLogoGObj, 1) == NULL);
        gMapsSeen[5] = 1;
    }
    /* right from 9 goes to 5, Yoshi's Island; and left from 0 lands on
     * 3, not 4, because Mushroom Kingdom is locked on a clean save
     * (mnMapsCheckLocked reads gSCManagerBackupData.unlock_mask) */
    if (tic == 100)
    {
        pad->stick_range.x = 80;        /* 9 -> 5 */
    }
    if (tic == 120)
    {
        pad->stick_range.y = 80;        /* 5 -> 0 */
    }
    if (tic == 140)
    {
        pad->stick_range.x = -80;       /* 0 -> 3, skipping locked 4 */
    }
    if (tic == 141)
    {
        CHECK(mnMapsCheckLocked(nGRKindInishie) == TRUE);
        CHECK(sMNMapsCursorSlot == 3);
        gMapsSeen[6] = 1;
    }
    /* back to Congo Jungle -- slot 3 to 2 to 1 -- and B out to the
     * character select */
    if (tic == 160 || tic == 180)
    {
        pad->stick_range.x = -80;
    }
    if (tic == 190)
    {
        CHECK(sMNMapsCursorSlot == 1);
        gMapsSeen[7] = 1;
    }
    /* B goes back to the character select, and the scene ends on it --
     * what it left behind is checked in test_maps below */
    if (tic == 200)
    {
        pad->button_tap = B_BUTTON;
    }
}

/* the second pass: the cursor comes back where the first pass left it,
 * and A takes the battle to that stage */
static void maps_tic2(void)
{
    SYController *pad = &gSYControllerDevices[0];
    s32 tic = (s32)dSYTaskmanUpdateCount;

    pad->button_tap = 0;
    pad->button_hold = 0;
    pad->stick_range.x = pad->stick_range.y = 0;

    if (tic == 2)
    {
        /* mnMapsInitVars: the cursor is where maps_vsmode_gkind says */
        CHECK(sMNMapsCursorSlot == mnMapsGetSlot(nGRKindJungle));
        CHECK(sMNMapsGroundInfo == &gMockStages[1]);
        gMapsSeen[8] = 1;
    }
    if (tic == 20)
    {
        pad->button_tap = A_BUTTON;
    }
}

static void test_maps(void)
{
    int i;

    maps_mock_stages();
    memset(gMapsSeen, 0, sizeof(gMapsSeen));
    sprite_bank_release_all();

    /* from the character select, as the loop arrives, on the stage the
     * VS chain last used */
    gSCManagerBackupData.unlock_mask = 0;
    gSCManagerSceneData.maps_vsmode_gkind = nGRKindHyrule;
    gSCManagerSceneData.scene_prev = nSCKindPlayersVS;
    gSCManagerSceneData.scene_curr = nSCKindMaps;
    gSYControllerDeviceStatuses[0] = 0;

    gSceneTicHook = maps_tic;
    syDmaLoadOverlay(OVERLAY_SUBSYS);
    syDmaLoadOverlay(OVERLAY_MAPS);
    mnMapsStartScene();
    gSceneTicHook = NULL;

    for (i = 0; i < 8; i++)
    {
        CHECK(gMapsSeen[i]);
    }
    CHECK(gMapsSeen[9]);
    /* B backed out to the character select, and mnMapsSaveSceneData2
     * ran on the way even so: the chain remembers the stage the cursor
     * was left on */
    CHECK(gSCManagerSceneData.scene_curr == nSCKindPlayersVS);
    CHECK(gSCManagerSceneData.scene_prev == nSCKindMaps);
    CHECK(gSCManagerSceneData.gkind == nGRKindJungle);
    CHECK(gSCManagerSceneData.maps_vsmode_gkind == nGRKindJungle);

    /* and again, to see the cursor come back and A leave for the battle */
    sprite_bank_release_all();
    gSCManagerSceneData.scene_prev = nSCKindPlayersVS;
    gSCManagerSceneData.scene_curr = nSCKindMaps;

    gSceneTicHook = maps_tic2;
    syDmaLoadOverlay(OVERLAY_SUBSYS);
    syDmaLoadOverlay(OVERLAY_MAPS);
    mnMapsStartScene();
    gSceneTicHook = NULL;

    CHECK(gMapsSeen[8]);
    /* A leaves for the battle, on the stage the cursor was on */
    CHECK(gSCManagerSceneData.scene_curr == nSCKindVSBattle);
    CHECK(gSCManagerSceneData.scene_prev == nSCKindMaps);
    CHECK(gSCManagerSceneData.gkind == nGRKindJungle);

    /* the table is what the battle binds out of, and Congo Jungle is in
     * it. A stage that is not -- there are three -- takes the port's substitution in scVSBattleStartScene,
     * which says so on the log; that arm is the target's to show. */
    CHECK(gGRStages[nGRKindJungle] != NULL);
    CHECK(gGRStages[nGRKindPupupu] == NULL);

    /* the scene acquired a stage for every preview it built
     * -- the cursor was walked over several -- and gave every one of
     * them back when it ended. A preview that forgot its release would
     * keep a stage's bytes for the rest of the run, and this is the
     * only place that would notice. */
    CHECK(gStageAcquires > 1);
    CHECK(gStageHolds == 0);
    CHECK(gStageReleaseUnderflows == 0);

    memset(gGRStages, 0, sizeof(gGRStages));
    stage_holds_reset();
}


/* ---- the VS options screen ------------------------------
 *
 * mn/mnvsmode/mnvsoptions.c driven by pad 0: the layout it builds with
 * the item switch locked (four options, at the y's the decomp gives the
 * four-option case), the ten deaf tics, the cursor round the four both
 * ways, each option stepped by the stick and cycled by A, the handicap
 * writing every player's handicap as it goes, the underline quad landing
 * under the word that is on, and B saving and asking for the VS mode
 * menu. Then again with the item switch unlocked: the fifth option, the
 * four others moved up, and A on it asking for nSCKindVSItemSwitch.
 *
 * The sprites are found as the other menus' are, by the bank sprite an
 * SObj was made from. */
extern void *sMNVSOptionsFiles[2];
extern GObj *sMNVSOptionsOptionGObjs[nMNVSOptionsOptionEnumCount];
extern GObj *sMNVSOptionsDamageGObj;
extern s32 sMNVSOptionsOption;
extern s32 sMNVSOptionsHandicapStatus;
extern s32 sMNVSOptionsTeamAttackStatus;
extern s32 sMNVSOptionsStageSelectStatus;
extern s32 sMNVSOptionsDamage;
extern s32 sMNVSOptionsFirstAvailableOption;
extern s32 sMNVSOptionsLastAvailableOption;
extern sb32 sMNVSOptionsIsHaveItemSwitch;
extern s32 sMNVSOptionsOptionChangeWait;
void mnVSOptionsUnderlineProcDisplay(GObj *gobj);
void mnVSOptionsStartScene(void);

#define VSO_BUBBLE       0x33D8
#define VSO_HANDICAP     0x3690
#define VSO_TEAMATTACK   0x3968
#define VSO_STAGESELECT  0x3CF8
#define VSO_ITEMSWITCH   0x3FC8
#define VSO_DAMAGE       0x4228
#define VSO_VSOPTIONS    0x2668
#define VSO_CONSOLEICON  0x5F60
#define MNC_ONTEXT       0x0B818
#define MNC_OFFTEXT      0x0B958
#define MNC_SLASH        0x0BA28
#define MNC_PERCENT      0x0DB30
#define MNC_AUTOTEXT     0x0DF48
#define MNC_COLLAGE      0x18000
#define MNC_DIGIT(n)     (0x0D310 + (n) * 0xD0)

static int gVSOptionsSeen[12];

static SObj *vs_options_find(s32 link, int file, u32 offset)
{
    return scene_sprite_find(sMNVSOptionsFiles, link, file, offset);
}

/* the option's own GObj: the bubble first, then the label, then whatever
 * toggle words the option has */
static SObj *vs_options_sobj(s32 option, int n)
{
    SObj *sobj = SObjGetStruct(sMNVSOptionsOptionGObjs[option]);

    while (n-- > 0 && sobj != NULL)
    {
        sobj = sobj->next;
    }
    return sobj;
}

/* mnVSOptionsSetOptionSpriteColors: the bubble carries the cursor */
static void vs_options_check_cursor(s32 lit)
{
    static const u8 kEnv[2][3] = { { 0x00, 0x00, 0x00 }, { 0xFA, 0x8C, 0x00 } };
    static const u8 kPrim[2][3] = { { 0x82, 0x82, 0xAA }, { 0xF4, 0xC8, 0x0A } };
    s32 i;

    CHECK(sMNVSOptionsOption == lit);
    for (i = sMNVSOptionsFirstAvailableOption; i <= sMNVSOptionsLastAvailableOption; i++)
    {
        SObj *bubble = vs_options_sobj(i, 0);
        int on = (i == lit);

        CHECK(bubble != NULL);
        if (bubble == NULL)
        {
            continue;
        }
        CHECK(bubble->envcolor.r == kEnv[on][0] && bubble->envcolor.g == kEnv[on][1] &&
              bubble->envcolor.b == kEnv[on][2]);
        CHECK(bubble->sprite.red == kPrim[on][0] && bubble->sprite.green == kPrim[on][1] &&
              bubble->sprite.blue == kPrim[on][2]);
    }
}

/* the word at `n` in the option's chain is the red one and the others grey */
static void vs_options_check_lit(s32 option, int first, int count, int lit)
{
    int i;

    for (i = 0; i < count; i++)
    {
        SObj *sobj = vs_options_sobj(option, first + i);

        CHECK(sobj != NULL);
        if (sobj == NULL)
        {
            continue;
        }
        if (i == lit)
        {
            CHECK(sobj->sprite.red == 0xFF && sobj->sprite.green == 0x00 &&
                  sobj->sprite.blue == 0x28);
        }
        else
        {
            CHECK(sobj->sprite.red == 0x32 && sobj->sprite.green == 0x32 &&
                  sobj->sprite.blue == 0x32);
        }
    }
}

/* the underline the display proc fills, in game pixels: the quad log
 * holds it at LB_SPRITE_SCREEN_SCALE, and the rectangle the decomp
 * writes is a G_CYC_FILL one, whose lower-right the RDP takes as
 * inclusive -- so the quad is one further right and down */
static void vs_options_check_underline(s32 ulx, s32 uly, s32 lrx, s32 lry)
{
    gLBCommonSpriteQuadLogCount = 0;
    mnVSOptionsUnderlineProcDisplay(NULL);
    CHECK(gLBCommonSpriteQuadLogCount == 1);
    if (gLBCommonSpriteQuadLogCount != 1)
    {
        return;
    }
    CHECK(gLBCommonSpriteQuadLog[0].x0 == ulx * 2.0F);
    CHECK(gLBCommonSpriteQuadLog[0].y0 == uly * 2.0F);
    CHECK(gLBCommonSpriteQuadLog[0].x1 == (lrx + 1) * 2.0F);
    CHECK(gLBCommonSpriteQuadLog[0].y1 == (lry + 1) * 2.0F);
    CHECK(gLBCommonSpriteQuadLog[0].argb == 0xFFFF0028);
}

static void vs_options_tic(void)
{
    s32 tic = (s32)dSYTaskmanUpdateCount;
    SYController *pad = &gSYControllerDevices[0];
    s32 i;

    pad->button_tap = 0;
    pad->button_hold = 0;
    pad->stick_range.x = 0;
    pad->stick_range.y = 0;

    /* the layout, before the scene listens to anything */
    if (tic == 3)
    {
        /* the wallpaper on link 0, the label on 1, the decal on 4 */
        CHECK(vs_options_find(2, 0, MNC_COLLAGE) != NULL);
        CHECK(vs_options_find(3, 1, VSO_VSOPTIONS) != NULL);
        CHECK(vs_options_find(6, 1, VSO_CONSOLEICON) != NULL);
        /* four options on link 2, the item switch's absent */
        CHECK(sMNVSOptionsIsHaveItemSwitch == FALSE);
        CHECK(sMNVSOptionsFirstAvailableOption == nMNVSOptionsOptionHandicap);
        CHECK(sMNVSOptionsLastAvailableOption == nMNVSOptionsOptionDamage);
        CHECK(vs_options_find(4, 1, VSO_HANDICAP) != NULL);
        CHECK(vs_options_find(4, 1, VSO_TEAMATTACK) != NULL);
        CHECK(vs_options_find(4, 1, VSO_STAGESELECT) != NULL);
        CHECK(vs_options_find(4, 1, VSO_DAMAGE) != NULL);
        CHECK(vs_options_find(4, 1, VSO_ITEMSWITCH) == NULL);
        CHECK(sMNVSOptionsOptionGObjs[nMNVSOptionsOptionItemSwitch] == NULL);
        /* the four-option y's (mnVSOptionsMake*Option, the FALSE arm) */
        CHECK(vs_options_sobj(nMNVSOptionsOptionHandicap, 0)->pos.y == 65.0F);
        CHECK(vs_options_sobj(nMNVSOptionsOptionTeamAttack, 0)->pos.y == 97.0F);
        CHECK(vs_options_sobj(nMNVSOptionsOptionStageSelect, 0)->pos.y == 129.0F);
        CHECK(vs_options_sobj(nMNVSOptionsOptionDamage, 0)->pos.y == 161.0F);
        /* the handicap's three words, ON / AUTO / OFF, at 191, 221, 257 */
        CHECK(vs_options_sobj(nMNVSOptionsOptionHandicap, 2)->pos.x == 191.0F);
        CHECK(vs_options_sobj(nMNVSOptionsOptionHandicap, 3)->pos.x == 221.0F);
        CHECK(vs_options_sobj(nMNVSOptionsOptionHandicap, 4)->pos.x == 257.0F);
        /* the damage digits: 100 at 187, 198, 209, the percent at 226 */
        CHECK(vs_options_find(4, 0, MNC_PERCENT) != NULL);
        {
            SObj *d = SObjGetStruct(sMNVSOptionsDamageGObj);
            Sprite *zero = sprite_bank_get(sMNVSOptionsFiles[0], MNC_DIGIT(0));
            Sprite *one = sprite_bank_get(sMNVSOptionsFiles[0], MNC_DIGIT(1));

            CHECK(d != NULL && d->next != NULL && d->next->next != NULL);
            CHECK(d->next->next->next == NULL);
            CHECK(d->sprite.bitmap == zero->bitmap && d->pos.x == 209.0F);
            CHECK(d->next->sprite.bitmap == zero->bitmap && d->next->pos.x == 198.0F);
            CHECK(d->next->next->sprite.bitmap == one->bitmap &&
                  d->next->next->pos.x == 187.0F);
            CHECK(d->pos.y == 164.0F);
        }
        /* the cursor starts on the handicap, and every toggle is off */
        vs_options_check_cursor(nMNVSOptionsOptionHandicap);
        vs_options_check_lit(nMNVSOptionsOptionHandicap, 2, 3, 2);
        vs_options_check_lit(nMNVSOptionsOptionTeamAttack, 2, 2, 1);
        vs_options_check_lit(nMNVSOptionsOptionStageSelect, 2, 2, 1);
        /* the underline under OFF, four pixels down as the four-option
         * layout puts it */
        vs_options_check_underline(255, 81, 283, 81);
        gVSOptionsSeen[0] = 1;
    }
    /* deaf for ten tics: A does nothing */
    if (tic == 5)
    {
        pad->button_tap = A_BUTTON;
    }
    if (tic == 8)
    {
        CHECK(sMNVSOptionsHandicapStatus == nSCBattleHandicapOff);
        gVSOptionsSeen[1] = 1;
    }
    /* A cycles the handicap: OFF -> ON, and every player takes the
     * default until it is AUTO */
    if (tic == 10 || tic == 12)
    {
        pad->button_tap = A_BUTTON;
    }
    if (tic == 11)
    {
        CHECK(sMNVSOptionsHandicapStatus == nSCBattleHandicapOn);
        vs_options_check_lit(nMNVSOptionsOptionHandicap, 2, 3, 0);
        vs_options_check_underline(190, 81, 216, 81);
        for (i = 0; i < GMCOMMON_PLAYERS_MAX; i++)
        {
            CHECK(gSCManagerTransferBattleState.players[i].handicap == 9);
        }
        gVSOptionsSeen[2] = 1;
    }
    if (tic == 13)
    {
        CHECK(sMNVSOptionsHandicapStatus == nSCBattleHandicapAuto);
        vs_options_check_lit(nMNVSOptionsOptionHandicap, 2, 3, 1);
        vs_options_check_underline(219, 81, 251, 81);
        for (i = 0; i < GMCOMMON_PLAYERS_MAX; i++)
        {
            CHECK(gSCManagerTransferBattleState.players[i].handicap == 5);
        }
        gVSOptionsSeen[3] = 1;
    }
    /* right steps the handicap towards OFF, left back towards ON */
    if (tic == 14)
    {
        pad->stick_range.x = 80;
    }
    if (tic == 15)
    {
        CHECK(sMNVSOptionsHandicapStatus == nSCBattleHandicapOff);
        gVSOptionsSeen[4] = 1;
    }
    if (tic == 16)
    {
        pad->stick_range.x = -80;
    }
    if (tic == 17)
    {
        CHECK(sMNVSOptionsHandicapStatus == nSCBattleHandicapAuto);
    }
    /* down to TEAM ATTACK, then STAGE SELECT: A turns each on */
    if (tic == 18 || tic == 22)
    {
        pad->stick_range.y = -80;
    }
    if (tic == 20 || tic == 24)
    {
        pad->button_tap = A_BUTTON;
    }
    if (tic == 19)
    {
        vs_options_check_cursor(nMNVSOptionsOptionTeamAttack);
    }
    if (tic == 21)
    {
        CHECK(sMNVSOptionsTeamAttackStatus == nMNOptionTabStatusOn);
        vs_options_check_lit(nMNVSOptionsOptionTeamAttack, 2, 2, 0);
        vs_options_check_underline(213, 113, 239, 113);
        gVSOptionsSeen[5] = 1;
    }
    if (tic == 25)
    {
        vs_options_check_cursor(nMNVSOptionsOptionStageSelect);
        CHECK(sMNVSOptionsStageSelectStatus == nMNOptionTabStatusOn);
        vs_options_check_lit(nMNVSOptionsOptionStageSelect, 2, 2, 0);
        vs_options_check_underline(208, 145, 234, 145);
        gVSOptionsSeen[6] = 1;
    }
    /* down to DAMAGE; left round the bottom to 200, right back to 50 */
    if (tic == 26 || tic == 30)
    {
        pad->stick_range.y = -80;
    }
    if (tic == 28)
    {
        pad->stick_range.x = -80;
    }
    if (tic == 27)
    {
        vs_options_check_cursor(nMNVSOptionsOptionDamage);
        CHECK(sMNVSOptionsDamage == 100);
    }
    if (tic == 29)
    {
        CHECK(sMNVSOptionsDamage == 99);
        /* two digits now, not three */
        {
            SObj *d = SObjGetStruct(sMNVSOptionsDamageGObj);

            CHECK(d != NULL && d->next != NULL && d->next->next == NULL);
        }
        /* the damage option has no underline of its own */
        gLBCommonSpriteQuadLogCount = 0;
        mnVSOptionsUnderlineProcDisplay(NULL);
        CHECK(gLBCommonSpriteQuadLogCount == 0);
        gVSOptionsSeen[7] = 1;
    }
    /* down again wraps to the handicap, up wraps back to the damage */
    if (tic == 31)
    {
        vs_options_check_cursor(nMNVSOptionsOptionHandicap);
    }
    if (tic == 32)
    {
        pad->stick_range.y = 80;
    }
    if (tic == 33)
    {
        vs_options_check_cursor(nMNVSOptionsOptionDamage);
        gVSOptionsSeen[8] = 1;
    }
    /* B saves and goes back to the VS mode menu */
    if (tic == 34)
    {
        pad->button_tap = B_BUTTON;
    }
}

/* the second run: the item switch unlocked. A on it asks for the scene
 * the port does not have. */
static void vs_options_itemswitch_tic(void)
{
    s32 tic = (s32)dSYTaskmanUpdateCount;
    SYController *pad = &gSYControllerDevices[0];

    pad->button_tap = 0;
    pad->button_hold = 0;
    pad->stick_range.x = 0;
    pad->stick_range.y = 0;

    if (tic == 3)
    {
        CHECK(sMNVSOptionsIsHaveItemSwitch == TRUE);
        CHECK(sMNVSOptionsLastAvailableOption == nMNVSOptionsOptionItemSwitch);
        CHECK(vs_options_find(4, 1, VSO_ITEMSWITCH) != NULL);
        /* the five-option y's: everything moves up to make room */
        CHECK(vs_options_sobj(nMNVSOptionsOptionHandicap, 0)->pos.y == 61.0F);
        CHECK(vs_options_sobj(nMNVSOptionsOptionTeamAttack, 0)->pos.y == 90.0F);
        CHECK(vs_options_sobj(nMNVSOptionsOptionStageSelect, 0)->pos.y == 119.0F);
        CHECK(vs_options_sobj(nMNVSOptionsOptionDamage, 0)->pos.y == 148.0F);
        CHECK(vs_options_sobj(nMNVSOptionsOptionItemSwitch, 0)->pos.y == 177.0F);
        CHECK(SObjGetStruct(sMNVSOptionsDamageGObj)->pos.y == 151.0F);
        /* the cursor came back onto the item switch, because that is
         * the scene the manager bounced us off (src/dc/scmanager.c) */
        vs_options_check_cursor(nMNVSOptionsOptionItemSwitch);
        /* and with five options the underlines lose their offset */
        gVSOptionsSeen[9] = 1;
    }
    /* up to DAMAGE and on to STAGE SELECT, then back down to the item
     * switch */
    if (tic == 10)
    {
        pad->stick_range.y = 80;
    }
    if (tic == 11)
    {
        vs_options_check_cursor(nMNVSOptionsOptionDamage);
    }
    if (tic == 12)
    {
        pad->stick_range.y = 80;
    }
    if (tic == 13)
    {
        vs_options_check_cursor(nMNVSOptionsOptionStageSelect);
        vs_options_check_underline(208, 135, 234, 135);
        gVSOptionsSeen[10] = 1;
    }
    /* down twice, back onto the item switch, and A takes it */
    if (tic == 16 || tic == 18)
    {
        pad->stick_range.y = -80;
    }
    if (tic == 20)
    {
        pad->button_tap = A_BUTTON;
    }
    if (tic == 19)
    {
        vs_options_check_cursor(nMNVSOptionsOptionItemSwitch);
        gVSOptionsSeen[11] = 1;
    }
}

static void test_vs_options(void)
{
    int i;

    memset(gVSOptionsSeen, 0, sizeof(gVSOptionsSeen));
    sprite_bank_release_all();

    /* from the VS mode menu, as the loop arrives; the settings are what
     * the last battle left, with the players' handicaps off the default
     * so the handicap arms can be seen writing them */
    gSCManagerBackupData.unlock_mask = 0;
    gSCManagerSceneData.scene_prev = nSCKindVSMode;
    gSCManagerSceneData.scene_curr = nSCKindVSOptions;
    gSCManagerTransferBattleState.handicap = nSCBattleHandicapOff;
    gSCManagerTransferBattleState.is_team_attack = FALSE;
    gSCManagerTransferBattleState.is_stage_select = FALSE;
    gSCManagerTransferBattleState.damage_ratio = 100;
    for (i = 0; i < GMCOMMON_PLAYERS_MAX; i++)
    {
        gSCManagerTransferBattleState.players[i].handicap = 3;
    }

    gSceneTicHook = vs_options_tic;
    syDmaLoadOverlay(OVERLAY_SUBSYS);
    syDmaLoadOverlay(OVERLAY_VSOPTIONS);
    mnVSOptionsStartScene();
    gSceneTicHook = NULL;

    for (i = 0; i < 9; i++)
    {
        CHECK(gVSOptionsSeen[i]);
    }
    /* B on tic 34 fired the load on tic 35 */
    CHECK((s32)dSYTaskmanUpdateCount == 35);
    CHECK(gSCManagerSceneData.scene_curr == nSCKindVSMode);
    CHECK(gSCManagerSceneData.scene_prev == nSCKindVSOptions);
    /* mnVSOptionsSetAllSettings wrote what the sticks left, and the
     * handicap on AUTO left every player's at 5 */
    CHECK(gSCManagerTransferBattleState.handicap == nSCBattleHandicapAuto);
    CHECK(gSCManagerTransferBattleState.is_team_attack == nMNOptionTabStatusOn);
    CHECK(gSCManagerTransferBattleState.is_stage_select == nMNOptionTabStatusOn);
    CHECK(gSCManagerTransferBattleState.damage_ratio == 99);
    for (i = 0; i < GMCOMMON_PLAYERS_MAX; i++)
    {
        CHECK(gSCManagerTransferBattleState.players[i].handicap == 5);
    }

    /* again, with the item switch unlocked and the manager having
     * bounced us back off it */
    sprite_bank_release_all();
    gSCManagerBackupData.unlock_mask = LBBACKUP_UNLOCK_MASK_ITEMSWITCH;
    gSCManagerSceneData.scene_prev = nSCKindVSItemSwitch;
    gSCManagerSceneData.scene_curr = nSCKindVSOptions;

    gSceneTicHook = vs_options_itemswitch_tic;
    syDmaLoadOverlay(OVERLAY_SUBSYS);
    syDmaLoadOverlay(OVERLAY_VSOPTIONS);
    mnVSOptionsStartScene();
    gSceneTicHook = NULL;

    for (i = 9; i < 12; i++)
    {
        CHECK(gVSOptionsSeen[i]);
    }
    /* A on tic 20 fired the load, and the scene asked for the one the
     * port does not have: the manager's default arm brings it back here
     * with the cursor where it was (src/dc/scmanager.c) */
    CHECK((s32)dSYTaskmanUpdateCount == 21);
    CHECK(gSCManagerSceneData.scene_curr == nSCKindVSItemSwitch);
    CHECK(gSCManagerSceneData.scene_prev == nSCKindVSOptions);
    gSCManagerBackupData.unlock_mask = 0;

    /* turning the handicap off is what resets the players' handicaps,
     * and it happens on the way out */
    CHECK(gSCManagerTransferBattleState.handicap == nSCBattleHandicapAuto);
}


/* ---- the VS results screen ----------------------------
 *
 * mn/mnvsmode/mnvsresults.c's screen, driven twice on a battle state
 * this test writes: a two-player stock free-for-all walked the whole
 * length of the screen's schedule, and a four-player one for the
 * layouts that only a full house reaches. The rankings under it are
 * test_results_ranks_the_match's, on the real battle's leftovers; what
 * is checked here is what the screen makes of them. */

#define RES_WALLPAPER   0x0D5C8
#define RES_WINNER      0x0E2A0
#define RES_KOSTEXT     0x00D38
#define RES_PLACETEXT   0x00990
#define RES_PTSTEXT     0x010D8
#define RES_1PARROW     0x049E8
#define RES_2PARROW     0x04B08
#define RES_3PARROW     0x04C28

#define ANN_A           0x005E0
#define ANN_M           0x03980
#define ANN_N           0x03E88
#define ANN_O           0x044B0
#define ANN_R           0x05418
#define ANN_S           0x057F0
#define ANN_W           0x06C00
#define ANN_EXCLAIM     0x07D98

#define TAG_1P          0x00258
#define TAG_2P          0x004F8
#define TAG_CP          0x00CD8

#define DIG_0           0x00068
#define DIG_5           0x003D8
#define DIG_DASH        0x00710

#define DMG_2           0x00500
#define DMG_4           0x008C0

#define GM_FREEFORALL   0x00280

static int gResultsSeen[17];
extern u8 sFTDisplayMainSkyFogAlpha;

/* src/dc/mpshim.c's host stub records what was asked for */
extern s32 gHostLastBGM;

static SObj *results_find(s32 link, int file, u32 offset)
{
    return scene_sprite_find(sMNVSResultsFiles, link, file, offset);
}

/* the GObj a sprite was made on, and how many SObjs hang off it: the
 * header's four (an arrow and a stock icon a side) are the check that
 * the icon came off the fighter manager without a fighter */
static GObj *results_gobj_with(s32 link, int file, u32 offset)
{
    Sprite *sp = sprite_bank_get(sMNVSResultsFiles[file], offset);
    GObj *gobj;

    for (gobj = gGCCommonLinks[link]; gobj != NULL; gobj = gobj->link_next)
    {
        SObj *sobj = (gobj->obj_kind == 2) ? SObjGetStruct(gobj) : NULL;

        for (; sobj != NULL; sobj = sobj->next)
        {
            if (sp != NULL && sobj->sprite.bitmap == sp->bitmap)
            {
                return gobj;
            }
        }
    }
    return NULL;
}

static int results_sobjs(GObj *gobj)
{
    SObj *sobj;
    int n = 0;

    for (sobj = (gobj != NULL) ? SObjGetStruct(gobj) : NULL; sobj != NULL;
         sobj = sobj->next)
    {
        n++;
    }
    return n;
}

/* a fill quad of the frame, by the row it starts on and the colour it
 * is: game pixels doubled, and the score rule's corner one further
 * right and down than the decomp's FILL rectangle names it.
 *
 * The colour is part of the search because three quads share the tint's
 * row. The two black sheets the screen fades in from never leave:
 * mnVSResultsWallpaperTintProcDisplay steps 0xFF down by 5 and ejects
 * when the alpha goes *below* zero, and 255 is exactly 51 steps of 5,
 * so each lands on zero, stops stepping and keeps drawing a fully
 * transparent screen for the rest of the scene. That is the decomp's,
 * and it costs two quads a frame. */
static const LBCommonSpriteQuad *results_fill_quad(f32 y0, u32 argb)
{
    int i;

    for (i = 0; i < gLBCommonSpriteQuadLogCount; i++)
    {
        const LBCommonSpriteQuad *q = &gLBCommonSpriteQuadLog[i];

        if (q->rect.copy == -1 && q->y0 == y0 && q->argb == argb)
        {
            return q;
        }
    }
    return NULL;
}

static void results_tic(void)
{
    s32 tic = (s32)dSYTaskmanUpdateCount;

    /* mnvsresults.c:3362-3364, the wipe in. Its camera is a GObj of its
     * own and stays; the wipe itself is gone by the time the first tic
     * ends, because the host build has no model under it and so no
     * animation, and lbTransitionProcUpdate ejects a GObj whose
     * anim_frame has run out on the first update it gets. That is the
     * eject the target reaches sixty tics later. */
    if (tic == 1)
    {
        GObj *cam = link_find(0, 0x20000002);

        CHECK(cam != NULL);
        if (cam != NULL)
        {
            CObj *cobj = CObjGetStruct(cam);

            CHECK(cobj != NULL);
            if (cobj != NULL)
            {
                CHECK(cobj->projection.persp.fovy == 45.0F);
                CHECK(cobj->projection.persp.aspect == 15.0F / 11.0F);
                /* lbtransition.c:143: 1100 units back along the eye at
                 * half the field of view -- 1100 / tan(22.5 deg) */
                CHECK(fabsf(cobj->vec.eye.z - 2655.63F) < 0.5F);
                CHECK(cobj->viewport.vp.vtrans[0] == (10 + 310) * 2);
            }
        }
        CHECK(link_find(0, 0x20000000) == NULL);
        gResultsSeen[12] = 1;
    }
    /* mnvsresults.c:2427 the emblem's camera and :615 the emblem, made
     * at scene start. The camera is the scene's first that is not a
     * sprite pass: a real perspective view 1800 units back down z over
     * the same 300x220, on DL link 33. The emblem itself is placed at
     * (0, 100, -11000) at scale 25 and does not move until tic 40. */
    if (tic == 1)
    {
        GObj *cam = NULL;
        GObj *emb = link_find(23, 0);
        GObj *g;

        /* Every one of the scene's cameras is id 0 on link 16, so this
         * one is found by the number that is its own: 1800 down z. */
        for (g = gGCCommonLinks[16]; g != NULL; g = g->link_next)
        {
            if (CObjGetStruct(g)->vec.eye.z == 1800.0F)
            {
                cam = g;
            }
        }
        CHECK(cam != NULL);
        if (cam != NULL)
        {
            CObj *cobj = CObjGetStruct(cam);

            CHECK(cobj->vec.at.z == 0.0F);
            CHECK(cobj->vec.up.y == 1.0F);
            CHECK(cobj->viewport.vp.vtrans[0] == (10 + 310) * 2);
            CHECK(cobj->viewport.vp.vscale[1] == (230 - 10) * 2);
        }
        CHECK(emb != NULL);
        if (emb != NULL)
        {
            DObj *dobj = DObjGetStruct(emb);

            CHECK(dobj != NULL);
            CHECK(dobj->translate.vec.f.y == 100.0F);
            CHECK(dobj->translate.vec.f.z == -11000.0F);
            CHECK(dobj->scale.vec.f.x == 25.0F);
            CHECK(dobj->scale.vec.f.y == 25.0F);
        }
        gResultsSeen[13] = 1;
    }
    /* :583 the emblem's own process: from tic 40 it shrinks 0.15 a tic
     * to a floor of 10 and rises 11 a tic to a ceiling of 1000. Read at
     * 41, which is one update after the tic the test is, so 25 - 0.15
     * and 100 + 11; and at 300, by which time both have arrived. */
    if (tic == 41 || tic == 300)
    {
        GObj *emb = link_find(23, 0);

        CHECK(emb != NULL);
        if (emb != NULL)
        {
            DObj *dobj = DObjGetStruct(emb);

            if (tic == 41)
            {
                CHECK(fabsf(dobj->scale.vec.f.x - 24.7F) < 1e-3F);
                CHECK(dobj->translate.vec.f.y == 122.0F);
            }
            else
            {
                CHECK(dobj->scale.vec.f.x == 10.0F);
                CHECK(dobj->scale.vec.f.y == 10.0F);
                CHECK(dobj->translate.vec.f.y == 1000.0F);
            }
        }
        gResultsSeen[tic == 41 ? 14 : 15] = 1;
    }
    /* nothing is drawn for the first eighty tics but the black sheet
     * the screen fades in from */
    if (tic == 3)
    {
        CHECK(results_find(17, 0, RES_WALLPAPER) == NULL);
        CHECK(results_find(22, 2, GM_FREEFORALL) == NULL);
        gResultsSeen[0] = 1;
    }
    /* mnvsresults.c:3235 the wallpaper at tic 80, in the winner's
     * colour: colors[0]'s prim into the SObj's env and its env into the
     * SObj's prim, which is the lerp the display proc programs */
    if (tic == 80)
    {
        SObj *wp = results_find(17, 0, RES_WALLPAPER);

        CHECK(wp != NULL);
        if (wp != NULL)
        {
            CHECK(wp->pos.x == 10.0F);
            CHECK(wp->pos.y == 10.0F);
            CHECK(wp->envcolor.r == 0x5C && wp->envcolor.g == 0x2B &&
                  wp->envcolor.b == 0x27);
            CHECK(wp->sprite.red == 0x98 && wp->sprite.green == 0x6F &&
                  wp->sprite.blue == 0x6C);
        }
        gResultsSeen[1] = 1;
    }
    /* mnvsresults.c:3242 the winner's name, WINS! after it, the label
     * and the tags, all at tic 120. MARIO is drawn at x 30 and the
     * letters step by the widths table; the digits inside the strings
     * are kerning and are not drawn, which is what puts WINS!'s S at
     * 255 and its ! at 280 rather than 254 and 278. */
    if (tic == 120)
    {
        SObj *m = results_find(20, 6, ANN_M);
        SObj *o = results_find(20, 6, ANN_O);
        SObj *w = results_find(20, 6, ANN_W);
        SObj *s = results_find(20, 6, ANN_S);
        SObj *bang = results_find(20, 6, ANN_EXCLAIM);
        SObj *label = results_find(22, 2, GM_FREEFORALL);
        SObj *t1 = results_find(18, 1, TAG_1P);
        SObj *t2 = results_find(18, 1, TAG_2P);

        CHECK(m != NULL && o != NULL && w != NULL && s != NULL && bang != NULL);
        if (m != NULL && o != NULL)
        {
            CHECK(m->pos.x == 30.0F && m->pos.y == 180.0F);
            CHECK(o->pos.x == 138.0F);
            /* colour 0: red env under a white prim */
            CHECK(m->envcolor.r == 0xFF && m->envcolor.g == 0x00);
            CHECK(m->sprite.red == 0xFF && m->sprite.green == 0xFF);
        }
        if (w != NULL && s != NULL && bang != NULL)
        {
            CHECK(w->pos.x == 175.0F);
            CHECK(s->pos.x == 255.0F);
            CHECK(bang->pos.x == 280.0F);
            /* colour 3 */
            CHECK(w->envcolor.r == 0x60 && w->envcolor.b == 0xD4);
        }
        CHECK(label != NULL);
        if (label != NULL)
        {
            CHECK(label->pos.x == 32.0F && label->pos.y == 29.0F);
        }
        /* two players, so the tags take the two-player spots: the
         * winner's at (115, 50) and the loser's at (177, 75) */
        CHECK(t1 != NULL && t2 != NULL);
        if (t1 != NULL && t2 != NULL)
        {
            CHECK(t1->pos.x == 115.0F && t1->pos.y == 50.0F);
            CHECK(t2->pos.x == 177.0F && t2->pos.y == 75.0F);
        }
        /* and the win jingle went on with them */
        CHECK(gHostLastBGM == nSYAudioBGMWinMario);
        gResultsSeen[2] = 1;
    }
    /* the audio thread waits for the jingle to end and puts the results
     * theme on behind the screen (mnVSResultsAudioThreadUpdate) */
    if (tic == 200)
    {
        CHECK(gHostLastBGM == nSYAudioBGMWinMario);
        gHostBGMPlaying = FALSE;
    }
    if (tic == 203)
    {
        CHECK(gHostLastBGM == nSYAudioBGMResults);
        gResultsSeen[3] = 1;
    }
    /* the tint that darkens the wallpaper: nine steps a tic from tic
     * 180, saturating at half, and by then the two fade-in sheets have
     * ejected themselves so it is the frame's only fill quad */
    if (tic == 209)
    {
        gLBCommonSpriteQuadLogCount = 0;
    }
    if (tic == 210)
    {
        const LBCommonSpriteQuad *q = results_fill_quad(20.0F, 0x80000000);

        CHECK(q != NULL);
        if (q != NULL)
        {
            CHECK(q->x0 == 20.0F && q->x1 == 620.0F && q->y1 == 460.0F);
        }
        /* and the two spent fade-in sheets are still there, at zero */
        CHECK(results_fill_quad(20.0F, 0x00000000) != NULL);
        gResultsSeen[4] = 1;
    }
    /* mnvsresults.c:2237 the stock schedule: the places at 210. Two
     * players and the winner takes the WINNER sprite, thirteen pixels
     * left of where a digit would sit; the loser takes the HUD's fat
     * 2. */
    if (tic == 211)
    {
        SObj *ptext = results_find(22, 0, RES_PLACETEXT);
        SObj *winner = results_find(22, 0, RES_WINNER);
        SObj *second = results_find(22, 3, DMG_2);

        CHECK(ptext != NULL && winner != NULL && second != NULL);
        if (ptext != NULL)
        {
            CHECK(ptext->pos.x == 10.0F && ptext->pos.y == 66.0F);
        }
        if (winner != NULL)
        {
            CHECK(winner->pos.x == 137.0F && winner->pos.y == 66.0F);
        }
        if (second != NULL)
        {
            CHECK(second->pos.x == 230.0F && second->pos.y == 66.0F);
        }
        gResultsSeen[5] = 1;
    }
    /* the score rule at 230, ten pixels a tic to 190 wide. Nineteen
     * tics later it has saturated, and its quad is the deviation this
     * scene shares with the VS options' underline: the decomp's FILL
     * rectangle (87, 110, 277, 110) is inclusive at the lower right, so
     * the port draws (87, 110, 278, 111) and the rule keeps its pixel. */
    if (tic == 259)
    {
        gLBCommonSpriteQuadLogCount = 0;
    }
    if (tic == 260)
    {
        const LBCommonSpriteQuad *q = results_fill_quad(220.0F, 0xFFFFFFFF);

        CHECK(q != NULL);
        if (q != NULL)
        {
            CHECK(q->x0 == 174.0F);
            CHECK(q->x1 == 556.0F);
            CHECK(q->y1 == 222.0F);
        }
        /* and the label's rule, the other FILL rectangle on this
         * screen: (32, 42, 282, 44) drawn as (32, 42, 283, 45) */
        q = results_fill_quad(84.0F, 0xFFFFFFFF);
        CHECK(q != NULL);
        if (q != NULL)
        {
            CHECK(q->x0 == 64.0F && q->x1 == 566.0F && q->y1 == 90.0F);
        }
        gResultsSeen[6] = 1;
    }
    /* the column heads at 250: an arrow and a stock icon a player, and
     * the icon is the one thing on this screen the game reaches through
     * a podium fighter (mnVSResultsMakeHeader) */
    if (tic == 251)
    {
        SObj *kos = results_find(22, 0, RES_KOSTEXT);
        SObj *a1 = results_find(22, 0, RES_1PARROW);
        SObj *a2 = results_find(22, 0, RES_2PARROW);
        GObj *header = results_gobj_with(22, 0, RES_1PARROW);

        CHECK(kos != NULL && a1 != NULL && a2 != NULL);
        if (kos != NULL)
        {
            CHECK(kos->pos.x == 26.0F && kos->pos.y == 124.0F);
        }
        if (a1 != NULL && a2 != NULL)
        {
            CHECK(a1->pos.x == 152.0F && a1->pos.y == 49.0F);
            CHECK(a2->pos.x == 232.0F && a2->pos.y == 49.0F);
        }
        CHECK(results_sobjs(header) == 4);
        /* neither player scored, so each column is one zero, at the
         * ones place of its column */
        CHECK(results_find(22, 5, DIG_0) != NULL);
        gResultsSeen[7] = 1;
    }
}

/* the four-player pass: the layouts a full house reaches, and the
 * points row, which a stock match never draws -- it is called here from
 * inside the scene, because what it wants tested is the negative
 * numbers under it and nothing else makes one. */
/* the podium. mnvsresults.c:803-841's tables for four
 * players, by distance id and spot, and :826-832's heights and depths. */
static const f32 kResultsPodiumX4[4][4] =
{
    { -450.0F, -900.0F, -2000.0F, -3000.0F },
    { -150.0F, -350.0F,  -700.0F, -1000.0F },
    {  150.0F,  300.0F,   700.0F,  1000.0F },
    {  400.0F,  800.0F,  1800.0F,  2800.0F }
};
static const f32 kResultsPodiumYZ[4][2] =
{
    { -350.0F,     0.0F },
    { -450.0F, -2000.0F },
    { -700.0F, -5000.0F },
    { -900.0F, -9000.0F }
};

static GObj *results_podium_fighter(s32 player)
{
    s32 spot = mnVSResultsGetSpot(player);
    f32 x = kResultsPodiumX4[mnVSResultsGetPlayerDistanceID(player)][spot];
    GObj *gobj;

    for (gobj = gGCCommonLinks[nGCCommonLinkIDFighter]; gobj != NULL; gobj = gobj->link_next)
    {
        Vec3f *t = &DObjGetStruct(gobj)->translate.vec.f;

        if ((t->x == x) && (t->y == kResultsPodiumYZ[spot][0]) &&
            (t->z == kResultsPodiumYZ[spot][1]))
        {
            return gobj;
        }
    }
    return NULL;
}

static void results_tic_4p(void)
{
    s32 tic = (s32)dSYTaskmanUpdateCount;

    /* the fighters, made at 120: one per player on its spot, a Demo
     * Mario at the menus' Mario scale, the losers turned to face the
     * winner, and fading in 0x16 a tic from the tic after */
    if (tic == 119)
    {
        CHECK(gGCCommonLinks[nGCCommonLinkIDFighter] == NULL);
    }
    if (tic == 120)
    {
        CHECK(gGCCommonLinks[nGCCommonLinkIDFighter] != NULL);
        CHECK(scSubsysFighterDrawLightColorGetAlpha(NULL) == 0x00);
    }
    if (tic == 125)
    {
        GObj *gobj;
        GObj *winner = results_podium_fighter(0);
        s32 i, n = 0;

        for (gobj = gGCCommonLinks[nGCCommonLinkIDFighter]; gobj != NULL; gobj = gobj->link_next)
        {
            n++;
        }
        CHECK(n == 4);
        CHECK(winner != NULL);
        for (i = 0; i < 4; i++)
        {
            GObj *f = results_podium_fighter(i);
            FTStruct *p;
            DObj *d;

            CHECK(f != NULL);
            if ((f == NULL) || (winner == NULL))
            {
                continue;
            }
            p = ftGetStruct(f);
            d = DObjGetStruct(f);
            CHECK(p->pkind == nFTPlayerKindDemo);
            CHECK(p->fkind == nFTKindMario);
            CHECK(d->scale.vec.f.x == dSCSubsysFighterScales[nFTKindMario] &&
                  d->scale.vec.f.y == dSCSubsysFighterScales[nFTKindMario] &&
                  d->scale.vec.f.z == dSCSubsysFighterScales[nFTKindMario]);
            if (i == 0)
            {
                CHECK(d->rotate.vec.f.y == 0.0F);
            }
            else
            {
                DObj *w = DObjGetStruct(winner);

                CHECK(d->rotate.vec.f.y ==
                      syUtilsArcTan2(w->translate.vec.f.x - d->translate.vec.f.x,
                                     w->translate.vec.f.z - d->translate.vec.f.z));
                CHECK(d->rotate.vec.f.y != 0.0F);
            }
        }
        /* five tics of the ramp, and the Demo arm of the draw reads it */
        CHECK(scSubsysFighterDrawLightColorGetAlpha(NULL) == 5 * 0x16);
        if (winner != NULL)
        {
            ftDisplayMainProcDisplay(winner);
            CHECK(sFTDisplayMainSkyFogAlpha == 5 * 0x16);
        }
        gResultsSeen[16] = 1;
    }
    if (tic == 140)
    {
        CHECK(scSubsysFighterDrawLightColorGetAlpha(NULL) == 0xFF);
    }

    if (tic == 120)
    {
        SObj *t1 = results_find(18, 1, TAG_1P);
        SObj *cp = results_find(18, 1, TAG_CP);

        /* four players: each tag over the spot its player would have
         * stood on, and the winner's spot is the second from the left
         * (mnVSResultsGetPlayerDistanceID moves it there) */
        CHECK(t1 != NULL && cp != NULL);
        if (t1 != NULL)
        {
            CHECK(t1->pos.x == 115.0F && t1->pos.y == 50.0F);
        }
        gResultsSeen[8] = 1;
    }
    /* the places, four across: WINNER, then the HUD's fat digits, each
     * fifteen right of its column. (mnVSResultsGetDisplayPlace's other
     * arm -- four players with two of them sharing second, where the
     * last is shown fourth -- is a TIME match's ranking and cannot be
     * reached: a stock match hands out distinct places, one per team
     * eliminated, and the TIME ranking is not ported.) */
    if (tic == 211)
    {
        SObj *winner = results_find(22, 0, RES_WINNER);
        SObj *second = results_find(22, 3, DMG_2);
        SObj *fourth = results_find(22, 3, DMG_4);

        CHECK(winner != NULL && second != NULL && fourth != NULL);
        if (winner != NULL)
        {
            CHECK(winner->pos.x == 117.0F);
        }
        if (second != NULL)
        {
            CHECK(second->pos.x == 170.0F);
        }
        if (fourth != NULL)
        {
            CHECK(fourth->pos.x == 250.0F);
        }
        gResultsSeen[9] = 1;
    }
    if (tic == 251)
    {
        SObj *a3 = results_find(22, 0, RES_3PARROW);
        SObj *five = results_find(22, 5, DIG_5);

        /* four columns at 115, 155, 195 and 235; the arrows sit
         * seventeen right of each and the KOs' ones place twenty-four */
        CHECK(a3 != NULL && five != NULL);
        if (a3 != NULL)
        {
            CHECK(a3->pos.x == 212.0F);
        }
        if (five != NULL)
        {
            CHECK(five->pos.x == 139.0F);
        }
        gResultsSeen[10] = 1;
    }
    /* the points row, by hand: KOs minus falls, and three of the four
     * are negative. mnVSResultsMakeNumber puts the dash eight pixels
     * further right for every digit the number does not have, so a
     * one-digit negative has it at x + 16. */
    if (tic == 300)
    {
        SObj *pts;
        SObj *dash;

        CHECK(mnVSResultsGetPoints(0) == 4);
        CHECK(mnVSResultsGetPoints(1) == -1);
        CHECK(mnVSResultsGetPoints(3) == -4);

        mnVSResultsMakePointsRow();

        pts = results_find(22, 0, RES_PTSTEXT);
        dash = results_find(22, 5, DIG_DASH);

        CHECK(pts != NULL && dash != NULL);
        if (pts != NULL)
        {
            CHECK(pts->pos.x == 26.0F && pts->pos.y == 104.0F);
        }
        if (dash != NULL)
        {
            CHECK(dash->pos.x == 171.0F && dash->pos.y == 107.0F);
        }
        gResultsSeen[11] = 1;
    }
}

static void results_set_player(s32 i, s32 pkind, s32 place, s32 score,
                               s32 falls)
{
    SCPlayerData *pd = &gSCManagerTransferBattleState.players[i];

    pd->pkind = pkind;
    pd->fkind = nFTKindMario;
    pd->costume = 0;
    pd->color = i;
    pd->team = 0;
    pd->place = place;
    pd->score = score;
    pd->falls = falls;
}

static void test_results_screen(void)
{
    s32 i;
    int stops = gHostFGMStopAllCount;
    LBBackupData before;

    memset(gResultsSeen, 0, sizeof(gResultsSeen));
    sprite_bank_release_all();

    /* the screen writes the match into the save data on its
     * way in (mnVSResultsSaveBackup) and reads it back on the way out
     * (the unlock check). Ninety-nine VS matches are behind us, so this
     * one is the hundredth and the item switch is what it buys. */
    gSCManagerBackupData.unlock_mask &= ~LBBACKUP_UNLOCK_MASK_ITEMSWITCH;
    gSCManagerBackupData.vs_itemswitch_battles = 99;
    for (i = 0; i < nLBBackupUnlockEnumCount; i++)
    {
        gSCManagerSceneData.unlock_messages[i] = nLBBackupUnlockEnumCount;
    }
    before = gSCManagerBackupData;

    gSCManagerSceneData.is_reset = FALSE;
    gSCManagerSceneData.scene_curr = nSCKindVSResults;
    gSCManagerTransferBattleState.game_rules = SCBATTLE_GAMERULE_STOCK;
    gSCManagerTransferBattleState.is_team_battle = FALSE;
    gSCManagerTransferBattleState.pl_count = 2;
    gSCManagerTransferBattleState.cp_count = 0;

    for (i = 0; i < GMCOMMON_PLAYERS_MAX; i++)
    {
        results_set_player(i, nFTPlayerKindNot, 0, 0, 0);
    }
    results_set_player(0, nFTPlayerKindMan, 0, 0, 0);
    results_set_player(1, nFTPlayerKindMan, 1, 0, 2);

    gHostBGMPlaying = TRUE;
    gHostLastBGM = -1;
    gSceneFeedStart = 1;
    gSceneTicHook = results_tic;
    syDmaLoadOverlay(OVERLAY_BATTLE);
    syDmaLoadOverlay(OVERLAY_SUBSYS);
    syDmaLoadOverlay(OVERLAY_VSRESULTS);
    mnVSResultsStartScene();
    gSceneTicHook = NULL;
    gSceneFeedStart = 0;

    for (i = 0; i < 8; i++)
    {
        CHECK(gResultsSeen[i]);
    }
    CHECK(gResultsSeen[12]);
    CHECK(gResultsSeen[13]);
    CHECK(gResultsSeen[14]);
    CHECK(gResultsSeen[15]);
    /* out of dLBTransitionDescs' range either way, and said so rather
     * than followed (src/dc/lbtransition.c) */
    CHECK(lbTransitionMakeTransition(-1, 0, 0, NULL, 0, NULL) == NULL);
    CHECK(lbTransitionMakeTransition(11, 0, 0, NULL, 0, NULL) == NULL);
    CHECK((s32)dSYTaskmanUpdateCount == 370);
    /* the hundredth match unlocked something, so the game's next scene
     * is the unlock message and so is the port's
     * (test_message_scene is what runs it) */
    CHECK(gSCManagerSceneData.scene_prev == nSCKindVSResults);
    CHECK(gSCManagerSceneData.scene_curr == nSCKindMessage);
    /* mnvsresults.c:3314: the results screen stops every sound on its
     * way out, which is where a battle's last one would otherwise go */
    CHECK(gHostFGMStopAllCount - stops == 1);

    /* The match went into the records: one more battle, one more towards
     * the item switch, this stage marked as played, and Mario -- both
     * players are Mario -- credited with two games and with meeting
     * himself twice. */
    CHECK(gSCManagerBackupData.vs_total_battles == before.vs_total_battles + 1);
    CHECK(gSCManagerBackupData.vs_itemswitch_battles == 100);
    CHECK(gSCManagerBackupData.ground_mask &
          (1 << gSCManagerTransferBattleState.gkind));
    CHECK(gSCManagerBackupData.vs_records[nFTKindMario].games_played ==
          before.vs_records[nFTKindMario].games_played + 2);
    CHECK(gSCManagerBackupData.vs_records[nFTKindMario].played_against[nFTKindMario] ==
          before.vs_records[nFTKindMario].played_against[nFTKindMario] + 2);
    /* pl_count 2 + cp_count 0, tallied once per player present */
    CHECK(gSCManagerBackupData.vs_records[nFTKindMario].player_count_tally ==
          before.vs_records[nFTKindMario].player_count_tally + 4);
    /* and it was written down: what is in RAM is what a read gets back */
    CHECK(lbBackupIsChecksumValid() != FALSE);
    {
        LBBackupData ram = gSCManagerBackupData;

        memset(&gSCManagerBackupData, 0, sizeof(gSCManagerBackupData));
        CHECK(lbBackupIsSramValid() != FALSE);
        CHECK(memcmp(&gSCManagerBackupData, &ram, sizeof(ram)) == 0);
    }
    /* the hundredth match queued the item switch's message, which is
     * what mn/mncommon/mnmessage.c shows (src/dc/mnmessage.c) */
    CHECK(gSCManagerSceneData.unlock_messages[0] == nLBBackupUnlockItemSwitch);
    CHECK(gSCManagerSceneData.unlock_messages[1] == nLBBackupUnlockEnumCount);

    /* again with four players, two of them computers and two of them
     * tied for second */
    sprite_bank_release_all();
    gSCManagerSceneData.scene_curr = nSCKindVSResults;
    gSCManagerTransferBattleState.pl_count = 2;
    gSCManagerTransferBattleState.cp_count = 2;
    results_set_player(0, nFTPlayerKindMan, 0, 5, 1);
    results_set_player(1, nFTPlayerKindCom, 1, 2, 3);
    results_set_player(2, nFTPlayerKindMan, 2, 1, 3);
    results_set_player(3, nFTPlayerKindCom, 3, 0, 4);

    gHostBGMPlaying = TRUE;
    gSceneFeedStart = 1;
    gSceneTicHook = results_tic_4p;
    syDmaLoadOverlay(OVERLAY_BATTLE);
    syDmaLoadOverlay(OVERLAY_SUBSYS);
    syDmaLoadOverlay(OVERLAY_VSRESULTS);
    mnVSResultsStartScene();
    gSceneTicHook = NULL;
    gSceneFeedStart = 0;

    for (i = 8; i < 12; i++)
    {
        CHECK(gResultsSeen[i]);
    }
    CHECK(gResultsSeen[16]);
    CHECK(mnVSResultsGetWinPlayer() == 0);
    CHECK(mnVSResultsGetDisplayPlace(0) == 1);
    CHECK(mnVSResultsGetDisplayPlace(3) == GMCOMMON_PLAYERS_MAX);
    CHECK((s32)dSYTaskmanUpdateCount == 370);
    /* the item switch is still queued and still not unlocked -- nothing
     * between the two halves ran the message scene -- so this exit is
     * the same one */
    CHECK(gSCManagerSceneData.scene_curr == nSCKindMessage);
    CHECK(gSCManagerSceneData.unlock_messages[0] == nLBBackupUnlockItemSwitch);
}


/* ---- the item switch -------------------------------------
 *
 * The screen behind the VS options' last row. Two halves are worth
 * separating: the mask arithmetic, which is the only logic in the file
 * and is checked by calling it directly, and the scene, which is
 * checked by driving the pad through it.
 *
 * The mask matters more than it looks. `item_toggles` is a bitmask over
 * nITKind*, and the four container items -- egg, capsule, barrel, crate
 * -- have no row on the screen: they are what everything else comes out
 * of, so any toggle on adds all four, and every toggle off zeroes the
 * word outright rather than leaving the containers behind. The red
 * shell has no row either; the green shell's carries it.
 */
#define ITS_LABEL_VSOPTIONS  0x009A8
#define ITS_LABEL_ITEMSWITCH 0x00B20
#define ITS_APPEARANCE(n)    (kItemSwitchRateOffsets[n])
#define ITS_TOGGLE_ON        0x01488
#define ITS_TOGGLE_OFF       0x01568
#define ITS_TOGGLE_SLASH     0x01608
#define ITS_DECAL_BUTTON     0x03430
#define ITS_ITEM_LIST        0x05E60
#define ITS_CURSOR           0x063A8

static const u32 kItemSwitchRateOffsets[6] =
{
    0x00CE8, 0x00EA8, 0x00F98, 0x010D0, 0x011E8, 0x013A8
};

static int gItemSwitchSeen[8];

static SObj *item_switch_find(s32 link, u32 offset)
{
    return scene_sprite_find(sMNVSItemSwitchFiles, link, 0, offset);
}

/* mnVSItemSwitchSetToggleSpriteColors: the live half red, the dead half
 * grey. The row GObj's first SObj is ON, its next OFF. */
static void item_switch_check_toggle(s32 row, s32 on)
{
    SObj *sobj = SObjGetStruct(sMNVSItemSwitchOptionGObjs[row]);

    CHECK(sobj != NULL && sobj->next != NULL);
    if (sobj == NULL || sobj->next == NULL) return;
    if (on)
    {
        CHECK(sobj->sprite.red == 0xFF && sobj->sprite.green == 0x00 &&
              sobj->sprite.blue == 0x28);
        CHECK(sobj->next->sprite.red == 0x32);
    }
    else
    {
        CHECK(sobj->sprite.red == 0x32);
        CHECK(sobj->next->sprite.red == 0xFF && sobj->next->sprite.green == 0x00 &&
              sobj->next->sprite.blue == 0x28);
    }
}

static void item_switch_tic(void)
{
    s32 tic = (s32)dSYTaskmanUpdateCount;
    SYController *pad = &gSYControllerDevices[0];

    pad->button_tap = 0;
    pad->button_hold = 0;
    pad->stick_range.x = 0;
    pad->stick_range.y = 0;

    /* the layout, before the scene listens to anything */
    if (tic == 3)
    {
        s32 i;

        /* the labels and the list on link 1, the cursor on 3, the
         * button decal on 4 */
        CHECK(item_switch_find(3, ITS_LABEL_VSOPTIONS) != NULL);
        CHECK(item_switch_find(3, ITS_LABEL_ITEMSWITCH) != NULL);
        CHECK(item_switch_find(3, ITS_ITEM_LIST) != NULL);
        CHECK(item_switch_find(5, ITS_CURSOR) != NULL);
        CHECK(item_switch_find(6, ITS_DECAL_BUTTON) != NULL);
        /* the rate the battle state came in on, MIDDLE, is the sprite
         * that got made, at the left edge its width asks for */
        CHECK(sMNVSItemSwitchOptionStatuses[0] == nSCBattleItemSwitchMiddle);
        {
            SObj *sobj = item_switch_find(3, ITS_APPEARANCE(nSCBattleItemSwitchMiddle));

            CHECK(sobj != NULL && sobj->pos.x == 244.0F && sobj->pos.y == 49.0F);
            CHECK(sobj != NULL && sobj->sprite.red == 0xFF &&
                  sobj->sprite.green == 0x00 && sobj->sprite.blue == 0x00);
        }
        CHECK(item_switch_find(3, ITS_APPEARANCE(nSCBattleItemSwitchNone)) == NULL);
        /* fifteen toggle rows, read off the mask the scene came in with:
         * the beam sword, the hammer and the green shell on, the rest
         * off (see test_item_switch) */
        for (i = 1; i < 16; i++)
        {
            s32 on = (i == 1 || i == 3 || i == 8);

            CHECK(sMNVSItemSwitchOptionStatuses[i] ==
                  (on ? nMNOptionTabStatusOn : nMNOptionTabStatusOff));
            item_switch_check_toggle(i, on);
        }
        /* the cursor starts on the rate's row, which sits four pixels
         * above where the arithmetic would put it */
        CHECK(sMNVSItemSwitchOptionSelectID == 0);
        CHECK(SObjGetStruct(sMNVSItemSwitchOptionGObjs[1])->pos.x == 244.0F);
        {
            SObj *cursor = item_switch_find(5, ITS_CURSOR);

            CHECK(cursor != NULL && cursor->pos.x == 115.0F && cursor->pos.y == 47.0F);
        }
        gItemSwitchSeen[0] = 1;
    }
    /* the first ten tics are deaf: this A goes nowhere */
    if (tic == 4)
    {
        pad->button_tap = A_BUTTON;
    }
    if (tic == 5)
    {
        CHECK(sMNVSItemSwitchOptionStatuses[0] == nSCBattleItemSwitchMiddle);
        gItemSwitchSeen[1] = 1;
    }
    /* right on the rate's row steps it up, and the sprite is remade */
    if (tic == 12)
    {
        pad->button_hold = R_JPAD;
    }
    if (tic == 13)
    {
        CHECK(sMNVSItemSwitchOptionStatuses[0] == nSCBattleItemSwitchHigh);
        CHECK(item_switch_find(3, ITS_APPEARANCE(nSCBattleItemSwitchHigh)) != NULL);
        CHECK(item_switch_find(3, ITS_APPEARANCE(nSCBattleItemSwitchMiddle)) == NULL);
        gItemSwitchSeen[2] = 1;
    }
    /* up from the first row wraps to the last, and costs eight extra
     * tics of change-wait for landing on an end */
    if (tic == 22)
    {
        pad->button_hold = U_JPAD;
    }
    if (tic == 23)
    {
        SObj *cursor = item_switch_find(5, ITS_CURSOR);

        CHECK(sMNVSItemSwitchOptionSelectID == 15);
        CHECK(cursor != NULL && cursor->pos.y == 15 * 10 + 51);
        gItemSwitchSeen[3] = 1;
    }
    /* down from the last row wraps back to the first */
    if (tic == 40)
    {
        pad->button_hold = D_JPAD;
    }
    if (tic == 41)
    {
        SObj *cursor = item_switch_find(5, ITS_CURSOR);

        CHECK(sMNVSItemSwitchOptionSelectID == 0);
        CHECK(cursor != NULL && cursor->pos.y == 47.0F);
        gItemSwitchSeen[4] = 1;
    }
    /* down once more onto the beam sword's row, which is on; right
     * sets a row off rather than flipping it, so it goes off and a
     * second right leaves it off */
    if (tic == 58)
    {
        pad->button_hold = D_JPAD;
    }
    if (tic == 59)
    {
        CHECK(sMNVSItemSwitchOptionSelectID == 1);
    }
    /* a held direction sets the change-wait to 12 tics
     * (mnCommonSetOptionChangeWait*), and a centred pad clears it again
     * at the top of the next tic -- so one released tic between inputs
     * is what the scene wants, and one is what it gets */
    if (tic == 61)
    {
        pad->button_hold = R_JPAD;
    }
    if (tic == 62)
    {
        CHECK(sMNVSItemSwitchOptionStatuses[1] == nMNOptionTabStatusOff);
        item_switch_check_toggle(1, 0);
        gItemSwitchSeen[5] = 1;
    }
    /* A flips, so the same row goes back on */
    if (tic == 68)
    {
        pad->button_tap = A_BUTTON;
    }
    if (tic == 69)
    {
        CHECK(sMNVSItemSwitchOptionStatuses[1] == nMNOptionTabStatusOn);
        item_switch_check_toggle(1, 1);
        gItemSwitchSeen[6] = 1;
    }
    /* nothing so far has left the scene */
    if (tic == 73)
    {
        CHECK(gSCManagerSceneData.scene_curr == nSCKindVSItemSwitch);
        gItemSwitchSeen[7] = 1;
    }
    /* B saves and leaves, on this tic: mnVSItemSwitchFuncRun sets the
     * scene and calls syTaskmanSetLoadScene in the same arm, so 74 is
     * the last tic the hook sees */
    if (tic == 74)
    {
        pad->button_tap = B_BUTTON;
    }
}

/* the mask rules, called directly */
static void test_item_switch_mask(void)
{
    s32 i;

    /* read out: the three bits set become three rows on */
    gSCManagerTransferBattleState.item_appearance_rate = nSCBattleItemSwitchLow;
    gSCManagerTransferBattleState.item_toggles =
        (1 << nITKindSword) | (1 << nITKindHammer) | (1 << nITKindGShell);
    mnVSItemSwitchGetItemSettings();
    CHECK(sMNVSItemSwitchOptionStatuses[0] == nSCBattleItemSwitchLow);
    for (i = 1; i < 16; i++)
    {
        s32 want = (dMNVSItemSwitchTogglesItemKinds[i] == nITKindSword ||
                    dMNVSItemSwitchTogglesItemKinds[i] == nITKindHammer ||
                    dMNVSItemSwitchTogglesItemKinds[i] == nITKindGShell);

        CHECK(sMNVSItemSwitchOptionStatuses[i] ==
              (want ? nMNOptionTabStatusOn : nMNOptionTabStatusOff));
    }
    CHECK(mnVSItemSwitchCheckAllTogglesOff() == FALSE);

    /* and back: the green shell carries the red one, and anything on
     * carries the four containers, which have no row */
    gSCManagerTransferBattleState.item_toggles = 0;
    mnVSItemSwitchSetItemToggles();
    CHECK(gSCManagerTransferBattleState.item_appearance_rate == nSCBattleItemSwitchLow);
    CHECK(gSCManagerTransferBattleState.item_toggles ==
          ((1 << nITKindSword) | (1 << nITKindHammer) |
           (1 << nITKindGShell) | (1 << nITKindRShell) |
           (1 << nITKindEgg) | (1 << nITKindCapsule) |
           (1 << nITKindTaru) | (1 << nITKindBox)));

    /* every row off is not "the containers only": it is nothing at all */
    for (i = 1; i < 16; i++)
    {
        sMNVSItemSwitchOptionStatuses[i] = nMNOptionTabStatusOff;
    }
    CHECK(mnVSItemSwitchCheckAllTogglesOff() != FALSE);
    mnVSItemSwitchSetItemToggles();
    CHECK(gSCManagerTransferBattleState.item_toggles == 0);
}

static void test_item_switch(void)
{
    s32 i;

    test_item_switch_mask();

    memset(gItemSwitchSeen, 0, sizeof(gItemSwitchSeen));
    sprite_bank_release_all();

    /* from the VS options screen, as the loop arrives, with the item
     * switch unlocked -- which is the only way to get here, and what
     * the unlock message writes */
    gSCManagerBackupData.unlock_mask |= LBBACKUP_UNLOCK_MASK_ITEMSWITCH;
    gSCManagerSceneData.scene_prev = nSCKindVSOptions;
    gSCManagerSceneData.scene_curr = nSCKindVSItemSwitch;
    gSCManagerTransferBattleState.item_appearance_rate = nSCBattleItemSwitchMiddle;
    gSCManagerTransferBattleState.item_toggles =
        (1 << nITKindSword) | (1 << nITKindHammer) | (1 << nITKindGShell);

    gSceneTicHook = item_switch_tic;
    syDmaLoadOverlay(OVERLAY_SUBSYS);
    syDmaLoadOverlay(OVERLAY_ITEMSWITCH);
    mnVSItemSwitchStartScene();
    gSceneTicHook = NULL;

    for (i = 0; i < 8; i++)
    {
        CHECK(gItemSwitchSeen[i]);
    }
    /* B on tic 74, which is the tic that ends the run */
    CHECK((s32)dSYTaskmanUpdateCount == 75);
    CHECK(gSCManagerSceneData.scene_prev == nSCKindVSItemSwitch);
    CHECK(gSCManagerSceneData.scene_curr == nSCKindVSOptions);

    /* what the pad left: the rate one step up, the same three items on,
     * and the containers and the red shell with them */
    CHECK(gSCManagerTransferBattleState.item_appearance_rate == nSCBattleItemSwitchHigh);
    CHECK(gSCManagerTransferBattleState.item_toggles ==
          ((1 << nITKindSword) | (1 << nITKindHammer) |
           (1 << nITKindGShell) | (1 << nITKindRShell) |
           (1 << nITKindEgg) | (1 << nITKindCapsule) |
           (1 << nITKindTaru) | (1 << nITKindBox)));
}

/* ---- the DATA menu ----------------------------------------
 *
 * mn/mndata/mndata.c driven by a scripted pad, twice: once with SOUND
 * TEST locked, which is the two-tab layout every fresh save has, and
 * once with it unlocked, which is the three-tab one. The layout is the
 * one piece of state the whole screen hangs on (mnDataInitVars reads the
 * unlock bit and every maker below reads the answer), so both arms are
 * walked rather than one.
 *
 * The second run also checks the return route the port leans on: the
 * three screens behind this menu are not ported, so mnDataInitVars
 * reading gSCManagerSceneData.scene_prev is what puts the cursor back on
 * the tab that was chosen when the scene manager's default arm bounces
 * (src/dc/mndata.h). Entering with scene_prev == nSCKindSoundTest is
 * exactly that bounce.
 */
extern u32 dMNDataFileIDs[];

static SObj *data_find(s32 link, int file, u32 offset)
{
    return scene_sprite_find(sMNDataFiles, link, file, offset);
}

/* the i'th SObj of an option tab's GObj: 0..2 are the tab's left,
 * middle and right pieces (mnDataMakeOptionTab) and 3 is its label */
static SObj *data_tab_sobj(GObj *gobj, int i)
{
    SObj *sobj = (gobj != NULL) ? SObjGetStruct(gobj) : NULL;

    while (sobj != NULL && i-- > 0)
    {
        sobj = sobj->next;
    }
    return sobj;
}

/* mnDataSetOptionSpriteColors puts the pair's prim in envcolor and its
 * env in the sprite, over all three of the tab's pieces and none of the
 * label */
static void data_check_tab(GObj *gobj, s32 status)
{
    static const u8 kPrim[3][3] =
    {
        { 0x00, 0x00, 0x00 },       /* nMNOptionTabStatusNot */
        { 0x82, 0x00, 0x28 },       /* nMNOptionTabStatusHighlight */
        { 0x00, 0x00, 0x00 }        /* nMNOptionTabStatusSelected */
    };
    static const u8 kEnv[3][3] =
    {
        { 0x82, 0x82, 0xAA },
        { 0xFF, 0x00, 0x28 },
        { 0xFF, 0xFF, 0xFF }
    };
    int i;

    CHECK(gobj != NULL);
    for (i = 0; i < 3; i++)
    {
        SObj *sobj = data_tab_sobj(gobj, i);

        CHECK(sobj != NULL);
        if (sobj == NULL)
        {
            return;
        }
        CHECK(sobj->envcolor.r == kPrim[status][0]);
        CHECK(sobj->envcolor.g == kPrim[status][1]);
        CHECK(sobj->envcolor.b == kPrim[status][2]);
        CHECK(sobj->sprite.red == kEnv[status][0]);
        CHECK(sobj->sprite.green == kEnv[status][1]);
        CHECK(sobj->sprite.blue == kEnv[status][2]);
    }
}

/* a tab's left piece is at (x, y) and its label 26 or 27 right and 4
 * down of it (mnDataMakeCharacters, MakeVSRecord, MakeSoundTest) */
static void data_check_tab_at(GObj *gobj, f32 x, f32 y, u32 label, f32 dx)
{
    SObj *sobj = data_tab_sobj(gobj, 0);

    CHECK(sobj != NULL);
    if (sobj != NULL)
    {
        CHECK_EQF(sobj->pos.x, x);
        CHECK_EQF(sobj->pos.y, y);
        /* the middle piece is one texel column tiled out 16 * 8 wide */
        sobj = data_tab_sobj(gobj, 1);
        CHECK(sobj != NULL && sobj->cms == 0 && sobj->masks == 4 &&
              sobj->lrs == 128 && sobj->lrt == 29);
        CHECK_EQF(sobj->pos.x, x + 16.0f);
        /* and the right piece sits past it */
        sobj = data_tab_sobj(gobj, 2);
        CHECK(sobj != NULL);
        if (sobj != NULL)
        {
            CHECK_EQF(sobj->pos.x, x + 16.0f + 128.0f);
        }
    }
    sobj = data_tab_sobj(gobj, 3);
    CHECK(sobj != NULL);
    if (sobj != NULL)
    {
        CHECK(sobj->sprite.bitmap == ((Sprite *)sprite_bank_get(sMNDataFiles[1], label))->bitmap);
        CHECK_EQF(sobj->pos.x, x + dx);
        CHECK_EQF(sobj->pos.y, y + 4.0f);
        /* the labels are black and the tab's colours never reach them */
        CHECK(sobj->sprite.red == 0x00 && sobj->sprite.green == 0x00 &&
              sobj->sprite.blue == 0x00);
    }
}

static int gDataSeen[12];
static int gDataQuads;
static int gDataRun;

static void data_tic(void)
{
    s32 tic = (s32)dSYTaskmanUpdateCount;
    SYController *pad = &gSYControllerDevices[0];
    int base = gDataRun * 6;
    SObj *sobj;

    pad->button_tap = 0;
    pad->button_hold = 0;
    pad->stick_range.x = 0;
    pad->stick_range.y = 0;

    if (tic == 3)
    {
        /* the decals, on link 2: the collage, two paper decals in the
         * same orange as the panel behind the logo, and the dark DATA
         * icon over the collage's own (mnDataMakeDecals) */
        sobj = data_find(2, 0, 0x18000);
        CHECK(sobj != NULL && sobj->pos.x == 10.0f && sobj->pos.y == 10.0f);
        CHECK(sobj != NULL && !(sobj->sprite.attr & SP_TRANSPARENT));
        sobj = data_find(2, 0, 0x02A30);                    /* the paper */
        CHECK(sobj != NULL);
        if (sobj != NULL)
        {
            CHECK_EQF(sobj->pos.x, 140.0f);
            CHECK_EQF(sobj->pos.y, 143.0f);
            CHECK(sobj->sprite.red == 0xA0 && sobj->sprite.green == 0x78 &&
                  sobj->sprite.blue == 0x14);
            CHECK(sobj->sprite.attr & SP_TRANSPARENT);
        }
        sobj = data_find(2, 1, 0x04A78);                    /* DATA, dark */
        CHECK(sobj != NULL);
        if (sobj != NULL)
        {
            CHECK_EQF(sobj->pos.x, 10.0f);
            CHECK_EQF(sobj->pos.y, 10.0f);
            CHECK(sobj->sprite.red == 0x99 && sobj->sprite.green == 0x99 &&
                  sobj->sprite.blue == 0x99);
        }

        /* the labels, on link 3, over the orange panel */
        sobj = data_find(3, 0, 0x031F8);                    /* the logo */
        CHECK(sobj != NULL && sobj->pos.x == 235.0f && sobj->pos.y == 158.0f);
        sobj = data_find(3, 1, 0x023A8);                    /* DATA */
        CHECK(sobj != NULL && sobj->pos.x == 206.0f && sobj->pos.y == 131.0f);

        if (gDataRun == 0)
        {
            /* SOUND TEST locked: two tabs, lower and further apart, and
             * no third GObj at all */
            CHECK(sMNDataIsHaveSoundTest == FALSE);
            CHECK(sMNDataLastAvailableOption == nMNDataOptionVSRecord);
            data_check_tab_at(sMNDataOptionCharactersGObj, 113.0f, 57.0f, 0x014E0, 26.0f);
            data_check_tab_at(sMNDataOptionVSRecordGObj, 81.0f, 126.0f, 0x01900, 27.0f);
            CHECK(data_find(4, 1, 0x01D20) == NULL);
            /* and the cursor is on CHARACTERS, because the mode select
             * is not one of the three screens mnDataInitVars knows */
            CHECK(sMNDataOption == nMNDataOptionCharacters);
            data_check_tab(sMNDataOptionCharactersGObj, nMNOptionTabStatusHighlight);
            data_check_tab(sMNDataOptionVSRecordGObj, nMNOptionTabStatusNot);
        }
        else
        {
            /* unlocked: three tabs, higher and closer together */
            CHECK(sMNDataIsHaveSoundTest != FALSE);
            CHECK(sMNDataLastAvailableOption == nMNDataOptionSoundTest);
            data_check_tab_at(sMNDataOptionCharactersGObj, 133.0f, 42.0f, 0x014E0, 26.0f);
            data_check_tab_at(sMNDataOptionVSRecordGObj, 101.0f, 89.0f, 0x01900, 27.0f);
            data_check_tab_at(sMNDataOptionSoundTestGObj, 69.0f, 136.0f, 0x01D20, 26.0f);
            /* it came back from SOUND TEST, so that is where the cursor
             * is (mnDataInitVars) */
            CHECK(sMNDataOption == nMNDataOptionSoundTest);
            data_check_tab(sMNDataOptionSoundTestGObj, nMNOptionTabStatusHighlight);
            data_check_tab(sMNDataOptionCharactersGObj, nMNOptionTabStatusNot);
            data_check_tab(sMNDataOptionVSRecordGObj, nMNOptionTabStatusNot);
        }
        gDataQuads = gLBCommonSpriteQuadLogCount;
        gLBCommonSpriteQuadLogCount = 0;
        gDataSeen[base + 0] = 1;
    }
    if (tic == 4)
    {
        /* mnDataLabelsProcDisplay: one orange fill under the labels'
         * camera, 85 by 87 game pixels onto the 640x480 frame */
        int i, fills = 0;

        for (i = 0; i < gLBCommonSpriteQuadLogCount && i < LB_SPRITE_QUAD_LOG_MAX; i++)
        {
            const LBCommonSpriteQuad *q = &gLBCommonSpriteQuadLog[i];

            CHECK(i == 0 || q->z > gLBCommonSpriteQuadLog[i - 1].z);
            if (q->rect.copy != -1)
            {
                continue;
            }
            fills++;
            CHECK_EQF(q->x0, 450.0f);
            CHECK_EQF(q->y0, 286.0f);
            CHECK_EQF(q->x1, 620.0f);
            CHECK_EQF(q->y1, 460.0f);
            CHECK(q->argb == 0xE6A07814);
        }
        CHECK(fills == 1);
        gDataSeen[base + 1] = 1;
    }
    /* the scene is deaf for ten tics (mnDataFuncRun) */
    if (tic == 12)
    {
        pad->button_hold = D_JPAD;
    }
    if (tic == 14)
    {
        if (gDataRun == 0)
        {
            /* two tabs: down landed on VS RECORD, the last one */
            CHECK(sMNDataOption == nMNDataOptionVSRecord);
            data_check_tab(sMNDataOptionVSRecordGObj, nMNOptionTabStatusHighlight);
            data_check_tab(sMNDataOptionCharactersGObj, nMNOptionTabStatusNot);
        }
        else
        {
            /* three tabs: down off the last one wrapped to the first */
            CHECK(sMNDataOption == nMNDataOptionCharacters);
            data_check_tab(sMNDataOptionCharactersGObj, nMNOptionTabStatusHighlight);
            data_check_tab(sMNDataOptionSoundTestGObj, nMNOptionTabStatusNot);
        }
        gDataSeen[base + 2] = 1;
        pad->button_hold = D_JPAD;
    }
    if (tic == 16)
    {
        /* one more down: the two-tab screen has wrapped to CHARACTERS,
         * the three-tab one has stepped to VS RECORD */
        CHECK(sMNDataOption == (gDataRun == 0 ? nMNDataOptionCharacters
                                              : nMNDataOptionVSRecord));
        gDataSeen[base + 3] = 1;
        pad->button_hold = U_JPAD;
    }
    if (tic == 18)
    {
        /* and up off the first wraps to the last, whichever that is */
        CHECK(sMNDataOption == (gDataRun == 0 ? nMNDataOptionVSRecord
                                              : nMNDataOptionCharacters));
        gDataSeen[base + 4] = 1;
        if (gDataRun == 0)
        {
            pad->button_tap = A_BUTTON;
        }
        else
        {
            pad->button_tap = B_BUTTON;
        }
    }
    if (tic == 19 && gDataRun == 0)
    {
        /* A selected VS RECORD last tic: its tab went white and the
         * scene asked for that screen */
        data_check_tab(sMNDataOptionVSRecordGObj, nMNOptionTabStatusSelected);
        CHECK(gSCManagerSceneData.scene_curr == nSCKindVSRecord);
        gDataSeen[base + 5] = 1;
    }
}

static void test_data_menu(void)
{
    u32 saved = gSCManagerBackupData.unlock_mask;
    int i;

    sprite_bank_release_all();
    memset(gDataSeen, 0, sizeof(gDataSeen));

    /* run one: SOUND TEST locked, entered from the mode select */
    gDataRun = 0;
    gSCManagerBackupData.unlock_mask &= ~LBBACKUP_UNLOCK_MASK_SOUNDTEST;
    gSCManagerSceneData.scene_prev = nSCKindModeSelect;
    gSCManagerSceneData.scene_curr = nSCKindData;

    gSceneTicHook = data_tic;
    syDmaLoadOverlay(OVERLAY_SUBSYS);
    syDmaLoadOverlay(OVERLAY_DATA);
    mnDataStartScene();
    gSceneTicHook = NULL;

    /* the two sprite banks are the ones the scene asked for */
    CHECK(dMNDataFileIDs[0] == 0 && dMNDataFileIDs[1] == 5);
    CHECK(sMNDataFiles[0] != NULL && sMNDataFiles[1] != NULL);
    /* A on tic 18 was seen by mnDataFuncRun that tic; the next tic's
     * run is the one that ends the scene */
    CHECK((s32)dSYTaskmanUpdateCount == 20);
    CHECK(gSCManagerSceneData.scene_prev == nSCKindData);
    CHECK(gSCManagerSceneData.scene_curr == nSCKindVSRecord);

    /* run two: SOUND TEST unlocked, and entered as the bounce off an
     * unported SOUND TEST would enter it */
    gDataRun = 1;
    gSCManagerBackupData.unlock_mask |= LBBACKUP_UNLOCK_MASK_SOUNDTEST;
    gSCManagerSceneData.scene_prev = nSCKindSoundTest;
    gSCManagerSceneData.scene_curr = nSCKindData;

    gSceneTicHook = data_tic;
    syDmaLoadOverlay(OVERLAY_SUBSYS);
    syDmaLoadOverlay(OVERLAY_DATA);
    mnDataStartScene();
    gSceneTicHook = NULL;

    /* B on tic 18 goes back to the mode select */
    CHECK((s32)dSYTaskmanUpdateCount == 19);
    CHECK(gSCManagerSceneData.scene_prev == nSCKindData);
    CHECK(gSCManagerSceneData.scene_curr == nSCKindModeSelect);

    for (i = 0; i < 6; i++)
    {
        CHECK(gDataSeen[i]);
    }
    for (i = 6; i < 11; i++)
    {
        CHECK(gDataSeen[i]);
    }
    /* the overlay reload is what makes the second run's layout the
     * second run's: every static above is back to zero before it */
    mnDataOverlayLoad();
    CHECK(sMNDataOption == 0 && sMNDataIsHaveSoundTest == 0);
    CHECK(sMNDataFiles[0] == NULL && sMNDataFiles[1] == NULL);
    CHECK(sMNDataOptionCharactersGObj == NULL);

    gSCManagerBackupData.unlock_mask = saved;
}

/* ---- the Records tab (src/dc/mnvsrecord.c) ---------------
 *
 * Two halves. First the arithmetic, called directly against a
 * controlled gSCManagerBackupData.vs_records -- KOs, TKOs, win%, use%,
 * self-destruct%, average player count, and the "locked fighter sorts
 * to the back regardless of its stat" rule every table sort shares.
 * Then a scripted-pad run of the scene itself: the three tables cycle
 * with A/START and clamp at Individual, B unwinds them and finally
 * leaves for the DATA menu, and the overlay clear zeroes every static.
 * The directional stick/D-pad inputs each table also reads (Ranking's
 * resort and column scroll, Individual's fighter step) are not driven
 * here -- they share sMNVSRecordChangeWait's repeat-pacing wait with
 * every other input in the scene, and getting a scripted pad's timing
 * against an unexposed wait constant wrong would make this test flaky
 * rather than prove anything; mnVSRecordSortData's direct-call coverage
 * below is what actually exercises the values those inputs display.
 */
extern u32 dMNVSRecordFileIDs[];

static void vsrecord_unit_tests(void)
{
    LBBackupData saved = gSCManagerBackupData;
    s32 i;

    memset(&gSCManagerBackupData.vs_records, 0, sizeof(gSCManagerBackupData.vs_records));
    sMNVSRecordFighterMask = LBBACKUP_CHARACTER_MASK_ALL;

    CHECK(mnVSRecordGetPowerOf(10, 0) == 1);
    CHECK(mnVSRecordGetPowerOf(10, 3) == 1000);
    CHECK(mnVSRecordGetPowerOf(2, 5) == 32);

    CHECK(mnVSRecordGetDigitCount(0, 4) == 0);
    CHECK(mnVSRecordGetDigitCount(5, 4) == 1);
    CHECK(mnVSRecordGetDigitCount(1234, 4) == 4);
    CHECK(mnVSRecordGetDigitCount(999, 4) == 3);

    CHECK(mnVSRecordGetCharacterID('A') == 0);
    CHECK(mnVSRecordGetCharacterID('Z') == 25);
    CHECK(mnVSRecordGetCharacterID('\'') == 0x1A);
    CHECK(mnVSRecordGetCharacterID('%') == 0x1B);
    CHECK(mnVSRecordGetCharacterID('.') == 0x1C);
    CHECK(mnVSRecordGetCharacterID(' ') == 0x1D);
    CHECK(mnVSRecordGetCharacterID('a') == 0x1D);

    /* four fighters gate on the mask (Luigi, Link... no -- Ness, Purin,
     * Captain, Luigi, src/dc/mnvsrecord.c:501); everyone else is always
     * "have" regardless of it. */
    sMNVSRecordFighterMask = 0;
    CHECK(mnVSRecordCheckHaveFighterKind(nFTKindNess) == FALSE);
    CHECK(mnVSRecordCheckHaveFighterKind(nFTKindPurin) == FALSE);
    CHECK(mnVSRecordCheckHaveFighterKind(nFTKindCaptain) == FALSE);
    CHECK(mnVSRecordCheckHaveFighterKind(nFTKindLuigi) == FALSE);
    CHECK(mnVSRecordCheckHaveFighterKind(nFTKindMario) != FALSE);
    CHECK(mnVSRecordCheckHaveFighterKind(nFTKindFox) != FALSE);
    sMNVSRecordFighterMask = LBBACKUP_MASK_FIGHTER(nFTKindNess);
    CHECK(mnVSRecordCheckHaveFighterKind(nFTKindNess) != FALSE);
    sMNVSRecordFighterMask = LBBACKUP_CHARACTER_MASK_ALL;

    /* Mario KO'd Fox three times; Fox KO'd Mario twice; Mario
     * self-destructed once. Six TKOs total in the save, half of them
     * Mario's KOs -- a round win% to check against. */
    gSCManagerBackupData.vs_records[nFTKindMario].ko_count[nFTKindFox] = 3;
    gSCManagerBackupData.vs_records[nFTKindFox].ko_count[nFTKindMario] = 2;
    gSCManagerBackupData.vs_records[nFTKindMario].selfdestructs = 1;

    CHECK(mnVSRecordGetKOs(nFTKindMario) == 3);
    CHECK(mnVSRecordGetKOs(nFTKindFox) == 2);
    CHECK(mnVSRecordGetTKO(nFTKindMario) == 3);   /* 2 KO'd by Fox + 1 SD */
    CHECK(mnVSRecordGetTKO(nFTKindFox) == 3);     /* 3 KO'd by Mario */
    CHECK(mnVSRecordGetTotalTKO() == 6);
    CHECK_EQF(mnVSRecordGetWinPercent(nFTKindMario), 50.0f);
    CHECK_NEAR(mnVSRecordGetWinPercent(nFTKindFox), 33.3333f, 0.01f);
    CHECK_NEAR(mnVSRecordGetSDPercent(nFTKindMario), 33.3333f, 0.01f);
    CHECK_EQF(mnVSRecordGetSDPercent(nFTKindFox), 0.0f);
    CHECK_NEAR(mnVSRecordGetWinPercentAgainst(nFTKindMario, nFTKindFox), 60.0f, 0.01f);
    CHECK_NEAR(mnVSRecordGetWinPercentAgainst(nFTKindFox, nFTKindMario), 40.0f, 0.01f);

    /* everyone else is tied at 0%: standard competition ranking gives
     * Mario 1st, Fox 2nd, and every tied fighter the same 3rd. */
    CHECK(mnVSRecordGetRanking(nFTKindMario) == 1);
    CHECK(mnVSRecordGetRanking(nFTKindFox) == 2);
    CHECK(mnVSRecordGetRanking(nFTKindDonkey) == 3);
    CHECK(mnVSRecordGetRanking(nFTKindKirby) == 3);

    gSCManagerBackupData.vs_records[nFTKindMario].games_played = 3;
    gSCManagerBackupData.vs_records[nFTKindMario].player_count_tally = 6;
    gSCManagerBackupData.vs_records[nFTKindFox].games_played = 1;

    CHECK_EQF(mnVSRecordGetAvg(nFTKindMario), 2.0f);
    CHECK_EQF(mnVSRecordGetAvg(nFTKindDonkey), 0.0f);   /* never played */
    CHECK(mnVSRecordGetGamesPlayedSum() == 4);
    CHECK_EQF(mnVSRecordGetUsePercent(nFTKindMario), 75.0f);
    CHECK_EQF(mnVSRecordGetUsePercent(nFTKindFox), 25.0f);

    gSCManagerBackupData.vs_records[nFTKindMario].played_against[nFTKindFox] = 2;
    gSCManagerBackupData.vs_records[nFTKindMario].player_count_tallies[nFTKindFox] = 5;
    CHECK_EQF(mnVSRecordGetAvgAgainst(nFTKindMario, nFTKindFox), 2.5f);
    CHECK_EQF(mnVSRecordGetAvgAgainst(nFTKindFox, nFTKindMario), 0.0f);

    /* Luigi outscores everyone on KOs but is locked: every sort puts a
     * locked fighter after every unlocked one regardless of its stat
     * (mnVSRecordCheckHaveFighterKind's four gated kinds). */
    gSCManagerBackupData.vs_records[nFTKindLuigi].ko_count[nFTKindMario] = 100;
    sMNVSRecordFighterMask = LBBACKUP_CHARACTER_MASK_STARTER;   /* Luigi, Ness, Purin, Captain locked */
    mnVSRecordSortData(nMNVSRecordKindBattleScore);

    for (i = nFTKindPlayableStart; i <= nFTKindPlayableEnd; i++)
    {
        sb32 have = mnVSRecordCheckHaveFighterKind(sMNVSRecordBattleScoreFighterKinds[i]);

        if (!have)
        {
            break;
        }
    }
    CHECK(i <= nFTKindPlayableEnd);   /* the locked block was reached at all */
    for (; i <= nFTKindPlayableEnd; i++)
    {
        CHECK(mnVSRecordCheckHaveFighterKind(sMNVSRecordBattleScoreFighterKinds[i]) == FALSE);
    }
    CHECK(sMNVSRecordBattleScoreFighterKinds[nFTKindPlayableStart] == nFTKindMario);

    gSCManagerBackupData = saved;
    sMNVSRecordFighterMask = 0;
}

static SObj *vsrecord_find(s32 link, int file, u32 offset)
{
    return scene_sprite_find(sMNVSRecordFiles, link, file, offset);
}

static int gVSRecordSeen[8];

static void vsrecord_tic(void)
{
    s32 tic = (s32)dSYTaskmanUpdateCount;
    SYController *pad = &gSYControllerDevices[0];
    SObj *sobj;

    pad->button_tap = 0;
    pad->button_hold = 0;
    pad->stick_range.x = 0;
    pad->stick_range.y = 0;

    if (tic == 3)
    {
        /* the two fixed labels (mnVSRecordMakeLabels), the arrows
         * (MakePortraitStatsArrows, MakeResortArrows, MakeColumnArrows)
         * and the Battle Score subtitle (MakeSubtitle) -- all six on
         * link 2, all made once and never remade, only shown or hidden
         * by their own GObj's flags. */
        sobj = vsrecord_find(2, 1, 0x00b40);                 /* DATA header */
        CHECK(sobj != NULL && sobj->pos.x == 24.0f && sobj->pos.y == 17.0f);
        sobj = vsrecord_find(2, 0, 0x05428);                 /* the gold label */
        CHECK(sobj != NULL && sobj->pos.x == 99.0f && sobj->pos.y == 23.0f);
        sobj = vsrecord_find(2, 0, 0x015d0);                 /* Battle Score subtitle */
        CHECK(sobj != NULL && sobj->pos.x == 222.0f && sobj->pos.y == 28.0f);
        sobj = vsrecord_find(2, 1, 0x00be0);                 /* portrait arrow L */
        CHECK(sobj != NULL && sobj->pos.x == 40.0f && sobj->pos.y == 78.0f);
        sobj = vsrecord_find(2, 1, 0x00c80);                 /* portrait arrow R */
        CHECK(sobj != NULL && sobj->pos.x == 105.0f && sobj->pos.y == 78.0f);
        sobj = vsrecord_find(2, 0, 0x01668);                 /* resort arrows */
        CHECK(sobj != NULL && sobj->pos.x == 281.0f && sobj->pos.y == 39.0f);
        sobj = vsrecord_find(2, 0, 0x017a8);                 /* column arrows */
        CHECK(sobj != NULL && sobj->pos.x == 25.0f && sobj->pos.y == 47.0f);

        CHECK(sMNVSRecordStatsKind == nMNVSRecordKindBattleScore);
        CHECK(sMNVSRecordCurrentIndex == 0);
        CHECK(sMNVSRecordFirstColumn == nMNVSRecordRankingKindStart);
        CHECK(sMNVSRecordFighterMask == gSCManagerBackupData.fighter_mask);
        CHECK(sMNVSRecordTableHeadersGObj != NULL && sMNVSRecordTableValuesGObj != NULL);
        gVSRecordSeen[0] = 1;
    }
    if (tic == 4)
    {
        pad->button_tap = A_BUTTON;
    }
    if (tic == 5)
    {
        CHECK(sMNVSRecordStatsKind == nMNVSRecordKindRanking);
        gVSRecordSeen[1] = 1;
        pad->button_tap = A_BUTTON;
    }
    if (tic == 6)
    {
        CHECK(sMNVSRecordStatsKind == nMNVSRecordKindIndiv);
        gVSRecordSeen[2] = 1;
        /* one more A: Individual is nMNVSRecordKindEnd, so this is a
         * clamp, not a wrap */
        pad->button_tap = A_BUTTON;
    }
    if (tic == 7)
    {
        CHECK(sMNVSRecordStatsKind == nMNVSRecordKindIndiv);
        gVSRecordSeen[3] = 1;
        pad->button_tap = B_BUTTON;
    }
    if (tic == 8)
    {
        CHECK(sMNVSRecordStatsKind == nMNVSRecordKindRanking);
        gVSRecordSeen[4] = 1;
        pad->button_tap = B_BUTTON;
    }
    if (tic == 9)
    {
        CHECK(sMNVSRecordStatsKind == nMNVSRecordKindBattleScore);
        gVSRecordSeen[5] = 1;
        /* B on the top table leaves for the DATA menu */
        pad->button_tap = B_BUTTON;
    }
}

static void test_vsrecord_menu(void)
{
    u16 saved_mask = gSCManagerBackupData.fighter_mask;
    int i;

    vsrecord_unit_tests();

    sprite_bank_release_all();
    memset(gVSRecordSeen, 0, sizeof(gVSRecordSeen));
    gSCManagerBackupData.fighter_mask = LBBACKUP_CHARACTER_MASK_ALL;
    gSCManagerSceneData.scene_prev = nSCKindData;
    gSCManagerSceneData.scene_curr = nSCKindVSRecord;

    gSceneTicHook = vsrecord_tic;
    syDmaLoadOverlay(OVERLAY_SUBSYS);
    syDmaLoadOverlay(OVERLAY_VSRECORD);
    mnVSRecordStartScene();
    gSceneTicHook = NULL;

    CHECK(dMNVSRecordFileIDs[0] == 0x1f && dMNVSRecordFileIDs[1] == 0x20 &&
          dMNVSRecordFileIDs[2] == 0x13 && dMNVSRecordFileIDs[3] == 0x21);
    CHECK(sMNVSRecordFiles[0] != NULL && sMNVSRecordFiles[1] != NULL &&
          sMNVSRecordFiles[2] != NULL && sMNVSRecordFiles[3] != NULL);
    /* B on Battle Score set scene_curr directly, in the same tic --
     * unlike mnDataFuncRun's deferred proceed flag, there is nothing
     * here to delay the exit by an extra tic (src/dc/mnvsrecord.c). */
    CHECK((s32)dSYTaskmanUpdateCount == 10);
    CHECK(gSCManagerSceneData.scene_prev == nSCKindVSRecord);
    CHECK(gSCManagerSceneData.scene_curr == nSCKindData);

    for (i = 0; i < 6; i++)
    {
        CHECK(gVSRecordSeen[i]);
    }

    mnVSRecordOverlayLoad();
    CHECK(sMNVSRecordStatsKind == 0 && sMNVSRecordCurrentIndex == 0);
    CHECK(sMNVSRecordTableHeadersGObj == NULL && sMNVSRecordTableValuesGObj == NULL);
    CHECK(sMNVSRecordFiles[0] == NULL && sMNVSRecordFiles[1] == NULL &&
          sMNVSRecordFiles[2] == NULL && sMNVSRecordFiles[3] == NULL);

    gSCManagerBackupData.fighter_mask = saved_mask;
}

/* ---- the Character Data tab (src/dc/mncharacters.c) -----
 *
 * A scripted-pad run of the page turn: R_TRIG held steps Mario (page 0)
 * to Luigi (page 1), L_TRIG held steps back. Both are read as *held*
 * buttons (mnCommonCheckGetOptionButtonInput), gated by
 * sMNCharactersChangeWait -- and mnCharactersFuncRun itself reads no
 * input at all until sMNCharactersTotalTimeTics reaches 10, so the
 * first press below sits at tic 9, not tic 3: the controller read for
 * a tic runs before that tic's own scene update (src/dc/taskman.c
 * syTaskmanCommonTaskUpdate), so a press set at tic 9 is what
 * mnCharactersFuncRun reads the moment TotalTimeTics's ==10 check
 * first passes. The tic right after is left neutral -- button_hold
 * defaults to 0 above -- so
 * mnCharactersFuncRun's own no-input-held arm clears the wait counter
 * back to 0 before the next press, the same lesson soundtest_tic
 * applies with its neutral tic between column moves. The point of this
 * test is really the FTSTAT_CHARDATA_START fix (src/dc/ftcommon.c's
 * ftMainGetStatusDesc, this file's header note): mnCharactersFuncStart's
 * own ftMainSetStatus call is the first live path in the port that
 * dispatches a FTSTATUS_CHARACTERS_DEMO-encoded status id, and without
 * the fix that call resolves to a NULL FTStatusDesc and crashes before
 * this test's first tic ever runs. */
static int gCharactersSeen[6];

static void characters_tic(void)
{
    s32 tic = (s32)dSYTaskmanUpdateCount;
    SYController *pad = &gSYControllerDevices[0];
    /* not `fp`: this file's #define fp (*fpp) (its mock-fighter physics
     * convention, above) shadows that name everywhere past its own
     * declaration, so a plain local called fp here silently becomes a
     * dereference of the unrelated, uninitialized global fpp instead of
     * this function's own fighter pointer. */
    FTStruct *cfp;

    pad->button_tap = 0;
    pad->button_hold = 0;
    pad->stick_range.x = 0;
    pad->stick_range.y = 0;

    if (tic == 3)
    {
        CHECK(sMNCharactersPage == 0);
        CHECK(mnCharactersGetFighterKind(sMNCharactersPage) == nFTKindMario);
        CHECK(sMNCharactersFighterGObj != NULL);
        CHECK(sMNCharactersStoryGObj != NULL && sMNCharactersWorksGObj != NULL &&
              sMNCharactersEmblemGObj != NULL && sMNCharactersNameGObj != NULL);

        cfp = ftGetStruct(sMNCharactersFighterGObj);
        CHECK(cfp != NULL && cfp->is_muted == TRUE);
        /* The status id is NOT pinned here, and this is the second
         * draft: the first asserted `status_id >= nFTDemoStatusNull` on
         * the grounds that the value is one of the two ENCODED kinds the
         * motion tables carry (FTSTAT_CHARDATA_START's, a fighter's own
         * motion, or a raw nFTDemoStatusNull one, a win/lose pose). That
         * is true of what mnCharactersFuncStart SETS and not of what is
         * there three tics later: a chardata status resolves to a real
         * FTStatusDesc whose own proc_update can move the fighter on to
         * a plain common status, which is a much smaller number.
         * mnCharactersRandMotionKind draws at random off the wall clock,
         * so whether that has happened by tic 3 varies by run, and the
         * check failed about one run in six.
         *
         * What this test proves about the FTSTAT_CHARDATA_START fix is
         * STRUCTURAL and does not need the value: without
         * ftMainGetStatusDesc's chardata strip (src/dc/ftcommon.c),
         * mnCharactersFuncStart's own ftMainSetStatus resolves to a NULL
         * FTStatusDesc and dereferences it, so the scene never reaches
         * this tic at all. Arriving here with a live, muted demo fighter
         * IS the assertion -- the same shape test_openingroom takes
         * with its own wall-clock draws: assert the invariant, not the
         * roll. */

        gCharactersSeen[0] = 1;
    }
    if (tic == 9)
    {
        pad->button_hold = R_TRIG;                        /* Mario -> Luigi */
    }
    if (tic == 10)
    {
        CHECK(sMNCharactersPage == 1);
        CHECK(mnCharactersGetFighterKind(sMNCharactersPage) == nFTKindLuigi);
        gCharactersSeen[1] = 1;
    }
    if (tic == 11)
    {
        pad->button_hold = L_TRIG;                        /* Luigi -> Mario */
    }
    if (tic == 12)
    {
        CHECK(sMNCharactersPage == 0);
        CHECK(mnCharactersGetFighterKind(sMNCharactersPage) == nFTKindMario);
        gCharactersSeen[2] = 1;
    }
    /* let the demo motion state machine run a few tics unscripted --
     * mnCharactersFighterProcUpdate ticking sMNCharactersAnimFramesRemain
     * down and mnCharactersFuncRun's own countdown -- proof the fix
     * holds past the first status set, not just on it. */
    if (tic == 20)
    {
        cfp = ftGetStruct(sMNCharactersFighterGObj);
        /* still standing seventeen tics later, for the reason above:
         * the fighter surviving its own motion machine is what the
         * chardata strip buys, and the id it is on by now is the
         * machine's business */
        CHECK(cfp != NULL && cfp->is_muted == TRUE);
        gCharactersSeen[3] = 1;
    }
    if (tic == 22)
    {
        gCharactersSeen[4] = 1;
        pad->button_tap = B_BUTTON;                       /* back to DATA */
    }
}

static void test_characters_menu(void)
{
    u16 saved_mask = gSCManagerBackupData.fighter_mask;
    s32 saved_fkind = gSCManagerBackupData.characters_fkind;
    int i;

    sprite_bank_release_all();
    memset(gCharactersSeen, 0, sizeof(gCharactersSeen));
    gSCManagerBackupData.fighter_mask = LBBACKUP_CHARACTER_MASK_ALL;
    gSCManagerBackupData.characters_fkind = nFTKindMario;
    gSCManagerSceneData.scene_prev = nSCKindData;
    gSCManagerSceneData.scene_curr = nSCKindCharacters;

    gSceneTicHook = characters_tic;
    syDmaLoadOverlay(OVERLAY_BATTLE);
    syDmaLoadOverlay(OVERLAY_SUBSYS);
    syDmaLoadOverlay(OVERLAY_CHARACTERS);
    mnCharactersStartScene();
    gSceneTicHook = NULL;

    CHECK(dMNCharactersFileIDs[0] == 16 && dMNCharactersFileIDs[1] == 32 &&
          dMNCharactersFileIDs[2] == 20 && dMNCharactersFileIDs[3] == 35);
    CHECK(sMNCharactersFiles[0] != NULL && sMNCharactersFiles[1] != NULL &&
          sMNCharactersFiles[2] != NULL && sMNCharactersFiles[3] == NULL);
    CHECK(gSCManagerSceneData.scene_prev == nSCKindCharacters);
    CHECK(gSCManagerSceneData.scene_curr == nSCKindData);
    /* mnCharactersBackupFighterKind, on the way out */
    CHECK(gSCManagerBackupData.characters_fkind == nFTKindMario);

    for (i = 0; i < 5; i++)
    {
        CHECK(gCharactersSeen[i]);
    }

    mnCharactersOverlayLoad();
    CHECK(sMNCharactersFiles[0] == NULL && sMNCharactersFiles[1] == NULL &&
          sMNCharactersFiles[2] == NULL && sMNCharactersFiles[3] == NULL);
    CHECK(sMNCharactersFighterGObj == NULL && sMNCharactersPage == 0);

    gSCManagerBackupData.fighter_mask = saved_mask;
    gSCManagerBackupData.characters_fkind = saved_fkind;
}

/* ---- the Sound Test tab (src/dc/mnsoundtest.c) ----------
 *
 * A scripted-pad run of the three columns: D_JPAD steps Music -> Sound
 * -> Voice -> (wraps) Music, U_JPAD from Music wraps the other way to
 * Voice, and L_TRIG/R_TRIG step Voice's own selected ID, wrapping at
 * both ends the way mnSoundTestUpdateControllerInputs has it. These are
 * all read as *held* buttons (mnCommonCheckGetOptionButtonInput), not
 * tapped, gated by sMNSoundTestOptionChangeWait -- a single tic's hold
 * is one step, since releasing it the next tic (every tic here resets
 * both fields to 0 first) zeroes the wait immediately, the same
 * neutral-input branch mnSoundTestUpdateControllerInputs opens with. A
 * and the fade/stop calls are not scripted here: what they reach is
 * sndres's "menu" set, already covered by every other menu scene's own
 * test, and mnsoundtest.h's DIVERGES note says why this screen cannot
 * have a set of its own to assert against.
 */
extern u32 dMNSoundTestFileIDs[];
extern u32 dMNSoundTestVoiceIDs[244];

static SObj *soundtest_find(s32 link, int file, u32 offset)
{
    return scene_sprite_find(sMNSoundTestFiles, link, file, offset);
}

static int gSoundTestSeen[8];

static void soundtest_tic(void)
{
    s32 tic = (s32)dSYTaskmanUpdateCount;
    SYController *pad = &gSYControllerDevices[0];
    SObj *sobj;

    pad->button_tap = 0;
    pad->button_hold = 0;
    pad->stick_range.x = 0;
    pad->stick_range.y = 0;

    if (tic == 3)
    {
        sobj = soundtest_find(2, 2, 0x00b40);            /* DATA header */
        CHECK(sobj != NULL && sobj->pos.x == 23.0f && sobj->pos.y == 17.0f);
        sobj = soundtest_find(2, 4, 0x01bb8);            /* SOUND TEST title */
        CHECK(sobj != NULL && sobj->pos.x == 152.0f && sobj->pos.y == 23.0f);
        sobj = soundtest_find(3, 4, 0x00438);            /* MUSIC label */
        CHECK(sobj != NULL && sobj->pos.x == 55.0f && sobj->pos.y == 61.0f);
        sobj = soundtest_find(3, 4, 0x009c0);            /* SOUND label */
        CHECK(sobj != NULL && sobj->pos.x == 64.0f && sobj->pos.y == 108.0f);
        sobj = soundtest_find(3, 4, 0x00e48);            /* VOICE label */
        CHECK(sobj != NULL && sobj->pos.x == 94.0f && sobj->pos.y == 156.0f);
        sobj = soundtest_find(3, 0, 0x00958);            /* A button decal */
        CHECK(sobj != NULL && sobj->pos.x == 55.0f && sobj->pos.y == 205.0f);

        CHECK(sMNSoundTestOption == nMNSoundTestOptionMusic);
        CHECK(sMNSoundTestOptionSelectID[nMNSoundTestOptionMusic] == 0 &&
              sMNSoundTestOptionSelectID[nMNSoundTestOptionSound] == 0 &&
              sMNSoundTestOptionSelectID[nMNSoundTestOptionVoice] == 0);
        /* the arrows sit on the Music row (dMNSoundTestArrowSpritePositions[0..2]) */
        sobj = soundtest_find(2, 3, 0x0de30);            /* arrow L */
        CHECK(sobj != NULL && sobj->pos.x == 162.0f && sobj->pos.y == 73.0f);
        sobj = soundtest_find(2, 3, 0x0dd90);            /* arrow R */
        CHECK(sobj != NULL && sobj->pos.x == 224.0f && sobj->pos.y == 73.0f);
        gSoundTestSeen[0] = 1;
    }
    /* Every press below sits on its own tic, one tic apart from its
     * neighbor, with the tic in between left at the neutral the top of
     * this function already defaults to. mnSoundTestUpdateControllerInputs'
     * neutral branch only zeroes sMNSoundTestOptionChangeWait when *no*
     * direction is held that tic -- two presses back to back read as one
     * continued hold and the second is swallowed, wait already nonzero
     * from the first. */
    if (tic == 4)
    {
        pad->button_hold = D_JPAD;                       /* Music -> Sound */
    }
    if (tic == 6)
    {
        CHECK(sMNSoundTestOption == nMNSoundTestOptionSound);
        gSoundTestSeen[1] = 1;
        pad->button_hold = D_JPAD;                       /* Sound -> Voice */
    }
    if (tic == 8)
    {
        CHECK(sMNSoundTestOption == nMNSoundTestOptionVoice);
        sobj = soundtest_find(2, 3, 0x0de30);            /* arrow L, Voice row */
        CHECK(sobj != NULL && sobj->pos.x == 201.0f && sobj->pos.y == 168.0f);
        gSoundTestSeen[2] = 1;
        /* one more down: Voice is nMNSoundTestOptionEnd, so this wraps
         * to Music rather than clamping (unlike the Records tab's
         * Individual tab, src/dc/mnvsrecord.c) */
        pad->button_hold = D_JPAD;
    }
    if (tic == 10)
    {
        CHECK(sMNSoundTestOption == nMNSoundTestOptionMusic);
        gSoundTestSeen[3] = 1;
        /* up from Music wraps the other way, back to Voice */
        pad->button_hold = U_JPAD;
    }
    if (tic == 12)
    {
        CHECK(sMNSoundTestOption == nMNSoundTestOptionVoice);
        gSoundTestSeen[4] = 1;
        pad->button_hold = L_TRIG;
    }
    if (tic == 14)
    {
        CHECK(sMNSoundTestOptionSelectID[nMNSoundTestOptionVoice] ==
              (s32)ARRAY_COUNT(dMNSoundTestVoiceIDs) - 1);
        gSoundTestSeen[5] = 1;
        pad->button_hold = R_TRIG;
    }
    if (tic == 16)
    {
        CHECK(sMNSoundTestOptionSelectID[nMNSoundTestOptionVoice] == 0);
        gSoundTestSeen[6] = 1;
        /* the scene_curr transition this tap causes is checked after the
         * loop (test_soundtest_menu), since B_BUTTON's own tap handling
         * runs before mnSoundTestUpdateControllerInputs in
         * mnSoundTestFuncRun and syTaskmanCheckBreakLoop ends the frame
         * loop the same tic the load flag is set -- there is no tic 17
         * for a hook to run on. */
        pad->button_tap = B_BUTTON;
        gSoundTestSeen[7] = 1;
    }
}

static void test_soundtest_menu(void)
{
    int i;

    sprite_bank_release_all();
    memset(gSoundTestSeen, 0, sizeof(gSoundTestSeen));
    gSCManagerSceneData.scene_prev = nSCKindData;
    gSCManagerSceneData.scene_curr = nSCKindSoundTest;

    gSceneTicHook = soundtest_tic;
    syDmaLoadOverlay(OVERLAY_SUBSYS);
    syDmaLoadOverlay(OVERLAY_SOUNDTEST);
    mnSoundTestStartScene();
    gSceneTicHook = NULL;

    CHECK(dMNSoundTestFileIDs[0] == 0xc5 && dMNSoundTestFileIDs[1] == 0xa4 &&
          dMNSoundTestFileIDs[2] == 0x20 && dMNSoundTestFileIDs[3] == 0x0 &&
          dMNSoundTestFileIDs[4] == 0xc4);
    CHECK(sMNSoundTestFiles[0] != NULL && sMNSoundTestFiles[1] != NULL &&
          sMNSoundTestFiles[2] != NULL && sMNSoundTestFiles[3] != NULL &&
          sMNSoundTestFiles[4] != NULL);
    CHECK(gSCManagerSceneData.scene_prev == nSCKindSoundTest);
    CHECK(gSCManagerSceneData.scene_curr == nSCKindData);

    for (i = 0; i < 8; i++)
    {
        CHECK(gSoundTestSeen[i]);
    }

    mnSoundTestOverlayLoad();
    CHECK(sMNSoundTestOption == 0);
    CHECK(sMNSoundTestOptionSelectID[0] == 0 && sMNSoundTestOptionSelectID[1] == 0 &&
          sMNSoundTestOptionSelectID[2] == 0);
    CHECK(sMNSoundTestFiles[0] == NULL && sMNSoundTestFiles[1] == NULL &&
          sMNSoundTestFiles[2] == NULL && sMNSoundTestFiles[3] == NULL &&
          sMNSoundTestFiles[4] == NULL);
}

/* ---- the Options screen (src/dc/mnoption.c) ---------------
 *
 * mn/mnoption/mnoption.c driven by a scripted pad, twice: once entered
 * from the mode select (scene_prev == nSCKindModeSelect), which walks
 * the sound toggle's two stick directions and its A-tap flip, then steps
 * down onto SCREEN ADJUST and selects it; once entered as the bounce off
 * BACKUP CLEAR (mnOptionInitVars's own scene_prev read, the same route
 * mndata.h documents for its own unported children), which checks the
 * cursor starts there and B exits to the mode select.
 *
 * The sound toggle's L and R checks are driven through the stick here,
 * not L_TRIG/R_TRIG: mnOptionFuncRun's own
 * `TapButtons(...) || CheckStickInputLR(stick_range, ...)` leaves
 * stick_range unread whenever the tap side of the OR already satisfies
 * it, and the branch that follows still folds stick_range into
 * sMNOptionOptionChangeWait -- mnOptionSetOptionSpriteColors' own
 * `default` arm is the same kind of thing elsewhere in this file. Going
 * through the stick side of the OR is what makes stick_range hold a
 * real value here rather than whatever the host's stack has that tic;
 * A's own flip reads no such value, so it is exercised with a tap.
 */
extern u32 dMNOptionFileIDs[];
extern sb32 dSYAudioSoundQuality;

static SObj *option_find(s32 link, int file, u32 offset)
{
    return scene_sprite_find(sMNOptionFiles, link, file, offset);
}

/* the i'th SObj of an option tab's GObj: 0..2 are the tab's left,
 * middle and right pieces (mnOptionMakeOptionTabs) and 3 is its label --
 * the same shape data_tab_sobj walks for the DATA menu's own tabs. */
static SObj *option_tab_sobj(GObj *gobj, int i)
{
    SObj *sobj = (gobj != NULL) ? SObjGetStruct(gobj) : NULL;

    while (sobj != NULL && i-- > 0)
    {
        sobj = sobj->next;
    }
    return sobj;
}

static void option_check_tab(GObj *gobj, s32 status)
{
    static const u8 kPrim[3][3] =
    {
        { 0x00, 0x00, 0x00 },       /* nMNOptionTabStatusNot */
        { 0x82, 0x00, 0x28 },       /* nMNOptionTabStatusHighlight */
        { 0x00, 0x00, 0x00 }        /* nMNOptionTabStatusSelected */
    };
    static const u8 kEnv[3][3] =
    {
        { 0x82, 0x82, 0xAA },
        { 0xFF, 0x00, 0x28 },
        { 0xFF, 0xFF, 0xFF }
    };
    int i;

    CHECK(gobj != NULL);
    for (i = 0; i < 3; i++)
    {
        SObj *sobj = option_tab_sobj(gobj, i);

        CHECK(sobj != NULL);
        if (sobj == NULL)
        {
            return;
        }
        CHECK(sobj->envcolor.r == kPrim[status][0]);
        CHECK(sobj->envcolor.g == kPrim[status][1]);
        CHECK(sobj->envcolor.b == kPrim[status][2]);
        CHECK(sobj->sprite.red == kEnv[status][0]);
        CHECK(sobj->sprite.green == kEnv[status][1]);
        CHECK(sobj->sprite.blue == kEnv[status][2]);
    }
}

static int gOptionSeen[8];
static int gOptionRun;

static void option_tic(void)
{
    s32 tic = (s32)dSYTaskmanUpdateCount;
    SYController *pad = &gSYControllerDevices[0];
    SObj *sobj;

    pad->button_tap = 0;
    pad->button_hold = 0;
    pad->stick_range.x = 0;
    pad->stick_range.y = 0;

    if (gOptionRun != 0)
    {
        /* run two: the bounce off BACKUP CLEAR -- just the cursor
         * placement and the way out */
        if (tic == 3)
        {
            CHECK(sMNOptionOption == nMNOptionOptionBackupClear);
            option_check_tab(sMNOptionOptionBackupClearGObj, nMNOptionTabStatusHighlight);
            option_check_tab(sMNOptionOptionSoundGObj, nMNOptionTabStatusNot);
            option_check_tab(sMNOptionOptionScreenAdjustGObj, nMNOptionTabStatusNot);
            gOptionSeen[6] = 1;
        }
        if (tic == 12)
        {
            pad->button_tap = B_BUTTON;
            gOptionSeen[7] = 1;
        }
        return;
    }

    /* run one: entered from the mode select. The GObjs below are found
     * by the link gcMakeGObjSPAfter gave each -- 2 for the decals, 3 for
     * the labels -- not the display link gcAddGObjDisplay draws them on;
     * data_find (mndata.c's own test) walks the same list the same way. */
    if (tic == 3)
    {
        sobj = option_find(2, 0, 0x18000);
        CHECK(sobj != NULL && sobj->pos.x == 10.0f && sobj->pos.y == 10.0f);
        CHECK(sobj != NULL && !(sobj->sprite.attr & SP_TRANSPARENT));
        sobj = option_find(2, 1, 0x0B958);                  /* settings icon, dark */
        CHECK(sobj != NULL);
        if (sobj != NULL)
        {
            CHECK_EQF(sobj->pos.x, 10.0f);
            CHECK_EQF(sobj->pos.y, 10.0f);
            CHECK(sobj->sprite.red == 0x99 && sobj->sprite.green == 0x99 &&
                  sobj->sprite.blue == 0x99);
        }

        /* the labels, over the orange panel */
        sobj = option_find(3, 0, 0x031F8);                  /* the logo */
        CHECK(sobj != NULL && sobj->pos.x == 235.0f && sobj->pos.y == 158.0f);
        sobj = option_find(3, 1, 0x09288);                  /* OPTION */
        CHECK(sobj != NULL && sobj->pos.x == 201.0f && sobj->pos.y == 120.0f);

        /* entered from the mode select, not a bounce off either child,
         * so the cursor is on SOUND (mnOptionInitVars's default arm) */
        CHECK(sMNOptionOption == nMNOptionOptionSound);
        option_check_tab(sMNOptionOptionSoundGObj, nMNOptionTabStatusHighlight);
        option_check_tab(sMNOptionOptionScreenAdjustGObj, nMNOptionTabStatusNot);
        option_check_tab(sMNOptionOptionBackupClearGObj, nMNOptionTabStatusNot);
        sobj = option_tab_sobj(sMNOptionOptionSoundGObj, 0);
        CHECK(sobj != NULL && sobj->pos.x == 113.0f && sobj->pos.y == 42.0f);
        sobj = option_tab_sobj(sMNOptionOptionScreenAdjustGObj, 0);
        CHECK(sobj != NULL && sobj->pos.x == 91.0f && sobj->pos.y == 89.0f);
        sobj = option_tab_sobj(sMNOptionOptionBackupClearGObj, 0);
        CHECK(sobj != NULL && sobj->pos.x == 69.0f && sobj->pos.y == 136.0f);

        /* the toggle starts stereo (dSYAudioSoundQuality == 1 by
         * default, mnOptionInitVars): STEREO white, MONO grey */
        CHECK(sMNOptionSoundMonoOrStereo == 1);
        sobj = option_tab_sobj(sMNOptionSoundOptionGObj, 0);        /* STEREO */
        CHECK(sobj != NULL && sobj->pos.x == 179.0f && sobj->pos.y == 48.0f);
        CHECK(sobj != NULL && sobj->sprite.red == 0xFF && sobj->sprite.green == 0xFF &&
              sobj->sprite.blue == 0xFF);
        sobj = option_tab_sobj(sMNOptionSoundOptionGObj, 1);        /* MONO */
        CHECK(sobj != NULL && sobj->pos.x == 236.0f && sobj->pos.y == 48.0f);
        CHECK(sobj != NULL && sobj->sprite.red == 0x32 && sobj->sprite.green == 0x32 &&
              sobj->sprite.blue == 0x32);
        sobj = option_tab_sobj(sMNOptionSoundOptionGObj, 2);        /* the slash */
        CHECK(sobj != NULL && sobj->pos.x == 229.0f && sobj->pos.y == 48.0f);

        /* what tic 4 reads below is the fill from THIS frame's draw, not
         * every sprite blit logged getting here (data_tic's own tic 3
         * does the same reset, right before its tic 4 reads the log). */
        gLBCommonSpriteQuadLogCount = 0;
        gOptionSeen[0] = 1;
    }
    if (tic == 4)
    {
        /* mnOptionLabelsProcDisplay's orange fill, and
         * mnOptionSoundUnderlineProcDisplay's white one under STEREO's
         * half (rect[1]) since SOUND is highlighted and the toggle is
         * still on STEREO -- both at LB_SPRITE_SCREEN_SCALE (2x) over
         * the decomp's own game-pixel corners. */
        int i, orange = 0, white = 0;

        for (i = 0; i < gLBCommonSpriteQuadLogCount && i < LB_SPRITE_QUAD_LOG_MAX; i++)
        {
            const LBCommonSpriteQuad *q = &gLBCommonSpriteQuadLog[i];

            if (q->rect.copy != -1)
            {
                continue;
            }
            if (q->argb == 0xE6A07814)
            {
                orange++;
                CHECK_EQF(q->x0, 450.0f);
                CHECK_EQF(q->y0, 286.0f);
                CHECK_EQF(q->x1, 620.0f);
                CHECK_EQF(q->y1, 460.0f);
            }
            else if (q->argb == 0xFFFFFFFF)
            {
                white++;
                CHECK_EQF(q->x0, 358.0f);
                CHECK_EQF(q->y0, 128.0f);
                CHECK_EQF(q->x1, 450.0f);
                CHECK_EQF(q->y1, 128.0f);
            }
        }
        CHECK(orange == 1);
        CHECK(white == 1);
        gOptionSeen[1] = 1;
    }
    /* the scene is deaf for ten tics (mnOptionFuncRun). Every press below
     * sits on its own tic with one neutral tic after it, the same rhythm
     * data_tic and soundtest_tic use and for the same reason: the L/R
     * stick presses fold into sMNOptionOptionChangeWait exactly like
     * U/D does (they share the one wait counter), so a second stick
     * press one tic after the first reads as a continued hold and is
     * swallowed, the wait already nonzero. A itself never touches that
     * counter, so it alone could chain tic to tic -- it does not here,
     * to keep every press's result checkable before the next one lands. */
    if (tic == 12)
    {
        pad->stick_range.x = 30;                             /* R: stereo -> mono */
    }
    if (tic == 14)
    {
        CHECK(sMNOptionSoundMonoOrStereo == 0);
        CHECK(dSYAudioSoundQuality == 0);
        sobj = option_tab_sobj(sMNOptionSoundOptionGObj, 0);
        CHECK(sobj != NULL && sobj->sprite.red == 0x32 && sobj->sprite.green == 0x32 &&
              sobj->sprite.blue == 0x32);
        sobj = option_tab_sobj(sMNOptionSoundOptionGObj, 1);
        CHECK(sobj != NULL && sobj->sprite.red == 0xFF && sobj->sprite.green == 0xFF &&
              sobj->sprite.blue == 0xFF);
        gOptionSeen[2] = 1;
        pad->stick_range.x = -30;                            /* L: mono -> stereo */
    }
    if (tic == 16)
    {
        CHECK(sMNOptionSoundMonoOrStereo == 1);
        CHECK(dSYAudioSoundQuality == 1);
        gOptionSeen[3] = 1;
        pad->button_tap = A_BUTTON;                          /* A: stereo -> mono */
    }
    if (tic == 17)
    {
        CHECK(sMNOptionSoundMonoOrStereo == 0);
        CHECK(dSYAudioSoundQuality == 0);
        gOptionSeen[4] = 1;
        pad->button_hold = D_JPAD;                           /* SOUND -> SCREEN ADJUST */
    }
    if (tic == 19)
    {
        CHECK(sMNOptionOption == nMNOptionOptionScreenAdjust);
        option_check_tab(sMNOptionOptionScreenAdjustGObj, nMNOptionTabStatusHighlight);
        option_check_tab(sMNOptionOptionSoundGObj, nMNOptionTabStatusNot);
        gOptionSeen[5] = 1;
        pad->button_tap = A_BUTTON;                          /* select SCREEN ADJUST */
    }
    if (tic == 20)
    {
        /* A on SCREEN ADJUST last tic: its tab went white, the backup
         * took the toggle's final value, and the scene asked for that
         * screen (mnOptionFuncRun's own switch never calls
         * syTaskmanSetLoadScene the same tic it sets scene_curr, unlike
         * B's own branch below -- the next tic's
         * sMNOptionIsProceedScene check is what ends the loop). */
        option_check_tab(sMNOptionOptionScreenAdjustGObj, nMNOptionTabStatusSelected);
        CHECK(sMNOptionIsProceedScene != FALSE);
        CHECK(gSCManagerSceneData.scene_prev == nSCKindOption);
        CHECK(gSCManagerSceneData.scene_curr == nSCKindScreenAdjust);
        CHECK(gSCManagerBackupData.sound_mono_or_stereo == sMNOptionSoundMonoOrStereo);
        CHECK(gSCManagerBackupData.is_allow_screenflash == sMNOptionIsScreenFlash);
    }
}

static void test_option_menu(void)
{
    sb32 saved_quality = dSYAudioSoundQuality;
    int i;

    sprite_bank_release_all();
    memset(gOptionSeen, 0, sizeof(gOptionSeen));

    /* run one: entered from the mode select */
    gOptionRun = 0;
    dSYAudioSoundQuality = 1;
    gSCManagerSceneData.scene_prev = nSCKindModeSelect;
    gSCManagerSceneData.scene_curr = nSCKindOption;

    gSceneTicHook = option_tic;
    syDmaLoadOverlay(OVERLAY_SUBSYS);
    syDmaLoadOverlay(OVERLAY_OPTION);
    mnOptionStartScene();
    gSceneTicHook = NULL;

    /* the two sprite banks are the ones the scene asked for */
    CHECK(dMNOptionFileIDs[0] == 0 && dMNOptionFileIDs[1] == 4);
    CHECK(sMNOptionFiles[0] != NULL && sMNOptionFiles[1] != NULL);
    CHECK((s32)dSYTaskmanUpdateCount == 21);

    /* run two: the bounce off BACKUP CLEAR */
    gOptionRun = 1;
    gSCManagerSceneData.scene_prev = nSCKindBackupClear;
    gSCManagerSceneData.scene_curr = nSCKindOption;

    gSceneTicHook = option_tic;
    syDmaLoadOverlay(OVERLAY_SUBSYS);
    syDmaLoadOverlay(OVERLAY_OPTION);
    mnOptionStartScene();
    gSceneTicHook = NULL;

    CHECK(gSCManagerSceneData.scene_prev == nSCKindOption);
    CHECK(gSCManagerSceneData.scene_curr == nSCKindModeSelect);

    for (i = 0; i < 8; i++)
    {
        CHECK(gOptionSeen[i]);
    }

    /* the overlay reload is what makes the next scene's layout its own:
     * every static above is back to zero before it */
    mnOptionOverlayLoad();
    CHECK(sMNOptionOption == 0 && sMNOptionSoundMonoOrStereo == 0);
    CHECK(sMNOptionFiles[0] == NULL && sMNOptionFiles[1] == NULL);
    CHECK(sMNOptionOptionSoundGObj == NULL && sMNOptionSoundOptionGObj == NULL);

    dSYAudioSoundQuality = saved_quality;
}

/* ---- SCREEN ADJUST -----------------------------------------
 *
 * Options' first child. No shared "change wait" counter gates its stick
 * reads the way mnOptionOptionChangeWait does (mnscreenadjust.c's own
 * note on sMNScreenAdjustButtonHoldWait: nothing in this file ever sets
 * it above zero, so every press here needs only the one neutral tic the
 * pad reset at the top of every hook call already gives it, not the
 * two-tic settle test_option_menu's own stick presses need).
 */
extern u32 dMNScreenAdjustFileIDs[];
extern s16 gSYVideoOffsetLeft, gSYVideoOffsetTop;
void syVideoSetCenterOffsets(s16 left, s16 right, s16 top, s16 bottom);

static SObj *screenadjust_find(s32 link, int file, u32 offset)
{
    return scene_sprite_find(sMNScreenAdjustFiles, link, file, offset);
}

static int gScreenAdjustSeen[8];

static void screenadjust_tic(void)
{
    s32 tic = (s32)dSYTaskmanUpdateCount;
    SYController *pad = &gSYControllerDevices[0];
    SObj *sobj;

    pad->button_tap = 0;
    pad->button_hold = 0;
    pad->stick_range.x = 0;
    pad->stick_range.y = 0;

    if (tic == 3)
    {
        /* the two GObjs gcMakeGObjSPAfter put on link 3
         * (mnScreenAdjustMakeGuide, mnScreenAdjustMakeInstruction) --
         * the crosshair/border GObj is link 2 and draws no SObj of its
         * own (mnScreenAdjustFrameProcDisplay's own note), checked via
         * the quad log at tic 4 below instead. */
        sobj = screenadjust_find(3, 0, 0x098A0);                    /* Guide */
        CHECK(sobj != NULL && sobj->pos.x == 10.0f && sobj->pos.y == 10.0f);

        sobj = screenadjust_find(3, 0, 0x00918);                    /* Instruction */
        CHECK(sobj != NULL);
        if (sobj != NULL)
        {
            CHECK_EQF(sobj->pos.x, 16.0f);
            CHECK_EQF(sobj->pos.y, 198.0f);
            CHECK(sobj->sprite.attr & SP_TRANSPARENT);
            CHECK(sobj->sprite.red == 0xFF && sobj->sprite.green == 0xFF &&
                  sobj->sprite.blue == 0xFF);
        }

        CHECK_EQF(sMNScreenAdjustOffsetH, 0.0f);
        CHECK_EQF(sMNScreenAdjustOffsetV, 0.0f);

        /* what tic 4 reads below is the fill from THIS frame's draw
         * (option_tic's own tic 3 does the same reset before its tic
         * 4's read) */
        gLBCommonSpriteQuadLogCount = 0;
        gScreenAdjustSeen[0] = 1;
    }
    if (tic == 4)
    {
        /* mnScreenAdjustFrameProcDisplay's six fills, both colours at
         * LB_SPRITE_SCREEN_SCALE (2x) over the decomp's own game-pixel
         * corners -- G_CYC_1CYCLE like mnOptionLabelsProcDisplay's own,
         * so the numbers are the decomp's unchanged (no +1). Drawn every
         * frame (this GObj is never ejected/remade), so any tic past
         * the reset above would show the same six. */
        static const struct { f32 x0, y0, x1, y1; u32 argb; } kRects[6] =
        {
            { 318.0f,   0.0f, 322.0f, 508.0f, 0xFFBFA447 },
            {   0.0f, 238.0f, 668.0f, 242.0f, 0xFFBFA447 },
            {  88.0f,  88.0f, 552.0f,  90.0f, 0xFF8B8B8B },
            {  88.0f, 392.0f, 552.0f, 394.0f, 0xFF8B8B8B },
            {  88.0f,  88.0f,  90.0f, 392.0f, 0xFF8B8B8B },
            { 552.0f,  88.0f, 554.0f, 392.0f, 0xFF8B8B8B },
        };
        int matched[6] = { 0 };
        int i, j, count = 0;

        for (i = 0; i < gLBCommonSpriteQuadLogCount && i < LB_SPRITE_QUAD_LOG_MAX; i++)
        {
            const LBCommonSpriteQuad *q = &gLBCommonSpriteQuadLog[i];

            if (q->rect.copy != -1)
            {
                continue;
            }
            for (j = 0; j < 6; j++)
            {
                if (matched[j] || q->argb != kRects[j].argb)
                {
                    continue;
                }
                if (q->x0 == kRects[j].x0 && q->y0 == kRects[j].y0 &&
                    q->x1 == kRects[j].x1 && q->y1 == kRects[j].y1)
                {
                    matched[j] = 1;
                    count++;
                    break;
                }
            }
        }
        CHECK(count == 6);
        gScreenAdjustSeen[1] = 1;
    }
    /* the scene is deaf for ten tics (mnScreenAdjustFuncRun); the first
     * press waits until tic 12 to stay comfortably clear of that gate,
     * the same margin test_option_menu's own first press keeps. */
    if (tic == 12)
    {
        pad->button_tap = D_JPAD;                            /* V: 0 -> 1 */
    }
    if (tic == 13)
    {
        CHECK_EQF(sMNScreenAdjustOffsetV, 1.0f);
        CHECK(gSYVideoOffsetTop == 1);
        gScreenAdjustSeen[2] = 1;

        pad->button_tap = U_CBUTTONS;                         /* V: 1 -> 0 */
    }
    if (tic == 14)
    {
        CHECK_EQF(sMNScreenAdjustOffsetV, 0.0f);
        gScreenAdjustSeen[3] = 1;

        pad->button_tap = R_TRIG;                             /* H: 0 -> 1 */
    }
    if (tic == 15)
    {
        CHECK_EQF(sMNScreenAdjustOffsetH, 1.0f);
        CHECK(gSYVideoOffsetLeft == 1);
        gScreenAdjustSeen[4] = 1;

        pad->button_tap = L_JPAD;                             /* H: 1 -> 0 */
    }
    if (tic == 16)
    {
        CHECK_EQF(sMNScreenAdjustOffsetH, 0.0f);
        gScreenAdjustSeen[5] = 1;

        pad->stick_range.x = 40;                       /* R, continuing */
        pad->stick_range.y = -40;                       /* D, continuing */
    }
    if (tic == 17)
    {
        CHECK_EQF(sMNScreenAdjustOffsetH, 40.0f / 50.0f);
        CHECK_EQF(sMNScreenAdjustOffsetV, 40.0f / 50.0f);
        gScreenAdjustSeen[6] = 1;

        pad->button_tap = Z_TRIG;                             /* reset */
    }
    if (tic == 18)
    {
        CHECK_EQF(sMNScreenAdjustOffsetH, 0.0f);
        CHECK_EQF(sMNScreenAdjustOffsetV, 0.0f);
        gScreenAdjustSeen[7] = 1;

        pad->button_tap = A_BUTTON;      /* back to Options, ends the run */
    }
}

static void test_screenadjust_menu(void)
{
    s16 saved_left = gSYVideoOffsetLeft, saved_top = gSYVideoOffsetTop;
    int i;

    sprite_bank_release_all();
    memset(gScreenAdjustSeen, 0, sizeof(gScreenAdjustSeen));
    syVideoSetCenterOffsets(0, 0, 0, 0);

    gSCManagerSceneData.scene_prev = nSCKindOption;
    gSCManagerSceneData.scene_curr = nSCKindScreenAdjust;

    gSceneTicHook = screenadjust_tic;
    syDmaLoadOverlay(OVERLAY_BATTLE);
    syDmaLoadOverlay(OVERLAY_SUBSYS);
    syDmaLoadOverlay(OVERLAY_SCREENADJUST);
    mnScreenAdjustStartScene();
    gSceneTicHook = NULL;

    CHECK(dMNScreenAdjustFileIDs[0] == 15);
    CHECK(sMNScreenAdjustFiles[0] != NULL);

    for (i = 0; i < 8; i++)
    {
        CHECK(gScreenAdjustSeen[i]);
    }

    /* A on tic 18 calls syTaskmanSetLoadScene that same tic
     * (mnScreenAdjustFuncRun's A/B/START branch, unlike
     * mnOptionFuncRun's own SCREEN ADJUST/BACKUP CLEAR arms, which defer
     * a tic) -- but the loop itself still takes one more hook call to
     * unwind, the same +1 test_item_switch_row's own A-press shows (its
     * own press at tic 12, its own final count 13), so 19 is the last
     * tic actually reached, and the backup/return checks are made here
     * rather than in a hook call at 19 that does nothing. */
    CHECK((s32)dSYTaskmanUpdateCount == 19);
    CHECK(gSCManagerSceneData.scene_prev == nSCKindScreenAdjust);
    CHECK(gSCManagerSceneData.scene_curr == nSCKindOption);
    CHECK(gSCManagerBackupData.screen_adjust_h == 0);
    CHECK(gSCManagerBackupData.screen_adjust_v == 0);

    /* the overlay reload is what makes the next scene's layout its own */
    mnScreenAdjustOverlayLoad();
    CHECK_EQF(sMNScreenAdjustOffsetH, 0.0f);
    CHECK(sMNScreenAdjustFiles[0] == NULL);

    syVideoSetCenterOffsets(saved_left, saved_left, saved_top, saved_top);
}

/* ---- BACKUP CLEAR ------------------------------------------
 *
 * Options' second child. A tab-move (D, held) to 1P HIGH SCORE, a confirm
 * dialog opened and cancelled with B (mnBackupClearUpdateOptionConfirmMenu's
 * immediate-return arm, no flash), the same dialog reopened, R to move the
 * pick off its default (No) onto Yes, then A to apply -- which is the one
 * path this test cares most about proving real: lbBackupClear1PHighScore
 * (lb/lbbackup.c, compiled unmodified) actually clears
 * gSCManagerBackupData, not a stand-in. The 60-tic confirm flash
 * (sMNBackupClearOptionConfirmAnimLength) is let run to its own end so the
 * final B-exit is checked against a menu that has actually returned to its
 * plain six-tab display, the same discipline test_option_menu's own
 * confirm-adjacent waits take.
 */
extern u32 dMNBackupClearFileIDs[];

static int gBackupClearSeen[6];

static void backupclear_tic(void)
{
    s32 tic = (s32)dSYTaskmanUpdateCount;
    SYController *pad = &gSYControllerDevices[0];

    pad->button_tap = 0;
    pad->button_hold = 0;
    pad->stick_range.x = 0;
    pad->stick_range.y = 0;

    /* the scene is deaf for ten tics (sMNBackupClearUpdateWait, set to
     * 10 by mnBackupClearInitVars); the first press waits until tic 12
     * to stay comfortably clear of that gate, test_screenadjust_menu's
     * own margin. */
    if (tic == 12)
    {
        CHECK(sMNBackupClearOption == nMNBackupClearOptionNewcomers);
        CHECK(SObjGetStruct(sMNBackupClearOptionNewcomersGObj)->sprite.red == 0xFF);
        gBackupClearSeen[0] = 1;

        pad->button_hold = D_JPAD;      /* main-menu nav reads HOLD, not tap */
    }
    if (tic == 13)
    {
        /* the D held at tic 12 landed: the highlight moved off Newcomers
         * (mnBackupClearUpdateOptionTabColors' "not" colour, 0x7D) onto
         * 1P High Score (its "highlight" colour, 0xFF) */
        CHECK(sMNBackupClearOption == nMNBackupClearOption1PHighScore);
        CHECK(SObjGetStruct(sMNBackupClearOptionNewcomersGObj)->sprite.red == 0x7D);
        CHECK(SObjGetStruct(sMNBackupClearOption1PHighScoreGObj)->sprite.red == 0xFF);
        gBackupClearSeen[1] = 1;

        pad->button_tap = A_BUTTON;     /* opens the confirm dialog */
    }
    if (tic == 24)
    {
        /* the confirm dialog is its own ten-tic-deaf window
         * (sMNBackupClearUpdateWait reset to 10 by the A above); this is
         * its first live read. It opens on No (yes_or_no 1). */
        CHECK(sMNBackupClearOptionMenuKind == 1);
        CHECK(sMNBackupClearOptionConfirmYesOrNo == 1);
        CHECK(sMNBackupClearOptionConfirmGObj != NULL);
        gBackupClearSeen[2] = 1;

        pad->button_tap = B_BUTTON;     /* cancels: no clear, no flash */
    }
    if (tic == 35)
    {
        /* B's cancel is immediate (mnBackupClearUpdateOptionConfirmMenu's
         * B arm), but still resets the ten-tic wait before the main
         * menu reads input again -- this is that first live read. The
         * cursor is still on 1P High Score; B did not move it. */
        CHECK(sMNBackupClearOptionMenuKind == 0);
        CHECK(sMNBackupClearOption == nMNBackupClearOption1PHighScore);
        gBackupClearSeen[3] = 1;

        pad->button_tap = A_BUTTON;     /* reopens the confirm dialog */
    }
    if (tic == 46)
    {
        /* the reopened dialog's own ten-tic wait, first live read; still
         * opens on No. R (a tap, not gated by this wait at all -- only
         * the main menu's U/D and the stick paths are) moves it to Yes. */
        CHECK(sMNBackupClearOptionConfirmYesOrNo == 1);
        gBackupClearSeen[4] = 1;

        pad->button_tap = R_JPAD;
    }
    if (tic == 47)
    {
        LBBackupData *bd = &gSCManagerBackupData;

        CHECK(sMNBackupClearOptionConfirmYesOrNo == 0);       /* Yes now */

        /* dirtied here, one tic before the apply, so the tic-48 check
         * below is proof of the real clear and not a coincidence of the
         * default already sitting there */
        bd->spgame_records[0].spgame_hiscore = 999999;
        bd->spgame_records[0].spgame_continues = 9;

        pad->button_tap = A_BUTTON;     /* confirms: applies, then flashes */
    }
    if (tic == 48)
    {
        /* mnBackupClearUpdateOptionConfirmMenu's case 0 ran inside tic
         * 47's own FuncRun call -- mnBackupClearApplyOptionID(1PHighScore)
         * -> lbBackupClear1PHighScore(), lb/lbbackup.c compiled
         * unmodified -- so the dirtied fields are already the real
         * defaults' by this, the very next tic. */
        CHECK(gSCManagerBackupData.spgame_records[0].spgame_hiscore ==
              dSCManagerDefaultBackupData.spgame_records[0].spgame_hiscore);
        CHECK(gSCManagerBackupData.spgame_records[0].spgame_continues ==
              dSCManagerDefaultBackupData.spgame_records[0].spgame_continues);
        CHECK(sMNBackupClearOptionMenuKind == 0);

        /* AnimLength was set to 60 during tic 47's own FuncRun call; the
         * decrement only happens the next time FuncRun runs with it
         * already nonzero at entry -- tic 48's own call, which this hook
         * fires before (the hook runs at the start of the tic, ahead of
         * that tic's FuncRun -- see mnbackupclear.h). So the value read
         * here is still 60; the first decrement (60 -> 59) shows up at
         * tic 49's hook instead, one tic later than the "landed" checks
         * above this one, which reflect the *previous* tic's FuncRun. */
        CHECK(sMNBackupClearOptionConfirmAnimLength == 60);
        gBackupClearSeen[5] = 1;

        /* no press: the 60-tic confirm flash (started at tic 47) is left
         * to run to its own end, tic 106, before the next press */
    }
    if (tic == 108)
    {
        /* the flash ended at tic 106 (mnBackupClearFuncRun's own
         * ConfirmAnimLength == 0 arm ejects the confirm GObj and calls
         * mnBackupClearSetOptionSpriteColors); this, the main menu's
         * next live read, is the first tic that could prove it -- and
         * the last press of the run. */
        CHECK(sMNBackupClearOptionConfirmAnimLength == 0);

        pad->button_tap = B_BUTTON;     /* back to Options, ends the run */
    }
}

static void test_backupclear_menu(void)
{
    int i;

    sprite_bank_release_all();
    memset(gBackupClearSeen, 0, sizeof(gBackupClearSeen));

    gSCManagerSceneData.scene_prev = nSCKindOption;
    gSCManagerSceneData.scene_curr = nSCKindBackupClear;

    gSceneTicHook = backupclear_tic;
    syDmaLoadOverlay(OVERLAY_BATTLE);
    syDmaLoadOverlay(OVERLAY_SUBSYS);
    syDmaLoadOverlay(OVERLAY_BACKUPCLEAR);
    mnBackupClearStartScene();
    gSceneTicHook = NULL;

    CHECK(dMNBackupClearFileIDs[0] == 0);
    CHECK(dMNBackupClearFileIDs[1] == 77);
    CHECK(dMNBackupClearFileIDs[2] == 78);
    CHECK(sMNBackupClearFiles[0] != NULL);
    CHECK(sMNBackupClearFiles[1] != NULL);
    CHECK(sMNBackupClearFiles[2] != NULL);

    for (i = 0; i < 6; i++)
    {
        CHECK(gBackupClearSeen[i]);
    }

    /* B on tic 108 calls syTaskmanSetLoadScene that same tic
     * (mnBackupClearUpdateOptionMainMenu's B branch), but the loop
     * itself still takes one more hook call to unwind -- the same +1
     * test_screenadjust_menu's own A-press shows -- so 109 is the last
     * tic actually reached. */
    CHECK((s32)dSYTaskmanUpdateCount == 109);
    CHECK(gSCManagerSceneData.scene_prev == nSCKindBackupClear);
    CHECK(gSCManagerSceneData.scene_curr == nSCKindOption);

    /* the overlay reload is what makes the next scene's layout its own */
    mnBackupClearOverlayLoad();
    CHECK(sMNBackupClearOption == 0);
    CHECK(sMNBackupClearFiles[0] == NULL);
}

/* ---- CONGRATULATIONS ---------------------------------------
 *
 * Reached only from 1P mode or Debug Battle, neither ported yet, so this
 * scripts scene_prev/fkind directly the way a real run would leave them
 * (mnCongraStartScene's own switch) -- nSCKind1PGame with fkind Fox, to
 * prove the per-fighter bank table picks something other than the Mario
 * default. A tap (A) after the 8-tic skip wait starts the 90-tic fade
 * (lbFadeMakeActor, sLBFadeLength = fade_length + 2 -- src/dc/lbfade.c);
 * when it ends, sMNCongraIsProceedScene flips and mnCongraFuncDraw's own
 * 5-tic sMNCongraSceneChangeWait counts down to the scene_curr write.
 */
static int gCongraSeen[2];

static void congra_tic(void)
{
    s32 tic = (s32)dSYTaskmanUpdateCount;
    SYController *pad = &gSYControllerDevices[0];

    pad->button_tap = 0;
    pad->button_hold = 0;
    pad->stick_range.x = 0;
    pad->stick_range.y = 0;

    if (tic == 1)
    {
        /* mnCongraFuncStart already ran (it is the task's start
         * function, called before the tic loop this hook rides), so
         * the fighter pick and its two banks are already resolved. */
        CHECK(sMNCongraFighterKind == nFTKindFox);
        CHECK(sMNCongraFiles[0] != NULL);
        CHECK(sMNCongraFiles[1] != NULL);
        CHECK(sMNCongraIsProceed == FALSE);
        gCongraSeen[0] = 1;

        pad->button_tap = A_BUTTON | START_BUTTON;    /* before the wait */
    }
    if (tic == 2)
    {
        /* sMNCongraSkipWait is 8: the tap above lands inside the deaf
         * window (mnCongraActorFuncRun decrements first, then checks
         * for ==0, so the earliest a tap can take is the 8th call) and
         * is dropped -- proceed is still false. */
        CHECK(sMNCongraIsProceed == FALSE);
    }
    if (tic == 12)
    {
        /* comfortably clear of the 8-tic window */
        pad->button_tap = A_BUTTON;
    }
    if (tic == 13)
    {
        CHECK(sMNCongraIsProceed != FALSE);
        gCongraSeen[1] = 1;
    }
}

static void test_congra_menu(void)
{
    int i;

    sprite_bank_release_all();
    memset(gCongraSeen, 0, sizeof(gCongraSeen));

    gSCManagerSceneData.scene_prev = nSCKind1PGame;
    gSCManagerSceneData.fkind = nFTKindFox;
    gSCManagerSceneData.scene_curr = nSCKindCongra;

    gSceneTicHook = congra_tic;
    syDmaLoadOverlay(OVERLAY_CONGRA);
    mnCongraStartScene();
    gSceneTicHook = NULL;

    for (i = 0; i < 2; i++)
    {
        CHECK(gCongraSeen[i]);
    }

    /* the fade (92 tics from the tic-12 press) then the 5-tic
     * sMNCongraSceneChangeWait, then the scene-kind write -- confirmed
     * against the run rather than derived, the same discipline
     * test_screenadjust_menu's and test_backupclear_menu's own final
     * tic counts took. */
    CHECK(gSCManagerSceneData.scene_prev == nSCKindCongra);
    CHECK(gSCManagerSceneData.scene_curr == nSCKindTitle);

    /* the overlay reload is what makes the next scene's layout its own */
    mnCongraOverlayLoad();
    CHECK(sMNCongraFighterKind == 0);
    CHECK(sMNCongraFiles[0] == NULL);
}

/* ---- NO CONTROLLER (the last of the menu scenes) ----------
 *
 * mn/mncommon/mnnocontroller.c is the "please insert a controller"
 * screen: the decomp writes gSCManagerSceneData.scene_curr =
 * nSCKindNoController exactly once, at boot, only when
 * gSYControllerConnectedNum == 0 (sc/scmanager.c:862-864), before the
 * scene loop even starts. Once inside the scene there is no exit of any
 * kind -- mnNoControllerFuncStart makes two GObjs, both with a NULL
 * update function, and nothing in the file ever writes scene_curr or
 * calls syTaskmanSetLoadScene() again (src/dc/mnnocontroller.h's own
 * header note). mnNoControllerStartScene therefore blocks forever, on
 * the target exactly as the decomp's own NoController screen leaves the
 * N64 stuck until it is power-cycled with a pad plugged in -- so unlike
 * every other menu test, this one never calls StartScene. It calls the
 * same two pieces syTaskmanStartTask itself calls -- syTaskmanSetupPools
 * then the scene's func_start -- without the blocking frame loop after
 * them (the same syTaskmanSetupPools-without-syTaskmanStartTask idiom
 * load_stage already uses above, for the same reason: the object system,
 * without a scene driving it). What there is to check is correspondingly
 * small: the scene starts, its one sprite is where
 * mnNoControllerMakeImage puts it, and driving gcRunAll/gcDrawAll by
 * hand for a few tics -- standing in for "the screen sits there" on
 * hardware -- neither crashes nor writes scene_curr out from under the
 * test, which is exactly what a scene with no update function anywhere
 * in it predicts. */
static SObj *nocontroller_find(s32 link, u32 offset)
{
    return scene_sprite_find(gMNNoControllerFiles, link, 0, offset);
}

static void test_nocontroller_menu(void)
{
    SObj *sobj;
    s32 scene_before;
    int i;

    sprite_bank_release_all();

    gSCManagerSceneData.scene_curr = scene_before = nSCKindNoController;

    syTaskmanSetupPools(&dMNNoControllerTaskmanSetup);
    mnNoControllerFuncStart();

    CHECK(gMNNoControllerFiles[0] != NULL);

    /* 0x08460, llMNNoControllerSprite (src/dc/mnnocontroller.c); link 1
     * is gcMakeGObjSPAfter's own third argument in mnNoControllerMakeImage,
     * not gcAddGObjDisplay's DL link (the m18-front-end recipe's own trap
     * on the two link kinds). */
    sobj = nocontroller_find(1, 0x08460);
    CHECK(sobj != NULL);
    CHECK(sobj->pos.x == 10.0F);
    CHECK(sobj->pos.y == 10.0F);

    /* no GObj in this scene has an update function, so driving frames by
     * hand changes nothing -- proving that, not guessing it */
    for (i = 0; i < 120; i++)
    {
        gcRunAll();
        gcDrawAll();
    }

    CHECK(gSCManagerSceneData.scene_curr == scene_before);
    sobj = nocontroller_find(1, 0x08460);
    CHECK(sobj != NULL);

    /* the overlay reload is what makes the next scene's layout its own */
    mnNoControllerOverlayLoad();
    CHECK(gMNNoControllerFiles[0] == NULL);
}

/* the row that leads here, on the screen before: with the item switch
 * unlocked the VS options screen grows a fifth option and the cursor
 * can reach it (mnVSOptionsCheckHaveItemSwitch). Short, because
 * test_vs_options already walks that screen with the row absent. */
static int gItemSwitchRowSeen[2];

static void item_switch_row_tic(void)
{
    s32 tic = (s32)dSYTaskmanUpdateCount;
    SYController *pad = &gSYControllerDevices[0];

    pad->button_tap = 0;
    pad->button_hold = 0;
    pad->stick_range.x = 0;
    pad->stick_range.y = 0;

    if (tic == 3)
    {
        CHECK(sMNVSOptionsIsHaveItemSwitch != FALSE);
        CHECK(sMNVSOptionsLastAvailableOption == nMNVSOptionsOptionItemSwitch);
        CHECK(vs_options_find(4, 1, VSO_ITEMSWITCH) != NULL);
        /* coming back from the item switch puts the cursor on its row
         * (mnVSOptionsInitVars reads scene_prev) */
        CHECK(sMNVSOptionsOption == nMNVSOptionsOptionItemSwitch);
        gItemSwitchRowSeen[0] = 1;
    }
    /* the first ten tics are deaf, so this A goes nowhere */
    if (tic == 4)
    {
        pad->button_tap = A_BUTTON;
    }
    if (tic == 5)
    {
        CHECK(gSCManagerSceneData.scene_curr == nSCKindVSOptions);
        gItemSwitchRowSeen[1] = 1;
    }
    /* and this one takes, on the row the cursor came back onto */
    if (tic == 12)
    {
        pad->button_tap = A_BUTTON;
    }
}

static void test_item_switch_row(void)
{
    s32 i;

    memset(gItemSwitchRowSeen, 0, sizeof(gItemSwitchRowSeen));
    sprite_bank_release_all();

    gSCManagerBackupData.unlock_mask |= LBBACKUP_UNLOCK_MASK_ITEMSWITCH;
    gSCManagerSceneData.scene_prev = nSCKindVSItemSwitch;
    gSCManagerSceneData.scene_curr = nSCKindVSOptions;

    gSceneTicHook = item_switch_row_tic;
    syDmaLoadOverlay(OVERLAY_SUBSYS);
    syDmaLoadOverlay(OVERLAY_VSOPTIONS);
    mnVSOptionsStartScene();
    gSceneTicHook = NULL;

    for (i = 0; i < 2; i++)
    {
        CHECK(gItemSwitchRowSeen[i]);
    }
    /* the A on tic 12 fired the load on the same tic -- mnVSOptionsFuncRun
     * ends the run where it sets the scene, so tic 12 is the last */
    CHECK((s32)dSYTaskmanUpdateCount == 13);
    CHECK(gSCManagerSceneData.scene_prev == nSCKindVSOptions);
    CHECK(gSCManagerSceneData.scene_curr == nSCKindVSItemSwitch);
}


/* ---- the unlock message ----------------------------------
 *
 * The scene that reads the queue every other unlock path writes. It is
 * the only scene in the game whose task runs more than once per visit:
 * mnMessageStartScene walks gSCManagerSceneData.unlock_messages from
 * the front and starts one task per message, and mnMessageInitVars
 * takes each message out of the queue as it goes, so the loop's own
 * condition is what stops it. Two messages are queued here and both
 * runs are checked, because the second run is what proves the scene
 * heap and the sprite banks survive being re-taken inside one visit.
 */
static int gMessageSeen[6];
static int gMessageQuads;

static void message_tic(void)
{
    s32 tic = (s32)dSYTaskmanUpdateCount;
    /* which of the two runs this is: the queue slot mnMessageStartScene
     * is on, which is also the message being shown */
    s32 run = sMNMessageQueueID;
    GObj *gobj;
    SObj *sobj;

    if (tic == 2)
    {
        /* the scene's three sprite GObjs, on common links 2 (the
         * wallpaper), 3 (the message) and 5 (the "!"). Link 0 is
         * mnMessageFuncRun's, which draws nothing, and link 4 the tint,
         * which is a fill rectangle and has no SObj. */
        static const int links[] = { 2, 3, 5 };
        int nbitmaps = 0;
        int link;

        CHECK(run == 0 || run == 1);
        /* the message this run took out of the queue, and the slot it
         * left behind (mnMessageInitVars) */
        CHECK(sMNMessageUnlockID ==
              (run == 0 ? nLBBackupUnlockLuigi : nLBBackupUnlockItemSwitch));
        CHECK(gSCManagerSceneData.unlock_messages[run] ==
              nLBBackupUnlockEnumCount);
        /* both banks are up, on the second run as on the first: the
         * scene heap under the records was re-inited by syTaskmanStartTask
         * and the first run's VRAM was given back (src/dc/sprite.c) */
        CHECK(sMNMessageFiles[0] != NULL);
        CHECK(sMNMessageFiles[1] != NULL);
        CHECK(sprite_bank_get((SpriteBank *)sMNMessageFiles[1], 0x05300) != NULL);

        for (link = 0; link < (int)ARRAY_COUNT(links); link++)
        {
            for (gobj = gGCCommonLinks[links[link]]; gobj != NULL; gobj = gobj->link_next)
            {
                if (gobj->flags == GOBJ_FLAG_HIDDEN) continue;
                for (sobj = SObjGetStruct(gobj); sobj != NULL; sobj = sobj->next)
                {
                    nbitmaps += sobj->sprite.nbitmaps;
                }
            }
        }
        /* the collage is 304x220 in 44 strips, the "!" one, and the
         * message two -- Luigi's and the item switch's are both two */
        CHECK(nbitmaps == 47);
        gMessageQuads = nbitmaps + 1;   /* + the tint */
        gLBCommonSpriteQuadLogCount = 0;
        gMessageSeen[run * 3 + 0] = 1;
    }
    if (tic == 3)
    {
        int i, fills = 0;

        CHECK(gLBCommonSpriteQuadLogCount == gMessageQuads);
        for (i = 0; i < gLBCommonSpriteQuadLogCount && i < LB_SPRITE_QUAD_LOG_MAX; i++)
        {
            const LBCommonSpriteQuad *q = &gLBCommonSpriteQuadLog[i];

            /* one depth counter across all four cameras */
            CHECK(i == 0 || q->z > gLBCommonSpriteQuadLog[i - 1].z);
            if (q->rect.copy != -1)
            {
                continue;
            }
            /* mnMessageTintProcDisplay: blue at a quarter alpha over the
             * whole safe area, 320x240 game pixels onto 640x480 */
            fills++;
            CHECK_EQF(q->x0, 20.0f);
            CHECK_EQF(q->y0, 20.0f);
            CHECK_EQF(q->x1, 620.0f);
            CHECK_EQF(q->y1, 460.0f);
            CHECK(q->argb == 0x3F0000FF);
            /* the wallpaper's camera is priority 80 and the tint's 70,
             * so the tint is over the collage's 44 rectangles and under
             * the "!" and the message */
            CHECK(i == 44);
        }
        CHECK(fills == 1);
        gMessageSeen[run * 3 + 1] = 1;
    }
    if (tic == 118)
    {
        /* mnmessage.c:298: the scene is deaf for two seconds.
         * gSceneFeedStart has had a START edge waiting on every odd tic
         * since the first and none of them has counted -- nothing is
         * unlocked and nothing is written. */
        CHECK((gSCManagerBackupData.unlock_mask &
               (1 << sMNMessageUnlockID)) == 0);
        gMessageSeen[run * 3 + 2] = 1;
    }
}

static void test_message_scene(void)
{
    s32 i;
    LBBackupData ram;

    sprite_bank_release_all();
    memset(gMessageSeen, 0, sizeof(gMessageSeen));

    /* two messages queued and neither unlocked: Luigi, which is one of
     * the four the scene also writes into fighter_mask, and the item
     * switch, which is only a bit. The results screen queued the second
     * of them for real a moment ago; this overwrites the queue so both
     * halves of mnMessageApplyUnlock's switch are covered. */
    gSCManagerBackupData.unlock_mask &=
        ~(LBBACKUP_UNLOCK_MASK_LUIGI | LBBACKUP_UNLOCK_MASK_ITEMSWITCH);
    gSCManagerBackupData.fighter_mask &= ~(1 << nFTKindLuigi);
    gSCManagerBackupData.characters_fkind = nFTKindMario;
    for (i = 0; i < nLBBackupUnlockEnumCount; i++)
    {
        gSCManagerSceneData.unlock_messages[i] = nLBBackupUnlockEnumCount;
    }
    gSCManagerSceneData.unlock_messages[0] = nLBBackupUnlockLuigi;
    gSCManagerSceneData.unlock_messages[1] = nLBBackupUnlockItemSwitch;

    gSCManagerSceneData.scene_prev = nSCKindVSResults;
    gSCManagerSceneData.scene_curr = nSCKindMessage;

    gSceneFeedStart = 1;
    gSceneTicHook = message_tic;
    syDmaLoadOverlay(OVERLAY_BATTLE);
    syDmaLoadOverlay(OVERLAY_SUBSYS);
    syDmaLoadOverlay(OVERLAY_MESSAGE);
    mnMessageStartScene();
    gSceneTicHook = NULL;
    gSceneFeedStart = 0;

    for (i = 0; i < 6; i++)
    {
        CHECK(gMessageSeen[i]);
    }
    /* the tic hook runs at the controller read, so the frame whose read
     * is tic 119 is the frame mnMessageFuncRun's counter first reaches
     * 120 on -- and that read had a START edge waiting. Both runs are
     * that long, and the second run's clock started again at zero,
     * because syTaskmanStartTask is a task each time. */
    CHECK((s32)dSYTaskmanUpdateCount == 120);
    /* the loop stopped on the third slot, which is where the queue's
     * "nothing here" is */
    CHECK(sMNMessageQueueID == 2);

    /* what the two runs put in the save data */
    CHECK(gSCManagerBackupData.unlock_mask & LBBACKUP_UNLOCK_MASK_LUIGI);
    CHECK(gSCManagerBackupData.unlock_mask & LBBACKUP_UNLOCK_MASK_ITEMSWITCH);
    CHECK(gSCManagerBackupData.fighter_mask & (1 << nFTKindLuigi));
    /* the cursor moved onto Luigi, and the item switch left it there */
    CHECK(gSCManagerBackupData.characters_fkind == nFTKindLuigi);
    for (i = 0; i < nLBBackupUnlockEnumCount; i++)
    {
        CHECK(gSCManagerSceneData.unlock_messages[i] == nLBBackupUnlockEnumCount);
    }
    /* and it is in the store, not just in RAM (mnMessageApplyUnlock ends
     * in lbBackupWrite) */
    ram = gSCManagerBackupData;
    memset(&gSCManagerBackupData, 0, sizeof(gSCManagerBackupData));
    CHECK(lbBackupIsSramValid() != FALSE);
    CHECK(memcmp(&gSCManagerBackupData, &ram, sizeof(ram)) == 0);

    /* it came from the results screen, so the character select is next
     * (mnmessage.c:445) */
    CHECK(gSCManagerSceneData.scene_prev == nSCKindMessage);
    CHECK(gSCManagerSceneData.scene_curr == nSCKindPlayersVS);

    /* An empty queue is no task at all, and the destination is still
     * written. From anywhere but the results screen the game goes to
     * nSCKindStartup, the N64 logo, which the port does not have: the
     * scene manager's default arm is what answers for it
     * (src/dc/scmanager.c). */
    {
        u32 before = dSYTaskmanUpdateCount;

        gSCManagerSceneData.scene_prev = nSCKindModeSelect;
        gSCManagerSceneData.scene_curr = nSCKindMessage;
        mnMessageStartScene();
        CHECK(dSYTaskmanUpdateCount == before);
        CHECK(sMNMessageQueueID == 0);
        CHECK(gSCManagerSceneData.scene_prev == nSCKindMessage);
        CHECK(gSCManagerSceneData.scene_curr == nSCKindStartup);
    }
}

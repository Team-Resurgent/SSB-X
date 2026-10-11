/* hosttest/text.c -- part of hosttest_ft.c: the port's own text.
 *
 * Not compiled on its own: hosttest_ft.c includes every part in
 * order, so this file sees everything the parts before it define. */

/* ---- the font and the strings ------------------------------------------
 *
 * src/dc/dctext.c lays the port's text out in the staff roll's popup
 * font, the way the credits lay theirs out. What is proved: every string
 * the port will ever show (src/dc/dcstrings.h) is made of glyphs the font
 * has; the index macros are the decomp's (A-Z, a-z interleaved in the
 * file but not in the table, digits 9 down to 0); the credits' advance,
 * space, line step and baseline drops; and that the resident bank's
 * sprites are the ones the glyph table names, which is the check the
 * credits failed until 2026-09-24 (the table read the file's interleaved
 * upper and lower case as two runs).
 */
static void test_dctext(void)
{
    DCTextGlyph g[16];
    s32 i;

    /* 1. Every string maps. A printf conversion is not text. */
    for (i = 0; i < nDCStrCount; i++)
    {
        const char *s = gDCStrings[i];

        for (; *s != '\0'; s++)
        {
            if (*s == '%')
            {
                s++;
                while ((*s >= '0') && (*s <= '9'))
                {
                    s++;
                }
                CHECK(*s == 'd' || *s == 's' || *s == 'c');
                continue;
            }
            if (dctext_index(*s) == DCTEXT_NO_GLYPH)
            {
                printf("dctext: string %d \"%s\" has no glyph for '%c'\n",
                       (int)i, gDCStrings[i], *s);
            }
            CHECK(dctext_index(*s) != DCTEXT_NO_GLYPH);
        }
    }

    /* 2. The decomp's indices, and what the font does not have. */
    CHECK(dctext_index('A') == 0 && dctext_index('Z') == 25);
    CHECK(dctext_index('a') == 26 && dctext_index('z') == 51);
    CHECK(dctext_index(':') == 0x34);
    CHECK(dctext_index('9') == 0x35 && dctext_index('0') == 0x3E);
    CHECK(dctext_index('.') == 0x3F && dctext_index(')') == 0x48);
    CHECK(dctext_index(' ') == DCTEXT_IS_SPACE);
    CHECK(dctext_index('\n') == DCTEXT_IS_NEWLINE);
    CHECK(dctext_index('!') == DCTEXT_NO_GLYPH);
    CHECK(dctext_index('%') == DCTEXT_NO_GLYPH);
    CHECK(dctext_index('<') == DCTEXT_NO_GLYPH);

    /* 3. The credits' drops: capitals 0, small letters 3, the tall ones
     *    and the digits 1, the period 9, the dash 5, the comma 10. */
    CHECK(dctext_drop(dctext_index('A')) == 0);
    CHECK(dctext_drop(dctext_index('a')) == 3);
    CHECK(dctext_drop(dctext_index('b')) == 1);
    CHECK(dctext_drop(dctext_index('7')) == 1);
    CHECK(dctext_drop(dctext_index('.')) == 9);
    CHECK(dctext_drop(dctext_index('-')) == 5);
    CHECK(dctext_drop(dctext_index(',')) == 10);

    /* 4. The walk: each glyph by its table width, a space by 3, a new
     *    line back to the left 20 lower. */
    CHECK(dctext_layout("Ab c\nI.", g, ARRAY_COUNT(g)) == 5);
    CHECK(g[0].index == 0 && g[0].x == 0 && g[0].y == 0 && g[0].w == 12);
    CHECK(g[1].x == 12 && g[1].y == 1 && g[1].h == 13);
    CHECK(g[2].x == 12 + 10 + 3 && g[2].y == 3);
    CHECK(g[3].x == 0 && g[3].y == 20 && g[3].w == 5);
    CHECK(g[4].x == 5 && g[4].y == 20 + 9);
    /* S A V I N G . . . */
    CHECK(dctext_width("SAVING...") == 12 + 12 + 14 + 5 + 12 + 12 + 15);
    CHECK(dctext_width("ab\nABC") == 36);
    CHECK(dctext_height("a") == 14 && dctext_height("a\nb") == 34);
    /* a character with no glyph lays out as a space and is not placed */
    CHECK(dctext_layout("A!B", g, ARRAY_COUNT(g)) == 2 && g[1].x == 15);

    /* 5. The resident bank: loaded once, and every glyph the table names
     *    is a sprite of the table's size -- but for capital Z, which the
     *    game's own table (sc/sccommon/scstaffroll.c:422) advances by 14
     *    over a sprite 12 wide. */
    CHECK(dctext_init() == 0);
    CHECK(dctext_init() == 0);
    for (i = 0; i < 74; i++)
    {
        Sprite *sp = dctext_sprite(i);

        CHECK(sp != NULL);
        if (sp != NULL)
        {
            s32 want = (i == dctext_index('Z'))
                           ? 12 : dSCStaffrollTextBoxSpriteInfo[i].width;

            if (sp->width != want)
            {
                printf("dctext: glyph %d is %d wide, not %d\n", (int)i,
                       sp->width, (int)want);
            }
            CHECK(sp->width == want);
            CHECK(sp->height == dSCStaffrollTextBoxSpriteInfo[i].height);
        }
    }
    CHECK(dctext_sprite(74) == NULL && dctext_sprite(-1) == NULL);

    /* 6. And it outlives a scene change, which a scene's bank does not. */
    sprite_bank_release_all();
    CHECK(dctext_sprite(0) != NULL);
}

/* ---- the "SAVING..." notice --------------------------------------------
 *
 * src/dc/vmunotice.c: the box a player sees while the memory card is
 * written. The host draws nothing, so what is proved is where it goes --
 * inside the title-safe area, every line inside the box, at the credits'
 * line step -- and when it is up, which is exactly when the store says
 * (src/dc/vmusave.h sy_sram_notice_visible; test_save_writer above has
 * the second and the coalescing).
 */
static void test_vmunotice(void)
{
    VMUNoticeRect b;
    int lx[3], ly[3], i;

    /* 1. The box sits in the safe area's bottom-right corner, and holds
     *    its three lines. */
    vmunotice_layout(&b, lx, ly);
    CHECK(b.x1 == VMUNOTICE_SAFE_X1 && b.y1 == VMUNOTICE_SAFE_Y1);
    CHECK(b.x0 >= VMUNOTICE_SAFE_X0 && b.y0 >= VMUNOTICE_SAFE_Y0);
    CHECK(b.x0 < b.x1 && b.y0 < b.y1);
    for (i = 0; i < 3; i++)
    {
        static const int ids[3] = { nDCStrSaving, nDCStrSavingWarn,
                                    nDCStrSavingPower };

        CHECK(lx[i] > b.x0 && ly[i] > b.y0);
        CHECK(lx[i] + dctext_width(gDCStrings[ids[i]]) < b.x1);
        CHECK(ly[i] + dctext_height(gDCStrings[ids[i]]) < b.y1);
        CHECK((i == 0) || (ly[i] - ly[i - 1] == DCTEXT_LINE));
    }
    /* the longest line sets the width, "Do not remove the VMU" */
    CHECK(b.x1 - b.x0 > dctext_width(gDCStrings[nDCStrSavingWarn]));

    /* 2. Up while a write is going and for its second, then down. */
    vmucard_host_reset();
    vmucard_host_insert(0, 1, 200);
    sy_sram_host_set_clock(0);
    sy_sram_init();
    CHECK(vmunotice_visible() == 0);
    sy_sram_host_set_clock(1000);
    sy_sram_save_now();
    CHECK(vmunotice_visible() != 0);
    sy_sram_host_set_clock(1999);
    CHECK(vmunotice_visible() != 0);
    sy_sram_host_set_clock(2000);
    CHECK(vmunotice_visible() == 0);
    CHECK(sy_sram_notice_is_load() == 0);
    /* the boot-time write says LOADING, the one after it SAVING */
    sy_sram_host_set_clock(5000);
    sy_sram_next_write_is_load();
    sy_sram_save_now();
    CHECK(sy_sram_notice_is_load() != 0);
    sy_sram_host_set_clock(7000);
    sy_sram_save_now();
    CHECK(sy_sram_notice_is_load() == 0);
    /* the overlay itself draws nothing on the host, on any list */
    vmunotice_overlay(0);

    vmucard_host_reset();
    sy_sram_host_set_clock(0);
    scManagerInitData();
}

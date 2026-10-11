/* hosttest/itemcore.c -- part of hosttest_ft.c: the item machinery under every item: the pack, itManagerInitItems,
 * attributes, models, containers, the Poke Ball monsters, and
 * itmain/itprocess/itdisplay.
 *
 * Not compiled on its own: hosttest_ft.c includes every part in
 * order, so this file sees everything the parts before it define. */

/* ---- the item pack (ITCD) ----------------------------
 *
 * `ITCommonData` is what every item's `o_attributes` reads through, and
 * its absence is why every ported `it.c` LEFT OUT its `itXxxMakeItem`
 * wrapper. This drives the reader against the file the exporter writes.
 *
 * The assertions are deliberately about values the DECOMP states, not
 * about the pack agreeing with itself: `Capsule`'s map collision box is
 * `120, 0, -100, 60` and its damage 5 in relocData/251_ITCommonData.c,
 * and its `data` pointer is the `DObjDesc` the ROM's own relocation at
 * that site names. A pack that round-trips its own reader would pass
 * without either being true.
 */
/* wpmanager.c's one shared branch, see the check at the end of this test */
extern sb32 wpManagerIsModelLess(WPAttributes *attr);

static void test_item_pack(void)
{
    ITItemAttr attr;

    CHECK(itemPackLoaded() == 0);
    CHECK(itemPackLoad("itcommon.itp") == 0);
    CHECK(itemPackLoaded() == 1);
    /* and the load is what publishes the file every item's ITDesc points
     * its p_file at */
    CHECK(gITManagerCommonData != NULL);

    /* Capsule: the decomp's own numbers */
    CHECK(itemPackGetAttr("Capsule", &attr) == 0);
    CHECK(attr.map_top == 120);
    CHECK(attr.map_center == 0);
    CHECK(attr.map_bottom == -100);
    CHECK(attr.map_width == 60);
    CHECK(attr.size == 200);
    CHECK(attr.damage == 5);
    CHECK(attr.attack_count == 1);
    CHECK(attr.spin_speed == 120);   /* the decomp says 120, not a guess */
    /* its four pointer fields: one DObjDesc and three NULLs, which the
     * pack carries as region-1 offsets and ITEM_PACK_NO_PTR */
    CHECK(attr.data_off != ITEM_PACK_NO_PTR);
    CHECK(attr.mobjsubs_off == ITEM_PACK_NO_PTR);
    CHECK(attr.animjoints_off == ITEM_PACK_NO_PTR);
    CHECK(attr.matanimjoints_off == ITEM_PACK_NO_PTR);
    /* and the model region really is addressable by name -- `Shell`, not
     * `Capsule`: Capsule's own `data` at 0x670 is one of the INTERIOR
     * pointers (43 of the 68 fixups are), a place inside a model block
     * rather than the start of one, so the descriptions list no
     * `DataStart Capsule` at all. That the pointer resolves into the
     * model region is all this side can say, and it is what the
     * exporter's alignment-and-containment check says too. */
    CHECK(itemPackBlock(ITEM_PACK_REGION_MODELS, "DataStart Shell",
                        NULL) != NULL);
    CHECK(itemPackBlock(ITEM_PACK_REGION_MODELS, "AnimJoint Shell",
                        NULL) != NULL);

    /* a shell's material chain is NOT NULL -- so the reader is not
     * reading a field of zeroes and calling it agreement */
    CHECK(itemPackGetAttr("GShell", &attr) == 0);
    CHECK(attr.mobjsubs_off != ITEM_PACK_NO_PTR);
    CHECK(attr.data_off != ITEM_PACK_NO_PTR);

    /* ---- the STAGE items ----------------------------
     *
     * PowerBlock's table is not in ITCommonData at all -- it is
     * Mushroom Kingdom's, `ItemAttributes PowerBlock` at 0xD8 of the
     * map file -- and the pack carries it as one more record keyed by
     * that same offset, which is the number the decomp's own `ITDesc`
     * carries. The numbers below are the decomp's
     * (relocData/260_GRInishieMap.c: `map_coll 110, 0, -110, 159`,
     * `size 10`, `angle 361`, `dmg 5`, `kb 20`, `type 6`, `vel 100`),
     * so a record that round-trips its own reader would still fail
     * these if the offsets were wrong.
     *
     * Its `data` is the marker the exporter writes in place of a
     * pointer it cannot carry -- region 1's base, non-NULL, which is
     * all itManagerMakeItem's model-path test reads. The other three
     * are NO_PTR. */
    CHECK(itemPackGetAttr("PowerBlock", &attr) == 0);
    CHECK(attr.map_top == 110);
    CHECK(attr.map_center == 0);
    CHECK(attr.map_bottom == -110);
    CHECK(attr.map_width == 159);
    CHECK(attr.size == 10);
    CHECK(attr.angle == 361);
    CHECK(attr.damage == 5);
    CHECK(attr.knockback_base == 20);
    CHECK(attr.type == 6);
    CHECK(attr.vel_scale == 100);
    CHECK(attr.data_off != ITEM_PACK_NO_PTR);
    CHECK(attr.mobjsubs_off == ITEM_PACK_NO_PTR);
    CHECK(attr.animjoints_off == ITEM_PACK_NO_PTR);
    CHECK(attr.matanimjoints_off == ITEM_PACK_NO_PTR);
    /* and the block table names it the way itemPackGetAttr asked */
    CHECK(itemPackBlock(ITEM_PACK_REGION_ATTRS, "ItemAttributes PowerBlock",
                        NULL) != NULL);

    /* ---- Porygon, and the MONSTER EVENTS --------------
     *
     * The second of Saffron City's five, and the first whose own update
     * reads a table the pack has to carry: `ITMonsterEvent` is BITFIELDS
     * in the ROM (`s32 angle : 10; u32 damage : 8; ub32 can_setoff : 1`),
     * so it is parsed and shipped as plain host fields too, in its own
     * section, keyed by the same `o_attributes` its ItemAttributes is.
     *
     * The numbers are the decomp's
     * (relocData/264_GRYamabukiMap.c:188, `map_coll 300, 0, -300, 360`,
     * `size 200`, `angle 361`, `ks 40`, `dmg 16`, `kb 80`, `type 6`,
     * `vel 100`) -- so a record that round-trips its own reader would
     * still fail these if the offsets were wrong. */
    CHECK(itemPackGetAttr("Porygon", &attr) == 0);
    CHECK(attr.map_top == 300);
    CHECK(attr.map_center == 0);
    CHECK(attr.map_bottom == -300);
    CHECK(attr.map_width == 360);
    CHECK(attr.size == 200);
    CHECK(attr.angle == 361);
    CHECK(attr.knockback_scale == 40);
    CHECK(attr.damage == 16);
    CHECK(attr.element == 0);
    CHECK(attr.knockback_weight == 0);
    CHECK(attr.attack_count == 1);
    CHECK(attr.knockback_base == 80);
    CHECK(attr.type == 6);
    CHECK(attr.vel_scale == 100);
    /* its `data` is Porygon's DObjDesc (the marker stands in), and the
     * other three pointers are the decomp's NULLs -- `anim_joints` is
     * `mobjlink_0x0F30`, an N64 pointer table the pack cannot carry, so
     * it is NO_PTR and MakeItem attaches that script BY NAME instead
     * (src/dc/itporygon.c). NO_PTR here is what keeps itManagerMakeItem's
     * own `attr->anim_joints != NULL` branch from playing garbage. */
    CHECK(attr.data_off != ITEM_PACK_NO_PTR);
    CHECK(attr.mobjsubs_off == ITEM_PACK_NO_PTR);
    CHECK(attr.animjoints_off == ITEM_PACK_NO_PTR);
    CHECK(attr.matanimjoints_off == ITEM_PACK_NO_PTR);

    /* `ITMonsterEvent[2] dGRYamabukiMap_Porygon_HitParties`, in the
     * struct's field order: timer, angle, damage, size, knockback_scale,
     * knockback_weight, knockback_base, element, can_setoff,
     * shield_damage, fgm_id. The two events alternate -- the update
     * installs one, arms the next, and wraps 2 back to 1 -- so both are
     * read here, and the wrap is test_it_porygon's. */
    {
        ITItemEvent ev;

        CHECK(itemPackMonsterEvent((intptr_t)0x16C, 0, &ev) == 0);
        CHECK(ev.timer == 0);
        CHECK(ev.angle == 40);
        CHECK(ev.damage == 18);
        CHECK(ev.size == 300);
        CHECK(ev.knockback_scale == 40);
        CHECK(ev.knockback_weight == 0);
        CHECK(ev.knockback_base == 70);
        CHECK(ev.element == 0);
        CHECK(ev.can_setoff == 0);
        CHECK(ev.shield_damage == 0);

        CHECK(itemPackMonsterEvent((intptr_t)0x16C, 1, &ev) == 0);
        CHECK(ev.timer == 8);
        CHECK(ev.angle == 40);
        CHECK(ev.damage == 8);
        CHECK(ev.size == 300);
        CHECK(ev.knockback_scale == 70);
        CHECK(ev.knockback_weight == 0);
        CHECK(ev.knockback_base == 40);

        /* past the end of the table, and a key the pack has not got */
        CHECK(itemPackMonsterEvent((intptr_t)0x16C, 2, &ev) == -1);
        CHECK(itemPackMonsterEvent((intptr_t)0x16D, 0, &ev) == -1);
        CHECK(itemPackMonsterEvent((intptr_t)0x0BC, 0, &ev) == -1);
    }

    /* ---- Marumine, and the ATTACK EVENTS --------------
     *
     * The Electrode. Same numbers, its own section: `ITAttackEvent` is a
     * four-field script (timer, angle, damage, size) and is BITFIELDS at
     * its head for the same reason `ITMonsterEvent` is, so it is parsed
     * and shipped the same way. The decomp's table
     * (relocData/264_GRYamabukiMap.c:179) is a burst that SHRINKS --
     * 700/30, 350/30, 300/20, 200/10 -- and the item wraps 4 back to 3,
     * so the last row is the one that re-fires. */
    CHECK(itemPackGetAttr("Marumine", &attr) == 0);
    CHECK(attr.map_top == 225);
    CHECK(attr.map_center == 0);
    CHECK(attr.map_bottom == -225);
    CHECK(attr.map_width == 248);
    CHECK(attr.size == 400);
    CHECK(attr.angle == 361);
    CHECK(attr.knockback_scale == 100);
    CHECK(attr.damage == 10);
    CHECK(attr.element == 1);          /* nGMHitElementFire */
    CHECK(attr.knockback_base == 40);
    CHECK(attr.type == 6);
    CHECK(attr.vel_scale == 100);
    CHECK(attr.data_off != ITEM_PACK_NO_PTR);
    CHECK(attr.mobjsubs_off == ITEM_PACK_NO_PTR);
    CHECK(attr.animjoints_off == ITEM_PACK_NO_PTR);
    CHECK(attr.matanimjoints_off == ITEM_PACK_NO_PTR);

    {
        ITItemAttackEvent aev;

        CHECK(itemPackAttackEvent((intptr_t)0x104, 0, &aev) == 0);
        CHECK(aev.timer == 0);
        CHECK(aev.angle == 361);
        CHECK(aev.damage == 30);
        CHECK(aev.size == 700);

        CHECK(itemPackAttackEvent((intptr_t)0x104, 1, &aev) == 0);
        CHECK(aev.timer == 2);
        CHECK(aev.damage == 30);
        CHECK(aev.size == 350);

        CHECK(itemPackAttackEvent((intptr_t)0x104, 2, &aev) == 0);
        CHECK(aev.timer == 4);
        CHECK(aev.damage == 20);
        CHECK(aev.size == 300);

        CHECK(itemPackAttackEvent((intptr_t)0x104, 3, &aev) == 0);
        CHECK(aev.timer == 6);
        CHECK(aev.damage == 10);
        CHECK(aev.size == 200);

        /* the fourth index is past the table, and the MONSTER events'
         * section must not answer for an attack-events key either */
        CHECK(itemPackAttackEvent((intptr_t)0x104, 4, &aev) == -1);
        CHECK(itemPackAttackEvent((intptr_t)0x16C, 0, &aev) == -1);
        CHECK(itemPackAttackEvent((intptr_t)0x105, 0, &aev) == -1);
    }

    /* ---- Hitokage's flame, and the WEAPON ATTRIBUTES ---
     *
     * The third parsed table, and the one a WEAPON reads rather than an
     * item: `WPAttributes` at 0x244 of Saffron City's map, which
     * `dITHitokageWeaponFlameWeaponDesc.p_weapon` names and which the port
     * has no stage file for. Its numbers are the decomp's
     * (relocData/264_GRYamabukiMap.c:273): map_coll 50/0/-50/50, size 320,
     * angle 0, ks 100, damage 2, element 1 (Fire), kw 3, sd 1, ac 1,
     * cs 0, priority 1, kb 0.
     *
     * ALL FOUR POINTERS ARE NULL, and that is the table's most important
     * field: a flame with no `data` has no model at all. It is an
     * invisible hitbox whose appearance is entirely
     * lbParticleMakePosVel particles, which is why `wpManagerAddModel`
     * needed a data-less arm rather than a pack row. */
    {
        WPAttributes *wa = itemPackWeaponAttr(gITManagerCommonData,
                                              (intptr_t)0x244);

        CHECK(wa != NULL);
        CHECK(wa->data == NULL);
        CHECK(wa->p_mobjsubs == NULL);
        CHECK(wa->anim_joints == NULL);
        CHECK(wa->p_matanim_joints == NULL);
        CHECK(wa->attack_offsets[0].x == 0);
        CHECK(wa->attack_offsets[0].y == 0);
        CHECK(wa->attack_offsets[0].z == 0);
        CHECK(wa->attack_offsets[1].x == 0);
        CHECK(wa->attack_offsets[1].y == 0);
        CHECK(wa->attack_offsets[1].z == 0);
        CHECK(wa->map_coll_top == 50);
        CHECK(wa->map_coll_center == 0);
        CHECK(wa->map_coll_bottom == -50);
        CHECK(wa->map_coll_width == 50);
        CHECK(wa->size == 320);
        CHECK(wa->angle == 0);
        CHECK(wa->knockback_scale == 100);
        CHECK(wa->damage == 2);
        CHECK(wa->element == 1);
        CHECK(wa->knockback_weight == 3);
        CHECK(wa->shield_damage == 1);
        CHECK(wa->attack_count == 1);
        CHECK(wa->can_setoff == 0);
        CHECK(wa->priority == 1);
        CHECK(wa->can_rehit_item == 1);
        CHECK(wa->can_rehit_fighter == 0);
        CHECK(wa->can_hop == 0);
        CHECK(wa->can_reflect == 1);
        CHECK(wa->can_absorb == 1);
        CHECK(wa->can_shield == 1);
        CHECK(wa->knockback_base == 0);

        /* a NULL file, a file that is not region 0, and an offset that is
         * not a table -- three ways to get the byte overlay back */
        CHECK(itemPackWeaponAttr(NULL, (intptr_t)0x244) == NULL);
        CHECK(itemPackWeaponAttr((void *)0x1000, (intptr_t)0x244) == NULL);
        CHECK(itemPackWeaponAttr(gITManagerCommonData, (intptr_t)0x245) == NULL);
        /* and an ITEM's key must not resolve through the weapon table */
        CHECK(itemPackWeaponAttr(gITManagerCommonData, (intptr_t)0x104) == NULL);

        /* ---- the ACCEPTANCE decision, on the machine that can check it --
         *
         * `wpManagerIsModelLess` is the one thing both arms of
         * wpManagerAddModel branch on, and it is why this host test can
         * say anything at all about a target-only arm: the target used to
         * require an sWPModels row and the host used to build a bare DObj
         * unconditionally, so the host was green for a weapon the console
         * refused. The decision is shared now, so it is checkable here --
         * with the flame's REAL table out of the pack, and with a
         * data-ful one for the other side. */
        {
            WPAttributes *flame = itemPackWeaponAttr(gITManagerCommonData,
                                                     (intptr_t)0x244);
            WPAttributes with_model;

            CHECK(flame != NULL);
            CHECK(wpManagerIsModelLess(flame) == TRUE);

            memset(&with_model, 0, sizeof(with_model));
            with_model.data = (void *)0x1000;
            CHECK(wpManagerIsModelLess(&with_model) == FALSE);
        }
    }

    /* ---- ITCommonData's own weapon tables --------------------------
     *
     * The Fire Flower's flame, the Ray Gun's shot and the Star Rod's two
     * stars used to fall through to the byte overlay: region 0 is the
     * ROM's big-endian bytes, read through little-endian bitfields, and
     * the flame came out size 3585 and damage 0 -- a 4-CPU probe logged
     * no hit from any of fourteen bursts. The pack carries all twelve of
     * the file's tables now; these are the decomp's numbers
     * (relocData/251_ITCommonData.c), reached the way wpManagerMakeWeapon
     * reaches them, through the WPDesc. */
    {
        WPAttributes *fl = itemPackWeaponAttr(
            *dITFFlowerWeaponFlameWeaponDesc.p_weapon,
            dITFFlowerWeaponFlameWeaponDesc.o_attributes);
        WPAttributes *lg = itemPackWeaponAttr(gITManagerCommonData,
                                              (intptr_t)0x2B0);
        WPAttributes *sr = itemPackWeaponAttr(gITManagerCommonData,
                                              (intptr_t)0x4D4);
        WPAttributes *ss = itemPackWeaponAttr(gITManagerCommonData,
                                              (intptr_t)0x508);
        static const intptr_t kMonsterKeys[8] = {
            0x774, 0x8C8, 0x944, 0x9D4, 0xA50, 0xB7C, 0xC40, 0xCBC
        };
        u32 k;

        CHECK(fl != NULL);
        if (fl != NULL)
        {
            CHECK(fl->data == NULL);            /* particles only */
            CHECK(fl->map_coll_top == 50 && fl->map_coll_bottom == -50);
            CHECK(fl->size == 270);
            CHECK(fl->knockback_scale == 100);
            CHECK(fl->damage == 3);
            CHECK(fl->element == 1);
            CHECK(fl->knockback_weight == 3);
            CHECK(fl->attack_count == 1);
            CHECK(fl->sfx == 28);
            CHECK(fl->can_reflect == 1 && fl->can_shield == 1);
        }
        CHECK(lg != NULL && lg->size == 120 && lg->damage == 10);
        CHECK(lg != NULL && lg->data != NULL);  /* drawn, wplgunammo.mdl */
        CHECK(sr != NULL && sr->size == 200 && sr->damage == 8);
        CHECK(ss != NULL && ss->size == 200 && ss->damage == 12);
        for (k = 0; k < 8; k++)
        {
            WPAttributes *m = itemPackWeaponAttr(gITManagerCommonData,
                                                 kMonsterKeys[k]);

            CHECK(m != NULL && m->size != 0 && m->damage != 0);
        }
    }

    /* ---- Hitokage's own attributes --------------------
     *
     * The Charmander's. Its numbers are the decomp's
     * (relocData/264_GRYamabukiMap.c:236-272): map_coll 252/0/-252/273,
     * size 340, angle 361, ks 100, damage 5, kb 20, type 6, vel 100. */
    CHECK(itemPackGetAttr("Hitokage", &attr) == 0);
    CHECK(attr.map_top == 252);
    CHECK(attr.map_center == 0);
    CHECK(attr.map_bottom == -252);
    CHECK(attr.map_width == 273);
    CHECK(attr.size == 340);
    CHECK(attr.angle == 361);
    CHECK(attr.knockback_scale == 100);
    CHECK(attr.damage == 5);
    CHECK(attr.element == 0);
    CHECK(attr.attack_count == 1);
    CHECK(attr.knockback_base == 20);
    CHECK(attr.type == 6);
    CHECK(attr.vel_scale == 100);
    /* Fushigibana's own, and the LAST of the five. The
     * decomp's numbers (264_GRYamabukiMap.c:302-345): map_coll 360/0/-360/
     * 510, size 200, angle 361, ks 100, damage 5, kb 20, type 6, vel 100.
     *
     * Its HitParties table is the one it SHARES with Porygon's shape --
     * two entries, the second's angle 361 where Porygon's is 40 -- and it
     * is checked through itemPackMonsterEvent below. */
    CHECK(itemPackGetAttr("Fushigibana", &attr) == 0);
    CHECK(attr.map_top == 360);
    CHECK(attr.map_center == 0);
    CHECK(attr.map_bottom == -360);
    CHECK(attr.map_width == 510);
    CHECK(attr.size == 200);
    CHECK(attr.angle == 361);
    CHECK(attr.knockback_scale == 100);
    CHECK(attr.damage == 5);
    CHECK(attr.knockback_base == 20);
    CHECK(attr.type == 6);
    CHECK(attr.vel_scale == 100);
    CHECK(attr.data_off != ITEM_PACK_NO_PTR);
    CHECK(attr.mobjsubs_off == ITEM_PACK_NO_PTR);
    CHECK(attr.animjoints_off == ITEM_PACK_NO_PTR);
    CHECK(attr.matanimjoints_off == ITEM_PACK_NO_PTR);

    {
        ITItemEvent ev;

        CHECK(itemPackMonsterEvent((intptr_t)0x278, 0, &ev) == 0);
        CHECK(ev.timer == 0);
        CHECK(ev.angle == 40);
        CHECK(ev.damage == 20);
        CHECK(ev.size == 300);
        CHECK(ev.knockback_scale == 100);
        CHECK(ev.knockback_weight == 90);
        CHECK(ev.knockback_base == 0);

        CHECK(itemPackMonsterEvent((intptr_t)0x278, 1, &ev) == 0);
        CHECK(ev.timer == 8);
        CHECK(ev.angle == 361);
        CHECK(ev.damage == 8);
        CHECK(ev.knockback_scale == 70);
        CHECK(ev.knockback_weight == 0);
        CHECK(ev.knockback_base == 30);

        /* and Porygon's key does not reach the Venusaur's table */
        CHECK(itemPackMonsterEvent((intptr_t)0x279, 0, &ev) == -1);
    }

    /* ---- and the RAZOR's attributes, the second stage WEAPON ---------
     *
     * 0x308 of Saffron City's map, and the one whose `data` is REAL: the
     * razor is a baked model where the flame is particles. The decomp's
     * numbers (264_GRYamabukiMap.c:358-390): map_coll 120/0/-120/360,
     * size 200, angle 90, ks 60, damage 3. */
    {
        WPAttributes *rz = itemPackWeaponAttr(gITManagerCommonData,
                                              (intptr_t)0x308);

        CHECK(rz != NULL);
        CHECK(wpManagerIsModelLess(rz) == FALSE);   /* it HAS a model */
        CHECK(rz->map_coll_top == 120);
        CHECK(rz->map_coll_center == 0);
        CHECK(rz->map_coll_bottom == -120);
        CHECK(rz->map_coll_width == 360);
        CHECK(rz->size == 200);
        CHECK(rz->angle == 90);
        CHECK(rz->knockback_scale == 60);
        CHECK(rz->damage == 3);
    }

    /* ---- Sector Z's two lasers ------------------------
     *
     * The first weapons in the pack whose owner is a STAGE rather than an
     * item, and they come in through the sector's own `weapon_head`.
     * `data` is REAL for both and names the SAME display list (file 153's
     * 0x1C50) -- the 3D laser differs only in its matrix -- so both take
     * wpManagerAddModel's PACK arm, and both need a row in the port's own
     * weapon table.
     *
     * The numbers are the decomp's (relocData/262_GRSectorMap.c:94 and
     * :124). Note how the two differ: the 2D laser is the heavier one
     * (16% at size 300, knockback 70) and the 3D one is 18% at the same
     * size with a width of 28 rather than 564 -- a bolt rather than a
     * beam. */
    {
        WPAttributes *l2 = itemPackWeaponAttr(gITManagerCommonData,
                                              (intptr_t)0xBC);
        WPAttributes *l3 = itemPackWeaponAttr(gITManagerCommonData,
                                              (intptr_t)0xF0);

        CHECK(l2 != NULL);
        CHECK(l3 != NULL);

        /* both have a model, so both take the pack arm */
        CHECK(wpManagerIsModelLess(l2) == FALSE);
        CHECK(wpManagerIsModelLess(l3) == FALSE);

        CHECK(l2->map_coll_top == 28);
        CHECK(l2->map_coll_bottom == -28);
        CHECK(l2->map_coll_width == 564);
        CHECK(l2->size == 300);
        CHECK(l2->angle == 361);
        CHECK(l2->knockback_scale == 75);
        CHECK(l2->damage == 16);
        CHECK(l2->element == 0);
        CHECK(l2->knockback_weight == 0);
        CHECK(l2->shield_damage == 5);
        CHECK(l2->attack_count == 1);
        CHECK(l2->can_setoff == 0);
        CHECK(l2->can_rehit_item == 0);
        CHECK(l2->can_rehit_fighter == 0);
        CHECK(l2->can_hop == 1);
        CHECK(l2->can_reflect == 1);
        CHECK(l2->can_absorb == 1);
        CHECK(l2->can_shield == 1);
        CHECK(l2->knockback_base == 70);

        CHECK(l3->map_coll_width == 28);
        CHECK(l3->size == 300);
        CHECK(l3->knockback_scale == 100);
        CHECK(l3->damage == 18);
        CHECK(l3->element == 1);
        CHECK(l3->shield_damage == 2);
        CHECK(l3->can_setoff == 0);
        CHECK(l3->can_rehit_item == 1);
        CHECK(l3->can_rehit_fighter == 1);
        CHECK(l3->can_hop == 0);
        CHECK(l3->can_reflect == 0);
        CHECK(l3->can_absorb == 1);
        CHECK(l3->can_shield == 0);
        CHECK(l3->knockback_base == 10);
    }

    /* ---- the TaruBomb --------------------------------
     *
     * Race to the Finish's exploding barrel, and the first stage item
     * whose map file has NO `#if defined(REGION_JP)` split at all --
     * 295_GRBonus3Map.c is one arm from end to end, where every earlier
     * one (276_GRBonus1LinkMap.c, 255_GRPupupuMap.c and a dozen more)
     * carries both. The exporter's "there must have been a split" check
     * is an ITCommonData tripwire and now says so.
     *
     * The numbers are the decomp's (relocData/295_GRBonus3Map.c:69-107):
     * map_coll 236/0/-236/221, size 290, angle 70, ks 20, damage 10,
     * kb 90, type 6, vel 100, and its hit sfx is the punch rather than
     * the explosion (the explosion is the ATTACK EVENTS' below).
     *
     * Its attack events sit at 0xF0 of that file but are keyed here by
     * 0xA8, the item's own `o_attributes`, exactly as Marumine's are --
     * the key names the table's OWNER, not the table. The burst shrinks
     * the way the Electrode's does, and the last row has size 0: a
     * hitbox that has stopped, which is how the barrel stops hurting
     * before its effect has finished playing. */
    {
        ITItemAttr ta;
        ITItemAttackEvent aev;
        ITItemEvent mev;

        CHECK(itemPackGetAttr("TaruBomb", &ta) == 0);
        CHECK(ta.map_top == 236);
        CHECK(ta.map_center == 0);
        CHECK(ta.map_bottom == -236);
        CHECK(ta.map_width == 221);
        CHECK(ta.size == 290);
        CHECK(ta.angle == 70);
        CHECK(ta.knockback_scale == 20);
        CHECK(ta.damage == 10);
        CHECK(ta.element == 0);
        CHECK(ta.attack_count == 1);
        CHECK(ta.knockback_base == 90);
        CHECK(ta.type == 6);
        CHECK(ta.vel_scale == 100);
        CHECK(ta.data_off != ITEM_PACK_NO_PTR);
        CHECK(ta.mobjsubs_off == ITEM_PACK_NO_PTR);
        CHECK(ta.animjoints_off == ITEM_PACK_NO_PTR);
        CHECK(ta.matanimjoints_off == ITEM_PACK_NO_PTR);

        CHECK(itemPackAttackEvent((intptr_t)0xA8, 0, &aev) == 0);
        CHECK(aev.timer == 0);
        CHECK(aev.angle == 361);
        CHECK(aev.damage == 16);
        CHECK(aev.size == 350);

        CHECK(itemPackAttackEvent((intptr_t)0xA8, 1, &aev) == 0);
        CHECK(aev.timer == 2);
        CHECK(aev.damage == 11);
        CHECK(aev.size == 250);

        CHECK(itemPackAttackEvent((intptr_t)0xA8, 2, &aev) == 0);
        CHECK(aev.timer == 4);
        CHECK(aev.damage == 8);
        CHECK(aev.size == 150);

        CHECK(itemPackAttackEvent((intptr_t)0xA8, 3, &aev) == 0);
        CHECK(aev.timer == 6);
        CHECK(aev.damage == 1);
        CHECK(aev.size == 0);

        /* four rows and no more, and 0xA8 must not answer for a monster
         * table either -- Sector Z's 2D laser is keyed 0xBC in the
         * WEAPON section, near enough to catch a section mix-up */
        CHECK(itemPackAttackEvent((intptr_t)0xA8, 4, &aev) == -1);
        CHECK(itemPackMonsterEvent((intptr_t)0xA8, 0, &mev) == -1);
    }

    /* ---- and its two models -----------------------------------------
     *
     * The barrel is file 162 + 0x788, three joints -- a root and the two
     * halves the smash splits it into -- and it comes through the item
     * model table keyed by the same 0xA8. The SMASH EFFECT is the other
     * one: `DisplayList TaruBombEffect` at 0x8A0 of the same file, a
     * bare display list with no DObjDesc above it, so it bakes as the
     * one-joint pack itbox.c's box-smash effect does. The maker that
     * uses it is separate; what this pins is that both files are on
     * the disc and parse. */
    {
        GObj gobj;
        Fighter *smash;

        memset(&gobj, 0, sizeof(gobj));
        CHECK(itemModelAddToGObj(&gobj, 0xA8) == 3);
        CHECK(DObjGetStruct(&gobj) != NULL);

        smash = itemModelTaruBombSmash();
        CHECK(smash != NULL);
    }

    /* ---- Break the Targets' target --------------------
     *
     * The ninth stage item, and the odd one out twice over. Its
     * ITAttributes is not in a stage's map file: relocData 253
     * (ITBonus1ObjectHeader) IS the table and nothing else, so the
     * decomp's ITDesc carries offset ZERO -- the only key in the pack
     * that is -- and names `&gSC1PBonusStageItemFile`, the pointer
     * sc1PBonusStageBonus1LoadFile fills. Its tree is relocData 150's
     * at 0x10F8, and one file serves all twelve courses.
     *
     * The numbers are the decomp's (relocData/253_ITBonus1ObjectHeader.c):
     * map_coll 180/0/-180/180, size 200, the Sakurai angle 361, ks 100,
     * damage 1, kb 10, type 6, vel 100 -- a target is an obstacle with
     * almost no hit in it, which is what those last three say. */
    {
        ITItemAttr ta;
        GObj gobj;

        CHECK(itemPackGetAttr("Target", &ta) == 0);
        CHECK(ta.map_top == 180);
        CHECK(ta.map_center == 0);
        CHECK(ta.map_bottom == -180);
        CHECK(ta.map_width == 180);
        CHECK(ta.size == 200);
        CHECK(ta.angle == 361);
        CHECK(ta.knockback_scale == 100);
        CHECK(ta.damage == 1);
        CHECK(ta.knockback_base == 10);
        CHECK(ta.type == 6);
        CHECK(ta.vel_scale == 100);
        /* the stage items' marker set, as every row above carries */
        CHECK(ta.data_off != ITEM_PACK_NO_PTR);
        CHECK(ta.mobjsubs_off == ITEM_PACK_NO_PTR);
        CHECK(ta.animjoints_off == ITEM_PACK_NO_PTR);
        CHECK(ta.matanimjoints_off == ITEM_PACK_NO_PTR);

        /* and the key itself: 0 resolves to the TARGET and to nothing
         * else, which is the whole risk in reusing the decomp's number */
        CHECK(itemPackAttr(gITManagerCommonData, (intptr_t)0) != NULL);

        /* TWO joints, not the three the decomp's DObjDesc[3] array
         * suggests: the third entry is the `{18, ...}` terminator the
         * bake stops at, so what comes out is the root and the one
         * joint carrying the sign's display list. */
        memset(&gobj, 0, sizeof(gobj));
        CHECK(itemModelAddToGObj(&gobj, 0x0) == 2);
        CHECK(DObjGetStruct(&gobj) != NULL);
    }

    /* the three pointer fields the pack writes markers for, as every
     * stage item's are -- the trees and the MObjSub chains come from the
     * BAKE, not from this record (test_it_hitokage checks the MObj). */
    CHECK(attr.mobjsubs_off == ITEM_PACK_NO_PTR);
    CHECK(attr.animjoints_off == ITEM_PACK_NO_PTR);
    CHECK(attr.matanimjoints_off == ITEM_PACK_NO_PTR);
    CHECK(attr.data_off != ITEM_PACK_NO_PTR);

    /* a name the pack has not got */
    CHECK(itemPackGetAttr("Nonesuch", &attr) == -1);
    CHECK(itemPackBlock(ITEM_PACK_REGION_MODELS, "Nonesuch", NULL) == NULL);

    /* region 0 is the file the port already reads as bytes, and it is
     * still there: VelocitiesY Container is its first block */
    {
        u32 size = 0;

        CHECK(itemPackBlock(ITEM_PACK_REGION_DATA,
                            "VelocitiesY Container", &size) != NULL);
        CHECK(size > 0);
    }

    /* release gives the global back */
    itemPackRelease();
    CHECK(gITManagerCommonData == NULL);
    CHECK(itemPackLoaded() == 0);
    CHECK(itemPackGetAttr("Capsule", &attr) == -1);

    /* and a second load works -- nothing is left half-torn-down */
    CHECK(itemPackLoad("itcommon.itp") == 0);
    CHECK(itemPackGetAttr("Capsule", &attr) == 0);
    itemPackRelease();
}

/* ---- itManagerInitItems ---------------------------------
 *
 * test_it_pool above builds the ITStruct pool by hand, because the one
 * thing the real itManagerInitItems did that the port could not was read
 * ITCommonData. The item pack is in now, so this calls the real thing
 * and checks what a scene gets out of it.
 *
 * None of this is allowed to be a tautology. `gITManagerCommonData` is
 * whatever itemPackLoad published, and the attribute read back through
 * it is a number the DECOMP states -- a manager that pointed the global
 * at a scratch buffer of its own would satisfy a != NULL and fail this.
 * Nothing else is reset first, so what is being observed is the state
 * the function left behind and not one the test arranged.
 */
static void test_it_init_items(void)
{
    extern Sprite *sIFCommonItemArrowSprite;
    ITItemAttr attr;
    ITStruct *ip;
    s32 count = 0;

    /* test_item_pack above gave the pack back, so this is a first load */
    CHECK(itemPackLoaded() == 0);
    CHECK(gITManagerCommonData == NULL);

    itManagerInitItems();

    /* the pool: ITEM_ALLOC_MAX links, the last one NULL */
    for (ip = gITManagerStructsAllocFree; ip != NULL; ip = ip->next)
    {
        count++;
    }
    CHECK(count == ITEM_ALLOC_MAX);

    /* the file published, and it is the real one */
    CHECK(gITManagerCommonData != NULL);
    CHECK(itemPackLoaded() == 1);
    CHECK(itemPackGetAttr("Capsule", &attr) == 0);
    CHECK(attr.damage == 5);
    CHECK(attr.map_top == 120);

    /* the container drop table ran, and with no stage weights on this
     * build (gMPCollisionGroundData->item_weights is NULL, src/dc/
     * stage.c) it says nothing drops -- rather than leaving the total at
     * whatever the last test put there */
    CHECK(gITManagerRandomWeights.weights_sum == 0);

    /* the Poké Ball monster index is the enum's own span */
    CHECK(gITManagerMonsterData.monsters_num ==
          (nITKindMBallMonsterEnd - nITKindMBallMonsterStart));

    CHECK(gITManagerDisplayMode == nDBDisplayModeMaster);

    /* The item particle bank. The function does not load one in this
     * build -- see the `#ifdef FT_HOSTTEST` arm in itManagerInitItems,
     * and efManagerLoadEffectBank for why no bank can be loaded here at
     * all -- so `id 0` is what it leaves. The bank that id names is
     * built here out of the same two files at this machine's width, the
     * way test_gr_hyrule builds Hyrule's. itcommon_scb.c declares three
     * scripts and itcommon_txb.c two textures, so a fixture that read
     * the wrong files fails this. */
    CHECK(gITManagerParticleBankID == 0);
    {
        LBScript **saved_scripts = sLBParticleScriptBanks[0];
        LBTexture **saved_textures = sLBParticleTextureBanks[0];
        s32 saved_nscripts = sLBParticleScriptBanksNum[0];
        s32 saved_ntextures = sLBParticleTextureBanksNum[0];

        host_load_particle_bank(0, "itcommon.scb", "itcommon.txb");
        CHECK(sLBParticleScriptBanksNum[0] == 3);
        CHECK(sLBParticleTextureBanksNum[0] == 2);

        sLBParticleScriptBanks[0] = saved_scripts;
        sLBParticleTextureBanks[0] = saved_textures;
        sLBParticleScriptBanksNum[0] = saved_nscripts;
        sLBParticleTextureBanksNum[0] = saved_ntextures;
    }

    /* the arrow: ifCommonItemArrowSetAttr loaded ifcommonitem.spr and
     * wrote the decomp's own red onto the one sprite in it. The three
     * colours and the blend bit are the four things that function sets;
     * everything below is the ROM's own Sprite record at IFCommonItem
     * +0x50, so a bank that resolved to a zeroed record -- or to the
     * wrong file -- fails it.
     *
     * 9x7 is the game's `width`/`height`, which are the size the arrow is
     * DRAWN at (the display proc centres on width), not the size of the
     * bitmap behind it: that one is 16 texels wide, which is what the
     * exporter's own line prints. i4 is fmt 4 / siz 0, the format the
     * decompressed bitmap leaves the exporter in. */
    CHECK(sIFCommonItemArrowSprite != NULL);
    CHECK(sIFCommonItemArrowSprite->red == 0xFF);
    CHECK(sIFCommonItemArrowSprite->green == 0x00);
    CHECK(sIFCommonItemArrowSprite->blue == 0x00);
    CHECK((sIFCommonItemArrowSprite->attr & SP_TEXSHUF) != 0);
    CHECK(sIFCommonItemArrowSprite->width == 9);
    CHECK(sIFCommonItemArrowSprite->height == 7);
    CHECK(sIFCommonItemArrowSprite->bmfmt == G_IM_FMT_I);
    CHECK(sIFCommonItemArrowSprite->bmsiz == G_IM_SIZ_4b);
    CHECK(sIFCommonItemArrowSprite->nbitmaps == 1);
}

/* ---- the item arrow, driven through a real MakeItem ------------------
 *
 * `ifCommonItemArrowMakeInterface` had never run in this build: every
 * caller of it is a roster item's MakeItem tail, and no MakeItem was
 * ported once. This drives the shortest of them -- Sword's
 * -- and checks the interface GObj it builds: the item in user_data, an
 * SObj on the interface link, and the bank sprite it loads.
 *
 * The attribute base is computed backward from a local ITAttributes,
 * exactly as test_it_star's own itStarMakeItem block does and for the
 * same reason: dITSwordItemDesc.o_attributes holds
 * llITCommonDataSwordItemAttributes's SYMBOL ADDRESS (itstar.c's file
 * header has the full reasoning), not an offset into anything, so
 * lbRelocGetFileData's file+offset arithmetic has to land on an
 * ITAttributes this test owns. The item pack does not change that --
 * o_attributes is not a real offset into region 0 here.
 */
static void test_it_make_item_arrow(void)
{
    extern Sprite *sIFCommonItemArrowSprite;
    extern ITDesc dITSwordItemDesc;
    static ITStruct pool[ITEM_ALLOC_MAX];
    ITStruct *save_free = gITManagerStructsAllocFree;
    void *save_common = gITManagerCommonData;
    ITAttributes attr;
    Vec3f pos = { 0.0F, 0.0F, 0.0F };
    Vec3f vel = { 0.0F, 0.0F, 0.0F };
    GObj *item_gobj;
    ITStruct *ip;
    int i;

    /* test_it_init_items above ran ifCommonItemArrowSetAttr for real, so
     * the sprite this checks the arrow was built from is the bank's */
    CHECK(sIFCommonItemArrowSprite != NULL);
    CHECK(dITSwordItemDesc.kind == nITKindSword);

    for (i = 0; i < ITEM_ALLOC_MAX - 1; i++) pool[i].next = &pool[i + 1];
    pool[ITEM_ALLOC_MAX - 1].next = NULL;
    gITManagerStructsAllocFree = pool;

    memset(&attr, 0, sizeof(attr));
    gITManagerCommonData = (void*)((uintptr_t)&attr -
                                   (uintptr_t)0x190);  /* reloc_data.us.h: llITCommonDataSwordItemAttributes */

    item_gobj = itSwordMakeItem(NULL, &pos, &vel, 0);
    CHECK(item_gobj != NULL);

    if (item_gobj != NULL)
    {
        ip = itGetStruct(item_gobj);

        CHECK(ip->kind == nITKindSword);
        CHECK(ip->is_unused_item_bool == TRUE);
        CHECK(DObjGetStruct(item_gobj)->rotate.vec.f.y == F_CST_DTOR32(90.0F));

        /* the arrow GObj is made */
        CHECK(ip->arrow_gobj != NULL);

        if (ip->arrow_gobj != NULL)
        {
            CHECK(ip->arrow_gobj->user_data.p == ip);
            CHECK(ip->arrow_gobj->link_id == nGCCommonLinkIDInterface);
            CHECK(ip->arrow_gobj->obj_kind == 2);   /* SObj */
        }
        /* Both GObjs go back on their links: this is the only test in the
         * suite that puts anything on the INTERFACE link, and a later one
         * asserts it is empty. The arrow first -- it reads the item's own
         * DObj, so it must not outlive the item. */
        if (ip->arrow_gobj != NULL) gcEjectGObj(ip->arrow_gobj);
        gcEjectGObj(item_gobj);

        CHECK(gGCCommonLinks[nGCCommonLinkIDInterface] == NULL);
    }
    gITManagerCommonData = save_common;
    gITManagerStructsAllocFree = save_free;
}

/* ---- the attribute tables, as the game reads them --------------------
 *
 * `itManagerMakeItem` reads `attr = lbRelocGetFileData(ITAttributes*,
 * *item_desc->p_file, o_attributes)` and keeps the pointer for the item's
 * whole life. The offset is a real one into region 0,
 * but the bytes there are the N64's: `ITAttributes` opens with four
 * POINTERS -- 16 bytes of them there and 32 here -- so a raw read on this
 * machine puts every field PAST them sixteen bytes out. That is the whole
 * reason region 2 exists, and these are the fields it is about.
 *
 * The numbers are Capsule's, and they are the same ones test_item_pack
 * pins out of the parsed record -- so this checks that the materialisation
 * carries them into the struct the game actually reads, which is a
 * different path (itemPackAttr and item_pack_setup_attr) from the one
 * that test pins.
 */
static void test_item_pack_attrs(void)
{
    ITAttributes *attr;

    /* the global is region 0, as itManagerInitItems leaves it */
    CHECK(itemPackLoaded() == 1);
    CHECK(gITManagerCommonData != NULL);

    /* Capsule: 0x50 into ITCommonData (reloc_data.us.h:
     * llITCommonDataCapsuleItemAttributes) */
    attr = itemPackAttr(gITManagerCommonData, 0x50);
    CHECK(attr != NULL);

    if (attr != NULL)
    {
        /* past the four pointers -- where a raw read goes wrong */
        CHECK(attr->map_coll_top == 120);
        CHECK(attr->map_coll_center == 0);
        CHECK(attr->map_coll_bottom == -100);
        CHECK(attr->map_coll_width == 60);
        CHECK(attr->size == 200);
        CHECK(attr->damage == 5);
        CHECK(attr->attack_count == 1);
        CHECK(attr->spin_speed == 120);
        /* its sounds by name, not 0 -- nSYAudioFGMExplodeS, which every
         * named item sound was until the item pack resolved the names */
        CHECK(attr->hit_sfx == nSYAudioFGMPunchL);
        CHECK(attr->drop_sfx == nSYAudioFGMItemThrow);
        CHECK(attr->throw_sfx == nSYAudioFGMItemThrow);
        CHECK(attr->smash_sfx == nSYAudioFGMItemThrow);
        CHECK(nSYAudioFGMPunchL != nSYAudioFGMExplodeS);

        /* and the pointers themselves: one DObjDesc, three NULLs, the
         * same shape the parsed record's four offsets have */
        CHECK(attr->data != NULL);
        CHECK(attr->p_mobjsubs == NULL);
        CHECK(attr->anim_joints == NULL);
        CHECK(attr->p_matanim_joints == NULL);

        /* `data` is region 1's base plus the block's own offset into the
         * model file, so it lands past region 0 -- which is what makes
         * itGetPData's `attr->data - A + B` answer for a block. A raw
         * read of region 0 would have produced a small integer
         * masquerading as a pointer instead. */
        CHECK((u8 *)attr->data > (u8 *)gITManagerCommonData);

        /* the flags the record packs as bits arrive as bitfields */
        CHECK(attr->is_display_xlu == 0);
        CHECK(attr->is_item_dobjs == 0);
    }

    /* a file that is not the pack's answers NULL, which is the contract
     * every item test without a pack relies on for its fallback */
    CHECK(itemPackAttr(sHostItemData, 0x50) == NULL);
    CHECK(itemPackAttr(gITManagerCommonData, 0x1234) == NULL);
}

/* ---- the item models, out of the baked pack -------------
 *
 * `itManagerMakeItem` used to build every item's DObj tree off
 * `attr->data`, an N64 DObjDesc array whose entries carry display lists
 * the port draws nothing from. This drives the replacement: the baked
 * pack, keyed by the item's own `o_attributes`, loaded on first use.
 *
 * The count is the pack's own joint count -- Capsule bakes 4 -- and the
 * keys are the same numbers src/dc/itemoffsets.h carries, so this also
 * pins that the loader's table and the exporter agree.
 */
static void test_it_model(void)
{
    GObj gobj;
    DObj *root;

    memset(&gobj, 0, sizeof(gobj));

    /* Capsule: 4 joints, the first a throwaway root with no geometry --
     * every item's DObjDesc[0] carries no display list, which is why
     * itManagerMakeItem ejects it (lbCommonEjectTreeDObj) after */
    CHECK(itemModelAddToGObj(&gobj, 0x50) == 4);
    root = DObjGetStruct(&gobj);
    CHECK(root != NULL);
    CHECK(root->child != NULL);

    /* Heart and Sword: 2 and 3 joints. These two baked to NOTHING until
     * their `dl` is a DObjDLLink array rather than a
     * display list, so read as commands it disassembled to gsDPNoOpTag
     * and gsDPNoOp (dobj_dl_links in the exporter has the story). They
     * are the reason this test checks a count rather than just success */
    CHECK(itemModelAddToGObj(&gobj, 0x100) == 2);   /* Heart */
    CHECK(itemModelAddToGObj(&gobj, 0x190) == 3);   /* Sword */

    /* and an offset that is not an item at all */
    CHECK(itemModelAddToGObj(&gobj, 0x1234) == -1);

    /* ---- the two models whose PROC sets the render mode -------------
     *
     * Snorlax and Clefairy are the only two items whose display list
     * sets no render mode while their display proc does, so the baker's
     * fall-back (DL head 0, the opaque list) decided it and both drew as
     * a solid rectangle of their TLUT's transparent entry.
     * PROC_RENDERMODE in tools/export/ssb_itemmodelexport.py seeds the mode the
     * proc sets: G_RM_AA_XLU_SURF for Snorlax's fall
     * (itKabigonFallProcDisplay) and G_RM_AA_ZB_TEX_EDGE for Clefairy
     * (itPippiCommonProcDisplay). Each is one quad, so the whole model
     * is the one batch checked here */
    {
        GObj kg, pg;
        Fighter *m;

        memset(&kg, 0, sizeof(kg));
        memset(&pg, 0, sizeof(pg));

        CHECK(itemModelAddToGObj(&kg, 0x7a8) == 2);      /* Kabigon */
        m = dc_model_of(&kg);
        CHECK(m != NULL);
        CHECK(m->hd->batch_count == 1);
        CHECK((m->batches[0].bucket & FPACK_LIST_MASK) == FPACK_LIST_TR);

        CHECK(itemModelAddToGObj(&pg, 0xc74) == 2);      /* Pippi */
        m = dc_model_of(&pg);
        CHECK(m != NULL);
        CHECK(m->hd->batch_count == 1);
        CHECK((m->batches[0].bucket & FPACK_LIST_MASK) == FPACK_LIST_PT);
    }

    /* ---- the MObj question, and the answer is NULL ----------------
     *
     * A pack carries an MObj section only if `f->mobjs != NULL`
     * (fighter.c:441), and dc_model_add_dobjs never invents one -- so a
     * DObj from a pack with no MObj section has `dobj->mobj == NULL`.
     *
     * THE BAKE PRODUCES NONE, FOR ANY ITEM. `pack_item` calls
     * `build_pack(...)` with no `mobjs=` argument at all, so every item
     * pack the port has is MObj-less -- including the two whose tables
     * carry a MatAnimJoint (Star, Fire Flower) and whose `animated` flag
     * was supposed to keep them.
     *
     * That would be cosmetic -- the baker folds each MObjSub into its own
     * batch with its own baked picture, so the geometry and colours are
     * right -- if nothing wrote through the pointer. FOUR PORTED ITEM
     * FILES DO, unconditionally:
     *
     *   itgshell.c:138, 145, 537    gcAddMObjMatAnimJoint(dobj->mobj, ...),
     *                               mobj->matanim_joint.event32, and the
     *                               palette_id that IS the difference
     *                               between a Green Shell and a Red one
     *   itrshell.c:240, 247, 696    the same three
     *   itbombhei.c:517, 691        the walk cycle's MatAnimJoint
     *   itnbumper.c:201 … 621        eleven writes of palette_id
     *
     * On the N64 every one of those DObjs HAS an MObj -- a DObjDesc entry
     * names its MObjSub and gcSetupCustomDObjs allocates one for it -- so
     * this is a gap in the bake, not a decomp quirk. On the target a NULL
     * `dobj->mobj->palette_id = 1.0F` is a store to whatever offset
     * palette_id sits at past address zero: a crash or memory corruption,
     * on a Bob-omb, a Green Shell, a Red Shell or a Bumper.
     *
     * FIXED: the exporter now emits the FPackMObjs section
     * for every table that carries `p_mobjsubs`, so the pointer is real.
     * The assertion below flipped from `== NULL` -- which is what made
     * this a ledger row rather than a guess -- to `!= NULL`, and the
     * exporter's own `--list` is the other half: all 34 items bake to
     * identical joints/verts/tris/texs/batches before and after, so the
     * section was added without the picture moving.
     *
     * Green Shell is the probe: its `p_mobjsubs` is the shell pair's
     * SHARED chain (0x5de0), and the palette_id that IS the Green/Red
     * difference is a write this test now knows lands on a real MObj. */
    {
        GObj sgobj;

        memset(&sgobj, 0, sizeof(sgobj));

        /* The MObj lands on the shell's REAL joint, which is pack joint
         * 1 -- joint 0 is the throwaway root with no display list, and
         * itManagerMakeItem ejects it, which is why `dobj->mobj` in
         * itGShellMakeItem is this one under another name. Asserted on
         * the child rather than the root for exactly that reason: this
         * fixture does not eject, so the root is still the throwaway.
         * (The first draft asserted the root and failed, which is what
         * said the MObj was there but one level down.) */
        CHECK(itemModelAddToGObj(&sgobj, 0x53c) == 2);   /* GShell */
        CHECK(DObjGetStruct(&sgobj) != NULL);
        CHECK(DObjGetStruct(&sgobj)->child != NULL);
        CHECK(DObjGetStruct(&sgobj)->child->mobj != NULL);
    }

    /* ---- the Poké Ball's opening rays -----------------
     *
     * The one effect in the port the ITEM system makes rather than a
     * fighter, and the first effect pack here to carry an MObjSub at all
     * -- efManagerMBallRaysEffectDesc's MObjSub offset is the first one in
     * the table that is not 0x0. Both halves are checked: the pack loads
     * with the tree and the MObjs the exporter says (4 joints, 4 MObjs,
     * tools/export/ssb_effectexport.py --what mballrays), and the maker
     * returns a GObj with its position set. */
    {
        static Fighter rays;
        int pal_bank = 0;

        memset(&rays, 0, sizeof(rays));

        CHECK(fighter_load(&rays, "efmballrays.mdl", &pal_bank) == 0);

        if (rays.hd != NULL)
        {
            CHECK(rays.hd->joint_count == 4);
            CHECK(rays.mobjs != NULL);
            CHECK(rays.mobjs->mobj_count == 4);
            CHECK(rays.hd->anim_count == 1);
        }
    }

    {
        Vec3f rays_pos;
        GObj *rays_gobj;

        rays_pos.x = 11.0F;
        rays_pos.y = 22.0F;
        rays_pos.z = 33.0F;

        rays_gobj = efManagerMBallRaysMakeEffect(&rays_pos);

        CHECK(rays_gobj != NULL);

        if (rays_gobj != NULL)
        {
            CHECK_NEAR(DObjGetStruct(rays_gobj)->translate.vec.f.x, 11.0F, 1e-3F);
            CHECK_NEAR(DObjGetStruct(rays_gobj)->translate.vec.f.y, 22.0F, 1e-3F);
            CHECK_NEAR(DObjGetStruct(rays_gobj)->translate.vec.f.z, 33.0F, 1e-3F);
        }
    }
}

/* effect calls the port had dropped though every maker
 * was ported: the shield-break burst at YRotN, Kirby's surface stars from
 * ftMainProcPhysicsMap, the Poke Ball rays from a Pikachu or Jigglypuff
 * entrance, and DK's barrel debris through ftParamMakeEffect. The two
 * particle ones are counted off the generator queue with the real common
 * bank installed; the two model ones off the effect link. Everything made
 * is handed back. */
#define V01_GN_MAX 64
extern LBGenerator *sLBParticleGeneratorsQueued;   /* src/dc/lbparticle.c */
static s32 v01_snapshot_generators(LBGenerator **out)
{
    LBGenerator *gn;
    s32 n = 0;

    for (gn = sLBParticleGeneratorsQueued; gn != NULL && n < V01_GN_MAX; gn = gn->next)
    {
        out[n++] = gn;
    }
    return n;
}
/* the generators queued now that were not in `before`, ejected; the
 * count is returned and the first two positions copied out */
static s32 v01_take_new_generators(LBGenerator **before, s32 before_num, Vec3f *pos)
{
    LBGenerator *now[V01_GN_MAX];
    s32 now_num = v01_snapshot_generators(now);
    s32 made = 0;
    s32 i, j;

    for (i = 0; i < now_num; i++)
    {
        for (j = 0; j < before_num; j++)
        {
            if (now[i] == before[j])
            {
                break;
            }
        }
        if (j == before_num)
        {
            if (made < 2)
            {
                pos[made] = now[i]->pos;
            }
            made++;
            lbParticleEjectGenerator(now[i]);
        }
    }
    return made;
}
static s32 v01_take_new_effects(GObj *tail_before)
{
    GObj *g = (tail_before != NULL) ? tail_before->link_next
                                    : gGCCommonLinks[nGCCommonLinkIDEffect];
    s32 made = 0;

    while (g != NULL)
    {
        GObj *next = g->link_next;

        made++;
        if (efGetStruct(g) != NULL)     /* the rays are made struct-less */
        {
            efManagerSetPrevStructAlloc(efGetStruct(g));
        }
        gcEjectGObj(g);
        g = next;
    }
    return made;
}
static GObj *v01_effect_tail(void)
{
    GObj *g, *tail = NULL;

    for (g = gGCCommonLinks[nGCCommonLinkIDEffect]; g != NULL; g = g->link_next)
    {
        tail = g;
    }
    return tail;
}
static void test_v01_dropped_effects(void)
{
    LBScript **saved_scripts = sLBParticleScriptBanks[0];
    LBTexture **saved_textures = sLBParticleTextureBanks[0];
    s32 saved_nscripts = sLBParticleScriptBanksNum[0];
    s32 saved_ntextures = sLBParticleTextureBanksNum[0];
    s32 saved_bank = gEFManagerParticleBankID;
    LBGenerator *before[V01_GN_MAX];
    s32 before_num;
    Vec3f pos[2];
    Vec3f top;
    GObj *tail;

    host_load_particle_bank(0, "efcommon.scb", "efcommon.txb");
    gEFManagerParticleBankID = 0;

    /* ---- the shield breaks: one burst (common script 3) at YRotN. The
     * mock has no YRotN, and a joint put in its slot would be taken for
     * hidden part 2's root, so the entry itself falls back to TopN on
     * this build. */
    spawn(0.0F, 300.0F);
    mock_anim_len = 1e9f;
    frame(0, 0, 0, 0, 0);
    top = fp.joints[nFTPartsJointTopN]->translate.vec.f;
    before_num = v01_snapshot_generators(before);
    ftCommonShieldBreakFlyCommonSetStatus(mock_gobj);
    CHECK_STATUS(nFTCommonStatusShieldBreakFly);
    CHECK(v01_take_new_generators(before, before_num, pos) == 1);
    CHECK_NEAR(pos[0].x, top.x, 1e-2F);
    CHECK_NEAR(pos[0].y, top.y, 1e-2F);

    /* ---- Kirby: a floor newly touched this tic throws one star at the
     * collision box's bottom; a floor he was already on throws none, and
     * nobody else throws any. ---- */
    {
        s32 i, stars = 0, landed_at = -1;

        spawn(0.0F, 700.0F);
        mock_anim_len = 1e9f;
        fp.fkind = nFTKindKirby;
        for (i = 0; i < 90; i++)
        {
            before_num = v01_snapshot_generators(before);
            frame(0, 0, 0, 0, 0);
            if (v01_take_new_generators(before, before_num, pos) != 0)
            {
                stars++;
                if (landed_at < 0)
                {
                    landed_at = i;
                    CHECK(fp.ga == nMPKineticsGround);
                    CHECK_NEAR(pos[0].y, DObjGetStruct(mock_gobj)->translate.vec.f.y + fp.coll_data.map_coll.bottom, 1e-2F);
                }
            }
        }
        CHECK(landed_at >= 0);
        CHECK(stars == 1);

        spawn(0.0F, 700.0F);
        mock_anim_len = 1e9f;
        stars = 0;
        for (i = 0; i < 90; i++)
        {
            before_num = v01_snapshot_generators(before);
            frame(0, 0, 0, 0, 0);
            stars += v01_take_new_generators(before, before_num, pos);
        }
        CHECK(fp.ga == nMPKineticsGround);
        CHECK(stars == 0);
    }

    /* ...and a wall and a ceiling together, two stars, off the box's
     * sides and top */
    fp.fkind = nFTKindKirby;
    fp.coll_data.mask_prev = 0;
    fp.coll_data.mask_curr = MAP_FLAG_RWALL | MAP_FLAG_CEIL;
    before_num = v01_snapshot_generators(before);
    ftParamKirbyTryMakeMapStarEffect(mock_gobj);
    CHECK(v01_take_new_generators(before, before_num, pos) == 2);
    fp.fkind = nFTKindMario;

    /* ---- the entrance: flag1 opens the Poke Ball for Pikachu, but only
     * clears for Mario ---- */
    spawn(0.0F, 300.0F);
    fp.entry_pos.x = 5.0F;
    fp.entry_pos.y = 6.0F;
    fp.entry_pos.z = 7.0F;
    tail = v01_effect_tail();
    fp.motion_vars.flags.flag1 = 1;
    ftCommonAppearUpdateEffects(mock_gobj);
    CHECK(fp.motion_vars.flags.flag1 == 0);
    CHECK(v01_take_new_effects(tail) == 0);
    fp.fkind = nFTKindPikachu;
    tail = v01_effect_tail();
    fp.motion_vars.flags.flag1 = 1;
    ftCommonAppearUpdateEffects(mock_gobj);
    CHECK(fp.motion_vars.flags.flag1 == 0);
    {
        GObj *rays = (tail != NULL) ? tail->link_next : gGCCommonLinks[nGCCommonLinkIDEffect];

        CHECK(rays != NULL);
        if (rays != NULL)
        {
            CHECK_NEAR(DObjGetStruct(rays)->translate.vec.f.x, 5.0F, 1e-3F);
        }
    }
    CHECK(v01_take_new_effects(tail) == 1);
    fp.fkind = nFTKindMario;

    /* ---- DK's Appear1 box-smash event: the debris GObj ---- */
    {
        Vec3f zero = { 0.0F, 0.0F, 0.0F };

        tail = v01_effect_tail();
        ftParamMakeEffect(mock_gobj, nEFKindBoxSmash, nFTPartsJointTopN, &zero, NULL, 0, FALSE, 0);
        CHECK(v01_take_new_effects(tail) == 1);
    }
    spawn(0.0F, 0.0F);

    gEFManagerParticleBankID = saved_bank;
    sLBParticleScriptBanks[0] = saved_scripts;
    sLBParticleTextureBanks[0] = saved_textures;
    sLBParticleScriptBanksNum[0] = saved_nscripts;
    sLBParticleTextureBanksNum[0] = saved_ntextures;
}

/* the KO screen's HUD particles: the stock-row burst, the
 * +1/-1 score popup and the stock steal's two sparkles, all made with
 * LBPARTICLE_MASK_GENLINK(2) -- struct list 3 -- in HUD coordinates times
 * four. Counted off that list with the common bank installed, and handed
 * back. */
extern LBParticle *sLBParticleStructsAllocLinks[];   /* src/dc/lbparticle.c */
extern IFPlayerSteal sIFCommonPlayerStealInterface[];
extern void ifCommonPlayerStockStealProcUpdate(GObj *interface_gobj);
static s32 v02_take_hud_particles(LBParticle *head_before, Vec3f *pos)
{
    LBParticle *pc = sLBParticleStructsAllocLinks[3];
    s32 made = 0;

    /* lbParticleMakeStruct pushes onto the head, so the new ones are
     * everything in front of the old head */
    while ((pc != NULL) && (pc != head_before))
    {
        LBParticle *next = pc->next;

        if (made == 0)
        {
            *pos = pc->pos;
            if (pc->xf != NULL)
            {
                *pos = pc->xf->translate;
            }
        }
        made++;
        lbParticleEjectStruct(pc);
        pc = next;
    }
    return made;
}
static void test_v02_hud_particles(void)
{
    LBScript **saved_scripts = sLBParticleScriptBanks[0];
    LBTexture **saved_textures = sLBParticleTextureBanks[0];
    s32 saved_nscripts = sLBParticleScriptBanksNum[0];
    s32 saved_ntextures = sLBParticleTextureBanksNum[0];
    s32 saved_bank = gEFManagerParticleBankID;
    LBGenerator *before[V01_GN_MAX];
    s32 before_num;
    LBParticle *head;
    Vec3f pos;
    FTStruct hud_fp;

    host_load_particle_bank(0, "efcommon.scb", "efcommon.txb");
    gEFManagerParticleBankID = 0;
    ifCommonPlayerDamageSetDigitPositions();

    memset(&hud_fp, 0, sizeof(hud_fp));
    hud_fp.player = 1;

    /* the burst on player 2's stock row */
    head = sLBParticleStructsAllocLinks[3];
    before_num = v01_snapshot_generators(before);
    ifCommonPlayerStockMakeStockSnap(&hud_fp);
    /* two: script 0x26's first bytecode makes a second at once */
    CHECK(v02_take_hud_particles(head, &pos) == 2);
    CHECK_NEAR(pos.x, 125.0F * 4.0F, 1e-2F);
    CHECK_NEAR(pos.y, 210.0F * 4.0F, 1e-2F);
    v01_take_new_generators(before, before_num, &pos);

    /* the popup, both signs, thirteen under the row */
    head = sLBParticleStructsAllocLinks[3];
    ifCommonPlayerScoreMakeEffect(&hud_fp, 1);
    CHECK(v02_take_hud_particles(head, &pos) == 1);
    CHECK_NEAR(pos.x, 125.0F * 4.0F, 1e-2F);
    CHECK_NEAR(pos.y, 223.0F * 4.0F, 1e-2F);
    head = sLBParticleStructsAllocLinks[3];
    ifCommonPlayerScoreMakeEffect(&hud_fp, -1);
    CHECK(v02_take_hud_particles(head, &pos) == 1);

    /* the stolen icon's last tic: the landing sparkle at the thief's
     * stock icon, and the icon gone */
    {
        GObj *icon = gcMakeGObjSPAfter(nGCCommonKindInterface, NULL, nGCCommonLinkIDInterface, GOBJ_PRIORITY_DEFAULT);

        CHECK(icon != NULL);
        if (icon != NULL)
        {
            ifSetPlayer(icon, 1);
            sIFCommonPlayerStealInterface[1].anim_frames = 1;
            head = sLBParticleStructsAllocLinks[3];
            before_num = v01_snapshot_generators(before);
            ifCommonPlayerStockStealProcUpdate(icon);
            CHECK(v02_take_hud_particles(head, &pos) == 1);
            CHECK_NEAR(pos.x, (125.0F - 24.0F) * 4.0F, 1e-2F);
            CHECK_NEAR(pos.y, (210.0F - 20.0F) * 4.0F, 1e-2F);
            v01_take_new_generators(before, before_num, &pos);
        }
    }

    gEFManagerParticleBankID = saved_bank;
    sLBParticleScriptBanks[0] = saved_scripts;
    sLBParticleTextureBanks[0] = saved_textures;
    sLBParticleScriptBanksNum[0] = saved_nscripts;
    sLBParticleTextureBanksNum[0] = saved_ntextures;
}

/* ---- the container machinery ---------------------------
 *
 * `itMainMakeContainerItem` is what a Container's own proc_hit and its
 * thrown map both reach: roll a kind out of `gITManagerRandomWeights` and
 * spawn it as a child item. That table is built from the stage's
 * MPGroundData.item_weights, which is a pointer that must be
 * copied -- with it NULL the sum is zero and this returns
 * FALSE, which is a container that explodes instead of dropping.
 *
 * So this drives both halves: a zero sum (the state of a battle with no
 * item table) and a one-entry table. The spawn is REAL and goes the
 * whole way -- itManagerMakeItemSetupCommon -> dITManagerProcMakeList ->
 * itTomatoMakeItem -> itManagerMakeItem, reading the tomato's attributes
 * out of the item pack and building its tree from the baked model -- so a
 * pass here is the first time this port has ever made an item end to end.
 */
static void test_it_container(void)
{
    static ITStruct pool[ITEM_ALLOC_MAX];
    static ITStruct ptip;
    static GObj parent;
    static DObj pdobj;
    ITStruct *save_free = gITManagerStructsAllocFree;
    ITRandomWeights save_weights = gITManagerRandomWeights;
    u8 kinds[1];
    u16 blocks[2];
    GObj *tail_before;
    s32 before;
    int i;

    CHECK(itemPackLoaded() == 1);
    CHECK(gITManagerCommonData != NULL);

    for (i = 0; i < ITEM_ALLOC_MAX - 1; i++) pool[i].next = &pool[i + 1];
    pool[ITEM_ALLOC_MAX - 1].next = NULL;
    gITManagerStructsAllocFree = pool;

    memset(&parent, 0, sizeof(parent));
    memset(&pdobj, 0, sizeof(pdobj));
    memset(&ptip, 0, sizeof(ptip));
    parent.obj = &pdobj;
    parent.user_data.p = &ptip;

    /* A parent ITEM's own collision state, as itManagerMakeItem leaves it:
     * ITEM_FLAG_PARENT_ITEM hands coll_data.p_translate straight to
     * mpCommonRunItemCollisionDefault, which dereferences it (a zeroed fixture
     * crashes on that line). */
    ptip.coll_data.p_translate = &pdobj.translate.vec.f;
    ptip.coll_data.p_map_coll = &ptip.coll_data.map_coll;

    /* and its attributes, which itManagerMakeItem also sets and which
     * itMainSetAppearSpin reads `spin_speed` out of. Capsule's own table
     * is the one to hand it -- 120 is the value test_item_pack pins from
     * the decomp -- so the parent stands in for a real container. */
    ptip.attr = itemPackAttr(gITManagerCommonData, 0x50);
    CHECK(ptip.attr != NULL);

    /* no weights: the container has nothing to give, and says so --
     * itMainSetAppearSpin is NOT called on this arm, which is the
     * difference between "dropped something" and "broke" */
    memset(&gITManagerRandomWeights, 0, sizeof(gITManagerRandomWeights));
    CHECK(itMainMakeContainerItem(&parent) == FALSE);

    /* one toggled kind, one weight: itMainGetWeightedItemKind samples
     * syUtilsRandIntRange(1) = 0 and the search returns index 0 */
    kinds[0] = nITKindTomato;
    blocks[0] = 0;
    blocks[1] = 1;
    gITManagerRandomWeights.kinds = kinds;
    gITManagerRandomWeights.blocks = blocks;
    gITManagerRandomWeights.weights_sum = 1;
    gITManagerRandomWeights.valids_num = 1;

    /* what is already on the item link: earlier item tests leave their
     * spawns there, so the teardown below must eject only what THIS test
     * adds and leave their chain alone */
    tail_before = NULL;
    {
        GObj *g;

        for (g = gGCCommonLinks[nGCCommonLinkIDItem]; g != NULL; g = g->link_next)
        {
            tail_before = g;
        }
    }

    before = 0;
    {
        ITStruct *ip;

        for (ip = gITManagerStructsAllocFree; ip != NULL; ip = ip->next)
        {
            before++;
        }
    }
    CHECK(itMainMakeContainerItem(&parent) == TRUE);

    /* and it took one out of the pool: an item exists */
    {
        ITStruct *ip;
        s32 after = 0;

        for (ip = gITManagerStructsAllocFree; ip != NULL; ip = ip->next)
        {
            after++;
        }
        CHECK(after == before - 1);
    }

    /* the spawn was real: a Tomato GObj is on the item link */
    {
        GObj *g;
        s32 tomatoes = 0;

        for (g = gGCCommonLinks[nGCCommonLinkIDItem]; g != NULL; g = g->link_next)
        {
            if (itGetStruct(g)->kind == nITKindTomato)
            {
                tomatoes++;
            }
        }
        CHECK(tomatoes == 1);
    }

    /* Drain what this test spawned -- the item AND its arrow, which sits
     * on the INTERFACE link and which a later test asserts is empty (the
     * same teardown test_it_make_item_arrow needs). Only the chain after
     * `tail_before`, so the earlier tests' own leftovers stay where they
     * left them. */
    {
        GObj *g = (tail_before != NULL) ? tail_before->link_next
                                        : gGCCommonLinks[nGCCommonLinkIDItem];

        while (g != NULL)
        {
            GObj *next = g->link_next;
            GObj *arrow = itGetStruct(g)->arrow_gobj;

            if (arrow != NULL)
            {
                gcEjectGObj(arrow);
            }
            gcEjectGObj(g);
            g = next;
        }
    }

    gITManagerRandomWeights = save_weights;
    gITManagerStructsAllocFree = save_free;
}

/* ---- the Container smash effect ------------------------
 *
 * The one place in it/ that the decomp draws from a RAW display list:
 * `itBoxContainerSmashMakeEffect` reads
 * `attr->data - BoxDataStart + BoxEffectDisplayList` and hands that Gfx*
 * to ITCONTAINER_EFFECT_COUNT hand-made DObjs -- which BOTH itbox.c and
 * ittaru.c reach, so it is shared machinery and not a Crate detail.
 *
 * The port bakes it instead: `EffectDisplayList Box` is one block, 3,416
 * bytes, and tools/export/ssb_itemmodelexport.py --effect boxsmash makes it a
 * one-joint model (4 verts, 2 tris -- a debris quad). This drives the
 * maker and checks that every piece came out carrying that payload, which
 * is the thing that decides whether a smashed Crate is visible or not.
 */
static void test_it_box_effect(void)
{
    Vec3f pos = { 10.0F, 20.0F, 30.0F };
    GObj *g;
    GObj *tail_before;
    s32 pieces = 0, with_payload = 0;

    /* the pack the effect needs, loaded on first use like the items */
    CHECK(itemModelBoxSmash() != NULL);

    /* NOT ef_pool_reset(). That hands every EFStruct back to the pool,
     * including the ones live effect GObjs from earlier tests still
     * point at -- and `effect_vars` is a UNION, so the smash effect's
     * `container.lifetime` lands on top of whatever the previous user's
     * field was. A quake GObj left over from an earlier test then reads
     * its `priority` out of this effect's lifetime when test_gobj_threads
     * pumps gcRunAll, gets 165, and objman.c spins on "GObjProcess's
     * priority is bad value" forever. (test found
     * that; the fix is to take nothing back and to give back what is
     * made -- below.) */
    tail_before = NULL;
    for (g = gGCCommonLinks[nGCCommonLinkIDEffect]; g != NULL; g = g->link_next)
    {
        tail_before = g;
    }

    itBoxContainerSmashMakeEffect(&pos);

    for (g = (tail_before != NULL) ? tail_before->link_next
                                   : gGCCommonLinks[nGCCommonLinkIDEffect];
         g != NULL; g = g->link_next)
    {
        DObj *d;

        for (d = DObjGetStruct(g); d != NULL; d = d->sib_next)
        {
            pieces++;
            if (d->dv != NULL)
            {
                with_payload++;
            }
        }
    }
    CHECK(pieces == ITCONTAINER_EFFECT_COUNT);
    CHECK(with_payload == ITCONTAINER_EFFECT_COUNT);

    /* give the effect GObj back: it is on the effect link and its process
     * is a FUNC, which gcRunAll runs -- a later test pumps the object
     * system and would otherwise execute this one against a scene that
     * has moved on */
    g = (tail_before != NULL) ? tail_before->link_next
                              : gGCCommonLinks[nGCCommonLinkIDEffect];

    while (g != NULL)
    {
        GObj *next = g->link_next;

        gcEjectGObj(g);
        g = next;
    }
}

/* ---- the Barrel's roll state ---------------------------
 *
 * The seventh status the other three Containers do not have, and the one
 * piece of state that exists only for it. A thrown Barrel that lands
 * slowly keeps rolling: `itTaruRollProcUpdate` accelerates it along the
 * floor's own angle, spins the model by the resulting speed, and spends a
 * LIFETIME -- blinking, one frame on and one off, once that falls below
 * ITTARU_DESPAWN_FLASH_START, and returning TRUE (destroy) at zero.
 *
 * The returns are what the item main loop reads, so they are the
 * assertions: FALSE while rolling, TRUE at the end.
 */
extern ITDesc dITTaruItemDesc;
extern ITStatusDesc dITTaruStatusDescs[];

static void test_it_taru_roll(void)
{
    GObj titem;
    DObj tdobj;
    ITStruct tip;

    CHECK(dITTaruItemDesc.kind == nITKindTaru);
    CHECK(dITTaruItemDesc.o_attributes == (intptr_t)0x634);  /* reloc_data.us.h: llITCommonDataTaruItemAttributes */
    CHECK(dITTaruItemDesc.proc_update == itTaruFallProcUpdate);

    /* a resting Barrel: zero velocity, so the lifetime is what runs */
    memset(&titem, 0, sizeof(titem));
    memset(&tdobj, 0, sizeof(tdobj));
    memset(&tip, 0, sizeof(tip));
    titem.user_data.p = &tip;
    titem.obj = &tdobj;
    tip.physics.vel_air.x = 0.0F;
    tip.physics.vel_air.y = 0.0F;

    /* A FLAT floor, in the convention the port already uses for one: the
     * Box's own test writes `syUtilsArcTan2(1.0F, 0.0F) - F_CLC_DTOR32(90.0F)`
     * for a floor at angle zero, so `floor_angle = (0, 1)` is the vector
     * that makes it one. Get this wrong and the roll's first line
     * ACCELERATES the Barrel instead -- which is what this test did on its
     * first run: sqrt_vel went to 141, the lifetime never reached its
     * decrement, and the loop below spun until the suite timed out. */
    tip.coll_data.floor_angle.x = 0.0F;
    tip.coll_data.floor_angle.y = 1.0F;

    /* well above the flash threshold: it just counts down, no blink */
    tip.lifetime = ITTARU_DESPAWN_FLASH_START + 2;
    CHECK(itTaruRollProcUpdate(&titem) == FALSE);
    CHECK(tip.lifetime == ITTARU_DESPAWN_FLASH_START + 1);
    CHECK((tdobj.flags & DOBJ_FLAG_HIDDEN) == 0);

    /* below it: the odd lifetimes flip the hidden flag. Two calls past
     * the threshold put it at an ODD lifetime, which is the frame that
     * blinks */
    CHECK(itTaruRollProcUpdate(&titem) == FALSE);   /* lifetime = START */
    CHECK(itTaruRollProcUpdate(&titem) == FALSE);   /* lifetime = START-1, odd */
    CHECK(tip.lifetime == ITTARU_DESPAWN_FLASH_START - 1);
    CHECK((tdobj.flags & DOBJ_FLAG_HIDDEN) != 0);

    /* and the call that brings it to ZERO is the one that destroys it --
     * `lifetime--` runs first, so a Barrel at 1 returns TRUE from its
     * next update and the item main loop ejects it. BOUNDED, because a
     * fixture that stops the countdown must fail here rather than hang
     * the suite, which is what the unbounded first draft of this loop
     * did. */
    {
        s32 guard = ITTARU_DESPAWN_FLASH_START + 4;
        sb32 last = FALSE;

        while ((tip.lifetime > 0) && (guard-- > 0))
        {
            last = itTaruRollProcUpdate(&titem);
        }
        CHECK(tip.lifetime == 0);
        CHECK(guard > 0);       /* the countdown really ran */
        CHECK(last == TRUE);    /* and the last step was the destroying one */
    }

    /* a Barrel with real speed spins by the value it stores back */
    memset(&tip, 0, sizeof(tip));
    tdobj.flags = 0;
    tip.coll_data.floor_angle.x = 0.0F;
    tip.coll_data.floor_angle.y = 1.0F;
    tip.physics.vel_air.x = -1.0F;   /* lr -1, and fast enough to spin */
    itTaruRollProcUpdate(&titem);
    CHECK(tip.lr == -1);
    /* the sign is INVERTED for a leftward roll -- `lr == -1` takes the
     * positive multiplier -- which is the decomp's own and is why this
     * asserts rather than assumes a direction */
    CHECK(tip.item_vars.taru.roll_rotate_step > 0.0F);
}

/* ---- the Poké Ball monsters ---------------------------
 *
 * `itMewMakeItem` and `itTosakintoMakeItem` are the first two of the
 * thirteen, and this test is about the one thing all thirteen share and
 * nothing has yet checked: the OFFSET ARITHMETIC that turns an item's
 * ITAttributes into its anim scripts.
 *
 * `itGetMonsterAnimNode(ip, off)` is `itGetPData(ip, off, Bank)` -- the
 * decomp's own macro, written `&llITCommonDataMonsterAnimBankStart` there
 * because on the N64 those symbols are the reloc file's boundary labels
 * and their ADDRESSES are the offsets. src/dc/itmonster.h `#undef`s it and
 * writes the numbers instead. It also silently depends on `attr->data`
 * pointing at the monster's own model block, which is what
 * `itemPackBlock(..., "DataStart Mew")` says independently.
 *
 * So the assertions below are a two-sided check rather than a tautology:
 * `attr->data` must EQUAL the pack's own `DataStart <Monster>` block, and
 * the macro's result must EQUAL its own `AnimBankStart Monster` block --
 * both of which the exporter recorded from the decomp's symbol table
 * (tools/export/ssb_itemexport.py's block list, which is where
 * src/dc/itemoffsets.h's numbers come from too). Get either wrong and
 * every monster animates from the wrong bytes with nothing to show for it.
 *
 * The spawn is REAL: itManagerMakeItem -> the pack's attributes -> the
 * baked model -> the monster's own rise-out-of-the-ball setup.
 */
extern ITDesc dITMewItemDesc;
extern ITDesc dITTosakintoItemDesc;
extern ITDesc dITKabigonItemDesc;
extern ITDesc dITMLuckyItemDesc;
extern ITDesc dITNyarsItemDesc;
extern WPDesc dITNyarsWeaponCoinWeaponDesc;
extern ITDesc dITDogasItemDesc;
extern WPDesc dITDogasWeaponSmogWeaponDesc;
extern ITDesc dITIwarkItemDesc;
extern WPDesc dITIwarkWeaponRockWeaponDesc;
extern ITDesc dITStarmieItemDesc;
extern WPDesc dITStarmieWeaponSwiftWeaponDesc;
extern ITDesc dITLizardonItemDesc;
extern WPDesc dITLizardonWeaponFlameWeaponDesc;
extern ITDesc dITSpearItemDesc;
extern WPDesc dITSpearWeaponSwarmWeaponDesc;
extern WPDesc dITPippiWeaponSwarmWeaponDesc;
extern ITDesc dITKamexItemDesc;
extern WPDesc dITKamexWeaponHydroWeaponDesc;
extern ITDesc dITPippiItemDesc;
extern void (*dITPippiStatusProcList[])(GObj*);
extern ITDesc dITMBallItemDesc;
extern s32 dITManagerForceMonsterKind;
/* the 45-entry dispatch table (itmanager.c), global so the roster's own
 * callers can reach it -- the decomp declares it in it/itmanager.h */
extern GObj* (*dITManagerProcMakeList[])(GObj*, Vec3f*, Vec3f*, u32);

static void test_it_monster(void)
{
    static ITStruct pool[ITEM_ALLOC_MAX];
    static ITStruct ptip;
    static GObj parent;
    static DObj pdobj;
    ITStruct *save_free = gITManagerStructsAllocFree;
    const void *bank;
    GObj *tail_before;
    Vec3f pos;
    Vec3f vel;
    int i;

    /* The desc's own two independent facts, before anything is spawned:
     * the kind and the attribute key src/dc/itemoffsets.h carries. */
    CHECK(dITMewItemDesc.kind == nITKindMew);
    CHECK(dITMewItemDesc.o_attributes == (intptr_t)llITCommonDataMewItemAttributes);
    CHECK(dITMewItemDesc.proc_update == itMewCommonProcUpdate);
    CHECK(dITTosakintoItemDesc.kind == nITKindTosakinto);
    CHECK(dITTosakintoItemDesc.o_attributes == (intptr_t)llITCommonDataTosakintoItemAttributes);
    CHECK(dITTosakintoItemDesc.proc_update == itTosakintoCommonProcUpdate);
    CHECK(dITKabigonItemDesc.kind == nITKindKabigon);
    CHECK(dITKabigonItemDesc.o_attributes == (intptr_t)llITCommonDataKabigonItemAttributes);
    CHECK(dITKabigonItemDesc.proc_update == itKabigonCommonProcUpdate);
    CHECK(dITMLuckyItemDesc.kind == nITKindMLucky);
    CHECK(dITMLuckyItemDesc.o_attributes == (intptr_t)llITCommonDataMLuckyItemAttributes);
    CHECK(dITMLuckyItemDesc.proc_update == itMLuckyCommonProcUpdate);
    CHECK(dITNyarsItemDesc.kind == nITKindNyars);
    CHECK(dITNyarsItemDesc.o_attributes == (intptr_t)llITCommonDataNyarsItemAttributes);
    CHECK(dITDogasItemDesc.kind == nITKindDogas);
    CHECK(dITDogasItemDesc.o_attributes == (intptr_t)llITCommonDataDogasItemAttributes);
    CHECK(dITDogasWeaponSmogWeaponDesc.kind == nWPKindDogasSmog);
    CHECK(dITDogasWeaponSmogWeaponDesc.o_attributes ==
          (intptr_t)llITCommonDataDogasSmogWeaponAttributes);
    CHECK(dITDogasWeaponSmogWeaponDesc.flags == 0x03);   /* DOBJDESC | DOBJLINKS */
    CHECK(dITIwarkItemDesc.kind == nITKindIwark);
    CHECK(dITIwarkItemDesc.o_attributes == (intptr_t)llITCommonDataWarkItemAttributes);
    CHECK(dITIwarkWeaponRockWeaponDesc.kind == nWPKindIwarkRock);
    CHECK(dITIwarkWeaponRockWeaponDesc.o_attributes ==
          (intptr_t)llITCommonDataWarkRockWeaponAttributes);
    CHECK(dITIwarkWeaponRockWeaponDesc.flags == 0x01);   /* DOBJLINKS */
    /* the rock is the one weapon that writes mobj->texture_id_curr, and
     * the pack it draws from has three MObj frames -- so its pack must
     * CARRY an MObj (fix is what makes that true) */
    CHECK(dITIwarkWeaponRockWeaponDesc.proc_update == itIwarkWeaponRockProcUpdate);
    CHECK(dITStarmieItemDesc.kind == nITKindStarmie);
    CHECK(dITStarmieItemDesc.o_attributes == (intptr_t)llITCommonDataStarmieItemAttributes);
    CHECK(dITStarmieWeaponSwiftWeaponDesc.kind == nWPKindStarmieSwift);
    CHECK(dITStarmieWeaponSwiftWeaponDesc.o_attributes ==
          (intptr_t)llITCommonDataStarmieSwiftWeaponAttributes);
    CHECK(dITLizardonItemDesc.kind == nITKindLizardon);
    CHECK(dITLizardonItemDesc.o_attributes == (intptr_t)llITCommonDataLizardonItemAttributes);
    CHECK(dITLizardonWeaponFlameWeaponDesc.kind == nWPKindLizardonFlame);
    CHECK(dITLizardonWeaponFlameWeaponDesc.o_attributes ==
          (intptr_t)llITCommonDataLizardonFlameWeaponAttributes);
    CHECK(dITLizardonWeaponFlameWeaponDesc.flags == 0x00);   /* one DL, one DObj */
    CHECK(dITSpearItemDesc.kind == nITKindSpear);
    CHECK(dITSpearItemDesc.o_attributes == (intptr_t)llITCommonDataSpearItemAttributes);
    /* TWO descriptors, one weapon kind -- the only place in the port's
     * WPDesc set where that is so */
    CHECK(dITSpearWeaponSwarmWeaponDesc.kind == nWPKindSpearSwarm);
    CHECK(dITPippiWeaponSwarmWeaponDesc.kind == nWPKindSpearSwarm);
    CHECK(dITSpearWeaponSwarmWeaponDesc.o_attributes ==
          (intptr_t)llITCommonDataSpearSwarmWeaponAttributes);
    CHECK(dITPippiWeaponSwarmWeaponDesc.o_attributes ==
          (intptr_t)llITCommonDataPippiSwarmWeaponAttributes);
    CHECK(dITSpearWeaponSwarmWeaponDesc.o_attributes !=
          dITPippiWeaponSwarmWeaponDesc.o_attributes);
    CHECK(dITKamexItemDesc.kind == nITKindKamex);
    CHECK(dITKamexItemDesc.o_attributes == (intptr_t)llITCommonDataKamexItemAttributes);
    CHECK(dITKamexWeaponHydroWeaponDesc.kind == nWPKindKamexHydro);
    CHECK(dITKamexWeaponHydroWeaponDesc.o_attributes ==
          (intptr_t)llITCommonDataKamexHydroWeaponAttributes);
    CHECK(dITKamexWeaponHydroWeaponDesc.flags == 0x01);
    /* its proc_hit is a bare FALSE and its proc_hop is NULL -- a stream
     * that survives what it touches and never bounces */
    CHECK(dITKamexWeaponHydroWeaponDesc.proc_hit == itKamexWeaponHydroProcHit);
    CHECK(dITKamexWeaponHydroWeaponDesc.proc_hop == NULL);

    /* ---- Clefairy, and the point of its file: THE BINDING ----
     *
     * `dITPippiStatusProcList` is the one table in the thirteen that names
     * something out of every OTHER monster file, and it is what made
     * itpippi.c the last one that could be ported. So the assertion that
     * matters is the whole list, entry by entry, against the twelve
     * SetStatus functions -- a wrong or missing entry is a metronome that
     * turns Clefairy into the wrong monster, silently. */
    CHECK(dITPippiItemDesc.kind == nITKindPippi);
    CHECK(dITPippiItemDesc.o_attributes == (intptr_t)llITCommonDataPippiItemAttributes);

    {
        static void (*const kPippi[12])(GObj*) = {
            itIwarkAttackSetStatus,
            itKabigonJumpSetStatus,
            itTosakintoAppearSetStatus,
            itNyarsAttackSetStatus,
            itLizardonFallSetStatus,
            itSpearFlySetStatus,
            itKamexAppearSetStatus,
            itMLuckyAppearSetStatus,
            itStarmieNFollowSetStatus,
            itSawamuraFallSetStatus,
            itDogasAttackSetStatus,
            itMewFlySetStatus
        };
        int i;

        CHECK(ARRAY_COUNT(kPippi) ==
              (u32)(nITKindMBallCommonEnd - nITKindMBallCommonStart + 1));

        for (i = 0; i < (int)ARRAY_COUNT(kPippi); i++)
        {
            CHECK(dITPippiStatusProcList[i] == kPippi[i]);
            CHECK(dITPippiStatusProcList[i] != NULL);
        }
        /* and the last entry is MEW's -- which is the decomp's own
         * off-by-one and is kept. `itPippiCommonSelectMonster` computes
         * `kind = index + nITKindMBallMonsterStart` for its four fix-up
         * tests and dispatches with `dITPippiStatusProcList[index]`, and
         * the list SKIPS Clefairy's own kind while including Mew's. So
         * index 11 computes kind 43 (nITKindPippi) and then calls Mew's
         * SetStatus: the last roll gets Pippi's fix-up -- its attack state
         * turned off -- and becomes a Mew. Written down because it is
         * exactly the kind of one-off a reader would otherwise "fix". */
        CHECK(dITPippiStatusProcList[11] == itMewFlySetStatus);
        CHECK((nITKindMBallMonsterStart + 11) == nITKindPippi);
    }

    /* Meowth's Coin is the FIRST weapon a monster fires, and a weapon
     * whose descriptor the port's model table does not name spawns with
     * correct physics and NO PICTURE -- the wpManagerMakeWeapon half of
     * the invisible-item class this port has fought three times now.
     *
     * That table (sWPManagerModels) is deliberately target-only -- the
     * host build stubs wpManagerAddModel, see src/dc/wpmanager.c -- so
     * what the host can check is the PACK the row names: it loads through
     * the same fighter_load the target's wpModelLoad calls, and what comes
     * back is the tree the exporter says it baked (tools/
     * ssb_itemmodelexport.py --list: two joints, 2.4 KB). The ROW itself
     * is covered where it can be: the Makefile bakes the file it names and
     * tools/export/disc_layout.py has to have it in the manifest. */
    CHECK(dITNyarsWeaponCoinWeaponDesc.kind == nWPKindNyarsCoin);
    CHECK(dITNyarsWeaponCoinWeaponDesc.o_attributes ==
          (intptr_t)llITCommonDataNyarsCoinWeaponAttributes);
    CHECK(dITNyarsWeaponCoinWeaponDesc.flags == 0x01);   /* WEAPON_FLAG_DOBJLINKS */

    /* and the pack the row names is real, loaded the way the target loads
     * it: two joints, one quad, one texture, no script (the coin's motion
     * is the WPStruct's) */
    {
        static Fighter coin;
        int pal_bank = 0;

        memset(&coin, 0, sizeof(coin));
        CHECK(fighter_load(&coin, "wpnyarscoin.mdl", &pal_bank) == 0);

        if (coin.hd != NULL)
        {
            CHECK(coin.hd->joint_count == 2);
            CHECK(coin.hd->vert_count == 4);
            CHECK(coin.hd->tri_count == 2);
            CHECK(coin.hd->batch_count == 1);
            CHECK(coin.hd->tex_count == 1);
            CHECK(coin.hd->pal_count == 0);
            CHECK(coin.hd->anim_count == 0);
            CHECK(coin.batches[0].tex >= 0);
        }
    }

    /* and the table entries the monsters land in. The table is indexed BY
     * KIND -- the monsters are kinds 32..44 of the SAME 45-entry table the
     * item switch reads, not a table of their own, which is why the
     * expression below is the kind and not an offset. The ones still NULL
     * are the monsters not yet ported, and a NULL there is an immediate
     * crash rather than a no-op -- itManagerMakeItemSetupCommon calls its
     * entry unguarded (correction) */
    CHECK(dITManagerProcMakeList[nITKindKabigon] == itKabigonMakeItem);
    CHECK(dITManagerProcMakeList[nITKindTosakinto] == itTosakintoMakeItem);
    CHECK(dITManagerProcMakeList[nITKindMLucky] == itMLuckyMakeItem);
    CHECK(dITManagerProcMakeList[nITKindSawamura] == itSawamuraMakeItem);
    CHECK(dITManagerProcMakeList[nITKindMew] == itMewMakeItem);

    CHECK(itemPackLoaded() == 1);
    CHECK(gITManagerCommonData != NULL);

    bank = itemPackBlock(ITEM_PACK_REGION_MODELS, "AnimBankStart Monster", NULL);
    CHECK(bank != NULL);

    for (i = 0; i < ITEM_ALLOC_MAX - 1; i++) pool[i].next = &pool[i + 1];
    pool[ITEM_ALLOC_MAX - 1].next = NULL;
    gITManagerStructsAllocFree = pool;

    /* the same parent fixture test_it_container uses: a parent ITEM hands
     * coll_data.p_translate straight to the collision runner, and
     * itManagerMakeItem reads its attributes */
    memset(&parent, 0, sizeof(parent));
    memset(&pdobj, 0, sizeof(pdobj));
    memset(&ptip, 0, sizeof(ptip));
    parent.obj = &pdobj;
    parent.user_data.p = &ptip;

    ptip.coll_data.p_translate = &pdobj.translate.vec.f;
    ptip.coll_data.p_map_coll = &ptip.coll_data.map_coll;
    ptip.attr = itemPackAttr(gITManagerCommonData, 0x50);
    CHECK(ptip.attr != NULL);

    tail_before = NULL;
    {
        GObj *g;

        for (g = gGCCommonLinks[nGCCommonLinkIDItem]; g != NULL; g = g->link_next)
        {
            tail_before = g;
        }
    }

    pos.x = 100.0F; pos.y = 200.0F; pos.z = 300.0F;
    vel.x = vel.y = vel.z = 0.0F;

    /* ---- Mew: the smallest monster, and the one with no anim of its
     * own past the rise ----
     *
     * COLLPROJECT with PARENT_GROUND on purpose. The spawner's own flags
     * are `ITEM_FLAG_COLLPROJECT | ITEM_FLAG_PARENT_ITEM`, and that parent
     * arm calls mpCommonRunItemCollisionDefault, which MOVES the item onto
     * the parent's collision inside itManagerMakeItem -- so an
     * absolute-position assertion over a real spawn would be a statement
     * about the collision runner rather than about Mew's own
     * `-= map_coll_bottom`. GROUND is one of the two arms that is a bare
     * `break`, and it is the honest one for a monster: nothing parents it,
     * it lands on the stage. (Bare COLLPROJECT is NOT that arm -- its
     * `flags & ITEM_MASK_PARENT` is 0, which is PARENT_FIGHTER, and the
     * call that follows reads a FTStruct out of this ITEM parent. That
     * mistake is what the first run of this test crashed on.) */
    {
        GObj *g = itMewMakeItem(&parent, &pos, &vel,
                                (ITEM_FLAG_COLLPROJECT | ITEM_FLAG_PARENT_GROUND));

        CHECK(g != NULL);

        if (g != NULL)
        {
            ITStruct *ip = itGetStruct(g);
            DObj *dobj = DObjGetStruct(g);

            CHECK(ip->kind == nITKindMew);

            /* the rise out of the ball, which is what the desc's own
             * proc_update/proc_map pair spends: ITMONSTER_RISE_STOP_WAIT
             * frames of levitation at ITMONSTER_RISE_VEL_Y */
            CHECK(ip->multi == ITMONSTER_RISE_STOP_WAIT);
            CHECK_NEAR(ip->physics.vel_air.y, ITMONSTER_RISE_VEL_Y, 1e-6F);
            CHECK(ip->physics.vel_air.x == 0.0F);
            CHECK(ip->physics.vel_air.z == 0.0F);

            CHECK(ip->attr == itemPackAttr(gITManagerCommonData,
                                           llITCommonDataMewItemAttributes));
            CHECK(ip->attr != NULL);

            /* the model came down by its own box height, so it stands on
             * the floor the ball opened on */
            CHECK_NEAR(dobj->translate.vec.f.y,
                       pos.y - ip->attr->map_coll_bottom, 1e-3F);

            /* ...and the two-sided offset check this test exists for */
            CHECK((uintptr_t)ip->attr->data ==
                  (uintptr_t)itemPackBlock(ITEM_PACK_REGION_MODELS,
                                           "DataStart Mew", NULL));
            CHECK((uintptr_t)itGetMonsterAnimNode(ip, llITCommonDataMewDataStart) ==
                  (uintptr_t)bank);
        }
    }

    /* ---- Goldeen: the first monster that ANIMATES, so its two offsets
     * are the first real use of `itGetPData` in it/ ---- */
    {
        GObj *g = itTosakintoMakeItem(&parent, &pos, &vel,
                                      (ITEM_FLAG_COLLPROJECT | ITEM_FLAG_PARENT_GROUND));

        CHECK(g != NULL);

        if (g != NULL)
        {
            ITStruct *ip = itGetStruct(g);
            DObj *dobj = DObjGetStruct(g);

            CHECK(ip->kind == nITKindTosakinto);
            CHECK(ip->multi == ITMONSTER_RISE_STOP_WAIT);
            CHECK_NEAR(dobj->translate.vec.f.y,
                       pos.y - ip->attr->map_coll_bottom, 1e-3F);

            /* itMainClearOwnerStats, which Goldeen's MakeItem calls and
             * Mew's does not -- a thrown monster must not carry its
             * thrower's stats */
            CHECK(ip->owner_gobj == NULL);
            CHECK(ip->team == ITEM_TEAM_DEFAULT);
            CHECK(ip->is_damage_all == TRUE);

            CHECK(ip->attr == itemPackAttr(gITManagerCommonData,
                                           llITCommonDataTosakintoItemAttributes));
            CHECK((uintptr_t)ip->attr->data ==
                  (uintptr_t)itemPackBlock(ITEM_PACK_REGION_MODELS,
                                           "DataStart Tosakinto", NULL));
            CHECK((uintptr_t)itGetMonsterAnimNode(ip, llITCommonDataTosakintoDataStart) ==
                  (uintptr_t)bank);

            /* the anim pair itTosakintoBounceInitVars hangs on the child
             * joint -- the AnimJoint and the MatAnimJoint, both named in
             * the pack's own block table */
            CHECK((uintptr_t)itGetPData(ip,
                                        llITCommonDataTosakintoDataStart,
                                        llITCommonDataTosakintoAnimJoint) ==
                  (uintptr_t)itemPackBlock(ITEM_PACK_REGION_MODELS,
                                           "AnimJoint Tosakinto", NULL));
            CHECK((uintptr_t)itGetPData(ip,
                                        llITCommonDataTosakintoDataStart,
                                        llITCommonDataTosakintoMatAnimJoint) ==
                  (uintptr_t)itemPackBlock(ITEM_PACK_REGION_MODELS,
                                           "MatAnimJoint Tosakinto", NULL));
        }
    }

    /* ---- Snorlax: the one whose model block is a bare AnimJoint, and the
     * one whose `interact_mask` is narrowed off ALL to FIGHTER ---- */
    {
        GObj *g = itKabigonMakeItem(&parent, &pos, &vel,
                                    (ITEM_FLAG_COLLPROJECT | ITEM_FLAG_PARENT_GROUND));

        CHECK(g != NULL);

        if (g != NULL)
        {
            ITStruct *ip = itGetStruct(g);
            DObj *dobj = DObjGetStruct(g);

            CHECK(ip->kind == nITKindKabigon);
            CHECK(ip->multi == ITMONSTER_RISE_STOP_WAIT);

            /* Snorlax is the one monster that does NOT come down by its
             * box height: itKabigonMakeItem never touches the DObj's
             * translate, so it stays exactly where itManagerMakeItem put
             * it. Asserted rather than skipped, because "one of the
             * thirteen differs here" is the kind of thing a later reader
             * would otherwise assume was a bug. (The first draft of this
             * test asserted the subtraction and read 200 against 785.) */
            CHECK_NEAR(dobj->translate.vec.f.y, pos.y, 1e-3F);

            /* Snorlax's own: itManagerMakeItem filled the mask with
             * GMHITCOLLISION_FLAG_ALL and MakeItem narrows it */
            CHECK(ip->attack_coll.interact_mask == GMHITCOLLISION_FLAG_FIGHTER);

            /* Kabigon has no `DataStart`: its `data` IS its anim joint
             * block, the way Meowth's is -- so the two names below are the
             * SAME address, and that is the point of asserting both */
            CHECK((uintptr_t)ip->attr->data ==
                  (uintptr_t)itemPackBlock(ITEM_PACK_REGION_MODELS,
                                           "AnimJoint Kabigon", NULL));
            CHECK((uintptr_t)itGetMonsterAnimNode(ip, llITCommonDataKabigonAnimJoint) ==
                  (uintptr_t)bank);
        }
    }

    /* ---- Chansey: the monster that spawns items, and the only one whose
     * anim hangs on the CHILD joint ---- */
    {
        GObj *g = itMLuckyMakeItem(&parent, &pos, &vel,
                                   (ITEM_FLAG_COLLPROJECT | ITEM_FLAG_PARENT_GROUND));

        CHECK(g != NULL);

        if (g != NULL)
        {
            ITStruct *ip = itGetStruct(g);
            DObj *dobj = DObjGetStruct(g);

            CHECK(ip->kind == nITKindMLucky);
            CHECK(ip->multi == ITMONSTER_RISE_STOP_WAIT);
            CHECK_NEAR(dobj->translate.vec.f.y,
                       pos.y - ip->attr->map_coll_bottom, 1e-3F);

            /* itMLuckyMakeItem gives the CHILD joint an XObj, so the tree
             * the bake built has to have one -- a 1-joint Chansey would
             * have crashed it */
            CHECK(dobj->child != NULL);

            CHECK((uintptr_t)ip->attr->data ==
                  (uintptr_t)itemPackBlock(ITEM_PACK_REGION_MODELS,
                                           "DataStart Lucky", NULL));
            CHECK((uintptr_t)itGetMonsterAnimNode(ip, llITCommonDataLuckyDataStart) ==
                  (uintptr_t)bank);
            CHECK((uintptr_t)itGetPData(ip,
                                        llITCommonDataLuckyDataStart,
                                        llITCommonDataLuckyAnimJoint) ==
                  (uintptr_t)itemPackBlock(ITEM_PACK_REGION_MODELS,
                                           "AnimJoint Lucky", NULL));

            /* and the egg state's own arithmetic, driven by hand: this is
             * the only proc_damage in the thirteen */
            ip->item_vars.mlucky.egg_spawn_wait = 10;
            CHECK(itMLuckyMakeEggProcDamage(g) == FALSE);
            CHECK(ip->item_vars.mlucky.egg_spawn_wait ==
                  10 + ITMLUCKY_EGG_SPAWN_WAIT_ADD);

            /* 3 eggs, then the disappear status. `disappear` turns the
             * hurtbox off and starts the lifetime -- driven to its TRUE */
            itMLuckyMakeEggInitVars(g);
            CHECK(ip->multi == ITMLUCKY_EGG_SPAWN_COUNT);
            CHECK(ip->item_vars.mlucky.egg_spawn_wait ==
                  ITMLUCKY_EGG_SPAWN_WAIT_CONST);
            CHECK(ip->damage_coll.hitstatus == nGMHitStatusNormal);

            itMLuckyDisappearSetStatus(g);
            CHECK(ip->damage_coll.hitstatus == nGMHitStatusNone);
            CHECK(ip->item_vars.mlucky.lifetime == ITMLUCKY_LIFETIME);

            {
                s32 guard = ITMLUCKY_LIFETIME + 4;
                sb32 last = TRUE;

                /* BOUNDED, because a fixture that stops the countdown must
                 * fail here rather than hang the suite. Note the shape:
                 * this update tests `lifetime` BEFORE it decrements, so the
                 * call that reaches zero returns FALSE and the NEXT one is
                 * the exit -- the opposite of the Barrel's roll, whose
                 * decrement comes first (test_it_taru_roll). */
                while ((ip->item_vars.mlucky.lifetime > 0) && (guard-- > 0))
                {
                    last = itMLuckyDisappearProcUpdate(g);
                }
                CHECK(guard > 0);
                CHECK(ip->item_vars.mlucky.lifetime == 0);
                CHECK(last == FALSE);
                CHECK(itMLuckyDisappearProcUpdate(g) == TRUE);
            }
        }
    }

    /* ---- Meowth: the first monster whose own file also carries a WEAPON
     * descriptor, and the first whose model block is an AnimJoint rather
     * than a DataStart ---- */
    {
        GObj *g = itNyarsMakeItem(&parent, &pos, &vel,
                                  (ITEM_FLAG_COLLPROJECT | ITEM_FLAG_PARENT_GROUND));

        CHECK(g != NULL);

        if (g != NULL)
        {
            ITStruct *ip = itGetStruct(g);
            DObj *dobj = DObjGetStruct(g);

            CHECK(ip->kind == nITKindNyars);
            CHECK(ip->multi == ITMONSTER_RISE_STOP_WAIT);
            CHECK_NEAR(dobj->translate.vec.f.y,
                       pos.y - ip->attr->map_coll_bottom, 1e-3F);

            /* Meowth has no `DataStart`: its data IS `AnimJoint Nyars`,
             * the way Snorlax's is `AnimJoint Kabigon` */
            CHECK((uintptr_t)ip->attr->data ==
                  (uintptr_t)itemPackBlock(ITEM_PACK_REGION_MODELS,
                                           "AnimJoint Nyars", NULL));
            CHECK((uintptr_t)itGetMonsterAnimNode(ip, llITCommonDataNyarsAnimJoint) ==
                  (uintptr_t)bank);

            /* the attack state's own schedule, driven by hand: the first
             * payout is at LIFETIME - SPAWN_WAIT/2, and each one pushes
             * the wait another SPAWN_WAIT out */
            itNyarsAttackInitVars(g);
            CHECK(ip->multi == ITNYARS_LIFETIME);
            CHECK(ip->item_vars.nyars.coin_spawn_wait ==
                  ITNYARS_LIFETIME - (ITNYARS_COIN_SPAWN_WAIT / 2));
            CHECK(ip->item_vars.nyars.coin_rotate_step == 0);
            CHECK(ip->item_vars.nyars.model_rotate_wait ==
                  ITNYARS_MODEL_ROTATE_WAIT);

            /* the model turns 180 degrees when its own wait runs out */
            {
                f32 rot = dobj->rotate.vec.f.y;

                ip->item_vars.nyars.model_rotate_wait = 0;
                CHECK(itNyarsAttackProcUpdate(g) == FALSE);
                CHECK_NEAR(dobj->rotate.vec.f.y,
                           rot + F_CST_DTOR32(180.0F), 1e-3F);
                /* the reset is MODEL_ROTATE_WAIT and the decrement that
                 * follows it in the same call takes it to one less -- so
                 * the turn really comes every MODEL_ROTATE_WAIT + 1
                 * frames, which is the decomp's own off-by-one and is
                 * kept rather than tidied */
                CHECK(ip->item_vars.nyars.model_rotate_wait ==
                      ITNYARS_MODEL_ROTATE_WAIT - 1);
            }
        }
    }

    /* ---- Koffing: the one whose attack state's proc_update calls into a
     * helper that is ALSO reachable, and whose second state is a plain
     * despawn countdown ---- */
    {
        GObj *g = itDogasMakeItem(&parent, &pos, &vel,
                                  (ITEM_FLAG_COLLPROJECT | ITEM_FLAG_PARENT_GROUND));

        CHECK(g != NULL);

        if (g != NULL)
        {
            ITStruct *ip = itGetStruct(g);
            DObj *dobj = DObjGetStruct(g);

            CHECK(ip->kind == nITKindDogas);
            CHECK(ip->multi == ITMONSTER_RISE_STOP_WAIT);
            CHECK_NEAR(dobj->translate.vec.f.y,
                       pos.y - ip->attr->map_coll_bottom, 1e-3F);

            CHECK((uintptr_t)ip->attr->data ==
                  (uintptr_t)itemPackBlock(ITEM_PACK_REGION_MODELS,
                                           "DataStart Dogas", NULL));
            CHECK((uintptr_t)itGetMonsterAnimNode(ip, llITCommonDataDogasDataStart) ==
                  (uintptr_t)bank);
            CHECK((uintptr_t)itGetPData(ip, llITCommonDataDogasDataStart,
                                        llITCommonDataDogasAnimJoint) ==
                  (uintptr_t)itemPackBlock(ITEM_PACK_REGION_MODELS,
                                           "AnimJoint Dogas", NULL));

            /* the cloud count, driven by hand. A zero `smog_spawn_wait`
             * means the FIRST cloud is due immediately -- and this is the
             * one place in the monster tests where the payout cannot be
             * driven, because it would call wpManagerMakeWeapon. So the
             * COUNT is what is checked, on the disappear status instead:
             * DESPAWN_WAIT in, TRUE out. */
            itDogasDisappearSetStatus(g);
            CHECK(ip->multi == ITDOGAS_DESPAWN_WAIT);

            {
                s32 guard = ITDOGAS_DESPAWN_WAIT + 4;
                sb32 last = TRUE;

                while ((ip->multi > 0) && (guard-- > 0))
                {
                    last = itDogasDisappearProcUpdate(g);
                }
                CHECK(guard > 0);
                CHECK(ip->multi == 0);
                CHECK(last == FALSE);          /* the test is before the decrement */
                CHECK(itDogasDisappearProcUpdate(g) == TRUE);
            }

            /* and the attack state's own init, which is the count and a
             * zero wait -- the cloud itself is not fired here */
            itDogasAttackInitVars(g);
            CHECK(ip->multi == ITDOGAS_SMOG_SPAWN_COUNT);
            CHECK(ip->item_vars.dogas.smog_spawn_wait == 0);
        }
    }

    /* ---- Onix: the biggest state machine of the thirteen, and the one
     * whose weapon count is driven by its OWN projectiles ---- */
    {
        GObj *g = itIwarkMakeItem(&parent, &pos, &vel,
                                  (ITEM_FLAG_COLLPROJECT | ITEM_FLAG_PARENT_GROUND));

        CHECK(g != NULL);

        if (g != NULL)
        {
            ITStruct *ip = itGetStruct(g);
            DObj *dobj = DObjGetStruct(g);

            CHECK(ip->kind == nITKindIwark);
            CHECK(ip->multi == ITMONSTER_RISE_STOP_WAIT);
            CHECK_NEAR(dobj->translate.vec.f.y,
                       pos.y - ip->attr->map_coll_bottom, 1e-3F);
            /* itMainClearOwnerStats first, and the mask narrowed to
             * FIGHTER, the same pair Snorlax's MakeItem has */
            CHECK(ip->owner_gobj == NULL);
            CHECK(ip->attack_coll.interact_mask == GMHITCOLLISION_FLAG_FIGHTER);

            CHECK((uintptr_t)ip->attr->data ==
                  (uintptr_t)itemPackBlock(ITEM_PACK_REGION_MODELS,
                                           "DataStart Wark", NULL));
            CHECK((uintptr_t)itGetMonsterAnimNode(ip, llITCommonDataWarkDataStart) ==
                  (uintptr_t)bank);

            /* the volley's own arithmetic, driven by hand. The count is
             * drawn from COUNT_MIN + rand(RANDOM), and `max` is a copy of
             * it so the end condition has both. */
            itIwarkAttackInitVars(g);
            CHECK(ip->item_vars.iwark.rock_spawn_remain >=
                  ITIWARK_ROCK_SPAWN_COUNT_MIN);
            CHECK(ip->item_vars.iwark.rock_spawn_remain <
                  ITIWARK_ROCK_SPAWN_COUNT_MIN + ITIWARK_ROCK_SPAWN_COUNT_RANDOM);
            CHECK(ip->item_vars.iwark.rock_spawn_max ==
                  ip->item_vars.iwark.rock_spawn_remain);
            CHECK(ip->item_vars.iwark.rock_spawn_count == 0);
            CHECK(ip->item_vars.iwark.rock_spawn_wait == 0);
            CHECK(ip->item_vars.iwark.rumble_frame == 0);
            CHECK(ip->item_vars.iwark.rumble_wait == 0);
            CHECK(ip->multi == 0);
            CHECK_NEAR(ip->physics.vel_air.y, ITIWARK_FLY_VEL_Y, 1e-6F);

            /* NOT driven further. `itIwarkAttackProcUpdate` is the one
             * monster update a host test must not call: above the stop
             * line it calls itIwarkAttackUpdateRock, which calls
             * wpManagerMakeWeapon for real. Its two decomp details are
             * recorded here instead -- the model turns on a counter that
             * goes UP (zeroed when it reaches MODEL_ROTATE_WAIT, so the
             * turn is every MODEL_ROTATE_WAIT + 1 frames, the opposite
             * direction from Meowth's), and the US build CLAMPS the
             * position to the stop line where the other arm only tests it. */
            CHECK(ip->multi == 0);   /* the rotate counter starts here */
        }
    }

    /* ---- Starmie: the one that hunts, and the one whose anim hangs on
     * a DObj's MObj rather than on the joint ---- */
    {
        GObj *g = itStarmieMakeItem(&parent, &pos, &vel,
                                    (ITEM_FLAG_COLLPROJECT | ITEM_FLAG_PARENT_GROUND));

        CHECK(g != NULL);

        if (g != NULL)
        {
            ITStruct *ip = itGetStruct(g);
            DObj *dobj = DObjGetStruct(g);

            CHECK(ip->kind == nITKindStarmie);
            CHECK(ip->multi == ITMONSTER_RISE_STOP_WAIT);
            CHECK_NEAR(dobj->translate.vec.f.y,
                       pos.y - ip->attr->map_coll_bottom, 1e-3F);

            CHECK((uintptr_t)ip->attr->data ==
                  (uintptr_t)itemPackBlock(ITEM_PACK_REGION_MODELS,
                                           "DataStart Starmie", NULL));
            CHECK((uintptr_t)itGetMonsterAnimNode(ip, llITCommonDataStarmieDataStart) ==
                  (uintptr_t)bank);
            /* the MatAnimJoint it hangs on the MObj comes out of the same
             * table -- and the MObj itself is real */
            CHECK((uintptr_t)itGetPData(ip, llITCommonDataStarmieDataStart,
                                        llITCommonDataStarmieMatAnimJoint) ==
                  (uintptr_t)itemPackBlock(ITEM_PACK_REGION_MODELS,
                                           "MatAnimJoint Starmie", NULL));
            CHECK(dobj->mobj != NULL);

            /* the attack state's own facing rule, driven by hand: it turns
             * 180 degrees only when the victim is on the OTHER side from
             * where it was already facing, and `add_vel_x` follows the new
             * `lr`. `itStarmieNFollowInitVars` is NOT called -- it walks
             * the fighter link and leaves `victim_gobj` uninitialised
             * exactly as the decomp does (see its own note). */
            ip->item_vars.starmie.victim_pos.x = dobj->translate.vec.f.x - 100.0F;
            ip->lr = -1;
            {
                f32 rot = dobj->rotate.vec.f.y;

                itStarmieAttackInitVars(g);
                CHECK(ip->lr == -1);                    /* same side: no turn */
                CHECK_NEAR(dobj->rotate.vec.f.y, rot, 1e-3F);
                CHECK_NEAR(ip->item_vars.starmie.add_vel_x,
                           -ITSTARMIE_ADD_VEL_X, 1e-6F);
                CHECK(ip->multi == ITSTARMIE_LIFETIME);
                CHECK(ip->item_vars.starmie.swift_spawn_wait == 0);

                ip->item_vars.starmie.victim_pos.x = dobj->translate.vec.f.x + 100.0F;
                itStarmieAttackInitVars(g);
                CHECK(ip->lr == +1);
                CHECK_NEAR(dobj->rotate.vec.f.y,
                           rot + F_CST_DTOR32(180.0F), 1e-3F);
                CHECK_NEAR(ip->item_vars.starmie.add_vel_x,
                           ITSTARMIE_ADD_VEL_X, 1e-6F);
            }
        }
    }

    /* ---- Charizard: the one that paces ---- */
    {
        GObj *g = itLizardonMakeItem(&parent, &pos, &vel,
                                     (ITEM_FLAG_COLLPROJECT | ITEM_FLAG_PARENT_GROUND));

        CHECK(g != NULL);

        if (g != NULL)
        {
            ITStruct *ip = itGetStruct(g);
            DObj *dobj = DObjGetStruct(g);

            CHECK(ip->kind == nITKindLizardon);
            CHECK(ip->multi == ITMONSTER_RISE_STOP_WAIT);
            CHECK_NEAR(dobj->translate.vec.f.y,
                       pos.y - ip->attr->map_coll_bottom, 1e-3F);
            /* NOT itMainClearOwnerStats -- Charizard's MakeItem is one of
             * the two that does not call it (Mew's is the other) */
            CHECK(ip->owner_gobj == NULL);   /* itManagerMakeItem zeroes it */

            CHECK((uintptr_t)ip->attr->data ==
                  (uintptr_t)itemPackBlock(ITEM_PACK_REGION_MODELS,
                                           "DataStart Lizardon", NULL));
            CHECK((uintptr_t)itGetMonsterAnimNode(ip, llITCommonDataLizardonDataStart) ==
                  (uintptr_t)bank);
            CHECK(dobj->mobj != NULL);

            /* the attack state's init, driven by hand: it faces LEFT,
             * arms the turn wait, and hangs both scripts. It does NOT
             * touch `multi` -- the lifetime came from the rise. */
            dobj->rotate.vec.f.y = 0.0F;
            itLizardonAttackInitVars(g);
            CHECK(ip->item_vars.lizardon.turn_wait == ITLIZARDON_TURN_WAIT);
            CHECK(ip->item_vars.lizardon.flame_spawn_wait == 0);
            CHECK(ip->lr == -1);
            CHECK((uintptr_t)itGetPData(ip, llITCommonDataLizardonDataStart,
                                        llITCommonDataLizardonAnimJoint) ==
                  (uintptr_t)itemPackBlock(ITEM_PACK_REGION_MODELS,
                                           "AnimJoint Lizardon", NULL));
            CHECK((uintptr_t)itGetPData(ip, llITCommonDataLizardonDataStart,
                                        llITCommonDataLizardonMatAnimJoint) ==
                  (uintptr_t)itemPackBlock(ITEM_PACK_REGION_MODELS,
                                           "MatAnimJoint Lizardon", NULL));

            /* the turn: every TURN_WAIT frames it flips `lr`, and only a
             * Pippi-summoned Charizard turns its model. `KIND` is Charizard
             * here, so the model does NOT rotate -- the opposite of what
             * the sibling assert would be; the flame is also not made,
             * because the wait is not zero, and driving that would call
             * wpManagerMakeWeapon for real. */
            {
                s32 n;

                /* TURN_WAIT + 1 calls, not TURN_WAIT: the reset to
                 * TURN_WAIT is followed by the same call's `turn_wait--`,
                 * so the counter starts at TURN_WAIT - 1 and the flip
                 * lands on the (TURN_WAIT + 1)th -- Meowth's off-by-one
                 * in the other direction */
                for (n = 0; n <= ITLIZARDON_TURN_WAIT; n++)
                {
                    ip->multi = 10;    /* keep the `multi == 0` exit away */
                    /* and keep the spawn wait off zero for the same reason
                     * `multi` is held: a zero wait MAKES a flame, which
                     * calls wpManagerMakeWeapon for real */
                    ip->item_vars.lizardon.flame_spawn_wait = 1000;

                    itLizardonAttackProcUpdate(g);
                }
                CHECK(ip->lr == +1);                        /* flipped */
                CHECK_NEAR(dobj->rotate.vec.f.y, 0.0F, 1e-3F);
            }
        }
    }

    /* ---- Beedrill: the one whose appear state ends on an animation
     * FRAME rather than a counter ---- */
    {
        GObj *g = itSpearMakeItem(&parent, &pos, &vel,
                                  (ITEM_FLAG_COLLPROJECT | ITEM_FLAG_PARENT_GROUND));

        CHECK(g != NULL);

        if (g != NULL)
        {
            ITStruct *ip = itGetStruct(g);
            DObj *dobj = DObjGetStruct(g);

            CHECK(ip->kind == nITKindSpear);
            CHECK(ip->multi == ITMONSTER_RISE_STOP_WAIT);
            CHECK_NEAR(dobj->translate.vec.f.y,
                       pos.y - ip->attr->map_coll_bottom, 1e-3F);
            CHECK(ip->owner_gobj == NULL);
            CHECK(ip->attack_coll.interact_mask == GMHITCOLLISION_FLAG_FIGHTER);

            /* the random facing: `lr` and the child's 180 are set
             * together, and this is the only monster OTHER than Pippi
             * that picks one */
            CHECK((ip->lr == +1) || (ip->lr == -1));

            CHECK((uintptr_t)ip->attr->data ==
                  (uintptr_t)itemPackBlock(ITEM_PACK_REGION_MODELS,
                                           "DataStart Spear", NULL));
            CHECK((uintptr_t)itGetMonsterAnimNode(ip, llITCommonDataSpearDataStart) ==
                  (uintptr_t)bank);
            CHECK(dobj->child != NULL);
            CHECK(dobj->child->mobj != NULL);

            /* the appear state's own two facts, driven by hand: it does
             * nothing until `anim_frame` reaches the call wait, and then
             * it NULLs the child's anim script and hands over to Fly --
             * which sets the count and captures the spawn height. */
            g->anim_frame = 0.0F;
            CHECK(itSpearAppearProcUpdate(g) == FALSE);
            /* still the DESC's own common update: the appear status is not
             * entered until itSpearCommonProcUpdate's counter runs out, and
             * this test drives the appear procs directly rather than
             * letting that happen */
            CHECK(ip->proc_update == itSpearCommonProcUpdate);

            g->anim_frame = (f32)ITSPEAR_SWARM_CALL_WAIT;
            itSpearAppearProcUpdate(g);
            CHECK(ip->proc_update == itSpearFlyProcUpdate);      /* handed over */
            CHECK(dobj->child->anim_joint.event32 == NULL);
            CHECK(ip->item_vars.spear.spear_spawn_count == ITSPEAR_SPAWN_COUNT);
            CHECK(ip->item_vars.spear.spear_spawn_wait == 0);
            CHECK_NEAR(ip->item_vars.spear.spear_spawn_pos_y,
                       dobj->translate.vec.f.y, 1e-3F);
            CHECK_NEAR(ip->physics.vel_air.y, ITSPEAR_SWARM_CALL_VEL_Y, 1e-6F);
        }
    }

    /* ---- Blastoise: the one whose MakeItem reads its PARENT, and the one
     * this test deliberately does NOT spawn.
     *
     * `itKamexMakeItem` calls `itKamexCommonFindTargetsSetLR`, the nearest-
     * enemy walk three of the thirteen share, and that walk leaves
     * `victim_gobj` uninitialised exactly as the decomp does -- safe in a
     * real battle, where a Poké Ball implies a thrower and therefore a
     * fighter on the link, and not safe here, where the link is empty and
     * `DObjGetStruct(victim_gobj)` would read a stale stack slot. Every
     * OTHER monster's walk is only reached from an ATTACK state, which
     * this test never enters; Blastoise's is in MakeItem, so the spawn
     * cannot be driven. The descriptors are asserted; the spawn is the
     * disc probe's job (a battle always has fighters).
     *
     * What it would have shown: `owner_gobj` and (US) `team` read off the
     * PARENT's ITStruct -- the Poké Ball, still alive one frame before the
     * monster exists -- and the 341-unit collision box the FALSE arm of
     * itKamexAttackInitVars installs. */
    CHECK(dITKamexItemDesc.proc_update == itKamexCommonProcUpdate);
    CHECK(dITKamexItemDesc.proc_map == itKamexCommonProcMap);

    /* ---- Clefairy: spawned, but the METRONOME is not rolled ----
     *
     * `itPippiCommonSelectMonster` picks one of the twelve at random and
     * calls its SetStatus, and two of those twelve cannot run here:
     * Starmie's walks the fighter link (and would dereference an
     * uninitialised victim with no fighters), and Hitmonlee's and
     * Koffing's reach into effect and animation state a bare GObj does
     * not have. A roll is a coin toss this test cannot take. What is
     * asserted instead is the table itself, above -- which is the binding
     * and therefore the part that can be wrong silently. */
    {
        GObj *g = itPippiMakeItem(&parent, &pos, &vel,
                                  (ITEM_FLAG_COLLPROJECT | ITEM_FLAG_PARENT_GROUND));

        CHECK(g != NULL);

        if (g != NULL)
        {
            ITStruct *ip = itGetStruct(g);
            DObj *dobj = DObjGetStruct(g);

            CHECK(ip->kind == nITKindPippi);
            CHECK(ip->multi == ITMONSTER_RISE_STOP_WAIT);
            CHECK_NEAR(dobj->translate.vec.f.y,
                       pos.y - ip->attr->map_coll_bottom, 1e-3F);

            /* and the display proc its MakeItem installs is the ZB one,
             * which the metronome REPLACES with the XLU one for the two
             * it picks that way */
            CHECK(g->proc_display == itPippiCommonProcDisplay);
        }
    }

    /* ---- the Poké Ball, and the gateway it is ----
     *
     * The last of the twenty kinds the item switch can pick, and the only
     * one whose job is to hand the match a MONSTER. Everything it needs
     * arrived in the thirteen steps before it, so this is the test that
     * says the chain is closed: a real Poké Ball, opened, puts a real
     * monster on the item link -- itMainMakeMonster ->
     * itManagerMakeItemKind -> dITManagerProcMakeList ->
     * itIwarkMakeItem -> itManagerMakeItem -> the pack's attributes and
     * the baked model.
     *
     * THE PICK IS MADE DETERMINISTIC rather than left to chance:
     * `gITManagerMonsterData.monsters_num` is set to 1, so the roll
     * `monster_id[syUtilsRandIntRange(1)]` is index 0, and the pool is
     * rebuilt from kinds 32..43 with the last-two exclusion empty, which
     * makes index 0 Onix. Onix matters because it is one of the monsters
     * whose MakeItem touches no fighter -- Kamex's and Starmie's walks
     * would dereference an uninitialised victim with the link empty. */
    {
        GObj *ball = itMBallMakeItem(&parent, &pos, &vel,
                                     (ITEM_FLAG_COLLPROJECT | ITEM_FLAG_PARENT_GROUND));

        CHECK(ball != NULL);

        if (ball != NULL)
        {
            ITStruct *ip = itGetStruct(ball);
            GObj *tail_before2;

            CHECK(ip->kind == nITKindMBall);
            CHECK(ip->multi == ITMBALL_SPAWN_WAIT);
            CHECK(ip->item_vars.mball.is_rebound == FALSE);
            CHECK(ip->is_unused_item_bool == TRUE);
            /* the lid is the visible half at spawn */
            CHECK((DObjGetStruct(ball)->child->flags & DOBJ_FLAG_HIDDEN) != 0);
            CHECK((DObjGetStruct(ball)->child->sib_next->flags & DOBJ_FLAG_HIDDEN) == 0);

            /* mark the link's tail before the monster is made */
            tail_before2 = NULL;
            {
                GObj *g;

                for (g = gGCCommonLinks[nGCCommonLinkIDItem]; g != NULL; g = g->link_next)
                {
                    tail_before2 = g;
                }
            }

            /* open it: the forced-kind hook stays at zero, which is the
             * arm that goes through itMainMakeMonster */
            ip->multi = 0;

            {
                u8 kinds[1];
                s32 saved_num = gITManagerMonsterData.monsters_num;
                s32 saved_curr = gITManagerMonsterData.monster_curr;
                s32 saved_prev = gITManagerMonsterData.monster_prev;

                (void)kinds;

                gITManagerMonsterData.monsters_num = 1;
                gITManagerMonsterData.monster_curr = 0;
                gITManagerMonsterData.monster_prev = 0;

                CHECK(dITManagerForceMonsterKind == 0);
                CHECK(itMBallOpenProcUpdate(ball) == TRUE);

                /* a monster came out, it is Onix, and it took the ball's
                 * owner stats -- the ball's own are what itManagerMakeItem
                 * reset, so the pair matching is the test */
                {
                    GObj *g;
                    s32 monsters = 0;

                    for (g = (tail_before2 != NULL) ? tail_before2->link_next
                                                    : gGCCommonLinks[nGCCommonLinkIDItem];
                         g != NULL; g = g->link_next)
                    {
                        ITStruct *mip = itGetStruct(g);

                        if (mip->kind == nITKindIwark)
                        {
                            monsters++;
                            CHECK(mip->owner_gobj == ip->owner_gobj);
                            CHECK(mip->team == ip->team);
                            CHECK(mip->player_num == ip->player_num);
                        }
                    }
                    CHECK(monsters == 1);
                }

                /* and the pool bookkeeping moved: the last-two window is
                 * the pick's own, and monsters_num only shrinks when it is
                 * not already 10 */
                CHECK(gITManagerMonsterData.monster_curr == nITKindIwark);
                CHECK(gITManagerMonsterData.monster_prev == 0);

                gITManagerMonsterData.monsters_num = saved_num;
                gITManagerMonsterData.monster_curr = saved_curr;
                gITManagerMonsterData.monster_prev = saved_prev;
            }
        }
    }

    /* ---- the item SPAWNER's two gates ----------------
     *
     * `itManagerAppearActorProcUpdate` is what makes an item appear with
     * neither a Container nor a stage asking for one -- the function the
     * switch's sixteen toggles actually control. Two of its four gates are
     * testable without a live stage and are checked here: the battle must
     * not be in `nSCBattleGameStatusWait`, and the appearance timer must
     * have run out. The other two (the ITStruct pool and the stage's item
     * map objects) need the scene, and the disc probe is what covers
     * them.
     *
     * `itManagerMakeAppearActor` itself is NOT called: it reads
     * `gMPCollisionGroundData->item_weights` and then asks the collision
     * system for the stage's `nMPMapObjKindItem` positions, neither of
     * which this test has a stage for. What it builds -- the weights pair
     * `itMainGetWeightedItemKind` samples -- is the same shape
     * test_it_container already drives. */
    {
        s32 saved_wait = gITManagerAppearActor.spawn_wait;
        s32 saved_status = gSCManagerBattleState->game_status;

        /* the pre-match hold: nothing counts down at all */
        gSCManagerBattleState->game_status = nSCBattleGameStatusWait;
        gITManagerAppearActor.spawn_wait = 5;

        itManagerAppearActorProcUpdate(NULL);

        CHECK(gITManagerAppearActor.spawn_wait == 5);

        /* out of the hold: it counts down, and does not spawn yet */
        gSCManagerBattleState->game_status = nSCBattleGameStatusGo;
        itManagerAppearActorProcUpdate(NULL);
        CHECK(gITManagerAppearActor.spawn_wait == 4);

        gITManagerAppearActor.spawn_wait = saved_wait;
        gSCManagerBattleState->game_status = saved_status;
    }

    /* Drain what this test spawned -- only the chain after tail_before,
     * so the earlier item tests' own leftovers stay where they left them,
     * and each GObj's arrow with it (the same teardown test_it_container
     * needs) */
    {
        GObj *g = (tail_before != NULL) ? tail_before->link_next
                                        : gGCCommonLinks[nGCCommonLinkIDItem];

        while (g != NULL)
        {
            GObj *next = g->link_next;
            GObj *arrow = itGetStruct(g)->arrow_gobj;

            if (arrow != NULL)
            {
                gcEjectGObj(arrow);
            }
            gcEjectGObj(g);
            g = next;
        }
    }

    gITManagerStructsAllocFree = save_free;
}

static sb32 test_it_status_proc(GObj *g) { (void)g; return TRUE; }

/* src/dc/itmain.c: the twenty self-contained ITStruct helpers.
 * Every function takes a GObj* and reaches its ITStruct through
 * itGetStruct (user_data.p), the same stack-GObj shape test_wp_main uses
 * for wpGetStruct -- no live scene needed for any of these. */
static void test_it_main(void)
{
    GObj titem, towner;
    ITStruct tip;
    ITAttributes tattr;
    FTStruct tfp;
    ITStatusDesc statuses[1];
    ITAttackEvent events[4];
    ITRandomWeights weights;
    u8 wkinds[4] = { 5, 6, 7, 8 };
    u16 wblocks[5] = { 0, 10, 30, 60, 60 };
    GMColAnim ca1, ca2;
    s32 i;

    titem.user_data.p = &tip;
    tip.attr = &tattr;

    /* itMainSetCommonSpin: F_PCT_TO_DEC(spin_speed) * ITEM_SPIN_SPEED_COMMON,
     * negated when lr == -1; zero when spin_speed is zero regardless of lr */
    tattr.spin_speed = 50; tip.lr = 1;
    itMainSetCommonSpin(&titem);
    CHECK_NEAR(tip.spin_step, F_PCT_TO_DEC(50) * ITEM_SPIN_SPEED_COMMON, 1e-6F);
    tip.lr = -1;
    itMainSetCommonSpin(&titem);
    CHECK_NEAR(tip.spin_step, -(F_PCT_TO_DEC(50) * ITEM_SPIN_SPEED_COMMON), 1e-6F);
    tattr.spin_speed = 0;
    itMainSetCommonSpin(&titem);
    CHECK_EQF(tip.spin_step, 0.0F);

    /* itMainSetAppearSpin: slow(0)/fast(1) pick a different constant;
     * spin_speed 0 is always 0 regardless of which */
    tattr.spin_speed = 40;
    itMainSetAppearSpin(&titem, 0);
    CHECK_NEAR(tip.spin_step, F_PCT_TO_DEC(40) * ITEM_SPIN_SPEED_APPEAR_SLOW, 1e-6F);
    itMainSetAppearSpin(&titem, 1);
    CHECK_NEAR(tip.spin_step, F_PCT_TO_DEC(40) * ITEM_SPIN_SPEED_APPEAR_FAST, 1e-6F);
    tattr.spin_speed = 0;
    itMainSetAppearSpin(&titem, 0);
    CHECK_EQF(tip.spin_step, 0.0F);
    itMainSetAppearSpin(&titem, 1);
    CHECK_EQF(tip.spin_step, 0.0F);

    /* itMainSetThrownSpin: smash/normal constant, negated when vel.x < 0,
     * then scaled by F_PCT_TO_DEC(spin_speed) (spin_speed 0 -> 0) */
    tattr.spin_speed = 100;
    {
        Vec3f vel = { 1.0F, 0.0F, 0.0F };
        itMainSetThrownSpin(&titem, &vel, TRUE);
        CHECK_NEAR(tip.spin_step, F_PCT_TO_DEC(100) * ITEM_SPIN_SPEED_SMASH_THROW, 1e-6F);
        vel.x = -1.0F;
        itMainSetThrownSpin(&titem, &vel, TRUE);
        CHECK_NEAR(tip.spin_step, -(F_PCT_TO_DEC(100) * ITEM_SPIN_SPEED_SMASH_THROW), 1e-6F);
        vel.x = 1.0F;
        itMainSetThrownSpin(&titem, &vel, FALSE);
        CHECK_NEAR(tip.spin_step, F_PCT_TO_DEC(100) * ITEM_SPIN_SPEED_NORMAL_THROW, 1e-6F);
        tattr.spin_speed = 0;
        itMainSetThrownSpin(&titem, &vel, TRUE);
        CHECK_EQF(tip.spin_step, 0.0F);
        tattr.spin_speed = 50;
    }

    /* itMainSetSpinVelLR: lr from vel_air.x's sign, then the common spin */
    tip.physics.vel_air.x = 3.0F;
    itMainSetSpinVelLR(&titem);
    CHECK(tip.lr == 1);
    CHECK_NEAR(tip.spin_step, F_PCT_TO_DEC(50) * ITEM_SPIN_SPEED_COMMON, 1e-6F);
    tip.physics.vel_air.x = -3.0F;
    itMainSetSpinVelLR(&titem);
    CHECK(tip.lr == -1);
    tip.physics.vel_air.x = 0.0F;
    itMainSetSpinVelLR(&titem);
    CHECK(tip.lr == 1); /* >= 0.0F takes the +1 arm */

    /* itMainApplyGravityClampTVel: same shape test_wp_main already proved
     * for the weapon twin -- gravity subtracted from y, then the 2D speed
     * clamped to terminal velocity once it exceeds it */
    tip.physics.vel_air.x = 0.0F; tip.physics.vel_air.y = 0.0F; tip.physics.vel_air.z = 5.0F;
    itMainApplyGravityClampTVel(&tip, 1.0F, 3.0F);
    CHECK_EQF(tip.physics.vel_air.y, -1.0F);
    itMainApplyGravityClampTVel(&tip, 1.0F, 3.0F);
    CHECK_EQF(tip.physics.vel_air.y, -2.0F);
    itMainApplyGravityClampTVel(&tip, 1.0F, 3.0F);
    CHECK_EQF(tip.physics.vel_air.y, -3.0F);
    itMainApplyGravityClampTVel(&tip, 1.0F, 3.0F);
    CHECK_NEAR(tip.physics.vel_air.y, -3.0F, 1e-5F);
    CHECK_EQF(tip.physics.vel_air.z, 5.0F);

    /* itMainResetPlayerVars: every field to its ITEM_*_DEFAULT, display_mode
     * from the live gITManagerDisplayMode */
    tip.owner_gobj = &towner; tip.team = 1; tip.player = 2; tip.handicap = 3;
    tip.player_num = 4; tip.attack_coll.throw_mul = 0.5F;
    gITManagerDisplayMode = 7;
    itMainResetPlayerVars(&titem);
    CHECK(tip.owner_gobj == NULL);
    CHECK(tip.team == ITEM_TEAM_DEFAULT);
    CHECK(tip.player == ITEM_PORT_DEFAULT);
    CHECK(tip.handicap == ITEM_HANDICAP_DEFAULT);
    CHECK(tip.player_num == 0);
    CHECK_EQF(tip.attack_coll.throw_mul, ITEM_THROW_DEFAULT);
    CHECK(tip.display_mode == 7);

    /* itMainClearAttackRecord: every record's gobj/hurt/shield/reflect/
     * rehit-timer cleared and group_id set to 7 -- is_interact_absorb is
     * NOT one of the fields the decomp touches, unlike wpMainClearAttack-
     * Record's WPStruct twin, so it is proven UNCHANGED here rather than
     * cleared */
    for (i = 0; i < ARRAY_COUNT(tip.attack_coll.attack_records); i++)
    {
        tip.attack_coll.attack_records[i].victim_gobj = (GObj *)0x1;
        tip.attack_coll.attack_records[i].victim_flags.is_interact_hurt = TRUE;
        tip.attack_coll.attack_records[i].victim_flags.is_interact_shield = TRUE;
        tip.attack_coll.attack_records[i].victim_flags.is_interact_reflect = TRUE;
        tip.attack_coll.attack_records[i].victim_flags.is_interact_absorb = TRUE;
        tip.attack_coll.attack_records[i].victim_flags.timer_rehit = 9;
        tip.attack_coll.attack_records[i].victim_flags.group_id = 0;
    }
    itMainClearAttackRecord(&tip);
    for (i = 0; i < ARRAY_COUNT(tip.attack_coll.attack_records); i++)
    {
        CHECK(tip.attack_coll.attack_records[i].victim_gobj == NULL);
        CHECK(tip.attack_coll.attack_records[i].victim_flags.is_interact_hurt == FALSE);
        CHECK(tip.attack_coll.attack_records[i].victim_flags.is_interact_shield == FALSE);
        CHECK(tip.attack_coll.attack_records[i].victim_flags.is_interact_reflect == FALSE);
        CHECK(tip.attack_coll.attack_records[i].victim_flags.is_interact_absorb == TRUE);
        CHECK(tip.attack_coll.attack_records[i].victim_flags.timer_rehit == 0);
        CHECK(tip.attack_coll.attack_records[i].victim_flags.group_id == 7);
    }

    /* itMainClearOwnerStats: damage-all raised, owner cleared, team default */
    tip.is_damage_all = FALSE; tip.owner_gobj = &towner; tip.team = 2;
    itMainClearOwnerStats(&titem);
    CHECK(tip.is_damage_all == TRUE);
    CHECK(tip.owner_gobj == NULL);
    CHECK(tip.team == ITEM_TEAM_DEFAULT);

    /* itMainCopyDamageStats: owner/team/player/handicap/display_mode all
     * come from the damage_* shadow fields; player_num is the decomp's own
     * self-assignment (`ip->player_num = ip->player_num`) -- unchanged */
    tip.damage_gobj = &towner; tip.damage_team = 3; tip.damage_port = 1;
    tip.damage_handicap = 5; tip.damage_display_mode = 2;
    tip.player_num = 9;
    itMainCopyDamageStats(&titem);
    CHECK(tip.owner_gobj == &towner);
    CHECK(tip.team == 3);
    CHECK(tip.player == 1);
    CHECK(tip.player_num == 9);
    CHECK(tip.handicap == 5);
    CHECK(tip.display_mode == 2);

    /* itMainGetDamageOutput: thrown adds |vel_air|*0.1 before throw_mul;
     * not thrown is just damage*stale, both rounded up by +0.999 */
    tip.is_thrown = FALSE;
    tip.attack_coll.damage = 12; tip.attack_coll.stale = 1.0F;
    CHECK(itMainGetDamageOutput(&tip) == 12);
    tip.is_thrown = TRUE;
    tip.physics.vel_air.x = 30.0F; tip.physics.vel_air.y = 0.0F; tip.physics.vel_air.z = 40.0F; /* mag 50 */
    tip.attack_coll.damage = 10; tip.attack_coll.throw_mul = 2.0F; tip.attack_coll.stale = 1.0F;
    CHECK(itMainGetDamageOutput(&tip) == 30); /* (10+5)*2 = 30 */

    /* itMainCheckShootNoAmmo: only the three ammo-limited kinds, and only
     * once multi (rounds left) hits 0 */
    tip.kind = nITKindStarRod; tip.multi = 0;
    CHECK(itMainCheckShootNoAmmo(&titem) == TRUE);
    tip.multi = 1;
    CHECK(itMainCheckShootNoAmmo(&titem) == FALSE);
    tip.kind = nITKindLGun; tip.multi = 0;
    CHECK(itMainCheckShootNoAmmo(&titem) == TRUE);
    tip.kind = nITKindFFlower; tip.multi = 0;
    CHECK(itMainCheckShootNoAmmo(&titem) == TRUE);
    tip.kind = nITKindHammer; tip.multi = 0;
    CHECK(itMainCheckShootNoAmmo(&titem) == FALSE);

    /* itMainSetStatus: every proc pointer copied off the row, is_thrown and
     * the stat flags cleared, stat_count from the live ftParamGetStat-
     * UpdateCount() */
    statuses[0].proc_update = test_it_status_proc;
    statuses[0].proc_map = test_it_status_proc;
    statuses[0].proc_hit = test_it_status_proc;
    statuses[0].proc_shield = test_it_status_proc;
    statuses[0].proc_hop = test_it_status_proc;
    statuses[0].proc_setoff = test_it_status_proc;
    statuses[0].proc_reflector = test_it_status_proc;
    statuses[0].proc_damage = test_it_status_proc;
    tip.is_thrown = TRUE;
    tip.attack_coll.stat_flags.attack_id = 1;
    tip.attack_coll.stat_flags.is_smash_attack = tip.attack_coll.stat_flags.ga = tip.attack_coll.stat_flags.is_projectile = TRUE;
    {
        /* ftParamGetStatUpdateCount is a post-increment counter, not a
         * pure query -- capture the value itMainSetStatus's own single
         * call will consume rather than calling it again after */
        u16 expect_count = gFTManagerStatUpdateCount;
        itMainSetStatus(&titem, statuses, 0);
        CHECK(tip.attack_coll.stat_count == expect_count);
    }
    CHECK(tip.proc_update == test_it_status_proc);
    CHECK(tip.proc_map == test_it_status_proc);
    CHECK(tip.proc_hit == test_it_status_proc);
    CHECK(tip.proc_shield == test_it_status_proc);
    CHECK(tip.proc_hop == test_it_status_proc);
    CHECK(tip.proc_setoff == test_it_status_proc);
    CHECK(tip.proc_reflector == test_it_status_proc);
    CHECK(tip.proc_damage == test_it_status_proc);
    CHECK(tip.is_thrown == FALSE);
    CHECK(tip.attack_coll.stat_flags.attack_id == nFTStatusAttackIDNull);
    CHECK(tip.attack_coll.stat_flags.is_smash_attack == FALSE);
    CHECK(tip.attack_coll.stat_flags.ga == FALSE);
    CHECK(tip.attack_coll.stat_flags.is_projectile == FALSE);

    /* itMainCheckSetColAnimID/ClearColAnim: thin pass-throughs over
     * ip->colanim -- proven by running the same call in parallel directly
     * against ftParamCheckSetColAnimID/ResetColAnim on an identical
     * GMColAnim and comparing */
    memset(&ca1, 0, sizeof(ca1));
    memset(&ca2, 0, sizeof(ca2));
    tip.colanim = ca1;
    {
        sb32 r1 = itMainCheckSetColAnimID(&titem, 1, 30);
        sb32 r2 = ftParamCheckSetColAnimID(&ca2, 1, 30);
        CHECK(r1 == r2);
        CHECK(memcmp(&tip.colanim, &ca2, sizeof(GMColAnim)) == 0);
    }
    itMainClearColAnim(&titem);
    ftParamResetColAnim(&ca2);
    CHECK(memcmp(&tip.colanim, &ca2, sizeof(GMColAnim)) == 0);

    /* itMainVelSetRebound: x *= -0.06, y = y*-0.3 + 25 */
    tip.physics.vel_air.x = 10.0F; tip.physics.vel_air.y = 20.0F;
    itMainVelSetRebound(&titem);
    CHECK_NEAR(tip.physics.vel_air.x, -0.6F, 1e-5F);
    CHECK_NEAR(tip.physics.vel_air.y, 19.0F, 1e-5F);

    /* itMainSearchRandomWeight/GetWeightedItemKind: a 4-entry weight table
     * (kinds 5/6/7/8, blocks 0/10/30/60/60, weights_sum 60, valids_num 4)
     * -- a random draw in [blocks[i], blocks[i+1]) picks kind i */
    weights.kinds = wkinds;
    weights.blocks = wblocks;
    weights.weights_sum = 60;
    weights.valids_num = 4;
    CHECK(itMainSearchRandomWeight(0, &weights, 0, 4) == 0);
    CHECK(itMainSearchRandomWeight(9, &weights, 0, 4) == 0);
    CHECK(itMainSearchRandomWeight(10, &weights, 0, 4) == 1);
    CHECK(itMainSearchRandomWeight(29, &weights, 0, 4) == 1);
    CHECK(itMainSearchRandomWeight(30, &weights, 0, 4) == 2);
    CHECK(itMainSearchRandomWeight(59, &weights, 0, 4) == 2);
    for (i = 0; i < 20; i++)
    {
        s32 kind = itMainGetWeightedItemKind(&weights);
        CHECK((kind >= 5) && (kind <= 8));
    }

    /* itMainUpdateAttackEvent: copies angle/damage/size from events[event_id]
     * when multi matches its timer, then advances event_id, wrapping 4->3 */
    events[0].timer = 10; events[0].angle = 1; events[0].damage = 1; events[0].size = 10;
    events[1].timer = 20; events[1].angle = 2; events[1].damage = 2; events[1].size = 20;
    events[2].timer = 30; events[2].angle = 3; events[2].damage = 3; events[2].size = 30;
    events[3].timer = 40; events[3].angle = 4; events[3].damage = 4; events[3].size = 40;
    tip.event_id = 0; tip.multi = 5; /* no match -> untouched */
    itMainUpdateAttackEvent(&titem, events);
    CHECK(tip.event_id == 0);
    tip.multi = 10; /* matches events[0].timer */
    itMainUpdateAttackEvent(&titem, events);
    CHECK(tip.attack_coll.angle == 1);
    CHECK(tip.attack_coll.damage == 1);
    CHECK(tip.attack_coll.size == 10);
    CHECK(tip.event_id == 1);
    tip.multi = 20;
    itMainUpdateAttackEvent(&titem, events);
    CHECK(tip.event_id == 2);
    tip.multi = 30;
    itMainUpdateAttackEvent(&titem, events);
    CHECK(tip.event_id == 3);
    tip.multi = 40;
    itMainUpdateAttackEvent(&titem, events); /* event_id would go to 4 -> wraps to 3 */
    CHECK(tip.event_id == 3);

    /* itMainCommonProcHop: rotates vel_air about shield_collide_dir by
     * 2*shield_collide_angle (checked against a direct syVectorRotate-
     * About3D call), then re-derives lr/spin from the result; always FALSE */
    {
        Vec3f v1 = { 1.0F, 0.0F, 0.0F }, v2 = { 1.0F, 0.0F, 0.0F };
        Vec3f dir = { 0.0F, 1.0F, 0.0F };
        tip.physics.vel_air = v1;
        tip.shield_collide_dir = dir;
        tip.shield_collide_angle = 0.5F;
        tattr.spin_speed = 50;
        CHECK(itMainCommonProcHop(&titem) == FALSE);
        syVectorRotateAbout3D(&v2, &dir, 1.0F);
        CHECK_NEAR(tip.physics.vel_air.x, v2.x, 1e-5F);
        CHECK_NEAR(tip.physics.vel_air.y, v2.y, 1e-5F);
        CHECK_NEAR(tip.physics.vel_air.z, v2.z, 1e-5F);
        CHECK(tip.lr == ((tip.physics.vel_air.x >= 0.0F) ? 1 : -1));
    }

    /* itMainCommonProcReflector: flips vel_air.x only when it opposes the
     * owner fighter's facing; always FALSE */
    tip.owner_gobj = &towner;
    towner.user_data.p = &tfp;
    tfp.lr = 1;
    tip.physics.vel_air.x = -5.0F;
    CHECK(itMainCommonProcReflector(&titem) == FALSE);
    CHECK_EQF(tip.physics.vel_air.x, 5.0F);
    CHECK(itMainCommonProcReflector(&titem) == FALSE);
    CHECK_EQF(tip.physics.vel_air.x, 5.0F); /* 5*1 >= 0 -> unchanged */
    tfp.lr = -1;
    CHECK(itMainCommonProcReflector(&titem) == FALSE);
    CHECK_EQF(tip.physics.vel_air.x, -5.0F);
}

/* src/dc/itprocess.c + itvisuals.c: the item's hit-detection bookkeeping
 * and the two top-level per-frame dispatchers. gm/gmcollision.c (the
 * geometry family these lean on) already compiles unmodified into this
 * binary via HOST_DECOMP_OBJS -- see itprocess.c's header comment for
 * why the whole file, not just a slice, is ported. */
static sb32 test_it_process_proc_true(GObj *g) { (void)g; return TRUE; }
static sb32 test_it_process_proc_false(GObj *g) { (void)g; return FALSE; }

static void test_it_process(void)
{
    GObj titem, towner;
    ITStruct tip;
    FTStruct tfp;
    DObj tdobj;
    s32 i;

    memset(&tip, 0, sizeof(tip));
    memset(&tdobj, 0, sizeof(tdobj));
    titem.user_data.p = &tip;
    titem.obj = &tdobj;
    towner.user_data.p = &tfp;

    /* itProcessUpdateAttackPositions: Off is a no-op; New seeds pos_curr
     * from offset+translate and advances to Transfer; the next call
     * (still Transfer) copies pos_curr straight to pos_prev then falls
     * into Interpolate's own refresh (same tic, per the decomp's missing
     * break/fallthrough) so the position is set immediately, not staged
     * a tic late; a further Interpolate call keeps refreshing pos_prev/
     * pos_curr each time. */
    {
        DObj *dobj = DObjGetStruct(&titem);
        dobj->translate.vec.f.x = 100.0F; dobj->translate.vec.f.y = 200.0F; dobj->translate.vec.f.z = 300.0F;
        tip.attack_coll.attack_count = 1;
        tip.attack_coll.offsets[0].x = 1.0F; tip.attack_coll.offsets[0].y = 2.0F; tip.attack_coll.offsets[0].z = 3.0F;

        tip.attack_coll.attack_state = nGMAttackStateOff;
        tip.attack_coll.attack_pos[0].pos_curr.x = -999.0F;
        itProcessUpdateAttackPositions(&titem);
        CHECK_EQF(tip.attack_coll.attack_pos[0].pos_curr.x, -999.0F); /* untouched */

        tip.attack_coll.attack_state = nGMAttackStateNew;
        tip.attack_coll.attack_pos[0].unk_ithitpos_0x18 = TRUE;
        tip.attack_coll.attack_pos[0].unk_ithitpos_0x5C = 5;
        itProcessUpdateAttackPositions(&titem);
        CHECK(tip.attack_coll.attack_state == nGMAttackStateTransfer);
        CHECK_EQF(tip.attack_coll.attack_pos[0].pos_curr.x, 101.0F);
        CHECK_EQF(tip.attack_coll.attack_pos[0].pos_curr.y, 202.0F);
        CHECK_EQF(tip.attack_coll.attack_pos[0].pos_curr.z, 303.0F);
        CHECK(tip.attack_coll.attack_pos[0].unk_ithitpos_0x18 == FALSE);
        CHECK(tip.attack_coll.attack_pos[0].unk_ithitpos_0x5C == 0);

        dobj->translate.vec.f.x = 110.0F;
        itProcessUpdateAttackPositions(&titem); /* Transfer -> falls into Interpolate this same tic */
        CHECK(tip.attack_coll.attack_state == nGMAttackStateInterpolate);
        CHECK_EQF(tip.attack_coll.attack_pos[0].pos_prev.x, 101.0F); /* old pos_curr */
        CHECK_EQF(tip.attack_coll.attack_pos[0].pos_curr.x, 111.0F); /* new offset+translate */

        dobj->translate.vec.f.x = 120.0F;
        itProcessUpdateAttackPositions(&titem); /* Interpolate stays Interpolate */
        CHECK(tip.attack_coll.attack_state == nGMAttackStateInterpolate);
        CHECK_EQF(tip.attack_coll.attack_pos[0].pos_prev.x, 111.0F);
        CHECK_EQF(tip.attack_coll.attack_pos[0].pos_curr.x, 121.0F);
    }

    /* itProcessUpdateAttackRecords: Off skips the whole sweep; otherwise
     * a positive timer_rehit counts down, and hitting zero clears the
     * record (gobj NULL, the three interact flags FALSE, group_id 7) --
     * a record with no timer (0) is left alone either way. */
    {
        memset(&tip.attack_coll.attack_records, 0, sizeof(tip.attack_coll.attack_records));
        tip.attack_coll.attack_records[0].victim_gobj = &towner;
        tip.attack_coll.attack_records[0].victim_flags.timer_rehit = 2;
        tip.attack_coll.attack_records[1].victim_gobj = &towner;
        tip.attack_coll.attack_records[1].victim_flags.timer_rehit = 0;

        tip.attack_coll.attack_state = nGMAttackStateOff;
        itProcessUpdateAttackRecords(&titem);
        CHECK(tip.attack_coll.attack_records[0].victim_flags.timer_rehit == 2); /* untouched */

        tip.attack_coll.attack_state = nGMAttackStateInterpolate;
        itProcessUpdateAttackRecords(&titem);
        CHECK(tip.attack_coll.attack_records[0].victim_flags.timer_rehit == 1);
        CHECK(tip.attack_coll.attack_records[0].victim_gobj == &towner);
        CHECK(tip.attack_coll.attack_records[1].victim_gobj == &towner); /* timer 0 never decremented */

        tip.attack_coll.attack_records[0].victim_flags.is_interact_hurt = TRUE;
        tip.attack_coll.attack_records[0].victim_flags.is_interact_shield = TRUE;
        tip.attack_coll.attack_records[0].victim_flags.is_interact_reflect = TRUE;
        itProcessUpdateAttackRecords(&titem); /* 1 -> 0, record clears */
        CHECK(tip.attack_coll.attack_records[0].victim_gobj == NULL);
        CHECK(tip.attack_coll.attack_records[0].victim_flags.is_interact_hurt == FALSE);
        CHECK(tip.attack_coll.attack_records[0].victim_flags.is_interact_shield == FALSE);
        CHECK(tip.attack_coll.attack_records[0].victim_flags.is_interact_reflect == FALSE);
        CHECK(tip.attack_coll.attack_records[0].victim_flags.group_id == 7);
    }

    /* itProcessSetHitInteractStats: same slot logic wpProcessSetHitInteract-
     * Stats was proven with (hosttest_ft.c:8727 et seq) -- an already-
     * recorded victim updates its own flags in place; a fresh victim
     * takes the first empty slot; once every slot is full the record
     * wraps back to slot 0. */
    {
        ITAttackColl ac;
        GObj victimA, victimB;
        memset(&ac, 0, sizeof(ac));

        itProcessSetHitInteractStats(&ac, &victimA, nGMHitTypeDamage, 0);
        CHECK(ac.attack_records[0].victim_gobj == &victimA);
        CHECK(ac.attack_records[0].victim_flags.is_interact_hurt == TRUE);

        itProcessSetHitInteractStats(&ac, &victimB, nGMHitTypeAttack, 5);
        CHECK(ac.attack_records[1].victim_gobj == &victimB);
        CHECK(ac.attack_records[1].victim_flags.group_id == 5);

        itProcessSetHitInteractStats(&ac, &victimA, nGMHitTypeShieldRehit, 0); /* revisits slot 0 */
        CHECK(ac.attack_records[0].victim_flags.is_interact_shield == TRUE);
        CHECK(ac.attack_records[0].victim_flags.timer_rehit == ITEM_REHIT_TIME_DEFAULT);

        for (i = 0; i < ARRAY_COUNT(ac.attack_records); i++)
        {
            ac.attack_records[i].victim_gobj = (GObj *)(intptr_t)(0x1000 + i);
        }
        {
            GObj victimC;
            itProcessSetHitInteractStats(&ac, &victimC, nGMHitTypeDamage, 0); /* all full -> wraps to 0 */
            CHECK(ac.attack_records[0].victim_gobj == &victimC);
        }
    }

    /* itVisualsUpdateSpin: rotate.z += spin_step, verbatim. */
    {
        DObj *dobj = DObjGetStruct(&titem);
        dobj->rotate.vec.f.z = 10.0F;
        tip.spin_step = 2.5F;
        itVisualsUpdateSpin(&titem);
        CHECK_EQF(dobj->rotate.vec.f.z, 12.5F);
        itVisualsUpdateSpin(&titem);
        CHECK_EQF(dobj->rotate.vec.f.z, 15.0F);
    }

    /* itVisualsUpdateColAnim: an all-zero colanim has nothing scheduled,
     * so ftMainUpdateColAnim returns FALSE and itMainClearColAnim is
     * never reached -- the colanim comes back byte-identical. */
    {
        GMColAnim before;
        memset(&tip.colanim, 0, sizeof(tip.colanim));
        before = tip.colanim;
        itVisualsUpdateColAnim(&titem);
        CHECK(memcmp(&tip.colanim, &before, sizeof(GMColAnim)) == 0);
    }

    /* itProcessProcSearchHitAll: is_hold TRUE skips the whole sweep
     * (nothing to check but "does not touch gGCCommonLinks/crash");
     * is_hold FALSE with interact_mask 0 on all three flags takes each
     * of the three search functions' own "nothing to interact with"
     * early-out, and with an empty gGCCommonLinks (no scene built) the
     * fighter/item/weapon walks see a NULL head and stop immediately
     * even with every interact flag raised. */
    {
        GObj *save_fighter = gGCCommonLinks[nGCCommonLinkIDFighter];
        GObj *save_item = gGCCommonLinks[nGCCommonLinkIDItem];
        GObj *save_weapon = gGCCommonLinks[nGCCommonLinkIDWeapon];
        gGCCommonLinks[nGCCommonLinkIDFighter] = NULL;
        gGCCommonLinks[nGCCommonLinkIDItem] = NULL;
        gGCCommonLinks[nGCCommonLinkIDWeapon] = NULL;

        tip.is_hold = TRUE;
        tip.damage_coll.interact_mask = 0;
        itProcessProcSearchHitAll(&titem);

        tip.is_hold = FALSE;
        tip.damage_coll.interact_mask = GMHITCOLLISION_FLAG_FIGHTER | GMHITCOLLISION_FLAG_WEAPON | GMHITCOLLISION_FLAG_ITEM;
        tip.owner_gobj = NULL;
        tip.team = 0;
        itProcessProcSearchHitAll(&titem);

        gGCCommonLinks[nGCCommonLinkIDFighter] = save_fighter;
        gGCCommonLinks[nGCCommonLinkIDItem] = save_item;
        gGCCommonLinks[nGCCommonLinkIDWeapon] = save_weapon;
    }

    /* itProcessProcHitCollisions: every proc_* pointer NULL (nothing
     * installs them yet -- itManagerInitItems and the per-item status
     * roster are both still ahead) so every dispatch guard is skipped
     * and the function falls straight through to its own tail: the
     * damage-queue add (clamped to GMCOMMON_PERCENT_DAMAGE_MAX) turns
     * into damage_lag, which turns into hitlag_tics via ftParamGetHitLag,
     * and every hit_.../damage_.../reflect_gobj field is zeroed at the end. */
    {
        memset(&tip, 0, sizeof(tip));
        tip.damage_queue = 10;
        tip.percent_damage = 5;
        {
            u16 expect_hitlag = ftParamGetHitLag(10, nFTCommonStatusWait, 1.0F);
            itProcessProcHitCollisions(&titem);
            CHECK(tip.percent_damage == 15);
            CHECK(tip.hitlag_tics == expect_hitlag);
        }
        CHECK(tip.damage_queue == 0);
        CHECK(tip.damage_lag == 0);
        CHECK(tip.hit_normal_damage == 0);
        CHECK(tip.hit_refresh_damage == 0);
        CHECK(tip.hit_attack_damage == 0);
        CHECK(tip.hit_shield_damage == 0);
        CHECK(tip.damage_highest == 0);
        CHECK_EQF(tip.damage_knockback, 0.0F);

        /* percent_damage clamps to GMCOMMON_PERCENT_DAMAGE_MAX */
        memset(&tip, 0, sizeof(tip));
        tip.percent_damage = GMCOMMON_PERCENT_DAMAGE_MAX - 2;
        tip.damage_queue = 10;
        itProcessProcHitCollisions(&titem);
        CHECK(tip.percent_damage == GMCOMMON_PERCENT_DAMAGE_MAX);

        /* reflect_gobj: owner/team/player/handicap/stat_flags/stat_count
         * copied from the reflector's FTStruct, is_static_damage FALSE
         * recomputes attack_coll.damage by the US reflect formula
         * (*1.8 +0.99), and reflect_gobj is cleared either way. */
        memset(&tip, 0, sizeof(tip));
        memset(&tfp, 0, sizeof(tfp));
        tfp.team = 2; tfp.player = 3; tfp.player_num = 1; tfp.handicap = 7;
        tip.reflect_gobj = &towner;
        tip.reflect_stat_flags.halfword = 0x1234;
        tip.reflect_stat_count = 42;
        tip.is_static_damage = FALSE;
        tip.attack_coll.damage = 10;
        itProcessProcHitCollisions(&titem);
        CHECK(tip.owner_gobj == &towner);
        CHECK(tip.team == 2);
        CHECK(tip.player == 3);
        CHECK(tip.player_num == 1);
        CHECK(tip.handicap == 7);
        CHECK(tip.attack_coll.stat_flags.halfword == 0x1234);
        CHECK(tip.attack_coll.stat_count == 42);
        CHECK(tip.attack_coll.damage == (s32)((10 * ITEM_REFLECT_MUL_DEFAULT) + ITEM_REFLECT_ADD_DEFAULT));
        CHECK(tip.reflect_gobj == NULL);

        /* is_static_damage TRUE leaves attack_coll.damage untouched */
        memset(&tip, 0, sizeof(tip));
        tip.reflect_gobj = &towner;
        tip.is_static_damage = TRUE;
        tip.attack_coll.damage = 77;
        itProcessProcHitCollisions(&titem);
        CHECK(tip.attack_coll.damage == 77);

        /* the reflect damage cap */
        memset(&tip, 0, sizeof(tip));
        tip.reflect_gobj = &towner;
        tip.is_static_damage = FALSE;
        tip.attack_coll.damage = ITEM_REFLECT_MAX_DEFAULT * 10;
        itProcessProcHitCollisions(&titem);
        CHECK(tip.attack_coll.damage == ITEM_REFLECT_MAX_DEFAULT);

        /* a proc_* pointer that returns TRUE makes the function return
         * immediately after itMainDestroyItem -- the tail zeroing below
         * it never runs, so a field seeded nonzero before the call stays
         * that way, proving the early return actually took.
         *
         * AND THE ITEM IS REALLY DESTROYED, so this case
         * cannot use the stack fixture the rest of the test uses: the real
         * itMainDestroyItem hands the ITStruct back to the MANAGER'S free
         * list and ejects the GObj, and doing that to `&tip`/`&titem`
         * would put a stack address on the pool's free list for every
         * later test to trip over. So this one takes a GObj and an
         * ITStruct out of the pools -- which is also what lets it check
         * the free, rather than only the early return: the struct really
         * does come back out again. */
        {
            static ITStruct probe_ip;
            GObj *real_gobj;
            ITStruct *real_ip;

            /* the pool is whatever the tests before this one left it, and
             * it can be empty -- so this lends the manager ONE struct of
             * its own and takes it straight back out, which is the same
             * borrow the hand-built pool tests make. Static, not stack:
             * itManagerSetPrevStructAlloc keeps the pointer. */
            memset(&probe_ip, 0, sizeof(probe_ip));
            itManagerSetPrevStructAlloc(&probe_ip);

            real_gobj = gcMakeGObjSPAfter(nGCCommonKindItem, NULL,
                                          nGCCommonLinkIDItem,
                                          GOBJ_PRIORITY_DEFAULT);
            real_ip = itManagerGetNextStructAlloc();

            CHECK(real_gobj != NULL);
            CHECK(real_ip == &probe_ip);

            /* the borrow is over: the pool is exactly as this test found
             * it, plus nothing */
            CHECK(itManagerGetCurrentAlloc() != &probe_ip);

            real_gobj->user_data.p = real_ip;
            gcAddDObjForGObj(real_gobj, NULL);
            CHECK(DObjGetStruct(real_gobj) != NULL);

            real_ip->damage_queue = 1;
            real_ip->proc_damage = test_it_process_proc_true;
            real_ip->hit_normal_damage = 55; /* zeroed by the tail if reached */
            /* a kind OUTSIDE the Gate monsters' range, so the destroy's
             * dust arm is taken rather than skipped -- and so the call
             * exercises the arm a real item takes */
            real_ip->kind = nITKindCapsule;

            itProcessProcHitCollisions(real_gobj);

            /* the early return: the tail never ran */
            CHECK(real_ip->hit_normal_damage == 55);

            /* and the free really happened -- the struct is back on the
             * manager's list, found by walking it rather than by popping
             * it, so the pool is left as this test found it */
            {
                ITStruct *ip;
                sb32 found = FALSE;

                for (ip = gITManagerStructsAllocFree; ip != NULL;
                     ip = ip->next)
                {
                    if (ip == real_ip)
                    {
                        found = TRUE;

                        break;
                    }
                }
                CHECK(found == TRUE);
            }
            /* and the GObj is off the item link */
            {
                GObj *g;
                sb32 still_linked = FALSE;

                for (g = gGCCommonLinks[nGCCommonLinkIDItem]; g != NULL;
                     g = g->link_next)
                {
                    if (g == real_gobj)
                    {
                        still_linked = TRUE;

                        break;
                    }
                }
                CHECK(still_linked == FALSE);
            }
        }

        /* proc_damage returning FALSE falls through to the rest as usual */
        memset(&tip, 0, sizeof(tip));
        tip.damage_queue = 1;
        tip.proc_damage = test_it_process_proc_false;
        tip.hit_normal_damage = 55;
        itProcessProcHitCollisions(&titem);
        CHECK(tip.hit_normal_damage == 0);
    }
}

/* ---- itMainDestroyItem ----------------------------------
 *
 * THE ITEM SUBSYSTEM'S FREE, and nothing that ends by returning TRUE
 * from its update leaves the field without it. Its arms are each a visible
 * thing, and each is checked here on real pool objects:
 *
 *   - a HELD item releases its owner's hands and clears the owner's
 *     `item_gobj`,
 *   - the arrow, if the item has one, is ejected off the interface link,
 *   - a NON-monster item puffs a large dust where it stood, and a GATE
 *     MONSTER does not -- the decomp's own range test, because a monster
 *     coming out of Saffron City's door is announced by its own walk,
 *   - and either way the ITStruct is back on the manager's free list and
 *     the GObj is off the item link.
 */
/* itmain.c's shared dust-range decision; see the check below */
extern sb32 itMainIsGroundMonster(ITStruct *ip);

static void test_it_destroy(void)
{
    /* The pool is whatever the tests before this one left it, so borrow
     * one struct at a time -- the same borrow test_it_process's destroy
     * case makes, and for the same reason. */
#define IT_DESTROY_BORROW(pip, pgobj)                                     \
    do {                                                                  \
        static ITStruct bor;                                              \
        memset(&bor, 0, sizeof(bor));                                     \
        itManagerSetPrevStructAlloc(&bor);                                \
        *(pip) = itManagerGetNextStructAlloc();                           \
        *(pgobj) = gcMakeGObjSPAfter(nGCCommonKindItem, NULL,             \
                                     nGCCommonLinkIDItem,                 \
                                     GOBJ_PRIORITY_DEFAULT);              \
        CHECK(*(pip) == &bor);                                            \
        CHECK(*(pgobj) != NULL);                                          \
        (*(pgobj))->user_data.p = *(pip);                                 \
        gcAddDObjForGObj(*(pgobj), NULL);                                 \
    } while (0)

    /* ---- the DUST RANGE TEST, as a decision ------------------------
     *
     * The decomp writes it inline -- `(ip->kind < nITKindGroundMonsterStart)
     * || (ip->kind > nITKindGroundMonsterEnd)` -- and it is the one part
     * of the destroy a host test cannot observe: with no particle bank the
     * large dust is allocated and handed straight back, so the effect
     * pool's count is the SAME whether the arm ran or was skipped (the
     * same limit test_it_porygon's dust check names). Rather than assert a
     * count that cannot distinguish them, the decision is a function both
     * the destroy and this test call -- the same fix as in the fighter
     * subsystem. */
    {
        ITStruct kip;

        memset(&kip, 0, sizeof(kip));

        kip.kind = nITKindCapsule;          /* an ordinary item: dust */
        CHECK(itMainIsGroundMonster(&kip) == FALSE);
        kip.kind = nITKindMBall;
        CHECK(itMainIsGroundMonster(&kip) == FALSE);

        /* the five Saffron City monsters, and their two ends */
        kip.kind = nITKindGLucky;
        CHECK(itMainIsGroundMonster(&kip) == TRUE);
        kip.kind = nITKindMarumine;
        CHECK(itMainIsGroundMonster(&kip) == TRUE);
        kip.kind = nITKindHitokage;
        CHECK(itMainIsGroundMonster(&kip) == TRUE);
        kip.kind = nITKindFushigibana;
        CHECK(itMainIsGroundMonster(&kip) == TRUE);
        kip.kind = nITKindPorygon;
        CHECK(itMainIsGroundMonster(&kip) == TRUE);

        /* the ENDS are inclusive, and the kinds either side are not */
        kip.kind = nITKindGroundMonsterStart - 1;
        CHECK(itMainIsGroundMonster(&kip) == FALSE);
        kip.kind = nITKindGroundMonsterEnd + 1;
        CHECK(itMainIsGroundMonster(&kip) == FALSE);
    }

    /* ---- a HELD item: the owner's hands come back -------------------- */
    {
        GObj owner;
        FTStruct ofp;
        ITStruct *ip;
        GObj *gobj;

        memset(&ofp, 0, sizeof(ofp));
        memset(&owner, 0, sizeof(owner));
        owner.user_data.p = &ofp;

        IT_DESTROY_BORROW(&ip, &gobj);

        ip->is_hold = TRUE;
        ip->owner_gobj = &owner;
        ofp.item_gobj = gobj;
        ip->kind = nITKindHammer;   /* a held Hammer, the case that matters */

        itMainDestroyItem(gobj);

        CHECK(ofp.item_gobj == NULL);
        CHECK(ip->is_hold == TRUE); /* the struct is not re-initialised */
    }

    /* ---- the ARROW is ejected off the interface link ----------------- */
    {
        GObj *arrow;
        ITStruct *ip;
        GObj *gobj;
        sb32 still_linked;

        IT_DESTROY_BORROW(&ip, &gobj);

        arrow = gcMakeGObjSPAfter(nGCCommonKindInterface, NULL,
                                  nGCCommonLinkIDInterface,
                                  GOBJ_PRIORITY_DEFAULT);
        CHECK(arrow != NULL);
        ip->arrow_gobj = arrow;
        ip->kind = nITKindCapsule;

        itMainDestroyItem(gobj);

        still_linked = FALSE;
        {
            GObj *g;

            for (g = gGCCommonLinks[nGCCommonLinkIDInterface]; g != NULL;
                 g = g->link_next)
            {
                if (g == arrow)
                {
                    still_linked = TRUE;

                    break;
                }
            }
        }
        CHECK(still_linked == FALSE);
    }

    /* ---- and the free itself, on the arm that skips the dust ---------
     *
     * A monster kind, so the destroy's only side effect is the two things
     * this checks: the struct goes back to the manager and the GObj leaves
     * the item link. */
    {
        ITStruct *ip;
        GObj *gobj;
        sb32 found = FALSE;
        sb32 still_linked = FALSE;
        ITStruct *sit;
        GObj *g;

        IT_DESTROY_BORROW(&ip, &gobj);
        ip->kind = nITKindGLucky;

        itMainDestroyItem(gobj);

        for (sit = gITManagerStructsAllocFree; sit != NULL; sit = sit->next)
        {
            if (sit == ip)
            {
                found = TRUE;

                break;
            }
        }
        CHECK(found == TRUE);

        for (g = gGCCommonLinks[nGCCommonLinkIDItem]; g != NULL;
             g = g->link_next)
        {
            if (g == gobj)
            {
                still_linked = TRUE;

                break;
            }
        }
        CHECK(still_linked == FALSE);
    }

#undef IT_DESTROY_BORROW
}

/* src/dc/itdisplay.c: itDisplayCheckItemVisible's gate, and the four
 * proc_display dispatchers' branch logic. The dispatchers' real draw
 * calls (gcDrawDObjTreeForGObj/gcDrawDObjTreeDLLinksForGObj) are safe
 * to run here -- an all-zero DObj's xobjs_num is 0, so gcPrepDObjMatrix
 * degrades to a genuine no-op, not a stand-in (see itdisplay.c's header
 * comment) -- but they leave nothing to check directly; the observable
 * signal is itDisplayColAnimOPA/XLU's env-colour word, recorded on the
 * host arm the same way wpdisplay.c's PK Thunder colours are. */
extern u32 gITDisplayColAnimLastEnv;

static void test_it_display(void)
{
    GObj titem, towner;
    ITStruct tip;
    FTStruct tfp;
    DObj tdobj;

    memset(&tip, 0, sizeof(tip));
    memset(&tdobj, 0, sizeof(tdobj));
    titem.user_data.p = &tip;
    titem.obj = &tdobj;
    towner.user_data.p = &tfp;

    /* itDisplayCheckItemVisible: no owner is always visible; an owner
     * that is not holding it is always visible; held-but-hidden checks
     * the owner's is_item_show/is_invisible. */
    tip.owner_gobj = NULL;
    CHECK(itDisplayCheckItemVisible(&tip) == TRUE);

    tip.owner_gobj = &towner;
    tip.is_hold = FALSE;
    CHECK(itDisplayCheckItemVisible(&tip) == TRUE);

    tip.is_hold = TRUE;
    memset(&tfp, 0, sizeof(tfp));
    tfp.is_item_show = FALSE;
    CHECK(itDisplayCheckItemVisible(&tip) == FALSE);

    tfp.is_item_show = TRUE;
    tfp.is_invisible = TRUE;
    CHECK(itDisplayCheckItemVisible(&tip) == FALSE);

    tfp.is_invisible = FALSE;
    CHECK(itDisplayCheckItemVisible(&tip) == TRUE);

    /* itDisplayColAnimOPAProcDisplay/XLUProcDisplay: Master mode draws,
     * and the env colour is packed from colanim.color1 (RGBA, high byte
     * first) when is_use_color1 is set. */
    memset(&tip, 0, sizeof(tip));
    tip.display_mode = nDBDisplayModeMaster;
    tip.colanim.is_use_color1 = TRUE;
    tip.colanim.color1.r = 0x11; tip.colanim.color1.g = 0x22;
    tip.colanim.color1.b = 0x33; tip.colanim.color1.a = 0x44;
    gITDisplayColAnimLastEnv = 0xDEADBEEFU;
    itDisplayColAnimOPAProcDisplay(&titem);
    CHECK(gITDisplayColAnimLastEnv == 0x11223344U);

    gITDisplayColAnimLastEnv = 0xDEADBEEFU;
    itDisplayColAnimXLUProcDisplay(&titem);
    CHECK(gITDisplayColAnimLastEnv == 0x11223344U);

    /* is_use_color1 FALSE packs zero regardless of the stale color1
     * fields. */
    tip.colanim.is_use_color1 = FALSE;
    gITDisplayColAnimLastEnv = 0xDEADBEEFU;
    itDisplayColAnimOPAProcDisplay(&titem);
    CHECK(gITDisplayColAnimLastEnv == 0U);

    /* is_hold TRUE also takes the draw arm regardless of display_mode. */
    tip.display_mode = 0;
    tip.is_hold = TRUE;
    tip.colanim.is_use_color1 = TRUE;
    gITDisplayColAnimLastEnv = 0xDEADBEEFU;
    itDisplayColAnimOPAProcDisplay(&titem);
    CHECK(gITDisplayColAnimLastEnv == 0x11223344U);
    tip.is_hold = FALSE;

    /* MapCollision mode draws too (itDisplayMapCollisions is a DIVERGES
     * no-op, so nothing extra to observe from it, but the draw itself
     * still runs and still sets the env colour). */
    tip.display_mode = nDBDisplayModeMapCollision;
    gITDisplayColAnimLastEnv = 0xDEADBEEFU;
    itDisplayColAnimOPAProcDisplay(&titem);
    CHECK(gITDisplayColAnimLastEnv == 0x11223344U);

    /* No hitstatus/no live hitbox with a non-Master, non-MapCollision
     * mode still takes the "draw normally" arm. */
    tip.display_mode = 999; /* anything not Master/MapCollision */
    tip.damage_coll.hitstatus = nGMHitStatusNone;
    tip.attack_coll.attack_state = nGMAttackStateOff;
    gITDisplayColAnimLastEnv = 0xDEADBEEFU;
    itDisplayColAnimOPAProcDisplay(&titem);
    CHECK(gITDisplayColAnimLastEnv == 0x11223344U);

    /* A live hitbox or hurtbox under a non-Master/MapCollision mode
     * diverts to itDisplayHitCollisions instead -- the draw (and its
     * env-colour set) never runs, so the sentinel survives. */
    tip.damage_coll.hitstatus = nGMHitStatusNormal;
    gITDisplayColAnimLastEnv = 0xDEADBEEFU;
    itDisplayColAnimOPAProcDisplay(&titem);
    CHECK(gITDisplayColAnimLastEnv == 0xDEADBEEFU);

    tip.damage_coll.hitstatus = nGMHitStatusNone;
    tip.attack_coll.attack_state = nGMAttackStateNew;
    gITDisplayColAnimLastEnv = 0xDEADBEEFU;
    itDisplayColAnimXLUProcDisplay(&titem);
    CHECK(gITDisplayColAnimLastEnv == 0xDEADBEEFU);

    /* itDisplayCheckItemVisible's own gate: a held item with an
     * invisible owner never reaches the draw at all. */
    tip.display_mode = nDBDisplayModeMaster;
    tip.attack_coll.attack_state = nGMAttackStateOff;
    tip.owner_gobj = &towner;
    tip.is_hold = TRUE;
    tfp.is_item_show = TRUE;
    tfp.is_invisible = TRUE;
    gITDisplayColAnimLastEnv = 0xDEADBEEFU;
    itDisplayColAnimOPAProcDisplay(&titem);
    CHECK(gITDisplayColAnimLastEnv == 0xDEADBEEFU);

    /* itDisplayOPAProcDisplay/XLUProcDisplay: no observable state (the
     * real draw is a genuine no-op on an all-zero DObj), so this proves
     * only that every branch runs without crashing. */
    tip.owner_gobj = NULL;
    tip.is_hold = FALSE;
    tip.display_mode = nDBDisplayModeMaster;
    itDisplayOPAProcDisplay(&titem);
    itDisplayXLUProcDisplay(&titem);
    tip.display_mode = nDBDisplayModeMapCollision;
    itDisplayOPAProcDisplay(&titem);
    itDisplayXLUProcDisplay(&titem);
    tip.display_mode = 999;
    tip.damage_coll.hitstatus = nGMHitStatusNone;
    tip.attack_coll.attack_state = nGMAttackStateOff;
    itDisplayOPAProcDisplay(&titem);
    itDisplayXLUProcDisplay(&titem);
    tip.attack_coll.attack_state = nGMAttackStateNew;
    itDisplayOPAProcDisplay(&titem); /* diverts to itDisplayHitCollisions */
    itDisplayXLUProcDisplay(&titem);
}

/* mnvsrecord.h -- the Records screen, mn/mndata/mnvsrecord.c.
 *
 * The first of DATA's three tabs: three tables of the save file's VS-mode tallies, cycled with
 * B -- Battle Score (a KOs-against grid), Ranking (seven sortable
 * columns) and Individual (one fighter's record against every other,
 * with the stick moving between fighters). Every function is the
 * decomp's by name and body, the REGION_US arms; the line numbers are
 * the decomp's.
 *
 * What is not here, and where it is said:
 *   - the lighting pre-render (mnVSRecordFuncLights) and the clear
 *     camera -- dMNVSRecordTaskmanSetup and mnVSRecordFuncStart;
 *   - syVideoInit and the arena_size line -- mnVSRecordStartScene;
 *   - the reloc setup: four sprite banks stand in for the four files,
 *     two of them (portraits, fonts) reused from banks the character
 *     select and the stage select already put on the disc --
 *     mnVSRecordLoadFiles;
 *   - the three grid-drawing functions' and the ranking highlight's raw
 *     RDP fill-rectangle sequences, in the port's spelling
 *     (src/dc/lbcommon.h), including syVideoGetFillColor's cut -- the
 *     port's most-applied divergence, dropped the same way syVideoInit
 *     is everywhere else.
 *
 * B from the top tab (Battle Score) leaves for the DATA menu; B from
 * either other tab steps back one. A and START are read nowhere in
 * this file -- there is nothing past the tables to select.
 */
#ifndef SSB_DC_MNVSRECORD_H
#define SSB_DC_MNVSRECORD_H

#include <sys/obj.h>
#include <sys/taskman.h>

/* mnvsrecord.c:2208 */
extern SYTaskmanSetup dMNVSRecordTaskmanSetup;

/* mnvsrecord.c:105, the four files' bases: the game's void*[4] with a
 * SpriteBank* in each (sprite.h). Not static in the decomp; the host
 * test reads sprites out of them by offset. */
extern void *sMNVSRecordFiles[4];

/* mnvsrecord.c:69-99, the state the host test reads: which table is
 * showing, the Ranking column it starts on, which fighter the
 * Individual tab is on, and the three sort orders each table keeps. */
extern s32 sMNVSRecordStatsKind;
extern s32 sMNVSRecordFirstColumn;
extern s32 sMNVSRecordCurrentIndex;
extern u16 sMNVSRecordFighterMask;
extern s32 sMNVSRecordBattleScoreFighterKinds[];
extern s32 sMNVSRecordRankingFighterKindOrder[];
extern s32 sMNVSRecordIndivFighterKinds[];
extern GObj *sMNVSRecordTableHeadersGObj;
extern GObj *sMNVSRecordTableValuesGObj;

/* mnvsrecord.c:113-2202, in the decomp's order. mnVSRecordFuncLights is
 * not here: it is cut with the rest of the pre-render (mnvsrecord.h's
 * header comment says where). */
s32 mnVSRecordGetFighterKindByIndex(s32 index);
s32 mnVSRecordGetKOs(s32 fkind);
s32 mnVSRecordGetTKO(s32 fkind);
s32 mnVSRecordGetTotalTKO(void);
f32 mnVSRecordGetWinPercent(s32 fkind);
s32 mnVSRecordGetPowerOf(s32 base, s32 exp);
void mnVSRecordSetSpriteColors(SObj *sobj, u32 *colors);
s32 mnVSRecordGetDigitCount(s32 number, s32 digit_count_max);
void mnVSRecordMakeDigits(GObj *gobj, s32 number, f32 x, f32 y, u32 *colors,
                          sb32 is_show_tenths, sb32 is_wide,
                          s32 digit_count_max, sb32 is_fixed_digit_count);
s32 mnVSRecordGetCharacterID(const char c);
f32 mnVSRecordGetCharacterSpacing(const char *str, s32 c);
void mnVSRecordMakeString(GObj *gobj, const char *str, f32 x, f32 y, u32 *color);
sb32 mnVSRecordCheckHaveFighterKind(s32 fkind);
void mnVSRecordMakeLabels(void);
void mnVSRecordSubtitleProcUpdate(GObj *gobj);
void mnVSRecordMakeSubtitle(void);
void mnVSRecordPortraitArrowsProcUpdate(GObj *gobj);
void mnVSRecordMakePortraitStatsArrows(void);
void mnVSRecordResortArrowsProcUpdate(GObj *gobj);
void mnVSRecordMakeResortArrows(void);
void mnVSRecordColumnArrowsProcUpdate(GObj *gobj);
void mnVSRecordMakeColumnArrows(void);
void mnVSRecordDrawBattleScoreGrid(void);
void mnVSRecordDrawRankingGrid(s32 first_column);
void mnVSRecordDrawIndivGrid(void);
void mnVSRecordTableGridProcDisplay(GObj *gobj);
void mnVSRecordMakeStatsGrid(void);
void mnVSRecordSetIconPositionForColumn(SObj *sobj, s32 column);
SObj *mnVSRecordMakeLockedIcon(GObj *gobj);
void mnVSRecordMakeColumnIcons(GObj *gobj);
void mnVSRecordSetRowIconPosition(SObj *sobj, s32 row);
void mnVSRecordMakeRowIcons(GObj *gobj);
s32 mnVSRecordGetRanking(s32 fkind);
void mnVSRecordMakePortraitStats(GObj *gobj, s32 fkind);
void mnVSRecordSortData(s32 stats_kind);
GObj *mnVSRecordMakeBattleScoreTableValues(void);
GObj *mnVSRecordMakeBattleScoreTableHeaders(void);
void mnVSRecordRankingHighlightProcDisplay(GObj *gobj);
void mnVSRecordMakeRankingHighlight(void);
f32 mnVSRecordGetAvg(s32 fkind);
s32 mnVSRecordGetGamesPlayedSum(void);
f32 mnVSRecordGetUsePercent(s32 fkind);
f32 mnVSRecordGetSDPercent(s32 fkind);
GObj *mnVSRecordMakeRankingTableValues(s32 column);
GObj *mnVSRecordMakeRankingTableHeaders(s32 column);
f32 mnVSRecordGetWinPercentAgainst(s32 this_fkind, s32 against_fkind);
f32 mnVSRecordGetAvgAgainst(s32 this_fkind, s32 against_fkind);
void func_ovl32_8013547C(void);
GObj *mnVSRecordMakeIndivTableValues(void);
GObj *mnVSRecordMakeIndivPortraitAll(void);
void mnVSRecordMakeStats(s32 stats_kind);
void mnVSRecordMakeTableValuesCamera(void);
void mnVSRecordMakeTableHeadersCamera(void);
void mnVSRecordMakeTableGridCamera(void);
void mnVSRecordMakeRankingHighlightCamera(void);
void mnVSRecordMakeLabelsCamera(void);
void mnVSRecordInitVars(void);
void mnVSRecordRedrawStats(s32 stats_kind);
void mnVSRecordFuncRun(GObj *gobj);
void mnVSRecordFuncStart(void);

/* mnvsrecord.c:2253 0x801365D0: one task, and the scene is over when it
 * ends. */
void mnVSRecordStartScene(void);

/* The bzero arm of syDmaLoadOverlay for overlay 32 (src/dc/overlay.h). */
void mnVSRecordOverlayLoad(void);

#endif /* SSB_DC_MNVSRECORD_H */

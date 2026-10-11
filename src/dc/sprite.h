/* sprite.h -- a bank of the game's sprites, as tools/export/ssb_spriteexport.py
 * writes one and the PVR wants them.
 *
 * On the N64 a scene's sprites arrive as part of a relocData file
 * (mn/mncommon/mntitlefiles.c lbRelocLoadFilesListed), and a Sprite is
 * reached as lbRelocGetFileData(Sprite*, file, offset): the file's base
 * plus the offset the scene's tables carry. Its Bitmap strips point at
 * texel rows the RDP loads into TMEM every time the sprite is drawn.
 *
 * The port's equivalent is this bank: one file per relocData file,
 * holding every Sprite and Bitmap the game reads (the fields untouched,
 * so lb/lbcommon.c's sprite arithmetic runs as written), and behind each
 * sprite one PVR texture with the strips stitched back into the image
 * they are. The texture is uploaded once when the bank loads, which is
 * the PVR's own model -- textures live in VRAM, a draw names one -- and
 * the thing the RDP had to redo per rectangle is gone.
 *
 * What a Bitmap's `buf` points at here is a DCSpriteTex rather than
 * texel bytes; its `t` is the row of the stitched image the strip
 * starts on, which is what libultra's field means (sp.h: "Vertical
 * offset into base") and what the N64 data leaves at zero because every
 * strip is its own load.
 *
 * A CI sprite is stored with its palette applied, and a sprite the
 * game re-palettes at runtime -- the stock icon, whose Sprite.LUT is
 * pointed at one of FTSprites.stock_luts per costume; the character
 * select's gate card, whose LUT is pointed at a palette named by its
 * offset in the file -- is stored once per palette. The bank hands the
 * game an int*[] of those textures for stock_luts, so `sprite.LUT =
 * stock_luts[costume]` (if/ifcommon.c) runs as written, and
 * sprite_bank_lut answers the by-offset form; either way the renderer
 * draws the texture the LUT names: on the PVR the palette is the
 * texture (lbcommon.c lbCommonDrawSObjBitmap).
 *
 * Banks are scene assets: they come out of the scene heap and the
 * scene manager releases their VRAM between scenes (sprite_bank_release
 * _all, from src/dc/scmanager.c), where the game swaps overlays and
 * re-inits the heap.
 */
#ifndef SSB_DC_SPRITE_H
#define SSB_DC_SPRITE_H

#include <ssb_types.h>
#include <PR/sp.h>

#ifndef FT_HOSTTEST
#include <dc/pvr.h>
#else
typedef void *pvr_ptr_t;
#endif

/* Sprite texture formats the bank carries, matched to the PVR's. RGBA16
 * and CI sprites are ARGB1555 (their palettes are RGBA5551); I, IA and
 * RGBA32 are ARGB4444, with the intensity of an I or IA sprite in RGB
 * and its alpha in A -- see the exporter's docstring. */
enum
{
    nDCSpriteTexFmtARGB1555 = 0,
    nDCSpriteTexFmtARGB4444 = 1,
    /* No bank carries this one: a rendered frame, non-twiddled at the
     * PVR's texture stride (taskman.h syTaskmanGetPhoto). texw/texh
     * are the powers of two the UVs are measured against. */
    nDCSpriteTexFmtRGB565Stride = 2
};

typedef struct DCSpriteTex
{
    pvr_ptr_t txr;              /* VRAM, or NULL on the host */
    u16 texw, texh;             /* power-of-two texture dims */
    u16 imgw, imgh;             /* the stitched image inside them */
    u8 fmt;                     /* nDCSpriteTexFmt* */
    u8 pad[3];
} DCSpriteTex;

typedef struct DCSpriteEntry
{
    char name[16];
    u32 file_off;               /* the game's key: offset in the relocData file */
    Sprite sprite;              /* fields as read from the ROM; bitmap -> below */
    Bitmap *bitmaps;
    DCSpriteTex *texs;          /* nluts textures, scene heap; [0] is the
                                 * sprite's own palette */
    int **luts;                 /* nluts pointers, one per texture: what
                                 * Sprite.LUT points into (FTSprites.stock_luts) */
    u32 *lut_offs;              /* nluts relocData offsets: which palette
                                 * each texture was drawn with */
    u32 nluts;
} DCSpriteEntry;

typedef struct SpriteBank
{
    u32 file_id;                /* which relocData file this was */
    u32 count;
    DCSpriteEntry *entries;     /* scene heap */
    Bitmap *bitmaps;            /* scene heap, all sprites' strips */
    size_t vram_bytes;          /* what the textures took */
    sb32 is_loaded;
} SpriteBank;

/* Load a .spr into `bank`, by name ("ifstatus.spr") -- the asset root
 * decides which medium it comes off (src/dc/assetroot.h). The records go
 * into the scene heap (so they vanish with it, like the game's file), and
 * the textures into VRAM. Returns 0, or -1 with the reason on the log.
 * Registers the bank so sprite_bank_release_all finds it. Loading a bank
 * that is already loaded releases the first upload first, and it stays
 * registered once -- the unlock message's task runs once per queued
 * message and re-loads on each (src/dc/mnmessage.c). */
int sprite_bank_load(SpriteBank *bank, const char *name);

/* sprite_bank_load for a bank that lives for the whole session (the port's own
 * font, src/dc/dctext.h): the records come out of the
 * C heap rather than the scene heap, and the bank is never registered, so
 * sprite_bank_release_all and the heap reset leave it and its VRAM alone.
 * A second call on a loaded bank does nothing. Load it at boot, before
 * the scene-sized allocations, so it sits low in both heaps. The game has
 * no such bank: its every sprite belongs to a scene. */
int sprite_bank_load_resident(SpriteBank *bank, const char *name);

/* lbRelocGetFileData(Sprite*, file, offset) for a bank: the Sprite the
 * game's tables name by its offset in the relocData file. NULL, with a
 * log line, when the bank has no such sprite -- which is a build error
 * (the exporter was asked for a subset), not a runtime condition. */
Sprite *sprite_bank_get(SpriteBank *bank, u32 file_off);

/* The whole entry behind sprite_bank_get's Sprite: for the palette
 * table a fighter's FTSprites.stock_luts wants. NULL as above. */
DCSpriteEntry *sprite_bank_entry(SpriteBank *bank, u32 file_off);

/* lbRelocGetFileData(int*, file, offset) for a bank: what Sprite.LUT is
 * pointed at when the game names a palette by its offset in the file
 * (mn/mnplayers/mnplayersvs.c mnPlayersVSSetGateLUT). The texture the
 * exporter drew with that palette (--luts), as the pointer the renderer
 * reads back. NULL, with a log line, when no sprite in the bank was
 * exported with it -- a build error, as above. */
int *sprite_bank_lut(SpriteBank *bank, u32 lut_off);

/* Free every loaded bank's VRAM and forget it. The scene heap side needs
 * no call: the next scene's setup re-inits the heap. Called at every
 * scene change (src/dc/scmanager.c) and again from
 * syTaskmanResetGeneralHeap, which is the one that catches a scene that
 * re-inits the heap without ending -- see src/dc/taskman.c. */
void sprite_bank_release_all(void);

#endif /* SSB_DC_SPRITE_H */

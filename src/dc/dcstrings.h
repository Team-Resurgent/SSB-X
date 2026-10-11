#ifndef SSB64DC_DCSTRINGS_H
#define SSB64DC_DCSTRINGS_H

/* dcstrings.h -- every line of text the port itself puts on the screen:
 * the VMU's notice, its boot check, its page in Options
 * and its failure prompt. The game has none of these -- a cartridge
 * never asks where to save -- so none of them is the game's writing; the
 * glyphs they are drawn in are (src/dc/dctext.h).
 *
 * The font is the staff roll's popup textbox, which has A-Z, a-z, 0-9
 * and : . - , & " / ' ? ( ) and nothing else: no ! + % < > or digits'
 * friends. A line here that uses a character outside that set draws a
 * gap, so the host test (hosttest/text.c) walks this list and fails on
 * any. A printf conversion ("%d", "%s", "%c", "%02d") is allowed -- what
 * it expands to is checked where it is formatted.
 *
 * One X-macro list, so the test and the table cannot disagree:
 * DCSTR(id, "text") becomes nDCStr<id> and gDCStrings[nDCStr<id>]. */
#ifdef TARGET_XBOX
/* XBOX: the same lines for the Xbox, where the "card" is the console's
 * hard disk (Platform/xbox/kos_vmu_xbox.c): one place, nothing to remove,
 * so no port letter or slot. The callers still pass those to snprintf; a
 * format that does not convert them ignores them. Same font, same
 * character set as below. */
#define DCSTRINGS(DCSTR)                                                     \
    DCSTR(Saving,        "SAVING...")                                       \
    DCSTR(Loading,       "LOADING...")                                      \
    DCSTR(SavingWarn,    "Do not turn off")                                 \
    DCSTR(SavingPower,   "the console.")                                    \
    DCSTR(Title,         "Hard Disk")                                       \
    DCSTR(CardName,      "Xbox Hard Disk")                                  \
    DCSTR(CardFree,      "%d blocks free")                                  \
    DCSTR(CardFreeInUse, "%d blocks free - in use")                         \
    DCSTR(CardSaved,     "Saved %04d/%02d/%02d %02d:%02d")                  \
    DCSTR(CardNoSave,    "No save data")                                    \
    DCSTR(CardDamaged,   "Damaged save data")                               \
    DCSTR(NoCard,        "The hard disk cannot be used.")                   \
    DCSTR(NoCard2,       "No progress will be saved.")                      \
    DCSTR(NoSave,        "There is no save data on the hard disk.")         \
    DCSTR(NoSave2,       "Create it now? It needs %d blocks.")              \
    DCSTR(NoSpace,       "The hard disk does not have enough")              \
    DCSTR(NoSpace2,      "free blocks: %d are needed, %d are free.")        \
    DCSTR(Damaged,       "The save data on the hard disk")                  \
    DCSTR(Damaged2,      "is damaged and cannot be loaded.")                \
    DCSTR(Choose,        "Choose where to save.")                           \
    DCSTR(Failed,        "The save could not be written")                   \
    DCSTR(Failed2,       "to the hard disk.")                               \
    DCSTR(Creating,      "Creating save data...")                           \
    DCSTR(CreatingWarn,  "Do not turn off the console.")                    \
    DCSTR(ActCreate,     "Create save data")                                \
    DCSTR(ActOverwrite,  "Overwrite it")                                    \
    DCSTR(ActRetry,      "Try again")                                       \
    DCSTR(ActOther,      "Choose again")                                    \
    DCSTR(ActWithout,    "Continue without saving")                         \
    DCSTR(Continue,      "Continue")                                        \
    DCSTR(ActSaveNow,    "Save now")                                        \
    DCSTR(ActLoad,       "Load from the hard disk")                         \
    DCSTR(ActSaveHere,   "Save to the hard disk")                           \
    DCSTR(ActDelete,     "Delete save data")                                \
    DCSTR(AreYouSure,    "Delete the save data? Are you sure?")             \
    DCSTR(Yes,           "Yes")                                             \
    DCSTR(No,            "No")                                              \
    DCSTR(Back,          "B: Back")
#else
#define DCSTRINGS(DCSTR)                                                     \
    /* the corner notice, while a write is going */            \
    DCSTR(Saving,        "SAVING...")                                       \
    DCSTR(Loading,       "LOADING...")                                      \
    DCSTR(SavingWarn,    "Do not remove the VMU")                   \
    DCSTR(SavingPower,   "or turn off the power.")                          \
    /* the page's title, and the name of a VMU: port letter, slot */       \
    DCSTR(Title,         "VMU")                                     \
    DCSTR(CardName,      "VMU %c%d")                                \
    DCSTR(CardFree,      "%d blocks free")                                  \
    DCSTR(CardFreeInUse, "%d blocks free - in use")                         \
    DCSTR(CardSaved,     "Saved %04d/%02d/%02d %02d:%02d")                  \
    DCSTR(CardNoSave,    "No save data")                                    \
    DCSTR(CardDamaged,   "Damaged save data")                               \
    /* the boot check */                                       \
    DCSTR(NoCard,        "No VMU is present.")                     \
    DCSTR(NoCard2,       "No progress will be saved.")         \
    DCSTR(NoSave,        "There is no save data on VMU %c%d.")      \
    DCSTR(NoSave2,       "Create it now? It needs %d blocks.")              \
    DCSTR(NoSpace,       "VMU %c%d does not have enough")           \
    DCSTR(NoSpace2,      "free blocks: %d are needed, %d are free.")        \
    DCSTR(Damaged,       "The save data on VMU %c%d")               \
    DCSTR(Damaged2,      "is damaged and cannot be loaded.")                \
    DCSTR(Choose,        "Choose a VMU to save to.")                \
    /* the failure prompt */                                  \
    DCSTR(Failed,        "The save could not be written")                   \
    DCSTR(Failed2,       "to VMU %c%d.")                            \
    /* while the boot check writes the file */                              \
    DCSTR(Creating,      "Creating save data...")                           \
    DCSTR(CreatingWarn,  "Do not remove the VMU or turn off the power.")    \
    /* the answers */                                                       \
    DCSTR(ActCreate,     "Create save data")                                \
    DCSTR(ActOverwrite,  "Overwrite it")                                    \
    DCSTR(ActRetry,      "Try again")                                       \
    DCSTR(ActOther,      "Choose another card")                             \
    DCSTR(ActWithout,    "Continue without saving")                 \
    DCSTR(Continue,      "Continue")                         \
    DCSTR(ActSaveNow,    "Save now")                                        \
    DCSTR(ActLoad,       "Load from this card")                             \
    DCSTR(ActSaveHere,   "Save to this card")                               \
    DCSTR(ActDelete,     "Delete save data")                                \
    DCSTR(AreYouSure,    "Delete the save data? Are you sure?")             \
    DCSTR(Yes,           "Yes")                                             \
    DCSTR(No,            "No")                                              \
    DCSTR(Back,          "B: Back")
#endif /* TARGET_XBOX */

#define DCSTR_ENUM(id, text) nDCStr##id,
enum
{
    DCSTRINGS(DCSTR_ENUM)
    nDCStrCount
};
#undef DCSTR_ENUM

/* src/dc/dctext.c */
extern const char *const gDCStrings[nDCStrCount];

#endif /* SSB64DC_DCSTRINGS_H */

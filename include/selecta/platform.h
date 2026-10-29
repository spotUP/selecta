/* Platform families and the file name -> family resolver (I691 platform covers, design doc section 1 and 4). Shared by the
 * library (majority format of an album or person) and the GUI (cover of a family); depends on nothing. */
#ifndef SELECTA_PLATFORM_H
#define SELECTA_PLATFORM_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Platform families (I691a). Each console that has formats of its own is its own family; SEL_PLATFORM_FAMILY_CONSOLE is
 * for consoles with too few formats to stand alone. The order of 1..12 is the order of the gui.cover.* tokens
 * (SEL_GUI_TOKEN_COVER_AMIGA ... SEL_GUI_TOKEN_COVER_CONSOLE); the music family uses the ACCENT token. */
enum {
    SEL_PLATFORM_FAMILY_MUSIC = 0,
    SEL_PLATFORM_FAMILY_AMIGA,
    SEL_PLATFORM_FAMILY_COMMODORE_64,
    SEL_PLATFORM_FAMILY_ATARI,
    SEL_PLATFORM_FAMILY_ZX_SPECTRUM,
    SEL_PLATFORM_FAMILY_PC_TRACKER,
    SEL_PLATFORM_FAMILY_NES,
    SEL_PLATFORM_FAMILY_GAME_BOY,
    SEL_PLATFORM_FAMILY_SNES,
    SEL_PLATFORM_FAMILY_PC_ENGINE,
    SEL_PLATFORM_FAMILY_MSX,
    SEL_PLATFORM_FAMILY_MEGA_DRIVE,
    SEL_PLATFORM_FAMILY_CONSOLE,
    SEL_PLATFORM_FAMILY_COUNT
};

/* The family of a file extension without the dot, any case; SEL_PLATFORM_FAMILY_MUSIC when unknown. */
uint32_t sel_platform_of_ext(const char* ext);
/* The last extension of a file name without the dot, original case ("x.Mod" gives "Mod"); NULL without one. The one
 * extension extractor of the desktop UI: the family resolver and the format column both use it. */
const char* sel_platform_path_ext(const char* name);
/* The family of a file name: its last extension ("x.mod"), else its Amiga prefix, the part before the first dot
 * ("mod.x"; the extension table plus the explicit UADE prefix list), case-insensitively. Music when neither is known. */
uint32_t sel_platform_of_name(const char* name);
/* I691c: the majority family of a set of songs (an album, a person, a playlist). Feed every song name to _add; _majority is the family
 * holding the most songs, a tie going to the family earlier in the enum (table) order, and SEL_PLATFORM_FAMILY_MUSIC only when no other
 * family has a song. _format writes the majority's representative extension (the first table extension of the family; the
 * extension of the first song for Music) into out, empty for an empty tally. The engine hands that string to the UI as `format`. */
typedef struct sel_platform_family_tally {
    uint32_t n[SEL_PLATFORM_FAMILY_COUNT];
    char music_ext[16];
} sel_platform_family_tally;
void sel_platform_family_tally_add(sel_platform_family_tally* t, const char* name);
uint32_t sel_platform_family_tally_majority(const sel_platform_family_tally* t);
void sel_platform_family_tally_format(const sel_platform_family_tally* t, char* out, size_t cap);
/* Number of rows of the one extension table (for tests). */
uint32_t sel_platform_ext_count(void);
/* The UADE file name prefixes ("dum" of "dum.song") that map to the Amiga family. */
uint32_t sel_platform_uade_prefix_count(void);
const char* sel_platform_uade_prefix(uint32_t i);
/* Lower case copy of src into dst (ASCII), at most cap-1 characters. */
void sel_platform_lower(char* dst, const char* src, size_t cap);
/* The family's name in full English words ("Commodore 64", "Game Boy") and its lower case id ("game_boy"). */
const char* sel_platform_label(uint32_t family);
const char* sel_platform_id(uint32_t family);

#ifdef __cplusplus
}
#endif

#endif /* SELECTA_PLATFORM_H */

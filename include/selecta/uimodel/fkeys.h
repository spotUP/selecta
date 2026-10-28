/* Stored programs on F1..F10 (DUI-P5-8, MUI-P8-2; HippoPlayer hippo:70). One model for every UI: a program is the saved
 * playlist named "F<n>"; F<n> recalls it (load, then play) and Shift+F<n> stores the current queue under that name.
 * Pure C99. */
#ifndef SELECTA_UIMODEL_FKEYS_H
#define SELECTA_UIMODEL_FKEYS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SEL_UIM_FKEY_COUNT 10u
#define SEL_UIM_FKEY_LINES 2u
#define SEL_UIM_FKEY_LINE_MAX 24u

/* The program slot 1..10 a key selects (code TXT_KEY_F1.. with the TXT_MOD_* bits), 0 for any other key. *store is 1 for
 * Shift+F<n>, 0 for the plain key. */
uint32_t sel_uim_fkey_slot(uint32_t code, uint32_t mods, int* store);
/* The engine lines for a slot: recall gives "PLAYLIST_LOAD F3" and "PLAY"; store gives "PLAYLIST_SAVE F3". Returns the
 * number of lines written (0 for a slot outside 1..10). */
uint32_t sel_uim_fkey_lines(uint32_t slot, int store, char out[SEL_UIM_FKEY_LINES][SEL_UIM_FKEY_LINE_MAX]);

#ifdef __cplusplus
}
#endif

#endif

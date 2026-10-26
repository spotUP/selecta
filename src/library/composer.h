/* Composer name normalisation (plan 2026-10-25-m3-library.md I182, T85; design doc 6/9 "author normalisation via
 * DEViLBOX songdb"). ONE function turns every spelling of a composer into a canonical key: the composer query, the
 * stored song_composers rows and the composer page header all go through it, so a display and a query never
 * disagree.
 *
 * Steps: drop "(handle)" groups, turn "Last, First" into "First Last", fold case and punctuation to single spaces,
 * There is no built-in handle table: a handle and a real name merge only when DEViLBOX songdb data links them
 * (I182a; the songdb reply shape for aliases is not yet known). Only ASCII is case-folded; bytes
 * >= 0x80 pass through unchanged (UTF-8 names compare byte-exact). */
#ifndef SELECTA_LIBRARY_COMPOSER_H
#define SELECTA_LIBRARY_COMPOSER_H

#include <stddef.h>
#include <selecta/engine.h> /* SEL_COMPOSER_MAX (I488): one public definition */

/* I353: songs with no song_composers row form one group, listed last. Its key is a canon that sel_composer_canon can never
 * produce (it drops "(handle)" groups and folds punctuation), so no real composer collides with it. */
#define SEL_COMPOSER_UNKNOWN_CANON "(unknown)"
#define SEL_COMPOSER_UNKNOWN_DISPLAY "Unknown composer"

/* Canonical key of `name` (lowercase words joined by one space). Returns 1 when non-empty. */
int sel_composer_canon(const char* name, char* out, size_t cap);

/* Display name for `name`: the cleaned spelling with all-caps words capitalised. Same canon <=> same display after this call on the canonical entry. Returns 1 when non-empty. */
int sel_composer_display(const char* name, char* out, size_t cap);

#endif

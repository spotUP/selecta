/* I1387: the one display tidy-up of a song title taken from a file name or a tag, shared by the desktop, text and MUI UIs.
 * Display only: stored titles, file names, paths, search and sort keys keep their text.
 * Rule: '_' becomes ' '; runs of spaces collapse; the ends are trimmed. A title that had no space of its own (before the
 * '_' step) is also split at lower-to-upper letter boundaries, but only in a word that starts with an upper case letter and
 * has 3 or more humps ("MusicLikeThis" -> "Music Like This"). Words with 1 boundary ("ProTracker", "McDonald", "YouTube"),
 * acronyms ("DJ", "DJShadow") and a boundary right after a leading "Mc" or "Mac" are left alone. ASCII letters only; other
 * bytes (UTF-8) pass through. A result with no capital letter gets the first letter of each word capitalised. out always gets a NUL when cap > 0; a result that does not fit is cut at a character start. */
#ifndef SELECTA_UIMODEL_TITLE_H
#define SELECTA_UIMODEL_TITLE_H

#include <stddef.h>

void sel_uim_tidy_title(const char* in, char* out, size_t cap);

#endif /* SELECTA_UIMODEL_TITLE_H */

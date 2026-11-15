/* Shared half of the UHC remix sources (row I1407): AmigaRemix and Remix.Kwed.org. A UHC row is {"path": "listen/<id>",
 * "filename": "<name>.mp3", ...}; the song key is "listen/<id>/<name>.mp3" and the audio sits at <base>/<key>
 * (HippoPlayer.js, UhcSearches). Both plugins only name their searchtype, base and label. */
#ifndef SELECTA_SHARED_UHC_REMIX_H
#define SELECTA_SHARED_UHC_REMIX_H

#include <selecta/source.h>

#include <stddef.h>

/* Row callback for sel_uhc_search: ctx is a sel_search_sink*. Rows that are not "listen/<id>" + a .mp3 file name, or whose
 * name holds a control character, '|', '"', '\\', '?' or '#', are skipped (-1). */
int sel_uhc_remix_row(void* ctx, const char* dir, const char* file, const char* title, const char* artist, const char* format);

/* The sel_source.parse hook: format column `label`, display name the file name without ".mp3". 0 for a malformed key. */
int sel_uhc_remix_parse(const char* label, const char* path, char* scratch, size_t n, sel_tag* out);

/* The sel_source.play_url body: `base` (ending in '/') + the percent-encoded key. 0 when it does not fit. */
int sel_uhc_remix_url(const char* base, const char* path, char* out, size_t n);

#endif

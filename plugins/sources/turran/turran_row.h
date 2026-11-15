/* Row handling of the `turran` source (row I1407): the Turran FTP index of the UHC service. A UHC row is {"path":
 * "Collection/Music/...", "filename": "<name>.<ext>", ...}; the song key is "<path>/<filename>" and the file sits at
 * https://uhc.megaburken.net/turran/<key> (HippoPlayer.js, UhcSearches; the base is the "turran" row of src/library/net/mirrors.c).
 * Many rows are archives, images or directories: only a file name whose extension is in the player's extension table
 * (sel_platform_ext_known) becomes a row. */
#ifndef SELECTA_TURRAN_ROW_H
#define SELECTA_TURRAN_ROW_H

#include <selecta/source.h>

#include <stddef.h>

/* Row callback for sel_uhc_search: ctx is a sel_search_sink*. -1 (skipped) for a row outside "Collection/", without a known
 * extension, or whose key holds "..", a control character, '|', '"', '\\', '?' (the index shows non-ASCII as '?') or '#'. */
int sel_turran_row(void* ctx, const char* dir, const char* file, const char* title, const char* artist, const char* format);

/* The sel_source.parse hook: format column "Turran FTP", display name the file name. 0 for a malformed key. */
int sel_turran_parse(const char* path, char* scratch, size_t n, sel_tag* out);

#endif

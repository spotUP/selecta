/* Row handling of the `amp` source (row I1407): AMP, Amiga Music Preservation (amp.dascene.net), through the UHC index. A UHC
 * "ampsearch" row is {"filename": "downmod.php?index=N&application=AMP", "destfilename": "MOD.name.gz", "author": ...}; the
 * song key is "<N>/<name>" (name = destfilename without ".gz") and the module is fetched, already unpacked, from the
 * hippoplayer.se proxy https://hippoplayer.se/cygnus/api/amp/<key> (HippoPlayer_static.js, HippoPlayer_AmpSearchEntry).
 * AMP names carry the Amiga prefix ("MOD.", "XM."): only a name whose prefix is in the player's extension table is a row. */
#ifndef SELECTA_AMP_ROW_H
#define SELECTA_AMP_ROW_H

#include <selecta/source.h>

#include <stddef.h>

/* Row callback for sel_uhc_search (dir = destfilename, file = the downmod filename): ctx is a sel_search_sink*. -1 (skipped) for
 * a row without an index, with an unknown prefix, or whose name holds '/', '\\', '|', '"', '?', '#' or a control character. */
int sel_amp_row(void* ctx, const char* dir, const char* file, const char* title, const char* artist, const char* format);

/* The sel_source.parse hook: format column "AMP", display name the module name. 0 for a malformed key. */
int sel_amp_parse(const char* path, char* scratch, size_t n, sel_tag* out);

/* The sel_source.play_url body: the proxy base + the percent-encoded key. 0 when it does not fit or the key is malformed. */
int sel_amp_url(const char* path, char* out, size_t n);

#endif

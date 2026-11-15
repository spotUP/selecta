/* The UHC search client (row I1407): the JSON index HippoPlayer searches, GET <base>/uhcsearch?format=json&searchtype=<type>&
 * searchlimit=<n>&searchwords=<q>. One client for every UHC-backed source (modulespl, amigaremix, rko); a plugin supplies the
 * searchtype and maps a row to its song key. */
#ifndef SELECTA_SHARED_UHC_H
#define SELECTA_SHARED_UHC_H

#include <selecta/host.h>

#include <stdio.h>

/* One UHC row. `dir` is the row's "path" field (AMP rows: "destfilename"), `file` its "filename"; `artist` is "author(s)", "artist(s)" or "author"; fields the
 * answer lacks are "". Return 1 for a row taken, -1 for a row skipped (not counted), 0 to stop. */
typedef int (*sel_uhc_row_fn)(void* ctx, const char* dir, const char* file, const char* title, const char* artist,
                              const char* format);

/* Reads the answer (a JSON array of objects) from `f`. Returns the rows taken (0 for the empty array), or -1 when the
 * text is not a JSON array (or the first row is malformed). */
int sel_uhc_parse(FILE* f, sel_uhc_row_fn row, void* ctx);

/* Searches `query` for `searchtype`. `cache_id` names the plugin's cache dir for the one temp file; `base_env` is the name of
 * the environment variable that replaces the base https://uhc.megaburken.net (tests), or NULL. Returns the row count, or -1
 * when the request or the JSON failed (an empty answer is 0 and final). */
int sel_uhc_search(const sel_host* host, const char* cache_id, const char* base_env, const char* searchtype, const char* query,
                   sel_uhc_row_fn row, void* ctx);

#endif

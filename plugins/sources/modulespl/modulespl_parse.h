/* Parser of the Modules.pl search result page (row I406). Pure: no I/O, no host. */
#ifndef SELECTA_MODULESPL_PARSE_H
#define SELECTA_MODULESPL_PARSE_H

#include <stddef.h>
#include <stdio.h>

/* One hit: `path` is "<author dir>/<file>.zip" (the part of the site's /modules/ tree), title and artist are plain
 * UTF-8 with the HTML entities decoded (several artists joined with " & "), format is the site's extension label
 * (mod, xm, it, ...). Return 0 to stop. */
typedef int (*sel_modulespl_row_fn)(void* ctx, const char* path, const char* title, const char* artist,
                                    const char* format);

/* Reads the page `html` (`len` bytes) and calls `row` for every result that has a download the site's own player
 * offers (rows whose format the site cannot play carry no file name and are skipped). Returns the number of rows
 * delivered. */
int sel_modulespl_parse(const char* html, size_t len, sel_modulespl_row_fn row, void* ctx);

/* Reads the UHC search answer (GET https://uhc.megaburken.net/uhcsearch?format=json&searchtype=modulessearch: a JSON
 * array of {"filename","path":"modules/<author dir>","author(s)","title","format"}) from `f` and calls `row` like the
 * HTML parser does: path is "<author dir>/<file>.zip". Rows with an unsafe path are skipped. Returns the rows
 * delivered (0 for the empty array), -1 when the text is not a JSON array. */
int sel_modulespl_parse_uhc(FILE* f, sel_modulespl_row_fn row, void* ctx);

#endif

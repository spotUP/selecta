/* Parser of the Modules.pl search result page (row I406). Pure: no I/O, no host. */
#ifndef SELECTA_MODULESPL_PARSE_H
#define SELECTA_MODULESPL_PARSE_H

#include <stddef.h>

/* One hit: `path` is "<author dir>/<file>.zip" (the part of the site's /modules/ tree), title and artist are plain
 * UTF-8 with the HTML entities decoded (several artists joined with " & "), format is the site's extension label
 * (mod, xm, it, ...). Return 0 to stop. */
typedef int (*sel_modulespl_row_fn)(void* ctx, const char* path, const char* title, const char* artist,
                                    const char* format);

/* Reads the page `html` (`len` bytes) and calls `row` for every result that has a download the site's own player
 * offers (rows whose format the site cannot play carry no file name and are skipped). Returns the number of rows
 * delivered. */
int sel_modulespl_parse(const char* html, size_t len, sel_modulespl_row_fn row, void* ctx);

#endif

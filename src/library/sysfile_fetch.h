/* Download of required system files (row I710ga3, plan thoughts/shared/plans/2026-11-06-I710ga-system-files.md
 * section 4). Tries the declared URLs in a fixed order (not sel_mirrors: the list is the plugin's, in the author's
 * order), hands every 200 body to sel_sysfile_install, and stops at the first that is accepted by its sha256. */
#ifndef SELECTA_LIBRARY_SYSFILE_FETCH_H
#define SELECTA_LIBRARY_SYSFILE_FETCH_H

#include <selecta/host.h>
#include <selecta/registry.h>

#define SEL_SYSFILE_URL_MAX 256u

enum {
    SEL_SYSF_NONE = 0,      /* poll: no request for this name */
    SEL_SYSF_RUNNING = 1,   /* poll: queued or running */
    SEL_SYSF_OK = 2,        /* the file is installed */
    SEL_SYSF_ALL_FAILED = 3, /* every URL failed on transport or status (or the build has no network) */
    SEL_SYSF_NO_MATCH = 4   /* at least one body arrived and none matched an accepted sha256 */
};

typedef struct sel_sysfile_res {
    unsigned tries;    /* URLs attempted */
    unsigned requests; /* HTTP requests made (redirects count) */
    unsigned rejected; /* bodies that arrived but matched no accepted sha256 */
    char served[SEL_SYSFILE_URL_MAX]; /* the URL that served the installed file, "" otherwise */
} sel_sysfile_res;

/* The I1076 seam: a manifest loader fills this process-wide table. `urls` is ';'-separated; replace != 0 swaps the
 * plugin's list for it, 0 puts it in front. Only URLs: the accepted sha256 list always comes from the build. The
 * strings are copied. At most 8 entries are kept. */
typedef struct sel_sysfile_override {
    const char* name;
    const char* urls;
    int replace;
} sel_sysfile_override;
void sel_sysfile_set_overrides(const sel_sysfile_override* list, unsigned n);

/* The try order for `d` into urls[0..max): env SELECTA_SYSFILE_URLS_<NAME> (NAME upper-cased, '.' and '-' as '_';
 * replace semantics) first, else the override table, else the plugin's own SYSFILE_URLS. Returns how many. */
unsigned sel_sysfile_urls(const sel_sysfile_decl* d, char urls[][SEL_SYSFILE_URL_MAX], unsigned max);

/* Runs the loop on the calling thread (the library job thread uses it; tests call it directly). Temp file
 * <config_dir>/<name>.dl is removed on every path. A host without network returns ALL_FAILED with 0 tries. */
int sel_sysfile_fetch(const sel_host* host, const char* config_dir, const sel_sysfile_decl* d, sel_sysfile_res* res);

#endif

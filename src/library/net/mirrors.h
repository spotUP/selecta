/* Mirrors transport (plan 2026-10-25-m3-library.md, I156; design doc section 9 "Transports").
 * One mirror set per source. A fetch tries the mirrors in rank order and moves to the next on
 * any failure (connect error, 3 s first-byte timeout, non-2xx status, short body); every mirror
 * is tried at most once per fetch. Rank: the home mirror (SELECTA_MIRROR) first, then mirrors
 * with a measured time to first byte, fastest first, then unmeasured mirrors in list order, and
 * mirrors that failed last. The time to first byte of every successful attempt updates the rank,
 * a failure marks the mirror, so one measurement round is one fetch.
 *
 * Amiga route (no TLS there): the only non-home mirror is the DEViLBOX endpoint
 * `<devilbox>/api/mirror/<source>/<path>` over plain HTTP.
 *
 * Environment (the test seam, like SELECTA_NET_MAP): SELECTA_MIRROR=<base url> home mirror,
 * SELECTA_MIRRORS_<SOURCE>=<base>,<base>,... replaces the built-in list for that source
 * (SOURCE upper case), SELECTA_DEVILBOX_URL replaces the DEViLBOX base. */
#ifndef SELECTA_LIBRARY_NET_MIRRORS_H
#define SELECTA_LIBRARY_NET_MIRRORS_H

#include <stddef.h>

#define SEL_MIRROR_MAX 8
#define SEL_MIRROR_URL_MAX 256
#define SEL_MIRROR_PATH_MAX 512

typedef struct sel_mirror_set {
    char source[24];
    char base[SEL_MIRROR_MAX][SEL_MIRROR_URL_MAX]; /* each ends with '/' */
    unsigned ttfb_ms[SEL_MIRROR_MAX];              /* 0 until measured */
    unsigned failures[SEL_MIRROR_MAX];
    unsigned home;                                 /* 1 when base[0] is the home mirror */
    unsigned count;
} sel_mirror_set;

typedef struct sel_mirror_res {
    unsigned tries;   /* mirrors attempted */
    unsigned retries; /* attempts after the first */
    unsigned rejected; /* complete bodies the verify callback refused (md5 mismatch) */
    int served;       /* index into the set, -1 when every mirror failed */
    char url[SEL_MIRROR_URL_MAX + SEL_MIRROR_PATH_MAX];
    unsigned long body_bytes;
    unsigned order[SEL_MIRROR_MAX]; /* the rank order used, indices into the set */
} sel_mirror_res;

/* Body bytes; return 0 to abort. restart is called before every attempt after the first so the
 * sink can discard a partial body (may be NULL when the sink is idempotent). */
typedef int (*sel_mirror_sink)(void* ctx, const void* data, size_t len);
typedef void (*sel_mirror_restart)(void* ctx);
/* Called after a mirror delivered a complete body; return 0 to reject it (the body counts as a
 * failure of that mirror and the next one is tried; used for the md5 check of the cache). */
typedef int (*sel_mirror_verify)(void* ctx);

/* Build the set for source ("modland", "hvsc"). amiga_route selects the DEViLBOX route.
 * Returns the mirror count (0 for an unknown source without an env list). */
unsigned sel_mirrors_init(sel_mirror_set* set, const char* source, int amiga_route);
/* Fills order[set->count] with the rank order for the current measurements. */
void sel_mirrors_rank(const sel_mirror_set* set, unsigned* order);
/* Fetch <path> (no leading slash). Returns 1 when a mirror served it, 0 when all failed. */
int sel_mirrors_fetch(sel_mirror_set* set, const char* path, sel_mirror_sink sink, sel_mirror_restart restart,
                      void* ctx, sel_mirror_res* res);

/* Same, with a verify step after every complete body (NULL: none). */
int sel_mirrors_fetch_verified(sel_mirror_set* set, const char* path, sel_mirror_sink sink, sel_mirror_restart restart,
                               sel_mirror_verify verify, void* ctx, sel_mirror_res* res);

#endif

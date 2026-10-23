/* File cache (plan 2026-10-25-m3-library.md Phase 5, I165, I166; design doc section 9.3).
 * Layout `<cache dir>/files/<source id>/<path>`. A fetch goes through the Mirrors transport
 * (net/mirrors.h), writes to `<file>.part`, checks the md5 when the catalogue knows it and renames
 * into place. A body whose md5 differs is rejected like any failed mirror: the next mirror is
 * tried (stats.md5_mismatches counts them); when every mirror failed on md5 the result is
 * SEL_CACHE_MD5, else SEL_CACHE_FAILED. A file already in the cache makes no request.
 *
 * Companions (I166): replayers declare SEL_RP_COMPANIONS, a STR of rules separated by ';'. A rule
 * is `<pattern>><companion>` over the base name, where `*` stands for the part of the name the
 * pattern leaves over: `mdat.*>smpl.*` maps `mdat.song` to `smpl.song`, `*.tfx>*.sam` maps
 * `a.tfx` to `a.sam`. Matching ignores case on the pattern; the companion keeps the directory of
 * the song and is spelled as the rule gives it. */
#ifndef SELECTA_LIBRARY_CACHE_H
#define SELECTA_LIBRARY_CACHE_H

#include "net/mirrors.h"

#include <selecta/tags.h>

#include <stddef.h>
#include <stdint.h>

#define SEL_CACHE_DIR_MAX 512
#define SEL_CACHE_PATH_MAX 1100
#define SEL_CACHE_SETS 4
#define SEL_CACHE_COMPANIONS_MAX 8

typedef enum sel_cache_result {
    SEL_CACHE_OK = 1,
    SEL_CACHE_FAILED = 0,
    SEL_CACHE_MD5 = -1
} sel_cache_result;

typedef struct sel_cache_stats {
    unsigned cache_hits;      /* files served from the cache without a request */
    unsigned fetches;         /* files fetched through the transport and stored */
    unsigned companions;      /* companion files delivered (fetched or already cached) */
    unsigned md5_mismatches;  /* bodies rejected for a wrong md5 */
    unsigned mirror_tries;    /* transport attempts, all fetches */
    unsigned trim_runs;       /* sel_cache_trim calls (I167 sentinel) */
    unsigned evicted;         /* files removed by trims */
    unsigned prefetches;      /* prefetch requests finished (I168) */
    unsigned prefetch_fetches; /* of those, songs the prefetch downloaded (not already cached) */
} sel_cache_stats;

typedef struct sel_cache {
    char dir[SEL_CACHE_DIR_MAX];
    int amiga_route;
    sel_cache_stats stats;
    sel_mirror_set sets[SEL_CACHE_SETS];
    unsigned set_count;
} sel_cache;

/* dir NULL or empty: $SELECTA_CACHE_DIR, else $XDG_CACHE_HOME/selecta, else $HOME/.cache/selecta. */
void sel_cache_init(sel_cache* c, const char* dir, int amiga_route);
/* Full path of source:path inside the cache. 0 when it does not fit or the path climbs out. */
int sel_cache_path(const sel_cache* c, const char* source, const char* path, char* out, size_t cap);
/* Fetch one file; md5 is 32 hex characters or NULL. out receives the cache path. */
sel_cache_result sel_cache_fetch(sel_cache* c, const char* source, const char* path, const char* md5, char* out,
                                 size_t cap);
/* Companion paths of `path` under `rules`; returns how many were written (at most max). */
unsigned sel_companion_paths(const char* rules, const char* path, char out[][SEL_CACHE_PATH_MAX], unsigned max);
/* The SEL_RP_COMPANIONS rules of every registered replayer, joined with ';'. */
void sel_companion_rules(uint32_t (*registry_count)(void), const sel_tag* (*registry_get)(uint32_t), char* out,
                         size_t cap);
/* The song and its companions. Companions the mirrors lack are skipped (a song may work without
 * its sample set); the song itself decides the result. */
sel_cache_result sel_cache_fetch_song(sel_cache* c, const char* source, const char* path, const char* md5,
                                      const char* rules, char* out, size_t cap);

/* ---- trim (I167, design doc section 9.3) ---------------------------------------------------------
 * The cache is trimmed to a byte budget by last access: every hit and every insert sets the file's
 * modification time to now, and the trim removes the oldest files first until the total fits.
 * Files the keep callback calls exempt (kept and liked songs) and the file named by `protect` are
 * never removed, so the total can stay above the budget. The callback gets the song key
 * `<source>:<path>`. */
typedef int (*sel_cache_keep_fn)(void* ctx, const char* key);

typedef struct sel_cache_trim_result {
    uint32_t bytes_before;
    uint32_t bytes_after;
    unsigned trimmed;      /* files removed */
    unsigned kept_exempt;  /* files in the cache the keep callback exempted */
    unsigned companion_exempt; /* further files exempt as companions of an exempt song (I167a) */
} sel_cache_trim_result;

/* 1 when a queue path is a remote song key `<source>:<path>` of a source with mirrors (modland, hvsc). */
int sel_cache_is_remote(const char* key);

/* Default budget in bytes for a machine: scaled by the fast RAM tiers the queue also uses
 * (below 8 MB, below 64 MB, more). The disk tag carries no size yet, so RAM decides. */
uint32_t sel_cache_budget(const sel_tag* machine);
/* Trim the cache under c->dir to `budget` bytes. keep, protect and rules may be NULL. `rules` are the
 * SEL_RP_COMPANIONS rules (sel_companion_rules): the companions of an exempt or protected song are exempt
 * too, found with sel_companion_paths, the lookup the fetch uses (I167a). */
void sel_cache_trim(sel_cache* c, uint32_t budget, sel_cache_keep_fn keep, void* ctx, const char* protect,
                    const char* rules, sel_cache_trim_result* out);

#endif

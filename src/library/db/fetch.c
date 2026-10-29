/* LIBRARY_FETCH on the library handle (I166a): `<source>:<path>[#md5=<hex>]` through the file cache
 * and the Mirrors transport, then the companion files the replayers declare. The handle owns the
 * cache (and with it the mirror sets and the cache dir), created on the first fetch. */
#include "db_internal.h"

#include <stdio.h>
#include <string.h>

static sel_cache* cache_get(sel_library* lib) {
    if (lib->cache == NULL) {
        lib->cache = lib->host->mem_alloc(lib->host, sizeof *lib->cache, SEL_MEM_FAST);
        if (lib->cache != NULL) {
            memset(lib->cache, 0, sizeof *lib->cache);
            sel_cache_init(lib->cache, NULL, sel_net_amiga_route());
        }
    }
    return lib->cache;
}

/* Runs one prefetch request on `c` (sel_cache_fetch_song, the code LIBRARY_FETCH uses: no second downloader).
 * Returns 1 when the request named a valid song and is finished, 2 when a sliced fetch (c->slice_ms) used its time
 * budget and the request goes on with the next call (I168b), 0 for an invalid request. The stats of `c` moved by the work done. */
/* One tick of the cooperative prefetch downloads for at most this long (milliseconds of body). */
#define SEL_PREFETCH_SLICE_MS 100u

static int prefetch_run(sel_cache* c, const char* spec, const char* rules) {
    char path[SEL_CACHE_PATH_MAX];
    char out[SEL_CACHE_PATH_MAX];
    char source[24];
    const char* colon = strchr(spec, ':');

    if (colon == NULL || colon == spec || (size_t)(colon - spec) >= sizeof source || strlen(colon + 1) >= sizeof path) {
        return 0;
    }
    memcpy(source, spec, (size_t)(colon - spec));
    source[colon - spec] = '\0';
    snprintf(path, sizeof path, "%s", colon + 1);
    return sel_cache_fetch_song(c, source, path, NULL, rules, out, sizeof out) == SEL_CACHE_PENDING ? 2 : 1;
}

/* Smart downloads respect the LRU budget (I169): one is not started once the cache already holds the budget. The
 * measurement is the trim walk with an unreachable budget, which removes nothing. */
static int smart_over_budget(sel_cache* c, uint32_t budget) {
    sel_cache_trim_result r;

    if (budget == 0) {
        return 0;
    }
    sel_cache_trim(c, 0xFFFFFFFFu, NULL, NULL, NULL, NULL, &r);
    return r.bytes_before >= budget;
}

#if !defined(SEL_LIBRARY_NO_THREAD)
static void cache_lock(sel_library* lib) {
    if (lib->job_started) {
        pthread_mutex_lock(&lib->cache_lock);
    }
}

static void cache_unlock(sel_library* lib) {
    if (lib->job_started) {
        pthread_mutex_unlock(&lib->cache_lock);
    }
}

static void stats_add(sel_cache_stats* to, const sel_cache_stats* from, int smart) {
    to->cache_hits += from->cache_hits;
    to->fetches += from->fetches;
    to->companions += from->companions;
    to->md5_mismatches += from->md5_mismatches;
    to->mirror_tries += from->mirror_tries;
    if (smart) {
        to->smart_requests++;
        if (from->fetches != 0) {
            to->smart_fetches++;
        }
    } else {
        to->prefetches++;
        if (from->fetches != 0) {
            to->prefetch_fetches++;
        }
    }
}

/* A foreground fetch never waits behind a prefetch (I168a): the pending request is dropped and a download in
 * flight is told to stop at its next chunk. The two do not share a lock across a transfer: the prefetch thread
 * works on its own sel_cache handle (same directory, own mirror ranking and stats, own ".pf.part" temporary), and
 * only merges its stats into the shared one under cache_lock when the request is over. */
static void prefetch_preempt(sel_library* lib) {
    if (lib->job_started) {
        pthread_mutex_lock(&lib->job_lock);
        lib->pf_spec[0] = '\0';
        lib->sm_n = 0; /* smart downloads are dropped too (I169) */
        lib->pf_cancel = 1;
        pthread_mutex_unlock(&lib->job_lock);
    }
}

/* The prefetch job (I168, run by the job thread of job.c): takes the pending request, fetches the song and its
 * companions, one request at a time. The trim and the kept query stay on the caller thread, so the next foreground
 * fetch enforces the budget. Called with job_lock held; drops it around the transfer. */
int sel_fetch_job_pending(sel_library* lib) {
    return lib->pf_spec[0] != '\0' || lib->sm_n != 0;
}

void sel_fetch_job_run(sel_library* lib) {
    char spec[sizeof lib->pf_spec];
    char rules[sizeof lib->pf_rules];
    int smart = lib->pf_spec[0] == '\0'; /* a prefetch request always goes first (I169) */

    if (smart) {
        memcpy(spec, lib->sm_spec[0], sizeof spec);
        memmove(lib->sm_spec[0], lib->sm_spec[1], sizeof lib->sm_spec[0] * (SEL_SMART_MAX - 1u));
        lib->sm_n--;
    } else {
        memcpy(spec, lib->pf_spec, sizeof spec);
        lib->pf_spec[0] = '\0';
    }
    memcpy(rules, lib->pf_rules, sizeof rules);
    lib->pf_cancel = 0;
    lib->pf_busy = 1;
    lib->pf_busy_smart = (unsigned)smart;
    pthread_mutex_unlock(&lib->job_lock);
    memcpy(lib->pf_cache->dir, lib->cache->dir, sizeof lib->pf_cache->dir);
    memset(&lib->pf_cache->stats, 0, sizeof lib->pf_cache->stats);
    if (smart && smart_over_budget(lib->pf_cache, lib->cache_budget)) {
        pthread_mutex_lock(&lib->cache_lock);
        lib->cache->stats.smart_skipped++;
        pthread_mutex_unlock(&lib->cache_lock);
    } else if (prefetch_run(lib->pf_cache, spec, rules)) {
        pthread_mutex_lock(&lib->cache_lock);
        stats_add(&lib->cache->stats, &lib->pf_cache->stats, smart);
        pthread_mutex_unlock(&lib->cache_lock);
    }
    pthread_mutex_lock(&lib->job_lock);
    lib->pf_busy = 0;
    pthread_cond_broadcast(&lib->job_wake);
}

static int thread_start(sel_library* lib) {
    if (lib->pf_cache == NULL) {
        lib->pf_cache = lib->host->mem_alloc(lib->host, sizeof *lib->pf_cache, SEL_MEM_FAST);
        if (lib->pf_cache == NULL) {
            return 0;
        }
        memset(lib->pf_cache, 0, sizeof *lib->pf_cache);
        sel_cache_init(lib->pf_cache, lib->cache->dir, lib->cache->amiga_route);
        lib->pf_cache->part_tag = ".pf";
        lib->pf_cache->cancel = &lib->pf_cancel;
    }
    return sel_job_start(lib);
}

int sel_library_prefetch(sel_library* lib, const char* spec, const char* rules) {
    if (lib == NULL || spec == NULL || spec[0] == '\0' || strlen(spec) >= sizeof lib->pf_spec || cache_get(lib) == NULL ||
        !thread_start(lib)) {
        return 0;
    }
    pthread_mutex_lock(&lib->job_lock);
    if (lib->pf_busy && lib->pf_busy_smart) {
        lib->pf_cancel = 1; /* a smart download yields to the prefetch */
    }
    snprintf(lib->pf_spec, sizeof lib->pf_spec, "%s", spec);
    snprintf(lib->pf_rules, sizeof lib->pf_rules, "%s", rules != NULL ? rules : "");
    sel_job_wake(lib);
    pthread_mutex_unlock(&lib->job_lock);
    return 1;
}

static void smart_enqueue(sel_library* lib, char cand[][160], unsigned n, const char* rules) {
    unsigned i;

    if (!thread_start(lib)) {
        return;
    }
    pthread_mutex_lock(&lib->job_lock);
    for (i = 0; i < n; i++) {
        memcpy(lib->sm_spec[i], cand[i], sizeof lib->sm_spec[i]);
    }
    lib->sm_n = n;
    snprintf(lib->pf_rules, sizeof lib->pf_rules, "%s", rules != NULL ? rules : "");
    pthread_cond_broadcast(&lib->job_wake);
    pthread_mutex_unlock(&lib->job_lock);
}

void sel_library_prefetch_cancel(sel_library* lib) {
    if (lib != NULL) {
        prefetch_preempt(lib); /* the pending request is dropped and a running download stops */
    }
}

void sel_library_prefetch_wait(sel_library* lib) {
    /* Only the fetch jobs: a rating, vote or lyrics push stuck on the network must not hold the caller (the script
     * end waits here before close, and close bounds a stuck push, I173f). */
    if (lib != NULL && lib->job_started) {
        pthread_mutex_lock(&lib->job_lock);
        while (sel_fetch_job_pending(lib) || lib->pf_busy) {
            pthread_cond_wait(&lib->job_wake, &lib->job_lock);
        }
        pthread_mutex_unlock(&lib->job_lock);
    }
}

void sel_library_prefetch_stop(sel_library* lib) {
    sel_job_stop(lib); /* stops the whole job thread: prefetch, smart downloads and rating flush */
}

int sel_library_prefetch_poll(sel_library* lib) {
    (void)lib;
    return 0;
}
#else
static void cache_lock(sel_library* lib) {
    (void)lib;
}

static void cache_unlock(sel_library* lib) {
    (void)lib;
}

/* A foreground fetch drops the queued requests and the one in progress, but not the partial download: the
 * foreground fetch of the same file adopts it (resumes after the bytes already on disk), a fetch of another file
 * discards it (sel_cache_fetch, I168b). */
static void prefetch_preempt(sel_library* lib) {
    lib->pf_spec[0] = '\0';
    lib->pf_cur[0] = '\0';
    lib->sm_n = 0;
}

/* No threads (Amiga build): the request is parked and run cooperatively by sel_library_prefetch_poll, called
 * from the engine's tick right after playback started. Each call transfers for at most SEL_PREFETCH_SLICE_MS of
 * body (and SEL_CACHE_SLICE_STALL_MS before the first byte) and resumes with a Range request on the next call, so
 * the pump is never blocked for a whole download. */
int sel_library_prefetch(sel_library* lib, const char* spec, const char* rules) {
    if (lib == NULL || spec == NULL || spec[0] == '\0' || strlen(spec) >= sizeof lib->pf_spec || cache_get(lib) == NULL) {
        return 0;
    }
    lib->pf_cur[0] = '\0'; /* a newer request supersedes the one in progress */
    snprintf(lib->pf_spec, sizeof lib->pf_spec, "%s", spec);
    snprintf(lib->pf_rules, sizeof lib->pf_rules, "%s", rules != NULL ? rules : "");
    return 1;
}

void sel_library_prefetch_cancel(sel_library* lib) {
    if (lib != NULL) {
        prefetch_preempt(lib);
    }
}

static void smart_enqueue(sel_library* lib, char cand[][160], unsigned n, const char* rules) {
    unsigned i;

    for (i = 0; i < n; i++) {
        memcpy(lib->sm_spec[i], cand[i], sizeof lib->sm_spec[i]);
    }
    lib->sm_n = n;
    snprintf(lib->pf_rules, sizeof lib->pf_rules, "%s", rules != NULL ? rules : "");
}

int sel_library_prefetch_poll(sel_library* lib) {
    char rules[sizeof lib->pf_rules];
    int st;

    if (lib == NULL || lib->cache == NULL) {
        return 0;
    }
    if (lib->pf_cur[0] == '\0') {
        if (lib->pf_spec[0] == '\0' && lib->sm_n == 0) {
            return 0;
        }
        lib->pf_cur_smart = lib->pf_spec[0] == '\0';
        if (lib->pf_cur_smart) {
            memcpy(lib->pf_cur, lib->sm_spec[0], sizeof lib->pf_cur);
            memmove(lib->sm_spec[0], lib->sm_spec[1], sizeof lib->sm_spec[0] * (SEL_SMART_MAX - 1u));
            lib->sm_n--;
        } else {
            memcpy(lib->pf_cur, lib->pf_spec, sizeof lib->pf_cur);
            lib->pf_spec[0] = '\0';
        }
        if (lib->pf_cur_smart && smart_over_budget(lib->cache, lib->cache_budget)) {
            lib->cache->stats.smart_skipped++;
            lib->pf_cur[0] = '\0';
            return 1;
        }
        lib->pf_cur_before = lib->cache->stats.fetches;
    }
    memcpy(rules, lib->pf_rules, sizeof rules);
    lib->cache->slice_ms = SEL_PREFETCH_SLICE_MS;
    st = prefetch_run(lib->cache, lib->pf_cur, rules);
    lib->cache->slice_ms = 0;
    if (st == 2) {
        return 1; /* more to download: the next tick resumes */
    }
    if (st == 1) {
        if (lib->pf_cur_smart) {
            lib->cache->stats.smart_requests++;
        } else {
            lib->cache->stats.prefetches++;
        }
        if (lib->cache->stats.fetches != lib->pf_cur_before) {
            if (lib->pf_cur_smart) {
                lib->cache->stats.smart_fetches++;
            } else {
                lib->cache->stats.prefetch_fetches++;
            }
        }
    }
    lib->pf_cur[0] = '\0';
    return 1;
}

void sel_library_prefetch_wait(sel_library* lib) {
    while (sel_library_prefetch_poll(lib)) {
    }
}

void sel_library_prefetch_stop(sel_library* lib) {
    if (lib != NULL) {
        lib->pf_spec[0] = '\0';
        lib->pf_cur[0] = '\0';
        lib->sm_n = 0;
        if (lib->cache != NULL) {
            sel_cache_slice_drop(lib->cache); /* close: nothing is left to resume */
        }
    }
}
#endif

/* A candidate: a remote song key that is not in the cache yet and not already picked. */
static void smart_add(sel_library* lib, char cand[][160], unsigned* n, unsigned top, const char* key) {
    char path[SEL_CACHE_PATH_MAX];
    char source[24];
    const char* colon = strchr(key, ':');
    FILE* f;
    unsigned i;

    if (*n >= top || colon == NULL || colon == key || (size_t)(colon - key) >= sizeof source || strlen(key) >= 160u ||
        !sel_cache_is_remote(key)) {
        return;
    }
    memcpy(source, key, (size_t)(colon - key));
    source[colon - key] = '\0';
    for (i = 0; i < *n; i++) {
        if (strcmp(cand[i], key) == 0) {
            return;
        }
    }
    if (sel_cache_path(lib->cache, source, colon + 1, path, sizeof path) && (f = fopen(path, "rb")) != NULL) {
        fclose(f);
        return;
    }
    snprintf(cand[*n], 160u, "%s", key);
    (*n)++;
}

unsigned sel_library_smart(sel_library* lib, const char* view, unsigned top, const char* rules) {
    char cand[SEL_SMART_MAX][160];
    char out[2048];
    unsigned n = 0;
    sqlite3_stmt* st = NULL;

    if (lib == NULL || top == 0 || cache_get(lib) == NULL) {
        return 0;
    }
    if (top > SEL_SMART_MAX) {
        top = SEL_SMART_MAX;
    }
    if (view != NULL && view[0] != '\0') { /* the smart playlist: a saved view, its songs in key order */
        char text[SEL_QUERY_TEXT_MAX];
        sel_lib_job job;
        const char* k;

        memset(&job, 0, sizeof job);
        snprintf(text, sizeof text, "view:%s", view);
        job.text = text;
        job.out = out;
        job.out_cap = sizeof out;
        if (sel_library_run(lib, SEL_LIB_REQ_SEARCH, &job) && job.error[0] == '\0') {
            uint32_t i;
            for (i = 0, k = out; i < job.count; i++, k += strlen(k) + 1u) {
                smart_add(lib, cand, &n, top, k);
            }
        }
    }
    if (sqlite3_prepare_v2(lib->db,
                           "SELECT key FROM history WHERE skipped = 0 GROUP BY key ORDER BY COUNT(*) DESC, "
                           "MAX(played_at) DESC, key LIMIT 32",
                           -1, &st, NULL) == SQLITE_OK) {
        while (n < top && sqlite3_step(st) == SQLITE_ROW) {
            smart_add(lib, cand, &n, top, (const char*)sqlite3_column_text(st, 0));
        }
    }
    sqlite3_finalize(st);
    lib->sm_planned++;
    if (n != 0) {
        smart_enqueue(lib, cand, n, rules);
    }
    return n;
}

static int is_kept(void* ctx, const char* key) {
    sqlite3_stmt* st = ctx;
    int found;

    sqlite3_reset(st);
    sqlite3_bind_text(st, 1, key, -1, SQLITE_STATIC);
    found = sqlite3_step(st) == SQLITE_ROW;
    sqlite3_reset(st);
    return found;
}

/* The one trim: budget 0 means the machine default; `protect` is the file just stored. */
static unsigned library_trim_protect(sel_library* lib, uint32_t budget, const char* protect,
                                     sel_cache_trim_result* out) {
    sel_cache* c = cache_get(lib);
    sqlite3_stmt* st = NULL;

    if (c == NULL) {
        return 0;
    }
    sqlite3_prepare_v2(lib->db,
                       "SELECT 1 FROM kept WHERE key = ?1 UNION ALL SELECT 1 FROM liked WHERE key = ?1 LIMIT 1", -1, &st,
                       NULL);
    sel_cache_trim(c, budget != 0 ? budget : lib->cache_budget, st != NULL ? is_kept : NULL, st, protect, lib->rules, out);
    sqlite3_finalize(st);
    return c->stats.trim_runs;
}

unsigned sel_library_cache_trim(sel_library* lib, uint32_t budget, sel_cache_trim_result* out) {
    return library_trim_protect(lib, budget, NULL, out);
}

void sel_library_set_rules(sel_library* lib, const char* rules) {
    snprintf(lib->rules, sizeof lib->rules, "%s", rules != NULL ? rules : "");
}

void sel_library_set_budget(sel_library* lib, uint32_t budget) {
    lib->cache_budget = budget;
}

int sel_library_mark(sel_library* lib, const char* table, const char* key) {
    sqlite3_stmt* st = NULL;
    char sql[96];
    int ok;

    if (strcmp(table, "kept") != 0 && strcmp(table, "liked") != 0) {
        return 0;
    }
    snprintf(sql, sizeof sql, "INSERT OR REPLACE INTO %s(key, at) VALUES(?1, 0)", table);
    if (sqlite3_prepare_v2(lib->db, sql, -1, &st, NULL) != SQLITE_OK) {
        return 0;
    }
    sqlite3_bind_text(st, 1, key, -1, SQLITE_STATIC);
    ok = sqlite3_step(st) == SQLITE_DONE;
    sqlite3_finalize(st);
    return ok;
}

int sel_library_fetch_status(sel_library* lib) {
    sel_cache* c = cache_get(lib);
    int st;

    if (c == NULL) {
        return 0;
    }
    cache_lock(lib);
    st = c->last_status;
    cache_unlock(lib);
    return st;
}

int sel_library_fetch(sel_library* lib, const char* spec, const char* rules, char* out, size_t cap, uint32_t* files) {
    char source[24];
    char path[SEL_CACHE_PATH_MAX];
    char md5[33];
    const char* colon = strchr(spec, ':');
    const char* tag = strstr(spec, "#md5=");
    sel_cache* c = cache_get(lib);
    size_t plen;
    unsigned before, fetched;
    sel_cache_result r;

    if (c == NULL || colon == NULL || colon == spec || (size_t)(colon - spec) >= sizeof source) {
        return 0;
    }
    memcpy(source, spec, (size_t)(colon - spec));
    source[colon - spec] = '\0';
    plen = tag != NULL ? (size_t)(tag - colon - 1) : strlen(colon + 1);
    if (plen == 0 || plen >= sizeof path) {
        return 0;
    }
    memcpy(path, colon + 1, plen);
    path[plen] = '\0';
    md5[0] = '\0';
    if (tag != NULL) {
        snprintf(md5, sizeof md5, "%s", tag + 5);
    }
    lib->fetch_requests++;
    prefetch_preempt(lib);
    cache_lock(lib);
    if (rules != NULL) {
        sel_library_set_rules(lib, rules);
    }
    before = c->stats.companions;
    fetched = c->stats.fetches;
    r = sel_cache_fetch_song(c, source, path, md5[0] != '\0' ? md5 : NULL, rules, out, cap);
    if (r == SEL_CACHE_OK && c->stats.fetches != fetched) {
        sel_cache_trim_result tr;
        library_trim_protect(lib, 0, out, &tr);
    }
    *files = 1u + (c->stats.companions - before);
    cache_unlock(lib);
    if (r != SEL_CACHE_OK) {
        return r == SEL_CACHE_MD5 ? -1 : 0;
    }
    return 1;
}

int sel_library_cache_stats(const sel_library* lib, sel_cache_stats* out) {
    if (lib == NULL || lib->cache == NULL || (lib->fetch_requests == 0 && lib->sm_planned == 0)) {
        return 0;
    }
    *out = lib->cache->stats;
    return 1;
}

void sel_library_cache_report(const sel_library* lib) {
    sel_cache_stats s;

    if (sel_library_cache_stats(lib, &s)) {
        printf("cache_hits=%u\nfetches=%u\ncompanions=%u\nmd5_mismatches=%u\nmirror_tries=%u\n"
               "trim_runs=%u\nprefetches=%u\nprefetch_fetches=%u\nsmart_requests=%u\nsmart_fetches=%u\n"
               "smart_skipped=%u\nzip_members=%u\nzip_requests=%u\n",
               s.cache_hits, s.fetches, s.companions, s.md5_mismatches, s.mirror_tries, s.trim_runs, s.prefetches,
               s.prefetch_fetches, s.smart_requests, s.smart_fetches, s.smart_skipped, s.zip_members, s.zip_requests);
    }
}

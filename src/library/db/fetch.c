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
            sel_cache_init(lib->cache, NULL, 0);
        }
    }
    return lib->cache;
}

#if !defined(SEL_LIBRARY_NO_THREAD)
static void cache_lock(sel_library* lib) {
    if (lib->pf_started) {
        pthread_mutex_lock(&lib->cache_lock);
    }
}

static void cache_unlock(sel_library* lib) {
    if (lib->pf_started) {
        pthread_mutex_unlock(&lib->cache_lock);
    }
}

/* The prefetch thread (I168): takes the pending request, fetches the song and its companions through the
 * same sel_cache_fetch_song as LIBRARY_FETCH (no second downloader), one request at a time. The trim and
 * the kept query stay on the caller thread, so the next foreground fetch enforces the budget. */
static void* prefetch_main(void* arg) {
    sel_library* lib = arg;

    pthread_mutex_lock(&lib->pf_lock);
    for (;;) {
        char spec[sizeof lib->pf_spec];
        char rules[sizeof lib->pf_rules];
        char path[SEL_CACHE_PATH_MAX];
        char out[SEL_CACHE_PATH_MAX];
        const char* colon;
        unsigned before;

        while (!lib->pf_quit && lib->pf_spec[0] == '\0') {
            pthread_cond_wait(&lib->pf_wake, &lib->pf_lock);
        }
        if (lib->pf_quit) {
            break;
        }
        memcpy(spec, lib->pf_spec, sizeof spec);
        memcpy(rules, lib->pf_rules, sizeof rules);
        lib->pf_spec[0] = '\0';
        lib->pf_busy = 1;
        pthread_mutex_unlock(&lib->pf_lock);
        colon = strchr(spec, ':');
        if (colon != NULL && colon != spec && (size_t)(colon - spec) < 24u && strlen(colon + 1) < sizeof path) {
            char source[24];

            memcpy(source, spec, (size_t)(colon - spec));
            source[colon - spec] = '\0';
            snprintf(path, sizeof path, "%s", colon + 1);
            pthread_mutex_lock(&lib->cache_lock);
            before = lib->cache->stats.fetches;
            sel_cache_fetch_song(lib->cache, source, path, NULL, rules, out, sizeof out);
            lib->cache->stats.prefetches++;
            if (lib->cache->stats.fetches != before) {
                lib->cache->stats.prefetch_fetches++;
            }
            pthread_mutex_unlock(&lib->cache_lock);
        }
        pthread_mutex_lock(&lib->pf_lock);
        lib->pf_busy = 0;
        pthread_cond_broadcast(&lib->pf_wake);
    }
    pthread_mutex_unlock(&lib->pf_lock);
    return NULL;
}

int sel_library_prefetch(sel_library* lib, const char* spec, const char* rules) {
    if (lib == NULL || spec == NULL || spec[0] == '\0' || strlen(spec) >= sizeof lib->pf_spec || cache_get(lib) == NULL) {
        return 0;
    }
    if (!lib->pf_started) {
        pthread_mutex_init(&lib->pf_lock, NULL);
        pthread_mutex_init(&lib->cache_lock, NULL);
        pthread_cond_init(&lib->pf_wake, NULL);
        if (pthread_create(&lib->pf_thread, NULL, prefetch_main, lib) != 0) {
            return 0;
        }
        lib->pf_started = 1;
    }
    pthread_mutex_lock(&lib->pf_lock);
    snprintf(lib->pf_spec, sizeof lib->pf_spec, "%s", spec);
    snprintf(lib->pf_rules, sizeof lib->pf_rules, "%s", rules != NULL ? rules : "");
    pthread_cond_broadcast(&lib->pf_wake);
    pthread_mutex_unlock(&lib->pf_lock);
    return 1;
}

void sel_library_prefetch_cancel(sel_library* lib) {
    if (lib != NULL && lib->pf_started) {
        pthread_mutex_lock(&lib->pf_lock);
        lib->pf_spec[0] = '\0'; /* a request already running finishes; its file stays cached */
        pthread_mutex_unlock(&lib->pf_lock);
    }
}

void sel_library_prefetch_wait(sel_library* lib) {
    if (lib != NULL && lib->pf_started) {
        pthread_mutex_lock(&lib->pf_lock);
        while (lib->pf_spec[0] != '\0' || lib->pf_busy) {
            pthread_cond_wait(&lib->pf_wake, &lib->pf_lock);
        }
        pthread_mutex_unlock(&lib->pf_lock);
    }
}

void sel_library_prefetch_stop(sel_library* lib) {
    if (lib != NULL && lib->pf_started) {
        pthread_mutex_lock(&lib->pf_lock);
        lib->pf_quit = 1;
        pthread_cond_broadcast(&lib->pf_wake);
        pthread_mutex_unlock(&lib->pf_lock);
        pthread_join(lib->pf_thread, NULL);
        lib->pf_started = 0;
    }
}
#else
static void cache_lock(sel_library* lib) {
    (void)lib;
}

static void cache_unlock(sel_library* lib) {
    (void)lib;
}

/* No threads (Amiga build): a prefetch would block the engine's pump, so it is not done there yet. */
int sel_library_prefetch(sel_library* lib, const char* spec, const char* rules) {
    (void)lib;
    (void)spec;
    (void)rules;
    return 0;
}

void sel_library_prefetch_cancel(sel_library* lib) {
    (void)lib;
}

void sel_library_prefetch_wait(sel_library* lib) {
    (void)lib;
}

void sel_library_prefetch_stop(sel_library* lib) {
    (void)lib;
}
#endif

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
    if (lib == NULL || lib->cache == NULL || lib->fetch_requests == 0) {
        return 0;
    }
    *out = lib->cache->stats;
    return 1;
}

void sel_library_cache_report(const sel_library* lib) {
    sel_cache_stats s;

    if (sel_library_cache_stats(lib, &s)) {
        printf("cache_hits=%u\nfetches=%u\ncompanions=%u\nmd5_mismatches=%u\nmirror_tries=%u\n"
               "trim_runs=%u\nprefetches=%u\nprefetch_fetches=%u\n",
               s.cache_hits, s.fetches, s.companions, s.md5_mismatches, s.mirror_tries, s.trim_runs, s.prefetches,
               s.prefetch_fetches);
    }
}

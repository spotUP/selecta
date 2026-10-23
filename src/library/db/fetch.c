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

int sel_library_fetch(sel_library* lib, const char* spec, const char* rules, char* out, size_t cap, uint32_t* files) {
    char source[24];
    char path[SEL_CACHE_PATH_MAX];
    char md5[33];
    const char* colon = strchr(spec, ':');
    const char* tag = strstr(spec, "#md5=");
    sel_cache* c = cache_get(lib);
    size_t plen;
    unsigned before;
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
    before = c->stats.companions;
    r = sel_cache_fetch_song(c, source, path, md5[0] != '\0' ? md5 : NULL, rules, out, cap);
    if (r != SEL_CACHE_OK) {
        return r == SEL_CACHE_MD5 ? -1 : 0;
    }
    *files = 1u + (c->stats.companions - before);
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
        printf("cache_hits=%u\nfetches=%u\ncompanions=%u\nmd5_mismatches=%u\nmirror_tries=%u\n", s.cache_hits,
               s.fetches, s.companions, s.md5_mismatches, s.mirror_tries);
    }
}

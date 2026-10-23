/* LIBRARY_FETCH hook for the CLI engine (I165, I166): `<source>:<path>[#md5=<hex>]` through the file
 * cache and the Mirrors transport, then the companion files the replayers declare in
 * SEL_RP_COMPANIONS. `script` installs it; the end of a script prints
 * `cache_hits= fetches= companions= md5_mismatches= mirror_tries=`. */
#include <selecta/registry.h>

#include "cache.h"
#include "cli.h"

#include <stdio.h>
#include <string.h>

struct sel_cli_cache {
    sel_cache cache;
    char rules[512];
    unsigned requests;
};

static int fetch_hook(void* vctx, const char* spec, char* out, size_t cap, uint32_t* files) {
    sel_cli_cache* ctx = vctx;
    char source[24];
    char path[SEL_CACHE_PATH_MAX];
    char md5[33];
    const char* colon = strchr(spec, ':');
    const char* tag = strstr(spec, "#md5=");
    size_t plen;
    unsigned before;
    sel_cache_result r;

    if (colon == NULL || colon == spec || (size_t)(colon - spec) >= sizeof source) {
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
    ctx->requests++;
    before = ctx->cache.stats.companions;
    r = sel_cache_fetch_song(&ctx->cache, source, path, md5[0] != '\0' ? md5 : NULL, ctx->rules, out, cap);
    if (r != SEL_CACHE_OK) {
        return r == SEL_CACHE_MD5 ? -1 : 0;
    }
    *files = 1u + (ctx->cache.stats.companions - before);
    return 1;
}

sel_cli_cache* sel_cli_cache_open(void) {
    static sel_cli_cache g_ctx;
    memset(&g_ctx, 0, sizeof g_ctx);
    sel_cache_init(&g_ctx.cache, NULL, 0);
    sel_companion_rules(sel_registry_count, sel_registry_get, g_ctx.rules, sizeof g_ctx.rules);
    return &g_ctx;
}

sel_library_fetch_fn sel_cli_cache_hook(void) {
    return fetch_hook;
}

void sel_cli_cache_report(const sel_cli_cache* c) {
    if (c->requests == 0) {
        return;
    }
    printf("cache_hits=%u\nfetches=%u\ncompanions=%u\nmd5_mismatches=%u\nmirror_tries=%u\n", c->cache.stats.cache_hits,
           c->cache.stats.fetches, c->cache.stats.companions, c->cache.stats.md5_mismatches,
           c->cache.stats.mirror_tries);
}

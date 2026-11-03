/* The `modulespl` source descriptor (row I406; plan 2026-10-26-online-sources.md section 5.2): the Modules.pl
 * tracker-module archive, https://www.modules.pl/. The site has no catalogue API, so this source has no sync: it is a
 * search source (SOURCE_SEARCH, D-OS-3). A search is one GET of
 *   https://www.modules.pl/?id=search&q=<url-encoded query>
 * (the site's own search form is a GET form with the hidden field id=search and the field q; the answer lists all
 * hits in one page, no paging). The hits are parsed by modulespl_parse.c. A song key is
 * `modulespl:<author dir>/<file>.zip`: the file lives at https://www.modules.pl/modules/<path> (the mirror base in
 * src/library/net/mirrors.c) and holds one module, which the engine unpacks after the fetch.
 * SELECTA_MODULESPL_SITE replaces the site base (default https://www.modules.pl; tests point it at a fixture). */
#include <selecta/catalogue.h>
#include <selecta/host.h>
#include <selecta/net.h>
#include <selecta/plugin.h>
#include <selecta/source.h>
#include "modulespl_parse.h"
#include "source/descriptor.h"
#include "source/url_path.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SITE_HOME "https://www.modules.pl/"
#define PAGE_MAX (4u * 1024u * 1024u)
/* www.modules.pl starts answering a search after 10-11 s; the client default of 3 s fails it (I816). */
#define SEARCH_FIRST_BYTE_MS 30000u

static const sel_tag g_tags[] = {
    {SEL_TAG_END, 0},
};

static const char* site_base(void) {
    const char* env = getenv("SELECTA_MODULESPL_SITE");

    return env != NULL && env[0] != '\0' ? env : "https://www.modules.pl";
}

static char* slurp(const char* path, size_t* n) {
    FILE* f = fopen(path, "rb");
    char* buf = NULL;
    long sz;

    if (f == NULL) {
        return NULL;
    }
    if (fseek(f, 0, SEEK_END) == 0 && (sz = ftell(f)) > 0 && (size_t)sz <= PAGE_MAX && fseek(f, 0, SEEK_SET) == 0) {
        buf = malloc((size_t)sz + 1u);
        if (buf != NULL && fread(buf, 1, (size_t)sz, f) == (size_t)sz) {
            buf[sz] = '\0';
            *n = (size_t)sz;
        } else {
            free(buf);
            buf = NULL;
        }
    }
    fclose(f);
    return buf;
}

static int forward(void* ctx, const char* path, const char* title, const char* artist, const char* format) {
    const sel_search_sink* out = ctx;

    return out->row(out->ctx, path, title, artist, format);
}

static int ms_search(const sel_host* host, const char* query, sel_search_sink* out) {
    const sel_net* net = host->net != NULL ? host->net(host) : NULL;
    const sel_catalogue* cat = host->catalogue != NULL ? host->catalogue(host) : NULL;
    char dir[512];
    char path[600];
    char enc[384];
    char url[900];
    uint32_t rq = 0;
    unsigned long got = 0;
    char* page;
    size_t n = 0;
    int rows;

    if (net == NULL || cat == NULL || query == NULL || query[0] == '\0' || !sel_url_query_encode(query, enc, sizeof enc) ||
        !cat->cache_dir(host, "modulespl", dir, sizeof dir)) {
        return -1;
    }
    snprintf(path, sizeof path, "%s/search.html", dir);
    snprintf(url, sizeof url, "%s/?id=search&q=%s", site_base(), enc);
    if (net->get_file_wait(host, url, path, SEARCH_FIRST_BYTE_MS, &rq, &got) != 200) {
        remove(path);
        return -1;
    }
    page = slurp(path, &n);
    remove(path);
    if (page == NULL) {
        return -1;
    }
    rows = sel_modulespl_parse(page, n, forward, out);
    free(page);
    return rows;
}

/* The file name without ".zip", underscores as spaces, is the display name; the format column names the source. */
static int parse(const char* path, char* scratch, size_t n, sel_tag* out) {
    static const char fmt[] = "Modules.pl";
    const char* name = path != NULL ? strchr(path, '/') : NULL;
    size_t nl;
    size_t i;

    if (name == NULL || name[1] == '\0') {
        return 0;
    }
    name++;
    nl = strlen(name);
    if (nl > 4u && strcmp(name + nl - 4u, ".zip") == 0) {
        nl -= 4u;
    }
    if (sizeof fmt + nl + 1u > n) {
        return 0;
    }
    memcpy(scratch, fmt, sizeof fmt);
    for (i = 0; i < nl; i++) {
        scratch[sizeof fmt + i] = name[i] == '_' ? ' ' : name[i];
    }
    scratch[sizeof fmt + nl] = '\0';
    out[0].tag = SEL_SRC_FORMAT;
    out[0].value = (uintptr_t)scratch;
    out[1].tag = SEL_SRC_NAME;
    out[1].value = (uintptr_t)(scratch + sizeof fmt);
    out[2].tag = SEL_TAG_END;
    out[2].value = 0;
    return 1;
}

static const sel_source g_modulespl = {
    "modulespl", "Modules.pl", SITE_HOME,
    "Tracker modules from the Modules.pl archive; found by searching the site, no catalogue to sync.",
    g_tags, NULL, parse, NULL, NULL, NULL, NULL, NULL, NULL, SEL_SOURCE_REMOTE | SEL_SOURCE_ZIPPED,
    NULL, NULL, ms_search,
    NULL, /* search_ready (I1037) */
};

const sel_tag* SEL_PLUGIN_ENTRY(uint32_t host_api) {
    (void)host_api;
    return sel_source_plugin_descriptor("modulespl", &g_modulespl);
}

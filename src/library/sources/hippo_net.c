/* See hippo_net.h. */
#include "hippo_net.h"

#include "sync_host.h"

#include <selecta/catalogue.h>
#include <selecta/net.h>
#include <stdio.h>
#include <string.h>

#define URL_BYTES 1100u
#define PATH_BYTES 1100u
#define LINE_BYTES 4096u

/* The JSON list is an array: its first non-space byte is '['. An HTML error page opens with '<'. */
static int json_ok(const char* path) {
    FILE* in = fopen(path, "rb");
    int c;

    if (in == NULL) {
        return 0;
    }
    do {
        c = fgetc(in);
    } while (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == 0xEF || c == 0xBB || c == 0xBF);
    fclose(in);
    return c == '[';
}

/* The TSV list has tab separated lines: the first data line holds a tab and is not markup. */
static int tsv_ok(const char* path) {
    FILE* in = fopen(path, "rb");
    char line[LINE_BYTES];
    int ok = 0;

    if (in == NULL) {
        return 0;
    }
    while (fgets(line, sizeof line, in) != NULL) {
        if (line[0] == '#' || line[0] == '\r' || line[0] == '\n') {
            continue;
        }
        ok = line[0] != '<' && strchr(line, '\t') != NULL;
        break;
    }
    fclose(in);
    return ok;
}

/* A HiPPrg body is binary and never opens with markup. */
static int prg_ok(const char* path) {
    FILE* in = fopen(path, "rb");
    int c;

    if (in == NULL) {
        return 0;
    }
    c = fgetc(in);
    fclose(in);
    return c != EOF && c != '<';
}

typedef struct prg_fetch {
    const sel_host* host;
    const sel_net* net;
    const char* base; /* without trailing '/' */
    int base_len;
    const char* dir;
    sel_hippo_net_stats* stats;
} prg_fetch;

/* GET <base>/api/v1/playlists/<filename> into <dir>/<filename>; the sync reads it from there. */
static void fetch_prg(void* user, const char* file) {
    prg_fetch* p = user;
    char url[URL_BYTES];
    char path[PATH_BYTES];
    unsigned long got = 0;

    if (snprintf(url, sizeof url, "%.*s/" SEL_HIPPO_API_PATH "/%s", p->base_len, p->base, file) >= (int)sizeof url ||
        snprintf(path, sizeof path, "%s/%s", p->dir, file) >= (int)sizeof path) {
        return;
    }
    if (p->net->get_file_checked(p->host, url, path, prg_ok, &p->stats->prg_requests, &got) == 200) {
        p->stats->prg_files++;
    }
}

int sel_hippo_net_sync(const sel_host* host, const char* base, sel_hippo_sync_stats* stats, sel_hippo_net_stats* net) {
    const sel_net* net_svc = host->net != NULL ? host->net(host) : NULL;
    const sel_catalogue* cat = host->catalogue != NULL ? host->catalogue(host) : NULL;
    char dir[PATH_BYTES - 64u];
    char json[PATH_BYTES];
    char tsv[PATH_BYTES];
    char url[URL_BYTES];
    unsigned long got = 0;
    size_t n = strlen(base);
    int have_json, have_tsv;

    memset(net, 0, sizeof *net);
    memset(stats, 0, sizeof *stats);
    while (n > 0 && base[n - 1] == '/') {
        n--;
    }
    if (n == 0 || n >= URL_BYTES - 64u) {
        return 0;
    }
    if (net_svc == NULL || cat == NULL || !cat->cache_dir(host, "hippo", dir, sizeof dir) ||
        snprintf(json, sizeof json, "%s/%s", dir, SEL_HIPPO_LIST_JSON) >= (int)sizeof json ||
        snprintf(tsv, sizeof tsv, "%s/%s", dir, SEL_HIPPO_LIST_TSV) >= (int)sizeof tsv) {
        return 0;
    }
    snprintf(url, sizeof url, "%.*s/" SEL_HIPPO_API_PATH, (int)n, base);
    have_json = net_svc->get_file_checked(host, url, json, json_ok, &net->http_requests, &got) == 200;
    net->json_bytes = have_json ? (uint32_t)got : 0u;
    snprintf(url, sizeof url, "%.*s/" SEL_HIPPO_TSV_PATH, (int)n, base);
    have_tsv = net_svc->get_file_checked(host, url, tsv, tsv_ok, &net->http_requests, &got) == 200;
    net->tsv_bytes = have_tsv ? (uint32_t)got : 0u;
    if (!have_json && !have_tsv) {
        return 0;
    }
    {
        prg_fetch p;

        p.host = host;
        p.net = net_svc;
        p.base = base;
        p.base_len = (int)n;
        p.dir = dir;
        p.stats = net;
        sel_hippo_each_file(host, dir, fetch_prg, &p);
    }
    return sel_hippo_sync(sel_sync_cfg(host), dir, stats);
}

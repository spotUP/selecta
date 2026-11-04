/* See hippo_net.h. */
#include "hippo_net.h"

#include "sync_host.h"
#include "../db/db.h"
#include "../source.h"

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

/* I1058a: finds the list record whose title is `title` and keeps its filename. */
typedef struct title_find {
    const char* title;
    char file[PATH_BYTES / 2];
    int found;
} title_find;

static int find_title(void* user, const char* file, const char* title) {
    title_find* f = user;

    if (strcmp(title, f->title) != 0 || strlen(file) >= sizeof f->file) {
        return 1;
    }
    memcpy(f->file, file, strlen(file) + 1u);
    f->found = 1;
    return 0;
}

#define OPEN_KEYS_BYTES (1024u * 1024u)
#define BASE_FILE "hippo_base.txt" /* the server the lists came from, kept next to them: a later open asks the same one */

static void base_save(const char* dir, const char* base, size_t n) {
    char path[PATH_BYTES];
    FILE* out;

    if (snprintf(path, sizeof path, "%s/%s", dir, BASE_FILE) >= (int)sizeof path || (out = fopen(path, "wb")) == NULL) {
        return;
    }
    fwrite(base, 1, n, out);
    fclose(out);
}

/* The saved server base (no trailing slash) into out; the source home when the sync saved none. */
static void base_load(const char* dir, char* out, size_t cap) {
    char path[PATH_BYTES];
    FILE* in;
    size_t n = 0;

    out[0] = '\0';
    if (snprintf(path, sizeof path, "%s/%s", dir, BASE_FILE) < (int)sizeof path && (in = fopen(path, "rb")) != NULL) {
        n = fread(out, 1, cap - 1u, in);
        fclose(in);
        out[n] = '\0';
    }
    while (n > 0 && out[n - 1] == '/') {
        out[--n] = '\0';
    }
    if (n == 0) {
        snprintf(out, cap, "%s", sel_source_hippo()->home);
        n = strlen(out);
        while (n > 0 && out[n - 1] == '/') {
            out[--n] = '\0';
        }
    }
}

int sel_hippo_open_playlist(sel_library* lib, const sel_host* base, uint32_t id) {
    sel_sync_ctx ctx;
    sel_host host;
    const sel_net* net_svc;
    const sel_catalogue* cat;
    char title[256];
    char dir[PATH_BYTES - 64u];
    char path[PATH_BYTES];
    char url[URL_BYTES];
    unsigned long got = 0;
    uint32_t requests = 0;
    title_find f;
    char* keys;
    const char* k;
    int rc = -1;

    if (!sel_hippo_playlist_title(lib, base, id, title, sizeof title)) {
        return 0; /* not a list of the HippoPlayer folder */
    }
    memset(&ctx, 0, sizeof ctx);
    ctx.scfg.host = base;
    ctx.scfg.lib = lib;
    sel_sync_host_bind(&host, base, &ctx);
    net_svc = host.net(&host);
    cat = host.catalogue(&host);
    if (net_svc == NULL || cat == NULL || !cat->cache_dir(&host, "hippo", dir, sizeof dir)) {
        return -1;
    }
    memset(&f, 0, sizeof f);
    f.title = title;
    sel_hippo_each_file(&host, dir, find_title, &f);
    if (!f.found) {
        return 0; /* a playlist the user made in that folder: nothing to fetch */
    }
    if (snprintf(path, sizeof path, "%s/%s", dir, f.file) >= (int)sizeof path) {
        return -1;
    }
    keys = base->mem_alloc(base, OPEN_KEYS_BYTES, SEL_MEM_FAST);
    if (keys == NULL) {
        return -1;
    }
    keys[0] = '\0';
    if (!sel_hippo_file_keys(base, path, keys, OPEN_KEYS_BYTES)) {
        /* GET <source url>api/v1/playlists/<filename> once; a 404 or a network failure leaves the list empty and retryable. */
        char server[URL_BYTES - 256u];

        base_load(dir, server, sizeof server);
        if (snprintf(url, sizeof url, "%s/" SEL_HIPPO_API_PATH "/%s", server, f.file) < (int)sizeof url &&
            net_svc->get_file_checked(&host, url, path, prg_ok, &requests, &got) == 200) {
            (void)sel_hippo_file_keys(base, path, keys, OPEN_KEYS_BYTES);
        }
    }
    for (k = keys; *k != '\0'; k += strlen(k) + 1u) {
        rc = 1;
        if (sel_library_playlist_add_song(lib, id, k) != SEL_FOLDER_OK) {
            rc = -1;
            break;
        }
    }
    base->mem_free(base, keys);
    return rc;
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
    base_save(dir, base, n);
    return sel_hippo_sync(sel_sync_cfg(host), dir, stats);
}

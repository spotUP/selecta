/* System-file download loop (row I710ga3). See sysfile_fetch.h. */
#include "sysfile_fetch.h"

#include "sysfile.h"
#include "sources/sync_host.h"

#include <selecta/net.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define OVR_MAX 8u
#define OVR_STR 512u
#define TRY_MAX 12u

static struct {
    char name[96];
    char urls[OVR_STR];
    int replace;
} g_ovr[OVR_MAX];
static unsigned g_ovr_n;

void sel_sysfile_set_overrides(const sel_sysfile_override* list, unsigned n) {
    unsigned i;
    unsigned k = 0;

    for (i = 0; list != NULL && i < n && k < OVR_MAX; i++) {
        if (list[i].name == NULL || list[i].urls == NULL || strlen(list[i].name) >= sizeof g_ovr[0].name ||
            strlen(list[i].urls) >= OVR_STR) {
            continue;
        }
        snprintf(g_ovr[k].name, sizeof g_ovr[k].name, "%s", list[i].name);
        snprintf(g_ovr[k].urls, OVR_STR, "%s", list[i].urls);
        g_ovr[k].replace = list[i].replace;
        k++;
    }
    g_ovr_n = k;
}

/* Appends the ';'-separated URLs of `list` to urls[*n..max). */
static void add_list(const char* list, char urls[][SEL_SYSFILE_URL_MAX], unsigned max, unsigned* n) {
    const char* p = list;

    while (p != NULL && *p != '\0' && *n < max) {
        const char* e = strchr(p, ';');
        size_t len = e != NULL ? (size_t)(e - p) : strlen(p);

        if (len > 0 && len < SEL_SYSFILE_URL_MAX) {
            memcpy(urls[*n], p, len);
            urls[*n][len] = '\0';
            (*n)++;
        }
        p += len;
        if (*p == ';') {
            p++;
        }
    }
}

unsigned sel_sysfile_urls(const sel_sysfile_decl* d, char urls[][SEL_SYSFILE_URL_MAX], unsigned max) {
    char var[128];
    const char* env;
    unsigned n = 0;
    unsigned i;
    size_t k;

    if (d == NULL || d->name == NULL || urls == NULL) {
        return 0;
    }
    if (strlen(d->name) + sizeof "SELECTA_SYSFILE_URLS_" < sizeof var) {
        snprintf(var, sizeof var, "SELECTA_SYSFILE_URLS_%s", d->name);
        for (k = sizeof "SELECTA_SYSFILE_URLS_" - 1; var[k] != '\0'; k++) {
            var[k] = (var[k] == '.' || var[k] == '-') ? '_' : (char)toupper((unsigned char)var[k]);
        }
        env = getenv(var);
        if (env != NULL && env[0] != '\0') {
            add_list(env, urls, max, &n);
            return n;
        }
    }
    for (i = 0; i < g_ovr_n; i++) {
        if (strcmp(g_ovr[i].name, d->name) == 0) {
            add_list(g_ovr[i].urls, urls, max, &n);
            if (g_ovr[i].replace) {
                return n;
            }
            break;
        }
    }
    add_list(d->urls, urls, max, &n);
    return n;
}

int sel_sysfile_fetch(const sel_host* host, const char* config_dir, const sel_sysfile_decl* d, sel_sysfile_res* res) {
    char urls[TRY_MAX][SEL_SYSFILE_URL_MAX];
    char tmp[600];
    char err[160];
    const sel_net* net;
    sel_sync_ctx ctx;
    sel_host bound;
    unsigned n;
    unsigned i;
    int got_body = 0;
    int w;

    if (res != NULL) {
        memset(res, 0, sizeof *res);
    }
    if (host == NULL || config_dir == NULL || d == NULL || d->name == NULL || res == NULL) {
        return SEL_SYSF_ALL_FAILED;
    }
    /* The host's net service exists on a host bound for a sync (sync_services.c); bind one like hippo_net.c does. */
    memset(&ctx, 0, sizeof ctx);
    ctx.scfg.host = host;
    sel_sync_host_bind(&bound, host, &ctx);
    host = &bound;
    net = host->net != NULL ? host->net(host) : NULL;
    if (net == NULL || net->get_file == NULL) {
        return SEL_SYSF_ALL_FAILED;
    }
    w = snprintf(tmp, sizeof tmp, "%s/%s.dl", config_dir, d->name);
    if (w < 0 || (size_t)w >= sizeof tmp) {
        return SEL_SYSF_ALL_FAILED;
    }
    n = sel_sysfile_urls(d, urls, TRY_MAX);
    for (i = 0; i < n; i++) {
        uint32_t reqs = 0;
        unsigned long bytes = 0;
        int st;

        res->tries++;
        st = net->get_file(host, urls[i], tmp, &reqs, &bytes);
        res->requests += reqs;
        if (st != 200) {
            remove(tmp);
            continue;
        }
        got_body = 1;
        if (sel_sysfile_install(config_dir, d, tmp, err, sizeof err)) {
            remove(tmp);
            snprintf(res->served, sizeof res->served, "%s", urls[i]);
            return SEL_SYSF_OK;
        }
        res->rejected++;
        remove(tmp);
    }
    return got_body ? SEL_SYSF_NO_MATCH : SEL_SYSF_ALL_FAILED;
}

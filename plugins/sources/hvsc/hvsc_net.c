/* See hvsc_net.h. */
#include "hvsc_net.h"

#include "source/sync_text.h"

#include <selecta/catalogue.h>
#include <selecta/net.h>
#include <stdio.h>
#include <string.h>

#define URL_BYTES 1100u
#define LINE_BYTES 2400u
#define PATH_BYTES 1100u

/* Songlengths.md5 starts with "[Database]" and carries `; /PATH` lines. A mirror's HTML page (200) has neither. */
static int songlengths_ok(const char* path) {
    FILE* in = fopen(path, "rb");
    char line[LINE_BYTES];
    int head = 0, paths = 0;

    if (in == NULL) {
        return 0;
    }
    if (fgets(line, sizeof line, in) != NULL) {
        head = strncmp(line, "[Database]", 10) == 0;
    }
    while (head && !paths && fgets(line, sizeof line, in) != NULL) {
        paths = line[0] == ';' && line[1] == ' ' && line[2] == '/';
    }
    fclose(in);
    return head && paths;
}

/* STIL.txt opens with a '#' comment banner naming "STIL" and carries `/PATH` entry lines. */
static int stil_ok(const char* path) {
    FILE* in = fopen(path, "rb");
    char line[LINE_BYTES];
    int banner = 0, entries = 0, n = 0;

    if (in == NULL) {
        return 0;
    }
    while (!entries && n++ < 100000 && fgets(line, sizeof line, in) != NULL) {
        if (line[0] == '#' && strstr(line, "STIL") != NULL) {
            banner = 1;
        } else if (line[0] == '/') {
            entries = 1;
        }
    }
    fclose(in);
    return banner && entries;
}

/* Writes the `; /PATH` lines of Songlengths.md5 as `/PATH` lines: the catalogue listing. Returns the count, or
 * -1 when a file cannot be opened or written. */
static int64_t derive_listing(const char* songlengths, const char* listing) {
    FILE* in = fopen(songlengths, "rb");
    FILE* out;
    char line[LINE_BYTES];
    int64_t n = 0;

    if (in == NULL) {
        return -1;
    }
    out = fopen(listing, "wb");
    if (out == NULL) {
        fclose(in);
        return -1;
    }
    while (fgets(line, sizeof line, in) != NULL) {
        sel_sync_chomp(line);
        if (line[0] == ';' && line[1] == ' ' && line[2] == '/') {
            if (fprintf(out, "%s\n", line + 2) < 0) {
                n = -1;
                break;
            }
            n++;
        }
    }
    fclose(in);
    if (fclose(out) != 0) {
        return -1;
    }
    return n;
}

int sel_hvsc_net_sync(const sel_host* host, const char* base, sel_hvsc_sync_stats* stats, sel_hvsc_net_stats* net) {
    const sel_net* net_svc = host->net != NULL ? host->net(host) : NULL;
    const sel_catalogue* cat = host->catalogue != NULL ? host->catalogue(host) : NULL;
    char dir[PATH_BYTES - 64u];
    char songlengths[PATH_BYTES];
    char stil[PATH_BYTES];
    char listing[PATH_BYTES];
    char url[URL_BYTES];
    unsigned long got = 0;
    size_t n = strlen(base);
    int64_t derived;

    memset(net, 0, sizeof *net);
    memset(stats, 0, sizeof *stats);
    while (n > 0 && base[n - 1] == '/') {
        n--;
    }
    if (n == 0 || n >= URL_BYTES - 64u) {
        return 0;
    }
    if (net_svc == NULL || cat == NULL || !cat->cache_dir(host, "hvsc", dir, sizeof dir) ||
        snprintf(songlengths, sizeof songlengths, "%s/%s", dir, SEL_HVSC_SONGLENGTHS) >= (int)sizeof songlengths ||
        snprintf(stil, sizeof stil, "%s/%s", dir, SEL_HVSC_STIL) >= (int)sizeof stil ||
        snprintf(listing, sizeof listing, "%s/%s", dir, SEL_HVSC_LISTING) >= (int)sizeof listing) {
        return 0;
    }
    snprintf(url, sizeof url, "%.*s" SEL_HVSC_DOC_DIR SEL_HVSC_SONGLENGTHS, (int)n, base);
    if (net_svc->get_file_checked(host, url, songlengths, songlengths_ok, &net->http_requests, &got) != 200) {
        return 0;
    }
    net->songlengths_bytes = (uint32_t)got;
    snprintf(url, sizeof url, "%.*s" SEL_HVSC_DOC_DIR SEL_HVSC_STIL, (int)n, base);
    if (net_svc->get_file_checked(host, url, stil, stil_ok, &net->http_requests, &got) != 200) {
        return 0;
    }
    net->stil_bytes = (uint32_t)got;
    derived = derive_listing(songlengths, listing);
    if (derived < 1) {
        return 0;
    }
    net->listing_derived = (uint32_t)derived;
    return sel_hvsc_sync(host, dir, stats);
}

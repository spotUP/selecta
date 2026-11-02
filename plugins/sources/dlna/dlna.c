/* The `dlna` source descriptor (row I894a3; network libs design, DLNA section): a UPnP / DLNA MediaServer (Synology
 * Media Server, Minidlna, Plex DLNA, ...) as a library source. Discovery (SSDP M-SEARCH, device description,
 * ContentDirectory controlURL) and the paged Browse SOAP live in plugins/shared/upnp; devices without a
 * ContentDirectory service (a Philips Hue bridge also answers the MediaServer search) never reach this plugin
 * because the description parser only accepts a device that has one.
 *
 * A "sync" takes one argument:
 *   - a device description URL (http://host:port/desc/device.xml): that server is scanned;
 *   - "discover": SSDP finds the servers on the LAN (each is named on stderr) and every one is scanned.
 * SELECTA_DLNA_SSDP="ip:port" replaces the multicast address (tests point it at a unicast fixture).
 * The scan walks by BrowseDirectChildren, depth first, paging each container by StartingIndex until TotalMatches.
 * Synology and Minidlna list every file under several virtual views (Artist, Album, Genre, Folder), so a walk of the
 * whole tree costs a request per virtual container and meets each file about seven times (I894a4: 23045 requests, the
 * row cap). The scan therefore looks for the server's folder view (a container titled "Folder", "Folders" or
 * "Browse Folders" among the children of the root or of one level below it, e.g. root > Music > Folder) and walks
 * only that; with no such container it walks everything from "0". A res URL already stored in this sync is skipped.
 * Every audio item with a res URL becomes one song, key
 * `dlna:<artist>/<title>/<res url>`; play_url returns the res URL and the engine streams it over HTTP (seekable by
 * Range when the server allows it). Browse is read-only; nothing is written to the server, the library or the cache. */
#include <selecta/catalogue.h>
#include <selecta/host.h>
#include <selecta/net.h>
#include <selecta/plugin.h>
#include <selecta/source.h>
#include "source/descriptor.h"
#include "source/sync_stat.h"
#include "upnp/upnp.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define USER_AGENT "Selecta-DLNA/1 (+https://github.com/spotUP/Selecta)"
#define DESC_CAP (64u * 1024u)
#define MAX_DEPTH 8u
#define MAX_SONGS 200000u
#define MAX_SERVERS 8u
#define URL_BYTES 1100u
#define SSDP_WAIT_MS 2500u

static const sel_tag g_tags[] = {
    {SEL_TAG_END, 0},
};

typedef struct run {
    const sel_host* host;
    const sel_catalogue* cat;
    const char* server;
    const char* control;
    uint32_t rows, skipped, requests, dups;
    uint64_t* seen; /* open addressing set of res URL hashes (0 = empty slot), I894a4 dedup */
    size_t seen_cap, seen_n;
} run;

static uint64_t url_hash(const char* s) {
    uint64_t h = 1469598103934665603ull; /* FNV-1a */

    for (; *s != '\0'; s++) {
        h = (h ^ (unsigned char)*s) * 1099511628211ull;
    }
    return h != 0u ? h : 1u;
}

/* Adds the res URL to the seen set. 1 when it was new, 0 when this sync already stored it. */
static int seen_add(run* r, const char* url) {
    uint64_t h = url_hash(url);
    size_t i;

    if ((r->seen_n + 1u) * 2u > r->seen_cap) {
        size_t ncap = r->seen_cap != 0u ? r->seen_cap * 2u : 4096u, k;
        uint64_t* nt = (uint64_t*)calloc(ncap, sizeof *nt);

        if (nt == NULL) {
            return 1; /* out of memory: keep the song rather than lose it */
        }
        for (k = 0; k < r->seen_cap; k++) {
            if (r->seen[k] != 0u) {
                size_t j = (size_t)(r->seen[k] % ncap);

                while (nt[j] != 0u) {
                    j = (j + 1u) % ncap;
                }
                nt[j] = r->seen[k];
            }
        }
        free(r->seen);
        r->seen = nt;
        r->seen_cap = ncap;
    }
    for (i = (size_t)(h % r->seen_cap); r->seen[i] != 0u; i = (i + 1u) % r->seen_cap) {
        if (r->seen[i] == h) {
            return 0;
        }
    }
    r->seen[i] = h;
    r->seen_n++;
    return 1;
}

static void clean_part(char* s) {
    for (; *s != '\0'; s++) {
        if (*s == '/' || *s == '|' || (unsigned char)*s < 32u) {
            *s = ' ';
        }
    }
}

static int is_audio(const sel_upnp_entry* e) {
    return e->res_url[0] != '\0' &&
           (strstr(e->protocol_info, ":audio/") != NULL || strstr(e->protocol_info, ":application/ogg") != NULL);
}

static void add_item(run* r, const sel_upnp_entry* e) {
    char artist[SEL_UPNP_TEXT_BYTES], title[SEL_UPNP_TEXT_BYTES], url[SEL_UPNP_URL_BYTES + 32u];
    char path[URL_BYTES + 2u * SEL_UPNP_TEXT_BYTES];
    sel_catalogue_song song;
    size_t i, j = 0;

    if (!is_audio(e) || e->title[0] == '\0') {
        r->skipped++;
        return;
    }
    if (!seen_add(r, e->res_url)) {
        r->dups++;
        return;
    }
    snprintf(artist, sizeof artist, "%s", e->artist[0] != '\0' ? e->artist : r->server);
    snprintf(title, sizeof title, "%s", e->title);
    clean_part(artist);
    clean_part(title);
    for (i = 0; e->res_url[i] != '\0' && j + 4u < sizeof url; i++) { /* '|' separates catalogue fields: escape it */
        if (e->res_url[i] == '|') {
            j += (size_t)snprintf(url + j, sizeof url - j, "%%7C");
        } else {
            url[j++] = e->res_url[i];
        }
    }
    url[j] = '\0';
    snprintf(path, sizeof path, "%s/%s/%s", artist, title, url);
    memset(&song, 0, sizeof song);
    song.path = path;
    song.format = "DLNA";
    song.artist = artist;
    song.album = e->album;
    song.name = title;
    song.md5 = "";
    song.duration_ms = (int32_t)e->duration_ms;
    if (!r->cat->song(r->host, &song)) {
        r->skipped++;
        return;
    }
    r->rows++;
}

/* Depth first walk of one container; pages by StartingIndex until TotalMatches. 0 on a transport error. */
static int walk(run* r, const char* id, unsigned depth) {
    sel_upnp_entry* page = (sel_upnp_entry*)malloc(SEL_UPNP_BROWSE_PAGE * sizeof *page);
    unsigned start = 0, total = 0;
    int ok = 1;

    if (page == NULL) {
        return 0;
    }
    do {
        int n = sel_upnp_browse(r->control, id, start, SEL_UPNP_BROWSE_PAGE, page, SEL_UPNP_BROWSE_PAGE, &total);
        int i;

        r->requests++;
        if (n < 0) {
            ok = 0;
            break;
        }
        if (n == 0) {
            break;
        }
        for (i = 0; i < n && r->rows < MAX_SONGS; i++) {
            if (!page[i].is_container) {
                add_item(r, &page[i]);
            } else if (depth + 1u < MAX_DEPTH) {
                walk(r, page[i].id, depth + 1u); /* a failing sub-container does not drop the rest of the share */
            }
        }
        start += (unsigned)n;
    } while (start < total && r->rows < MAX_SONGS);
    free(page);
    return ok;
}

static int ci_has(const char* s, const char* word) {
    size_t n = strlen(word), i;

    for (i = 0; s[i] != '\0'; i++) {
        size_t k = 0;

        while (k < n && s[i + k] != '\0' && (s[i + k] | 32) == word[k]) {
            k++;
        }
        if (k == n) {
            return 1;
        }
    }
    return 0;
}

/* Looks for the server's folder view in the children of container `id`: a container whose title holds "folder".
 * Returns 1 and copies its id; 0 when none. `below` collects the other containers for a one level deeper search. */
static int find_in(run* r, const char* id, char* folder, size_t fn, char (*below)[SEL_UPNP_ID_BYTES], unsigned* nb) {
    sel_upnp_entry* page = (sel_upnp_entry*)malloc(SEL_UPNP_BROWSE_PAGE * sizeof *page);
    unsigned start = 0, total = 0;
    int found = 0;

    if (page == NULL) {
        return 0;
    }
    do {
        int n = sel_upnp_browse(r->control, id, start, SEL_UPNP_BROWSE_PAGE, page, SEL_UPNP_BROWSE_PAGE, &total);
        int i;

        r->requests++;
        if (n <= 0) {
            break;
        }
        for (i = 0; i < n && !found; i++) {
            if (!page[i].is_container) {
                continue;
            }
            if (ci_has(page[i].title, "folder")) {
                snprintf(folder, fn, "%s", page[i].id);
                found = 1;
            } else if (below != NULL && *nb < 16u) {
                snprintf(below[(*nb)++], SEL_UPNP_ID_BYTES, "%s", page[i].id);
            }
        }
        start += (unsigned)n;
    } while (!found && start < total);
    free(page);
    return found;
}

/* The container to walk: the folder view under the root or one level below it (Synology: Music > Folder), else
 * the root "0" (full walk). */
static void pick_start(run* r, char* start, size_t n) {
    char (*below)[SEL_UPNP_ID_BYTES] = (char(*)[SEL_UPNP_ID_BYTES])malloc(16u * SEL_UPNP_ID_BYTES);
    unsigned nb = 0, i;

    snprintf(start, n, "0");
    if (below == NULL) {
        return;
    }
    if (!find_in(r, "0", start, n, below, &nb)) {
        for (i = 0; i < nb; i++) {
            if (find_in(r, below[i], start, n, NULL, NULL)) {
                break;
            }
        }
    }
    free(below);
}

/* Reads the device description at `location` into `out`. 0 when it is unreachable or has no ContentDirectory. */
static int describe(const sel_host* host, const sel_net* net, const char* location, sel_upnp_server* out,
                    uint32_t* rq) {
    char* xml = (char*)malloc(DESC_CAP);
    uint32_t got = 0;
    int ok = 0;

    if (xml == NULL) {
        return 0;
    }
    if (net->request_ex(host, location, USER_AGENT, NULL, xml, DESC_CAP - 1u, &got, rq) == 200) {
        xml[got] = '\0';
        memset(out, 0, sizeof *out);
        snprintf(out->location, sizeof out->location, "%s", location);
        ok = sel_upnp_parse_description(xml, location, out);
    }
    free(xml);
    return ok;
}

static int dl_sync(const sel_host* host, const char* arg, int64_t now, sel_tag* stats, uint32_t cap) {
    const sel_net* net = host->net != NULL ? host->net(host) : NULL;
    const sel_catalogue* cat = host->catalogue != NULL ? host->catalogue(host) : NULL;
    sel_upnp_server servers[MAX_SERVERS];
    char from[SEL_UPNP_ID_BYTES];
    int n = 0, i, ok = 0;
    run r;

    (void)now;
    if (net == NULL || net->request_ex == NULL || cat == NULL || arg == NULL) {
        return 0;
    }
    memset(&r, 0, sizeof r);
    r.host = host;
    r.cat = cat;
    if (strcmp(arg, "discover") == 0) {
        const char* env = getenv("SELECTA_DLNA_SSDP");
        char ip[48] = "";
        unsigned port = 0;

        if (env != NULL && sscanf(env, "%47[^:]:%u", ip, &port) == 2) {
            n = sel_upnp_discover(ip, port, SSDP_WAIT_MS, servers, MAX_SERVERS);
        } else {
            n = sel_upnp_discover(NULL, 0u, SSDP_WAIT_MS, servers, MAX_SERVERS);
        }
        if (n <= 0) {
            fputs("dlna: no MediaServer with a ContentDirectory answered the SSDP search\n", stderr);
            n = 0;
        }
    } else if (strncmp(arg, "http", 4) == 0 && describe(host, net, arg, &servers[0], &r.requests)) {
        n = 1;
    } else {
        fputs("dlna: sync takes a device description URL (http://host:port/desc.xml) or \"discover\"\n", stderr);
        return 0;
    }
    if (!cat->begin(host, "dlna")) {
        return 0;
    }
    ok = 1;
    for (i = 0; i < n; i++) {
        fprintf(stderr, "dlna: server %s control %s\n", servers[i].name, servers[i].control_url);
        r.server = servers[i].name;
        r.control = servers[i].control_url;
        pick_start(&r, from, sizeof from);
        fprintf(stderr, "dlna: walking %s\n", strcmp(from, "0") == 0 ? "the whole tree (no folder view)" : "the folder view");
        ok &= walk(&r, from, 0u);
    }
    fprintf(stderr, "dlna: %u songs, %u repeated res URLs skipped, %u requests\n", (unsigned)r.rows, (unsigned)r.dups,
            (unsigned)r.requests);
    free(r.seen);
    if (ok) {
        ok = cat->commit(host);
    } else {
        cat->abort(host);
    }
    sel_sync_stat(stats, cap, SEL_SYNC_LISTING_ROWS, r.rows);
    sel_sync_stat(stats, cap, SEL_SYNC_SKIPPED_ROWS, r.skipped);
    sel_sync_stat(stats, cap, SEL_SYNC_NET_REQUESTS, r.requests);
    return ok;
}

/* The res URL is everything after the second '/'. */
static int dl_play_url(const sel_host* host, const char* path, char* out, size_t n) {
    const char* a = strchr(path, '/');
    const char* b = a != NULL ? strchr(a + 1, '/') : NULL;
    size_t i, o = 0;

    (void)host;
    if (b == NULL || b[1] == '\0') {
        return 0;
    }
    for (b++, i = 0; b[i] != '\0' && o + 1 < n; i++) {
        if (strncmp(b + i, "%7C", 3) == 0) {
            out[o++] = '|';
            i += 2;
        } else {
            out[o++] = b[i];
        }
    }
    out[o] = '\0';
    return b[i] == '\0';
}

static int parse(const char* path, char* scratch, size_t n, sel_tag* out) {
    static const char fmt[] = "DLNA";
    const char* name = path != NULL ? strchr(path, '/') : NULL;
    const char* url = name != NULL ? strchr(name + 1, '/') : NULL;
    size_t al, nl;

    if (url == NULL) {
        return 0;
    }
    al = (size_t)(name - path);
    nl = (size_t)(url - name - 1);
    if (sizeof fmt + al + 1u + nl + 1u > n) {
        return 0;
    }
    memcpy(scratch, fmt, sizeof fmt);
    memcpy(scratch + sizeof fmt, path, al);
    scratch[sizeof fmt + al] = '\0';
    memcpy(scratch + sizeof fmt + al + 1u, name + 1, nl);
    scratch[sizeof fmt + al + 1u + nl] = '\0';
    out[0].tag = SEL_SRC_FORMAT;
    out[0].value = (uintptr_t)scratch;
    out[1].tag = SEL_SRC_ARTIST;
    out[1].value = (uintptr_t)(scratch + sizeof fmt);
    out[2].tag = SEL_SRC_NAME;
    out[2].value = (uintptr_t)(scratch + sizeof fmt + al + 1u);
    out[3].tag = SEL_TAG_END;
    out[3].value = 0;
    return 1;
}

static const sel_source g_dlna = {
    "dlna", "DLNA", "http://www.upnp.org/",
    "Music from a UPnP / DLNA media server on your network (Synology Media Server, Minidlna, ...).",
    g_tags, NULL, parse, NULL, NULL, NULL, NULL, NULL, dl_sync,
    SEL_SOURCE_REMOTE | SEL_SOURCE_SYNCS | SEL_SOURCE_STREAMS | SEL_SOURCE_SEEKABLE, "http://www.upnp.org/", dl_play_url,
    NULL, /* search (I406) */
};

const sel_tag* SEL_PLUGIN_ENTRY(uint32_t host_api) {
    (void)host_api;
    return sel_source_plugin_descriptor("dlna", &g_dlna);
}

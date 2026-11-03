/* The `ccmixter` source descriptor (row I725g; plan 2026-10-30-open-streaming-sources, row 8): Creative Commons music
 * from http://ccmixter.org/ through its query API (no key, plain HTTP).
 * Terms read 2026-10-30, https://ccmixter.org/terms (quoted in the research doc): no framing or co-branding, and the
 * user may not "overburden, damage or impair" the servers; every upload is under its own Creative Commons licence.
 * Consequences kept here: a sync is ONE query request (default 25 uploads, never more than 100) so the server is not
 * burdened; every request carries the descriptive User-Agent below and the host's request_ex backs off on 429
 * (net/backoff.c); the audio is only streamed (SEL_SOURCE_STREAMS: nothing is kept in the library or cache dir except
 * the JSON of one call, removed at once); the upload's own licence is shown in the format column ("ccMixter - CC BY-NC
 * 4.0"); the url hook gives the upload page https://ccmixter.org/files/<user>/<id>; nothing is framed or re-branded.
 *
 * The argument is `<base url>[?<parameters>]`: limit (<= 100), offset, tags (default mp3), search. The Sources page
 * Sync button sends the web site home = the default listing. Each upload yields one row for its first MP3 file. A song
 * key is `<upload id>/<user name>/<upload title>`. Playing it asks <base>/api/query?f=json&ids=<id> for the file and
 * returns its download URL; a download URL on https://ccmixter.org/ is sent through the base (plain HTTP by default,
 * so the Amiga net stack can reach it). SELECTA_CCMIXTER_BASE replaces the base (tests point it at a fixture). */
#include <selecta/catalogue.h>
#include <selecta/host.h>
#include <selecta/net.h>
#include <selecta/plugin.h>
#include <selecta/source.h>
#include "source/descriptor.h"
#include "source/json_scan.h"
#include "source/license_name.h"
#include "source/sync_stat.h"
#include "source/url_path.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_BASE "http://ccmixter.org"
#define SITE_HOME "http://ccmixter.org/"
#define SECURE_HOME "https://ccmixter.org/"
#define ITEM_PAGE "https://ccmixter.org/files/"
#define USER_AGENT "Selecta-CcMixter/1 (+https://github.com/spotUP/selecta; music player, streams on request, one query per sync)"
#define URL_BYTES 1100u
#define ID_MAX 12u
#define CAP (2u * 1024u * 1024u)
#define LIMIT_MAX 100

static const sel_tag g_tags[] = {
    {SEL_TAG_END, 0},
};

/* One upload: the strings the parser keeps. */
typedef struct rec {
    char id[ID_MAX];
    char title[120];
    char user[64];
    char license[96];
    char file[260];
    char url[400];
} rec;

static int is_mp3(const char* name) {
    size_t n = strlen(name);

    return n > 4 && strcmp(name + n - 4, ".mp3") == 0;
}

/* A number whose first byte `c` is consumed: its digits go to `out`; *end gets the delimiter that ends it. */
static int read_number(FILE* f, int c, char* out, size_t n, int* end) {
    size_t i = 0;

    while (c != EOF && c != ',' && c != '}' && c != ']' && !isspace(c)) {
        if (i + 1 < n && isdigit(c)) {
            out[i++] = (char)c;
        }
        c = fgetc(f);
    }
    out[i] = '\0';
    *end = isspace(c) ? sel_json_skip_ws(f) : c;
    return c != EOF;
}

/* Skips the value starting with `c`; *end is the delimiter after it. */
static int skip(FILE* f, int c, int* end) {
    return c != EOF && sel_json_skip_value(f, c, end);
}

/* The "files" array of one upload, its '[' consumed: the first file name ending in .mp3 and its download_url win. */
static int read_files(FILE* f, rec* t) {
    char key[24], name[260], url[400];
    int c;

    for (;;) {
        c = sel_json_skip_ws(f);
        if (c == ']') {
            return 1;
        }
        if (c == ',') {
            continue;
        }
        if (c != '{') {
            return 0;
        }
        name[0] = url[0] = '\0';
        for (;;) {
            int end = 0;

            c = sel_json_skip_ws(f);
            if (c == '}') {
                break;
            }
            if (c == ',') {
                continue;
            }
            if (c != '"' || !sel_json_read_string(f, key, sizeof key) || sel_json_skip_ws(f) != ':') {
                return 0;
            }
            c = sel_json_skip_ws(f);
            if (c == '"' && (strcmp(key, "file_name") == 0 || strcmp(key, "download_url") == 0)) {
                int file_name = strcmp(key, "file_name") == 0;

                if (!sel_json_read_string(f, file_name ? name : url, file_name ? sizeof name : sizeof url)) {
                    return 0;
                }
                c = sel_json_skip_ws(f);
                if (c == '}') {
                    break;
                }
            } else {
                if (!skip(f, c, &end)) {
                    return 0;
                }
                if (end == '}') {
                    break;
                }
            }
        }
        if (t->file[0] == '\0' && is_mp3(name) && url[0] != '\0') {
            snprintf(t->file, sizeof t->file, "%s", name);
            snprintf(t->url, sizeof t->url, "%s", url);
        }
    }
}

/* One upload object, its '{' consumed. */
static int read_rec(FILE* f, rec* t) {
    char key[24];
    int c;

    memset(t, 0, sizeof *t);
    for (;;) {
        int end = 0;

        c = sel_json_skip_ws(f);
        if (c == '}') {
            return 1;
        }
        if (c == ',') {
            continue;
        }
        if (c != '"' || !sel_json_read_string(f, key, sizeof key) || sel_json_skip_ws(f) != ':') {
            return 0;
        }
        c = sel_json_skip_ws(f);
        if (c == '"') {
            char* dst = strcmp(key, "upload_name") == 0 ? t->title : strcmp(key, "user_name") == 0 ? t->user
                        : strcmp(key, "license_url") == 0 ? t->license : NULL;
            size_t n = dst == t->title ? sizeof t->title : dst == t->user ? sizeof t->user : sizeof t->license;

            if (!sel_json_read_string(f, dst, dst != NULL ? n : 0)) {
                return 0;
            }
            c = sel_json_skip_ws(f);
            if (c == '}') {
                return 1;
            }
        } else if (c == '[' && strcmp(key, "files") == 0) {
            if (!read_files(f, t)) {
                return 0;
            }
            c = sel_json_skip_ws(f);
            if (c == '}') {
                return 1;
            }
        } else if (strcmp(key, "upload_id") == 0 && isdigit(c)) {
            if (!read_number(f, c, t->id, sizeof t->id, &end)) {
                return 0;
            }
            if (end == '}') {
                return 1;
            }
        } else {
            if (!skip(f, c, &end)) {
                return 0;
            }
            if (end == '}') {
                return 1;
            }
        }
    }
}

typedef void (*rec_fn)(void* ctx, rec* t);

/* The top-level array of uploads, each handed to `each`. */
static void read_uploads(FILE* f, rec_fn each, void* ctx) {
    rec t;
    int c;

    if (sel_json_skip_ws(f) != '[') {
        return;
    }
    while ((c = sel_json_skip_ws(f)) != EOF && c != ']') {
        if (c == ',') {
            continue;
        }
        if (c != '{' || !read_rec(f, &t)) {
            return;
        }
        each(ctx, &t);
    }
}

static const char* api_base(void) {
    const char* env = getenv("SELECTA_CCMIXTER_BASE");

    return env != NULL && env[0] != '\0' ? env : DEFAULT_BASE;
}

/* GET `url` through the host (descriptive User-Agent, 429 / Retry-After back-off) into `path`. Returns the status. */
static int fetch(const sel_host* host, const sel_net* net, const char* url, const char* path, uint32_t* requests,
                 unsigned long* bytes) {
    uint32_t got = 0;
    void* buf = malloc(CAP);
    FILE* f;
    int status;

    if (buf == NULL) {
        return 0;
    }
    status = net->request_ex(host, url, USER_AGENT, NULL, buf, CAP, &got, requests);
    if (status == 200 && (f = fopen(path, "wb")) != NULL) {
        if (fwrite(buf, 1, got, f) != got) {
            status = 0;
        }
        fclose(f);
    } else if (status == 200) {
        status = 0;
    }
    *bytes += got;
    free(buf);
    return status;
}

/* A key part has no '/', '|' or control byte. */
static void clean_part(char* s) {
    for (; *s != '\0'; s++) {
        if (*s == '/' || *s == '|' || (unsigned char)*s < 32u) {
            *s = ' ';
        }
    }
}

static int id_ok(const char* id) {
    size_t i;

    for (i = 0; id[i] != '\0'; i++) {
        if (!isdigit((unsigned char)id[i])) {
            return 0;
        }
    }
    return i > 0 && i < ID_MAX;
}

typedef struct run {
    const sel_host* host;
    const sel_catalogue* cat;
    uint32_t rows, skipped;
    int stopped;
    const sel_search_sink* sink; /* search: rows go to the sink and nothing is written to the catalogue; NULL: sync */
} run;

static void take_upload(void* ctx, rec* t) {
    run* r = (run*)ctx;
    char path[ID_MAX + 240u], format[128], lic[64];
    sel_catalogue_song song;

    if (r->stopped) {
        return; /* the search sink had enough */
    }
    if (!id_ok(t->id) || t->file[0] == '\0' || !sel_license_is_open(t->license) || t->user[0] == '\0') {
        r->skipped++; /* no MP3, or a licence that is not Creative Commons / public domain */
        return;
    }
    clean_part(t->title);
    clean_part(t->user);
    sel_license_name(t->license, lic, sizeof lic);
    snprintf(path, sizeof path, "%s/%s/%s", t->id, t->user, t->title);
    snprintf(format, sizeof format, "ccMixter - %s", lic);
    if (r->sink != NULL) {
        r->rows++;
        if (!r->sink->row(r->sink->ctx, path, t->title, t->user, format)) {
            r->stopped = 1; /* the sink asked to stop */
        }
        return;
    }
    memset(&song, 0, sizeof song);
    song.path = path;
    song.format = format;
    song.artist = t->user;
    song.name = t->title;
    song.md5 = "";
    if (!r->cat->song(r->host, &song)) {
        r->skipped++;
        return;
    }
    r->rows++;
}

/* The value of `name` in `query` up to '&' (letters, digits, "-_.,+" only; else empty). */
static void param_of(const char* query, const char* name, char* out, size_t n) {
    size_t ln = strlen(name), i = 0;
    const char* p = query;

    out[0] = '\0';
    while (p != NULL && *p != '\0') {
        if (strncmp(p, name, ln) == 0 && p[ln] == '=') {
            for (p += ln + 1; *p != '\0' && *p != '&' && i + 1 < n; p++) {
                if (!isalnum((unsigned char)*p) && strchr("-_.,+", *p) == NULL) {
                    out[0] = '\0';
                    return;
                }
                out[i++] = *p;
            }
            out[i] = '\0';
            return;
        }
        p = strchr(p, '&');
        if (p != NULL) {
            p++;
        }
    }
}

static int cc_sync(const sel_host* host, const char* arg, int64_t now, sel_tag* stats, uint32_t cap) {
    const char* query = strchr(arg, '?');
    const char* q = query != NULL ? query + 1 : "";
    size_t bl = query != NULL ? (size_t)(query - arg) : strlen(arg);
    char base[URL_BYTES], dir[URL_BYTES - 64u], url[URL_BYTES], path[URL_BYTES];
    char limit[8], offset[8], tags[64], search[64];
    uint32_t rq = 0;
    unsigned long got = 0;
    const sel_net* net = host->net != NULL ? host->net(host) : NULL;
    run r;
    FILE* f;
    int ok = 0;

    (void)now;
    memset(&r, 0, sizeof r);
    r.host = host;
    r.cat = host->catalogue != NULL ? host->catalogue(host) : NULL;
    while (bl > 0 && arg[bl - 1] == '/') {
        bl--;
    }
    if (bl == 0 || (bl >= sizeof SITE_HOME - 2u && (strncmp(arg, SITE_HOME, sizeof SITE_HOME - 2u) == 0 ||
                                                    strncmp(arg, SECURE_HOME, sizeof SECURE_HOME - 2u) == 0))) {
        snprintf(base, sizeof base, "%s", api_base());
    } else if (bl < sizeof base) {
        snprintf(base, sizeof base, "%.*s", (int)bl, arg);
    } else {
        return 0;
    }
    if (net == NULL || net->request_ex == NULL || r.cat == NULL ||
        !r.cat->cache_dir(host, "ccmixter", dir, sizeof dir) || !r.cat->begin(host, "ccmixter")) {
        return 0;
    }
    param_of(q, "limit", limit, sizeof limit);
    param_of(q, "offset", offset, sizeof offset);
    param_of(q, "tags", tags, sizeof tags);
    param_of(q, "search", search, sizeof search);
    if (limit[0] == '\0' || atoi(limit) > LIMIT_MAX || atoi(limit) < 1) {
        snprintf(limit, sizeof limit, "25");
    }
    snprintf(url, sizeof url, "%s/api/query?f=json&sort=rank&limit=%s&offset=%s&tags=%s%s%s", base, limit,
             offset[0] != '\0' ? offset : "0", tags[0] != '\0' ? tags : "mp3", search[0] != '\0' ? "&search=" : "",
             search);
    snprintf(path, sizeof path, "%s/query.json", dir);
    if (fetch(host, net, url, path, &rq, &got) == 200 && (f = fopen(path, "rb")) != NULL) {
        read_uploads(f, take_upload, &r);
        fclose(f);
        ok = 1;
    }
    remove(path);
    if (ok) {
        ok = r.cat->commit(host);
    } else {
        r.cat->abort(host);
    }
    sel_sync_stat(stats, cap, SEL_SYNC_LISTING_ROWS, r.rows);
    sel_sync_stat(stats, cap, SEL_SYNC_SKIPPED_ROWS, r.skipped);
    sel_sync_stat(stats, cap, SEL_SYNC_NET_BYTES, (uint32_t)got);
    sel_sync_stat(stats, cap, SEL_SYNC_NET_REQUESTS, rq);
    return ok;
}

#define SEARCH_ROWS 50u /* uploads a search asks for, best rank first */

/* Search: one GET /api/query?search=<query> (the sync's query, MP3 only, open licences only). The rows go to the sink with
 * the sync's key <id>/<user>/<title>, the catalogue is not touched. Returns the row count, -1 when the service answers no 200. */
static int cc_search(const sel_host* host, const char* query, sel_search_sink* out) {
    const sel_net* net = host != NULL && host->net != NULL ? host->net(host) : NULL;
    const sel_catalogue* cat = host != NULL && host->catalogue != NULL ? host->catalogue(host) : NULL;
    char dir[URL_BYTES - 64u], enc[300], url[URL_BYTES], path[URL_BYTES];
    uint32_t rq = 0;
    unsigned long got = 0;
    run r;
    FILE* f;
    int ok = 0;

    memset(&r, 0, sizeof r);
    r.host = host;
    r.cat = cat;
    r.sink = out;
    if (net == NULL || net->request_ex == NULL || cat == NULL || query == NULL || out == NULL ||
        !cat->cache_dir(host, "ccmixter", dir, sizeof dir) || !sel_url_query_encode(query, enc, sizeof enc)) {
        return -1;
    }
    snprintf(url, sizeof url, "%s/api/query?f=json&sort=rank&limit=%u&tags=mp3&search=%s", api_base(), SEARCH_ROWS, enc);
    /* A search may run while a sync of this source is fetching: each keeps its own file. */
    snprintf(path, sizeof path, "%s/search-query.json", dir);
    if (fetch(host, net, url, path, &rq, &got) == 200 && (f = fopen(path, "rb")) != NULL) {
        read_uploads(f, take_upload, &r);
        fclose(f);
        ok = 1;
    }
    remove(path);
    return ok ? (int)r.rows : -1;
}

typedef struct found {
    char url[400];
} found;

static void take_found(void* ctx, rec* t) {
    found* fo = (found*)ctx;

    if (fo->url[0] == '\0' && t->url[0] != '\0') {
        snprintf(fo->url, sizeof fo->url, "%s", t->url);
    }
}

/* Play: GET <base>/api/query?f=json&ids=<id>, hand the MP3 download URL to sel_live. */
static int cc_play_url(const sel_host* host, const char* path, char* out, size_t n) {
    const sel_net* net = host->net != NULL ? host->net(host) : NULL;
    const sel_catalogue* cat = host->catalogue != NULL ? host->catalogue(host) : NULL;
    const char* slash = strchr(path, '/');
    size_t il = slash != NULL ? (size_t)(slash - path) : 0;
    char dir[URL_BYTES - 64u], file[URL_BYTES], url[URL_BYTES], id[ID_MAX];
    uint32_t rq = 0;
    unsigned long got = 0;
    found fo;
    FILE* f;
    int ok = 0;

    if (net == NULL || net->request_ex == NULL || cat == NULL || il == 0 || il >= sizeof id) {
        return 0;
    }
    memcpy(id, path, il);
    id[il] = '\0';
    if (!id_ok(id) || !cat->cache_dir(host, "ccmixter", dir, sizeof dir)) {
        return 0;
    }
    snprintf(url, sizeof url, "%s/api/query?f=json&ids=%s", api_base(), id);
    snprintf(file, sizeof file, "%s/play.json", dir);
    if (fetch(host, net, url, file, &rq, &got) != 200 || (f = fopen(file, "rb")) == NULL) {
        remove(file);
        return 0;
    }
    memset(&fo, 0, sizeof fo);
    read_uploads(f, take_found, &fo);
    fclose(f);
    remove(file);
    if (fo.url[0] != '\0') {
        if (strncmp(fo.url, SECURE_HOME, sizeof SECURE_HOME - 1u) == 0) {
            /* The download host is the base's: plain HTTP by default, percent-encoded. */
            snprintf(url, sizeof url, "%s/", api_base());
            ok = sel_url_path_encode(url, fo.url + sizeof SECURE_HOME - 1u, out, n);
        } else {
            ok = snprintf(out, n, "%s", fo.url) < (int)n;
        }
    }
    return ok;
}

/* The upload page backlink. */
static int url_of(const char* path, char* out, size_t n) {
    const char* a = strchr(path, '/');
    const char* b = a != NULL ? strchr(a + 1, '/') : NULL;

    if (a == NULL || b == NULL || a == path || b == a + 1) {
        return 0;
    }
    return snprintf(out, n, ITEM_PAGE "%.*s/%.*s", (int)(b - a - 1), a + 1, (int)(a - path), path) < (int)n;
}

static int parse(const char* path, char* scratch, size_t n, sel_tag* out) {
    static const char fmt[] = "ccMixter";
    const char* artist = path != NULL ? strchr(path, '/') : NULL;
    const char* name = artist != NULL ? strchr(artist + 1, '/') : NULL;
    size_t al, nl;

    if (name == NULL || name[1] == '\0') {
        return 0;
    }
    artist++;
    al = (size_t)(name - artist);
    name++;
    nl = strlen(name);
    if (sizeof fmt + al + 1u + nl + 1u > n) {
        return 0;
    }
    memcpy(scratch, fmt, sizeof fmt);
    memcpy(scratch + sizeof fmt, artist, al);
    scratch[sizeof fmt + al] = '\0';
    memcpy(scratch + sizeof fmt + al + 1u, name, nl + 1u);
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

static const sel_source g_ccmixter = {
    "ccmixter", "ccMixter", SITE_HOME,
    "Creative Commons remixes and samples streamed from ccMixter; each upload shows its own licence.",
    g_tags, NULL, parse, NULL, url_of, NULL, NULL, NULL, cc_sync, SEL_SOURCE_REMOTE | SEL_SOURCE_SYNCS | SEL_SOURCE_STREAMS,
    SITE_HOME, cc_play_url,
    cc_search,
};

const sel_tag* SEL_PLUGIN_ENTRY(uint32_t host_api) {
    (void)host_api;
    return sel_source_plugin_descriptor("ccmixter", &g_ccmixter);
}

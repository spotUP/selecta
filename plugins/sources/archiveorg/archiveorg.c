/* The `archiveorg` source descriptor (row I725d; plan 2026-10-30-open-streaming-sources, row 5): Creative Commons and
 * public-domain audio from https://archive.org/ through advancedsearch and the per-item metadata API.
 * Terms read 2026-10-30:
 *   Terms of Use, https://archive.org/about/terms.php: "Access to the Archive's Collections is provided at no cost to
 *    you and is granted for scholarship and research purposes only."
 *   https://archive.org/services/docs/api/bots.html welcomes tools that send a descriptive User-Agent and honour
 *    "429 Too Many Requests" and Retry-After.
 * Consequences kept here: only items whose `licenseurl` names Creative Commons (creativecommons.org) or the public
 * domain are listed (items with no licence, or another one, are skipped: Johan's decision); every request carries the
 * descriptive User-Agent below; the host's request_ex backs off on 429 per Retry-After (net/backoff.c); requests are
 * paused between items; the audio is only streamed (SEL_SOURCE_STREAMS: nothing is written to the library or cache
 * dir except the small JSON of one API call, removed at once); every row names the creator, "Internet Archive" and the
 * licence (format column "Internet Archive - CC BY 4.0"); the url hook gives the item page https://archive.org/details/<id>.
 *
 * A "sync" is one advancedsearch page (default: 25 items by downloads, licence filter in the query and again on every
 * hit) followed by one metadata request per hit. The argument is `<base url>[?<parameters>]`: limit, page,
 * collection (items of that collection only). The Sources page Sync button sends the web site home = the default
 * listing. Each item yields one row per MP3 file (Ogg Vorbis files when the item has no MP3), at most 40. A song key
 * is `<identifier>/<creator>/<track title>`. Playing it asks /metadata/<identifier> for the file with that title and
 * returns https://archive.org/download/<identifier>/<file> so no file name is stored. HTTPS only: not playable on the
 * Amiga until I725i. SELECTA_ARCHIVEORG_BASE replaces the base (default https://archive.org; tests point it at a
 * fixture). */
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

#define DEFAULT_BASE "https://archive.org"
#define SITE_HOME "https://archive.org/"
#define ITEM_PAGE SITE_HOME "details/"
#define USER_AGENT "Selecta-ArchiveOrg/1 (+https://github.com/spotUP/selecta; music player, descriptive agent per archive.org/services/docs/api/bots.html)"
#define ID_MAX 100u
#define URL_BYTES 1100u
#define PAUSE_MS 500u
#define SEARCH_CAP (256u * 1024u)
#define META_CAP (2u * 1024u * 1024u)
#define FILES_PER_ITEM 40u

static const sel_tag g_tags[] = {
    {SEL_TAG_END, 0},
};

/* One search hit or one metadata file entry: the strings the parsers keep. */
typedef struct rec {
    char id[ID_MAX];
    char title[120];
    char creator[96];
    char license[96];
    char name[260];
    char format[40];
} rec;

static char* field_of(rec* t, const char* key, size_t* n) {
    if (strcmp(key, "identifier") == 0) {
        *n = sizeof t->id;
        return t->id;
    }
    if (strcmp(key, "title") == 0) {
        *n = sizeof t->title;
        return t->title;
    }
    if (strcmp(key, "creator") == 0) {
        *n = sizeof t->creator;
        return t->creator;
    }
    if (strcmp(key, "licenseurl") == 0) {
        *n = sizeof t->license;
        return t->license;
    }
    if (strcmp(key, "name") == 0) {
        *n = sizeof t->name;
        return t->name;
    }
    if (strcmp(key, "format") == 0) {
        *n = sizeof t->format;
        return t->format;
    }
    return NULL;
}

/* One object, its '{' consumed. Strings named in field_of are kept; every other value is skipped (a creator array keeps its first string). */
static int read_rec(FILE* f, rec* t) {
    char key[24];
    int c;

    memset(t, 0, sizeof *t);
    for (;;) {
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
            size_t n = 0;
            char* dst = field_of(t, key, &n);

            if (!sel_json_read_string(f, dst, dst != NULL ? n : 0)) {
                return 0;
            }
            c = sel_json_skip_ws(f);
            if (c == '}') {
                return 1;
            }
        } else if (c == '[' && strcmp(key, "creator") == 0) {
            /* The API answers a creator either as a string or as an array of strings: the first one names the row. */
            int first = 1;

            while ((c = sel_json_skip_ws(f)) != EOF && c != ']') {
                if (c == ',') {
                    continue;
                }
                if (c == '"' && first) {
                    first = 0;
                    if (!sel_json_read_string(f, t->creator, sizeof t->creator)) {
                        return 0;
                    }
                } else {
                    int end = 0;

                    if (!sel_json_skip_value(f, c, &end) || end == ']') {
                        break;
                    }
                }
            }
            if (c == EOF) {
                return 0;
            }
            c = sel_json_skip_ws(f);
            if (c == '}') {
                return 1;
            }
        } else {
            int end = 0;

            if (c == EOF || !sel_json_skip_value(f, c, &end)) {
                return 0;
            }
            if (end == '}') {
                return 1;
            }
        }
    }
}

typedef void (*rec_fn)(void* ctx, rec* t);

/* Walks the array `array_key` (docs, files) found at depth 1, or inside the object `outer` (NULL: top level), calling
 * `each` per element object. Everything else is skipped. */
static void read_array(FILE* f, const char* outer, const char* array_key, rec_fn each, void* ctx) {
    char key[24];
    rec t;
    int c, depth = 0;

    if (sel_json_skip_ws(f) != '{') {
        return;
    }
    for (;;) {
        c = sel_json_skip_ws(f);
        if (c == ',') {
            continue;
        }
        if (c == '}' || c == EOF) {
            return;
        }
        if (c != '"' || !sel_json_read_string(f, key, sizeof key) || sel_json_skip_ws(f) != ':') {
            return;
        }
        c = sel_json_skip_ws(f);
        if (outer != NULL && depth == 0 && strcmp(key, outer) == 0 && c == '{') {
            depth = 1; /* descend: the next keys belong to the outer object */
            continue;
        }
        if (strcmp(key, array_key) == 0 && c == '[' && (outer == NULL || depth == 1)) {
            while ((c = sel_json_skip_ws(f)) != EOF && c != ']') {
                if (c == ',') {
                    continue;
                }
                if (c != '{' || !read_rec(f, &t)) {
                    return;
                }
                each(ctx, &t);
            }
            return;
        } else {
            int end = 0;

            if (c == EOF || !sel_json_skip_value(f, c, &end)) {
                return;
            }
            if (end == '}') {
                return;
            }
        }
    }
}

static const char* api_base(void) {
    const char* env = getenv("SELECTA_ARCHIVEORG_BASE");

    return env != NULL && env[0] != '\0' ? env : DEFAULT_BASE;
}

/* GET `url` through the host (descriptive User-Agent, 429 / Retry-After back-off) into `path`. Returns the status. */
static int fetch(const sel_host* host, const sel_net* net, const char* url, const char* path, uint32_t cap,
                 uint32_t* requests, unsigned long* bytes) {
    uint32_t got = 0;
    void* buf = malloc(cap);
    FILE* f;
    int status;

    if (buf == NULL) {
        return 0;
    }
    status = net->request_ex(host, url, USER_AGENT, NULL, buf, cap, &got, requests);
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

/* Identifiers are letters, digits and "-_.". */
static int id_ok(const char* id) {
    size_t i;

    for (i = 0; id[i] != '\0'; i++) {
        if (!isalnum((unsigned char)id[i]) && id[i] != '-' && id[i] != '_' && id[i] != '.') {
            return 0;
        }
    }
    return i > 0 && i < ID_MAX;
}

/* The track title of a file entry: its title field, else the file name without folder and extension. Cleaned. */
static void track_title(const rec* t, char* out, size_t n) {
    const char* base = strrchr(t->name, '/');
    char* dot;

    if (t->title[0] != '\0') {
        snprintf(out, n, "%s", t->title);
    } else {
        snprintf(out, n, "%s", base != NULL ? base + 1 : t->name);
        dot = strrchr(out, '.');
        if (dot != NULL && dot != out) {
            *dot = '\0';
        }
    }
    clean_part(out);
}

/* The format ranks of a file entry: 2 = an MP3, 1 = Ogg Vorbis, 0 = anything else. */
static int audio_rank(const rec* t) {
    if (strstr(t->format, "MP3") != NULL) {
        return 2;
    }
    return strcmp(t->format, "Ogg Vorbis") == 0 ? 1 : 0;
}

/* File entries of one metadata answer: the best rank seen so far decides which files are kept. */
typedef struct files {
    const char* want_title; /* play: the track title to find; NULL while syncing */
    int best;               /* highest audio_rank seen */
    unsigned count;         /* entries of rank `best` kept */
    rec keep[FILES_PER_ITEM];
    char found[260];        /* play: the file name of the wanted title */
    int found_rank;
} files;

static void take_file(void* ctx, rec* t) {
    files* fl = (files*)ctx;
    int rk = audio_rank(t);

    if (rk == 0 || t->name[0] == '\0') {
        return;
    }
    if (fl->want_title != NULL) {
        char title[120];

        track_title(t, title, sizeof title);
        if (strcmp(title, fl->want_title) == 0 && rk > fl->found_rank) {
            snprintf(fl->found, sizeof fl->found, "%s", t->name);
            fl->found_rank = rk;
        }
        return;
    }
    if (rk > fl->best) {
        fl->best = rk;
        fl->count = 0;
    }
    if (rk == fl->best && fl->count < FILES_PER_ITEM) {
        fl->keep[fl->count++] = *t;
    }
}

typedef struct run {
    const sel_host* host;
    const sel_net* net;
    const sel_catalogue* cat;
    char dir[URL_BYTES - 64u];
    char base[URL_BYTES];
    uint32_t rows, skipped, requests, items;
    unsigned long bytes;
} run;

static void add_row(run* r, const rec* hit, const rec* file, const char* lic) {
    char path[ID_MAX + 240u], format[128], title[120], artist[96];
    sel_catalogue_song song;

    track_title(file, title, sizeof title);
    snprintf(artist, sizeof artist, "%s", hit->creator[0] != '\0' ? hit->creator : "unknown");
    clean_part(artist);
    snprintf(path, sizeof path, "%s/%s/%s", hit->id, artist, title);
    snprintf(format, sizeof format, "Internet Archive - %s", lic);
    memset(&song, 0, sizeof song);
    song.path = path;
    song.format = format;
    song.artist = artist;
    song.album = hit->title;
    song.name = title;
    song.md5 = "";
    if (!r->cat->song(r->host, &song)) {
        r->skipped++;
        return;
    }
    r->rows++;
}

/* One search hit: licence gate, then the item's metadata for its audio files. */
static void take_hit(void* ctx, rec* hit) {
    run* r = (run*)ctx;
    char url[URL_BYTES], path[URL_BYTES], lic[64];
    files* fl;
    FILE* f;
    unsigned i;

    if (!id_ok(hit->id) || !sel_license_is_open(hit->license)) {
        r->skipped++; /* no licence, or one that is not Creative Commons / public domain */
        return;
    }
    clean_part(hit->title);
    r->net->pause(r->host, PAUSE_MS);
    snprintf(url, sizeof url, "%s/metadata/%s", r->base, hit->id);
    snprintf(path, sizeof path, "%s/meta.json", r->dir);
    r->items++;
    if (fetch(r->host, r->net, url, path, META_CAP, &r->requests, &r->bytes) != 200) {
        remove(path);
        r->skipped++;
        return;
    }
    fl = (files*)calloc(1, sizeof *fl);
    f = fl != NULL ? fopen(path, "rb") : NULL;
    if (f == NULL) {
        free(fl);
        remove(path);
        r->skipped++;
        return;
    }
    read_array(f, NULL, "files", take_file, fl);
    fclose(f);
    remove(path);
    sel_license_name(hit->license, lic, sizeof lic);
    if (fl->count == 0) {
        r->skipped++; /* no MP3 or Ogg Vorbis derivative */
    }
    for (i = 0; i < fl->count; i++) {
        add_row(r, hit, &fl->keep[i], lic);
    }
    free(fl);
}

/* The value of `name` in `query` up to '&' (letters, digits, "-_." only; else empty). */
static void param_of(const char* query, const char* name, char* out, size_t n) {
    size_t ln = strlen(name), i = 0;
    const char* p = query;

    out[0] = '\0';
    while (p != NULL && *p != '\0') {
        if (strncmp(p, name, ln) == 0 && p[ln] == '=') {
            for (p += ln + 1; *p != '\0' && *p != '&' && i + 1 < n; p++) {
                if (!isalnum((unsigned char)*p) && *p != '-' && *p != '_' && *p != '.') {
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

static int ao_sync(const sel_host* host, const char* arg, int64_t now, sel_tag* stats, uint32_t cap) {
    const char* query = strchr(arg, '?');
    const char* q = query != NULL ? query + 1 : "";
    size_t bl = query != NULL ? (size_t)(query - arg) : strlen(arg);
    char url[URL_BYTES], path[URL_BYTES], limit[8], page[8], coll[64];
    uint32_t rq = 0;
    unsigned long got = 0;
    run r;
    FILE* f;
    int ok = 0;

    (void)now;
    memset(&r, 0, sizeof r);
    r.host = host;
    r.net = host->net != NULL ? host->net(host) : NULL;
    r.cat = host->catalogue != NULL ? host->catalogue(host) : NULL;
    while (bl > 0 && arg[bl - 1] == '/') {
        bl--;
    }
    if (bl == 0 || (bl >= sizeof SITE_HOME - 2u && strncmp(arg, SITE_HOME, sizeof SITE_HOME - 2u) == 0)) {
        snprintf(r.base, sizeof r.base, "%s", api_base());
    } else if (bl < sizeof r.base) {
        snprintf(r.base, sizeof r.base, "%.*s", (int)bl, arg);
    } else {
        return 0;
    }
    if (r.net == NULL || r.net->request_ex == NULL || r.cat == NULL ||
        !r.cat->cache_dir(host, "archiveorg", r.dir, sizeof r.dir) || !r.cat->begin(host, "archiveorg")) {
        return 0;
    }
    param_of(q, "limit", limit, sizeof limit);
    param_of(q, "page", page, sizeof page);
    param_of(q, "collection", coll, sizeof coll);
    /* mediatype:audio, a licence that names Creative Commons or the public domain, optionally one collection. */
    snprintf(url, sizeof url,
             "%s/advancedsearch.php?q=mediatype%%3Aaudio%%20AND%%20%%28licenseurl%%3A%%2Acreativecommons.org%%2A"
             "%%20OR%%20licenseurl%%3A%%2Apublicdomain%%2A%%29%s%s&fl%%5B%%5D=identifier&fl%%5B%%5D=title"
             "&fl%%5B%%5D=creator&fl%%5B%%5D=licenseurl&sort%%5B%%5D=downloads%%20desc&rows=%s&page=%s&output=json",
             r.base, coll[0] != '\0' ? "%20AND%20collection%3A" : "", coll, limit[0] != '\0' ? limit : "25",
             page[0] != '\0' ? page : "1");
    snprintf(path, sizeof path, "%s/search.json", r.dir);
    if (fetch(host, r.net, url, path, SEARCH_CAP, &rq, &got) == 200 && (f = fopen(path, "rb")) != NULL) {
        r.requests = rq;
        r.bytes = got;
        read_array(f, "response", "docs", take_hit, &r);
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
    sel_sync_stat(stats, cap, SEL_SYNC_NET_BYTES, (uint32_t)r.bytes);
    sel_sync_stat(stats, cap, SEL_SYNC_NET_REQUESTS, r.requests);
    return ok;
}

/* Play: GET <base>/metadata/<id>, find the file whose track title matches, hand its download URL to sel_live. */
static int ao_play_url(const sel_host* host, const char* path, char* out, size_t n) {
    const sel_net* net = host->net != NULL ? host->net(host) : NULL;
    const sel_catalogue* cat = host->catalogue != NULL ? host->catalogue(host) : NULL;
    const char* a = strchr(path, '/');
    const char* b = a != NULL ? strchr(a + 1, '/') : NULL;
    size_t il = a != NULL ? (size_t)(a - path) : 0;
    char dir[URL_BYTES - 64u], file[URL_BYTES], url[URL_BYTES], id[ID_MAX], rel[300], title[120];
    uint32_t rq = 0;
    unsigned long got = 0;
    files* fl;
    FILE* f;
    int ok = 0;

    if (net == NULL || net->request_ex == NULL || cat == NULL || il == 0 || il >= sizeof id || b == NULL ||
        b[1] == '\0') {
        return 0;
    }
    memcpy(id, path, il);
    id[il] = '\0';
    if (!id_ok(id) || !cat->cache_dir(host, "archiveorg", dir, sizeof dir)) {
        return 0;
    }
    snprintf(url, sizeof url, "%s/metadata/%s", api_base(), id);
    snprintf(file, sizeof file, "%s/play.json", dir);
    if (fetch(host, net, url, file, META_CAP, &rq, &got) != 200) {
        remove(file);
        return 0;
    }
    fl = (files*)calloc(1, sizeof *fl);
    f = fl != NULL ? fopen(file, "rb") : NULL;
    if (f == NULL) {
        free(fl);
        remove(file);
        return 0;
    }
    snprintf(title, sizeof title, "%s", b + 1);
    fl->want_title = title;
    read_array(f, NULL, "files", take_file, fl);
    fclose(f);
    remove(file);
    if (fl->found[0] != '\0') {
        snprintf(rel, sizeof rel, "%s/%s", id, fl->found);
        /* The download host is the base's: https://archive.org/download/<id>/<file>, percent-encoded. */
        snprintf(url, sizeof url, "%s/download/", api_base());
        ok = sel_url_path_encode(url, rel, out, n);
    }
    free(fl);
    return ok;
}

/* The item page backlink. */
static int url_of(const char* path, char* out, size_t n) {
    const char* slash = strchr(path, '/');
    size_t il = slash != NULL ? (size_t)(slash - path) : 0;

    if (il == 0 || il >= ID_MAX) {
        return 0;
    }
    return snprintf(out, n, ITEM_PAGE "%.*s", (int)il, path) < (int)n;
}

static int parse(const char* path, char* scratch, size_t n, sel_tag* out) {
    static const char fmt[] = "Internet Archive";
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

static const sel_source g_archiveorg = {
    "archiveorg", "Internet Archive", "https://archive.org/",
    "Creative Commons and public-domain audio streamed from the Internet Archive (scholarship and research use).",
    g_tags, NULL, parse, NULL, url_of, NULL, NULL, NULL, ao_sync, SEL_SOURCE_REMOTE | SEL_SOURCE_SYNCS | SEL_SOURCE_STREAMS,
    SITE_HOME, ao_play_url, NULL,
};

const sel_tag* SEL_PLUGIN_ENTRY(uint32_t host_api) {
    (void)host_api;
    return sel_source_plugin_descriptor("archiveorg", &g_archiveorg);
}

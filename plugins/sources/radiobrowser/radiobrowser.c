/* The `radiobrowser` source descriptor (row I725c; plan 2026-10-30-open-streaming-sources, row 4): the community
 * directory of internet radio stations, https://www.radio-browser.info/. Terms read 2026-10-30 at
 * https://api.radio-browser.info: "You may use it in free and non free software." / "Send a speaking http agent
 * string (e.g. mycoolapp/1.4)" (the shared client sends Selecta/<version>, I725p) / "Do a DNS-lookup of
 * 'all.api.radio-browser.info'. This gives you a list of all available servers." / "Never use a direct link to a
 * single new server." / "Send /json/url requests for every click the user makes".
 *
 * A "sync" is one station search: the argument is `<base url>[?<search parameters>]` (name, tag, country,
 * countrycode, language, order, limit; the codec is added here). Every MP3 and Ogg station found becomes a library
 * row; other codecs are skipped until their decoders exist (I725h). A song key is `radiobrowser:<stationuuid>/<name>`.
 * Playing it (SEL_SOURCE_STREAMS) asks /json/url/<stationuuid>, which is the click count the API asks for and answers
 * with the station's resolved stream URL; the engine plays that URL through the one stream path (sel_live, ICY). */
#include <selecta/catalogue.h>
#include <selecta/net.h>
#include <selecta/plugin.h>
#include <selecta/source.h>
#include "source/descriptor.h"
#include "source/json_scan.h"
#include "source/sync_stat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_BASE "https://all.api.radio-browser.info"
#define SITE_HOME "https://www.radio-browser.info"
#define SITE SITE_HOME "/history/"
#define UUID_MAX 40u
#define URL_BYTES 1100u
#define PAUSE_MS 500u

static const sel_tag g_tags[] = {
    {SEL_TAG_END, 0},
};

typedef struct station {
    char uuid[UUID_MAX];
    char name[120];
    char codec[16];
    char country[48];
    char tags[96];
    char url[URL_BYTES - 64u];
} station;

static char* field_of(station* st, const char* key, size_t* n) {
    if (strcmp(key, "stationuuid") == 0) {
        *n = sizeof st->uuid;
        return st->uuid;
    }
    if (strcmp(key, "name") == 0) {
        *n = sizeof st->name;
        return st->name;
    }
    if (strcmp(key, "codec") == 0) {
        *n = sizeof st->codec;
        return st->codec;
    }
    if (strcmp(key, "country") == 0) {
        *n = sizeof st->country;
        return st->country;
    }
    if (strcmp(key, "tags") == 0) {
        *n = sizeof st->tags;
        return st->tags;
    }
    if (strcmp(key, "url_resolved") == 0 || strcmp(key, "url") == 0) { /* the click answer names the stream "url" */
        *n = sizeof st->url;
        return st->url;
    }
    return NULL;
}

/* One object, its '{' consumed. Strings only; numbers and nested values are skipped. 0 on malformed input. */
static int read_station(FILE* f, station* st) {
    char key[24];
    int c;

    memset(st, 0, sizeof *st);
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
            char* dst = field_of(st, key, &n);
            int keep = dst != NULL && !(strcmp(key, "url") == 0 && st->url[0] != '\0'); /* url_resolved wins */

            if (!sel_json_read_string(f, keep ? dst : NULL, keep ? n : 0)) {
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

static int is_mp3(const char* codec) {
    return strcmp(codec, "MP3") == 0 || strcmp(codec, "mp3") == 0;
}

static int is_ogg(const char* codec) {
    return strcmp(codec, "OGG") == 0 || strcmp(codec, "ogg") == 0 || strcmp(codec, "Ogg") == 0;
}

/* A path is `<uuid>/<name>`; the name carries no '/' and no control byte. */
static void clean_name(char* s) {
    for (; *s != '\0'; s++) {
        if (*s == '/' || *s == '|' || (unsigned char)*s < 32u) {
            *s = ' ';
        }
    }
}

typedef struct run {
    const sel_host* host;
    const sel_catalogue* cat;
    uint32_t rows, skipped;
} run;

static void add_station(run* r, station* st) {
    char path[UUID_MAX + 130u], artist[160];
    sel_catalogue_song song;

    if (st->uuid[0] == '\0' || st->name[0] == '\0' || st->url[0] == '\0' || (!is_mp3(st->codec) && !is_ogg(st->codec)) ||
        strchr(st->uuid, '/') != NULL) {
        r->skipped++;
        return;
    }
    clean_name(st->name);
    snprintf(path, sizeof path, "%s/%s", st->uuid, st->name);
    snprintf(artist, sizeof artist, "%s%s%s", st->country, st->country[0] != '\0' && st->tags[0] != '\0' ? " - " : "",
             st->tags);
    memset(&song, 0, sizeof song);
    song.path = path;
    song.format = is_mp3(st->codec) ? "Radio - MP3" : "Radio - Ogg";
    song.artist = artist;
    song.album = "";
    song.name = st->name;
    song.md5 = "";
    if (!r->cat->song(r->host, &song)) {
        r->skipped++;
        return;
    }
    r->rows++;
}

static void read_page(FILE* f, run* r) {
    station st;
    int c;

    if (sel_json_skip_ws(f) != '[') {
        return;
    }
    while ((c = sel_json_skip_ws(f)) != EOF && c != ']') {
        if (c == ',') {
            continue;
        }
        if (c != '{' || !read_station(f, &st)) {
            break;
        }
        add_station(r, &st);
    }
}

static int has_param(const char* query, const char* name) {
    size_t n = strlen(name);
    const char* p = query;

    while (p != NULL && *p != '\0') {
        if (strncmp(p, name, n) == 0 && p[n] == '=') {
            return 1;
        }
        p = strchr(p, '&');
        if (p != NULL) {
            p++;
        }
    }
    return 0;
}

/* LIBRARY_SYNC radiobrowser <base>[?query]: one search per codec (MP3, Ogg) unless the query names a codec. */
/* The API server: SELECTA_RADIO_BROWSER (a mirror of your own, or the test fixture) or the pool name all.api.radio-browser.info. */
static const char* api_base(void) {
    const char* env = getenv("SELECTA_RADIO_BROWSER");

    return env != NULL && env[0] != '\0' ? env : DEFAULT_BASE;
}

static int rb_sync(const sel_host* host, const char* arg, int64_t now, sel_tag* stats, uint32_t cap) {
    const sel_net* net = host->net != NULL ? host->net(host) : NULL;
    const sel_catalogue* cat = host->catalogue != NULL ? host->catalogue(host) : NULL;
    char dir[URL_BYTES - 64u], base[URL_BYTES], url[URL_BYTES], path[URL_BYTES];
    const char* query = strchr(arg, '?');
    const char* q = query != NULL ? query + 1 : "";
    size_t bl = query != NULL ? (size_t)(query - arg) : strlen(arg);
    static const char* const codecs[] = {"mp3", "ogg"};
    unsigned k, pages = has_param(q, "codec") ? 1u : 2u;
    run r;
    uint32_t requests = 0, bytes = 0;
    int ok = 1;

    (void)now;
    memset(&r, 0, sizeof r);
    r.host = host;
    r.cat = cat;
    while (bl > 0 && arg[bl - 1] == '/') {
        bl--;
    }
    if (bl == 0 || (bl >= sizeof SITE_HOME - 1u && strncmp(arg, SITE_HOME, sizeof SITE_HOME - 1u) == 0)) {
        /* The Sources page Sync button sends the source's home (the web site, no API): use the API pool name. */
        snprintf(base, sizeof base, "%s", api_base());
    } else if (bl < sizeof base) {
        snprintf(base, sizeof base, "%.*s", (int)bl, arg);
    } else {
        return 0;
    }
    if (net == NULL || cat == NULL || !cat->cache_dir(host, "radiobrowser", dir, sizeof dir) ||
        !cat->begin(host, "radiobrowser")) {
        return 0;
    }
    for (k = 0; k < pages && ok; k++) {
        uint32_t rq = 0;
        unsigned long got = 0;
        FILE* f;

        snprintf(path, sizeof path, "%s/search%u.json", dir, k);
        snprintf(url, sizeof url, "%s/json/stations/search?%s%shidebroken=true&order=clickcount&reverse=true%s%s%s", base, q,
                 q[0] != '\0' ? "&" : "", has_param(q, "limit") ? "" : "&limit=100", pages == 2u ? "&codec=" : "",
                 pages == 2u ? codecs[k] : "");
        if (k > 0) {
            net->pause(host, PAUSE_MS);
        }
        if (net->get_file(host, url, path, &rq, &got) != 200) {
            requests += rq;
            ok = 0;
            break;
        }
        requests += rq;
        bytes += (uint32_t)got;
        f = fopen(path, "rb");
        if (f == NULL) {
            ok = 0;
            break;
        }
        read_page(f, &r);
        fclose(f);
        remove(path);
    }
    if (ok) {
        ok = cat->commit(host);
    } else {
        cat->abort(host);
    }
    sel_sync_stat(stats, cap, SEL_SYNC_LISTING_ROWS, r.rows);
    sel_sync_stat(stats, cap, SEL_SYNC_SKIPPED_ROWS, r.skipped);
    sel_sync_stat(stats, cap, SEL_SYNC_NET_BYTES, bytes);
    sel_sync_stat(stats, cap, SEL_SYNC_NET_REQUESTS, requests);
    return ok;
}

/* Play: GET <base>/json/url/<stationuuid>. The API counts that as the click and answers {"ok":..,"url":"<stream>"}.
 * The base is SELECTA_RADIO_BROWSER when set (tests, a chosen mirror), else the discovery name. */
static int rb_play_url(const sel_host* host, const char* path, char* out, size_t n) {
    const sel_net* net = host->net != NULL ? host->net(host) : NULL;
    const sel_catalogue* cat = host->catalogue != NULL ? host->catalogue(host) : NULL;
    const char* slash = strchr(path, '/');
    size_t ul = slash != NULL ? (size_t)(slash - path) : strlen(path);
    char dir[URL_BYTES - 64u], file[URL_BYTES], url[URL_BYTES];
    uint32_t rq = 0;
    unsigned long got = 0;
    station st;
    FILE* f;
    int ok = 0;

    if (net == NULL || cat == NULL || ul == 0 || ul >= UUID_MAX || !cat->cache_dir(host, "radiobrowser", dir, sizeof dir)) {
        return 0;
    }
    snprintf(url, sizeof url, "%s/json/url/%.*s", api_base(), (int)ul, path);
    snprintf(file, sizeof file, "%s/click.json", dir);
    if (net->get_file(host, url, file, &rq, &got) != 200 || (f = fopen(file, "rb")) == NULL) {
        return 0;
    }
    if (sel_json_skip_ws(f) == '{' && read_station(f, &st) && st.url[0] != '\0') {
        ok = snprintf(out, n, "%s", st.url) < (int)n;
    }
    fclose(f);
    remove(file);
    return ok;
}

static int url_of(const char* path, char* out, size_t n) {
    const char* slash = strchr(path, '/');
    size_t ul = slash != NULL ? (size_t)(slash - path) : 0;

    if (ul == 0 || ul >= UUID_MAX) {
        return 0;
    }
    return snprintf(out, n, SITE "%.*s", (int)ul, path) < (int)n;
}

static int parse(const char* path, char* scratch, size_t n, sel_tag* out) {
    static const char fmt[] = "Radio";
    static const char artist[] = "- unknown";
    const char* name = path != NULL ? strchr(path, '/') : NULL;
    size_t nl;

    if (name == NULL || name[1] == '\0') {
        return 0;
    }
    name++;
    nl = strlen(name);
    if (sizeof fmt + sizeof artist + nl + 1u > n) {
        return 0;
    }
    memcpy(scratch, fmt, sizeof fmt);
    memcpy(scratch + sizeof fmt, artist, sizeof artist);
    memcpy(scratch + sizeof fmt + sizeof artist, name, nl + 1u);
    out[0].tag = SEL_SRC_FORMAT;
    out[0].value = (uintptr_t)scratch;
    out[1].tag = SEL_SRC_ARTIST;
    out[1].value = (uintptr_t)(scratch + sizeof fmt);
    out[2].tag = SEL_SRC_NAME;
    out[2].value = (uintptr_t)(scratch + sizeof fmt + sizeof artist);
    out[3].tag = SEL_TAG_END;
    out[3].value = 0;
    return 1;
}

static const sel_source g_radiobrowser = {
    "radiobrowser", "Radio Browser", "https://www.radio-browser.info/",
    "Internet radio stations from the community directory radio-browser.info: search by name, tag or country.",
    g_tags, NULL, parse, NULL, url_of, NULL, NULL, NULL, rb_sync, SEL_SOURCE_REMOTE | SEL_SOURCE_SYNCS | SEL_SOURCE_STREAMS,
    DEFAULT_BASE "/", rb_play_url,
};

const sel_tag* SEL_PLUGIN_ENTRY(uint32_t host_api) {
    (void)host_api;
    return sel_source_plugin_descriptor("radiobrowser", &g_radiobrowser);
}

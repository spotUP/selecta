/* The `jamendo` source descriptor (row I725e; plan 2026-10-30-open-streaming-sources, row 6): Creative Commons music
 * from https://www.jamendo.com/ through the v3.0 API. Terms read 2026-10-30 at
 * https://devportal.jamendo.com/api_terms_of_use:
 *   "The API may be used freely for non-commercial uses. For any other type of use including but not limited to
 *    commercial uses please contact our sales team at licensing@jamendo.com"
 *   "Commercial use shall be understood as any use that is intended for or directed toward commercial advantage or any
 *    monetary compensation, including any revenue arising from affiliation programs or advertising."
 *   "Applications must not be specifically designed to cache the content nor offering an offline access to the
 *    content. Caching system may only be used to the extent reasonably necessary for the operation of the Application."
 *   "The Developer commits to ensure that the Application credits the JAMENDO Members as the creators of the Content,
 *    credits JAMENDO as the provider of the Content and provide a direct backlink from each Content in the Application
 *    to the relevant Content's page on the JAMENDO Platform."
 * Consequences kept here: Selecta stays non-commercial (no ads, no affiliate links); the audio is only streamed
 * (SEL_SOURCE_STREAMS: the engine plays the URL through sel_live, nothing is written to the library or cache dir; the
 * only file this plugin writes is the small JSON answer of one API call, removed at once); every row carries the
 * creator (artist column), Jamendo and the Creative Commons licence (format column "Jamendo - CC BY-NC-SA 3.0") and
 * the source's url hook gives the track page backlink https://www.jamendo.com/track/<id> (the API's shareurl).
 *
 * Configuration: the client id of your own app from https://devportal.jamendo.com/ (free). Set pref
 * `sources.jamendo_client_id` in prefs.conf, or the environment variable SELECTA_JAMENDO_CLIENT_ID. Without one the
 * source is disabled: a sync fails with the line "jamendo: no client id" on stderr and plays nothing.
 * SELECTA_JAMENDO_API replaces the API base (default https://api.jamendo.com/v3.0; tests point it at a fixture).
 *
 * A "sync" is one /tracks request: the argument is `<base url>[?<parameters>]` (search, tags, order, limit, offset;
 * defaults: order=popularity_total, limit=100). The Sources page Sync button sends the web site home, which means the
 * default popular listing. A song key is `jamendo:<track id>/<artist>/<title>`. Playing it asks /tracks?id=<id> for a
 * fresh `audio` URL (audioformat=mp31, the 96 kbit MP3 stream) so no stream URL is stored. HTTPS only: not playable
 * on the Amiga until I725i. */
#include <selecta/catalogue.h>
#include <selecta/host.h>
#include <selecta/net.h>
#include <selecta/plugin.h>
#include <selecta/source.h>
#include "source/descriptor.h"
#include "source/json_scan.h"
#include "source/sync_stat.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_BASE "https://api.jamendo.com/v3.0"
#define SITE_HOME "https://www.jamendo.com/"
#define TRACK_PAGE SITE_HOME "track/"
#define ID_MAX 24u
#define URL_BYTES 1100u
#define CLIENT_MAX 48u

static const sel_tag g_tags[] = {
    {SEL_TAG_END, 0},
};

typedef struct track {
    char id[ID_MAX];
    char name[120];
    char artist[96];
    char license[96];
    char audio[URL_BYTES - 64u];
} track;

static char* field_of(track* t, const char* key, size_t* n) {
    if (strcmp(key, "id") == 0) {
        *n = sizeof t->id;
        return t->id;
    }
    if (strcmp(key, "name") == 0) {
        *n = sizeof t->name;
        return t->name;
    }
    if (strcmp(key, "artist_name") == 0) {
        *n = sizeof t->artist;
        return t->artist;
    }
    if (strcmp(key, "license_ccurl") == 0) {
        *n = sizeof t->license;
        return t->license;
    }
    if (strcmp(key, "audio") == 0) {
        *n = sizeof t->audio;
        return t->audio;
    }
    return NULL;
}

/* One object, its '{' consumed. Strings are kept when named in field_of; every other value is skipped. */
static int read_track(FILE* f, track* t) {
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

/* The answer is {"headers":{..},"results":[{track}, ..]}: walks to "results" and calls `each` per track. */
typedef void (*track_fn)(void* ctx, track* t);

static void read_results(FILE* f, track_fn each, void* ctx) {
    char key[24];
    track t;
    int c;

    if (sel_json_skip_ws(f) != '{') {
        return;
    }
    for (;;) {
        c = sel_json_skip_ws(f);
        if (c == ',') {
            continue;
        }
        if (c != '"' || !sel_json_read_string(f, key, sizeof key) || sel_json_skip_ws(f) != ':') {
            return;
        }
        c = sel_json_skip_ws(f);
        if (strcmp(key, "results") == 0 && c == '[') {
            while ((c = sel_json_skip_ws(f)) != EOF && c != ']') {
                if (c == ',') {
                    continue;
                }
                if (c != '{' || !read_track(f, &t)) {
                    return;
                }
                each(ctx, &t);
            }
            return;
        } else {
            int end = 0;

            if (c == EOF || !sel_json_skip_value(f, c, &end) || end == '}') {
                return;
            }
        }
    }
}

/* "http://creativecommons.org/licenses/by-nc-sa/3.0/" -> "CC BY-NC-SA 3.0"; anything else -> "Creative Commons". */
static void license_name(const char* url, char* out, size_t n) {
    const char* p = strstr(url, "/licenses/");
    char kind[24], ver[12];
    size_t i = 0;

    snprintf(out, n, "Creative Commons");
    if (p == NULL) {
        return;
    }
    p += 10;
    while (*p != '\0' && *p != '/' && i + 1 < sizeof kind) {
        kind[i++] = (char)toupper((unsigned char)*p++);
    }
    kind[i] = '\0';
    ver[0] = '\0';
    if (*p == '/') {
        p++;
        for (i = 0; *p != '\0' && *p != '/' && i + 1 < sizeof ver;) {
            ver[i++] = *p++;
        }
        ver[i] = '\0';
    }
    if (kind[0] != '\0') {
        snprintf(out, n, "CC %s%s%s", kind, ver[0] != '\0' ? " " : "", ver);
    }
}

/* A key is `<id>/<artist>/<title>`: no '/', '|' or control byte inside a part. */
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
    return i > 0;
}

typedef struct run {
    const sel_host* host;
    const sel_catalogue* cat;
    uint32_t rows, skipped;
} run;

static void add_track(void* ctx, track* t) {
    run* r = (run*)ctx;
    char path[ID_MAX + 240u], format[128], lic[64];
    sel_catalogue_song song;

    if (!id_ok(t->id) || t->name[0] == '\0' || t->audio[0] == '\0') {
        r->skipped++;
        return;
    }
    clean_part(t->name);
    clean_part(t->artist);
    snprintf(path, sizeof path, "%s/%s/%s", t->id, t->artist[0] != '\0' ? t->artist : "unknown", t->name);
    license_name(t->license, lic, sizeof lic);
    snprintf(format, sizeof format, "Jamendo - %s", lic);
    memset(&song, 0, sizeof song);
    song.path = path;
    song.format = format;
    song.artist = t->artist;
    song.album = "";
    song.name = t->name;
    song.md5 = "";
    if (!r->cat->song(r->host, &song)) {
        r->skipped++;
        return;
    }
    r->rows++;
}

static const char* api_base(void) {
    const char* env = getenv("SELECTA_JAMENDO_API");

    return env != NULL && env[0] != '\0' ? env : DEFAULT_BASE;
}

/* The client id: pref sources.jamendo_client_id, else SELECTA_JAMENDO_CLIENT_ID. Letters and digits only. */
static int client_id(const sel_host* host, char* out, size_t n) {
    const char* v = host->config_get != NULL ? host->config_get(host, "sources.jamendo_client_id", NULL) : NULL;
    size_t i;

    if (v == NULL || v[0] == '\0') {
        v = getenv("SELECTA_JAMENDO_CLIENT_ID");
    }
    if (v == NULL || v[0] == '\0') {
        fputs("jamendo: no client id: set pref sources.jamendo_client_id (or SELECTA_JAMENDO_CLIENT_ID); get one at "
              "https://devportal.jamendo.com/\n",
              stderr);
        return 0;
    }
    for (i = 0; v[i] != '\0'; i++) {
        if (!isalnum((unsigned char)v[i]) || i + 1 >= n) {
            fputs("jamendo: the client id must be letters and digits only\n", stderr);
            return 0;
        }
        out[i] = v[i];
    }
    out[i] = '\0';
    return 1;
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

static int jm_sync(const sel_host* host, const char* arg, int64_t now, sel_tag* stats, uint32_t cap) {
    const sel_net* net = host->net != NULL ? host->net(host) : NULL;
    const sel_catalogue* cat = host->catalogue != NULL ? host->catalogue(host) : NULL;
    char cid[CLIENT_MAX], dir[URL_BYTES - 64u], base[URL_BYTES], url[URL_BYTES], path[URL_BYTES];
    const char* query = strchr(arg, '?');
    const char* q = query != NULL ? query + 1 : "";
    size_t bl = query != NULL ? (size_t)(query - arg) : strlen(arg);
    uint32_t rq = 0;
    unsigned long got = 0;
    run r;
    FILE* f;
    int ok = 0;

    (void)now;
    memset(&r, 0, sizeof r);
    r.host = host;
    r.cat = cat;
    if (!client_id(host, cid, sizeof cid)) {
        return 0;
    }
    while (bl > 0 && arg[bl - 1] == '/') {
        bl--;
    }
    if (bl == 0 || (bl >= sizeof SITE_HOME - 2u && strncmp(arg, SITE_HOME, sizeof SITE_HOME - 2u) == 0)) {
        snprintf(base, sizeof base, "%s", api_base());
    } else if (bl < sizeof base) {
        snprintf(base, sizeof base, "%.*s", (int)bl, arg);
    } else {
        return 0;
    }
    if (net == NULL || cat == NULL || !cat->cache_dir(host, "jamendo", dir, sizeof dir) || !cat->begin(host, "jamendo")) {
        return 0;
    }
    snprintf(path, sizeof path, "%s/tracks.json", dir);
    snprintf(url, sizeof url, "%s/tracks/?client_id=%s&format=json&audioformat=mp31%s%s%s%s", base, cid,
             q[0] != '\0' ? "&" : "", q, has_param(q, "limit") ? "" : "&limit=100",
             has_param(q, "order") ? "" : "&order=popularity_total");
    if (net->get_file(host, url, path, &rq, &got) == 200 && (f = fopen(path, "rb")) != NULL) {
        read_results(f, add_track, &r);
        fclose(f);
        ok = 1;
    }
    remove(path);
    if (ok) {
        ok = cat->commit(host);
    } else {
        cat->abort(host);
    }
    sel_sync_stat(stats, cap, SEL_SYNC_LISTING_ROWS, r.rows);
    sel_sync_stat(stats, cap, SEL_SYNC_SKIPPED_ROWS, r.skipped);
    sel_sync_stat(stats, cap, SEL_SYNC_NET_BYTES, (uint32_t)got);
    sel_sync_stat(stats, cap, SEL_SYNC_NET_REQUESTS, rq);
    return ok;
}

typedef struct pick {
    char audio[URL_BYTES - 64u];
} pick;

static void take_audio(void* ctx, track* t) {
    pick* p = (pick*)ctx;

    if (p->audio[0] == '\0') {
        snprintf(p->audio, sizeof p->audio, "%s", t->audio);
    }
}

/* Play: GET <base>/tracks/?id=<id>: the fresh mp31 stream URL, handed to the engine's stream path (sel_live). */
static int jm_play_url(const sel_host* host, const char* path, char* out, size_t n) {
    const sel_net* net = host->net != NULL ? host->net(host) : NULL;
    const sel_catalogue* cat = host->catalogue != NULL ? host->catalogue(host) : NULL;
    const char* slash = strchr(path, '/');
    size_t il = slash != NULL ? (size_t)(slash - path) : strlen(path);
    char cid[CLIENT_MAX], dir[URL_BYTES - 64u], file[URL_BYTES], url[URL_BYTES], id[ID_MAX];
    uint32_t rq = 0;
    unsigned long got = 0;
    pick p;
    FILE* f;
    int ok = 0;

    if (net == NULL || cat == NULL || il == 0 || il >= sizeof id) {
        return 0;
    }
    memcpy(id, path, il);
    id[il] = '\0';
    if (!id_ok(id) || !client_id(host, cid, sizeof cid) || !cat->cache_dir(host, "jamendo", dir, sizeof dir)) {
        return 0;
    }
    snprintf(url, sizeof url, "%s/tracks/?client_id=%s&format=json&audioformat=mp31&id=%s", api_base(), cid, id);
    snprintf(file, sizeof file, "%s/play.json", dir);
    memset(&p, 0, sizeof p);
    if (net->get_file(host, url, file, &rq, &got) != 200 || (f = fopen(file, "rb")) == NULL) {
        remove(file);
        return 0;
    }
    read_results(f, take_audio, &p);
    fclose(f);
    remove(file);
    if (p.audio[0] != '\0') {
        ok = snprintf(out, n, "%s", p.audio) < (int)n;
    }
    return ok;
}

/* The track page backlink (the API's shareurl is exactly this form). */
static int url_of(const char* path, char* out, size_t n) {
    const char* slash = strchr(path, '/');
    size_t il = slash != NULL ? (size_t)(slash - path) : 0;

    if (il == 0 || il >= ID_MAX) {
        return 0;
    }
    return snprintf(out, n, TRACK_PAGE "%.*s", (int)il, path) < (int)n;
}

static int parse(const char* path, char* scratch, size_t n, sel_tag* out) {
    static const char fmt[] = "Jamendo";
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

static const sel_source g_jamendo = {
    "jamendo", "Jamendo", "https://www.jamendo.com/",
    "Creative Commons music streamed from Jamendo (non-commercial use; needs your own free API client id).",
    g_tags, NULL, parse, NULL, url_of, NULL, NULL, NULL, jm_sync, SEL_SOURCE_REMOTE | SEL_SOURCE_SYNCS | SEL_SOURCE_STREAMS,
    SITE_HOME, jm_play_url,
};

const sel_tag* SEL_PLUGIN_ENTRY(uint32_t host_api) {
    (void)host_api;
    return sel_source_plugin_descriptor("jamendo", &g_jamendo);
}

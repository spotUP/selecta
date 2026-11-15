/* Online cover lookup (I1003a, design thoughts/shared/plans/2026-11-09-artwork-scraper-design.md).
 *
 * A background pass over the albums that have no 'cover' artwork row. Per album: MusicBrainz release search by album +
 * artist, the Cover Art Archive front image of the best release, else the iTunes Search API. A hit is stored as the
 * album's 'cover' row, the row the tiles and Now Playing already draw; a definite miss as a 'scrape_miss' row holding
 * the epoch seconds, so the album is not asked again for SEL_ART_SCRAPE_RETRY_DAYS. A network failure records nothing
 * and ends the pass; the next kick (a scan, the next start) tries again.
 *
 * Threads: runs on the library job thread (job.c), one album per turn, never on the UI or engine thread.
 * Without threads: sel_library_scrape_poll does one album per call from the engine idle tick (the hash pass pattern). */
#include "db_internal.h"

#include "../net/http.h"
#include "../net/net.h"
#include "../../core/pal.h"
#include "../song_key.h"
#include "selecta/platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define SCRAPE_IMAGE_MAX (1024u * 1024u)
#define SCRAPE_JSON_MAX 16384u
#define SCRAPE_URL_MAX 2048u
#define SCRAPE_MB_GAP_MS 1100u /* MusicBrainz allows one request per second */
#define SCRAPE_MIN_SCORE 90
#define SCRAPE_UA "Selecta/" SEL_VERSION_STRING " ( https://hippoplayer.se )"

#if !defined(SEL_LIBRARY_NO_THREAD)
#define SCR_QUIT(l) ((l)->job_quit)
#else
#define SCR_QUIT(l) 0
#endif

typedef struct scrape_buf {
    uint8_t* data;
    size_t cap;
    size_t len;
} scrape_buf;

static int buf_sink(void* ctx, const void* data, size_t len) {
    scrape_buf* b = ctx;
    size_t room = b->cap - b->len;

    memcpy(b->data + b->len, data, len < room ? len : room); /* an overlong body is cut, never an abort: the ids come first */
    b->len += len < room ? len : room;
    return 1;
}

/* One GET into `b`. 1 = 200 with a body, 0 = a definite no (404 and other 4xx but 429), -1 = try again later
 * (no connection, 429, 5xx). */
static int scrape_get(const char* url, scrape_buf* b, sel_scrape_stats* st) {
    sel_http_req req;
    sel_http_res res;
    sel_http_err e;

    memset(&req, 0, sizeof req);
    memset(&res, 0, sizeof res);
    req.url = url;
    req.user_agent = SCRAPE_UA;
    b->len = 0;
    st->requests++;
    e = sel_http_request(&req, buf_sink, b, &res);
    if (e != SEL_HTTP_OK) return -1;
    if (res.status == 200 && b->len > 0) return 1;
    if (res.status == 200 || res.status == 404 || (res.status >= 400 && res.status < 500 && res.status != 429)) return 0;
    return -1;
}

static const char* base_url(const char* env, const char* fallback, char* out, size_t cap) {
    const char* v = getenv(env);
    size_t n;

    snprintf(out, cap, "%s", v != NULL && v[0] != '\0' ? v : fallback);
    n = strlen(out);
    while (n > 0 && out[n - 1] == '/') out[--n] = '\0';
    return out;
}

/* Waits out the MusicBrainz gap in slices, so a closing library does not wait a second for it. */
static void mb_gap(sel_library* lib) {
    unsigned long now = sel_net_now_ms();

    if (lib->scrape_mb_ms != 0) {
        unsigned long since = now - lib->scrape_mb_ms;

        while (since < SCRAPE_MB_GAP_MS && !SCR_QUIT(lib)) {
            sel_pal_sleep_ms(50);
            since += 50;
        }
    }
    lib->scrape_mb_ms = sel_net_now_ms();
}

static void clean_term(const char* in, char* out, size_t cap) {
    size_t n = 0;

    for (; *in != '\0' && n + 1 < cap; in++) out[n++] = (*in == '"' || *in == '\\') ? ' ' : *in; /* quotes would end the phrase */
    out[n] = '\0';
}

static int ci_contains(const char* hay, const char* needle) {
    size_t i, j;

    if (needle[0] == '\0') return 1;
    for (i = 0; hay[i] != '\0'; i++) {
        for (j = 0; needle[j] != '\0' && hay[i + j] != '\0'; j++) {
            char a = hay[i + j], b = needle[j];

            if (a >= 'A' && a <= 'Z') a = (char)(a + 32);
            if (b >= 'A' && b <= 'Z') b = (char)(b + 32);
            if (a != b) break;
        }
        if (needle[j] == '\0') return 1;
    }
    return 0;
}

/* After a quoted key: skips optional blanks, the colon and optional blanks; NULL when there is no colon. */
static const char* json_value(const char* p) {
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    if (*p != ':') return NULL;
    p++;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    return p;
}

/* Text of the string value of `"key":"..."` at or after `from` (NUL-terminated json). \/ is unescaped, other escapes are kept. */
static const char* json_str(const char* from, const char* key, char* out, size_t cap) {
    char pat[40];
    const char* p;
    size_t n = 0;

    snprintf(pat, sizeof pat, "\"%s\"", key);
    p = strstr(from, pat);
    if (p == NULL) return NULL;
    p = json_value(p + strlen(pat));
    if (p == NULL || *p != '"') return NULL;
    p++;
    while (*p != '\0' && *p != '"' && n + 1 < cap) {
        if (p[0] == '\\' && p[1] == '/') p++;
        out[n++] = *p++;
    }
    out[n] = '\0';
    return p;
}

static int is_image(const scrape_buf* b) {
    if (b->len < 8) return 0;
    return (b->data[0] == 0x89 && b->data[1] == 'P' && b->data[2] == 'N' && b->data[3] == 'G') || (b->data[0] == 0xFF && b->data[1] == 0xD8);
}

/* A lookup outcome: 1 = image in img, 0 = nothing found, -1 = network trouble (record nothing). */
static int from_musicbrainz(sel_library* lib, const char* album, const char* artist, scrape_buf* js, scrape_buf* img, sel_scrape_stats* st) {
    char base[256], caa[256], url[SCRAPE_URL_MAX], q[640], qe[1500], mbid[48], score[8];
    const char* p;
    int r;

    base_url("SELECTA_ART_MB_URL", "https://musicbrainz.org", base, sizeof base);
    base_url("SELECTA_ART_CAA_URL", "https://coverartarchive.org", caa, sizeof caa);
    snprintf(q, sizeof q, "release:\"%s\" AND artist:\"%s\"", album, artist);
    sel_url_encode(q, qe, sizeof qe);
    snprintf(url, sizeof url, "%s/ws/2/release/?query=%s&limit=1&fmt=json", base, qe);
    mb_gap(lib);
    r = scrape_get(url, js, st);
    if (r <= 0) return r;
    js->data[js->len < js->cap ? js->len : js->cap - 1u] = '\0';
    p = strstr((const char*)js->data, "\"releases\"");
    if (p == NULL || json_str(p, "id", mbid, sizeof mbid) == NULL || strlen(mbid) != 36u) return 0;
    {
        const char* sc = strstr(p, "\"score\"");

        if (sc == NULL) return 0;
        sc = json_value(sc + 7);
        if (sc == NULL) return 0;
        score[0] = '\0';
        snprintf(score, sizeof score, "%.3s", sc);
        if (atoi(score) < SCRAPE_MIN_SCORE) return 0;
    }
    snprintf(url, sizeof url, "%s/release/%s/front-500", caa, mbid);
    r = scrape_get(url, img, st);
    if (r <= 0) return r;
    return is_image(img);
}

static int from_itunes(const char* album, const char* artist, scrape_buf* js, scrape_buf* img, sel_scrape_stats* st) {
    char base[256], url[SCRAPE_URL_MAX], q[640], qe[1500], art[512], coll[256], *s;
    const char* p;
    int r;

    base_url("SELECTA_ART_ITUNES_URL", "https://itunes.apple.com", base, sizeof base);
    snprintf(q, sizeof q, "%s %s", artist, album);
    sel_url_encode(q, qe, sizeof qe);
    snprintf(url, sizeof url, "%s/search?term=%s&entity=album&limit=1", base, qe);
    r = scrape_get(url, js, st);
    if (r <= 0) return r;
    js->data[js->len < js->cap ? js->len : js->cap - 1u] = '\0';
    p = (const char*)js->data;
    /* The top hit must carry the album name: a wrong cover is worse than none. */
    if (json_str(p, "collectionName", coll, sizeof coll) == NULL || !ci_contains(coll, album)) return 0;
    if (json_str(p, "artworkUrl100", art, sizeof art) == NULL) return 0;
    s = strstr(art, "100x100");
    if (s != NULL) memcpy(s, "600x600", 7);
    r = scrape_get(art, img, st);
    if (r <= 0) return r;
    return is_image(img);
}

static int art_request(sel_library* lib, const sel_art_op* op) {
    sel_message reply;

    memset(&reply, 0, sizeof reply);
    if (!sel_library_request(lib, SEL_LIB_REQ_ART, op, &reply) || reply.argc < 1) return 0;
    return reply.args[0].v.i;
}

/* One album. 1 = looked one up (hit or recorded miss), 0 = none left, -1 = network trouble, stop the pass. */
static int scrape_step(sel_library* lib, int64_t* after, sel_scrape_stats* st) {
    uint8_t names[1200];
    sel_art_op op;
    scrape_buf js, img;
    char album[400], artist[400];
    const char* n;
    int64_t id;
    int r;

    memset(&op, 0, sizeof op);
    op.op = SEL_ART_NEXT_MISSING;
    op.album = *after;
    op.len = (uint32_t)(time(NULL) - (time_t)SEL_ART_SCRAPE_RETRY_DAYS * 86400);
    op.data = names;
    op.cap = sizeof names;
    id = art_request(lib, &op);
    if (id == 0) return 0;
    *after = id;
    n = (const char*)names;
    clean_term(n, album, sizeof album);
    clean_term(n + strlen(n) + 1u, artist, sizeof artist);
    js.data = malloc(SCRAPE_JSON_MAX);
    js.cap = SCRAPE_JSON_MAX;
    img.data = malloc(SCRAPE_IMAGE_MAX);
    img.cap = SCRAPE_IMAGE_MAX;
    if (js.data == NULL || img.data == NULL) {
        free(js.data);
        free(img.data);
        return -1;
    }
    st->albums++;
    r = from_musicbrainz(lib, album, artist, &js, &img, st);
    if (r == 0) r = from_itunes(album, artist, &js, &img, st);
    if (r == 1) {
        memset(&op, 0, sizeof op);
        op.op = SEL_ART_PUT;
        op.album = id;
        op.kind = "cover";
        op.data = img.data;
        op.len = (uint32_t)img.len;
        if (art_request(lib, &op)) st->scraped++;
    } else if (r == 0) {
        char stamp[24];

        snprintf(stamp, sizeof stamp, "%lu", (unsigned long)time(NULL));
        memset(&op, 0, sizeof op);
        op.op = SEL_ART_PUT;
        op.album = id;
        op.kind = "scrape_miss";
        op.data = (uint8_t*)stamp;
        op.len = (uint32_t)strlen(stamp);
        art_request(lib, &op);
        st->misses++;
    } else {
        st->errors++;
    }
    free(js.data);
    free(img.data);
    return r < 0 ? -1 : 1;
}

/* I1303c: Demozoo screenshots for one Modland song. Same fetch, cap, User-Agent and miss rule as the album pass; the rows hang off
 * sel_art_dz_id(key) (no albums row, so no album list shows them). Returns 1 done (hit, miss or nothing to do), -1 network trouble. */
#define DZ_SHOTS_MAX 8
#define DZ_IMAGE_MAX (2u * 1024u * 1024u) /* an image that fills the buffer was cut: skipped, never stored half */

static int dz_has(sel_library* lib, int id, const char* kind, uint8_t* out, uint32_t cap) {
    sel_art_op op;

    memset(&op, 0, sizeof op);
    op.op = SEL_ART_GET;
    op.album = id;
    op.kind = kind;
    op.data = out;
    op.cap = cap;
    return art_request(lib, &op);
}

static void dz_put(sel_library* lib, int id, const char* kind, const void* data, size_t len) {
    sel_art_op op;

    memset(&op, 0, sizeof op);
    op.op = SEL_ART_PUT;
    op.album = id;
    op.kind = kind;
    op.data = (uint8_t*)data;
    op.len = (uint32_t)len;
    art_request(lib, &op);
}

/* I1267: the family token the Demozoo lookup takes (hippoplayer.se demozooPlatforms.ts is the table behind it); NULL for a family with no machine. */
const char* sel_dz_family(uint32_t family) {
    switch (family) {
    case SEL_PLATFORM_FAMILY_AMIGA: return "amiga";
    case SEL_PLATFORM_FAMILY_COMMODORE_64: return "c64";
    case SEL_PLATFORM_FAMILY_ATARI: return "atari";
    case SEL_PLATFORM_FAMILY_ZX_SPECTRUM: return "zx";
    case SEL_PLATFORM_FAMILY_NES: return "nes";
    case SEL_PLATFORM_FAMILY_GAME_BOY: return "gb";
    case SEL_PLATFORM_FAMILY_SNES: return "snes";
    case SEL_PLATFORM_FAMILY_PC_ENGINE: return "pce";
    case SEL_PLATFORM_FAMILY_MSX: return "msx";
    case SEL_PLATFORM_FAMILY_MEGA_DRIVE: return "md";
    case SEL_PLATFORM_FAMILY_SEGA_MASTER_SYSTEM: return "sms";
    default: return NULL;
    }
}

/* I1267: the first production of an answer that is on the platform `fam` (its "families" array holds the token); with fam NULL the first
 * one. Returns the start of its "screenshots" key and sets *end to its "video_url" key (NULL when absent), or NULL when none matches. An
 * older server that ignores platform= sends no "families": nothing matches, so no image rather than a wrong platform. */
const char* sel_dz_pick(const char* js, const char* fam, const char** end) {
    const char* p = strstr(js, "\"production_id\"");
    char tok[16];

    if (fam != NULL && snprintf(tok, sizeof tok, "\"%s\"", fam) >= (int)sizeof tok) return NULL;
    while (p != NULL) {
        const char* next = strstr(p + 1, "\"production_id\"");
        const char* f = strstr(p, "\"families\"");
        const char *shots, *close;

        if (fam == NULL || (f != NULL && (next == NULL || f < next) && (close = strchr(f, ']')) != NULL && (shots = strstr(f, tok)) != NULL && shots < close)) {
            shots = strstr(p, "\"screenshots\"");
            if (shots != NULL && (next == NULL || shots < next)) {
                *end = strstr(shots, "\"video_url\"");
                if (*end != NULL && next != NULL && *end > next) *end = NULL;
                return shots;
            }
        }
        p = next;
    }
    return NULL;
}

int sel_dz_query(const char* key, const char* title, const char* artist, const char* platform, char* out, size_t cap) {
    char a[600], b[600];

    if (strncmp(key, "modland:pub/modules/", 20) == 0) {
        if (!sel_url_encode(key, a, sizeof a)) return 0;
        return snprintf(out, cap, "key=%s", a) < (int)cap;
    }
    if (title == NULL || artist == NULL || title[0] == '\0' || artist[0] == '\0' || !sel_url_encode(title, a, sizeof a) || !sel_url_encode(artist, b, sizeof b)) return 0;
    if (platform != NULL && platform[0] != '\0') return snprintf(out, cap, "title=%s&author=%s&platform=%s", a, b, platform) < (int)cap;
    return snprintf(out, cap, "title=%s&author=%s", a, b) < (int)cap;
}

static int dz_step(sel_library* lib, const char* key, sel_scrape_stats* st) {
    char base[256], url[SCRAPE_URL_MAX], ke[1500], meta[600], shot[SCRAPE_URL_MAX], kind[12], stamp[24];
    uint8_t seen[24];
    scrape_buf js, img;
    sel_art_op op;
    const char *p, *end = NULL, *fam = NULL;
    int id, alb, n = 0, r, shots_max = DZ_SHOTS_MAX;
    const char* cap_env = getenv("SELECTA_ART_DZ_SHOTS"); /* test knob: a live check fetches one image */

    if (cap_env != NULL && atoi(cap_env) > 0 && atoi(cap_env) < shots_max) shots_max = atoi(cap_env);
    id = sel_art_dz_id(key);
    memset(&op, 0, sizeof op);
    op.op = SEL_ART_FIND;
    op.ref = key;
    alb = art_request(lib, &op);
    if (alb != 0 && dz_has(lib, alb, "cover", NULL, 0) > 0) return 1; /* real art, or screenshots already cached */
    memset(meta, 0, sizeof meta);
    if (strncmp(key, "modland:pub/modules/", 20) != 0) { /* I1401: any other source is asked by title + artist from the songs row */
        memset(&op, 0, sizeof op);
        op.op = SEL_ART_SONG_META;
        op.ref = key;
        op.data = (uint8_t*)meta;
        op.cap = sizeof meta;
        if (art_request(lib, &op) <= 0) return 1; /* no title or no artist: nothing to ask */
    }
    memset(seen, 0, sizeof seen);
    if (dz_has(lib, id, "dz_miss", seen, sizeof seen - 1u) > 0 && (time_t)atol((const char*)seen) > time(NULL) - (time_t)SEL_ART_SCRAPE_RETRY_DAYS * 86400) return 1;
    js.data = malloc(SCRAPE_JSON_MAX);
    js.cap = SCRAPE_JSON_MAX - 1u;
    img.data = malloc(DZ_IMAGE_MAX);
    img.cap = DZ_IMAGE_MAX;
    if (js.data == NULL || img.data == NULL) {
        free(js.data);
        free(img.data);
        return -1;
    }
    base_url("SELECTA_ART_DZ_URL", "https://hippoplayer.se", base, sizeof base);
    if (strncmp(key, "modland:pub/modules/", 20) != 0) fam = sel_dz_family(sel_platform_of_name(key)); /* I1267: Modland keys stay as they were */
    if (!sel_dz_query(key, meta, meta + strlen(meta) + 1, fam, ke, sizeof ke)) {
        free(js.data);
        free(img.data);
        return 1;
    }
    snprintf(url, sizeof url, "%s/api/demozoo/lookup?%s", base, ke);
    st->albums++;
    r = scrape_get(url, &js, st);
    if (r == 1) {
        js.data[js.len] = '\0';
        p = strstr((const char*)js.data, "\"match\"");
        p = p != NULL ? json_value(p + 7) : NULL;
        if (p == NULL || strncmp(p, "true", 4) != 0) r = 0;
    }
    if (r == 1) { /* the first production (earliest release) on the song's platform only: its screenshots end where its video_url starts */
        p = sel_dz_pick((const char*)js.data, fam, &end);
        while (p != NULL && n < shots_max && !SCR_QUIT(lib) && (p = json_str(p, "url", shot, sizeof shot)) != NULL && (end == NULL || p < end)) {
            if (strncmp(shot, "http", 4) != 0) continue;
            r = scrape_get(shot, &img, st);
            if (r < 0) break;
            if (r == 1 && img.len < img.cap && is_image(&img)) {
                if (n == 0) strcpy(kind, "cover");
                else snprintf(kind, sizeof kind, "dz%d", n);
                dz_put(lib, id, kind, img.data, img.len);
                n++;
            }
        }
        if (n > 0) r = 1;
        else if (r >= 0) r = 0; /* r < 0 here: network trouble before anything was stored, record nothing */
        if (n > 0) {
            dz_put(lib, id, "dz_key", key, strlen(key));
            if (end != NULL && json_str(end, "video_url", shot, sizeof shot) != NULL && shot[0] != '\0') dz_put(lib, id, "dz_video", shot, strlen(shot));
            st->scraped++;
        }
    }
    if (r == 0) {
        snprintf(stamp, sizeof stamp, "%lu", (unsigned long)time(NULL));
        dz_put(lib, id, "dz_miss", stamp, strlen(stamp));
        st->misses++;
    } else if (r < 0) {
        st->errors++;
    }
    free(js.data);
    free(img.data);
    return r < 0 ? -1 : 1;
}

static int scrape_disabled(sel_library* lib) {
    const char* e = getenv("SELECTA_ART_SCRAPE");

    return lib->scrape_off || (e != NULL && (strcmp(e, "off") == 0 || strcmp(e, "0") == 0));
}

void sel_library_demozoo_set_enabled(sel_library* lib, int on) {
    if (lib != NULL) lib->dz_off = on ? 0 : 1;
}

void sel_library_scrape_set_enabled(sel_library* lib, int on) {
    if (lib != NULL) lib->scrape_off = on ? 0 : 1;
}

void sel_library_scrape_stats(sel_library* lib, sel_scrape_stats* out) {
    memset(out, 0, sizeof *out);
    if (lib == NULL) return;
#if !defined(SEL_LIBRARY_NO_THREAD)
    pthread_mutex_lock(&lib->job_lock);
#endif
    *out = lib->scrape_stats;
#if !defined(SEL_LIBRARY_NO_THREAD)
    pthread_mutex_unlock(&lib->job_lock);
#endif
}

static void add_stats(sel_scrape_stats* to, const sel_scrape_stats* d) {
    to->albums += d->albums;
    to->scraped += d->scraped;
    to->misses += d->misses;
    to->errors += d->errors;
    to->requests += d->requests;
}

#if !defined(SEL_LIBRARY_NO_THREAD)

void sel_scrape_job_run(sel_library* lib) {
    sel_scrape_stats d;
    int64_t after;
    int r;

    memset(&d, 0, sizeof d);
    if (lib->dz_key[0] != '\0') { /* I1303c: the song that just started goes before the album pass */
        char key[sizeof lib->dz_key];

        memcpy(key, lib->dz_key, sizeof key);
        lib->dz_key[0] = '\0';
        pthread_mutex_unlock(&lib->job_lock);
        dz_step(lib, key, &d);
        pthread_mutex_lock(&lib->job_lock);
        add_stats(&lib->scrape_stats, &d);
        lib->job_scrape = 1; /* the album pass (if one is running) goes on after it */
        return;
    }
    if (lib->scrape_pending) {
        lib->scrape_pending = 0;
        lib->scrape_after = 0;
    }
    after = lib->scrape_after;
    pthread_mutex_unlock(&lib->job_lock);
    r = scrape_step(lib, &after, &d);
    pthread_mutex_lock(&lib->job_lock);
    add_stats(&lib->scrape_stats, &d);
    if (!lib->scrape_pending) lib->scrape_after = after; /* a kick during the album restarts the pass */
    if (r > 0 || lib->scrape_pending) lib->job_scrape = 1;
}

void sel_library_scrape_kick(sel_library* lib) {
    if (lib == NULL || scrape_disabled(lib) || !sel_job_start(lib)) return;
    pthread_mutex_lock(&lib->job_lock);
    lib->scrape_pending = 1;
    lib->job_scrape = 1;
    sel_job_wake(lib);
    pthread_mutex_unlock(&lib->job_lock);
}

void sel_library_demozoo_kick(sel_library* lib, const char* ref) {
    char key[sizeof lib->dz_key];

    if (lib == NULL || ref == NULL || lib->dz_off || scrape_disabled(lib) || !sel_song_key_of(ref, key, sizeof key) || !sel_job_start(lib)) return;
    pthread_mutex_lock(&lib->job_lock);
    memcpy(lib->dz_key, key, sizeof key);
    lib->job_scrape = 1;
    sel_job_wake(lib);
    pthread_mutex_unlock(&lib->job_lock);
}

int sel_library_scrape_poll(sel_library* lib) {
    (void)lib;
    return 0;
}

void sel_library_scrape_wait(sel_library* lib) {
    sel_job_wait(lib);
}

#else

void sel_library_scrape_kick(sel_library* lib) {
    if (lib == NULL || scrape_disabled(lib)) return;
    lib->scrape_after = 0;
    lib->scrape_pending = 1;
}

void sel_library_demozoo_kick(sel_library* lib, const char* ref) {
    char key[sizeof lib->dz_key];

    if (lib != NULL && ref != NULL && !lib->dz_off && !scrape_disabled(lib) && sel_song_key_of(ref, key, sizeof key)) {
        memcpy(lib->dz_key, key, sizeof key);
    }
}

int sel_library_scrape_poll(sel_library* lib) {
    sel_scrape_stats d;
    int r;

    if (lib != NULL && lib->dz_key[0] != '\0') {
        char key[sizeof lib->dz_key];

        memcpy(key, lib->dz_key, sizeof key);
        lib->dz_key[0] = '\0';
        memset(&d, 0, sizeof d);
        dz_step(lib, key, &d);
        add_stats(&lib->scrape_stats, &d);
        return 1;
    }
    if (lib == NULL || !lib->scrape_pending) return 0;
    memset(&d, 0, sizeof d);
    r = scrape_step(lib, &lib->scrape_after, &d);
    add_stats(&lib->scrape_stats, &d);
    if (r <= 0) lib->scrape_pending = 0;
    return r > 0;
}

void sel_library_scrape_wait(sel_library* lib) {
    while (sel_library_scrape_poll(lib)) {
    }
}

#endif

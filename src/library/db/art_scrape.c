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
#if !defined(__amigaos__) && !defined(AMIGA)
#include <unistd.h>
#endif
#include "db_internal.h"

#include "../net/http.h"
#include "../net/net.h"
#include "../../core/pal.h"
#include "../dir.h"
#include "../song_key.h"
#include <selecta/uimodel/csdb.h>
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

/* I1459: 1 while a scrape GET is on the wire (from request start to the body in hand); the UI cover spinner reads it via sel_library_cover_fetching.
 * One job thread runs every scrape GET, so a plain store is enough. SELECTA_SCRAPE_HOLD_MS (test knob) keeps the flag up that long per GET. */
static int g_fetching;

int sel_library_cover_fetching(sel_library* lib) {
    return lib != NULL && __atomic_load_n(&g_fetching, __ATOMIC_RELAXED);
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
    __atomic_store_n(&g_fetching, 1, __ATOMIC_RELAXED);
#if !defined(__amigaos__) && !defined(AMIGA)
    if (getenv("SELECTA_SCRAPE_HOLD_MS") != NULL) usleep((useconds_t)atol(getenv("SELECTA_SCRAPE_HOLD_MS")) * 1000u);
#endif
    e = sel_http_request(&req, buf_sink, b, &res);
    __atomic_store_n(&g_fetching, 0, __ATOMIC_RELAXED);
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
#define DS_JSON_MAX 65536u /* a longer body is cut by buf_sink: the tags and ids come first */
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
    int n;

    if (strncmp(key, "modland:pub/modules/", 20) == 0) {
        if (!sel_url_encode(key, a, sizeof a)) return 0;
        return snprintf(out, cap, "key=%s", a) < (int)cap;
    }
    if (title == NULL || artist == NULL || title[0] == '\0' || artist[0] == '\0' || !sel_url_encode(title, a, sizeof a) || !sel_url_encode(artist, b, sizeof b)) return 0;
    n = snprintf(out, cap, "title=%s&author=%s", a, b);
    if (n >= (int)cap) return 0;
    /* I1401: link-first. The server matches the song's own Demozoo link (AMP id, HVSC path) before title + author; other sources have none. */
    if ((strncmp(key, "amp:", 4) == 0 || strncmp(key, "hvsc:", 5) == 0) && sel_url_encode(key, a, sizeof a)) {
        n += snprintf(out + n, cap - (size_t)n, "&link=%s", a);
        if (n >= (int)cap) return 0;
    }
    if (platform != NULL && platform[0] != '\0') n += snprintf(out + n, cap - (size_t)n, "&platform=%s", platform);
    return n < (int)cap;
}

/* I1401c: folder art for a local song. The first of cover / folder / front (.jpg .jpeg .png, any case) in the song's directory, else the
 * only image there. Free and local, so it goes before any request. shortcut: files over DZ_IMAGE_MAX are skipped; a song without album
 * gets no MusicBrainz step (the album pass already covers songs that have one). */
typedef struct folder_pick {
    char best[512];
    char only[512];
    int images;
    int named;
} folder_pick;

static int folder_each(void* ctx, const char* name, int is_dir) {
    folder_pick* f = ctx;
    size_t n = strlen(name), i, stem;
    const char* dot = strrchr(name, '.');
    char low[512];

    if (is_dir || dot == NULL || n >= sizeof low) return 1;
    for (i = 0; i < n; i++) low[i] = (char)((name[i] >= 'A' && name[i] <= 'Z') ? name[i] + 32 : name[i]);
    low[n] = '\0';
    stem = (size_t)(dot - name);
    if (strcmp(low + stem, ".jpg") != 0 && strcmp(low + stem, ".jpeg") != 0 && strcmp(low + stem, ".png") != 0) return 1;
    f->images++;
    snprintf(f->only, sizeof f->only, "%s", name);
    low[stem] = '\0';
    if (!f->named && (strcmp(low, "cover") == 0 || strcmp(low, "folder") == 0 || strcmp(low, "front") == 0)) {
        f->named = 1;
        snprintf(f->best, sizeof f->best, "%s", name);
    }
    return 1;
}

/* 1 = cover stored under the song's key, 0 = none found. */
static int folder_art(sel_library* lib, const char* key, int id) {
    char source[32], path[SCRAPE_URL_MAX], dir[SCRAPE_URL_MAX], full[SCRAPE_URL_MAX + 520];
    folder_pick f;
    FILE* fp;
    uint8_t* buf;
    size_t n, cut;
    scrape_buf b;

    if (!sel_song_key_split(key, source, sizeof source, path, sizeof path) || strcmp(source, "local") != 0) return 0;
    for (cut = strlen(path); cut > 0 && path[cut - 1] != '/' && path[cut - 1] != ':'; cut--) {
    }
    if (cut == 0) return 0;
    memset(&f, 0, sizeof f);
    memcpy(dir, path, cut);
    dir[cut] = '\0';
    if (cut > 1u && dir[cut - 1] == '/') dir[cut - 1] = '\0'; /* a trailing '/' ends the directory; an Amiga "Vol:" keeps its colon */
    if (!sel_dir_each(dir, folder_each, &f)) return 0;
    if (!f.named && f.images == 1) snprintf(f.best, sizeof f.best, "%s", f.only);
    if (f.best[0] == '\0') return 0;
    snprintf(full, sizeof full, "%.*s%s", (int)cut, path, f.best);
    fp = fopen(full, "rb");
    if (fp == NULL) return 0;
    buf = malloc(DZ_IMAGE_MAX);
    if (buf == NULL) {
        fclose(fp);
        return 0;
    }
    n = fread(buf, 1, DZ_IMAGE_MAX, fp);
    fclose(fp);
    b.data = buf;
    b.cap = DZ_IMAGE_MAX;
    b.len = n;
    if (n < DZ_IMAGE_MAX && is_image(&b)) {
        dz_put(lib, id, "cover", buf, n);
        dz_put(lib, id, "dz_key", key, strlen(key));
        n = 1;
    } else {
        n = 0;
    }
    free(buf);
    return (int)n;
}

/* I1401d: a source that names its cover (DLNA upnp:albumArtURI) has the URL stored next to the song's art under kind "cover_url" when the song is
 * catalogued; dz_step fetches it like any other image (2 MB cap). Only http(s) is kept. shortcut: written again on every walk, one small row
 * write per item; add a compare when a library is large enough to notice. */
void sel_library_cover_url_put(sel_library* lib, const char* source, const char* path, const char* url) {
    char key[1024];

    if (lib == NULL || url == NULL || (strncmp(url, "http://", 7) != 0 && strncmp(url, "https://", 8) != 0) || strlen(url) >= SCRAPE_URL_MAX) return;
    if (snprintf(key, sizeof key, "%s:%s", source, path) >= (int)sizeof key) return;
    dz_put(lib, sel_art_dz_id(key), "cover_url", url, strlen(url));
}

/* 1 = the stored cover URL answered with an image, now the song's cover. */
static int url_art(sel_library* lib, const char* key, int id, sel_scrape_stats* st) {
    char url[SCRAPE_URL_MAX];
    uint8_t* buf;
    scrape_buf b;
    int ok = 0;
    int n = dz_has(lib, id, "cover_url", (uint8_t*)url, sizeof url - 1u);

    if (n <= 0) return 0;
    url[n] = '\0';
    buf = malloc(DZ_IMAGE_MAX);
    if (buf == NULL) return 0;
    b.data = buf;
    b.cap = DZ_IMAGE_MAX;
    b.len = 0;
    if (scrape_get(url, &b, st) == 1 && b.len < DZ_IMAGE_MAX && is_image(&b)) {
        dz_put(lib, id, "cover", buf, b.len);
        dz_put(lib, id, "dz_key", key, strlen(key));
        ok = 1;
    }
    free(buf);
    return ok;
}

/* I1454: a song keeps every still it has: the first is its 'cover', the next ones 'dz1'..'dz7' (the slide show of I1384b cycles them). An image the
 * song already holds (same bytes) is not added twice. 1 = stored, 0 = duplicate or no free slot. */
static int still_add(sel_library* lib, int id, const char* key, const void* data, size_t len) {
    uint8_t* tmp = (uint8_t*)malloc(DZ_IMAGE_MAX);
    char kind[8];
    int k, ok = 0;

    if (tmp == NULL) return 0;
    for (k = 0; k < 8; k++) {
        int n;

        if (k == 0) strcpy(kind, "cover");
        else snprintf(kind, sizeof kind, "dz%d", k);
        n = dz_has(lib, id, kind, tmp, DZ_IMAGE_MAX);
        if (n <= 0) {
            dz_put(lib, id, kind, data, len);
            dz_put(lib, id, "dz_key", key, strlen(key));
            ok = 1;
            break;
        }
        if ((size_t)n == len && memcmp(tmp, data, len) == 0) break;
    }
    free(tmp);
    return ok;
}

static int ds_step(sel_library* lib, const char* ref, sel_scrape_stats* st);
static int csdb_cover(sel_library* lib, const char* key, int id, sel_scrape_stats* st);

/* I1440i (design 2.3, D5): a SID song with no folder art or cover_url takes the DeepSID composer photo as its cover. The ds_composer row is the
 * cache of I1440h's fetcher (ds_step runs when the row is missing); the photo is the server's resized JPEG, never the original. The placeholder
 * composer.png is never a cover. shortcut: one fixed width (320, the largest cover thumbnail); the Amiga resize shrinks it further. 1 = cover stored. */
static int composer_art(sel_library* lib, const char* key, int id, sel_scrape_stats* st) {
    char dk[1100], fkey[1100], photo[256], enc[600], base[256], url[SCRAPE_URL_MAX];
    sel_art_op op;
    scrape_buf js, img;
    const char* slash;
    int ok = 0;

    memset(&op, 0, sizeof op);
    op.op = SEL_ART_DS_KEY;
    op.ref = key;
    op.data = (uint8_t*)dk;
    op.cap = sizeof dk;
    if (art_request(lib, &op) <= 0 || (slash = strrchr(dk, '/')) == NULL || (size_t)(slash - dk) >= sizeof fkey) return 0;
    snprintf(fkey, sizeof fkey, "%.*s", (int)(slash - dk), dk);
    if (dz_has(lib, sel_art_dz_id(fkey), "ds_composer", NULL, 0) <= 0) ds_step(lib, key, st);
    js.data = malloc(DS_JSON_MAX);
    img.data = malloc(DZ_IMAGE_MAX);
    if (js.data != NULL && img.data != NULL) {
        int n = dz_has(lib, sel_art_dz_id(fkey), "ds_composer", js.data, DS_JSON_MAX - 1u);

        if (n > 0) {
            js.data[n] = '\0';
            if (json_str((const char*)js.data, "photo", photo, sizeof photo) != NULL && photo[0] != '\0' && strcmp(photo, "composer.png") != 0 &&
                sel_url_encode(photo, enc, sizeof enc)) {
                base_url("SELECTA_DEEPSID_URL", "https://hippoplayer.se", base, sizeof base);
                snprintf(url, sizeof url, "%s/api/deepsid/image/composers/%s?w=320", base, enc);
                img.cap = DZ_IMAGE_MAX;
                img.len = 0;
                if (scrape_get(url, &img, st) == 1 && img.len < DZ_IMAGE_MAX && is_image(&img)) {
                    ok = still_add(lib, id, key, img.data, img.len);
                }
            }
        }
    }
    free(js.data);
    free(img.data);
    return ok;
}

static int dz_step(sel_library* lib, const char* key, sel_scrape_stats* st) {
    char base[256], url[SCRAPE_URL_MAX], ke[1500], meta[600], shot[SCRAPE_URL_MAX], stamp[24];
    uint8_t seen[24];
    scrape_buf js, img;
    sel_art_op op;
    const char *p, *end = NULL, *fam = NULL;
    int id, alb, n = 0, r, sid, shots_max = DZ_SHOTS_MAX;
    const char* cap_env = getenv("SELECTA_ART_DZ_SHOTS"); /* test knob: a live check fetches one image */

    if (cap_env != NULL && atoi(cap_env) > 0 && atoi(cap_env) < shots_max) shots_max = atoi(cap_env);
    id = sel_art_dz_id(key);
    memset(&op, 0, sizeof op);
    op.op = SEL_ART_FIND;
    op.ref = key;
    alb = art_request(lib, &op);
    if (alb != 0 && dz_has(lib, alb, "cover", NULL, 0) > 0) { /* real art, or screenshots already cached */
        if (alb == id && dz_has(lib, id, "dz_more", NULL, 0) <= 0) { /* I1454: a SID cover cached before the rotation gets the CSDb shots once */
            csdb_cover(lib, key, id, st);
            dz_put(lib, id, "dz_more", "1", 1);
        }
        return 1;
    }
    if (folder_art(lib, key, id) || url_art(lib, key, id, st)) {
        st->scraped++;
        return 1;
    }
    sid = composer_art(lib, key, id, st); /* I1454: the composer photo, then the CSDb shots, then Demozoo: all of them rotate, not the first only */
    sid |= csdb_cover(lib, key, id, st);
    if (sid) {
        st->scraped++;
        dz_put(lib, id, "dz_more", "1", 1);
    }
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
                still_add(lib, id, key, img.data, img.len);
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

/* I1440h: DeepSID. One GET into `js` (NUL-terminated), the artwork row `kind` of `key` filled with the body. 1 = stored, 0 = definite no, -1 = try later. */

static int ds_fetch(sel_library* lib, const char* key, const char* kind, const char* url, scrape_buf* js, sel_scrape_stats* st) {
    int r = scrape_get(url, js, st);

    if (r != 1) return r;
    js->data[js->len] = '\0';
    dz_put(lib, sel_art_dz_id(key), kind, js->data, js->len);
    return 1;
}

/* I1440l2 (design 3.2): the screenshot of each CSDb release of a ds_csdb answer, as the 160 px JPEG of the server, stored as the cover of the artwork key
 * sel_deepsid_csdb_key(id) so the Song Information thumbnails read it like any cover. A miss or failure is not retried (the file row is the cache gate). */
static void csdb_thumbs(sel_library* lib, const char* base, const char* json, sel_scrape_stats* st) {
    int ids[6], i;
    uint32_t n = sel_deepsid_csdb_ids(json, ids, 6u);
    scrape_buf img;

    img.data = n > 0 ? malloc(DZ_IMAGE_MAX) : NULL;
    for (i = 0; img.data != NULL && i < (int)n; i++) {
        char raw[40], key[64], url[SCRAPE_URL_MAX];

        sel_csdb_art_key(ids[i], raw, sizeof raw);
        if (!sel_song_key_of(raw, key, sizeof key) || dz_has(lib, sel_art_dz_id(key), "cover", NULL, 0) > 0) continue;
        snprintf(url, sizeof url, "%s/api/deepsid/image/csdb/%d.jpg?w=160", base, ids[i]);
        img.cap = DZ_IMAGE_MAX;
        if (scrape_get(url, &img, st) == 1 && img.len < DZ_IMAGE_MAX && is_image(&img)) {
            dz_put(lib, sel_art_dz_id(key), "cover", img.data, img.len);
            dz_put(lib, sel_art_dz_id(key), "dz_key", key, strlen(key));
        }
    }
    free(img.data);
}

/* I1450 (design D5): after the composer photo, the first CSDb release screenshot of the song is its cover, before the Demozoo fallback. The ids come from
 * the ds_csdb row of I1440h (ds_step fetches it when missing); the image is the server's resized JPEG, as in csdb_thumbs. 1 = cover stored. */
static int csdb_cover(sel_library* lib, const char* key, int id, sel_scrape_stats* st) {
    char dk[1100], base[256], url[SCRAPE_URL_MAX];
    sel_art_op op;
    scrape_buf js, img;
    int ids[3], ok = 0, n, i, ni;

    memset(&op, 0, sizeof op);
    op.op = SEL_ART_DS_KEY;
    op.ref = key;
    op.data = (uint8_t*)dk;
    op.cap = sizeof dk;
    if (art_request(lib, &op) <= 0) return 0;
    ds_step(lib, key, st);
    js.data = malloc(DS_JSON_MAX);
    img.data = malloc(DZ_IMAGE_MAX);
    if (js.data != NULL && img.data != NULL && (n = dz_has(lib, sel_art_dz_id(dk), "ds_csdb", js.data, DS_JSON_MAX - 1u)) > 0) {
        js.data[n] = '\0';
        ni = (int)sel_deepsid_csdb_ids((const char*)js.data, ids, 3u); /* I1454: up to 3 release screenshots, all join the rotation */
        base_url("SELECTA_DEEPSID_URL", "https://hippoplayer.se", base, sizeof base);
        for (i = 0; i < ni; i++) {
            snprintf(url, sizeof url, "%s/api/deepsid/image/csdb/%d.jpg?w=320", base, ids[i]);
            img.cap = DZ_IMAGE_MAX;
            img.len = 0;
            if (scrape_get(url, &img, st) == 1 && img.len < DZ_IMAGE_MAX && is_image(&img) && still_add(lib, id, key, img.data, img.len)) ok = 1;
        }
    }
    free(js.data);
    free(img.data);
    return ok;
}

/* The names of the "tags" array of a file answer, joined by ", ", into the sticker "deepsid.tags" of the key (an emptied list clears it). */
static void ds_tags(sel_library* lib, const char* key, const char* json) {
    char all[512], name[96];
    const char *p = strstr(json, "\"tags\""), *end;
    size_t n = 0;

    all[0] = '\0';
    p = p != NULL ? json_value(p + 6) : NULL;
    end = p != NULL && *p == '[' ? strchr(p, ']') : NULL;
    while (end != NULL && (p = json_str(p, "name", name, sizeof name)) != NULL && p < end) {
        n += (size_t)snprintf(all + n, sizeof all - n, "%s%s", n > 0 ? ", " : "", name);
        if (n >= sizeof all) break; /* shortcut: tags past 511 bytes are dropped, the list is a few names */
    }
    sel_library_sticker_set(lib, key, "deepsid.tags", all);
}


/* I1440m: one DeepSID list (spec = what US arg) into its 'ds_list' row. Returns 1 done, -1 network trouble (nothing recorded). */
static int ds_list_step(sel_library* lib, const char* spec, sel_scrape_stats* st) {
    char base[256], what[24], enc[1500], url[SCRAPE_URL_MAX];
    const char* arg = strchr(spec, 0x1f);
    scrape_buf js;
    int r;

    if (arg == NULL || (size_t)(arg - spec) >= sizeof what) return 1;
    snprintf(what, sizeof what, "%.*s", (int)(arg - spec), spec);
    arg++;
    if (dz_has(lib, sel_art_dz_id(spec), "ds_list", NULL, 0) > 0) return 1;
    base_url("SELECTA_DEEPSID_URL", "https://hippoplayer.se", base, sizeof base);
    sel_url_encode(arg, enc, sizeof enc);
    if (strcmp(what, "tags") == 0) snprintf(url, sizeof url, "%s/api/deepsid/tags", base);
    else if (strcmp(what, "tag") == 0 && atoi(arg) > 0) snprintf(url, sizeof url, "%s/api/deepsid/tags/%d/files?limit=200", base, atoi(arg));
    else if (strcmp(what, "groups") == 0) snprintf(url, sizeof url, "%s/api/deepsid/groups?limit=200", base);
    else if (strcmp(what, "group") == 0) snprintf(url, sizeof url, "%s/api/deepsid/groups/%s/files?limit=200", base, enc);
    else if (strcmp(what, "players") == 0) snprintf(url, sizeof url, "%s/api/deepsid/players?limit=200", base);
    else if (strcmp(what, "player") == 0) snprintf(url, sizeof url, "%s/api/deepsid/search?q=%s&category=player&limit=200", base, enc); /* shortcut: no player-songs route, the search by player; add one when the server gets it */
    else if (strcmp(what, "charts") == 0 && strncmp(arg, "new", 3) == 0) snprintf(url, sizeof url, "%s/api/deepsid/charts?kind=new&limit=200", base); /* top and played: the server answers 501 */
    else if (strcmp(what, "comps") == 0) snprintf(url, sizeof url, "%s/api/deepsid/competitions?limit=200", base);
    else if (strcmp(what, "placings") == 0 && atoi(arg) > 0) snprintf(url, sizeof url, "%s/api/deepsid/competitions/%d?limit=200", base, atoi(arg));
    else if (strcmp(what, "folders") == 0) snprintf(url, sizeof url, "%s/api/deepsid/folders?path=%s&limit=200", base, enc);
    else if (strcmp(what, "tunes") == 0) snprintf(url, sizeof url, "%s/api/deepsid/tunes?path=%s&limit=200", base, enc);
    else return 1;
    js.data = malloc(DS_JSON_MAX);
    js.cap = DS_JSON_MAX - 1u;
    if (js.data == NULL) return -1;
    js.len = 0;
    st->albums++;
    r = ds_fetch(lib, spec, "ds_list", url, &js, st);
    if (r == 0) dz_put(lib, sel_art_dz_id(spec), "ds_list", "[]", 2); /* a definite no: an empty list, not asked again */
    else if (r == 1) st->scraped++;
    free(js.data);
    return r < 0 ? -1 : 1;
}

/* One song. Returns 1 done (stored, cached or nothing to ask), -1 network trouble (nothing recorded). */
static int ds_step(sel_library* lib, const char* ref, sel_scrape_stats* st) {
    char base[256], key[1100], fkey[1100], enc[1100], url[SCRAPE_URL_MAX], stamp[24];
    uint8_t seen[24];
    scrape_buf js;
    sel_art_op op;
    const char *p, *slash;
    int id, r;

    memset(&op, 0, sizeof op);
    op.op = SEL_ART_DS_KEY;
    op.ref = ref;
    op.data = (uint8_t*)key;
    op.cap = sizeof key;
    if (art_request(lib, &op) <= 0) return 1; /* not a SID of HVSC/CGSC: no DeepSID data */
    id = sel_art_dz_id(key);
    if (dz_has(lib, id, "ds_file", NULL, 0) > 0) return 1;
    memset(seen, 0, sizeof seen);
    if (dz_has(lib, id, "ds_miss2", seen, sizeof seen - 1u) > 0 && (time_t)atol((const char*)seen) > time(NULL) - (time_t)SEL_ART_SCRAPE_RETRY_DAYS * 86400) return 1;
    js.data = malloc(DS_JSON_MAX);
    js.cap = DS_JSON_MAX - 1u;
    if (js.data == NULL) return -1;
    base_url("SELECTA_DEEPSID_URL", "https://hippoplayer.se", base, sizeof base);
    sel_url_encode(strchr(key, ':') + 1, enc, sizeof enc);
    snprintf(url, sizeof url, "%s/api/deepsid/file-by-path?path=%s&full=1", base, enc);
    st->albums++;
    r = ds_fetch(lib, key, "ds_file", url, &js, st);
    if (r == 1) {
        ds_tags(lib, key, (const char*)js.data);
        p = strstr((const char*)js.data, "\"csdbId\"");
        p = p != NULL ? json_value(p + 8) : NULL;
        if (p != NULL && *p >= '1' && *p <= '9') { /* null or 0: the file has no CSDb release */
            p = strstr((const char*)js.data, "\"id\"");
            p = p != NULL ? json_value(p + 4) : NULL;
            if (p != NULL && atoi(p) > 0) {
                snprintf(url, sizeof url, "%s/api/deepsid/csdb/%d", base, atoi(p));
                if (ds_fetch(lib, key, "ds_csdb", url, &js, st) == 1) csdb_thumbs(lib, base, (const char*)js.data, st); /* a failure here is retried never: the file row is the cache gate; shortcut, upgrade when CSDb flaps */
            }
        }
        slash = strrchr(key, '/');
        if (slash != NULL && (size_t)(slash - key) < sizeof fkey) {
            snprintf(fkey, sizeof fkey, "%.*s", (int)(slash - key), key);
            if (dz_has(lib, sel_art_dz_id(fkey), "ds_composer", NULL, 0) <= 0 && sel_url_encode(strchr(fkey, ':') + 1, enc, sizeof enc)) {
                snprintf(url, sizeof url, "%s/api/deepsid/composer?path=%s", base, enc);
                ds_fetch(lib, fkey, "ds_composer", url, &js, st);
            }
        }
        st->scraped++;
    } else if (r == 0 && js.len > 0 && js.data[0] == '{') {
        /* I1450: only a JSON answer is the API saying "not found". An HTML 404 (route missing on the site) records nothing, so the next scrape retries.
         * The kind is ds_miss2: the old ds_miss rows were written while the route was missing and are ignored once. */
        snprintf(stamp, sizeof stamp, "%lu", (unsigned long)time(NULL));
        dz_put(lib, id, "ds_miss2", stamp, strlen(stamp));
        st->misses++;
    } else {
        st->errors++;
    }
    free(js.data);
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
    if (lib->ds_key[0] != '\0') { /* I1440h: same turn rules as the Demozoo song above */
        char ref[sizeof lib->ds_key];

        memcpy(ref, lib->ds_key, sizeof ref);
        lib->ds_key[0] = '\0';
        pthread_mutex_unlock(&lib->job_lock);
        ds_step(lib, ref, &d);
        pthread_mutex_lock(&lib->job_lock);
        add_stats(&lib->scrape_stats, &d);
        lib->job_scrape = 1;
        return;
    }
    if (lib->ds_list[0] != '\0') { /* I1440m: a DeepSID list, same turn rules as the song above */
        char spec[sizeof lib->ds_list];

        memcpy(spec, lib->ds_list, sizeof spec);
        lib->ds_list[0] = '\0';
        pthread_mutex_unlock(&lib->job_lock);
        ds_list_step(lib, spec, &d);
        pthread_mutex_lock(&lib->job_lock);
        add_stats(&lib->scrape_stats, &d);
        lib->job_scrape = 1;
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

void sel_library_deepsid_kick(sel_library* lib, const char* ref) {
    char key[sizeof lib->ds_key];

    if (lib == NULL || ref == NULL || scrape_disabled(lib) || !sel_song_key_of(ref, key, sizeof key) || !sel_job_start(lib)) return;
    pthread_mutex_lock(&lib->job_lock);
    memcpy(lib->ds_key, key, sizeof key);
    lib->job_scrape = 1;
    sel_job_wake(lib);
    pthread_mutex_unlock(&lib->job_lock);
}

void sel_library_deepsid_list_kick(sel_library* lib, const char* spec) {
    if (lib == NULL || spec == NULL || strlen(spec) >= sizeof lib->ds_list || scrape_disabled(lib) || !sel_job_start(lib)) return;
    pthread_mutex_lock(&lib->job_lock);
    memcpy(lib->ds_list, spec, strlen(spec) + 1u);
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

void sel_library_deepsid_kick(sel_library* lib, const char* ref) {
    char key[sizeof lib->ds_key];

    if (lib != NULL && ref != NULL && !scrape_disabled(lib) && sel_song_key_of(ref, key, sizeof key)) memcpy(lib->ds_key, key, sizeof key);
}

void sel_library_deepsid_list_kick(sel_library* lib, const char* spec) {
    if (lib != NULL && spec != NULL && strlen(spec) < sizeof lib->ds_list && !scrape_disabled(lib)) memcpy(lib->ds_list, spec, strlen(spec) + 1u);
}

int sel_library_scrape_poll(sel_library* lib) {
    sel_scrape_stats d;
    int r;

    if (lib != NULL && lib->ds_list[0] != '\0') {
        char spec[sizeof lib->ds_list];

        memcpy(spec, lib->ds_list, sizeof spec);
        lib->ds_list[0] = '\0';
        memset(&d, 0, sizeof d);
        ds_list_step(lib, spec, &d);
        add_stats(&lib->scrape_stats, &d);
        return 1;
    }

    if (lib != NULL && lib->ds_key[0] != '\0') {
        char ref[sizeof lib->ds_key];

        memcpy(ref, lib->ds_key, sizeof ref);
        lib->ds_key[0] = '\0';
        memset(&d, 0, sizeof d);
        ds_step(lib, ref, &d);
        add_stats(&lib->scrape_stats, &d);
        return 1;
    }

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

/* I1440k: the two JSON scanners above for deepsid_info.c (same flat-key scanning, no second parser). */
const char* sel_json_value(const char* p) { return json_value(p); }
const char* sel_json_str(const char* from, const char* key, char* out, size_t cap) { return json_str(from, key, out, cap); }

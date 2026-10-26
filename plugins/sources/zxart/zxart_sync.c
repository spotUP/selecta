/* See zxart_sync.h. */
#include "zxart_sync.h"

#include "source/json_scan.h"
#include "source/sync_text.h"
#include "source/url_path.h"

#include <selecta/catalogue.h>

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#define SYNC_BATCH 32u
#define UNKNOWN_ARTIST "- unknown"
#define PEOPLE_MAX 65536u
#define POOL_BYTES (2ul << 20)

/* ---- the answer: a streaming read of one page ---- */

/* Reads the digits of a number whose first byte `c` is read. Returns the byte after it; a fraction or exponent is
 * dropped (`*v` keeps the integer part). A negative number gives -1. */
static int read_number(FILE* f, int c, int64_t* v) {
    int neg = 0;

    *v = 0;
    if (c == '-') {
        neg = 1;
        c = getc(f);
    }
    for (; c >= '0' && c <= '9'; c = getc(f)) {
        if (*v < ((int64_t)1 << 56)) {
            *v = *v * 10 + (c - '0');
        }
    }
    while (c == '.' || c == 'e' || c == 'E' || c == '+' || c == '-' || (c >= '0' && c <= '9')) {
        c = getc(f);
    }
    if (neg) {
        *v = -1;
    }
    return c;
}

/* After a value: `c` is the byte read past it (or white space). Returns ',' or '}' / ']' (the close), 0 on EOF. */
static int after_value(FILE* f, int c) {
    while (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
        c = getc(f);
    }
    return c == EOF ? 0 : c;
}

/* The array authorIds, its '[' consumed. Whole numbers only (bbmp: ints). */
static int read_ids(FILE* f, sel_zxart_item* it) {
    for (;;) {
        int c = sel_json_skip_ws(f);

        if (c == ']') {
            return 1;
        }
        if (c == ',') {
            continue;
        }
        if (c >= '0' && c <= '9') {
            int64_t v;

            c = read_number(f, c, &v);
            if (it->author_n < SEL_ZXART_AUTHORS_MAX) {
                it->authors[it->author_n++] = v;
            }
            if (c == ']') {
                return 1;
            }
            continue;
        }
        {
            int end = 0;

            if (c == EOF || !sel_json_skip_value(f, c, &end)) {
                return 0;
            }
            if (end == ']') {
                return 1;
            }
        }
    }
}

/* One object, its '{' consumed. Fills the wanted keys. Returns 0 on malformed input. */
static int read_item(FILE* f, sel_zxart_item* it) {
    char key[24];
    int c;

    memset(it, 0, sizeof *it);
    it->id = -1;
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
            char* dst = NULL;
            size_t n = 0;

            if (strcmp(key, "title") == 0) {
                dst = it->title;
                n = sizeof it->title;
            } else if (strcmp(key, "type") == 0) {
                dst = it->type;
                n = sizeof it->type;
            } else if (strcmp(key, "time") == 0) {
                dst = it->time;
                n = sizeof it->time;
            } else if (strcmp(key, "originalFileName") == 0) {
                dst = it->file;
                n = sizeof it->file;
            } else if (strcmp(key, "originalUrl") == 0) {
                char one[2];

                if (!sel_json_read_string(f, one, sizeof one)) {
                    return 0;
                }
                it->has_url = one[0] != '\0';
                continue;
            }
            if (dst != NULL) {
                if (!sel_json_read_string(f, dst, n)) {
                    return 0;
                }
                continue;
            }
        } else if ((c >= '0' && c <= '9') && (strcmp(key, "id") == 0 || strcmp(key, "year") == 0)) {
            int64_t v;

            c = after_value(f, read_number(f, c, &v));
            if (key[0] == 'i') {
                it->id = v;
            } else {
                it->year = v;
            }
            if (c == '}' || c == 0) {
                return c == '}';
            }
            continue;
        } else if (c == '[' && strcmp(key, "authorIds") == 0) {
            if (!read_ids(f, it)) {
                return 0;
            }
            continue;
        }
        {
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

/* The list `what`, its '[' consumed. Returns the objects read, -1 when malformed. */
static int32_t read_list(FILE* f, int (*fn)(void* ctx, const sel_zxart_item* it), void* ctx, int* stopped) {
    static sel_zxart_item it; /* 1.4 KB: static, not stack (one reader at a time) */
    int32_t n = 0;
    int c;

    while ((c = sel_json_skip_ws(f)) != EOF && c != ']') {
        if (c == ',') {
            continue;
        }
        if (c != '{' || !read_item(f, &it)) {
            return -1;
        }
        n++;
        if (fn != NULL && !fn(ctx, &it)) {
            *stopped = 1;
            return n;
        }
    }
    return c == ']' ? n : -1;
}

int32_t sel_zxart_read_page(FILE* f, const char* what, int (*fn)(void* ctx, const sel_zxart_item* it), void* ctx,
                            uint32_t* total) {
    char key[24];
    char text[16];
    int32_t n = 0;
    int success = 0, stopped = 0, c;

    *total = 0;
    if (f == NULL || sel_json_skip_ws(f) != '{') {
        return -1;
    }
    for (;;) {
        c = sel_json_skip_ws(f);
        if (c == '}') {
            break;
        }
        if (c == ',') {
            continue;
        }
        if (c != '"' || !sel_json_read_string(f, key, sizeof key) || sel_json_skip_ws(f) != ':') {
            return -1;
        }
        c = sel_json_skip_ws(f);
        if (strcmp(key, "totalAmount") == 0 && c >= '0' && c <= '9') {
            int64_t v;

            c = after_value(f, read_number(f, c, &v));
            *total = v > 0xFFFFFFF ? 0xFFFFFFFu : (uint32_t)v;
            if (c == '}') {
                break;
            }
            continue;
        }
        if (strcmp(key, "responseStatus") == 0 && c == '"') {
            if (!sel_json_read_string(f, text, sizeof text)) {
                return -1;
            }
            success = strcmp(text, "success") == 0;
            continue;
        }
        if (strcmp(key, "responseData") == 0 && c == '{') {
            for (;;) {
                c = sel_json_skip_ws(f);
                if (c == '}') {
                    break;
                }
                if (c == ',') {
                    continue;
                }
                if (c != '"' || !sel_json_read_string(f, key, sizeof key) || sel_json_skip_ws(f) != ':') {
                    return -1;
                }
                c = sel_json_skip_ws(f);
                if (strcmp(key, what) == 0 && c == '[') {
                    n = read_list(f, fn, ctx, &stopped);
                    if (n < 0) {
                        return -1;
                    }
                    if (stopped) {
                        return n; /* the caller does not want the rest */
                    }
                } else {
                    int end = 0;

                    if (c == EOF || !sel_json_skip_value(f, c, &end)) {
                        return -1;
                    }
                    if (end == '}') {
                        break;
                    }
                }
            }
            continue;
        }
        {
            int end = 0;

            if (c == EOF || !sel_json_skip_value(f, c, &end)) {
                return -1;
            }
            if (end == '}') {
                break;
            }
        }
    }
    return success ? n : -1;
}

/* ---- the row: format, length, artist ---- */

typedef struct kind_format {
    const char* kind;
    const char* format;
} kind_format;

/* ZX-Art's kind of tune -> the format Modland files it under (its replayer, its platform); kinds Modland has no
 * folder for get a "Spectrum - " name of their own (bbmp zxart.py TYPES). */
static const kind_format TYPES[] = {
    {"PT3", "Spectrum - Pro Tracker 3"}, {"TS", "Spectrum - Pro Tracker 3"}, {"PT2", "Spectrum - Pro Tracker 2"},
    {"PT1", "Spectrum - Pro Tracker 1"}, {"STC", "Spectrum - ST Song Compiler"}, {"ST1", "Spectrum - Sound Tracker 1.1"},
    {"ST3", "Spectrum - Sound Tracker 1.3"}, {"STP", "Spectrum - Sound Tracker Pro"}, {"SQT", "Spectrum - SQ Tracker"},
    {"ASC", "Spectrum - ASC Sound Master"}, {"PSC", "Spectrum - Pro Sound Creator"}, {"PSM", "Spectrum - Pro Sound Maker"},
    {"GTR", "Spectrum - Global Tracker"}, {"FTC", "Spectrum - Fast Tracker"}, {"FLS", "Spectrum - Flash Tracker"},
    {"CHI", "Spectrum - Chip Tracker"}, {"VTX", "Spectrum - Vortex"}, {"ZXS", "Spectrum - ZXS"},
    {"FXM", "Spectrum - Fuxoft AY Language"}, {"AY", "AY Emul"}, {"YM", "YM"}, {"TFD", "TFM Music Maker"},
    {"TFC", "TFM Music Maker"}, {"TFE", "TFM Music Maker"}, {"TF0", "TFM Music Maker"}, {"AKS", "ArkosTracker"},
    {"COP", "Sam Coupe COP"}, {"DST", "Spectrum - Digital Studio"}, {"DMM", "Spectrum - Digital Music Maker"},
    {"ET1", "Spectrum - Extreme Tracker"}, {"SQD", "Spectrum - SQ Digital Tracker"}, {"STR", "Spectrum - Sample Tracker"},
    {"MTC", "Spectrum - Multitrack"}, {"PDT", "Spectrum - ProDigiTracker"}, {"PSG", "Spectrum - PSG"},
    {"AS0", "Spectrum - ASC Sound Master 0"}, {"FUR", "Spectrum - Furnace"}, {"VGM", "Spectrum - VGM"},
    {"MOD", "Spectrum - General Sound"}, {"CHP", "Spectrum - CHIPNSFX"},
};

/* The whole computer's memory, or its tape or disk */
static const char* const SNAPSHOTS[] = {"sna", "z80", "tap", "tzx", "scl", "trd"};
/* Recordings, not chip music */
static const char* const SOUND[] = {"MP3", "OGG", "WAV", "FLAC"};

static int in_list(const char* const* list, size_t n, const char* s) {
    size_t i;

    for (i = 0; i < n; i++) {
        if (strcmp(list[i], s) == 0) {
            return 1;
        }
    }
    return 0;
}

const char* sel_zxart_format(const char* kind, const char* ext, char* scratch, size_t n) {
    size_t i;

    if (in_list(SNAPSHOTS, sizeof SNAPSHOTS / sizeof SNAPSHOTS[0], ext)) {
        return "Spectrum - Snapshot";
    }
    for (i = 0; i < sizeof TYPES / sizeof TYPES[0]; i++) {
        if (strcmp(TYPES[i].kind, kind) == 0) {
            return TYPES[i].format;
        }
    }
    if (kind[0] == '\0') {
        return "Spectrum";
    }
    snprintf(scratch, n, "Spectrum - %s", kind);
    return scratch;
}

int32_t sel_zxart_millis(const char* time) {
    const char* colon = strrchr(time, ':');
    char* end;
    double seconds;
    long minutes = 0;

    if (colon != NULL) {
        minutes = strtol(time, &end, 10);
        if (end != colon || minutes < 0) {
            return 0;
        }
        colon++;
    } else {
        colon = time;
    }
    seconds = strtod(colon, &end);
    if (end == colon || *end != '\0' || seconds <= 0.0) {
        return 0;
    }
    return (int32_t)((double)minutes * 60000.0 + seconds * 1000.0 + 0.5);
}

/* ---- the people ---- */

typedef struct person {
    int64_t id;
    uint32_t off; /* into the pool */
} person;

typedef struct people {
    person* list;
    uint32_t n;
    char* pool;
    unsigned long used;
} people;

static int add_person(void* ctx, const sel_zxart_item* it) {
    people* p = ctx;
    const char* t = it->title;
    size_t len;

    while (*t == ' ') {
        t++;
    }
    len = strlen(t);
    while (len > 0 && t[len - 1u] == ' ') {
        len--;
    }
    if (it->id < 0 || len == 0 || p->n >= PEOPLE_MAX || p->used + len + 1u > POOL_BYTES) {
        return 1; /* a person left out is an unknown artist, not a failed sync */
    }
    p->list[p->n].id = it->id;
    p->list[p->n].off = (uint32_t)p->used;
    memcpy(p->pool + p->used, t, len);
    p->pool[p->used + len] = '\0';
    p->used += len + 1u;
    p->n++;
    return 1;
}

static int by_id(const void* a, const void* b) {
    int64_t x = ((const person*)a)->id, y = ((const person*)b)->id;

    return x < y ? -1 : x > y;
}

static const char* person_name(const people* p, int64_t id) {
    uint32_t lo = 0, hi = p->n;

    while (lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2u;

        if (p->list[mid].id == id) {
            return p->pool + p->list[mid].off;
        }
        if (p->list[mid].id < id) {
            lo = mid + 1u;
        } else {
            hi = mid;
        }
    }
    return NULL;
}

/* ---- the catalogue ---- */

typedef struct row_text {
    char path[1048];
    char format[64];
    char artist[256];
    char name[300];
} row_text;

typedef struct sync_state {
    const sel_host* host;
    const sel_catalogue* cat;
    sel_zxart_sync_stats* st;
    const people* ppl;
    row_text* text;
    int failed;
} sync_state;

static void upper(char* s) {
    for (; *s != '\0'; s++) {
        *s = (char)toupper((unsigned char)*s);
    }
}

static int on_tune(void* ctx, const sel_zxart_item* it) {
    sync_state* s = ctx;
    row_text* t = s->text;
    sel_catalogue_song song;
    char file[1024], kind[16], ext[16], fmt_scratch[40];
    const char* dot;
    const char* title = it->title;
    size_t used = 0;
    uint32_t i;

    s->st->tunes++;
    if (it->id < 0) {
        return 1;
    }
    memcpy(kind, it->type, sizeof kind);
    upper(kind);
    if (in_list(SOUND, sizeof SOUND / sizeof SOUND[0], kind) || !it->has_url || it->file[0] == '\0' ||
        !sel_url_unquote(it->file, file, sizeof file) || file[0] == '\0') {
        s->st->skipped_rows++;
        return 1;
    }
    if (snprintf(t->path, sizeof t->path, "%lld/%s", (long long)it->id, file) >= (int)sizeof t->path ||
        !sel_sync_safe_path(file) || strchr(file, '/') != NULL) {
        s->st->skipped_rows++;
        return 1;
    }
    dot = strrchr(file, '.');
    snprintf(ext, sizeof ext, "%s", dot != NULL ? dot + 1 : kind);
    for (i = 0; ext[i] != '\0'; i++) {
        ext[i] = (char)tolower((unsigned char)ext[i]);
    }
    if (kind[0] == '\0') { /* ZX-Art names no kind for some (113 in 2026): .fxm, .fls, .vtx */
        char up[16];

        memcpy(up, ext, sizeof up);
        upper(up);
        if (strcmp(sel_zxart_format(up, "", fmt_scratch, sizeof fmt_scratch), "Spectrum") != 0 &&
            strncmp(sel_zxart_format(up, "", fmt_scratch, sizeof fmt_scratch), "Spectrum - ", 11) != 0) {
            memcpy(kind, up, sizeof kind);
        } else {
            for (i = 0; i < sizeof TYPES / sizeof TYPES[0]; i++) {
                if (strcmp(TYPES[i].kind, up) == 0) {
                    memcpy(kind, up, sizeof kind);
                }
            }
        }
    }
    snprintf(t->format, sizeof t->format, "%s", sel_zxart_format(kind, ext, fmt_scratch, sizeof fmt_scratch));
    t->artist[0] = '\0';
    for (i = 0; i < it->author_n; i++) {
        const char* who = person_name(s->ppl, it->authors[i]);
        size_t len;

        if (who == NULL) {
            continue;
        }
        len = strlen(who);
        if (used + (used > 0 ? 3u : 0u) + len + 1u > sizeof t->artist) {
            break;
        }
        if (used > 0) {
            memcpy(t->artist + used, " & ", 3);
            used += 3u;
        }
        memcpy(t->artist + used, who, len + 1u);
        used += len;
    }
    if (used == 0) {
        memcpy(t->artist, UNKNOWN_ARTIST, sizeof UNKNOWN_ARTIST);
    }
    if (title[0] == '\0') { /* the file name without its extension */
        size_t stem = dot != NULL ? (size_t)(dot - file) : strlen(file);

        snprintf(t->name, sizeof t->name, "%.*s.%s", (int)stem, file, ext);
    } else {
        snprintf(t->name, sizeof t->name, "%s.%s", title, ext);
    }
    memset(&song, 0, sizeof song);
    song.path = t->path;
    song.format = t->format;
    song.artist = t->artist;
    song.name = t->name;
    song.album = song.md5 = "";
    song.duration_ms = sel_zxart_millis(it->time);
    song.year = it->year > 0 && it->year < 10000 ? (int32_t)it->year : 0;
    s->st->listing_rows++;
    if (!s->cat->song(s->host, &song)) {
        s->failed = 1;
        return 0;
    }
    return 1;
}

/* Reads page after page of `fmt` below `dir` into `fn` until a page is missing. Returns the pages read, -1 when a
 * page is malformed. */
static int read_pages(const char* dir, const char* fmt, const char* what, int (*fn)(void* ctx, const sel_zxart_item*),
                      void* ctx) {
    unsigned k;

    for (k = 0; k < SEL_ZXART_PAGES_MAX; k++) {
        char name[32], path[1100];
        FILE* f;
        uint32_t total;
        int32_t n;

        snprintf(name, sizeof name, fmt, k);
        if (snprintf(path, sizeof path, "%s/%s", dir, name) >= (int)sizeof path || (f = fopen(path, "rb")) == NULL) {
            return (int)k;
        }
        n = sel_zxart_read_page(f, what, fn, ctx, &total);
        fclose(f);
        if (n < 0) {
            return -1;
        }
    }
    return (int)k;
}

int sel_zxart_sync(const sel_host* host, const char* dir, sel_zxart_sync_stats* stats) {
    sync_state s;
    people ppl;
    const sel_catalogue* cat = host->catalogue != NULL ? host->catalogue(host) : NULL;
    int ok = 0, open = 0;

    memset(&s, 0, sizeof s);
    memset(&ppl, 0, sizeof ppl);
    memset(stats, 0, sizeof *stats);
    s.host = host;
    s.cat = cat;
    s.st = stats;
    s.ppl = &ppl;
    s.text = host->mem_alloc(host, sizeof *s.text, SEL_MEM_FAST);
    ppl.list = host->mem_alloc(host, (size_t)PEOPLE_MAX * sizeof *ppl.list, SEL_MEM_FAST);
    ppl.pool = host->mem_alloc(host, POOL_BYTES, SEL_MEM_FAST);
    if (s.text != NULL && ppl.list != NULL && ppl.pool != NULL && cat != NULL && cat->begin(host, "zxart") &&
        (open = 1) && read_pages(dir, SEL_ZXART_PEOPLE, "author", add_person, &ppl) > 0) {
        int pages;

        stats->people = ppl.n;
        qsort(ppl.list, ppl.n, sizeof *ppl.list, by_id);
        pages = read_pages(dir, SEL_ZXART_MUSIC, "zxMusic", on_tune, &s);
        ok = pages > 0 && !s.failed;
    }
    if (open) {
        if (ok) {
            ok = cat->commit(host);
        } else {
            cat->abort(host);
        }
    }
    if (s.text != NULL) {
        host->mem_free(host, s.text);
    }
    if (ppl.list != NULL) {
        host->mem_free(host, ppl.list);
    }
    if (ppl.pool != NULL) {
        host->mem_free(host, ppl.pool);
    }
    return ok;
}

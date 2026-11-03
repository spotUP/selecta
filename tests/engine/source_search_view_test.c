/* I1019b: the Search view shows one "From <name> (n)" section per source after Artists. Through the real entry points (command lines in, pump,
 * engine view out): stand-in sources "i1019va" (two hits), "i1019vc" (unreachable) and "i1019vd" (no hits) are searched by `SOURCE_SEARCH *`
 * while the view source is `search`. The test reads the rows the UI would draw: the headings in order (Songs, Albums, Artists, then the From
 * sections in registry order), the keyed song rows of the first section (key = source:path, the song rows play by it), and the keyless state
 * rows (Could not reach, No results). While a task still runs its section shows the keyless Searching row (sentinel: the blocked hook).
 * Enter (VIEW_ACTIVATE) on a From row queues the key and starts it; on a state row nothing happens.
 * With the From append removed in search_reload the headings are missing and the test fails; with the key play broken the queue stays empty. */
#include <selecta/engine.h>
#include <selecta/host.h>
#include <selecta/machine.h>
#include <selecta/source.h>

#include "db/db.h"
#include "source.h"

#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static atomic_int g_in_slow, g_release;

static void nap(long ms) {
    struct timespec ts;

    ts.tv_sec = 0;
    ts.tv_nsec = ms * 1000000L;
    nanosleep(&ts, NULL);
}

static int hook_a(const sel_host* host, const char* q, sel_search_sink* out) {
    char p[64];
    int i;

    (void)host;
    if (strcmp(q, "slow") == 0) { /* stands in for a network wait */
        int waited = 0;

        atomic_store(&g_in_slow, 1);
        while (!atomic_load(&g_release) && waited < 3000) {
            nap(1);
            waited++;
        }
    }
    for (i = 0; i < 2; i++) {
        snprintf(p, sizeof p, "%d/%s", i, q);
        if (!out->row(out->ctx, p, i == 0 ? "First Tune" : "Second Tune", "Some Artist", "mod")) break;
    }
    return 2;
}

static int hook_c(const sel_host* host, const char* q, sel_search_sink* out) {
    (void)host;
    (void)q;
    (void)out;
    return -1;
}

static int hook_d(const sel_host* host, const char* q, sel_search_sink* out) {
    (void)host;
    (void)q;
    (void)out;
    return 0;
}

#define SRC(ID, NAME, HOOK) { ID, NAME, "", "I1019b stand-in.", NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL, NULL, HOOK }
static const sel_source g_a = SRC("i1019va", "Fake Alpha", hook_a), g_c = SRC("i1019vc", "Fake Gamma", hook_c), g_d = SRC("i1019vd", "Fake Delta", hook_d);

static void sink(void* ctx, const sel_message* evt) {
    (void)ctx;
    (void)evt;
}

static int send_line(sel_engine* e, const char* line) {
    static char pool[8][256];
    static unsigned next;

    return sel_engine_send_line(e, line, pool[next++ & 7u], sizeof pool[0]) == 0;
}

static void drain(sel_engine* e) {
    unsigned i;

    sel_engine_pump(e, sink, NULL);
    for (i = 0; i < 5000 && sel_engine_scan_busy(e); i++) {
        sel_engine_pump(e, sink, NULL);
        nap(1);
    }
    sel_engine_pump(e, sink, NULL);
}

#define CHECK(c)                                                 \
    do {                                                         \
        if (!(c)) {                                              \
            printf("FAIL line %d: %s\n", __LINE__, #c);          \
            dump(e);                                             \
            return 1;                                            \
        }                                                        \
    } while (0)

static void dump(sel_engine* e) {
    const sel_view_state* v = sel_engine_state(e)->view;
    uint32_t i;

    for (i = 0; i < v->row_n; i++) {
        printf("  row %u kind=%u text='%s' key='%s'\n", (unsigned)i, (unsigned)v->rows[i].kind, v->rows[i].text, v->rows[i].key != NULL ? v->rows[i].key : "(null)");
    }
}

/* Index of the first row whose text starts with `prefix`, or UINT32_MAX. */
static uint32_t row_of(sel_engine* e, const char* prefix) {
    const sel_view_state* v = sel_engine_state(e)->view;
    uint32_t i;

    for (i = 0; i < v->row_n; i++) {
        if (strncmp(v->rows[i].text, prefix, strlen(prefix)) == 0) return i;
    }
    return UINT32_MAX;
}

int main(void) {
    const sel_host* host = sel_host_default();
    char path[] = "/tmp/sel-i1019b-XXXXXX";
    char db[128];
    sel_engine_config cfg;
    sel_engine* e;
    sel_library* lib;
    const sel_view_state* v;
    uint32_t i, h_songs, h_albums, h_artists, h_a, h_c, h_d, before;
    int fd = mkstemp(path);

    close(fd);
    snprintf(db, sizeof db, "%s.db", path);
    lib = sel_library_open(host, sel_machine_probe(host), 0, db);
    memset(&cfg, 0, sizeof cfg);
    cfg.library = lib;
    cfg.queue_capacity = 32u;
    e = sel_engine_create(host, &cfg);
    if (lib == NULL || e == NULL) {
        puts("FAIL setup");
        return 1;
    }
    sel_source_register_builtin();
    CHECK(sel_source_register(&g_a) && sel_source_register(&g_c) && sel_source_register(&g_d));
    CHECK(send_line(e, "PREF_SET library.sources_off \"modulespl\""));
    sel_engine_pump(e, sink, NULL);
    CHECK(send_line(e, "VIEW_SOURCE search"));
    drain(e);
    v = sel_engine_state(e)->view;
    CHECK(row_of(e, "From ") == UINT32_MAX); /* no query, no sections */

    /* A query whose first source blocks: its section shows Searching while the others have not started. */
    CHECK(send_line(e, "SOURCE_SEARCH * \"slow\""));
    sel_engine_pump(e, sink, NULL);
    for (i = 0; i < 2000 && !atomic_load(&g_in_slow); i++) nap(1);
    CHECK(atomic_load(&g_in_slow) == 1);
    sel_engine_pump(e, sink, NULL);
    v = sel_engine_state(e)->view;
    h_a = row_of(e, "From Fake Alpha (0)");
    CHECK(h_a != UINT32_MAX && h_a + 1u < v->row_n);
    CHECK(strcmp(v->rows[h_a + 1u].text, "Searching") == 0 && v->rows[h_a + 1u].key == NULL);
    CHECK(row_of(e, "From Fake Gamma (0)") != UINT32_MAX && row_of(e, "From Fake Delta (0)") != UINT32_MAX);
    atomic_store(&g_release, 1);
    drain(e);

    /* Done: Songs, Albums, Artists, then the From sections in registry order. */
    CHECK(send_line(e, "SOURCE_SEARCH * \"abc\""));
    drain(e);
    v = sel_engine_state(e)->view;
    h_songs = row_of(e, "Songs (");
    h_albums = row_of(e, "Albums (");
    h_artists = row_of(e, "Artists (");
    h_a = row_of(e, "From Fake Alpha (2)");
    h_c = row_of(e, "From Fake Gamma (0)");
    h_d = row_of(e, "From Fake Delta (0)");
    CHECK(h_songs == 0u && h_albums > h_songs && h_artists > h_albums);
    CHECK(h_a != UINT32_MAX && h_a > h_artists && h_c > h_a && h_d > h_c);
    CHECK(v->row_n == h_d + 2u);
    CHECK(v->rows[h_a + 1u].key != NULL && strcmp(v->rows[h_a + 1u].key, "i1019va:0/abc") == 0);
    CHECK(strncmp(v->rows[h_a + 1u].text, "First Tune", 10) == 0);
    CHECK(v->rows[h_a + 2u].key != NULL && strcmp(v->rows[h_a + 2u].key, "i1019va:1/abc") == 0);
    CHECK(strcmp(v->rows[h_c + 1u].text, "Could not reach Fake Gamma") == 0 && v->rows[h_c + 1u].key == NULL);
    CHECK(strcmp(v->rows[h_d + 1u].text, "No results") == 0 && v->rows[h_d + 1u].key == NULL);

    /* Enter on a state row does nothing; Enter on a From song row queues its key and starts it. */
    before = sel_engine_state(e)->queue_count;
    {
        char line[64];

        snprintf(line, sizeof line, "VIEW_CURSOR %u", (unsigned)(h_d + 1u));
        CHECK(send_line(e, line));
        sel_engine_pump(e, sink, NULL);
        CHECK(send_line(e, "VIEW_ACTIVATE"));
        sel_engine_pump(e, sink, NULL);
        CHECK(sel_engine_state(e)->queue_count == before);
        snprintf(line, sizeof line, "VIEW_CURSOR %u", (unsigned)(h_a + 2u));
        CHECK(send_line(e, line));
        sel_engine_pump(e, sink, NULL);
        CHECK(send_line(e, "VIEW_ACTIVATE"));
        sel_engine_pump(e, sink, NULL);
    }
    {
        const sel_state* st = sel_engine_state(e);

        CHECK(st->queue_count == before + 1u);
        CHECK(strcmp(st->queue_text + st->queue[before].path_offset, "i1019va:1/abc") == 0);
        CHECK(st->queue_playing == st->queue[before].id); /* the key was started, not only queued */
    }

    sel_engine_destroy(e);
    sel_library_close(lib);
    unlink(path);
    unlink(db);
    puts("source_search_view_ok");
    return 0;
}

/* I1082d: the published engine snapshot. Run under -fsanitize=thread (tests/thread/CMakeLists.txt builds a TSan copy of
 * this file when SELECTA_TSAN_TESTS is on, and the plain build runs it too).
 *
 * Part 1 (real engine): the engine thread edits the play queue and rebuilds the view 10 000 times (QUEUE_ADD, QUEUE_REMOVE,
 * QUEUE_SHUFFLE, VIEW_SOURCE queue/home/folders, VIEW_WINDOW) with a pump after each; the UI thread acquires the newest
 * snapshot through sel_engine_ui_acquire and walks the queue entries and path text, the view rows and their strings through
 * sel_engine_state. Every string must be intact. A reader that saw the live engine memory would race with the pump: TSan
 * reports it (red when the deep copy is reverted).
 * Part 2 (synthetic state): the same triple buffer with a hand-built sel_state that has every list field and a collection
 * page; a writer thread rewrites all of it in place before each publish, the reader walks every pointer field. */
#include "../../src/engine/snapshot.h"

#include <selecta/engine.h>
#include <selecta/host.h>
#include <selecta/protocol.h>

#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <string.h>

#define ROUNDS 10000u

static volatile int g_stop; /* set by the engine thread after the last pump; plain flag read through __atomic */
static uint32_t g_walks, g_bad, g_moves;

static int stopped(void) {
    return __atomic_load_n(&g_stop, __ATOMIC_ACQUIRE);
}

static unsigned walk_engine_state(const sel_state* st) {
    unsigned bad = 0, i;
    for (i = 0; i < st->queue_count; i++) {
        const char* p = st->queue_text + st->queue[i].path_offset;
        bad += strncmp(p, "/nonexistent/snap_", 18) != 0;
    }
    if (st->view != NULL) {
        for (i = 0; i < st->view->row_n; i++) {
            const sel_view_row* r = &st->view->rows[i];
            bad += r->text == NULL || strlen(r->text) > 4096u;
            if (r->key != NULL) bad += strlen(r->key) > 4096u;
        }
        if (st->view->browse_path != NULL) bad += strlen(st->view->browse_path) > 4096u;
    }
    return bad;
}

static void* ui_thread(void* arg) {
    sel_engine* e = (sel_engine*)arg;
    while (!stopped()) {
        g_moves += (uint32_t)sel_engine_ui_acquire(e);
        g_bad += walk_engine_state(sel_engine_state(e));
        g_walks++;
        sched_yield();
    }
    g_moves += (uint32_t)sel_engine_ui_acquire(e);
    g_bad += walk_engine_state(sel_engine_state(e));
    return NULL;
}

static void sink(void* ctx, const sel_message* evt) {
    (void)ctx;
    (void)evt;
}

static int part_engine(void) {
    sel_engine_config cfg;
    sel_engine* e;
    pthread_t ui;
    char line[160], pool[256];
    static const char* const views[] = {"VIEW_SOURCE queue", "VIEW_SOURCE home", "VIEW_SOURCE folders"};
    unsigned i, pubs, bmax, umax;
    uint32_t next_id = 1;

    memset(&cfg, 0, sizeof cfg);
    cfg.queue_capacity = 256u;
    e = sel_engine_create(sel_host_default(), &cfg);
    if (e == NULL || !sel_engine_publish_enable(e, 1u)) {
        puts("FAIL: engine create or publish enable");
        return 1;
    }
    sel_engine_send_line(e, "VIEW_WINDOW 64", pool, sizeof pool);
    sel_engine_pump(e, sink, NULL);
    pthread_create(&ui, NULL, ui_thread, e);
    for (i = 0; i < ROUNDS; i++) {
        if (i % 4u == 0u) {
            snprintf(line, sizeof line, "QUEUE_ADD /nonexistent/snap_%05u_with_a_longer_path_component.mod", i);
            sel_engine_send_line(e, line, pool, sizeof pool);
            next_id++;
        } else if (i % 4u == 1u && next_id > 8u) {
            snprintf(line, sizeof line, "QUEUE_REMOVE %u", (unsigned)(i / 4u));
            sel_engine_send_line(e, line, pool, sizeof pool);
        } else if (i % 97u == 0u) {
            sel_engine_send_line(e, "QUEUE_SHUFFLE", pool, sizeof pool);
        }
        sel_engine_send_line(e, views[i % 3u], pool, sizeof pool);
        sel_engine_pump(e, sink, NULL);
    }
    __atomic_store_n(&g_stop, 1, __ATOMIC_RELEASE);
    pthread_join(ui, NULL);
    sel_engine_publish_stats(e, &pubs, &bmax, &umax);
    printf("snapshot_engine publishes=%u walks=%u moves=%u bad=%u pub_bytes_max=%u pub_us_max=%u queue=%u\n", pubs, (unsigned)g_walks,
           (unsigned)g_moves, (unsigned)g_bad, bmax, umax, (unsigned)sel_engine_state(e)->queue_count);
    sel_engine_destroy(e);
    if (pubs < ROUNDS || g_bad != 0u || g_moves == 0u) {
        puts("FAIL: engine part (publishes, torn strings or no snapshot reached the reader)");
        return 1;
    }
    return 0;
}

/* ---- part 2: synthetic state with every list field ---- */
#define SYN_ROUNDS 10000u

static char s_view_text[8][48], s_rows[512], s_stats[128], s_comp[160], s_page[160];
static sel_view_row s_vrows[8];
static sel_view_state s_view;
static sel_collection_page s_page_st;
static uint32_t s_order[4];
static sel_state s_live;
static sel_queue_entry s_q[16];
static char s_qtext[16 * 48];
static sel_snap s_snap;
static volatile int g_syn_stop;
static uint32_t g_syn_bad, g_syn_moves;

static void fill(char* d, size_t n, char c) {
    memset(d, c, n - 1u);
    d[n - 1u] = '\0';
}

static void syn_rewrite(unsigned round) {
    char c = (char)('a' + round % 26u);
    unsigned i;
    size_t off = 0;
    for (i = 0; i < 8; i++) {
        fill(s_view_text[i], 48, c);
        s_vrows[i].text = s_view_text[i];
        s_vrows[i].key = s_view_text[i];
    }
    for (i = 0; i < 16; i++) {
        s_q[i].path_offset = (uint32_t)off;
        memset(s_qtext + off, c, 31);
        s_qtext[off + 31] = '\0';
        off += 32;
    }
    s_live.queue_text_bytes = (uint32_t)off;
    s_live.queue_version = round / 8u; /* the queue blob is recopied only when this moves: content moves with it */
    /* stats "k\0v\0" x2 then "\0": every byte of a string equals c */
    memset(s_stats, 0, sizeof s_stats);
    memset(s_stats, c, 5);
    memset(s_stats + 6, c, 5);
    memset(s_stats + 12, c, 5);
    memset(s_stats + 18, c, 5);
    memset(s_comp, 0, sizeof s_comp);
    memset(s_comp, c, 4);
    memset(s_comp + 5, c, 4);
    memset(s_comp + 10, '7', 1);
    memset(s_page, 0, sizeof s_page);
    memset(s_page, c, 3);
    memset(s_page + 4, c, 3);
    memset(s_page + 8, '2', 1);
    memset(s_page + 10, c, 3);
    memset(s_page + 14, c, 3);
    memset(s_page + 18, c, 3);
    memset(s_page + 22, c, 3);
    memset(s_rows, 0, sizeof s_rows);
    memset(s_rows, c, 6);
    memset(s_rows + 7, '3', 1);
    memset(s_rows + 9, c, 6);
    memset(s_rows + 16, '4', 1);
    s_page_st.serial = round / 8u;
    for (i = 0; i < 4; i++) s_order[i] = (i + round) % 4u;
}

static unsigned uniform(const char* p, char c) {
    size_t i, n = strlen(p);
    for (i = 0; i < n; i++) {
        if (p[i] != c) return 1;
    }
    return 0;
}

static unsigned syn_walk(const sel_state* st) {
    unsigned bad = 0, i;
    char c;
    if (st->view == NULL || st->view->row_n != 8u) return st->view != NULL || st->queue_count != 0u;
    c = st->view->rows[0].text[0];
    for (i = 0; i < 8; i++) bad += uniform(st->view->rows[i].text, c);
    if (st->queue_count == 16u) {
        char q = st->queue_text[st->queue[0].path_offset];
        for (i = 0; i < 16; i++) bad += uniform(st->queue_text + st->queue[i].path_offset, q);
    }
    if (st->stats != NULL) bad += uniform(st->stats, st->stats[0]) + uniform(st->stats + 6, st->stats[0]);
    if (st->composers != NULL) bad += uniform(st->composers, st->composers[0]) + uniform(st->composers + 5, st->composers[0]);
    if (st->composer_page != NULL) bad += uniform(st->composer_page + 10, st->composer_page[10]);
    if (st->collection != NULL && st->collection->rows != NULL) {
        bad += uniform(st->collection->rows, st->collection->rows[0]);
        for (i = 0; i < st->collection->shown && st->collection->order != NULL; i++) bad += st->collection->order[i] > 3u;
    }
    return bad;
}

static void* syn_reader(void* arg) {
    (void)arg;
    while (!__atomic_load_n(&g_syn_stop, __ATOMIC_ACQUIRE)) {
        g_syn_moves += (uint32_t)sel_snap_acquire(&s_snap);
        g_syn_bad += syn_walk(sel_snap_state(&s_snap));
        sched_yield();
    }
    g_syn_moves += (uint32_t)sel_snap_acquire(&s_snap);
    g_syn_bad += syn_walk(sel_snap_state(&s_snap));
    return NULL;
}

static int part_synthetic(void) {
    pthread_t rd;
    unsigned i;
    memset(&s_live, 0, sizeof s_live);
    memset(&s_view, 0, sizeof s_view);
    memset(&s_page_st, 0, sizeof s_page_st);
    s_view.rows = s_vrows;
    s_view.row_n = 8;
    s_live.view = &s_view;
    s_live.queue = s_q;
    s_live.queue_text = s_qtext;
    s_live.queue_count = 16;
    s_live.stats = s_stats;
    s_live.composers = s_comp;
    s_live.composer_page = s_page;
    s_page_st.rows = s_rows;
    s_page_st.songs = 2;
    s_page_st.shown = 4;
    s_page_st.order = s_order;
    s_page_st.drill = SEL_DRILL_PAGE_SONGS;
    s_live.collection = &s_page_st;
    sel_snap_init(&s_snap, sel_host_default());
    pthread_create(&rd, NULL, syn_reader, NULL);
    for (i = 0; i < SYN_ROUNDS; i++) {
        syn_rewrite(i);
        if (!sel_snap_publish(&s_snap, &s_live, NULL)) {
            puts("FAIL: publish ran out of memory");
            __atomic_store_n(&g_syn_stop, 1, __ATOMIC_RELEASE);
            pthread_join(rd, NULL);
            return 1;
        }
    }
    __atomic_store_n(&g_syn_stop, 1, __ATOMIC_RELEASE);
    pthread_join(rd, NULL);
    printf("snapshot_synthetic publishes=%u moves=%u bad=%u pub_bytes_max=%u pub_us_max=%u\n", (unsigned)s_snap.publishes,
           (unsigned)g_syn_moves, (unsigned)g_syn_bad, (unsigned)s_snap.bytes_max, (unsigned)s_snap.us_max);
    sel_snap_free(&s_snap);
    if (g_syn_bad != 0u || g_syn_moves == 0u) {
        puts("FAIL: synthetic part (torn copy or no snapshot reached the reader)");
        return 1;
    }
    return 0;
}

int main(void) {
    if (part_engine() != 0 || part_synthetic() != 0) return 1;
    puts("snapshot_tsan_ok");
    return 0;
}

/* T24 view-unit: the browse listing and the view model (design doc section 9b),
 * driven through the engine's command queue and snapshot. Behaviour is tested
 * where it lives; the reachability test of the whole UI is the text UI test.
 * Usage: view_test <songs dir> [group...]
 * groups: browse sort cursor activate window style   (none: all) */
#include <selecta/engine.h>
#include <selecta/host.h>
#include <selecta/protocol.h>
#include <selecta/registry.h>

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int failures;

#define CHECK(cond, ...)                                \
    do {                                                \
        if (!(cond)) {                                  \
            failures++;                                 \
            printf("FAIL %s:%d: ", __FILE__, __LINE__); \
            printf(__VA_ARGS__);                        \
            printf("\n");                               \
        }                                               \
    } while (0)

static char evlog[8192];

static void sink(void* ctx, const sel_message* evt) {
    char line[512];
    (void)ctx;
    if (sel_codec_encode(evt, line, sizeof line) > 0 && strlen(evlog) + strlen(line) + 2u < sizeof evlog) {
        strcat(evlog, line);
        strcat(evlog, "\n");
    }
}

static sel_engine* eng;

/* Sends one command line, pumps, leaves the events in evlog. */
static void run(const char* line) {
    sel_message m;
    char pool[1536];
    int err = sel_codec_decode(line, SEL_MSG_CMD, &m, pool, sizeof pool);
    evlog[0] = '\0';
    if (err != SEL_CODEC_OK) {
        CHECK(0, "decode '%s': %s", line, sel_codec_strerror(err));
        return;
    }
    sel_engine_send(eng, &m);
    sel_engine_pump(eng, sink, NULL);
}

/* An empty view stands in while the engine publishes none, so a missing view
 * model fails checks instead of crashing the test. */
static const sel_view_state* view(void) {
    static const sel_view_state none;
    const sel_view_state* v = sel_engine_state(eng)->view;
    return v ? v : &none;
}

static const char* row_text(uint32_t i) {
    const sel_view_state* v = view();
    return i >= v->window_start && i - v->window_start < v->row_n ? v->rows[i - v->window_start].text : "";
}

static int wanted(int argc, char** argv, const char* group) {
    int i;
    if (argc < 3) return 1;
    for (i = 2; i < argc; i++) {
        if (strcmp(argv[i], group) == 0) return 1;
    }
    return 0;
}

static char root[PATH_MAX - 64];

static void touch(const char* name) {
    char p[PATH_MAX];
    FILE* f;
    snprintf(p, sizeof p, "%s/%s", root, name);
    f = fopen(p, "w");
    if (f) fclose(f);
}

static void make_dir(const char* name) {
    char p[PATH_MAX];
    snprintf(p, sizeof p, "%s/%s", root, name);
    mkdir(p, 0755);
}

static void remove_tree(void) {
    const char* files[] = {"apple.mod", "Beta.mod", ".hidden", "alpha/inner.mod"};
    const char* dirs[] = {"alpha", "Zeta"};
    char p[PATH_MAX];
    size_t i;
    for (i = 0; i < sizeof files / sizeof *files; i++) {
        snprintf(p, sizeof p, "%s/%s", root, files[i]);
        unlink(p);
    }
    for (i = 0; i < sizeof dirs / sizeof *dirs; i++) {
        snprintf(p, sizeof p, "%s/%s", root, dirs[i]);
        rmdir(p);
    }
    rmdir(root);
}

static void browse_root(void) {
    char cmd[PATH_MAX];
    snprintf(cmd, sizeof cmd, "LIBRARY_BROWSE %s", root);
    run(cmd);
}

static void test_browse_and_sort(void) {
    browse_root();
    CHECK(strstr(evlog, "LIBRARY_RESULT") != NULL, "no LIBRARY_RESULT: %s", evlog);
    CHECK(strstr(evlog, "SRC_COUNT=5") != NULL, "count tag missing (.. alpha Zeta apple Beta): %s", evlog);
    CHECK(strstr(evlog, "SRC_PATH") != NULL, "path tag missing: %s", evlog);
    run("VIEW_SHOW browse");
    CHECK(view()->source == SEL_VIEW_BROWSE, "source is not browse");
    CHECK(view() && view()->row_count == 5u, "row_count %u", view()->row_count);
    CHECK(strcmp(row_text(0), "..") == 0, "row0 '%s'", row_text(0));
    CHECK(strcmp(row_text(1), "alpha") == 0, "row1 '%s'", row_text(1));
    CHECK(strcmp(row_text(2), "Zeta") == 0, "row2 '%s'", row_text(2));
    CHECK(strcmp(row_text(3), "apple.mod") == 0, "row3 '%s'", row_text(3));
    CHECK(strcmp(row_text(4), "Beta.mod") == 0, "row4 '%s'", row_text(4));
    run("LIBRARY_BROWSE /");
    CHECK(strstr(evlog, "LIBRARY_RESULT") != NULL, "root browse failed: %s", evlog);
    run("VIEW_SHOW browse");
    CHECK(strcmp(row_text(0), "..") != 0, "root listing has a parent row");
    run("LIBRARY_BROWSE /selecta-no-such-dir");
    CHECK(strstr(evlog, "ERROR browse_failed") != NULL, "missing dir: %s", evlog);
}

static void test_cursor(void) {
    uint32_t id2;
    char cmd[64];
    run("QUEUE_ADD a.mod");
    run("QUEUE_ADD b.mod");
    run("QUEUE_ADD c.mod");
    run("VIEW_SHOW queue");
    CHECK(view()->source == SEL_VIEW_QUEUE && view()->row_count == 3u, "queue view rows");
    if (view()->row_count == 0u) return;
    run("VIEW_CURSOR 1");
    CHECK(view()->cursor == 1u, "cursor %u", view()->cursor);
    run("VIEW_CURSOR +1");
    CHECK(view()->cursor == 2u, "relative cursor %u", view()->cursor);
    run("VIEW_CURSOR -9");
    CHECK(view()->cursor == 0u, "cursor clamps at top: %u", view()->cursor);
    run("VIEW_CURSOR bottom");
    CHECK(view()->cursor == 2u, "bottom %u", view()->cursor);
    run("VIEW_CURSOR top");
    CHECK(view()->cursor == 0u, "top %u", view()->cursor);
    run("VIEW_CURSOR 1");
    id2 = view()->cursor_id;
    snprintf(cmd, sizeof cmd, "QUEUE_REMOVE %u", sel_engine_state(eng)->queue[0].id);
    run(cmd);
    CHECK(view()->row_count == 2u, "rows after remove %u", view()->row_count);
    CHECK(view()->cursor_id == id2 && view()->cursor == 0u, "cursor lost its row: id %u (want %u) at %u", view()->cursor_id,
          id2, view()->cursor);
    run("QUEUE_PLAY_NEXT d.mod");
    CHECK(view()->cursor_id == id2, "cursor id moved on insert");
    CHECK(strcmp(row_text(view()->cursor), "b.mod") == 0, "queue row text '%s'", row_text(view()->cursor));
}

static void test_activate(void) {
    browse_root();
    run("VIEW_SHOW browse");
    run("VIEW_CURSOR 1");
    CHECK(strcmp(row_text(1), "alpha") == 0, "cursor row '%s'", row_text(1));
    run("VIEW_ACTIVATE");
    CHECK(strstr(evlog, "LIBRARY_RESULT") != NULL, "directory activate did not list: %s", evlog);
    CHECK(view()->row_count == 2u && strcmp(row_text(1), "inner.mod") == 0, "alpha listing rows %u", view()->row_count);
    run("VIEW_CURSOR 0");
    run("VIEW_ACTIVATE");
    CHECK(view()->row_count == 5u, "back to parent rows %u", view()->row_count);
    CHECK(view()->cursor == 1u, "cursor lands on the directory left: %u", view()->cursor);
}

static void test_activate_file(const char* songs) {
    char cmd[PATH_MAX];
    uint32_t i, found = 0;
    run("QUEUE_CLEAR");
    snprintf(cmd, sizeof cmd, "LIBRARY_BROWSE %s", songs);
    run(cmd);
    run("VIEW_SHOW browse");
    for (i = 0; i < view()->row_count; i++) {
        if (strcmp(row_text(i), "selecta-test.mod") == 0) {
            found = 1;
            break;
        }
    }
    CHECK(found, "selecta-test.mod not listed");
    if (!found) return;
    snprintf(cmd, sizeof cmd, "VIEW_CURSOR %u", i);
    run(cmd);
    run("VIEW_ACTIVATE");
    CHECK(sel_engine_state(eng)->queue_count == 1u, "queue_count %u", sel_engine_state(eng)->queue_count);
    CHECK(sel_engine_state(eng)->queue_count == 1u && sel_engine_state(eng)->queue_playing == sel_engine_state(eng)->queue[0].id &&
              sel_engine_state(eng)->queue_playing != 0u,
          "queue_playing %u", sel_engine_state(eng)->queue_playing);
}

static void test_window(void) {
    browse_root();
    run("VIEW_SHOW browse");
    run("VIEW_WINDOW 2");
    CHECK(view()->row_n == 2u, "row_n %u", view()->row_n);
    if (view()->row_count == 0u) return;
    run("VIEW_CURSOR bottom");
    CHECK(view()->window_start == view()->row_count - 2u, "window_start %u", view()->window_start);
    CHECK(view()->cursor >= view()->window_start && view()->cursor < view()->window_start + view()->row_n,
          "cursor outside window");
    CHECK(strstr(evlog, "VIEW_ROWS") != NULL, "no VIEW_ROWS: %s", evlog);
    run("VIEW_CURSOR top");
    CHECK(view()->window_start == 0u, "window follows cursor up: %u", view()->window_start);
}

static void test_style(void) {
    run("VIEW_STYLE regular");
    CHECK(strstr(evlog, "ERROR") == NULL, "regular rejected: %s", evlog);
    CHECK(strstr(evlog, "VIEW_CHANGED regular") != NULL, "no VIEW_CHANGED: %s", evlog);
    run("VIEW_STYLE tree");
    CHECK(strstr(evlog, "ERROR unsupported_style") != NULL, "tree: %s", evlog);
}

int main(int argc, char** argv) {
    const sel_host* host = sel_host_default();
    sel_engine_config cfg;
    char songs[PATH_MAX - 128];
    if (argc < 2) {
        printf("usage: view_test <songs dir> [group...]\n");
        return 2;
    }
    if (realpath(argv[1], songs) == NULL) {
        printf("songs dir '%s' not found\n", argv[1]);
        return 2;
    }
    snprintf(root, sizeof root, "%s/vt-tree", songs);
    sel_registry_init(host);
    remove_tree();
    make_dir("");
    make_dir("alpha");
    make_dir("Zeta");
    touch("apple.mod");
    touch("Beta.mod");
    touch(".hidden");
    touch("alpha/inner.mod");
    memset(&cfg, 0, sizeof cfg);
    cfg.registry_count = sel_registry_count;
    cfg.registry_get = sel_registry_get;
    eng = sel_engine_create(host, &cfg);
    if (eng == NULL) {
        printf("engine allocation failed\n");
        return 2;
    }
    if (wanted(argc, argv, "browse") || wanted(argc, argv, "sort")) test_browse_and_sort();
    if (wanted(argc, argv, "cursor")) test_cursor();
    if (wanted(argc, argv, "activate")) {
        test_activate();
        test_activate_file(songs);
    }
    if (wanted(argc, argv, "window")) test_window();
    if (wanted(argc, argv, "style")) test_style();
    sel_engine_destroy(eng);
    remove_tree();
    printf(failures ? "view-unit: %d failure(s)\n" : "view-unit: ok\n", failures);
    return failures ? 1 : 0;
}

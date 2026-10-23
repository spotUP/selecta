/* T33 title-format: the foobar2000-subset resolver (src/engine/format.c) and
 * the PREF_GET / PREF_SET verbs that feed the view model with it.
 * Fields, optional sections, %% and the bad_format fallback are tested on the
 * resolver where they live; the engine part checks that PREF_SET reaches the
 * rows of the view model and raises PREFS_CHANGED. */
#include <selecta/engine.h>
#include <selecta/host.h>
#include <selecta/protocol.h>
#include <selecta/registry.h>

#include "format.h"

#include <stdio.h>
#include <string.h>

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

static void expect(const sel_format_values* v, const char* fmt, const char* want, int want_rc) {
    char out[128];
    int rc = sel_format_expand(fmt, v, "[%author% - ]%title%", out, sizeof out);
    CHECK(rc == want_rc, "'%s': rc %d, want %d", fmt, rc, want_rc);
    CHECK(strcmp(out, want) == 0, "'%s': got '%s', want '%s'", fmt, out, want);
}

static void test_resolver(void) {
    sel_format_values v;
    memset(&v, 0, sizeof v);
    v.field[SEL_FMT_TITLE] = "Tune";
    v.field[SEL_FMT_AUTHOR] = "Composer";
    v.field[SEL_FMT_FORMAT] = "mod";
    v.field[SEL_FMT_SUBSONG] = "2";
    v.field[SEL_FMT_SUBSONGS] = "5";
    v.field[SEL_FMT_DURATION] = "1:05";
    v.field[SEL_FMT_ELAPSED] = "0:10";
    v.field[SEL_FMT_PATH] = "/m/Tune.mod";
    v.field[SEL_FMT_FILE] = "Tune.mod";
    v.field[SEL_FMT_CHANNELS] = "4";
    v.field[SEL_FMT_STATE] = "playing";
    expect(&v, "%title%", "Tune", SEL_FORMAT_OK);
    expect(&v, "%author%", "Composer", SEL_FORMAT_OK);
    expect(&v, "%format%", "mod", SEL_FORMAT_OK);
    expect(&v, "%subsong%/%subsongs%", "2/5", SEL_FORMAT_OK);
    expect(&v, "%duration% %elapsed%", "1:05 0:10", SEL_FORMAT_OK);
    expect(&v, "%path%", "/m/Tune.mod", SEL_FORMAT_OK);
    expect(&v, "%file%", "Tune.mod", SEL_FORMAT_OK);
    expect(&v, "%channels%", "4", SEL_FORMAT_OK);
    expect(&v, "%title%[  Votes %votes%]", "Tune", SEL_FORMAT_OK); /* no count: the optional section drops (I174a) */
    v.field[SEL_FMT_VOTES] = "7";
    expect(&v, "%title%[  Votes %votes%]", "Tune  Votes 7", SEL_FORMAT_OK);
    v.field[SEL_FMT_VOTES] = NULL;
    expect(&v, "%state%", "playing", SEL_FORMAT_OK);
    expect(&v, "[%author% - ]%title%", "Composer - Tune", SEL_FORMAT_OK);
    expect(&v, "100%% %title%", "100% Tune", SEL_FORMAT_OK);
    expect(&v, "%foo%", "Composer - Tune", SEL_FORMAT_BAD);
    expect(&v, "[%author%", "Composer - Tune", SEL_FORMAT_BAD);
    expect(&v, "%title", "Composer - Tune", SEL_FORMAT_BAD);
    v.field[SEL_FMT_AUTHOR] = "";
    expect(&v, "[%author% - ]%title%", "Tune", SEL_FORMAT_OK);
    v.field[SEL_FMT_AUTHOR] = NULL;
    expect(&v, "[%author% - ]%title%", "Tune", SEL_FORMAT_OK);
    expect(&v, "a[]b", "ab", SEL_FORMAT_OK);
}

static char evlog[2048];

static void sink(void* ctx, const sel_message* evt) {
    char line[256];
    (void)ctx;
    if (sel_codec_encode(evt, line, sizeof line) > 0 && strlen(evlog) + strlen(line) + 2u < sizeof evlog) {
        strcat(evlog, line);
        strcat(evlog, "\n");
    }
}

static sel_engine* eng;

static void run(const char* line) {
    char pool[512];
    evlog[0] = '\0';
    if (sel_engine_send_line(eng, line, pool, sizeof pool) != SEL_CODEC_OK) {
        CHECK(0, "decode '%s'", line);
    }
    sel_engine_pump(eng, sink, NULL);
}

static const char* row0(void) {
    const sel_view_state* v = sel_engine_state(eng)->view;
    return v && v->row_n ? v->rows[0].text : "";
}

static void test_prefs_and_rows(void) {
    run("PREF_GET view.title_format");
    CHECK(strstr(evlog, "PREFS_CHANGED view.title_format") != NULL, "PREF_GET reply: %s", evlog);
    run("PREF_GET nope.nothing");
    CHECK(strstr(evlog, "ERROR unknown_pref") != NULL, "unknown pref: %s", evlog);
    run("PREF_SET nope.nothing x");
    CHECK(strstr(evlog, "ERROR unknown_pref") != NULL, "unknown pref set: %s", evlog);
    run("QUEUE_ADD /music/Tune.mod");
    run("VIEW_SHOW queue");
    CHECK(strcmp(row0(), "Tune.mod") == 0, "default row '%s'", row0());
    run("PREF_SET view.title_format \"%file%|%format%\"");
    CHECK(strstr(evlog, "PREFS_CHANGED view.title_format") != NULL, "PREF_SET event: %s", evlog);
    CHECK(strcmp(row0(), "Tune.mod|mod") == 0, "formatted row '%s'", row0());
    run("PREF_SET view.title_format \"%foo%\"");
    CHECK(strstr(evlog, "ERROR bad_format") != NULL, "bad format rejected: %s", evlog);
    CHECK(strcmp(row0(), "Tune.mod|mod") == 0, "row kept after bad format '%s'", row0());
}

int main(void) {
    const sel_host* host = sel_host_default();
    sel_engine_config cfg;
    test_resolver();
    sel_registry_init(host);
    memset(&cfg, 0, sizeof cfg);
    cfg.registry_count = sel_registry_count;
    cfg.registry_get = sel_registry_get;
    eng = sel_engine_create(host, &cfg);
    if (eng == NULL) {
        printf("engine allocation failed\n");
        return 2;
    }
    test_prefs_and_rows();
    sel_engine_destroy(eng);
    printf(failures ? "title-format: %d failures\n" : "title-format: ok\n", failures);
    return failures ? 1 : 0;
}

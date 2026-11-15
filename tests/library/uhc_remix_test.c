/* I1407: the shared UHC remix helper (plugins/shared/source/uhc_remix.c) over the recorded AmigaRemix and Remix.Kwed.org
 * answers (tests/net/fixtures/uhc.megaburken.net/{amigaremixsearch,rkosearch}_turrican.json): rows become the key
 * "listen/<id>/<file>.mp3", a file with spaces and parentheses is percent-encoded for the stream URL, and rows outside
 * "listen/<id>/<name>.mp3" or with a hostile name are skipped. */
#include "source/uhc.h"
#include "source/uhc_remix.h"

#include <stdio.h>
#include <string.h>

#ifndef SEL_UHC_AR_FIX
#error "SEL_UHC_AR_FIX and SEL_UHC_RKO_FIX must name the recorded answers"
#endif

typedef struct {
    int n;
    char path[4][160];
    char title[4][64];
    char artist[4][32];
} rows;

static int g_fail;

static int take(void* ctx, const char* path, const char* title, const char* artist, const char* format) {
    rows* r = ctx;

    if (r->n < 4) {
        snprintf(r->path[r->n], sizeof r->path[0], "%s", path);
        snprintf(r->title[r->n], sizeof r->title[0], "%s", title);
        snprintf(r->artist[r->n], sizeof r->artist[0], "%s", artist);
    }
    r->n++;
    return strcmp(format, "mp3") == 0;
}

static void is(int line, const char* what, const char* got, const char* want) {
    if (strcmp(got, want) != 0) {
        printf("FAIL line %d: %s = '%s', want '%s'\n", line, what, got, want);
        g_fail++;
    }
}

static void eq(int line, const char* what, int got, int want) {
    if (got != want) {
        printf("FAIL line %d: %s = %d, want %d\n", line, what, got, want);
        g_fail++;
    }
}

#define IS(w, g, x) is(__LINE__, w, g, x)
#define EQ(w, g, x) eq(__LINE__, w, g, x)

static int fixture(const char* path, rows* r) {
    FILE* f = fopen(path, "rb");
    sel_search_sink sink = {r, take};
    int n;

    memset(r, 0, sizeof *r);
    if (f == NULL) {
        printf("FAIL: cannot open %s\n", path);
        g_fail++;
        return -2;
    }
    n = sel_uhc_parse(f, sel_uhc_remix_row, &sink);
    fclose(f);
    return n;
}

static int one_row(const char* dir, const char* file) {
    rows r;
    sel_search_sink sink = {&r, take};

    memset(&r, 0, sizeof r);
    return sel_uhc_remix_row(&sink, dir, file, "t", "a", "");
}

int main(void) {
    rows r;
    char url[256], scratch[256];
    sel_tag tags[4];

    EQ("amigaremix rows", fixture(SEL_UHC_AR_FIX, &r), 2);
    IS("ar path0", r.path[0], "listen/4/chris_huelsbeck_-_turrican_3_air_combat_the_rebound_by_honorabili.mp3");
    IS("ar title0", r.title[0], "Turrican 3 Air Combat (the rebound)");
    IS("ar artist0", r.artist[0], "Honorabili");

    EQ("rko rows", fixture(SEL_UHC_RKO_FIX, &r), 3);
    IS("rko path0", r.path[0], "listen/289/LMan - Turrican 2 Loader (LManic Mix).mp3");
    IS("rko artist0", r.artist[0], "LMania");

    EQ("url ok", sel_uhc_remix_url("https://uhc.remix.kwed.org/", r.path[0], url, sizeof url), 1);
    IS("url", url, "https://uhc.remix.kwed.org/listen/289/LMan%20-%20Turrican%202%20Loader%20%28LManic%20Mix%29.mp3");
    EQ("url too small", sel_uhc_remix_url("https://uhc.remix.kwed.org/", r.path[0], url, 20), 0);

    EQ("parse ok", sel_uhc_remix_parse("Remix.Kwed.org", r.path[1], scratch, sizeof scratch, tags), 1);
    EQ("tag0", (int)tags[0].tag, (int)SEL_SRC_FORMAT);
    IS("format", (const char*)tags[0].value, "Remix.Kwed.org");
    EQ("tag1", (int)tags[1].tag, (int)SEL_SRC_NAME);
    IS("name", (const char*)tags[1].value, "J Lof - Turrican Juno Mix");
    EQ("parse small", sel_uhc_remix_parse("Remix.Kwed.org", r.path[1], scratch, 8, tags), 0);

    EQ("good row", one_row("listen/4", "a.mp3"), 1);
    EQ("not listen", one_row("modules/x", "a.mp3"), -1);
    EQ("not mp3", one_row("listen/4", "a.zip"), -1);
    EQ("dotdot", one_row("listen/4", "..a.mp3"), -1);
    EQ("nested dir", one_row("listen/4/5", "a.mp3"), -1);
    EQ("no id", one_row("listen/", "a.mp3"), -1);
    EQ("query char", one_row("listen/4", "a?b.mp3"), -1);
    EQ("hash char", one_row("listen/4", "a#b.mp3"), -1);
    EQ("pipe char", one_row("listen/4", "a|b.mp3"), -1);
    EQ("empty file", one_row("listen/4", ""), -1);
    EQ("bad key url", sel_uhc_remix_url("https://x/", "listen/4/a.zip", url, sizeof url), 0);
    EQ("bad key parse", sel_uhc_remix_parse("X", "nope", scratch, sizeof scratch, tags), 0);

    if (g_fail == 0) {
        printf("uhc_remix_ok\n");
    }
    return g_fail != 0;
}

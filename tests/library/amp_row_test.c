/* I1407: the AMP row filter and URL (plugins/sources/amp/amp_row.c) over a recorded UHC "ampsearch" answer
 * (tests/net/fixtures/uhc.megaburken.net/ampsearch_jogeir.json): rows become "<index>/<name>" keys; a name whose Amiga prefix
 * is not in the extension table is skipped. */
#include "source/uhc.h"
#include "amp_row.h"

#include <stdio.h>
#include <string.h>

#ifndef SEL_AMP_FIX
#error "SEL_AMP_FIX must name the recorded answer"
#endif

typedef struct {
    int n;
    char path[8][160];
    char artist[8][64];
    char title[8][64];
} rows;

static int g_fail;

static int take(void* ctx, const char* path, const char* title, const char* artist, const char* format) {
    rows* r = ctx;

    (void)format;
    if (r->n < 8) {
        snprintf(r->path[r->n], sizeof r->path[0], "%s", path);
        snprintf(r->artist[r->n], sizeof r->artist[0], "%s", artist);
        snprintf(r->title[r->n], sizeof r->title[0], "%s", title);
    }
    r->n++;
    return 1;
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

static int one_row(const char* dest, const char* file) {
    rows r;
    sel_search_sink sink = {&r, take};

    memset(&r, 0, sizeof r);
    return sel_amp_row(&sink, dest, file, "t", "a", "");
}

static void title_of(const char* title, char* out, size_t n) {
    rows r;
    sel_search_sink sink = {&r, take};

    memset(&r, 0, sizeof r);
    sel_amp_row(&sink, "MOD.flr_deephouse.gz", "downmod.php?index=44259", title, "", "");
    snprintf(out, n, "%s", r.title[0]);
}

int main(void) {
    rows r;
    sel_search_sink sink = {&r, take};
    FILE* f = fopen(SEL_AMP_FIX, "rb");
    sel_tag tags[4];
    char scratch[256];
    char url[300];

    memset(&r, 0, sizeof r);
    if (f == NULL) {
        printf("FAIL: cannot open %s\n", SEL_AMP_FIX);
        return 1;
    }
    EQ("rows", sel_uhc_parse(f, sel_amp_row, &sink), 4); /* AON. is not in the extension table */
    fclose(f);
    IS("path0", r.path[0], "34755/XM.Celtic waves...");
    IS("artist0", r.artist[0], "Jogeir Liljedahl");
    IS("path2", r.path[2], "34758/MOD.overture");
    IS("artist2", r.artist[2], "Shorty,Jogeir Liljedahl");
    IS("path3", r.path[3], "34759/XM.sincerely...        ~");

    EQ("url", sel_amp_url(r.path[1], url, sizeof url), 1);
    IS("url text", url, "https://hippoplayer.se/cygnus/api/amp/34756/MOD.Keys%20to%20Imagination");
    EQ("url bad key", sel_amp_url("MOD.x", url, sizeof url), 0);
    EQ("url small", sel_amp_url(r.path[1], url, 10), 0);

    EQ("parse ok", sel_amp_parse(r.path[2], scratch, sizeof scratch, tags), 1);
    IS("format", (const char*)tags[0].value, "AMP");
    IS("name", (const char*)tags[1].value, "MOD.overture");
    EQ("parse small", sel_amp_parse(r.path[2], scratch, 8, tags), 0);
    EQ("bad key parse", sel_amp_parse("nope", scratch, sizeof scratch, tags), 0);

    EQ("lower prefix", one_row("mod.a.gz", "downmod.php?index=5&application=AMP"), 1);
    EQ("no gz", one_row("MOD.a", "downmod.php?index=5&application=AMP"), -1);
    EQ("no index", one_row("MOD.a.gz", "downmod.php?application=AMP"), -1);
    EQ("slash", one_row("MOD.a/b.gz", "downmod.php?index=5"), -1);
    EQ("query char", one_row("MOD.a?b.gz", "downmod.php?index=5"), -1);
    EQ("no prefix", one_row("readme.gz", "downmod.php?index=5"), -1);
    EQ("ten digits", one_row("MOD.a.gz", "downmod.php?index=1234567890"), -1);
    {
        char t[64];

        title_of("", t, sizeof t);
        IS("empty title -> module name", t, "flr_deephouse");
        title_of("Real Title", t, sizeof t);
        IS("title kept", t, "Real Title");
    }
    EQ("empty name", one_row(".gz", "downmod.php?index=5"), -1);

    if (g_fail == 0) {
        printf("amp_row_ok\n");
    }
    return g_fail != 0;
}

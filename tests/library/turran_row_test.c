/* I1407: the Turran FTP row filter (plugins/sources/turran/turran_row.c) over a recorded UHC answer
 * (tests/net/fixtures/uhc.megaburken.net/turransearch_mod.json): archives (7z, zip), .pp, a file without extension and a name the
 * index damaged ('?') are skipped; .mod and mp3 names become rows keyed "<path>/<file>". */
#include "source/uhc.h"
#include "turran_row.h"

#include <stdio.h>
#include <string.h>

#ifndef SEL_TURRAN_FIX
#error "SEL_TURRAN_FIX must name the recorded answer"
#endif

typedef struct {
    int n;
    char path[8][160];
    char format[8][16];
    char title[8][64];
} rows;

static int g_fail;

static int take(void* ctx, const char* path, const char* title, const char* artist, const char* format) {
    rows* r = ctx;

    (void)artist;
    if (r->n < 8) {
        snprintf(r->title[r->n], sizeof r->title[0], "%s", title);
        snprintf(r->path[r->n], sizeof r->path[0], "%s", path);
        snprintf(r->format[r->n], sizeof r->format[0], "%s", format);
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

static int one_row(const char* dir, const char* file) {
    rows r;
    sel_search_sink sink = {&r, take};

    memset(&r, 0, sizeof r);
    return sel_turran_row(&sink, dir, file, "t", "a", "");
}

int main(void) {
    rows r;
    sel_search_sink sink = {&r, take};
    FILE* f = fopen(SEL_TURRAN_FIX, "rb");
    sel_tag tags[4];
    char scratch[256];

    memset(&r, 0, sizeof r);
    if (f == NULL) {
        printf("FAIL: cannot open %s\n", SEL_TURRAN_FIX);
        return 1;
    }
    EQ("rows", sel_uhc_parse(f, sel_turran_row, &sink), 3);
    fclose(f);
    IS("path0", r.path[0], "Collection/Music/Mod/Ripped By Chip/Protracker/Andy (UK)/runaway short.mod");
    IS("format0", r.format[0], "mod");
    IS("title0 falls back to the file name (I1457)", r.title[0], "runaway short");
    IS("title2", r.title[2], "Awesome-A - Turrican (WIP)");
    IS("path1", r.path[1], "Collection/Music/Mod/Ripped By Chip/Protracker/Animotion/revolution.mod");
    IS("path2", r.path[2], "Collection/Music/MP3_OGG_WAV/AmigaRemix/Awesome-A - Turrican (WIP).mp3");
    IS("format2", r.format[2], "mp3");
    EQ(".pp is not in the extension table", one_row("Collection/Music/Mod/MODtolkien", "WE.MOD.pp"), -1);

    EQ("parse ok", sel_turran_parse(r.path[1], scratch, sizeof scratch, tags), 1);
    EQ("tag0", (int)tags[0].tag, (int)SEL_SRC_FORMAT);
    IS("format", (const char*)tags[0].value, "Turran FTP");
    EQ("tag1", (int)tags[1].tag, (int)SEL_SRC_NAME);
    IS("name", (const char*)tags[1].value, "revolution.mod");
    EQ("parse small", sel_turran_parse(r.path[1], scratch, 8, tags), 0);
    EQ("bad key parse", sel_turran_parse("nope.mod", scratch, sizeof scratch, tags), 0);

    EQ("upper ext", one_row("Collection/Music/X", "A.MOD"), 1);
    EQ("zip", one_row("Collection/Music/X", "a.zip"), -1);
    EQ("no ext", one_row("Collection/Music/X", "TURRICAN"), -1);
    EQ("outside", one_row("TOSEC/X", "a.mod"), -1);
    EQ("dotdot", one_row("Collection/../X", "a.mod"), -1);
    EQ("query char", one_row("Collection/Music", "a?b.mod"), -1);
    EQ("hash char", one_row("Collection/Music", "a#b.mod"), -1);
    EQ("empty file", one_row("Collection/Music", ""), -1);

    if (g_fail == 0) {
        printf("turran_row_ok\n");
    }
    return g_fail != 0;
}

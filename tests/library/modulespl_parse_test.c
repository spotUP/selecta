/* I406: the Modules.pl search page parser (plugins/sources/modulespl/modulespl_parse.c) over the recorded response
 * (tests/net/fixtures/www.modules.pl/search_chip.html, see PROVENANCE.md): rows with a file name are delivered with the
 * entities decoded and several artists joined, a row whose format the site cannot play is skipped, a page without the
 * result line and a hostile file name deliver nothing, and the receiver can stop the walk. */
#include "modulespl_parse.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef SEL_MODULESPL_FIX
#error "SEL_MODULESPL_FIX must name the recorded search page"
#endif

typedef struct {
    int n;
    int stop_after;
    char path[8][128];
    char title[8][64];
    char artist[8][64];
    char format[8][16];
} rows;

static int g_fail;

static int take(void* ctx, const char* path, const char* title, const char* artist, const char* format) {
    rows* r = ctx;

    if (r->n < 8) {
        snprintf(r->path[r->n], sizeof r->path[0], "%s", path);
        snprintf(r->title[r->n], sizeof r->title[0], "%s", title);
        snprintf(r->artist[r->n], sizeof r->artist[0], "%s", artist);
        snprintf(r->format[r->n], sizeof r->format[0], "%s", format);
    }
    r->n++;
    return r->stop_after == 0 || r->n < r->stop_after;
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

static char* slurp(const char* path, size_t* n) {
    FILE* f = fopen(path, "rb");
    char* buf;
    long sz;

    if (f == NULL) {
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    buf = malloc((size_t)sz + 1u);
    if (buf == NULL || fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
        fclose(f);
        free(buf);
        return NULL;
    }
    fclose(f);
    buf[sz] = '\0';
    *n = (size_t)sz;
    return buf;
}

/* One hand-made result row around `file` (the text inside modPlayer("../modules/ ... ")). */
static int one_row(const char* file, rows* r) {
    char page[1024];

    memset(r, 0, sizeof *r);
    snprintf(page, sizeof page,
             "<p>1 module(s) met your search criteria:</p><table><tr valign=\"middle\"><td>"
             "<span class=\"format-small\" style=\"x\">mod</span> <a  href='?id=modules&aid=1'>A</a> "
             "<a class=\"module\" href=\"?id=module&mod=1\">T</a> "
             "<img onclick='modPlayer(\"../modules/%s\", \"x\", \"1\")'></td></tr></table>",
             file);
    return sel_modulespl_parse(page, strlen(page), take, r);
}

int main(void) {
    rows r;
    size_t n = 0;
    char* page = slurp(SEL_MODULESPL_FIX, &n);

    if (page == NULL) {
        printf("FAIL cannot read %s\n", SEL_MODULESPL_FIX);
        return 1;
    }
    memset(&r, 0, sizeof r);
    EQ("delivered rows (4 rows, one unplayable)", sel_modulespl_parse(page, n, take, &r), 3);
    EQ("callback count", r.n, 3);
    IS("path 0", r.path[0], "sixtus/Sixtus_-_--_Only_chip_--.zip");
    IS("title 0", r.title[0], "-- Only chip --");
    IS("artist 0", r.artist[0], "Sixtus");
    IS("format 0", r.format[0], "mod");
    IS("path 1 decodes &#39;", r.path[1], "arachno/Arachno_-_Chip_'o_hoy.zip");
    IS("title 1", r.title[1], "Chip 'o hoy");
    IS("artist 1", r.artist[1], "Arachno");
    IS("path 2 keeps the raw &", r.path[2], "cooperation/Dan_&_Scorpik_-_Chip-chop.zip");
    IS("title 2", r.title[2], "Chip-chop");
    free(page);

    EQ("page without the result line", sel_modulespl_parse("<html></html>", 13, take, &r), 0);

    EQ("plain row", one_row("a/b.zip", &r), 1);
    IS("plain path", r.path[0], "a/b.zip");
    EQ("path with a second slash", one_row("a/b/c.zip", &r), 0);
    EQ("path escaping its directory", one_row("a/../b.zip", &r), 0);
    EQ("path that is not a zip", one_row("a/b.mod", &r), 0);
    EQ("path with a query", one_row("a/b.zip?x", &r), 0);

    memset(&r, 0, sizeof r);
    r.stop_after = 1;
    page = slurp(SEL_MODULESPL_FIX, &n);
    EQ("receiver stops the walk", sel_modulespl_parse(page, n, take, &r), 1);
    free(page);

    if (g_fail != 0) {
        printf("modulespl_parse: %d failure(s)\n", g_fail);
        return 1;
    }
    printf("modulespl_parse_ok\n");
    return 0;
}

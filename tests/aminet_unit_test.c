/* I405: the Aminet INDEX line parser and source descriptor (plugins/sources/aminet) over the recorded INDEX
 * (tests/net/fixtures/aminet.net/INDEX, 14 real data lines of the 2026-10-30 file, see PROVENANCE.md).
 * SEL_AMINET_INDEX is its path. Expected: the mods/ archive rows are kept (10), the other directories and the MP3
 * file are dropped (4), header lines are never rows. */
#include "aminet_sync.h"

#include <selecta/catalogue.h>
#include <selecta/plugin.h>
#include <selecta/source.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static int failures;

#define CHECK(cond)                                                       \
    do {                                                                  \
        if (!(cond)) {                                                    \
            printf("FAIL line %d: %s\n", __LINE__, #cond);                \
            failures++;                                                   \
        }                                                                 \
    } while (0)

static sel_aminet_row g_rows[32];

/* I1411: a stub library that records each synced (path, artist). */
static char g_paths[16][64], g_artists[16][64];
static int g_songs;

static void* t_alloc(const sel_host* h, size_t n, uint32_t kind) {
    (void)h;
    (void)kind;
    return malloc(n);
}
static void t_free(const sel_host* h, void* p) {
    (void)h;
    free(p);
}
static int t_begin(const sel_host* h, const char* id) {
    (void)h;
    (void)id;
    return 1;
}
static int t_song(const sel_host* h, const sel_catalogue_song* s) {
    (void)h;
    if (g_songs < 16) {
        snprintf(g_paths[g_songs], sizeof g_paths[0], "%s", s->path);
        snprintf(g_artists[g_songs], sizeof g_artists[0], "%s", s->artist);
    }
    g_songs++;
    return 1;
}
static int t_commit(const sel_host* h) {
    (void)h;
    return 1;
}
static void t_abort(const sel_host* h) {
    (void)h;
}
static sel_catalogue g_cat;
static const sel_catalogue* t_cat(const sel_host* h) {
    (void)h;
    return &g_cat;
}

/* One INDEX line whose description ends in "by <artist>"; `full` pads it to the whole column so the artist reads as cut. */
static void put(FILE* f, const char* name, const char* artist, int full) {
    char desc[64];
    size_t tail = strlen(artist) + 4u, pad = full ? SEL_AMINET_DESC_COLS - tail : 4u;

    memset(desc, 'x', pad);
    snprintf(desc + pad, sizeof desc - pad, " by %s", artist);
    fprintf(f, "%-30s mods/misc   10K 999 %s\n", name, desc);
}

static const char* synced(const char* path) {
    int i;

    for (i = 0; i < g_songs && i < 16; i++) {
        if (strcmp(g_paths[i], path) == 0) {
            return g_artists[i];
        }
    }
    return "(missing)";
}

int main(void) {
    FILE* f = fopen(SEL_AMINET_INDEX, "rb");
    char line[1100];
    int kept = 0, dropped = 0, headers = 0, i;
    const sel_source* src = NULL;
    char buf[256], scratch[256], path[256];
    sel_tag tags[4];

    CHECK(f != NULL);
    while (f != NULL && fgets(line, sizeof line, f) != NULL) {
        size_t n = strlen(line);

        while (n > 0 && (line[n - 1u] == '\n' || line[n - 1u] == '\r')) {
            line[--n] = '\0';
        }
        if (line[0] == '|') {
            headers++;
            CHECK(!sel_aminet_parse_line(line, &g_rows[0]));
        } else if (sel_aminet_parse_line(line, &g_rows[kept < 32 ? kept : 31])) {
            kept++;
        } else {
            dropped++;
        }
    }
    if (f != NULL) {
        fclose(f);
    }
    CHECK(headers == 2);
    CHECK(kept == 10);
    CHECK(dropped == 4);
    if (kept == 10) {
        for (i = 0; i < kept; i++) {
            CHECK(strncmp(g_rows[i].path, "mods/", 5) == 0);
        }
        CHECK(strcmp(g_rows[1].path, "mods/4mat/AnarchyMenu10.lha") == 0);
        CHECK(strcmp(g_rows[1].name, "AnarchyMenu10") == 0);
        CHECK(strcmp(g_rows[1].artist, "4mat") == 0);
        CHECK(g_rows[1].size == 5120u);
        CHECK(strcmp(g_rows[0].artist, "- unknown") == 0); /* "MP3 version of Velvet Memories": no "by" */
        CHECK(g_rows[0].size == 9017753u);                 /* 8.6M */
        CHECK(strcmp(g_rows[4].artist, "HKvalhe") == 0);
        CHECK(strcmp(g_rows[5].path, "mods/8voic/OkinawaTeaCer.zip") == 0);
        CHECK(strcmp(g_rows[5].name, "OkinawaTeaCer") == 0);
        CHECK(strcmp(g_rows[6].path, "mods/8voic/ambi-ank.lzh") == 0);
        CHECK(strcmp(g_rows[7].artist, "volton") == 0);
        CHECK(strcmp(g_rows[9].artist, "The Kraken") == 0);
    }
    CHECK(sel_aminet_parse_line("x.lha mods/a 1K 1 y", &g_rows[0])); /* a well formed mods line parses */
    CHECK(!sel_aminet_parse_line("x.mp3 mods/a 1K 1 y", &g_rows[0]));
    CHECK(!sel_aminet_parse_line("x.lha util/a 1K 1 y", &g_rows[0]));
    CHECK(!sel_aminet_parse_line("x.lha mods/../a 1K 1 y", &g_rows[0]));

    {
        const sel_tag* desc = sel_plugin_entry(1);
        const sel_tag* hooks = (const sel_tag*)sel_tag_find(desc, SEL_PLUGIN_TAGS, 0);

        CHECK(sel_tag_find(desc, SEL_PLUGIN_KIND, 0) == SEL_PLUGIN_KIND_SOURCE);
        src = (const sel_source*)sel_tag_find(hooks, SEL_SRC_DESCRIPTOR, 0);
    }
    CHECK(src != NULL);
    if (src != NULL) {
        CHECK(strcmp(src->id, "aminet") == 0);
        CHECK(src->url("mods/4mat/AnarchyMenu10.lha", buf, sizeof buf));
        CHECK(strcmp(buf, "https://aminet.net/mods/4mat/AnarchyMenu10.lha") == 0);
        CHECK(src->path_in(buf, path, sizeof path));
        CHECK(strcmp(path, "mods/4mat/AnarchyMenu10.lha") == 0);
        CHECK(!src->path_in("https://example.org/x.lha", path, sizeof path));
        CHECK(src->parse("mods/4mat/AnarchyMenu10.lha", scratch, sizeof scratch, tags));
        CHECK(tags[0].tag == SEL_SRC_FORMAT && strcmp((const char*)tags[0].value, SEL_AMINET_FORMAT) == 0);
        CHECK(tags[1].tag == SEL_SRC_ARTIST && strcmp((const char*)tags[1].value, "4mat") == 0);
        CHECK(tags[2].tag == SEL_SRC_NAME && strcmp((const char*)tags[2].value, "AnarchyMenu10") == 0);
        CHECK(tags[3].tag == SEL_TAG_END);
        CHECK(!src->parse("Loose.lha", scratch, sizeof scratch, tags));
    }
    {
        /* I1411: " - <title>" tails go; a cut artist takes the one full artist it prefixes; two candidates leave it. */
        sel_host host;
        sel_aminet_sync_stats st;
        FILE* idx;

        CHECK(sel_aminet_parse_line("m_jazz.lha mods/misc 10K 999 Jazz by Mortimer Twang - jazzy", &g_rows[0]));
        CHECK(strcmp(g_rows[0].artist, "Mortimer Twang") == 0 && !g_rows[0].cut);
        CHECK(sel_aminet_parse_line("s.lha mods/8voic 1K 999 S3M-Mod by Nemesis - iCE & REM", &g_rows[0]));
        CHECK(strcmp(g_rows[0].artist, "Nemesis") == 0);
        memset(&host, 0, sizeof host);
        memset(&g_cat, 0, sizeof g_cat);
        host.mem_alloc = t_alloc;
        host.mem_free = t_free;
        host.catalogue = t_cat;
        g_cat.begin = t_begin;
        g_cat.song = t_song;
        g_cat.commit = t_commit;
        g_cat.abort = t_abort;
        mkdir("aminet_cut_test", 0755);
        idx = fopen("aminet_cut_test/" SEL_AMINET_LIST, "wb");
        CHECK(idx != NULL);
        if (idx != NULL) {
            put(idx, "m_near.lha", "Mortimer Tw", 1);
            put(idx, "m_play.lha", "Mortimer T", 1);
            put(idx, "c_cut.lha", "Coun", 1);
            put(idx, "z_cut.lha", "Zed", 1);
            put(idx, "m_full.lha", "Mortimer Twang", 0);
            put(idx, "c_a.lha", "Counterpoint", 0);
            put(idx, "c_b.lha", "Country Kid", 0);
            fclose(idx);
            CHECK(sel_aminet_sync(&host, "aminet_cut_test", &st));
            CHECK(g_songs == 7);
            CHECK(strcmp(synced("mods/misc/m_near.lha"), "Mortimer Twang") == 0);
            CHECK(strcmp(synced("mods/misc/m_play.lha"), "Mortimer Twang") == 0);
            CHECK(strcmp(synced("mods/misc/c_cut.lha"), "Coun") == 0);
            CHECK(strcmp(synced("mods/misc/z_cut.lha"), "Zed") == 0);
            CHECK(strcmp(synced("mods/misc/c_a.lha"), "Counterpoint") == 0);
        }
    }
    if (failures == 0) {
        printf("aminet_unit_ok\n");
    }
    return failures == 0 ? 0 : 1;
}

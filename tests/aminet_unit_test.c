/* I405: the Aminet INDEX line parser and source descriptor (plugins/sources/aminet) over the recorded INDEX
 * (tests/net/fixtures/aminet.net/INDEX, 14 real data lines of the 2026-10-30 file, see PROVENANCE.md).
 * SEL_AMINET_INDEX is its path. Expected: the mods/ archive rows are kept (10), the other directories and the MP3
 * file are dropped (4), header lines are never rows. */
#include "aminet_sync.h"

#include <selecta/plugin.h>
#include <selecta/source.h>

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(cond)                                                       \
    do {                                                                  \
        if (!(cond)) {                                                    \
            printf("FAIL line %d: %s\n", __LINE__, #cond);                \
            failures++;                                                   \
        }                                                                 \
    } while (0)

static sel_aminet_row g_rows[32];

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
    if (failures == 0) {
        printf("aminet_unit_ok\n");
    }
    return failures == 0 ? 0 : 1;
}

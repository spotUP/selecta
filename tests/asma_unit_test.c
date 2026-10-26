/* I413: the asma list reader, the artist rule, the source URL round trip and parse (plugins/sources/asma).
 * The list is the recorded excerpt tests/net/fixtures/asma.atari.org/asmadb/asmadb.js (SEL_ASMA_FIXTURE). */
#include "asma_sync.h"

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

typedef struct seen {
    int n;
    sel_asma_entry e[16];
} seen;

static int collect(void* ctx, const sel_asma_entry* e) {
    seen* s = ctx;

    if (s->n < 16) {
        s->e[s->n] = *e;
    }
    s->n++;
    return 1;
}

static void artist(const char* author, const char* want) {
    char out[128];

    CHECK(sel_asma_artist(author, out, sizeof out));
    if (strcmp(out, want) != 0) {
        printf("artist of '%s': got '%s', want '%s'\n", author, out, want);
        failures++;
    }
}

static seen g_seen;

int main(void) {
    FILE* f = fopen(SEL_ASMA_FIXTURE, "rb");
    const sel_source* src;
    char buf[256], scratch[256];
    sel_tag tags[4];
    int32_t n;

    CHECK(f != NULL);
    if (f != NULL) {
        n = sel_asma_read_list(f, collect, &g_seen);
        CHECK(n == 10 && g_seen.n == 10);
        CHECK(strcmp(g_seen.e[0].path, "Composers/Aki/Atari_Style.sap") == 0);
        CHECK(strcmp(g_seen.e[0].title, "[pong] Atari Style") == 0);
        CHECK(g_seen.e[0].size == 7291);
        CHECK(strcmp(g_seen.e[0].author, "<?> (Aki)") == 0);
        CHECK(strcmp(g_seen.e[9].path, "Games/221B_Baker_Street.sap") == 0);
        CHECK(g_seen.e[9].size == 1969 && g_seen.e[9].author[0] == '\0');
        fclose(f);
    }
    {
        FILE* none = tmpfile(); /* a file with no "fileInfos" is an error, not an empty list */

        if (none != NULL) {
            fputs("const asma = {\"composerInfos\": []}", none);
            rewind(none);
            CHECK(sel_asma_read_list(none, collect, &g_seen) == -1);
            fclose(none);
        }
    }

    artist("<?> (Aki)", "Aki");
    artist("Jeremie Bernard (Glafouk)", "Glafouk");
    artist("Glafouk", "Glafouk");
    artist("Adam Bienias (SoTe) & Michal Szpilowski (Miker) & Lukasz Sychowicz (X-Ray)", "SoTe & Miker & X-Ray");
    artist("", "- unknown");
    artist("<?>", "- unknown");

    {
        const sel_tag* desc = sel_plugin_entry(1);
        const sel_tag* hooks = (const sel_tag*)sel_tag_find(desc, SEL_PLUGIN_TAGS, 0);

        CHECK(sel_tag_find(desc, SEL_PLUGIN_KIND, 0) == SEL_PLUGIN_KIND_SOURCE);
        src = (const sel_source*)sel_tag_find(hooks, SEL_SRC_DESCRIPTOR, 0);
    }
    CHECK(src != NULL);
    if (src != NULL) {
        CHECK(src->url("Composers/Aki/Atari_Style.sap", buf, sizeof buf));
        CHECK(strcmp(buf, "https://asma.atari.org/asma/Composers/Aki/Atari_Style.sap") == 0);
        CHECK(src->url("Games/a b&c.sap", buf, sizeof buf));
        CHECK(strcmp(buf, "https://asma.atari.org/asma/Games/a%20b%26c.sap") == 0);
        CHECK(src->path_in(buf, scratch, sizeof scratch) && strcmp(scratch, "Games/a b&c.sap") == 0);
        CHECK(!src->path_in("https://modland.com/pub/modules/x.mod", scratch, sizeof scratch));
        CHECK(!src->url("/abs.sap", buf, sizeof buf));

        CHECK(src->parse("Composers/Bernard_Jeremie/Altazar.ttt", scratch, sizeof scratch, tags));
        CHECK(strcmp((const char*)tags[0].value, "TIATracker") == 0);
        CHECK(strcmp((const char*)tags[1].value, "Bernard Jeremie") == 0);
        CHECK(strcmp((const char*)tags[2].value, "Altazar") == 0 && tags[3].tag == SEL_TAG_END);
        CHECK(src->parse("Games/221B_Baker_Street.sap", scratch, sizeof scratch, tags));
        CHECK(strcmp((const char*)tags[0].value, "Slight Atari Player") == 0);
        CHECK(((const char*)tags[1].value)[0] == '\0');
        CHECK(strcmp((const char*)tags[2].value, "221B_Baker_Street") == 0);
    }

    if (failures == 0) {
        puts("asma_unit_ok");
    }
    return failures == 0 ? 0 : 1;
}

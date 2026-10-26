/* I416: the keygenmusic list reader, path decoding, format and group rules and source descriptor
 * (plugins/sources/keygenmusic). SEL_KEYGEN_FIX_LIST is the recorded lib.txt under tests/net/fixtures. */
#include "keygenmusic_sync.h"

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
    sel_keygen_entry first, last;
} seen;

static int collect(void* ctx, const sel_keygen_entry* e) {
    seen* s = ctx;

    if (s->n++ == 0) {
        s->first = *e;
    }
    s->last = *e;
    return 1;
}

int main(void) {
    seen s;
    char buf[256], url[512], path[256];
    const sel_source* src;
    FILE* f;

    memset(&s, 0, sizeof s);
    f = fopen(SEL_KEYGEN_FIX_LIST, "rb");
    CHECK(f != NULL);
    CHECK(sel_keygen_read_list(f, collect, &s) == 9);
    fclose(f);
    CHECK(s.n == 9);
    CHECK(strcmp(s.first.path, "/kgm/!Others/0BiT - Fax Spider 2.1 kg.xm") == 0);
    CHECK(strcmp(s.first.group, "0BiT") == 0 && strcmp(s.first.what, "Fax Spider 2.1 kg") == 0);
    CHECK(strcmp(s.last.group, "Serials2000 v7") == 0);

    f = tmpfile(); /* not a list, and a list that stops inside an object */
    fputs("{\"a\":1}", f);
    rewind(f);
    CHECK(sel_keygen_read_list(f, NULL, NULL) == -1);
    fclose(f);
    f = tmpfile();
    fputs("[{\"path\":\"/kgm/a/b.xm\"},{\"path\":\"/kg", f);
    rewind(f);
    CHECK(sel_keygen_read_list(f, collect, &s) == 1);
    fclose(f);

    /* paths: decoded, below /kgm/ only, safe only */
    CHECK(sel_keygen_path("/kgm/!Others/Lazzy - CrackMe%231.xm", buf, sizeof buf) &&
          strcmp(buf, "!Others/Lazzy - CrackMe#1.xm") == 0);
    CHECK(sel_keygen_path("/kgm/!Others/DOT.EXE - Alcohol 120%25 1.xm", buf, sizeof buf) &&
          strcmp(buf, "!Others/DOT.EXE - Alcohol 120% 1.xm") == 0);
    CHECK(!sel_keygen_path("/other/a.xm", buf, sizeof buf));
    CHECK(!sel_keygen_path("/kgm/", buf, sizeof buf));
    CHECK(!sel_keygen_path("/kgm/a/../b.xm", buf, sizeof buf));
    CHECK(!sel_keygen_path("/kgm/a%2Fb%zz.xm", buf, sizeof buf));
    CHECK(!sel_keygen_path("/kgm/a/b.xm", buf, 3));

    CHECK(strcmp(sel_keygen_format("a/b.XM"), "Fasttracker 2") == 0);
    CHECK(strcmp(sel_keygen_format("a/b.mod"), "Protracker") == 0);
    CHECK(strcmp(sel_keygen_format("a/b.it"), "Impulse Tracker") == 0);
    CHECK(strcmp(sel_keygen_format("a.b/c.s3m"), "Scream Tracker 3") == 0);
    CHECK(strcmp(sel_keygen_format("a.mod/c"), "Fasttracker 2") == 0);

    CHECK(sel_keygen_group("TSRh/TSRh - Opus.xm", "", buf, sizeof buf) && strcmp(buf, "TSRh") == 0);
    CHECK(sel_keygen_group("!Others/CiO - Mini.xm", "", buf, sizeof buf) && strcmp(buf, "CiO") == 0);
    CHECK(sel_keygen_group("!Others/CiO - Mini.xm", "Real", buf, sizeof buf) && strcmp(buf, "Real") == 0);
    CHECK(sel_keygen_group("!Others/NoSeparator.xm", "", buf, sizeof buf) && strcmp(buf, "- unknown") == 0);
    CHECK(!sel_keygen_group("TSRh/x.xm", "", buf, 3));

    {
        const sel_tag* desc = sel_plugin_entry(1);
        const sel_tag* hooks = (const sel_tag*)sel_tag_find(desc, SEL_PLUGIN_TAGS, 0);

        CHECK(sel_tag_find(desc, SEL_PLUGIN_KIND, 0) == SEL_PLUGIN_KIND_SOURCE);
        src = (const sel_source*)sel_tag_find(hooks, SEL_SRC_DESCRIPTOR, 0);
    }
    CHECK(src != NULL && src->url != NULL && src->path_in != NULL && src->parse != NULL);
    if (src != NULL) {
        sel_tag tags[4];

        CHECK(src->url("!Others/Lazzy - CrackMe#1.xm", url, sizeof url) &&
              strcmp(url, "https://keygenmusic.tk/kgm/%21Others/Lazzy%20-%20CrackMe%231.xm") == 0);
        CHECK(src->path_in(url, path, sizeof path) && strcmp(path, "!Others/Lazzy - CrackMe#1.xm") == 0);
        CHECK(!src->url("/abs.xm", url, sizeof url));
        CHECK(!src->path_in("https://example.com/x", path, sizeof path));
        CHECK(src->parse("TSRh/TSRh - Opus Creator 9.01 kg.xm", buf, sizeof buf, tags));
        CHECK(strcmp((const char*)tags[0].value, "Fasttracker 2") == 0);
        CHECK(strcmp((const char*)tags[1].value, "TSRh") == 0);
        CHECK(strcmp((const char*)tags[2].value, "TSRh - Opus Creator 9.01 kg") == 0);
    }
    if (failures == 0) {
        printf("keygenmusic_unit_ok\n");
    }
    return failures != 0;
}

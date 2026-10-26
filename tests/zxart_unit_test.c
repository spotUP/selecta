/* I415: the zxart page reader, format table, length parser and source descriptor (src/library/sources/zxart*.c).
 * SEL_ZXART_FIX_TUNES and SEL_ZXART_FIX_PEOPLE are the recorded API answers under tests/net/fixtures/zxart.ee. */
#include "sources/zxart_sync.h"
#include "source.h"

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
    sel_zxart_item first, ts;
} seen;

static int collect(void* ctx, const sel_zxart_item* it) {
    seen* s = ctx;

    if (s->n++ == 0) {
        s->first = *it;
    }
    if (strcmp(it->type, "TS") == 0) {
        s->ts = *it;
    }
    return 1;
}

int main(void) {
    seen s;
    uint32_t total = 0;
    char buf[64], url[256], path[256];
    const sel_source* src;
    FILE* f;

    memset(&s, 0, sizeof s);
    f = fopen(SEL_ZXART_FIX_TUNES, "rb");
    CHECK(f != NULL);
    CHECK(sel_zxart_read_page(f, "zxMusic", collect, &s, &total) == 6);
    fclose(f);
    CHECK(total == 6 && s.n == 6);
    CHECK(s.first.id == 19636 && s.first.author_n == 1 && s.first.authors[0] == 7744);
    CHECK(strcmp(s.first.type, "PT3") == 0 && s.first.has_url && s.first.year == 2013);
    CHECK(strstr(s.first.file, "Beyond_Time_%282013%29") != NULL);
    CHECK(s.ts.id == 19721 && s.ts.authors[0] == 19720);

    memset(&s, 0, sizeof s);
    f = fopen(SEL_ZXART_FIX_PEOPLE, "rb");
    CHECK(f != NULL);
    CHECK(sel_zxart_read_page(f, "author", collect, &s, &total) == 6 && s.n == 6);
    fclose(f);
    f = fopen(SEL_ZXART_FIX_PEOPLE, "rb"); /* the wrong list name reads no objects and so is no success */
    CHECK(sel_zxart_read_page(f, "zxMusic", NULL, NULL, &total) == 0);
    fclose(f);

    CHECK(strcmp(sel_zxart_format("PT3", "pt3", buf, sizeof buf), "Spectrum - Pro Tracker 3") == 0);
    CHECK(strcmp(sel_zxart_format("TS", "ts", buf, sizeof buf), "Spectrum - Pro Tracker 3") == 0);
    CHECK(strcmp(sel_zxart_format("PT3", "sna", buf, sizeof buf), "Spectrum - Snapshot") == 0);
    CHECK(strcmp(sel_zxart_format("XYZ", "xyz", buf, sizeof buf), "Spectrum - XYZ") == 0);
    CHECK(strcmp(sel_zxart_format("", "", buf, sizeof buf), "Spectrum") == 0);
    CHECK(sel_zxart_millis("3:14.88") == 194880);
    CHECK(sel_zxart_millis("0:07.00") == 7000);
    CHECK(sel_zxart_millis("") == 0 && sel_zxart_millis("x:yy") == 0);

    sel_source_register_builtin();
    src = sel_source_find("zxart");
    CHECK(src != NULL && src->url != NULL && src->path_in != NULL);
    if (src != NULL) {
        CHECK(src->url("19636/A B(1).pt3", url, sizeof url) &&
              strcmp(url, "https://zxart.ee/file/id:19636/filename:A%20B%281%29.pt3") == 0);
        CHECK(src->path_in(url, path, sizeof path) && strcmp(path, "19636/A B(1).pt3") == 0);
        CHECK(!src->url("noslash", url, sizeof url));
        CHECK(!src->path_in("https://example.com/x", path, sizeof path));
    }
    if (failures == 0) {
        printf("zxart_unit_ok\n");
    }
    return failures != 0;
}

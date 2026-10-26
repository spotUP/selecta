/* I414: the sndh central directory reader, end-of-directory finder, download page parser and source descriptor
 * (src/library/sources/sndh*.c). SEL_SNDH_ZIP is the recorded trimmed archive, SEL_SNDH_PAGE the recorded
 * /download.php, both under tests/net/fixtures/sndh.atari.org. */
#include "sources/sndh_sync.h"
#include "source.h"

#include <stdio.h>
#include <stdlib.h>
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
    int n, songs;
    char first_song[SEL_SNDH_PATH_MAX];
} seen;

static int collect(void* ctx, const sel_sndh_entry* e) {
    seen* s = ctx;
    size_t len = strlen(e->name);

    s->n++;
    if (len > 5 && strcmp(e->name + len - 5, ".sndh") == 0) {
        if (s->songs++ == 0) {
            strcpy(s->first_song, e->name);
        }
    }
    return 1;
}

static unsigned char* slurp(const char* path, size_t* n) {
    FILE* f = fopen(path, "rb");
    unsigned char* buf;

    if (f == NULL) {
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    *n = (size_t)ftell(f);
    fseek(f, 0, SEEK_SET);
    buf = malloc(*n + 1u);
    if (buf != NULL && fread(buf, 1, *n, f) != *n) {
        free(buf);
        buf = NULL;
    }
    fclose(f);
    return buf;
}

int main(void) {
    FILE* f = fopen(SEL_SNDH_ZIP, "rb");
    const sel_source* src;
    seen s;
    unsigned char* zip;
    unsigned char* page;
    size_t zip_n = 0, page_n = 0;
    uint32_t off = 99, size = 99;
    char buf[256], scratch[256], path[256];
    sel_tag tags[4];

    memset(&s, 0, sizeof s);
    CHECK(f != NULL);
    if (f != NULL) {
        CHECK(sel_sndh_read_cd(f, collect, &s) == 14);
        CHECK(s.n == 14 && s.songs == 12);
        CHECK(strcmp(s.first_song, "sndh_lf/Micro_Mr/Magic_Fly.sndh") == 0);
        fclose(f);
    }
    f = fopen(SEL_SNDH_PAGE, "rb");
    CHECK(f != NULL);
    if (f != NULL) {
        CHECK(sel_sndh_read_cd(f, collect, &s) == -1); /* an HTML page is not a directory */
        fclose(f);
    }

    zip = slurp(SEL_SNDH_ZIP, &zip_n);
    CHECK(zip != NULL && zip_n == 1439u);
    if (zip != NULL) {
        CHECK(sel_sndh_find_directory(zip, zip_n, &off, &size));
        CHECK(off == 0u && size == 1417u);
        CHECK(!sel_sndh_find_directory(zip, 10, &off, &size));
        free(zip);
    }
    page = slurp(SEL_SNDH_PAGE, &page_n);
    CHECK(page != NULL);
    if (page != NULL) {
        CHECK(sel_sndh_newest_year((const char*)page, page_n) == 2026u);
        CHECK(sel_sndh_newest_year("nothing", 7) == 0u);
        free(page);
    }

    src = sel_source_find("sndh");
    if (src == NULL) {
        sel_source_register_builtin();
        src = sel_source_find("sndh");
    }
    CHECK(src != NULL);
    if (src != NULL) {
        CHECK(src->url != NULL && src->path_in != NULL && src->parse != NULL);
        CHECK(src->url("Tao/One_On_One_(UCM 11).sndh", buf, sizeof buf));
        CHECK(strcmp(buf, "https://sndh.atari.org/sndh/sndh_lf/Tao/One_On_One_%28UCM%2011%29.sndh") == 0);
        CHECK(src->path_in(buf, path, sizeof path));
        CHECK(strcmp(path, "Tao/One_On_One_(UCM 11).sndh") == 0);
        CHECK(!src->path_in("https://example.org/x.sndh", path, sizeof path));
        CHECK(src->parse("505/Relix/EIL99_testsong.sndh", scratch, sizeof scratch, tags));
        CHECK(tags[0].tag == SEL_SRC_FORMAT && strcmp((const char*)tags[0].value, "SNDH") == 0);
        CHECK(tags[1].tag == SEL_SRC_ARTIST && strcmp((const char*)tags[1].value, "505") == 0);
        CHECK(tags[2].tag == SEL_SRC_NAME && strcmp((const char*)tags[2].value, "EIL99_testsong") == 0);
        CHECK(tags[3].tag == SEL_TAG_END);
        CHECK(src->parse("Loose.sndh", scratch, sizeof scratch, tags));
        CHECK(strcmp((const char*)tags[1].value, "- unknown") == 0);
    }
    if (failures == 0) {
        printf("sndh_unit_ok\n");
    }
    return failures == 0 ? 0 : 1;
}

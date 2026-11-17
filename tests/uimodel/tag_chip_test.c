/* I1440n3: the Tag chip rules shared by the Mac, text and MUI UIs (sel_uim_tag_*), and that all three call them. */
#include <selecta/uimodel/pages.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;

#define CHECK(cond)                                        \
    do {                                                   \
        if (!(cond)) {                                     \
            printf("FAIL line %d: %s\n", __LINE__, #cond); \
            failures++;                                    \
        }                                                  \
    } while (0)

static char* slurp(const char* path) {
    FILE* f = fopen(path, "rb");
    char* b;
    long n;
    if (f == NULL) {
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    b = (char*)calloc((size_t)n + 1u, 1u);
    if (b != NULL && fread(b, 1u, (size_t)n, f) != (size_t)n) {
        free(b);
        b = NULL;
    }
    fclose(f);
    return b;
}

int main(void) {
    static sel_collection_page pg;
    char out[64], line[128];
    char* src;
    uint32_t i;

    memcpy(pg.tags, "Chiptune\0Rob Hubbard\0Cover\0", 28); /* an extra NUL ends the list */
    CHECK(sel_uim_tag_count(&pg) == 3u);
    CHECK(strcmp(sel_uim_tag_name(&pg, 1u), "Rob Hubbard") == 0 && sel_uim_tag_name(&pg, 3u) == NULL);
    CHECK(sel_uim_tag_count(NULL) == 0u);

    /* the cycle: All Tags -> Chiptune -> Rob Hubbard -> Cover -> All Tags (empty) */
    sel_uim_tag_next(&pg, "", out, sizeof out);
    CHECK(strcmp(out, "Chiptune") == 0);
    sel_uim_tag_next(&pg, "chiptune", out, sizeof out); /* case-insensitive like the engine filter */
    CHECK(strcmp(out, "Rob Hubbard") == 0);
    sel_uim_tag_next(&pg, "Rob Hubbard", out, sizeof out);
    CHECK(strcmp(out, "Cover") == 0);
    sel_uim_tag_next(&pg, "Cover", out, sizeof out);
    CHECK(out[0] == '\0');
    sel_uim_tag_next(&pg, "Gone", out, sizeof out); /* a tag the page does not list restarts at the first */
    CHECK(strcmp(out, "Chiptune") == 0);
    pg.tags[0] = '\0';
    sel_uim_tag_next(&pg, "", out, sizeof out); /* no tags: stays All Tags */
    CHECK(out[0] == '\0');

    sel_uim_tagchip_label("", out, sizeof out);
    CHECK(strcmp(out, "Tag") == 0);
    sel_uim_tagchip_label("Cover", out, sizeof out);
    CHECK(strcmp(out, "Tag: Cover") == 0);
    sel_uim_tag_line("Cover", line, sizeof line);
    CHECK(strcmp(line, "COLLECTION_TAG \"Cover\"") == 0);
    sel_uim_tag_line("", line, sizeof line);
    CHECK(strcmp(line, "COLLECTION_TAG \"\"") == 0);

    for (i = 0; i < 2; i++) { /* the Mac and text UIs call the shared helpers, none keeps its own copy of the list or the label */
        char* s = slurp(i == 0 ? DESKTOP_SOURCE : TEXT_SOURCE);
        CHECK(s != NULL);
        if (s != NULL) {
            CHECK(strstr(s, "sel_uim_tagchip_label(") != NULL && strstr(s, "sel_uim_tag_line(") != NULL);
            CHECK(strstr(s, i == 0 ? "sel_uim_tag_name(" : "sel_uim_tag_next(") != NULL);
            CHECK(strstr(s, "\"COLLECTION_TAG") == NULL);
            free(s);
        }
    }
    src = slurp(TEXT_SOURCE);
    CHECK(src != NULL && strstr(src, "k->code == 't'") != NULL && strstr(src, "\"Tag filter: %s\"") != NULL);
    free(src);

    if (failures == 0) {
        printf("uim_tag_chip_ok\n");
    }
    return failures != 0;
}

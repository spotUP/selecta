/* I678a: the Your Library row subtitle formatter (src/uimodel/browse.c), one text for every UI. */
#include <selecta/library_item.h>
#include <selecta/uimodel/browse.h>
#include <stdio.h>
#include <string.h>

static int g_fail;

#define CHECK(c)                                        \
    do {                                                \
        if (!(c)) {                                     \
            printf("FAIL line %d: %s\n", __LINE__, #c); \
            g_fail++;                                   \
        }                                               \
    } while (0)

static int is(uint32_t kind, const char* sub, uint32_t songs, const char* want) {
    char buf[96];
    uint32_t len = sel_uim_library_item_subtitle(kind, sub, songs, buf, sizeof buf);

    if (strcmp(buf, want) != 0 || len != strlen(want)) {
        printf("FAIL kind %u: got \"%s\" want \"%s\"\n", (unsigned)kind, buf, want);
        return 0;
    }
    return 1;
}

int main(void) {
    char tiny[8];

    CHECK(is(SEL_LIB_ITEM_LIKED, "", 12, "Playlist \xe2\x80\xa2 12 songs"));
    CHECK(is(SEL_LIB_ITEM_RECENT, "", 1, "Playlist \xe2\x80\xa2 1 song"));
    CHECK(is(SEL_LIB_ITEM_LOCAL, "", 0, "Playlist \xe2\x80\xa2 0 songs"));
    CHECK(is(SEL_LIB_ITEM_PLAYLIST, "You", 9, "You"));
    CHECK(is(SEL_LIB_ITEM_ALBUM, "Ann", 9, "Album \xe2\x80\xa2 Ann"));
    CHECK(is(SEL_LIB_ITEM_ALBUM, "", 9, "Album"));
    CHECK(is(SEL_LIB_ITEM_ARTIST, "", 9, "Artist"));
    CHECK(is(SEL_LIB_ITEM_COMPOSER, "", 2, "Composer \xe2\x80\xa2 2 songs"));
    CHECK(is(SEL_LIB_ITEM_SOURCE, "", 1, "Source \xe2\x80\xa2 1 song"));
    /* A short buffer truncates with a NUL and still reports the full length. */
    CHECK(sel_uim_library_item_subtitle(SEL_LIB_ITEM_ARTIST, "", 0, tiny, 4u) == 6u && strcmp(tiny, "Art") == 0);
    if (g_fail != 0) return 1;
    printf("uim_library_subtitle_ok\n");
    return 0;
}

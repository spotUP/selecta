/* I1306b: the collection page song titles of the shared briefs model (src/uimodel/briefs.c), read from a filled answer buffer. The posted
 * read itself is driven end to end by home-view (text UI); this test covers which row gets which title and when no title is given. */
#include <selecta/uimodel/briefs.h>
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

int main(void) {
    /* answer: "title\0artist\0album\0album id\0" per song; song 1 has no title */
    static const char answer[] = "mk tune\0Ann\0Heroes\0" "7\0" "\0Bob\0\0\0" "test tune\0\0\0\0";
    char buf[sizeof answer];
    sel_collection_page pg;
    sel_uim_briefs b;
    const char* t;
    memcpy(buf, answer, sizeof answer);
    memset(&pg, 0, sizeof pg);
    pg.serial = 5u;
    pg.songs = 3u;
    pg.rows = "a.mod\0" "0\0b.mod\0" "0\0c.mod\0" "0\0";
    memset(&b, 0, sizeof b);
    b.buf = buf;
    b.cap = sizeof buf;
    b.serial = 5u;
    b.done = 3u;
    t = sel_uim_briefs_title(&b, &pg, 0);
    CHECK(t != NULL && strcmp(t, "mk tune") == 0);
    /* a song without a catalogued title falls back to the caller's file name */
    CHECK(sel_uim_briefs_title(&b, &pg, 1) == NULL);
    t = sel_uim_briefs_title(&b, &pg, 2);
    CHECK(t != NULL && strcmp(t, "test tune") == 0);
    /* rows past the answer, an answer for another page, and a read still in flight give no title */
    CHECK(sel_uim_briefs_title(&b, &pg, 3) == NULL);
    b.done = 1u;
    CHECK(sel_uim_briefs_title(&b, &pg, 2) == NULL);
    b.done = 3u;
    pg.serial = 6u;
    CHECK(sel_uim_briefs_title(&b, &pg, 0) == NULL);
    pg.serial = 5u;
    b.inflight = 1u;
    CHECK(sel_uim_briefs_title(&b, &pg, 0) == NULL);
    b.inflight = 0u;
    CHECK(sel_uim_briefs_title(NULL, &pg, 0) == NULL);
    /* the buffer bound per song is the desktop's */
    CHECK(sel_uim_briefs_cap(&pg) == 3u * 448u + 16u && sel_uim_briefs_cap(NULL) == 0u);
    if (g_fail == 0) {
        printf("uim_briefs_ok\n");
    }
    return g_fail != 0;
}

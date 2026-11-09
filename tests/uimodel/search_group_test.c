/* I1019d: sel_uim_search_group (heading kinds 1..4) and sel_uim_search_all_line (SOURCE_SEARCH * "<q>"). */
#include <selecta/uimodel.h>
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

/* I1206: the zero-hit predicate over a built view (heading rows, then state or song rows). */
static void hidden_cases(void) {
    sel_view_row r[16];
    memset(r, 0, sizeof r);
#define HEAD(i, t) do { r[i].kind = SEL_ROW_HEADING; r[i].text = (t); } while (0)
#define ROW(i, t, k) do { r[i].kind = SEL_ROW_HOME; r[i].text = (t); r[i].key = (k); } while (0)
    HEAD(0, "Songs (0)");
    HEAD(1, "Albums (2)");
    ROW(2, "A", "@A");
    ROW(3, "B", "@B");
    HEAD(4, "Artists (0)");
    HEAD(5, "From Done Empty (0)");
    ROW(6, "No results", NULL);
    HEAD(7, "From Pending (0)");
    ROW(8, "Searching", NULL);
    HEAD(9, "From Broken (0)");
    ROW(10, "Could not reach Broken", NULL);
    HEAD(11, "From Unsynced (0)");
    ROW(12, "Not synced yet", NULL);
    HEAD(13, "From Hits (1)");
    ROW(14, "Tune  Artist", "hits:1");
    CHECK(sel_uim_search_section_hidden(r, 15, 0) == 1);  /* Songs (0), next is a heading */
    CHECK(sel_uim_search_section_hidden(r, 15, 1) == 0);  /* Albums (2) */
    CHECK(sel_uim_search_section_hidden(r, 15, 4) == 1);  /* Artists (0) */
    CHECK(sel_uim_search_section_hidden(r, 15, 5) == 1);  /* source done, no hits */
    CHECK(sel_uim_search_section_hidden(r, 15, 7) == 1);  /* source pending */
    CHECK(sel_uim_search_section_hidden(r, 15, 9) == 0);  /* failure stays */
    CHECK(sel_uim_search_section_hidden(r, 15, 11) == 1); /* Not synced yet hides with its Sync row */
    CHECK(sel_uim_search_section_hidden(r, 15, 13) == 0);
    CHECK(sel_uim_search_section_hidden(r, 15, 2) == 0);  /* not a heading */
    CHECK(sel_uim_search_section_hidden(r, 15, 99) == 0);
    CHECK(sel_uim_search_section_hidden(NULL, 0, 0) == 0);
    HEAD(14, "Songs (0)"); /* last row of the view: no row under it */
    CHECK(sel_uim_search_section_hidden(r, 15, 14) == 1);
}

int main(void) {
    char buf[64];
    hidden_cases();
    char tiny[8];
    CHECK(sel_uim_search_group("Songs (12)") == 1u);
    CHECK(sel_uim_search_group("Albums (3)") == 2u);
    CHECK(sel_uim_search_group("Artists (1)") == 3u);
    CHECK(sel_uim_search_group("From Modules.pl (40)") == 4u);
    CHECK(sel_uim_search_group("Searching") == 0u);
    CHECK(sel_uim_search_group("") == 0u);
    CHECK(sel_uim_search_group(NULL) == 0u);

    CHECK(sel_uim_search_all_line("chip", buf, sizeof buf) == strlen("SOURCE_SEARCH * \"chip\""));
    CHECK(strcmp(buf, "SOURCE_SEARCH * \"chip\"") == 0);
    CHECK(sel_uim_search_all_line("a\"b\\c", buf, sizeof buf) > 0u);
    CHECK(strcmp(buf, "SOURCE_SEARCH * \"a\\\"b\\\\c\"") == 0);
    CHECK(sel_uim_search_all_line(NULL, buf, sizeof buf) > 0u);
    CHECK(strcmp(buf, "SOURCE_SEARCH * \"\"") == 0);
    CHECK(sel_uim_search_all_line("x", tiny, sizeof tiny) == 0u);
    CHECK(sel_uim_search_line("chip", buf, sizeof buf) > 0u);
    CHECK(strcmp(buf, "LIBRARY_SEARCH \"chip\"") == 0);
    if (g_fail == 0) {
        printf("uim_search_group_ok\n");
    }
    return g_fail != 0;
}

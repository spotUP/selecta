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

/* I1456/I1454: the shared chip row filter, From helpers and the slide clock. */
static void shared_lift_cases(void) {
    sel_view_row r[8];
    sel_uim_slide sl;
    char nm[32];
    uint32_t t;
    memset(r, 0, sizeof r);
    r[0].kind = SEL_ROW_HEADING; r[0].text = "Songs (1)";
    r[1].kind = SEL_ROW_HOME;    r[1].text = "S";
    r[2].kind = SEL_ROW_HEADING; r[2].text = "Albums (1)";
    r[3].kind = SEL_ROW_HOME;    r[3].text = "A";
    r[4].kind = SEL_ROW_HEADING; r[4].text = "From Alpha (1)";
    r[5].kind = SEL_ROW_HOME;    r[5].text = "X";
    r[6].kind = SEL_ROW_HEADING; r[6].text = "From Beta (1)";
    r[7].kind = SEL_ROW_HOME;    r[7].text = "Y";
    CHECK(sel_uim_search_from_n(r, 8) == 2u);
    sel_uim_search_from_name(r, 8, 1, nm, sizeof nm);
    CHECK(strcmp(nm, "Beta") == 0);
    CHECK(sel_uim_search_row_hidden(r, 8, 3, 0u, 0u) == 0);  /* All shows everything */
    CHECK(sel_uim_search_row_hidden(r, 8, 5, 1u | 2u, 0u) == 0); /* Songs+Albums: From rows are song results */
    CHECK(sel_uim_search_row_hidden(r, 8, 3, 1u, 0u) == 1);  /* Songs only hides Albums */
    CHECK(sel_uim_search_row_hidden(r, 8, 7, 0u, 1u) == 1);  /* source Alpha on hides Beta */
    CHECK(sel_uim_search_row_hidden(r, 8, 5, 0u, 1u) == 0);
    CHECK(sel_uim_search_row_hidden(r, 8, 1, 0u, 1u) == 1);  /* and Songs */
    CHECK(sel_uim_art_frames(0u) == 1u && sel_uim_art_frames(0x105u) == 2u);
    CHECK(sel_uim_art_frame_kind(0x105u, 1u) == 2u);
    CHECK(sel_uim_slide_period_ms("3") == 3000u && sel_uim_slide_period_ms("x") == 6000u && sel_uim_slide_period_ms(NULL) == 6000u);
    memset(&sl, 0, sizeof sl);
    CHECK(!sel_uim_slide_step(&sl, "a", 3u, 1, 100u, 1000u));
    CHECK(!sel_uim_slide_step(&sl, "a", 3u, 0, 1500u, 1000u)); /* paused: no time counts */
    CHECK(sel_uim_slide_step(&sl, "a", 3u, 1, 2600u, 1000u) && sl.idx == 1u);
    CHECK(sel_uim_slide_step(&sl, "a", 3u, 1, 3700u, 1000u) && sl.idx == 2u);
    CHECK(sel_uim_slide_step(&sl, "a", 3u, 1, 4800u, 1000u) && sl.idx == 0u && sl.switches == 3u); /* wraps */
    CHECK(!sel_uim_slide_step(&sl, "b", 3u, 1, 4900u, 1000u) && sl.idx == 0u); /* key change resets */
    t = 4900u + 60000u;
    CHECK(sel_uim_slide_step(&sl, "b", 3u, 1, t, 1000u) && sl.idx == 1u && sl.acc_ms == 0u); /* step capped at 5000 ms */
}

int main(void) {
    char buf[64];
    hidden_cases();
    shared_lift_cases();
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

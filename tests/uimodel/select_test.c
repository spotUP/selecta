/* I632a: the shared selection model (src/uimodel/select.c). Toggle, Shift range over a base set, select-all as one run,
 * clear, no run cap (D9: 100 runs, past the old 32), bind clears on a list or version change, ranges format/parse round trip.
 * Sentinel: s.allocs counts heap growths, so the run arrays provably went through the model's own allocator. */
#include <selecta/uimodel/select.h>
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

static void fmt_is(const sel_uim_selset* s, const char* want, int line) {
    char buf[512];
    (void)sel_uim_selset_format(s, buf, sizeof buf);
    if (strcmp(buf, want) != 0) {
        printf("FAIL line %d: format \"%s\" want \"%s\"\n", line, buf, want);
        g_fail++;
    }
}
#define FMT(s, w) fmt_is(s, w, __LINE__)

int main(void) {
    sel_uim_selset s;
    sel_uim_list_id a = {SEL_UIM_LIST_VIEW, 0, 1}, b = {SEL_UIM_LIST_VIEW, 0, 2}, q = {SEL_UIM_LIST_QUEUE, 0, 2};
    char buf[16], big[8192];
    uint32_t i;

    sel_uim_selset_init(&s);
    CHECK(sel_uim_selset_bind(&s, a) == 1); /* first bind: the list changed from "none" */
    CHECK(sel_uim_selset_bind(&s, a) == 0);

    /* click, Cmd-click x2, Cmd-click off */
    CHECK(sel_uim_selset_select_one(&s, 0));
    CHECK(sel_uim_selset_toggle(&s, 2));
    CHECK(sel_uim_selset_toggle(&s, 5));
    FMT(&s, "0,2,5");
    CHECK(sel_uim_selset_count(&s) == 3u);
    CHECK(sel_uim_selset_toggle(&s, 2));
    FMT(&s, "0,5");
    CHECK(!sel_uim_selset_contains(&s, 2) && sel_uim_selset_contains(&s, 5));
    CHECK(sel_uim_selset_toggle(&s, 5) && sel_uim_selset_toggle(&s, 5)); /* anchor back on 5, set still {0,5} */
    FMT(&s, "0,5");

    /* Shift range over the base set {0,5}: anchor 5, extend to 9, then back to 7, then up past it */
    CHECK(sel_uim_selset_extend(&s, 9));
    FMT(&s, "0,5-9");
    CHECK(sel_uim_selset_extend(&s, 7));
    FMT(&s, "0,5-7"); /* the live range shrinks, the base stays */
    CHECK(sel_uim_selset_extend(&s, 2));
    FMT(&s, "0,2-5");
    CHECK(sel_uim_selset_extend(&s, 1)); /* 1-5 touches base 0: one merged run */
    FMT(&s, "0-5");
    CHECK(sel_uim_selset_run_count(&s) == 1u);

    /* a plain click resets the base */
    CHECK(sel_uim_selset_select_one(&s, 3));
    CHECK(sel_uim_selset_extend(&s, 4));
    FMT(&s, "3-4");

    /* select all is one run, even for 30000 rows */
    CHECK(sel_uim_selset_select_all(&s, 30000u));
    CHECK(sel_uim_selset_run_count(&s) == 1u && sel_uim_selset_count(&s) == 30000u);
    FMT(&s, "0-29999");
    CHECK(sel_uim_selset_toggle(&s, 100)); /* a hole splits the run */
    FMT(&s, "0-99,101-29999");

    /* clear */
    sel_uim_selset_clear(&s);
    CHECK(sel_uim_selset_count(&s) == 0u && !sel_uim_selset_contains(&s, 0));
    CHECK(sel_uim_selset_format(&s, buf, sizeof buf) == 0u);

    /* D9: no run cap. 100 separate runs (every second row), past the old 32 */
    CHECK(sel_uim_selset_select_one(&s, 0));
    for (i = 1; i < 100u; i++) CHECK(sel_uim_selset_toggle(&s, i * 2u));
    CHECK(sel_uim_selset_run_count(&s) == 100u && sel_uim_selset_count(&s) == 100u);
    CHECK(s.allocs > 1u); /* the arrays doubled: the model's own heap path ran */

    /* format / parse round trip, with the runs past 512 bytes only in the 8 KB buffer */
    CHECK(sel_uim_selset_format(&s, big, sizeof big) > 300u);
    {
        sel_uim_selset t;
        char again[8192];
        sel_uim_selset_init(&t);
        CHECK(sel_uim_selset_parse(&t, big));
        CHECK(sel_uim_selset_run_count(&t) == 100u && sel_uim_selset_count(&t) == 100u);
        CHECK(sel_uim_selset_format(&t, again, sizeof again) == strlen(big) && strcmp(again, big) == 0);
        CHECK(!sel_uim_selset_parse(&t, "5-3"));     /* backwards */
        CHECK(!sel_uim_selset_parse(&t, "4,2"));     /* not ascending */
        CHECK(!sel_uim_selset_parse(&t, "1,x"));     /* syntax */
        CHECK(!sel_uim_selset_parse(&t, ""));        /* empty */
        CHECK(sel_uim_selset_count(&t) == 100u);     /* failed parses leave the set alone */
        CHECK(sel_uim_selset_parse(&t, "0-11,40,52-60"));
        CHECK(sel_uim_selset_count(&t) == 22u);
        FMT(&t, "0-11,40,52-60");
        sel_uim_selset_free(&t);
    }
    CHECK(sel_uim_selset_format(&s, buf, sizeof buf) == 0u && buf[0] == '\0'); /* does not fit: empty, not truncated */

    /* bind: the same list keeps the set, a new version or another list kind clears it */
    CHECK(sel_uim_selset_bind(&s, a) == 0 && sel_uim_selset_count(&s) == 100u);
    CHECK(sel_uim_selset_bind(&s, b) == 1 && sel_uim_selset_count(&s) == 0u && s.run.v == NULL);
    CHECK(sel_uim_selset_select_one(&s, 7));
    CHECK(sel_uim_selset_bind(&s, q) == 1 && sel_uim_selset_count(&s) == 0u);

    sel_uim_selset_free(&s);
    if (g_fail == 0) printf("uim_select_ok\n");
    return g_fail == 0 ? 0 : 1;
}

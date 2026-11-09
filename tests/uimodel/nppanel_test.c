/* I1222f: the Now Playing panel lines (src/uimodel/nppanel.c): a song with a context, 6 pairs, a lyric and 3 up next; the release as the
 * context fallback; an empty queue; no song. The desktop draws from these lines, so the numbers here are what its panel shows. */
#include <selecta/uimodel/nppanel.h>
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

static uint32_t count_kind(const sel_uim_np_line* l, uint32_t n, sel_uim_np_kind k) {
    uint32_t i, c = 0;
    for (i = 0; i < n; i++) {
        c += l[i].kind == k;
    }
    return c;
}

/* "label\0value\0" pairs ended by an empty label: 8 rows, only 6 are shown */
static const char k_panel[] = "Liked\0yes\0Rating\0" "4\0Composer\0Rob Hubbard\0Year\0" "1986\0Format\0SID\0Length\0" "3:02\0Extra\0x\0Extra2\0y\0";
static const char k_queue_text[] = "/a/one.sid\0/b/two.mod\0/c/three.xm\0/d/four.s3m\0/e/five.it";

int main(void) {
    static sel_uim m;
    static sel_state st;
    sel_uim_np_line l[SEL_UIM_NP_LINES_MAX];
    sel_queue_entry q[5];
    uint32_t n, i;
    const sel_uim_np_line* f;

    memset(&m, 0, sizeof m);
    memset(&st, 0, sizeof st);
    st.ctx_name = "Road Trip";
    snprintf(m.frame.title, sizeof m.frame.title, "Commando");
    snprintf(m.frame.artist, sizeof m.frame.artist, "Rob Hubbard");
    st.panel = k_panel;
    st.lyric_lines = 3u;
    st.lyric_prev = "before";
    st.lyric_current = "now";
    st.lyric_next = NULL;
    st.song.path = "/a/one.sid";
    for (i = 0; i < 5u; i++) {
        q[i].id = i + 1u;
        q[i].subsong_plus1 = 0u;
        q[i].flags = 0u;
    }
    q[0].path_offset = 0u;
    q[1].path_offset = 11u;
    q[2].path_offset = 22u;
    q[3].path_offset = 34u;
    q[4].path_offset = 46u;
    st.queue = q;
    st.queue_text = k_queue_text;
    st.queue_count = 5u;
    st.queue_playing = 1u;

    n = sel_uim_np_lines(&m, &st, l, SEL_UIM_NP_LINES_MAX);
    CHECK(n <= SEL_UIM_NP_LINES_MAX);
    CHECK(l[0].kind == SEL_UIM_NP_CONTEXT && strcmp(l[0].val, "Road Trip") == 0);
    CHECK(l[1].kind == SEL_UIM_NP_TITLE && strcmp(l[1].val, "Commando") == 0);
    CHECK(l[2].kind == SEL_UIM_NP_ARTIST && strcmp(l[2].val, "Rob Hubbard") == 0);
    f = sel_uim_np_find(l, n, SEL_UIM_NP_LIKE, 0u);
    CHECK(f != NULL && f->liked == 1 && f->rating == 4);
    /* 8 rows in the panel, 6 shown, in order */
    CHECK(count_kind(l, n, SEL_UIM_NP_PAIR) == SEL_UIM_NP_PAIRS_MAX);
    f = sel_uim_np_find(l, n, SEL_UIM_NP_PAIR, 0u);
    CHECK(f != NULL && strcmp(f->key, "Liked") == 0 && strcmp(f->val, "yes") == 0);
    f = sel_uim_np_find(l, n, SEL_UIM_NP_PAIR, (uint32_t)(f - l) + 2u);
    CHECK(f != NULL && strcmp(f->key, "Composer") == 0);
    /* three lyric lines in order; a missing line is empty */
    CHECK(count_kind(l, n, SEL_UIM_NP_LYRIC) == 3u);
    f = sel_uim_np_find(l, n, SEL_UIM_NP_LYRIC, 0u);
    CHECK(f != NULL && strcmp(f[0].key, "previous") == 0 && strcmp(f[0].val, "before") == 0 && strcmp(f[1].val, "now") == 0 && f[2].val[0] == '\0');
    CHECK(sel_uim_np_find(l, n, SEL_UIM_NP_MESSAGE, 0u) == NULL);
    /* up next: the header, then the 3 entries after the playing one by file name */
    CHECK(count_kind(l, n, SEL_UIM_NP_UPNEXT_HDR) == 1u && count_kind(l, n, SEL_UIM_NP_UPNEXT) == SEL_UIM_NP_UPNEXT_MAX);
    f = sel_uim_np_find(l, n, SEL_UIM_NP_UPNEXT, 0u);
    CHECK(f != NULL && strcmp(f[0].val, "two.mod") == 0 && strcmp(f[1].val, "three.xm") == 0 && strcmp(f[2].val, "four.s3m") == 0);

    /* playing the third entry leaves two up next */
    st.queue_playing = 3u;
    n = sel_uim_np_lines(&m, &st, l, SEL_UIM_NP_LINES_MAX);
    CHECK(count_kind(l, n, SEL_UIM_NP_UPNEXT) == 2u);
    /* playing the last entry, or an empty queue: the header only */
    st.queue_playing = 5u;
    n = sel_uim_np_lines(&m, &st, l, SEL_UIM_NP_LINES_MAX);
    CHECK(count_kind(l, n, SEL_UIM_NP_UPNEXT_HDR) == 1u && count_kind(l, n, SEL_UIM_NP_UPNEXT) == 0u);
    st.queue_count = 0u;
    st.queue_playing = 0u;
    n = sel_uim_np_lines(&m, &st, l, SEL_UIM_NP_LINES_MAX);
    CHECK(count_kind(l, n, SEL_UIM_NP_UPNEXT_HDR) == 1u && count_kind(l, n, SEL_UIM_NP_UPNEXT) == 0u);

    /* no context name: the Release pair heads the panel; none at all: "Now Playing"; a tracker message replaces the lyrics */
    st.ctx_name = "";
    st.panel = "Release\0Some Album\0\0";
    st.lyric_lines = 0u;
    st.lyric_text = "line one\nline two";
    n = sel_uim_np_lines(&m, &st, l, SEL_UIM_NP_LINES_MAX);
    CHECK(strcmp(l[0].val, "Some Album") == 0);
    f = sel_uim_np_find(l, n, SEL_UIM_NP_MESSAGE, 0u);
    CHECK(f != NULL && f->ref == st.lyric_text && count_kind(l, n, SEL_UIM_NP_LYRIC) == 0u);

    /* no song: the heading, an empty title, no artist, the like row (unset), no pairs */
    memset(&m, 0, sizeof m);
    st.panel = NULL;
    st.lyric_text = NULL;
    n = sel_uim_np_lines(&m, &st, l, SEL_UIM_NP_LINES_MAX);
    CHECK(strcmp(l[0].val, "Now Playing") == 0 && l[1].kind == SEL_UIM_NP_TITLE && l[1].val[0] == '\0');
    CHECK(count_kind(l, n, SEL_UIM_NP_ARTIST) == 0u && count_kind(l, n, SEL_UIM_NP_PAIR) == 0u);
    f = sel_uim_np_find(l, n, SEL_UIM_NP_LIKE, 0u);
    CHECK(f != NULL && f->liked == 0 && f->rating == 0);

    /* max caps the lines written */
    n = sel_uim_np_lines(&m, &st, l, 2u);
    CHECK(n == 2u);

    printf(g_fail == 0 ? "uim_nppanel_ok\n" : "uim_nppanel_fail %d\n", g_fail);
    return g_fail != 0;
}

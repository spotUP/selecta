/* I548: a stopped transport reads 00:00 and an empty progress bar even though STOP keeps the slot loaded with its frozen
 * position (the rig 3 screenshot showed 00:30 while stopped). Playing and paused keep the real position. */
#include <selecta/uimodel.h>

#include <stdio.h>
#include <string.h>

static int fails;
static sel_state st;
static sel_uim uim;

#define CHECK(c) \
    do { \
        if (!(c)) { \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); \
            fails++; \
        } \
    } while (0)

static void run(sel_play_state p) {
    memset(&st, 0, sizeof st);
    memset(&uim, 0, sizeof uim);
    sel_uim_init(&uim);
    st.play = p;
    st.song.path = "/music/a.mod";
    st.song.position_ms = 30000u;
    st.song.duration_ms = 120000u;
    sel_uim_update(&uim, &st);
}

int main(void) {
    run(SEL_STOPPED);
    CHECK(strcmp(uim.frame.clock, "00:00") == 0);
    CHECK(uim.frame.progress_q16 == 0u);
    run(SEL_PLAYING);
    CHECK(strcmp(uim.frame.clock, "00:30") == 0);
    run(SEL_PAUSED);
    CHECK(strcmp(uim.frame.clock, "00:30") == 0);
    printf(fails == 0 ? "stopped_clock_ok\n" : "stopped_clock_failed\n");
    return fails == 0 ? 0 : 1;
}

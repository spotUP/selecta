/* I219g: the KPlayer timing walker that drives the native seek. Synthetic M.K. modules, one effect per case,
 * expected rows counted by hand from KPlayer v33's row rules (see plugins/replayers/kplayer/modwalk.c). At the
 * start values (speed 6, tempo 125) one row is 6 * 2.5 s / 125 = 120 ms. */
#include "modwalk.h"

#include <stdio.h>
#include <string.h>

enum { PATTERNS = 4, MOD_SIZE = 1084 + PATTERNS * 1024 };

static uint8_t g_mod[MOD_SIZE];
static int g_fail;

static void mod_reset(uint32_t length) {
    uint32_t i;
    memset(g_mod, 0, sizeof g_mod);
    g_mod[950] = (uint8_t)length;
    for (i = 0; i < length; i++) {
        g_mod[952 + i] = (uint8_t)i; /* order i plays pattern i */
    }
    memcpy(g_mod + 1080, "M.K.", 4);
}

static void fx(uint32_t pattern, uint32_t row, uint32_t channel, uint32_t cmd, uint32_t param) {
    uint8_t* cell = g_mod + 1084 + pattern * 1024 + row * 16 + channel * 4;
    cell[2] = (uint8_t)cmd;
    cell[3] = (uint8_t)param;
}

static void expect(const char* name, uint32_t ms, int found, uint32_t songpos, uint32_t row, uint32_t speed,
                   uint32_t tempo, uint32_t at_ms) {
    kp_walk_pos p;
    int got;
    memset(&p, 0, sizeof p);
    got = kp_walk_find(g_mod, sizeof g_mod, ms, &p);
    if (got != found || (found && (p.songpos != songpos || p.row != row || p.speed != speed || p.tempo != tempo ||
                                   p.at_ms != at_ms))) {
        printf("FAIL %s ms=%u: got %d pos=%u row=%u speed=%u tempo=%u at=%u\n", name, (unsigned)ms, got,
               (unsigned)p.songpos, (unsigned)p.row, (unsigned)p.speed, (unsigned)p.tempo, (unsigned)p.at_ms);
        g_fail = 1;
    }
}

static void expect_length(const char* name, uint32_t want_ms, int want_found) {
    uint32_t got_ms = 0;
    int got = kp_walk_length(g_mod, sizeof g_mod, &got_ms);
    if (got != want_found || (want_found && got_ms != want_ms)) {
        printf("FAIL %s: got %d length=%u want %d length=%u\n", name, got, (unsigned)got_ms, want_found, (unsigned)want_ms);
        g_fail = 1;
    }
}

int main(void) {
    kp_walk_pos p;

    mod_reset(2);
    expect("start", 0, 1, 0, 0, 6, 125, 0);
    expect("mid row", 1000, 1, 0, 8, 6, 125, 960);
    expect("second order", 7680, 1, 1, 0, 6, 125, 7680);
    expect("last row", 15359, 1, 1, 63, 6, 125, 15240);
    expect("past the wrap", 15360, 0, 0, 0, 0, 0, 0);

    /* F03 on row 0 makes row 0 itself 60 ms; the walker reports the speed in force before the row (6). */
    mod_reset(2);
    fx(0, 0, 0, 0x0f, 0x03);
    expect("speed row", 30, 1, 0, 0, 6, 125, 0);
    expect("after speed", 60, 1, 0, 1, 3, 125, 60);
    /* F00 is speed 31 in KPlayer. */
    mod_reset(2);
    fx(0, 0, 0, 0x0f, 0x00);
    expect("F00", 620, 1, 0, 1, 31, 125, 620);
    /* FFA is tempo 250: 60 ms rows. */
    mod_reset(2);
    fx(0, 0, 2, 0x0f, 0xfa);
    expect("tempo", 125, 1, 0, 2, 6, 250, 120);

    /* D16 (BCD) on order 0 row 0: the next row is order 1 row 16. */
    mod_reset(2);
    fx(0, 0, 1, 0x0d, 0x16);
    expect("pattern break", 130, 1, 1, 16, 6, 125, 120);
    /* B01 on order 0 row 3 jumps to order 1 row 0. */
    mod_reset(2);
    fx(0, 3, 3, 0x0b, 0x01);
    expect("position jump", 480, 1, 1, 0, 6, 125, 480);

    /* B to its own order without a D is the song end: the row still plays, the next one does not. */
    mod_reset(2);
    fx(1, 10, 0, 0x0b, 0x01);
    expect("inside the end row", 7680 + 10 * 120 + 50, 1, 1, 10, 6, 125, 7680 + 1200);
    expect("past the end row", 7680 + 11 * 120, 0, 0, 0, 0, 0, 0);
    /* B from the last order back to 0 is the song end too. */
    mod_reset(2);
    fx(1, 5, 0, 0x0b, 0x00);
    expect("loop back from the last order", 7680 + 6 * 120, 0, 0, 0, 0, 0, 0);

    /* E60 on row 0, E62 on row 1: rows 0 1 0 1 0 1 2, so 600 ms is row 1 (third pass) and 720 ms row 2. */
    mod_reset(2);
    fx(0, 0, 0, 0x0e, 0x60);
    fx(0, 1, 0, 0x0e, 0x62);
    expect("loop third pass", 600, 1, 0, 1, 6, 125, 600);
    expect("after the loop", 720, 1, 0, 2, 6, 125, 720);

    /* EE2 on row 0: row 0 lasts three rows (360 ms). */
    mod_reset(2);
    fx(0, 0, 0, 0x0e, 0xe2);
    expect("pattern delay", 300, 1, 0, 0, 6, 125, 0);
    expect("after the delay", 360, 1, 0, 1, 6, 125, 360);

    /* Malformed: too short, and an order pointing past the data. */
    if (kp_walk_find(g_mod, 1000, 0, &p) != 0) {
        printf("FAIL short module accepted\n");
        g_fail = 1;
    }
    mod_reset(2);
    g_mod[952] = 9;
    expect("pattern past the data", 0, 0, 0, 0, 0, 0, 0);

    /* I605: the song length, where k_songover would count. Two plain orders: 128 rows of 120 ms. */
    mod_reset(2);
    expect_length("length two orders", 15360, 1);
    /* B01 on the last order's row 10 is the song end after that row. */
    mod_reset(2);
    fx(1, 10, 0, 0x0b, 0x01);
    expect_length("length position jump end", 7680 + 11 * 120, 1);
    /* F03 on row 0 sets speed 3 for the whole song: 128 rows of 60 ms. */
    mod_reset(2);
    fx(0, 0, 0, 0x0f, 0x03);
    expect_length("length with speed change", 7680, 1);
    mod_reset(2);
    g_mod[952] = 9;
    expect_length("length of a malformed module", 0, 0);

    if (g_fail) {
        return 1;
    }
    printf("kplayer_modwalk_ok\n");
    return 0;
}

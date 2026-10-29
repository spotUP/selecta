/* I665 spike: rounded-shape geometry and caption colour ramp of the flat button (map/mui_aa.c). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mui_aa.h"
#include "mui_theme.h"
#include <selecta/uimodel/theme.h>
#include <selecta/uimodel/tokens.h>

static int failures;
#define CHECK(cond, ...)                                \
    do {                                                \
        if (!(cond)) {                                  \
            failures++;                                 \
            printf("FAIL %s:%d: ", __FILE__, __LINE__); \
            printf(__VA_ARGS__);                        \
            printf("\n");                               \
        }                                               \
    } while (0)

static void test_round(void) {
    int y, prev = 1 << 20;
    int w = 80, h = 24, r = 8;
    CHECK(mui_aa_round_inset(w, h, 0, 0) == 0, "radius 0 is a plain rectangle");
    CHECK(mui_aa_round_inset(w, h, r, 0) >= 3, "top row cut back by the corner: %d", mui_aa_round_inset(w, h, r, 0));
    CHECK(mui_aa_round_inset(w, h, r, h / 2) == 0, "middle row full width");
    for (y = 0; y < h; y++) {
        int in = mui_aa_round_inset(w, h, r, y);
        CHECK(in == mui_aa_round_inset(w, h, r, h - 1 - y), "row %d mirrors row %d", y, h - 1 - y);
        if (y < h / 2) {
            CHECK(in <= prev, "inset shrinks towards the middle at row %d", y);
            prev = in;
        }
        CHECK(in < w / 2, "row %d keeps a span", y);
    }
    CHECK(mui_aa_round_inset(10, 4, 99, 0) <= 3, "radius clamped to half the height");
}

static void test_ramp(void) {
    uint32_t fg = 0xF0F0F0u, bg = 0x303030u;
    int i;
    CHECK(mui_aa_ramp_index(0, 8) == 0 && mui_aa_ramp_index(255, 8) == 7, "ends map to the ramp ends");
    CHECK(mui_aa_ramp_index(128, 8) == 4, "half coverage mid ramp: %d", mui_aa_ramp_index(128, 8));
    CHECK(mui_aa_ramp_rgb(fg, bg, 8, 0) == bg && mui_aa_ramp_rgb(fg, bg, 8, 7) == fg, "ramp ends are bg and fg");
    for (i = 1; i < 8; i++) {
        CHECK(mui_aa_ramp_rgb(fg, bg, 8, i) > mui_aa_ramp_rgb(fg, bg, 8, i - 1), "ramp rises at %d", i);
    }
    CHECK(mui_aa_pressed_rgb(0xE0E0E0u) < 0xE0E0E0u, "light face presses darker");
    CHECK(mui_aa_pressed_rgb(0x303030u) > 0x303030u, "dark face presses lighter");
    {
        /* I667a: the AGA ramp is the four held token pens, lowest coverage first; the pixmap coverage picks the slot. */
        txt_theme th;
        uint32_t out[MUI_AA_RAMP_MAX];
        txt_theme_default(&th);
        CHECK(mui_aa_ramp_pens(MUI_AA_RAMP_AGA, th.token, fg, bg, out) == 4, "AGA ramp has 4 pens");
        CHECK(out[0] == bg && out[1] == th.token[SEL_GUI_TOKEN_TEXT_DISABLED] && out[2] == th.token[SEL_GUI_TOKEN_TEXT_MUTED] &&
                  out[3] == th.token[SEL_GUI_TOKEN_TEXT],
              "AGA ramp order bg, disabled, muted, text");
        CHECK(mui_aa_ramp_slot(MUI_AA_RAMP_AGA, 0) == 0 && mui_aa_ramp_slot(MUI_AA_RAMP_AGA, 63) == 0 &&
                  mui_aa_ramp_slot(MUI_AA_RAMP_AGA, 64) == 1 && mui_aa_ramp_slot(MUI_AA_RAMP_AGA, 128) == 2 &&
                  mui_aa_ramp_slot(MUI_AA_RAMP_AGA, 192) == 3 && mui_aa_ramp_slot(MUI_AA_RAMP_AGA, 255) == 3,
              "AGA coverage bands 0-63 64-127 128-191 192-255");
        CHECK(mui_aa_ramp_pens(MUI_AA_RAMP_RTG, th.token, fg, bg, out) == 8, "RTG ramp has 8 steps");
        CHECK(mui_aa_ramp_pens(MUI_AA_RAMP_TC, th.token, fg, bg, out) == 0, "truecolour needs no ramp pens");
    }
}

/* The theme tokens feed the button: the face is SURFACE_RAISED and a pressed face differs from it. */
static void test_tokens(void) {
    txt_theme th;
    mui_theme_look look;
    txt_theme_default(&th);
    CHECK(mui_theme_look_for(MUI_TH_BUTTON, th.token, &look), "button look");
    CHECK(look.bg_rgb == th.token[SEL_GUI_TOKEN_SURFACE_RAISED], "face is the raised token");
    CHECK(mui_aa_pressed_rgb(look.bg_rgb) != look.bg_rgb, "pressed differs from raised");
}

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
    b = (char*)calloc((size_t)n + 1, 1);
    if (b != NULL && fread(b, 1, (size_t)n, f) != (size_t)n) {
        b[0] = 0;
    }
    fclose(f);
    return b;
}

/* I667a: states are named tokens, the font size follows the screen rows, and the Amiga classes really call them. */
static void test_state(void) {
    txt_theme th;
    uint32_t face = 0x2A2A32u;
    unsigned long before;
    char *cls, *draw;
    mui_aa_metrics_t m;
    txt_theme_default(&th);
    before = mui_aa_state_calls();
    CHECK(mui_aa_state_rgb(th.token, face, MUI_AA_NORMAL) == face, "normal is the face");
    CHECK(mui_aa_state_rgb(th.token, face, MUI_AA_HOVER) == th.token[SEL_GUI_TOKEN_SURFACE_HOVER], "hover token");
    CHECK(mui_aa_state_rgb(th.token, face, MUI_AA_PRESSED) == th.token[SEL_GUI_TOKEN_SURFACE_PRESSED], "pressed token");
    CHECK(mui_aa_state_rgb(th.token, face, MUI_AA_SELECTED) == th.token[SEL_GUI_TOKEN_SURFACE_PRESSED], "selected token");
    CHECK(mui_aa_state_rgb(th.token, face, MUI_AA_DISABLED) == face, "disabled keeps the face");
    CHECK(mui_aa_text_rgb(th.token, 0xC8C8D0u, MUI_AA_DISABLED) == th.token[SEL_GUI_TOKEN_TEXT_DISABLED], "disabled text token");
    CHECK(mui_aa_text_rgb(th.token, 0xC8C8D0u, MUI_AA_HOVER) == 0xC8C8D0u, "other states keep the text");
    CHECK(mui_aa_state_calls() > before, "sentinel: state calls counted");
    CHECK(mui_aa_font_px(600, 800) == 13 && mui_aa_font_px(800, 1280) == 15 && mui_aa_font_px(1024, 1280) == 17 &&
              mui_aa_font_px(256, 640) == 0 && mui_aa_font_px(256, 320) == 0,
          "font px by screen rows");
    mui_aa_metrics(13, &m);
    CHECK(m.height == 36 && m.pad_x == 10 && m.pad_y == 6 && m.icon_px == 24 && m.radius == 6, "M metrics");
    mui_aa_metrics(0, &m);
    CHECK(m.height == 14 && m.pad_x == 6 && m.icon_px == 16 && m.radius == 4, "S metrics");
    cls = slurp(AABTN_SOURCE);
    draw = slurp(AADRAW_SOURCE);
    CHECK(cls != NULL && draw != NULL, "class sources readable");
    if (cls != NULL && draw != NULL) {
        CHECK(strstr(cls, "mui_aa_button_rgb(") != NULL && strstr(cls, "aa_text(") != NULL && strstr(cls, "aa_round_fill(") != NULL,
              "class draws through mui_aa_button_rgb and aa_*");
        CHECK(strstr(cls, "ObtainBestPenA") == NULL && strstr(cls, "TT_") == NULL, "class holds no pen or ttengine code of its own");
        CHECK(strstr(draw, "mui_aa_ramp_pens(") != NULL && strstr(draw, "mui_aa_ramp_slot(") != NULL &&
                  strstr(draw, "mui_theme_readable(") != NULL && strstr(draw, "ReleasePen") != NULL,
              "aa_draw uses the ramp, the contrast gate and releases pens");
    }
    free(cls);
    free(draw);
}

int main(int argc, char** argv) {
    const char* t = argc > 1 ? argv[1] : "";
    if (!strcmp(t, "round")) {
        test_round();
    } else if (!strcmp(t, "ramp")) {
        test_ramp();
    } else if (!strcmp(t, "state")) {
        test_state();
    } else if (!strcmp(t, "tokens")) {
        test_tokens();
    } else {
        printf("usage: round|ramp|state|tokens\n");
        return 2;
    }
    if (failures) {
        return 1;
    }
    printf("ok %s\n", t);
    return 0;
}

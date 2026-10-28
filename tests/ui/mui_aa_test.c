/* I665 spike: rounded-shape geometry and caption colour ramp of the flat button (map/mui_aa.c). */
#include <stdio.h>
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

int main(int argc, char** argv) {
    const char* t = argc > 1 ? argv[1] : "";
    if (!strcmp(t, "round")) {
        test_round();
    } else if (!strcmp(t, "ramp")) {
        test_ramp();
    } else if (!strcmp(t, "tokens")) {
        test_tokens();
    } else {
        printf("usage: round|ramp|tokens\n");
        return 2;
    }
    if (failures) {
        return 1;
    }
    printf("ok %s\n", t);
    return 0;
}

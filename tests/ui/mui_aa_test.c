/* I665 spike: rounded-shape geometry and caption colour ramp of the flat button (map/mui_aa.c). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mui_aa.h"
#include "mui_pens.h"
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

/* I667b-2 mui-aa-check: the class really implements the toggle role (checkbox) through the shared helpers: toggle input mode and a
 * MUIA_Selected notification, the box from the metrics (check_px), the DIVIDER outline token and the check icon drawn with aabtn_icon. */
static void test_check(void) {
    char* cls = slurp(AABTN_SOURCE);
    CHECK(cls != NULL, "class source readable");
    if (cls == NULL) {
        return;
    }
    CHECK(strstr(cls, "MUIV_InputMode_Toggle") != NULL && strstr(cls, "MUIA_Selected") != NULL, "toggle input mode or selected notify missing");
    CHECK(strstr(cls, "MUI_AA_ROLE_TOGGLE") != NULL && strstr(cls, "check_px") != NULL && strstr(cls, "mui_aa_check_plan(") != NULL, "toggle box not drawn from the shared check plan");
    CHECK(strstr(cls, "MUI_AA_SELECTED") != NULL && strstr(cls, "aabtn_icon(") != NULL, "checked state does not draw the check icon");
    CHECK(strstr(cls, "ObtainBestPenA") == NULL && strstr(cls, "TT_") == NULL, "class holds no pen or ttengine code of its own");
    free(cls);
}

/* I667d mui-text-fit: the ellipsis cut on a fixed advance table (every byte 10 px wide, so "..." is 30 px). */
static int adv10(void* ctx, const char* s, int len) {
    (void)ctx;
    (void)s;
    return len * 10;
}
static void test_text_fit(void) {
    char out[64];
    mui_aa_text_fit("abcdef", 60, adv10, NULL, out, (int)sizeof out);
    CHECK(strcmp(out, "abcdef") == 0, "text that fits stays whole: '%s'", out);
    mui_aa_text_fit("abcdefgh", 60, adv10, NULL, out, (int)sizeof out);
    CHECK(strcmp(out, "abc...") == 0, "cut at 60 px keeps 3 chars and dots: '%s'", out);
    mui_aa_text_fit("abcdefgh", 65, adv10, NULL, out, (int)sizeof out);
    CHECK(strcmp(out, "abc...") == 0, "spare pixels do not add half a char: '%s'", out);
    mui_aa_text_fit("abcdefgh", 30, adv10, NULL, out, (int)sizeof out);
    CHECK(strcmp(out, "...") == 0, "only the dots fit: '%s'", out);
    mui_aa_text_fit("abcdefgh", 29, adv10, NULL, out, (int)sizeof out);
    CHECK(out[0] == '\0', "dots do not fit: nothing drawn, got '%s'", out);
    mui_aa_text_fit("", 5, adv10, NULL, out, (int)sizeof out);
    CHECK(out[0] == '\0', "empty text stays empty");
    mui_aa_text_fit("abcdefgh", 1000, adv10, NULL, out, 5);
    CHECK(strlen(out) < 5, "output capped by cap: '%s'", out);
}

/* I667d mui-text-tone: tone -> token -> held pen slot, title = font + 4, label tones, alignment. */
static void test_text_tone(void) {
    CHECK(mui_aa_tone_token(MUI_AA_TONE_NORMAL) == SEL_GUI_TOKEN_TEXT, "normal tone is TEXT");
    CHECK(mui_aa_tone_token(MUI_AA_TONE_MUTED) == SEL_GUI_TOKEN_TEXT_MUTED, "muted tone is TEXT_MUTED");
    CHECK(mui_aa_tone_token(MUI_AA_TONE_TITLE) == SEL_GUI_TOKEN_TEXT, "title tone is TEXT");
    CHECK(mui_pens_aga_slot(mui_aa_tone_token(MUI_AA_TONE_NORMAL)) == MUI_PEN_TEXT, "normal tone holds the TEXT pen");
    CHECK(mui_pens_aga_slot(mui_aa_tone_token(MUI_AA_TONE_MUTED)) == MUI_PEN_TEXT_MUTED, "muted tone holds the TEXT_MUTED pen");
    CHECK(mui_aa_tone_font_px(MUI_AA_TONE_TITLE, 15) == 19, "title is font + 4: %d", mui_aa_tone_font_px(MUI_AA_TONE_TITLE, 15));
    CHECK(mui_aa_tone_font_px(MUI_AA_TONE_NORMAL, 15) == 15 && mui_aa_tone_font_px(MUI_AA_TONE_MUTED, 15) == 15, "other tones keep the font");
    CHECK(mui_aa_tone_font_px(MUI_AA_TONE_TITLE, 0) == 0, "bitmap tier stays on its font");
    CHECK(mui_pens_label_tone(MUI_LBL_TITLE) == MUI_AA_TONE_TITLE, "title label");
    CHECK(mui_pens_label_slot(MUI_LBL_ALBUM) != MUI_PEN_TEXT_MUTED || mui_pens_label_tone(MUI_LBL_ALBUM) == MUI_AA_TONE_MUTED, "muted label slot gives muted tone");
    CHECK(mui_pens_label_tone(MUI_LBL_SECTION) == (mui_pens_label_slot(MUI_LBL_SECTION) == MUI_PEN_TEXT_MUTED ? MUI_AA_TONE_MUTED : MUI_AA_TONE_NORMAL), "section label tone follows its pen slot");
    CHECK(mui_aa_align_x(MUI_AA_ALIGN_LEFT, 100, 40) == 0 && mui_aa_align_x(MUI_AA_ALIGN_CENTRE, 100, 40) == 30 && mui_aa_align_x(MUI_AA_ALIGN_RIGHT, 100, 40) == 60,
          "align offsets");
}

static int count_of(const char* hay, const char* needle) {
    int n = 0;
    const char* p = hay;
    while ((p = strstr(p, needle)) != NULL) {
        n++;
        p += strlen(needle);
    }
    return n;
}

/* I667d mui-text-source: the MUI UI builds every text line through AAText: no MUIC_Text, TextObject or pen escape text left,
 * eight construction sites (counted from the source: the helper definition plus eight calls = 9 "mui_aatext(" spellings). */
static void test_text_source(void) {
    char* ui = slurp(UI_SOURCE);
    char* cls = slurp(AATEXT_SOURCE);
    char* cm = slurp(MUI_CMAKE);
    CHECK(ui != NULL && cls != NULL && cm != NULL, "sources readable");
    if (ui == NULL || cls == NULL || cm == NULL) {
        return;
    }
    CHECK(count_of(ui, "MUIC_Text") == 0, "MUIC_Text left in mui_ui.c: %d", count_of(ui, "MUIC_Text"));
    CHECK(count_of(ui, "TextObject") == 0, "TextObject left in mui_ui.c: %d", count_of(ui, "TextObject"));
    CHECK(count_of(ui, "MUIA_Text_Contents") == 0 && count_of(ui, "MUIA_Text_PreParse") == 0, "Text attribute left in mui_ui.c");
    CHECK(count_of(ui, "mui_aatext(") == 9, "text constructor sites: %d (definition + 8)", count_of(ui, "mui_aatext("));
    CHECK(count_of(ui, "mui_aatext_new(") == 1, "one AAText constructor call: %d", count_of(ui, "mui_aatext_new("));
    CHECK(strstr(cls, "mui_aa_text_fit(") != NULL && strstr(cls, "mui_aa_tone_token(") != NULL && strstr(cls, "mui_aa_tone_font_px(") != NULL && strstr(cls, "aa_text(") != NULL,
          "class does not draw through the shared tone, fit and text helpers");
    CHECK(strstr(cls, "ObtainBestPenA") == NULL && strstr(cls, "TT_") == NULL, "class holds no pen or ttengine code of its own");
    CHECK(strstr(cm, "mui_aatext_class.c") != NULL, "class not in the build");
    free(ui);
    free(cls);
    free(cm);
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
    } else if (!strcmp(t, "check")) {
        test_check();
    } else if (!strcmp(t, "text-fit")) {
        test_text_fit();
    } else if (!strcmp(t, "text-tone")) {
        test_text_tone();
    } else if (!strcmp(t, "text-source")) {
        test_text_source();
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

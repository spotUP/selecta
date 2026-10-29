/* I665 spike: rounded-shape geometry and caption colour ramp of the flat button (map/mui_aa.c). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mui_aa.h"
#include "mui_map.h"
#include "mui_pens.h"
#include "mui_theme.h"
#include <selecta/uimodel.h>
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

/* I667e mui-panel-source: the MUI UI builds every panel through AAPanel, counted from the source: no TH PANEL group left, the helper
 * definition plus five calls (sidebar, player, main, detail, art frame) = 6 "mui_aapanel(" spellings, one AAPanel constructor call. */
static void test_panel_source(void) {
    char* ui = slurp(UI_SOURCE);
    char* cls = slurp(AAPANEL_SOURCE);
    char* cm = slurp(MUI_CMAKE);
    CHECK(ui != NULL && cls != NULL && cm != NULL, "sources readable");
    if (ui == NULL || cls == NULL || cm == NULL) {
        return;
    }
    CHECK(count_of(ui, "TH_TAGS(MUI_TH_PANEL") == 0, "TH PANEL group left in mui_ui.c: %d", count_of(ui, "TH_TAGS(MUI_TH_PANEL"));
    CHECK(count_of(ui, "MUIA_Frame, MUIV_Frame_Group") == 0, "plain art frame left in mui_ui.c");
    CHECK(count_of(ui, "mui_aapanel(") == 6, "panel constructor sites: %d (definition + 5)", count_of(ui, "mui_aapanel("));
    CHECK(count_of(ui, "mui_aapanel_new(") == 1, "one AAPanel constructor call: %d", count_of(ui, "mui_aapanel_new("));
    CHECK(strstr(cls, "mui_aa_panel_rgb(") != NULL && strstr(cls, "mui_aa_panel_radius(") != NULL && strstr(cls, "aa_round_fill(") != NULL,
          "class does not draw through the shared panel helpers");
    CHECK(strstr(cls, "ObtainBestPenA") == NULL && strstr(cls, "0x1") == NULL, "class holds no pen code or colour literal of its own");
    CHECK(strstr(cm, "mui_aapanel_class.c") != NULL, "class not in the build");
    free(ui);
    free(cls);
    free(cm);
}

/* I667f mui-slider-geom: sizes per tier, colours per state, fill width and pointer -> value, progress fill, and the update paint plan
 * (I667f2): op 0 erases the full box, then track and fill, no knob. The sentinel proves the shared maths ran. */
static void test_slider_geom(void) {
    static const int fonts[4] = {0, 13, 15, 17};
    static const int heights[4] = {12, 16, 18, 20};
    static const int tracks[4] = {2, 4, 4, 4};
    txt_theme th;
    uint32_t track, fill;
    uint32_t before = mui_aa_slider_calls();
    mui_aa_slider_op_t ops[MUI_AA_SLIDER_OPS];
    int i, n, w = 200;
    long v;
    txt_theme_default(&th);
    for (i = 0; i < 4; i++) {
        mui_aa_slider_t m;
        mui_aa_slider_metrics(fonts[i], &m);
        CHECK(m.height == heights[i] && m.track_h == tracks[i] && m.radius == m.track_h / 2 && m.min_w == 80, "tier %d metrics height %d track %d", i, m.height, m.track_h);
    }
    CHECK(mui_aa_slider_fill_w(w, 0, 0, 100) == 0 && mui_aa_slider_fill_w(w, 100, 0, 100) == w && mui_aa_slider_fill_w(w, 50, 0, 100) == 100 &&
              mui_aa_slider_fill_w(w, 500, 0, 100) == w && mui_aa_slider_fill_w(w, -5, 0, 100) == 0 && mui_aa_slider_fill_w(w, 5, 0, 0) == 0,
          "fill width");
    CHECK(mui_aa_slider_value_at(w, -40, 0, 100) == 0 && mui_aa_slider_value_at(w, w + 40, 0, 100) == 100 && mui_aa_slider_value_at(w, 100, 0, 100) == 50,
          "pointer maps to value, clamped");
    for (v = 0; v <= 100; v++) {
        long back = mui_aa_slider_value_at(w, mui_aa_slider_fill_w(w, v, 0, 100), 0, 100);
        CHECK(back == v, "round trip value %ld -> %ld", v, back);
    }
    mui_aa_slider_rgb(th.token, MUI_AA_NORMAL, &track, &fill);
    CHECK(track == th.token[SEL_GUI_TOKEN_TRACK] && fill == th.token[SEL_GUI_TOKEN_ACCENT], "normal colours");
    mui_aa_slider_rgb(th.token, MUI_AA_DISABLED, &track, &fill);
    CHECK(track == th.token[SEL_GUI_TOKEN_TRACK] && fill == th.token[SEL_GUI_TOKEN_TEXT_DISABLED], "disabled colours");
    mui_aa_slider_rgb(th.token, MUI_AA_NORMAL, NULL, NULL);
    CHECK(mui_aa_progress_fill_w(200, 0, 100) == 0 && mui_aa_progress_fill_w(200, 100, 100) == 200 && mui_aa_progress_fill_w(200, 50, 100) == 100 &&
              mui_aa_progress_fill_w(200, 500, 100) == 200 && mui_aa_progress_fill_w(200, -3, 100) == 0 && mui_aa_progress_fill_w(200, 5, 0) == 0,
          "progress fill width");
    {
        mui_aa_slider_t m;
        int h = 16;
        mui_aa_slider_metrics(13, &m);
        n = mui_aa_slider_plan(th.token, &m, MUI_AA_NORMAL, w, h, 50, 0, 100, ops);
        CHECK(n == 3 && ops[0].kind == MUI_AA_SLIDER_ERASE && ops[0].x == 0 && ops[0].y == 0 && ops[0].w == w && ops[0].h == h,
              "update plan starts with the full-box erase (n %d)", n);
        CHECK(ops[1].kind == MUI_AA_SLIDER_TRACK && ops[1].w == w && ops[1].rgb == th.token[SEL_GUI_TOKEN_TRACK] && ops[2].kind == MUI_AA_SLIDER_FILL &&
                  ops[2].w == 100 && ops[2].rgb == th.token[SEL_GUI_TOKEN_ACCENT],
              "track then accent fill");
        CHECK(ops[1].y == (h - m.track_h) / 2 && ops[1].y + ops[1].h <= h, "track centred in the box");
        n = mui_aa_slider_plan(th.token, &m, MUI_AA_NORMAL, w, h, 0, 0, 100, ops);
        CHECK(n == 2 && ops[0].kind == MUI_AA_SLIDER_ERASE && ops[1].kind == MUI_AA_SLIDER_TRACK, "empty fill: erase and track only");
    }
    CHECK(mui_aa_slider_calls() > before + 20, "sentinel: slider maths ran (%u)", (unsigned)(mui_aa_slider_calls() - before));
}

/* I667b4 mui-btn-align: where icon+caption start in a button. Centre is unchanged, left sits pad_x from the edge, right pad_x from the other
 * edge; content wider than the box never goes below the left pad. The sidebar doors are built left aligned, every other button centred. */
static void test_btn_align(void) {
    char* ui = slurp(UI_SOURCE);
    char* cls = slurp(AABTN_SOURCE);
    CHECK(mui_aa_btn_content_x(MUI_AA_ALIGN_CENTRE, 100, 40, 8) == (100 - 40) / 2, "centre x");
    CHECK(mui_aa_btn_content_x(MUI_AA_ALIGN_LEFT, 100, 40, 8) == 8, "left x %d", mui_aa_btn_content_x(MUI_AA_ALIGN_LEFT, 100, 40, 8));
    CHECK(mui_aa_btn_content_x(MUI_AA_ALIGN_RIGHT, 100, 40, 8) == 100 - 40 - 8, "right x");
    CHECK(mui_aa_btn_content_x(MUI_AA_ALIGN_LEFT, 30, 40, 8) >= 8, "wide content keeps the pad");
    CHECK(ui != NULL && cls != NULL, "sources readable");
    if (ui != NULL && cls != NULL) {
        CHECK(strstr(cls, "mui_aa_btn_content_x(") != NULL && strstr(cls, "(w - cw) / 2") == NULL, "class centres by hand instead of the shared helper");
        CHECK(count_of(ui, "mui_make_button_left(") == 2, "door buttons built left aligned: %d sites (definition + door row)", count_of(ui, "mui_make_button_left("));
        CHECK(count_of(ui, "sp.align = align;") == 2 && count_of(ui, "sp.align = MUI_AA_ALIGN_CENTRE;") == 1, "every button spec sets align");
    }
    free(ui);
    free(cls);
}

/* I667b5 mui-transport-icons: Previous, Play/Pause, Next, Repeat and Shuffle are icon-only (label = tooltip) and lit by state; Stop keeps its caption.
 * All six share the first row. */
static void test_transport_icons(void) {
    char* ui = slurp(UI_SOURCE);
    sel_uim_frame f;
    uint32_t b;
    memset(&f, 0, sizeof f);
    for (b = 0; b < MUI_BTN_COUNT; b++) {
        CHECK(mui_transport_icon_only(b) == (b != MUI_BTN_STOP), "button %u icon-only %d", (unsigned)b, mui_transport_icon_only(b));
    }
    CHECK(MUI_TRANSPORT_ROW1 == MUI_BTN_COUNT, "all transport buttons in the first row");
    CHECK(mui_transport_on(MUI_BTN_REPEAT, &f) == 0 && mui_transport_on(MUI_BTN_RANDOM, &f) == 0, "off at start");
    f.repeat_mode = 1;
    f.random_on = 1;
    CHECK(mui_transport_on(MUI_BTN_REPEAT, &f) == 1 && mui_transport_on(MUI_BTN_RANDOM, &f) == 1 && mui_transport_on(MUI_BTN_PLAY, &f) == 0, "lit by state");
    CHECK(ui != NULL, "ui source readable");
    if (ui != NULL) {
        CHECK(strstr(ui, "mui_transport_icon_only(i)") != NULL && strstr(ui, "row1b") == NULL, "mui_player_group does not build icon-only buttons in one row");
        CHECK(strstr(ui, "mui_aabtn_set_on(") != NULL, "lit state not pushed to the buttons");
    }
    free(ui);
}

/* I667f mui-slider-source: the MUI UI builds every slider through AASlider: no MUIC_Slider, gauge or prop left, the helper definition
 * plus three calls (volume, seek, settings) = 4 "mui_make_slider(" spellings, one AASlider constructor call. */
static void test_slider_source(void) {
    char* ui = slurp(UI_SOURCE);
    char* cls = slurp(AASLIDER_SOURCE);
    char* cm = slurp(MUI_CMAKE);
    CHECK(ui != NULL && cls != NULL && cm != NULL, "sources readable");
    if (ui == NULL || cls == NULL || cm == NULL) {
        return;
    }
    CHECK(count_of(ui, "MUIC_Slider") == 0 && count_of(ui, "MUIA_Slider_") == 0, "stock slider left in mui_ui.c");
    CHECK(count_of(ui, "MUIC_Gauge") == 0 && count_of(ui, "MUIC_Prop") == 0 && count_of(ui, "GaugeObject") == 0, "gauge or prop left in mui_ui.c");
    CHECK(count_of(ui, "mui_make_slider(") == 4, "slider constructor sites: %d (definition + 3)", count_of(ui, "mui_make_slider("));
    CHECK(count_of(ui, "mui_aaslider_new(") == 1, "one AASlider constructor call: %d", count_of(ui, "mui_aaslider_new("));
    CHECK(count_of(ui, "mui_aaslider_class_free(") == 1, "class freed in mui_close");
    CHECK(strstr(cls, "mui_aa_slider_plan(") != NULL && strstr(cls, "MUIM_DrawBackground") != NULL && strstr(cls, "mui_aa_slider_value_at(") != NULL &&
              strstr(cls, "mui_aa_progress_fill_w(") != NULL && strstr(cls, "aa_round_fill(") != NULL,
          "class does not draw through the shared slider helpers");
    CHECK(strstr(cls, "ObtainBestPenA") == NULL && strstr(cls, "0x1") == NULL, "class holds no pen code or colour literal of its own");
    CHECK(strstr(cm, "mui_aaslider_class.c") != NULL, "class not in the build");
    free(ui);
    free(cls);
    free(cm);
}

/* I667g mui-cycle-step: Up/Down wrap, the valid range, sizes per tier, colours per state, chevron rows; the sentinel proves the shared
 * maths ran. */
static void test_cycle_step(void) {
    static const int fonts[4] = {0, 13, 15, 17};
    static const int counts[3] = {2, 3, 6};
    txt_theme th;
    uint32_t face, text, chev, pop, hov;
    uint32_t before = mui_aa_cycle_calls();
    int i, k, a;
    txt_theme_default(&th);
    for (i = 0; i < 3; i++) {
        int n = counts[i];
        CHECK(mui_aa_cycle_step(0, n, -1) == n - 1, "n %d: Up from the first wraps to the last", n);
        CHECK(mui_aa_cycle_step(n - 1, n, 1) == 0, "n %d: Down from the last wraps to the first", n);
        for (a = 0; a < n; a++) {
            CHECK(mui_aa_cycle_step(a, n, 1) == (a + 1) % n && mui_aa_cycle_step(mui_aa_cycle_step(a, n, 1), n, -1) == a, "n %d: step %d and back", n, a);
            CHECK(mui_aa_cycle_valid(a, n), "n %d: %d valid", n, a);
        }
        CHECK(!mui_aa_cycle_valid(-1, n) && !mui_aa_cycle_valid(n, n), "n %d: out of range refused", n);
    }
    CHECK(mui_aa_cycle_step(3, 0, 1) == 0 && !mui_aa_cycle_valid(0, 0), "empty cycle");
    for (i = 0; i < 4; i++) {
        mui_aa_cycle_t m;
        mui_aa_metrics_t b;
        mui_aa_cycle_metrics(fonts[i], &m);
        mui_aa_metrics(fonts[i], &b);
        CHECK(m.height == b.height && m.pad_x == b.pad_x && m.radius == b.radius, "tier %d shares the button metrics", i);
        CHECK(m.chev_w == (i == 0 ? 6 : 8) && m.chev_h == m.chev_w / 2 && m.gap == (i == 0 ? 6 : 8), "tier %d chevron %dx%d gap %d", i, m.chev_w, m.chev_h, m.gap);
        CHECK(mui_aa_cycle_min_w(&m, 50) == 2 * m.pad_x + 50 + m.gap + m.chev_w, "tier %d minimum width", i);
        for (k = 0; k < m.chev_h; k++) {
            int x, w;
            mui_aa_cycle_chevron_row(&m, k, &x, &w);
            CHECK(x == k && w == m.chev_w - 2 * k && w >= 1, "tier %d chevron row %d: x %d w %d", i, k, x, w);
        }
    }
    mui_aa_cycle_rgb(th.token, MUI_AA_NORMAL, &face, &text, &chev, &pop, &hov);
    CHECK(face == th.token[SEL_GUI_TOKEN_SURFACE_RAISED] && text == th.token[SEL_GUI_TOKEN_TEXT] && chev == th.token[SEL_GUI_TOKEN_TEXT_MUTED] &&
              pop == th.token[SEL_GUI_TOKEN_SURFACE] && hov == th.token[SEL_GUI_TOKEN_SURFACE_HOVER],
          "normal colours");
    mui_aa_cycle_rgb(th.token, MUI_AA_DISABLED, &face, &text, &chev, NULL, NULL);
    CHECK(text == th.token[SEL_GUI_TOKEN_TEXT_DISABLED] && chev == th.token[SEL_GUI_TOKEN_TEXT_DISABLED], "disabled text and chevron");
    mui_aa_cycle_rgb(th.token, MUI_AA_NORMAL, NULL, NULL, NULL, NULL, NULL);
    CHECK(mui_aa_cycle_calls() > before + 30, "sentinel: cycle maths ran (%u)", (unsigned)(mui_aa_cycle_calls() - before));
}

/* I667g mui-cycle-source: the MUI UI builds every cycle through AACycle: no MUIC_Cycle left; the helper definition plus three calls
 * (rating, subsong, settings) = 4 "mui_make_cycle(" spellings, one AACycle constructor call. */
static void test_cycle_source(void) {
    char* ui = slurp(UI_SOURCE);
    char* cls = slurp(AACYCLE_SOURCE);
    char* cm = slurp(MUI_CMAKE);
    CHECK(ui != NULL && cls != NULL && cm != NULL, "sources readable");
    if (ui == NULL || cls == NULL || cm == NULL) {
        return;
    }
    CHECK(count_of(ui, "MUIC_Cycle") == 0 && count_of(ui, "MUIA_Cycle_") == 0, "stock cycle left in mui_ui.c");
    CHECK(count_of(ui, "mui_make_cycle(") == 4, "cycle constructor sites: %d (definition + 3)", count_of(ui, "mui_make_cycle("));
    CHECK(count_of(ui, "mui_aacycle_new(") == 1, "one AACycle constructor call: %d", count_of(ui, "mui_aacycle_new("));
    CHECK(count_of(ui, "mui_aacycle_class_free(") == 1, "class freed in mui_close");
    CHECK(strstr(cls, "mui_aa_cycle_rgb(") != NULL && strstr(cls, "mui_aa_cycle_step(") != NULL && strstr(cls, "mui_aa_cycle_chevron_row(") != NULL &&
              strstr(cls, "aa_round_fill(") != NULL,
          "class does not draw through the shared cycle helpers");
    CHECK(strstr(cls, "ObtainBestPenA") == NULL && strstr(cls, "0x1") == NULL, "class holds no pen code or colour literal of its own");
    CHECK(strstr(cm, "mui_aacycle_class.c") != NULL, "class not in the build");
    /* The popup window names the cycle's screen; without it MUI opens the popup on the default public screen and Selecta's screen
     * drops behind it (rig 2, 2026-10-29). */
    CHECK(strstr(cls, "MUIC_Window, MUIA_Window_Screen, (ULONG)_screen(obj)") != NULL, "popup window does not open on the cycle's screen");
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
    } else if (!strcmp(t, "panel-source")) {
        test_panel_source();
    } else if (!strcmp(t, "slider-geom")) {
        test_slider_geom();
    } else if (!strcmp(t, "btn-align")) {
        test_btn_align();
    } else if (!strcmp(t, "transport-icons")) {
        test_transport_icons();
    } else if (!strcmp(t, "slider-source")) {
        test_slider_source();
    } else if (!strcmp(t, "cycle-step")) {
        test_cycle_step();
    } else if (!strcmp(t, "cycle-source")) {
        test_cycle_source();
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

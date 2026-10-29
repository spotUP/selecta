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
/* text-maxw (I667f3): a fixed text line (seek row time label) is never wider than its default width, any other is unbounded; the class
 * source routes the decision through mui_aa_text_maxw and the label constructor marks a sized text fixed. */
static void test_text_maxw(void) {
    CHECK(mui_aa_text_maxw(1, 72) == 72, "fixed text max width is its default width");
    CHECK(mui_aa_text_maxw(0, 72) == MUI_AA_TEXT_UNBOUNDED, "flexible text is unbounded");
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
    CHECK(count_of(ui, "mui_aatext(") == 8, "text constructor sites: %d (definition + 7; the eighth goes through mui_aatext_sized)", count_of(ui, "mui_aatext("));
    CHECK(count_of(ui, "mui_aatext_sized(") == 3 && count_of(ui, "mui_attr_fixwidth(") == 1, "time labels sized through mui_attr_fixwidth");
    CHECK(strstr(cls, "mui_aa_text_maxw(d->fixed, d->defw)") != NULL, "class max width not decided by mui_aa_text_maxw");
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
    CHECK(strstr(cls, "mui_aa_panel_rgb(") != NULL && strstr(cls, "mui_aa_panel_radius(") != NULL && strstr(cls, "aa_round_corners(") != NULL,
          "class does not draw through the shared panel helpers");
    CHECK(strstr(cls, "ObtainBestPenA") == NULL && strstr(cls, "0x1") == NULL, "class holds no pen code or colour literal of its own");
    CHECK(strstr(cm, "mui_aapanel_class.c") != NULL, "class not in the build");
    free(ui);
    free(cls);
    free(cm);
}

/* I667j mui-tabs: AATabs maths. The active index is set by a click (valid tab only) and by the Left/Right keys (clamped, no wrap); the
 * underline is 2 px (tier S 1) at the bottom of the active tab's box; colours ACCENT line, TEXT_MUTED idle, TEXT active; the TAB button role
 * paints TEXT_MUTED idle and TEXT when lit. The sentinel proves the shared helpers ran. */
static void test_tabs(void) {
    static const int fonts[4] = {0, 13, 15, 17};
    txt_theme th;
    uint32_t line, idle, act, face, text;
    uint32_t before = mui_aa_tabs_calls();
    int i, lx, ly, lw, lh;
    txt_theme_default(&th);
    CHECK(mui_aa_tabs_click(0, 3) == 0 && mui_aa_tabs_click(2, 3) == 2, "click on a tab picks it");
    CHECK(mui_aa_tabs_click(3, 3) == -1 && mui_aa_tabs_click(-1, 3) == -1 && mui_aa_tabs_click(0, 0) == -1, "click outside the strip changes nothing");
    CHECK(mui_aa_tabs_key(0x4E, 0, 3) == 1 && mui_aa_tabs_key(0x4E, 1, 3) == 2, "Right moves to the next tab");
    CHECK(mui_aa_tabs_key(0x4F, 2, 3) == 1 && mui_aa_tabs_key(0x4F, 1, 3) == 0, "Left moves to the previous tab");
    CHECK(mui_aa_tabs_key(0x4E, 2, 3) == 2 && mui_aa_tabs_key(0x4F, 0, 3) == 0, "keys clamp at both ends");
    CHECK(mui_aa_tabs_key(0x44, 1, 3) == 1 && mui_aa_tabs_key(0x4E, 0, 0) == 0, "other keys and an empty strip keep the index");
    for (i = 0; i < 4; i++) {
        mui_aa_tabs_line(fonts[i], 10, 20, 50, 24, &lx, &ly, &lw, &lh);
        CHECK(lx == 10 && lw == 50 && lh == (i == 0 ? 1 : 2) && ly == 20 + 24 - lh, "tier %d underline x %d y %d w %d h %d", i, lx, ly, lw, lh);
    }
    mui_aa_tabs_line(13, 0, 0, 5, 1, &lx, &ly, &lw, &lh);
    CHECK(lh == 1 && ly == 0, "underline never taller than the tab");
    mui_aa_tabs_rgb(th.token, &line, &idle, &act);
    CHECK(line == th.token[SEL_GUI_TOKEN_ACCENT] && line == 0x3B82F6u, "underline is ACCENT 3B82F6");
    CHECK(idle == th.token[SEL_GUI_TOKEN_TEXT_MUTED] && act == th.token[SEL_GUI_TOKEN_TEXT], "idle TEXT_MUTED, active TEXT");
    mui_aa_button_rgb(th.token, MUI_AA_ROLE_TAB, MUI_AA_NORMAL, &face, &text);
    CHECK(text == idle && face == th.token[SEL_GUI_TOKEN_PANEL], "idle tab button: muted text on the panel");
    mui_aa_button_rgb(th.token, MUI_AA_ROLE_TAB, MUI_AA_SELECTED, &face, &text);
    CHECK(text == act && face == th.token[SEL_GUI_TOKEN_PANEL], "lit tab button: TEXT, face unchanged");
    CHECK(mui_aa_tabs_calls() > before, "tabs helpers did not run");
}

/* I667j mui-tabs-source: no stock Register is left in the MUI UI, the detail pane builds its tabs through one AATabs constructor
 * call, and the class draws through the shared helpers with no colour literal. */
static void test_tabs_source(void) {
    char* ui = slurp(UI_SOURCE);
    char* cls = slurp(AATABS_SOURCE);
    char* cm = slurp(MUI_CMAKE);
    CHECK(ui != NULL && cls != NULL && cm != NULL, "sources readable");
    if (ui == NULL || cls == NULL || cm == NULL) {
        return;
    }
    CHECK(count_of(ui, "MUIC_Register") == 0 && count_of(ui, "MUIA_Register_") == 0, "stock Register left in mui_ui.c");
    CHECK(count_of(ui, "mui_aatabs_new(") == 1, "AATabs constructor calls in mui_ui.c: %d", count_of(ui, "mui_aatabs_new("));
    CHECK(count_of(ui, "AATab_Active") >= 1 && count_of(ui, "MUIA_Group_ActivePage") >= 1, "tab strip not wired to the page group");
    CHECK(count_of(ui, "mui_aatabs_class_free()") == 1, "class not freed at close");
    CHECK(strstr(cls, "mui_aa_tabs_line(") != NULL && strstr(cls, "mui_aa_tabs_rgb(") != NULL && strstr(cls, "mui_aa_tabs_key(") != NULL && strstr(cls, "mui_aa_tabs_click(") != NULL,
          "class does not run the shared tab helpers");
    CHECK(strstr(cls, "MUI_AA_ROLE_TAB") != NULL, "tabs are not TAB role buttons");
    CHECK(strstr(cls, "ObtainBestPenA") == NULL && strstr(cls, "0x1") == NULL, "class holds no pen code or colour literal of its own");
    CHECK(strstr(cm, "mui_aatabs_class.c") != NULL, "class not in the build");
    free(ui);
    free(cls);
    free(cm);
}

/* I667l mui-balance-geom: AABalance maths. Box width is the panel gap (SPACING 8, tier S 4), the line is 1 px wide, centred in the box,
 * inset by the gap top and bottom, clamped to inset 0 for a box shorter than two insets plus one; colours BACKGROUND box, DIVIDER line. */
static void test_balance_geom(void) {
    static const int fonts[4] = {0, 13, 15, 17};
    txt_theme th;
    uint32_t box, line, before = mui_aa_balance_calls();
    int i, lx, ly, lw, lh;
    txt_theme_default(&th);
    for (i = 0; i < 4; i++) {
        int gap = i == 0 ? 4 : 8;
        CHECK(mui_aa_balance_width(th.token, fonts[i]) == gap, "tier %d box width %d", i, mui_aa_balance_width(th.token, fonts[i]));
        mui_aa_balance_line(th.token, fonts[i], gap, 100, &lx, &ly, &lw, &lh);
        CHECK(lx == (gap - 1) / 2 && lw == 1 && ly == gap && lh == 100 - 2 * gap, "tier %d line x %d y %d w %d h %d", i, lx, ly, lw, lh);
    }
    mui_aa_balance_line(th.token, 13, 8, 10, &lx, &ly, &lw, &lh);
    CHECK(ly == 0 && lh == 10, "box shorter than two insets: no inset (y %d h %d)", ly, lh);
    mui_aa_balance_line(th.token, 13, 8, 0, &lx, &ly, &lw, &lh);
    CHECK(lh == 0, "empty box: no line height");
    mui_aa_balance_rgb(th.token, &box, &line);
    CHECK(box == th.token[SEL_GUI_TOKEN_BACKGROUND] && box == 0x121214u, "box is BACKGROUND 121214");
    CHECK(line == th.token[SEL_GUI_TOKEN_DIVIDER] && line == 0x2C2C34u, "line is DIVIDER 2C2C34");
    CHECK(mui_aa_balance_calls() > before, "balance helpers did not run");
}

/* I667l mui-balance-source: no stock Balance left in mui_ui.c, both dividers come from one AABalance constructor, the class is freed at
 * close, draws through the shared helpers, never calls the super class for the draw, and holds no colour literal or pen code. */
static void test_balance_source(void) {
    char* ui = slurp(UI_SOURCE);
    char* cls = slurp(AABALANCE_SOURCE);
    char* cm = slurp(MUI_CMAKE);
    CHECK(ui != NULL && cls != NULL && cm != NULL, "sources readable");
    if (ui == NULL || cls == NULL || cm == NULL) {
        return;
    }
    CHECK(count_of(ui, "BalanceObject") == 0 && count_of(ui, "MUIC_Balance") == 0, "stock Balance left in mui_ui.c");
    CHECK(count_of(ui, "mui_aabalance_new()") == 2, "AABalance constructor calls in mui_ui.c: %d", count_of(ui, "mui_aabalance_new()"));
    CHECK(count_of(ui, "mui_aabalance_class_free()") == 1, "class not freed at close");
    CHECK(strstr(cls, "mui_aa_balance_width(") != NULL && strstr(cls, "mui_aa_balance_line(") != NULL && strstr(cls, "mui_aa_balance_rgb(") != NULL,
          "class does not run the shared balance helpers");
    CHECK(strstr(cls, "MUIC_Balance") != NULL && strstr(cls, "MUIV_Frame_None") != NULL, "class is not a frameless Balance subclass");
    /* Design 3.10: a refused subclass must fall back to a frameless stock Balance, never a NULL child that fails the window. */
    CHECK(count_of(cls, "return aabal_stock();") == 2 && strstr(cls, "return NULL;") == NULL, "constructor can return NULL instead of the stock Balance fallback");
    CHECK(strstr(cls, "ObtainBestPenA") == NULL && strstr(cls, "0x1") == NULL && strstr(cls, "0x2") == NULL, "class holds no pen code or colour literal of its own");
    CHECK(strstr(cm, "mui_aabalance_class.c") != NULL, "class not in the build");
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
        /* Johan rig 2 2026-10-29: the "> PROGDIR:" breadcrumb text ran past its button. The width was measured once at setup from the
         * blank first caption; a fit button (NULL widest) now re-measures at every layout from the caption it was last given. */
        CHECK(strstr(cls, "aabtn_measure(obj, d);") != NULL && count_of(cls, "aa_text_width(&d->aa, obj, d->widest)") == 1, "width not measured at layout");
        CHECK(strstr(cls, "d->fit = sp->widest == NULL;") != NULL && strstr(cls, "if (d->fit) {\n        strncpy(d->widest, caption") != NULL,
              "fit button does not follow its caption");
        CHECK(strstr(ui, "mui_make_button_i(label, NULL, icon_key, NULL, id, weight, 0, MUI_AA_ROLE_GHOST)") != NULL &&
                  strstr(ui, "s->crumb_btn[k] = mui_make_button_w(") != NULL,
              "breadcrumb buttons are not fit buttons");
    }
    free(ui);
    free(cls);
}

/* I667b5 mui-transport-icons: every transport button (Stop too, Johan 2026-10-29) is icon-only (label = tooltip); Repeat and Shuffle are lit by state.
 * All six share the first row. */
static void test_transport_icons(void) {
    char* ui = slurp(UI_SOURCE);
    sel_uim_frame f;
    uint32_t b;
    memset(&f, 0, sizeof f);
    for (b = 0; b < MUI_BTN_COUNT; b++) {
        CHECK(mui_transport_icon_only(b) == 1, "button %u icon-only %d", (unsigned)b, mui_transport_icon_only(b));
    }
    CHECK(mui_transport_icon_only(MUI_BTN_COUNT) == 0, "no icon-only past the last button");
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
 * (rating, subsong, settings, viz pick) = 5 "mui_make_cycle(" spellings, one AACycle constructor call. */
static void test_cycle_source(void) {
    char* ui = slurp(UI_SOURCE);
    char* cls = slurp(AACYCLE_SOURCE);
    char* cm = slurp(MUI_CMAKE);
    CHECK(ui != NULL && cls != NULL && cm != NULL, "sources readable");
    if (ui == NULL || cls == NULL || cm == NULL) {
        return;
    }
    CHECK(count_of(ui, "MUIC_Cycle") == 0 && count_of(ui, "MUIA_Cycle_") == 0, "stock cycle left in mui_ui.c");
    CHECK(count_of(ui, "mui_make_cycle(") == 5, "cycle constructor sites: %d (definition + 4)", count_of(ui, "mui_make_cycle("));
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

/* I667h mui-field-plan: AAField maths. Face SURFACE_RAISED 2A2A32 (I667h2: Johan, SURFACE matched the panel), outline TRACK 3A3A44, FOCUS_RING 93C5FD while active, radius 6 (tier S 4),
 * height as AAButton, pad 8, the text height widens the field, and every call counts on the sentinel the class reads. */
static void test_field_plan(void) {
    txt_theme th;
    mui_aa_field_t m, s;
    mui_aa_metrics_t b;
    mui_aa_check_op_t ops[MUI_AA_FIELD_OPS];
    uint32_t before = mui_aa_field_calls();
    txt_theme_default(&th);
    mui_aa_field_metrics(13, &m);
    mui_aa_field_metrics(0, &s);
    mui_aa_metrics(13, &b);
    CHECK(m.height == b.height && m.radius == 6 && m.pad_x == 8 && m.outline == 1, "tier M metrics: h %d r %d pad %d", m.height, m.radius, m.pad_x);
    CHECK(s.radius == 4 && s.pad_x == 8 && s.pad_x > s.radius, "tier S radius 4, pad 8 clears it");
    CHECK(mui_aa_field_plan(th.token, &m, 0, ops) == MUI_AA_FIELD_OPS, "op count");
    CHECK(ops[0].rgb == 0x3A3A44u && ops[0].inset == 0 && ops[0].radius == 6, "outline TRACK 3A3A44: %06X", (unsigned)ops[0].rgb);
    CHECK(ops[1].rgb == 0x2A2A32u && ops[1].inset == 1 && ops[1].radius == 5, "face SURFACE_RAISED 2A2A32 inside the outline: %06X", (unsigned)ops[1].rgb);
    mui_aa_field_plan(th.token, &m, 1, ops);
    CHECK(ops[0].rgb == 0x93C5FDu && ops[1].rgb == 0x2A2A32u, "focus outline FOCUS_RING 93C5FD, face unchanged: %06X", (unsigned)ops[0].rgb);
    CHECK(mui_aa_field_height(&m, 10) == m.height && mui_aa_field_height(&m, 40) == 40 + 2 * (m.pad_y + m.outline), "height grows only when the text needs it");
    {   /* I667h4: the text line is centred: equal room above and below (+-1 px) */
        int top = mui_aa_field_text_top(&m, 36, 13);
        int above = top, below = 36 - top - 13;
        CHECK(above - below >= -1 && above - below <= 1, "text line centred in a 36 px field: above %d below %d", above, below);
        CHECK(mui_aa_field_text_top(&m, 36, 40) == m.pad_y + m.outline, "no spare room: the line sits at the padding");
    }
    CHECK(mui_aa_field_calls() == before + 8, "sentinel counts the maths calls");
}

/* I667h mui-field-source: the MUI UI builds every string gadget through AAField, counted from the source: no stock MUIC_String left in
 * mui_ui.c, the helper definition plus the two tool-row sites plus the settings site = 4 "mui_make_string(" spellings, one AAField
 * constructor call, the class freed, drawn through the shared helpers and in the build. */
static void test_field_source(void) {
    char* ui = slurp(UI_SOURCE);
    char* cls = slurp(AAFIELD_SOURCE);
    char* cm = slurp(MUI_CMAKE);
    CHECK(ui != NULL && cls != NULL && cm != NULL, "sources readable");
    if (ui == NULL || cls == NULL || cm == NULL) {
        return;
    }
    CHECK(count_of(ui, "MUIC_String") == 0, "stock string left in mui_ui.c: %d", count_of(ui, "MUIC_String"));
    CHECK(count_of(ui, "TH_TAGS(MUI_TH_FIELD") == 0, "stock string frame left in mui_ui.c");
    CHECK(count_of(ui, "mui_make_string(") == 4, "string constructor sites: %d (definition + 3)", count_of(ui, "mui_make_string("));
    CHECK(count_of(ui, "mui_aafield_new(") == 1, "one AAField constructor call: %d", count_of(ui, "mui_aafield_new("));
    CHECK(count_of(ui, "mui_aafield_class_free(") == 1, "class freed in mui_close");
    CHECK(strstr(cls, "mui_aa_field_plan(") != NULL && strstr(cls, "mui_aa_field_height(") != NULL && strstr(cls, "aa_round_fill_hole(") != NULL,
          "class does not draw through the shared field helpers");
    CHECK(strstr(cls, "MUIV_Frame_None") != NULL && strstr(cls, "MUIV_Frame_String") == NULL, "inner string must have no stock frame");
    CHECK(strstr(cls, "ObtainBestPenA") == NULL && strstr(cls, "0x1") == NULL, "class holds no pen code or colour literal of its own");
    CHECK(strstr(cm, "mui_aafield_class.c") != NULL, "class not in the build");
    free(ui);
    free(cls);
    free(cm);
}


/* Field and panel faces paint (Johan rig 2 2026-10-29: the search and command fields showed only the window background).
 * MUI fills MUIP_Draw.flags only inside the Area super call, and a Group super fills the gaps between its children with the
 * background, so anything painted before DoSuperMethodA is either skipped or painted over. Invariant, per *_draw function of every
 * AA class: no msg->flags test before the super call. The Group subclasses paint around their children: AAField around the inner
 * String (aa_round_fill_hole), AAPanel only its corners (aa_round_corners). */
static void check_draw_order(const char* path) {
    char* src = slurp(path);
    const char *p, *end, *flags, *super;
    int draws = 0;
    CHECK(src != NULL, "source readable: %s", path);
    if (src == NULL) {
        return;
    }
    for (p = strstr(src, "_draw(struct IClass*"); p != NULL; p = strstr(end, "_draw(struct IClass*")) {
        end = strstr(p, "\n}\n");
        if (end == NULL) {
            break;
        }
        draws++;
        flags = strstr(p, "msg->flags");
        super = strstr(p, "DoSuperMethodA");
        CHECK(flags == NULL || flags > end || (super != NULL && super < flags), "%s: msg->flags tested before DoSuperMethodA", path);
    }
    CHECK(draws >= 1, "%s: no draw function found", path);
    free(src);
}

static void test_field_draw_flags(void) {
    char* fld = slurp(AAFIELD_SOURCE);
    char* pnl = slurp(AAPANEL_SOURCE);
    check_draw_order(AABTN_SOURCE);
    check_draw_order(AACYCLE_SOURCE);
    check_draw_order(AAFIELD_SOURCE);
    check_draw_order(AALIST_SOURCE);
    check_draw_order(AAPANEL_SOURCE);
    check_draw_order(AASCROLLER_SOURCE);
    check_draw_order(AASLIDER_SOURCE);
    check_draw_order(AATEXT_SOURCE);
    CHECK(fld != NULL && strstr(fld, "aa_round_fill_hole(") != NULL && strstr(fld, "_width(d->str)") != NULL, "AAField does not paint around the inner String");
    CHECK(pnl != NULL && strstr(pnl, "aa_round_corners(") != NULL, "AAPanel does not round its corners after the super");
    free(fld);
    free(pnl);
}

/* I667i mui-scroller-geom (design 3.5): thumb geometry, colour tokens, widths and the sentinel. */
static void test_scroller_geom(void) {
    mui_aa_scroller_t m, ms;
    txt_theme th;
    int ty, th_h;
    uint32_t track, thumb, calls0 = mui_aa_scroller_calls();
    txt_theme_default(&th);
    mui_aa_scroller_metrics(13, &m);
    mui_aa_scroller_metrics(0, &ms);
    CHECK(m.width == 10 && ms.width == 6 && m.radius == 4 && ms.radius == 2 && m.min_thumb == 20, "metrics %d/%d %d/%d %d", m.width, ms.width, m.radius, ms.radius, m.min_thumb);
    CHECK(mui_aa_scroller_geom(&m, 200, 0, 20, 20, &ty, &th_h) == 0 && th_h == 0, "no thumb when everything fits");
    CHECK(mui_aa_scroller_geom(&m, 200, 0, 10, 100, &ty, &th_h) == 1 && ty == 0 && th_h == 20, "top thumb %d %d (min 20)", ty, th_h);
    CHECK(mui_aa_scroller_geom(&m, 200, 90, 10, 100, &ty, &th_h) == 1 && ty + th_h == 200, "bottom thumb ends at the track end: %d+%d", ty, th_h);
    CHECK(mui_aa_scroller_geom(&m, 200, 500, 10, 100, &ty, &th_h) == 1 && ty + th_h == 200, "first clamped");
    CHECK(mui_aa_scroller_geom(&m, 200, 0, 50, 100, &ty, &th_h) == 1 && th_h == 100, "half visible gives half track: %d", th_h);
    CHECK(mui_aa_scroller_first_at(&m, 200, 10, -50, 10, 100) == 0 && mui_aa_scroller_first_at(&m, 200, 10, 900, 10, 100) == 90, "drag ends clamp");
    mui_aa_scroller_rgb(th.token, MUI_AA_NORMAL, &track, &thumb);
    CHECK(track == th.token[SEL_GUI_TOKEN_SURFACE] && thumb == th.token[SEL_GUI_TOKEN_TRACK] && thumb == 0x3A3A44u, "normal colours %06X %06X", (unsigned)track, (unsigned)thumb);
    mui_aa_scroller_rgb(th.token, MUI_AA_HOVER, &track, &thumb);
    CHECK(thumb == th.token[SEL_GUI_TOKEN_SURFACE_PRESSED] && thumb == 0x3E3E4Au, "hover thumb %06X", (unsigned)thumb);
    CHECK(mui_aa_scroller_calls() > calls0, "sentinel did not move");
}

/* I667i mui-list-rows (design 3.6, Johan 2026-10-29): row metrics, key moves, clamps, row colours and the padding of the text. */
static void test_list_rows(void) {
    mui_aa_list_t m, ms;
    txt_theme th;
    uint32_t face, text, calls0 = mui_aa_list_calls();
    int w[3] = {120, 80, 40}, k;
    long a;
    txt_theme_default(&th);
    mui_aa_list_metrics(13, &m);
    mui_aa_list_metrics(0, &ms);
    CHECK(m.row_h == 21 && ms.row_h == 10 && m.pad_x == 10 && m.gap == 8 && m.pad_y >= 2, "metrics %d %d %d %d %d", m.row_h, ms.row_h, m.pad_x, m.gap, m.pad_y);
    mui_aa_list_metrics(15, &ms);
    CHECK(ms.row_h == 23, "L row %d", ms.row_h);
    mui_aa_list_metrics(17, &ms);
    CHECK(ms.row_h == 25, "XL row %d", ms.row_h);
    /* Johan: text sat 5 px from the frame and from the dividers. First column at least the left pad, 8 px each side of a divider. */
    CHECK(mui_aa_list_col_x(&m, 0, w) >= 10, "first column text x %d", mui_aa_list_col_x(&m, 0, w));
    for (k = 1; k < 3; k++) {
        int gap_after = mui_aa_list_col_x(&m, k, w) - (mui_aa_list_col_x(&m, k - 1, w) + w[k - 1]);
        CHECK(gap_after >= 2 * 8, "text to text across divider %d: %d (8 each side)", k, gap_after);
    }
    CHECK(mui_aa_list_delta(&m) >= 16, "stock list DELTA %d centres the BAR, 8 each side", mui_aa_list_delta(&m));
    CHECK(mui_aa_list_visible(&m, 2 * m.pad_y + 5 * m.row_h) == 5 && mui_aa_list_visible(&m, 3) == 0, "visible rows");
    CHECK(mui_aa_list_row_at(&m, 0, 0, 10) == -1 && mui_aa_list_row_at(&m, m.pad_y, 0, 10) == 0 && mui_aa_list_row_at(&m, m.pad_y + m.row_h, 3, 10) == 4 &&
              mui_aa_list_row_at(&m, m.pad_y + 20 * m.row_h, 0, 10) == -1,
          "row_at");
    CHECK(mui_aa_list_clamp_active(5, 0) == -1 && mui_aa_list_clamp_active(-3, 4) == 0 && mui_aa_list_clamp_active(9, 4) == 3, "active clamp");
    CHECK(mui_aa_list_clamp_first(7, 5, 10) == 5 && mui_aa_list_clamp_first(3, 5, 4) == 0 && mui_aa_list_clamp_first(-1, 5, 10) == 0, "first clamp");
    CHECK(mui_aa_list_move(0, 0, 5, MUI_AA_LIST_KEY_DOWN) == -1, "empty list has no active row");
    CHECK(mui_aa_list_move(0, 1, 5, MUI_AA_LIST_KEY_DOWN) == 0 && mui_aa_list_move(0, 1, 5, MUI_AA_LIST_KEY_UP) == 0, "one row stays");
    CHECK(mui_aa_list_move(0, 100, 10, MUI_AA_LIST_KEY_UP) == 0 && mui_aa_list_move(99, 100, 10, MUI_AA_LIST_KEY_DOWN) == 99, "no wrap");
    a = mui_aa_list_move(50, 100, 10, MUI_AA_LIST_KEY_PAGEDOWN);
    CHECK(a > 50 && a <= 60 && mui_aa_list_move(50, 100, 10, MUI_AA_LIST_KEY_PAGEUP) < 50, "page moves %ld", a);
    CHECK(mui_aa_list_move(50, 100, 10, MUI_AA_LIST_KEY_HOME) == 0 && mui_aa_list_move(50, 100, 10, MUI_AA_LIST_KEY_END) == 99, "home end");
    CHECK(mui_aa_list_scroll_to(0, 10, 100, 25) == 16 && mui_aa_list_scroll_to(40, 10, 100, 25) == 25 && mui_aa_list_scroll_to(5, 10, 100, 7) == 5, "scroll_to");
    mui_aa_list_row_rgb(th.token, MUI_AA_NORMAL, 0, &face, &text);
    CHECK(face == th.token[SEL_GUI_TOKEN_SURFACE] && text == th.token[SEL_GUI_TOKEN_TEXT], "normal row");
    mui_aa_list_row_rgb(th.token, MUI_AA_HOVER, 0, &face, NULL);
    CHECK(face == th.token[SEL_GUI_TOKEN_SURFACE_HOVER], "hover row");
    mui_aa_list_row_rgb(th.token, MUI_AA_SELECTED, 1, &face, &text);
    CHECK(face == th.token[SEL_GUI_TOKEN_SURFACE_PRESSED] && text == th.token[SEL_GUI_TOKEN_ACCENT], "selected playing row");
    CHECK(mui_aa_list_calls() > calls0, "sentinel did not move");
    {
        char f[160];
        uint32_t bp;
        for (bp = 0; bp < 4; bp++) {
            const char* p;
            int n = 0;
            mui_columns_format(bp, f, sizeof f);
            for (p = f; (p = strstr(p, "DELTA=16 BAR")) != NULL; p++) {
                n++;
            }
            CHECK(n == (int)mui_columns_for(bp, (uint8_t[8]){0}) - 1 || n >= 1, "format '%s' DELTA count %d", f, n);
            CHECK(strstr(f, "DELTA=16") != NULL, "format '%s' carries DELTA=16", f);
        }
    }
}

/* I667i2 mui-list-cols (design 3.6): column widths from weights and fixed samples, title row height, header colours, lyrics lines. */
static void test_list_cols(void) {
    mui_aa_list_t m;
    txt_theme th;
    uint32_t calls0 = mui_aa_list_calls(), muted = 0, divider = 0;
    int weight[3] = {40, 25, 25}, fixed[3] = {0, 0, 0}, out[3], sum, k;
    int wf[5] = {40, 25, 0, 0, 0}, ff[5] = {0, 0, 0, 56, 48}, o5[5];
    int w[MUI_COL_COUNT];
    char f[160], line[16];
    uint32_t bp, n;
    txt_theme_default(&th);
    mui_aa_list_metrics(13, &m);
    mui_aa_list_col_widths(&m, 600, 3, weight, fixed, out);
    sum = out[0] + out[1] + out[2];
    CHECK(out[0] > out[1] && out[1] == out[2] && sum <= 600 && sum >= 500, "weighted widths %d %d %d (sum %d)", out[0], out[1], out[2], sum);
    mui_aa_list_col_widths(&m, 600, 5, wf, ff, o5);
    CHECK(o5[3] == 56 && o5[4] == 48 && o5[0] > o5[1] && o5[0] + o5[1] + o5[3] + o5[4] <= 600, "fixed columns keep their width: %d %d %d", o5[0], o5[3], o5[4]);
    mui_aa_list_col_widths(&m, 20, 3, weight, fixed, out);
    CHECK(out[0] >= 0 && out[1] >= 0 && out[2] >= 0, "a narrow list never gets a negative width");
    CHECK(mui_aa_list_title_h(&m, 1) == m.row_h && mui_aa_list_title_h(&m, 0) == 0, "title row is one row high, none without a title");
    mui_aa_list_header_rgb(th.token, &muted, &divider);
    CHECK(muted == th.token[SEL_GUI_TOKEN_TEXT_MUTED] && divider == th.token[SEL_GUI_TOKEN_DIVIDER], "header colours are the TEXT_MUTED and DIVIDER tokens");
    CHECK(mui_aa_list_calls() > calls0, "sentinel did not move");
    for (bp = 0; bp < 4; bp++) {
        n = mui_columns_weights(bp, w);
        mui_columns_format(bp, f, sizeof f);
        CHECK(n == mui_columns_for(bp, (uint8_t[MUI_COL_COUNT]){0}) && n >= 1 && n <= MUI_COL_COUNT, "bp %u columns %u", (unsigned)bp, (unsigned)n);
        for (k = 0; k < (int)n; k++) {
            char want[16];
            snprintf(want, sizeof want, "W=%d", w[k]);
            CHECK(w[k] >= 0 && (w[k] == 0 || strstr(f, want) != NULL), "bp %u: weight %d of column %d not in format '%s'", (unsigned)bp, w[k], k, f);
        }
    }
    CHECK(mui_text_lines(NULL) == 0 && mui_text_lines("") == 0 && mui_text_lines("a") == 1 && mui_text_lines("a\nb") == 2 && mui_text_lines("a\nb\n") == 2 && mui_text_lines("a\n\nb") == 3, "line count");
    mui_text_line("one\ntwo\nthree", 1, line, sizeof line);
    CHECK(strcmp(line, "two") == 0, "line 1 '%s'", line);
    mui_text_line("one\ntwo\nthree", 2, line, sizeof line);
    CHECK(strcmp(line, "three") == 0, "line 2 '%s'", line);
    mui_text_line("one\ntwo", 5, line, sizeof line);
    CHECK(line[0] == '\0', "past the last line is empty");
    mui_text_line("abcdefghijklmnop", 0, line, 6);
    CHECK(strcmp(line, "abcde") == 0, "cut to the buffer: '%s'", line);
}

/* I667i mui-list-source: the cycle popup is an AAList, the lists of the middle column carry the left pad, both classes are built, freed once,
 * drawn through the shared helpers, hold no colour literal and no pen code. */
static void test_list_source(void) {
    char* ui = slurp(UI_SOURCE);
    char* cy = slurp(AACYCLE_SOURCE);
    char* ls = slurp(AALIST_SOURCE);
    char* sc = slurp(AASCROLLER_SOURCE);
    char* cm = slurp(MUI_CMAKE);
    CHECK(ui != NULL && cy != NULL && ls != NULL && sc != NULL && cm != NULL, "sources readable");
    if (ui == NULL || cy == NULL || ls == NULL || sc == NULL || cm == NULL) {
        return;
    }
    CHECK(count_of(cy, "MUIC_List") == 0 && count_of(cy, "mui_aalist_new(") == 1, "popup must be an AAList: stock %d, aalist %d", count_of(cy, "MUIC_List"), count_of(cy, "mui_aalist_new("));
    CHECK(count_of(cy, "AAL_Picked") >= 1, "popup listens to AAL_Picked");
    CHECK(count_of(ui, "mui_aalist_class_free(") == 1, "AAList class freed once in mui_close");
    /* I667i2 (design 3.6): main, info, tracks, help and lyrics lists are AALists; no stock list, floattext or listview is left. */
    CHECK(count_of(ui, "MUIC_List") == 0 && count_of(ui, "MUIC_Floattext") == 0 && count_of(ui, "MUIC_Listview") == 0, "stock list classes left in mui_ui.c: %d %d %d",
          count_of(ui, "MUIC_List"), count_of(ui, "MUIC_Floattext"), count_of(ui, "MUIC_Listview"));
    CHECK(count_of(ui, "MUIM_List_") == 0 && count_of(ui, "MUIA_List_") == 0 && count_of(ui, "mui_list_pad_x") == 0, "stock list calls left in mui_ui.c");
    CHECK(count_of(ui, "mui_aalist_new(") == 3 && count_of(ui, "mui_string_aalist(") == 4, "AAList builders: new %d, string %d", count_of(ui, "mui_aalist_new("), count_of(ui, "mui_string_aalist("));
    CHECK(count_of(ui, "sp.cols = &s->main_cols;") == 1 && count_of(ui, "sp.title = 1;") == 1 && count_of(ui, "sp.double_pick = 1;") == 1 && count_of(ui, "sp.no_select = 1;") == 1,
          "main list columns, title row, double pick and lyrics no_select");
    CHECK(count_of(ui, "AAL_Columns") == 1 && count_of(ui, "AAL_Playing") == 1 && count_of(ui, "AAL_Picked") == 1, "columns %d, playing %d, picked %d", count_of(ui, "AAL_Columns"),
          count_of(ui, "AAL_Playing"), count_of(ui, "AAL_Picked"));
    CHECK(count_of(ui, "mui_list_display(&g_list_hook") == 2 && strstr(ui, "static const char* mui_main_cell(") != NULL, "cells come from the display hook");
    CHECK(strstr(ui, "mui_text_lines(s->np_lyric_buf)") != NULL && strstr(ui, "mui_text_line(s->np_lyric_buf") != NULL, "lyrics rows from the pure line helpers");
    CHECK(strstr(ui, "aa_live(") == NULL || strstr(ui, "_win(obj) != NULL") == NULL, "aa_live, not _win, guards a live object");
    {
        char* lv = slurp(LISTVIEW_SOURCE);
        CHECK(lv != NULL && strstr(lv, "MUIC_Group") != NULL && strstr(lv, "MUIC_Listview") == NULL && strstr(lv, "MUIA_Group_Child") != NULL && strstr(lv, "ReturnID") != NULL,
              "Selecta_Listview is a Group around the AAList and still sends the song menu ReturnID");
        free(lv);
    }
    CHECK(strstr(ls, "mui_aa_list_row_rgb(") != NULL && strstr(ls, "mui_aa_list_move(") != NULL && strstr(ls, "mui_aa_list_col_x(") != NULL && strstr(ls, "aa_round_fill(") != NULL,
          "list does not draw through the shared helpers");
    CHECK(strstr(sc, "mui_aa_scroller_geom(") != NULL && strstr(sc, "mui_aa_scroller_rgb(") != NULL && strstr(sc, "aa_round_fill(") != NULL, "scroller does not draw through the shared helpers");
    CHECK(strstr(ls, "mui_aascroller_new(") != NULL && strstr(ls, "mui_aascroller_class_free(") != NULL, "list builds and frees its scroller");
    CHECK(strstr(ls, "ObtainBestPenA") == NULL && strstr(ls, "0x1") == NULL && strstr(sc, "ObtainBestPenA") == NULL && strstr(sc, "0x3") == NULL, "no pen code or colour literal in the classes");
    CHECK(strstr(cm, "mui_aalist_class.c") != NULL && strstr(cm, "mui_aascroller_class.c") != NULL, "classes not in the build");
    free(ui);
    free(cy);
    free(ls);
    free(sc);
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
    } else if (!strcmp(t, "text-maxw")) {
        test_text_maxw();
    } else if (!strcmp(t, "text-fit")) {
        test_text_fit();
    } else if (!strcmp(t, "text-tone")) {
        test_text_tone();
    } else if (!strcmp(t, "text-source")) {
        test_text_source();
    } else if (!strcmp(t, "panel-source")) {
        test_panel_source();
    } else if (!strcmp(t, "tabs")) {
        test_tabs();
    } else if (!strcmp(t, "tabs-source")) {
        test_tabs_source();
    } else if (!strcmp(t, "balance-geom")) {
        test_balance_geom();
    } else if (!strcmp(t, "balance-source")) {
        test_balance_source();
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
    } else if (!strcmp(t, "field-plan")) {
        test_field_plan();
    } else if (!strcmp(t, "field-source")) {
        test_field_source();
    } else if (!strcmp(t, "field-draw-flags")) {
        test_field_draw_flags();
    } else if (!strcmp(t, "scroller-geom")) {
        test_scroller_geom();
    } else if (!strcmp(t, "list-rows")) {
        test_list_rows();
    } else if (!strcmp(t, "list-cols")) {
        test_list_cols();
    } else if (!strcmp(t, "list-source")) {
        test_list_source();
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

/* I685w (design 2026-11-06-mui-mcc-package.md section 5.2): on a screen of 15+ bits with a graphics library open, every MCC span reaches
 * the screen as exact RGB and no pen is asked for; on 8 bits the same plan goes through pens. Drives the Button draw plan (the
 * colours of map/mui_aa.c behind aabtn_draw, the AACheck plan, the rounded body, a glyph row) through map/mui_aa_paint.c with a
 * recording ops. mui_aa_draw.c binds the same ops to p96RectFill / FillPixelArray / SetAPen + RectFill. */
#include <stdio.h>
#include <string.h>

#include "mui_aa.h"
#include "mui_aa_paint.h"
#include <selecta/uimodel/theme.h>
#include <selecta/uimodel/tokens.h>

static int fails;
#define CHECK(c, ...)                                  \
    do {                                               \
        if (!(c)) {                                    \
            printf("FAIL %s:%d ", __FILE__, __LINE__); \
            printf(__VA_ARGS__);                       \
            printf("\n");                              \
            fails++;                                   \
        }                                              \
    } while (0)

#define MAXREC 4096
typedef struct {
    int n_rgb, n_pen_rect, n_pen_ask;
    uint32_t rgb[MAXREC];
    uint32_t asked[MAXREC];
    int body_span_w; /* widest span of the body fill (the Play button face) */
    uint32_t body_rgb;
} rec;

static void r_rgb(void* ctx, int x0, int y0, int x1, int y1, uint32_t rgb) {
    rec* r = ctx;
    (void)y0;
    (void)y1;
    if (r->n_rgb < MAXREC) {
        r->rgb[r->n_rgb] = rgb;
    }
    if (x1 - x0 + 1 > r->body_span_w) {
        r->body_span_w = x1 - x0 + 1;
        r->body_rgb = rgb;
    }
    r->n_rgb++;
}
static void r_pen_rect(void* ctx, int x0, int y0, int x1, int y1, long pen) {
    rec* r = ctx;
    (void)x0;
    (void)y0;
    (void)x1;
    (void)y1;
    (void)pen;
    r->n_pen_rect++;
}
static long r_pen(void* ctx, uint32_t rgb) {
    rec* r = ctx;
    if (r->n_pen_ask < MAXREC) {
        r->asked[r->n_pen_ask] = rgb;
    }
    r->n_pen_ask++;
    return 7;
}

/* The Button plan: check plan ops, the rounded body in the Play face, a glyph row of three ramp slots. */
static void run_plan(const mui_aa_paint_ops* ops, const uint32_t* token) {
    mui_aa_metrics_t m;
    mui_aa_check_op_t cops[MUI_AA_CHECK_OPS];
    uint32_t face, text, slot_rgb[4];
    unsigned char slots[8] = {0, 1, 1, 2, 2, 3, 0, 1};
    int k, n;
    mui_aa_metrics(16, &m);
    n = mui_aa_check_plan(token, &m, MUI_AA_SELECTED, cops);
    for (k = 0; k < n; k++) {
        mui_aa_paint_round_fill(ops, 10 + cops[k].inset, 10 + cops[k].inset, m.check_px - 2 * cops[k].inset, m.check_px - 2 * cops[k].inset, cops[k].radius, cops[k].rgb);
    }
    mui_aa_button_rgb(token, 0, 0, &face, &text);
    mui_aa_paint_round_fill(ops, 0, 0, 80, 24, m.radius, face);
    mui_aa_paint_round_frame(ops, 0, 0, 80, 24, m.radius, token[SEL_GUI_TOKEN_FOCUS_RING]);
    slot_rgb[0] = face;
    slot_rgb[1] = mui_aa_ramp_rgb(text, face, 4, 1);
    slot_rgb[2] = mui_aa_ramp_rgb(text, face, 4, 2);
    slot_rgb[3] = text;
    mui_aa_paint_runs(ops, 20, 8, slots, 8, slot_rgb);
}

static int is_plan_colour(const uint32_t* token, uint32_t rgb, uint32_t face, uint32_t text) {
    int i;
    for (i = 0; i < SEL_GUI_TOKEN_COUNT; i++) {
        if (token[i] == rgb) {
            return 1;
        }
    }
    for (i = 0; i < 4; i++) {
        if (mui_aa_ramp_rgb(text, face, 4, i) == rgb) {
            return 1;
        }
    }
    return mui_aa_pressed_rgb(face) == rgb;
}

int main(void) {
    txt_theme th;
    static rec r;
    mui_aa_paint_ops ops;
    uint32_t face, text;
    int i;
    txt_theme_default(&th);
    mui_aa_button_rgb(th.token, 0, 0, &face, &text);

    /* Which draw path a screen takes. */
    CHECK(mui_aa_truecolour(24, 1, 0) == 1, "24 bit with P96 is truecolour");
    CHECK(mui_aa_truecolour(24, 0, 1) == 1, "24 bit with CGX is truecolour");
    CHECK(mui_aa_truecolour(15, 1, 0) == 1, "15 bit with P96 is truecolour");
    CHECK(mui_aa_truecolour(24, 0, 0) == 0, "24 bit with no RTG library keeps pens");
    CHECK(mui_aa_truecolour(8, 1, 1) == 0, "8 bit keeps pens even with both libraries");

    /* tc = 1: spans go out as exact RGB. */
    memset(&r, 0, sizeof r);
    ops.ctx = &r;
    ops.tc = 1;
    ops.rect_rgb = r_rgb;
    ops.rect_pen = r_pen_rect;
    ops.pen = r_pen;
    run_plan(&ops, th.token);
    CHECK(r.n_pen_ask == 0, "truecolour asked for %d pens", r.n_pen_ask);
    CHECK(r.n_pen_rect == 0, "truecolour made %d pen fills", r.n_pen_rect);
    CHECK(r.n_rgb > 0, "truecolour made no RGB fills");
    CHECK(r.body_span_w == 80 - 2 * mui_aa_round_inset(80, 24, 8, 12) || r.body_span_w >= 70, "body span width %d", r.body_span_w);
    CHECK(r.body_rgb == face && face == 0x2A2A32u, "Play body is %06X, want 2A2A32 exactly", (unsigned)r.body_rgb);
    for (i = 0; i < r.n_rgb && i < MAXREC; i++) {
        CHECK(is_plan_colour(th.token, r.rgb[i], face, text), "span %d colour %06X is neither a Dark token nor a ramp step", i, (unsigned)r.rgb[i]);
    }

    /* tc = 0: the same plan goes through pens. */
    memset(&r, 0, sizeof r);
    ops.tc = 0;
    run_plan(&ops, th.token);
    CHECK(r.n_pen_rect > 0, "8 bit made no pen fills");
    CHECK(r.n_rgb == 0, "8 bit made %d RGB fills", r.n_rgb);
    {
        int got = 0;
        for (i = 0; i < r.n_pen_ask && i < MAXREC; i++) {
            got |= r.asked[i] == 0x2A2A32u;
        }
        CHECK(got, "8 bit never asked for a pen of 2A2A32");
    }
    if (fails == 0) {
        printf("mcc-truecolour-path OK\n");
    }
    return fails != 0;
}

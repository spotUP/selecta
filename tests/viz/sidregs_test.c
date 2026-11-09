/* I434j2: SID PatternScope surface test. Includes the plugin source (its label tables and render are static), drives it
 * through the plugin descriptor's VIZ_FRAME hook like the host does, on hand-built frames in INDEX8 surfaces. */
#include "../../plugins/viz/sidregs/sidregs.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;

#define CHECK(cond, ...) \
    do { \
        if (!(cond)) { \
            printf("FAIL %s:%d ", __FILE__, __LINE__); \
            printf(__VA_ARGS__); \
            printf("\n"); \
            failures++; \
        } \
    } while (0)

enum { BG = 0, GRID = 1, PRIMARY = 2, SECONDARY = 3, ACCENT = 4 };
static const sel_viz_colors k_colors = {BG, GRID, PRIMARY, SECONDARY, ACCENT};

typedef struct shot {
    sel_surface s;
    uint8_t* px;
} shot;

static void shot_make(shot* t, uint32_t w, uint32_t h) {
    t->px = calloc(w * h, 1);
    t->s.w = w;
    t->s.h = h;
    t->s.pitch = w;
    t->s.fmt = SEL_PIX_INDEX8;
    t->s.pixels = t->px;
    t->s.palette = NULL;
    t->s.pix_aspect_q8 = 0u;
    t->s.pen = NULL; /* I1230: the plugin's own drawing, as on the Amiga */
}

static uint8_t at(const shot* t, int x, int y) { return t->px[(uint32_t)y * t->s.pitch + (uint32_t)x]; }

static uint32_t count_in(const shot* t, int x0, int y0, int x1, int y1, uint8_t v) {
    uint32_t n = 0;
    int x, y;
    for (y = y0; y < y1; y++) {
        for (x = x0; x < x1; x++) {
            n += at(t, x, y) == v;
        }
    }
    return n;
}

/* The host path: plugin entry, descriptor tags, VIZ_FRAME hook. */
static sel_viz_render_fn hook(void) {
    const sel_tag* d = sel_plugin_entry(1u);
    const sel_tag* t;
    for (; d != NULL && d->tag != SEL_TAG_END; d++) {
        if (d->tag == SEL_PLUGIN_TAGS) {
            for (t = (const sel_tag*)d->value; t->tag != SEL_TAG_END; t++) {
                if (t->tag == SEL_VIZ_FRAME) {
                    return (sel_viz_render_fn)t->value;
                }
            }
        }
    }
    return NULL;
}

static uint32_t draw(const sel_viz_frame* f, shot* t) { return hook()(f, &t->s, &k_colors); }

static sel_viz_frame* frame_make(uint32_t chips) {
    sel_viz_frame* f = calloc(1, sizeof *f);
    f->seq = 1u;
    f->sid_chips = chips;
    return f;
}

static void name_and_labels(void) {
    const sel_tag* d = sel_plugin_entry(1u);
    uint32_t i, bad = 0;
    const char* const* h;
    CHECK(d != NULL && hook() != NULL, "descriptor has no VIZ_FRAME hook");
    CHECK(sel_plugin_entry(0u) == NULL, "host api 0 accepted");
    for (h = k_headings; h < k_headings + 9; h++) {
        const char* p;
        for (p = *h; *p; p++) {
            bad += vs_glyph_rows(*p) == NULL;
        }
    }
    for (i = 0; i < sizeof k_voice_labels / sizeof k_voice_labels[0]; i++) {
        const char* p;
        for (p = k_voice_labels[i].text; *p; p++) {
            bad += vs_glyph_rows(*p) == NULL;
        }
    }
    for (i = 0; i < sizeof k_chip_labels / sizeof k_chip_labels[0]; i++) {
        const char* p;
        for (p = k_chip_labels[i].text; *p; p++) {
            bad += vs_glyph_rows(*p) == NULL;
        }
    }
    CHECK(bad == 0u, "%u label characters without a glyph", (unsigned)bad);
}

static void surface(void) {
    shot t;
    sel_viz_frame* f = frame_make(1u);
    uint32_t n;
    /* 320 x 64: cw 8, rh 8. Gate on in voice 1: the Gate cell (column 5, row 3). */
    shot_make(&t, 320, 64);
    n = draw(f, &t);
    CHECK(at(&t, 5 * 8 + 4, 3 * 8 + 4) != PRIMARY && n > 0u, "gate cell primary with the gate off");
    f->sid[0].reg[4] = 0x01;
    n = draw(f, &t);
    CHECK(at(&t, 5 * 8 + 4, 3 * 8 + 4) == PRIMARY, "gate on: cell is %u", (unsigned)at(&t, 5 * 8 + 4, 3 * 8 + 4));
    CHECK(at(&t, 11 * 8 + 4, 3 * 8 + 4) != PRIMARY, "triangle cell lit by the gate");
    /* Fr bar: 8 columns = 64 px at $FFFF, $0800 scaled x4 is $2000 = 8 px of 64 (Hippo: scaled >> 10). */
    f->sid[0].reg[0] = 0xFF;
    f->sid[0].reg[1] = 0xFF;
    draw(f, &t);
    CHECK(at(&t, 3 * 8 + 63, 12) == PRIMARY && at(&t, 3 * 8 + 64, 12) != PRIMARY, "$FFFF bar not 8 columns");
    f->sid[0].reg[0] = 0x00;
    f->sid[0].reg[1] = 0x08;
    draw(f, &t);
    CHECK(at(&t, 3 * 8 + 7, 12) == PRIMARY && at(&t, 3 * 8 + 8, 12) == GRID, "$0800 bar is not 8 px");
    /* Glow 2 on a gate that is off: secondary, 2/5 of the 8 px cell (3 px), anchored at the bottom of the inset cell (y 28..30). */
    f->sid[0].reg[4] = 0x00;
    f->sid[0].glow[0][0] = 2;
    draw(f, &t);
    CHECK(at(&t, 5 * 8 + 4, 3 * 8 + 4) == SECONDARY && at(&t, 5 * 8 + 4, 3 * 8 + 3) != SECONDARY,
          "glow 2 height: %u %u", (unsigned)at(&t, 5 * 8 + 4, 3 * 8 + 4), (unsigned)at(&t, 5 * 8 + 4, 3 * 8 + 3));
    CHECK(count_in(&t, 40, 24, 48, 32, SECONDARY) == 6u * 3u, "glow 2 secondary pixels: %u",
          (unsigned)count_in(&t, 40, 24, 48, 32, SECONDARY));
    /* Labels are drawn in accent at this size. */
    CHECK(count_in(&t, 0, 0, 320, 64, ACCENT) > 100u, "no label pixels");
    free(t.px);
    free(f);
}

/* I918: labels need a glyph height plus one blank row per cell; below that they are an unreadable smear and are left out. */
static void label_threshold(void) {
    shot t;
    sel_viz_frame* f = frame_make(1u);
    shot_make(&t, 320, 48); /* rh 6 */
    draw(f, &t);
    CHECK(count_in(&t, 0, 0, 320, 48, ACCENT) == 0u, "labels drawn at 6 px rows: %u", (unsigned)count_in(&t, 0, 0, 320, 48, ACCENT));
    free(t.px);
    shot_make(&t, 320, 56); /* rh 7 */
    draw(f, &t);
    CHECK(count_in(&t, 0, 0, 320, 56, ACCENT) > 50u, "labels missing at 7 px rows");
    free(t.px);
    free(f);
}

static void zero_chips(void) {
    shot t;
    sel_viz_frame* f = frame_make(0u);
    uint32_t n;
    shot_make(&t, 320, 64);
    n = draw(f, &t);
    CHECK(n == 0u, "0 chips returned %u", (unsigned)n);
    CHECK(count_in(&t, 0, 0, 320, 64, GRID) > 100u && count_in(&t, 0, 0, 320, 64, ACCENT) == 0u, "greyed labels missing");
    free(t.px);
    free(f);
}

static void three_chips(void) {
    shot t;
    sel_viz_frame* f = frame_make(3u);
    uint32_t n;
    shot_make(&t, 320, 192);
    memset(f->sid[2].reg, 0, sizeof f->sid[2].reg);
    f->sid[2].reg[0] = 0xFF;
    f->sid[2].reg[1] = 0xFF;
    n = draw(f, &t);
    /* chip 3 band starts at y 128: Voice7 heading in accent there, its Fr bar full on row 1 of the band */
    CHECK(count_in(&t, 0, 128, 30, 136, ACCENT) > 10u, "no Voice7 heading at y 128");
    CHECK(at(&t, 3 * 8 + 63, 128 + 12) == PRIMARY, "chip 3 Fr bar not at y 128 + row 1");
    CHECK(at(&t, 3 * 8 + 63, 64 + 12) != PRIMARY && at(&t, 3 * 8 + 63, 12) != PRIMARY, "Fr bar leaked to chip 1 or 2");
    /* worst case primitives per chip: every flag on or glowing, every bar full, envelopes full */
    {
        uint32_t v, one;
        shot t1;
        sel_viz_frame* g = frame_make(1u);
        memset(g->sid[0].reg, 0xFF, sizeof g->sid[0].reg);
        for (v = 0; v < 3; v++) {
            g->sid[0].env[v] = 63;
        }
        shot_make(&t1, 320, 64);
        one = draw(g, &t1);
        printf("primitives per chip (all on): %u\n", (unsigned)one);
        CHECK(one <= 600u, "chip uses %u primitives", (unsigned)one);
        memset(g->sid[0].reg, 0, sizeof g->sid[0].reg);
        one = draw(g, &t1);
        CHECK(one <= 600u, "chip (all off) uses %u primitives", (unsigned)one);
        printf("primitives per chip (all off): %u\n", (unsigned)one);
        free(t1.px);
        free(g);
    }
    CHECK(n > 0u, "3 chips drew nothing");
    free(t.px);
    free(f);
}

static void strip(void) {
    shot t;
    sel_viz_frame* f = frame_make(1u);
    uint32_t n;
    shot_make(&t, 160, 16);
    memset(f->sid[0].reg, 0xFF, sizeof f->sid[0].reg);
    f->sid[0].env[0] = 63;
    n = draw(f, &t);
    CHECK(n > 0u, "strip drew nothing");
    CHECK(count_in(&t, 0, 0, 160, 16, ACCENT) == 0u, "strip has accent pixels");
    CHECK(count_in(&t, 0, 0, 160, 16, PRIMARY) > 0u, "strip has no bars");
    free(t.px);
    free(f);
}

/* I950 (closes the SID PatternScope \"very zoomed in characters\"): labels fit their columns and the glyph pixel is square on screen.
 * Before: scale = rh / 7 with no width cap, one scale for both axes. */
static int run_len(const shot* t, int x, int y, int dx, int dy, int lim) {
    int n = 0;
    while (n < lim && at(t, x + n * dx, y + n * dy) == ACCENT) {
        n++;
    }
    return n;
}

static void glyph_aspect(void) {
    shot t;
    sel_viz_frame* f = frame_make(1u);
    int x, y, ytop = -1, w, l;
    uint32_t n;
    /* Square pixels, tall cells (rh 28, cw 8): the heading "Voice1" owns columns 0..6 and "Flt" starts at column 7; the old scale 4 ran Voice1 to x 120. */
    shot_make(&t, 320, 224);
    n = draw(f, &t);
    CHECK(n > 0u, "nothing drawn");
    CHECK(count_in(&t, 72, 0, 104, 28, ACCENT) == 0u, "heading overruns into the next voice: %u accent pixels at x 72..104", (unsigned)count_in(&t, 72, 0, 104, 28, ACCENT));
    free(t.px);
    /* Tall pixels (aspect 512), rh 16, cw 16: a glyph pixel is 2 wide for 1 high; the row-1 label "Fr" starts with a full-height stem. */
    shot_make(&t, 640, 128);
    t.s.pix_aspect_q8 = 512u;
    draw(f, &t);
    for (y = 16; y < 32 && ytop < 0; y++) {
        if (at(&t, 0, y) == ACCENT) {
            ytop = y;
        }
    }
    CHECK(ytop >= 0, "no Fr label found");
    if (ytop >= 0) {
        l = run_len(&t, 0, ytop, 0, 1, 16);
        w = run_len(&t, 0, ytop, 1, 0, 16);
        x = w / 4;
        y = l / 6;
        CHECK(x * 256 == y * 512 && x >= 1 && y >= 1, "glyph pixel %d wide x %d high, not square at aspect 512", x, y);
    }
    free(t.px);
    free(f);
}

int main(int argc, char** argv) {
    const char* mode = argc > 1 ? argv[1] : "all";
    if (strcmp(mode, "all") == 0 || strcmp(mode, "labels") == 0) {
        name_and_labels();
    }
    if (strcmp(mode, "all") == 0 || strcmp(mode, "surface") == 0) {
        surface();
        label_threshold();
        glyph_aspect();
        zero_chips();
        three_chips();
        strip();
    }
    if (failures) {
        return 1;
    }
    printf("sidregs_test %s ok\n", mode);
    return 0;
}

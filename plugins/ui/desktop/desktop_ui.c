/* Desktop UI plugin (M3d, DUI-P1-4). It renders the engine snapshot through the shared UI model
 * (include/selecta/uimodel.h): one widget per region, redrawn only when the region's dirty bit is set. Drawing goes
 * to a backend; the only backend so far is `mem` (headless, no SDL), selected with `--backend mem`. */
#include <selecta/plugin.h>
#include <selecta/ui.h>
#include <selecta/uimodel.h>

#include "gui_draw.h"
#include "theme.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct desk {
    sel_engine* engine;
    const sel_host* host;
    sel_uim uim;
    uint32_t frames;
    uint32_t widget_draws;
    uint32_t hash;
    const char* resize; /* --resize WxH[,WxH...]: resize events the mem backend replays at open */
    sel_uim_layout layout;
    int actions; /* --actions: the close hook prints the action table and the help rows */
    int report; /* --stats: the close hook prints its sentinel line */
    txt_theme theme;       /* design tokens (DUI-P1-10): defaults, then --theme <file> over them */
    int tokens;            /* --tokens: the close hook prints what the mem backend painted with the tokens */
    sel_uim_budget budget; /* --format: the effects the canvas format affords */
    uint32_t tweens;       /* tweens started (title cross-fade on a song change) */
    uint32_t effect_prims; /* blur and shadow primitives run */
    sel_gui_canvas* canvas; /* the drawing surface (DUI-P2-1), --canvas WxH, default 1280x800 */
    sel_gui_shadow shadow;  /* baked once when the governor allows shadows */
    int shadow_ready;
    int testcard;           /* --view testcard: draw the primitive test card once, no widgets */
    uint32_t dirty_pixels;  /* pixels the latest present repainted (DUI-P2-2) */
    uint32_t dirty_total;   /* pixels repainted over all presents */
    uint32_t ticks;         /* --ticks N: clock seconds the mem backend replays after the quiet turn */
    sel_tag stats[13];
} desk;

static const char* desk_opt(const sel_ui_open_args* a, const char* name) {
    int i;
    for (i = 0; i + 1 < a->argc; i++) {
        if (strcmp(a->argv[i], name) == 0) {
            return a->argv[i + 1];
        }
    }
    return NULL;
}

static void desk_hash(desk* d, const char* s) {
    for (; *s != '\0'; s++) {
        d->hash = (d->hash ^ (uint8_t)*s) * 16777619u;
    }
}

#define ARGB(tok) (0xFF000000u | (tok))

static sel_gui_rect rect_of(int32_t x, int32_t y, int32_t w, int32_t h) {
    sel_gui_rect r;
    r.x = x;
    r.y = y;
    r.w = w;
    r.h = h;
    return r;
}

static sel_gui_rect region_rect(const desk* d, const char* name) {
    uint32_t i;
    for (i = 0; i < d->layout.count; i++) {
        const sel_uim_region* r = &d->layout.regions[i];
        if (strcmp(r->name, name) == 0) {
            return rect_of(r->x, r->y, (int32_t)r->w, (int32_t)r->h);
        }
    }
    return rect_of(0, 0, 0, 0);
}

/* Fill the box with the surface colour, then draw one cell per visible character (real glyphs come with the text
 * renderer, DUI-P2-3), and mark the box dirty. */
static void draw_label(desk* d, sel_gui_rect box, const char* s) {
    int32_t i;
    sel_gui_fill_rect(d->canvas, box, ARGB(d->theme.token[SEL_GUI_TOKEN_SURFACE]));
    for (i = 0; s[i] != '\0' && (i + 1) * 6 <= box.w; i++) {
        if (s[i] != ' ') {
            sel_gui_fill_rect(d->canvas, rect_of(box.x + i * 6, box.y + 3, 5, 10),
                              ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT]));
        }
    }
    sel_gui_dirty_add(d->canvas, box);
}

static void draw_bar(desk* d, sel_gui_rect box, uint32_t q16) {
    uint32_t fill = (uint32_t)(((uint64_t)q16 * (uint32_t)box.w) >> 16);
    sel_gui_fill_rect(d->canvas, box, ARGB(d->theme.token[SEL_GUI_TOKEN_SURFACE]));
    sel_gui_fill_rect(d->canvas, rect_of(box.x, box.y, (int32_t)fill, box.h),
                      ARGB(d->theme.token[SEL_GUI_TOKEN_ACCENT]));
    sel_gui_dirty_add(d->canvas, box);
}

/* One widget in its rectangle inside its layout region. Only the dirty widgets run, so a clock tick touches the
 * clock, the remaining time and the progress bar and nothing else. */
static void draw_widget(desk* d, uint32_t bit) {
    const sel_uim_frame* f = &d->uim.frame;
    int32_t sp = (int32_t)d->theme.token[SEL_GUI_TOKEN_SPACING];
    sel_gui_rect pl = region_rect(d, "player"), mn = region_rect(d, "main"), sb = region_rect(d, "sidebar");
    int32_t y0 = pl.y + sp;
    char buf[32];
    if (bit == SEL_UIM_DIRTY_SONG) {
        sel_gui_rect card = rect_of(mn.x + sp, mn.y + sp, mn.w - 2 * sp < 400 ? mn.w - 2 * sp : 400, 48);
        if (d->shadow_ready) {
            sel_gui_shadow_draw(d->canvas, &d->shadow, card);
            sel_gui_dirty_add(d->canvas, rect_of(card.x - 8, card.y - 8, card.w + 16, card.h + 16));
        }
        sel_gui_round_rect(d->canvas, card, (int32_t)d->theme.token[SEL_GUI_TOKEN_RADIUS],
                           ARGB(d->theme.token[SEL_GUI_TOKEN_SURFACE]));
        draw_label(d, rect_of(card.x + sp, card.y + sp, card.w - 2 * sp, 16), f->title);
        draw_label(d, rect_of(card.x + sp, card.y + sp + 20, card.w - 2 * sp, 16), f->artist);
        sel_gui_dirty_add(d->canvas, card);
    } else if (bit == SEL_UIM_DIRTY_QUEUE) {
        sprintf(buf, "%u", (unsigned)f->queue_count);
        draw_label(d, rect_of(sb.x + sp, sb.y + sp, 120, 16), buf);
    } else if (bit == SEL_UIM_DIRTY_TRANSPORT) {
        sel_gui_rect b = rect_of(pl.x + pl.w / 2 - 16, y0 + 24, 32, 32);
        sel_gui_round_rect(d->canvas, b, (int32_t)d->theme.token[SEL_GUI_TOKEN_RADIUS],
                           ARGB(d->theme.token[SEL_GUI_TOKEN_ACCENT]));
        sel_gui_dirty_add(d->canvas, b);
    } else if (bit == SEL_UIM_DIRTY_PROGRESS) {
        draw_label(d, rect_of(pl.x + sp, y0, 56, 16), f->clock);
        draw_label(d, rect_of(pl.x + pl.w - sp - 56, y0, 56, 16), f->remaining);
        draw_bar(d, rect_of(pl.x + 2 * sp + 56, y0 + 6, 480, 4), f->progress_q16);
    } else if (bit == SEL_UIM_DIRTY_VOLUME) {
        int32_t v = f->volume < 0 ? 0 : f->volume > 100 ? 100 : f->volume;
        draw_bar(d, rect_of(pl.x + pl.w - sp - 96, y0 + 36, 96, 8), (uint32_t)v * 65536u / 100u);
    } else if (bit == SEL_UIM_DIRTY_STATUS) {
        draw_label(d, rect_of(pl.x + sp, y0 + 30, 200, 16), f->status);
    }
}

/* The primitive test card (DUI-P2-1): every rasterizer primitive in fixed places, for the golden hash. */
static void draw_testcard(desk* d) {
    sel_gui_canvas* c = d->canvas;
    uint32_t src[32 * 32], i, j;
    uint8_t mask[16 * 16];
    uint32_t accent = ARGB(d->theme.token[SEL_GUI_TOKEN_ACCENT]), text = ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT]);
    sel_gui_fill_rect(c, rect_of(0, 0, c->w, c->h), ARGB(d->theme.token[SEL_GUI_TOKEN_SURFACE]));
    sel_gui_fill_rect(c, rect_of(10, 10, 60, 40), 0xFFCC3333u);
    sel_gui_fill_rect(c, rect_of(40, 30, 60, 40), 0x8033CC33u);
    sel_gui_round_rect(c, rect_of(120, 10, 120, 60), (int32_t)d->theme.token[SEL_GUI_TOKEN_RADIUS], accent);
    sel_gui_round_rect(c, rect_of(250, 10, 40, 40), 20, text);
    sel_gui_gradient(c, rect_of(10, 90, 200, 30), 0, 0xFF000000u, 0xFFFFFFFFu);
    sel_gui_gradient(c, rect_of(220, 90, 60, 90), 1, accent, 0xFFFFD040u);
    for (i = 0; i < 8u; i++) {
        sel_gui_line(c, 330, 50, 330 + (int32_t)i * 12, 10, text);
        sel_gui_line(c, 330, 50, 330 + (int32_t)i * 12, 90, accent);
    }
    sel_gui_line(c, 420, 90, 420, 10, text);
    for (j = 0; j < 16u; j++) {
        for (i = 0; i < 16u; i++) {
            mask[j * 16u + i] = (uint8_t)(((i * 17u) ^ (j * 17u)) & 255u);
        }
    }
    sel_gui_blit_mask(c, 440, 10, mask, 16, 16, 16, text);
    for (j = 0; j < 32u; j++) {
        for (i = 0; i < 32u; i++) {
            src[j * 32u + i] = 0xFF000000u | ((i * 8u) << 16) | ((j * 8u) << 8) | (((i ^ j) & 1u) * 120u);
        }
    }
    sel_gui_blit_image(c, rect_of(10, 140, 64, 64), src, 32, 32, 32, 0);
    sel_gui_blit_image(c, rect_of(90, 140, 16, 16), src, 32, 32, 32, 1);
    if (d->shadow_ready) {
        sel_gui_shadow_draw(c, &d->shadow, rect_of(320, 140, 100, 60));
    }
    sel_gui_round_rect(c, rect_of(320, 140, 100, 60), 8, accent);
    sel_gui_scrim(c, rect_of(0, 220, c->w, 60), 0x000000u, 128u);
    sel_gui_clip_push(c, rect_of(450, 140, 30, 30));
    sel_gui_fill_rect(c, rect_of(440, 130, 80, 80), 0xFFFFFFFFu);
    sel_gui_clip_pop(c);
    sel_gui_dirty_add(c, rect_of(0, 0, c->w, c->h));
}

/* Redraw the dirty regions; the mem backend records what a draw would show in the frame hash. */
static int desk_present(desk* d) {
    uint32_t dirty = sel_uim_take_dirty(&d->uim);
    const sel_uim_frame* f = &d->uim.frame;
    uint32_t bit;
    if (dirty == 0u) {
        return 0;
    }
    if (d->frames == 0u) { /* the first frame paints the whole canvas */
        sel_gui_fill_rect(d->canvas, rect_of(0, 0, d->canvas->w, d->canvas->h),
                          ARGB(d->theme.token[SEL_GUI_TOKEN_SURFACE]));
        sel_gui_dirty_add(d->canvas, rect_of(0, 0, d->canvas->w, d->canvas->h));
    }
    for (bit = 1u; bit <= SEL_UIM_DIRTY_STATUS; bit <<= 1) {
        if ((dirty & bit) == 0u) {
            continue;
        }
        draw_widget(d, bit);
        d->widget_draws++;
        d->hash = (d->hash ^ bit) * 16777619u;
        if (bit == SEL_UIM_DIRTY_SONG) {
            desk_hash(d, f->title);
            desk_hash(d, f->artist);
            desk_hash(d, f->format);
            /* Every effect asks the governor first (DUI-P1-9): the cover backdrop blur, the panel shadow and, from
             * the second frame on, a title cross-fade. */
            if (sel_uim_budget_allows(&d->budget, SEL_UIM_FX_BLUR)) {
                d->effect_prims++;
            }
            if (sel_uim_budget_allows(&d->budget, SEL_UIM_FX_SHADOW)) {
                d->effect_prims++;
            }
            if (d->frames > 0u && sel_uim_budget_allows(&d->budget, SEL_UIM_FX_TWEEN)) {
                d->tweens++;
            }
        } else if (bit == SEL_UIM_DIRTY_PROGRESS) {
            desk_hash(d, f->clock);
            desk_hash(d, f->remaining);
        } else if (bit == SEL_UIM_DIRTY_STATUS) {
            desk_hash(d, f->status);
        }
    }
    d->dirty_pixels = sel_gui_present(d->canvas);
    d->dirty_total += d->dirty_pixels;
    d->frames++;
    return 1;
}

static void desk_print_actions(void) {
    uint32_t i, n = sel_uim_action_count();
    char row[96], msg[160];
    for (i = 0; i < n; i++) {
        const sel_uim_action* a = sel_uim_action_at(i);
        printf("action id=%s label=%s icon=%s menu=%s key=%s line=%s tooltip=%s\n", a->id, a->label, a->icon, a->menu,
               a->key[0] != '\0' ? a->key : "-", a->line, a->tooltip);
        sel_uim_help_row(i, row, sizeof row);
        printf("help %s\n", row);
    }
    printf("actions count=%u exemptions=%u check=%s\n", (unsigned)n, (unsigned)sel_uim_exemption_count(),
           sel_uim_actions_check(msg, sizeof msg) == 0 ? "ok" : msg);
}

/* Each WxH is one resize event: the layout is recomputed and the result printed, regions with their rectangles. */
static void desk_replay_resizes(desk* d) {
    const char* p = d->resize;
    unsigned w, h;
    int used;
    char msg[96];
    while (sscanf(p, "%ux%u%n", &w, &h, &used) == 2) {
        uint32_t i;
        sel_uim_layout_compute(&d->layout, w, h);
        printf("layout width=%u height=%u id=%s regions=%u check=%s\n", w, h,
               sel_uim_breakpoint_name(d->layout.breakpoint), (unsigned)d->layout.count,
               sel_uim_layout_check(&d->layout, msg, sizeof msg) == 0 ? "ok" : msg);
        for (i = 0; i < d->layout.count; i++) {
            const sel_uim_region* r = &d->layout.regions[i];
            printf("region name=%s x=%d y=%d w=%u h=%u\n", r->name, (int)r->x, (int)r->y, (unsigned)r->w,
                   (unsigned)r->h);
        }
        p += used;
        if (*p != ',') {
            break;
        }
        p++;
    }
}

/* What the mem backend paints: every widget takes its colour and size from a token, never from a literal. */
static void desk_print_tokens(const desk* d) {
    static const uint32_t none[SEL_GUI_TOKEN_COUNT] = {
#define X(id, name, def, lo, hi) def,
        SEL_GUI_TOKENS(X)
#undef X
    };
    uint32_t changed = sel_gui_tokens_diff(d->theme.token, none), i, n = 0u;
    printf("pixel widget=play_button rgb=0x%06x\n", (unsigned)d->theme.token[SEL_GUI_TOKEN_ACCENT]);
    printf("pixel widget=panel rgb=0x%06x\n", (unsigned)d->theme.token[SEL_GUI_TOKEN_SURFACE]);
    printf("pixel widget=label rgb=0x%06x\n", (unsigned)d->theme.token[SEL_GUI_TOKEN_TEXT]);
    printf("metric name=corner_radius px=%u\n", (unsigned)d->theme.token[SEL_GUI_TOKEN_RADIUS]);
    printf("metric name=gap px=%u\n", (unsigned)d->theme.token[SEL_GUI_TOKEN_SPACING]);
    for (i = 0; i < SEL_GUI_TOKEN_COUNT; i++) {
        n += (changed >> i) & 1u;
    }
    printf("tokens count=%u changed=%u\n", (unsigned)SEL_GUI_TOKEN_COUNT, (unsigned)n);
    for (i = 0; i < SEL_GUI_TOKEN_COUNT; i++) {
        if ((changed >> i) & 1u) {
            printf("token changed=%s\n", sel_gui_token_name((sel_gui_token)i));
        }
    }
}

/* Reads the --theme file over the defaults; 0 with a message on a missing file or a bad line. */
static int desk_load_theme(desk* d, const char* path) {
    char buf[4096];
    size_t n;
    int bad = 0;
    FILE* f = fopen(path, "rb");
    if (f == NULL) {
        fprintf(stderr, "selecta: desktop theme '%s' cannot be read\n", path);
        return 0;
    }
    n = fread(buf, 1, sizeof buf - 1, f);
    fclose(f);
    buf[n] = '\0';
    if (txt_theme_parse(&d->theme, buf, &bad) < 0) {
        fprintf(stderr, "selecta: desktop theme '%s' line %d is not a role or token line in range\n", path, bad);
        return 0;
    }
    return 1;
}

static void desk_close(void* state) {
    desk* d = (desk*)state;
    if (d != NULL) {
        if (d->report) {
            printf("desktop_ui closed frames=%u title=%s clock=%s status=%s\n", (unsigned)d->frames, d->uim.frame.title,
                   d->uim.frame.clock, d->uim.frame.status);
        }
        if (d->tokens) {
            desk_print_tokens(d);
        }
        if (d->resize != NULL) {
            desk_replay_resizes(d);
        }
        if (d->actions) {
            desk_print_actions();
        }
        if (d->canvas != NULL) {
            d->host->mem_free(d->host, d->canvas->pixels);
            d->host->mem_free(d->host, d->canvas);
        }
        d->host->mem_free(d->host, d);
    }
}

/* Allocates the canvas from --canvas WxH (default 1280x800) in the --format; 0 on a bad size or no memory. */
static int desk_make_canvas(desk* d, const char* size) {
    unsigned w = 1280u, h = 800u;
    uint32_t pal[256];
    void* pixels;
    uint32_t format = d->budget.format == SEL_UIM_FMT_INDEX8 ? SEL_GUI_INDEX8 : SEL_GUI_RGBA32;
    if (size != NULL && (sscanf(size, "%ux%u", &w, &h) != 2 || w < 64u || h < 64u || w > 4096u || h > 4096u)) {
        fprintf(stderr, "selecta: desktop canvas '%s' is not WxH between 64x64 and 4096x4096\n", size);
        return 0;
    }
    d->canvas = (sel_gui_canvas*)d->host->mem_alloc(d->host, sizeof *d->canvas, SEL_MEM_FAST);
    pixels = d->host->mem_alloc(d->host, sel_gui_pixel_bytes(format, (int32_t)w, (int32_t)h), SEL_MEM_FAST);
    if (d->canvas == NULL || pixels == NULL) {
        if (d->canvas != NULL) {
            d->host->mem_free(d->host, d->canvas);
            d->canvas = NULL;
        }
        if (pixels != NULL) {
            d->host->mem_free(d->host, pixels);
        }
        return 0;
    }
    sel_gui_canvas_init(d->canvas, format, (int32_t)w, (int32_t)h, pixels);
    if (format == SEL_GUI_INDEX8) {
        sel_gui_palette_default(pal, d->theme.token[SEL_GUI_TOKEN_ACCENT], d->theme.token[SEL_GUI_TOKEN_SURFACE],
                                d->theme.token[SEL_GUI_TOKEN_TEXT]);
        sel_gui_set_palette(d->canvas, pal, 0u, 256u);
    }
    if (sel_uim_budget_allows(&d->budget, SEL_UIM_FX_SHADOW)) {
        sel_gui_shadow_bake(&d->shadow, 8u, 0x000000u, 96u);
        d->shadow_ready = 1;
    }
    sel_uim_layout_compute(&d->layout, w, h);
    return 1;
}

static void* desk_open(const sel_ui_open_args* a) {
    const char* backend = desk_opt(a, "--backend");
    desk* d;
    int i;
    if (backend != NULL && strcmp(backend, "mem") != 0) {
        fprintf(stderr, "selecta: desktop backend '%s' is not available in this build\n", backend);
        return NULL;
    }
    d = (desk*)a->host->mem_alloc(a->host, sizeof *d, SEL_MEM_FAST);
    if (d == NULL) {
        return NULL;
    }
    memset(d, 0, sizeof *d);
    d->engine = a->engine;
    d->host = a->host;
    d->hash = 2166136261u;
    if (!sel_uim_budget_for(&d->budget, desk_opt(a, "--format"))) {
        fprintf(stderr, "selecta: desktop format '%s' is not rgba32 or index8\n", desk_opt(a, "--format"));
        a->host->mem_free(a->host, d);
        return NULL;
    }
    txt_theme_default(&d->theme);
    if (desk_opt(a, "--theme") != NULL && !desk_load_theme(d, desk_opt(a, "--theme"))) {
        a->host->mem_free(a->host, d);
        return NULL;
    }
    if (desk_opt(a, "--view") != NULL) {
        if (strcmp(desk_opt(a, "--view"), "testcard") != 0) {
            fprintf(stderr, "selecta: desktop view '%s' is not testcard\n", desk_opt(a, "--view"));
            a->host->mem_free(a->host, d);
            return NULL;
        }
        d->testcard = 1;
    }
    if (!desk_make_canvas(d, desk_opt(a, "--canvas"))) {
        desk_close(d);
        return NULL;
    }
    for (i = 0; i < a->argc; i++) {
        if (strcmp(a->argv[i], "--tokens") == 0) {
            d->tokens = 1;
        }
        if (strcmp(a->argv[i], "--stats") == 0) {
            d->report = 1;
        }
        if (strcmp(a->argv[i], "--resize") == 0 && i + 1 < a->argc) {
            d->resize = a->argv[i + 1];
        }
        if (strcmp(a->argv[i], "--ticks") == 0 && i + 1 < a->argc) {
            d->ticks = (uint32_t)atoi(a->argv[i + 1]);
        }
        if (strcmp(a->argv[i], "--actions") == 0) {
            d->actions = 1;
        }
    }
    sel_uim_init(&d->uim);
    sel_uim_update(&d->uim, sel_engine_state(d->engine));
    if (d->testcard) {
        draw_testcard(d);
        d->dirty_pixels = sel_gui_present(d->canvas);
        d->dirty_total = d->dirty_pixels;
    } else {
        desk_present(d);
    }
    return d;
}

/* The snapshot is refreshed at the end of a pump, after the events, so the model reads the state the previous
 * pump left: an event here and the wait below together never miss a change. */
static void desk_on_event(void* state, const sel_message* evt) {
    desk* d = (desk*)state;
    if (evt->id == SEL_EVT_PREFS_CHANGED && strcmp(evt->args[0].v.s, "ui.desktop.mode") == 0) {
        sel_uim_set_layout(&d->uim, evt->args[1].v.s);
    }
    sel_uim_update(&d->uim, sel_engine_state(d->engine));
}

/* The mem backend has no input source: a turn with something to draw is handled, a quiet turn ends the run. */
static int desk_wait(void* state, uint32_t timeout_ms) {
    desk* d = (desk*)state;
    (void)timeout_ms;
    if (d->testcard) {
        return -1; /* the test card is one frame */
    }
    sel_uim_update(&d->uim, sel_engine_state(d->engine));
    if (d->ticks > 0u && d->uim.dirty == 0u) { /* --ticks N: the mem backend has no clock, so it replays N seconds */
        d->ticks--;
        d->uim.dirty |= SEL_UIM_DIRTY_PROGRESS;
    }
    return desk_present(d) ? 1 : -1;
}

static const sel_tag* desk_stats(void* state) {
    desk* d = (desk*)state;
    d->stats[0].tag = SEL_UI_STAT_FRAMES;
    d->stats[0].value = d->frames;
    d->stats[1].tag = SEL_UI_STAT_HASH;
    d->stats[1].value = d->hash;
    d->stats[2].tag = SEL_UI_STAT_WIDGET_DRAWS;
    d->stats[2].value = d->widget_draws;
    d->stats[3].tag = SEL_UI_STAT_UIM_UPDATES;
    d->stats[3].value = d->uim.updates;
    d->stats[4].tag = SEL_UI_STAT_UIM_DIRTY;
    d->stats[4].value = d->uim.last_taken;
    d->stats[5].tag = SEL_UI_STAT_UIM_LAYOUT;
    d->stats[5].value = d->uim.layout;
    d->stats[6].tag = SEL_UI_STAT_TWEENS;
    d->stats[6].value = d->tweens;
    d->stats[7].tag = SEL_UI_STAT_EFFECT_PRIMS;
    d->stats[7].value = d->effect_prims;
    d->stats[8].tag = SEL_UI_STAT_DIRTY_PIXELS;
    d->stats[8].value = d->dirty_pixels;
    d->stats[9].tag = SEL_UI_STAT_DIRTY_TOTAL;
    d->stats[9].value = d->dirty_total;
    d->stats[10].tag = SEL_UI_STAT_CANVAS_HASH;
    d->stats[10].value = sel_gui_hash(d->canvas);
    d->stats[11].tag = SEL_UI_STAT_DRAW_PRIMS;
    d->stats[11].value = d->canvas->prims;
    d->stats[12].tag = SEL_TAG_END;
    d->stats[12].value = 0;
    return d->stats;
}

static sel_tag g_hooks[8];
static sel_tag g_desc[5];

static void set(sel_tag* t, uint32_t tag, uintptr_t value) {
    t->tag = tag;
    t->value = value;
}

const sel_tag* SEL_PLUGIN_ENTRY(uint32_t host_api) {
    if (host_api < 1u) {
        return NULL;
    }
    set(&g_hooks[0], SEL_UI_NAME, (uintptr_t) "Desktop");
    set(&g_hooks[1], SEL_UI_OPEN, SEL_TAG_FN(desk_open));
    set(&g_hooks[2], SEL_UI_ON_EVENT, SEL_TAG_FN(desk_on_event));
    set(&g_hooks[3], SEL_UI_WAIT, SEL_TAG_FN(desk_wait));
    set(&g_hooks[4], SEL_UI_CLOSE, SEL_TAG_FN(desk_close));
    set(&g_hooks[5], SEL_UI_STATS, SEL_TAG_FN(desk_stats));
    set(&g_hooks[6], SEL_UI_VIEW_STYLES, 1);
    set(&g_hooks[7], SEL_TAG_END, 0);

    set(&g_desc[0], SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_UI);
    set(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "desktop");
    set(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    set(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    set(&g_desc[4], SEL_TAG_END, 0);
    return g_desc;
}

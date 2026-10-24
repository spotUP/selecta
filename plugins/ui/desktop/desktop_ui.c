/* Desktop UI plugin (M3d, DUI-P1-4). It renders the engine snapshot through the shared UI model
 * (include/selecta/uimodel.h): one widget per region, redrawn only when the region's dirty bit is set. Drawing goes
 * to a backend: `mem` (headless, no SDL, DUI-P2-6) or `sdl` (a window, DUI-P2-7), selected with `--backend`. */
#include <selecta/plugin.h>
#include <selecta/ui.h>
#include <selecta/uimodel.h>

#include "art.h"
#include <selecta/gui/gui_genart.h>
#include "atlas_build.h"
#include <selecta/gui/gui_anim.h>
#include <selecta/gui/gui_draw.h>
#include <selecta/gui/gui_text.h>
#include <selecta/gui/gui_widget.h>
#include <selecta/gui/gui_input.h>
#include <selecta/uimodel/keybind.h>
#include <selecta/uimodel/keys.h>
#include <selecta/uimodel/theme.h>

#ifdef SELECTA_DESKTOP_SDL
#include "backend_sdl.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The text size tokens (plan 2.6) as pixel sizes; each gets one atlas per weight. */
#define DESK_SIZES 5
static const int g_sizes[DESK_SIZES] = {11, 13, 16, 24, 32};
/* The logical icon sizes (DUI-P3-1); at scale k the icon atlas holds each at k times the size (DUI-P2-8). */
#define DESK_ICON_SIZES 4
static const int g_icon_sizes[DESK_ICON_SIZES] = {16, 20, 24, 32};

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
    int textsheet;          /* --view text: specimen lines and the queue names, once (DUI-P2-3) */
    int widgets;            /* --view widgets: the widget gallery, once (DUI-P3-1) */
    sel_gui_widget_stats wstats; /* per-kind draw counts of the gallery */
    int home;               /* --view home: the tile grid of the queue entries in the main region (DUI-P2-6) */
    sel_gui_text_stats text; /* glyphs blitted and code points the atlas lacked (DUI-P2-3) */
    sel_gui_atlas* atlas[2][DESK_SIZES]; /* [semibold][size], built on first use */
    uint32_t view_opens;    /* tiles opened by input (DUI-P2-6) */
    int32_t open_tile;      /* 1-based queue index of the opened tile, 0 = home */
    const char* frame_dump; /* --frame-dump <file>: the canvas as PPM at close */
    char* input;            /* --input <file>: scripted clicks, replayed once on the first quiet turn */
    int input_done;
    desk_art* art;          /* tile artwork from the library, NULL without one (DUI-P2-4) */
    uint32_t art_tile_hash; /* pixels of the first home tile, STAT_ART_TILE_HASH (DUI-P2-5) */
#ifdef SELECTA_DESKTOP_SDL
    desk_sdl* sdl;          /* --backend sdl: the window the canvas is presented in (DUI-P2-7) */
#endif
    sel_gui_timeline tl;    /* the animation timeline (DUI-P3-5): at most 16 tweens, Q16 values, ms */
    uint32_t now_ms;        /* the UI clock: virtual in the mem backend (--input `wait N`), SDL ticks when live */
    int32_t hover_tile;     /* 1-based tile under the pointer, 0 = none */
    int live;               /* sdl on a real display without --input: the window stays until it is closed */
    int repaint;            /* the next present paints the whole canvas (first frame, resize) */
    int32_t scale;          /* integer pixel scale 1, 2 or 3 (DUI-P2-8): layout is in logical units, drawn times scale */
    uint32_t atlas_builds;   /* text atlases built */
    uint32_t atlas_rebuilds; /* scale changes that dropped the built text atlases */
    uint32_t icon_rebuilds;  /* scale changes that rebuilt the icon atlas view */
    sel_gui_icons icons;     /* the icon atlas at the current scale: the baked masks of size k * logical */
    int32_t icon_px[DESK_ICON_SIZES];
    const uint8_t* icon_alpha[DESK_ICON_SIZES];
    txt_bindings binds;      /* the keys of the action table (DUI-P3-3) */
    uint32_t keys_in, actions_fired, actions_distinct, focus_moves, searches, arrows;
    uint32_t fired_seen[4];  /* one bit per action row that fired */
    int32_t focus;           /* focus stop: layout regions in order, then the search field; -1 = none */
    char query[64];          /* the type-to-search text */
    size_t qlen;
    int songs;               /* --view songs: the queue as a sortable list with a header row (DUI-P3-2) */
    uint16_t order[512];     /* display order of the list: queue indices */
    uint32_t order_n;
    uint32_t sort_col;       /* 0 title, 1 format */
    uint32_t sort_dir;       /* 0 queue order, 1 ascending, 2 descending */
    int32_t hover_row;       /* 1-based display row under the pointer, 0 = none */
    int32_t menu_row;        /* 1-based display row of the open context menu, 0 = closed */
    int32_t menu_x, menu_y;  /* canvas pixels, top left of the menu */
    uint32_t hovers, clicks, double_clicks, drags, sorts, menus, row_plays;
    int32_t tip_btn;         /* 1-based transport button under the pointer, 0 = none (DUI-P3-6) */
    int32_t tip_shown;       /* 1-based button whose tooltip is visible, 0 = none */
    uint32_t tip_since;      /* UI clock when the pointer came to tip_btn */
    uint32_t tooltips;       /* tooltips shown over the run */
    char tip_text[96];
    uint32_t gestures;       /* pinch and swipe events handled (DUI-P3-4) */
    uint32_t tile_step;      /* index into g_tile_steps: the tile size token step, pinch moves it */
    int32_t scroll_q16;      /* songs list scroll offset in logical pixels, Q16: precise wheel deltas are fractions of a pixel */
    int32_t hist[16];        /* view history (DUI-P3-4): the open_tile of each visited view, 0 = home */
    uint32_t hist_n, hist_pos;
    sel_tag stats[46];
} desk;

static int desk_resize(desk* d, int32_t w, int32_t h);
static int desk_set_scale(desk* d, int32_t scale);
static int desk_present(desk* d);
static void desk_draw_focus(desk* d);
static void desk_draw_tip(desk* d);

/* Hands the dirty rectangles to the backend, then empties the list; returns the pixels repainted. The sdl backend
 * must see the list before sel_gui_present clears it. */
static uint32_t desk_flush(desk* d) {
#ifdef SELECTA_DESKTOP_SDL
    if (d->sdl != NULL) {
        desk_sdl_present(d->sdl, d->canvas, d->report);
    }
#endif
    return sel_gui_present(d->canvas);
}

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

/* A logical length in canvas pixels. */
static int32_t lpx(const desk* d, int32_t logical) {
    return logical * d->scale;
}

static sel_gui_rect region_rect(const desk* d, const char* name) {
    uint32_t i;
    for (i = 0; i < d->layout.count; i++) {
        const sel_uim_region* r = &d->layout.regions[i];
        if (strcmp(r->name, name) == 0) {
            return rect_of(lpx(d, r->x), lpx(d, r->y), lpx(d, (int32_t)r->w), lpx(d, (int32_t)r->h));
        }
    }
    return rect_of(0, 0, 0, 0);
}

/* The atlas for a weight and a size token (pixel size), built from the embedded Inter on first use; NULL when
 * the size is not a token or memory ran out. */
static const sel_gui_atlas* desk_font(desk* d, int semibold, int size) {
    int i;
    for (i = 0; i < DESK_SIZES; i++) {
        if (g_sizes[i] == size) {
            if (d->atlas[semibold][i] == NULL) {
                d->atlas[semibold][i] =
                    desk_atlas_build(semibold ? sel_font_inter_semibold : sel_font_inter_regular, size * d->scale);
                d->atlas_builds++;
            }
            return d->atlas[semibold][i];
        }
    }
    return NULL;
}

/* Blend of two 0xAARRGGBB colours, q16 = 0 gives a, 65536 gives b. */
static uint32_t desk_mix(uint32_t a, uint32_t b, int32_t q16) {
    uint32_t out = 0xFF000000u;
    int sh;
    for (sh = 0; sh < 24; sh += 8) {
        int64_t ca = (a >> sh) & 0xFFu, cb = (b >> sh) & 0xFFu;
        out |= (uint32_t)(ca + (((cb - ca) * q16) >> 16)) << sh;
    }
    return out;
}

/* One line of text at the top left of the box, ellipsized to the box width. */
static void draw_text(desk* d, sel_gui_rect box, const char* s, int semibold, int px, uint32_t argb) {
    const sel_gui_atlas* at = desk_font(d, semibold, px);
    if (at != NULL) {
        sel_gui_clip_push(d->canvas, box);
        sel_gui_text_draw(d->canvas, at, box.x, box.y + (box.h - at->line_height) / 2, s, argb, box.w, &d->text);
        sel_gui_clip_pop(d->canvas);
    }
}

/* Fill the box with the surface colour, then draw the text (regular 13 pixel Inter, ellipsized), and mark the box
 * dirty. */
static void draw_label(desk* d, sel_gui_rect box, const char* s) {
    sel_gui_fill_rect(d->canvas, box, ARGB(d->theme.token[SEL_GUI_TOKEN_SURFACE]));
    draw_text(d, box, s, 0, 13, ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT]));
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
static void queue_title(const sel_state* st, uint32_t i, char* out, size_t cap);
static void draw_home(desk* d);

static void draw_widget(desk* d, uint32_t bit) {
    const sel_uim_frame* f = &d->uim.frame;
    int32_t sp = lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_SPACING]);
    sel_gui_rect pl = region_rect(d, "player"), mn = region_rect(d, "main"), sb = region_rect(d, "sidebar");
    int32_t y0 = pl.y + sp;
    char buf[32];
    if (bit == SEL_UIM_DIRTY_SONG) {
        sel_gui_rect card = rect_of(mn.x + sp, mn.y + sp, mn.w - 2 * sp < lpx(d, 400) ? mn.w - 2 * sp : lpx(d, 400), lpx(d, 48));
        if (d->shadow_ready) {
            sel_gui_shadow_draw(d->canvas, &d->shadow, card);
            sel_gui_dirty_add(d->canvas, rect_of(card.x - lpx(d, 8), card.y - lpx(d, 8), card.w + lpx(d, 16), card.h + lpx(d, 16)));
        }
        sel_gui_round_rect(d->canvas, card, lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_RADIUS]),
                           ARGB(d->theme.token[SEL_GUI_TOKEN_SURFACE]));
        draw_label(d, rect_of(card.x + sp, card.y + sp, card.w - 2 * sp, lpx(d, 16)), f->title);
        draw_label(d, rect_of(card.x + sp, card.y + sp + lpx(d, 20), card.w - 2 * sp, lpx(d, 16)), f->artist);
        sel_gui_dirty_add(d->canvas, card);
    } else if (bit == SEL_UIM_DIRTY_QUEUE) {
        sprintf(buf, "%u", (unsigned)f->queue_count);
        draw_label(d, rect_of(sb.x + sp, sb.y + sp, lpx(d, 120), lpx(d, 16)), buf);
        if (d->home) {
            draw_home(d);
        }
    } else if (bit == SEL_UIM_DIRTY_TRANSPORT) {
        sel_gui_rect b = rect_of(pl.x + pl.w / 2 - lpx(d, 16), y0 + lpx(d, 24), lpx(d, 32), lpx(d, 32));
        sel_gui_round_rect(d->canvas, b, lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_RADIUS]),
                           ARGB(d->theme.token[SEL_GUI_TOKEN_ACCENT]));
        sel_gui_dirty_add(d->canvas, b);
    } else if (bit == SEL_UIM_DIRTY_PROGRESS) {
        draw_label(d, rect_of(pl.x + sp, y0, lpx(d, 56), lpx(d, 16)), f->clock);
        draw_label(d, rect_of(pl.x + pl.w - sp - lpx(d, 56), y0, lpx(d, 56), lpx(d, 16)), f->remaining);
        draw_bar(d, rect_of(pl.x + 2 * sp + lpx(d, 56), y0 + lpx(d, 6), lpx(d, 480), lpx(d, 4)), f->progress_q16);
    } else if (bit == SEL_UIM_DIRTY_VOLUME) {
        int32_t v = f->volume < 0 ? 0 : f->volume > 100 ? 100 : f->volume;
        draw_bar(d, rect_of(pl.x + pl.w - sp - lpx(d, 96), y0 + lpx(d, 36), lpx(d, 96), lpx(d, 8)), (uint32_t)v * 65536u / 100u);
    } else if (bit == SEL_UIM_DIRTY_STATUS) {
        draw_label(d, rect_of(pl.x + sp, y0 + lpx(d, 30), lpx(d, 200), lpx(d, 16)), f->status);
    }
}

/* The display title of queue entry i: the file name without directory and extension, UTF-8 as stored. */
static void queue_title(const sel_state* st, uint32_t i, char* out, size_t cap) {
    const char* path = st->queue_text + st->queue[i].path_offset;
    const char* base = path;
    const char* p;
    size_t n;
    for (p = path; *p != '\0'; p++) {
        if (*p == '/' || *p == ':') {
            base = p + 1;
        }
    }
    n = strlen(base);
    for (p = base + n; p > base; p--) {
        if (p[-1] == '.') {
            n = (size_t)(p - 1 - base);
            break;
        }
    }
    if (n >= cap) {
        n = cap - 1u;
        while (n > 0u && (((unsigned char)base[n]) & 0xC0u) == 0x80u) { /* never cut a UTF-8 sequence */
            n--;
        }
    }
    memcpy(out, base, n);
    out[n] = '\0';
}

/* The text sheet (DUI-P2-3): every size in both weights, then each queue name once ellipsized to one narrow line and
 * once wrapped into three lines, so the golden hash covers glyphs, kerning, ellipsis and wrap. */
static void draw_textsheet(desk* d) {
    sel_gui_canvas* c = d->canvas;
    const sel_state* st = sel_engine_state(d->engine);
    uint32_t text = ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT]);
    int32_t y = 8, row;
    uint32_t i;
    int k, w;
    char name[256];
    sel_gui_fill_rect(c, rect_of(0, 0, c->w, c->h), ARGB(d->theme.token[SEL_GUI_TOKEN_SURFACE]));
    for (k = 0; k < DESK_SIZES; k++) {
        for (w = 0; w < 2; w++) {
            const sel_gui_atlas* at = desk_font(d, w, g_sizes[k]);
            if (at != NULL) {
                sel_gui_text_draw(c, at, 8, y, "Selecta: the quick brown fox jumps over 0123456789", text, 500, &d->text);
                y += at->line_height + 2;
            }
        }
    }
    for (i = 0, row = 0; i < st->queue_count && i < 12u; i++, row++) {
        const sel_gui_atlas* semi = desk_font(d, 1, 13);
        const sel_gui_atlas* reg = desk_font(d, 0, 13);
        queue_title(st, i, name, sizeof name);
        if (semi != NULL && reg != NULL) {
            sel_gui_text_draw(c, semi, 520, 8 + row * 60, name, text, 200, &d->text);
            sel_gui_text_wrap(c, reg, rect_of(740, 8 + row * 60, 140, 3 * reg->line_height), name, text, 3, &d->text);
        }
    }
    sel_gui_dirty_add(c, rect_of(0, 0, c->w, c->h));
}

/* The widget gallery (DUI-P3-1): every widget kind drawn once on the background token, in a grid of surface cards.
 * The style comes from the theme tokens, so the golden hash covers a dark and a light theme. */
static void gallery_put_ex(desk* d, const sel_gui_style* st, uint32_t kind, int32_t x, int32_t y, int32_t w, int32_t h,
                           uint32_t state, const char* label, int32_t icon, int32_t value, const char* const* items,
                           uint32_t item_n, int32_t sel, const int32_t* icons) {
    sel_gui_widget wd;
    memset(&wd, 0, sizeof wd);
    wd.kind = kind;
    wd.r = rect_of(x, y, w, h);
    wd.state = state;
    wd.label = label;
    wd.icon = icon;
    wd.value = value;
    wd.extent = SEL_GUI_Q16_ONE / 4;
    wd.items = items;
    wd.item_n = item_n;
    wd.sel = sel;
    wd.item_icons = icons;
    wd.top = 1;
    sel_gui_widget_draw(d->canvas, st, &wd, &d->wstats);
    d->widget_draws++;
}

static void gallery_put(desk* d, const sel_gui_style* st, uint32_t kind, int32_t x, int32_t y, int32_t w, int32_t h,
                        uint32_t state, const char* label, int32_t icon, int32_t value, const char* const* items,
                        uint32_t item_n, int32_t sel) {
    gallery_put_ex(d, st, kind, x, y, w, h, state, label, icon, value, items, item_n, sel, NULL);
}

static void draw_widgets(desk* d) {
    static const char* const songs[] = {"Axel F", "Cannon Fodder", "Stardust Memories", "Hybris", "Elysium"};
    static const char* const tabs[] = {"Songs", "Albums", "Artists"};
    static const char* const head[] = {"Title", "Artist", "Time"};
    static const char* const side[] = {"Home", "Search", "Library", "Favourites"};
    static const int32_t side_icons[] = {SEL_GUI_ICON_HOUSE, SEL_GUI_ICON_SEARCH, SEL_GUI_ICON_LIST, SEL_GUI_ICON_HEART};
    static const char* const panes[] = {"Library", "Details"};
    static const char* const ask[] = {"This removes the song from your library. The file stays on disk.", "Cancel", "Remove"};
    static const char* const menu[] = {"Play next", "Add to queue", "-", "Show in library"};
    static const char* const seg[] = {"Day", "Week", "Month"};
    static const char* const tiles[] = {"Amiga Classics", "Chip Hits", "Tracker Night", "Deep Focus"};
    sel_gui_canvas* c = d->canvas;
    sel_gui_style st;
    const int32_t half = SEL_GUI_Q16_ONE / 2, c1 = 20, c2 = 330, c3 = 640, c4 = 950, cw = 300;
    const uint32_t hov = SEL_GUI_WS_HOVER, sl = SEL_GUI_WS_SELECTED;
    int32_t cardr = (int32_t)d->theme.token[SEL_GUI_TOKEN_RADIUS_LARGE];
    int32_t i;
    memset(&st, 0, sizeof st);
    st.background = ARGB(d->theme.token[SEL_GUI_TOKEN_BACKGROUND]);
    st.surface = ARGB(d->theme.token[SEL_GUI_TOKEN_SURFACE]);
    st.raised = ARGB(d->theme.token[SEL_GUI_TOKEN_SURFACE_RAISED]);
    st.hover = ARGB(d->theme.token[SEL_GUI_TOKEN_SURFACE_HOVER]);
    st.pressed = ARGB(d->theme.token[SEL_GUI_TOKEN_SURFACE_PRESSED]);
    st.text = ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT]);
    st.muted = ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT_MUTED]);
    st.disabled = ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT_DISABLED]);
    st.accent = ARGB(d->theme.token[SEL_GUI_TOKEN_ACCENT]);
    st.accent_text = ARGB(d->theme.token[SEL_GUI_TOKEN_ACCENT_TEXT]);
    st.divider = ARGB(d->theme.token[SEL_GUI_TOKEN_DIVIDER]);
    st.focus = ARGB(d->theme.token[SEL_GUI_TOKEN_FOCUS_RING]);
    st.error = ARGB(d->theme.token[SEL_GUI_TOKEN_ERROR]);
    st.warning = ARGB(d->theme.token[SEL_GUI_TOKEN_WARNING]);
    st.success = ARGB(d->theme.token[SEL_GUI_TOKEN_SUCCESS]);
    st.scrim = ARGB(d->theme.token[SEL_GUI_TOKEN_SCRIM]);
    st.radius_small = (int32_t)d->theme.token[SEL_GUI_TOKEN_RADIUS_SMALL];
    st.radius = (int32_t)d->theme.token[SEL_GUI_TOKEN_RADIUS];
    st.radius_large = cardr;
    st.spacing = (int32_t)d->theme.token[SEL_GUI_TOKEN_SPACING];
    st.body = desk_font(d, 0, 13);
    st.semibold = desk_font(d, 1, 13);
    st.caption = desk_font(d, 0, 11);
    st.icons = &d->icons;
    st.shadow = d->shadow_ready ? &d->shadow : NULL;
    if (st.body == NULL || st.semibold == NULL || st.caption == NULL) {
        return;
    }
    sel_gui_fill_rect(c, rect_of(0, 0, c->w, c->h), st.background);
    for (i = 0; i < 4; i++) { /* the four column cards */
        sel_gui_round_rect(c, rect_of(10 + i * 310, 10, cw + 20, c->h - 20), cardr, st.surface);
    }
    /* column 1: controls */
    gallery_put(d, &st, SEL_GUI_WIDGET_LABEL, c1, 24, cw, 22, 0, "Now playing", -1, 0, NULL, 0, 0);
    gallery_put(d, &st, SEL_GUI_WIDGET_ICON, c1, 60, 24, 24, 0, NULL, SEL_GUI_ICON_MUSIC, 0, NULL, 0, 0);
    gallery_put(d, &st, SEL_GUI_WIDGET_ICON_BUTTON, c1 + 40, 52, 40, 40, sl, NULL, SEL_GUI_ICON_PLAY, 0, NULL, 0, 0);
    gallery_put(d, &st, SEL_GUI_WIDGET_ICON_BUTTON, c1 + 90, 52, 40, 40, hov, NULL, SEL_GUI_ICON_SHUFFLE, 0, NULL, 0, 0);
    gallery_put(d, &st, SEL_GUI_WIDGET_ICON_BUTTON, c1 + 140, 52, 40, 40, SEL_GUI_WS_DISABLED, NULL, SEL_GUI_ICON_REPEAT, 0,
                NULL, 0, 0);
    gallery_put(d, &st, SEL_GUI_WIDGET_TEXT_BUTTON, c1, 108, 130, 36, sl, "Play", -1, 0, NULL, 0, 0);
    gallery_put(d, &st, SEL_GUI_WIDGET_TEXT_BUTTON, c1 + 140, 108, 130, 36, hov, "Follow", -1, 0, NULL, 0, 0);
    gallery_put(d, &st, SEL_GUI_WIDGET_TOGGLE, c1, 160, 44, 24, sl, NULL, -1, 0, NULL, 0, 0);
    gallery_put(d, &st, SEL_GUI_WIDGET_TOGGLE, c1 + 60, 160, 44, 24, 0, NULL, -1, 0, NULL, 0, 0);
    gallery_put(d, &st, SEL_GUI_WIDGET_SLIDER, c1, 200, cw, 20, hov, NULL, -1, SEL_GUI_Q16_ONE * 2 / 5, NULL, 0, 0);
    gallery_put(d, &st, SEL_GUI_WIDGET_PROGRESS, c1, 236, cw, 6, 0, NULL, -1, SEL_GUI_Q16_ONE * 13 / 20, NULL, 0, 0);
    gallery_put(d, &st, SEL_GUI_WIDGET_CHECKBOX, c1, 258, 140, 24, sl, "Shuffle", -1, 0, NULL, 0, 0);
    gallery_put(d, &st, SEL_GUI_WIDGET_RADIO, c1 + 150, 258, 140, 24, sl, "Repeat", -1, 0, NULL, 0, 0);
    gallery_put(d, &st, SEL_GUI_WIDGET_SEGMENTED, c1, 298, cw, 32, 0, NULL, -1, 0, seg, 3, 1);
    gallery_put(d, &st, SEL_GUI_WIDGET_DROPDOWN, c1, 346, cw, 36, 0, "Sort by recent", -1, 0, NULL, 0, 0);
    gallery_put(d, &st, SEL_GUI_WIDGET_TEXT_FIELD, c1, 398, cw, 36, SEL_GUI_WS_FOCUS, "Search", SEL_GUI_ICON_SEARCH, 0,
                NULL, 0, 0);
    gallery_put(d, &st, SEL_GUI_WIDGET_TABS, c1, 450, cw, 36, 0, NULL, -1, 0, tabs, 3, 0);
    gallery_put(d, &st, SEL_GUI_WIDGET_TABLE_HEADER, c1, 502, cw, 28, 0, NULL, -1, 0, head, 3, 0);
    gallery_put(d, &st, SEL_GUI_WIDGET_SPLIT_PANE, c1, 550, cw - 24, 200, 0, NULL, -1, half, panes, 2, 0);
    gallery_put(d, &st, SEL_GUI_WIDGET_SCROLL_BAR, c1 + cw - 12, 550, 12, 200, 0, NULL, -1, half, NULL, 0, 0);
    /* column 2: lists and menus */
    gallery_put(d, &st, SEL_GUI_WIDGET_VIRTUAL_LIST, c2, 24, cw, 200, 0, NULL, -1, 0, songs, 5, 1);
    gallery_put_ex(d, &st, SEL_GUI_WIDGET_SIDEBAR_LIST, c2, 244, cw, 200, 0, NULL, SEL_GUI_ICON_HOUSE, 0, side, 4, 0,
                   side_icons);
    gallery_put(d, &st, SEL_GUI_WIDGET_MENU, c2, 464, cw, 140, 0, NULL, -1, 0, menu, 4, 1);
    gallery_put(d, &st, SEL_GUI_WIDGET_TOOLTIP, c2, 630, 140, 28, 0, "Add to favourites", -1, 0, NULL, 0, 0);
    gallery_put(d, &st, SEL_GUI_WIDGET_TOAST, c2, 680, cw, 56, 0, "Added to queue", -1, 0, NULL, 0, 0);
    /* column 3: artwork */
    gallery_put(d, &st, SEL_GUI_WIDGET_TILE_GRID, c3, 24, cw, 270, 0, "Playlist", -1, 0, tiles, 2, 1);
    gallery_put(d, &st, SEL_GUI_WIDGET_COVER_FLOW, c3, 314, cw, 190, 0, NULL, -1, 0, NULL, 0, 3);
    gallery_put(d, &st, SEL_GUI_WIDGET_ARTWORK, c3, 524, 130, 130, 0, NULL, SEL_GUI_ICON_DISC, 0, NULL, 0, 0);
    gallery_put(d, &st, SEL_GUI_WIDGET_VIZ_SURFACE, c3 + 150, 524, 150, 130, 0, NULL, -1, 7 << 16, NULL, 0, 0);
    /* column 4: an overlay stack, a scrim and a dialog over a tile grid */
    gallery_put(d, &st, SEL_GUI_WIDGET_TILE_GRID, c4, 24, cw, 270, 0, "Playlist", -1, 0, tiles, 2, 0);
    gallery_put(d, &st, SEL_GUI_WIDGET_SCRIM, 0, 0, c->w, c->h, 0, NULL, -1, SEL_GUI_Q16_ONE * 3 / 5, NULL, 0, 0);
    gallery_put(d, &st, SEL_GUI_WIDGET_MODAL_DIALOG, (c->w - 280) / 2, (c->h - 190) / 2, 280, 190, 0, "Remove from library",
                -1, 0, ask, 3, 0);
    d->text.glyphs += d->wstats.text.glyphs;
    d->text.missing += d->wstats.text.missing;
    d->wstats.text.glyphs = 0;
    d->wstats.text.missing = 0;
    sel_gui_dirty_add(c, rect_of(0, 0, c->w, c->h));
}

#define TILE_ART 144 /* logical, the default step */
#define TILE_STEP_DEFAULT 2u
#define SCROLL_NOTCH 16 /* logical pixels one wheel notch scrolls */
static const int32_t g_tile_steps[] = {96, 120, 144, 176, 208}; /* the tile size token steps, logical pixels */
#define TILE_STEPS ((uint32_t)(sizeof g_tile_steps / sizeof g_tile_steps[0]))
#define HOME_MAX_LINES 2

/* Where tile i sits (DUI-P2-6): a grid below the song card in the main region, columns as many as fit. */
static sel_gui_rect tile_rect(desk* d, uint32_t i) {
    int32_t sp = lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_SPACING]);
    sel_gui_rect mn = region_rect(d, "main");
    const sel_gui_atlas* at = desk_font(d, 1, 13);
    int32_t title_h = HOME_MAX_LINES * (at != NULL ? at->line_height : 16);
    int32_t art = lpx(d, g_tile_steps[d->tile_step]);
    int32_t cols = (mn.w - sp) / (art + sp);
    int32_t h = art + lpx(d, 4) + title_h;
    if (cols < 1) {
        cols = 1;
    }
    return rect_of(mn.x + sp + (int32_t)(i % (uint32_t)cols) * (art + sp),
                   mn.y + sp + lpx(d, 48) + 2 * sp + (int32_t)(i / (uint32_t)cols) * (h + sp), art, h);
}

/* The detail view of an opened tile: the main region cleared, the title large. */
static int32_t songs_top(const desk* d);
static void draw_detail(desk* d) {
    const sel_state* st = sel_engine_state(d->engine);
    int32_t sp = lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_SPACING]);
    sel_gui_rect mn = region_rect(d, "main");
    const sel_gui_atlas* big = desk_font(d, 1, 24);
    char name[256];
    /* Below the now-playing card strip, like the home and songs views: the card is a widget with its own dirty bit,
     * so a view that wipes it leaves a hole on the way back (I245). */
    sel_gui_rect body = rect_of(mn.x, songs_top(d), mn.w, mn.y + mn.h - songs_top(d));
    sel_gui_fill_rect(d->canvas, body, ARGB(d->theme.token[SEL_GUI_TOKEN_SURFACE]));
    if (big != NULL && d->open_tile >= 1 && (uint32_t)d->open_tile <= st->queue_count) {
        queue_title(st, (uint32_t)d->open_tile - 1u, name, sizeof name);
        sel_gui_clip_push(d->canvas, body);
        sel_gui_text_wrap(d->canvas, big, rect_of(mn.x + sp, body.y, mn.w - 2 * sp, 3 * big->line_height), name,
                          ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT]), 3, &d->text);
        sel_gui_clip_pop(d->canvas);
    }
    sel_gui_dirty_add(d->canvas, body);
}

/* The home view (DUI-P2-6): one tile per queue entry, its album artwork (DUI-P2-4) or else generated art of its
 * format (DUI-P2-5), and the title wrapped under it. The queue is the tile source. */
/* The format of queue entry i: its file extension, "" without one. */
static void queue_format(const sel_state* st, uint32_t i, char* out, size_t cap) {
    const char* path = st->queue_text + st->queue[i].path_offset;
    const char* dot = strrchr(path, '.');
    const char* slash = strrchr(path, '/');
    if (dot == NULL || (slash != NULL && dot < slash) || strlen(dot + 1) >= cap) {
        out[0] = '\0';
        return;
    }
    strcpy(out, dot + 1);
}

/* FNV-1a over the pixels of r. */
static uint32_t rect_hash(const sel_gui_canvas* c, sel_gui_rect r) {
    uint32_t h = 2166136261u;
    int32_t x, y;
    for (y = r.y; y < r.y + r.h; y++) {
        for (x = r.x; x < r.x + r.w; x++) {
            h = (h ^ sel_gui_pixel(c, x, y)) * 16777619u;
        }
    }
    return h;
}

/* Loads the artwork of every queue path on the worker (art.h) before the tiles are drawn. */
static void load_tile_art(desk* d, const sel_state* st) {
    const char** ptrs;
    uint32_t i;
    if (d->art == NULL || st->queue_count == 0u) {
        return;
    }
    ptrs = (const char**)malloc((size_t)st->queue_count * sizeof *ptrs);
    if (ptrs != NULL) {
        for (i = 0; i < st->queue_count; i++) {
            ptrs[i] = st->queue_text + st->queue[i].path_offset;
        }
        art_load(d->art, ptrs, st->queue_count);
    }
    free((void*)ptrs);
}

/* The pref ui.desktop.reduce_motion and the format governor together decide whether a tween may start. */
static int desk_motion(const desk* d) {
    const sel_host* h = sel_engine_plugin_host(d->engine);
    const char* v = h != NULL && h->config_get != NULL ? h->config_get(h, "ui.desktop.reduce_motion", "0") : "0";
    return sel_uim_motion_allowed(&d->budget, v != NULL && strcmp(v, "1") == 0);
}

/* Starts a tween when the governor allows it; returns 1 when it runs. */
static int desk_tween(desk* d, uint32_t key, uint32_t ms, uint32_t ease) {
    if (!desk_motion(d) || !sel_gui_tween_start(&d->tl, key, 0, 65536, ms, ease)) {
        return 0;
    }
    d->tweens++;
    return 1;
}


/* ---- Song list (DUI-P3-2): header row, one row per queue entry, drop targets in the sidebar. All geometry in canvas
 * pixels; a pointer position is mapped to a region of the layout tree first (sel_uim_hit), then to a part of it. ---- */
enum { SONG_HEADER_H = 24, SONG_ROW_H = 32, SONG_TARGET_H = 32, SONG_MENU_ITEM_H = 24, SONG_MENU_W = 140, SONG_MENU_ITEMS = 3 };
static const char* const g_menu_items[SONG_MENU_ITEMS] = {"Play", "Play Next", "Add to Queue"};

static int desk_send(desk* d, const char* line);

static int32_t songs_top(const desk* d) {
    sel_gui_rect mn = region_rect(d, "main");
    int32_t sp = lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_SPACING]);
    return mn.y + sp + lpx(d, 48) + sp;
}

static sel_gui_rect songs_header_col(const desk* d, uint32_t col) {
    sel_gui_rect mn = region_rect(d, "main");
    int32_t split = mn.w * 3 / 4;
    return col == 0u ? rect_of(mn.x, songs_top(d), split, lpx(d, SONG_HEADER_H))
                     : rect_of(mn.x + split, songs_top(d), mn.w - split, lpx(d, SONG_HEADER_H));
}

static sel_gui_rect songs_row_rect(const desk* d, uint32_t row) {
    sel_gui_rect mn = region_rect(d, "main");
    return rect_of(mn.x, songs_top(d) + lpx(d, SONG_HEADER_H) + (int32_t)row * lpx(d, SONG_ROW_H) - (int32_t)(((int64_t)d->scroll_q16 * (d->scale > 0 ? d->scale : 1)) >> 16), mn.w, lpx(d, SONG_ROW_H));
}

/* Drop target 0 = Queue, 1 = Playlist, stacked under the queue count in the sidebar. */
static sel_gui_rect songs_target_rect(const desk* d, uint32_t t) {
    sel_gui_rect sb = region_rect(d, "sidebar");
    int32_t sp = lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_SPACING]);
    return rect_of(sb.x + sp, sb.y + sp + lpx(d, 24) + (int32_t)t * (lpx(d, SONG_TARGET_H) + sp), sb.w - 2 * sp, lpx(d, SONG_TARGET_H));
}

static int in_rect(sel_gui_rect r, int32_t x, int32_t y) {
    return x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h;
}

static void songs_key(const sel_state* st, uint32_t col, uint32_t i, char* out, size_t cap) {
    if (col == 0u) {
        queue_title(st, i, out, cap);
    } else {
        queue_format(st, i, out, cap);
    }
}

/* Rebuilds the display order from the queue: queue order, then a stable insertion sort by the sort column. */
static void songs_resort(desk* d) {
    const sel_state* st = sel_engine_state(d->engine);
    uint32_t n = st->queue_count < 512u ? st->queue_count : 512u, i, j;
    for (i = 0; i < n; i++) {
        d->order[i] = (uint16_t)i;
    }
    d->order_n = n;
    if (d->sort_dir == 0u) {
        return;
    }
    for (i = 1; i < n; i++) {
        uint16_t v = d->order[i];
        char kv[256], kj[256];
        songs_key(st, d->sort_col, v, kv, sizeof kv);
        for (j = i; j > 0u; j--) {
            int c;
            songs_key(st, d->sort_col, d->order[j - 1u], kj, sizeof kj);
            c = strcmp(kj, kv);
            if (d->sort_dir == 2u ? c >= 0 : c <= 0) {
                break;
            }
            d->order[j] = d->order[j - 1u];
        }
        d->order[j] = v;
    }
}

static void draw_songs(desk* d) {
    const sel_state* st = sel_engine_state(d->engine);
    sel_gui_rect mn = region_rect(d, "main");
    const sel_gui_atlas* semi = desk_font(d, 1, 13);
    uint32_t text = ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT]);
    uint32_t i, c;
    int32_t pad = lpx(d, 8);
    if (semi == NULL) {
        return;
    }
    if (d->order_n != st->queue_count) {
        songs_resort(d);
    }
    sel_gui_clip_push(d->canvas, mn);
    sel_gui_fill_rect(d->canvas, rect_of(mn.x, songs_top(d), mn.w, mn.h), ARGB(d->theme.token[SEL_GUI_TOKEN_SURFACE]));
    for (c = 0; c < 2u; c++) {
        static const char* const names[2] = {"Title", "Format"};
        char label[48];
        sel_gui_rect h = songs_header_col(d, c);
        snprintf(label, sizeof label, "%s%s", names[c], d->sort_dir != 0u && d->sort_col == c ? (d->sort_dir == 1u ? " (ascending)" : " (descending)") : "");
        sel_gui_text_wrap(d->canvas, semi, rect_of(h.x + pad, h.y + lpx(d, 4), h.w - 2 * pad, semi->line_height), label,
                          ARGB(d->theme.token[SEL_GUI_TOKEN_ACCENT]), 1, &d->text);
    }
    for (i = 0; i < d->order_n; i++) {
        sel_gui_rect r = songs_row_rect(d, i);
        char name[256], format[16];
        uint32_t q = d->order[i];
        if (r.y >= mn.y + mn.h) {
            break;
        }
        if (r.y < songs_top(d) + lpx(d, SONG_HEADER_H)) { /* scrolled under the header */
            continue;
        }
        if ((int32_t)i + 1 == d->hover_row) {
            sel_gui_scrim(d->canvas, r, 0xFFFFFFu, 36u);
        }
        queue_title(st, q, name, sizeof name);
        queue_format(st, q, format, sizeof format);
        sel_gui_text_wrap(d->canvas, semi, rect_of(r.x + pad, r.y + lpx(d, 8), songs_header_col(d, 0).w - 2 * pad, semi->line_height),
                          name, text, 1, &d->text);
        sel_gui_text_wrap(d->canvas, semi, rect_of(songs_header_col(d, 1).x + pad, r.y + lpx(d, 8), songs_header_col(d, 1).w - 2 * pad, semi->line_height),
                          format, text, 1, &d->text);
    }
    if (d->menu_row > 0) {
        uint32_t k;
        for (k = 0; k < SONG_MENU_ITEMS; k++) {
            sel_gui_rect m = rect_of(d->menu_x, d->menu_y + (int32_t)k * lpx(d, SONG_MENU_ITEM_H), lpx(d, SONG_MENU_W), lpx(d, SONG_MENU_ITEM_H));
            sel_gui_fill_rect(d->canvas, m, ARGB(d->theme.token[SEL_GUI_TOKEN_ACCENT]));
            sel_gui_text_wrap(d->canvas, semi, rect_of(m.x + pad, m.y + lpx(d, 4), m.w - 2 * pad, semi->line_height), g_menu_items[k],
                              text, 1, &d->text);
        }
    }
    sel_gui_clip_pop(d->canvas);
    sel_gui_dirty_add(d->canvas, mn);
}

/* The sidebar drop targets, drawn over the queue count. */
static void draw_songs_targets(desk* d) {
    const sel_gui_atlas* semi = desk_font(d, 1, 13);
    uint32_t t;
    if (semi == NULL) {
        return;
    }
    for (t = 0; t < 2u; t++) {
        sel_gui_rect r = songs_target_rect(d, t);
        sel_gui_fill_rect(d->canvas, r, ARGB(d->theme.token[SEL_GUI_TOKEN_SURFACE]));
        sel_gui_text_wrap(d->canvas, semi, rect_of(r.x + lpx(d, 8), r.y + lpx(d, 8), r.w - lpx(d, 16), semi->line_height),
                          t == 0u ? "Queue" : "Playlist", ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT]), 1, &d->text);
        sel_gui_dirty_add(d->canvas, r);
    }
}

static void draw_home_content(desk* d) {
    const sel_state* st = sel_engine_state(d->engine);
    sel_gui_rect mn = region_rect(d, "main");
    int32_t sp = lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_SPACING]);
    const sel_gui_atlas* semi = desk_font(d, 1, 13);
    uint32_t i;
    char name[256];
    if (d->songs) {
        draw_songs(d);
        draw_songs_targets(d);
        return;
    }
    if (d->open_tile > 0) {
        draw_detail(d);
        return;
    }
    if (semi == NULL) {
        return;
    }
    load_tile_art(d, st);
    sel_gui_clip_push(d->canvas, mn);
    sel_gui_fill_rect(d->canvas, rect_of(mn.x, mn.y + sp + lpx(d, 48) + sp, mn.w, mn.h - (sp + lpx(d, 48) + sp)),
                      ARGB(d->theme.token[SEL_GUI_TOKEN_SURFACE]));
    for (i = 0; i < st->queue_count; i++) {
        sel_gui_rect t = tile_rect(d, i);
        const uint32_t* art;
        queue_title(st, i, name, sizeof name);
        art = art_tile(d->art, st->queue_text + st->queue[i].path_offset);
        if (art != NULL) {
            sel_gui_blit_image(d->canvas, rect_of(t.x, t.y, t.w, t.w), art, DESK_ART_TILE_EDGE, DESK_ART_TILE_EDGE,
                               DESK_ART_TILE_EDGE, 0);
        } else {
            /* No cover: the generated art of the song's format and chip (gui_genart.h). */
            char format[16];
            queue_format(st, i, format, sizeof format);
            sel_gui_genart_draw(d->canvas, rect_of(t.x, t.y, t.w, t.w), format, sel_gui_str_hash(name));
        }
        if (i == 0u) {
            d->art_tile_hash = rect_hash(d->canvas, rect_of(t.x, t.y, t.w, t.w));
        }
        sel_gui_text_wrap(d->canvas, semi, rect_of(t.x, t.y + t.w + lpx(d, 4), t.w, HOME_MAX_LINES * semi->line_height),
                          name, ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT]), HOME_MAX_LINES, &d->text);
    }
    sel_gui_clip_pop(d->canvas);
    sel_gui_dirty_add(d->canvas, mn);
}

enum { ANIM_KEY_VIEW = 1, ANIM_KEY_HOVER = 2, ANIM_KEY_TIP = 3, HOVER_ALPHA = 36 };

/* The main region with the running tweens on top: the hover fade of one tile (a light veil, full strength at rest)
 * and the view cross-fade (the new view fades in from the surface colour). At rest both are the plain view. */
static void draw_home(desk* d) {
    sel_gui_rect mn = region_rect(d, "main");
    int32_t view = sel_gui_tween_value(&d->tl, ANIM_KEY_VIEW, 65536);
    draw_home_content(d);
    if (d->hover_tile > 0 && d->open_tile == 0) {
        int32_t hv = sel_gui_tween_value(&d->tl, ANIM_KEY_HOVER, 65536);
        sel_gui_scrim(d->canvas, tile_rect(d, (uint32_t)d->hover_tile - 1u), 0xFFFFFFu,
                      (uint32_t)(((int64_t)hv * HOVER_ALPHA) >> 16));
    }
    if (view < 65536) {
        sel_gui_scrim(d->canvas, rect_of(mn.x, songs_top(d), mn.w, mn.y + mn.h - songs_top(d)),
                      ARGB(d->theme.token[SEL_GUI_TOKEN_SURFACE]) & 0xFFFFFFu,
                      255u - (uint32_t)(((int64_t)view * 255) >> 16));
    }
}

/* Enters a view (DUI-P3-4): the forward history is dropped and the view is appended; the oldest entry falls off. */
static void view_goto(desk* d, int32_t tile) {
    if (d->hist_n == 0u) {
        d->hist[0] = d->open_tile;
        d->hist_n = 1u;
        d->hist_pos = 0u;
    }
    d->hist_n = d->hist_pos + 1u;
    if (d->hist_n == sizeof d->hist / sizeof d->hist[0]) {
        memmove(d->hist, d->hist + 1, (d->hist_n - 1u) * sizeof d->hist[0]);
        d->hist_n--;
        d->hist_pos--;
    }
    d->hist[d->hist_n++] = tile;
    d->hist_pos = d->hist_n - 1u;
    d->open_tile = tile;
}

/* Swipe (DUI-P3-4): right is Back, left is Forward in the view history. Returns 1 when the view changed. */
static int view_swipe(desk* d, int32_t dir) {
    if (d->hist_n == 0u) {
        return 0;
    }
    if (dir > 0 && d->hist_pos > 0u) {
        d->hist_pos--;
    } else if (dir < 0 && d->hist_pos + 1u < d->hist_n) {
        d->hist_pos++;
    } else {
        return 0;
    }
    d->open_tile = d->hist[d->hist_pos];
    return 1;
}

/* A click at (x, y): opens the tile under it. Returns 1 when a tile opened. */
static int desk_click(desk* d, int32_t x, int32_t y) {
    const sel_state* st = sel_engine_state(d->engine);
    sel_gui_rect mn = region_rect(d, "main");
    uint32_t i;
    if (!d->home || d->songs || d->open_tile > 0 || x < mn.x || y < mn.y || x >= mn.x + mn.w || y >= mn.y + mn.h) {
        return 0;
    }
    for (i = 0; i < st->queue_count; i++) {
        sel_gui_rect t = tile_rect(d, i);
        if (x >= t.x && x < t.x + t.w && y >= t.y && y < t.y + t.h) {
            view_goto(d, (int32_t)i + 1);
            d->view_opens++;
            desk_tween(d, ANIM_KEY_VIEW, SEL_GUI_ANIM_NORMAL_MS, SEL_GUI_EASE_OUT);
            draw_home(d);
            return 1;
        }
    }
    return 0;
}

/* An input event changed the picture: flush it as one frame. */
static int desk_event_frame(desk* d, int changed) {
    if (changed) {
        d->dirty_pixels = desk_flush(d);
        d->dirty_total += d->dirty_pixels;
        d->frames++;
    }
    return changed;
}

/* A frame is drawn only when a dirty bit is set or a tween runs (plan 2.9); this is the idle block. */
static int desk_should_draw(const desk* d, int animated) {
    return d->uim.dirty != 0u || animated;
}

/* ---- Tooltips (DUI-P3-6): a pointer resting 600 ms on a transport button shows the action's tooltip, with its key in
 * brackets; moving off or clicking hides it. The buttons are the actions named here, in this order, centred in the
 * player region ("play" sits where the placeholder play box is drawn). ---- */
enum { TIP_DELAY_MS = 600, TIP_BTN = 32, TIP_GAP = 8, TIP_H = 24 };
static const char* const g_transport[] = {"prev", "play", "next"};
#define TRANSPORT_COUNT ((uint32_t)(sizeof g_transport / sizeof g_transport[0]))

static const sel_uim_action* desk_action_by_id(const char* id) {
    uint32_t i, n = sel_uim_action_count();
    for (i = 0; i < n; i++) {
        const sel_uim_action* a = sel_uim_action_at(i);
        if (strcmp(a->id, id) == 0) {
            return a;
        }
    }
    return NULL;
}

/* Canvas rectangle of transport button i. */
static sel_gui_rect desk_button_rect(const desk* d, uint32_t i) {
    sel_gui_rect pl = region_rect(d, "player");
    int32_t sp = lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_SPACING]);
    int32_t cx = pl.x + pl.w / 2;
    return rect_of(cx - lpx(d, TIP_BTN / 2) + ((int32_t)i - 1) * lpx(d, TIP_BTN + TIP_GAP), pl.y + sp + lpx(d, 24),
                   lpx(d, TIP_BTN), lpx(d, TIP_BTN));
}

/* 1-based transport button under the canvas point, 0 = none. */
static int32_t desk_button_at(const desk* d, int32_t x, int32_t y) {
    uint32_t i;
    for (i = 0; i < TRANSPORT_COUNT; i++) {
        if (in_rect(desk_button_rect(d, i), x, y)) {
            return (int32_t)i + 1;
        }
    }
    return 0;
}

/* The tooltip text of an action: its tooltip field (the label when that is empty), then the default key in brackets
 * the Spotify way: "C-Right" becomes "(Ctrl+Right)", a single letter is upper case, "M-" is "Alt+", "S-" is "Shift+". */
static void desk_tip_text_of(const sel_uim_action* a, char* out, size_t cap) {
    const char* body = a->tooltip != NULL && a->tooltip[0] != '\0' ? a->tooltip : a->label;
    char key[48];
    const char* k = a->key;
    size_t n = 0;
    key[0] = '\0';
    while (k[0] != '\0' && k[1] == '-' && n + 8u < sizeof key) {
        const char* mod = k[0] == 'C' ? "Ctrl+" : k[0] == 'M' ? "Alt+" : k[0] == 'S' ? "Shift+" : NULL;
        if (mod == NULL) {
            break;
        }
        strcpy(key + n, mod);
        n += strlen(mod);
        k += 2;
    }
    snprintf(key + n, sizeof key - n, "%s", k);
    if (k[0] >= 'a' && k[0] <= 'z' && k[1] == '\0') {
        key[n] = (char)(k[0] - 'a' + 'A');
    }
    if (key[0] != '\0') {
        snprintf(out, cap, "%s (%s)", body, key);
    } else {
        snprintf(out, cap, "%s", body);
    }
}

/* The tooltip box above its button; the fade tween moves the box and the text from the surface to their final
 * colours. */
static void desk_draw_tip(desk* d) {
    sel_gui_rect b, box;
    const sel_gui_atlas* at;
    int32_t w, pad, q;
    if (d->tip_shown <= 0) {
        return;
    }
    at = desk_font(d, 0, 13);
    if (at == NULL) {
        return;
    }
    q = sel_gui_tween_value(&d->tl, ANIM_KEY_TIP, 65536);
    b = desk_button_rect(d, (uint32_t)d->tip_shown - 1u);
    pad = lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_SPACING]);
    w = sel_gui_text_width(at, d->tip_text, strlen(d->tip_text)) + 2 * pad;
    box = rect_of(b.x + b.w / 2 - w / 2, b.y - lpx(d, TIP_H + 4), w, lpx(d, TIP_H));
    sel_gui_round_rect(d->canvas, box, lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_RADIUS_SMALL]),
                       desk_mix(d->theme.token[SEL_GUI_TOKEN_SURFACE], d->theme.token[SEL_GUI_TOKEN_SURFACE_PRESSED], q));
    draw_text(d, rect_of(box.x + pad, box.y, w - 2 * pad, box.h), d->tip_text, 0, 13,
              desk_mix(d->theme.token[SEL_GUI_TOKEN_SURFACE_PRESSED], d->theme.token[SEL_GUI_TOKEN_TEXT], q));
    sel_gui_dirty_add(d->canvas, box);
}

/* The pointer moved to canvas (x, y), or clicked (x < 0): a tooltip that is up goes away when the button changes;
 * the delay restarts at the new button. Returns 1 when the picture changed. */
static int desk_tip_pointer(desk* d, int32_t x, int32_t y) {
    int32_t btn = x < 0 ? 0 : desk_button_at(d, x, y);
    int hide = d->tip_shown > 0 && btn != d->tip_shown;
    if (btn != d->tip_btn) {
        d->tip_btn = btn;
        d->tip_since = d->now_ms;
    }
    if (!hide) {
        return 0;
    }
    d->tip_shown = 0;
    d->repaint = 1;
    d->uim.dirty |= SEL_UIM_DIRTY_ALL;
    desk_present(d);
    if (d->home || d->songs) {
        draw_home(d);
        desk_event_frame(d, 1);
    }
    return 1;
}

/* Milliseconds until the pending tooltip is due, 0 when none is pending (or it is due now). */
static uint32_t desk_tip_wait(const desk* d) {
    uint32_t due = d->tip_since + TIP_DELAY_MS;
    return d->tip_btn > 0 && d->tip_shown == 0 && due > d->now_ms ? due - d->now_ms : 0u;
}

/* Shows the tooltip when the pointer has rested long enough. */
static void desk_tip_tick(desk* d) {
    const sel_uim_action* a;
    if (d->tip_btn <= 0 || d->tip_shown > 0 || d->now_ms < d->tip_since + TIP_DELAY_MS) {
        return;
    }
    a = desk_action_by_id(g_transport[d->tip_btn - 1]);
    if (a == NULL) {
        return;
    }
    desk_tip_text_of(a, d->tip_text, sizeof d->tip_text);
    d->tip_shown = d->tip_btn;
    d->tooltips++;
    desk_tween(d, ANIM_KEY_TIP, SEL_GUI_ANIM_FAST_MS, SEL_GUI_EASE_OUT);
    desk_draw_tip(d);
    desk_event_frame(d, 1);
}

/* Moves the UI clock ms forward the way the live loop would: 16 ms steps while a tween runs, otherwise one jump to the
 * next visible-clock second boundary (the idle block). Draws a frame per step that has a reason. */
static void desk_advance(desk* d, uint32_t ms) {
    uint32_t end = d->now_ms + ms;
    while (d->now_ms < end) {
        uint32_t step = end - d->now_ms;
        int animated = sel_gui_timeline_running(&d->tl) > 0u;
        uint32_t tw = desk_tip_wait(d);
        if (animated) {
            if (step > 16u) {
                step = 16u;
            }
        } else if (step > 1000u - d->now_ms % 1000u) {
            step = 1000u - d->now_ms % 1000u;
        }
        if (tw > 0u && step > tw) {
            step = tw;
        }
        d->now_ms += step;
        sel_gui_timeline_advance(&d->tl, d->now_ms);
        sel_uim_update(&d->uim, sel_engine_state(d->engine));
        desk_tip_tick(d);
        if (!desk_should_draw(d, animated)) {
            continue;
        }
        if (animated && d->home) {
            draw_home(d);
        }
        if (animated) {
            desk_draw_tip(d);
        }
        if (!desk_present(d)) {
            desk_event_frame(d, 1);
        }
    }
}

/* Focus stops (DUI-P3-3): the layout regions in order, then the search field. */
static int32_t desk_stops(const desk* d) {
    return (int32_t)d->layout.count + 1;
}

/* Moves the focus ring one stop (dir +1 Tab, -1 Shift+Tab); the whole canvas repaints so the old ring is gone. */
static void desk_focus_move(desk* d, int32_t dir) {
    int32_t n = desk_stops(d);
    d->focus = d->focus < 0 ? (dir > 0 ? 0 : n - 1) : (d->focus + dir + n) % n;
    d->focus_moves++;
    d->repaint = 1;
    d->uim.dirty |= SEL_UIM_DIRTY_ALL;
}

/* The ring of the focused stop: two logical pixels in the focus-ring token. The search stop is the field at the top
 * of the main region. */
static void desk_draw_focus(desk* d) {
    sel_gui_rect r;
    int32_t t = lpx(d, 2);
    uint32_t col = ARGB(d->theme.token[SEL_GUI_TOKEN_FOCUS_RING]);
    if (d->focus < 0 || d->layout.count == 0u) {
        return;
    }
    if ((uint32_t)d->focus < d->layout.count) {
        const sel_uim_region* g = &d->layout.regions[d->focus];
        r = rect_of(lpx(d, g->x), lpx(d, g->y), lpx(d, (int32_t)g->w), lpx(d, (int32_t)g->h));
    } else {
        r = region_rect(d, "main");
        r = rect_of(r.x + lpx(d, 8), r.y + lpx(d, 8), r.w - lpx(d, 16), lpx(d, 32));
    }
    if (r.w <= 2 * t || r.h <= 2 * t) {
        return;
    }
    sel_gui_fill_rect(d->canvas, rect_of(r.x, r.y, r.w, t), col);
    sel_gui_fill_rect(d->canvas, rect_of(r.x, r.y + r.h - t, r.w, t), col);
    sel_gui_fill_rect(d->canvas, rect_of(r.x, r.y, t, r.h), col);
    sel_gui_fill_rect(d->canvas, rect_of(r.x + r.w - t, r.y, t, r.h), col);
    sel_gui_dirty_add(d->canvas, r);
}

/* Sends one protocol line to the engine; 1 when it parsed. */
static int desk_send(desk* d, const char* line) {
    /* A decoded string argument points into the pool and the engine reads it on its next pump, after this returns: the
     * pool outlives the call, a ring so several lines queued before one pump keep their text. */
    static char ring[16][512];
    static uint32_t next;
    char* pool = ring[next++ % 16u];
    return sel_engine_send_line(d->engine, line, pool, sizeof ring[0]) == SEL_CODEC_OK;
}

/* The keys of the action table become the desktop bindings, loaded through the same txt_bindings the text UI uses. */
static void desk_bind_actions(desk* d) {
    uint32_t i, n = sel_uim_action_count();
    memset(&d->binds, 0, sizeof d->binds);
    for (i = 0; i < n; i++) {
        const sel_uim_action* a = sel_uim_action_at(i);
        char row[160];
        if (a->key[0] == '\0') {
            continue;
        }
        snprintf(row, sizeof row, "%s %s\n", a->key, a->line);
        txt_bindings_load(&d->binds, row, NULL);
    }
}

/* A key event (DUI-P3-3). Order: the search field takes printable keys while focused; Tab moves the focus; `/` focuses
 * the search; a bound key fires its action; Up and Down move the list cursor. Returns 1 when the picture changed. */
static int desk_key(desk* d, const sel_gui_input* ev) {
    const char* act;
    d->keys_in++;
    if (d->focus == (int32_t)d->layout.count) { /* the search field is focused */
        if (ev->code == 27u) {
            d->focus = 0;
        } else if (ev->code == 13u) {
            char line[96];
            if (d->qlen > 0u) {
                snprintf(line, sizeof line, "LIBRARY_SEARCH %s", d->query);
                desk_send(d, line);
                d->searches++;
            }
            d->focus = 0;
        } else if (ev->code == 127u) {
            if (d->qlen > 0u) {
                d->query[--d->qlen] = '\0';
            }
            return desk_event_frame(d, 0);
        } else if (ev->code >= 32u && ev->code < 127u && (ev->mods & (TXT_MOD_CTRL | TXT_MOD_ALT)) == 0u) {
            if (d->qlen + 1u < sizeof d->query) {
                d->query[d->qlen++] = (char)ev->code;
                d->query[d->qlen] = '\0';
            }
            return 0;
        } else {
            return 0;
        }
        d->repaint = 1;
        d->uim.dirty |= SEL_UIM_DIRTY_ALL;
        return desk_present(d);
    }
    if (ev->code == 9u && (ev->mods & ~TXT_MOD_SHIFT) == 0u) {
        desk_focus_move(d, (ev->mods & TXT_MOD_SHIFT) != 0u ? -1 : 1);
        return desk_present(d);
    }
    if (ev->code == '/' && ev->mods == 0u) {
        d->focus = (int32_t)d->layout.count;
        d->qlen = 0u;
        d->query[0] = '\0';
        d->repaint = 1;
        d->uim.dirty |= SEL_UIM_DIRTY_ALL;
        return desk_present(d);
    }
    act = txt_bindings_lookup(&d->binds, TXT_CTX_ANY, ev->code, ev->mods);
    if (act != NULL) {
        uint32_t i, n = sel_uim_action_count();
        for (i = 0; i < n; i++) {
            const sel_uim_action* a = sel_uim_action_at(i);
            if (strcmp(a->line, act) == 0) {
                if (desk_send(d, a->line)) {
                    d->actions_fired++;
                    if ((d->fired_seen[i / 32u] & (1u << (i % 32u))) == 0u) {
                        d->fired_seen[i / 32u] |= 1u << (i % 32u);
                        d->actions_distinct++;
                    }
                }
                break;
            }
        }
        return 0;
    }
    if (ev->code == TXT_KEY_UP || ev->code == TXT_KEY_DOWN) { /* arrow navigation in lists */
        desk_send(d, ev->code == TXT_KEY_UP ? "VIEW_CURSOR -1" : "VIEW_CURSOR +1");
        d->arrows++;
    }
    return 0;
}

/* Replays the --input script once:`resize W H`, `click X Y`, `tile N` (the centre of tile N, 0-based), `#` comments. */
/* The display row under the canvas point, or -1. */
static int32_t songs_row_at(desk* d, int32_t x, int32_t y) {
    uint32_t i;
    for (i = 0; i < d->order_n; i++) {
        if (in_rect(songs_row_rect(d, i), x, y)) {
            return (int32_t)i;
        }
    }
    return -1;
}

/* Sends "<verb> <path>" for a display row. */
static int songs_send_path(desk* d, const char* verb, int32_t row) {
    const sel_state* st = sel_engine_state(d->engine);
    char line[320];
    snprintf(line, sizeof line, "%s %s", verb, st->queue_text + st->queue[d->order[row]].path_offset);
    return desk_send(d, line);
}

/* One pointer event (DUI-P3-2): the region is found on the layout tree first, then the part inside it. Returns 1 when
 * the picture changed. */
static int desk_pointer_view(desk* d, const sel_gui_input* ev) {
    int32_t sc = d->scale > 0 ? d->scale : 1;
    int region = sel_uim_hit(&d->layout, ev->x / sc, ev->y / sc);
    const char* rname = region >= 0 ? d->layout.regions[region].name : "";
    int in_main = strcmp(rname, "main") == 0;
    if (!d->songs) {
        if (ev->kind == SEL_GUI_IN_CLICK && ev->button == 0u) {
            d->clicks++;
            return desk_click(d, ev->x, ev->y);
        }
        return 0;
    }
    if (d->order_n != sel_engine_state(d->engine)->queue_count) {
        songs_resort(d);
    }
    if (ev->kind == SEL_GUI_IN_MOVE) {
        int32_t row = in_main ? songs_row_at(d, ev->x, ev->y) : -1;
        if (row + 1 == d->hover_row) {
            return 0;
        }
        d->hover_row = row + 1;
        d->hovers++;
        return 1;
    }
    if (ev->kind == SEL_GUI_IN_DRAG) {
        int32_t row = in_main ? songs_row_at(d, ev->x, ev->y) : -1;
        int end = sel_uim_hit(&d->layout, ev->x2 / sc, ev->y2 / sc);
        if (row < 0 || end < 0 || strcmp(d->layout.regions[end].name, "sidebar") != 0) {
            return 0;
        }
        if (in_rect(songs_target_rect(d, 0), ev->x2, ev->y2)) { /* drop on Queue: the dragged song is queued again */
            d->drags++;
            return songs_send_path(d, "QUEUE_ADD", row);
        }
        return 0; /* drop on Playlist: needs an engine verb that adds one song to a saved playlist (open) */
    }
    if (ev->kind != SEL_GUI_IN_CLICK) {
        return 0;
    }
    if (d->menu_row > 0) { /* a click while the context menu is open picks an item or dismisses it */
        uint32_t k;
        int32_t row = d->menu_row - 1;
        d->menu_row = 0;
        for (k = 0; k < SONG_MENU_ITEMS; k++) {
            sel_gui_rect m = rect_of(d->menu_x, d->menu_y + (int32_t)k * lpx(d, SONG_MENU_ITEM_H), lpx(d, SONG_MENU_W), lpx(d, SONG_MENU_ITEM_H));
            if (in_rect(m, ev->x, ev->y)) {
                songs_send_path(d, k == 0u ? "OPEN" : k == 1u ? "QUEUE_PLAY_NEXT" : "QUEUE_ADD", row);
                d->row_plays += k == 0u;
            }
        }
        return 1;
    }
    if (ev->button == 1u) {
        int32_t row = in_main ? songs_row_at(d, ev->x, ev->y) : -1;
        if (row < 0) {
            return 0;
        }
        d->menu_row = row + 1;
        d->menu_x = ev->x;
        d->menu_y = ev->y;
        d->menus++;
        return 1;
    }
    d->clicks++;
    if (in_main) {
        uint32_t c;
        int32_t row = songs_row_at(d, ev->x, ev->y);
        for (c = 0; c < 2u; c++) {
            if (in_rect(songs_header_col(d, c), ev->x, ev->y)) { /* header: ascending first, then toggles */
                if (d->sort_col == c && d->sort_dir == 1u) {
                    d->sort_dir = 2u;
                } else {
                    d->sort_col = c;
                    d->sort_dir = 1u;
                }
                d->sorts++;
                songs_resort(d);
                return 1;
            }
        }
        if (row >= 0 && ev->clicks >= 2u) { /* double-click plays the row */
            d->double_clicks++;
            d->row_plays++;
            songs_send_path(d, "OPEN", row);
            return 1;
        }
    }
    return 0;
}

/* A pointer event: the view handles it, and the tooltip tracker follows the transport buttons (DUI-P3-6). */
static int desk_pointer(desk* d, const sel_gui_input* ev) {
    int tip = desk_tip_pointer(d, ev->kind == SEL_GUI_IN_MOVE ? ev->x : -1, ev->y);
    return desk_pointer_view(d, ev) | tip;
}

/* Touchpad events (DUI-P3-4). Wheel scrolls the songs list by Q16 logical pixels (negative dy scrolls down), clamped to
 * the list; pinch in the tile grid moves the tile size one token step (clamped); swipe is Back and Forward. Returns 1
 * when the picture changed. */
static int desk_gesture(desk* d, const sel_gui_input* ev) {
    if (ev->kind == SEL_GUI_IN_WHEEL) {
        if (d->songs) {
            sel_gui_rect mn = region_rect(d, "main");
            int32_t sc = d->scale > 0 ? d->scale : 1;
            int32_t view_h = (mn.y + mn.h - songs_top(d) - lpx(d, SONG_HEADER_H)) / sc;
            int32_t max_q16 = ((int32_t)d->order_n * SONG_ROW_H - view_h) << 16;
            int64_t v = (int64_t)d->scroll_q16 - (int64_t)ev->dy * SCROLL_NOTCH;
            if (max_q16 < 0) {
                max_q16 = 0;
            }
            d->scroll_q16 = v < 0 ? 0 : (v > max_q16 ? max_q16 : (int32_t)v);
            draw_home(d);
            return 1;
        }
        return 0;
    }
    if (ev->kind == SEL_GUI_IN_PINCH) {
        if (!d->home || d->songs || d->open_tile > 0) {
            return 0;
        }
        d->gestures++;
        if (ev->scale > SEL_GUI_Q16_ONE && d->tile_step + 1u < TILE_STEPS) {
            d->tile_step++;
        } else if (ev->scale < SEL_GUI_Q16_ONE && d->tile_step > 0u) {
            d->tile_step--;
        }
        draw_home(d);
        return 1;
    }
    if (ev->kind == SEL_GUI_IN_SWIPE) {
        d->gestures++;
        if (view_swipe(d, ev->x)) {
            desk_tween(d, ANIM_KEY_VIEW, SEL_GUI_ANIM_NORMAL_MS, SEL_GUI_EASE_OUT);
            draw_home(d);
            return 1;
        }
    }
    return 0;
}

static int desk_replay_input(desk* d) {
    char* line = d->input;
    int opened = 0;
    d->input_done = 1;
    while (line != NULL && *line != '\0') {
        char* nl = strchr(line, '\n');
        int x, y;
        unsigned n;
        sel_gui_input ev;
        if (nl != NULL) {
            *nl = '\0';
        }
        if (sel_gui_input_parse(line, &ev) && ev.kind == SEL_GUI_IN_KEY) {
            opened |= desk_key(d, &ev);
        } else if (sel_gui_input_parse(line, &ev) && ev.kind == SEL_GUI_IN_WAIT) {
            desk_advance(d, ev.ms);
        } else if (sel_gui_input_parse(line, &ev) &&
                   (ev.kind == SEL_GUI_IN_CLICK || ev.kind == SEL_GUI_IN_MOVE || ev.kind == SEL_GUI_IN_DRAG)) {
            int changed = desk_pointer(d, &ev);
            if (changed && d->songs) {
                draw_home(d);
            }
            opened |= desk_event_frame(d, changed);
        } else if (sel_gui_input_parse(line, &ev) &&
                   (ev.kind == SEL_GUI_IN_WHEEL || ev.kind == SEL_GUI_IN_PINCH || ev.kind == SEL_GUI_IN_SWIPE)) {
            opened |= desk_event_frame(d, desk_gesture(d, &ev));
        } else if (sscanf(line, "resize %d %d", &x, &y) == 2) { /* the window was resized: relayout and repaint */
            opened |= desk_resize(d, x, y);
        } else if (sscanf(line, "scale %d", &x) == 1) { /* the window moved to a screen of another scale */
            if (desk_set_scale(d, x)) {
                desk_resize(d, d->canvas->w, d->canvas->h);
            }
        } else if (sscanf(line, "tile %u", &n) == 1) {
            sel_gui_rect t = tile_rect(d, n);
            opened |= desk_event_frame(d, desk_click(d, t.x + t.w / 2, t.y + t.h / 2));
        } else if (sscanf(line, "hover %u", &n) == 1 && d->home && d->open_tile == 0) {
            d->hover_tile = (int32_t)n + 1;
            desk_tween(d, ANIM_KEY_HOVER, SEL_GUI_ANIM_FAST_MS, SEL_GUI_EASE_OUT);
            draw_home(d);
            opened |= desk_event_frame(d, 1);
        } else if (sscanf(line, "wait %u", &n) == 1) {
            desk_advance(d, n);
        }
        line = nl != NULL ? nl + 1 : NULL;
    }
    return opened;
}

/* Reads --input <file> (at most 4096 bytes) into a heap copy; 0 when unreadable. */
static int desk_load_input(desk* d, const char* path) {
    FILE* f = fopen(path, "rb");
    size_t n;
    if (f == NULL) {
        return 0;
    }
    d->input = (char*)d->host->mem_alloc(d->host, 4097u, SEL_MEM_FAST);
    if (d->input == NULL) {
        fclose(f);
        return 0;
    }
    n = fread(d->input, 1, 4096u, f);
    d->input[n] = '\0';
    fclose(f);
    return 1;
}

/* Writes the canvas as a binary PPM (P6, 0xRRGGBB per pixel). */
static void desk_write_frame(const desk* d, const char* path) {
    FILE* f = fopen(path, "wb");
    int32_t x, y;
    if (f == NULL) {
        fprintf(stderr, "selecta: cannot write frame dump '%s'\n", path);
        return;
    }
    fprintf(f, "P6\n%d %d\n255\n", (int)d->canvas->w, (int)d->canvas->h);
    for (y = 0; y < d->canvas->h; y++) {
        for (x = 0; x < d->canvas->w; x++) {
            uint32_t rgb = sel_gui_pixel(d->canvas, x, y);
            fputc((int)((rgb >> 16) & 0xFFu), f);
            fputc((int)((rgb >> 8) & 0xFFu), f);
            fputc((int)(rgb & 0xFFu), f);
        }
    }
    fclose(f);
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
    if (d->frames == 0u || d->repaint) { /* the first frame and a resize paint the whole canvas */
        d->repaint = 0;
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
            if (d->frames > 0u && desk_motion(d)) {
                d->tweens++;
            }
        } else if (bit == SEL_UIM_DIRTY_PROGRESS) {
            desk_hash(d, f->clock);
            desk_hash(d, f->remaining);
        } else if (bit == SEL_UIM_DIRTY_STATUS) {
            desk_hash(d, f->status);
        }
    }
    desk_draw_focus(d);
    desk_draw_tip(d);
    d->dirty_pixels = desk_flush(d);
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
        for (i = 0; i < TRANSPORT_COUNT; i++) {
            sel_gui_rect b = desk_button_rect(d, i);
            printf("button id=%s x=%d y=%d w=%d h=%d\n", g_transport[i], (int)b.x, (int)b.y, (int)b.w, (int)b.h);
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
        if (d->theme.bad_token[0] != '\0') {
            if (d->theme.bad_range) {
                fprintf(stderr, "selecta: desktop theme '%s' line %d: token '%s' resolves to a value outside its range\n",
                        path, bad, d->theme.bad_token);
            } else {
                fprintf(stderr, "selecta: desktop theme '%s' line %d: token '%s' has a missing or circular reference\n",
                        path, bad, d->theme.bad_token);
            }
        } else {
            fprintf(stderr, "selecta: desktop theme '%s' line %d is not a role or token line in range\n", path, bad);
        }
        return 0;
    }
    return 1;
}

static void desk_close(void* state) {
    desk* d = (desk*)state;
    int i;
    if (d != NULL) {
        if (d->report) {
            printf("desktop_ui closed frames=%u title=%s clock=%s status=%s\n", (unsigned)d->frames, d->uim.frame.title,
                   d->uim.frame.clock, d->uim.frame.status);
        }
        if (d->report && d->widgets) {
            for (i = 0; i < (int)SEL_GUI_WIDGET_KIND_COUNT; i++) {
                printf("widget_kind name=%s draws=%u\n", sel_gui_widget_kind_name((uint32_t)i),
                       (unsigned)d->wstats.draws[i]);
            }
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
        if (d->frame_dump != NULL && d->canvas != NULL) {
            desk_write_frame(d, d->frame_dump);
        }
        if (d->input != NULL) {
            d->host->mem_free(d->host, d->input);
        }
        art_close(d->art);
        for (i = 0; i < 2 * DESK_SIZES; i++) {
            desk_atlas_free(d->atlas[i / DESK_SIZES][i % DESK_SIZES]);
        }
#ifdef SELECTA_DESKTOP_SDL
        desk_sdl_close(d->sdl);
#endif
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
        sel_gui_shadow_bake(&d->shadow, (uint32_t)lpx(d, 8), 0x000000u, 96u);
        d->shadow_ready = 1;
    }
    sel_uim_layout_compute(&d->layout, w / (unsigned)d->scale, h / (unsigned)d->scale);
    return 1;
}

/* The icon atlas for the current scale (DUI-P2-8): each logical icon size at k times the size, picked from the
 * baked masks (tools/gui_iconbake.c bakes every logical size times 1, 2 and 3). Counts one icon rebuild. */
static void desk_build_icons(desk* d) {
    int i;
    uint32_t s;
    for (i = 0; i < DESK_ICON_SIZES; i++) {
        d->icon_px[i] = g_icon_sizes[i] * d->scale;
        d->icon_alpha[i] = sel_gui_icons_baked.alpha[0];
        for (s = 0; s < sel_gui_icons_baked.size_n; s++) {
            if (sel_gui_icons_baked.sizes[s] == d->icon_px[i]) {
                d->icon_alpha[i] = sel_gui_icons_baked.alpha[s];
            }
        }
    }
    d->icons.count = sel_gui_icons_baked.count;
    d->icons.size_n = DESK_ICON_SIZES;
    d->icons.sizes = d->icon_px;
    d->icons.alpha = d->icon_alpha;
    d->icon_rebuilds++;
}

/* A new integer pixel scale (DUI-P2-8): the text atlases are dropped (desk_font rebuilds them at the new pixel
 * size on first use), the icon atlas and the shadow are rebuilt, and the next resize relays out and repaints. The
 * same scale again does nothing. */
static int desk_set_scale(desk* d, int32_t scale) {
    int i;
    if (scale < 1 || scale > 3 || scale == d->scale) {
        return 0;
    }
    d->scale = scale;
    for (i = 0; i < 2 * DESK_SIZES; i++) {
        desk_atlas_free(d->atlas[i / DESK_SIZES][i % DESK_SIZES]);
        d->atlas[i / DESK_SIZES][i % DESK_SIZES] = NULL;
    }
    d->atlas_rebuilds++;
    desk_build_icons(d);
    if (d->shadow_ready) {
        sel_gui_shadow_bake(&d->shadow, (uint32_t)lpx(d, 8), 0x000000u, 96u);
    }
    return 1;
}

/* The window changed size (a scripted `resize W H`, or the user or a scale change on the sdl backend): the canvas
 * takes the new pixel size, the layout is recomputed and the whole canvas is repainted and presented. Returns 0: the
 * repaint is already presented, so the caller has nothing left to flush. */
static int desk_resize(desk* d, int32_t w, int32_t h) {
    void* pixels;
    if (w < 64 || h < 64 || w > 4096 || h > 4096 || d->canvas->format != SEL_GUI_RGBA32) {
        return 0;
    }
    pixels = d->host->mem_alloc(d->host, sel_gui_pixel_bytes(SEL_GUI_RGBA32, w, h), SEL_MEM_FAST);
    if (pixels == NULL) {
        return 0;
    }
    d->host->mem_free(d->host, d->canvas->pixels);
    sel_gui_canvas_init(d->canvas, SEL_GUI_RGBA32, w, h, pixels);
#ifdef SELECTA_DESKTOP_SDL
    if (d->sdl != NULL && !desk_sdl_resize(d->sdl, w, h)) {
        return 0;
    }
#endif
    sel_uim_layout_compute(&d->layout, (uint32_t)(w / d->scale), (uint32_t)(h / d->scale));
    d->repaint = 1;
    d->uim.dirty |= SEL_UIM_DIRTY_ALL;
    desk_present(d);
    return 0;
}

static void* desk_open(const sel_ui_open_args* a) {
    const char* backend = desk_opt(a, "--backend");
    desk* d;
    int i;
    int want_sdl = backend != NULL && strcmp(backend, "sdl") == 0;
    if (backend != NULL && strcmp(backend, "mem") != 0 && !want_sdl) {
        fprintf(stderr, "selecta: desktop backend '%s' is not mem or sdl\n", backend);
        return NULL;
    }
#ifndef SELECTA_DESKTOP_SDL
    if (want_sdl) {
        fprintf(stderr, "selecta: desktop backend 'sdl' is not available in this build (configure with -DSELECTA_UI_DESKTOP=ON)\n");
        return NULL;
    }
#endif
    d = (desk*)a->host->mem_alloc(a->host, sizeof *d, SEL_MEM_FAST);
    if (d == NULL) {
        return NULL;
    }
    memset(d, 0, sizeof *d);
    d->tile_step = TILE_STEP_DEFAULT;
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
        const char* view = desk_opt(a, "--view");
        if (strcmp(view, "testcard") == 0) {
            d->testcard = 1;
        } else if (strcmp(view, "text") == 0) {
            d->textsheet = 1;
        } else if (strcmp(view, "widgets") == 0) {
            d->widgets = 1;
        } else if (strcmp(view, "home") == 0) {
            d->home = 1;
        } else if (strcmp(view, "songs") == 0) {
            d->home = 1;
            d->songs = 1;
        } else {
            fprintf(stderr, "selecta: desktop view '%s' is not testcard, text, home, songs or widgets\n", view);
            a->host->mem_free(a->host, d);
            return NULL;
        }
    }
    d->scale = 1;
    d->focus = -1;
    desk_bind_actions(d);
    if (desk_opt(a, "--scale") != NULL) {
        int sc = atoi(desk_opt(a, "--scale"));
        if (sc < 1 || sc > 3) {
            fprintf(stderr, "selecta: desktop scale '%s' is not 1, 2 or 3\n", desk_opt(a, "--scale"));
            a->host->mem_free(a->host, d);
            return NULL;
        }
        d->scale = sc;
    }
    desk_build_icons(d);
    d->icon_rebuilds = 0u; /* the first build is not a rebuild */
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
        if (strcmp(a->argv[i], "--frame-dump") == 0 && i + 1 < a->argc) {
            d->frame_dump = a->argv[i + 1];
        }
        if (strcmp(a->argv[i], "--input") == 0 && i + 1 < a->argc && !desk_load_input(d, a->argv[i + 1])) {
            fprintf(stderr, "selecta: cannot read input script '%s'\n", a->argv[i + 1]);
            desk_close(d);
            return NULL;
        }
        if (strcmp(a->argv[i], "--clock") == 0 && i + 1 < a->argc && strcmp(a->argv[i + 1], "fixed") != 0) {
            fprintf(stderr, "selecta: desktop clock '%s' is not fixed\n", a->argv[i + 1]);
            desk_close(d);
            return NULL;
        }
        if (strcmp(a->argv[i], "--actions") == 0) {
            d->actions = 1;
        }
    }
#ifdef SELECTA_DESKTOP_SDL
    if (want_sdl) {
        if (d->canvas->format != SEL_GUI_RGBA32) {
            fprintf(stderr, "selecta: the sdl backend shows an rgba32 canvas, not index8\n");
            desk_close(d);
            return NULL;
        }
        d->sdl = desk_sdl_open(d->canvas->w, d->canvas->h);
        if (d->sdl == NULL) {
            desk_close(d);
            return NULL;
        }
        d->live = d->input == NULL && !desk_sdl_headless(d->sdl);
    }
#endif
    d->art = art_open(a->art);
    sel_gui_timeline_init(&d->tl);
    sel_uim_init(&d->uim);
    sel_uim_update(&d->uim, sel_engine_state(d->engine));
    if (d->testcard) {
        draw_testcard(d);
        d->dirty_pixels = desk_flush(d);
        d->dirty_total = d->dirty_pixels;
    } else if (d->textsheet || d->widgets) {
        d->dirty_total = 0; /* drawn on the first turn, after the --exec commands queued their names */
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
#ifndef SELECTA_DESKTOP_SDL
    (void)timeout_ms;
#endif
    if (d->testcard) {
        return -1; /* the test card is one frame */
    }
    if (d->textsheet) { /* the text sheet is one frame, drawn on the first turn */
        if (d->frames > 0u) {
            return -1;
        }
        draw_textsheet(d);
        d->dirty_pixels = desk_flush(d);
        d->dirty_total = d->dirty_pixels;
        d->frames++;
        return 1;
    }
#ifdef SELECTA_DESKTOP_SDL
    if (d->sdl != NULL) {
        int32_t w = 0, h = 0;
        int ev = desk_sdl_poll(d->sdl, 0u, &w, &h);
        if (ev == DESK_SDL_QUIT) {
            return -1;
        }
        if (ev == DESK_SDL_RESIZE) {
            desk_set_scale(d, desk_sdl_scale(d->sdl));
            desk_resize(d, w, h);
        }
    }
#endif
    if (d->widgets) { /* the gallery is one frame, drawn on the first turn */
        if (d->frames > 0u) {
            return -1;
        }
        draw_widgets(d);
        d->dirty_pixels = desk_flush(d);
        d->dirty_total = d->dirty_pixels;
        d->frames++;
        return 1;
    }
    sel_uim_update(&d->uim, sel_engine_state(d->engine));
    if (d->ticks > 0u && d->uim.dirty == 0u) { /* --ticks N: the mem backend has no clock, so it replays N seconds */
        d->ticks--;
        d->uim.dirty |= SEL_UIM_DIRTY_PROGRESS;
    }
    if (desk_present(d)) {
        return 1;
    }
    if (d->input != NULL && !d->input_done && d->ticks == 0u && desk_replay_input(d)) { /* scripted input, once */
        return 1; /* every event flushed its own frame */
    }
#ifdef SELECTA_DESKTOP_SDL
    if (d->live) { /* a window on a real display: idle until an event or the next second boundary */
        int32_t w = 0, h = 0;
        int ev;
        uint32_t wait_ms;
        d->now_ms = desk_sdl_ticks();
        sel_gui_timeline_advance(&d->tl, d->now_ms);
        wait_ms = 1000u - d->now_ms % 1000u;
        if (sel_gui_timeline_running(&d->tl) > 0u) { /* a tween runs: one frame per 16 ms step */
            wait_ms = 16u;
            if (d->home) {
                draw_home(d);
            }
            desk_event_frame(d, 1);
        }
        (void)timeout_ms;
        ev = desk_sdl_poll(d->sdl, wait_ms, &w, &h);
        if (ev == DESK_SDL_QUIT) {
            return -1;
        }
        if (ev == DESK_SDL_RESIZE) {
            desk_set_scale(d, desk_sdl_scale(d->sdl));
            desk_resize(d, w, h);
        }
        return ev == DESK_SDL_NONE ? 0 : 1;
    }
#endif
    if (sel_gui_timeline_running(&d->tl) > 0u) { /* the run ends while a tween runs: show the end state once */
        sel_gui_timeline_finish(&d->tl);
        if (d->home) {
            draw_home(d);
        }
        desk_event_frame(d, 1);
        return 1;
    }
    return -1;
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
    d->stats[12].tag = SEL_UI_STAT_MISSING_GLYPHS;
    d->stats[12].value = d->text.missing;
    d->stats[13].tag = SEL_UI_STAT_GLYPHS_DRAWN;
    d->stats[13].value = d->text.glyphs;
    d->stats[14].tag = SEL_UI_STAT_VIEW_OPENS;
    d->stats[14].value = d->view_opens;
    d->stats[15].tag = SEL_UI_STAT_ART_DECODES;
    d->stats[15].value = art_decodes(d->art);
    d->stats[16].tag = SEL_UI_STAT_ART_TILE_HASH;
    d->stats[16].value = d->art_tile_hash;
    d->stats[17].tag = SEL_UI_STAT_PRESENT_HASH;
    d->stats[18].tag = SEL_UI_STAT_UPLOAD_PIXELS;
    d->stats[19].tag = SEL_UI_STAT_PRESENTS;
#ifdef SELECTA_DESKTOP_SDL
    if (d->sdl != NULL) {
        d->stats[17].value = desk_sdl_present_hash(d->sdl);
        d->stats[18].value = desk_sdl_upload_pixels(d->sdl);
        d->stats[19].value = desk_sdl_presents(d->sdl);
    }
#endif
    d->stats[20].tag = SEL_UI_STAT_SCALE;
    d->stats[20].value = (uintptr_t)d->scale;
    d->stats[21].tag = SEL_UI_STAT_ATLAS_REBUILDS;
    d->stats[21].value = d->atlas_rebuilds;
    d->stats[22].tag = SEL_UI_STAT_ICON_REBUILDS;
    d->stats[22].value = d->icon_rebuilds;
    d->stats[23].tag = SEL_UI_STAT_ATLAS_BUILDS;
    d->stats[23].value = d->atlas_builds;
    d->stats[24].tag = SEL_UI_STAT_ACTIONS_FIRED;
    d->stats[24].value = d->actions_fired;
    d->stats[25].tag = SEL_UI_STAT_ACTIONS_DISTINCT;
    d->stats[25].value = d->actions_distinct;
    d->stats[26].tag = SEL_UI_STAT_FOCUS_MOVES;
    d->stats[26].value = d->focus_moves;
    d->stats[27].tag = SEL_UI_STAT_SEARCHES;
    d->stats[27].value = d->searches;
    d->stats[28].tag = SEL_UI_STAT_ARROWS;
    d->stats[28].value = d->arrows;
    d->stats[29].tag = SEL_UI_STAT_FOCUS;
    d->stats[29].value = (uintptr_t)(d->focus + 1);
    d->stats[30].tag = SEL_UI_STAT_HOVERS;
    d->stats[30].value = d->hovers;
    d->stats[31].tag = SEL_UI_STAT_CLICKS;
    d->stats[31].value = d->clicks;
    d->stats[32].tag = SEL_UI_STAT_DOUBLE_CLICKS;
    d->stats[32].value = d->double_clicks;
    d->stats[33].tag = SEL_UI_STAT_DRAGS;
    d->stats[33].value = d->drags;
    d->stats[34].tag = SEL_UI_STAT_SORTS;
    d->stats[34].value = d->sorts;
    d->stats[35].tag = SEL_UI_STAT_SORT_DIR;
    d->stats[35].value = d->sort_dir;
    d->stats[36].tag = SEL_UI_STAT_TOP_ROW;
    d->stats[36].value = (d->order_n > 0u ? (uintptr_t)d->order[0] + 1u : 0u);
    d->stats[37].tag = SEL_UI_STAT_MENUS;
    d->stats[37].value = d->menus;
    d->stats[38].tag = SEL_UI_STAT_ROW_PLAYS;
    d->stats[38].value = d->row_plays;
    d->stats[39].tag = SEL_UI_STAT_TOOLTIPS;
    d->stats[39].value = d->tooltips;
    d->stats[40].tag = SEL_UI_STAT_TOOLTIP_TEXT;
    d->stats[40].value = (uintptr_t)(d->tip_shown > 0 ? d->tip_text : "");
    d->stats[41].tag = SEL_UI_STAT_GESTURES;
    d->stats[41].value = d->gestures;
    d->stats[42].tag = SEL_UI_STAT_TILE_SIZE;
    d->stats[42].value = (uintptr_t)g_tile_steps[d->tile_step];
    d->stats[43].tag = SEL_UI_STAT_SCROLL_PX;
    d->stats[43].value = (uintptr_t)(d->scroll_q16 >> 16);
    d->stats[44].tag = SEL_UI_STAT_OPEN_TILE;
    d->stats[44].value = (uintptr_t)d->open_tile;
    d->stats[45].tag = SEL_TAG_END;

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

/* Desktop UI plugin (M3d, DUI-P1-4). It renders the engine snapshot through the shared UI model
 * (include/selecta/uimodel.h): one widget per region, redrawn only when the region's dirty bit is set. Drawing goes
 * to a backend: `mem` (headless, no SDL, DUI-P2-6) or `sdl` (a window, DUI-P2-7), selected with `--backend`. */
#include <selecta/plugin.h>
#include <selecta/ui.h>
#include <selecta/uimodel.h>

#include "art.h"
#include <selecta/gui/gui_genart.h>
#include "atlas_build.h"
#include <selecta/gui/gui_draw.h>
#include <selecta/gui/gui_text.h>
#include <selecta/gui/gui_widget.h>
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
    int live;               /* sdl on a real display without --input: the window stays until it is closed */
    int repaint;            /* the next present paints the whole canvas (first frame, resize) */
    int32_t scale;          /* integer pixel scale 1, 2 or 3 (DUI-P2-8): layout is in logical units, drawn times scale */
    uint32_t atlas_builds;   /* text atlases built */
    uint32_t atlas_rebuilds; /* scale changes that dropped the built text atlases */
    uint32_t icon_rebuilds;  /* scale changes that rebuilt the icon atlas view */
    sel_gui_icons icons;     /* the icon atlas at the current scale: the baked masks of size k * logical */
    int32_t icon_px[DESK_ICON_SIZES];
    const uint8_t* icon_alpha[DESK_ICON_SIZES];
    sel_tag stats[25];
} desk;

static int desk_resize(desk* d, int32_t w, int32_t h);
static int desk_set_scale(desk* d, int32_t scale);
static int desk_present(desk* d);

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
static void gallery_put(desk* d, const sel_gui_style* st, uint32_t kind, int32_t x, int32_t y, int32_t w, int32_t h,
                        uint32_t state, const char* label, int32_t icon, int32_t value, const char* const* items,
                        uint32_t item_n, int32_t sel) {
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
    wd.top = 1;
    sel_gui_widget_draw(d->canvas, st, &wd, &d->wstats);
    d->widget_draws++;
}

static void draw_widgets(desk* d) {
    static const char* const songs[] = {"Axel F", "Cannon Fodder", "Stardust Memories", "Hybris", "Elysium"};
    static const char* const tabs[] = {"Songs", "Albums", "Artists"};
    static const char* const head[] = {"Title", "Artist", "Time"};
    static const char* const side[] = {"Home", "Search", "Library", "Favourites"};
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
    gallery_put(d, &st, SEL_GUI_WIDGET_SPLIT_PANE, c1, 550, cw - 24, 200, 0, NULL, -1, half, NULL, 0, 0);
    gallery_put(d, &st, SEL_GUI_WIDGET_SCROLL_BAR, c1 + cw - 12, 550, 12, 200, 0, NULL, -1, half, NULL, 0, 0);
    /* column 2: lists and menus */
    gallery_put(d, &st, SEL_GUI_WIDGET_VIRTUAL_LIST, c2, 24, cw, 200, 0, NULL, -1, 0, songs, 5, 1);
    gallery_put(d, &st, SEL_GUI_WIDGET_SIDEBAR_LIST, c2, 244, cw, 200, 0, NULL, SEL_GUI_ICON_HOUSE, 0, side, 4, 0);
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
    gallery_put(d, &st, SEL_GUI_WIDGET_SCRIM, c4 - 10, 10, cw + 20, 400, 0, NULL, -1, SEL_GUI_Q16_ONE * 3 / 5, NULL, 0, 0);
    gallery_put(d, &st, SEL_GUI_WIDGET_MODAL_DIALOG, c4 + 10, 80, cw - 20, 190, 0, "Remove from library", -1, 0, NULL, 0,
                0);
    d->text.glyphs += d->wstats.text.glyphs;
    d->text.missing += d->wstats.text.missing;
    d->wstats.text.glyphs = 0;
    d->wstats.text.missing = 0;
    sel_gui_dirty_add(c, rect_of(0, 0, c->w, c->h));
}

#define TILE_ART 144 /* logical */
#define HOME_MAX_LINES 2

/* Where tile i sits (DUI-P2-6): a grid below the song card in the main region, columns as many as fit. */
static sel_gui_rect tile_rect(desk* d, uint32_t i) {
    int32_t sp = lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_SPACING]);
    sel_gui_rect mn = region_rect(d, "main");
    const sel_gui_atlas* at = desk_font(d, 1, 13);
    int32_t title_h = HOME_MAX_LINES * (at != NULL ? at->line_height : 16);
    int32_t art = lpx(d, TILE_ART);
    int32_t cols = (mn.w - sp) / (art + sp);
    int32_t h = art + lpx(d, 4) + title_h;
    if (cols < 1) {
        cols = 1;
    }
    return rect_of(mn.x + sp + (int32_t)(i % (uint32_t)cols) * (art + sp),
                   mn.y + sp + lpx(d, 48) + 2 * sp + (int32_t)(i / (uint32_t)cols) * (h + sp), art, h);
}

/* The detail view of an opened tile: the main region cleared, the title large. */
static void draw_detail(desk* d) {
    const sel_state* st = sel_engine_state(d->engine);
    int32_t sp = lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_SPACING]);
    sel_gui_rect mn = region_rect(d, "main");
    const sel_gui_atlas* big = desk_font(d, 1, 24);
    char name[256];
    sel_gui_fill_rect(d->canvas, mn, ARGB(d->theme.token[SEL_GUI_TOKEN_SURFACE]));
    if (big != NULL && d->open_tile >= 1 && (uint32_t)d->open_tile <= st->queue_count) {
        queue_title(st, (uint32_t)d->open_tile - 1u, name, sizeof name);
        sel_gui_clip_push(d->canvas, mn);
        sel_gui_text_wrap(d->canvas, big, rect_of(mn.x + sp, mn.y + sp, mn.w - 2 * sp, 3 * big->line_height), name,
                          ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT]), 3, &d->text);
        sel_gui_clip_pop(d->canvas);
    }
    sel_gui_dirty_add(d->canvas, mn);
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

static void draw_home(desk* d) {
    const sel_state* st = sel_engine_state(d->engine);
    sel_gui_rect mn = region_rect(d, "main");
    int32_t sp = lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_SPACING]);
    const sel_gui_atlas* semi = desk_font(d, 1, 13);
    uint32_t i;
    char name[256];
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

/* A click at (x, y): opens the tile under it. Returns 1 when a tile opened. */
static int desk_click(desk* d, int32_t x, int32_t y) {
    const sel_state* st = sel_engine_state(d->engine);
    sel_gui_rect mn = region_rect(d, "main");
    uint32_t i;
    if (!d->home || d->open_tile > 0 || x < mn.x || y < mn.y || x >= mn.x + mn.w || y >= mn.y + mn.h) {
        return 0;
    }
    for (i = 0; i < st->queue_count; i++) {
        sel_gui_rect t = tile_rect(d, i);
        if (x >= t.x && x < t.x + t.w && y >= t.y && y < t.y + t.h) {
            d->open_tile = (int32_t)i + 1;
            d->view_opens++;
            draw_detail(d);
            return 1;
        }
    }
    return 0;
}

/* Replays the --input script once: `resize W H`, `click X Y`, `tile N` (the centre of tile N, 0-based), `#` comments. */
static int desk_replay_input(desk* d) {
    char* line = d->input;
    int opened = 0;
    d->input_done = 1;
    while (line != NULL && *line != '\0') {
        char* nl = strchr(line, '\n');
        int x, y;
        unsigned n;
        if (nl != NULL) {
            *nl = '\0';
        }
        if (sscanf(line, "resize %d %d", &x, &y) == 2) { /* the window was resized: relayout and repaint */
            opened |= desk_resize(d, x, y);
        } else if (sscanf(line, "scale %d", &x) == 1) { /* the window moved to a screen of another scale */
            if (desk_set_scale(d, x)) {
                desk_resize(d, d->canvas->w, d->canvas->h);
            }
        } else if (sscanf(line, "click %d %d", &x, &y) == 2) {
            opened |= desk_click(d, x, y);
        } else if (sscanf(line, "tile %u", &n) == 1) {
            sel_gui_rect t = tile_rect(d, n);
            opened |= desk_click(d, t.x + t.w / 2, t.y + t.h / 2);
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
        } else {
            fprintf(stderr, "selecta: desktop view '%s' is not testcard, text, home or widgets\n", view);
            a->host->mem_free(a->host, d);
            return NULL;
        }
    }
    d->scale = 1;
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
        d->dirty_pixels = desk_flush(d);
        d->dirty_total += d->dirty_pixels;
        d->frames++;
        return 1;
    }
#ifdef SELECTA_DESKTOP_SDL
    if (d->live) { /* a window on a real display: idle until an event or the next turn */
        int32_t w = 0, h = 0;
        int ev = desk_sdl_poll(d->sdl, timeout_ms > 0u ? timeout_ms : 50u, &w, &h);
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
    d->stats[24].tag = SEL_TAG_END;
    d->stats[24].value = 0;
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

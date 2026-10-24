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
#define DESK_SIZES 10
/* The type scale tokens (Main.dc.html): caption 11/12, body 13/14, nav 15, subtitle 16, brand 18, heading 22, 24, 32. */
static const int g_sizes[DESK_SIZES] = {11, 12, 13, 14, 15, 16, 18, 22, 24, 32};
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
    int* input_consumed;    /* the shell's once-per-process flag for the --input script */
    desk_art* art;          /* tile artwork from the library, NULL without one (DUI-P2-4) */
    uint64_t art_tile_hash; /* pixels of the first home tile, STAT_ART_TILE_HASH (DUI-P2-5) */
    char canvas_hash_hex[17]; /* the 64-bit hashes as 16 hex digits: the stats carry strings, a tag value may be 32 bits */
    char art_tile_hash_hex[17];
    char present_hash_hex[17];
#ifdef SELECTA_DESKTOP_SDL
    desk_sdl* sdl;          /* --backend sdl: the window the canvas is presented in (DUI-P2-7) */
#endif
    sel_gui_timeline tl;    /* the animation timeline (DUI-P3-5): at most 16 tweens, Q16 values, ms */
    uint32_t now_ms;        /* the UI clock: virtual in the mem backend (--input `wait N`), SDL ticks when live */
    int32_t hover_tile;     /* 1-based tile under the pointer, 0 = none */
    int live;               /* sdl on a real display without --input: the window stays until it is closed */
    int repaint;            /* the next present paints the whole canvas (first frame, resize) */
    int32_t seek_fill;      /* pixel width of the seek bar fill last drawn; -1 = not drawn since the last full repaint */
    int32_t vol_fill;       /* the same for the volume bar */
    char seek_length[16];   /* the length text last drawn, so a clock tick marks it dirty only when it changed */
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
    uint32_t transport_hits; /* transport button clicks sent to the engine (DUI-P4-1) */
    uint32_t home_parts;     /* bit set of the Home frame parts drawn (HP_*), the I230 proof that each is on screen */
    uint32_t tile_cards;     /* tile card surfaces drawn in the Home view */
    uint32_t frame_parts;    /* frame parts drawn: sidebar entries, Now Playing panel and transport buttons */
    int32_t shelf_q16[8];    /* horizontal scroll of each Home shelf in logical pixels, Q16 */
    int32_t ptr_x, ptr_y;    /* the last pointer position: the wheel scrolls the shelf under it */
    uint32_t shelf_plays;    /* shelf tiles activated (song or album sent to the engine) */
    int32_t home_y_q16;      /* vertical scroll of the whole Home content in logical pixels, Q16 (I247) */
    int32_t tile_menu;       /* open tile menu: 1 + shelf * 64 + tile, 0 = closed; menu_x and menu_y place it (I247) */
    uint32_t tile_queued;    /* tiles sent to the engine as queue next through the tile menu */
    uint32_t home_sig;       /* source and row count of the last HOME window drawn: a change redraws */
    char home_dump[2048];    /* shelves as text for the stats */
    sel_tag stats[64];
    char* input_at;         /* where the scripted input resumes after a turn that sent an engine line */
    uint32_t sends;         /* engine lines sent so far: the replay yields a turn so the shell pumps them */
    char library_dump[3072]; /* the Library page as text for the stats (DUI-P4-3) */
    uint32_t library_sig;    /* hash of the Library page last drawn: a change redraws */
    uint32_t lib_mode;       /* Library page mode: 0 composers, 1 tree, 2 column browser (DUI-P4-3 slice B) */
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

/* Same as draw_label but on the frame colour with a type size and an ink: the player bar texts. */
static void draw_frame_text(desk* d, sel_gui_rect box, const char* s, int px, int mark) {
    sel_gui_fill_rect(d->canvas, box, ARGB(d->theme.token[SEL_GUI_TOKEN_FRAME]));
    draw_text(d, box, s, 0, px, ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT_MUTED]));
    if (mark) {
        sel_gui_dirty_add(d->canvas, box);
    }
}

/* A slim rounded bar (seek, volume): the track token behind, the text colour in front, as in the mockup player. The
 * whole bar is repainted into the canvas (the result is the same pixels wherever it did not change), but only the span
 * between the previous and the new fill, plus one bar height of rounded cap on each side, is marked dirty (plan 2.11).
 * *prev is the fill drawn last, or -1 when the bar was not drawn since the last full repaint. */
static void draw_bar(desk* d, sel_gui_rect box, uint32_t q16, int32_t* prev) {
    int32_t fill = (int32_t)(((uint64_t)q16 * (uint32_t)box.w) >> 16);
    int32_t m = lpx(d, 2), lo = 0, hi = box.w;
    sel_gui_rect wide = rect_of(box.x, box.y - m, box.w, box.h + 2 * m);
    sel_gui_fill_rect(d->canvas, wide, ARGB(d->theme.token[SEL_GUI_TOKEN_FRAME]));
    sel_gui_round_rect(d->canvas, box, box.h / 2, ARGB(d->theme.token[SEL_GUI_TOKEN_TRACK]));
    if (fill >= box.h) {
        sel_gui_round_rect(d->canvas, rect_of(box.x, box.y, fill, box.h), box.h / 2,
                           ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT]));
    }
    if (*prev >= 0) {
        lo = (*prev < fill ? *prev : fill) - box.h;
        hi = (*prev < fill ? fill : *prev) + box.h;
        lo = lo < 0 ? 0 : lo;
        hi = hi > box.w ? box.w : hi;
    }
    *prev = fill;
    if (hi > lo) {
        sel_gui_dirty_add(d->canvas, rect_of(box.x + lo, wide.y, hi - lo, wide.h));
    }
}

/* Bits of desk.home_parts. */
enum {
    HP_NAV = 1, HP_BRAND = 2, HP_TILE_CARD = 4, HP_PLAY_HOVER = 8, HP_PLAYER_ART = 16, HP_PLAYER_TITLE = 32,
    HP_SEEK = 64, HP_GREETING = 128, HP_SECTION = 256, HP_ARTIST_LINE = 512, HP_SEARCH_ICON = 1024,
    HP_HEADER_TINT = 2048, HP_PANELS = 4096, HP_CHIPS = 8192
};

/* A region as a raised panel (Main.dc.html: panels on the frame colour, large radius): half the spacing token off each
 * side leaves one spacing token of frame colour between neighbours. */
static sel_gui_rect draw_panel(desk* d, const char* name) {
    sel_gui_rect r = region_rect(d, name);
    int32_t g = lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_SPACING]) / 2;
    sel_gui_rect pr = rect_of(r.x + g, r.y + g, r.w - 2 * g, r.h - 2 * g);
    if (pr.w > 0 && pr.h > 0) {
        /* The frame colour first: the rounded corners blend with what is under them, so painting a panel again over
         * its own earlier corners would change them. The region is repainted whole each time (idempotent). */
        sel_gui_fill_rect(d->canvas, r, ARGB(d->theme.token[SEL_GUI_TOKEN_FRAME]));
        sel_gui_round_rect(d->canvas, pr, lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_RADIUS_LARGE]),
                           ARGB(d->theme.token[SEL_GUI_TOKEN_PANEL]));
        sel_gui_dirty_add(d->canvas, r);
        d->home_parts |= HP_PANELS;
    }
    return pr;
}

/* One widget in its rectangle inside its layout region. Only the dirty widgets run, so a clock tick touches the
 * clock, the remaining time and the progress bar and nothing else. */
static void queue_title(const sel_state* st, uint32_t i, char* out, size_t cap);
static void draw_home(desk* d);
static void draw_transport(desk* d);
static void draw_sidebar(desk* d);
static void draw_nowplaying(desk* d);
static void draw_player_left(desk* d);

static void draw_widget(desk* d, uint32_t bit) {
    const sel_uim_frame* f = &d->uim.frame;
    int32_t sp = lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_SPACING]);
    sel_gui_rect pl = region_rect(d, "player"), mn = region_rect(d, "main");
    int32_t y0 = pl.y + sp;
    if (bit == SEL_UIM_DIRTY_SONG) {
        draw_player_left(d);
        if (d->home && !d->songs && d->open_tile == 0) { /* the Home header replaces the song card */
            draw_nowplaying(d);
            return;
        }
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
        draw_nowplaying(d);
    } else if (bit == SEL_UIM_DIRTY_QUEUE) {
        draw_sidebar(d);
        if (d->home) {
            draw_home(d);
        }
    } else if (bit == SEL_UIM_DIRTY_TRANSPORT) {
        draw_transport(d);
    } else if (bit == SEL_UIM_DIRTY_PROGRESS) {
        /* The seek row (mockup): times at both ends of a slim bar, centred over the transport buttons. */
        int32_t lw = lpx(d, 40), tw = lpx(d, 560), x0;
        if (tw > pl.w - 2 * lpx(d, 300)) {
            tw = pl.w - 2 * lpx(d, 300);
        }
        if (tw < 4 * lw) {
            tw = 4 * lw;
        }
        x0 = pl.x + pl.w / 2 - tw / 2;
        int len_changed = strncmp(d->seek_length, f->length, sizeof d->seek_length) != 0;
        draw_frame_text(d, rect_of(x0, y0, lw, lpx(d, 16)), f->clock, 12, 1);
        draw_bar(d, rect_of(x0 + lw + sp, y0 + lpx(d, 6), tw - 2 * lw - 2 * sp, lpx(d, 4)), f->progress_q16,
                 &d->seek_fill);
        draw_frame_text(d, rect_of(x0 + tw - lw, y0, lw, lpx(d, 16)), f->length, 12, len_changed);
        snprintf(d->seek_length, sizeof d->seek_length, "%s", f->length);
        d->home_parts |= HP_SEEK;
    } else if (bit == SEL_UIM_DIRTY_VOLUME) {
        int32_t v = f->volume < 0 ? 0 : f->volume > 100 ? 100 : f->volume;
        draw_bar(d, rect_of(pl.x + pl.w - sp - lpx(d, 96), y0 + lpx(d, 40), lpx(d, 96), lpx(d, 4)),
                 (uint32_t)v * 65536u / 100u, &d->vol_fill);
    } else if (bit == SEL_UIM_DIRTY_STATUS) {
        draw_frame_text(d, rect_of(pl.x + pl.w - sp - lpx(d, 200), y0, lpx(d, 200), lpx(d, 16)), f->status, 12, 1);
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

/* Home layout (Main.dc.html), logical pixels from the main panel's top: the header with the search field, the
 * greeting, the filter chips, the section heading, then the tile grid. */
enum { HOME_MARGIN = 24, HOME_SEARCH_Y = 16, HOME_SEARCH_H = 40, HOME_SEARCH_MAX = 360, HOME_GREET_Y = 72,
       HOME_GREET_H = 40, HOME_CHIPS_Y = 124, HOME_CHIP_H = 32, HOME_SECTION_Y = 172, HOME_SECTION_H = 32,
       HOME_TILES_Y = 212, HOME_TILE_GAP = 4, HOME_TINT_H = 300, HOME_PLAY_SIZE = 40, HOME_PLAY_INSET = 8,
       HOME_TILE_TEXT_GAP = 8, HOME_TITLE_H = 18, HOME_META_H = 16, HOME_BRAND = 28 };

/* Where tile card i sits (DUI-P2-6, Main.dc.html): a grid of cards, each the art plus the tile padding token on every
 * side and the title and meta lines below, columns as many as fit. */
static sel_gui_rect tile_rect(desk* d, uint32_t i) {
    int32_t pad = lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_TILE_PAD]);
    sel_gui_rect mn = region_rect(d, "main");
    int32_t art = lpx(d, g_tile_steps[d->tile_step]);
    int32_t gap = lpx(d, HOME_TILE_GAP);
    int32_t cw = art + 2 * pad;
    int32_t cols = (mn.w - 2 * lpx(d, HOME_MARGIN) + 2 * pad + gap) / (cw + gap);
    int32_t h = pad + art + lpx(d, HOME_TILE_TEXT_GAP + HOME_TITLE_H + HOME_META_H) + pad;
    if (cols < 1) {
        cols = 1;
    }
    return rect_of(mn.x + lpx(d, HOME_MARGIN) - pad + (int32_t)(i % (uint32_t)cols) * (cw + gap),
                   mn.y + lpx(d, HOME_TILES_Y) - pad + (int32_t)(i / (uint32_t)cols) * (h + gap), cw, h);
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

/* 64-bit FNV-1a over the pixels of r. */
static uint64_t rect_hash(const sel_gui_canvas* c, sel_gui_rect r) {
    uint64_t h = SEL_GUI_FNV64_BASIS;
    int32_t x, y;
    for (y = r.y; y < r.y + r.h; y++) {
        for (x = r.x; x < r.x + r.w; x++) {
            h = sel_gui_fnv64_word(h, sel_gui_pixel(c, x, y));
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

enum { ANIM_KEY_VIEW = 1, ANIM_KEY_HOVER = 2, ANIM_KEY_TIP = 3 };

/* The Home header (Main.dc.html): the tinted band at the top of the main panel, the search field with its icon, the
 * greeting, the filter chips and the section heading. */
static void draw_home_header(desk* d, sel_gui_rect mn, int with_section) {
    uint32_t text = ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT]);
    uint32_t muted = ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT_MUTED]);
    int32_t rad = lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_RADIUS_LARGE]);
    int32_t mg = lpx(d, HOME_MARGIN), sp = lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_SPACING]);
    int32_t sw = mn.w - 2 * mg < lpx(d, HOME_SEARCH_MAX) ? mn.w - 2 * mg : lpx(d, HOME_SEARCH_MAX);
    sel_gui_rect sr = rect_of(mn.x + mg, mn.y + lpx(d, HOME_SEARCH_Y), sw, lpx(d, HOME_SEARCH_H));
    const sel_gui_atlas* chip = desk_font(d, 1, 13);
    static const char* const chips[] = {"All", "Songs", "Composers", "Playlists"};
    int32_t x, k, ic = lpx(d, 18);
    sel_gui_round_rect(d->canvas, rect_of(mn.x, mn.y, mn.w, 2 * rad), rad, ARGB(d->theme.token[SEL_GUI_TOKEN_HEADER_TINT]));
    sel_gui_gradient(d->canvas, rect_of(mn.x, mn.y + rad, mn.w, lpx(d, HOME_TINT_H) - rad), 1,
                     ARGB(d->theme.token[SEL_GUI_TOKEN_HEADER_TINT]), ARGB(d->theme.token[SEL_GUI_TOKEN_PANEL]));
    d->home_parts |= HP_HEADER_TINT;
    sel_gui_round_rect(d->canvas, sr, rad, ARGB(d->theme.token[SEL_GUI_TOKEN_SURFACE]));
    sel_gui_icon_draw(d->canvas, &d->icons, SEL_GUI_ICON_SEARCH, rect_of(sr.x + lpx(d, 14), sr.y + (sr.h - ic) / 2, ic, ic), ic,
                      muted);
    d->home_parts |= HP_SEARCH_ICON;
    draw_text(d, rect_of(sr.x + lpx(d, 42), sr.y, sr.w - lpx(d, 56), sr.h), "Search songs, composers, playlists", 0, 14, muted);
    draw_text(d, rect_of(mn.x + mg, mn.y + lpx(d, HOME_GREET_Y), mn.w - 2 * mg, lpx(d, HOME_GREET_H)), "Welcome back", 1, 32, text);
    d->home_parts |= HP_GREETING;
    x = mn.x + mg;
    for (k = 0; k < 4 && chip != NULL; k++) {
        int32_t w = sel_gui_text_width(chip, chips[k], strlen(chips[k])) + 2 * lpx(d, 14);
        sel_gui_rect cr = rect_of(x, mn.y + lpx(d, HOME_CHIPS_Y), w, lpx(d, HOME_CHIP_H));
        sel_gui_round_rect(d->canvas, cr, lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_RADIUS]),
                           ARGB(d->theme.token[k == 0 ? SEL_GUI_TOKEN_ACCENT : SEL_GUI_TOKEN_SURFACE_RAISED]));
        draw_text(d, rect_of(cr.x + lpx(d, 14), cr.y, w, cr.h), chips[k], 1, 13,
                  ARGB(d->theme.token[k == 0 ? SEL_GUI_TOKEN_ACCENT_TEXT : SEL_GUI_TOKEN_TEXT]));
        x += w + sp;
    }
    d->home_parts |= HP_CHIPS;
    if (with_section) {
        draw_text(d, rect_of(mn.x + mg, mn.y + lpx(d, HOME_SECTION_Y), mn.w - 2 * mg, lpx(d, HOME_SECTION_H)), "Recently played", 1, 22, text);
        d->home_parts |= HP_SECTION;
    }
}

/* ---- Home shelves (DUI-P4-2): the HOME view source (engine) as rows of tiles. A heading row starts a shelf; the
 * song and album rows after it are its tiles; a row without a key (the "new this week" count line) is plain text.
 * Each shelf scrolls horizontally on its own. ---- */
enum { SHELF_MAX = 6, SHELF_HEAD_H = 40, SHELF_GAP = 16, SHELF_WINDOW = 32, SHELF_NONE = 0x7FFFFFFF };

typedef struct shelf {
    uint32_t head;  /* index of the heading in the view window */
    uint32_t first; /* index of the first row after it */
    uint32_t tiles; /* rows with a key */
    uint32_t rows;  /* rows after the heading */
} shelf;

/* Splits the HOME window into shelves; 0 when the view is not the HOME source. */
static uint32_t home_shelves(const sel_state* st, shelf* out) {
    const sel_view_state* v = st->view;
    uint32_t i, n = 0;
    if (v == NULL || v->source != SEL_VIEW_HOME) {
        return 0;
    }
    for (i = 0; i < v->row_n; i++) {
        if (v->rows[i].kind == SEL_ROW_HEADING) {
            if (n == SHELF_MAX) {
                break;
            }
            out[n].head = i;
            out[n].first = i + 1u;
            out[n].tiles = 0;
            out[n].rows = 0;
            n++;
        } else if (n > 0u) {
            out[n - 1u].rows++;
            out[n - 1u].tiles += v->rows[i].key != NULL;
        }
    }
    return n;
}

/* 1 when the HOME source holds at least one tile: the shelves replace the queue grid. */
static int home_shelf_mode(const sel_state* st);

/* 1 when Home has nothing to show: the HOME source holds no tile (an empty library still lists its shelf headings)
 * and no song is queued. The view stays on Home and draws the empty state (one line and a Browse files action). */
static int home_empty(const sel_state* st) {
    return st->view != NULL && st->view->source == SEL_VIEW_HOME && !home_shelf_mode(st) && st->queue_count == 0u;
}

/* The Browse files action of the empty Home state. */
static sel_gui_rect home_empty_button(desk* d) {
    sel_gui_rect mn = region_rect(d, "main");
    return rect_of(mn.x + lpx(d, HOME_MARGIN), mn.y + lpx(d, HOME_SECTION_Y + 40), lpx(d, 140), lpx(d, HOME_CHIP_H));
}

/* 1 when the HOME source holds at least one tile: the shelves replace the queue grid. */
static int home_shelf_mode(const sel_state* st) {
    shelf sh[SHELF_MAX];
    uint32_t i, n = home_shelves(st, sh);
    for (i = 0; i < n; i++) {
        if (sh[i].tiles > 0u) {
            return 1;
        }
    }
    return 0;
}

/* Window row of tile k of a shelf (keyed rows only), or UINT32_MAX. */
static uint32_t shelf_tile_row(const sel_state* st, const shelf* sh, uint32_t k) {
    uint32_t i;
    for (i = sh->first; i < sh->first + sh->rows; i++) {
        if (st->view->rows[i].key != NULL) {
            if (k == 0u) {
                return i;
            }
            k--;
        }
    }
    return 0xFFFFFFFFu;
}

static int32_t shelf_card_h(desk* d) {
    int32_t pad = lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_TILE_PAD]);
    return pad + lpx(d, g_tile_steps[d->tile_step]) + lpx(d, HOME_TILE_TEXT_GAP + HOME_TITLE_H + HOME_META_H) + pad;
}

/* Whole Home content height in logical pixels: the header, then every shelf. */
static int32_t home_content_h(desk* d, uint32_t shelves) {
    return HOME_SECTION_Y + (int32_t)shelves * (SHELF_HEAD_H + SHELF_GAP + shelf_card_h(d) / (lpx(d, 1) > 0 ? lpx(d, 1) : 1));
}

/* The vertical scroll limit in Q16 logical pixels: the content beyond the main panel height, 0 when it fits. */
static int32_t home_max_y_q16(desk* d) {
    shelf sh[SHELF_MAX];
    uint32_t n = home_shelves(sel_engine_state(d->engine), sh);
    int32_t unit = lpx(d, 1) > 0 ? lpx(d, 1) : 1;
    int32_t over = home_content_h(d, n) - region_rect(d, "main").h / unit;
    return over > 0 ? over << 16 : 0;
}

static int32_t shelf_top(desk* d, uint32_t s) {
    sel_gui_rect mn = region_rect(d, "main");
    return mn.y - lpx(d, d->home_y_q16 >> 16) + lpx(d, HOME_SECTION_Y) + (int32_t)s * (lpx(d, SHELF_HEAD_H + SHELF_GAP) + shelf_card_h(d));
}

/* The scroll limit of a shelf in Q16 logical pixels: the tiles' width beyond the visible width. */
static int32_t shelf_max_q16(desk* d, const shelf* sh) {
    int32_t pad = lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_TILE_PAD]);
    sel_gui_rect mn = region_rect(d, "main");
    int32_t cw = lpx(d, g_tile_steps[d->tile_step]) + 2 * pad + lpx(d, HOME_TILE_GAP);
    int32_t total = (int32_t)sh->tiles * cw;
    int32_t vis = mn.w - 2 * lpx(d, HOME_MARGIN) + 2 * pad;
    int32_t unit = lpx(d, 1) > 0 ? lpx(d, 1) : 1;
    return total > vis ? ((total - vis) / unit) << 16 : 0;
}

static sel_gui_rect shelf_tile_rect(desk* d, uint32_t s, uint32_t k) {
    int32_t pad = lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_TILE_PAD]);
    sel_gui_rect mn = region_rect(d, "main");
    int32_t cw = lpx(d, g_tile_steps[d->tile_step]) + 2 * pad;
    return rect_of(mn.x + lpx(d, HOME_MARGIN) - pad + (int32_t)k * (cw + lpx(d, HOME_TILE_GAP)) - lpx(d, d->shelf_q16[s] >> 16),
                   shelf_top(d, s) + lpx(d, SHELF_HEAD_H) - pad, cw, shelf_card_h(d));
}

/* Writes "shelf S tile K heading|key|title" lines in order into home_dump; returns the shelf count. */
static uintptr_t home_dump_build(desk* d) {
    const sel_state* st = sel_engine_state(d->engine);
    shelf sh[SHELF_MAX];
    uint32_t s, k, n = home_shelves(st, sh);
    size_t o = 0;
    d->home_dump[0] = '\0';
    for (s = 0; s < n; s++) {
        for (k = 0; k < sh[s].tiles && o + 200u < sizeof d->home_dump; k++) {
            const sel_view_row* r = &st->view->rows[shelf_tile_row(st, &sh[s], k)];
            o += (size_t)snprintf(d->home_dump + o, sizeof d->home_dump - o, "shelf %u tile %u %s|%s|%s\n", (unsigned)s, (unsigned)k,
                                  st->view->rows[sh[s].head].text, r->key, r->text);
        }
    }
    return (uintptr_t)n;
}

/* The shelf and tile under a canvas point, or 0. */
static int shelf_hit(desk* d, int32_t x, int32_t y, uint32_t* ps, uint32_t* pk) {
    const sel_state* st = sel_engine_state(d->engine);
    sel_gui_rect mn = region_rect(d, "main");
    shelf sh[SHELF_MAX];
    uint32_t s, k, n = home_shelves(st, sh);
    for (s = 0; s < n; s++) {
        for (k = 0; k < sh[s].tiles; k++) {
            sel_gui_rect t = shelf_tile_rect(d, s, k);
            if (x >= t.x && x < t.x + t.w && y >= t.y && y < t.y + t.h && x >= mn.x && x < mn.x + mn.w) {
                *ps = s;
                *pk = k;
                return 1;
            }
        }
    }
    return 0;
}

/* The shelf whose band holds the canvas row y, or SHELF_NONE. */
static uint32_t shelf_at_y(desk* d, int32_t y) {
    shelf sh[SHELF_MAX];
    uint32_t s, n = home_shelves(sel_engine_state(d->engine), sh);
    for (s = 0; s < n; s++) {
        int32_t top = shelf_top(d, s);
        if (y >= top && y < top + lpx(d, SHELF_HEAD_H) + shelf_card_h(d)) {
            return s;
        }
    }
    return SHELF_NONE;
}

/* Activates a shelf tile through the engine's own HOME path: the cursor goes to its row and VIEW_ACTIVATE plays a
 * song or opens an album search; the HOME source is then asked for again so the shelves follow the new history. */
static void shelf_activate(desk* d, uint32_t s, uint32_t k) {
    const sel_state* st = sel_engine_state(d->engine);
    shelf sh[SHELF_MAX];
    char line[48];
    uint32_t row;
    if (home_shelves(st, sh) <= s) {
        return;
    }
    row = shelf_tile_row(st, &sh[s], k);
    if (row == 0xFFFFFFFFu) {
        return;
    }
    snprintf(line, sizeof line, "VIEW_CURSOR %u", (unsigned)(st->view->window_start + row));
    desk_send(d, line);
    desk_send(d, "VIEW_ACTIVATE");
    desk_send(d, "VIEW_SOURCE home");
    d->shelf_plays++;
}

/* Scrolls the whole Home content by wheel notches (positive = toward the end), clamped to the overflow. */
static void home_scroll_y(desk* d, int32_t notches_q16) {
    int64_t v = (int64_t)d->home_y_q16 + (int64_t)notches_q16 * SCROLL_NOTCH;
    int32_t max = home_max_y_q16(d);
    d->home_y_q16 = v < 0 ? 0 : (v > max ? max : (int32_t)v);
}

/* 1 when canvas row y is over the tile cards of a shelf (below its heading), where the wheel scrolls that shelf. */
static int shelf_over_cards(desk* d, int32_t y) {
    uint32_t s = shelf_at_y(d, y);
    return s != SHELF_NONE && y >= shelf_top(d, s) + lpx(d, SHELF_HEAD_H);
}

/* Right-click on a song tile opens its menu with the one verb a tile cannot send by itself, Play Next; an album tile
 * has no song to queue, so it opens no menu. */
static int shelf_menu_open(desk* d, int32_t x, int32_t y) {
    const sel_state* st = sel_engine_state(d->engine);
    shelf sh[SHELF_MAX];
    uint32_t s, k, row;
    if (!shelf_hit(d, x, y, &s, &k) || home_shelves(st, sh) <= s) {
        return 0;
    }
    row = shelf_tile_row(st, &sh[s], k);
    if (row == 0xFFFFFFFFu || st->view->rows[row].album) {
        return 0;
    }
    d->tile_menu = 1 + (int32_t)(s * 64u + k);
    d->menu_x = x;
    d->menu_y = y;
    d->menus++;
    return 1;
}

/* A click while the tile menu is open: the item sends QUEUE_PLAY_NEXT with the tile's song key (existing verb, as the
 * songs list menu); any click closes the menu. Returns 1 (the picture changed). */
static int shelf_menu_click(desk* d, int32_t x, int32_t y) {
    const sel_state* st = sel_engine_state(d->engine);
    shelf sh[SHELF_MAX];
    sel_gui_rect m = rect_of(d->menu_x, d->menu_y, lpx(d, SONG_MENU_W), lpx(d, SONG_MENU_ITEM_H));
    uint32_t s = (uint32_t)(d->tile_menu - 1) / 64u, k = (uint32_t)(d->tile_menu - 1) % 64u;
    d->tile_menu = 0;
    if (in_rect(m, x, y) && home_shelves(st, sh) > s) {
        uint32_t row = shelf_tile_row(st, &sh[s], k);
        if (row != 0xFFFFFFFFu) {
            char line[320];
            snprintf(line, sizeof line, "QUEUE_PLAY_NEXT %s", st->view->rows[row].key);
            desk_send(d, line);
            d->tile_queued++;
        }
    }
    draw_home(d);
    return 1;
}

/* Scrolls the shelf under canvas row y by wheel notches (positive = toward the end). */
static void shelf_scroll(desk* d, int32_t y, int32_t notches_q16) {
    shelf sh[SHELF_MAX];
    uint32_t s = shelf_at_y(d, y);
    int64_t v;
    if (s == SHELF_NONE || home_shelves(sel_engine_state(d->engine), sh) <= s) {
        return;
    }
    v = (int64_t)d->shelf_q16[s] + (int64_t)notches_q16 * SCROLL_NOTCH;
    if (v < 0) {
        v = 0;
    }
    if (v > shelf_max_q16(d, &sh[s])) {
        v = shelf_max_q16(d, &sh[s]);
    }
    d->shelf_q16[s] = (int32_t)v;
}

static void draw_shelves(desk* d, sel_gui_rect mn) {
    const sel_state* st = sel_engine_state(d->engine);
    int32_t pad = lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_TILE_PAD]);
    int32_t mg = lpx(d, HOME_MARGIN);
    int32_t hv = sel_gui_tween_value(&d->tl, ANIM_KEY_HOVER, 65536);
    uint32_t panel = ARGB(d->theme.token[SEL_GUI_TOKEN_PANEL]);
    shelf sh[SHELF_MAX];
    uint32_t s, k, flat = 0, n = home_shelves(st, sh);
    for (s = 0; s < n; s++) {
        const sel_view_row* head = &st->view->rows[sh[s].head];
        int32_t top = shelf_top(d, s);
        if (top >= mn.y + mn.h) {
            break;
        }
        draw_text(d, rect_of(mn.x + mg, top, mn.w - 2 * mg, lpx(d, HOME_SECTION_H)), head->text, 1, 22,
                  ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT]));
        d->home_parts |= HP_SECTION;
        if (sh[s].tiles == 0u && sh[s].rows > 0u) { /* a count line, no tiles */
            draw_text(d, rect_of(mn.x + mg, top + lpx(d, SHELF_HEAD_H), mn.w - 2 * mg, lpx(d, HOME_TITLE_H)),
                      st->view->rows[sh[s].first].text, 0, 14, ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT_MUTED]));
        }
        for (k = 0; k < sh[s].tiles; k++, flat++) {
            const sel_view_row* r = &st->view->rows[shelf_tile_row(st, &sh[s], k)];
            sel_gui_rect t = shelf_tile_rect(d, s, k);
            sel_gui_rect ar = rect_of(t.x + pad, t.y + pad, t.w - 2 * pad, t.w - 2 * pad);
            int hovered = d->hover_tile > 0 && (uint32_t)d->hover_tile - 1u == flat;
            const char* dot = r->key != NULL ? strrchr(r->key, '.') : NULL;
            char format[16];
            size_t fi = 0;
            if (t.x + t.w <= mn.x || t.x >= mn.x + mn.w) {
                continue;
            }
            while (!r->album && dot != NULL && dot[1 + fi] != '\0' && fi + 1u < sizeof format) {
                format[fi] = dot[1 + fi];
                fi++;
            }
            format[fi] = '\0';
            sel_gui_fill_rect(d->canvas, t, panel);
            sel_gui_round_rect(d->canvas, t, lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_RADIUS_LARGE]),
                               hovered ? desk_mix(panel, ARGB(d->theme.token[SEL_GUI_TOKEN_SURFACE]), (uint32_t)hv) : panel);
            d->tile_cards++;
            d->home_parts |= HP_TILE_CARD;
            if (d->shadow_ready) {
                sel_gui_shadow_draw(d->canvas, &d->shadow, ar);
            }
            sel_gui_genart_draw(d->canvas, ar, r->album ? "album" : format, sel_gui_str_hash(r->text));
            draw_text(d, rect_of(ar.x, ar.y + ar.h + lpx(d, HOME_TILE_TEXT_GAP), ar.w, lpx(d, HOME_TITLE_H)), r->text, 1, 14,
                      ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT]));
            draw_text(d, rect_of(ar.x, ar.y + ar.h + lpx(d, HOME_TILE_TEXT_GAP + HOME_TITLE_H), ar.w, lpx(d, HOME_META_H)),
                      r->album ? "Album" : format, 0, 13, ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT_MUTED]));
            d->home_parts |= HP_ARTIST_LINE;
            if (hovered && hv > 0) {
                int32_t ps = lpx(d, HOME_PLAY_SIZE), in = lpx(d, HOME_PLAY_INSET);
                sel_gui_rect pb = rect_of(ar.x + ar.w - in - ps, ar.y + ar.h - in - ps, ps, ps);
                sel_gui_round_rect(d->canvas, pb, ps / 2, ARGB(d->theme.token[SEL_GUI_TOKEN_ACCENT]));
                sel_gui_icon_draw(d->canvas, &d->icons, SEL_GUI_ICON_PLAY, rect_of(pb.x + ps / 4, pb.y + ps / 4, ps / 2, ps / 2),
                                  ps / 2, ARGB(d->theme.token[SEL_GUI_TOKEN_ACCENT_TEXT]));
                d->home_parts |= HP_PLAY_HOVER;
            }
        }
    }
}

/* ---- Library page (DUI-P4-3 slice A): the engine's composers with their song counts, a composer drilldown and the
 * Stats page (F24) beside them. Every number comes from the engine state (COMPOSERS_SHOW, STATS_SHOW).
 * TODO DUI-P4-3 slice B/C: the tree, column browser, sources, Files and History pages hook in here, as further pages
 * of this view chosen by a segmented control above the list. */
enum { LIB_MARGIN = 24, LIB_TITLE_Y = 24, LIB_TITLE_H = 40, LIB_LIST_Y = 88, LIB_ROW_H = 48, LIB_STATS_W = 300 };

/* Next NUL-separated field after `p`. */
static const char* lib_next(const char* p) {
    return p + strlen(p) + 1u;
}

static sel_gui_rect lib_row_rect(desk* d, sel_gui_rect mn, uint32_t i) {
    int32_t mg = lpx(d, LIB_MARGIN);
    int32_t w = mn.w - 3 * mg - lpx(d, LIB_STATS_W);
    return rect_of(mn.x + mg, mn.y + lpx(d, LIB_LIST_Y) + (int32_t)i * lpx(d, LIB_ROW_H), w > 0 ? w : 0, lpx(d, LIB_ROW_H));
}

/* The Library page modes (slice B): the composers list, the tree and the column browser. The last two show the engine's
 * own view rows (VIEW_STYLE tree / drilldown over a LIBRARY_SEARCH result), the same commands the text UI sends. */
enum { LIB_MODES = 3, LIB_TAB_Y = LIB_TITLE_Y + LIB_TITLE_H, LIB_TAB_W = 120, LIB_TAB_H = 24, LIB_WINDOW_ROWS = 12 };
static const char* const lib_mode_name[LIB_MODES] = {"Composers", "Tree", "Columns"};

static sel_gui_rect lib_tab_rect(desk* d, sel_gui_rect mn, uint32_t i) {
    return rect_of(mn.x + lpx(d, LIB_MARGIN) + (int32_t)i * lpx(d, LIB_TAB_W), mn.y + lpx(d, LIB_TAB_Y), lpx(d, LIB_TAB_W), lpx(d, LIB_TAB_H));
}

/* The crumb after the tabs: the values picked in the column browser; a click on it clears them. */
static sel_gui_rect lib_crumb_rect(desk* d, sel_gui_rect mn) {
    return rect_of(mn.x + lpx(d, LIB_MARGIN) + LIB_MODES * lpx(d, LIB_TAB_W), mn.y + lpx(d, LIB_TAB_Y), lpx(d, 300), lpx(d, LIB_TAB_H));
}

static int lib_view_on(const desk* d, const sel_state* st) {
    return d->lib_mode != 0u && st->view != NULL && st->view->source == SEL_VIEW_QUERY &&
           st->view->style == (d->lib_mode == 1u ? (uint32_t)SEL_VIEW_STYLE_TREE : (uint32_t)SEL_VIEW_STYLE_DRILLDOWN);
}

static uint32_t lib_hash(const desk* d, const sel_state* st) {
    uint32_t h = st->composer_page != NULL ? 5381u : 0u;
    const char* p = st->composer_page;
    for (; p != NULL && *p != '\0'; p++) {
        h = h * 33u + (uint8_t)*p;
    }
    h += d->lib_mode * 977u;
    if (lib_view_on(d, st)) {
        uint32_t k;
        h += st->view->row_count * 31u + st->view->drill_n * 131u + st->view->cursor;
        for (k = 0; k < st->view->row_n; k++) {
            h += st->view->rows[k].expanded * (k + 7u) + st->view->rows[k].count * (k + 3u);
        }
    }
    return h + (st->composers != NULL ? 1u : 0u) + (st->stats != NULL ? 2u : 0u);
}

/* Writes "composer I canon|display|songs", "row I x y w h" (the click rectangle), "drill display|songs" and "stat label|value" lines into library_dump. */
static const char* library_dump_build(desk* d) {
    const sel_state* st = sel_engine_state(d->engine);
    size_t o = 0;
    uint32_t i = 0;
    const char* p;
    d->library_dump[0] = '\0';
    for (p = st->composers; p != NULL && *p != '\0' && o + 400u < sizeof d->library_dump; i++) {
        const char* disp = lib_next(p);
        const char* cnt = lib_next(disp);
        o += (size_t)snprintf(d->library_dump + o, sizeof d->library_dump - o, "composer %u %s|%s|%s\n", (unsigned)i, p, disp, cnt);
        {
            sel_gui_rect rr = lib_row_rect(d, region_rect(d, "main"), i);
            o += (size_t)snprintf(d->library_dump + o, sizeof d->library_dump - o, "row %u %d %d %d %d\n", (unsigned)i, (int)rr.x, (int)rr.y, (int)rr.w, (int)rr.h);
        }
        p = lib_next(cnt);
    }
    p = st->composer_page;
    if (p != NULL && o + 400u < sizeof d->library_dump) {
        o += (size_t)snprintf(d->library_dump + o, sizeof d->library_dump - o, "drill %s|%s\n", p, lib_next(p));
    }
    o += (size_t)snprintf(d->library_dump + o, sizeof d->library_dump - o, "mode %s\n", lib_mode_name[d->lib_mode]);
    for (i = 0; i < LIB_MODES; i++) {
        sel_gui_rect tr = lib_tab_rect(d, region_rect(d, "main"), i);
        o += (size_t)snprintf(d->library_dump + o, sizeof d->library_dump - o, "tab %u %d %d %d %d\n", (unsigned)i, (int)tr.x, (int)tr.y, (int)tr.w, (int)tr.h);
    }
    if (lib_view_on(d, st)) {
        const sel_view_state* v = st->view;
        for (i = 0; i < v->drill_n && i < 2u; i++) {
            o += (size_t)snprintf(d->library_dump + o, sizeof d->library_dump - o, "picked %u|%s\n", (unsigned)i, v->drill[i]);
        }
        for (i = 0; i < v->row_n && o + 400u < sizeof d->library_dump; i++) {
            const sel_view_row* vr = &v->rows[i];
            sel_gui_rect rr = lib_row_rect(d, region_rect(d, "main"), i);
            o += (size_t)snprintf(d->library_dump + o, sizeof d->library_dump - o, "vrow %u %s|%u|%u|%u|%s\n", (unsigned)i,
                                  vr->kind == (uint32_t)SEL_ROW_NODE ? "node" : "song", (unsigned)vr->depth, (unsigned)vr->count,
                                  (unsigned)vr->expanded, vr->text != NULL ? vr->text : "");
            o += (size_t)snprintf(d->library_dump + o, sizeof d->library_dump - o, "vrect %u %d %d %d %d\n", (unsigned)i, (int)rr.x, (int)rr.y, (int)rr.w, (int)rr.h);
        }
    }
    for (p = st->stats; p != NULL && *p != '\0' && o + 400u < sizeof d->library_dump;) {
        const char* val = lib_next(p);
        o += (size_t)snprintf(d->library_dump + o, sizeof d->library_dump - o, "stat %s|%s\n", p, val);
        p = lib_next(val);
    }
    return d->library_dump;
}

static void draw_library(desk* d) {
    const sel_state* st = sel_engine_state(d->engine);
    sel_gui_rect mn = draw_panel(d, "main");
    int32_t mg = lpx(d, LIB_MARGIN), sp = lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_SPACING]);
    uint32_t text = ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT]), muted = ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT_MUTED]);
    uint32_t i = 0;
    const char* p;
    sel_gui_rect col = rect_of(mn.x + mg, mn.y + lpx(d, LIB_TITLE_Y), mn.w - 2 * mg, lpx(d, LIB_TITLE_H));
    sel_gui_clip_push(d->canvas, mn);
    d->library_sig = lib_hash(d, st);
    if (st->composer_page != NULL) { /* drilldown: the composer, its song count and its songs */
        const char* cnt = lib_next(st->composer_page);
        char line[96];
        draw_text(d, col, st->composer_page, 1, 32, text);
        snprintf(line, sizeof line, "%s songs", cnt);
        draw_text(d, rect_of(col.x, col.y + col.h, col.w, lpx(d, 24)), line, 0, 14, muted);
        draw_text(d, rect_of(col.x, col.y + col.h + lpx(d, 24), col.w, lpx(d, 24)), "Back to all composers", 0, 14,
                  ARGB(d->theme.token[SEL_GUI_TOKEN_ACCENT]));
        for (p = lib_next(cnt); *p != '\0'; i++) {
            const char* name = lib_next(p);
            sel_gui_rect r = lib_row_rect(d, mn, i + 1u);
            draw_text(d, r, name, 0, 15, text);
            p = lib_next(name);
        }
    } else {
        uint32_t m;
        draw_text(d, col, "Library", 1, 32, text);
        for (m = 0; m < LIB_MODES; m++) { /* the mode tabs: the active one in the accent colour */
            sel_gui_rect tr = lib_tab_rect(d, mn, m);
            draw_text(d, tr, lib_mode_name[m], m == d->lib_mode, 14, m == d->lib_mode ? ARGB(d->theme.token[SEL_GUI_TOKEN_ACCENT]) : muted);
        }
        if (lib_view_on(d, st) && d->lib_mode == 2u && st->view->drill_n > 0u) {
            char crumb[160];
            snprintf(crumb, sizeof crumb, "Back from %s", st->view->drill[st->view->drill_n - 1u]);
            draw_text(d, lib_crumb_rect(d, mn), crumb, 0, 14, muted);
        }
        if (lib_view_on(d, st)) { /* the engine's tree or column rows: node rows with their song count, songs indented */
            uint32_t k;
            for (k = 0; k < st->view->row_n; k++) {
                const sel_view_row* vr = &st->view->rows[k];
                sel_gui_rect r = lib_row_rect(d, mn, k);
                char line[48];
                int node = vr->kind == (uint32_t)SEL_ROW_NODE;
                int32_t ind = (int32_t)vr->depth * lpx(d, 24);
                draw_text(d, rect_of(r.x + ind, r.y, r.w * 2 / 3 - ind, r.h), vr->text != NULL ? vr->text : "", node, 15,
                          node && vr->expanded ? ARGB(d->theme.token[SEL_GUI_TOKEN_ACCENT]) : text);
                if (node) {
                    snprintf(line, sizeof line, "%u songs", (unsigned)vr->count);
                    draw_text(d, rect_of(r.x + r.w * 2 / 3, r.y, r.w / 3, r.h), line, 0, 14, muted);
                }
                sel_gui_dirty_add(d->canvas, r);
            }
            p = NULL;
        } else {
        for (p = st->composers; p != NULL && *p != '\0'; i++) {
            const char* disp = lib_next(p);
            const char* cnt = lib_next(disp);
            sel_gui_rect r = lib_row_rect(d, mn, i);
            char line[48];
            draw_text(d, rect_of(r.x, r.y, r.w * 2 / 3, r.h), disp, 1, 15, text);
            snprintf(line, sizeof line, "%s songs", cnt);
            draw_text(d, rect_of(r.x + r.w * 2 / 3, r.y, r.w / 3, r.h), line, 0, 14, muted);
            sel_gui_dirty_add(d->canvas, r);
            p = lib_next(cnt);
        }
        }
    }
    if (st->stats != NULL) { /* the Stats page (F24): the library totals as label and value */
        sel_gui_rect sr = rect_of(mn.x + mn.w - mg - lpx(d, LIB_STATS_W), mn.y + lpx(d, LIB_LIST_Y), lpx(d, LIB_STATS_W), lpx(d, 28));
        draw_text(d, sr, "Statistics", 1, 18, text);
        sr.y += lpx(d, 28) + sp;
        for (p = st->stats; *p != '\0'; sr.y += lpx(d, 24)) {
            const char* val = lib_next(p);
            draw_text(d, rect_of(sr.x, sr.y, sr.w * 2 / 3, lpx(d, 24)), p, 0, 14, muted);
            draw_text(d, rect_of(sr.x + sr.w * 2 / 3, sr.y, sr.w / 3, lpx(d, 24)), val, 1, 14, text);
            p = lib_next(val);
        }
    }
    sel_gui_clip_pop(d->canvas);
    d->home_parts |= HP_SECTION;
}

/* Switches the Library page mode. Tree and columns list the library's songs through the engine view, the sequence the
 * text UI's library screen sends (LIBRARY_SEARCH, VIEW_SOURCE query, VIEW_STYLE, VIEW_GROUP format). */
static int lib_set_mode(desk* d, uint32_t mode) {
    char line[64];
    if (mode == d->lib_mode) {
        return 0;
    }
    d->lib_mode = mode;
    if (mode == 0u) {
        desk_send(d, "VIEW_STYLE regular");
        desk_send(d, "VIEW_SOURCE queue");
    } else {
        desk_send(d, "LIBRARY_SEARCH \"\"");
        desk_send(d, "VIEW_SOURCE query");
        snprintf(line, sizeof line, "VIEW_WINDOW %u", (unsigned)LIB_WINDOW_ROWS);
        desk_send(d, line);
        desk_send(d, mode == 1u ? "VIEW_STYLE tree" : "VIEW_STYLE drilldown");
        desk_send(d, "VIEW_GROUP format");
    }
    return 1;
}

/* A click on a composer row drills into it (COMPOSERS_SHOW canon); on the back line it returns to the list. */
static int library_click(desk* d, int32_t x, int32_t y) {
    const sel_state* st = sel_engine_state(d->engine);
    sel_gui_rect mn = region_rect(d, "main");
    uint32_t i = 0;
    const char* p;
    char line[256];
    size_t o;
    if (st->composers == NULL) {
        return 0;
    }
    if (st->composer_page == NULL) {
        uint32_t m;
        for (m = 0; m < LIB_MODES; m++) {
            if (in_rect(lib_tab_rect(d, mn, m), x, y)) {
                return lib_set_mode(d, m);
            }
        }
        if (lib_view_on(d, st)) {
            uint32_t k;
            if (d->lib_mode == 2u && st->view->drill_n > 0u && in_rect(lib_crumb_rect(d, mn), x, y)) {
                return desk_send(d, "VIEW_DRILL 0 ..");
            }
            for (k = 0; k < st->view->row_n; k++) {
                if (in_rect(lib_row_rect(d, mn, k), x, y)) {
                    snprintf(line, sizeof line, "VIEW_CURSOR %u", (unsigned)(st->view->window_start + k));
                    desk_send(d, line);
                    return desk_send(d, "VIEW_ACTIVATE"); /* a node expands or drills in, a song plays */
                }
            }
            return 0;
        }
    }
    if (st->composer_page != NULL) {
        sel_gui_rect back = rect_of(mn.x + lpx(d, LIB_MARGIN), mn.y + lpx(d, LIB_TITLE_Y + LIB_TITLE_H + 24), mn.w / 2, lpx(d, 24));
        if (in_rect(back, x, y)) {
            return desk_send(d, "COMPOSERS_SHOW");
        }
        return 0;
    }
    for (p = st->composers; *p != '\0'; i++) {
        const char* disp = lib_next(p);
        if (in_rect(lib_row_rect(d, mn, i), x, y)) {
            /* A canon holds spaces: one quoted argument; the codec reads a backslash as the escape of the next character. */
            o = (size_t)snprintf(line, sizeof line, "COMPOSERS_SHOW \"");
            for (const char* c = p; *c != '\0' && o + 3u < sizeof line; c++) {
                if (*c == '"' || *c == '\\') {
                    line[o++] = '\\';
                }
                line[o++] = *c;
            }
            line[o++] = '"';
            line[o] = '\0';
            return desk_send(d, line);
        }
        p = lib_next(lib_next(disp));
    }
    return 0;
}

static void draw_home_content(desk* d) {
    const sel_state* st = sel_engine_state(d->engine);
    sel_gui_rect mn = draw_panel(d, "main");
    int32_t pad = lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_TILE_PAD]);
    int32_t hv = sel_gui_tween_value(&d->tl, ANIM_KEY_HOVER, 65536);
    uint32_t i;
    char name[256];
    if (st->composers != NULL) {
        draw_library(d);
        return;
    }
    if (d->songs) {
        draw_songs(d);
        draw_songs_targets(d);
        return;
    }
    if (d->open_tile > 0) {
        draw_detail(d);
        return;
    }
    load_tile_art(d, st);
    sel_gui_clip_push(d->canvas, mn);
    if (home_shelf_mode(st)) {
        sel_gui_rect shifted = mn;
        shifted.y -= lpx(d, d->home_y_q16 >> 16); /* the header scrolls away with the shelves */
        draw_home_header(d, shifted, 0);
        draw_shelves(d, mn);
        if (d->tile_menu > 0) {
            const sel_gui_atlas* semi = desk_font(d, 1, 13);
            sel_gui_rect m = rect_of(d->menu_x, d->menu_y, lpx(d, SONG_MENU_W), lpx(d, SONG_MENU_ITEM_H));
            sel_gui_fill_rect(d->canvas, m, ARGB(d->theme.token[SEL_GUI_TOKEN_ACCENT]));
            sel_gui_text_wrap(d->canvas, semi, rect_of(m.x + pad, m.y + lpx(d, 4), m.w - 2 * pad, semi->line_height),
                              g_menu_items[1], ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT]), 1, &d->text);
        }
        sel_gui_clip_pop(d->canvas);
        sel_gui_dirty_add(d->canvas, mn);
        return;
    }
    if (home_empty(st)) {
        sel_gui_rect b = home_empty_button(d);
        draw_home_header(d, mn, 0);
        draw_text(d, rect_of(mn.x + lpx(d, HOME_MARGIN), mn.y + lpx(d, HOME_SECTION_Y), mn.w - 2 * lpx(d, HOME_MARGIN), lpx(d, HOME_SECTION_H)),
                  "Nothing played yet. Songs you play show up here.", 0, 14, ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT_MUTED]));
        sel_gui_round_rect(d->canvas, b, lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_RADIUS]), ARGB(d->theme.token[SEL_GUI_TOKEN_ACCENT]));
        draw_text(d, rect_of(b.x + lpx(d, 14), b.y, b.w, b.h), "Browse files", 1, 13, ARGB(d->theme.token[SEL_GUI_TOKEN_ACCENT_TEXT]));
        sel_gui_clip_pop(d->canvas);
        sel_gui_dirty_add(d->canvas, mn);
        return;
    }
    draw_home_header(d, mn, 1);
    for (i = 0; i < st->queue_count; i++) {
        sel_gui_rect t = tile_rect(d, i);
        sel_gui_rect ar = rect_of(t.x + pad, t.y + pad, t.w - 2 * pad, t.w - 2 * pad);
        const uint32_t* art;
        char format[16];
        int hovered = d->hover_tile > 0 && (uint32_t)d->hover_tile - 1u == i;
        uint32_t panel = ARGB(d->theme.token[SEL_GUI_TOKEN_PANEL]);
        queue_title(st, i, name, sizeof name);
        queue_format(st, i, format, sizeof format);
        sel_gui_fill_rect(d->canvas, t, panel); /* the corners blend with the panel, not with the last frame's card */
        sel_gui_round_rect(d->canvas, t, lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_RADIUS_LARGE]),
                           hovered ? desk_mix(panel, ARGB(d->theme.token[SEL_GUI_TOKEN_SURFACE]), (uint32_t)hv) : panel);
        d->tile_cards++;
        d->home_parts |= HP_TILE_CARD;
        art = art_tile(d->art, st->queue_text + st->queue[i].path_offset);
        if (d->shadow_ready) {
            sel_gui_shadow_draw(d->canvas, &d->shadow, ar);
        }
        if (art != NULL) {
            sel_gui_blit_image(d->canvas, ar, art, DESK_ART_TILE_EDGE, DESK_ART_TILE_EDGE, DESK_ART_TILE_EDGE, 0);
        } else {
            /* No cover: the generated art of the song's format and chip (gui_genart.h). */
            sel_gui_genart_draw(d->canvas, ar, format, sel_gui_str_hash(name));
        }
        if (i == 0u) {
            d->art_tile_hash = rect_hash(d->canvas, ar);
        }
        draw_text(d, rect_of(ar.x, ar.y + ar.h + lpx(d, HOME_TILE_TEXT_GAP), ar.w, lpx(d, HOME_TITLE_H)), name, 1, 14,
                  ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT]));
        draw_text(d, rect_of(ar.x, ar.y + ar.h + lpx(d, HOME_TILE_TEXT_GAP + HOME_TITLE_H), ar.w, lpx(d, HOME_META_H)), format, 0,
                  13, ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT_MUTED]));
        d->home_parts |= HP_ARTIST_LINE;
        if (hovered && hv > 0) { /* the play affordance */
            int32_t ps = lpx(d, HOME_PLAY_SIZE), in = lpx(d, HOME_PLAY_INSET);
            sel_gui_rect pb = rect_of(ar.x + ar.w - in - ps, ar.y + ar.h - in - ps, ps, ps);
            sel_gui_round_rect(d->canvas, pb, ps / 2, ARGB(d->theme.token[SEL_GUI_TOKEN_ACCENT]));
            sel_gui_icon_draw(d->canvas, &d->icons, SEL_GUI_ICON_PLAY, rect_of(pb.x + ps / 4, pb.y + ps / 4, ps / 2, ps / 2),
                              ps / 2, ARGB(d->theme.token[SEL_GUI_TOKEN_ACCENT_TEXT]));
            d->home_parts |= HP_PLAY_HOVER;
        }
    }
    sel_gui_clip_pop(d->canvas);
    sel_gui_dirty_add(d->canvas, mn);
}

/* The main region with the running tweens on top: the hover fade of one tile (a light veil, full strength at rest)
 * and the view cross-fade (the new view fades in from the surface colour). At rest both are the plain view. */
static void draw_home(desk* d) {
    sel_gui_rect mn = region_rect(d, "main");
    int32_t view = sel_gui_tween_value(&d->tl, ANIM_KEY_VIEW, 65536);
    draw_home_content(d);
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
    if (home_empty(st)) { /* the empty state's only action */
        if (in_rect(home_empty_button(d), x, y)) {
            desk_send(d, "VIEW_SOURCE browse");
            return 1;
        }
        return 0;
    }
    if (home_shelf_mode(st)) { /* a shelf tile: the engine plays the song or opens the album */
        uint32_t s, k;
        if (!shelf_hit(d, x, y, &s, &k)) {
            return 0;
        }
        shelf_activate(d, s, k);
        draw_home(d);
        return 1;
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
enum { TIP_DELAY_MS = 600, TIP_BTN = 32, TIP_H = 24 };
/* The transport row, left to right (DUI-P4-1): previous subsong, shuffle, previous, play or pause, next, repeat, next
 * subsong. The play slot sits in the middle where the placeholder box was drawn. */
enum { SLOT_SUB_PREV, SLOT_SHUFFLE, SLOT_PREV, SLOT_PLAY, SLOT_NEXT, SLOT_REPEAT, SLOT_SUB_NEXT, TRANSPORT_COUNT };

static const sel_uim_action* desk_action_by_id(const char* id) {
    uint32_t i, n = sel_uim_action_count();
    if (id == NULL) {
        return NULL;
    }
    for (i = 0; i < n; i++) {
        const sel_uim_action* a = sel_uim_action_at(i);
        if (strcmp(a->id, id) == 0) {
            return a;
        }
    }
    return NULL;
}

/* The action id of transport slot i from the engine state, NULL when the slot is not shown (the subsong buttons need
 * a song with more than one subsong). Shuffle and repeat name the action a click fires: the next state. */
static const char* desk_slot_action(const desk* d, uint32_t i) {
    const sel_state* st = sel_engine_state(d->engine);
    int repeat_all = strcmp(st->options[SEL_OPT_REPEAT], "all") == 0;
    int repeat_one = strcmp(st->options[SEL_OPT_REPEAT], "one") == 0;
    switch (i) {
    case SLOT_SUB_PREV:
        return st->song.subsong_count > 1u ? "subsong_prev" : NULL;
    case SLOT_SHUFFLE:
        return strcmp(st->options[SEL_OPT_RANDOM], "on") == 0 ? "random_off" : "random_on";
    case SLOT_PREV:
        return "prev";
    case SLOT_PLAY:
        return st->play == SEL_PLAYING ? "pause" : "play";
    case SLOT_NEXT:
        return "next";
    case SLOT_REPEAT:
        return repeat_all ? "repeat_one" : repeat_one ? "repeat_off" : "repeat_all";
    default:
        return st->song.subsong_count > 1u ? "subsong_next" : NULL;
    }
}

/* Canvas rectangle of transport button i: the row is centred in the player region, the gap is the spacing token. */
static sel_gui_rect desk_button_rect(const desk* d, uint32_t i) {
    sel_gui_rect pl = region_rect(d, "player");
    int32_t sp = lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_SPACING]);
    int32_t cx = pl.x + pl.w / 2;
    return rect_of(cx - lpx(d, TIP_BTN / 2) + ((int32_t)i - SLOT_PLAY) * (lpx(d, TIP_BTN) + sp), pl.y + sp + lpx(d, 24),
                   lpx(d, TIP_BTN), lpx(d, TIP_BTN));
}

/* 1-based transport button under the canvas point, 0 = none. */
static int32_t desk_button_at(const desk* d, int32_t x, int32_t y) {
    uint32_t i;
    for (i = 0; i < TRANSPORT_COUNT; i++) {
        if (desk_slot_action(d, i) != NULL && in_rect(desk_button_rect(d, i), x, y)) {
            return (int32_t)i + 1;
        }
    }
    return 0;
}

/* The sidebar entries (DUI-P4-1): an icon and a label per library door, one row each under the queue count. The
 * icon-only breakpoint (a narrow sidebar) draws the icons alone. */
static void draw_sidebar(desk* d) {
    static const struct {
        uint32_t icon;
        const char* label;
    } entries[] = {{SEL_GUI_ICON_HOUSE, "Home"}, {SEL_GUI_ICON_SEARCH, "Search"}, {SEL_GUI_ICON_LIST, "Your Library"}};
    sel_gui_rect sb = draw_panel(d, "sidebar");
    int32_t sp = lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_SPACING]);
    int32_t row = lpx(d, 40), isz = lpx(d, 22), br = lpx(d, HOME_BRAND), y = sb.y + 2 * sp;
    int32_t pad = lpx(d, 12), lab = lpx(d, 16);
    sel_gui_rect logo = rect_of(sb.x + pad + sp, y, br, br);
    uint32_t i;
    sel_gui_round_rect(d->canvas, logo, lpx(d, 7), ARGB(d->theme.token[SEL_GUI_TOKEN_ACCENT]));
    sel_gui_icon_draw(d->canvas, &d->icons, SEL_GUI_ICON_PLAY, rect_of(logo.x + lpx(d, 6), logo.y + lpx(d, 6), lab, lab), lab,
                      ARGB(d->theme.token[SEL_GUI_TOKEN_ACCENT_TEXT]));
    if (sb.w > lpx(d, 120)) {
        draw_text(d, rect_of(logo.x + br + lpx(d, 10), y, sb.x + sb.w - (logo.x + br + lpx(d, 10)), br), "Selecta", 1, 18,
                  ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT]));
    }
    d->home_parts |= HP_BRAND;
    y += br + pad;
    for (i = 0; i < sizeof entries / sizeof entries[0]; i++, y += row) {
        sel_gui_rect r = rect_of(sb.x + sp, y, sb.w - 2 * sp, row);
        sel_gui_rect ic = rect_of(r.x + pad, y + (row - isz) / 2, isz, isz);
        uint32_t ink = ARGB(d->theme.token[i == 0u ? SEL_GUI_TOKEN_TEXT : SEL_GUI_TOKEN_TEXT_MUTED]);
        sel_gui_icon_draw(d->canvas, &d->icons, entries[i].icon, ic, isz, ink);
        if (sb.w > lpx(d, 120)) {
            draw_text(d, rect_of(ic.x + isz + lab, y, r.x + r.w - (ic.x + isz + lab), row), entries[i].label, 1, 15, ink);
        }
        sel_gui_dirty_add(d->canvas, r);
        d->frame_parts++;
        d->home_parts |= HP_NAV;
    }
}

/* The player bar's left block (mockup footer): artwork of the playing song with its title and artist beside it. Hidden
 * when the bar is too narrow to share the row with the transport. */
static void draw_player_left(desk* d) {
    const sel_uim_frame* f = &d->uim.frame;
    sel_gui_rect pl = region_rect(d, "player");
    int32_t sp = lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_SPACING]);
    int32_t as = lpx(d, 56), gap = lpx(d, 12);
    sel_gui_rect ar, box, all;
    if (pl.w < lpx(d, 900) || pl.h < as) {
        return;
    }
    ar = rect_of(pl.x + sp, pl.y + (pl.h - as) / 2, as, as);
    box = rect_of(ar.x + as + gap, ar.y, lpx(d, 300) - as - gap - sp, as);
    all = rect_of(pl.x, pl.y, lpx(d, 300) + sp, pl.h);
    sel_gui_fill_rect(d->canvas, all, ARGB(d->theme.token[SEL_GUI_TOKEN_FRAME]));
    sel_gui_genart_draw(d->canvas, ar, f->format, sel_gui_str_hash(f->title));
    d->home_parts |= HP_PLAYER_ART;
    draw_text(d, rect_of(box.x, box.y + lpx(d, 10), box.w, lpx(d, 18)), f->title, 1, 14, ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT]));
    draw_text(d, rect_of(box.x, box.y + lpx(d, 30), box.w, lpx(d, 16)), f->artist, 0, 12,
              ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT_MUTED]));
    d->home_parts |= HP_PLAYER_TITLE;
    sel_gui_dirty_add(d->canvas, all);
}

/* The Now Playing panel (DUI-P4-1) in the detail region: title, artist and format of the playing song. */
static void draw_nowplaying(desk* d) {
    const sel_uim_frame* f = &d->uim.frame;
    sel_gui_rect dt = region_rect(d, "detail");
    int32_t sp = lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_SPACING]);
    sel_gui_rect card;
    if (dt.w <= 0 || dt.h <= 0) {
        return;
    }
    draw_panel(d, "detail");
    card = rect_of(dt.x + sp, dt.y + sp, dt.w - 2 * sp, lpx(d, 96));
    sel_gui_round_rect(d->canvas, card, lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_RADIUS]),
                       ARGB(d->theme.token[SEL_GUI_TOKEN_SURFACE_RAISED]));
    draw_text(d, rect_of(card.x + sp, card.y + sp, card.w - 2 * sp, lpx(d, 20)), "Now Playing", 1, 11,
              ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT_MUTED]));
    draw_text(d, rect_of(card.x + sp, card.y + sp + lpx(d, 24), card.w - 2 * sp, lpx(d, 20)), f->title, 1, 16,
              ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT]));
    draw_text(d, rect_of(card.x + sp, card.y + sp + lpx(d, 46), card.w - 2 * sp, lpx(d, 16)), f->artist, 0, 13,
              ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT_MUTED]));
    draw_text(d, rect_of(card.x + sp, card.y + sp + lpx(d, 64), card.w - 2 * sp, lpx(d, 14)), f->format, 0, 11,
              ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT_MUTED]));
    sel_gui_dirty_add(d->canvas, card);
    d->frame_parts++;
}

/* The transport row (DUI-P4-1): one round button per slot with its icon. Play is the accent button; shuffle and repeat
 * take the accent colour while on, previous and next fade when there is nothing to go to, the repeat button carries a
 * "1" in the repeat-one mode, and the subsong buttons come with an "n/m" label. */
static void draw_transport(desk* d) {
    const sel_uim_frame* f = &d->uim.frame;
    int32_t sp = lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_SPACING]);
    sel_gui_rect first = desk_button_rect(d, SLOT_SUB_PREV);
    sel_gui_rect label = rect_of(first.x - sp - lpx(d, 56), first.y, lpx(d, 56), first.h);
    uint32_t i;
    sel_gui_fill_rect(d->canvas, label, ARGB(d->theme.token[SEL_GUI_TOKEN_FRAME]));
    if (f->subsong_count > 1u) {
        char buf[24];
        snprintf(buf, sizeof buf, "%u/%u", (unsigned)f->subsong, (unsigned)f->subsong_count);
        draw_text(d, label, buf, 0, 11, ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT_MUTED]));
    }
    sel_gui_dirty_add(d->canvas, label);
    for (i = 0; i < TRANSPORT_COUNT; i++) {
        sel_gui_rect b = desk_button_rect(d, i);
        uint32_t icon = SEL_GUI_ICON_PLAY;
        uint32_t fill = d->theme.token[SEL_GUI_TOKEN_FRAME];
        uint32_t ink = d->theme.token[SEL_GUI_TOKEN_TEXT];
        uint32_t off = d->theme.token[SEL_GUI_TOKEN_TEXT_DISABLED];
        uint32_t on = d->theme.token[SEL_GUI_TOKEN_ACCENT];
        uint32_t mute = d->theme.token[SEL_GUI_TOKEN_TEXT_MUTED];
        int32_t isz = lpx(d, 20);
        sel_gui_rect ic;
        if (desk_slot_action(d, i) == NULL) {
            sel_gui_fill_rect(d->canvas, b, ARGB(d->theme.token[SEL_GUI_TOKEN_FRAME]));
            sel_gui_dirty_add(d->canvas, b);
            continue;
        }
        switch (i) {
        case SLOT_SUB_PREV:
            icon = SEL_GUI_ICON_MINUS;
            ink = f->subsong > 1u ? ink : off;
            break;
        case SLOT_SUB_NEXT:
            icon = SEL_GUI_ICON_PLUS;
            ink = f->subsong < f->subsong_count ? ink : off;
            break;
        case SLOT_SHUFFLE:
            icon = SEL_GUI_ICON_SHUFFLE;
            ink = f->random_on ? on : mute;
            break;
        case SLOT_PREV:
            icon = SEL_GUI_ICON_SKIP_BACK;
            ink = f->can_prev ? ink : off;
            break;
        case SLOT_PLAY:
            icon = f->play == SEL_PLAYING ? SEL_GUI_ICON_PAUSE : SEL_GUI_ICON_PLAY;
            fill = d->theme.token[SEL_GUI_TOKEN_TEXT];
            ink = d->theme.token[SEL_GUI_TOKEN_FRAME];
            isz = lpx(d, 24);
            break;
        case SLOT_NEXT:
            icon = SEL_GUI_ICON_SKIP_FORWARD;
            ink = f->can_next ? ink : off;
            break;
        default:
            icon = SEL_GUI_ICON_REPEAT;
            ink = f->repeat_mode != 0u ? on : mute;
            break;
        }
        sel_gui_fill_rect(d->canvas, b, ARGB(d->theme.token[SEL_GUI_TOKEN_FRAME])); /* a redraw must not blend the edges twice */
        sel_gui_round_rect(d->canvas, b, i == SLOT_PLAY ? b.h / 2 : lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_RADIUS]), ARGB(fill));
        ic = rect_of(b.x + (b.w - isz) / 2, b.y + (b.h - isz) / 2, isz, isz);
        sel_gui_icon_draw(d->canvas, &d->icons, icon, ic, isz, ARGB(ink));
        if (i == SLOT_REPEAT && f->repeat_mode == 2u) {
            draw_text(d, rect_of(b.x + b.w - lpx(d, 12), b.y + lpx(d, 2), lpx(d, 10), lpx(d, 12)), "1", 1, 11, ARGB(ink));
        }
        sel_gui_dirty_add(d->canvas, b);
        d->frame_parts++;
    }
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
    a = desk_action_by_id(desk_slot_action(d, (uint32_t)d->tip_btn - 1u));
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
    d->sends++;
    char* pool = ring[next++ % 16u];
    return sel_engine_send_line(d->engine, line, pool, sizeof ring[0]) == SEL_CODEC_OK;
}

/* A click on transport slot i fires the action the button names: its table line, or for the subsong buttons the line
 * SUBSONG with the neighbouring absolute index. Returns 1 when the engine took the line. */
static int desk_transport_click(desk* d, uint32_t i) {
    const sel_uim_action* a = desk_action_by_id(desk_slot_action(d, i));
    const sel_uim_frame* f = &d->uim.frame;
    char line[32];
    if (a == NULL) {
        return 0;
    }
    if (i == SLOT_SUB_PREV || i == SLOT_SUB_NEXT) {
        uint32_t to = i == SLOT_SUB_PREV ? f->subsong - 2u : f->subsong;
        if (i == SLOT_SUB_PREV ? f->subsong < 2u : f->subsong >= f->subsong_count) {
            return 0;
        }
        snprintf(line, sizeof line, "SUBSONG %u", (unsigned)to);
    } else {
        snprintf(line, sizeof line, "%s", a->line);
    }
    if (!desk_send(d, line)) {
        return 0;
    }
    d->transport_hits++;
    d->actions_fired++;
    return 1;
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
    if (ev->kind == SEL_GUI_IN_CLICK && ev->button == 0u) {
        int32_t btn = desk_button_at(d, ev->x, ev->y);
        if (btn > 0) {
            d->clicks++;
            return desk_transport_click(d, (uint32_t)btn - 1u);
        }
    }
    if (ev->kind == SEL_GUI_IN_CLICK && ev->button == 0u && in_main && library_click(d, ev->x, ev->y)) { /* the Library page */
        d->clicks++;
        return 1;
    }
    if (!d->songs) {
        if (ev->kind == SEL_GUI_IN_CLICK && d->tile_menu > 0) {
            return shelf_menu_click(d, ev->x, ev->y);
        }
        if (ev->kind == SEL_GUI_IN_CLICK && ev->button == 1u && in_main && d->home && d->open_tile == 0 &&
            home_shelf_mode(sel_engine_state(d->engine)) && shelf_menu_open(d, ev->x, ev->y)) {
            draw_home(d);
            return 1;
        }
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
    int tip;
    if (ev->kind == SEL_GUI_IN_MOVE) {
        d->ptr_x = ev->x;
        d->ptr_y = ev->y;
    }
    tip = desk_tip_pointer(d, ev->kind == SEL_GUI_IN_MOVE ? ev->x : -1, ev->y);
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
        if (d->home && d->open_tile == 0 && home_shelf_mode(sel_engine_state(d->engine))) { /* the shelf under the pointer */
            /* Rule (I247): a horizontal wheel (dx) always scrolls the shelf under the pointer sideways; a vertical wheel
             * (dy) scrolls that shelf sideways over its tile cards and the whole Home content vertically everywhere
             * else (shelf headings, gaps, the header block), so a tall Home stays reachable with a plain mouse wheel. */
            if (ev->dx != 0 || shelf_over_cards(d, d->ptr_y)) {
                shelf_scroll(d, d->ptr_y, -(ev->dx != 0 ? ev->dx : ev->dy));
            } else {
                home_scroll_y(d, -ev->dy);
            }
            d->tile_menu = 0;
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
    char* line = d->input_at != NULL ? d->input_at : d->input;
    uint32_t sent = d->sends;
    int opened = 0, pointer_sent = 0;
    d->input_done = 1;
    d->input_at = NULL;
    if (d->input_consumed != NULL) {
        *d->input_consumed = 1;
    }
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
            pointer_sent = d->sends != sent;
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
            if (home_shelf_mode(sel_engine_state(d->engine))) { /* n counts the tiles over all shelves in order */
                shelf sh[SHELF_MAX];
                uint32_t s, cnt = home_shelves(sel_engine_state(d->engine), sh), left = n;
                for (s = 0; s < cnt && left >= sh[s].tiles; s++) {
                    left -= sh[s].tiles;
                }
                t = s < cnt ? shelf_tile_rect(d, s, left) : rect_of(-1, -1, 0, 0);
            }
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
        if (pointer_sent && line != NULL && *line != '\0') { /* the engine answers on the shell's next pump: resume then */
            d->input_at = line;
            d->input_done = 0;
            return 1;
        }
        sent = d->sends;
        pointer_sent = 0;
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
        d->seek_fill = -1;
        d->vol_fill = -1;
        d->seek_length[0] = '\0';
        sel_gui_fill_rect(d->canvas, rect_of(0, 0, d->canvas->w, d->canvas->h),
                          ARGB(d->theme.token[SEL_GUI_TOKEN_FRAME]));
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
            if (desk_slot_action(d, i) != NULL) {
                printf("button id=%s x=%d y=%d w=%d h=%d\n", desk_slot_action(d, i), (int)b.x, (int)b.y, (int)b.w,
                       (int)b.h);
            }
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
    d->input_consumed = a->input_consumed;
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
        } else if (strcmp(view, "home") == 0 || strcmp(view, "library") == 0) { /* library: the Home frame with the Library page (DUI-P4-3) */
            d->home = 1;
        } else if (strcmp(view, "songs") == 0) {
            d->home = 1;
            d->songs = 1;
        } else {
            fprintf(stderr, "selecta: desktop view '%s' is not testcard, text, home, library, songs or widgets\n", view);
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
        if (strcmp(a->argv[i], "--input") == 0 && i + 1 < a->argc && !(a->input_consumed != NULL && *a->input_consumed) &&
            !desk_load_input(d, a->argv[i + 1])) {
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
    if (lib_hash(d, sel_engine_state(d->engine)) != d->library_sig && sel_engine_state(d->engine)->composers != NULL) {
        d->library_sig = lib_hash(d, sel_engine_state(d->engine)); /* the Library page changed: redraw */
        d->uim.dirty |= SEL_UIM_DIRTY_ALL;
    }
    if (d->home && sel_engine_state(d->engine)->view != NULL) { /* HOME rows arrived or changed: redraw */
        const sel_view_state* hv = sel_engine_state(d->engine)->view;
        uint32_t sig = hv->source == SEL_VIEW_HOME ? 1u + hv->row_n : 0u;
        if (sig != d->home_sig) {
            d->home_sig = sig;
            d->uim.dirty |= SEL_UIM_DIRTY_ALL;
        }
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
    sel_gui_hash_hex(sel_gui_hash(d->canvas), d->canvas_hash_hex);
    d->stats[10].value = (uintptr_t)d->canvas_hash_hex;
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
    sel_gui_hash_hex(d->art_tile_hash, d->art_tile_hash_hex);
    d->stats[16].value = (uintptr_t)d->art_tile_hash_hex;
    sel_gui_hash_hex(0u, d->present_hash_hex);
    d->stats[17].value = (uintptr_t)d->present_hash_hex;
    d->stats[17].tag = SEL_UI_STAT_PRESENT_HASH;
    d->stats[18].tag = SEL_UI_STAT_UPLOAD_PIXELS;
    d->stats[19].tag = SEL_UI_STAT_PRESENTS;
#ifdef SELECTA_DESKTOP_SDL
    if (d->sdl != NULL) {
        sel_gui_hash_hex(desk_sdl_present_hash(d->sdl), d->present_hash_hex);
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
    d->stats[45].tag = SEL_UI_STAT_TRANSPORT_HITS;
    d->stats[45].value = d->transport_hits;
    d->stats[46].tag = SEL_UI_STAT_PLAY_STATE;
    d->stats[46].value = (uintptr_t)sel_engine_state(d->engine)->play;
    d->stats[47].tag = SEL_UI_STAT_REPEAT_MODE;
    d->stats[47].value = d->uim.frame.repeat_mode;
    d->stats[48].tag = SEL_UI_STAT_RANDOM_ON;
    d->stats[48].value = d->uim.frame.random_on;
    d->stats[49].tag = SEL_UI_STAT_SUBSONG;
    d->stats[49].value = d->uim.frame.subsong;
    d->stats[50].tag = SEL_UI_STAT_FRAME_PARTS;
    d->stats[50].value = d->frame_parts;
    d->stats[51].tag = SEL_UI_STAT_HOME_PARTS;
    d->stats[51].value = d->home_parts;
    d->stats[52].tag = SEL_UI_STAT_TILE_CARDS;
    d->stats[52].value = d->tile_cards;
    d->stats[53].tag = SEL_UI_STAT_HOME_SHELVES;
    d->stats[53].value = home_dump_build(d);
    d->stats[54].tag = SEL_UI_STAT_SHELF_SCROLL;
    d->stats[54].value = (uintptr_t)(((uint32_t)d->shelf_q16[0] >> 16) | (((uint32_t)d->shelf_q16[1] >> 16) << 16));
    d->stats[55].tag = SEL_UI_STAT_HOME_DUMP;
    d->stats[55].value = (uintptr_t)d->home_dump;
    d->stats[56].tag = SEL_UI_STAT_HOME_SCROLL_Y;
    d->stats[56].value = (uintptr_t)(uint32_t)(d->home_y_q16 >> 16);
    d->stats[57].tag = SEL_UI_STAT_TILE_QUEUED;
    d->stats[57].value = d->tile_queued;
    d->stats[58].tag = SEL_UI_STAT_LIBRARY_DUMP;
    d->stats[58].value = (uintptr_t)library_dump_build(d);
    d->stats[59].tag = SEL_UI_STAT_HOME_EMPTY;
    d->stats[59].value = (uintptr_t)home_empty(sel_engine_state(d->engine));
    d->stats[60].tag = SEL_TAG_END;

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

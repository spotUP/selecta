/* Cell grid and diff flush, plan M1.1 phase 5 (row I59). */
#include "grid.h"

#include <stdlib.h>
#include <string.h>

#define SYNC_BEGIN "\x1b[?2026h"
#define SYNC_END "\x1b[?2026l"

txt_style txt_style_make(uint16_t fg, uint16_t bg, uint8_t attr) {
    txt_style s;
    s.fg = fg;
    s.bg = bg;
    s.attr = attr;
    return s;
}

static int style_eq(txt_style a, txt_style b) {
    return a.fg == b.fg && a.bg == b.bg && a.attr == b.attr;
}

static int style_is_default(txt_style s) {
    return s.fg == TXT_COLOR_DEFAULT && s.bg == TXT_COLOR_DEFAULT && s.attr == 0;
}

static void blank(txt_cell* cells, size_t n) {
    size_t i;
    for (i = 0; i < n; i++) {
        cells[i].cp = ' ';
        cells[i].style = txt_style_make(TXT_COLOR_DEFAULT, TXT_COLOR_DEFAULT, 0);
    }
}

int txt_grid_init(txt_grid* g, uint32_t cols, uint32_t rows) {
    memset(g, 0, sizeof *g);
    return txt_grid_resize(g, cols, rows);
}

void txt_grid_free(txt_grid* g) {
    free(g->back);
    free(g->front);
    memset(g, 0, sizeof *g);
}

int txt_grid_resize(txt_grid* g, uint32_t cols, uint32_t rows) {
    size_t n;
    txt_cell* b;
    txt_cell* f;
    if (cols == 0 || rows == 0 || cols > 1000u || rows > 1000u) return -1;
    n = (size_t)cols * rows;
    b = (txt_cell*)malloc(n * sizeof *b);
    f = (txt_cell*)malloc(n * sizeof *f);
    if (!b || !f) {
        free(b);
        free(f);
        return -1;
    }
    blank(b, n);
    blank(f, n);
    free(g->back);
    free(g->front);
    g->back = b;
    g->front = f;
    g->cols = cols;
    g->rows = rows;
    g->dirty_all = 1;
    return 0;
}

void txt_grid_invalidate(txt_grid* g) { g->dirty_all = 1; }

void txt_grid_clear(txt_grid* g, txt_style style) {
    txt_grid_fill(g, 0, 0, g->cols, g->rows, ' ', style);
}

void txt_grid_put(txt_grid* g, uint32_t col, uint32_t row, uint32_t cp, txt_style style) {
    txt_cell* c;
    if (col >= g->cols || row >= g->rows) return;
    c = &g->back[(size_t)row * g->cols + col];
    c->cp = cp;
    c->style = style;
}

void txt_grid_fill(txt_grid* g, uint32_t col, uint32_t row, uint32_t w, uint32_t h, uint32_t cp,
                   txt_style style) {
    uint32_t x, y;
    for (y = row; y < row + h && y < g->rows; y++)
        for (x = col; x < col + w && x < g->cols; x++) txt_grid_put(g, x, y, cp, style);
}

/* Decodes one UTF-8 sequence; returns bytes consumed (>= 1). */
static size_t utf8_next(const unsigned char* s, uint32_t* cp) {
    unsigned char c = s[0];
    size_t n, i;
    uint32_t v, min;
    if (c < 0x80) {
        *cp = c;
        return 1;
    }
    if (c >= 0xC2 && c < 0xE0) {
        n = 2;
        v = c & 0x1Fu;
        min = 0x80;
    } else if (c >= 0xE0 && c < 0xF0) {
        n = 3;
        v = c & 0x0Fu;
        min = 0x800;
    } else if (c >= 0xF0 && c < 0xF5) {
        n = 4;
        v = c & 0x07u;
        min = 0x10000;
    } else {
        *cp = 0xFFFD;
        return 1;
    }
    for (i = 1; i < n; i++) {
        if ((s[i] & 0xC0u) != 0x80u) {
            *cp = 0xFFFD;
            return i;
        }
        v = (v << 6) | (s[i] & 0x3Fu);
    }
    if (v < min || v > 0x10FFFFu || (v >= 0xD800u && v < 0xE000u)) v = 0xFFFD;
    *cp = v;
    return n;
}

uint32_t txt_grid_text(txt_grid* g, uint32_t col, uint32_t row, const char* utf8, uint32_t maxcols,
                       txt_style style) {
    const unsigned char* s = (const unsigned char*)utf8;
    uint32_t drawn = 0;
    while (*s && col + drawn < g->cols && (maxcols == 0 || drawn < maxcols)) {
        uint32_t cp;
        s += utf8_next(s, &cp);
        txt_grid_put(g, col + drawn, row, cp, style);
        drawn++;
    }
    return drawn;
}

/* ---- flush ---- */

typedef struct out_buf {
    char* p;
    size_t cap, len;
    int over;
} out_buf;

static void put_bytes(out_buf* o, const char* s, size_t n) {
    if (o->len + n > o->cap) {
        o->over = 1;
        return;
    }
    memcpy(o->p + o->len, s, n);
    o->len += n;
}

static void put_str(out_buf* o, const char* s) { put_bytes(o, s, strlen(s)); }

static void put_num(out_buf* o, unsigned v) {
    char t[12];
    int n = 0;
    do {
        t[n++] = (char)('0' + v % 10u);
        v /= 10u;
    } while (v);
    while (n) put_bytes(o, &t[--n], 1);
}

static void put_cp(out_buf* o, uint32_t cp, int latin1) {
    char t[4];
    if (cp < 0x20u || (cp >= 0x7Fu && cp < 0xA0u)) cp = ' ';
    if (latin1) {
        t[0] = (char)(cp < 0x100u ? cp : '?');
        put_bytes(o, t, 1);
    } else if (cp < 0x80u) {
        t[0] = (char)cp;
        put_bytes(o, t, 1);
    } else if (cp < 0x800u) {
        t[0] = (char)(0xC0u | (cp >> 6));
        t[1] = (char)(0x80u | (cp & 0x3Fu));
        put_bytes(o, t, 2);
    } else if (cp < 0x10000u) {
        t[0] = (char)(0xE0u | (cp >> 12));
        t[1] = (char)(0x80u | ((cp >> 6) & 0x3Fu));
        t[2] = (char)(0x80u | (cp & 0x3Fu));
        put_bytes(o, t, 3);
    } else {
        t[0] = (char)(0xF0u | (cp >> 18));
        t[1] = (char)(0x80u | ((cp >> 12) & 0x3Fu));
        t[2] = (char)(0x80u | ((cp >> 6) & 0x3Fu));
        t[3] = (char)(0x80u | (cp & 0x3Fu));
        put_bytes(o, t, 4);
    }
}

static void put_move(out_buf* o, uint32_t row, uint32_t col) {
    put_str(o, "\x1b[");
    put_num(o, row + 1u);
    put_str(o, ";");
    put_num(o, col + 1u);
    put_str(o, "H");
}

/* RGB of an xterm-256color index: 0-15 the standard colours, 16-231 the 6x6x6 cube, 232-255 greys. */
static void xterm_rgb(uint32_t n, int rgb[3]) {
    static const int base[16][3] = {{0, 0, 0},       {205, 0, 0},     {0, 205, 0},     {205, 205, 0},
                                    {0, 0, 238},     {205, 0, 205},   {0, 205, 205},   {229, 229, 229},
                                    {127, 127, 127}, {255, 0, 0},     {0, 255, 0},     {255, 255, 0},
                                    {92, 92, 255},   {255, 0, 255},   {0, 255, 255},   {255, 255, 255}};
    static const int lv[6] = {0, 95, 135, 175, 215, 255};
    if (n < 16u) {
        memcpy(rgb, base[n], sizeof base[n]);
    } else if (n < 232u) {
        n -= 16u;
        rgb[0] = lv[n / 36u];
        rgb[1] = lv[(n / 6u) % 6u];
        rgb[2] = lv[n % 6u];
    } else {
        rgb[0] = rgb[1] = rgb[2] = 8 + (int)(n - 232u) * 10;
    }
}

/* The console.device knows only the system colours 0-7 (SGR 30-37 / 40-47; Amiga ROM Kernel Reference,
 * console.device, Select Graphic Rendition). The pen of an xterm-256color index is the nearest of the
 * eight ANSI colours (black, red, green, yellow, blue, magenta, cyan, white by pen number 0-7). */
static uint32_t console_pen(uint32_t n) {
    static const int pen[8][3] = {{0, 0, 0},     {205, 0, 0},   {0, 205, 0},   {205, 205, 0},
                                  {0, 0, 238},   {205, 0, 205}, {0, 205, 205}, {229, 229, 229}};
    int c[3];
    uint32_t i, best = 0;
    long bd = -1;
    xterm_rgb(n, c);
    for (i = 0; i < 8u; i++) {
        long d = 0;
        int k;
        for (k = 0; k < 3; k++) d += (long)(c[k] - pen[i][k]) * (c[k] - pen[i][k]);
        if (bd < 0 || d < bd) {
            bd = d;
            best = i;
        }
    }
    return best;
}

/* SGR that selects exactly this style from a reset state. A console grid gets SGR 30-37 / 40-47 only. */
static void put_sgr(out_buf* o, txt_style s, int console) {
    put_str(o, "\x1b[0");
    if (s.attr & TXT_ATTR_BOLD) put_str(o, ";1");
    if (s.attr & TXT_ATTR_REVERSE) put_str(o, ";7");
    if (s.fg != TXT_COLOR_DEFAULT) {
        if (console) {
            put_str(o, ";");
            put_num(o, 30u + console_pen(s.fg));
        } else {
            put_str(o, ";38;5;");
            put_num(o, s.fg);
        }
    }
    if (s.bg != TXT_COLOR_DEFAULT) {
        if (console) {
            put_str(o, ";");
            put_num(o, 40u + console_pen(s.bg));
        } else {
            put_str(o, ";48;5;");
            put_num(o, s.bg);
        }
    }
    put_str(o, "m");
}

size_t txt_grid_flush_bound(const txt_grid* g) {
    /* per cell: move (up to 10) + sgr (up to 32) + 4 byte glyph; plus framing */
    return (size_t)g->cols * g->rows * 46u + 64u;
}

long txt_grid_flush(txt_grid* g, char* out, size_t cap) {
    out_buf o;
    uint32_t x, y;
    txt_style cur = txt_style_make(TXT_COLOR_DEFAULT, TXT_COLOR_DEFAULT, 0);
    int full = g->dirty_all;
    int any = 0;
    long next_pos = -1; /* cell index the cursor sits at after the last glyph */
    o.p = out;
    o.cap = cap;
    o.len = 0;
    o.over = 0;
    if (full && !g->console) put_str(&o, SYNC_BEGIN);
    for (y = 0; y < g->rows; y++) {
        for (x = 0; x < g->cols; x++) {
            size_t i = (size_t)y * g->cols + x;
            const txt_cell* b = &g->back[i];
            const txt_cell* f = &g->front[i];
            /* The console.device scrolls the window when its last cell is written, which shifts the
             * whole picture one row up; the corner stays blank there (row I218e). */
            if (g->console && y + 1u == g->rows && x + 1u == g->cols) continue;
            if (!full && b->cp == f->cp && style_eq(b->style, f->style)) continue;
            any = 1;
            if (next_pos != (long)i || x == 0) put_move(&o, y, x);
            if (!style_eq(b->style, cur)) {
                put_sgr(&o, b->style, g->console);
                cur = b->style;
            }
            put_cp(&o, b->cp, g->latin1);
            next_pos = (long)i + 1;
        }
    }
    if (!any && !full) return 0;
    if (!style_is_default(cur)) put_str(&o, "\x1b[0m");
    if (full && !g->console) put_str(&o, SYNC_END);
    if (o.over) return -1;
    memcpy(g->front, g->back, (size_t)g->cols * g->rows * sizeof *g->front);
    g->dirty_all = 0;
    g->flushes++;
    g->bytes += (uint32_t)o.len;
    return (long)o.len;
}

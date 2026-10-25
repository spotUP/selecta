/* Glyph-atlas text layout and blit (DUI-P2-3). Integer only (plan 2.5, 2.11); see gui_text.h. */
#include <selecta/gui/gui_text.h>

#include <string.h>

#define ELLIPSIS_CP 0x2026u

uint32_t sel_gui_utf8_next(const char* s, size_t n, size_t* used) {
    const unsigned char* p = (const unsigned char*)s;
    uint32_t cp;
    size_t need, i;
    *used = 1;
    if (n == 0u) {
        return 0u;
    }
    if (p[0] < 0x80u) {
        return p[0];
    }
    if (p[0] >= 0xC2u && p[0] <= 0xDFu) {
        need = 1;
        cp = p[0] & 0x1Fu;
    } else if (p[0] >= 0xE0u && p[0] <= 0xEFu) {
        need = 2;
        cp = p[0] & 0x0Fu;
    } else if (p[0] >= 0xF0u && p[0] <= 0xF4u) {
        need = 3;
        cp = p[0] & 0x07u;
    } else {
        return 0xFFFDu;
    }
    if (n < need + 1u) {
        return 0xFFFDu;
    }
    for (i = 1; i <= need; i++) {
        if ((p[i] & 0xC0u) != 0x80u) {
            return 0xFFFDu;
        }
        cp = (cp << 6) | (p[i] & 0x3Fu);
    }
    *used = need + 1u;
    return cp;
}

const sel_gui_glyph* sel_gui_atlas_glyph(const sel_gui_atlas* at, uint32_t cp) {
    uint32_t lo = 0, hi = at->glyph_n;
    while (lo < hi) {
        uint32_t mid = (lo + hi) / 2u;
        if (at->glyphs[mid].cp < cp) {
            lo = mid + 1u;
        } else {
            hi = mid;
        }
    }
    return lo < at->glyph_n && at->glyphs[lo].cp == cp ? &at->glyphs[lo] : NULL;
}

static int32_t kern_of(const sel_gui_atlas* at, uint32_t a, uint32_t b) {
    uint32_t lo = 0, hi = at->kern_n;
    while (lo < hi) {
        uint32_t mid = (lo + hi) / 2u;
        const sel_gui_kern* k = &at->kerns[mid];
        if (k->a < a || (k->a == a && k->b < b)) {
            lo = mid + 1u;
        } else {
            hi = mid;
        }
    }
    return lo < at->kern_n && at->kerns[lo].a == a && at->kerns[lo].b == b ? at->kerns[lo].amount : 0;
}

/* I290: a code point below 0x20 (or DEL) is layout, not ink. The single-line paths (width, draw, ellipsis) skip it: it
 * has no glyph and no advance, so a stray newline never paints the missing-glyph box. Only sel_gui_text_wrap gives
 * a newline a meaning: a hard line break. */
static int is_control(uint32_t cp) {
    return cp < 0x20u || cp == 0x7Fu;
}

int32_t sel_gui_text_width(const sel_gui_atlas* at, const char* s, size_t len) {
    size_t i = 0, used;
    int32_t w = 0;
    uint32_t prev = 0;
    while (i < len) {
        uint32_t cp = sel_gui_utf8_next(s + i, len - i, &used);
        const sel_gui_glyph* g;
        if (is_control(cp)) { /* never a glyph, never an advance */
            i += used;
            continue;
        }
        g = sel_gui_atlas_glyph(at, cp);
        if (g == NULL) {
            g = &at->missing;
        }
        if (prev != 0u) {
            w += kern_of(at, prev, cp);
        }
        w += g->adv;
        prev = cp;
        i += used;
    }
    return w;
}

/* Blits the first `len` bytes of s at the pen; returns the advance. */
static int32_t draw_run(sel_gui_canvas* c, const sel_gui_atlas* at, int32_t x, int32_t y, const char* s, size_t len,
                        uint32_t argb, sel_gui_text_stats* st) {
    size_t i = 0, used;
    int32_t pen = x;
    uint32_t prev = 0;
    while (i < len) {
        uint32_t cp = sel_gui_utf8_next(s + i, len - i, &used);
        const sel_gui_glyph* g;
        if (is_control(cp)) { /* I290: a control character never reaches a glyph */
            i += used;
            continue;
        }
        g = sel_gui_atlas_glyph(at, cp);
        if (g == NULL) {
            g = &at->missing;
            if (st != NULL) {
                st->missing++;
            }
        }
        if (prev != 0u) {
            pen += kern_of(at, prev, cp);
        }
        if (g->w > 0u && g->h > 0u) {
            sel_gui_blit_mask(c, pen + g->bx, y + at->ascent + g->by, at->alpha + (size_t)g->y * (size_t)at->page_w + g->x,
                              g->w, g->h, at->page_w, argb);
            if (st != NULL && g != &at->missing) {
                st->glyphs++;
            }
        }
        pen += g->adv;
        prev = cp;
        i += used;
    }
    return pen - x;
}

static const char* ellipsis_of(const sel_gui_atlas* at) {
    return sel_gui_atlas_glyph(at, ELLIPSIS_CP) != NULL ? "\xE2\x80\xA6" : "...";
}

int32_t sel_gui_text_draw(sel_gui_canvas* c, const sel_gui_atlas* at, int32_t x, int32_t y, const char* s,
                          uint32_t argb, int32_t max_w, sel_gui_text_stats* st) {
    size_t len = strlen(s), i = 0, used, keep = 0;
    const char* ell;
    int32_t ell_w, w, pen = 0, drawn;
    uint32_t prev = 0;
    if (max_w < 0 || sel_gui_text_width(at, s, len) <= max_w) {
        return draw_run(c, at, x, y, s, len, argb, st);
    }
    ell = ellipsis_of(at);
    ell_w = sel_gui_text_width(at, ell, strlen(ell));
    /* The longest prefix that leaves room for the ellipsis; trailing spaces are not kept. */
    while (i < len) {
        uint32_t cp = sel_gui_utf8_next(s + i, len - i, &used);
        const sel_gui_glyph* g;
        if (is_control(cp)) {
            i += used;
            continue;
        }
        g = sel_gui_atlas_glyph(at, cp);
        if (g == NULL) {
            g = &at->missing;
        }
        w = pen + (prev != 0u ? kern_of(at, prev, cp) : 0) + g->adv;
        if (w + ell_w > max_w) {
            break;
        }
        pen = w;
        prev = cp;
        i += used;
        if (cp != ' ') {
            keep = i;
        }
    }
    drawn = draw_run(c, at, x, y, s, keep, argb, st);
    return drawn + draw_run(c, at, x + drawn, y, ell, strlen(ell), argb, st);
}

int32_t sel_gui_text_wrap(sel_gui_canvas* c, const sel_gui_atlas* at, sel_gui_rect box, const char* s,
                          uint32_t argb, int32_t max_lines, sel_gui_text_stats* st) {
    size_t len = strlen(s), start = 0;
    int32_t line = 0;
    while (line < max_lines && start < len) {
        size_t i, used, last_space = 0, brk;
        int32_t pen = 0;
        uint32_t prev = 0;
        int have_space = 0, fits = 1, hard = 0;
        while (start < len && s[start] == ' ') {
            start++;
        }
        if (start >= len) {
            break;
        }
        i = start;
        brk = len;
        while (i < len) {
            uint32_t cp = sel_gui_utf8_next(s + i, len - i, &used);
            const sel_gui_glyph* g = sel_gui_atlas_glyph(at, cp);
            int32_t w;
            if (cp == '\n' || cp == '\r') { /* a hard break; "\r\n" is one break (consumed below) */
                hard = 1;
                brk = i;
                break;
            }
            if (is_control(cp)) {
                i += used;
                continue;
            }
            if (g == NULL) {
                g = &at->missing;
            }
            w = pen + (prev != 0u ? kern_of(at, prev, cp) : 0) + g->adv;
            if (w > box.w && cp != ' ') {
                fits = 0;
                if (have_space) {
                    brk = last_space;
                } else {
                    brk = i > start ? i : i + used;
                }
                break;
            }
            if (cp == ' ') {
                have_space = 1;
                last_space = i;
            }
            pen = w;
            prev = cp;
            i += used;
        }
        if (!fits && line == max_lines - 1) {
            sel_gui_text_draw(c, at, box.x, box.y + line * at->line_height, s + start, argb, box.w, st);
            return line + 1;
        }
        draw_run(c, at, box.x, box.y + line * at->line_height, s + start, brk - start, argb, st);
        line++;
        start = brk;
        if (hard) {
            start += s[start] == '\r' && s[start + 1] == '\n' ? 2u : 1u;
        }
    }
    return line;
}

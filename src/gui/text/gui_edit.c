#include "selecta/gui/gui_edit.h"

#include <string.h>

void sel_gui_edit_set(sel_gui_edit* e, size_t anchor, size_t caret, size_t len) {
    e->anchor = anchor < len ? anchor : len;
    e->caret = caret < len ? caret : len;
    e->seen = len;
}

void sel_gui_edit_sync(sel_gui_edit* e, size_t len) {
    if (e->seen != len) {
        sel_gui_edit_set(e, len, len, len);
    } else {
        sel_gui_edit_set(e, e->anchor, e->caret, len);
    }
}

int sel_gui_edit_has(const sel_gui_edit* e) {
    return e->anchor != e->caret;
}

void sel_gui_edit_range(const sel_gui_edit* e, size_t* lo, size_t* hi) {
    *lo = e->anchor < e->caret ? e->anchor : e->caret;
    *hi = e->anchor < e->caret ? e->caret : e->anchor;
}

void sel_gui_edit_all(sel_gui_edit* e, size_t len) {
    sel_gui_edit_set(e, 0u, len, len);
}

static int word_byte(char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == '-' || c == '\'';
}

void sel_gui_edit_word(sel_gui_edit* e, const char* buf, size_t len, size_t pos) {
    size_t lo, hi;
    if (pos > len) {
        pos = len;
    }
    lo = pos;
    hi = pos;
    if (pos < len && !word_byte(buf[pos]) && pos > 0u && word_byte(buf[pos - 1u])) { /* a click just after a word picks that word */
        pos--;
    }
    if (pos < len) {
        int w = word_byte(buf[pos]);
        lo = pos;
        hi = pos + 1u;
        while (lo > 0u && word_byte(buf[lo - 1u]) == w && (w || buf[lo - 1u] == buf[pos])) {
            lo--;
        }
        while (hi < len && word_byte(buf[hi]) == w && (w || buf[hi] == buf[pos])) {
            hi++;
        }
    }
    sel_gui_edit_set(e, lo, hi, len);
}

void sel_gui_edit_move(sel_gui_edit* e, size_t len, int dir, int extend) {
    size_t lo, hi, to;
    sel_gui_edit_sync(e, len);
    sel_gui_edit_range(e, &lo, &hi);
    if (!extend && lo != hi) {
        to = dir < 0 ? lo : hi;
    } else {
        to = dir < 0 ? (e->caret > 0u ? e->caret - 1u : 0u) : (e->caret < len ? e->caret + 1u : len);
    }
    sel_gui_edit_set(e, extend ? e->anchor : to, to, len);
}

static void erase(sel_gui_edit* e, char* buf, size_t* len, size_t lo, size_t hi) {
    memmove(buf + lo, buf + hi, *len - hi);
    *len -= hi - lo;
    buf[*len] = '\0';
    sel_gui_edit_set(e, lo, lo, *len);
}

int sel_gui_edit_insert(sel_gui_edit* e, char* buf, size_t* len, size_t cap, char reject, const char* text, size_t n) {
    size_t lo, hi, i, before = *len;
    int changed = 0;
    sel_gui_edit_sync(e, *len);
    sel_gui_edit_range(e, &lo, &hi);
    if (lo != hi) {
        erase(e, buf, len, lo, hi);
        changed = 1;
    }
    for (i = 0u; i < n; i++) {
        unsigned char c = (unsigned char)text[i];
        if (c >= 32u && c < 127u && (char)c != reject && *len + 1u < cap) {
            memmove(buf + lo + 1u, buf + lo, *len - lo);
            buf[lo++] = (char)c;
            buf[++*len] = '\0';
            changed = 1;
        }
    }
    sel_gui_edit_set(e, lo, lo, *len);
    return changed || *len != before;
}

int sel_gui_edit_backspace(sel_gui_edit* e, char* buf, size_t* len) {
    size_t lo, hi;
    sel_gui_edit_sync(e, *len);
    sel_gui_edit_range(e, &lo, &hi);
    if (lo == hi) {
        if (lo == 0u) {
            return 0;
        }
        lo--;
    }
    erase(e, buf, len, lo, hi);
    return 1;
}

size_t sel_gui_edit_copy(const sel_gui_edit* e, const char* buf, char* out, size_t cap) {
    size_t lo, hi, n;
    sel_gui_edit_range(e, &lo, &hi);
    if (cap == 0u) {
        return 0u;
    }
    n = hi - lo < cap - 1u ? hi - lo : cap - 1u;
    memcpy(out, buf + lo, n);
    out[n] = '\0';
    return n;
}

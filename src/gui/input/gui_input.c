/* Scripted input parser (DUI-P3-3). Integers only: Q16 values are read one numeral at a time. */
#include <selecta/gui/gui_input.h>
#include <selecta/uimodel/keys.h>

#include <stdio.h>
#include <string.h>

static const struct {
    const char* name;
    uint32_t code;
} k_keys[] = {
    {"space", ' '},           {"tab", 9},                  {"enter", 13},         {"escape", 27},
    {"backspace", 127},       {"up", TXT_KEY_UP},          {"down", TXT_KEY_DOWN}, {"left", TXT_KEY_LEFT},
    {"right", TXT_KEY_RIGHT}, {"home", TXT_KEY_HOME},      {"end", TXT_KEY_END},   {"pageup", TXT_KEY_PAGE_UP},
    {"pagedown", TXT_KEY_PAGE_DOWN}, {"insert", TXT_KEY_INSERT}, {"delete", TXT_KEY_DELETE},
};

static int ieq(const char* a, const char* b) {
    for (; *a != '\0' && *b != '\0'; a++, b++) {
        char x = (*a >= 'A' && *a <= 'Z') ? (char)(*a + 32) : *a;
        char y = (*b >= 'A' && *b <= 'Z') ? (char)(*b + 32) : *b;
        if (x != y) {
            return 0;
        }
    }
    return *a == *b;
}

int sel_gui_key_parse(const char* spec, uint32_t* code, uint32_t* mods) {
    size_t i;
    *mods = 0u;
    for (;;) {
        const char* plus = strchr(spec, '+');
        char word[16];
        size_t n;
        if (plus == NULL || plus == spec || plus[1] == '\0') { /* a lone "+" is the plus key */
            break;
        }
        n = (size_t)(plus - spec);
        if (n >= sizeof word) {
            return 0;
        }
        memcpy(word, spec, n);
        word[n] = '\0';
        if (ieq(word, "ctrl")) {
            *mods |= TXT_MOD_CTRL;
        } else if (ieq(word, "alt")) {
            *mods |= TXT_MOD_ALT;
        } else if (ieq(word, "shift")) {
            *mods |= TXT_MOD_SHIFT;
        } else {
            return 0;
        }
        spec = plus + 1;
    }
    for (i = 0; i < sizeof k_keys / sizeof k_keys[0]; i++) {
        if (ieq(spec, k_keys[i].name)) {
            *code = k_keys[i].code;
            return 1;
        }
    }
    if ((spec[0] == 'f' || spec[0] == 'F') && spec[1] >= '1' && spec[1] <= '9') {
        unsigned n = 0;
        if (sscanf(spec + 1, "%u", &n) == 1 && n >= 1u && n <= 12u) {
            *code = TXT_KEY_F1 + n - 1u;
            return 1;
        }
    }
    if (spec[0] != '\0' && spec[1] == '\0') {
        *code = (uint32_t)(unsigned char)spec[0];
        return 1;
    }
    return 0;
}

/* "1.25" or "-3" as Q16; at most 4 fractional numerals. */
static int q16(const char* s, int32_t* out) {
    int neg = 0;
    int64_t whole = 0, frac = 0, den = 1;
    int seen = 0;
    if (*s == '-') {
        neg = 1;
        s++;
    }
    for (; *s >= '0' && *s <= '9'; s++, seen++) {
        whole = whole * 10 + (*s - '0');
        if (whole > 30000) {
            return 0;
        }
    }
    if (*s == '.') {
        for (s++; *s >= '0' && *s <= '9'; s++, seen++) {
            if (den < 10000) {
                frac = frac * 10 + (*s - '0');
                den *= 10;
            }
        }
    }
    if (seen == 0) {
        return 0;
    }
    whole = whole * 65536 + (frac * 65536) / den;
    *out = (int32_t)(neg ? -whole : whole);
    return 1;
}

int sel_gui_input_parse(const char* line, sel_gui_input* ev) {
    char verb[16], a[SEL_GUI_INPUT_PATH], b[32];
    int n;
    memset(ev, 0, sizeof *ev);
    line += strspn(line, " \t");
    if (*line == '\0' || *line == '#') {
        return 0;
    }
    n = sscanf(line, "%15s %255s %31s", verb, a, b);
    if (n < 1) {
        return 0;
    }
    if (strcmp(verb, "key") == 0 && n >= 2) {
        ev->kind = SEL_GUI_IN_KEY;
        return sel_gui_key_parse(a, &ev->code, &ev->mods);
    }
    if ((strcmp(verb, "click") == 0 || strcmp(verb, "dclick") == 0 || strcmp(verb, "rclick") == 0) && n == 3) {
        int count = 1;
        ev->kind = SEL_GUI_IN_CLICK;
        ev->clicks = verb[0] == 'd' ? 2u : 1u;
        ev->button = verb[0] == 'r' ? 1u : 0u;
        if (verb[0] == 'c' && sscanf(line, "%*s %*s %*s %d", &count) == 1 && count >= 1) { /* click X Y COUNT */
            ev->clicks = (uint32_t)count;
        }
        return sscanf(a, "%d", &ev->x) == 1 && sscanf(b, "%d", &ev->y) == 1;
    }
    if (strcmp(verb, "move") == 0 && n == 3) {
        ev->kind = SEL_GUI_IN_MOVE;
        return sscanf(a, "%d", &ev->x) == 1 && sscanf(b, "%d", &ev->y) == 1;
    }
    if (strcmp(verb, "drag") == 0) {
        ev->kind = SEL_GUI_IN_DRAG;
        return sscanf(line, "%*s %d %d %d %d", &ev->x, &ev->y, &ev->x2, &ev->y2) == 4;
    }
    if (strcmp(verb, "resize") == 0 && n == 3) {
        ev->kind = SEL_GUI_IN_RESIZE;
        return sscanf(a, "%d", &ev->x) == 1 && sscanf(b, "%d", &ev->y) == 1;
    }
    if (strcmp(verb, "wheel") == 0 && n == 3) {
        ev->kind = SEL_GUI_IN_WHEEL;
        return q16(a, &ev->dx) && q16(b, &ev->dy);
    }
    if (strcmp(verb, "pinch") == 0 && n == 2) {
        ev->kind = SEL_GUI_IN_PINCH;
        return q16(a, &ev->scale);
    }
    if (strcmp(verb, "swipe") == 0 && n == 2) {
        ev->kind = SEL_GUI_IN_SWIPE;
        ev->x = strcmp(a, "right") == 0 ? 1 : -1;
        return strcmp(a, "right") == 0 || strcmp(a, "left") == 0;
    }
    if (strcmp(verb, "drop") == 0 && n >= 2) {
        ev->kind = SEL_GUI_IN_DROP;
        strncpy(ev->path, a, sizeof ev->path - 1u);
        return 1;
    }
    if (strcmp(verb, "wait") == 0 && n == 2) {
        ev->kind = SEL_GUI_IN_WAIT;
        return sscanf(a, "%u", &ev->ms) == 1;
    }
    ev->kind = SEL_GUI_IN_NONE;
    return 0;
}

static int32_t absdiff(int32_t a, int32_t b) {
    return a > b ? a - b : b - a;
}

int sel_gui_input_map(sel_gui_input_map_state* st, const sel_gui_raw* raw, sel_gui_input* out) {
    memset(out, 0, sizeof *out);
    switch (raw->kind) {
    case SEL_GUI_RAW_BUTTON_DOWN:
        if (raw->button == 1u || raw->button == 3u) {
            st->down_x = raw->x;
            st->down_y = raw->y;
            st->down_button = raw->button;
        }
        return 0;
    case SEL_GUI_RAW_BUTTON_UP:
        if (raw->button == 4u || raw->button == 5u) { /* the back and forward buttons are the swipe */
            out->kind = SEL_GUI_IN_SWIPE;
            out->x = raw->button == 4u ? 1 : -1;
            return 1;
        }
        if (raw->button != 1u && raw->button != 3u) {
            return 0;
        }
        out->x = raw->x;
        out->y = raw->y;
        out->button = raw->button == 3u ? 1u : 0u;
        if (st->down_button == raw->button &&
            (absdiff(raw->x, st->down_x) > SEL_GUI_DRAG_SLOP || absdiff(raw->y, st->down_y) > SEL_GUI_DRAG_SLOP)) {
            out->kind = SEL_GUI_IN_DRAG;
            out->x = st->down_x;
            out->y = st->down_y;
            out->x2 = raw->x;
            out->y2 = raw->y;
        } else {
            out->kind = SEL_GUI_IN_CLICK;
            out->clicks = raw->clicks > 0u ? raw->clicks : 1u;
        }
        st->down_button = 0u;
        return 1;
    case SEL_GUI_RAW_MOTION:
        out->kind = SEL_GUI_IN_MOVE;
        out->x = raw->x;
        out->y = raw->y;
        return 1;
    case SEL_GUI_RAW_WHEEL:
        if (raw->dx == 0 && raw->dy == 0) {
            return 0;
        }
        out->kind = SEL_GUI_IN_WHEEL;
        out->dx = raw->dx;
        out->dy = raw->dy;
        return 1;
    case SEL_GUI_RAW_PINCH:
        if (raw->scale <= 0) {
            return 0;
        }
        out->kind = SEL_GUI_IN_PINCH;
        out->scale = raw->scale;
        return 1;
    case SEL_GUI_RAW_SWIPE:
        if (raw->dx == 0) {
            return 0;
        }
        out->kind = SEL_GUI_IN_SWIPE;
        out->x = raw->dx > 0 ? 1 : -1;
        return 1;
    case SEL_GUI_RAW_KEY:
    case SEL_GUI_RAW_TEXT:
        if (raw->code == 0u) {
            return 0;
        }
        out->kind = SEL_GUI_IN_KEY;
        out->code = raw->code;
        out->mods = raw->kind == SEL_GUI_RAW_KEY ? raw->mods : 0u;
        return 1;
    default:
        return 0;
    }
}

/* I685j: token / gadget mapping of the UP-MCC.mcp page; see up_prefs_map.h. */
#include "up_prefs_map.h"

#include <selecta/uimodel/tokens.h>

#include <stdio.h>
#include <string.h>

static int parse_hex(const char* s, uint32_t* out) {
    uint32_t v = 0;
    int n = 0;
    if (s[0] == '#') {
        s++;
    } else if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        s += 2;
    }
    for (; *s != '\0'; s++, n++) {
        char c = *s;
        uint32_t d;
        if (c >= '0' && c <= '9') {
            d = (uint32_t)(c - '0');
        } else if (c >= 'a' && c <= 'f') {
            d = (uint32_t)(c - 'a' + 10);
        } else if (c >= 'A' && c <= 'F') {
            d = (uint32_t)(c - 'A' + 10);
        } else {
            return -1;
        }
        v = (v << 4) | d;
    }
    if (n != 6) {
        return -1;
    }
    *out = v;
    return 0;
}

static void put_hex(char* dst, uint32_t rgb) {
    snprintf(dst, 12, "%06X", (unsigned)(rgb & 0xFFFFFFu));
}

void up_prefs_tokens_to_gadgets(const uint32_t* tokens, up_prefs_gadgets* g) {
    put_hex(g->accent, tokens[SEL_GUI_TOKEN_ACCENT]);
    put_hex(g->surface, tokens[SEL_GUI_TOKEN_SURFACE]);
    put_hex(g->text, tokens[SEL_GUI_TOKEN_TEXT]);
    put_hex(g->background, tokens[SEL_GUI_TOKEN_BACKGROUND]);
    g->radius = (int)tokens[SEL_GUI_TOKEN_RADIUS];
}

int up_prefs_gadgets_to_tokens(const up_prefs_gadgets* g, uint32_t* tokens) {
    uint32_t a, s, t, b;
    int r = g->radius;
    if (parse_hex(g->accent, &a) != 0 || parse_hex(g->surface, &s) != 0 || parse_hex(g->text, &t) != 0 || parse_hex(g->background, &b) != 0) {
        return -1;
    }
    if (r < (int)sel_gui_token_min(SEL_GUI_TOKEN_RADIUS)) {
        r = (int)sel_gui_token_min(SEL_GUI_TOKEN_RADIUS);
    }
    if (r > (int)sel_gui_token_max(SEL_GUI_TOKEN_RADIUS)) {
        r = (int)sel_gui_token_max(SEL_GUI_TOKEN_RADIUS);
    }
    tokens[SEL_GUI_TOKEN_ACCENT] = a;
    tokens[SEL_GUI_TOKEN_SURFACE] = s;
    tokens[SEL_GUI_TOKEN_TEXT] = t;
    tokens[SEL_GUI_TOKEN_BACKGROUND] = b;
    tokens[SEL_GUI_TOKEN_RADIUS] = (uint32_t)r;
    return 0;
}

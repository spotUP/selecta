/* I1139: see mui_text.h. */
#include "mui_text.h"

#include <stdlib.h>
#include <string.h>

#include "source/latin1.h"

typedef struct keep_node {
    struct keep_node* next;
    char text[1];
} keep_node;
static keep_node* g_keep;

static int is_ascii(const char* s) {
    const unsigned char* p = (const unsigned char*)s;
    while (*p != 0u) {
        if (*p >= 0x80u) {
            return 0;
        }
        p++;
    }
    return 1;
}

const char* mui_l1(char* buf, size_t cap, const char* s) {
    if (s == NULL || is_ascii(s)) {
        return s;
    }
    sel_utf8_to_latin1_draw(buf, cap, s);
    return buf;
}

const char* mui_l1_keep(const char* s) {
    keep_node* n;
    if (s == NULL || is_ascii(s)) {
        return s;
    }
    n = (keep_node*)malloc(sizeof *n + strlen(s));
    if (n == NULL) {
        return s;
    }
    sel_utf8_to_latin1_draw(n->text, strlen(s) + 1u, s);
    n->next = g_keep;
    g_keep = n;
    return n->text;
}

void mui_text_free(void) {
    while (g_keep != NULL) {
        keep_node* n = g_keep;
        g_keep = n->next;
        free(n);
    }
}

void mui_aatext_spec_l1(const mui_aatext_spec* in, mui_aatext_spec* out, char* a, char* b) {
    *out = *in;
    out->contents = mui_l1(a, MUI_L1_TEXT, in->contents);
    out->widest = mui_l1(b, MUI_L1_TEXT, in->widest);
}

void mui_aabtn_spec_l1(const mui_aabtn_spec* in, mui_aabtn_spec* out, char* a, char* b, char* c) {
    *out = *in;
    out->caption = mui_l1(a, MUI_L1_TEXT, in->caption);
    out->widest = mui_l1(b, MUI_L1_TEXT, in->widest);
    out->help = mui_l1(c, MUI_L1_TEXT, in->help);
}

void mui_aafield_spec_l1(const mui_aafield_spec* in, mui_aafield_spec* out) {
    *out = *in;
    out->placeholder = mui_l1_keep(in->placeholder); /* the field keeps it by pointer */
}

size_t mui_l1_to_utf8(char* dst, size_t cap, const char* src) {
    size_t n = src != NULL ? strlen(src) : 0u;
    if (cap == 0u) {
        return 0u;
    }
    if (n > (cap - 1u) / 2u) { /* worst case two bytes per input byte: cut the input so the output fits */
        n = (cap - 1u) / 2u;
    }
    return sel_latin1_to_utf8(dst, cap, src != NULL ? src : "", n);
}

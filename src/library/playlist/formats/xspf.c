/* XSPF 1: <track> with <location> (the key), <title>, <duration> (ms) and a
 * Selecta extension for what XSPF has no field for:
 * <extension application="selecta"><subsong>N</subsong><stealth>1</stealth></extension>.
 * No divider: dropped with the warning "lossy divider". The reader is a
 * small tag scanner (no DTD, no CDATA, no namespaces beyond the default). */
#include "../playlist_internal.h"

#include <string.h>

#define XSPF_BUF 1024u

static const char* find_str(const char* s, size_t len, const char* needle) {
    size_t m = strlen(needle);
    size_t i;
    for (i = 0; i + m <= len; i++) {
        if (memcmp(s + i, needle, m) == 0) {
            return s + i;
        }
    }
    return NULL;
}

/* Content of <tag>...</tag> inside [s, s+len); NULL when absent. */
static const char* element(const char* s, size_t len, const char* tag, size_t* out_len) {
    char open[24];
    char close[24];
    const char* a;
    const char* b;
    size_t t = strlen(tag);
    if (t + 4u > sizeof open) {
        return NULL;
    }
    open[0] = '<';
    memcpy(open + 1, tag, t);
    open[t + 1] = '>';
    open[t + 2] = '\0';
    close[0] = '<';
    close[1] = '/';
    memcpy(close + 2, tag, t);
    close[t + 2] = '>';
    close[t + 3] = '\0';
    a = find_str(s, len, open);
    if (a == NULL) {
        return NULL;
    }
    a += t + 2u;
    b = find_str(a, (size_t)(s + len - a), close);
    if (b == NULL) {
        return NULL;
    }
    *out_len = (size_t)(b - a);
    return a;
}

static const struct {
    const char* ent;
    size_t len;
    char ch;
} entities[] = {{"&amp;", 5, '&'}, {"&lt;", 4, '<'}, {"&gt;", 4, '>'}, {"&quot;", 6, '"'}, {"&apos;", 6, '\''}};

/* Unescapes into dst; returns the length or (size_t)-1 when it does not fit. */
static size_t unescape(const char* s, size_t len, char* dst, size_t cap) {
    size_t i = 0;
    size_t o = 0;
    while (i < len) {
        char c = s[i];
        size_t k;
        size_t step = 1;
        if (c == '&') {
            for (k = 0; k < sizeof entities / sizeof entities[0]; k++) {
                if (i + entities[k].len <= len && memcmp(s + i, entities[k].ent, entities[k].len) == 0) {
                    c = entities[k].ch;
                    step = entities[k].len;
                    break;
                }
            }
        }
        if (o + 1u >= cap) {
            return (size_t)-1;
        }
        dst[o++] = c;
        i += step;
    }
    return o;
}

int sel_pl_read_xspf(const char* bytes, size_t len, sel_playlist* pl) {
    const char* p = bytes;
    const char* end = bytes + len;
    while (p < end) {
        const char* t = find_str(p, (size_t)(end - p), "<track>");
        const char* te;
        const char* v;
        size_t vl;
        char key[XSPF_BUF];
        char name[XSPF_BUF];
        size_t key_len = 0;
        size_t name_len = 0;
        uint32_t duration = 0;
        uint32_t subsong = 0;
        sel_pl_kind kind = SEL_PL_SONG;
        int rc;
        if (t == NULL) {
            break;
        }
        t += 7;
        te = find_str(t, (size_t)(end - t), "</track>");
        if (te == NULL) {
            return SEL_PL_FORMAT;
        }
        p = te + 8;
        v = element(t, (size_t)(te - t), "location", &vl);
        if (v == NULL) {
            continue;
        }
        key_len = unescape(v, vl, key, sizeof key);
        v = element(t, (size_t)(te - t), "title", &vl);
        if (v != NULL) {
            name_len = unescape(v, vl, name, sizeof name);
        }
        if (key_len == (size_t)-1 || name_len == (size_t)-1) {
            return SEL_PL_RANGE;
        }
        v = element(t, (size_t)(te - t), "duration", &vl);
        if (v != NULL) {
            size_t used;
            duration = sel_pl_parse_u(v, vl, &used);
        }
        v = element(t, (size_t)(te - t), "subsong", &vl);
        if (v != NULL) {
            size_t used;
            subsong = sel_pl_parse_u(v, vl, &used);
        }
        if (find_str(t, (size_t)(te - t), "<stealth>1</stealth>") != NULL) {
            kind = SEL_PL_STEALTH;
        }
        rc = sel_playlist_add(pl, kind, key, key_len, name, name_len, (uint8_t)(subsong > 255u ? 0u : subsong), duration);
        if (rc != SEL_PL_OK) {
            return rc;
        }
    }
    return SEL_PL_OK;
}

static void put_escaped(sel_pl_out* o, const char* s) {
    const char* run = s;
    for (; *s != '\0'; s++) {
        const char* rep = NULL;
        if (*s == '&') {
            rep = "&amp;";
        } else if (*s == '<') {
            rep = "&lt;";
        } else if (*s == '>') {
            rep = "&gt;";
        }
        if (rep != NULL) {
            sel_pl_put(o, run, (size_t)(s - run));
            sel_pl_puts(o, rep);
            run = s + 1;
        }
    }
    sel_pl_put(o, run, (size_t)(s - run));
}

int sel_pl_write_xspf(const sel_playlist* pl, sel_pl_out* o, sel_pl_warn_fn warn, void* warn_ctx) {
    uint32_t i;
    sel_pl_puts(o, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                   "<playlist version=\"1\" xmlns=\"http://xspf.org/ns/0/\">\n<trackList>\n");
    for (i = 0; i < pl->count; i++) {
        const sel_pl_entry* e = &pl->entries[i];
        if (e->kind == SEL_PL_DIVIDER) {
            if (warn != NULL) {
                warn(warn_ctx, "lossy divider");
            }
            continue;
        }
        sel_pl_puts(o, "<track><location>");
        put_escaped(o, sel_playlist_str(pl, e->key));
        sel_pl_puts(o, "</location>");
        if (e->name != 0) {
            sel_pl_puts(o, "<title>");
            put_escaped(o, sel_playlist_str(pl, e->name));
            sel_pl_puts(o, "</title>");
        }
        if (e->duration_ms != 0) {
            sel_pl_puts(o, "<duration>");
            sel_pl_putu(o, e->duration_ms);
            sel_pl_puts(o, "</duration>");
        }
        if (e->subsong != 0 || e->kind == SEL_PL_STEALTH) {
            sel_pl_puts(o, "<extension application=\"selecta\">");
            if (e->subsong != 0) {
                sel_pl_puts(o, "<subsong>");
                sel_pl_putu(o, e->subsong);
                sel_pl_puts(o, "</subsong>");
            }
            if (e->kind == SEL_PL_STEALTH) {
                sel_pl_puts(o, "<stealth>1</stealth>");
            }
            sel_pl_puts(o, "</extension>");
        }
        sel_pl_puts(o, "</track>\n");
    }
    sel_pl_puts(o, "</trackList>\n</playlist>\n");
    return SEL_PL_OK;
}

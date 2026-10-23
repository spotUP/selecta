/* Playlist model, format detection and dispatch (plan M1.2 row I72). */
#include "playlist_internal.h"

#include <string.h>

void sel_playlist_init(sel_playlist* pl, sel_pl_entry* entries, uint32_t cap, char* text, uint32_t text_cap) {
    pl->entries = entries;
    pl->count = 0;
    pl->cap = cap;
    pl->text = text;
    pl->text_cap = text_cap;
    pl->text_len = 0;
    if (text_cap > 0) {
        text[0] = '\0';
        pl->text_len = 1;
    }
}

const char* sel_playlist_str(const sel_playlist* pl, uint32_t offset) {
    return pl->text + offset;
}

static int add_text(sel_playlist* pl, const char* s, size_t len, uint32_t* off) {
    if (len == 0) {
        *off = 0;
        return SEL_PL_OK;
    }
    if (len >= (size_t)pl->text_cap || pl->text_len + len + 1u > pl->text_cap) {
        return SEL_PL_RANGE;
    }
    *off = pl->text_len;
    memcpy(pl->text + pl->text_len, s, len);
    pl->text[pl->text_len + len] = '\0';
    pl->text_len += (uint32_t)len + 1u;
    return SEL_PL_OK;
}

int sel_playlist_add(sel_playlist* pl, sel_pl_kind kind, const char* key, size_t key_len, const char* name, size_t name_len,
                     uint8_t subsong, uint32_t duration_ms) {
    sel_pl_entry* e;
    uint32_t koff = 0;
    uint32_t noff = 0;
    if (pl->count >= pl->cap || pl->text_cap == 0) {
        return SEL_PL_RANGE;
    }
    if (add_text(pl, key, key_len, &koff) != SEL_PL_OK || add_text(pl, name, name_len, &noff) != SEL_PL_OK) {
        return SEL_PL_RANGE;
    }
    e = &pl->entries[pl->count++];
    e->kind = (uint8_t)kind;
    e->subsong = subsong;
    e->key = koff;
    e->name = noff;
    e->duration_ms = duration_ms;
    return SEL_PL_OK;
}

static int ieq_tail(const char* name, const char* ext) {
    size_t n = strlen(name);
    size_t e = strlen(ext);
    size_t i;
    if (n < e) {
        return 0;
    }
    for (i = 0; i < e; i++) {
        char c = name[n - e + i];
        if (c >= 'A' && c <= 'Z') {
            c = (char)(c - 'A' + 'a');
        }
        if (c != ext[i]) {
            return 0;
        }
    }
    return 1;
}

sel_pl_format sel_playlist_format_from_name(const char* name) {
    if (ieq_tail(name, ".prg")) {
        return SEL_PL_HIPPO;
    }
    if (ieq_tail(name, ".m3u") || ieq_tail(name, ".m3u8")) {
        return SEL_PL_M3U;
    }
    if (ieq_tail(name, ".pls")) {
        return SEL_PL_PLS;
    }
    if (ieq_tail(name, ".xspf")) {
        return SEL_PL_XSPF;
    }
    return SEL_PL_AUTO;
}

static int starts(const char* b, size_t len, const char* magic) {
    size_t m = strlen(magic);
    return len >= m && memcmp(b, magic, m) == 0;
}

sel_pl_format sel_playlist_sniff(const char* bytes, size_t len) {
    if (starts(bytes, len, "\xEF\xBB\xBF")) { /* UTF-8 byte order mark */
        bytes += 3;
        len -= 3;
    }
    if (starts(bytes, len, "HiPPrg")) {
        return SEL_PL_HIPPO;
    }
    if (starts(bytes, len, "#EXTM3U")) {
        return SEL_PL_M3U;
    }
    if (starts(bytes, len, "[playlist]")) {
        return SEL_PL_PLS;
    }
    if (starts(bytes, len, "<?xml")) {
        return SEL_PL_XSPF;
    }
    return SEL_PL_AUTO;
}

int sel_pl_next_line(const char* bytes, size_t total, size_t* pos, const char** line, size_t* len) {
    size_t p = *pos;
    size_t s = p;
    if (p >= total) {
        return 0;
    }
    while (p < total && bytes[p] != '\n' && bytes[p] != '\r') {
        p++;
    }
    *line = bytes + s;
    *len = p - s;
    if (p < total && bytes[p] == '\r' && p + 1 < total && bytes[p + 1] == '\n') {
        p++;
    }
    if (p < total) {
        p++;
    }
    *pos = p;
    return 1;
}

uint32_t sel_pl_parse_u(const char* s, size_t len, size_t* used) {
    uint32_t v = 0;
    size_t i = 0;
    while (i < len && s[i] >= '0' && s[i] <= '9') {
        v = v * 10u + (uint32_t)(s[i] - '0');
        i++;
    }
    *used = i;
    return v;
}

void sel_pl_put(sel_pl_out* o, const char* s, size_t len) {
    if (o->err || len == 0) {
        return;
    }
    if (!o->fn(o->ctx, s, (uint32_t)len)) {
        o->err = SEL_PL_SINK;
    }
}

void sel_pl_puts(sel_pl_out* o, const char* s) {
    sel_pl_put(o, s, strlen(s));
}

void sel_pl_putu(sel_pl_out* o, uint32_t v) {
    char buf[12];
    int i = 12;
    do {
        buf[--i] = (char)('0' + v % 10u);
        v /= 10u;
    } while (v != 0);
    sel_pl_put(o, buf + i, (size_t)(12 - i));
}

int sel_playlist_read(sel_pl_format fmt, const char* bytes, size_t len, sel_playlist* pl, sel_pl_warn_fn warn, void* warn_ctx) {
    (void)warn;
    (void)warn_ctx;
    if (fmt == SEL_PL_AUTO) {
        fmt = sel_playlist_sniff(bytes, len);
    }
    if (len >= 3 && memcmp(bytes, "\xEF\xBB\xBF", 3) == 0) {
        bytes += 3;
        len -= 3;
    }
    switch (fmt) {
    case SEL_PL_HIPPO: return sel_pl_read_hippo(bytes, len, pl);
    case SEL_PL_M3U: return sel_pl_read_m3u(bytes, len, pl);
    case SEL_PL_PLS: return sel_pl_read_pls(bytes, len, pl);
    case SEL_PL_XSPF: return sel_pl_read_xspf(bytes, len, pl);
    default: return SEL_PL_FORMAT;
    }
}

int sel_playlist_write(sel_pl_format fmt, const sel_playlist* pl, sel_pl_sink_fn sink, void* sink_ctx, sel_pl_warn_fn warn,
                       void* warn_ctx) {
    sel_pl_out o;
    int rc;
    o.fn = sink;
    o.ctx = sink_ctx;
    o.err = 0;
    switch (fmt) {
    case SEL_PL_HIPPO: rc = sel_pl_write_hippo(pl, &o, warn, warn_ctx); break;
    case SEL_PL_M3U: rc = sel_pl_write_m3u(pl, &o, warn, warn_ctx); break;
    case SEL_PL_PLS: rc = sel_pl_write_pls(pl, &o, warn, warn_ctx); break;
    case SEL_PL_XSPF: rc = sel_pl_write_xspf(pl, &o, warn, warn_ctx); break;
    default: return SEL_PL_FORMAT;
    }
    return rc != SEL_PL_OK ? rc : o.err;
}

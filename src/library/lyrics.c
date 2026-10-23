/* LRC lyrics parser (F19, I193). See lyrics.h. */
#include "lyrics.h"

#include <string.h>

static int is_digit(char c) {
    return c >= '0' && c <= '9';
}

/* Parses "mm:ss", "mm:ss.f", "mm:ss.ff", "mm:ss.fff" or "mm:ss:ff" starting at p (after '['), ends at ']'. Returns the
 * position after ']' and sets *ms, or NULL when it is not a timestamp. */
static const char* parse_stamp(const char* p, const char* end, uint32_t* ms) {
    uint32_t min = 0, sec = 0, frac = 0, digits = 0, scale = 1;

    if (p >= end || !is_digit(*p)) return NULL;
    while (p < end && is_digit(*p) && digits < 4) {
        min = min * 10u + (uint32_t)(*p++ - '0');
        digits++;
    }
    if (p >= end || *p != ':') return NULL;
    p++;
    digits = 0;
    while (p < end && is_digit(*p) && digits < 3) {
        sec = sec * 10u + (uint32_t)(*p++ - '0');
        digits++;
    }
    if (digits == 0 || sec > 99u) return NULL;
    if (p < end && (*p == '.' || *p == ':')) {
        p++;
        digits = 0;
        while (p < end && is_digit(*p) && digits < 3) {
            frac = frac * 10u + (uint32_t)(*p++ - '0');
            digits++;
            scale *= 10u;
        }
        if (digits == 0) return NULL;
        frac = frac * 1000u / scale;
    }
    if (p >= end || *p != ']') return NULL;
    *ms = (min * 60u + sec) * 1000u + frac;
    return p + 1;
}

/* Inserts a line, keeping `ms` sorted (equal times keep file order). 0 when full. */
static int add_line(sel_lrc* l, uint32_t ms, const char* text, size_t len) {
    uint32_t i;

    if (l->count >= SEL_LRC_MAX_LINES || (size_t)l->used + len + 1u > SEL_LRC_TEXT_MAX) return 0;
    for (i = l->count; i > 0 && l->ms[i - 1u] > ms; i--) {
        l->ms[i] = l->ms[i - 1u];
        l->off[i] = l->off[i - 1u];
    }
    l->ms[i] = ms;
    l->off[i] = l->used;
    memcpy(l->text + l->used, text, len);
    l->text[l->used + len] = '\0';
    l->used = (uint16_t)(l->used + len + 1u);
    l->count++;
    return 1;
}

uint32_t sel_lrc_parse(sel_lrc* out, const char* src, size_t len) {
    const char* p = src;
    const char* end = src + len;
    int32_t offset = 0;
    uint32_t i;

    memset(out, 0, sizeof *out);
    if (len >= 3 && (unsigned char)p[0] == 0xEFu && (unsigned char)p[1] == 0xBBu && (unsigned char)p[2] == 0xBFu) p += 3;
    while (p < end) {
        const char* eol = p;
        const char* q = p;
        uint32_t stamps[16];
        unsigned n = 0;

        while (eol < end && *eol != '\n' && *eol != '\r') eol++;
        while (q < eol && *q == '[') {
            uint32_t ms;
            const char* after = parse_stamp(q + 1, eol, &ms);

            if (after == NULL) break;
            if (n < 16u) stamps[n++] = ms;
            q = after;
        }
        if (n == 0 && eol - p > 8 && strncmp(p, "[offset:", 8) == 0) {
            const char* o = p + 8;
            int neg = 0;
            int32_t v = 0;

            if (o < eol && (*o == '+' || *o == '-')) neg = *o++ == '-';
            while (o < eol && is_digit(*o) && v < 3600000) v = v * 10 + (*o++ - '0');
            offset = neg ? -v : v;
        } else if (n > 0) {
            while (q < eol && (*q == ' ' || *q == '\t')) q++;
            for (i = 0; i < n; i++) {
                if (!add_line(out, stamps[i], q, (size_t)(eol - q))) break;
            }
        }
        p = eol;
        while (p < end && (*p == '\n' || *p == '\r')) p++;
    }
    if (offset != 0) {
        for (i = 0; i < out->count; i++) {
            int32_t t = (int32_t)out->ms[i] - offset;

            out->ms[i] = t < 0 ? 0u : (uint32_t)t; /* every line moves the same way, so the order holds */
        }
    }
    return out->count;
}

const char* sel_lrc_text(const sel_lrc* l, uint32_t i) {
    return i < l->count ? l->text + l->off[i] : NULL;
}

int32_t sel_lrc_index_at(const sel_lrc* l, uint32_t ms) {
    int32_t lo = 0, hi = (int32_t)l->count - 1, best = -1;

    while (lo <= hi) {
        int32_t mid = (lo + hi) / 2;

        if (l->ms[mid] <= ms) {
            best = mid;
            lo = mid + 1;
        } else {
            hi = mid - 1;
        }
    }
    return best;
}

int sel_lrc_path(const char* song, char* out, size_t cap) {
    size_t n = strlen(song), base = 0, dot = (size_t)-1, i;

    for (i = 0; i < n; i++) {
        if (song[i] == '/' || song[i] == ':') {
            base = i + 1u;
            dot = (size_t)-1;
        } else if (song[i] == '.' && i > base) {
            dot = i;
        }
    }
    if (dot != (size_t)-1) n = dot;
    if (n + 5u > cap) return 0;
    memcpy(out, song, n);
    memcpy(out + n, ".lrc", 5);
    return 1;
}

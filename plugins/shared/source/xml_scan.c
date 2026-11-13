/* A small XML tag walker shared by the podcast RSS reader and the UPnP device description reader (I894a1). Not a
 * general XML parser: it finds the first `<tag ...>` in a bounded range and returns its text or an attribute. No
 * allocation, no floating point. */
#include "source/xml_scan.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

/* Appends code point cp as UTF-8 to out (cap bytes free); returns the byte count, 0 when it does not fit or is invalid. */
static size_t put_utf8(unsigned long cp, char* out, size_t cap) {
    if (cp == 0 || cp > 0x10FFFFul || (cp >= 0xD800ul && cp <= 0xDFFFul)) {
        return 0;
    }
    if (cp < 0x80ul && cap >= 1u) {
        out[0] = (char)cp;
        return 1;
    }
    if (cp < 0x800ul && cap >= 2u) {
        out[0] = (char)(0xC0u | (cp >> 6));
        out[1] = (char)(0x80u | (cp & 0x3Fu));
        return 2;
    }
    if (cp < 0x10000ul && cap >= 3u) {
        out[0] = (char)(0xE0u | (cp >> 12));
        out[1] = (char)(0x80u | ((cp >> 6) & 0x3Fu));
        out[2] = (char)(0x80u | (cp & 0x3Fu));
        return 3;
    }
    if (cp >= 0x10000ul && cap >= 4u) {
        out[0] = (char)(0xF0u | (cp >> 18));
        out[1] = (char)(0x80u | ((cp >> 12) & 0x3Fu));
        out[2] = (char)(0x80u | ((cp >> 6) & 0x3Fu));
        out[3] = (char)(0x80u | (cp & 0x3Fu));
        return 4;
    }
    return 0;
}

/* The code point of the entity whose '&' is at p (decimal and hex numeric references, the named ones feeds use);
 * *len is the entity's length with the ';'. 0 when p does not start a known entity. */
static unsigned long entity_codepoint(const char* p, const char* end, size_t* len) {
    static const struct {
        const char* name;
        unsigned long cp;
    } named[] = {{"amp", '&'},      {"lt", '<'},        {"gt", '>'},        {"quot", '"'},     {"apos", '\''},
                 {"nbsp", 0xA0},    {"hellip", 0x2026}, {"ndash", 0x2013},  {"mdash", 0x2014}, {"rsquo", 0x2019},
                 {"lsquo", 0x2018}, {"ldquo", 0x201C},  {"rdquo", 0x201D},  {"bull", 0x2022},  {"middot", 0xB7},
                 {"copy", 0xA9},    {"reg", 0xAE},      {"trade", 0x2122},  {"laquo", 0xAB},   {"raquo", 0xBB}};
    const char* q = p + 1;
    const char* semi = q;
    size_t i;

    while (semi < end && semi - q < 12 && *semi != ';' && *semi != '&' && *semi != ' ') {
        semi++;
    }
    if (semi >= end || *semi != ';' || semi == q) {
        return 0;
    }
    *len = (size_t)(semi - p) + 1u;
    if (*q == '#') {
        unsigned long cp = 0;
        int hex = q[1] == 'x' || q[1] == 'X';
        const char* d = q + 1 + (hex ? 1 : 0);

        if (d == semi) {
            return 0;
        }
        for (; d < semi; d++) {
            int v = isdigit((unsigned char)*d) ? *d - '0' : (hex && isxdigit((unsigned char)*d)) ? (tolower((unsigned char)*d) - 'a' + 10) : -1;

            if (v < 0) {
                return 0;
            }
            cp = cp * (hex ? 16ul : 10ul) + (unsigned long)v;
            if (cp > 0x10FFFFul) {
                return 0;
            }
        }
        return cp;
    }
    for (i = 0; i < sizeof named / sizeof named[0]; i++) {
        if (strlen(named[i].name) == (size_t)(semi - q) && strncmp(q, named[i].name, (size_t)(semi - q)) == 0) {
            return named[i].cp;
        }
    }
    return 0;
}

size_t sel_xml_entity(const char* p, const char* end, char* out, size_t cap, size_t* used) {
    size_t len = 0, k;
    unsigned long cp = entity_codepoint(p, end, &len);

    if (cp == 0) {
        return 0;
    }
    if (cp == '&' && p + len < end && p[len] == '#') {
        /* Feeds that escape twice send "&amp;#039;": parse "#039;" as if the ';' of "&amp;" were the '&'. */
        size_t len2 = 0;
        unsigned long cp2 = entity_codepoint(p + len - 1, end, &len2);

        if (cp2 != 0) {
            cp = cp2;
            len = len - 1u + len2;
        }
    }
    k = put_utf8(cp, out, cap);
    if (k == 0) {
        return 0;
    }
    *used = len;
    return k;
}

/* Copies the text between `<tag ...>` and `</tag>` inside [s, end) to out, decoding CDATA and the XML/HTML entities
 * (sel_xml_entity), so a feed's "&#8230;" and "&#039;" reach the catalogue as UTF-8. */
int sel_xml_tag_text(const char* s, const char* end, const char* tag, char* out, size_t n) {
    char open[32];
    const char* p = s;
    size_t ol = (size_t)snprintf(open, sizeof open, "<%s", tag), o = 0;

    while ((p = memchr(p, '<', (size_t)(end - p))) != NULL) {
        if ((size_t)(end - p) > ol && strncmp(p, open, ol) == 0 && (p[ol] == '>' || p[ol] == ' ' || p[ol] == '/')) {
            break;
        }
        p++;
    }
    if (p == NULL || (p = memchr(p, '>', (size_t)(end - p))) == NULL || p[-1] == '/') {
        return 0;
    }
    for (p++; p < end && o + 1 < n;) {
        if (strncmp(p, "</", 2) == 0 && (size_t)(end - p) >= 2u) {
            break;
        }
        if ((size_t)(end - p) >= 9u && strncmp(p, "<![CDATA[", 9) == 0) {
            p += 9;
            while (p < end && strncmp(p, "]]>", 3) != 0 && o + 1 < n) {
                out[o++] = *p++;
            }
            p += p < end ? 3 : 0;
        } else if (*p == '&') {
            size_t used = 0, k = sel_xml_entity(p, end, out + o, n - o - 1u, &used);

            if (k > 0) {
                o += k;
                p += used;
            } else {
                out[o++] = *p++;
            }
        } else {
            out[o++] = *p++;
        }
    }
    out[o] = '\0';
    while (o > 0 && isspace((unsigned char)out[o - 1])) {
        out[--o] = '\0';
    }
    return o > 0;
}

/* The value of attribute `attr` of the first `<tag ...>` in [s, end). */
int sel_xml_tag_attr(const char* s, const char* end, const char* tag, const char* attr, char* out, size_t n) {
    char open[32], needle[24];
    const char* p = s;
    const char* close;
    size_t ol = (size_t)snprintf(open, sizeof open, "<%s ", tag), o = 0, nl = (size_t)snprintf(needle, sizeof needle, "%s=\"", attr);

    while ((p = memchr(p, '<', (size_t)(end - p))) != NULL) {
        if ((size_t)(end - p) > ol && strncmp(p, open, ol) == 0) {
            break;
        }
        p++;
    }
    if (p == NULL || (close = memchr(p, '>', (size_t)(end - p))) == NULL) {
        return 0;
    }
    for (p += ol; p + nl < close; p++) {
        if (strncmp(p, needle, nl) == 0 && (p[-1] == ' ' || p[-1] == '\n' || p[-1] == '\t')) {
            p += nl;
            while (p < close && *p != '"' && o + 1 < n) {
                size_t used = 0, k = *p == '&' ? sel_xml_entity(p, close, out + o, n - o - 1u, &used) : 0;

                if (k > 0) {
                    o += k;
                    p += used;
                } else {
                    out[o++] = *p++;
                }
            }
            out[o] = '\0';
            return o > 0;
        }
    }
    return 0;
}


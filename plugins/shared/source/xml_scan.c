/* A small XML tag walker shared by the podcast RSS reader and the UPnP device description reader (I894a1). Not a
 * general XML parser: it finds the first `<tag ...>` in a bounded range and returns its text or an attribute. No
 * allocation, no floating point. */
#include "source/xml_scan.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

/* Copies the text between `<tag ...>` and `</tag>` inside [s, end) to out, decoding CDATA and the five XML entities. */
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
            static const struct {
                const char* e;
                char c;
            } ent[] = {{"&amp;", '&'}, {"&lt;", '<'}, {"&gt;", '>'}, {"&quot;", '"'}, {"&apos;", '\''}};
            size_t i;
            int hit = 0;

            for (i = 0; i < 5 && !hit; i++) {
                size_t l = strlen(ent[i].e);

                if ((size_t)(end - p) >= l && strncmp(p, ent[i].e, l) == 0) {
                    out[o++] = ent[i].c;
                    p += l;
                    hit = 1;
                }
            }
            if (!hit) {
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
                if (strncmp(p, "&amp;", 5) == 0) {
                    out[o++] = '&';
                    p += 5;
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


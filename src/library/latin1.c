/* ISO-8859-1 <-> UTF-8 conversion, declared in latin1.h. */
#include "latin1.h"

#include <string.h>

size_t sel_latin1_to_utf8(char* dst, size_t n, const char* src, size_t len) {
    size_t o = 0, i;

    if (n == 0) {
        return 0;
    }
    for (i = 0; i < len; i++) {
        unsigned char c = (unsigned char)src[i];

        if (c < 0x80u) {
            if (o + 1u >= n) {
                break;
            }
            dst[o++] = (char)c;
        } else {
            if (o + 2u >= n) {
                break;
            }
            dst[o++] = (char)(0xC0u | (c >> 6));
            dst[o++] = (char)(0x80u | (c & 0x3Fu));
        }
    }
    dst[o] = '\0';
    return o;
}

int sel_utf8_to_latin1(char* dst, size_t n, const char* src) {
    size_t o = 0;

    if (n == 0) {
        return 0;
    }
    while (*src != '\0') {
        unsigned char c = (unsigned char)*src;
        unsigned cp;

        if (c < 0x80u) {
            cp = c;
            src++;
        } else if ((c == 0xC2u || c == 0xC3u) && ((unsigned char)src[1] & 0xC0u) == 0x80u) {
            cp = ((c & 0x1Fu) << 6) | ((unsigned char)src[1] & 0x3Fu);
            src += 2;
        } else {
            dst[0] = '\0'; /* above U+00FF, overlong (C0, C1) or malformed */
            return 0;
        }
        if (o + 1u >= n) {
            dst[0] = '\0';
            return 0;
        }
        dst[o++] = (char)cp;
    }
    dst[o] = '\0';
    return 1;
}

int sel_utf8_valid(const char* s) {
    const unsigned char* p = (const unsigned char*)s;

    while (*p != 0u) {
        unsigned c = *p, cp, need, i;

        if (c < 0x80u) {
            p++;
            continue;
        }
        if (c >= 0xC2u && c <= 0xDFu) {
            need = 1;
            cp = c & 0x1Fu;
        } else if (c >= 0xE0u && c <= 0xEFu) {
            need = 2;
            cp = c & 0x0Fu;
        } else if (c >= 0xF0u && c <= 0xF4u) {
            need = 3;
            cp = c & 0x07u;
        } else {
            return 0; /* continuation byte, C0/C1 overlong lead, or F5..FF */
        }
        for (i = 1; i <= need; i++) {
            if ((p[i] & 0xC0u) != 0x80u) {
                return 0; /* also stops at the NUL of a truncated sequence */
            }
            cp = (cp << 6) | (p[i] & 0x3Fu);
        }
        if ((need == 2 && cp < 0x800u) || (need == 3 && (cp < 0x10000u || cp > 0x10FFFFu)) ||
            (cp >= 0xD800u && cp <= 0xDFFFu)) {
            return 0;
        }
        p += need + 1u;
    }
    return 1;
}

size_t sel_text_to_utf8(char* dst, size_t n, const char* src) {
    size_t l = strlen(src);

    if (!sel_utf8_valid(src)) {
        return sel_latin1_to_utf8(dst, n, src, l);
    }
    if (l >= n) {
        l = n - 1u;
        while (l > 0u && ((unsigned char)src[l] & 0xC0u) == 0x80u) {
            l--; /* src[l] is the first byte cut off: back up to a character start */
        }
    }
    memcpy(dst, src, l);
    dst[l] = '\0';
    return l;
}

int sel_latin1_is_ascii(const char* s) {
    for (; *s != '\0'; s++) {
        if ((unsigned char)*s >= 0x80u) {
            return 0;
        }
    }
    return 1;
}

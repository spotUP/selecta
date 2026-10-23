/* See record_name.h. */
#include "record_name.h"

#include <stdio.h>
#include <string.h>
#if !defined(__amigaos__) && !defined(AMIGA)
#include <sys/stat.h>
#endif

#define NAME_MAX_BYTES 120u

static int exists(const char* path) {
    FILE* f = fopen(path, "rb");
    if (f == NULL) {
        return 0;
    }
    fclose(f);
    return 1;
}

static size_t clean(char* dst, size_t cap, const char* src) {
    size_t n = 0;
    while (*src == ' ') {
        src++;
    }
    for (; *src != '\0' && n < NAME_MAX_BYTES && n + 1 < cap; src++) {
        unsigned char c = (unsigned char)*src;
        int bad = c < 32 || c == 127 || strchr("/\\:*?\"<>|", c) != NULL;
        dst[n++] = bad ? '_' : (char)c;
    }
    while (n > 0 && (dst[n - 1] == ' ' || dst[n - 1] == '.')) {
        n--;
    }
    dst[n] = '\0';
    return n;
}

int sel_record_name(char* out, size_t cap, const char* dir, const char* name, const char* fallback, const char* ext) {
    char base[NAME_MAX_BYTES + 2];
    unsigned i;
    int n;
    if (clean(base, sizeof base, name != NULL ? name : "") == 0 &&
        clean(base, sizeof base, fallback != NULL ? fallback : "") == 0) {
        strcpy(base, "stream");
    }
#if !defined(__amigaos__) && !defined(AMIGA)
    if (dir != NULL && dir[0] != '\0') {
        (void)mkdir(dir, 0777); /* exists already is fine; a real failure shows at fopen */
    }
#endif
    for (i = 1; i < 10000u; i++) {
        if (dir == NULL || dir[0] == '\0') {
            n = i == 1 ? snprintf(out, cap, "%s.%s", base, ext) : snprintf(out, cap, "%s (%u).%s", base, i, ext);
        } else {
            n = i == 1 ? snprintf(out, cap, "%s/%s.%s", dir, base, ext)
                       : snprintf(out, cap, "%s/%s (%u).%s", dir, base, i, ext);
        }
        if (n < 0 || (size_t)n >= cap) {
            return 0;
        }
        if (!exists(out)) {
            return 1;
        }
    }
    return 0;
}

const char* sel_record_ext(const char* content_type, const char* url, char* buf, size_t cap) {
    const char* dot = NULL;
    const char* p;
    size_t n = 0;
    if (content_type != NULL) {
        if (strcmp(content_type, "audio/mpeg") == 0 || strcmp(content_type, "audio/mp3") == 0) {
            return "mp3";
        }
        if (strcmp(content_type, "audio/aac") == 0 || strcmp(content_type, "audio/aacp") == 0) {
            return "aac";
        }
        if (strcmp(content_type, "audio/ogg") == 0 || strcmp(content_type, "application/ogg") == 0) {
            return "ogg";
        }
    }
    for (p = url != NULL ? url : ""; *p != '\0' && *p != '?' && *p != '#'; p++) {
        if (*p == '.') {
            dot = p + 1;
        } else if (*p == '/') {
            dot = NULL;
        }
    }
    if (dot != NULL) {
        for (; dot + n < p && n + 1 < cap && n < 5; n++) {
            buf[n] = dot[n];
        }
        buf[n] = '\0';
        if (n > 0) {
            return buf;
        }
    }
    return "mp3";
}

void sel_record_id3(uint8_t* dst, const char* title) {
    size_t len = title != NULL ? strlen(title) : 0, body = SEL_RECORD_ID3_SIZE - 10u, frame;
    memset(dst, 0, SEL_RECORD_ID3_SIZE);
    memcpy(dst, "ID3\3\0\0", 6);
    dst[6] = (uint8_t)((body >> 21) & 0x7F); /* synchsafe tag size, header excluded */
    dst[7] = (uint8_t)((body >> 14) & 0x7F);
    dst[8] = (uint8_t)((body >> 7) & 0x7F);
    dst[9] = (uint8_t)(body & 0x7F);
    if (len == 0) {
        return;
    }
    if (len > body - 11u) {
        len = body - 11u;
    }
    frame = len + 1u; /* encoding byte (ISO-8859-1) + text */
    memcpy(dst + 10, "TIT2", 4);
    dst[14] = (uint8_t)(frame >> 24);
    dst[15] = (uint8_t)(frame >> 16);
    dst[16] = (uint8_t)(frame >> 8);
    dst[17] = (uint8_t)frame;
    dst[20] = 0;
    memcpy(dst + 21, title, len);
}

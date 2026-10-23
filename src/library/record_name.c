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

/* Length of the MPEG audio frame (Layer I, II or III; MPEG 1, 2 or 2.5) whose header is at h, 0 if h is no frame
 * header. Free-format frames (bitrate index 0) carry no length and count as no header. The minimp3 plugin has its
 * own Layer III-only copy (plugins build against the SDK alone). */
static uint32_t mpeg_frame(const uint8_t* h) {
    static const uint16_t kbps[5][15] = {
        {0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448}, /* MPEG 1 Layer I */
        {0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384},    /* MPEG 1 Layer II */
        {0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320},     /* MPEG 1 Layer III */
        {0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256},    /* MPEG 2 / 2.5 Layer I */
        {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160},         /* MPEG 2 / 2.5 Layer II, III */
    };
    static const uint32_t hz[3] = {44100u, 48000u, 32000u};
    unsigned ver = (h[1] >> 3) & 3u, layer = (h[1] >> 1) & 3u, idx = (unsigned)(h[2] >> 4), rate = (h[2] >> 2) & 3u;
    unsigned pad = (h[2] >> 1) & 1u, row;
    uint32_t br, sr;
    if (h[0] != 0xFFu || (h[1] & 0xE0u) != 0xE0u || ver == 1u || layer == 0u || idx == 0u || idx == 15u ||
        rate == 3u) {
        return 0;
    }
    row = ver == 3u ? 3u - layer : (layer == 3u ? 3u : 4u);
    br = kbps[row][idx] * 1000u;
    sr = hz[rate] >> (ver == 3u ? 0 : (ver == 2u ? 1 : 2));
    if (layer == 3u) {
        return (12u * br / sr + pad) * 4u;
    }
    return (layer == 1u && ver != 3u ? 72u : 144u) * br / sr + pad;
}

/* The frame starting with the held bytes: its length, 0 while more header bytes are needed, -1 if none starts. */
static long cut_check(const sel_record_cut* c) {
    if (c->kind == 1) {
        return c->held < 4u ? 0 : (mpeg_frame(c->hold) != 0u ? (long)mpeg_frame(c->hold) : -1);
    }
    if (c->held < 4u) {
        return 0;
    }
    if (memcmp(c->hold, "OggS", 4) != 0 || (c->held >= 5u && c->hold[4] != 0u)) {
        return -1;
    }
    if (c->held < 27u || c->held < 27u + c->hold[26]) {
        return 0;
    }
    {
        long n = 27 + c->hold[26];
        unsigned i;
        for (i = 0; i < c->hold[26]; i++) {
            n += c->hold[27u + i];
        }
        return n;
    }
}

static uint32_t cut_need(const sel_record_cut* c) {
    if (c->kind == 1) {
        return 4u;
    }
    return c->held < 27u ? 27u : 27u + c->hold[26];
}

/* A frame (or, out of sync too long, any byte) starts: is it the split asked for? */
static int cut_take(sel_record_cut* c) {
    if (c->split == 1) {
        c->split = 0;
        return 1;
    }
    if (c->split == 2) {
        c->split = 1;
    }
    return 0;
}

void sel_record_cut_init(sel_record_cut* c, const char* ext) {
    memset(c, 0, sizeof *c);
    if (strcmp(ext, "mp3") == 0 || strcmp(ext, "mp2") == 0 || strcmp(ext, "mpga") == 0) {
        c->kind = 1;
    } else if (strcmp(ext, "ogg") == 0 || strcmp(ext, "oga") == 0 || strcmp(ext, "opus") == 0) {
        c->kind = 2;
    }
}

void sel_record_cut_request(sel_record_cut* c) {
    /* Held header bytes belong to a frame that started before the request: the split waits for the one after. */
    c->split = c->kind != 0 && c->held > 0u ? 2 : 1;
}

void sel_record_cut_feed(sel_record_cut* c, const uint8_t* p, size_t len, sel_record_cut_out out, void* ctx) {
    while (len > 0) {
        size_t n;
        long frame;
        if (c->kind == 0) {
            out(ctx, p, len, cut_take(c));
            return;
        }
        if (c->left > 0u) {
            n = len < c->left ? len : c->left;
            out(ctx, p, n, 0);
            c->left -= (uint32_t)n;
            p += n;
            len -= n;
            continue;
        }
        n = cut_need(c) - c->held;
        if (n > len) {
            n = len;
        }
        if (c->held < 4u && n > 4u - c->held) {
            n = 4u - c->held; /* check the sync bytes before taking a whole Ogg header */
        }
        memcpy(c->hold + c->held, p, n);
        c->held += (uint32_t)n;
        p += n;
        len -= n;
        while (c->held > 0u && (frame = cut_check(c)) != 0) {
            if (frame > 0) {
                out(ctx, c->hold, c->held, cut_take(c));
                c->left = (uint32_t)frame - c->held;
                c->held = 0;
                c->lost = 0;
            } else {
                /* Not a frame start: pass one byte on and look again one byte later. */
                out(ctx, c->hold, 1, c->lost >= SEL_RECORD_CUT_GIVE_UP ? cut_take(c) : 0);
                c->lost++;
                memmove(c->hold, c->hold + 1, --c->held);
            }
        }
    }
}

void sel_record_cut_flush(sel_record_cut* c, sel_record_cut_out out, void* ctx) {
    if (c->held > 0u) {
        out(ctx, c->hold, c->held, 0);
        c->held = 0;
    }
}

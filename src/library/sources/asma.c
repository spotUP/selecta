/* The `asma` source descriptor (row I413; online-sources plan section 2; design doc 9.1): ASMA, the Atari SAP
 * Music Archive. A path is relative to the /asma/ root: `Composers/Aki/Atari_Style.sap`, `Games/221B_Baker_Street.sap`;
 * the song key is `asma:<path>` and the canonical URL is https://asma.atari.org/asma/<path>, percent-encoded. */
#include "../source.h"
#include "asma_sync.h"

#include <stdio.h>
#include <string.h>

#define ROOT "https://asma.atari.org/" SEL_ASMA_ROOT

static const sel_tag g_tags[] = {
    {SEL_TAG_END, 0},
};

static int url_of(const char* path, char* out, size_t n) {
    static const char hex[] = "0123456789ABCDEF";
    size_t used = sizeof ROOT - 1u;

    if (path == NULL || path[0] == '\0' || path[0] == '/' || n < sizeof ROOT) {
        return 0;
    }
    memcpy(out, ROOT, sizeof ROOT);
    for (; *path != '\0'; path++) {
        unsigned char c = (unsigned char)*path;
        int plain = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '/' ||
                    c == '-' || c == '_' || c == '.' || c == '~';

        if (used + (plain ? 1u : 3u) + 1u > n) {
            return 0;
        }
        if (plain) {
            out[used++] = (char)c;
        } else {
            out[used++] = '%';
            out[used++] = hex[c >> 4];
            out[used++] = hex[c & 15u];
        }
    }
    out[used] = '\0';
    return 1;
}

static int hex_value(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    return -1;
}

static int path_in(const char* url, char* path, size_t n) {
    size_t used = 0;

    if (url == NULL || strncmp(url, ROOT, sizeof ROOT - 1u) != 0) {
        return 0;
    }
    for (url += sizeof ROOT - 1u; *url != '\0'; url++) {
        char c = *url;

        if (c == '%') {
            int hi = hex_value(url[1]);
            int lo = hi >= 0 ? hex_value(url[2]) : -1;

            if (lo < 0) {
                return 0;
            }
            c = (char)(hi * 16 + lo);
            url += 2;
        }
        if (used + 1u >= n) {
            return 0;
        }
        path[used++] = c;
    }
    path[used] = '\0';
    return used > 0;
}

/* Format from the extension; the artist of `Composers/<Folder>/...` is the folder ("Bernard_Jeremie" ->
 * "Bernard Jeremie"; the sync overrides it with the author of the list); name = file without extension. */
static int parse(const char* path, char* scratch, size_t n, sel_tag* out) {
    const char* file;
    const char* dot;
    const char* artist = "";
    size_t artist_len = 0, fmt_len, name_len, i;
    const char* fmt;

    if (path == NULL || (file = strrchr(path, '/')) == NULL || file[1] == '\0') {
        file = path != NULL ? path : "";
        if (file[0] == '\0') {
            return 0;
        }
    } else {
        file++;
    }
    dot = strrchr(file, '.');
    fmt = dot != NULL && strcmp(dot, ".ttt") == 0 ? "TIATracker" : "Slight Atari Player";
    if (strncmp(path, "Composers/", 10) == 0) {
        const char* e = strchr(path + 10, '/');

        if (e != NULL) {
            artist = path + 10;
            artist_len = (size_t)(e - artist);
        }
    }
    fmt_len = strlen(fmt);
    name_len = dot != NULL && dot != file ? (size_t)(dot - file) : strlen(file);
    if (fmt_len + artist_len + name_len + 3u > n) {
        return 0;
    }
    memcpy(scratch, fmt, fmt_len + 1u);
    for (i = 0; i < artist_len; i++) {
        scratch[fmt_len + 1u + i] = artist[i] == '_' ? ' ' : artist[i];
    }
    scratch[fmt_len + 1u + artist_len] = '\0';
    memcpy(scratch + fmt_len + artist_len + 2u, file, name_len);
    scratch[fmt_len + artist_len + 2u + name_len] = '\0';
    out[0].tag = SEL_SRC_FORMAT;
    out[0].value = (uintptr_t)scratch;
    out[1].tag = SEL_SRC_ARTIST;
    out[1].value = (uintptr_t)(scratch + fmt_len + 1u);
    out[2].tag = SEL_SRC_NAME;
    out[2].value = (uintptr_t)(scratch + fmt_len + artist_len + 2u);
    out[3].tag = SEL_TAG_END;
    out[3].value = 0;
    return 1;
}

static const sel_source g_asma = {
    "asma", "ASMA", "https://asma.atari.org/", "Atari SAP Music Archive: music of the Atari 8-bit computers.", g_tags,
    NULL, parse, NULL, url_of, path_in, NULL, NULL,
};

const sel_source* sel_source_asma(void) {
    return &g_asma;
}

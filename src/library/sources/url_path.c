/* See url_path.h. */
#include "url_path.h"

#include <string.h>

int sel_url_path_encode(const char* root, const char* path, char* out, size_t n) {
    static const char hex[] = "0123456789ABCDEF";
    size_t root_len = strlen(root), used = root_len;

    if (path == NULL || path[0] == '\0' || path[0] == '/' || n < root_len + 1u) {
        return 0;
    }
    memcpy(out, root, root_len + 1u);
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

int sel_url_path_decode(const char* root, const char* url, char* path, size_t n) {
    size_t used = 0, root_len = strlen(root);

    if (url == NULL || strncmp(url, root, root_len) != 0) {
        return 0;
    }
    for (url += root_len; *url != '\0'; url++) {
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

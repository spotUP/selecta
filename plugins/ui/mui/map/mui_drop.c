/* MUI-P5-6: the pure halves of the drop and clipboard hook-up. The Amiga side (mui_ui.c) resolves an AppMessage
 * argument to a path and hands it to mui_drop_line; it hands the shown song to mui_clip_text and the text to
 * mui_clip_iff, then writes the image to clipboard.device. */
#include "mui_map.h"

#include <stdio.h>
#include <string.h>

/* One AppWindow argument to its engine line: a file joins the queue (M4-P7-2), a drawer runs the scan the desktop runs
 * for a dropped folder. The path is quoted and escaped like the desktop's desk_send_path. Returns the line length,
 * 0 when the line would not fit or the path is empty. */
uint32_t mui_drop_line(const char* path, int is_dir, char* out, size_t cap) {
    const char* verb = is_dir ? "LIBRARY_SCAN" : "QUEUE_ADD";
    size_t o;
    if (path == NULL || path[0] == '\0' || cap < 20u) {
        return 0;
    }
    o = (size_t)snprintf(out, cap, "%s \"", verb);
    for (; *path != '\0'; path++) {
        if (o + 3u >= cap) {
            out[0] = '\0';
            return 0;
        }
        if (*path == '"' || *path == '\\') {
            out[o++] = '\\';
        }
        out[o++] = *path;
    }
    if (o + 2u > cap) {
        out[0] = '\0';
        return 0;
    }
    out[o++] = '"';
    out[o] = '\0';
    return (uint32_t)o;
}

/* The text Copy Song Title puts on the clipboard: "Artist - Title", or the title alone. 0 when nothing is shown. */
uint32_t mui_clip_text(const char* artist, const char* title, char* out, size_t cap) {
    int n;
    if (title == NULL || title[0] == '\0' || cap == 0u) {
        return 0;
    }
    n = artist != NULL && artist[0] != '\0' ? snprintf(out, cap, "%s - %s", artist, title) : snprintf(out, cap, "%s", title);
    return n > 0 && (size_t)n < cap ? (uint32_t)n : 0;
}

static void put_be32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

/* The IFF FTXT image clipboard.device holds for text: FORM <len> FTXT CHRS <n> <text> [pad]. Returns the byte count,
 * 0 when it does not fit. */
uint32_t mui_clip_iff(const char* text, uint32_t n, uint8_t* out, size_t cap) {
    uint32_t pad = n & 1u, total = 20u + n + pad;
    if (n == 0u || cap < total) {
        return 0;
    }
    memcpy(out, "FORM", 4);
    put_be32(out + 4, 12u + n + pad);
    memcpy(out + 8, "FTXT", 4);
    memcpy(out + 12, "CHRS", 4);
    put_be32(out + 16, n);
    memcpy(out + 20, text, n);
    if (pad != 0u) {
        out[20u + n] = 0;
    }
    return total;
}

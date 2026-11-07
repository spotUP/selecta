/* I625a3: see mui_fontprobe.h. */
#include "mui_fontprobe.h"

#include <stddef.h>
#include <string.h>

/* I625a6a: miss memory. The list is at most FONT_ADD_MAX (4) added paths + the theme font + MUI_FONT_DEFAULT = 6 names; 8 slots of
 * 128 bytes (the length of one added path) leave room. A longer list is probed without memory beyond the slots. */
#define MISS_MAX 8
#define MISS_LEN 128

static char g_list[MISS_MAX][MISS_LEN];
static unsigned char g_miss[MISS_MAX];
static int g_list_n = -1; /* -1: no list remembered */

void mui_fontprobe_reset(void) {
    g_list_n = -1;
}

static int same_list(const char* const* names, int n) {
    int i;
    if (g_list_n != n) {
        return 0;
    }
    for (i = 0; i < n; i++) {
        if (strncmp(g_list[i], names[i], MISS_LEN - 1) != 0) {
            return 0;
        }
    }
    return names[n] == NULL;
}

void* mui_fontprobe_open(const mui_fontprobe_ops* ops, const char* const* names) {
    void* saved;
    void* font = NULL;
    int i, n = 0;
    while (names[n] != NULL) {
        n++;
    }
    if (n > MISS_MAX) {
        n = -2; /* too long to remember: no memory */
    }
    if (n >= 0 && !same_list(names, n)) {
        for (i = 0; i < n; i++) {
            strncpy(g_list[i], names[i], MISS_LEN - 1);
            g_list[i][MISS_LEN - 1] = '\0';
            g_miss[i] = 0;
        }
        g_list_n = n;
    }
    saved = ops->quiet_begin(ops->ctx);
    for (i = 0; names[i] != NULL && font == NULL; i++) {
        if (n >= 0 && g_miss[i]) {
            continue;
        }
        font = ops->open(ops->ctx, names[i]);
        if (font == NULL && n >= 0) {
            g_miss[i] = 1;
        }
    }
    ops->quiet_end(ops->ctx, saved);
    return font;
}

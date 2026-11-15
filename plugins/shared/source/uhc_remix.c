/* Shared half of the UHC remix sources (row I1407); see uhc_remix.h. */
#include "source/uhc_remix.h"
#include "source/url_path.h"

#include <stdio.h>
#include <string.h>

#define KEY_CAP 400
#define DIR_MARK "listen/"

/* "listen/<id>/<name>.mp3": exactly one slash after the mark, a non-empty id and name, and no character a key must not hold. */
static int key_ok(const char* key) {
    size_t n = strlen(key);
    const char* slash;
    const char* c;

    if (strncmp(key, DIR_MARK, 7) != 0 || strcmp(key + n - (n >= 4 ? 4 : n), ".mp3") != 0 || n < 7u + 1u + 1u + 4u) {
        return 0;
    }
    slash = strchr(key + 7, '/');
    if (slash == NULL || slash == key + 7 || slash[1] == '\0' || strchr(slash + 1, '/') != NULL || strstr(key, "..") != NULL) {
        return 0;
    }
    for (c = key; *c != '\0'; c++) {
        if ((unsigned char)*c < 32 || *c == '|' || *c == '"' || *c == '\\' || *c == '?' || *c == '#') {
            return 0;
        }
    }
    return 1;
}

int sel_uhc_remix_row(void* ctx, const char* dir, const char* file, const char* title, const char* artist, const char* format) {
    const sel_search_sink* out = ctx;
    char key[KEY_CAP];

    (void)format;
    if ((size_t)snprintf(key, sizeof key, "%s/%s", dir, file) >= sizeof key || !key_ok(key)) {
        return -1;
    }
    return out->row(out->ctx, key, title, artist, "mp3");
}

int sel_uhc_remix_parse(const char* label, const char* path, char* scratch, size_t n, sel_tag* out) {
    const char* name = path != NULL && key_ok(path) ? strchr(path + 7, '/') : NULL;
    size_t ll = strlen(label) + 1u;
    size_t nl;

    if (name == NULL) {
        return 0;
    }
    name++;
    nl = strlen(name) - 4u; /* without ".mp3" */
    if (ll + nl + 1u > n) {
        return 0;
    }
    memcpy(scratch, label, ll);
    memcpy(scratch + ll, name, nl);
    scratch[ll + nl] = '\0';
    out[0].tag = SEL_SRC_FORMAT;
    out[0].value = (uintptr_t)scratch;
    out[1].tag = SEL_SRC_NAME;
    out[1].value = (uintptr_t)(scratch + ll);
    out[2].tag = SEL_TAG_END;
    out[2].value = 0;
    return 1;
}

int sel_uhc_remix_url(const char* base, const char* path, char* out, size_t n) {
    return path != NULL && key_ok(path) && sel_url_path_encode(base, path, out, n);
}

/* Row handling of the `turran` source; see turran_row.h. */
#include "turran_row.h"

#include <selecta/platform.h>

#include <stdio.h>
#include <string.h>

#define KEY_CAP 400
#define DIR_MARK "Collection/"

static int key_ok(const char* key) {
    const char* c;
    const char* name;

    if (strncmp(key, DIR_MARK, 11) != 0 || strstr(key, "..") != NULL) {
        return 0;
    }
    name = strrchr(key, '/') + 1; /* the mark holds a '/' */
    if (name[0] == '\0' || !sel_platform_ext_known(sel_platform_path_ext(name))) {
        return 0;
    }
    for (c = key; *c != '\0'; c++) {
        if ((unsigned char)*c < 32 || *c == '|' || *c == '"' || *c == '\\' || *c == '?' || *c == '#') {
            return 0;
        }
    }
    return 1;
}

int sel_turran_row(void* ctx, const char* dir, const char* file, const char* title, const char* artist, const char* format) {
    const sel_search_sink* out = ctx;
    char key[KEY_CAP], name[KEY_CAP];

    (void)format;
    if ((size_t)snprintf(key, sizeof key, "%s/%s", dir, file) >= sizeof key || !key_ok(key)) {
        return -1;
    }
    if (title == NULL || title[0] == '\0') { /* I1457: the index carries no title; the search row shows the file name without its extension */
        const char* dot = strrchr(file, '.');
        snprintf(name, sizeof name, "%.*s", (int)(dot != NULL && dot != file ? (size_t)(dot - file) : strlen(file)), file);
        title = name;
    }
    return out->row(out->ctx, key, title, artist, sel_platform_path_ext(file));
}

int sel_turran_parse(const char* path, char* scratch, size_t n, sel_tag* out) {
    static const char label[] = "Turran FTP";
    const char* name = path != NULL && key_ok(path) ? strrchr(path, '/') + 1 : NULL;
    size_t nl;

    if (name == NULL) {
        return 0;
    }
    nl = strlen(name) + 1u;
    if (sizeof label + nl > n) {
        return 0;
    }
    memcpy(scratch, label, sizeof label);
    memcpy(scratch + sizeof label, name, nl);
    out[0].tag = SEL_SRC_FORMAT;
    out[0].value = (uintptr_t)scratch;
    out[1].tag = SEL_SRC_NAME;
    out[1].value = (uintptr_t)(scratch + sizeof label);
    out[2].tag = SEL_TAG_END;
    out[2].value = 0;
    return 1;
}

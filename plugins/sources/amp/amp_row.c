/* Row handling of the `amp` source; see amp_row.h. */
#include "amp_row.h"
#include "source/url_path.h"

#include <selecta/platform.h>

#include <stdio.h>
#include <string.h>

#define KEY_CAP 300
#define PROXY "https://hippoplayer.se/cygnus/api/amp/"
#define INDEX_MAX 9 /* the proxy accepts 1..9 digits */

/* "<digits>/<name>": a non-empty name with a known Amiga prefix and none of the characters a key must not hold. Returns the name. */
static const char* key_name(const char* key) {
    const char* name = key;
    const char* dot;
    const char* c;
    char pre[16];
    size_t n;

    while (*name >= '0' && *name <= '9') {
        name++;
    }
    if (name == key || name - key > INDEX_MAX || *name != '/' || name[1] == '\0') {
        return NULL;
    }
    name++;
    dot = strchr(name, '.');
    if (dot == NULL || (n = (size_t)(dot - name)) == 0 || n >= sizeof pre) {
        return NULL;
    }
    memcpy(pre, name, n);
    pre[n] = '\0';
    if (!sel_platform_ext_known(pre)) {
        return NULL;
    }
    for (c = name; *c != '\0'; c++) {
        if ((unsigned char)*c < 32 || *c == '/' || *c == '|' || *c == '"' || *c == '\\' || *c == '?' || *c == '#') {
            return NULL;
        }
    }
    return name;
}

int sel_amp_row(void* ctx, const char* dir, const char* file, const char* title, const char* artist, const char* format) {
    const sel_search_sink* out = ctx;
    const char* idx = strstr(file, "index=");
    char key[KEY_CAP];
    size_t dl = strlen(dir);
    int n;

    (void)format;
    if (idx == NULL || dl < 3u || strcmp(dir + dl - 3, ".gz") != 0) {
        return -1;
    }
    n = snprintf(key, sizeof key, "%.*s/%.*s", (int)strspn(idx + 6, "0123456789"), idx + 6, (int)(dl - 3u), dir);
    if (n < 0 || (size_t)n >= sizeof key || key_name(key) == NULL) {
        return -1;
    }
    return out->row(out->ctx, key, title, artist, "amp");
}

int sel_amp_parse(const char* path, char* scratch, size_t n, sel_tag* out) {
    static const char label[] = "AMP";
    const char* name = path != NULL ? key_name(path) : NULL;
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

int sel_amp_url(const char* path, char* out, size_t n) {
    return path != NULL && key_name(path) != NULL && sel_url_path_encode(PROXY, path, out, n);
}

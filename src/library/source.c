/* Source registry and song keys (plan Phase 4, I157; design doc 9.1). */
#include "source.h"

#include <string.h>

#define SOURCE_MAX 16u

static const sel_source* g_sources[SOURCE_MAX];
static uint32_t g_count;

int sel_source_register(const sel_source* src) {
    if (src == NULL || src->id == NULL || src->id[0] == '\0' || strlen(src->id) >= SEL_SOURCE_ID_MAX ||
        strchr(src->id, ':') != NULL || g_count >= SOURCE_MAX || sel_source_find(src->id) != NULL) {
        return 0;
    }
    g_sources[g_count++] = src;
    return 1;
}

const sel_source* sel_source_find(const char* id) {
    uint32_t i;

    for (i = 0; id != NULL && i < g_count; i++) {
        if (strcmp(g_sources[i]->id, id) == 0) {
            return g_sources[i];
        }
    }
    return NULL;
}

uint32_t sel_source_count(void) {
    return g_count;
}

const sel_source* sel_source_get(uint32_t index) {
    return index < g_count ? g_sources[index] : NULL;
}

void sel_source_register_builtin(void) {
    if (sel_source_find("local") == NULL) {
        (void)sel_source_register(sel_source_local());
    }
    if (sel_source_find("modland") == NULL) {
        (void)sel_source_register(sel_source_modland());
    }
    if (sel_source_find("hvsc") == NULL) {
        (void)sel_source_register(sel_source_hvsc());
    }
    if (sel_source_find("hippo") == NULL) {
        (void)sel_source_register(sel_source_hippo());
    }
}

int sel_song_key(const char* source, const char* path, char* out, size_t n) {
    size_t a, b;

    if (source == NULL || path == NULL || source[0] == '\0') {
        return 0;
    }
    a = strlen(source);
    b = strlen(path);
    if (a + 1u + b + 1u > n) {
        return 0;
    }
    memcpy(out, source, a);
    out[a] = ':';
    memcpy(out + a + 1u, path, b + 1u);
    return 1;
}

int sel_song_key_of(const char* ref, char* out, size_t n) {
    char id[SEL_SOURCE_ID_MAX + 1];
    const char* colon;
    size_t a;

    if (ref == NULL || out == NULL) {
        return 0;
    }
    if (strstr(ref, "://") != NULL) { /* a URL stream is its own identity */
        size_t len = strlen(ref);
        if (len + 1u > n) {
            return 0;
        }
        memcpy(out, ref, len + 1u);
        return 1;
    }
    sel_source_register_builtin();
    colon = strchr(ref, ':');
    a = colon != NULL ? (size_t)(colon - ref) : 0;
    if (a > 0 && a < sizeof id) {
        memcpy(id, ref, a);
        id[a] = '\0';
        if (sel_source_find(id) != NULL) { /* already a key */
            size_t len = strlen(ref);
            if (len + 1u > n) {
                return 0;
            }
            memcpy(out, ref, len + 1u);
            return 1;
        }
    }
    return sel_song_key("local", ref, out, n);
}

int sel_song_key_split(const char* key, char* source, size_t source_n, char* path, size_t path_n) {
    const char* colon = key != NULL ? strchr(key, ':') : NULL;
    size_t a, b;

    if (colon == NULL || colon == key) {
        return 0;
    }
    a = (size_t)(colon - key);
    b = strlen(colon + 1);
    if (a + 1u > source_n || b + 1u > path_n || a >= SEL_SOURCE_ID_MAX) {
        return 0;
    }
    memcpy(source, key, a);
    source[a] = '\0';
    memcpy(path, colon + 1, b + 1u);
    return 1;
}

int sel_source_is_remote(const char* id) {
    if (id == NULL) {
        return 0;
    }
    sel_source_register_builtin();
    return sel_source_find(id) != NULL && strcmp(id, "local") != 0 && strcmp(id, "radio") != 0;
}

/* Source registry and song keys (plan Phase 4, I157; design doc 9.1). */
#include "source.h"

#include <selecta/plugin.h>

#include <stdio.h>
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

uint32_t sel_source_each(sel_source_each_fn fn, void* ctx) {
    uint32_t i;
    sel_source_register_builtin();
    for (i = 0; i < g_count; i++) {
        fn(ctx, g_sources[i]->id, g_sources[i]->name, g_sources[i]->home, sel_source_is_remote(g_sources[i]->id));
    }
    return g_count;
}

/* The plugin enumeration the front end hands in (sel_source_set_plugins). The library does not link the registry:
 * the registry links the engine core, which links the library, so a direct call would close a link cycle. */
static uint32_t (*g_reg_count)(void);
static const sel_tag* (*g_reg_get)(uint32_t);
static const sel_host* g_reg_host;

void sel_source_set_plugins(uint32_t (*count)(void), const sel_tag* (*get)(uint32_t), const sel_host* host) {
    g_reg_count = count;
    g_reg_get = get;
    g_reg_host = host;
}

static void reject(const char* id, const char* why) {
    char msg[96];
    const sel_host* host = g_reg_host;

    snprintf(msg, sizeof msg, "source plugin '%.24s' rejected: %s", id != NULL ? id : "(none)", why);
    if (host != NULL && host->log != NULL) {
        host->log(host, SEL_LOG_ERROR, msg);
    } else {
        fprintf(stderr, "[ERROR] %s\n", msg);
    }
}

/* Registers the source plugins (kind 7) of the plugin registry (I517 phase 3). A plugin whose descriptor is
 * registered already is skipped silently (the walk runs on every lookup); a clashing or over-long id is logged. */
static void register_plugins(void) {
    uint32_t i, n = g_reg_count != NULL && g_reg_get != NULL ? g_reg_count() : 0;

    for (i = 0; i < n; i++) {
        const sel_tag* desc = g_reg_get(i);
        const sel_tag* hooks;
        const sel_source* src;
        const sel_source* have;

        if (desc == NULL || sel_tag_find(desc, SEL_PLUGIN_KIND, 0) != (uintptr_t)SEL_PLUGIN_KIND_SOURCE) {
            continue;
        }
        hooks = (const sel_tag*)sel_tag_find(desc, SEL_PLUGIN_TAGS, 0);
        src = hooks != NULL ? (const sel_source*)sel_tag_find(hooks, SEL_SRC_DESCRIPTOR, 0) : NULL;
        if (src == NULL || src->id == NULL) {
            reject(NULL, "no source descriptor");
        } else if ((have = sel_source_find(src->id)) == src) {
            continue;
        } else if (have != NULL) {
            reject(src->id, "id already registered");
        } else if (strlen(src->id) >= SEL_SOURCE_ID_MAX) {
            reject(src->id, "id too long");
        } else if (!sel_source_register(src)) {
            reject(src->id, "invalid id or registry full");
        }
    }
}

void sel_source_register_builtin(void) {
    if (sel_source_find("local") == NULL) {
        (void)sel_source_register(sel_source_local());
    }
    if (sel_source_find("modland") == NULL) {
        (void)sel_source_register(sel_source_modland());
    }
    if (sel_source_find("hippo") == NULL) {
        (void)sel_source_register(sel_source_hippo());
    }
    register_plugins();
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
    const sel_source* src;

    if (id == NULL) {
        return 0;
    }
    sel_source_register_builtin();
    src = sel_source_find(id);
    return src != NULL && (src->flags & SEL_SOURCE_REMOTE) != 0;
}

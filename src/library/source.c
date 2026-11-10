/* Source registry and song keys (plan Phase 4, I157; design doc 9.1). */
#include "source.h"

#include "archive/archive.h"
#include "../core/count.h"

#include <selecta/plugin.h>

#include <stdio.h>
#include <string.h>

static const sel_source* g_sources[SEL_SOURCE_MAX];
static uint32_t g_count;
static const sel_host* g_reg_host;
static void reject(const char* id, const char* why);

int sel_source_register(const sel_source* src) {
    if (src == NULL || src->id == NULL || src->id[0] == '\0' || strlen(src->id) >= SEL_SOURCE_ID_MAX ||
        strchr(src->id, ':') != NULL || sel_source_find(src->id) != NULL) {
        return 0;
    }
    if (g_count >= SEL_SOURCE_MAX) { /* I738: never a silent drop */
        reject(src->id, "source registry is full");
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

/* I1300: a raw URL of a known host is the same song as its source key. Hosts and the key each one maps to (key shapes read from the
 * plugins: modland keys are `modland:pub/modules/<...>` (modland_sync.c full_path), hvsc keys are `hvsc:<path under C64Music>`
 * (plugins/sources/hvsc/hvsc.c)):
 *   http(s)|ftp://ftp.modland.com/pub/modules/<p>        -> modland:pub/modules/<p>
 *   http(s)://hvsc.c64.org/download/C64Music/<p>         -> hvsc:<p>
 * The path is percent-decoded once. Anything else (unknown host, a key, a local path) is copied unchanged. 0 when it does not fit. */
static size_t url_host_prefix(const char* ref, const char** rest, const char** key_prefix) {
    static const struct { const char* url; const char* key; } map[] = {
        {"http://ftp.modland.com/", "modland:"},
        {"https://ftp.modland.com/", "modland:"},
        {"ftp://ftp.modland.com/", "modland:"},
        {"http://hvsc.c64.org/download/C64Music/", "hvsc:"},
        {"https://hvsc.c64.org/download/C64Music/", "hvsc:"},
    };
    size_t i;

    for (i = 0; i < sizeof map / sizeof map[0]; i++) {
        size_t n = strlen(map[i].url);
        size_t j;

        for (j = 0; j < n; j++) {
            char c = ref[j];
            if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
            char m = map[i].url[j];
            if (m >= 'A' && m <= 'Z') m = (char)(m - 'A' + 'a');
            if (c != m) break;
        }
        if (j == n) {
            *rest = ref + n;
            *key_prefix = map[i].key;
            return n;
        }
    }
    return 0;
}

int sel_song_key_of_url(const char* ref, char* out, size_t n) {
    const char* p;
    const char* kp;
    size_t w;

    if (ref == NULL || out == NULL || n == 0) {
        return 0;
    }
    if (url_host_prefix(ref, &p, &kp) == 0 || (strcmp(kp, "modland:") == 0 && strncmp(p, "pub/modules/", 12) != 0)) {
        size_t len = strlen(ref);
        if (len + 1u > n) {
            return 0;
        }
        memcpy(out, ref, len + 1u);
        return 1;
    }
    w = strlen(kp);
    if (w + 1u > n) {
        return 0;
    }
    memcpy(out, kp, w);
    while (*p != '\0') {
        unsigned v;
        char c = *p;

        if (p[0] == '%' && p[1] != '\0' && p[2] != '\0' && sscanf((char[]){p[1], p[2], 0}, "%2x", &v) == 1) {
            c = (char)v;
            p += 3;
        } else {
            p++;
        }
        if (w + 2u > n) {
            return 0;
        }
        out[w++] = c;
    }
    out[w] = '\0';
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

int sel_source_is_stream(const char* id) {
    const sel_source* src;

    if (id == NULL) {
        return 0;
    }
    sel_source_register_builtin();
    src = sel_source_find(id);
    return src != NULL && (src->flags & SEL_SOURCE_STREAMS) != 0 && src->play_url != NULL;
}

static uint32_t g_stream_resolves;

uint32_t sel_source_stream_resolves(void) {
    return g_stream_resolves;
}

int sel_source_key_seekable(const char* key) {
    char id[SEL_SOURCE_ID_MAX];
    const char* colon = key != NULL ? strchr(key, ':') : NULL;
    size_t len = colon != NULL ? (size_t)(colon - key) : 0;

    if (len == 0 || len >= sizeof id || colon[1] == '/' || colon[1] == '\0') {
        return 0;
    }
    memcpy(id, key, len);
    id[len] = '\0';
    return sel_source_is_stream(id) && (sel_source_find(id)->flags & SEL_SOURCE_SEEKABLE) != 0;
}

const char* sel_source_stream_name(const char* key) {
    char id[SEL_SOURCE_ID_MAX];
    const char* colon = key != NULL ? strchr(key, ':') : NULL;
    size_t len = colon != NULL ? (size_t)(colon - key) : 0;

    if (len == 0 || len >= sizeof id || colon[1] == '/' || colon[1] == '\0') {
        return NULL;
    }
    memcpy(id, key, len);
    id[len] = '\0';
    return sel_source_is_stream(id) ? sel_source_find(id)->name : NULL;
}

int sel_source_stream_url(const sel_host* host, const char* key, char* out, size_t n) {
    char id[SEL_SOURCE_ID_MAX];
    const char* colon = key != NULL ? strchr(key, ':') : NULL;
    size_t len = colon != NULL ? (size_t)(colon - key) : 0;

    if (len == 0 || len >= sizeof id || colon[1] == '/' || colon[1] == '\0') {
        return 0;
    }
    memcpy(id, key, len);
    id[len] = '\0';
    if (!sel_source_is_stream(id)) {
        return 0;
    }
    g_stream_resolves++;
    return sel_source_find(id)->play_url(host, colon + 1, out, n) ? 1 : -1;
}

static uint32_t g_search_runs;

uint32_t sel_source_search_runs(void) {
    return SEL_COUNT_GET(&g_search_runs);
}

int sel_source_search(const sel_host* host, const char* id, const char* query, sel_search_sink* out) {
    const sel_source* src;

    if (id == NULL || query == NULL || out == NULL || out->row == NULL) {
        return -1;
    }
    sel_source_register_builtin();
    src = sel_source_find(id);
    if (src == NULL || src->search == NULL) {
        return -1;
    }
    SEL_COUNT_ADD(&g_search_runs, 1u); /* I1044: searches run on parallel threads */
    return src->search(host, query, out);
}

typedef struct unzip_member {
    const sel_host* host;
    uint8_t* data;
    size_t size;
    int members;
} unzip_member;

static int keep_member(void* ctx, const char* name, const uint8_t* data, size_t size) {
    unzip_member* u = (unzip_member*)ctx;

    (void)name;
    if (++u->members == 1 && size > 0) {
        u->data = u->host->mem_alloc(u->host, size, SEL_MEM_FAST);
        if (u->data != NULL) {
            memcpy(u->data, data, size);
            u->size = size;
        }
    }
    return 1;
}

int sel_source_unzip(const sel_host* host, const char* key, const char* file) {
    char id[SEL_SOURCE_ID_MAX];
    const char* colon = key != NULL ? strchr(key, ':') : NULL;
    size_t len = colon != NULL ? (size_t)(colon - key) : 0;
    const sel_source* src;
    FILE* f;
    long fsz;
    uint8_t* zip;
    unzip_member u;
    char tmp[1100];
    int result = 0;

    if (len == 0 || len >= sizeof id || file == NULL) {
        return 0;
    }
    memcpy(id, key, len);
    id[len] = '\0';
    sel_source_register_builtin();
    src = sel_source_find(id);
    if (src == NULL || (src->flags & SEL_SOURCE_ZIPPED) == 0 || (f = fopen(file, "rb")) == NULL) {
        return 0;
    }
    if (fseek(f, 0, SEEK_END) != 0 || (fsz = ftell(f)) <= 0 || fseek(f, 0, SEEK_SET) != 0 ||
        (zip = host->mem_alloc(host, (size_t)fsz, SEL_MEM_FAST)) == NULL) {
        fclose(f);
        return 0;
    }
    if (fread(zip, 1, (size_t)fsz, f) != (size_t)fsz) {
        fclose(f);
        host->mem_free(host, zip);
        return 0;
    }
    fclose(f);
    memset(&u, 0, sizeof u);
    u.host = host;
    if (sel_archive_kind(zip, (size_t)fsz) == SEL_ARCHIVE_ZIP &&
        sel_archive_each(host, SEL_ARCHIVE_ZIP, zip, (size_t)fsz, 32u * 1024u * 1024u, keep_member, &u) >= 0 &&
        u.members == 1 && u.data != NULL) {
        FILE* o;
        int wrote = 0;

        snprintf(tmp, sizeof tmp, "%s.unzip", file);
        o = fopen(tmp, "wb");
        if (o != NULL) {
            wrote = fwrite(u.data, 1, u.size, o) == u.size;
            wrote = fclose(o) == 0 && wrote;
        }
        if (wrote && rename(tmp, file) == 0) {
            result = 1;
        } else {
            remove(tmp);
            result = -1;
        }
    }
    if (u.data != NULL) {
        host->mem_free(host, u.data);
    }
    host->mem_free(host, zip);
    return result;
}

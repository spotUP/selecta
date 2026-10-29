/* POSIX plugin module loader (design 2026-10-30-plugin-loader-sdk D1, D3, D4). A module is a shared library named
 * sel_*.dylib (macOS) or sel_*.so that exports sel_plugin_entry (include/selecta/plugin.h). Search path, first hit
 * per file name wins: $SELECTA_PLUGIN_PATH (colon list), the app plugin folder (Contents/PlugIns inside a macOS
 * bundle, else <exe dir>/plugins), the user plugin folder (<config dir>/plugins). The Windows LoadLibrary variant
 * lands with the Windows port. */
#include <selecta/engine.h>
#include <selecta/registry.h>

#include "loader.h"

#include <dirent.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#define SEL_MODULE_SUFFIX ".dylib"
#else
#define SEL_MODULE_SUFFIX ".so"
#endif

#define PATH_MAX_SEL 1024
#define SEEN_MAX 128

static char g_seen[SEEN_MAX][256];
static unsigned g_seen_n;

static int already_seen(const char* name) {
    unsigned i;
    for (i = 0; i < g_seen_n; i++) {
        if (strcmp(g_seen[i], name) == 0) {
            return 1;
        }
    }
    return 0;
}

static void load_module(const char* path) {
    void* h = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    void* sym;
    sel_plugin_entry_fn fn;
    if (h == NULL) {
        const char* e = dlerror();
        char why[400];
        snprintf(why, sizeof why, "dlopen failed: %.300s", e != NULL ? e : "unknown error");
        sel_registry_note_rejected(path, why);
        return;
    }
    sym = dlsym(h, "sel_plugin_entry");
    if (sym == NULL) {
        sel_registry_note_rejected(path, "no sel_plugin_entry symbol");
        dlclose(h);
        return;
    }
    memcpy(&fn, &sym, sizeof fn); /* object to function pointer without a pedantic cast */
    if (!sel_registry_add_entry(fn, path)) {
        sel_registry_note_rejected(path, "module table full");
        dlclose(h);
    }
}

static int by_name(const void* a, const void* b) {
    return strcmp((const char*)a, (const char*)b);
}

static void scan_dir(const char* dir) {
    DIR* d = opendir(dir);
    struct dirent* de;
    char names[SEEN_MAX][256];
    unsigned n = 0, i;
    size_t slen = strlen(SEL_MODULE_SUFFIX);
    if (d == NULL) {
        return;
    }
    while ((de = readdir(d)) != NULL && n < SEEN_MAX) {
        size_t l = strlen(de->d_name);
        if (l >= sizeof names[0] || l <= slen + 4u || strncmp(de->d_name, "sel_", 4) != 0 ||
            strcmp(de->d_name + l - slen, SEL_MODULE_SUFFIX) != 0) {
            continue;
        }
        memcpy(names[n++], de->d_name, l + 1u);
    }
    closedir(d);
    qsort(names, n, sizeof names[0], by_name);
    for (i = 0; i < n; i++) {
        char path[PATH_MAX_SEL];
        if (already_seen(names[i]) || g_seen_n >= SEEN_MAX) {
            continue;
        }
        memcpy(g_seen[g_seen_n++], names[i], strlen(names[i]) + 1u);
        if (snprintf(path, sizeof path, "%s/%s", dir, names[i]) < (int)sizeof path) {
            load_module(path);
        }
    }
}

/* The app plugin folder next to the running executable; 0 when the executable path is unknown. */
static int app_plugin_dir(char* out, size_t cap) {
    char exe[PATH_MAX_SEL];
    char* slash;
    int n;
#ifdef __APPLE__
    uint32_t sz = (uint32_t)sizeof exe;
    if (_NSGetExecutablePath(exe, &sz) != 0) {
        return 0;
    }
#else
    ssize_t r = readlink("/proc/self/exe", exe, sizeof exe - 1u);
    if (r <= 0) {
        return 0;
    }
    exe[r] = '\0';
#endif
    slash = strrchr(exe, '/');
    if (slash == NULL) {
        return 0;
    }
    *slash = '\0'; /* exe now holds the executable's directory */
    slash = strrchr(exe, '/');
    if (slash != NULL && strcmp(slash, "/MacOS") == 0) {
        *slash = '\0'; /* .../Contents */
        n = snprintf(out, cap, "%s/PlugIns", exe);
    } else {
        n = snprintf(out, cap, "%s/plugins", exe);
    }
    return n > 0 && (size_t)n < cap;
}

void sel_loader_scan(void) {
    const char* env = getenv("SELECTA_PLUGIN_PATH");
    char dir[PATH_MAX_SEL];
    while (env != NULL && *env != '\0') {
        const char* e = strchr(env, ':');
        size_t len = e != NULL ? (size_t)(e - env) : strlen(env);
        if (len > 0 && len < sizeof dir) {
            memcpy(dir, env, len);
            dir[len] = '\0';
            scan_dir(dir);
        }
        env = e != NULL ? e + 1 : NULL;
    }
    if (app_plugin_dir(dir, sizeof dir)) {
        scan_dir(dir);
    }
    if (sel_config_dir(dir, sizeof dir - 16u)) {
        strcat(dir, "/plugins");
        scan_dir(dir);
    }
}

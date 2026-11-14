/* Amiga on-demand plugin modules (I427e; plugin-loader-sdk D8; addendum
 * thoughts/shared/plans/2026-11-10-amiga-loadseg-plugins.md). The registry holds a small manifest descriptor per
 * module plugin (SEL_PLUGIN_MODULE names the file; TAGS carries the static facts and the probe, so probing and the
 * CPU/FPU filter never load anything). sel_plugin_resolve, called by the engine and the library right before a
 * replayer is opened, lands here: the first call for a manifest LoadSegs its module, checks the 'SELP' header at
 * the start of the first hunk, runs the module's libnix init, calls its sel_plugin_entry and swaps in the real
 * hooks. Modules stay loaded until the process exits (then fini + UnLoadSeg), except those a library scan loaded:
 * the scan end unloads them unless a song holds them, and the next resolve loads them again (I1314b). Search path:
 * $SELECTA_PLUGIN_PATH (one directory), then PROGDIR:Players/. Feeds the same registry as loader_dl.c. */
#include "loader.h"

#include <selecta/plugin.h>
#include <selecta/registry.h>

#include "../core/amiga/dos_quiet.h"

#include <proto/dos.h>
#include <proto/exec.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { kMaxModules = 96, kSegMagic = 0x53454C50 };

/* Core symbols modules reach through their generated thunks (I1378a; cmake/amiga_seg_imports.cmake generates the
 * table from the append-only list cmake/amiga_seg_imports.txt). */
extern const void* const sel_seg_imports[];
extern const uint32_t sel_seg_imports_n;

typedef int (*seg_init_fn)(void);
typedef void (*seg_fini_fn)(void);

typedef struct seg_module {
    const sel_tag* manifest; /* the manifest's hooks, as the registry hands them out */
    const sel_tag* real;     /* the module's hooks once loaded */
    BPTR seg;
    seg_fini_fn fini;
    uint32_t seq; /* g_load_seq at the load (I1314b) */
    int failed;   /* load tried and failed: never retried */
} seg_module;

static seg_module g_mod[kMaxModules];
static unsigned g_mod_n;
static int g_atexit;
static uint32_t g_load_seq;

static void unload_one(seg_module* m) {
    m->fini();
    UnLoadSeg(m->seg);
    m->seg = 0;
    m->real = NULL;
}

static void unload_all(void) {
    unsigned i;
    for (i = 0; i < g_mod_n; i++) {
        if (g_mod[i].seg != 0) {
            unload_one(&g_mod[i]);
        }
    }
}

static uint32_t load_mark(void) {
    return g_load_seq;
}

/* Unloads every module loaded after `mark` whose hooks no open song holds (I1314b). The entry stays, so the next
 * resolve of its manifest loads the file again. */
static uint32_t unload_since(uint32_t mark) {
    unsigned i;
    uint32_t n = 0;
    for (i = 0; i < g_mod_n; i++) {
        seg_module* m = &g_mod[i];
        if (m->seg != 0 && m->seq > mark && !sel_plugin_held(m->real)) {
            unload_one(m);
            sel_registry_note_module_unload();
            n++;
        }
    }
    return n;
}

static BPTR load_from(const char* dir, const char* file) {
    char path[256];
    size_t dl = strlen(dir);
    void* quiet;
    BPTR seg;
    if (dl == 0u) {
        return 0;
    }
    snprintf(path, sizeof path, "%s%s%s", dir, dir[dl - 1u] == ':' || dir[dl - 1u] == '/' ? "" : "/", file);
    quiet = sel_amiga_dos_quiet_begin(); /* a missing assign must not open a requester */
    seg = LoadSeg((STRPTR)path);
    sel_amiga_dos_quiet_end(quiet);
    return seg;
}

/* Loads `file` and returns its descriptor's hooks, or NULL. */
static const sel_tag* load_module(seg_module* m, const char* id, const char* file) {
    const char* env = getenv("SELECTA_PLUGIN_PATH");
    ULONG* hdr;
    const sel_tag* desc;
    const sel_tag* hooks;
    const char* real_id;
    BPTR seg = env != NULL ? load_from(env, file) : 0;
    if (seg == 0) {
        seg = load_from("PROGDIR:Players/", file);
    }
    if (seg == 0) {
        sel_registry_note_rejected(file, "module file not found");
        return NULL;
    }
    hdr = (ULONG*)((UBYTE*)BADDR(seg) + 4);
    if (hdr[1] != kSegMagic || (hdr[2] != 1u && hdr[2] != 2u)) {
        UnLoadSeg(seg);
        sel_registry_note_rejected(file, "not a Selecta module (no SELP header)");
        return NULL;
    }
    if (hdr[2] == 2u) { /* v2: imports slot +24, import count +28 (seg_crt.c) */
        if (hdr[7] > sel_seg_imports_n) {
            UnLoadSeg(seg);
            sel_registry_note_rejected(file, "module needs more imports than this Selecta has (built for a newer one)");
            return NULL;
        }
        hdr[6] = (ULONG)sel_seg_imports; /* before init: constructors may already call an import */
    }
    if (!((seg_init_fn)hdr[3])()) {
        UnLoadSeg(seg);
        sel_registry_note_rejected(file, "module init failed");
        return NULL;
    }
    desc = ((sel_plugin_entry_fn)hdr[5])(1u);
    hooks = desc != NULL ? (const sel_tag*)sel_tag_find(desc, SEL_PLUGIN_TAGS, 0) : NULL;
    real_id = desc != NULL ? (const char*)sel_tag_find(desc, SEL_PLUGIN_ID, 0) : NULL;
    if (hooks == NULL || real_id == NULL || id == NULL || strcmp(real_id, id) != 0) {
        ((seg_fini_fn)hdr[4])();
        UnLoadSeg(seg);
        sel_registry_note_rejected(file, "module descriptor does not match its manifest");
        return NULL;
    }
    m->seg = seg;
    m->fini = (seg_fini_fn)hdr[4];
    m->seq = ++g_load_seq;
    if (!g_atexit) {
        g_atexit = atexit(unload_all) == 0;
    }
    sel_registry_note_module_load();
    return hooks;
}

static const sel_tag* resolve(const sel_tag* hooks) {
    uint32_t i, n;
    seg_module* m = NULL;
    for (i = 0; i < g_mod_n; i++) {
        if (g_mod[i].manifest == hooks) {
            m = &g_mod[i];
            if (m->failed || m->seg != 0) {
                return m->failed ? NULL : m->real;
            }
            break; /* unloaded after a scan (I1314b): load it again below */
        }
    }
    n = sel_registry_count();
    for (i = 0; i < n; i++) {
        const sel_tag* desc = sel_registry_get(i);
        const char* file;
        if ((const sel_tag*)sel_tag_find(desc, SEL_PLUGIN_TAGS, 0) != hooks) {
            continue;
        }
        file = (const char*)sel_tag_find(desc, SEL_PLUGIN_MODULE, 0);
        if (file == NULL) {
            return hooks; /* a plain linked-in plugin */
        }
        if (m == NULL) {
            if (g_mod_n == kMaxModules) {
                return NULL;
            }
            m = &g_mod[g_mod_n++];
            memset(m, 0, sizeof *m);
            m->manifest = hooks;
        }
        m->real = load_module(m, (const char*)sel_tag_find(desc, SEL_PLUGIN_ID, 0), file);
        m->failed = m->real == NULL;
        return m->real;
    }
    return hooks;
}

/* The inverse of resolve (I427e5): a loaded module's real hooks back to the manifest the registry lists. */
static const sel_tag* manifest_of(const sel_tag* hooks) {
    unsigned i;
    for (i = 0; i < g_mod_n; i++) {
        if (g_mod[i].real == hooks && hooks != NULL) {
            return g_mod[i].manifest;
        }
    }
    return hooks;
}

void sel_loader_scan(void) {
    sel_plugin_set_resolver(resolve, manifest_of);
    sel_plugin_set_unloader(load_mark, unload_since);
}

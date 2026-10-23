#include "local.h"

#include "dir.h"
#include "sources/archive.h"

#include <stdio.h>
#include <string.h>

int sel_browse_init(sel_browse* b, const sel_host* host, uint32_t capacity, uint32_t text_bytes) {
    memset(b, 0, sizeof *b);
    b->host = host;
    b->entries = host->mem_alloc(host, (size_t)capacity * sizeof *b->entries, SEL_MEM_FAST);
    b->text = host->mem_alloc(host, text_bytes, SEL_MEM_FAST);
    if (b->entries == NULL || b->text == NULL) {
        sel_browse_free(b, host);
        return 0;
    }
    b->capacity = capacity;
    b->text_bytes = text_bytes;
    b->path[0] = '/';
    return 1;
}

void sel_browse_free(sel_browse* b, const sel_host* host) {
    if (b->entries) {
        host->mem_free(host, b->entries);
    }
    if (b->text) {
        host->mem_free(host, b->text);
    }
    memset(b, 0, sizeof *b);
}

const char* sel_browse_name(const sel_browse* b, uint32_t index) {
    return index < b->count ? b->text + b->entries[index].name_offset : "";
}

uint32_t sel_browse_find(const sel_browse* b, const char* name) {
    uint32_t i;
    for (i = 0; i < b->count; i++) {
        if (strcmp(sel_browse_name(b, i), name) == 0) {
            return i;
        }
    }
    return b->count;
}

static char lower(char c) {
    return c >= 'A' && c <= 'Z' ? (char)(c - 'A' + 'a') : c;
}

/* Directories first, then case-insensitive name. */
static int entry_less(const sel_browse* b, uint32_t is_dir, const char* name, const sel_browse_entry* o) {
    const char* on = b->text + o->name_offset;
    if ((is_dir != 0) != (o->is_dir != 0)) {
        return is_dir != 0;
    }
    while (*name && *on && lower(*name) == lower(*on)) {
        name++;
        on++;
    }
    return lower(*name) < lower(*on);
}

typedef struct list_ctx {
    sel_browse* b;
    uint32_t first; /* entries below this index are fixed (the ".." row) */
} list_ctx;

/* Inserts in sorted position at or after `first`. A full buffer drops the
 * entry and counts it. */
static void add_entry(sel_browse* b, uint32_t first, const char* name, int is_dir) {
    size_t n = strlen(name) + 1u;
    uint32_t lo = first, hi = b->count;
    if (b->count >= b->capacity || b->text_used + n > b->text_bytes) {
        b->truncated++;
        return;
    }
    while (lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2u;
        if (entry_less(b, (uint32_t)is_dir, name, &b->entries[mid])) {
            hi = mid;
        } else {
            lo = mid + 1u;
        }
    }
    memmove(&b->entries[lo + 1u], &b->entries[lo], (size_t)(b->count - lo) * sizeof *b->entries);
    memcpy(b->text + b->text_used, name, n);
    b->entries[lo].name_offset = b->text_used;
    b->entries[lo].is_dir = (uint32_t)is_dir;
    b->text_used += (uint32_t)n;
    b->count++;
}

/* Archives list as folders (I159a): zip, lha/lzh and lzx files by extension. */
static int has_archive_ext(const char* name) {
    static const char* const exts[] = {".zip", ".lha", ".lzh", ".lzx"};
    size_t n = strlen(name), i, j;

    for (i = 0; i < sizeof exts / sizeof exts[0]; i++) {
        int same = n > 4u;

        for (j = 0; same && j < 4u; j++) {
            char c = name[n - 4u + j];

            same = (char)(c >= 'A' && c <= 'Z' ? c + 32 : c) == exts[i][j];
        }
        if (same) {
            return 1;
        }
    }
    return 0;
}

static int on_entry(void* ctx, const char* name, int is_dir) {
    const list_ctx* lc = ctx;
    if (name[0] != '.') {
        add_entry(lc->b, lc->first, name, is_dir ? 1 : (has_archive_ext(name) ? 2 : 0));
    }
    return 1;
}

#define BROWSE_ARCHIVE_MAX (64u * 1024u * 1024u)

static uint8_t* load_file(const sel_host* host, const char* path, size_t* size) {
    FILE* f = fopen(path, "rb");
    long n;
    uint8_t* buf = NULL;

    if (f == NULL) {
        return NULL;
    }
    if (fseek(f, 0, SEEK_END) == 0 && (n = ftell(f)) > 0 && (unsigned long)n <= BROWSE_ARCHIVE_MAX &&
        fseek(f, 0, SEEK_SET) == 0 && (buf = host->mem_alloc(host, (size_t)n, SEL_MEM_FAST)) != NULL) {
        if (fread(buf, 1, (size_t)n, f) == (size_t)n) {
            *size = (size_t)n;
        } else {
            host->mem_free(host, buf);
            buf = NULL;
        }
    }
    fclose(f);
    return buf;
}

typedef struct arch_ctx {
    sel_browse* b;
    const char* inner;
    size_t inner_len;
} arch_ctx;

/* One archive member as an entry of the folder `inner`: a deeper name shows its first component. */
static int on_arch_member(void* vctx, const char* name, const uint8_t* data, size_t size) {
    arch_ctx* a = vctx;
    const char* rest = name;
    const char* slash;
    char part[256];
    size_t n;

    (void)data;
    (void)size;
    if (a->inner_len) {
        if (strncmp(name, a->inner, a->inner_len) != 0 || name[a->inner_len] != '/') {
            return 1;
        }
        rest = name + a->inner_len + 1u;
    }
    if (*rest == '\0') {
        return 1;
    }
    slash = strchr(rest, '/');
    if (slash == NULL) {
        add_entry(a->b, 1, rest, 0);
        return 1;
    }
    n = (size_t)(slash - rest);
    if (n > 0u && n < sizeof part) {
        memcpy(part, rest, n);
        part[n] = '\0';
        if (sel_browse_find(a->b, part) == a->b->count) {
            add_entry(a->b, 1, part, 1);
        }
    }
    return 1;
}

/* `path` is "<archive file>[/<folder inside>]": finds the longest prefix that is an archive file and
 * lists its folder. Returns 0 when no prefix is an archive. */
static int fill_archive(sel_browse* b, const char* path) {
    char arch[SEL_BROWSE_PATH_MAX];
    char inner[SEL_BROWSE_PATH_MAX];
    size_t n = strlen(path);

    memcpy(arch, path, n + 1u);
    for (;;) {
        size_t size = 0;
        uint8_t* data = load_file(b->host, arch, &size);
        char* cut;

        if (data != NULL) {
            int kind = sel_archive_kind(data, size);

            if (sel_archive_is_folder(kind)) {
                arch_ctx a;
                size_t il;
                int got;

                memcpy(inner, path + strlen(arch), n - strlen(arch) + 1u);
                il = strlen(inner);
                while (il > 0u && inner[il - 1u] == '/') {
                    inner[--il] = '\0';
                }
                a.b = b;
                a.inner = inner[0] == '/' ? inner + 1 : inner;
                a.inner_len = strlen(a.inner);
                b->count = 0;
                b->text_used = 0;
                b->truncated = 0;
                add_entry(b, 0, "..", 1);
                got = sel_archive_list(b->host, kind, data, size, on_arch_member, &a);
                b->host->mem_free(b->host, data);
                return got >= 0;
            }
            b->host->mem_free(b->host, data);
        }
        cut = strrchr(arch, '/');
        if (cut == NULL || cut == arch) {
            return 0;
        }
        *cut = '\0';
    }
}

static int fill(sel_browse* b, const char* path) {
    list_ctx lc;
    b->count = 0;
    b->text_used = 0;
    b->truncated = 0;
    lc.b = b;
    lc.first = 0;
    if (!sel_browse_is_root(path)) {
        add_entry(b, 0, "..", 1);
        lc.first = 1;
    }
    return sel_dir_each(path, on_entry, &lc);
}

int sel_browse_list(sel_browse* b, const char* path) {
    if (path[0] == '\0') {
        path = "/";
    }
    if (strlen(path) >= SEL_BROWSE_PATH_MAX) {
        return 0;
    }
    if (!fill(b, path) && !fill_archive(b, path)) {
        if (!fill(b, b->path)) { /* the buffer was cleared: restore the old listing */
            (void)fill_archive(b, b->path);
        }
        return 0;
    }
    memcpy(b->path, path, strlen(path) + 1u);
    b->generation++;
    return 1;
}

const char* sel_browse_basename_style(const char* path, int amiga_style) {
    const char* base = path;
    const char* p;
    for (p = path; *p; p++) {
        if (*p == '/' || (amiga_style && *p == ':')) {
            base = p + 1;
        }
    }
    return base;
}

const char* sel_browse_basename(const char* path) {
    return sel_browse_basename_style(path, SEL_PATH_STYLE_AMIGA);
}

int sel_browse_is_root_style(const char* path, int amiga_style) {
    size_t n = strlen(path);
    if (strcmp(path, "/") == 0) {
        return 1;
    }
    /* AmigaDOS volume root: "Work:" (a non-empty volume name ending in ':') */
    return amiga_style && n > 1u && path[n - 1u] == ':' && strchr(path, '/') == NULL &&
           strchr(path, ':') == path + n - 1u;
}

int sel_browse_is_root(const char* path) {
    return sel_browse_is_root_style(path, SEL_PATH_STYLE_AMIGA);
}

int sel_browse_join_style(char* out, size_t cap, const char* dir, const char* name, int amiga_style) {
    size_t n = strlen(dir);
    if (amiga_style) {
        while (n > 0u && dir[n - 1u] == '/') {
            n--; /* "Work:Music/" is "Work:Music"; "Work:" keeps its colon */
        }
        if (strcmp(name, "..") == 0) {
            size_t keep = n;
            while (keep > 0u && dir[keep - 1u] != '/' && dir[keep - 1u] != ':') {
                keep--;
            }
            if (keep == 0u) {
                keep = n; /* relative single component: stays */
            } else if (dir[keep - 1u] == '/') {
                keep--; /* drop the separator: "Work:Music/Mods" -> "Work:Music" */
            } /* else a ':' stays: "Work:Music" -> "Work:" */
            if (keep + 1u > cap) {
                return 0;
            }
            memcpy(out, dir, keep);
            out[keep] = '\0';
            return 1;
        }
        {
            size_t m = strlen(name);
            size_t sep = n == 0u || dir[n - 1u] == ':' ? 0u : 1u;
            if (n + sep + m + 1u > cap) {
                return 0;
            }
            memcpy(out, dir, n);
            if (sep) {
                out[n] = '/';
            }
            memcpy(out + n + sep, name, m + 1u);
        }
        return 1;
    }
    while (n > 1u && dir[n - 1u] == '/') {
        n--;
    }
    if (strcmp(name, "..") == 0) {
        while (n > 1u && dir[n - 1u] != '/') {
            n--;
        }
        while (n > 1u && dir[n - 1u] == '/') {
            n--;
        }
        if (n == 1u && dir[0] != '/') {
            dir = "."; /* the parent of a relative single component */
        }
        if (n + 1u > cap) {
            return 0;
        }
        memcpy(out, dir, n);
        out[n] = '\0';
        return 1;
    }
    {
        size_t m = strlen(name);
        int root = n == 1u && dir[0] == '/';
        size_t sep = root ? 0u : 1u;
        if (n + sep + m + 1u > cap) {
            return 0;
        }
        memcpy(out, dir, n);
        if (sep) {
            out[n] = '/';
        }
        memcpy(out + n + sep, name, m + 1u);
    }
    return 1;
}

int sel_browse_join(char* out, size_t cap, const char* dir, const char* name) {
    return sel_browse_join_style(out, cap, dir, name, SEL_PATH_STYLE_AMIGA);
}

int sel_path_is_absolute_style(const char* path, int amiga_style) {
    const char* p;
    if (path[0] == '/') {
        return 1;
    }
    if (!amiga_style) {
        return 0;
    }
    /* AmigaDOS: "Volume:dir" and "PROGDIR:" are absolute; the volume name is
     * non-empty and ends at the first ':' before any '/'. */
    for (p = path; *p && *p != '/'; p++) {
        if (*p == ':') {
            return p != path;
        }
    }
    return 0;
}

int sel_path_is_absolute(const char* path) {
#if defined(__amigaos__) || defined(AMIGA)
    return sel_path_is_absolute_style(path, 1);
#else
    return sel_path_is_absolute_style(path, 0);
#endif
}

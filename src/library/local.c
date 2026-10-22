#include "local.h"

#include "dir.h"

#include <string.h>

int sel_browse_init(sel_browse* b, const sel_host* host, uint32_t capacity, uint32_t text_bytes) {
    memset(b, 0, sizeof *b);
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
    if (is_dir != o->is_dir) {
        return is_dir > o->is_dir;
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
        if (entry_less(b, is_dir ? 1u : 0u, name, &b->entries[mid])) {
            hi = mid;
        } else {
            lo = mid + 1u;
        }
    }
    memmove(&b->entries[lo + 1u], &b->entries[lo], (size_t)(b->count - lo) * sizeof *b->entries);
    memcpy(b->text + b->text_used, name, n);
    b->entries[lo].name_offset = b->text_used;
    b->entries[lo].is_dir = is_dir ? 1u : 0u;
    b->text_used += (uint32_t)n;
    b->count++;
}

static int on_entry(void* ctx, const char* name, int is_dir) {
    const list_ctx* lc = ctx;
    if (name[0] != '.') {
        add_entry(lc->b, lc->first, name, is_dir);
    }
    return 1;
}

static int fill(sel_browse* b, const char* path) {
    list_ctx lc;
    b->count = 0;
    b->text_used = 0;
    b->truncated = 0;
    lc.b = b;
    lc.first = 0;
    if (strcmp(path, "/") != 0) {
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
    if (!fill(b, path)) {
        (void)fill(b, b->path); /* the buffer was cleared: restore the old listing */
        return 0;
    }
    memcpy(b->path, path, strlen(path) + 1u);
    b->generation++;
    return 1;
}

const char* sel_browse_basename(const char* path) {
    const char* base = path;
    const char* p;
    for (p = path; *p; p++) {
        if (*p == '/') {
            base = p + 1;
        }
    }
    return base;
}

int sel_browse_join(char* out, size_t cap, const char* dir, const char* name) {
    size_t n = strlen(dir);
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

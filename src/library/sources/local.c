/* The `local` source (plan Phase 4, I158; design doc 9.1): the filesystem, scanned into the
 * catalogue. The directory walk reuses sel_browse (src/library/local.c) and the probe order of the
 * engine (first replayer answering YES, else the first UNSURE). */
#include "local_source.h"

#include "../source.h"
#include "archive.h"
#include "dir.h"
#include "local.h"

#include <selecta/plugin.h>
#include <selecta/replayer.h>
#include <stdio.h>
#include <string.h>

#define SCAN_ENTRIES 512u
#define SCAN_TEXT_BYTES 32768u
#define SCAN_STACK 64u        /* directories waiting to be listed */
#define SCAN_BATCH 32u        /* rows per SEL_LIB_REQ_SCAN_BATCH */
#define SCAN_MAX_FILE (8u * 1024u * 1024u)
#define TEXT_NAME 128u
#define TEXT_FORMAT 48u

typedef struct scan_row_text {
    char path[SEL_BROWSE_PATH_MAX];
    char name[TEXT_NAME];
    char artist[TEXT_NAME];
    char album[TEXT_NAME];
    char format[TEXT_FORMAT];
} scan_row_text;

typedef struct scan_state {
    const sel_local_scan_config* cfg;
    sel_local_scan_stats* stats;
    sel_browse browse;
    char (*stack)[SEL_BROWSE_PATH_MAX];
    uint32_t stack_n;
    scan_row_text* text;
    sel_scan_row* rows;
    uint32_t row_n;
    int failed;
    int flat; /* rescan: list the directory without entering its subdirectories */
} scan_state;

static void copy_text(char* dst, size_t n, const char* src) {
    size_t l = src != NULL ? strlen(src) : 0;

    if (l >= n) {
        l = n - 1u;
    }
    if (l > 0) {
        memcpy(dst, src, l);
    }
    dst[l] = '\0';
}

static int flush_rows(scan_state* s) {
    sel_scan_batch b;
    sel_message reply;

    if (s->row_n == 0) {
        return 1;
    }
    b.rows = s->rows;
    b.count = s->row_n;
    b.now = s->cfg->now;
    memset(&reply, 0, sizeof reply);
    if (!sel_library_request(s->cfg->lib, SEL_LIB_REQ_SCAN_BATCH, &b, &reply) || reply.args[1].v.i != 0) {
        s->failed = 1;
        return 0;
    }
    s->stats->scan_batches++;
    s->stats->scanned_files += (uint32_t)reply.args[0].v.i;
    s->row_n = 0;
    return 1;
}

/* Reads a whole file into host memory; NULL when unreadable or over the cap. */
static uint8_t* read_file(const sel_host* host, const char* path, size_t* size) {
    FILE* f = fopen(path, "rb");
    long len;
    uint8_t* buf;

    if (f == NULL) {
        return NULL;
    }
    if (fseek(f, 0, SEEK_END) != 0 || (len = ftell(f)) <= 0 || (unsigned long)len > SCAN_MAX_FILE ||
        fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return NULL;
    }
    buf = host->mem_alloc(host, (size_t)len, SEL_MEM_FAST);
    if (buf == NULL || fread(buf, 1, (size_t)len, f) != (size_t)len) {
        if (buf != NULL) {
            host->mem_free(host, buf);
        }
        fclose(f);
        return NULL;
    }
    fclose(f);
    *size = (size_t)len;
    return buf;
}

/* Same choice as the engine: first YES, else first UNSURE. */
static const sel_tag* find_replayer(scan_state* s, const uint8_t* data, size_t size, const char* path) {
    const sel_tag* best = NULL;
    uint32_t i, n = s->cfg->registry_count();

    for (i = 0; i < n; i++) {
        const sel_tag* desc = s->cfg->registry_get(i);
        const sel_tag* hooks;
        sel_rp_probe_fn probe;
        int r;

        if (sel_tag_find(desc, SEL_PLUGIN_KIND, 0) != SEL_PLUGIN_KIND_REPLAYER) {
            continue;
        }
        hooks = (const sel_tag*)sel_tag_find(desc, SEL_PLUGIN_TAGS, 0);
        probe = hooks != NULL ? (sel_rp_probe_fn)sel_tag_find(hooks, SEL_RP_PROBE, 0) : NULL;
        if (probe == NULL) {
            continue;
        }
        s->stats->probed++;
        r = probe(data, size, path);
        if (r == SEL_PROBE_YES) {
            return hooks;
        }
        if (r == SEL_PROBE_UNSURE && best == NULL) {
            best = hooks;
        }
    }
    return best;
}

/* Probes, opens and describes one module held in memory and queues its catalogue row. `probe_path`
 * is what the replayers see (the extension matters), `row_path` is the path stored in the catalogue
 * (for an archive member: "<archive>/<member>"). Does not free `data`. Returns 1 when a row was
 * queued. */
static int scan_blob(scan_state* s, const uint8_t* data, size_t size, const char* probe_path,
                     const char* row_path) {
    const sel_tag* hooks;
    scan_row_text* t;
    sel_scan_row* r;
    sel_rp_open_fn open_fn;
    sel_rp_close_fn close_fn;
    sel_rp_metadata_fn meta_fn;
    sel_rp_info info;
    sel_song_source src;
    void* state;

    hooks = find_replayer(s, data, size, probe_path);
    if (hooks == NULL) {
        s->stats->skipped_files++;
        return 0;
    }
    if (s->row_n == SCAN_BATCH && !flush_rows(s)) {
        return 0;
    }
    t = &s->text[s->row_n];
    r = &s->rows[s->row_n];
    memset(r, 0, sizeof *r);
    copy_text(t->path, sizeof t->path, row_path);
    copy_text(t->name, sizeof t->name, sel_browse_basename(probe_path));
    t->artist[0] = '\0';
    t->album[0] = '\0';
    t->format[0] = '\0';
    open_fn = (sel_rp_open_fn)sel_tag_find(hooks, SEL_RP_OPEN, 0);
    close_fn = (sel_rp_close_fn)sel_tag_find(hooks, SEL_RP_CLOSE, 0);
    meta_fn = (sel_rp_metadata_fn)sel_tag_find(hooks, SEL_RP_METADATA, 0);
    src.data = data;
    src.size = size;
    src.path = probe_path;
    memset(&info, 0, sizeof info);
    state = open_fn != NULL ? open_fn(&src, s->cfg->host, &info) : NULL;
    if (state != NULL) {
        const sel_tag* meta = meta_fn != NULL ? meta_fn(state) : NULL;
        const char* title = meta != NULL ? (const char*)sel_tag_find(meta, SEL_META_TITLE, 0) : NULL;

        if (title != NULL && title[0] != '\0') {
            copy_text(t->name, sizeof t->name, title);
        }
        if (meta != NULL) {
            copy_text(t->artist, sizeof t->artist, (const char*)sel_tag_find(meta, SEL_META_AUTHOR, 0));
            copy_text(t->album, sizeof t->album, (const char*)sel_tag_find(meta, SEL_META_RELEASE, 0));
            copy_text(t->format, sizeof t->format, (const char*)sel_tag_find(meta, SEL_META_FORMAT, 0));
            r->duration_ms = (int32_t)sel_tag_find(meta, SEL_META_DURATION_MS, 0);
        }
        if (close_fn != NULL) {
            close_fn(state);
        }
    }
    r->source = "local";
    r->path = t->path;
    r->name = t->name;
    r->artist = t->artist;
    r->album = t->album;
    r->format = t->format;
    r->md5 = "";
    r->size = (int64_t)size;
    s->row_n++;
    return 1;
}

typedef struct member_ctx {
    scan_state* s;
    const char* archive;
} member_ctx;

static int on_member(void* vctx, const char* name, const uint8_t* data, size_t size) {
    member_ctx* c = vctx;
    char row[SEL_BROWSE_PATH_MAX];

    if (snprintf(row, sizeof row, "%s/%s", c->archive, name) >= (int)sizeof row) {
        c->s->stats->skipped_files++;
        return 1;
    }
    if (scan_blob(c->s, data, size, name, row)) {
        c->s->stats->archive_members++;
    }
    return !c->s->failed;
}

static void scan_file(scan_state* s, const char* path) {
    size_t size = 0;
    uint8_t* data = read_file(s->cfg->host, path, &size);
    int kind;

    if (data == NULL) {
        s->stats->skipped_files++;
        return;
    }
    kind = sel_archive_kind(data, size);
    if (kind == SEL_ARCHIVE_PP20) {
        size_t raw = 0;
        uint8_t* plain = sel_pp20_depack(s->cfg->host, data, size, SCAN_MAX_FILE, &raw);

        if (plain == NULL) {
            s->stats->skipped_files++; /* damaged packed file */
        } else {
            s->stats->depack_pp++;
            scan_blob(s, plain, raw, path, path);
            s->cfg->host->mem_free(s->cfg->host, plain);
        }
    } else if (kind != SEL_ARCHIVE_NONE) {
        member_ctx mc;

        mc.s = s;
        mc.archive = path;
        s->stats->archives++;
        if (kind == SEL_ARCHIVE_LZX) {
            s->stats->archive_lzx++;
        }
        if (sel_archive_each(s->cfg->host, kind, data, size, SCAN_MAX_FILE, on_member, &mc) < 0) {
            s->stats->skipped_files++; /* damaged archive */
        }
    } else {
        scan_blob(s, data, size, path, path);
    }
    s->cfg->host->mem_free(s->cfg->host, data);
}

static void push_dir(scan_state* s, const char* path) {
    if (s->stack_n >= SCAN_STACK) {
        s->stats->skipped_files++; /* directory dropped: the stack is full */
        return;
    }
    copy_text(s->stack[s->stack_n++], SEL_BROWSE_PATH_MAX, path);
}

static void scan_dir(scan_state* s, const char* dir) {
    uint32_t i;
    char full[SEL_BROWSE_PATH_MAX];

    if (!sel_browse_list(&s->browse, dir)) {
        s->stats->skipped_files++;
        return;
    }
    s->stats->dirs++;
    for (i = 0; i < s->browse.count && !s->failed; i++) {
        const char* name = sel_browse_name(&s->browse, i);

        if (strcmp(name, "..") == 0 || !sel_browse_join(full, sizeof full, dir, name)) {
            continue;
        }
        if (s->browse.entries[i].is_dir == 1u) {
            if (!s->flat) {
                push_dir(s, full);
            }
        } else {
            scan_file(s, full);
        }
    }
}

/* Shared by sel_local_scan and sel_source_rescan: scratch memory, then `run` over the state. */
static int scan_with(const sel_local_scan_config* cfg, sel_local_scan_stats* stats, int flat,
                     void (*run)(scan_state*, const void*), const void* arg) {
    scan_state s;
    const sel_host* host = cfg->host;
    int ok = 0;

    memset(&s, 0, sizeof s);
    memset(stats, 0, sizeof *stats);
    s.cfg = cfg;
    s.stats = stats;
    s.flat = flat;
    s.stack = host->mem_alloc(host, (size_t)SCAN_STACK * SEL_BROWSE_PATH_MAX, SEL_MEM_FAST);
    s.text = host->mem_alloc(host, (size_t)SCAN_BATCH * sizeof *s.text, SEL_MEM_FAST);
    s.rows = host->mem_alloc(host, (size_t)SCAN_BATCH * sizeof *s.rows, SEL_MEM_FAST);
    if (s.stack != NULL && s.text != NULL && s.rows != NULL &&
        sel_browse_init(&s.browse, host, SCAN_ENTRIES, SCAN_TEXT_BYTES)) {
        run(&s, arg);
        ok = !s.failed && flush_rows(&s);
        sel_browse_free(&s.browse, host);
    }
    if (s.stack != NULL) {
        host->mem_free(host, s.stack);
    }
    if (s.text != NULL) {
        host->mem_free(host, s.text);
    }
    if (s.rows != NULL) {
        host->mem_free(host, s.rows);
    }
    return ok;
}

static void run_tree(scan_state* s, const void* root) {
    char first[SEL_BROWSE_PATH_MAX];

    if (!sel_browse_list(&s->browse, (const char*)root)) {
        s->failed = 1;
        return;
    }
    copy_text(first, sizeof first, (const char*)root);
    push_dir(s, first);
    while (s->stack_n > 0 && !s->failed) {
        copy_text(first, sizeof first, s->stack[--s->stack_n]);
        scan_dir(s, first);
    }
}

int sel_local_scan(const sel_local_scan_config* cfg, const char* root, sel_local_scan_stats* stats) {
    return scan_with(cfg, stats, 0, run_tree, root);
}

typedef struct rescan_list {
    const char* const* paths;
    uint32_t count;
} rescan_list;

static void run_flat(scan_state* s, const void* arg) {
    const rescan_list* l = arg;
    uint32_t i;

    for (i = 0; i < l->count && !s->failed; i++) {
        scan_dir(s, l->paths[i]);
        s->stats->rescanned_dirs++;
    }
}

int sel_source_rescan(const sel_local_scan_config* cfg, const char* const* paths, uint32_t count,
                      sel_local_scan_stats* stats) {
    rescan_list l;

    l.paths = paths;
    l.count = count;
    return scan_with(cfg, stats, 1, run_flat, &l);
}

/* mtime tracker: the table doubles as the breadth-first queue of the directory walk. */
struct sel_local_tracker {
    char (*path)[SEL_BROWSE_PATH_MAX];
    int64_t mtime[SEL_TRACK_DIRS];
    uint32_t count;
};

static int walk_dirs(const sel_host* host, const char* root, char (*path)[SEL_BROWSE_PATH_MAX], int64_t* mtime,
                     uint32_t* count) {
    sel_browse b;
    uint32_t n = 0, i, j;
    char full[SEL_BROWSE_PATH_MAX];

    if (!sel_browse_init(&b, host, SCAN_ENTRIES, SCAN_TEXT_BYTES)) {
        return 0;
    }
    copy_text(path[n++], SEL_BROWSE_PATH_MAX, root);
    for (i = 0; i < n; i++) {
        mtime[i] = sel_dir_mtime(path[i]);
        if (!sel_browse_list(&b, path[i])) {
            continue;
        }
        for (j = 0; j < b.count && n < SEL_TRACK_DIRS; j++) {
            const char* name = sel_browse_name(&b, j);

            if (strcmp(name, "..") != 0 && b.entries[j].is_dir == 1u && sel_browse_join(full, sizeof full, path[i], name)) {
                copy_text(path[n++], SEL_BROWSE_PATH_MAX, full);
            }
        }
    }
    sel_browse_free(&b, host);
    *count = n;
    return 1;
}

sel_local_tracker* sel_local_track(const sel_local_scan_config* cfg, const char* root) {
    const sel_host* host = cfg->host;
    sel_local_tracker* t = host->mem_alloc(host, sizeof *t, SEL_MEM_FAST);

    if (t == NULL) {
        return NULL;
    }
    memset(t, 0, sizeof *t);
    t->path = host->mem_alloc(host, (size_t)SEL_TRACK_DIRS * SEL_BROWSE_PATH_MAX, SEL_MEM_FAST);
    if (t->path == NULL || !walk_dirs(host, root, t->path, t->mtime, &t->count)) {
        sel_local_untrack(cfg, t);
        return NULL;
    }
    return t;
}

uint32_t sel_local_poll_changed(const sel_local_scan_config* cfg, sel_local_tracker* t,
                                sel_local_scan_stats* stats) {
    const sel_host* host = cfg->host;
    char (*np)[SEL_BROWSE_PATH_MAX] = host->mem_alloc(host, (size_t)SEL_TRACK_DIRS * SEL_BROWSE_PATH_MAX, SEL_MEM_FAST);
    const char* changed[SEL_TRACK_DIRS];
    int64_t nm[SEL_TRACK_DIRS];
    uint32_t nn = 0, i, j, nc = 0;

    memset(stats, 0, sizeof *stats);
    if (np == NULL || !walk_dirs(host, t->path[0], np, nm, &nn)) {
        if (np != NULL) {
            host->mem_free(host, np);
        }
        return 0;
    }
    for (i = 0; i < nn; i++) {
        int known = 0;

        for (j = 0; j < t->count; j++) {
            if (strcmp(t->path[j], np[i]) == 0) {
                known = 1;
                break;
            }
        }
        if (!known || t->mtime[j] != nm[i]) {
            changed[nc++] = np[i];
        }
    }
    if (nc > 0) {
        sel_source_rescan(cfg, changed, nc, stats);
    }
    memcpy(t->path, np, (size_t)nn * SEL_BROWSE_PATH_MAX);
    memcpy(t->mtime, nm, (size_t)nn * sizeof nm[0]);
    t->count = nn;
    host->mem_free(host, np);
    return nc;
}

void sel_local_untrack(const sel_local_scan_config* cfg, sel_local_tracker* t) {
    if (t != NULL) {
        if (t->path != NULL) {
            cfg->host->mem_free(cfg->host, t->path);
        }
        cfg->host->mem_free(cfg->host, t);
    }
}

/* Descriptor: a local path is its own URL; the key is local:<absolute path>. */
static const sel_tag g_tags[] = {{SEL_TAG_END, 0}};

static int local_url(const char* path, char* out, size_t n) {
    return path != NULL && strlen(path) < n && (strcpy(out, path), 1);
}

static const sel_source g_local = {
    "local", "Local files", "", "Folders on this machine.", g_tags,
    NULL, NULL, NULL, local_url, local_url, NULL, NULL,
};

const sel_source* sel_source_local(void) {
    return &g_local;
}

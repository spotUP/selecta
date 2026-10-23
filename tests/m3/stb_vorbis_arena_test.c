/* I195h: the stb_vorbis adapter allocates only through host->mem_alloc, and only in rp_open. The adapter and the
 * stb_vorbis implementation are compiled into this test with stb_vorbis_count.h force-included, so every malloc and
 * alloca of the decoder is counted. Driven through the plugin entry point and its RP_* hooks, the way the registry
 * does: open (live stream and whole file), start, render across a chained Ogg stream (the BOS page re-opens the
 * decoder inside the render path), close.
 *   stb_vorbis_arena_test <chained.ogg>          arena of the build: no stb malloc or alloca at all, host
 *                                                allocations fixed after open, the title follows the chain
 *   stb_vorbis_arena_test <chained.ogg> tiny     arena of 4 KB (-DVORBIS_ARENA_BYTES): rp_open fails cleanly, every
 *                                                host allocation is returned */
#include <selecta/host.h>
#include <selecta/plugin.h>
#include <selecta/replayer.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

unsigned long sel_test_mallocs, sel_test_allocas;

#undef malloc
void* sel_test_malloc(size_t n) {
    sel_test_mallocs++;
    return malloc(n);
}
void* sel_test_alloca(size_t n) {
    sel_test_allocas++;
    return malloc(n); /* leaked on purpose: the count is the point */
}

static unsigned long host_allocs, host_frees;
static void* h_alloc(const sel_host* h, size_t n, uint32_t kind) {
    (void)h;
    (void)kind;
    host_allocs++;
    return malloc(n);
}
static void h_free(const sel_host* h, void* p) {
    (void)h;
    if (p != NULL) {
        host_frees++;
    }
    free(p);
}

typedef struct feed {
    const uint8_t* d;
    size_t n, pos;
} feed;
static size_t f_read(void* ctx, uint8_t* dst, size_t max) {
    feed* f = (feed*)ctx;
    size_t k = f->n - f->pos < max ? f->n - f->pos : max;
    memcpy(dst, f->d + f->pos, k);
    f->pos += k;
    return k;
}
static int f_ended(void* ctx) {
    feed* f = (feed*)ctx;
    return f->pos >= f->n;
}

static uintptr_t tag_value(const sel_tag* t, uint32_t tag) {
    for (; t->tag != SEL_TAG_END; t++) {
        if (t->tag == tag) {
            return t->value;
        }
    }
    return 0;
}

static int fails;
#define CHECK(c, ...)                                                                                              \
    do {                                                                                                           \
        if (!(c)) {                                                                                                \
            printf("FAIL: " __VA_ARGS__);                                                                          \
            printf("\n");                                                                                          \
            fails++;                                                                                               \
        }                                                                                                          \
    } while (0)

int main(int argc, char** argv) {
    static sel_host host;
    static uint8_t file[1 << 20];
    static int16_t pcm[2048 * 2];
    size_t len;
    FILE* fp;
    int tiny = argc > 2 && strcmp(argv[2], "tiny") == 0;
    int mode;
    const sel_tag* desc;
    const sel_tag* hooks;
    sel_rp_open_fn open_fn;
    sel_rp_close_fn close_fn;
    sel_rp_start_fn start_fn;
    sel_rp_metadata_fn meta_fn;
    uint32_t (*render_fn)(void*, int16_t*, uint32_t);
    if (argc < 2 || (fp = fopen(argv[1], "rb")) == NULL) {
        printf("usage: stb_vorbis_arena_test <chained.ogg> [tiny]\n");
        return 2;
    }
    len = fread(file, 1, sizeof file, fp);
    fclose(fp);
    host.size = sizeof host;
    host.mem_alloc = h_alloc;
    host.mem_free = h_free;
    desc = sel_plugin_entry(1);
    hooks = (const sel_tag*)tag_value(desc, SEL_PLUGIN_TAGS);
    open_fn = (sel_rp_open_fn)tag_value(hooks, SEL_RP_OPEN);
    close_fn = (sel_rp_close_fn)tag_value(hooks, SEL_RP_CLOSE);
    start_fn = (sel_rp_start_fn)tag_value(hooks, SEL_RP_START);
    meta_fn = (sel_rp_metadata_fn)tag_value(hooks, SEL_RP_METADATA);
    render_fn = (uint32_t(*)(void*, int16_t*, uint32_t))tag_value(hooks, SEL_RP_RENDER);
    for (mode = 0; mode < 2; mode++) { /* 0: live stream, 1: whole file in memory */
        feed fd;
        sel_byte_stream bs;
        sel_song_source src;
        sel_rp_info info;
        void* st;
        unsigned long allocs_open, frames = 0, loud = 0, loud_late = 0;
        const char* t0 = "";
        char first[128] = "", last[128] = "";
        fd.d = file;
        fd.n = len;
        fd.pos = 0;
        bs.ctx = &fd;
        bs.read = f_read;
        bs.read_wait = f_read;
        bs.ended = f_ended;
        src.data = file;
        src.size = mode == 0 ? (len < 4096 ? len : 4096) : len;
        src.path = NULL;
        src.stream = mode == 0 ? &bs : NULL;
        sel_test_mallocs = sel_test_allocas = host_allocs = host_frees = 0;
        st = open_fn(&src, &host, &info);
        if (tiny) {
            CHECK(st == NULL, "mode %d: an arena of 4 KB opened a song", mode);
            CHECK(host_allocs == host_frees, "mode %d: open failure leaked host memory (%lu allocs, %lu frees)", mode,
                  host_allocs, host_frees);
            CHECK(sel_test_mallocs == 0 && sel_test_allocas == 0, "mode %d: stb_vorbis malloc %lu alloca %lu", mode,
                  sel_test_mallocs, sel_test_allocas);
            continue;
        }
        CHECK(st != NULL, "mode %d: open failed", mode);
        if (st == NULL) {
            continue;
        }
        CHECK(start_fn(st, 0, 44100, NULL) == 1, "mode %d: start failed", mode);
        allocs_open = host_allocs;
        t0 = (const char*)tag_value(meta_fn(st), SEL_META_TITLE);
        snprintf(first, sizeof first, "%s", t0 != NULL ? t0 : "");
        while (frames < 44100u * 3u) {
            uint32_t got = render_fn(st, pcm, 2048);
            uint32_t i;
            if (got == 0) {
                break;
            }
            for (i = 0; i < got * 2u; i++) {
                if (pcm[i] > 1000 || pcm[i] < -1000) {
                    loud++;
                    if (frames > 66150u) {
                        loud_late++;
                    }
                }
            }
            frames += got;
        }
        t0 = (const char*)tag_value(meta_fn(st), SEL_META_TITLE);
        snprintf(last, sizeof last, "%s", t0 != NULL ? t0 : "");
        CHECK(sel_test_mallocs == 0, "mode %d: stb_vorbis called malloc %lu times", mode, sel_test_mallocs);
        CHECK(sel_test_allocas == 0, "mode %d: stb_vorbis called alloca %lu times", mode, sel_test_allocas);
        CHECK(host_allocs == allocs_open, "mode %d: host->mem_alloc ran %lu times after start", mode,
              host_allocs - allocs_open);
        CHECK(host_allocs == 2, "mode %d: %lu host allocations at open, want 2 (state, arena)", mode, host_allocs);
        CHECK(loud > 1000, "mode %d: the output is silent", mode);
        if (mode == 0) { /* the sentinel that the BOS page was reached and re-opened inside render */
            CHECK(strcmp(first, "Chain One") == 0, "first title %s", first);
            CHECK(strcmp(last, "Chain Two") == 0, "title after the chain %s", last);
            CHECK(loud_late > 1000, "no sound after the chain");
        }
        close_fn(st);
        CHECK(host_allocs == host_frees, "mode %d: %lu host allocs, %lu frees", mode, host_allocs, host_frees);
        printf("mode %d frames=%lu title_first=%s title_last=%s stb_malloc=%lu stb_alloca=%lu host_allocs=%lu\n", mode,
               frames, first, last, sel_test_mallocs, sel_test_allocas, host_allocs);
    }
    if (fails == 0) {
        printf("stb_vorbis_arena_ok%s\n", tiny ? "_tiny" : "");
    }
    return fails != 0;
}

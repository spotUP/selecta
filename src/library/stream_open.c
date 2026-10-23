/* URL and pipe song sources for OPEN. See stream_open.h. */
#include "stream_open.h"

#include "net/http.h"
#include "net/net.h"

#include <selecta/tags.h>

#include <stdio.h>
#include <string.h>

#if defined(__amigaos__) || defined(AMIGA)
#define SEL_STREAM_MAX (24u * 1024u * 1024u)
#else
#define SEL_STREAM_MAX (256u * 1024u * 1024u)
#endif

static uint32_t g_bytes, g_reads;

typedef struct sbuf {
    const sel_host* host;
    uint8_t* p;
    size_t len, cap;
} sbuf;

static int sbuf_add(sbuf* b, const void* data, size_t n) {
    if (b->len + n > SEL_STREAM_MAX) {
        return 0;
    }
    if (b->len + n + 1u > b->cap) {
        size_t cap = b->cap == 0 ? 65536u : b->cap;
        uint8_t* np;
        while (cap < b->len + n + 1u) {
            cap *= 2u;
        }
        np = b->host->mem_alloc(b->host, cap, SEL_MEM_FAST);
        if (np == NULL) {
            return 0;
        }
        if (b->len > 0) {
            memcpy(np, b->p, b->len);
        }
        if (b->p != NULL) {
            b->host->mem_free(b->host, b->p);
        }
        b->p = np;
        b->cap = cap;
    }
    memcpy(b->p + b->len, data, n);
    b->len += n;
    return 1;
}

static int sink(void* ctx, const void* data, size_t len) {
    return sbuf_add((sbuf*)ctx, data, len);
}

int sel_stream_is_url(const char* path) {
    return path != NULL && (strncmp(path, "http://", 7) == 0 || strncmp(path, "https://", 8) == 0);
}

#if defined(__amigaos__) || defined(AMIGA)
int sel_stream_is_pipe(const char* path) {
    (void)path;
    return 0;
}
static int read_pipe(sbuf* b, const char* cmd) {
    (void)b;
    (void)cmd;
    return 0;
}
#else
int sel_stream_is_pipe(const char* path) {
    return path != NULL && strncmp(path, "pipe:", 5) == 0 && path[5] != '\0';
}

static int read_pipe(sbuf* b, const char* cmd) {
    FILE* f = popen(cmd, "r");
    unsigned char chunk[8192];
    size_t n;
    int ok = 1;
    if (f == NULL) {
        return 0;
    }
    while ((n = fread(chunk, 1, sizeof chunk, f)) > 0) {
        if (!sbuf_add(b, chunk, n)) {
            ok = 0;
            break;
        }
    }
    return pclose(f) == 0 && ok;
}
#endif

uint8_t* sel_stream_read(const sel_host* host, const char* path, size_t* size) {
    sbuf b;
    int ok;
    memset(&b, 0, sizeof b);
    b.host = host;
    if (sel_stream_is_url(path)) {
        sel_http_req req;
        sel_http_res res;
        memset(&req, 0, sizeof req);
        memset(&res, 0, sizeof res);
        req.url = path;
        ok = sel_http_request(&req, sink, &b, &res) == SEL_HTTP_OK && res.status >= 200 && res.status < 300;
    } else if (sel_stream_is_pipe(path)) {
        ok = read_pipe(&b, path + 5);
    } else {
        return NULL;
    }
    if (!ok || b.len == 0) {
        if (b.p != NULL) {
            host->mem_free(host, b.p);
        }
        return NULL;
    }
    b.p[b.len] = 0;
    *size = b.len;
    g_bytes = (uint32_t)b.len;
    g_reads++;
    return b.p;
}

uint32_t sel_stream_bytes(void) {
    return g_bytes;
}

uint32_t sel_stream_reads(void) {
    return g_reads;
}

/* ---- Live streams (I195b, see stream_open.h) ---- */
static uint32_t g_live_opens, g_audio_bytes, g_audio_done, g_received, g_record_taps, g_recorded;

#if defined(__amigaos__) || defined(AMIGA)
/* No download thread on the Amiga until bsdsocket arrives with I199: OPEN keeps reading the whole file. */
sel_live* sel_live_open(const sel_host* host, const char* url, const volatile unsigned* gen, unsigned start) {
    (void)host;
    (void)url;
    (void)gen;
    (void)start;
    return NULL;
}
int sel_live_head(sel_live* s, uint8_t* dst, size_t want, size_t* n) {
    (void)s;
    (void)dst;
    (void)want;
    (void)n;
    return 0;
}
const sel_byte_stream* sel_live_source(sel_live* s) {
    (void)s;
    return NULL;
}
uint8_t* sel_live_drain(sel_live* s, size_t* size) {
    (void)s;
    (void)size;
    return NULL;
}
void sel_live_note_audio(sel_live* s) {
    (void)s;
}
void sel_live_close(sel_live* s) {
    (void)s;
}
sel_live* sel_live_open_record(const sel_host* host, const char* url, const volatile unsigned* gen, unsigned start,
                               const char* path, unsigned long limit) {
    (void)host;
    (void)url;
    (void)gen;
    (void)start;
    (void)path;
    (void)limit;
    return NULL;
}
unsigned long sel_live_recorded(const sel_live* s) {
    (void)s;
    return 0;
}
int sel_live_finished(const sel_live* s) {
    (void)s;
    return 1;
}
int sel_live_failed(const sel_live* s) {
    (void)s;
    return 1;
}
size_t sel_live_discard(sel_live* s, size_t max) {
    (void)s;
    (void)max;
    return 0;
}
#else
#include <pthread.h>
#include <unistd.h>

#define SEL_STREAM_RING (256u * 1024u) /* power of two: the bound on what an endless stream holds in memory */
#define SEL_STREAM_FOREVER 0x7FFFFFFFu /* total_ms of the request: a radio stream has no end to time out */

struct sel_live {
    const sel_host* host;
    char* url;
    uint8_t* ring;
    volatile unsigned wr, rd;       /* byte counters, free running; wr - rd bytes are buffered */
    volatile unsigned eof, failed;  /* set by the download thread when it ends */
    volatile unsigned stop;         /* the thread's cancel generation (sel_net_set_cancel) and the sink's abort */
    unsigned long received;
    FILE* rec;                      /* record sink (F36); written by the download thread only */
    unsigned long rec_limit, rec_written;
    volatile unsigned rec_count;    /* rec_written for other threads */
    const volatile unsigned* gen;   /* the engine's cancel generation, for the pump-side waits */
    unsigned gen_start;
    pthread_t thread;
    int started;
    sel_byte_stream view;
};

static unsigned ld(const volatile unsigned* p) {
    return __atomic_load_n(p, __ATOMIC_ACQUIRE);
}
static void st_store(volatile unsigned* p, unsigned v) {
    __atomic_store_n(p, v, __ATOMIC_RELEASE);
}

static int cancelled(const sel_live* s) {
    return ld(&s->stop) != 0u || (s->gen != NULL && *s->gen != s->gen_start);
}

static int ring_sink(void* ctx, const void* data, size_t len) {
    sel_live* s = (sel_live*)ctx;
    const uint8_t* p = (const uint8_t*)data;
    if (s->rec != NULL) {
        size_t n = len;
        if (s->rec_limit != 0 && n > s->rec_limit - s->rec_written) {
            n = (size_t)(s->rec_limit - s->rec_written);
        }
        if (n > 0 && fwrite(p, 1, n, s->rec) == n) {
            s->rec_written += n;
            g_record_taps++;
            st_store(&s->rec_count, (unsigned)s->rec_written);
        }
    }
    while (len > 0) {
        unsigned w = ld(&s->wr), space = SEL_STREAM_RING - (w - ld(&s->rd));
        size_t n, at;
        if (ld(&s->stop) != 0u) {
            return 0;
        }
        if (space == 0u) {
            usleep(2000); /* the reader polls too; a full ring is the back pressure on the socket */
            continue;
        }
        n = len < space ? len : space;
        at = w & (SEL_STREAM_RING - 1u);
        if (n > SEL_STREAM_RING - at) {
            n = SEL_STREAM_RING - at;
        }
        memcpy(s->ring + at, p, n);
        s->received += n;
        st_store(&s->wr, w + (unsigned)n);
        p += n;
        len -= n;
    }
    return 1;
}

static void* download(void* arg) {
    sel_live* s = (sel_live*)arg;
    sel_http_req req;
    sel_http_res res;
    int ok;
    sel_net_set_cancel(&s->stop, 0u);
    memset(&req, 0, sizeof req);
    memset(&res, 0, sizeof res);
    req.url = s->url;
    req.total_ms = SEL_STREAM_FOREVER;
    ok = sel_http_request(&req, ring_sink, s, &res) == SEL_HTTP_OK && res.status >= 200 && res.status < 300;
    sel_net_set_cancel(NULL, 0u);
    if (!ok) {
        st_store(&s->failed, 1u);
    }
    st_store(&s->eof, 1u);
    return NULL;
}

static size_t copy_out(sel_live* s, uint8_t* dst, size_t max, int consume) {
    unsigned r = ld(&s->rd), avail = ld(&s->wr) - r;
    size_t n = max < avail ? max : avail, at = r & (SEL_STREAM_RING - 1u), first;
    if (n == 0) {
        return 0;
    }
    first = n < SEL_STREAM_RING - at ? n : SEL_STREAM_RING - at;
    memcpy(dst, s->ring + at, first);
    memcpy(dst + first, s->ring, n - first);
    if (consume) {
        st_store(&s->rd, r + (unsigned)n);
    }
    return n;
}

static int is_ended(sel_live* s) {
    return ld(&s->stop) != 0u || (ld(&s->eof) != 0u && ld(&s->wr) == ld(&s->rd));
}

static size_t view_read(void* ctx, uint8_t* dst, size_t max) {
    return copy_out((sel_live*)ctx, dst, max, 1);
}

static size_t view_read_wait(void* ctx, uint8_t* dst, size_t max) {
    sel_live* s = (sel_live*)ctx;
    for (;;) {
        size_t n = copy_out(s, dst, max, 1);
        if (n > 0 || is_ended(s) || cancelled(s)) {
            return n;
        }
        usleep(1000);
    }
}

static int view_ended(void* ctx) {
    return is_ended((sel_live*)ctx);
}

static sel_live* live_open(const sel_host* host, const char* url, const volatile unsigned* gen, unsigned start,
                           const char* rec_path, unsigned long rec_limit) {
    sel_live* s = (sel_live*)host->mem_alloc(host, sizeof *s, SEL_MEM_FAST);
    size_t n = strlen(url) + 1u;
    if (s == NULL) {
        return NULL;
    }
    memset(s, 0, sizeof *s);
    s->host = host;
    s->gen = gen;
    s->gen_start = start;
    s->url = (char*)host->mem_alloc(host, n, SEL_MEM_FAST);
    s->ring = (uint8_t*)host->mem_alloc(host, SEL_STREAM_RING, SEL_MEM_FAST);
    if (s->url == NULL || s->ring == NULL) {
        sel_live_close(s);
        return NULL;
    }
    memcpy(s->url, url, n);
    if (rec_path != NULL) {
        s->rec = fopen(rec_path, "wb"); /* before the thread starts, so no early byte is missed */
        s->rec_limit = rec_limit;
        if (s->rec == NULL) {
            sel_live_close(s);
            return NULL;
        }
    }
    s->view.ctx = s;
    s->view.read = view_read;
    s->view.read_wait = view_read_wait;
    s->view.ended = view_ended;
    if (pthread_create(&s->thread, NULL, download, s) != 0) {
        sel_live_close(s);
        return NULL;
    }
    s->started = 1;
    g_live_opens++;
    return s;
}

sel_live* sel_live_open(const sel_host* host, const char* url, const volatile unsigned* gen, unsigned start) {
    return live_open(host, url, gen, start, NULL, 0);
}

sel_live* sel_live_open_record(const sel_host* host, const char* url, const volatile unsigned* gen, unsigned start,
                               const char* path, unsigned long limit) {
    return live_open(host, url, gen, start, path, limit);
}

int sel_live_head(sel_live* s, uint8_t* dst, size_t want, size_t* n) {
    while ((size_t)(ld(&s->wr) - ld(&s->rd)) < want && ld(&s->eof) == 0u) {
        if (cancelled(s)) {
            return 0;
        }
        usleep(1000);
    }
    *n = copy_out(s, dst, want, 0);
    return *n > 0;
}

const sel_byte_stream* sel_live_source(sel_live* s) {
    return &s->view;
}

uint8_t* sel_live_drain(sel_live* s, size_t* size) {
    sbuf b;
    uint8_t chunk[8192];
    size_t n;
    int ok = 1;
    memset(&b, 0, sizeof b);
    b.host = s->host;
    while ((n = view_read_wait(s, chunk, sizeof chunk)) > 0) {
        if (!sbuf_add(&b, chunk, n)) {
            ok = 0;
            break;
        }
    }
    if (!ok || ld(&s->failed) != 0u || cancelled(s) || b.len == 0) {
        if (b.p != NULL) {
            s->host->mem_free(s->host, b.p);
        }
        return NULL;
    }
    b.p[b.len] = 0;
    *size = b.len;
    g_bytes = (uint32_t)b.len;
    g_reads++;
    return b.p;
}

void sel_live_note_audio(sel_live* s) {
    g_audio_bytes = (uint32_t)s->received;
    g_audio_done = ld(&s->eof) != 0u ? 1u : 0u;
}

unsigned long sel_live_recorded(const sel_live* s) {
    return ld(&s->rec_count);
}
int sel_live_finished(const sel_live* s) {
    return ld(&s->eof) != 0u;
}
int sel_live_failed(const sel_live* s) {
    return ld(&s->failed) != 0u;
}
size_t sel_live_discard(sel_live* s, size_t max) {
    unsigned r = ld(&s->rd), avail = ld(&s->wr) - r;
    size_t n = max < avail ? max : avail;
    st_store(&s->rd, r + (unsigned)n);
    return n;
}

void sel_live_close(sel_live* s) {
    if (s == NULL) {
        return;
    }
    st_store(&s->stop, 1u);
    if (s->started) {
        pthread_join(s->thread, NULL);
    }
    if (s->rec != NULL) {
        g_recorded = (uint32_t)s->rec_written;
        fclose(s->rec);
        s->rec = NULL;
    }
    g_received = (uint32_t)s->received;
    if (s->ring != NULL) {
        s->host->mem_free(s->host, s->ring);
    }
    if (s->url != NULL) {
        s->host->mem_free(s->host, s->url);
    }
    s->host->mem_free(s->host, s);
}
#endif

uint32_t sel_stream_live_opens(void) {
    return g_live_opens;
}
uint32_t sel_stream_audio_bytes(void) {
    return g_audio_bytes;
}
uint32_t sel_stream_audio_done(void) {
    return g_audio_done;
}
uint32_t sel_stream_recorded_total(void) {
    return g_recorded;
}
uint32_t sel_stream_record_taps(void) {
    return g_record_taps;
}
uint32_t sel_stream_received(void) {
    return g_received;
}

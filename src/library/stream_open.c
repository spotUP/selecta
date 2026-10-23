/* URL and pipe song sources for OPEN. See stream_open.h. */
#include "stream_open.h"

#include "net/http.h"
#include "net/net.h"
#include "record_name.h"

#include <selecta/tags.h>

#include <stdio.h>
#include <string.h>

#if defined(__amigaos__) || defined(AMIGA)
#define SEL_STREAM_MAX (24u * 1024u * 1024u)
#else
#define SEL_STREAM_MAX (256u * 1024u * 1024u)
#endif

static uint32_t g_bytes, g_reads;
static int g_status; /* HTTP status of the last sel_stream_read (0: no response) */

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
        g_status = res.status;
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

int sel_stream_status(void) {
    return g_status;
}

uint32_t sel_stream_bytes(void) {
    return g_bytes;
}

uint32_t sel_stream_reads(void) {
    return g_reads;
}

/* ---- Live streams (I195b, see stream_open.h) ---- */
static uint32_t g_live_opens, g_audio_bytes, g_audio_done, g_received, g_record_taps, g_recorded, g_record_files;

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
sel_live* sel_live_open_record_dir(const sel_host* host, const char* url, const volatile unsigned* gen, unsigned start,
                                   const char* dir) {
    (void)host;
    (void)url;
    (void)gen;
    (void)start;
    (void)dir;
    return NULL;
}
int sel_live_title(const sel_live* s, char* dst, size_t cap) {
    (void)s;
    (void)dst;
    (void)cap;
    return 0;
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
int sel_live_status(const sel_live* s) {
    (void)s;
    return 0;
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
    volatile unsigned status;       /* HTTP status of the response, stored before eof (0: no response) */
    const sel_http_res* res;        /* the download thread's response record: status is set before any body byte */
    volatile unsigned stop;         /* the thread's cancel generation (sel_net_set_cancel) and the sink's abort */
    unsigned long received;
    FILE* rec;                      /* record sink (F36); written by the download thread only */
    unsigned long rec_limit, rec_written;
    char rec_dir[512];              /* I196c: directory mode, one file per OPEN and per song; empty in file mode */
    char rec_path[1100];            /* the file being written (directory mode) */
    char rec_ext[8], rec_fallback[64];
    int rec_id3;                    /* the file starts with a SEL_RECORD_ID3_SIZE tag block (ICY stream, mp3) */
    int rec_first_pending;          /* the first file still carries the fallback name: the first title renames it */
    sel_record_cut cut;             /* I196d: directory mode splits at the next MP3 frame or Ogg page start */
    char split_title[256];          /* names the file the pending split opens */
    /* ICY in-band metadata (I196a): every icy_metaint audio bytes one length byte (x16) and that many bytes follow. */
    unsigned icy_metaint, icy_left, icy_phase, icy_need, icy_have;
    int icy_started;
    char icy_meta[16u * 255u + 1u];
    char cur_title[256];            /* download thread's copy of the last StreamTitle */
    volatile unsigned title_seq;    /* odd while title is being written */
    char title[256];                /* last StreamTitle for the engine (sel_live_title); seqlock title_seq */
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

/* Starts a recording file in directory mode. `title` names it (NULL: the stream's fallback name). Closes the
 * previous one. On failure recording stops quietly; playback goes on. */
static void rec_open_segment(sel_live* s, const char* title) {
    char path[sizeof s->rec_path];
    FILE* f;
    if (!sel_record_name(path, sizeof path, s->rec_dir, title, s->rec_fallback, s->rec_ext)) {
        return;
    }
    f = fopen(path, "wb");
    if (f == NULL) {
        return;
    }
    if (s->rec != NULL) {
        fclose(s->rec);
    }
    s->rec = f;
    memcpy(s->rec_path, path, sizeof path);
    s->rec_id3 = s->icy_metaint != 0u && strcmp(s->rec_ext, "mp3") == 0;
    if (s->rec_id3) {
        uint8_t tag[SEL_RECORD_ID3_SIZE];
        sel_record_id3(tag, title);
        fwrite(tag, 1, sizeof tag, f);
    }
    g_record_files++;
}

/* A new StreamTitle arrived in the metadata block (download thread). */
static void on_title(sel_live* s, const char* title) {
    if (s->rec_dir[0] != '\0' && s->rec != NULL) {
        if (s->rec_first_pending) {
            /* The first title names the file that already holds the song's start: rename it, write the tag. */
            char path[sizeof s->rec_path];
            s->rec_first_pending = 0;
            if (sel_record_name(path, sizeof path, s->rec_dir, title, s->rec_fallback, s->rec_ext) &&
                rename(s->rec_path, path) == 0) {
                memcpy(s->rec_path, path, sizeof path);
            }
            if (s->rec_id3) {
                uint8_t tag[SEL_RECORD_ID3_SIZE];
                sel_record_id3(tag, title);
                if (fseek(s->rec, 0, SEEK_SET) == 0) {
                    fwrite(tag, 1, sizeof tag, s->rec);
                }
                fseek(s->rec, 0, SEEK_END);
            }
        } else {
            /* A new song: a new file, opened where the next frame starts so both files decode whole. */
            snprintf(s->split_title, sizeof s->split_title, "%s", title);
            sel_record_cut_request(&s->cut);
        }
    }
    st_store(&s->title_seq, s->title_seq + 1u);
    snprintf(s->title, sizeof s->title, "%s", title);
    st_store(&s->title_seq, s->title_seq + 1u);
}

static void icy_block_done(sel_live* s) {
    const char* at;
    const char* end;
    char title[sizeof s->cur_title];
    size_t n;
    s->icy_meta[s->icy_need] = '\0';
    at = strstr(s->icy_meta, "StreamTitle='");
    if (at == NULL) {
        return;
    }
    at += 13;
    end = strstr(at, "';");
    n = end != NULL ? (size_t)(end - at) : strlen(at);
    if (n >= sizeof title) {
        n = sizeof title - 1u;
    }
    memcpy(title, at, n);
    title[n] = '\0';
    if (title[0] != '\0' && strcmp(title, s->cur_title) != 0) {
        memcpy(s->cur_title, title, n + 1u);
        on_title(s, title);
    }
}

static void rec_write(sel_live* s, const uint8_t* p, size_t len) {
    size_t n = len;
    if (s->rec == NULL) {
        return;
    }
    if (s->rec_limit != 0 && n > s->rec_limit - s->rec_written) {
        n = (size_t)(s->rec_limit - s->rec_written);
    }
    if (n > 0 && fwrite(p, 1, n, s->rec) == n) {
        s->rec_written += n;
        g_record_taps++;
        st_store(&s->rec_count, (unsigned)s->rec_written);
    }
}

/* sel_record_cut_out: directory mode's bytes, `split` set on the first byte of the next song's file. */
static void rec_emit(void* ctx, const uint8_t* p, size_t n, int split) {
    sel_live* s = ctx;
    if (split) {
        rec_open_segment(s, s->split_title);
    }
    rec_write(s, p, n);
}

/* Audio bytes (no metadata) to the record file and the ring. 0: the download must end. */
static int feed_audio(sel_live* s, const uint8_t* p, size_t len) {
    if (s->rec_dir[0] != '\0') {
        if (s->rec != NULL) {
            sel_record_cut_feed(&s->cut, p, len, rec_emit, s);
        }
    } else {
        rec_write(s, p, len);
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

/* First body byte: learn icy-metaint, and in directory mode open the first file. */
static void sink_start(sel_live* s) {
    s->icy_started = 1;
    if (s->res != NULL) {
        s->icy_metaint = s->res->icy_metaint;
        s->icy_left = s->icy_metaint;
    }
    if (s->rec_dir[0] != '\0') {
        char buf[8];
        const char* base = strrchr(s->url, '/');
        size_t n;
        snprintf(s->rec_ext, sizeof s->rec_ext, "%s",
                 sel_record_ext(s->res != NULL ? s->res->content_type : NULL, s->url, buf, sizeof buf));
        base = base != NULL && base[1] != '\0' && base[1] != '?' ? base + 1 : "stream";
        n = strcspn(base, ".?#"); /* the URL's file name without extension names the file until a title arrives */
        if (n >= sizeof s->rec_fallback) {
            n = sizeof s->rec_fallback - 1u;
        }
        memcpy(s->rec_fallback, base, n);
        s->rec_fallback[n] = '\0';
        s->rec_first_pending = 1;
        sel_record_cut_init(&s->cut, s->rec_ext);
        rec_open_segment(s, NULL);
    }
}

static int ring_sink(void* ctx, const void* data, size_t len) {
    sel_live* s = (sel_live*)ctx;
    const uint8_t* p = (const uint8_t*)data;
    if (s->res != NULL && (s->res->status < 200 || s->res->status >= 300)) {
        return 0; /* I195e: an error page never reaches the ring or the record file; the request ends as sink_aborted */
    }
    if (!s->icy_started) {
        sink_start(s);
    }
    if (s->icy_metaint == 0u) {
        return feed_audio(s, p, len);
    }
    while (len > 0) {
        size_t n;
        if (s->icy_phase == 0u) {
            n = len < s->icy_left ? len : s->icy_left;
            if (n > 0 && !feed_audio(s, p, n)) {
                return 0;
            }
            p += n;
            len -= n;
            s->icy_left -= (unsigned)n;
            if (s->icy_left == 0u) {
                s->icy_phase = 1u;
            }
        } else if (s->icy_phase == 1u) {
            s->icy_need = (unsigned)p[0] * 16u;
            s->icy_have = 0;
            p++;
            len--;
            if (s->icy_need == 0u) {
                s->icy_phase = 0u;
                s->icy_left = s->icy_metaint;
            } else {
                s->icy_phase = 2u;
            }
        } else {
            n = s->icy_need - s->icy_have;
            if (n > len) {
                n = len;
            }
            memcpy(s->icy_meta + s->icy_have, p, n);
            s->icy_have += (unsigned)n;
            p += n;
            len -= n;
            if (s->icy_have == s->icy_need) {
                icy_block_done(s);
                s->icy_phase = 0u;
                s->icy_left = s->icy_metaint;
            }
        }
    }
    return 1;
}

static const char* const icy_headers[] = {"Icy-MetaData: 1", NULL}; /* radio servers answer with icy-metaint (I196a) */

static void* download(void* arg) {
    sel_live* s = (sel_live*)arg;
    sel_http_req req;
    sel_http_res res;
    int ok;
    sel_net_set_cancel(&s->stop, 0u);
    memset(&req, 0, sizeof req);
    memset(&res, 0, sizeof res);
    req.url = s->url;
    req.headers = icy_headers;
    req.total_ms = SEL_STREAM_FOREVER;
    s->res = &res;
    ok = sel_http_request(&req, ring_sink, s, &res) == SEL_HTTP_OK && res.status >= 200 && res.status < 300;
    sel_net_set_cancel(NULL, 0u);
    st_store(&s->status, (unsigned)res.status);
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
                           const char* rec_path, unsigned long rec_limit, const char* rec_dir) {
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
    if (rec_dir != NULL) {
        if (strlen(rec_dir) >= sizeof s->rec_dir) {
            sel_live_close(s);
            return NULL;
        }
        memcpy(s->rec_dir, rec_dir, strlen(rec_dir) + 1u); /* the file is created by the first body byte */
    }
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
    s->view.total = 0;
    s->view.seek = NULL;
    if (pthread_create(&s->thread, NULL, download, s) != 0) {
        sel_live_close(s);
        return NULL;
    }
    s->started = 1;
    g_live_opens++;
    return s;
}

sel_live* sel_live_open(const sel_host* host, const char* url, const volatile unsigned* gen, unsigned start) {
    return live_open(host, url, gen, start, NULL, 0, NULL);
}

sel_live* sel_live_open_record(const sel_host* host, const char* url, const volatile unsigned* gen, unsigned start,
                               const char* path, unsigned long limit) {
    return live_open(host, url, gen, start, path, limit, NULL);
}

sel_live* sel_live_open_record_dir(const sel_host* host, const char* url, const volatile unsigned* gen, unsigned start,
                                   const char* dir) {
    return live_open(host, url, gen, start, NULL, 0, dir);
}

int sel_live_title(const sel_live* s, char* dst, size_t cap) {
    unsigned a, b;
    int tries = 0;
    do {
        a = ld(&s->title_seq);
        snprintf(dst, cap, "%s", s->title);
        b = ld(&s->title_seq);
    } while ((a != b || (a & 1u) != 0u) && ++tries < 100);
    return dst[0] != '\0';
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
int sel_live_status(const sel_live* s) {
    return (int)ld(&s->status);
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
        if (s->rec_dir[0] != '\0') {
            sel_record_cut_flush(&s->cut, rec_emit, s); /* header bytes held back at the stream's end */
        }
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
uint32_t sel_stream_record_files(void) {
    return g_record_files;
}
uint32_t sel_stream_record_taps(void) {
    return g_record_taps;
}
uint32_t sel_stream_received(void) {
    return g_received;
}

/* File-backed stream (I184d): a local song of a SEL_RP_STREAMING replayer is read from disk in pieces instead of
 * being loaded whole. Same sel_byte_stream the live download uses, plus the file length and seek. */
struct sel_file_stream {
    const sel_host* host;
    FILE* f;
    uint64_t pos;
    uint64_t size;
    sel_byte_stream view;
};

#define SEL_FILE_HEAD_MAX (1024u * 1024u)

static uint32_t g_file_opens;
static uint32_t g_file_bytes;

static size_t file_read(void* ctx, uint8_t* dst, size_t max) {
    sel_file_stream* s = (sel_file_stream*)ctx;
    size_t n = s->pos >= s->size ? 0u : fread(dst, 1, max, s->f);
    s->pos += n;
    g_file_bytes += (uint32_t)n;
    return n;
}

static int file_ended(void* ctx) {
    sel_file_stream* s = (sel_file_stream*)ctx;
    return s->pos >= s->size;
}

static int file_seek(void* ctx, uint64_t pos) {
    sel_file_stream* s = (sel_file_stream*)ctx;
    if (pos > s->size || fseek(s->f, (long)pos, SEEK_SET) != 0) {
        return 0;
    }
    s->pos = pos;
    return 1;
}

/* Bytes the probe needs: SEL_STREAM_HEAD, or for a file that starts with an ID3v2 tag the whole tag plus room for
 * the first frames behind it (cover art makes tags of several hundred KB common), capped at SEL_FILE_HEAD_MAX. */
static size_t file_head_want(const uint8_t* h, size_t n, uint64_t size) {
    size_t want = SEL_STREAM_HEAD;

    if (n >= 10u && h[0] == 'I' && h[1] == 'D' && h[2] == '3' && ((h[6] | h[7] | h[8] | h[9]) & 0x80u) == 0u) {
        size_t tag = 10u + (((size_t)h[6] << 21) | ((size_t)h[7] << 14) | ((size_t)h[8] << 7) | h[9]) +
                     ((h[5] & 0x10u) != 0u ? 10u : 0u);
        if (tag + SEL_STREAM_HEAD > want) {
            want = tag + SEL_STREAM_HEAD;
        }
    }
    if (want > SEL_FILE_HEAD_MAX) {
        want = SEL_FILE_HEAD_MAX;
    }
    return want > size ? (size_t)size : want;
}

sel_file_stream* sel_file_stream_open(const sel_host* host, const char* path, uint8_t** head, size_t* headn) {
    sel_file_stream* s;
    uint8_t first[10];
    size_t want, got;
    long len;

    s = (sel_file_stream*)host->mem_alloc(host, sizeof *s, SEL_MEM_FAST);
    if (s == NULL) {
        return NULL;
    }
    memset(s, 0, sizeof *s);
    s->host = host;
    s->f = fopen(path, "rb");
    if (s->f == NULL || fseek(s->f, 0, SEEK_END) != 0 || (len = ftell(s->f)) <= 0 || fseek(s->f, 0, SEEK_SET) != 0) {
        sel_file_stream_close(s);
        return NULL;
    }
    s->size = (uint64_t)len;
    got = fread(first, 1, sizeof first, s->f);
    want = file_head_want(first, got, s->size);
    *head = (uint8_t*)host->mem_alloc(host, want + 1u, SEL_MEM_FAST);
    if (*head == NULL || fseek(s->f, 0, SEEK_SET) != 0) {
        sel_file_stream_close(s);
        return NULL;
    }
    *headn = fread(*head, 1, want, s->f);
    (*head)[*headn] = 0;
    g_file_bytes += (uint32_t)(got + *headn);
    if (*headn == 0u || fseek(s->f, 0, SEEK_SET) != 0) {
        host->mem_free(host, *head);
        *head = NULL;
        sel_file_stream_close(s);
        return NULL;
    }
    s->view.ctx = s;
    s->view.read = file_read;
    s->view.read_wait = file_read; /* a file never makes the reader wait */
    s->view.ended = file_ended;
    s->view.total = s->size;
    s->view.seek = file_seek;
    g_file_opens++;
    return s;
}

const sel_byte_stream* sel_file_stream_source(sel_file_stream* s) {
    return &s->view;
}

uint64_t sel_file_stream_size(const sel_file_stream* s) {
    return s->size;
}

uint8_t* sel_file_stream_drain(sel_file_stream* s, size_t* size) {
    uint8_t* buf;
    size_t n;

    if ((uint64_t)(size_t)s->size != s->size || fseek(s->f, 0, SEEK_SET) != 0) {
        return NULL;
    }
    s->pos = 0;
    buf = (uint8_t*)s->host->mem_alloc(s->host, (size_t)s->size + 1u, SEL_MEM_FAST);
    if (buf == NULL) {
        return NULL;
    }
    n = file_read(s, buf, (size_t)s->size);
    if (n != (size_t)s->size) {
        s->host->mem_free(s->host, buf);
        return NULL;
    }
    buf[n] = 0;
    *size = n;
    return buf;
}

void sel_file_stream_close(sel_file_stream* s) {
    if (s != NULL) {
        const sel_host* host = s->host;
        if (s->f != NULL) {
            fclose(s->f);
        }
        host->mem_free(host, s);
    }
}

uint32_t sel_file_stream_opens(void) {
    return g_file_opens;
}

uint32_t sel_file_stream_bytes_read(void) {
    return g_file_bytes;
}

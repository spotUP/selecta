/* I168b: the cooperative (no thread) prefetch downloads in slices. On the Amiga sel_library_prefetch_poll runs the
 * fetch on the engine pump's task, so one call may block only briefly (cache.slice_ms), a download resumes with a
 * Range request, and a foreground fetch of the same file adopts the partial download. The poll glue in
 * db/fetch.c calls exactly sel_cache_fetch_song with slice_ms set; this test drives that entry over a socket
 * server in this process (slow body, never-answering, Range) with the real HTTP client and mirror code.
 *
 * Tests named after the symptom:
 *   pump_tick_stays_short_during_slow_download   defect 1: no call blocks for the whole transfer
 *   stalled_prefetch_tick_returns_within_bound   defect 2: no answer before the first byte ends the call early
 *   foreground_fetch_adopts_partial_prefetch     defect 3: the same file is resumed, not downloaded again */
#include "cache.h"
#include "net/net.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define BODY_LEN 200000u
#define CHUNK 10000u
#define GAP_US 50000u /* 20 chunks, 1 s for the whole body */

static int failures;
static int lfd;
static unsigned short port;
static pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;
static unsigned long sent_total; /* body bytes the server wrote, all requests */
static unsigned requests, ranged;
static unsigned long last_from;

#define CHECK(cond)                                                \
    do {                                                           \
        if (!(cond)) {                                             \
            printf("FAIL line %d: %s\n", __LINE__, #cond);         \
            failures++;                                            \
        }                                                          \
    } while (0)

static unsigned char body_byte(unsigned long i) {
    return (unsigned char)((i * 7u + (i >> 8)) & 0xFFu);
}

static void* serve(void* arg) {
    int fd = (int)(long)arg;
    char req[2048];
    ssize_t n = recv(fd, req, sizeof req - 1, 0);
    unsigned long from = 0;
    char head[256];
    unsigned long off;
    int stall;
    char* r;

    if (n <= 0) {
        close(fd);
        return NULL;
    }
    req[n] = '\0';
    stall = strstr(req, "GET /stall") != NULL;
    r = strstr(req, "Range: bytes=");
    if (r != NULL) {
        from = strtoul(r + 13, NULL, 10);
    }
    pthread_mutex_lock(&mu);
    requests++;
    if (from > 0) {
        ranged++;
        last_from = from;
    }
    pthread_mutex_unlock(&mu);
    if (stall) {
        sleep(6); /* longer than the 3 s first-byte limit of the client */
        close(fd);
        return NULL;
    }
    if (from > 0) {
        snprintf(head, sizeof head,
                 "HTTP/1.1 206 Partial Content\r\nContent-Length: %lu\r\nContent-Range: bytes %lu-%u/%u\r\nConnection: close\r\n\r\n",
                 BODY_LEN - from, from, BODY_LEN - 1u, BODY_LEN);
    } else {
        snprintf(head, sizeof head, "HTTP/1.1 200 OK\r\nContent-Length: %u\r\nConnection: close\r\n\r\n", BODY_LEN);
    }
    send(fd, head, strlen(head), 0);
    for (off = from; off < BODY_LEN;) {
        unsigned char buf[CHUNK];
        size_t m = BODY_LEN - off < CHUNK ? BODY_LEN - off : CHUNK, i;
        for (i = 0; i < m; i++) {
            buf[i] = body_byte(off + i);
        }
        if (send(fd, buf, m, 0) != (ssize_t)m) {
            break;
        }
        pthread_mutex_lock(&mu);
        sent_total += m;
        pthread_mutex_unlock(&mu);
        off += m;
        usleep(GAP_US);
    }
    close(fd);
    return NULL;
}

static void* accept_loop(void* arg) {
    (void)arg;
    for (;;) {
        int fd = accept(lfd, NULL, NULL);
        pthread_t t;
        if (fd < 0) {
            return NULL;
        }
        pthread_create(&t, NULL, serve, (void*)(long)fd);
        pthread_detach(t);
    }
}

static void start_server(void) {
    struct sockaddr_in a;
    socklen_t l = sizeof a;
    pthread_t t;
    char buf[128];

    lfd = socket(AF_INET, SOCK_STREAM, 0);
    memset(&a, 0, sizeof a);
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(lfd, (struct sockaddr*)&a, sizeof a) != 0 || listen(lfd, 16) != 0 ||
        getsockname(lfd, (struct sockaddr*)&a, &l) != 0) {
        printf("FAIL server\n");
        exit(1);
    }
    port = ntohs(a.sin_port);
    pthread_create(&t, NULL, accept_loop, NULL);
    pthread_detach(t);
    setenv("SELECTA_NET_ALLOW", "127.0.0.1", 1);
    snprintf(buf, sizeof buf, "http://127.0.0.1:%u/", (unsigned)port);
    setenv("SELECTA_MIRRORS_MODLAND", buf, 1);
}

static void new_cache(sel_cache* c, const char* dir) {
    char cmd[300];
    snprintf(cmd, sizeof cmd, "rm -rf '%s'", dir);
    (void)system(cmd);
    sel_cache_init(c, dir, 0);
}

static int body_ok(const char* path) {
    FILE* f = fopen(path, "rb");
    unsigned long i;
    if (f == NULL) {
        return 0;
    }
    for (i = 0; i < BODY_LEN; i++) {
        if (fgetc(f) != (int)body_byte(i)) {
            fclose(f);
            return 0;
        }
    }
    i = (unsigned long)(fgetc(f) == EOF);
    fclose(f);
    return (int)i;
}

static unsigned long ms_since(unsigned long t0) {
    return sel_net_now_ms() - t0;
}

static void pump_tick_stays_short_during_slow_download(const char* dir) {
    sel_cache c;
    char out[SEL_CACHE_PATH_MAX];
    unsigned long t0, worst = 0, whole;
    unsigned calls = 0;
    sel_cache_result r;

    /* the old behaviour: one blocking call for the whole transfer */
    new_cache(&c, dir);
    t0 = sel_net_now_ms();
    r = sel_cache_fetch(&c, "modland", "whole.bin", NULL, out, sizeof out);
    whole = ms_since(t0);
    CHECK(r == SEL_CACHE_OK);

    new_cache(&c, dir);
    c.slice_ms = 100;
    do {
        t0 = sel_net_now_ms();
        r = sel_cache_fetch(&c, "modland", "sliced.bin", NULL, out, sizeof out);
        if (ms_since(t0) > worst) {
            worst = ms_since(t0);
        }
        calls++;
    } while (r == SEL_CACHE_PENDING && calls < 100);
    printf("tick: unsliced call %lu ms; sliced worst call %lu ms over %u calls\n", whole, worst, calls);
    CHECK(r == SEL_CACHE_OK);
    CHECK(worst < 400);
    CHECK(calls >= 3);
    CHECK(body_ok(out)); /* resumed pieces add up to the exact body, md5 state carried over */
    CHECK(c.stats.fetches == 1);
}

static void stalled_prefetch_tick_returns_within_bound(const char* dir) {
    sel_cache c;
    char out[SEL_CACHE_PATH_MAX];
    unsigned long t0, sliced, whole;
    sel_cache_result r;

    new_cache(&c, dir);
    t0 = sel_net_now_ms();
    r = sel_cache_fetch(&c, "modland", "stall.bin", NULL, out, sizeof out);
    whole = ms_since(t0);
    CHECK(r != SEL_CACHE_OK);

    new_cache(&c, dir);
    c.slice_ms = 100;
    t0 = sel_net_now_ms();
    r = sel_cache_fetch(&c, "modland", "stall.bin", NULL, out, sizeof out);
    sliced = ms_since(t0);
    sel_cache_slice_drop(&c); /* library close with a request in progress */
    printf("stall: unsliced call %lu ms; sliced call %lu ms\n", whole, sliced);
    CHECK(r != SEL_CACHE_OK);
    CHECK(sliced < 1600); /* SEL_CACHE_SLICE_STALL_MS plus margin, not the 3 s first-byte limit */
    CHECK(whole >= 2500);
    CHECK(!c.slice.active);
}

static void foreground_fetch_adopts_partial_prefetch(const char* dir) {
    sel_cache c;
    char out[SEL_CACHE_PATH_MAX], part[SEL_CACHE_PATH_MAX + 8];
    unsigned long before, offset;
    unsigned rq0, rg0;
    sel_cache_result r;
    FILE* f;

    new_cache(&c, dir);
    c.slice_ms = 100;
    pthread_mutex_lock(&mu);
    before = sent_total;
    rq0 = requests;
    rg0 = ranged;
    pthread_mutex_unlock(&mu);
    r = sel_cache_fetch(&c, "modland", "adopt.bin", NULL, out, sizeof out);
    CHECK(r == SEL_CACHE_PENDING);
    CHECK(c.slice.active && c.slice.offset > 0 && c.slice.offset < BODY_LEN);
    offset = c.slice.offset;
    snprintf(part, sizeof part, "%s.part", out);
    f = fopen(part, "rb");
    CHECK(f != NULL); /* the partial download is on disk for the foreground fetch */
    if (f != NULL) {
        fclose(f);
    }
    /* foreground: unsliced, same file */
    c.slice_ms = 0;
    r = sel_cache_fetch(&c, "modland", "adopt.bin", NULL, out, sizeof out);
    CHECK(r == SEL_CACHE_OK);
    CHECK(body_ok(out));
    pthread_mutex_lock(&mu);
    printf("adopt: partial %lu bytes; second request Range from %lu; requests %u (ranged %u); server sent %lu of %u\n",
           offset, last_from, requests - rq0, ranged - rg0, sent_total - before, BODY_LEN);
    CHECK(requests - rq0 == 2 && ranged - rg0 == 1);
    CHECK(last_from == offset);
    CHECK(sent_total - before < (unsigned long)BODY_LEN + 2u * CHUNK); /* not downloaded twice */
    pthread_mutex_unlock(&mu);

    /* a foreground fetch of another file discards the partial download */
    c.slice_ms = 100;
    r = sel_cache_fetch(&c, "modland", "other1.bin", NULL, out, sizeof out);
    CHECK(r == SEL_CACHE_PENDING);
    c.slice_ms = 0;
    r = sel_cache_fetch(&c, "modland", "other2.bin", NULL, out, sizeof out);
    CHECK(r == SEL_CACHE_OK);
    snprintf(part, sizeof part, "%s/files/modland/other1.bin.part", dir);
    f = fopen(part, "rb");
    CHECK(f == NULL);
    if (f != NULL) {
        fclose(f);
    }
}

int main(void) {
    char dir[64];

    signal(SIGPIPE, SIG_IGN);
    snprintf(dir, sizeof dir, "/tmp/sel-cache-slice-%d", (int)getpid());
    start_server();
    pump_tick_stays_short_during_slow_download(dir);
    stalled_prefetch_tick_returns_within_bound(dir);
    foreground_fetch_adopts_partial_prefetch(dir);
    {
        char cmd[100];
        snprintf(cmd, sizeof cmd, "rm -rf '%s'", dir);
        (void)system(cmd);
    }
    if (failures == 0) {
        printf("cache_slice_ok\n");
    }
    return failures != 0;
}

/* I774o R12: the UADE data set downloader. A stub HTTP server in this process serves manifest.txt and <bake>/<file>;
 * the real mirror, cache and md5 code fetch from it. Cases:
 *   fresh_install            every file arrives and is installed
 *   unchanged_set_is_current a second run changes nothing
 *   wrong_md5_keeps_old_set  a body whose md5 differs from the manifest is rejected, the old files stay, no *.new left
 *   fixed_server_updates     the same update succeeds once the server serves the right bytes */
#include "cache.h"
#include "uadedata.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#define NFILES 5
static const char* const NAMES[NFILES] = {"songlengths.bin", "uadecredits.bin", "uadeinfo.bin", "uademd5.bin", "uadeprov.bin"};

static int failures;
static int lfd;
static unsigned short port;
static pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;
static char bake[16] = "1";
static char content[NFILES][64]; /* what the manifest promises */
static int corrupt = -1;         /* index served with wrong bytes, -1 none */

#define CHECK(cond)                                        \
    do {                                                   \
        if (!(cond)) {                                     \
            printf("FAIL line %d: %s\n", __LINE__, #cond); \
            failures++;                                    \
        }                                                  \
    } while (0)

static void md5_of(const char* s, char out[33]) {
    sel_md5 m;

    sel_md5_init(&m);
    sel_md5_update(&m, s, strlen(s));
    sel_md5_hex(&m, out);
}

static void reply(int fd, const char* body) {
    char head[128];

    snprintf(head, sizeof head, "HTTP/1.1 200 OK\r\nContent-Length: %zu\r\nConnection: close\r\n\r\n", strlen(body));
    send(fd, head, strlen(head), 0);
    send(fd, body, strlen(body), 0);
}

static void* serve(void* arg) {
    int fd = (int)(long)arg;
    char req[1024], man[1024], md5[33], want[64];
    unsigned i;

    if (recv(fd, req, sizeof req - 1, 0) > 0) {
        pthread_mutex_lock(&mu);
        if (strstr(req, "GET /manifest.txt") != NULL) {
            snprintf(man, sizeof man, "selecta-uadedata 1\nbake %s\n", bake);
            for (i = 0; i < NFILES; i++) {
                md5_of(content[i], md5);
                snprintf(man + strlen(man), sizeof man - strlen(man), "%s %zu %s\n", md5, strlen(content[i]), NAMES[i]);
            }
            reply(fd, man);
        } else {
            for (i = 0; i < NFILES; i++) {
                snprintf(want, sizeof want, "GET /%s/%s ", bake, NAMES[i]);
                if (strstr(req, want) != NULL) {
                    reply(fd, (int)i == corrupt ? "tampered bytes" : content[i]);
                    break;
                }
            }
            if (i == NFILES) {
                send(fd, "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n", 66, 0);
            }
        }
        pthread_mutex_unlock(&mu);
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
    if (bind(lfd, (struct sockaddr*)&a, sizeof a) != 0 || listen(lfd, 16) != 0 || getsockname(lfd, (struct sockaddr*)&a, &l) != 0) {
        printf("server setup failed\n");
        exit(2);
    }
    port = ntohs(a.sin_port);
    pthread_create(&t, NULL, accept_loop, NULL);
    setenv("SELECTA_NET_ALLOW", "127.0.0.1", 1);
    snprintf(buf, sizeof buf, "http://127.0.0.1:%u/", (unsigned)port);
    setenv("SELECTA_MIRRORS_UADEDATA", buf, 1);
}

static int has(const char* dir, const char* name, const char* text) {
    char path[256], got[80] = "";
    FILE* f;

    snprintf(path, sizeof path, "%s/%s", dir, name);
    f = fopen(path, "rb");
    if (f != NULL) {
        size_t n = fread(got, 1, sizeof got - 1, f);
        got[n] = '\0';
        fclose(f);
    }
    return text != NULL ? strcmp(got, text) == 0 : f == NULL;
}

static void set_content(const char* tag) {
    unsigned i;

    for (i = 0; i < NFILES; i++) {
        snprintf(content[i], sizeof content[i], "%s %s", NAMES[i], tag);
    }
}

int main(void) {
    char root[64], cdir[96], dir[96], tmp[160];
    sel_cache c;
    unsigned i;

    signal(SIGPIPE, SIG_IGN);
    snprintf(root, sizeof root, "/tmp/uadedata_test_XXXXXX");
    if (mkdtemp(root) == NULL) {
        return 2;
    }
    snprintf(cdir, sizeof cdir, "%s/cache", root);
    snprintf(dir, sizeof dir, "%s/cfg", root);
    mkdir(cdir, 0777);
    mkdir(dir, 0777);
    start_server();
    sel_cache_init(&c, cdir, 0);

    set_content("v1");
    CHECK(sel_uadedata_update(&c, dir) == SEL_UADEDATA_UPDATED); /* fresh_install */
    for (i = 0; i < NFILES; i++) {
        CHECK(has(dir, NAMES[i], content[i]));
    }
    CHECK(sel_uadedata_update(&c, dir) == SEL_UADEDATA_CURRENT); /* unchanged_set_is_current */

    pthread_mutex_lock(&mu);
    snprintf(bake, sizeof bake, "2");
    set_content("v2");
    corrupt = 2;
    pthread_mutex_unlock(&mu);
    CHECK(sel_uadedata_update(&c, dir) == SEL_UADEDATA_MD5); /* wrong_md5_keeps_old_set */
    for (i = 0; i < NFILES; i++) {
        snprintf(tmp, sizeof tmp, "%s v1", NAMES[i]);
        CHECK(has(dir, NAMES[i], tmp));
        snprintf(tmp, sizeof tmp, "%s.new", NAMES[i]);
        CHECK(has(dir, tmp, NULL));
    }

    pthread_mutex_lock(&mu);
    corrupt = -1;
    pthread_mutex_unlock(&mu);
    CHECK(sel_uadedata_update(&c, dir) == SEL_UADEDATA_UPDATED); /* fixed_server_updates */
    for (i = 0; i < NFILES; i++) {
        CHECK(has(dir, NAMES[i], content[i]));
    }

    snprintf(tmp, sizeof tmp, "rm -rf %s", root);
    if (system(tmp) != 0) {
        printf("cleanup failed\n");
    }
    if (failures == 0) {
        printf("uadedata_ok\n");
    }
    return failures != 0;
}

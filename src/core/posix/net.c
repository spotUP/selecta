/* POSIX socket table (macOS, Linux); interface and env contract in
 * src/library/net/net.h. Non-blocking sockets driven through poll() so every
 * wait has a timeout. */
#include "../../library/net/net.h"

#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

/* The slot table is shared by every thread that talks HTTP (engine, prefetch, DEViLBOX push, I173c): g_lock guards
 * slot choice and the stats; a slot is reserved (-2) while its connect runs, and a handle belongs to one caller. */
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static int g_fd[SEL_NET_MAX_CONN];
static int g_init;
static sel_net_stats g_stats;

static void table_init(void) {
    int i;
    if (!g_init) {
        for (i = 0; i < SEL_NET_MAX_CONN; i++) {
            g_fd[i] = -1;
        }
        signal(SIGPIPE, SIG_IGN);
        g_init = 1;
    }
}

unsigned long sel_net_now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (unsigned long)ts.tv_sec * 1000ul + (unsigned long)(ts.tv_nsec / 1000000);
}

const char* sel_net_ca_file(void) {
    const char* p = getenv("SELECTA_CA_FILE");
    return p != NULL && p[0] != '\0' ? p : NULL;
}

/* Reads everything the command or file yields into a malloc'd NUL-terminated buffer. */
static char* slurp(FILE* f, size_t* len) {
    size_t cap = 1u << 16, n = 0, got;
    char* buf = malloc(cap);
    while (buf != NULL && (got = fread(buf + n, 1, cap - n - 1, f)) > 0) {
        char* grown;
        n += got;
        if (cap - n < 2) {
            cap *= 2;
            grown = realloc(buf, cap);
            if (grown == NULL) {
                free(buf);
                return NULL;
            }
            buf = grown;
        }
    }
    if (buf != NULL) {
        buf[n] = '\0';
        *len = n;
    }
    return buf;
}

char* sel_net_os_ca_bundle(size_t* len) {
    char* out = NULL;
    *len = 0;
#ifdef __APPLE__
    {
        FILE* p = popen("/usr/bin/security find-certificate -a -p "
                        "/System/Library/Keychains/SystemRootCertificates.keychain "
                        "/Library/Keychains/System.keychain 2>/dev/null", "r");
        if (p != NULL) {
            out = slurp(p, len);
            pclose(p);
        }
    }
#else
    {
        static const char* const files[] = {"/etc/ssl/certs/ca-certificates.crt", "/etc/pki/tls/certs/ca-bundle.crt",
                                            "/etc/ssl/cert.pem", NULL};
        int i;
        for (i = 0; files[i] != NULL && out == NULL; i++) {
            FILE* f = fopen(files[i], "rb");
            if (f != NULL) {
                out = slurp(f, len);
                fclose(f);
            }
        }
    }
#endif
    if (out != NULL && *len == 0) {
        free(out);
        out = NULL;
    }
    return out;
}

void sel_net_stats_get(sel_net_stats* out) {
    pthread_mutex_lock(&g_lock);
    *out = g_stats;
    pthread_mutex_unlock(&g_lock);
}

static int wait_fd(int fd, short ev, unsigned ms) {
    struct pollfd p;
    int r;
    p.fd = fd;
    p.events = ev;
    p.revents = 0;
    do {
        r = poll(&p, 1, (int)ms);
    } while (r < 0 && errno == EINTR);
    return r;
}

int sel_net_open(const char* host, unsigned port, unsigned connect_ms) {
    char addr[256];
    char portstr[16];
    struct addrinfo hints, *res, *ai;
    const char* target = host;
    unsigned tport = port;
    int slot, fd = -1, err = SEL_NET_E_CONNECT;

    pthread_mutex_lock(&g_lock);
    table_init();
    if (sel_net_map_lookup(getenv("SELECTA_NET_MAP"), host, addr, sizeof addr, &tport)) {
        target = addr;
        g_stats.mapped++;
    }
    /* The guard judges the real target (after the map), so a mapped fixture address passes. */
    if (!sel_net_allowed(getenv("SELECTA_NET_ALLOW"), target)) {
        g_stats.blocked++;
        pthread_mutex_unlock(&g_lock);
        return SEL_NET_E_BLOCKED;
    }
    for (slot = 0; slot < SEL_NET_MAX_CONN && g_fd[slot] != -1; slot++) {
    }
    if (slot == SEL_NET_MAX_CONN) {
        pthread_mutex_unlock(&g_lock);
        return SEL_NET_E_TABLE;
    }
    g_fd[slot] = -2;
    pthread_mutex_unlock(&g_lock);
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    snprintf(portstr, sizeof portstr, "%u", tport);
    if (getaddrinfo(target, portstr, &hints, &res) != 0) {
        pthread_mutex_lock(&g_lock);
        g_fd[slot] = -1;
        pthread_mutex_unlock(&g_lock);
        return SEL_NET_E_RESOLVE;
    }
    for (ai = res; ai != NULL && fd < 0; ai = ai->ai_next) {
        int s = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (s < 0) {
            continue;
        }
        fcntl(s, F_SETFL, fcntl(s, F_GETFL, 0) | O_NONBLOCK);
        if (connect(s, ai->ai_addr, ai->ai_addrlen) == 0) {
            fd = s;
        } else if (errno == EINPROGRESS) {
            int w = wait_fd(s, POLLOUT, connect_ms);
            if (w == 0) {
                err = SEL_NET_E_TIMEOUT;
            } else if (w > 0) {
                int soerr = 0;
                socklen_t sl = sizeof soerr;
                if (getsockopt(s, SOL_SOCKET, SO_ERROR, &soerr, &sl) == 0 && soerr == 0) {
                    fd = s;
                }
            }
        }
        if (fd < 0) {
            close(s);
        }
    }
    freeaddrinfo(res);
    pthread_mutex_lock(&g_lock);
    g_fd[slot] = fd < 0 ? -1 : fd;
    if (fd >= 0) {
        g_stats.connects++;
    }
    pthread_mutex_unlock(&g_lock);
    return fd < 0 ? err : slot;
}

int sel_net_send(int h, const void* data, size_t len, unsigned timeout_ms) {
    const char* p = data;
    unsigned long end = sel_net_now_ms() + timeout_ms;
    if (h < 0 || h >= SEL_NET_MAX_CONN || g_fd[h] < 0) {
        return SEL_NET_E_IO;
    }
    while (len > 0) {
        unsigned long now = sel_net_now_ms();
        ssize_t n;
        if (now >= end || wait_fd(g_fd[h], POLLOUT, (unsigned)(end - now)) <= 0) {
            return SEL_NET_E_TIMEOUT;
        }
        n = send(g_fd[h], p, len, 0);
        if (n < 0) {
            if (errno == EAGAIN || errno == EINTR) {
                continue;
            }
            return SEL_NET_E_IO;
        }
        p += n;
        len -= (size_t)n;
    }
    return 0;
}

int sel_net_recv(int h, void* buf, size_t cap, unsigned timeout_ms) {
    ssize_t n;
    int w;
    if (h < 0 || h >= SEL_NET_MAX_CONN || g_fd[h] < 0) {
        return SEL_NET_E_IO;
    }
    for (;;) {
        w = wait_fd(g_fd[h], POLLIN, timeout_ms);
        if (w == 0) {
            return SEL_NET_E_TIMEOUT;
        }
        if (w < 0) {
            return SEL_NET_E_IO;
        }
        n = recv(g_fd[h], buf, cap, 0);
        if (n > 0) {
            return (int)n;
        }
        if (n == 0) {
            return SEL_NET_E_CLOSED;
        }
        if (errno != EAGAIN && errno != EINTR) {
            return SEL_NET_E_IO;
        }
    }
}

void sel_net_close(int h) {
    if (h >= 0 && h < SEL_NET_MAX_CONN && g_fd[h] >= 0) {
        int fd = g_fd[h];
        pthread_mutex_lock(&g_lock);
        g_fd[h] = -1;
        pthread_mutex_unlock(&g_lock);
        close(fd);
    }
}

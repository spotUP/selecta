/* Amiga socket table over bsdsocket.library; interface and env contract in
 * src/library/net/net.h. Sockets are non-blocking (IoctlSocket FIONBIO) and every
 * wait goes through WaitSelect with a timeout. The library is opened on first use,
 * so a machine without a TCP/IP stack reports SEL_NET_E_CONNECT instead of failing
 * at startup. Runtime untested (no stack under vamos); it compiles and links. */
#include "../../library/net/net.h"

#include <dos/dos.h>
#include <exec/types.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/socket.h>

#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <netdb.h>
#include <netinet/in.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Library* SocketBase; /* proto/socket.h inline stubs read this name */

static long g_sock[SEL_NET_MAX_CONN];
static int g_init;
static int g_open_failed;
static sel_net_stats g_stats;

static int stack_ready(void) {
    int i;
    if (!g_init) {
        for (i = 0; i < SEL_NET_MAX_CONN; i++) {
            g_sock[i] = -1;
        }
        g_init = 1;
    }
    if (SocketBase == NULL && !g_open_failed) {
        SocketBase = OpenLibrary((CONST_STRPTR) "bsdsocket.library", 4);
        g_open_failed = SocketBase == NULL;
    }
    return SocketBase != NULL;
}

unsigned long sel_net_now_ms(void) {
    struct DateStamp ds;
    DateStamp(&ds);
    return (unsigned long)ds.ds_Days * 86400000ul + (unsigned long)ds.ds_Minute * 60000ul +
           (unsigned long)ds.ds_Tick * 20ul;
}

const char* sel_net_ca_file(void) {
    const char* p = getenv("SELECTA_CA_FILE");
    return p != NULL && p[0] != '\0' ? p : NULL;
}

void sel_net_stats_get(sel_net_stats* out) {
    *out = g_stats;
}

/* mode 0 = readable, 1 = writable. Returns the WaitSelect result. */
static long wait_sock(long s, int mode, unsigned ms) {
    fd_set set;
    struct timeval tv;
    FD_ZERO(&set);
    FD_SET(s, &set);
    tv.tv_secs = (long)(ms / 1000u);
    tv.tv_micro = (long)(ms % 1000u) * 1000L;
    return mode == 0 ? WaitSelect(s + 1, &set, NULL, NULL, &tv, NULL) : WaitSelect(s + 1, NULL, &set, NULL, &tv, NULL);
}

int sel_net_open(const char* host, unsigned port, unsigned connect_ms) {
    char addr[256];
    const char* target = host;
    unsigned tport = port;
    struct sockaddr_in sa;
    unsigned long ip = 0;
    unsigned q[4];
    char tail;
    long s, nb = 1;
    int slot;

    if (!stack_ready()) {
        return SEL_NET_E_CONNECT;
    }
    if (sel_net_map_lookup(getenv("SELECTA_NET_MAP"), host, addr, sizeof addr, &tport)) {
        target = addr;
        g_stats.mapped++;
    }
    /* The guard judges the real target (after the map), so a mapped fixture address passes. */
    if (!sel_net_allowed(getenv("SELECTA_NET_ALLOW"), target)) {
        g_stats.blocked++;
        return SEL_NET_E_BLOCKED;
    }
    for (slot = 0; slot < SEL_NET_MAX_CONN && g_sock[slot] >= 0; slot++) {
    }
    if (slot == SEL_NET_MAX_CONN) {
        return SEL_NET_E_TABLE;
    }
    if (sscanf(target, "%u.%u.%u.%u%c", &q[0], &q[1], &q[2], &q[3], &tail) == 4 && q[0] < 256 && q[1] < 256 &&
        q[2] < 256 && q[3] < 256) {
        ip = /* 68k is big-endian: host order equals network order */
        (q[0] << 24) | (q[1] << 16) | (q[2] << 8) | q[3];
    } else {
        struct hostent* he = gethostbyname((STRPTR)target);
        if (he == NULL || he->h_addrtype != AF_INET || he->h_length != 4) {
            return SEL_NET_E_RESOLVE;
        }
        memcpy(&ip, he->h_addr, 4);
    }
    memset(&sa, 0, sizeof sa);
    sa.sin_family = AF_INET;
    sa.sin_port = htons((unsigned short)tport);
    sa.sin_addr.s_addr = (unsigned long)ip;
    s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) {
        return SEL_NET_E_CONNECT;
    }
    IoctlSocket(s, FIONBIO, (char*)&nb);
    if (connect(s, (struct sockaddr*)&sa, sizeof sa) < 0) {
        int e = Errno();
        long w;
        if (e != EINPROGRESS) {
            CloseSocket(s);
            return SEL_NET_E_CONNECT;
        }
        w = wait_sock(s, 1, connect_ms);
        if (w <= 0) {
            CloseSocket(s);
            return w == 0 ? SEL_NET_E_TIMEOUT : SEL_NET_E_CONNECT;
        }
        {
            long soerr = 0;
            socklen_t sl = sizeof soerr;
            if (getsockopt(s, SOL_SOCKET, SO_ERROR, (char*)&soerr, &sl) != 0 || soerr != 0) {
                CloseSocket(s);
                return SEL_NET_E_CONNECT;
            }
        }
    }
    g_sock[slot] = s;
    g_stats.connects++;
    return slot;
}

int sel_net_send(int h, const void* data, size_t len, unsigned timeout_ms) {
    const char* p = data;
    unsigned long end = sel_net_now_ms() + timeout_ms;
    if (h < 0 || h >= SEL_NET_MAX_CONN || g_sock[h] < 0) {
        return SEL_NET_E_IO;
    }
    while (len > 0) {
        unsigned long now = sel_net_now_ms();
        long n;
        if (now >= end || wait_sock(g_sock[h], 1, (unsigned)(end - now)) <= 0) {
            return SEL_NET_E_TIMEOUT;
        }
        n = send(g_sock[h], (UBYTE*)p, (long)len, 0);
        if (n < 0) {
            if (Errno() == EWOULDBLOCK) {
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
    long n, w;
    if (h < 0 || h >= SEL_NET_MAX_CONN || g_sock[h] < 0) {
        return SEL_NET_E_IO;
    }
    for (;;) {
        w = wait_sock(g_sock[h], 0, timeout_ms);
        if (w == 0) {
            return SEL_NET_E_TIMEOUT;
        }
        if (w < 0) {
            return SEL_NET_E_IO;
        }
        n = recv(g_sock[h], (UBYTE*)buf, (long)cap, 0);
        if (n > 0) {
            return (int)n;
        }
        if (n == 0) {
            return SEL_NET_E_CLOSED;
        }
        if (Errno() != EWOULDBLOCK) {
            return SEL_NET_E_IO;
        }
    }
}

void sel_net_close(int h) {
    if (h >= 0 && h < SEL_NET_MAX_CONN && g_sock[h] >= 0) {
        CloseSocket(g_sock[h]);
        g_sock[h] = -1;
    }
}

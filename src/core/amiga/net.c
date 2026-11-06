/* Amiga socket table over bsdsocket.library; interface and env contract in
 * src/library/net/net.h. Sockets are non-blocking (IoctlSocket FIONBIO) and every
 * wait goes through WaitSelect with a timeout. The library is opened on first use,
 * so a machine without a TCP/IP stack reports SEL_NET_E_CONNECT instead of failing
 * at startup. Runtime untested (no stack under vamos); it compiles and links. */
#include "net_udp.h"
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

char* sel_net_os_ca_bundle(size_t* len) {
    *len = 0;
    return NULL; /* no OS trust store; the Amiga build is plain HTTP (D-M3-4) */
}

void sel_net_set_cancel(const volatile unsigned* gen, unsigned start) {
    (void)gen; /* jobs run inline on the one task, nothing to cancel from (revisit with I199) */
    (void)start;
}

void sel_net_stats_get(sel_net_stats* out) {
    *out = g_stats;
}

/* bsdsocket's default break mask is SIGBREAKF_CTRL_C: a Ctrl-C that arrives while a
 * blocking call waits (or is already pending) aborts that call with EINTR and the stack
 * CLEARS the signal bit. Networking runs inline on the main task, so the UI's own poll
 * never saw the user's break and Selecta did not quit (I623, measured on rig 3:
 * ctrl_c_before=1 ctrl_c_after=0, errno 4). Raise the bit again so the call still fails
 * fast and the break reaches the main loop. */
static void keep_break(void) {
    if (Errno() == EINTR) {
        SetSignal(SIGBREAKF_CTRL_C, SIGBREAKF_CTRL_C);
    }
}

/* mode 0 = readable, 1 = writable. Returns the WaitSelect result. */
static long wait_sock(long s, int mode, unsigned ms) {
    fd_set set;
    struct timeval tv;
    long r;
    FD_ZERO(&set);
    FD_SET(s, &set);
    tv.tv_secs = (long)(ms / 1000u);
    tv.tv_micro = (long)(ms % 1000u) * 1000L;
    r = mode == 0 ? WaitSelect(s + 1, &set, NULL, NULL, &tv, NULL) : WaitSelect(s + 1, NULL, &set, NULL, &tv, NULL);
    if (r < 0) {
        keep_break();
    }
    return r;
}

/* Dotted quad to a host-order (= network order on the 68k) address. 1 on success. */
static int parse_ip4(const char* text, unsigned long* ip) {
    unsigned q[4];
    char tail;
    if (sscanf(text, "%u.%u.%u.%u%c", &q[0], &q[1], &q[2], &q[3], &tail) != 4 || q[0] > 255 || q[1] > 255 ||
        q[2] > 255 || q[3] > 255) {
        return 0;
    }
    *ip = (q[0] << 24) | (q[1] << 16) | (q[2] << 8) | q[3];
    return 1;
}

static void format_ip4(unsigned long ip, char* out, size_t cap) {
    snprintf(out, cap, "%lu.%lu.%lu.%lu", (ip >> 24) & 255u, (ip >> 16) & 255u, (ip >> 8) & 255u, ip & 255u);
}

int sel_net_open(const char* host, unsigned port, unsigned connect_ms) {
    char addr[256];
    const char* target = host;
    unsigned tport = port;
    struct sockaddr_in sa;
    unsigned long ip = 0;
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
    if (parse_ip4(target, &ip)) {
    } else {
        struct hostent* he = gethostbyname((STRPTR)target);
        if (he == NULL || he->h_addrtype != AF_INET || he->h_length != 4) {
            keep_break(); /* the resolver waits on the stack too */
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
            keep_break();
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

/* ---- Listening side (I892e2): the Chromecast media server. Same handle table. ---- */

int sel_net_listen(unsigned* port) {
    struct sockaddr_in sa;
    socklen_t sl = sizeof sa;
    long s, nb = 1, one = 1;
    int slot;

    if (!stack_ready()) {
        return SEL_NET_E_CONNECT;
    }
    for (slot = 0; slot < SEL_NET_MAX_CONN && g_sock[slot] >= 0; slot++) {
    }
    if (slot == SEL_NET_MAX_CONN) {
        return SEL_NET_E_TABLE;
    }
    s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) {
        return SEL_NET_E_CONNECT;
    }
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char*)&one, sizeof one);
    memset(&sa, 0, sizeof sa);
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = INADDR_ANY;
    sa.sin_port = htons((unsigned short)*port);
    if (bind(s, (struct sockaddr*)&sa, sizeof sa) != 0 || listen(s, 4) != 0 ||
        getsockname(s, (struct sockaddr*)&sa, &sl) != 0) {
        CloseSocket(s);
        return SEL_NET_E_IO;
    }
    IoctlSocket(s, FIONBIO, (char*)&nb);
    g_sock[slot] = s;
    *port = ntohs(sa.sin_port);
    return slot;
}

int sel_net_accept(int lh, unsigned timeout_ms) {
    struct sockaddr_in sa;
    socklen_t sl = sizeof sa;
    long w, c, nb = 1;
    int slot;

    if (lh < 0 || lh >= SEL_NET_MAX_CONN || g_sock[lh] < 0) {
        return SEL_NET_E_IO;
    }
    for (slot = 0; slot < SEL_NET_MAX_CONN && g_sock[slot] >= 0; slot++) {
    }
    if (slot == SEL_NET_MAX_CONN) {
        return SEL_NET_E_TABLE;
    }
    w = wait_sock(g_sock[lh], 0, timeout_ms);
    if (w == 0) {
        return SEL_NET_E_TIMEOUT;
    }
    if (w < 0) {
        return SEL_NET_E_IO;
    }
    c = accept(g_sock[lh], (struct sockaddr*)&sa, &sl);
    if (c < 0) {
        return Errno() == EWOULDBLOCK ? SEL_NET_E_TIMEOUT : SEL_NET_E_IO;
    }
    IoctlSocket(c, FIONBIO, (char*)&nb);
    g_sock[slot] = c;
    return slot;
}

/* ---- UDP and address helpers for the UPnP and RAOP socket shims (I1004). Same handle table, same stack opening. ---- */

static int udp_slot(long s) {
    int slot;
    for (slot = 0; slot < SEL_NET_MAX_CONN && g_sock[slot] >= 0; slot++) {
    }
    if (slot == SEL_NET_MAX_CONN) {
        CloseSocket(s);
        return SEL_NET_E_TABLE;
    }
    g_sock[slot] = s;
    return slot;
}

int sel_net_udp_open(void) {
    long s;
    if (!stack_ready()) {
        return SEL_NET_E_CONNECT;
    }
    s = socket(AF_INET, SOCK_DGRAM, 0);
    return s < 0 ? SEL_NET_E_CONNECT : udp_slot(s);
}

int sel_net_udp_bind(unsigned* port) {
    struct sockaddr_in sa;
    socklen_t sl = sizeof sa;
    int h = sel_net_udp_open();
    if (h < 0) {
        return h;
    }
    memset(&sa, 0, sizeof sa);
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = INADDR_ANY;
    if (bind(g_sock[h], (struct sockaddr*)&sa, sizeof sa) != 0 || getsockname(g_sock[h], (struct sockaddr*)&sa, &sl) != 0) {
        sel_net_close(h);
        return SEL_NET_E_IO;
    }
    *port = ntohs(sa.sin_port);
    return h;
}

int sel_net_udp_send(int h, const char* ip, unsigned port, const void* data, size_t len) {
    struct sockaddr_in sa;
    unsigned long addr;
    if (h < 0 || h >= SEL_NET_MAX_CONN || g_sock[h] < 0 || !parse_ip4(ip, &addr)) {
        return SEL_NET_E_IO;
    }
    memset(&sa, 0, sizeof sa);
    sa.sin_family = AF_INET;
    sa.sin_port = htons((unsigned short)port);
    sa.sin_addr.s_addr = addr;
    return sendto(g_sock[h], (UBYTE*)data, (long)len, 0, (struct sockaddr*)&sa, sizeof sa) == (long)len ? 0 : SEL_NET_E_IO;
}

int sel_net_udp_recv(int h, void* buf, size_t cap, unsigned timeout_ms, char* from, size_t from_cap, unsigned* from_port) {
    struct sockaddr_in sa;
    socklen_t sl = sizeof sa;
    long n, w;
    if (h < 0 || h >= SEL_NET_MAX_CONN || g_sock[h] < 0) {
        return SEL_NET_E_IO;
    }
    w = wait_sock(g_sock[h], 0, timeout_ms);
    if (w == 0) {
        return SEL_NET_E_TIMEOUT;
    }
    if (w < 0) {
        return SEL_NET_E_IO;
    }
    n = recvfrom(g_sock[h], (UBYTE*)buf, (long)cap, 0, (struct sockaddr*)&sa, &sl);
    if (n < 0) {
        return SEL_NET_E_IO;
    }
    if (from != NULL && from_cap > 0) {
        format_ip4((unsigned long)sa.sin_addr.s_addr, from, from_cap);
    }
    if (from_port != NULL) {
        *from_port = ntohs(sa.sin_port);
    }
    return (int)n;
}

int sel_net_udp_multicast(int h, const char* group, unsigned ttl) {
    struct ip_mreq mr;
    unsigned long addr;
    UBYTE t = (UBYTE)ttl;
    if (h < 0 || h >= SEL_NET_MAX_CONN || g_sock[h] < 0 || !parse_ip4(group, &addr)) {
        return SEL_NET_E_IO;
    }
    (void)setsockopt(g_sock[h], IPPROTO_IP, IP_MULTICAST_TTL, (char*)&t, sizeof t);
    memset(&mr, 0, sizeof mr);
    mr.imr_multiaddr.s_addr = addr;
    mr.imr_interface.s_addr = INADDR_ANY;
    /* Sending to the group needs only the TTL. Membership (NOTIFY datagrams) is best effort: an older stack may refuse. */
    return setsockopt(g_sock[h], IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&mr, sizeof mr) == 0 ? 0 : SEL_NET_E_IO;
}

int sel_net_local_ip(int h, char* out, size_t cap) {
    struct sockaddr_in sa;
    socklen_t sl = sizeof sa;
    if (h < 0 || h >= SEL_NET_MAX_CONN || g_sock[h] < 0 || getsockname(g_sock[h], (struct sockaddr*)&sa, &sl) != 0) {
        return SEL_NET_E_IO;
    }
    format_ip4((unsigned long)sa.sin_addr.s_addr, out, cap);
    return 0;
}

/* AmiSSL (tls_amissl.c, I899g) needs the bsdsocket base to open; NULL when there is no TCP/IP stack. */
struct Library* sel_net_amiga_socketbase(void) {
    return stack_ready() ? SocketBase : NULL;
}

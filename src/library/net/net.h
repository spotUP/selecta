/* Socket table (plan 2026-10-25-m3-library.md, I152, D-M3-4/D-M3-6). The only
 * module that touches OS sockets: src/core/posix/net.c (macOS, Linux) and
 * src/core/amiga/net.c (bsdsocket.library). A connection is a small integer
 * handle into a fixed table, so nothing allocates after start.
 *
 * Environment contract (read by the back ends only, see tests/net/fixture_server.py):
 *   SELECTA_NET_ALLOW   comma list of hosts/addresses a connect may reach. When set,
 *                       any other target fails before any SYN with SEL_NET_E_BLOCKED.
 *   SELECTA_NET_MAP     <host>=http://<addr>:<port>[,...]; connects to <host> are
 *                       redirected to <addr>:<port>. The caller keeps sending the
 *                       original name in the Host header.
 *   SELECTA_CA_FILE     TLS trust anchor path (consumed by tls.c, row I154).
 */
#ifndef SELECTA_LIBRARY_NET_NET_H
#define SELECTA_LIBRARY_NET_NET_H

#include <stddef.h>

#define SEL_NET_MAX_CONN 8

enum {
    SEL_NET_E_BLOCKED = -1, /* guard refused the target; no socket was created */
    SEL_NET_E_RESOLVE = -2,
    SEL_NET_E_CONNECT = -3,
    SEL_NET_E_TIMEOUT = -4,
    SEL_NET_E_TABLE = -5, /* no free slot */
    SEL_NET_E_IO = -6,
    SEL_NET_E_CLOSED = -7 /* orderly shutdown by the peer (recv returned 0) */
};

typedef struct sel_net_stats {
    unsigned blocked; /* connects refused by the guard */
    unsigned mapped;  /* connects redirected by SELECTA_NET_MAP */
    unsigned connects;
} sel_net_stats;

/* Connect to host:port with the guard and map applied. Returns a handle >= 0
 * or a negative SEL_NET_E_* code. connect_ms bounds the connect. */
int sel_net_open(const char* host, unsigned port, unsigned connect_ms);
/* Send all len bytes within timeout_ms. Returns 0 or a negative code. */
int sel_net_send(int h, const void* data, size_t len, unsigned timeout_ms);
/* Receive up to cap bytes. Returns the count (> 0), SEL_NET_E_TIMEOUT after
 * timeout_ms, SEL_NET_E_CLOSED on peer shutdown, or another negative code. */
int sel_net_recv(int h, void* buf, size_t cap, unsigned timeout_ms);
void sel_net_close(int h);
void sel_net_stats_get(sel_net_stats* out);
/* Milliseconds on a monotonic clock (for deadlines). */
unsigned long sel_net_now_ms(void);
/* SELECTA_CA_FILE or NULL. */
const char* sel_net_ca_file(void);
/* The OS trust store as one malloc'd, NUL-terminated PEM text (macOS: Keychain export through
 * `security`, Linux: the distribution bundle), or NULL when none is found or the OS has none
 * (the Amiga). The caller frees it. *len is the text length without the NUL. */
char* sel_net_os_ca_bundle(size_t* len);

/* Pure helpers shared by both back ends (src/library/net/netpolicy.c). */
/* 1 when host is allowed by the allow list (list NULL or empty allows all). */
int sel_net_allowed(const char* allow_list, const char* host);
/* Look up host in a map string; on a hit fill addr/port and return 1. */
int sel_net_map_lookup(const char* map, const char* host, char* addr, size_t addr_cap, unsigned* port);

#endif

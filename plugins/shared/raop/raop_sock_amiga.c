/* Amiga bsdsocket backend of raop_sock.h (I1004). Every call goes through the one Amiga socket layer
 * (src/core/amiga/net.c; TCP is sel_net_*, UDP and addresses are net_udp.h), so this file holds no socket code. */
#include "raop_sock.h"

#include <proto/dos.h>

#include "net_udp.h"
#include "../../../src/library/net/net.h"

int raop_tcp_open(const char *host, unsigned port, unsigned timeout_ms) {
    int h = sel_net_open(host, port, timeout_ms);
    return h < 0 ? -1 : h;
}

int raop_tcp_send(int h, const void *data, size_t len, unsigned timeout_ms) {
    return sel_net_send(h, data, len, timeout_ms) == 0 ? 0 : -1;
}

int raop_tcp_recv(int h, void *buf, size_t cap, unsigned timeout_ms) {
    int n = sel_net_recv(h, buf, cap, timeout_ms);
    if (n == SEL_NET_E_TIMEOUT) return 0;
    return n > 0 ? n : -1;
}

int raop_tcp_local_ip(int h, char *out, size_t cap) { return sel_net_local_ip(h, out, cap) == 0 ? 0 : -1; }

void raop_tcp_close(int h) { sel_net_close(h); }

int raop_udp_open(void) {
    int h = sel_net_udp_open();
    return h < 0 ? -1 : h;
}

int raop_udp_send(int h, const char *host, unsigned port, const void *data, size_t len) {
    return sel_net_udp_send(h, host, port, data, len) == 0 ? 0 : -1;
}

void raop_udp_close(int h) { sel_net_close(h); }

int raop_udp_bind(unsigned *port) {
    int h = sel_net_udp_bind(port);
    return h < 0 ? -1 : h;
}

int raop_udp_recv(int h, void *buf, size_t cap, unsigned timeout_ms, char *from, size_t from_cap, unsigned *from_port) {
    int n = sel_net_udp_recv(h, buf, cap, timeout_ms, from, from_cap, from_port);
    if (n == SEL_NET_E_TIMEOUT) return 0;
    return n > 0 ? n : -1;
}

/* The Amiga clock is DateStamp (local time, seconds since 1978-01-01; there is no UTC offset without the locale). RAOP
 * uses it for sync-packet timestamps only as a monotonic reference, so the epoch shift to 1900 is exact and the zone
 * offset is not corrected. */
void raop_ntp_now(uint32_t *sec, uint32_t *frac) {
    struct DateStamp ds;
    uint32_t secs;
    DateStamp(&ds);
    secs = (uint32_t)ds.ds_Days * 86400u + (uint32_t)ds.ds_Minute * 60u + (uint32_t)ds.ds_Tick / 50u;
    *sec = secs + 2461449600u; /* 1900 to 1978 */
    *frac = ((uint32_t)ds.ds_Tick % 50u) * 85899346u /* 2^32 / 50, integer: no float in this file */;
}

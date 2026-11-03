/* UDP and address calls of the Amiga socket layer (src/core/amiga/net.c), used by the UPnP and RAOP socket shims
 * (plugins/shared/upnp/upnp_sock_amiga.c, plugins/shared/raop/raop_sock_amiga.c; I1004). Handles come from the same
 * table as sel_net_open, so sel_net_close closes them. Addresses are dotted quads; the calls return a handle / count
 * >= 0 or a negative SEL_NET_E_* code. The host has its own POSIX shims, so these exist on the Amiga only. */
#ifndef SELECTA_CORE_AMIGA_NET_UDP_H
#define SELECTA_CORE_AMIGA_NET_UDP_H
#include <stddef.h>

int sel_net_udp_open(void);
/* Bound to an ephemeral local port, written to *port. */
int sel_net_udp_bind(unsigned* port);
/* 0 on success. */
int sel_net_udp_send(int h, const char* ip, unsigned port, const void* data, size_t len);
/* Datagram length (0 allowed), SEL_NET_E_TIMEOUT after timeout_ms. `from` / `from_port` may be NULL. */
int sel_net_udp_recv(int h, void* buf, size_t cap, unsigned timeout_ms, char* from, size_t from_cap, unsigned* from_port);
/* Sets the multicast TTL and joins group (IP_ADD_MEMBERSHIP). Join failure returns an error but the socket still sends. */
int sel_net_udp_multicast(int h, const char* group, unsigned ttl);
/* Local address of the socket as dotted text (TCP: the interface the connection uses). */
int sel_net_local_ip(int h, char* out, size_t cap);

#endif

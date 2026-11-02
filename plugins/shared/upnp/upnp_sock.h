/* The only OS surface of the UPnP library: one UDP socket for SSDP (I894a1). The POSIX backend is
 * upnp_sock_posix.c; the Amiga bsdsocket.library backend (I897) implements the same four calls, so upnp.c has no
 * OS include. Addresses travel as dotted-quad strings so the callers need no sockaddr. */
#ifndef SELECTA_SHARED_UPNP_SOCK_H
#define SELECTA_SHARED_UPNP_SOCK_H

#include <stddef.h>

/* An open UDP socket, or NULL. */
typedef struct sel_usock sel_usock;

sel_usock* sel_usock_open(void);

/* Sends one datagram to ip:port. 1 on success. */
int sel_usock_send(sel_usock* s, const char* ip, unsigned port, const void* data, size_t len);

/* Waits up to timeout_ms for a datagram. Returns its length (0 for an empty one), -1 on timeout or error. `from`
 * (at least 16 bytes) receives the sender's dotted-quad address. */
long sel_usock_recv(sel_usock* s, void* buf, size_t cap, unsigned timeout_ms, char* from);

void sel_usock_close(sel_usock* s);

#endif

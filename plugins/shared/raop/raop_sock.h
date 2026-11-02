/* Socket shim for the RAOP sender (I892c2). The sender core (raop_session.c) calls only these functions, so the Amiga
 * bsdsocket.library backend (I895) is one more source file with the same signatures. Handles are small ints, negative on
 * error. Nothing here allocates. The POSIX backend is raop_sock_posix.c. */
#ifndef SELECTA_RAOP_SOCK_H
#define SELECTA_RAOP_SOCK_H

#include <stddef.h>

/* TCP client connect to host:port (numeric or name), bounded by timeout_ms. Returns a handle or -1. */
int raop_tcp_open(const char *host, unsigned port, unsigned timeout_ms);
/* Sends all len bytes within timeout_ms. Returns 0 or -1. */
int raop_tcp_send(int h, const void *data, size_t len, unsigned timeout_ms);
/* Receives up to cap bytes. Returns the count (> 0), 0 on timeout, -1 on error or peer close. */
int raop_tcp_recv(int h, void *buf, size_t cap, unsigned timeout_ms);
/* Writes the local address of the connection as dotted text. Returns 0 or -1. */
int raop_tcp_local_ip(int h, char *out, size_t cap);
void raop_tcp_close(int h);

/* UDP socket for sending. Returns a handle or -1. */
int raop_udp_open(void);
/* Sends one datagram to host:port (dotted address). Returns 0 or -1. */
int raop_udp_send(int h, const char *host, unsigned port, const void *data, size_t len);
void raop_udp_close(int h);

#endif

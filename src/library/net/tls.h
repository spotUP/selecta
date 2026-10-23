/* TLS stream wrapper over the socket table (plan 2026-10-25-m3-library.md, I154, D-M3-4).
 * Desktop builds link the vendored mbedTLS (third_party/mbedtls); the Amiga build is plain
 * HTTP and compiles the stubs (sel_tls_open answers SEL_TLS_E_UNAVAILABLE).
 *
 * Trust anchors: the file named by SELECTA_CA_FILE when set, otherwise the OS store
 * (sel_net_os_ca_bundle). The server certificate chain and the host name are always verified.
 *
 * sel_stream_* take the socket-table handle and route through TLS when sel_tls_open succeeded
 * on it, so the HTTP client has one code path for both schemes. */
#ifndef SELECTA_LIBRARY_NET_TLS_H
#define SELECTA_LIBRARY_NET_TLS_H

#include <stddef.h>

enum {
    SEL_TLS_OK = 0,
    SEL_TLS_E_UNAVAILABLE = -1, /* built without TLS (Amiga) */
    SEL_TLS_E_VERIFY = -2,      /* certificate chain or host name rejected */
    SEL_TLS_E_HANDSHAKE = -3,
    SEL_TLS_E_TIMEOUT = -4,
    SEL_TLS_E_INIT = -5         /* RNG or configuration setup failed */
};

/* Handshake on the open socket handle h, verifying host, within timeout_ms. */
int sel_tls_open(int h, const char* host, unsigned timeout_ms);
/* Completed handshakes since start (the tls_handshakes counter). */
unsigned sel_tls_handshakes(void);

/* Same contracts as sel_net_send / sel_net_recv / sel_net_close; TLS when h was upgraded. */
int sel_stream_send(int h, const void* data, size_t len, unsigned timeout_ms);
int sel_stream_recv(int h, void* buf, size_t cap, unsigned timeout_ms);
void sel_stream_close(int h);

#endif

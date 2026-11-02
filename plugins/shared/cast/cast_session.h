/* CASTV2 control session over the socket table and TLS (I892e1). Sockets: src/library/net/net.h (the one socket layer,
 * POSIX on the host, bsdsocket on the Amiga); TLS: src/library/net/tls.h. The Chromecast certificate is self-signed, so
 * the handshake runs without chain verification (sel_tls_open_insecure). On the Amiga the TLS half waits on I899:
 * sel_cast_open answers SEL_CAST_E_TLS there. */
#ifndef SELECTA_SHARED_CAST_SESSION_H
#define SELECTA_SHARED_CAST_SESSION_H

#include "cast/cast.h"

enum {
    SEL_CAST_OK = 0,
    SEL_CAST_E_CONNECT = -1,
    SEL_CAST_E_TLS = -2,
    SEL_CAST_E_IO = -3,      /* send failed, peer closed, or a frame was malformed */
    SEL_CAST_E_TIMEOUT = -4
};

#define SEL_CAST_SENDER "sender-0"
#define SEL_CAST_RECEIVER "receiver-0"

typedef struct sel_cast {
    int h; /* socket table handle, -1 when closed */
    long next_request;
    unsigned pings_answered; /* PING frames from the receiver that were answered with PONG */
    unsigned pongs;          /* PONG frames received */
    sel_cast_status status;  /* last RECEIVER_STATUS */
    int have_status;
} sel_cast;

/* Connects to host:port (8009), TLS handshake, and sends CONNECT to receiver-0. */
int sel_cast_open(sel_cast* c, const char* host, unsigned port, unsigned timeout_ms);

/* Sends one JSON payload on a namespace to dest (NULL = receiver-0). */
int sel_cast_send(sel_cast* c, const char* dest, const char* ns, const char* json);

/* Reads one frame within timeout_ms and handles it: a PING is answered with PONG, a PONG is counted, a
 * RECEIVER_STATUS is parsed into c->status. *msg receives the frame (may be NULL). Returns SEL_CAST_OK, or an error. */
int sel_cast_pump(sel_cast* c, unsigned timeout_ms, sel_cast_msg* msg);

/* Heartbeat round: send PING and pump until the PONG arrives (other frames are handled on the way). */
int sel_cast_heartbeat(sel_cast* c, unsigned timeout_ms);

/* Sends GET_STATUS on the receiver namespace and pumps until a RECEIVER_STATUS arrives. */
int sel_cast_get_status(sel_cast* c, unsigned timeout_ms);

void sel_cast_close(sel_cast* c);

#endif

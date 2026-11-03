/* CASTV2 control session over the socket table and TLS (I892e1). Sockets: src/library/net/net.h (the one socket layer,
 * POSIX on the host, bsdsocket on the Amiga); TLS: src/library/net/tls.h. The Chromecast certificate is self-signed, so
 * the handshake runs without chain verification (sel_tls_open_insecure). On the Amiga the TLS half waits on I899:
 * sel_cast_open answers SEL_CAST_E_TLS there. */
#ifndef SELECTA_SHARED_CAST_SESSION_H
#define SELECTA_SHARED_CAST_SESSION_H

#include "cast/cast.h"
#include "cast/cast_http.h"

enum {
    SEL_CAST_OK = 0,
    SEL_CAST_E_CONNECT = -1,
    SEL_CAST_E_TLS = -2,
    SEL_CAST_E_IO = -3,      /* send failed, peer closed, or a frame was malformed */
    SEL_CAST_E_TIMEOUT = -4
};

#define SEL_CAST_SENDER "sender-0"
#define SEL_CAST_RECEIVER "receiver-0"
#define SEL_CAST_APP_DEFAULT_MEDIA "CC1AD845" /* Default Media Receiver */

typedef struct sel_cast {
    int h; /* socket table handle, -1 when closed */
    long next_request;
    unsigned pings_answered; /* PING frames from the receiver that were answered with PONG */
    unsigned pongs;          /* PONG frames received */
    sel_cast_status status;  /* last RECEIVER_STATUS */
    int have_status;
    char transport[SEL_CAST_ID_MAX]; /* transportId of the launched application, "" before sel_cast_launch */
    sel_cast_status media;           /* last MEDIA_STATUS */
    unsigned media_updates;          /* MEDIA_STATUS frames received */
    unsigned loads;                  /* LOAD requests the receiver answered with a media session (the cast_loads stat) */
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

/* LAUNCH on the receiver namespace (app_id, e.g. SEL_CAST_APP_DEFAULT_MEDIA), waits for the RECEIVER_STATUS that
 * lists the application with its transportId, stores it in c->transport and sends CONNECT to it. */
int sel_cast_launch(sel_cast* c, const char* app_id, unsigned timeout_ms);

/* LOAD of `url` (content_type e.g. "audio/mpeg") on the media namespace, autoplay from position 0. While waiting for
 * the MEDIA_STATUS that carries the new mediaSessionId, `http` (may be NULL) is polled so the receiver can fetch the
 * file from this process. Needs sel_cast_launch first. */
int sel_cast_load(sel_cast* c, sel_cast_http* http, const char* url, const char* content_type, unsigned timeout_ms);

/* Media controls; each sends the request and waits for the MEDIA_STATUS answer (read c->media). */
int sel_cast_play(sel_cast* c, unsigned timeout_ms);
int sel_cast_pause(sel_cast* c, unsigned timeout_ms);
int sel_cast_seek(sel_cast* c, double seconds, unsigned timeout_ms);
/* GET_STATUS on the media namespace. */
int sel_cast_media_status(sel_cast* c, unsigned timeout_ms);
/* SET_VOLUME on the receiver namespace (level 0..1); waits for the RECEIVER_STATUS (read c->status.volume_level). */
int sel_cast_set_volume(sel_cast* c, double level, unsigned timeout_ms);

void sel_cast_close(sel_cast* c);

#endif

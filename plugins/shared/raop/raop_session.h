/* RAOP (AirPlay 1) sender session (I892c2): RTSP over TCP and RTP audio over UDP on top of the pure part (raop.h) and the
 * socket shim (raop_sock.h). No malloc, no float: the session struct holds every buffer. The caller owns pacing (one
 * packet per 352 frames, about 7.98 ms at 44.1 kHz). Flow: thoughts/shared/research/2026-11-01-airplay-chromecast.md,
 * section 1. Control-port sync, timing replies and resends are I892c3. */
#ifndef SELECTA_RAOP_SESSION_H
#define SELECTA_RAOP_SESSION_H

#include "raop.h"

typedef struct raop_stats {
    unsigned raop_packets_sent; /* RTP audio packets handed to the socket */
} raop_stats;

typedef struct raop_session {
    int tcp, udp;
    char host[64];
    char client_ip[48];
    char url[96];
    char sid[24]; /* RTSP Session header value */
    unsigned audio_port, control_port, timing_port;
    unsigned latency; /* Audio-Latency in frames from RECORD, 0 when absent */
    unsigned cseq;
    uint16_t seq;
    uint32_t timestamp, ssrc;
    uint8_t key[RAOP_AES_KEY_BYTES], iv[RAOP_AES_IV_BYTES];
    int started;    /* first packet after RECORD or FLUSH carries the RTP marker bit */
    char buf[2048]; /* RTSP request and response text */
    char sdp[RAOP_SDP_MAX];
    uint8_t pkt[12 + RAOP_ALAC_BYTES];
} raop_session;

/* Connects to host:rtsp_port and runs OPTIONS, ANNOUNCE, SETUP, RECORD. rng seeds key, IV, session id, SSRC and the OAEP
 * padding. Returns 0 or a negative step code (-1 connect, -2 OPTIONS, -3 ANNOUNCE, -4 SETUP, -5 RECORD). */
int raop_session_open(raop_session *s, const char *host, unsigned rtsp_port, raop_rng_fn rng, void *rng_ctx);
/* Encodes and sends one packet from RAOP_FRAMES interleaved stereo frames. Returns 0 or -1. */
int raop_session_send(raop_session *s, const int16_t *pcm);
/* Volume in tenths of a dB, -300 .. 0, or -1440 for mute (integer only, no float on the 020). */
int raop_session_set_volume(raop_session *s, int tenth_db);
int raop_session_flush(raop_session *s);
/* TEARDOWN, then closes both sockets. */
void raop_session_close(raop_session *s);

void raop_stats_get(raop_stats *out);

#endif

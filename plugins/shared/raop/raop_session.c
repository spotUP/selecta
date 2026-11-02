#include "raop_session.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "raop_sock.h"

#define IO_MS 3000

static raop_stats g_stats;

void raop_stats_get(raop_stats *out) { *out = g_stats; }

/* Sends one request and reads the response headers into s->buf. Returns the status code, or -1. */
static int rtsp_call(raop_session *s, const char *method, const char *extra, const char *body, const char *ctype) {
    size_t blen = body ? strlen(body) : 0, have = 0;
    int n, got = 0;

    n = snprintf(s->buf, sizeof s->buf, "%s %s RTSP/1.0\r\nCSeq: %u\r\nUser-Agent: Selecta\r\n", method, s->url, ++s->cseq);
    if (s->sid[0]) n += snprintf(s->buf + n, sizeof s->buf - (size_t)n, "Session: %s\r\n", s->sid);
    if (extra) n += snprintf(s->buf + n, sizeof s->buf - (size_t)n, "%s", extra);
    if (body)
        n += snprintf(s->buf + n, sizeof s->buf - (size_t)n, "Content-Type: %s\r\nContent-Length: %u\r\n", ctype, (unsigned)blen);
    if (n < 0 || (size_t)n + 2 >= sizeof s->buf) return -1;
    s->buf[n++] = '\r';
    s->buf[n++] = '\n';
    if (raop_tcp_send(s->tcp, s->buf, (size_t)n, IO_MS) != 0) return -1;
    if (body && raop_tcp_send(s->tcp, body, blen, IO_MS) != 0) return -1;

    while (have < sizeof s->buf - 1) {
        int r = raop_tcp_recv(s->tcp, s->buf + have, sizeof s->buf - 1 - have, IO_MS);
        if (r <= 0) return -1;
        have += (size_t)r;
        s->buf[have] = 0;
        if (strstr(s->buf, "\r\n\r\n")) {
            got = 1;
            break;
        }
    }
    if (!got || strncmp(s->buf, "RTSP/1.0 ", 9) != 0) return -1;
    /* The receivers we target send no response body; headers are all that is read. */
    return atoi(s->buf + 9);
}

static const char *header(const raop_session *s, const char *name) {
    const char *p = s->buf;
    size_t nl = strlen(name);
    while ((p = strstr(p, "\r\n")) != NULL) {
        p += 2;
        if (strncmp(p, name, nl) == 0 && p[nl] == ':') {
            p += nl + 1;
            while (*p == ' ') p++;
            return p;
        }
    }
    return NULL;
}

static unsigned field_uint(const char *p, const char *key) {
    const char *q = strstr(p, key);
    return q ? (unsigned)strtoul(q + strlen(key), NULL, 10) : 0;
}

static int fail_open(raop_session *s, int code) {
    raop_udp_close(s->udp);
    raop_tcp_close(s->tcp);
    s->udp = s->tcp = -1;
    return code;
}

int raop_session_open(raop_session *s, const char *host, unsigned rtsp_port, raop_rng_fn rng, void *rng_ctx) {
    raop_pubkey pk;
    uint8_t wrapped[RAOP_RSA_BYTES], rnd[10];
    char extra[160];
    uint32_t sid_num;
    const char *h;

    memset(s, 0, sizeof *s);
    s->tcp = s->udp = -1;
    snprintf(s->host, sizeof s->host, "%s", host);
    if (rng(rng_ctx, rnd, sizeof rnd) != 0) return -1;
    sid_num = (uint32_t)rnd[0] << 24 | (uint32_t)rnd[1] << 16 | (uint32_t)rnd[2] << 8 | rnd[3];
    s->ssrc = (uint32_t)rnd[4] << 24 | (uint32_t)rnd[5] << 16 | (uint32_t)rnd[6] << 8 | rnd[7];
    s->seq = (uint16_t)(rnd[8] << 8 | rnd[9]);
    s->timestamp = 0;
    if (rng(rng_ctx, s->key, sizeof s->key) != 0 || rng(rng_ctx, s->iv, sizeof s->iv) != 0) return -1;

    s->tcp = raop_tcp_open(host, rtsp_port, IO_MS);
    if (s->tcp < 0) return -1;
    if (raop_tcp_local_ip(s->tcp, s->client_ip, sizeof s->client_ip) != 0) return fail_open(s, -1);
    snprintf(s->url, sizeof s->url, "rtsp://%s/%lu", s->client_ip, (unsigned long)sid_num);

    if (rtsp_call(s, "OPTIONS", "Apple-Challenge: U2VsZWN0YQ\r\n", NULL, NULL) != 200) return fail_open(s, -2);

    if (raop_airport_pubkey(&pk) != 0 || raop_rsa_wrap_key(&pk, s->key, rng, rng_ctx, wrapped) != 0) return fail_open(s, -3);
    if (raop_sdp_build(s->sdp, sizeof s->sdp, sid_num, s->client_ip, host, wrapped, s->iv) < 0) return fail_open(s, -3);
    if (rtsp_call(s, "ANNOUNCE", NULL, s->sdp, "application/sdp") != 200) return fail_open(s, -3);

    s->udp = raop_udp_open();
    if (s->udp < 0) return fail_open(s, -4);
    /* Control and timing sockets belong to I892c3; their ports are announced as 0 until then. */
    if (rtsp_call(s, "SETUP", "Transport: RTP/AVP/UDP;unicast;interleaved=0-1;mode=record;control_port=0;timing_port=0\r\n",
                  NULL, NULL) != 200)
        return fail_open(s, -4);
    h = header(s, "Session");
    if (h) {
        size_t i = 0;
        while (h[i] && h[i] != '\r' && h[i] != ';' && i < sizeof s->sid - 1) {
            s->sid[i] = h[i];
            i++;
        }
        s->sid[i] = 0;
    }
    h = header(s, "Transport");
    if (!h) return fail_open(s, -4);
    s->audio_port = field_uint(h, "server_port=");
    s->control_port = field_uint(h, "control_port=");
    s->timing_port = field_uint(h, "timing_port=");
    if (!s->audio_port) return fail_open(s, -4);

    snprintf(extra, sizeof extra, "Range: npt=0-\r\nRTP-Info: seq=%u;rtptime=%lu\r\n", (unsigned)s->seq,
             (unsigned long)s->timestamp);
    if (rtsp_call(s, "RECORD", extra, NULL, NULL) != 200) return fail_open(s, -5);
    h = header(s, "Audio-Latency");
    s->latency = h ? (unsigned)strtoul(h, NULL, 10) : 0;
    return 0;
}

int raop_session_send(raop_session *s, const int16_t *pcm) {
    uint8_t *p = s->pkt;
    if (s->udp < 0) return -1;
    p[0] = 0x80;
    p[1] = s->started ? 0x60 : 0xE0; /* payload type 0x60, marker bit on the first packet after RECORD or FLUSH */
    p[2] = (uint8_t)(s->seq >> 8);
    p[3] = (uint8_t)s->seq;
    p[4] = (uint8_t)(s->timestamp >> 24);
    p[5] = (uint8_t)(s->timestamp >> 16);
    p[6] = (uint8_t)(s->timestamp >> 8);
    p[7] = (uint8_t)s->timestamp;
    p[8] = (uint8_t)(s->ssrc >> 24);
    p[9] = (uint8_t)(s->ssrc >> 16);
    p[10] = (uint8_t)(s->ssrc >> 8);
    p[11] = (uint8_t)s->ssrc;
    raop_alac_pack(pcm, p + 12);
    if (raop_aes_encrypt_packet(s->key, s->iv, p + 12, RAOP_ALAC_BYTES, p + 12) != 0) return -1;
    if (raop_udp_send(s->udp, s->host, s->audio_port, p, sizeof s->pkt) != 0) return -1;
    s->started = 1;
    s->seq++;
    s->timestamp += RAOP_FRAMES;
    g_stats.raop_packets_sent++;
    return 0;
}

int raop_session_set_volume(raop_session *s, int tenth_db) {
    char body[48];
    int neg = tenth_db < 0, v = neg ? -tenth_db : tenth_db;
    snprintf(body, sizeof body, "volume: %s%d.%d00000\r\n", neg ? "-" : "", v / 10, v % 10);
    return rtsp_call(s, "SET_PARAMETER", NULL, body, "text/parameters") == 200 ? 0 : -1;
}

int raop_session_flush(raop_session *s) {
    char extra[96];
    int r;
    snprintf(extra, sizeof extra, "RTP-Info: seq=%u;rtptime=%lu\r\n", (unsigned)s->seq, (unsigned long)s->timestamp);
    r = rtsp_call(s, "FLUSH", extra, NULL, NULL);
    s->started = 0;
    return r == 200 ? 0 : -1;
}

void raop_session_close(raop_session *s) {
    if (s->tcp >= 0) rtsp_call(s, "TEARDOWN", NULL, NULL, NULL);
    raop_udp_close(s->udp);
    raop_tcp_close(s->tcp);
    s->udp = s->tcp = -1;
}

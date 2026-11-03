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

static void close_all(raop_session *s) {
    raop_udp_close(s->udp);
    raop_udp_close(s->ctl);
    raop_udp_close(s->tim);
    raop_tcp_close(s->tcp);
    s->udp = s->ctl = s->tim = s->tcp = -1;
}

static int fail_open(raop_session *s, int code) {
    close_all(s);
    return code;
}

int raop_session_open(raop_session *s, const char *host, unsigned rtsp_port, raop_rng_fn rng, void *rng_ctx) {
    raop_pubkey pk;
    uint8_t wrapped[RAOP_RSA_BYTES], rnd[10];
    char extra[200];
    uint32_t sid_num;
    const char *h;

    memset(s, 0, sizeof *s);
    s->tcp = s->udp = s->ctl = s->tim = -1;
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
    s->ctl = raop_udp_bind(&s->local_ctl);
    s->tim = raop_udp_bind(&s->local_tim);
    if (s->ctl < 0 || s->tim < 0) return fail_open(s, -4);
    snprintf(extra, sizeof extra,
             "Transport: RTP/AVP/UDP;unicast;interleaved=0-1;mode=record;control_port=%u;timing_port=%u\r\n", s->local_ctl,
             s->local_tim);
    if (rtsp_call(s, "SETUP", extra, NULL, NULL) != 200) return fail_open(s, -4);
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

static void put32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

static uint16_t get16(const uint8_t *p) { return (uint16_t)(p[0] << 8 | p[1]); }

/* Sync packet (type 0xD4, 20 bytes): RTP time of what plays now (stream time minus latency), NTP time, stream time.
 * The first one after RECORD or FLUSH carries the extension bit. */
static void send_sync(raop_session *s) {
    uint8_t p[20];
    uint32_t sec, frac;
    raop_ntp_now(&sec, &frac);
    p[0] = s->started ? 0x80 : 0x90;
    p[1] = 0xD4;
    p[2] = 0;
    p[3] = 7;
    put32(p + 4, s->timestamp - s->latency);
    put32(p + 8, sec);
    put32(p + 12, frac);
    put32(p + 16, s->timestamp);
    if (raop_udp_send(s->ctl, s->host, s->control_port, p, sizeof p) == 0) g_stats.raop_syncs++;
    s->last_sync = s->timestamp;
}

void raop_session_service(raop_session *s) {
    uint8_t in[64], out[4 + RAOP_PKT_BYTES];
    char from[48];
    unsigned port;
    int n;

    while (s->tim >= 0 && (n = raop_udp_recv(s->tim, in, sizeof in, 0, from, sizeof from, &port)) > 0) {
        uint32_t sec, frac;
        if (n < 32 || in[1] != 0xD2) continue;
        raop_ntp_now(&sec, &frac);
        out[0] = 0x80;
        out[1] = 0xD3;
        out[2] = 0;
        out[3] = 7;
        memset(out + 4, 0, 4);
        memcpy(out + 8, in + 24, 8); /* reference: the request's send time */
        put32(out + 16, sec);        /* received */
        put32(out + 20, frac);
        put32(out + 24, sec);        /* sent */
        put32(out + 28, frac);
        if (raop_udp_send(s->tim, from, port, out, 32) == 0) g_stats.raop_timing_replies++;
    }
    while (s->ctl >= 0 && (n = raop_udp_recv(s->ctl, in, sizeof in, 0, from, sizeof from, &port)) > 0) {
        unsigned first, count, i;
        if (n < 8 || in[1] != 0xD5) continue;
        first = get16(in + 4);
        count = get16(in + 6);
        for (i = 0; i < count && i < RAOP_RING; i++) {
            uint16_t seq = (uint16_t)(first + i);
            const uint8_t *pk = s->ring[seq % RAOP_RING];
            /* Slot holds this sequence number only if nothing newer overwrote it. */
            if (get16(pk + 2) != seq || pk[0] != 0x80) continue;
            out[0] = 0x80;
            out[1] = 0xD6;
            out[2] = 0;
            out[3] = 1;
            memcpy(out + 4, pk, RAOP_PKT_BYTES);
            if (raop_udp_send(s->ctl, from, port, out, sizeof out) == 0) g_stats.raop_resends++;
        }
    }
}

unsigned raop_session_latency_ms(const raop_session *s) {
    unsigned frames = s->latency ? s->latency : 88200u;
    return (frames / 441u) * 10u + (frames % 441u) * 10u / 441u;
}

int raop_session_send(raop_session *s, const int16_t *pcm) {
    uint8_t *p = s->pkt;
    if (s->udp < 0) return -1;
    raop_session_service(s);
    if (!s->started || s->timestamp - s->last_sync >= RAOP_SYNC_FRAMES) send_sync(s);
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
    memcpy(s->ring[s->seq % RAOP_RING], p, sizeof s->pkt);
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
    close_all(s);
}

/* CASTV2 control session; see cast_session.h. */
#include "cast/cast_session.h"

#include <stdio.h>
#include <string.h>

#include "net/net.h"
#include "net/tls.h"

int sel_cast_open(sel_cast* c, const char* host, unsigned port, unsigned timeout_ms) {
    int rc;

    memset(c, 0, sizeof *c);
    c->h = -1;
    c->next_request = 1;
    c->h = sel_net_open(host, port, timeout_ms);
    if (c->h < 0) {
        c->h = -1;
        return SEL_CAST_E_CONNECT;
    }
    if (sel_tls_open_insecure(c->h, host, timeout_ms) != SEL_TLS_OK) {
        sel_net_close(c->h);
        c->h = -1;
        return SEL_CAST_E_TLS;
    }
    rc = sel_cast_send(c, NULL, SEL_CAST_NS_CONNECTION, "{\"type\":\"CONNECT\"}");
    if (rc != SEL_CAST_OK) {
        sel_cast_close(c);
    }
    return rc;
}

int sel_cast_send(sel_cast* c, const char* dest, const char* ns, const char* json) {
    unsigned char frame[SEL_CAST_FRAME_MAX + 4u];
    size_t n = sel_cast_encode(frame, sizeof frame, SEL_CAST_SENDER, dest != NULL ? dest : SEL_CAST_RECEIVER, ns, json);

    if (n == 0 || c->h < 0) {
        return SEL_CAST_E_IO;
    }
    return sel_stream_send(c->h, frame, n, 5000u) == 0 ? SEL_CAST_OK : SEL_CAST_E_IO;
}

/* Reads exactly len bytes within the overall timeout. */
static int read_full(int h, unsigned char* buf, size_t len, unsigned timeout_ms) {
    size_t got = 0;
    unsigned long end = sel_net_now_ms() + timeout_ms;

    while (got < len) {
        unsigned long now = sel_net_now_ms();
        int n;

        if (now >= end) {
            return SEL_CAST_E_TIMEOUT;
        }
        n = sel_stream_recv(h, buf + got, len - got, (unsigned)(end - now));
        if (n == SEL_NET_E_TIMEOUT) {
            return SEL_CAST_E_TIMEOUT;
        }
        if (n <= 0) {
            return SEL_CAST_E_IO;
        }
        got += (size_t)n;
    }
    return SEL_CAST_OK;
}

int sel_cast_pump(sel_cast* c, unsigned timeout_ms, sel_cast_msg* msg) {
    unsigned char hdr[4];
    unsigned char body[SEL_CAST_FRAME_MAX];
    sel_cast_msg local;
    sel_cast_status st;
    unsigned long len;
    int rc;

    if (msg == NULL) {
        msg = &local;
    }
    if (c->h < 0) {
        return SEL_CAST_E_IO;
    }
    rc = read_full(c->h, hdr, 4u, timeout_ms);
    if (rc != SEL_CAST_OK) {
        return rc;
    }
    len = sel_cast_frame_len(hdr);
    if (len == 0 || len > sizeof body) {
        return SEL_CAST_E_IO;
    }
    rc = read_full(c->h, body, len, 5000u);
    if (rc != SEL_CAST_OK || sel_cast_decode(body, len, msg) != 0) {
        return rc != SEL_CAST_OK ? rc : SEL_CAST_E_IO;
    }
    if (msg->binary || sel_cast_parse_status(msg->payload, &st) != 0) {
        return SEL_CAST_OK; /* not a message this layer reads */
    }
    if (strcmp(msg->ns, SEL_CAST_NS_HEARTBEAT) == 0) {
        if (strcmp(st.type, "PING") == 0) {
            c->pings_answered++;
            return sel_cast_send(c, msg->source, SEL_CAST_NS_HEARTBEAT, "{\"type\":\"PONG\"}");
        }
        if (strcmp(st.type, "PONG") == 0) {
            c->pongs++;
        }
    } else if (strcmp(msg->ns, SEL_CAST_NS_RECEIVER) == 0 && strcmp(st.type, "RECEIVER_STATUS") == 0) {
        c->status = st;
        c->have_status = 1;
    }
    return SEL_CAST_OK;
}

int sel_cast_heartbeat(sel_cast* c, unsigned timeout_ms) {
    unsigned before = c->pongs;
    unsigned long end = sel_net_now_ms() + timeout_ms;
    int rc = sel_cast_send(c, NULL, SEL_CAST_NS_HEARTBEAT, "{\"type\":\"PING\"}");

    while (rc == SEL_CAST_OK && c->pongs == before) {
        unsigned long now = sel_net_now_ms();

        if (now >= end) {
            return SEL_CAST_E_TIMEOUT;
        }
        rc = sel_cast_pump(c, (unsigned)(end - now), NULL);
    }
    return rc;
}

int sel_cast_get_status(sel_cast* c, unsigned timeout_ms) {
    char json[64];
    unsigned long end = sel_net_now_ms() + timeout_ms;
    int rc;

    c->have_status = 0;
    snprintf(json, sizeof json, "{\"type\":\"GET_STATUS\",\"requestId\":%ld}", c->next_request++);
    rc = sel_cast_send(c, NULL, SEL_CAST_NS_RECEIVER, json);
    while (rc == SEL_CAST_OK && !c->have_status) {
        unsigned long now = sel_net_now_ms();

        if (now >= end) {
            return SEL_CAST_E_TIMEOUT;
        }
        rc = sel_cast_pump(c, (unsigned)(end - now), NULL);
    }
    return rc;
}

void sel_cast_close(sel_cast* c) {
    if (c->h >= 0) {
        sel_stream_close(c->h);
        c->h = -1;
    }
}

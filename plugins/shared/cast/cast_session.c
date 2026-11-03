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
    } else if (strcmp(msg->ns, SEL_CAST_NS_MEDIA) == 0 && strcmp(st.type, "MEDIA_STATUS") == 0) {
        c->media = st;
        c->media_updates++;
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

/* Pumps (and serves http) until done(c) holds or the deadline passes. */
static int wait_for(sel_cast* c, sel_cast_http* http, unsigned timeout_ms, int (*done)(const sel_cast*, unsigned),
                    unsigned arg) {
    unsigned long end = sel_net_now_ms() + timeout_ms;
    int rc = SEL_CAST_OK;

    while (rc == SEL_CAST_OK && !done(c, arg)) {
        unsigned long now = sel_net_now_ms();
        unsigned left;

        if (now >= end) {
            return SEL_CAST_E_TIMEOUT;
        }
        left = (unsigned)(end - now);
        if (http != NULL && http->lh >= 0) {
            int served = sel_cast_http_poll(http, 20u < left ? 20u : left);

            if (served < 0) {
                return SEL_CAST_E_IO;
            }
            if (served > 0) {
                continue; /* the answer may have arrived while the file went out */
            }
            now = sel_net_now_ms();
            left = now >= end ? 1u : (unsigned)(end - now);
        }
        rc = sel_cast_pump(c, left < 100u ? left : 100u, NULL);
        if (rc == SEL_CAST_E_TIMEOUT) {
            rc = SEL_CAST_OK; /* an idle slice; the deadline above ends the wait */
        }
    }
    return rc;
}

static int media_updated(const sel_cast* c, unsigned before) {
    return c->media_updates != before;
}

static int status_seen(const sel_cast* c, unsigned unused) {
    (void)unused;
    return c->have_status;
}

int sel_cast_launch(sel_cast* c, const char* app_id, unsigned timeout_ms) {
    char json[128];
    int rc;

    c->have_status = 0;
    c->transport[0] = '\0';
    snprintf(json, sizeof json, "{\"type\":\"LAUNCH\",\"requestId\":%ld,\"appId\":\"%s\"}", c->next_request++, app_id);
    rc = sel_cast_send(c, NULL, SEL_CAST_NS_RECEIVER, json);
    /* The receiver may answer with a status that does not list the application yet; keep reading until it does. */
    for (;;) {
        unsigned long start = sel_net_now_ms();

        if (rc != SEL_CAST_OK) {
            return rc;
        }
        c->have_status = 0;
        rc = wait_for(c, NULL, timeout_ms, status_seen, 0);
        if (rc != SEL_CAST_OK) {
            return rc;
        }
        if (strcmp(c->status.app_id, app_id) == 0 && c->status.transport_id[0] != '\0') {
            break;
        }
        if (sel_net_now_ms() - start >= timeout_ms) {
            return SEL_CAST_E_TIMEOUT;
        }
    }
    snprintf(c->transport, sizeof c->transport, "%s", c->status.transport_id);
    return sel_cast_send(c, c->transport, SEL_CAST_NS_CONNECTION, "{\"type\":\"CONNECT\"}");
}

/* Sends a media-namespace request and waits for the MEDIA_STATUS that answers it. */
static int media_request(sel_cast* c, sel_cast_http* http, const char* json, unsigned timeout_ms) {
    unsigned before = c->media_updates;
    int rc;

    if (c->transport[0] == '\0') {
        return SEL_CAST_E_IO;
    }
    rc = sel_cast_send(c, c->transport, SEL_CAST_NS_MEDIA, json);
    return rc != SEL_CAST_OK ? rc : wait_for(c, http, timeout_ms, media_updated, before);
}

int sel_cast_load(sel_cast* c, sel_cast_http* http, const char* url, const char* content_type, unsigned timeout_ms) {
    char json[SEL_CAST_PAYLOAD_MAX];
    int n, rc;

    n = snprintf(json, sizeof json,
                 "{\"type\":\"LOAD\",\"requestId\":%ld,\"sessionId\":\"%s\",\"autoplay\":true,\"currentTime\":0,"
                 "\"media\":{\"contentId\":\"%s\",\"streamType\":\"BUFFERED\",\"contentType\":\"%s\"}}",
                 c->next_request++, c->status.session_id, url, content_type);
    if (n <= 0 || (size_t)n >= sizeof json) {
        return SEL_CAST_E_IO;
    }
    rc = media_request(c, http, json, timeout_ms);
    if (rc == SEL_CAST_OK && c->media.media_session_id != 0) {
        c->loads++;
    }
    return rc;
}

static int media_simple(sel_cast* c, const char* type, const char* extra, unsigned timeout_ms) {
    char json[160];

    snprintf(json, sizeof json, "{\"type\":\"%s\",\"requestId\":%ld,\"mediaSessionId\":%ld%s}", type, c->next_request++,
             c->media.media_session_id, extra);
    return media_request(c, NULL, json, timeout_ms);
}

int sel_cast_play(sel_cast* c, unsigned timeout_ms) {
    return media_simple(c, "PLAY", "", timeout_ms);
}

int sel_cast_pause(sel_cast* c, unsigned timeout_ms) {
    return media_simple(c, "PAUSE", "", timeout_ms);
}

int sel_cast_seek(sel_cast* c, double seconds, unsigned timeout_ms) {
    char extra[64];

    snprintf(extra, sizeof extra, ",\"currentTime\":%.3f", seconds);
    return media_simple(c, "SEEK", extra, timeout_ms);
}

int sel_cast_media_status(sel_cast* c, unsigned timeout_ms) {
    return media_simple(c, "GET_STATUS", "", timeout_ms);
}

int sel_cast_set_volume(sel_cast* c, double level, unsigned timeout_ms) {
    char json[128];
    int rc;

    c->have_status = 0;
    snprintf(json, sizeof json, "{\"type\":\"SET_VOLUME\",\"requestId\":%ld,\"volume\":{\"level\":%.3f}}",
             c->next_request++, level);
    rc = sel_cast_send(c, NULL, SEL_CAST_NS_RECEIVER, json);
    return rc != SEL_CAST_OK ? rc : wait_for(c, NULL, timeout_ms, status_seen, 0);
}

void sel_cast_close(sel_cast* c) {
    if (c->h >= 0) {
        sel_stream_close(c->h);
        c->h = -1;
    }
}

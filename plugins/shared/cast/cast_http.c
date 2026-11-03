/* HTTP file server for Chromecast playback; see cast_http.h. */
#include "cast/cast_http.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "entropy.h"
#include "net/net.h"

#define REQ_MAX 2048
#define CHUNK 4096

/* 128 random bits as hex from the shared entropy pool (/dev/urandom on desktop, the timing pool on the Amiga). The token
 * is a capability that keeps other LAN hosts from guessing the URL, so there is no fallback: -1 when the pool is not
 * ready and the server must not start (I899a). */
static int token_make(char* out) {
    static const char hex[] = "0123456789abcdef";
    unsigned char raw[SEL_CAST_HTTP_TOKEN_LEN / 2];
    unsigned i;

    if (sel_entropy_bytes(raw, sizeof raw) != 0) {
        return -1;
    }
    for (i = 0; i < sizeof raw; i++) {
        out[2 * i] = hex[raw[i] >> 4];
        out[2 * i + 1] = hex[raw[i] & 15];
    }
    out[SEL_CAST_HTTP_TOKEN_LEN] = '\0';
    return 0;
}

/* Constant-time compare of the request path against the token: no early exit on the first differing byte. */
static int token_equal(const char* got, const char* token) {
    unsigned char diff = 0;
    unsigned i;

    if (strlen(got) != SEL_CAST_HTTP_TOKEN_LEN) { /* the length is public */
        return 0;
    }
    for (i = 0; i < SEL_CAST_HTTP_TOKEN_LEN; i++) {
        diff |= (unsigned char)(got[i] ^ token[i]);
    }
    return diff == 0;
}

int sel_cast_http_open(sel_cast_http* s, const char* file, const char* mime) {
    FILE* f;
    unsigned port = 0;

    memset(s, 0, sizeof *s);
    s->lh = -1;
    if (strlen(file) >= sizeof s->file || strlen(mime) >= sizeof s->mime) {
        return SEL_CAST_HTTP_E_FILE;
    }
    f = fopen(file, "rb");
    if (f == NULL) {
        return SEL_CAST_HTTP_E_FILE;
    }
    fclose(f);
    snprintf(s->file, sizeof s->file, "%s", file);
    snprintf(s->mime, sizeof s->mime, "%s", mime);
    if (token_make(s->token) != 0) {
        return SEL_CAST_HTTP_E_ENTROPY;
    }
    s->lh = sel_net_listen(&port);
    if (s->lh < 0) {
        int rc = s->lh;

        s->lh = -1;
        return rc;
    }
    s->port = port;
    return 0;
}

int sel_cast_http_url(const sel_cast_http* s, const char* ip, char* out, size_t cap) {
    int n = snprintf(out, cap, "http://%s:%u/%s", ip, s->port, s->token);

    return n > 0 && (size_t)n < cap ? 0 : -1;
}

int sel_cast_http_range(const char* spec, unsigned long size, unsigned long* first, unsigned long* last) {
    char* end;
    unsigned long a, b;

    if (*spec == '-') { /* suffix: the last b bytes */
        b = strtoul(spec + 1, &end, 10);
        if (end == spec + 1) {
            return -1;
        }
        if (b == 0 || size == 0) {
            return 0;
        }
        *first = b >= size ? 0 : size - b;
        *last = size - 1;
        return 1;
    }
    if (!isdigit((unsigned char)*spec)) {
        return -1;
    }
    a = strtoul(spec, &end, 10);
    if (*end != '-') {
        return -1;
    }
    end++;
    if (*end == '\0' || *end == ',' || isspace((unsigned char)*end)) {
        b = size > 0 ? size - 1 : 0;
    } else {
        b = strtoul(end, &end, 10);
        if (*end != '\0' && *end != ',' && !isspace((unsigned char)*end)) {
            return -1;
        }
    }
    if (a >= size || b < a) {
        return 0;
    }
    *first = a;
    *last = b >= size ? size - 1 : b;
    return 1;
}

static int send_str(int h, const char* text) {
    return sel_net_send(h, text, strlen(text), 5000u);
}

static void reply_plain(int h, int code, const char* reason, const char* extra) {
    char buf[256];

    snprintf(buf, sizeof buf, "HTTP/1.1 %d %s\r\n%sContent-Length: 0\r\nConnection: keep-alive\r\n\r\n", code, reason,
             extra);
    send_str(h, buf);
}

static unsigned long file_size(const char* path, FILE** out) {
    FILE* f = fopen(path, "rb");
    long n;

    *out = f;
    if (f == NULL || fseek(f, 0, SEEK_END) != 0 || (n = ftell(f)) < 0) {
        return 0;
    }
    return (unsigned long)n;
}

/* One request (NUL-terminated header block). Returns 0 to keep the connection, -1 to drop it. */
static int answer(sel_cast_http* s, int h, char* req) {
    char method[8], target[SEL_CAST_HTTP_TOKEN_LEN + 8];
    char hdr[384];
    char* range;
    FILE* f;
    unsigned long size, first, last;
    int rr, head;

    if (sscanf(req, "%7s %39s", method, target) != 2) {
        reply_plain(h, 400, "Bad Request", "");
        return -1;
    }
    s->requests++;
    head = strcmp(method, "HEAD") == 0;
    if (!head && strcmp(method, "GET") != 0) {
        reply_plain(h, 405, "Method Not Allowed", "Allow: GET, HEAD\r\n");
        return 0;
    }
    if (target[0] != '/' || !token_equal(target + 1, s->token)) {
        reply_plain(h, 404, "Not Found", "");
        return 0;
    }
    size = file_size(s->file, &f);
    if (f == NULL) {
        reply_plain(h, 404, "Not Found", "");
        return 0;
    }
    first = 0;
    last = size > 0 ? size - 1 : 0;
    rr = -1;
    range = strstr(req, "\r\nRange:");
    if (range == NULL) {
        range = strstr(req, "\r\nrange:");
    }
    if (range != NULL) {
        range += 8;
        while (*range == ' ') {
            range++;
        }
        if (strncmp(range, "bytes=", 6) == 0) {
            rr = sel_cast_http_range(range + 6, size, &first, &last);
        }
    }
    if (rr == 0) {
        snprintf(hdr, sizeof hdr, "Content-Range: bytes */%lu\r\n", size);
        reply_plain(h, 416, "Range Not Satisfiable", hdr);
        fclose(f);
        return 0;
    }
    snprintf(hdr, sizeof hdr,
             "HTTP/1.1 %s\r\nContent-Type: %s\r\nAccept-Ranges: bytes\r\nContent-Length: %lu\r\n"
             "Access-Control-Allow-Origin: *\r\nConnection: keep-alive\r\n",
             rr == 1 ? "206 Partial Content" : "200 OK", s->mime, size == 0 ? 0ul : last - first + 1);
    if (send_str(h, hdr) != 0) {
        fclose(f);
        return -1;
    }
    if (rr == 1) {
        snprintf(hdr, sizeof hdr, "Content-Range: bytes %lu-%lu/%lu\r\n", first, last, size);
        send_str(h, hdr);
    }
    send_str(h, "\r\n");
    if (!head && size > 0) {
        unsigned char chunk[CHUNK];
        unsigned long left = last - first + 1;

        fseek(f, (long)first, SEEK_SET);
        while (left > 0) {
            size_t want = left < sizeof chunk ? (size_t)left : sizeof chunk;
            size_t got = fread(chunk, 1, want, f);

            if (got == 0 || sel_net_send(h, chunk, got, 10000u) != 0) {
                fclose(f);
                return -1;
            }
            s->bytes_sent += got;
            left -= got;
        }
    }
    fclose(f);
    return 0;
}

int sel_cast_http_poll(sel_cast_http* s, unsigned timeout_ms) {
    char req[REQ_MAX];
    int h, answered = 0;

    if (s->lh < 0) {
        return SEL_NET_E_IO;
    }
    h = sel_net_accept(s->lh, timeout_ms);
    if (h == SEL_NET_E_TIMEOUT) {
        return 0;
    }
    if (h < 0) {
        return h;
    }
    for (;;) {
        size_t used = 0;
        char* end = NULL;

        while (end == NULL) {
            int n;

            if (used + 1 >= sizeof req) {
                reply_plain(h, 431, "Request Header Fields Too Large", "");
                goto done;
            }
            n = sel_net_recv(h, req + used, sizeof req - 1 - used, 5000u);
            if (n <= 0) {
                goto done;
            }
            used += (size_t)n;
            req[used] = '\0';
            end = strstr(req, "\r\n\r\n");
        }
        /* Requests carry no body; bytes after the blank line would be a pipelined request, which this server does
         * not support (a receiver waits for each answer). */
        end[2] = '\0';
        answered++;
        if (answer(s, h, req) != 0) {
            break;
        }
    }
done:
    sel_net_close(h);
    return answered;
}

void sel_cast_http_close(sel_cast_http* s) {
    if (s->lh >= 0) {
        sel_net_close(s->lh);
        s->lh = -1;
    }
}

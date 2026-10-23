/* See hippo_publish.h. */
#include "hippo_publish.h"

#include "../net/http.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned g_posts;

unsigned sel_hippo_publish_posts(void) {
    return g_posts;
}

typedef struct reply_buf {
    char text[512];
    size_t len;
} reply_buf;

static int reply_sink(void* ctx, const void* data, size_t len) {
    reply_buf* r = (reply_buf*)ctx;
    if (len > sizeof r->text - 1u - r->len) {
        len = sizeof r->text - 1u - r->len;
    }
    memcpy(r->text + r->len, data, len);
    r->len += len;
    r->text[r->len] = '\0';
    return 1;
}

/* Appends `key=value` (value percent-encoded) to url; 0 when it does not fit. */
static int add_param(char* url, size_t cap, const char* key, const char* value) {
    static const char hex[] = "0123456789ABCDEF";
    size_t n = strlen(url);
    int first = strchr(url, '?') == NULL;
    size_t kl = strlen(key);
    if (n + 2u + kl + 1u >= cap) {
        return 0;
    }
    url[n++] = first ? '?' : '&';
    memcpy(url + n, key, kl);
    n += kl;
    url[n++] = '=';
    for (; *value != '\0'; value++) {
        unsigned char c = (unsigned char)*value;
        int plain = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' ||
                    c == '.' || c == '~';
        if (n + 4u >= cap) {
            return 0;
        }
        if (plain) {
            url[n++] = (char)c;
        } else {
            url[n++] = '%';
            url[n++] = hex[c >> 4];
            url[n++] = hex[c & 15u];
        }
    }
    url[n] = '\0';
    return 1;
}

/* `OK <id> <filename> <url>`: single spaces, first line only. */
static int parse_reply(const char* text, sel_hippo_publish_reply* out) {
    char line[512];
    char* id;
    char* name;
    char* url;
    size_t n = strcspn(text, "\r\n");
    if (n == 0 || n >= sizeof line) {
        return 0;
    }
    memcpy(line, text, n);
    line[n] = '\0';
    if (strncmp(line, "OK ", 3) != 0) {
        return 0;
    }
    id = line + 3;
    name = strchr(id, ' ');
    if (name == NULL) {
        return 0;
    }
    *name++ = '\0';
    url = strchr(name, ' ');
    if (url == NULL) {
        return 0;
    }
    *url++ = '\0';
    if (*id == '\0' || *name == '\0' || *url == '\0' || strlen(id) >= sizeof out->id ||
        strlen(name) >= sizeof out->filename || strlen(url) >= sizeof out->url) {
        return 0;
    }
    strcpy(out->id, id);
    strcpy(out->filename, name);
    strcpy(out->url, url);
    return 1;
}

int sel_hippo_publish(const char* body, size_t len, const char* title, const char* author, const char* genre,
                      const char* filename, sel_hippo_publish_reply* reply) {
    char url[1024];
    const char* base = getenv(SEL_HIPPO_PUBLISH_ENV);
    static const char* const hdrs[] = {"Content-Type: application/octet-stream", NULL};
    reply_buf rb;
    sel_http_req req;
    sel_http_res res;
    sel_http_err err;

    memset(reply, 0, sizeof *reply);
    if (len > SEL_HIPPO_PUBLISH_MAX) {
        return SEL_HIPPO_PUB_TOO_LARGE;
    }
    if (len == 0 || filename == NULL || filename[0] == '\0') {
        return SEL_HIPPO_PUB_ARGS;
    }
    if (base == NULL || base[0] == '\0') {
        base = SEL_HIPPO_PUBLISH_URL;
    }
    if (strlen(base) >= sizeof url) {
        return SEL_HIPPO_PUB_ARGS;
    }
    strcpy(url, base);
    if (!add_param(url, sizeof url, "title", title) || !add_param(url, sizeof url, "author", author) ||
        !add_param(url, sizeof url, "genre", genre) || !add_param(url, sizeof url, "filename", filename)) {
        return SEL_HIPPO_PUB_ARGS;
    }
    memset(&req, 0, sizeof req);
    req.method = "POST";
    req.url = url;
    req.headers = hdrs;
    req.body = body;
    req.body_len = len;
    memset(&rb, 0, sizeof rb);
    g_posts++;
    err = sel_http_request(&req, reply_sink, &rb, &res);
    if (err != SEL_HTTP_OK) {
        return SEL_HIPPO_PUB_NET;
    }
    reply->http_status = res.status;
    if (res.status < 200 || res.status > 299) {
        return SEL_HIPPO_PUB_STATUS;
    }
    return parse_reply(rb.text, reply) ? SEL_HIPPO_PUB_OK : SEL_HIPPO_PUB_REPLY;
}

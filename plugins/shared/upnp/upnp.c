/* UPnP / DLNA media server discovery (upnp.h). All socket calls go through upnp_sock.h. */
#include "upnp/upnp.h"

#include "net/http.h"
#include "net/net.h"
#include "source/xml_scan.h"
#include "upnp/upnp_sock.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DESC_BYTES 16384u
#define DGRAM_BYTES 1024u
#define CD_TYPE "urn:schemas-upnp-org:service:ContentDirectory:"

static int ieq_prefix(const char* s, const char* name) {
    for (; *name != '\0'; s++, name++) {
        if (tolower((unsigned char)*s) != tolower((unsigned char)*name)) {
            return 0;
        }
    }
    return 1;
}

/* The value of header `name` (with its colon) in the CRLF lines of msg, trimmed, copied to out. */
static int header_value(const char* msg, size_t len, const char* name, char* out, size_t n) {
    const char* end = msg + len;
    size_t nl = strlen(name), o = 0;

    while (msg < end) {
        const char* eol = memchr(msg, '\n', (size_t)(end - msg));
        const char* le = eol != NULL ? eol : end;

        if ((size_t)(le - msg) > nl && ieq_prefix(msg, name)) {
            const char* v = msg + nl;

            while (v < le && (*v == ' ' || *v == '\t')) {
                v++;
            }
            while (le > v && isspace((unsigned char)le[-1])) {
                le--;
            }
            while (v < le && o + 1 < n) {
                out[o++] = *v++;
            }
            out[o] = '\0';
            return o > 0;
        }
        msg = eol != NULL ? eol + 1 : end;
    }
    return 0;
}

int sel_upnp_parse_ssdp(const char* msg, size_t len, char* location, size_t loc_n, char* usn, size_t usn_n) {
    if (!(len >= 12u && (strncmp(msg, "HTTP/1.1 200", 12) == 0 || strncmp(msg, "HTTP/1.0 200", 12) == 0)) &&
        !(len >= 6u && strncmp(msg, "NOTIFY", 6) == 0)) {
        return 0;
    }
    usn[0] = '\0';
    (void)header_value(msg, len, "usn:", usn, usn_n);
    return header_value(msg, len, "location:", location, loc_n);
}

int sel_upnp_resolve(const char* base, const char* ref, char* out, size_t n) {
    const char* auth = strstr(base, "://");
    const char* path;
    const char* last;
    int w;

    if (strncmp(ref, "http://", 7) == 0 || strncmp(ref, "https://", 8) == 0) {
        w = snprintf(out, n, "%s", ref);
        return w > 0 && (size_t)w < n;
    }
    if (auth == NULL) {
        return 0;
    }
    path = strchr(auth + 3, '/');
    if (path == NULL) {
        path = base + strlen(base);
    }
    if (ref[0] == '/') {
        w = snprintf(out, n, "%.*s%s", (int)(path - base), base, ref);
    } else {
        const char* q = strchr(path, '?');
        size_t plen = q != NULL ? (size_t)(q - path) : strlen(path);

        last = path;
        for (w = 0; (size_t)w < plen; w++) {
            if (path[w] == '/') {
                last = path + w;
            }
        }
        w = snprintf(out, n, "%.*s/%s", (int)(last - base), base, ref);
    }
    return w > 0 && (size_t)w < n;
}

int sel_upnp_parse_description(const char* xml, const char* location, sel_upnp_server* out) {
    const char* end = xml + strlen(xml);
    const char* hit = strstr(xml, CD_TYPE);
    const char* open = NULL;
    const char* close;
    const char* p;
    char ctl[SEL_UPNP_URL_BYTES], base[SEL_UPNP_URL_BYTES];

    if (hit == NULL || !sel_xml_tag_text(xml, end, "friendlyName", out->name, sizeof out->name)) {
        return 0;
    }
    for (p = xml; (p = strstr(p, "<service>")) != NULL && p < hit; p++) {
        open = p;
    }
    close = strstr(hit, "</service>");
    if (open == NULL || close == NULL || !sel_xml_tag_text(open, close, "controlURL", ctl, sizeof ctl)) {
        return 0;
    }
    if (!sel_xml_tag_text(xml, end, "URLBase", base, sizeof base)) {
        snprintf(base, sizeof base, "%s", location);
    }
    return sel_upnp_resolve(base, ctl, out->control_url, sizeof out->control_url);
}

typedef struct body {
    char* buf;
    size_t len;
} body;

static int body_sink(void* ctx, const void* data, size_t len) {
    body* b = (body*)ctx;
    size_t room = DESC_BYTES - 1u - b->len;

    memcpy(b->buf + b->len, data, len < room ? len : room);
    b->len += len < room ? len : room;
    return 1;
}

static int fetch_description(const char* url, const char* usn, sel_upnp_server* out) {
    sel_http_req req;
    sel_http_res res;
    body b;
    int ok = 0;

    b.buf = (char*)malloc(DESC_BYTES);
    if (b.buf == NULL) {
        return 0;
    }
    b.len = 0;
    memset(&req, 0, sizeof req);
    req.url = url;
    if (sel_http_request(&req, body_sink, &b, &res) == SEL_HTTP_OK && res.status == 200) {
        b.buf[b.len] = '\0';
        snprintf(out->location, sizeof out->location, "%s", url);
        snprintf(out->usn, sizeof out->usn, "%s", usn);
        ok = sel_upnp_parse_description(b.buf, url, out);
    }
    free(b.buf);
    return ok;
}

int sel_upnp_discover(const char* ip, unsigned port, unsigned wait_ms, sel_upnp_server* out, size_t max) {
    static const char search[] = "M-SEARCH * HTTP/1.1\r\n"
                                 "HOST: 239.255.255.250:1900\r\n"
                                 "MAN: \"ssdp:discover\"\r\n"
                                 "MX: 2\r\n"
                                 "ST: urn:schemas-upnp-org:device:MediaServer:1\r\n\r\n";
    sel_usock* s = sel_usock_open();
    char* dg;
    char (*locs)[SEL_UPNP_URL_BYTES];
    char usn[SEL_UPNP_NAME_BYTES + 32], from[16];
    size_t nloc = 0, found = 0, i;
    unsigned long t0 = sel_net_now_ms();

    if (s == NULL || max == 0u) {
        sel_usock_close(s);
        return s == NULL ? -1 : 0;
    }
    dg = (char*)malloc(DGRAM_BYTES);
    locs = (char(*)[SEL_UPNP_URL_BYTES])malloc(max * SEL_UPNP_URL_BYTES);
    if (dg == NULL || locs == NULL) {
        free(dg);
        free(locs);
        sel_usock_close(s);
        return -1;
    }
    if (sel_usock_send(s, ip != NULL ? ip : SEL_UPNP_MULTICAST_IP, port != 0 ? port : SEL_UPNP_SSDP_PORT, search,
                       sizeof search - 1u)) {
        /* Collect LOCATIONs first (one per distinct URL), fetch afterwards so the socket queue is drained quickly. */
        char (*usns)[SEL_UPNP_NAME_BYTES + 32] = (char(*)[SEL_UPNP_NAME_BYTES + 32])malloc(max * (SEL_UPNP_NAME_BYTES + 32));
        unsigned long el;

        while (usns != NULL && nloc < max && (el = sel_net_now_ms() - t0) < wait_ms) {
            long n = sel_usock_recv(s, dg, DGRAM_BYTES - 1u, (unsigned)(wait_ms - el), from);
            int dup = 0;

            if (n <= 0) {
                continue;
            }
            dg[n] = '\0';
            if (!sel_upnp_parse_ssdp(dg, (size_t)n, locs[nloc], SEL_UPNP_URL_BYTES, usn, sizeof usn)) {
                continue;
            }
            for (i = 0; i < nloc; i++) {
                dup |= strcmp(locs[i], locs[nloc]) == 0;
            }
            if (!dup) {
                snprintf(usns[nloc], sizeof usns[nloc], "%s", usn);
                nloc++;
            }
        }
        for (i = 0; usns != NULL && i < nloc; i++) {
            if (fetch_description(locs[i], usns[i], &out[found])) {
                found++;
            }
        }
        free(usns);
    }
    free(dg);
    free(locs);
    sel_usock_close(s);
    return (int)found;
}

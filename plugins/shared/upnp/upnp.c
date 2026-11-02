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
    char ctl[SEL_UPNP_URL_BYTES], evt[SEL_UPNP_URL_BYTES], base[SEL_UPNP_URL_BYTES];

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
    out->cd_version = hit[sizeof CD_TYPE - 1u] >= '1' && hit[sizeof CD_TYPE - 1u] <= '9' ? hit[sizeof CD_TYPE - 1u] - '0' : 1;
    out->udn[0] = '\0';
    (void)sel_xml_tag_text(xml, end, "UDN", out->udn, sizeof out->udn);
    out->event_url[0] = '\0';
    if (sel_xml_tag_text(open, close, "eventSubURL", evt, sizeof evt)) {
        (void)sel_upnp_resolve(location, evt, out->event_url, sizeof out->event_url);
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

static int capped_sink(body* b, size_t cap, const void* data, size_t len) {
    size_t room = cap - 1u - b->len;

    memcpy(b->buf + b->len, data, len < room ? len : room);
    b->len += len < room ? len : room;
    return 1;
}

static int body_sink(void* ctx, const void* data, size_t len) {
    return capped_sink((body*)ctx, DESC_BYTES, data, len);
}

#define SOAP_BYTES 98304u /* raw Browse response cap: a 100 entry page, entity-escaped */
#define DIDL_BYTES 65536u /* decoded Result cap */

static int soap_sink(void* ctx, const void* data, size_t len) {
    return capped_sink((body*)ctx, SOAP_BYTES, data, len);
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

/* ---- ContentDirectory browse (I894a2) ---- */


int sel_upnp_parse_duration(const char* s, unsigned long* ms) {
    unsigned long part[3] = {0, 0, 0}, frac = 0, scale = 100;
    int i;

    for (i = 0; i < 3; i++) {
        if (!isdigit((unsigned char)*s)) {
            return 0;
        }
        while (isdigit((unsigned char)*s)) {
            part[i] = part[i] * 10u + (unsigned long)(*s++ - '0');
        }
        if (i < 2 && *s++ != ':') {
            return 0;
        }
    }
    if (*s == '.') {
        for (s++; isdigit((unsigned char)*s) && scale > 0u; s++, scale /= 10u) {
            frac += (unsigned long)(*s - '0') * scale;
        }
    }
    *ms = ((part[0] * 60u + part[1]) * 60u + part[2]) * 1000u + frac;
    return 1;
}

/* Start of the next `<container ` or `<item ` open tag at or after p; *is_container tells which. */
static const char* next_entry(const char* p, int* is_container) {
    while ((p = strchr(p, '<')) != NULL) {
        if (strncmp(p, "<container ", 11) == 0) {
            *is_container = 1;
            return p;
        }
        if (strncmp(p, "<item ", 6) == 0) {
            *is_container = 0;
            return p;
        }
        p++;
    }
    return NULL;
}

int sel_upnp_parse_didl(const char* didl, sel_upnp_entry* out, size_t max) {
    const char* p = didl;
    size_t n = 0;
    int cont;

    while (n < max && (p = next_entry(p, &cont)) != NULL) {
        const char* close = strstr(p, cont ? "</container>" : "</item>");
        sel_upnp_entry* e = &out[n];
        char num[16];

        if (close == NULL) {
            break;
        }
        memset(e, 0, sizeof *e);
        e->is_container = cont;
        (void)sel_xml_tag_attr(p, close, cont ? "container" : "item", "id", e->id, sizeof e->id);
        (void)sel_xml_tag_text(p, close, "dc:title", e->title, sizeof e->title);
        if (!sel_xml_tag_text(p, close, "upnp:artist", e->artist, sizeof e->artist)) {
            (void)sel_xml_tag_text(p, close, "dc:creator", e->artist, sizeof e->artist);
        }
        (void)sel_xml_tag_text(p, close, "upnp:album", e->album, sizeof e->album);
        if (cont) {
            if (sel_xml_tag_attr(p, close, "container", "childCount", num, sizeof num)) {
                e->child_count = (unsigned)strtoul(num, NULL, 10);
            }
        } else {
            char dur[24];

            (void)sel_xml_tag_text(p, close, "res", e->res_url, sizeof e->res_url);
            (void)sel_xml_tag_attr(p, close, "res", "protocolInfo", e->protocol_info, sizeof e->protocol_info);
            if (sel_xml_tag_attr(p, close, "res", "duration", dur, sizeof dur)) {
                (void)sel_upnp_parse_duration(dur, &e->duration_ms);
            }
        }
        if (e->id[0] != '\0') {
            n++;
        }
        p = close;
    }
    return (int)n;
}

int sel_upnp_parse_browse(const char* soap, sel_upnp_entry* out, size_t max, unsigned* returned, unsigned* total) {
    const char* end = soap + strlen(soap);
    char num[16];
    char* didl = (char*)malloc(DIDL_BYTES);
    int n = -1;

    if (didl == NULL) {
        return -1;
    }
    *returned = 0;
    *total = 0;
    if (sel_xml_tag_text(soap, end, "Result", didl, DIDL_BYTES) && strlen(didl) + 1u < DIDL_BYTES) {
        n = sel_upnp_parse_didl(didl, out, max);
        if (sel_xml_tag_text(soap, end, "NumberReturned", num, sizeof num)) {
            *returned = (unsigned)strtoul(num, NULL, 10);
        }
        if (sel_xml_tag_text(soap, end, "TotalMatches", num, sizeof num)) {
            *total = (unsigned)strtoul(num, NULL, 10);
        }
    }
    free(didl);
    return n;
}

/* Appends s to out[*o], escaping the XML characters; 0 when it does not fit. */
static int xml_escape_into(char* out, size_t n, size_t* o, const char* s) {
    for (; *s != '\0'; s++) {
        const char* r = *s == '&' ? "&amp;" : *s == '<' ? "&lt;" : *s == '>' ? "&gt;" : *s == '"' ? "&quot;" : NULL;
        size_t l = r != NULL ? strlen(r) : 1u;

        if (*o + l + 1u > n) {
            return 0;
        }
        if (r != NULL) {
            memcpy(out + *o, r, l);
        } else {
            out[*o] = *s;
        }
        *o += l;
    }
    out[*o] = '\0';
    return 1;
}

/* One ContentDirectory SOAP call: POSTs <u:action xmlns:u="CD_TYPE<ver>">args</u:action> to control_url and returns the
 * malloc'd, NUL terminated response (SOAP_BYTES cap) or NULL on a transport error / non-200 answer. */
static char* cd_call(const char* control_url, unsigned ver, const char* action, const char* args) {
    char hdr[160], req_body[1024];
    const char* headers[3];
    sel_http_req req;
    sel_http_res res;
    body b;
    int w;

    snprintf(hdr, sizeof hdr, "SOAPACTION: \"" CD_TYPE "%u#%s\"", ver, action);
    headers[0] = "Content-Type: text/xml; charset=\"utf-8\"";
    headers[1] = hdr;
    headers[2] = NULL;
    w = snprintf(req_body, sizeof req_body,
                 "<?xml version=\"1.0\"?>\r\n<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
                 "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\"><s:Body>"
                 "<u:%s xmlns:u=\"" CD_TYPE "%u\">%s</u:%s></s:Body></s:Envelope>",
                 action, ver, args, action);
    if (w <= 0 || (size_t)w >= sizeof req_body) {
        return NULL;
    }
    b.buf = (char*)malloc(SOAP_BYTES);
    if (b.buf == NULL) {
        return NULL;
    }
    b.len = 0;
    memset(&req, 0, sizeof req);
    req.method = "POST";
    req.url = control_url;
    req.headers = headers;
    req.body = req_body;
    req.body_len = (size_t)w;
    if (sel_http_request(&req, soap_sink, &b, &res) == SEL_HTTP_OK && res.status == 200 && b.len < SOAP_BYTES - 1u) {
        b.buf[b.len] = '\0';
        return b.buf;
    }
    free(b.buf);
    return NULL;
}

int sel_upnp_browse(const char* control_url, const char* object_id, unsigned start, unsigned count,
                    sel_upnp_entry* out, size_t max, unsigned* total) {
    char args[SEL_UPNP_ID_BYTES * 6 + 256], idesc[SEL_UPNP_ID_BYTES * 6];
    size_t o = 0;
    unsigned returned = 0;
    char* soap;
    int n = -1;

    if (count == 0u || count > SEL_UPNP_BROWSE_PAGE) {
        count = SEL_UPNP_BROWSE_PAGE;
    }
    if (count > max) {
        count = (unsigned)max;
    }
    idesc[0] = '\0';
    if (count == 0u || !xml_escape_into(idesc, sizeof idesc, &o, object_id)) {
        return -1;
    }
    snprintf(args, sizeof args,
             "<ObjectID>%s</ObjectID><BrowseFlag>BrowseDirectChildren</BrowseFlag><Filter>*</Filter>"
             "<StartingIndex>%u</StartingIndex><RequestedCount>%u</RequestedCount><SortCriteria></SortCriteria>",
             idesc, start, count);
    soap = cd_call(control_url, 1u, "Browse", args);
    if (soap != NULL) {
        n = sel_upnp_parse_browse(soap, out, count, &returned, total);
        free(soap);
    }
    return n;
}

int sel_upnp_get_system_update_id(const char* control_url, unsigned long* id) {
    char num[24];
    char* soap = cd_call(control_url, 1u, "GetSystemUpdateID", "");
    int ok = 0;

    if (soap != NULL) {
        ok = sel_xml_tag_text(soap, soap + strlen(soap), "Id", num, sizeof num) && num[0] >= '0' && num[0] <= '9';
        if (ok) {
            *id = strtoul(num, NULL, 10);
        }
        free(soap);
    }
    return ok;
}

int sel_upnp_get_reset_token(const char* control_url, char* out, size_t n) {
    char* soap = cd_call(control_url, 3u, "GetServiceResetToken", "");
    int ok = 0;

    if (soap != NULL) {
        ok = sel_xml_tag_text(soap, soap + strlen(soap), "ResetToken", out, n) && out[0] != '\0';
        free(soap);
    }
    return ok;
}

int sel_upnp_search_changed(const char* control_url, unsigned long since, unsigned start, sel_upnp_entry* out,
                            size_t max, unsigned* total) {
    char args[512];
    unsigned returned = 0, count = max < SEL_UPNP_BROWSE_PAGE ? (unsigned)max : SEL_UPNP_BROWSE_PAGE;
    char* soap;
    int n = -1;

    snprintf(args, sizeof args,
             "<ContainerID>0</ContainerID><SearchCriteria>upnp:class derivedfrom &quot;object.container&quot; and "
             "upnp:objectUpdateID &gt; %lu</SearchCriteria><Filter>*</Filter><StartingIndex>%u</StartingIndex>"
             "<RequestedCount>%u</RequestedCount><SortCriteria></SortCriteria>",
             since, start, count);
    soap = cd_call(control_url, 3u, "Search", args);
    if (soap != NULL) {
        n = sel_upnp_parse_browse(soap, out, count, &returned, total);
        free(soap);
    }
    return n;
}

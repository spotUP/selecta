/* Multicast DNS codec and browse (mdns.h, I892e1). */
#include "mdns/mdns.h"

#include "net/net.h"
#include "upnp/upnp_sock.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PKT_BYTES 1500u
#define POOL 96u

static unsigned rd16(const unsigned char* p) { return ((unsigned)p[0] << 8) | p[1]; }

size_t sel_mdns_encode_query(unsigned char* out, size_t cap, const char* name, unsigned qtype) {
    size_t o = 12u;
    const char* p = name;

    if (cap < 12u + strlen(name) + 2u + 4u) {
        return 0;
    }
    memset(out, 0, 12u);
    out[5] = 1; /* QDCOUNT */
    while (*p != '\0') {
        const char* dot = strchr(p, '.');
        size_t l = dot != NULL ? (size_t)(dot - p) : strlen(p);

        if (l == 0 || l > 63u) {
            return 0;
        }
        out[o++] = (unsigned char)l;
        memcpy(out + o, p, l);
        o += l;
        p += l;
        if (*p == '.') {
            p++;
        }
    }
    out[o++] = 0;
    out[o++] = (unsigned char)(qtype >> 8);
    out[o++] = (unsigned char)qtype;
    out[o++] = 0;
    out[o++] = 1; /* class IN, QU bit clear */
    return o;
}

/* Reads a possibly compressed name at *pos into out and advances *pos past it in the original stream. 0 on success. */
static int read_name(const unsigned char* pkt, size_t len, size_t* pos, char* out, size_t cap) {
    size_t p = *pos, o = 0;
    int jumped = 0, hops = 0;

    for (;;) {
        unsigned l;

        if (p >= len) {
            return -1;
        }
        l = pkt[p];
        if (l == 0) {
            p++;
            break;
        }
        if ((l & 0xC0u) == 0xC0u) {
            size_t to;

            if (p + 1u >= len || ++hops > 16) {
                return -1;
            }
            to = (((size_t)(l & 0x3Fu) << 8) | pkt[p + 1u]);
            if (!jumped) {
                *pos = p + 2u;
                jumped = 1;
            }
            p = to;
            continue;
        }
        if ((l & 0xC0u) != 0 || p + 1u + l > len || o + l + 2u > cap) {
            return -1;
        }
        if (o > 0) {
            out[o++] = '.';
        }
        memcpy(out + o, pkt + p + 1u, l);
        o += l;
        p += 1u + l;
    }
    out[o] = '\0';
    if (!jumped) {
        *pos = p;
    }
    return 0;
}

int sel_mdns_parse(const unsigned char* pkt, size_t len, sel_mdns_rec* recs, size_t* n, size_t max) {
    size_t pos = 12u, i, total;
    char scratch[SEL_MDNS_NAME_MAX];

    if (len < 12u || (pkt[2] & 0x80u) == 0) {
        return -1;
    }
    for (i = rd16(pkt + 4); i > 0; i--) { /* questions */
        if (read_name(pkt, len, &pos, scratch, sizeof scratch) != 0 || pos + 4u > len) {
            return -1;
        }
        pos += 4u;
    }
    total = (size_t)rd16(pkt + 6) + rd16(pkt + 8) + rd16(pkt + 10);
    for (i = 0; i < total; i++) {
        sel_mdns_rec r;
        unsigned type, rdlen;
        size_t rd_end, p;

        memset(&r, 0, sizeof r);
        if (read_name(pkt, len, &pos, r.name, sizeof r.name) != 0 || pos + 10u > len) {
            break;
        }
        type = rd16(pkt + pos);
        rdlen = rd16(pkt + pos + 8);
        pos += 10u;
        if (pos + rdlen > len) {
            break;
        }
        rd_end = pos + rdlen;
        p = pos;
        r.type = type;
        if (type == SEL_MDNS_T_PTR) {
            if (read_name(pkt, len, &p, r.target, sizeof r.target) != 0) {
                break;
            }
        } else if (type == SEL_MDNS_T_SRV) {
            if (rdlen < 7u) {
                break;
            }
            r.port = rd16(pkt + p + 4);
            p += 6u;
            if (read_name(pkt, len, &p, r.target, sizeof r.target) != 0) {
                break;
            }
        } else if (type == SEL_MDNS_T_TXT) {
            r.txt_len = rdlen < SEL_MDNS_TXT_MAX ? rdlen : SEL_MDNS_TXT_MAX;
            memcpy(r.txt, pkt + p, r.txt_len);
        } else if (type == SEL_MDNS_T_A) {
            if (rdlen != 4u) {
                break;
            }
            snprintf(r.ip, sizeof r.ip, "%u.%u.%u.%u", pkt[p], pkt[p + 1u], pkt[p + 2u], pkt[p + 3u]);
        } else {
            pos = rd_end;
            continue;
        }
        pos = rd_end;
        if (*n < max) {
            recs[(*n)++] = r;
        }
    }
    return 0;
}

int sel_mdns_txt_get(const unsigned char* txt, size_t len, const char* key, char* out, size_t cap) {
    size_t p = 0, kl = strlen(key);

    while (p < len) {
        size_t l = txt[p++];

        if (p + l > len) {
            return 0;
        }
        if (l > kl && memcmp(txt + p, key, kl) == 0 && txt[p + kl] == '=') {
            size_t vl = l - kl - 1u;

            if (vl >= cap) {
                vl = cap - 1u;
            }
            memcpy(out, txt + p + kl + 1u, vl);
            out[vl] = '\0';
            return 1;
        }
        p += l;
    }
    return 0;
}

static const sel_mdns_rec* find_rec(const sel_mdns_rec* recs, size_t n, unsigned type, const char* name) {
    size_t i;

    for (i = 0; i < n; i++) {
        if (recs[i].type == type && strcmp(recs[i].name, name) == 0) {
            return &recs[i];
        }
    }
    return NULL;
}

size_t sel_mdns_resolve(const sel_mdns_rec* recs, size_t n, const char* service, sel_mdns_service* out, size_t max) {
    size_t i, j, found = 0;

    for (i = 0; i < n && found < max; i++) {
        const sel_mdns_rec *srv, *a, *txt;
        int dup = 0;

        if (recs[i].type != SEL_MDNS_T_PTR || strcmp(recs[i].name, service) != 0) {
            continue;
        }
        for (j = 0; j < found; j++) {
            dup |= strcmp(out[j].instance, recs[i].target) == 0;
        }
        srv = find_rec(recs, n, SEL_MDNS_T_SRV, recs[i].target);
        a = srv != NULL ? find_rec(recs, n, SEL_MDNS_T_A, srv->target) : NULL;
        if (dup || srv == NULL || a == NULL) {
            continue;
        }
        txt = find_rec(recs, n, SEL_MDNS_T_TXT, recs[i].target);
        memset(&out[found], 0, sizeof out[found]);
        snprintf(out[found].instance, sizeof out[found].instance, "%s", recs[i].target);
        snprintf(out[found].host, sizeof out[found].host, "%s", srv->target);
        snprintf(out[found].ip, sizeof out[found].ip, "%s", a->ip);
        out[found].port = srv->port;
        if (txt != NULL) {
            memcpy(out[found].txt, txt->txt, txt->txt_len);
            out[found].txt_len = txt->txt_len;
        }
        found++;
    }
    return found;
}

static void send_query(sel_usock* s, const char* ip, unsigned port, const char* name, unsigned qtype) {
    unsigned char q[300];
    size_t l = sel_mdns_encode_query(q, sizeof q, name, qtype);

    if (l > 0) {
        (void)sel_usock_send(s, ip, port, q, l);
    }
}

/* Receives datagrams until wait_ms after t0, parsing each into recs. */
static void collect(sel_usock* s, unsigned long t0, unsigned wait_ms, unsigned char* dg, sel_mdns_rec* recs, size_t* n) {
    char from[16];
    unsigned long el;

    while ((el = sel_net_now_ms() - t0) < wait_ms) {
        long len = sel_usock_recv(s, dg, PKT_BYTES, (unsigned)(wait_ms - el), from);

        if (len > 0) {
            (void)sel_mdns_parse(dg, (size_t)len, recs, n, POOL);
        }
    }
}

int sel_mdns_browse(const char* ip, unsigned port, const char* service, unsigned wait_ms, sel_mdns_service* out,
                    size_t max) {
    sel_usock* s = sel_usock_open();
    unsigned char* dg = (unsigned char*)malloc(PKT_BYTES);
    sel_mdns_rec* recs = (sel_mdns_rec*)malloc(POOL * sizeof *recs);
    size_t n = 0, i, found;
    const char* tip = ip != NULL ? ip : SEL_MDNS_GROUP_IP;
    unsigned tport = port != 0 ? port : SEL_MDNS_PORT;
    unsigned long t0 = sel_net_now_ms();
    int asked = 0;

    if (s == NULL || dg == NULL || recs == NULL) {
        sel_usock_close(s);
        free(dg);
        free(recs);
        return -1;
    }
    send_query(s, tip, tport, service, SEL_MDNS_T_PTR);
    collect(s, t0, wait_ms / 2u + 1u, dg, recs, &n);
    /* An instance whose SRV or A record did not come with its PTR is asked for by name, once. */
    for (i = 0; i < n; i++) {
        if (recs[i].type == SEL_MDNS_T_PTR && strcmp(recs[i].name, service) == 0) {
            const sel_mdns_rec* srv = find_rec(recs, n, SEL_MDNS_T_SRV, recs[i].target);

            if (srv == NULL) {
                send_query(s, tip, tport, recs[i].target, SEL_MDNS_T_ANY);
                asked = 1;
            } else if (find_rec(recs, n, SEL_MDNS_T_A, srv->target) == NULL) {
                send_query(s, tip, tport, srv->target, SEL_MDNS_T_A);
                asked = 1;
            }
        }
    }
    if (asked) {
        collect(s, t0, wait_ms, dg, recs, &n);
        /* the SRV answer may name a host whose A record is still missing */
        for (i = 0; i < n; i++) {
            if (recs[i].type == SEL_MDNS_T_SRV && find_rec(recs, n, SEL_MDNS_T_A, recs[i].target) == NULL) {
                send_query(s, tip, tport, recs[i].target, SEL_MDNS_T_A);
            }
        }
        collect(s, t0, wait_ms + wait_ms / 2u, dg, recs, &n);
    }
    found = sel_mdns_resolve(recs, n, service, out, max);
    sel_usock_close(s);
    free(dg);
    free(recs);
    return (int)found;
}

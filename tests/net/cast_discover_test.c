/* I892e1: Chromecast mDNS discovery. Part 1 (always): the mDNS parser on a response packet built here with name
 * compression (PTR rdata, SRV/TXT/A owners and the SRV target all use pointers), plus the query encoder. Part 2 (argv: ip,
 * UDP port): sel_cast_discover against the loopback responder tests/net/mdns_server.py. */
#include "cast/cast_discover.h"
#include "mdns/mdns.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static size_t put16(unsigned char* b, size_t o, unsigned v) {
    b[o] = (unsigned char)(v >> 8);
    b[o + 1] = (unsigned char)v;
    return o + 2;
}

static size_t put_label(unsigned char* b, size_t o, const char* s) {
    size_t l = strlen(s);
    b[o++] = (unsigned char)l;
    memcpy(b + o, s, l);
    return o + l;
}

static size_t put_ptr(unsigned char* b, size_t o, size_t to) { return put16(b, o, 0xC000u | (unsigned)to); }

/* record header after the owner: type, class IN (+cache flush), ttl 120, rdlength placeholder; returns offset of rdlength */
static size_t rr_head(unsigned char* b, size_t o, unsigned type) {
    o = put16(b, o, type);
    o = put16(b, o, 0x8001u);
    b[o++] = 0;
    b[o++] = 0;
    b[o++] = 0;
    b[o++] = 120;
    return o;
}

static int build_packet(unsigned char* b) {
    size_t o = 12, svc, local, inst, host, rl, rs;

    memset(b, 0, 12);
    b[2] = 0x84; /* response, authoritative */
    b[7] = 1;    /* ANCOUNT */
    b[11] = 3;   /* ARCOUNT */
    svc = o;
    o = put_label(b, o, "_googlecast");
    o = put_label(b, o, "_tcp");
    local = o;
    o = put_label(b, o, "local");
    b[o++] = 0;
    /* PTR: owner = full name, rdata = label + pointer to the service name */
    o = rr_head(b, o, SEL_MDNS_T_PTR);
    rl = o;
    o += 2;
    rs = o;
    inst = o;
    o = put_label(b, o, "Chromecast-abc123");
    o = put_ptr(b, o, svc);
    put16(b, rl, (unsigned)(o - rs));
    /* additional SRV: owner = pointer to the instance name */
    o = put_ptr(b, o, inst);
    o = rr_head(b, o, SEL_MDNS_T_SRV);
    rl = o;
    o += 2;
    rs = o;
    o = put16(b, o, 0);
    o = put16(b, o, 0);
    o = put16(b, o, 8009);
    host = o;
    o = put_label(b, o, "abc123");
    o = put_ptr(b, o, local);
    put16(b, rl, (unsigned)(o - rs));
    /* additional TXT */
    o = put_ptr(b, o, inst);
    o = rr_head(b, o, SEL_MDNS_T_TXT);
    rl = o;
    o += 2;
    rs = o;
    o = put_label(b, o, "id=0123456789abcdef");
    o = put_label(b, o, "md=Chromecast Audio");
    o = put_label(b, o, "fn=Living Room");
    put16(b, rl, (unsigned)(o - rs));
    /* additional A: owner = pointer to the SRV target */
    o = put_ptr(b, o, host);
    o = rr_head(b, o, SEL_MDNS_T_A);
    o = put16(b, o, 4);
    b[o++] = 192;
    b[o++] = 168;
    b[o++] = 1;
    b[o++] = 77;
    return (int)o;
}

static int fail(const char* what) {
    printf("cast_discover_FAIL %s\n", what);
    return 1;
}

static int pure_part(void) {
    unsigned char pkt[512], q[128];
    sel_mdns_rec recs[8];
    sel_mdns_service svc[2];
    sel_cast_device dev[2];
    size_t n = 0, ql;
    int len = build_packet(pkt);

    if (sel_mdns_parse(pkt, (size_t)len, recs, &n, 8) != 0 || n != 4) {
        return fail("parse count");
    }
    if (strcmp(recs[0].name, "_googlecast._tcp.local") != 0 ||
        strcmp(recs[0].target, "Chromecast-abc123._googlecast._tcp.local") != 0) {
        return fail("ptr compression");
    }
    if (recs[1].port != 8009 || strcmp(recs[1].name, "Chromecast-abc123._googlecast._tcp.local") != 0 ||
        strcmp(recs[1].target, "abc123.local") != 0) {
        return fail("srv");
    }
    if (strcmp(recs[3].ip, "192.168.1.77") != 0 || strcmp(recs[3].name, "abc123.local") != 0) {
        return fail("a record");
    }
    if (sel_mdns_resolve(recs, n, SEL_CAST_SERVICE, svc, 2) != 1) {
        return fail("resolve");
    }
    if (sel_cast_devices_from_services(svc, 1, dev, 2) != 1 || strcmp(dev[0].name, "Living Room") != 0 ||
        strcmp(dev[0].id, "0123456789abcdef") != 0 || strcmp(dev[0].model, "Chromecast Audio") != 0 ||
        strcmp(dev[0].host, "192.168.1.77") != 0 || dev[0].port != 8009) {
        return fail("device fields");
    }
    /* a query is not a response */
    ql = sel_mdns_encode_query(q, sizeof q, SEL_CAST_SERVICE, SEL_MDNS_T_PTR);
    n = 0;
    if (ql != 12u + 1 + 11 + 1 + 4 + 1 + 5 + 1 + 4 || sel_mdns_parse(q, ql, recs, &n, 8) != -1) {
        return fail("query encode");
    }
    /* a pointer loop must not hang or crash */
    pkt[12] = 0xC0;
    pkt[13] = 12;
    n = 0;
    (void)sel_mdns_parse(pkt, (size_t)len, recs, &n, 8);
    printf("mdns_parse_ok\n");
    return 0;
}

int main(int argc, char** argv) {
    sel_cast_device d[4];
    int n;

    if (pure_part() != 0) {
        return 1;
    }
    if (argc < 3) {
        return 0;
    }
    n = sel_cast_discover(argv[1], (unsigned)atoi(argv[2]), 1500u, d, 4u);
    printf("cast_found=%d\n", n);
    if (n > 0) {
        printf("cast_name=%s\ncast_id=%s\ncast_model=%s\ncast_host=%s\ncast_port=%u\n", d[0].name, d[0].id, d[0].model,
               d[0].host, d[0].port);
    }
    return n < 0;
}

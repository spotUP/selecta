/* Multicast DNS service browsing (I892e1; shared by Chromecast _googlecast._tcp and later AirPlay _raop._tcp /
 * _airplay._tcp). Portable C: the packet codec is pure, the only OS surface is the UDP shim upnp/upnp_sock.h
 * (posix + bsdsocket backends), so it runs on the Amiga too.
 *
 * The browse sends a "legacy unicast" query (RFC 6762 section 6.7): from an ephemeral source port, so responders answer
 * to that port by unicast. No bind to 5353 and no group join are needed, which is also why the shim is unchanged. */
#ifndef SELECTA_SHARED_MDNS_H
#define SELECTA_SHARED_MDNS_H

#include <stddef.h>

#define SEL_MDNS_GROUP_IP "224.0.0.251"
#define SEL_MDNS_PORT 5353u

#define SEL_MDNS_T_A 1u
#define SEL_MDNS_T_PTR 12u
#define SEL_MDNS_T_TXT 16u
#define SEL_MDNS_T_SRV 33u
#define SEL_MDNS_T_ANY 255u

#define SEL_MDNS_NAME_MAX 128
#define SEL_MDNS_TXT_MAX 512

/* One resource record, decoded. Only the member that matches `type` is filled. Other types are skipped by the parser. */
typedef struct sel_mdns_rec {
    unsigned type;
    char name[SEL_MDNS_NAME_MAX];   /* owner name, dotted, no trailing dot */
    char target[SEL_MDNS_NAME_MAX]; /* PTR: the instance name; SRV: the host name */
    unsigned port;                  /* SRV */
    char ip[16];                    /* A: dotted quad */
    unsigned char txt[SEL_MDNS_TXT_MAX]; /* TXT: the raw length-prefixed strings */
    size_t txt_len;
} sel_mdns_rec;

/* Encodes a one-question query (class IN, QU bit clear) for name ("_googlecast._tcp.local") into out. Returns the byte
 * count, or 0 when it does not fit or a label is longer than 63 bytes. */
size_t sel_mdns_encode_query(unsigned char* out, size_t cap, const char* name, unsigned qtype);

/* Parses a response packet (answers, authority and additional sections; name compression followed) and appends the
 * PTR / SRV / TXT / A records to recs[*n]. Returns 0, or -1 for a malformed packet or one that is not a response. A
 * truncated record ends the parse; records read before it are kept. */
int sel_mdns_parse(const unsigned char* pkt, size_t len, sel_mdns_rec* recs, size_t* n, size_t max);

/* Looks up key in a TXT blob ("fn=Living Room"); copies the value into out. Returns 1 when found. */
int sel_mdns_txt_get(const unsigned char* txt, size_t len, const char* key, char* out, size_t cap);

/* A resolved service instance: PTR -> instance, SRV -> host + port, A -> ip, TXT -> txt. */
typedef struct sel_mdns_service {
    char instance[SEL_MDNS_NAME_MAX]; /* "Chromecast-abc123._googlecast._tcp.local" */
    char host[SEL_MDNS_NAME_MAX];
    char ip[16];
    unsigned port;
    unsigned char txt[SEL_MDNS_TXT_MAX];
    size_t txt_len;
} sel_mdns_service;

/* Joins records into services: one per distinct PTR instance of `service` that has an SRV and an A record. Returns the
 * count written to out. */
size_t sel_mdns_resolve(const sel_mdns_rec* recs, size_t n, const char* service, sel_mdns_service* out, size_t max);

/* Browses `service` ("_googlecast._tcp.local") for wait_ms and returns the resolved services. ip/port override the
 * target (NULL / 0 = 224.0.0.251:5353): a test points them at a unicast responder on loopback, where real multicast is
 * unreliable. An instance announced without its SRV or A record is asked for once more by name. Returns the count, or
 * -1 when no socket could be opened. */
int sel_mdns_browse(const char* ip, unsigned port, const char* service, unsigned wait_ms, sel_mdns_service* out,
                    size_t max);

#endif

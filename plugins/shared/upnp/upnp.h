/* UPnP / DLNA media server discovery (I894a1; ContentDirectory browse follows in I894a2). SSDP M-SEARCH for
 * urn:schemas-upnp-org:device:MediaServer:1, then the device description over src/library/net/http.c. Integer only,
 * bounded memory (one 16 KB description buffer at a time, at most `max` servers). */
#ifndef SELECTA_SHARED_UPNP_H
#define SELECTA_SHARED_UPNP_H

#include <stddef.h>

#define SEL_UPNP_MULTICAST_IP "239.255.255.250"
#define SEL_UPNP_SSDP_PORT 1900u
#define SEL_UPNP_URL_BYTES 256
#define SEL_UPNP_NAME_BYTES 96

typedef struct sel_upnp_server {
    char name[SEL_UPNP_NAME_BYTES];            /* friendlyName */
    char usn[SEL_UPNP_NAME_BYTES + 32];        /* USN of the reply */
    char location[SEL_UPNP_URL_BYTES];         /* device description URL */
    char control_url[SEL_UPNP_URL_BYTES];      /* absolute ContentDirectory controlURL */
} sel_upnp_server;

/* Parses an SSDP response (or NOTIFY) datagram: LOCATION and USN headers (case-insensitive names). 1 when the
 * datagram is a 200 response or NOTIFY with a LOCATION. usn may come back empty. */
int sel_upnp_parse_ssdp(const char* msg, size_t len, char* location, size_t loc_n, char* usn, size_t usn_n);

/* Parses a device description XML (NUL terminated): friendlyName and the controlURL of the ContentDirectory service,
 * resolved against URLBase when present, else against `location`. 1 when both were found. */
int sel_upnp_parse_description(const char* xml, const char* location, sel_upnp_server* out);

/* Resolves `ref` (absolute URL, absolute path or relative path) against `base` into out. 0 if it does not fit. */
int sel_upnp_resolve(const char* base, const char* ref, char* out, size_t n);

/* Sends an M-SEARCH to ip:port (NULL / 0 select 239.255.255.250:1900; a test passes the unicast fixture address),
 * collects replies for wait_ms, fetches each distinct LOCATION and fills `out` with the servers that have a
 * ContentDirectory. Returns the count (0 when none, -1 when the socket cannot open). */
int sel_upnp_discover(const char* ip, unsigned port, unsigned wait_ms, sel_upnp_server* out, size_t max);

/* ---- ContentDirectory browse (I894a2) ---- */
#define SEL_UPNP_ID_BYTES 64
#define SEL_UPNP_TEXT_BYTES 96
#define SEL_UPNP_BROWSE_PAGE 100u /* RequestedCount sent per page; the caller's `max` caps a page too */

typedef struct sel_upnp_entry {
    int is_container;                        /* 1 container, 0 item */
    char id[SEL_UPNP_ID_BYTES];
    char title[SEL_UPNP_TEXT_BYTES];         /* dc:title */
    char artist[SEL_UPNP_TEXT_BYTES];        /* upnp:artist, else dc:creator; may be empty */
    char album[SEL_UPNP_TEXT_BYTES];         /* upnp:album; may be empty */
    unsigned long duration_ms;               /* res@duration "H+:MM:SS[.mmm]"; 0 when absent */
    unsigned child_count;                    /* container@childCount; 0 when absent */
    char res_url[SEL_UPNP_URL_BYTES];        /* first <res> URL (items) */
    char protocol_info[SEL_UPNP_TEXT_BYTES]; /* first <res>@protocolInfo */
} sel_upnp_entry;

/* "H:MM:SS" or "H:MM:SS.mmm" to milliseconds (a fraction like "F0/F1" is ignored). 1 on success. */
int sel_upnp_parse_duration(const char* s, unsigned long* ms);

/* Parses an entity-decoded DIDL-Lite document: <container> and <item> elements in order, at most `max`.
 * Returns the number of entries filled. */
int sel_upnp_parse_didl(const char* didl, sel_upnp_entry* out, size_t max);

/* Parses a Browse SOAP response: the entity-escaped Result is decoded and handed to sel_upnp_parse_didl;
 * NumberReturned / TotalMatches are read. Returns the entries filled, -1 when the response is malformed or the
 * Result does not fit the fixed 64 KB decode buffer. */
int sel_upnp_parse_browse(const char* soap, sel_upnp_entry* out, size_t max, unsigned* returned, unsigned* total);

/* One BrowseDirectChildren page of object_id from control_url: entries [start, start + count), count clamped to
 * SEL_UPNP_BROWSE_PAGE and `max`. *total receives TotalMatches; the caller pages with start += return value until
 * start >= *total (or the return is 0). Returns the entries filled, -1 on a transport or parse error. */
int sel_upnp_browse(const char* control_url, const char* object_id, unsigned start, unsigned count,
                    sel_upnp_entry* out, size_t max, unsigned* total);

#endif

/* Publishes a playlist to hippoplayer.se (plan Phase 4, I164; design doc 17). The body is the Hippo `.prg`
 * export of the queue; the metadata travels in the query string. The real service format is UNPROVEN: this
 * header holds the whole assumption (URL, method, query keys, reply grammar). Change them here only. */
#ifndef SELECTA_LIBRARY_SOURCES_HIPPO_PUBLISH_H
#define SELECTA_LIBRARY_SOURCES_HIPPO_PUBLISH_H

#include <stddef.h>

/* ASSUMED: POST <url>?title=..&author=..&genre=..&filename=.. with the `.prg` bytes as the body
 * (Content-Type application/octet-stream). The environment variable replaces the URL (tests point it at the
 * fixture; SELECTA_NET_MAP does the host rewrite). */
#define SEL_HIPPO_PUBLISH_URL "https://hippoplayer.se/api/v1/playlists/publish"
#define SEL_HIPPO_PUBLISH_ENV "SELECTA_HIPPO_PUBLISH_URL"
#define SEL_HIPPO_PUBLISH_MAX 524288u /* 512 KB: the server limit, checked before any request */
/* ASSUMED reply, one line: `OK <id> <filename> <url>`; anything else is a rejection. */

enum {
    SEL_HIPPO_PUB_OK = 0,
    SEL_HIPPO_PUB_TOO_LARGE = -1, /* body over SEL_HIPPO_PUBLISH_MAX: no request made */
    SEL_HIPPO_PUB_ARGS = -2,      /* empty filename or body */
    SEL_HIPPO_PUB_NET = -3,       /* transport failure */
    SEL_HIPPO_PUB_STATUS = -4,    /* HTTP status other than 2xx */
    SEL_HIPPO_PUB_REPLY = -5      /* reply is not `OK <id> <filename> <url>` */
};

typedef struct sel_hippo_publish_reply {
    char id[24];
    char filename[128];
    char url[256];
    int http_status;
} sel_hippo_publish_reply;

/* POSTs `body` (len bytes). Counts every POST it sends in sel_hippo_publish_posts(). */
int sel_hippo_publish(const char* body, size_t len, const char* title, const char* author, const char* genre,
                      const char* filename, sel_hippo_publish_reply* reply);
unsigned sel_hippo_publish_posts(void);

#endif

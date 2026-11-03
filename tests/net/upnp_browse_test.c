/* I894a2: browses the fixture's ContentDirectory (tests/net/upnp_server.py). argv: control URL.
 * Lists the root, then pages through container "1" (151 children, the server returns at most 100 per page) and prints
 * counts plus one item's fields; the cmake script asserts them exactly. */
#include "upnp/upnp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char** argv) {
    sel_upnp_entry* e;
    unsigned total = 0, start = 0, pages = 0, nc = 0, ni = 0, i;
    int n;

    if (argc < 2) {
        return 2;
    }
    e = (sel_upnp_entry*)malloc(SEL_UPNP_BROWSE_PAGE * sizeof *e);
    if (e == NULL) {
        return 2;
    }
    n = sel_upnp_browse(argv[1], "0", 0u, SEL_UPNP_BROWSE_PAGE, e, SEL_UPNP_BROWSE_PAGE, &total);
    printf("upnp_root n=%d total=%u\n", n, total);
    for (i = 0; n > 0 && i < (unsigned)n; i++) {
        printf("upnp_root_container id=%s title=%s children=%u\n", e[i].id, e[i].title, e[i].child_count);
    }
    do {
        n = sel_upnp_browse(argv[1], "1", start, SEL_UPNP_BROWSE_PAGE, e, SEL_UPNP_BROWSE_PAGE, &total);
        if (n < 0) {
            printf("upnp_browse_error start=%u\n", start);
            return 1;
        }
        pages++;
        for (i = 0; i < (unsigned)n; i++) {
            if (e[i].is_container) {
                nc++;
            } else {
                ni++;
            }
            if (strcmp(e[i].id, "1$042") == 0) {
                printf("upnp_item id=%s\nupnp_title=%s\nupnp_artist=%s\nupnp_album=%s\nupnp_ms=%lu\nupnp_res=%s\n"
                       "upnp_proto=%s\n",
                       e[i].id, e[i].title, e[i].artist, e[i].album, e[i].duration_ms, e[i].res_url,
                       e[i].protocol_info);
            }
        }
        start += (unsigned)n;
    } while (n > 0 && start < total);
    printf("upnp_pages=%u containers=%u items=%u total=%u\n", pages, nc, ni, total);
    free(e);
    return 0;
}

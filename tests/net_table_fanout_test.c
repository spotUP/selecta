/* Regression (I1044): the all-sources search runs one thread per searchable source, each with its own connection,
 * while playback streams, the prefetch downloads, a sync runs and the DLNA server listens and serves one client.
 * The POSIX connection table held 8 slots, sized for the serial search: with 7 searchable sources the 9th user got
 * SEL_NET_E_TABLE and one source's search failed at random. Every user of the fan-out must get a slot at once. */
#include "net/net.h"
#include "source.h"

#include <selecta/host.h>
#include <selecta/registry.h>

#include <stdio.h>

#define OTHER_USERS 5 /* stream, prefetch, sync, DLNA listener, one DLNA client */

int main(void) {
    int h[64];
    uint32_t i;
    uint32_t searchable = 0;
    uint32_t want;
    uint32_t got = 0;
    int fail = 0;

    /* The network sources are plugins: load the real registry and hand it to the source table, as the engine does. */
    if (sel_registry_init(sel_host_default()) == 0) {
        printf("FAIL registry\n");
        return 1;
    }
    sel_source_set_plugins(sel_registry_count, sel_registry_get, sel_host_default());
    sel_source_register_builtin();
    for (i = 0; i < sel_source_count(); i++) {
        const sel_source* s = sel_source_get(i);
        if (s != NULL && s->search != NULL) {
            searchable++;
        }
    }
    want = searchable + OTHER_USERS;
    if (searchable == 0 || want > sizeof h / sizeof h[0]) {
        printf("FAIL searchable=%u\n", searchable);
        return 1;
    }
    for (i = 0; i < want; i++) {
        unsigned port = 0;
        h[i] = sel_net_listen(&port);
        if (h[i] < 0) {
            printf("FAIL handle %u of %u: %d (table %d)\n", i + 1, want, h[i], SEL_NET_MAX_CONN);
            fail = 1;
            break;
        }
        got++;
    }
    for (i = 0; i < got; i++) {
        sel_net_close(h[i]);
    }
    if (fail) {
        return 1;
    }
    printf("net_table_fanout_ok searchable=%u handles=%u table=%d\n", searchable, want, SEL_NET_MAX_CONN);
    return 0;
}

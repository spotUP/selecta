/* Amiga bsdsocket backend of the SSDP socket shim (upnp_sock.h, I1004). Every call goes through the one Amiga socket
 * layer (src/core/amiga/net.c, declared in net_udp.h); there is no second socket code here. */
#include "upnp/upnp_sock.h"

#include <stdlib.h>

#include "net_udp.h"
#include "../../../src/library/net/net.h"

#define SSDP_GROUP "239.255.255.250"

struct sel_usock {
    int h;
};

sel_usock* sel_usock_open(void) {
    sel_usock* s = (sel_usock*)malloc(sizeof *s);
    if (s == NULL) {
        return NULL;
    }
    s->h = sel_net_udp_open();
    if (s->h < 0) {
        free(s);
        return NULL;
    }
    /* Replies to an M-SEARCH are unicast, so a stack without multicast membership still discovers servers; the join only
     * adds the servers' own NOTIFY datagrams. Its result is ignored for that reason. */
    (void)sel_net_udp_multicast(s->h, SSDP_GROUP, 4);
    return s;
}

int sel_usock_send(sel_usock* s, const char* ip, unsigned port, const void* data, size_t len) {
    return sel_net_udp_send(s->h, ip, port, data, len) == 0;
}

long sel_usock_recv(sel_usock* s, void* buf, size_t cap, unsigned timeout_ms, char* from) {
    int n = sel_net_udp_recv(s->h, buf, cap, timeout_ms, from, 16, NULL);
    return n < 0 ? -1 : (long)n;
}

void sel_usock_close(sel_usock* s) {
    if (s != NULL) {
        sel_net_close(s->h);
        free(s);
    }
}

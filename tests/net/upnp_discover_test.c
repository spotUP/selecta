/* I894a1: runs sel_upnp_discover against the unicast SSDP fixture (tests/net/upnp_server.py) and prints the result.
 * argv: ip, SSDP port. The cmake script asserts the output and the fixture's request log. */
#include "upnp/upnp.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char** argv) {
    sel_upnp_server s[4];
    int n;

    if (argc < 3) {
        return 2;
    }
    n = sel_upnp_discover(argv[1], (unsigned)atoi(argv[2]), 1500u, s, 4u);
    printf("upnp_found=%d\n", n);
    if (n > 0) {
        printf("upnp_name=%s\nupnp_control=%s\nupnp_usn=%s\nupnp_location=%s\n", s[0].name, s[0].control_url, s[0].usn,
               s[0].location);
    }
    return n < 0;
}

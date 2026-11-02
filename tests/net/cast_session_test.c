/* I892e1: drives the CASTV2 session against the TLS fixture (tests/net/cast_server.py) and prints the result.
 * argv: ip, port. The cmake script asserts the output and the fixture's request log. */
#include "cast/cast_session.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char** argv) {
    sel_cast c;
    int rc, i, rounds = 0;

    if (argc < 3) {
        return 2;
    }
    rc = sel_cast_open(&c, argv[1], (unsigned)atoi(argv[2]), 3000u);
    printf("cast_open=%d\n", rc);
    if (rc != SEL_CAST_OK) {
        return 1;
    }
    for (i = 0; i < 3; i++) {
        if (sel_cast_heartbeat(&c, 3000u) == SEL_CAST_OK) {
            rounds++;
        }
    }
    printf("cast_heartbeats=%d\ncast_pongs=%u\ncast_pings_answered=%u\n", rounds, c.pongs, c.pings_answered);
    rc = sel_cast_get_status(&c, 3000u);
    printf("cast_status=%d\ncast_type=%s\ncast_request=%ld\ncast_apps=%d\ncast_app=%s\ncast_name=%s\ncast_transport=%s\n"
           "cast_session=%s\ncast_volume=%.2f\ncast_muted=%d\n",
           rc, c.status.type, c.status.request_id, c.status.app_count, c.status.app_id, c.status.display_name,
           c.status.transport_id, c.status.session_id, c.status.volume_level, c.status.volume_muted);
    sel_cast_close(&c);
    return rc != SEL_CAST_OK || rounds != 3;
}

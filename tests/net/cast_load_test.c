/* I892e2: launches the Default Media Receiver on the TLS fixture (tests/net/cast_server.py), serves a file over the
 * built-in HTTP server, LOADs its URL and drives PLAY / PAUSE / SEEK / SET_VOLUME. argv: ip, port, file.
 * The cmake script asserts the output and the fixture's log (what the receiver fetched). */
#include "cast/cast_session.h"

#include <stdio.h>
#include <stdlib.h>

#include "net/net.h"

int main(int argc, char** argv) {
    sel_cast c;
    sel_cast_http srv;
    char ip[40], url[256];
    int rc;

    if (argc < 4) {
        return 2;
    }
    rc = sel_cast_open(&c, argv[1], (unsigned)atoi(argv[2]), 3000u);
    printf("cast_open=%d\n", rc);
    if (rc != SEL_CAST_OK) {
        return 1;
    }
    rc = sel_cast_launch(&c, SEL_CAST_APP_DEFAULT_MEDIA, 5000u);
    printf("cast_launch=%d\ncast_transport=%s\n", rc, c.transport);
    rc |= sel_net_local_ip(c.h, ip, sizeof ip);
    rc |= sel_cast_http_open(&srv, argv[3], "audio/mpeg");
    rc |= sel_cast_http_url(&srv, ip, url, sizeof url);
    printf("cast_local_ip=%s\ncast_url_prefix=%.7s\n", ip, url);
    if (rc != 0) {
        return 1;
    }
    rc = sel_cast_load(&c, &srv, url, "audio/mpeg", 15000u);
    printf("cast_load=%d\ncast_loads=%u\ncast_http_requests=%u\ncast_session_id=%ld\ncast_state=%s\ncast_time=%.1f\n"
           "cast_duration=%.1f\n",
           rc, c.loads, srv.requests, c.media.media_session_id, c.media.player_state, c.media.current_time,
           c.media.duration);
    printf("cast_http_bytes=%lu\n", srv.bytes_sent);
    printf("cast_pause=%d\n", sel_cast_pause(&c, 3000u));
    printf("cast_state_paused=%s\n", c.media.player_state);
    printf("cast_seek=%d\n", sel_cast_seek(&c, 42.5, 3000u));
    printf("cast_seek_time=%.1f\n", c.media.current_time);
    printf("cast_play=%d\n", sel_cast_play(&c, 3000u));
    printf("cast_state_playing=%s\n", c.media.player_state);
    printf("cast_volume=%d\ncast_volume_level=%.2f\n", sel_cast_set_volume(&c, 0.25, 3000u), c.status.volume_level);
    sel_cast_http_close(&srv);
    sel_cast_close(&c);
    return rc != SEL_CAST_OK || c.loads != 1;
}

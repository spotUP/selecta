/* I1401d: upnp:albumArtURI of a DIDL-Lite item lands in sel_upnp_entry.art_url (first one, profileID ignored); an item without stays empty. */
#include "upnp/upnp.h"

#include <stdio.h>
#include <string.h>

int main(void) {
    static const char didl[] =
        "<DIDL-Lite xmlns:upnp=\"urn:schemas-upnp-org:metadata-1-0/upnp/\" xmlns:dlna=\"urn:schemas-dlna-org:metadata-1-0/\">"
        "<item id=\"a\" parentID=\"0\"><dc:title>With</dc:title><upnp:album>Alb</upnp:album>"
        "<upnp:albumArtURI dlna:profileID=\"JPEG_TN\">http://192.168.0.198:50001/art/1.jpg?a=1&amp;b=2</upnp:albumArtURI>"
        "<upnp:albumArtURI dlna:profileID=\"JPEG_LRG\">http://192.168.0.198:50001/art/2.jpg</upnp:albumArtURI>"
        "<res protocolInfo=\"http-get:*:audio/mpeg:*\">http://192.168.0.198:50001/m/1.mp3</res></item>"
        "<item id=\"b\" parentID=\"0\"><dc:title>Without</dc:title><upnp:album>Alb</upnp:album>"
        "<res protocolInfo=\"http-get:*:audio/mpeg:*\">http://192.168.0.198:50001/m/2.mp3</res></item></DIDL-Lite>";
    sel_upnp_entry e[2];
    int n = sel_upnp_parse_didl(didl, e, 2);

    if (n != 2 || strcmp(e[0].art_url, "http://192.168.0.198:50001/art/1.jpg?a=1&b=2") != 0 || e[1].art_url[0] != '\0') {
        printf("upnp_didl_art_fail n=%d a=[%s] b=[%s]\n", n, n > 0 ? e[0].art_url : "", n > 1 ? e[1].art_url : "");
        return 1;
    }
    printf("upnp_didl_art_ok\n");
    return 0;
}

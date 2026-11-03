/* Chromecast discovery (I892e1): mDNS browse of _googlecast._tcp.local through plugins/shared/mdns. */
#ifndef SELECTA_SHARED_CAST_DISCOVER_H
#define SELECTA_SHARED_CAST_DISCOVER_H

#include <stddef.h>

#include "mdns/mdns.h"

#define SEL_CAST_SERVICE "_googlecast._tcp.local"

typedef struct sel_cast_device {
    char name[64];  /* TXT fn= (friendly name); the instance label when absent */
    char id[48];    /* TXT id= */
    char model[48]; /* TXT md= */
    char host[16];  /* IPv4, dotted quad */
    unsigned port;  /* SRV port, normally 8009 */
} sel_cast_device;

/* Maps resolved mDNS services to devices (pure; the unit test drives it). */
size_t sel_cast_devices_from_services(const sel_mdns_service* svc, size_t n, sel_cast_device* out, size_t max);

/* Browses for Chromecast receivers for wait_ms. ip/port override the multicast target (NULL / 0 = 224.0.0.251:5353),
 * only the tests use it. Returns the number of devices, or -1 when no socket could be opened. */
int sel_cast_discover(const char* ip, unsigned port, unsigned wait_ms, sel_cast_device* out, size_t max);

#endif

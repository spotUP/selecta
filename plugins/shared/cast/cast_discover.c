/* Chromecast discovery over mDNS (cast_discover.h, I892e1). */
#include "cast/cast_discover.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

size_t sel_cast_devices_from_services(const sel_mdns_service* svc, size_t n, sel_cast_device* out, size_t max) {
    size_t i, k = 0;

    for (i = 0; i < n && k < max; i++) {
        sel_cast_device* d = &out[k++];
        const char* dot;

        memset(d, 0, sizeof *d);
        if (!sel_mdns_txt_get(svc[i].txt, svc[i].txt_len, "fn", d->name, sizeof d->name)) {
            dot = strchr(svc[i].instance, '.');
            snprintf(d->name, sizeof d->name, "%.*s", dot != NULL ? (int)(dot - svc[i].instance) : 40, svc[i].instance);
        }
        (void)sel_mdns_txt_get(svc[i].txt, svc[i].txt_len, "id", d->id, sizeof d->id);
        (void)sel_mdns_txt_get(svc[i].txt, svc[i].txt_len, "md", d->model, sizeof d->model);
        snprintf(d->host, sizeof d->host, "%s", svc[i].ip);
        d->port = svc[i].port;
    }
    return k;
}

int sel_cast_discover(const char* ip, unsigned port, unsigned wait_ms, sel_cast_device* out, size_t max) {
    sel_mdns_service* svc = (sel_mdns_service*)malloc(max * sizeof *svc);
    int n;

    if (svc == NULL) {
        return -1;
    }
    n = sel_mdns_browse(ip, port, SEL_CAST_SERVICE, wait_ms, svc, max);
    if (n > 0) {
        n = (int)sel_cast_devices_from_services(svc, (size_t)n, out, max);
    }
    free(svc);
    return n;
}

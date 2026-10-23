/* Guard and host-map parsing shared by the POSIX and Amiga socket tables
 * (net.h). Pure string code, no OS calls. */
#include "net.h"

#include <stdlib.h>
#include <string.h>

static int ieq(const char* a, size_t alen, const char* b) {
    size_t i;
    if (strlen(b) != alen) {
        return 0;
    }
    for (i = 0; i < alen; i++) {
        char x = a[i], y = b[i];
        if (x >= 'A' && x <= 'Z') {
            x = (char)(x + 32);
        }
        if (y >= 'A' && y <= 'Z') {
            y = (char)(y + 32);
        }
        if (x != y) {
            return 0;
        }
    }
    return 1;
}

int sel_net_allowed(const char* list, const char* host) {
    const char* p = list;
    if (list == NULL || list[0] == '\0') {
        return 1;
    }
    while (*p != '\0') {
        const char* e = strchr(p, ',');
        size_t n = e != NULL ? (size_t)(e - p) : strlen(p);
        if (n > 0 && ieq(p, n, host)) {
            return 1;
        }
        p += n;
        if (*p == ',') {
            p++;
        }
    }
    return 0;
}

int sel_net_map_lookup(const char* map, const char* host, char* addr, size_t addr_cap, unsigned* port) {
    const char* p = map;
    if (map == NULL) {
        return 0;
    }
    while (*p != '\0') {
        const char* e = strchr(p, ',');
        size_t n = e != NULL ? (size_t)(e - p) : strlen(p);
        const char* eq = memchr(p, '=', n);
        if (eq != NULL && ieq(p, (size_t)(eq - p), host)) {
            const char* v = eq + 1;
            const char* end = p + n;
            const char* colon;
            size_t alen;
            if ((size_t)(end - v) > 7 && strncmp(v, "http://", 7) == 0) {
                v += 7;
            }
            colon = memchr(v, ':', (size_t)(end - v));
            alen = colon != NULL ? (size_t)(colon - v) : (size_t)(end - v);
            if (alen == 0 || alen >= addr_cap) {
                return 0;
            }
            memcpy(addr, v, alen);
            addr[alen] = '\0';
            *port = colon != NULL ? (unsigned)strtoul(colon + 1, NULL, 10) : 80u;
            return 1;
        }
        p += n;
        if (*p == ',') {
            p++;
        }
    }
    return 0;
}

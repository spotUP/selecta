/* HTTP download shared by the network source syncs (modland_net.c, hvsc_net.c). */
#ifndef SELECTA_LIBRARY_SOURCES_SYNC_NET_H
#define SELECTA_LIBRARY_SOURCES_SYNC_NET_H

#include <stdint.h>

/* GETs `url` into `path` through the HTTP client. Returns the HTTP status (0 on a transport failure); any result
 * but 200 removes the file. `*requests` gains the requests made (redirects count), `*bytes` is the body size. */
int sel_sync_net_get_file(const char* url, const char* path, uint32_t* requests, unsigned long* bytes);

#endif

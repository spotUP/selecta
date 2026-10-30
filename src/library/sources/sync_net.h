/* HTTP download shared by the network source syncs (modland_net.c, hvsc_net.c). */
#ifndef SELECTA_LIBRARY_SOURCES_SYNC_NET_H
#define SELECTA_LIBRARY_SOURCES_SYNC_NET_H

#include <selecta/host.h>
#include <stdint.h>

/* GETs `url` into `path` through the HTTP client. Returns the HTTP status (0 on a transport failure); any result
 * but 200 removes the file. `*requests` gains the requests made (redirects count), `*bytes` is the body size. */
int sel_sync_net_get_file(const char* url, const char* path, uint32_t* requests, unsigned long* bytes);

/* sel_sync_net_get_file, then `check(path)` on a 200 body (1 keeps it). A refused body removes the file and the call
 * returns SEL_NET_CONTENT_REJECTED (-2): a mirror that answers 200 with an HTML error page must not leave it behind
 * under a data file's name (I718). A NULL `check` keeps every 200. */
int sel_sync_net_get_file_checked(const char* url, const char* path, int (*check)(const char* path),
                                  uint32_t* requests, unsigned long* bytes);

/* GETs bytes `from`..`to` (inclusive) of `url` into `path` with one Range request. Returns the HTTP status; anything
 * but 206 removes the file (a 200 would be the whole file). */
int sel_sync_net_get_range(const char* url, const char* path, unsigned long from, unsigned long to,
                           uint32_t* requests, unsigned long* bytes);

/* HEAD `url`; 1 and `*length` = Content-Length on a 200 that carries one, else 0. */
int sel_sync_net_head_length(const char* url, uint32_t* requests, unsigned long* length);

/* Waits `ms` milliseconds between two requests to one host (a sync that reads many pages stays gentle). Sleeps on a
 * POSIX host; elsewhere it polls the host clock (the Amiga build has no sleep call in this layer). */
void sel_sync_net_pause(const sel_host* host, uint32_t ms);

#endif

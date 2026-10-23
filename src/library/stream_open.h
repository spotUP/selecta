/* URL and pipe song sources for OPEN (I195, T94). An http(s) URL is downloaded through the one HTTP client
 * (net/http.c, honouring SELECTA_NET_ALLOW/MAP and the thread's sel_net_set_cancel flag); `pipe:<command>` runs the
 * command on the desktop and reads its standard output. On the Amiga `PIPE:` is an AmigaDOS device, so it is NOT a
 * stream path here: it goes through the ordinary file path (fopen). */
#ifndef SELECTA_LIBRARY_STREAM_OPEN_H
#define SELECTA_LIBRARY_STREAM_OPEN_H

#include <selecta/host.h>

#include <stddef.h>
#include <stdint.h>

int sel_stream_is_url(const char* path);
int sel_stream_is_pipe(const char* path); /* desktop `pipe:<cmd>` only; always 0 on the Amiga */

/* The whole stream in host memory (mem_free to release), size in *size; NULL on failure or a non-2xx status.
 * Counters below move on each call (the test reads them). */
uint8_t* sel_stream_read(const sel_host* host, const char* path, size_t* size);

uint32_t sel_stream_bytes(void);  /* bytes delivered by the last successful read */
uint32_t sel_stream_reads(void);  /* successful reads */

#endif

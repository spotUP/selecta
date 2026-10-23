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

/* Live streams (I195b): an http(s) URL played while it downloads. A download thread feeds a bounded byte ring
 * (SEL_STREAM_RING bytes, so an endless radio stream holds no more than that); the reader never blocks except in
 * the pump-side waits below. The thread honours sel_net_set_cancel through its own stop flag, so sel_live_close
 * ends the download within one poll slice. Desktop only: the Amiga build has no download thread until its sockets
 * come with I199, sel_live_open returns NULL there and OPEN reads the whole file as before. */
#include <selecta/replayer.h>

typedef struct sel_live sel_live;

#define SEL_STREAM_HEAD 16384u /* bytes handed to the format probe before the rest has arrived */

/* Starts the download. `gen`/`start` (the engine's cancel generation) end the head wait below when a newer
 * STOP or OPEN arrives. NULL when streaming is not available or the thread did not start. */
sel_live* sel_live_open(const sel_host* host, const char* url, const volatile unsigned* gen, unsigned start);
/* Waits until `want` bytes arrived or the stream ended, then copies up to `want` of the first bytes WITHOUT
 * consuming them (*n = how many). Returns 0 when the download failed before any byte or the wait was cancelled. */
int sel_live_head(sel_live* s, uint8_t* dst, size_t want, size_t* n);
/* The consumer view handed to a SEL_RP_STREAMING replayer; valid until sel_live_close. */
const sel_byte_stream* sel_live_source(sel_live* s);
/* Reads the rest of the stream into one host buffer (the replayer cannot stream): memory grows with the file,
 * capped like sel_stream_read. NULL on failure. */
uint8_t* sel_live_drain(sel_live* s, size_t* size);
/* Sentinel: records bytes received so far and whether the download had finished, at the first audio frame. */
void sel_live_note_audio(sel_live* s);
void sel_live_close(sel_live* s);

uint32_t sel_stream_live_opens(void);   /* streams started */
uint32_t sel_stream_audio_bytes(void);  /* bytes received when the first audio frame was rendered (last stream) */
uint32_t sel_stream_audio_done(void);   /* 1 when the download had already finished at that moment */
uint32_t sel_stream_received(void);     /* bytes received by the last closed stream */

#endif

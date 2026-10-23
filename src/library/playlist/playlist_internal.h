/* Shared by playlist.c and the codecs under formats/. */
#ifndef SELECTA_LIBRARY_PLAYLIST_INTERNAL_H
#define SELECTA_LIBRARY_PLAYLIST_INTERNAL_H

#include "playlist.h"

typedef struct {
    sel_pl_sink_fn fn;
    void* ctx;
    int err;
} sel_pl_out;

void sel_pl_put(sel_pl_out* o, const char* s, size_t len);
void sel_pl_puts(sel_pl_out* o, const char* s);
void sel_pl_putu(sel_pl_out* o, uint32_t v);

/* Line iterator: returns 1 and sets [*line, *line+*len) without the line end
 * (LF, CRLF or CR), advancing *pos; 0 at the end. */
int sel_pl_next_line(const char* bytes, size_t total, size_t* pos, const char** line, size_t* len);
/* Parses a run of decimal digits at s (up to len); returns the value and sets *used. */
uint32_t sel_pl_parse_u(const char* s, size_t len, size_t* used);

int sel_pl_read_hippo(const char* bytes, size_t len, sel_playlist* pl);
int sel_pl_read_m3u(const char* bytes, size_t len, sel_playlist* pl);
int sel_pl_read_pls(const char* bytes, size_t len, sel_playlist* pl);
int sel_pl_read_xspf(const char* bytes, size_t len, sel_playlist* pl);
int sel_pl_write_hippo(const sel_playlist* pl, sel_pl_out* o, sel_pl_warn_fn warn, void* warn_ctx);
int sel_pl_write_m3u(const sel_playlist* pl, sel_pl_out* o, sel_pl_warn_fn warn, void* warn_ctx);
int sel_pl_write_pls(const sel_playlist* pl, sel_pl_out* o, sel_pl_warn_fn warn, void* warn_ctx);
int sel_pl_write_xspf(const sel_playlist* pl, sel_pl_out* o, sel_pl_warn_fn warn, void* warn_ctx);

#endif

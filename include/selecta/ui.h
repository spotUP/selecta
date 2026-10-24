/* UI plugin hooks, stored in the descriptor's PLUGIN_TAGS list under the
 * UI_* tags (design doc section 7). A UI renders sel_engine_state() and sends
 * commands; it holds no playback logic. */
#ifndef SELECTA_UI_H
#define SELECTA_UI_H

#include <selecta/engine.h>

SEL_EXTERN_C_BEGIN

/* Album artwork source the shell fills from the library (design doc 2.7). Calls may come from any thread. The
 * blobs are the encoded cover (kind "cover") and the written-back thumbnails (kind "thumb<size>"). */
typedef struct sel_ui_art {
    void* ctx;
    int64_t (*find)(void* ctx, const char* song_ref);                                          /* album id of the song a queue path (or song key) names, 0 = none */
    uint32_t (*get)(void* ctx, int64_t album, const char* kind, uint8_t* out, uint32_t cap);   /* blob size, 0 = none */
    int (*put)(void* ctx, int64_t album, const char* kind, const uint8_t* data, uint32_t len); /* 1 = stored */
} sel_ui_art;

/* Passed to UI_OPEN. `library` is reserved and NULL until M3 creates
 * sel_library. `argv` carries the UI's own options. */
typedef struct sel_ui_open_args {
    sel_engine* engine;
    const sel_host* host;
    int argc;
    const char* const* argv;
    void* library;
    const sel_ui_art* art; /* NULL without a library */
} sel_ui_open_args;

typedef void* (*sel_ui_open_fn)(const sel_ui_open_args* args);               /* UI_OPEN: state or NULL */
typedef void (*sel_ui_on_event_fn)(void* state, const sel_message* evt);     /* UI_ON_EVENT */
/* UI_WAIT: blocks up to timeout_ms; 0 = timeout, 1 = input handled, -1 = quit. */
typedef int (*sel_ui_wait_fn)(void* state, uint32_t timeout_ms);
typedef void (*sel_ui_close_fn)(void* state);                                /* UI_CLOSE */
typedef const sel_tag* (*sel_ui_stats_fn)(void* state);                      /* UI_STATS */

SEL_EXTERN_C_END

#endif /* SELECTA_UI_H */

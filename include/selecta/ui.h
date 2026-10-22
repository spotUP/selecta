/* UI plugin hooks, stored in the descriptor's PLUGIN_TAGS list under the
 * UI_* tags (design doc section 7). A UI renders sel_engine_state() and sends
 * commands; it holds no playback logic. */
#ifndef SELECTA_UI_H
#define SELECTA_UI_H

#include <selecta/engine.h>

SEL_EXTERN_C_BEGIN

/* Passed to UI_OPEN. `library` is reserved and NULL until M3 creates
 * sel_library. `argv` carries the UI's own options. */
typedef struct sel_ui_open_args {
    sel_engine* engine;
    const sel_host* host;
    int argc;
    const char* const* argv;
    void* library;
} sel_ui_open_args;

typedef void* (*sel_ui_open_fn)(const sel_ui_open_args* args);               /* UI_OPEN: state or NULL */
typedef void (*sel_ui_on_event_fn)(void* state, const sel_message* evt);     /* UI_ON_EVENT */
/* UI_WAIT: blocks up to timeout_ms; 0 = timeout, 1 = input handled, -1 = quit. */
typedef int (*sel_ui_wait_fn)(void* state, uint32_t timeout_ms);
typedef void (*sel_ui_close_fn)(void* state);                                /* UI_CLOSE */
typedef const sel_tag* (*sel_ui_stats_fn)(void* state);                      /* UI_STATS */

SEL_EXTERN_C_END

#endif /* SELECTA_UI_H */

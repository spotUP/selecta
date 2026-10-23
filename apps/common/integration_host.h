/* Host side of integration plugins (plan M1.2 phase 6, design doc integration
 * section). Shared by the shell and selecta-cli: finds the plugin in the
 * registry, opens it with a send_command that feeds the engine, forwards
 * pumped events and polls once per turn. */
#ifndef SELECTA_INTEGRATION_HOST_H
#define SELECTA_INTEGRATION_HOST_H

#include <selecta/engine.h>

typedef struct sel_int_session sel_int_session;

/* Called with every event a send_command line produces when the engine is
 * pumped for it; the app routes these to its own sink (which also calls
 * sel_int_session_event). May be NULL to drop them. */
typedef void (*sel_int_sink)(void* ctx, const sel_message* evt);

/* NULL when the plugin is not registered or lacks a hook. */
sel_int_session* sel_int_session_open(sel_engine* e, const sel_host* host, const char* id,
                                      sel_int_sink sink, void* sink_ctx);
void sel_int_session_event(sel_int_session* s, const sel_message* evt);
void sel_int_session_poll(sel_int_session* s);
void sel_int_session_close(sel_int_session* s);

#endif /* SELECTA_INTEGRATION_HOST_H */

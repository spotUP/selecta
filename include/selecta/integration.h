/* Integration plugin hooks (kind SEL_PLUGIN_KIND_INTEGRATION), stored in the
 * descriptor's PLUGIN_TAGS list under the INT_* tags. An integration mirrors
 * the player to an OS service (Now Playing, MPRIS) and turns remote commands
 * from that service into protocol lines. It holds no playback logic.
 *
 * The host calls INT_ON_EVENT for every event the engine pumps (with the
 * snapshot as of that pump) and INT_POLL once per shell turn; remote commands
 * are queued only from INT_POLL, never from INT_ON_EVENT. */
#ifndef SELECTA_INTEGRATION_H
#define SELECTA_INTEGRATION_H

#include <selecta/engine.h>

SEL_EXTERN_C_BEGIN

/* Given to INT_OPEN. send_command queues one protocol line ("NEXT", "SEEK 5000")
 * exactly as `selecta-cli send` would; it returns 0 when the line parsed and
 * non-zero when it did not. Valid until INT_CLOSE returns. */
typedef struct sel_int_host {
    void* ctx;
    int (*send_command)(void* ctx, const char* line);
    /* Append only below this line. POST `body` (application/x-www-form-urlencoded) to `url` through the host's
     * one HTTP client (net guard and proxies apply). Returns 0 when a response arrived, non-zero on a transport
     * failure; *status is the HTTP status and `reply` receives up to reply_cap-1 bytes of the body, NUL ended. */
    int (*http_post)(void* ctx, const char* url, const char* body, char* reply, size_t reply_cap, int* status);
    /* MD5 of `data` as 32 lower-case hex characters plus NUL. */
    void (*md5_hex)(void* ctx, const void* data, size_t len, char out[33]);
} sel_int_host;

typedef void* (*sel_int_open_fn)(const sel_host* host, const sel_int_host* ih); /* INT_OPEN: state or NULL */
typedef void (*sel_int_on_event_fn)(void* state, const sel_message* evt,
                                    const sel_state* snapshot);                   /* INT_ON_EVENT */
typedef void (*sel_int_poll_fn)(void* state);                                     /* INT_POLL */
typedef void (*sel_int_close_fn)(void* state);                                    /* INT_CLOSE */
/* INT_LOGIN (optional): one step of the service's account login, run without a session. token NULL asks the
 * service for a request token and writes "token=<t> url=<page where the person authorises it>"; a token asks for
 * the session that token grants and writes "session=<key> user=<name>". 0 on success; on failure `out` holds the
 * reason. Configuration (API key, secret, endpoint) comes from host->config_get like INT_OPEN. */
typedef int (*sel_int_login_fn)(const sel_host* host, const sel_int_host* ih, const char* token, char* out,
                                size_t cap);

SEL_EXTERN_C_END

#endif /* SELECTA_INTEGRATION_H */

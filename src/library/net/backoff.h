/* Shared 429 / Retry-After handling for HTTP callers (row I725d). Services such as the Internet Archive
 * (https://archive.org/services/docs/api/bots.html: honour "429 Too Many Requests" and "Retry-After") and DEViLBOX
 * answer 429 when a client is too fast. */
#ifndef SELECTA_LIBRARY_NET_BACKOFF_H
#define SELECTA_LIBRARY_NET_BACKOFF_H

#include "http.h"

#define SEL_BACKOFF_MAX_S 30ul     /* longest single wait; a larger Retry-After is clamped */
#define SEL_BACKOFF_DEFAULT_S 2ul  /* wait when a 429 carries no usable Retry-After */

/* 1 when the response is a 429 Too Many Requests. */
int sel_http_rate_limited(const sel_http_res* res);

/* Milliseconds to wait before retrying a rate-limited response: Retry-After seconds, clamped to
 * SEL_BACKOFF_MAX_S; SEL_BACKOFF_DEFAULT_S when absent. 0 when the response is not rate limited. */
unsigned long sel_http_retry_wait_ms(const sel_http_res* res);

/* sel_http_request, repeated up to `max_retries` more times while the answer is 429, sleeping per Retry-After
 * between tries. `rewind` (may be NULL) is called before each retry so the caller can discard the sink's body of
 * the refused try. `*waited_ms` (may be NULL) gains the time slept. Returns the last attempt's result.
 * Targets without a sleep call (Amiga) make no retry: the 429 is returned at once. */
sel_http_err sel_http_request_retry(const sel_http_req* req, sel_http_sink sink, void* ctx, sel_http_res* res,
                                    unsigned max_retries, void (*rewind)(void* ctx), unsigned long* waited_ms);

#endif

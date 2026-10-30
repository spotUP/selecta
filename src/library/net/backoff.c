#include "backoff.h"

#if defined(__unix__) || defined(__APPLE__)
#include <time.h>
#define SEL_BACKOFF_CAN_WAIT 1
#endif

int sel_http_rate_limited(const sel_http_res* res) {
    return res != NULL && res->status == 429;
}

unsigned long sel_http_retry_wait_ms(const sel_http_res* res) {
    unsigned long s;

    if (!sel_http_rate_limited(res)) {
        return 0ul;
    }
    s = res->retry_after_s != 0ul ? res->retry_after_s : SEL_BACKOFF_DEFAULT_S;
    if (s > SEL_BACKOFF_MAX_S) {
        s = SEL_BACKOFF_MAX_S;
    }
    return s * 1000ul;
}

sel_http_err sel_http_request_retry(const sel_http_req* req, sel_http_sink sink, void* ctx, sel_http_res* res,
                                    unsigned max_retries, void (*rewind)(void* ctx), unsigned long* waited_ms) {
    sel_http_err e = sel_http_request(req, sink, ctx, res);
#ifdef SEL_BACKOFF_CAN_WAIT
    unsigned n;

    for (n = 0; n < max_retries && sel_http_rate_limited(res); n++) {
        unsigned long ms = sel_http_retry_wait_ms(res);
        struct timespec ts;

        ts.tv_sec = (time_t)(ms / 1000ul);
        ts.tv_nsec = (long)(ms % 1000ul) * 1000000L;
        nanosleep(&ts, NULL);
        if (waited_ms != NULL) {
            *waited_ms += ms;
        }
        if (rewind != NULL) {
            rewind(ctx);
        }
        e = sel_http_request(req, sink, ctx, res);
    }
#else
    (void)max_retries;
    (void)rewind;
    (void)waited_ms;
#endif
    return e;
}

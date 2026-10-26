#include "host_internal.h"

static const char* config_get(const sel_host* h, const char* key, const char* def) {
    (void)h;
    (void)key;
    return def;
}

static int query(const sel_host* h, const sel_tag* ask, sel_tag* answer) {
    (void)h;
    if (ask && ask->tag == SEL_HOSTQ_API_VERSION) {
        answer->tag = SEL_HOSTQ_API_VERSION;
        answer->value = SEL_API_VERSION;
        return 1;
    }
    if (ask && ask->tag == SEL_HOSTQ_LOCAL_TIME) {
        answer->tag = SEL_HOSTQ_LOCAL_TIME;
        answer->value = sel_host_local_ms(h);
        return 1;
    }
    return 0;
}

static const sel_host k_default = {
    sizeof(sel_host),  SEL_API_VERSION, sel_host_mem_alloc, sel_host_mem_free, NULL,
    sel_host_log,      sel_host_clock_ms, NULL,             config_get,        query,
    NULL,              NULL,              NULL,
};

const sel_host* sel_host_default(void) {
    return &k_default;
}

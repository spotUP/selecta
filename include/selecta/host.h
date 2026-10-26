/* Host services handed to every plugin (design doc section 10). The struct
 * is versioned by size: a plugin checks `size` before touching a field
 * that arrived after its own build. Fields are append-only; T6 pins the
 * layout per architecture in tests/abi/. */
#ifndef SELECTA_HOST_H
#define SELECTA_HOST_H

#include <selecta/tags.h>

SEL_EXTERN_C_BEGIN

enum { SEL_MEM_FAST = 0, SEL_MEM_SAMPLE = 1 };
enum { SEL_LOG_ERROR = 0, SEL_LOG_WARN, SEL_LOG_INFO, SEL_LOG_DEBUG };

struct sel_voices; /* M0b */
struct sel_net;       /* net.h (I517) */
struct sel_catalogue; /* catalogue.h (I517) */

typedef struct sel_host sel_host;
struct sel_host {
    uint32_t size;        /* sizeof(sel_host) of the host build */
    uint32_t api_version; /* SEL_API_VERSION */
    void* (*mem_alloc)(const sel_host*, size_t size, uint32_t kind);
    void (*mem_free)(const sel_host*, void* p);
    int32_t (*read)(const sel_host*, void* file, void* buf, uint32_t n);
    void (*log)(const sel_host*, int level, const char* msg);
    uint32_t (*clock_ms)(const sel_host*);
    const struct sel_voices* (*voices)(const sel_host*);
    const char* (*config_get)(const sel_host*, const char* key, const char* def);
    int (*query)(const sel_host*, const sel_tag* ask, sel_tag* answer);
    void* ctx;
    /* append only below this line; never reorder, never remove */
    const struct sel_net* (*net)(const sel_host*);             /* NULL accessor or NULL result: no network (I517) */
    const struct sel_catalogue* (*catalogue)(const sel_host*); /* NULL accessor or NULL result: no library (I517) */
};

/* The host of this build: malloc-backed memory, stderr log, C clock. */
const sel_host* sel_host_default(void);

SEL_EXTERN_C_END

#endif /* SELECTA_HOST_H */

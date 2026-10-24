/* ahi-shadow (plan M2 phase 4, I124/I125): the AHI output over a counting backend instead of ahi.device. A buffer is
 * "finished" never, so the shadow reports no underruns; the swap count is the same one the rig reads from ahi. */
#include "common/ahi_out.h"

static uint32_t g_submits;

static void* sh_open(const sel_host* host, uint32_t rate, uint32_t frames) {
    (void)host;
    (void)rate;
    (void)frames;
    return &g_submits;
}

static void sh_close(const sel_host* host, void* ctx) {
    (void)host;
    (void)ctx;
}

static void sh_submit(void* ctx, uint32_t idx, const int16_t* pcm, uint32_t frames) {
    (void)idx;
    (void)pcm;
    (void)frames;
    (*(uint32_t*)ctx)++;
}

static void sh_wait(void* ctx, uint32_t idx) {
    (void)ctx;
    (void)idx;
}

static int sh_finished(void* ctx, uint32_t idx) {
    (void)ctx;
    (void)idx;
    return 0;
}

static const sel_ahi_backend g_be = {"ahi-shadow", "AHI shadow", sh_open, sh_close, sh_submit, sh_wait, sh_finished};
static sel_tag g_hooks[12];
static sel_tag g_desc[5];

static void* shadow_open(const sel_host* host, uint32_t rate, uint32_t channels) {
    return sel_ahi_out_open(&g_be, host, rate, channels);
}

const sel_tag* SEL_PLUGIN_ENTRY(uint32_t host_api) {
    if (host_api < 1u) {
        return NULL;
    }
    return sel_ahi_out_describe(&g_be, shadow_open, g_hooks, g_desc);
}

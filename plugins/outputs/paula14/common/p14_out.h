/* Shared skeleton of the two paula14 outputs (plan M2 phase 4, I123): `paula14` writes the custom chip,
 * `paula14-shadow` counts the same writes. Both are PCM outputs: the engine pulls a mixed stream (RENDER
 * kind, or VOICE kind through the mixer, because paula14 has no voice backend) and the sink splits it into
 * the chip RAM double buffer. They differ only in the register bus. Integer only. */
#ifndef SELECTA_P14_OUT_H
#define SELECTA_P14_OUT_H

#include <selecta/output.h>

#include "p14_sink.h"

#define SEL_P14_BUF_FRAMES 512u /* default; `paula14.buffer` in the host config overrides (sel_prefs per CPU class: I125) */

typedef struct sel_p14_out_kind {
    const char* id;
    const char* name;
    int strict_rate; /* 1: the real chip, rates above the hardware period limit fail to open */
    void* (*bus_open)(const sel_host* host, sel_paula_bus* bus);
    void (*bus_close)(const sel_host* host, void* bus_ctx);
} sel_p14_out_kind;

void* sel_p14_out_open(const sel_p14_out_kind* kind, const sel_host* host, uint32_t rate, uint32_t channels);

/* Fills `hooks` (12 tags) and `desc` (5 tags) for a plugin entry. */
const sel_tag* sel_p14_out_describe(const sel_p14_out_kind* kind, sel_out_open_fn open, sel_tag* hooks, sel_tag* desc);

#endif

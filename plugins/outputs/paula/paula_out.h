/* Shared skeleton of the two Paula outputs (plan M2 phase 3, I118/I120): `paula` writes the custom chip,
 * `paula-shadow` logs the same register writes. Both run this file's open/close/pump/stats/describe code
 * around paula_voices.c and differ only in the sel_paula_out_kind (the register bus). Integer only. */
#ifndef SELECTA_PAULA_OUT_H
#define SELECTA_PAULA_OUT_H

#include <selecta/output.h>

#include "paula_voices.h"

typedef struct sel_paula_out_kind {
    const char* id;    /* plugin id */
    const char* name;  /* SEL_OUT_NAME */
    uint32_t hardware; /* 1: the real chip; the engine bypasses LED and width (SEL_OUT_PAULA_HW) */
    /* Fills `bus`; returns the bus context, or NULL on failure. */
    void* (*bus_open)(const sel_host* host, sel_paula_bus* bus);
    void (*bus_close)(const sel_host* host, void* bus_ctx);
    /* Writes up to 3 extra stat tags into `out`; returns how many. May be NULL. */
    uint32_t (*bus_stats)(void* bus_ctx, sel_tag* out);
} sel_paula_out_kind;

void* sel_paula_out_open(const sel_paula_out_kind* kind, const sel_host* host, uint32_t rate, uint32_t channels);

/* Fills `hooks` (16 tags) and `desc` (5 tags) for a plugin entry; `open` is the kind's wrapper around
 * sel_paula_out_open. Every other hook is shared. */
const sel_tag* sel_paula_out_describe(const sel_paula_out_kind* kind, sel_out_open_fn open, sel_tag* hooks, sel_tag* desc);

#endif

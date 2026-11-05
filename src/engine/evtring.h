/* Engine -> UI event ring (I1082c, design doc 2026-11-05-I1082-engine-thread section 2.1).
 * The shell's event sink pushes every event into an arena ring (strings and tag lists copied, so the engine's buffers
 * are free at once); the UI turn drains it and calls on_event on the UI thread. One producer (the pump thread), one
 * consumer (the UI thread). A single-thread shell sets `inline_drain`: a full ring is then drained on the spot and an
 * event too large for a slot goes to the UI sink directly, so nothing is lost; a threaded shell sets `idle` (yield) and
 * a full ring waits for the UI up to `wait_spins` tries before the event is counted in `dropped`. */
#ifndef SELECTA_EVTRING_H
#define SELECTA_EVTRING_H

#include "spsc.h"

#include <selecta/engine.h>

typedef struct sel_evt_ring {
    sel_spsc q;
    sel_event_sink ui;      /* on_event fan-out; called by drain only (or inline) */
    void* ui_ctx;
    int inline_drain;       /* single-thread shell */
    void (*idle)(void*);    /* threaded shell: called between retries of a full ring */
    void* idle_ctx;
    uint32_t wait_spins;
    uint32_t pushed;        /* events that went through the ring */
    uint32_t drained;       /* events handed to ui by drain */
    uint32_t direct;        /* inline fallback: events too big for a slot, delivered directly */
    uint32_t dropped;       /* events lost (threaded shell only) */
} sel_evt_ring;

int sel_evt_ring_init(sel_evt_ring* r, const sel_host* host, uint32_t capacity, uint32_t arena_bytes, sel_event_sink ui, void* ui_ctx);
void sel_evt_ring_free(sel_evt_ring* r, const sel_host* host);
/* A sel_event_sink: pass the ring as ctx. Producer side. */
void sel_evt_ring_sink(void* ring, const sel_message* evt);
/* UI side: pops every queued event, calls ui for each, then hands the slots back. Returns the count. */
uint32_t sel_evt_ring_drain(sel_evt_ring* r);

#endif

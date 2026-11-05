#include "evtring.h"

#include <string.h>

int sel_evt_ring_init(sel_evt_ring* r, const sel_host* host, uint32_t capacity, uint32_t arena_bytes, sel_event_sink ui, void* ui_ctx) {
    memset(r, 0, sizeof *r);
    r->ui = ui;
    r->ui_ctx = ui_ctx;
    r->wait_spins = 1000000u;
    return sel_spsc_init_arena(&r->q, host, capacity, arena_bytes);
}

void sel_evt_ring_free(sel_evt_ring* r, const sel_host* host) {
    sel_spsc_free(&r->q, host);
}

uint32_t sel_evt_ring_drain(sel_evt_ring* r) {
    sel_message m;
    uint32_t n = 0;

    while (sel_spsc_pop(&r->q, &m)) {
        if (r->ui != NULL) {
            r->ui(r->ui_ctx, &m);
        }
        n++;
    }
    sel_spsc_release(&r->q);
    r->drained += n;
    return n;
}

void sel_evt_ring_sink(void* ring, const sel_message* evt) {
    sel_evt_ring* r = (sel_evt_ring*)ring;
    uint32_t spins = 0;

    for (;;) {
        if (sel_spsc_push(&r->q, evt)) {
            r->pushed++;
            return;
        }
        if (r->inline_drain) {
            if (sel_evt_ring_drain(r) == 0) { /* empty ring, so the event itself does not fit a slot */
                r->direct++;
                if (r->ui != NULL) {
                    r->ui(r->ui_ctx, evt);
                }
                return;
            }
            continue;
        }
        if (spins++ >= r->wait_spins) {
            r->dropped++;
            return;
        }
        if (r->idle != NULL) {
            r->idle(r->idle_ctx);
        }
    }
}

/* `selecta-cli spsc-selftest` (I114): pushes 1000 messages through a small
 * queue that wraps many times and checks every message arrives in order.
 * Prints spsc_pushed=, spsc_popped=, spsc_publishes= (each push and pop
 * publishes one index inside the pal's interrupt guard). */
#include <selecta/host.h>
#include <selecta/protocol.h>

#include "cli.h"
#include "../../src/engine/spsc.h"

#include <stdio.h>
#include <string.h>

int sel_cli_spsc_selftest(void) {
    const sel_host* host = sel_host_default();
    sel_spsc q;
    sel_message m, out;
    uint32_t pushed = 0, popped = 0, i;
    int bad = 0;

    if (!sel_spsc_init(&q, host, 8)) {
        fputs("selecta-cli: queue allocation failed\n", stderr);
        return 1;
    }
    memset(&m, 0, sizeof m);
    for (i = 0; i < 1000; i++) {
        m.id = i;
        while (!sel_spsc_push(&q, &m)) { /* full: drain one, as the consumer would */
            if (sel_spsc_pop(&q, &out)) {
                bad |= out.id != popped;
                popped++;
            }
        }
        pushed++;
    }
    while (sel_spsc_pop(&q, &out)) {
        bad |= out.id != popped;
        popped++;
    }
    sel_spsc_free(&q, host);
    printf("spsc_pushed=%u\nspsc_popped=%u\nspsc_publishes=%u\n", (unsigned)pushed, (unsigned)popped,
           (unsigned)sel_spsc_publishes());
    return bad || pushed != popped ? 1 : 0;
}

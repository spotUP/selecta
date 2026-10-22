/* selecta-cli abi [voices]: prints the layout of struct sel_host (plan I14,
 * Phase 5) or struct sel_voices (plan I29). Output is compared against
 * tests/abi/sel_host.<arch>.txt and tests/abi/sel_voices.<arch>.txt. */
#include "cli.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "selecta/host.h"
#include "selecta/voices.h"

/* Declaration order of struct sel_host. A field appended to the struct is one
 * new line here. */
#define SEL_HOST_FIELDS(X) \
    X(size)                \
    X(api_version)         \
    X(mem_alloc)           \
    X(mem_free)            \
    X(read)                \
    X(log)                 \
    X(clock_ms)            \
    X(voices)              \
    X(config_get)          \
    X(query)               \
    X(ctx)

/* Declaration order of struct sel_voices (plan I29). */
#define SEL_VOICES_FIELDS(X) \
    X(size)                  \
    X(play)                  \
    X(queue)                 \
    X(stop)                  \
    X(set_pitch_hz)          \
    X(set_period)            \
    X(set_volume)            \
    X(set_pan)               \
    X(is_playing)            \
    X(get_state)             \
    X(ctx)

int sel_cli_abi(const char* what) {
    if (what != NULL && strcmp(what, "voices") == 0) {
        printf("sel_voices size=%lu\n", (unsigned long)sizeof(sel_voices));
#define X(f) printf(#f " offset=%lu\n", (unsigned long)offsetof(sel_voices, f));
        SEL_VOICES_FIELDS(X)
#undef X
        return 0;
    }
    printf("sel_host size=%lu\n", (unsigned long)sizeof(sel_host));
#define X(f) printf(#f " offset=%lu\n", (unsigned long)offsetof(sel_host, f));
    SEL_HOST_FIELDS(X)
#undef X
    return 0;
}

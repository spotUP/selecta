/* selecta-cli abi: prints the layout of struct sel_host (plan I14, Phase 5).
 * Output is compared against tests/abi/sel_host.<arch>.txt. */
#include "cli.h"

#include <stddef.h>
#include <stdio.h>

#include "selecta/host.h"

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

int sel_cli_abi(void) {
    printf("sel_host size=%lu\n", (unsigned long)sizeof(sel_host));
#define X(f) printf(#f " offset=%lu\n", (unsigned long)offsetof(sel_host, f));
    SEL_HOST_FIELDS(X)
#undef X
    return 0;
}

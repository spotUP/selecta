/* SEL_UI_WAIT loop (plan 2026-10-25-amiga-mui.md MUI-P1-7). */
#include "mui_map.h"

int mui_wait_step(const mui_wait_io* io, void* ctx, uint32_t timeout_ms) {
    uint32_t sigs = 0;
    uint32_t got;
    int id = io->new_input(ctx, &sigs);
    if (id == MUI_WAIT_ID_QUIT) {
        return -1;
    }
    if (id != 0) {
        return 1;
    }
    got = io->wait(ctx, sigs | io->wake_mask | io->break_mask, timeout_ms);
    if ((got & io->break_mask) != 0) {
        return -1;
    }
    return (got & (sigs | io->wake_mask)) != 0 ? 1 : 0;
}

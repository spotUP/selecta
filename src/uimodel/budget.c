/* Effects governor of the desktop UI (plan DUI-P1-9, M4-P2-6): which visual effects a canvas format can afford.
 * A palette canvas (INDEX8, the Amiga and low-memory case) runs no tweens and no blur or shadow primitive; a
 * true-colour canvas (RGBA32) runs all of them. Every effect asks the budget first. Integers only. */
#include <selecta/uimodel.h>

#include <string.h>

/* The format a backend opens with when --format is absent: the AGA screen shows an 8-bitplane CLUT, so aga defaults to
 * index8 (I522); every other backend keeps rgba32 (NULL). */
const char* sel_uim_default_format(const char* backend) {
    if (backend != NULL && strcmp(backend, "aga") == 0) {
        return "index8";
    }
    return NULL;
}

int sel_uim_budget_for(sel_uim_budget* b, const char* format) {
    memset(b, 0, sizeof *b);
    if (format == NULL || strcmp(format, "rgba32") == 0) {
        b->format = SEL_UIM_FMT_RGBA32;
        b->tweens = b->blur = b->shadow = 1u;
        return 1;
    }
    if (strcmp(format, "index8") == 0) {
        b->format = SEL_UIM_FMT_INDEX8;
        return 1;
    }
    return 0;
}

int sel_uim_budget_allows(const sel_uim_budget* b, uint32_t effect) {
    switch (effect) {
    case SEL_UIM_FX_TWEEN: return b->tweens != 0u;
    case SEL_UIM_FX_BLUR: return b->blur != 0u;
    case SEL_UIM_FX_SHADOW: return b->shadow != 0u;
    default: return 0;
    }
}

int sel_uim_motion_allowed(const sel_uim_budget* b, int reduce_motion) {
    return reduce_motion == 0 && sel_uim_budget_allows(b, SEL_UIM_FX_TWEEN);
}

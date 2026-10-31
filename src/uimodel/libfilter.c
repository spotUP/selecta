/* The Your Library kind filter: see include/selecta/uimodel/libfilter.h (I669, I670). */
#include <selecta/uimodel/libfilter.h>
#include <selecta/library_item.h>

static const char* const k_lfilter_label[SEL_UIM_LFILTERS] = {"All", "Playlists", "Albums", "Artists", "Composers", "Sources", "Local Files"};
/* I678b: the kind mask each chip hands the library call (design 2026-10-29 section 4); index 0 is All. */
static const uint32_t k_lfilter_mask[SEL_UIM_LFILTERS] = {
    SEL_LIB_ITEM_ALL,
    SEL_LIB_ITEM_BIT(SEL_LIB_ITEM_PLAYLIST),
    SEL_LIB_ITEM_BIT(SEL_LIB_ITEM_ALBUM),
    SEL_LIB_ITEM_BIT(SEL_LIB_ITEM_ARTIST),
    SEL_LIB_ITEM_BIT(SEL_LIB_ITEM_COMPOSER),
    SEL_LIB_ITEM_BIT(SEL_LIB_ITEM_SOURCE),
    SEL_LIB_ITEM_BIT(SEL_LIB_ITEM_LOCAL)};

static uint32_t g_passes;

const char* sel_uim_lfilter_label(uint32_t filter) { return filter < SEL_UIM_LFILTERS ? k_lfilter_label[filter] : NULL; }

uint32_t sel_uim_lfilter_mask(uint32_t filter) { return filter < SEL_UIM_LFILTERS ? k_lfilter_mask[filter] : SEL_LIB_ITEM_ALL; }

uint32_t sel_uim_lfilter_step(uint32_t filter, int dir) {
    if (filter >= SEL_UIM_LFILTERS) {
        filter = SEL_UIM_LFILTERS - 1u;
    }
    if (dir < 0 && filter > 0u) {
        return filter - 1u;
    }
    if (dir > 0 && filter + 1u < SEL_UIM_LFILTERS) {
        return filter + 1u;
    }
    return filter;
}

uint32_t sel_uim_lfilter_pass(sel_engine* e, uint32_t filter, sel_engine_library_item* out, uint32_t max, uint32_t* total) {
    g_passes++;
    if (e == NULL) {
        if (total != NULL) {
            *total = 0u;
        }
        return 0u;
    }
    return sel_engine_library_items(e, sel_uim_lfilter_mask(filter), 0u, out, max, total);
}

uint32_t sel_uim_lfilter_passes(void) { return g_passes; }

/* Stored programs on F1..F10 (DUI-P5-8, MUI-P8-2). */
#include <selecta/uimodel/fkeys.h>
#include <selecta/uimodel/keys.h>

#include <stdio.h>

uint32_t sel_uim_fkey_slot(uint32_t code, uint32_t mods, int* store) {
    if (code < TXT_KEY_F1 || code >= TXT_KEY_F1 + SEL_UIM_FKEY_COUNT || (mods & ~(uint32_t)TXT_MOD_SHIFT) != 0u) {
        return 0;
    }
    if (store != NULL) {
        *store = (mods & TXT_MOD_SHIFT) != 0u;
    }
    return code - TXT_KEY_F1 + 1u;
}

uint32_t sel_uim_fkey_lines(uint32_t slot, int store, char out[SEL_UIM_FKEY_LINES][SEL_UIM_FKEY_LINE_MAX]) {
    if (slot < 1u || slot > SEL_UIM_FKEY_COUNT) {
        return 0;
    }
    if (store) {
        snprintf(out[0], SEL_UIM_FKEY_LINE_MAX, "PLAYLIST_SAVE F%u", (unsigned)slot);
        return 1;
    }
    snprintf(out[0], SEL_UIM_FKEY_LINE_MAX, "PLAYLIST_LOAD F%u", (unsigned)slot);
    snprintf(out[1], SEL_UIM_FKEY_LINE_MAX, "PLAY");
    return 2;
}

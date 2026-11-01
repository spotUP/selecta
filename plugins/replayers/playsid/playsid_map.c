/* Pure mappings for the playsid adapter (I434b); host tested without the library. */
#include "playsid_api.h"

#include <string.h>

uint32_t ps_mode_from_pref(const char* value) {
    static const char* const names[PS_OM_COUNT] = {"normal",     "6581",   "8580",  "auto",
                                                    "sidblaster", "zorrosid", "usbsid", "trinity"};
    uint32_t i;
    if (value == NULL) {
        return PS_OM_NORMAL;
    }
    for (i = 0; i < PS_OM_COUNT; i++) {
        if (strcmp(value, names[i]) == 0) {
            return i;
        }
    }
    return PS_OM_NORMAL;
}

int ps_mode_is_resid(uint32_t om) {
    return om == PS_OM_RESID_6581 || om == PS_OM_RESID_8580 || om == PS_OM_RESID_AUTO;
}

uint32_t ps_ahi_mode_for_output(const char* output_id, const char* ahi_pref) {
    uint32_t v = 0;
    if (output_id == NULL || strcmp(output_id, "ahi") != 0 || ahi_pref == NULL) {
        return 0;
    }
    if (ahi_pref[0] == '0' && (ahi_pref[1] == 'x' || ahi_pref[1] == 'X')) {
        ahi_pref += 2;
    }
    for (; *ahi_pref != '\0'; ahi_pref++) {
        char c = *ahi_pref;
        uint32_t d;
        if (c >= '0' && c <= '9') {
            d = (uint32_t)(c - '0');
        } else if (c >= 'a' && c <= 'f') {
            d = (uint32_t)(c - 'a' + 10);
        } else if (c >= 'A' && c <= 'F') {
            d = (uint32_t)(c - 'A' + 10);
        } else {
            return 0;
        }
        v = (v << 4) | d;
    }
    return v;
}

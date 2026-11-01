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

void ps_filter_from_pref(const char* value, uint32_t* internal_on, uint32_t* external_on) {
    *internal_on = 1;
    *external_on = 0;
    if (value != NULL && strcmp(value, "internal+external") == 0) {
        *external_on = 1;
    } else if (value != NULL && strcmp(value, "off") == 0) {
        *internal_on = 0;
    }
}

/* Index of `value` in a 4-name list, 0 when absent. */
static uint32_t pick4(const char* value, const char* const names[4]) {
    uint32_t i;
    for (i = 1; value != NULL && i < 4; i++) {
        if (strcmp(value, names[i]) == 0) {
            return i;
        }
    }
    return 0;
}

uint32_t ps_boost_from_pref(const char* value) {
    static const char* const names[4] = {"off", "2x", "3x", "4x"};
    return pick4(value, names) + 1u;
}

uint32_t ps_oversample_from_pref(const char* value) {
    static const char* const names[4] = {"normal", "2x", "3x", "4x"};
    return pick4(value, names);
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

uint32_t ps_sid_count(const uint8_t* hdr, size_t size) {
    /* PSID/RSID header: version at 4 (BE16), SID2 base at 0x7a, SID3 base at 0x7b; both bytes are the high part of
     * $Dxx0, 0 = absent. SID2 from v3 on, SID3 from v4 on (HippoPlayer .sidIsStereo / .sidIsThree). */
    uint32_t version;
    if (hdr == NULL || size < 0x7cu || (memcmp(hdr, "PSID", 4) != 0 && memcmp(hdr, "RSID", 4) != 0)) {
        return 1u;
    }
    version = ((uint32_t)hdr[4] << 8) | hdr[5];
    if (version >= 4u && hdr[0x7b] != 0u) {
        return 3u;
    }
    if (version >= 3u && hdr[0x7a] != 0u) {
        return 2u;
    }
    return 1u;
}

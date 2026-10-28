/* Your Library filter and sort (I611): the one implementation, in the core so the engine (COMPOSERS_VIEW) and every UI share it.
 * Declared in include/selecta/uimodel/browse.h. */
#include <selecta/uimodel/browse.h>

#include <ctype.h>
#include <string.h>

static int32_t lower_cmp(const char* a, const char* b) {
    for (;; a++, b++) {
        int ca = tolower((unsigned char)*a), cb = tolower((unsigned char)*b);
        if (ca != cb) return ca < cb ? -1 : 1;
        if (ca == 0) return 0;
    }
}

int32_t sel_uim_lsort_cmp(uint32_t mode, const char* a_name, uint32_t a_added, const char* a_creator, const char* b_name,
                          uint32_t b_added, const char* b_creator) {
    int32_t c;
    switch (mode) {
    case SEL_UIM_LSORT_ADDED:
        return a_added == b_added ? 0 : (a_added > b_added ? -1 : 1);
    case SEL_UIM_LSORT_ALPHA:
        return lower_cmp(a_name, b_name);
    case SEL_UIM_LSORT_CREATOR:
        c = lower_cmp(a_creator, b_creator);
        return c != 0 ? c : lower_cmp(a_name, b_name);
    default:
        return 0;
    }
}

int32_t sel_uim_lfilter_match(const char* name, const char* filter) {
    size_t i, n;
    if (filter == NULL || filter[0] == '\0') return 1;
    n = strlen(filter);
    for (i = 0; name[i] != '\0'; i++) {
        size_t k = 0;
        while (k < n && name[i + k] != '\0' && tolower((unsigned char)name[i + k]) == tolower((unsigned char)filter[k])) k++;
        if (k == n) return 1;
    }
    return 0;
}

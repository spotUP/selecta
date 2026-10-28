#include <selecta/uimodel/sources.h>

#include <stdio.h>
#include <string.h>

static uint32_t done(int n, uint32_t cap) { return n > 0 && (uint32_t)n < cap ? (uint32_t)n : 0u; }

uint32_t sel_uim_sources_off_line(const sel_uim_source* s, uint32_t n, uint32_t flip, uint32_t force_off, char* out, uint32_t cap) {
    char csv[160];
    size_t len = 0;
    uint32_t k;
    csv[0] = '\0';
    for (k = 0; k < n; k++) {
        int off = (force_off & (1u << k)) != 0u || (k == flip ? s[k].enabled : !s[k].enabled);
        if (off) {
            len += (size_t)snprintf(csv + len, sizeof csv - len, "%s%s", len > 0 ? "," : "", s[k].key);
            if (len >= sizeof csv) {
                len = sizeof csv - 1u;
            }
        }
    }
    return done(snprintf(out, cap, "PREF_SET library.sources_off \"%s\"", csv), cap);
}

uint32_t sel_uim_sync_line(const char* key, const char* path, char* out, uint32_t cap) {
    return done(snprintf(out, cap, "LIBRARY_SYNC %s \"%s\"", key, path), cap);
}

uint32_t sel_uim_sync_notnow_line(int64_t now, char* out, uint32_t cap) {
    return done(snprintf(out, cap, "PREF_SET library.sync_notice_at %lld", (long long)now), cap);
}

const char* sel_uim_sync_status(int pending, int failed, int64_t synced) {
    return pending ? "Syncing" : failed ? "Sync failed" : synced > 0 ? "Synced" : "Not synced";
}

static const char* const k_search_sources[SEL_UIM_SEARCH_SOURCES] = {"Library", "Web", "Demozoo"};

const char* sel_uim_search_source_name(uint32_t i) { return i < SEL_UIM_SEARCH_SOURCES ? k_search_sources[i] : NULL; }

uint32_t sel_uim_search_source_line(uint32_t i, char* out, uint32_t cap) {
    return i < SEL_UIM_SEARCH_SOURCES ? done(snprintf(out, cap, "PREF_SET search.source \"%s\"", k_search_sources[i]), cap) : 0u;
}

uint32_t sel_uim_search_source_find(const char* stored) {
    uint32_t i;
    for (i = 0; stored != NULL && i < SEL_UIM_SEARCH_SOURCES; i++) {
        if (strcmp(stored, k_search_sources[i]) == 0) {
            return i;
        }
    }
    return 0u;
}

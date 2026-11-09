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

void sel_uim_src_parse(const sel_tag* t, sel_uim_src_row* r) {
    memset(r, 0, sizeof *r);
    snprintf(r->key, sizeof r->key, "%s", (const char*)sel_tag_find(t, SEL_SRC_KEY, (uintptr_t)""));
    snprintf(r->kind, sizeof r->kind, "%s", (const char*)sel_tag_find(t, SEL_SRC_KIND, (uintptr_t)""));
    snprintf(r->name, sizeof r->name, "%s", (const char*)sel_tag_find(t, SEL_SRC_NAME, (uintptr_t)""));
    snprintf(r->path, sizeof r->path, "%s", (const char*)sel_tag_find(t, SEL_SRC_PATH, (uintptr_t)""));
    r->enabled = sel_tag_find(t, SEL_SRC_ENABLED, 1) != 0;
    r->searchable = sel_tag_find(t, SEL_SRC_SEARCHABLE, 0) != 0;
    r->syncable = sel_tag_find(t, SEL_SRC_SYNCABLE, 0) != 0;
    r->due = sel_tag_find(t, SEL_SRC_DUE, 0) != 0;
    r->count = (uint32_t)sel_tag_find(t, SEL_SRC_COUNT, 0);
    r->synced = (int64_t)sel_tag_find(t, SEL_SRC_SYNCED, 0);
    r->snooze = (int64_t)sel_tag_find(t, SEL_SRC_SNOOZED, 0);
}

int sel_uim_src_remote(const sel_uim_src_row* r) { return strcmp(r->kind, "remote") == 0; }

int sel_uim_src_syncable(const sel_uim_src_row* r) { return sel_uim_src_remote(r) && r->syncable; }

uint32_t sel_uim_sync_age(int64_t synced, int64_t now, char* out, size_t cap) {
    int n = synced > 0 ? snprintf(out, cap, "last synced %lld days ago", (long long)((now - synced) / 86400)) : snprintf(out, cap, "never synced");
    return n < 0 ? 0u : (uint32_t)n;
}

uint32_t sel_uim_src_status(const sel_uim_src_row* r, int pending, int failed, int64_t now, char* out, size_t cap) {
    char age[48];
    int n;
    if (!r->enabled) {
        n = snprintf(out, cap, "%s, switched off", sel_uim_src_remote(r) ? "Remote" : "Local");
    } else if (!sel_uim_src_remote(r)) {
        n = snprintf(out, cap, "Local, switched on");
    } else if (!r->syncable) {
        n = snprintf(out, cap, "Remote, search only");
    } else if (pending || failed) {
        n = snprintf(out, cap, "Remote, %s", pending ? "Syncing" : "sync failed");
    } else {
        sel_uim_sync_age(r->synced, now, age, sizeof age);
        n = snprintf(out, cap, "Remote, %s", age);
    }
    return n < 0 ? 0u : (uint32_t)n;
}

uint32_t sel_uim_offer_due(const sel_uim_src_row* r, uint32_t n) {
    uint32_t i, c = 0;
    for (i = 0; i < n; i++) {
        c += r[i].due ? 1u : 0u;
    }
    return c;
}

int sel_uim_offer_wanted(const sel_uim_offer* o, int blocked, const sel_uim_src_row* r, uint32_t n) {
    return !o->done && !blocked && sel_uim_offer_due(r, n) > 0u;
}

void sel_uim_offer_open(sel_uim_offer* o, const sel_uim_src_row* r, uint32_t n) {
    uint32_t i;
    o->n = 0;
    for (i = 0; i < n && o->n < SEL_SOURCE_MAX; i++) {
        if (r[i].due) {
            snprintf(o->key[o->n++], sizeof o->key[0], "%s", r[i].key);
        }
    }
    o->off = 0;
    o->done = 1;
}

void sel_uim_offer_toggle(sel_uim_offer* o, uint32_t k) {
    if (k < o->n) {
        o->off ^= 1u << k;
    }
}

int sel_uim_offer_ticked(const sel_uim_offer* o, uint32_t k) { return k < o->n && (o->off & (1u << k)) == 0u; }

uint32_t sel_uim_offer_title(const sel_uim_offer* o, char* out, size_t cap) {
    int n = snprintf(out, cap, "%u source%s not synced for a while", (unsigned)o->n, o->n == 1u ? "" : "s");
    return n < 0 ? 0u : (uint32_t)n;
}

uint32_t sel_uim_offer_row_text(const sel_uim_src_row* r, int pending, int64_t now, char* out, size_t cap) {
    char age[48];
    int n;
    if (pending) {
        n = snprintf(out, cap, "%s, Syncing", r->name);
    } else {
        sel_uim_sync_age(r->synced, now, age, sizeof age);
        n = snprintf(out, cap, "%s, %s", r->name, age);
    }
    return n < 0 ? 0u : (uint32_t)n;
}

void sel_uim_offer_masks(const sel_uim_offer* o, const sel_uim_src_row* r, uint32_t n, uint32_t* ticked, uint32_t* unticked) {
    uint32_t k, i;
    *ticked = 0;
    *unticked = 0;
    for (k = 0; k < o->n; k++) {
        for (i = 0; i < n; i++) {
            if (strcmp(r[i].key, o->key[k]) == 0) {
                if (o->off & (1u << k)) {
                    *unticked |= 1u << i;
                } else {
                    *ticked |= 1u << i;
                }
                break;
            }
        }
    }
}

uint32_t sel_uim_offer_snooze_line(const sel_uim_src_row* r, uint32_t n, uint32_t unticked, int64_t now, char* out, uint32_t cap) {
    char csv[440]; /* 16 sources of "key:days" must fit (the send ring slot is 512) */
    size_t len = 0;
    uint32_t k;
    int w;
    csv[0] = '\0';
    for (k = 0; k < n; k++) {
        int64_t days = (unticked & (1u << k)) ? now / 86400 : r[k].snooze / 86400;
        char entry[48];
        size_t el;
        if (days <= 0) {
            continue;
        }
        el = (size_t)snprintf(entry, sizeof entry, "%s%s:%lld", len > 0 ? "," : "", r[k].key, (long long)days);
        if (len + el < sizeof csv) {
            memcpy(csv + len, entry, el + 1u);
            len += el;
        }
    }
    w = snprintf(out, cap, "PREF_SET library.sync_snoozed \"%s\"", csv);
    return w < 0 || (uint32_t)w >= cap ? 0u : (uint32_t)w;
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

const char* sel_uim_search_scope_of_source(const sel_uim_src_row* r, uint32_t n, const char* page_key) {
    uint32_t i;
    for (i = 0; page_key != NULL && page_key[0] != '\0' && i < n; i++) {
        if (r[i].searchable && r[i].enabled && (strcmp(r[i].key, page_key) == 0 || strcmp(r[i].name, page_key) == 0)) {
            return r[i].key;
        }
    }
    return NULL;
}

/* Preferences table (header: include/selecta/prefs.def). Fixed storage, no
 * allocation. prefs.conf holds `key = value` lines; unknown keys and values
 * that fail validation are skipped on load. The engine owns one sel_prefs. */
#ifndef SELECTA_ENGINE_PREFS_H
#define SELECTA_ENGINE_PREFS_H

#include <stdint.h>

#define SEL_PREF_VALUE_MAX 128

typedef enum sel_pref_kind { SEL_PREF_PLAIN = 0, SEL_PREF_FORMAT } sel_pref_kind;

typedef enum sel_pref_id {
#define SEL_PREF(id, key, def, kind) SEL_PREF_##id,
#include <selecta/prefs.def>
#undef SEL_PREF
    SEL_PREF_COUNT
} sel_pref_id;

typedef struct sel_prefs {
    char value[SEL_PREF_COUNT][SEL_PREF_VALUE_MAX];
    uint32_t generation; /* bumps on every successful set */
} sel_prefs;

enum { SEL_PREFS_SET_OK = 0, SEL_PREFS_SET_UNKNOWN, SEL_PREFS_SET_BAD };

void sel_prefs_init(sel_prefs* p);

/* Index of `key`, or -1. */
int sel_prefs_find(const char* key);
const char* sel_prefs_key(int id);

/* SEL_PREFS_SET_UNKNOWN: no such key. SEL_PREFS_SET_BAD: a format-kind value
 * that does not parse, or one too long. Otherwise stores and bumps generation. */
int sel_prefs_set(sel_prefs* p, const char* key, const char* value);

/* prefs.conf in `dir`. Load: returns 1 when the file was read. Save creates
 * the directory; returns 1 on success. */
int sel_prefs_load(sel_prefs* p, const char* dir);
int sel_prefs_save(const sel_prefs* p, const char* dir);

#endif /* SELECTA_ENGINE_PREFS_H */

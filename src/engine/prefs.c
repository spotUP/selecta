/* Preferences table and prefs.conf (header: prefs.h). stdio only; the
 * directory is made by sel_config_mkdir (src/core/<os>/). Load and save run on
 * the pump thread, never on the audio thread. */
#include "prefs.h"

#include <selecta/engine.h>

#include <stdio.h>
#include <string.h>

#include "format.h"

static const char* const k_keys[SEL_PREF_COUNT] = {
#define SEL_PREF(id, key, def, kind) key,
#include <selecta/prefs.def>
#undef SEL_PREF
};

static const char* const k_defaults[SEL_PREF_COUNT] = {
#define SEL_PREF(id, key, def, kind) def,
#include <selecta/prefs.def>
#undef SEL_PREF
};

static const sel_pref_kind k_kinds[SEL_PREF_COUNT] = {
#define SEL_PREF(id, key, def, kind) kind,
#include <selecta/prefs.def>
#undef SEL_PREF
};

void sel_prefs_init(sel_prefs* p) {
    int i;
    memset(p, 0, sizeof *p);
    for (i = 0; i < SEL_PREF_COUNT; i++) {
        strncpy(p->value[i], k_defaults[i], SEL_PREF_VALUE_MAX - 1u);
    }
}

int sel_prefs_find(const char* key) {
    int i;
    for (i = 0; i < SEL_PREF_COUNT; i++) {
        if (strcmp(k_keys[i], key) == 0) {
            return i;
        }
    }
    return -1;
}

const char* sel_prefs_key(int id) {
    return id >= 0 && id < SEL_PREF_COUNT ? k_keys[id] : NULL;
}

int sel_prefs_set(sel_prefs* p, const char* key, const char* value) {
    int id = sel_prefs_find(key);
    if (id < 0) {
        return SEL_PREFS_SET_UNKNOWN;
    }
    if (strlen(value) >= SEL_PREF_VALUE_MAX || (k_kinds[id] == SEL_PREF_FORMAT && !sel_format_valid(value))) {
        return SEL_PREFS_SET_BAD;
    }
    strcpy(p->value[id], value);
    p->generation++;
    return SEL_PREFS_SET_OK;
}

static int conf_path(char* out, size_t cap, const char* dir) {
    int n = snprintf(out, cap, "%s/prefs.conf", dir);
    return n > 0 && (size_t)n < cap;
}

int sel_prefs_load(sel_prefs* p, const char* dir) {
    char path[512], line[SEL_PREF_VALUE_MAX + 64];
    FILE* f;
    if (!conf_path(path, sizeof path, dir) || (f = fopen(path, "r")) == NULL) {
        return 0;
    }
    while (fgets(line, sizeof line, f) != NULL) {
        char* eq = strchr(line, '=');
        char* key = line;
        char* val;
        size_t n;
        if (eq == NULL || line[0] == '#') {
            continue;
        }
        *eq = '\0';
        val = eq + 1;
        while (*key == ' ') {
            key++;
        }
        n = strlen(key);
        while (n > 0 && key[n - 1u] == ' ') {
            key[--n] = '\0';
        }
        while (*val == ' ') {
            val++;
        }
        n = strlen(val);
        while (n > 0 && (val[n - 1u] == '\n' || val[n - 1u] == '\r')) {
            val[--n] = '\0';
        }
        (void)sel_prefs_set(p, key, val);
    }
    fclose(f);
    return 1;
}

int sel_prefs_save(const sel_prefs* p, const char* dir) {
    char path[512];
    FILE* f;
    int i;
    if (!conf_path(path, sizeof path, dir) || !sel_config_mkdir(dir) || (f = fopen(path, "w")) == NULL) {
        return 0;
    }
    for (i = 0; i < SEL_PREF_COUNT; i++) {
        fprintf(f, "%s = %s\n", k_keys[i], p->value[i]);
    }
    return fclose(f) == 0;
}

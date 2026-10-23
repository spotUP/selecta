#include "options.h"

#include <selecta/protocol.h>

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

typedef enum opt_kind { OPT_TEXT, OPT_UINT, OPT_INT, OPT_ONOFF } opt_kind;

typedef struct opt_def {
    const char* key;  /* option name, upper case */
    const char* pref; /* prefs.def key it sets */
    const char* verb; /* commands.def verb with the same meaning, or NULL */
    opt_kind kind;
} opt_def;

static const opt_def k_opts[] = {
    {"OUTPUT", "output.id", "OUTPUT", OPT_TEXT},
    {"PRI", "audio.priority", NULL, OPT_INT},
    {"BUFFER", "output.buffer", NULL, OPT_UINT},
    {"LED", "output.led", NULL, OPT_ONOFF},
};

static int ieq(const char* a, const char* b) {
    for (; *a != '\0' && *b != '\0'; a++, b++) {
        if (toupper((unsigned char)*a) != toupper((unsigned char)*b)) {
            return 0;
        }
    }
    return *a == *b;
}

static int all_digits(const char* s, int allow_sign) {
    if (allow_sign && (*s == '-' || *s == '+')) {
        s++;
    }
    if (*s == '\0') {
        return 0;
    }
    for (; *s != '\0'; s++) {
        if (!isdigit((unsigned char)*s)) {
            return 0;
        }
    }
    return 1;
}

int sel_opts_apply(sel_prefs* p, sel_opts* o, const char* key, const char* value) {
    char norm[SEL_PREF_VALUE_MAX];
    size_t i, n;

    for (i = 0; i < sizeof k_opts / sizeof k_opts[0]; i++) {
        const opt_def* d = &k_opts[i];
        uint32_t verb_id;
        if (!ieq(key, d->key)) {
            continue;
        }
        if (d->verb != NULL && !sel_cmd_by_name(d->verb, &verb_id)) {
            return SEL_OPTS_UNKNOWN_KEY; /* commands.def lost the verb this option stands for */
        }
        n = strlen(value);
        if (n == 0 || n >= sizeof norm) {
            return SEL_OPTS_BAD_VALUE;
        }
        if ((d->kind == OPT_UINT && !all_digits(value, 0)) || (d->kind == OPT_INT && !all_digits(value, 1))) {
            return SEL_OPTS_BAD_VALUE;
        }
        if (d->kind == OPT_ONOFF && !ieq(value, "on") && !ieq(value, "off")) {
            return SEL_OPTS_BAD_VALUE;
        }
        for (n = 0; value[n] != '\0'; n++) { /* ids and switches are stored lower case */
            norm[n] = (d->kind == OPT_TEXT || d->kind == OPT_ONOFF) ? (char)tolower((unsigned char)value[n]) : value[n];
        }
        norm[n] = '\0';
        if (sel_prefs_set(p, d->pref, norm) != SEL_PREFS_SET_OK) {
            return SEL_OPTS_BAD_VALUE;
        }
        o->count++;
        return SEL_OPTS_OK;
    }
    return SEL_OPTS_UNKNOWN_KEY;
}

int sel_opts_apply_word(sel_prefs* p, sel_opts* o, const char* word) {
    char key[32];
    const char* eq = strchr(word, '=');
    size_t n;

    if (eq == NULL || (n = (size_t)(eq - word)) == 0 || n >= sizeof key) {
        return SEL_OPTS_BAD_VALUE;
    }
    memcpy(key, word, n);
    key[n] = '\0';
    return sel_opts_apply(p, o, key, eq + 1);
}

int sel_opts_parse_args(sel_prefs* p, sel_opts* o, int argc, char** argv) {
    int i, r;

    for (i = 0; i < argc; i++) {
        r = sel_opts_apply_word(p, o, argv[i]);
        if (r != SEL_OPTS_OK) {
            return r;
        }
    }
    return SEL_OPTS_OK;
}

int sel_opts_parse_text(sel_prefs* p, sel_opts* o, const char* text) {
    char line[128];

    while (*text != '\0') {
        size_t n = strcspn(text, "\r\n");
        int r;
        if (n < sizeof line) {
            memcpy(line, text, n);
            line[n] = '\0';
            if (line[0] != '\0' && line[0] != ';' && line[0] != '#') {
                r = sel_opts_apply_word(p, o, line);
                if (r != SEL_OPTS_OK) {
                    return r;
                }
            }
        } else {
            return SEL_OPTS_BAD_VALUE;
        }
        text += n;
        text += strspn(text, "\r\n");
    }
    return SEL_OPTS_OK;
}

int sel_opts_priority(const sel_prefs* p) {
    long v = strtol(p->value[SEL_PREF_AUDIO_PRIORITY], NULL, 10);

    return v < -128 ? -128 : (v > 127 ? 127 : (int)v);
}

uint32_t sel_opts_hash(const sel_prefs* p) {
    uint32_t h = 2166136261u;
    int i;
    const char* s;

    for (i = 0; i < SEL_PREF_COUNT; i++) {
        for (s = p->value[i]; ; s++) {
            h = (h ^ (uint8_t)*s) * 16777619u;
            if (*s == '\0') {
                break;
            }
        }
    }
    return h;
}

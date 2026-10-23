/* Key bindings of the text UI, plan M1.2 phase 5 (row I76). */
#include "keybind.h"

#include "keys.h"

#include <selecta/protocol.h>

#include <stdio.h>
#include <string.h>

static const char* const k_actions[] = {
    "quit",       "info",       "command",      "palette",       "help",         "add_file",
    "remove_entry", "play_pause", "next_style",   "next_group",    "back",         "switch_view",
    "subsong_prev", "subsong_next", "page_up",    "page_down",    "mark",
    "viz",        "pattern",    "solo",         "voice1",        "voice2",       "voice3",
    "voice4",     "voice5",     "voice6",       "voice7",        "voice8",       "voice9",
};

int txt_action_known(const char* name) {
    size_t i;
    for (i = 0; i < sizeof k_actions / sizeof k_actions[0]; i++) {
        if (strcmp(k_actions[i], name) == 0) {
            return 1;
        }
    }
    return 0;
}

static const struct {
    const char* name;
    uint32_t code;
} k_names[] = {
    {"Space", ' '},          {"Tab", 9},
    {"Enter", 13},           {"Escape", 27},
    {"Backspace", 127},      {"Up", TXT_KEY_UP},
    {"Down", TXT_KEY_DOWN},  {"Left", TXT_KEY_LEFT},
    {"Right", TXT_KEY_RIGHT}, {"Home", TXT_KEY_HOME},
    {"End", TXT_KEY_END},    {"PageUp", TXT_KEY_PAGE_UP},
    {"PageDown", TXT_KEY_PAGE_DOWN}, {"Insert", TXT_KEY_INSERT},
    {"Delete", TXT_KEY_DELETE},
};

int txt_key_parse(const char* name, uint32_t* code, uint32_t* mods) {
    size_t i;
    *mods = 0;
    for (;;) {
        if (strncmp(name, "C-", 2) == 0 && name[2] != '\0') {
            *mods |= TXT_MOD_CTRL;
            name += 2;
        } else if (strncmp(name, "M-", 2) == 0 && name[2] != '\0') {
            *mods |= TXT_MOD_ALT;
            name += 2;
        } else {
            break;
        }
    }
    if (name[0] == '\0') {
        return 0;
    }
    for (i = 0; i < sizeof k_names / sizeof k_names[0]; i++) {
        if (strcmp(k_names[i].name, name) == 0) {
            *code = k_names[i].code;
            return 1;
        }
    }
    if (name[0] == 'F' && name[1] >= '1' && name[1] <= '9') {
        unsigned n = 0;
        const char* p = name + 1;
        while (*p >= '0' && *p <= '9' && n < 100u) {
            n = n * 10u + (unsigned)(*p++ - '0');
        }
        if (*p == '\0' && n >= 1u && n <= 12u) {
            *code = TXT_KEY_F1 + n - 1u;
            return 1;
        }
        return 0; /* "F13", "F1x": not a key name */
    }
    if (name[1] == '\0' && (unsigned char)name[0] >= 0x21 && (unsigned char)name[0] < 0x7F) {
        *code = (unsigned char)name[0];
        return 1;
    }
    return 0;
}

int txt_key_name(uint32_t code, uint32_t mods, char* out, size_t cap) {
    char base[16];
    size_t i;
    int n;
    base[0] = '\0';
    for (i = 0; i < sizeof k_names / sizeof k_names[0]; i++) {
        if (k_names[i].code == code) {
            snprintf(base, sizeof base, "%s", k_names[i].name);
        }
    }
    if (base[0] == '\0' && code >= TXT_KEY_F1 && code < TXT_KEY_F1 + 12u) {
        snprintf(base, sizeof base, "F%u", (unsigned)(code - TXT_KEY_F1 + 1u));
    }
    if (base[0] == '\0' && code > 0x20u && code < 0x7Fu) {
        snprintf(base, sizeof base, "%c", (char)code);
    }
    if (base[0] == '\0') {
        return 0;
    }
    n = snprintf(out, cap, "%s%s%s", (mods & TXT_MOD_CTRL) ? "C-" : "", (mods & TXT_MOD_ALT) ? "M-" : "", base);
    return n > 0 && (size_t)n < cap;
}

static const struct {
    const char* key;
    const char* action;
} k_defaults[] = {
    {"k", "VIEW_CURSOR -1"},   {"j", "VIEW_CURSOR +1"},
    {"Up", "VIEW_CURSOR -1"},  {"Down", "VIEW_CURSOR +1"},
    {"PageUp", "@page_up"},    {"PageDown", "@page_down"},
    {"Home", "VIEW_CURSOR top"}, {"End", "VIEW_CURSOR bottom"},
    {"Enter", "VIEW_ACTIVATE"}, {"v", "@next_style"},
    {"g", "@next_group"},      {"Backspace", "@back"},
    {"Tab", "@switch_view"},   {"Space", "@play_pause"},
    {"s", "STOP"},             {"n", "NEXT"},
    {"p", "PREV"},             {"Left", "SEEK -5000"},
    {"Right", "SEEK +5000"},   {"[", "@subsong_prev"},
    {"]", "@subsong_next"},    {"+", "VOLUME +5"},
    {"=", "VOLUME +5"},        {"-", "VOLUME -5"},
    {"a", "@add_file"},        {"d", "@remove_entry"},
    {"i", "@info"},            {"q", "@quit"},
    {":", "@command"},         {"C-p", "@palette"},
    {"?", "@help"},            {"u", "UNDO"},
    {"C-r", "REDO"},           {"m", "@mark"},
    /* M1.3 I101. Pitch cannot take + and - (volume has them): braces move it a semitone. */
    {"V", "@viz"},             {"P", "@pattern"},
    {"1", "@voice1"},          {"2", "@voice2"},
    {"3", "@voice3"},          {"4", "@voice4"},
    {"5", "@voice5"},          {"6", "@voice6"},
    {"7", "@voice7"},          {"8", "@voice8"},
    {"9", "@voice9"},          {"0", "SOLO 0"},
    {"S", "@solo"},            {"}", "PITCH +100"},
    {"{", "PITCH -100"},       {">", "TEMPO +5"},
    {"<", "TEMPO -5"},
};

static int find(const txt_bindings* b, uint32_t code, uint32_t mods) {
    uint32_t i;
    for (i = 0; i < b->count; i++) {
        if (b->rows[i].code == code && b->rows[i].mods == mods) {
            return (int)i;
        }
    }
    return -1;
}

static int set(txt_bindings* b, uint32_t code, uint32_t mods, const char* action) {
    int at = find(b, code, mods);
    if (strlen(action) >= TXT_BIND_ACTION) {
        return 0;
    }
    if (at < 0) {
        if (b->count >= TXT_BIND_MAX) {
            return 0;
        }
        at = (int)b->count++;
        b->rows[at].code = code;
        b->rows[at].mods = mods;
    }
    strcpy(b->rows[at].action, action);
    return 1;
}

static void unset(txt_bindings* b, uint32_t code, uint32_t mods) {
    int at = find(b, code, mods);
    if (at >= 0) {
        /* shift, not swap: the row order is the order the palette shows */
        memmove(&b->rows[at], &b->rows[at + 1], (b->count - (uint32_t)at - 1u) * sizeof b->rows[0]);
        b->count--;
    }
}

void txt_bindings_default(txt_bindings* b) {
    size_t i;
    uint32_t code, mods;
    memset(b, 0, sizeof *b);
    for (i = 0; i < sizeof k_defaults / sizeof k_defaults[0]; i++) {
        if (txt_key_parse(k_defaults[i].key, &code, &mods)) {
            (void)set(b, code, mods, k_defaults[i].action);
        }
    }
}

static int action_valid(const char* action) {
    char verb[48];
    size_t n = strcspn(action, " \t");
    if (n == 0 || n >= sizeof verb) {
        return 0;
    }
    memcpy(verb, action, n);
    verb[n] = '\0';
    if (verb[0] == '@') {
        return txt_action_known(verb + 1);
    }
    return sel_cmd_by_name(verb, NULL);
}

uint32_t txt_bindings_load(txt_bindings* b, const char* text, uint32_t* first_bad_line) {
    uint32_t line_no = 0, bad = 0;
    const char* p = text;
    if (first_bad_line != NULL) {
        *first_bad_line = 0;
    }
    while (*p != '\0') {
        char line[TXT_BIND_ACTION + 32];
        size_t n = strcspn(p, "\n"), len = n;
        char* key;
        char* rest;
        uint32_t code, mods;
        int ok = 1;
        line_no++;
        if (len >= sizeof line) {
            len = sizeof line - 1u;
            ok = 0;
        }
        memcpy(line, p, len);
        line[len] = '\0';
        p += n;
        if (*p == '\n') {
            p++;
        }
        while (len > 0 && (line[len - 1] == '\r' || line[len - 1] == ' ' || line[len - 1] == '\t')) {
            line[--len] = '\0';
        }
        key = line + strspn(line, " \t");
        if (*key == '\0' || *key == '#') {
            continue;
        }
        rest = key + strcspn(key, " \t");
        if (*rest != '\0') {
            *rest++ = '\0';
            rest += strspn(rest, " \t");
        }
        if (ok && strcmp(key, "unbind") == 0) {
            ok = txt_key_parse(rest, &code, &mods);
            if (ok) {
                unset(b, code, mods);
            }
        } else if (ok) {
            ok = txt_key_parse(key, &code, &mods) && action_valid(rest) && set(b, code, mods, rest);
        }
        if (!ok) {
            if (bad == 0 && first_bad_line != NULL) {
                *first_bad_line = line_no;
            }
            bad++;
        }
    }
    return bad;
}

const char* txt_bindings_lookup(const txt_bindings* b, uint32_t code, uint32_t mods) {
    int at = find(b, code, mods);
    return at < 0 ? NULL : b->rows[at].action;
}

const char* txt_bindings_key_for(const txt_bindings* b, const char* action, char* out, size_t cap) {
    uint32_t i;
    size_t verb = strcspn(action, " \t");
    int first_word = -1;
    for (i = 0; i < b->count; i++) {
        if (strcmp(b->rows[i].action, action) == 0) {
            return txt_key_name(b->rows[i].code, b->rows[i].mods, out, cap) ? out : NULL;
        }
        if (first_word < 0 && strncmp(b->rows[i].action, action, verb) == 0 &&
            (b->rows[i].action[verb] == ' ' || b->rows[i].action[verb] == '\0')) {
            first_word = (int)i;
        }
    }
    if (first_word >= 0) {
        return txt_key_name(b->rows[first_word].code, b->rows[first_word].mods, out, cap) ? out : NULL;
    }
    return NULL;
}

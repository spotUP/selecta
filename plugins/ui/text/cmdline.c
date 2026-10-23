/* Command line of the text UI, plan M1.2 phase 5 (row I77). */
#include "cmdline.h"

#include "keys.h"

#include <selecta/protocol.h>

#include <stdio.h>
#include <string.h>

#define MATCH_MAX 128u

static char upper(char ch) {
    return ch >= 'a' && ch <= 'z' ? (char)(ch - 'a' + 'A') : ch;
}

static void set_text(txt_cmdline* c, const char* s) {
    snprintf(c->text, sizeof c->text, "%s", s);
    c->len = (uint32_t)strlen(c->text);
}

void txt_cmdline_open(txt_cmdline* c) {
    c->text[0] = '\0';
    c->len = 0;
    c->status[0] = '\0';
    c->prefix[0] = '\0';
    c->draft[0] = '\0';
    c->tab_at = 0;
    c->history_at = c->history_count;
}

const char* txt_cmdline_text(const txt_cmdline* c) {
    return c->text;
}

/* The verbs that start with the (upper-cased) text; ids in COMMANDS order. */
static uint32_t matches(const char* prefix, uint32_t* ids) {
    uint32_t id, n = 0, len = (uint32_t)strlen(prefix), k;
    for (id = 0; id < SEL_CMD_COUNT && n < MATCH_MAX; id++) {
        const char* name = sel_cmd_name(id);
        for (k = 0; k < len && name[k] != '\0' && name[k] == upper(prefix[k]); k++) {
        }
        if (k == len) {
            ids[n++] = id;
        }
    }
    return n;
}

static void tab(txt_cmdline* c) {
    uint32_t ids[MATCH_MAX], n, i;
    if (strchr(c->text, ' ') != NULL || c->len == 0) {
        return; /* only the verb is completed */
    }
    if (c->tab_at == 0) {
        snprintf(c->prefix, sizeof c->prefix, "%s", c->text);
    }
    n = matches(c->prefix, ids);
    c->status[0] = '\0';
    if (n == 0) {
        snprintf(c->status, sizeof c->status, "No command starts with %s", c->prefix);
        c->tab_at = 0;
        return;
    }
    if (n == 1) {
        set_text(c, sel_cmd_name(ids[0]));
        c->tab_at = 1;
        return;
    }
    if (c->tab_at == 0) {
        /* first Tab: list the matches and extend to what they share */
        size_t at = 0, common = strlen(sel_cmd_name(ids[0]));
        for (i = 0; i < n; i++) {
            const char* name = sel_cmd_name(ids[i]);
            size_t k = 0;
            while (k < common && name[k] == sel_cmd_name(ids[0])[k]) {
                k++;
            }
            common = k;
            if (at + strlen(name) + 2u < sizeof c->status) {
                at += (size_t)snprintf(c->status + at, sizeof c->status - at, "%s%s", at != 0 ? " " : "", name);
            }
        }
        if (common > c->len) {
            char buf[TXT_CMD_LINE];
            snprintf(buf, sizeof buf, "%.*s", (int)common, sel_cmd_name(ids[0]));
            set_text(c, buf);
        }
        c->tab_at = 1;
        return;
    }
    set_text(c, sel_cmd_name(ids[(c->tab_at - 1u) % n]));
    c->tab_at++;
}

static void push_history(txt_cmdline* c) {
    if (c->len == 0 || (c->history_count > 0 && strcmp(c->history[c->history_count - 1u], c->text) == 0)) {
        return;
    }
    if (c->history_count == TXT_CMD_HISTORY) {
        memmove(c->history[0], c->history[1], (TXT_CMD_HISTORY - 1u) * sizeof c->history[0]);
        c->history_count--;
    }
    snprintf(c->history[c->history_count++], TXT_CMD_LINE, "%s", c->text);
}

txt_cmd_result txt_cmdline_key(txt_cmdline* c, uint32_t code, uint32_t mods) {
    if (code == 9u && mods == 0) {
        tab(c);
        return TXT_CMD_EDITING;
    }
    c->tab_at = 0;
    if (code == 13u) {
        push_history(c);
        c->history_at = c->history_count;
        return TXT_CMD_SUBMIT;
    }
    if (code == 27u) {
        return TXT_CMD_CANCEL;
    }
    if (code == TXT_KEY_UP) {
        if (c->history_at == c->history_count) {
            snprintf(c->draft, sizeof c->draft, "%s", c->text);
        }
        if (c->history_at > 0) {
            c->history_at--;
            set_text(c, c->history[c->history_at]);
        }
        return TXT_CMD_EDITING;
    }
    if (code == TXT_KEY_DOWN) {
        if (c->history_at < c->history_count) {
            c->history_at++;
            set_text(c, c->history_at == c->history_count ? c->draft : c->history[c->history_at]);
        }
        return TXT_CMD_EDITING;
    }
    if (code == 127u) {
        if (c->len == 0) {
            return TXT_CMD_CANCEL;
        }
        c->text[--c->len] = '\0';
        return TXT_CMD_EDITING;
    }
    if (mods == 0 && code >= 0x20u && code < 0x7Fu && c->len + 1u < TXT_CMD_LINE) {
        c->text[c->len++] = (char)code;
        c->text[c->len] = '\0';
        c->status[0] = '\0';
    }
    return TXT_CMD_EDITING;
}

uint32_t txt_cmdline_history_load(txt_cmdline* c, const char* text) {
    uint32_t n = 0;
    const char* p = text;
    c->history_count = 0;
    while (*p != '\0') {
        size_t len = strcspn(p, "\n");
        if (len > 0 && len < TXT_CMD_LINE) {
            snprintf(c->text, sizeof c->text, "%.*s", (int)len, p);
            c->len = (uint32_t)len;
            push_history(c);
            n++;
        }
        p += len;
        if (*p == '\n') {
            p++;
        }
    }
    c->text[0] = '\0';
    c->len = 0;
    c->history_at = c->history_count;
    return n;
}

size_t txt_cmdline_history_dump(const txt_cmdline* c, char* out, size_t cap) {
    size_t at = 0;
    uint32_t i;
    for (i = 0; i < c->history_count; i++) {
        size_t len = strlen(c->history[i]);
        if (at + len + 2u > cap) {
            if (cap > 0) {
                out[0] = '\0';
            }
            return 0;
        }
        memcpy(out + at, c->history[i], len);
        out[at + len] = '\n';
        at += len + 1u;
    }
    if (cap > 0) {
        out[at] = '\0';
    }
    return at;
}

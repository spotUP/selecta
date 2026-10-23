/* Command palette model of the text UI, plan M1.2 phase 5 (row I78). */
#include "palette.h"

#include "keys.h"

#include <string.h>

static const struct {
    const char* area;
    const char* title;
} k_areas[] = {
    {"transport", "Transport"},
    {"queue", "Queue"},
    {"playlists", "Playlists and library"},
    {"view", "View model and shell"},
    {"protocol", "Protocol"},
};

const char* txt_palette_area_title(const char* area) {
    size_t i;
    for (i = 0; i < sizeof k_areas / sizeof k_areas[0]; i++) {
        if (strcmp(k_areas[i].area, area) == 0) {
            return k_areas[i].title;
        }
    }
    return area;
}

static uint32_t area_rank(const char* area) {
    uint32_t i;
    for (i = 0; i < sizeof k_areas / sizeof k_areas[0]; i++) {
        if (strcmp(k_areas[i].area, area) == 0) {
            return i;
        }
    }
    return (uint32_t)(sizeof k_areas / sizeof k_areas[0]);
}

static char lower(char c) {
    return c >= 'A' && c <= 'Z' ? (char)(c - 'A' + 'a') : c;
}

/* Subsequence match of the query in the name, case-insensitive. Returns -1
 * when it does not match. Score: early matches, matches at a word start (after
 * `_`) and matches next to the previous one score higher; a shorter name wins
 * a tie. */
static int32_t score(const char* name, const char* query) {
    const size_t full = strlen(name);
    int32_t s = 0;
    int32_t prev = -2;
    int32_t at = 0;
    const char* q = query;
    for (; *name != '\0' && *q != '\0'; name++, at++) {
        if (lower(*name) != lower(*q)) {
            continue;
        }
        s += 10;
        if (at == 0 || name[-1] == '_') {
            s += 8;
        }
        if (prev == at - 1) {
            s += 6;
        }
        s -= at < 10 ? at / 2 : 5;
        prev = at;
        q++;
    }
    if (*q != '\0') {
        return -1;
    }
    return s * 4 - (int32_t)(full / 4);
}

static void fill_key(txt_palette* p, txt_palette_item* it) {
    it->key[0] = '\0';
    if (p->binds != NULL) {
        (void)txt_bindings_key_for(p->binds, it->name, it->key, sizeof it->key);
    }
}

void txt_palette_set_query(txt_palette* p, const char* query) {
    uint32_t id, n = 0, i, j;
    strncpy(p->query, query, TXT_PAL_QUERY - 1u);
    p->query[TXT_PAL_QUERY - 1u] = '\0';
    p->len = (uint32_t)strlen(p->query);
    for (id = 0; id < SEL_CMD_COUNT; id++) {
        txt_palette_item it;
        int32_t sc = p->len == 0 ? 0 : score(sel_cmd_name(id), p->query);
        if (sc < 0) {
            continue;
        }
        it.id = id;
        it.name = sel_cmd_name(id);
        it.area = sel_cmd_area(id);
        it.score = sc;
        fill_key(p, &it);
        /* insertion sort: stable, so ties keep commands.def order */
        for (i = n; i > 0; i--) {
            const txt_palette_item* prev = &p->items[i - 1u];
            int before = p->len == 0 ? area_rank(prev->area) > area_rank(it.area) : prev->score < it.score;
            if (!before) {
                break;
            }
        }
        for (j = n; j > i; j--) {
            p->items[j] = p->items[j - 1u];
        }
        p->items[i] = it;
        n++;
    }
    p->count = n;
    p->cursor = 0;
}

void txt_palette_open(txt_palette* p, const txt_bindings* binds) {
    memset(p, 0, sizeof *p);
    p->binds = binds;
    txt_palette_set_query(p, "");
}

txt_pal_result txt_palette_key(txt_palette* p, uint32_t code, uint32_t mods) {
    if (code == 27u) {
        return TXT_PAL_CANCEL;
    }
    if (code == 13u) {
        return p->count > 0 ? TXT_PAL_RUN : TXT_PAL_EDITING;
    }
    if (code == TXT_KEY_UP) {
        if (p->cursor > 0) {
            p->cursor--;
        }
    } else if (code == TXT_KEY_DOWN) {
        if (p->cursor + 1u < p->count) {
            p->cursor++;
        }
    } else if (code == 127u) {
        char q[TXT_PAL_QUERY];
        if (p->len == 0) {
            return TXT_PAL_CANCEL;
        }
        memcpy(q, p->query, p->len);
        q[p->len - 1u] = '\0';
        txt_palette_set_query(p, q);
    } else if (mods == 0 && code >= 0x20u && code < 0x7Fu && p->len + 1u < TXT_PAL_QUERY) {
        char q[TXT_PAL_QUERY];
        memcpy(q, p->query, p->len);
        q[p->len] = (char)code;
        q[p->len + 1u] = '\0';
        txt_palette_set_query(p, q);
    }
    return TXT_PAL_EDITING;
}

const char* txt_palette_picked(const txt_palette* p) {
    return p->cursor < p->count ? p->items[p->cursor].name : NULL;
}

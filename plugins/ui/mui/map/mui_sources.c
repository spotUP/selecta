#include "mui_map.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#include <selecta/tags.h>

/* I1175: the Sources page model. The desktop keeps the SOURCES rows in d->src (desk_on_event) and draws status, switch and Sync per
 * source (draw_col_src, src_toggle_send, src_sync_send); the MUI keeps the same rows here and fires the same shared lines. */

static uint32_t slot_of(const mui_srcs* m, const char* key) {
    uint32_t i;
    for (i = 0; i < m->n; i++) {
        if (strcmp(m->it[i].key, key) == 0) {
            return i;
        }
    }
    return m->n;
}

void mui_srcs_asked(mui_srcs* m) { m->wait = 1; }

static int row_of(const mui_srcs* m, uint32_t mask, const char* name) {
    uint32_t i;
    for (i = 0; i < m->n; i++) {
        if ((mask & (1u << i)) != 0u && (strcmp(m->it[i].key, name) == 0 || strcmp(m->it[i].path, name) == 0)) {
            return (int)i;
        }
    }
    return -1;
}

static void settle(mui_srcs* m, int row, int failed) {
    m->pending &= ~(1u << row);
    if (failed) {
        m->failed |= 1u << row;
    } else {
        m->failed &= ~(1u << row);
    }
    m->gen++;
}

int mui_srcs_event(mui_srcs* m, const sel_message* evt) {
    if (evt->id == SEL_EVT_LIBRARY_RESULT && evt->argc > 0u && evt->args[0].type == SEL_ARG_TAGS) {
        const sel_tag* t = evt->args[0].v.t;
        if (sel_tag_find(t, SEL_SRC_KEY, 0) != 0) {
            const char* key = (const char*)sel_tag_find(t, SEL_SRC_KEY, (uintptr_t)"");
            uint32_t n;
            if (m->wait) {
                m->wait = 0;
                m->n = 0u;
            }
            n = slot_of(m, key);
            if (n >= SEL_SOURCE_MAX) {
                return 1;
            }
            if (n == m->n) {
                m->n++;
            }
            sel_uim_src_parse(t, &m->it[n]);
            m->gen++;
            return 1;
        }
        if (m->pending != 0u) { /* the engine's completion of a LIBRARY_SYNC: a path LIBRARY_RESULT */
            int row = row_of(m, m->pending, (const char*)sel_tag_find(t, SEL_SRC_PATH, (uintptr_t)""));
            if (row >= 0) {
                settle(m, row, 0);
            }
        }
    } else if (evt->id == SEL_EVT_ERROR && m->pending != 0u && evt->argc > 1u && evt->args[0].type == SEL_ARG_STR && evt->args[1].type == SEL_ARG_STR &&
               (strcmp(evt->args[0].v.s, "sync_failed") == 0 || strcmp(evt->args[0].v.s, "sync_busy") == 0)) {
        int row = row_of(m, m->pending, evt->args[1].v.s);
        if (row >= 0) {
            settle(m, row, 1);
        }
    }
    return 0;
}

uint32_t mui_srcs_row(const mui_srcs* m, uint32_t i, char* out, size_t cap) {
    char state[96];
    if (i >= m->n) {
        out[0] = '\0';
        return 0;
    }
    sel_uim_src_status(&m->it[i], (m->pending & (1u << i)) != 0u, (m->failed & (1u << i)) != 0u, (int64_t)time(NULL), state, sizeof state);
    return (uint32_t)snprintf(out, cap, "%-24.24s  %s", m->it[i].name, state);
}

static void fill(const mui_srcs* m, sel_uim_source* s) {
    uint32_t k;
    for (k = 0; k < m->n; k++) {
        s[k].key = m->it[k].key;
        s[k].path = m->it[k].path;
        s[k].enabled = m->it[k].enabled;
    }
}

uint32_t mui_srcs_toggle(mui_srcs* m, uint32_t i, mui_send_fn send, void* ctx) {
    sel_uim_source s[SEL_SOURCE_MAX];
    if (i >= m->n) {
        return 0;
    }
    fill(m, s);
    m->wait = 1;
    return mui_source_toggle(s, m->n, i, send, ctx);
}

uint32_t mui_srcs_sync(mui_srcs* m, uint32_t i, mui_send_fn send, void* ctx) {
    sel_uim_source s[SEL_SOURCE_MAX];
    if (i >= m->n || !sel_uim_src_syncable(&m->it[i]) || !m->it[i].enabled) {
        return 0;
    }
    fill(m, s);
    m->pending |= 1u << i;
    m->failed &= ~(1u << i);
    m->wait = 1;
    m->gen++;
    return mui_source_sync(&s[i], send, ctx);
}

/* I1182: the first-start sync offer. Mirrors desktop nagdlg_open / nagdlg_close; the decisions are the shared sel_uim_offer_*. */

int mui_offer_panel_follow(mui_offer_panel* p, int blocked, const mui_srcs* m) {
    if (!sel_uim_offer_wanted(&p->o, blocked || p->open || m->wait, m->it, m->n)) {
        return 0;
    }
    sel_uim_offer_open(&p->o, m->it, m->n);
    p->open = 1;
    p->opens++;
    return 1;
}

uint32_t mui_offer_panel_title(const mui_offer_panel* p, char* out, size_t cap) { return sel_uim_offer_title(&p->o, out, cap); }

uint32_t mui_offer_panel_row(const mui_offer_panel* p, const mui_srcs* m, uint32_t k, char* out, size_t cap) {
    uint32_t i;
    if (k >= p->o.n) {
        out[0] = '\0';
        return 0;
    }
    for (i = 0; i < m->n; i++) {
        if (strcmp(m->it[i].key, p->o.key[k]) == 0) {
            return sel_uim_offer_row_text(&m->it[i], (m->pending & (1u << i)) != 0u, (int64_t)time(NULL), out, cap);
        }
    }
    return (uint32_t)snprintf(out, cap, "%s", p->o.key[k]); /* the source left the list since the offer opened: still a row */
}

void mui_offer_panel_tick(mui_offer_panel* p, uint32_t k) { sel_uim_offer_toggle(&p->o, k); }

uint32_t mui_offer_panel_sync(mui_offer_panel* p, mui_srcs* m, int64_t now, mui_send_fn send, void* ctx) {
    sel_uim_source s[SEL_SOURCE_MAX];
    uint32_t ticked, unticked, sent, i, send_mask = 0;
    char line[480];
    if (!p->open) {
        return 0;
    }
    sel_uim_offer_masks(&p->o, m->it, m->n, &ticked, &unticked);
    fill(m, s);
    for (i = 0; i < m->n; i++) {
        if ((ticked & (1u << i)) != 0u && (m->pending & (1u << i)) == 0u) {
            send_mask |= 1u << i;
            m->pending |= 1u << i;
            m->failed &= ~(1u << i);
            p->syncs++;
        }
    }
    sent = mui_offer_sync(s, m->n, send_mask, send, ctx);
    if (unticked != 0u) {
        sent += sel_uim_offer_snooze_line(m->it, m->n, unticked, now, line, sizeof line) != 0u && send != NULL && send(ctx, line) != 0;
        m->wait = 1;
        sent += send != NULL && send(ctx, "SOURCES") != 0;
    }
    p->open = 0;
    m->gen++;
    return sent;
}

uint32_t mui_offer_panel_not_now(mui_offer_panel* p, mui_srcs* m, int64_t now, mui_send_fn send, void* ctx) {
    uint32_t sent;
    if (!p->open) {
        return 0;
    }
    p->open = 0;
    p->later++;
    m->wait = 1;
    sent = mui_offer_not_now(now, send, ctx);
    m->gen++;
    return sent;
}

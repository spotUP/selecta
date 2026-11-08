#include "mui_map.h"

#include <stdio.h>
#include <string.h>

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
            mui_src* r;
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
            r = &m->it[n];
            snprintf(r->key, sizeof r->key, "%s", key);
            snprintf(r->kind, sizeof r->kind, "%s", (const char*)sel_tag_find(t, SEL_SRC_KIND, (uintptr_t)""));
            snprintf(r->name, sizeof r->name, "%s", (const char*)sel_tag_find(t, SEL_SRC_NAME, (uintptr_t)""));
            snprintf(r->path, sizeof r->path, "%s", (const char*)sel_tag_find(t, SEL_SRC_PATH, (uintptr_t)""));
            r->enabled = sel_tag_find(t, SEL_SRC_ENABLED, 1) != 0;
            r->syncable = sel_tag_find(t, SEL_SRC_SYNCABLE, 0) != 0;
            r->count = (uint32_t)sel_tag_find(t, SEL_SRC_COUNT, 0);
            r->synced = (int64_t)sel_tag_find(t, SEL_SRC_SYNCED, 0);
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

static int remote(const mui_src* r) { return strcmp(r->kind, "remote") == 0; }

uint32_t mui_srcs_row(const mui_srcs* m, uint32_t i, char* out, size_t cap) {
    const mui_src* r;
    const char* state;
    if (i >= m->n) {
        out[0] = '\0';
        return 0;
    }
    r = &m->it[i];
    if (!remote(r)) {
        state = "Local";
    } else if (!r->syncable) {
        state = "Remote, search only";
    } else {
        state = sel_uim_sync_status((m->pending & (1u << i)) != 0u, (m->failed & (1u << i)) != 0u, r->synced);
    }
    return (uint32_t)snprintf(out, cap, "%-24.24s %-4s  %s", r->name, r->enabled ? "On" : "Off", state);
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
    if (i >= m->n || !remote(&m->it[i]) || !m->it[i].syncable || !m->it[i].enabled) {
        return 0;
    }
    fill(m, s);
    m->pending |= 1u << i;
    m->failed &= ~(1u << i);
    m->wait = 1;
    m->gen++;
    return mui_source_sync(&s[i], send, ctx);
}

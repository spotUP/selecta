/* I794: MUI route handler, see mui_route.h. */
#include "mui_route.h"

#include "mui_map.h"

#include <selecta/library_item.h>

#include <selecta/uimodel/doors.h>
#include <stdio.h>
#include <string.h>

void mui_route_init(mui_route* r) {
    memset(r, 0, sizeof *r);
    sel_route_hist_init(&r->rh);
}

static void seed_home(mui_route* r) {
    sel_route_entry cur;
    memset(&cur, 0, sizeof cur);
    cur.page = (uint32_t)SEL_ROUTE_PAGE_HOME;
    sel_route_hist_seed(&r->rh, &cur); /* the window opens on Home */
}

/* The lines that make the page of entry `e` (desk_page_load, desk_route_enter, col_kind_open). `restore` marks Back and Forward: the
 * collection is rebuilt without touching Recents. */
static void enter(mui_route* r, const sel_route_entry* e, int restore, mui_route_send_fn send, void* ctx) {
    char line[512];
    r->entered++;
    if (e->page == (uint32_t)SEL_ROUTE_PAGE_HOME || e->page == (uint32_t)SEL_ROUTE_PAGE_SHELF) { /* the Shelf page reads the Home source too */
        send(ctx, "VIEW_SOURCE home");
    } else if (e->page == (uint32_t)SEL_ROUTE_PAGE_SEARCH) {
        send(ctx, "VIEW_SOURCE search");
    } else if (e->page == (uint32_t)SEL_ROUTE_PAGE_COLLECTION) {
        char head[40];
        if (e->kind == (uint32_t)SEL_LIB_ITEM_PLAYLIST || e->key[0] == '\0') {
            return; /* a playlist page is the Playlists door's; the MUI has no sidebar row for it */
        }
        snprintf(head, sizeof head, "COLLECTION_OPEN %u", (unsigned)e->kind);
        if (mui_path_line(head, e->key, line, sizeof line) != 0u) {
            if (restore) {
                size_t n = strlen(line);
                snprintf(line + n, sizeof line - n, " 1");
            }
            send(ctx, line);
        }
    } else if (e->page >= (uint32_t)SEL_ROUTE_PAGE_OTHER_BASE) { /* a door page: key holds its lines, ';' apart */
        const char* p = e->key;
        while (*p != '\0') {
            const char* semi = strchr(p, ';');
            size_t n = semi != NULL ? (size_t)(semi - p) : strlen(p);
            if (n > 0u && n < sizeof line) {
                memcpy(line, p, n);
                line[n] = '\0';
                send(ctx, line);
            }
            p += n + (semi != NULL ? 1u : 0u);
        }
    }
}

int mui_route_click(mui_route* r, sel_route_target target, uint32_t item_kind, uint32_t id, const char* key, const char* name, const char* sub,
                    mui_route_send_fn send, void* ctx) {
    sel_route_entry e;
    seed_home(r);
    if (!sel_route_go(&r->rh, target, item_kind, id, key != NULL ? key : "", name != NULL ? name : "", sub != NULL ? sub : "", &e)) {
        return 0;
    }
    sel_route_hist_push(&r->rh, &e);
    enter(r, &e, 0, send, ctx);
    return 1;
}

int mui_route_door(mui_route* r, uint32_t door, const char* lines, mui_route_send_fn send, void* ctx) {
    sel_route_target t = door == 0u ? SEL_ROUTE_DOOR_HOME : SEL_ROUTE_DOOR_PAGE;
    return mui_route_click(r, t, door, 0u, lines, "", "", send, ctx);
}

int mui_route_walk(mui_route* r, int dir, mui_route_send_fn send, void* ctx) {
    const sel_route_entry* w;
    sel_route_entry e;
    seed_home(r);
    if (!sel_route_hist_can(&r->rh, dir)) {
        return 0;
    }
    w = sel_route_hist_walk(&r->rh, dir);
    e = *w;
    enter(r, &e, 1, send, ctx);
    return 1;
}

static uint32_t g_lit_calls;

uint32_t mui_route_lit_calls(void) { return g_lit_calls; }

int mui_route_lit_door(const mui_route* r) {
    const sel_route_entry* e = mui_route_cur(r);
    g_lit_calls++;
    if (e == NULL || e->page == (uint32_t)SEL_ROUTE_PAGE_HOME) { /* nothing entered yet: the window opens on Home */
        return 0;
    }
    if (e->page >= (uint32_t)SEL_ROUTE_PAGE_OTHER_BASE && e->page < (uint32_t)SEL_ROUTE_PAGE_OTHER_BASE + SEL_UIM_DOOR_COUNT) {
        return (int)(e->page - (uint32_t)SEL_ROUTE_PAGE_OTHER_BASE);
    }
    return -1;
}

int mui_route_can(const mui_route* r, int dir) { return sel_route_hist_can(&r->rh, dir); }

const sel_route_entry* mui_route_cur(const mui_route* r) { return r->rh.n > 0u && r->rh.pos < r->rh.n ? &r->rh.e[r->rh.pos] : NULL; }

int mui_route_on_collection(const mui_route* r) {
    const sel_route_entry* e = mui_route_cur(r);
    return e != NULL && e->page == (uint32_t)SEL_ROUTE_PAGE_COLLECTION && e->kind != (uint32_t)SEL_LIB_ITEM_PLAYLIST;
}

int mui_route_on_shelf(const mui_route* r) {
    const sel_route_entry* e = mui_route_cur(r);
    return e != NULL && e->page == (uint32_t)SEL_ROUTE_PAGE_SHELF;
}

void mui_route_title(const mui_route* r, char* out, size_t cap) {
    const sel_route_entry* e = mui_route_cur(r);
    if (e == NULL) {
        snprintf(out, cap, " ");
    } else if (e->page == (uint32_t)SEL_ROUTE_PAGE_SHELF) {
        snprintf(out, cap, "Shelf %u", (unsigned)e->id);
    } else {
        snprintf(out, cap, "%s", e->name[0] != '\0' ? e->name : e->key);
    }
}

/* I966: see mui_route.h. */
static uint32_t g_flag_writes, g_flag_skips;

int mui_flag_commit(mui_flag* f, int value) {
    uint8_t v = value != 0 ? 1u : 0u;
    if (f->known != 0u && f->value == v) {
        g_flag_skips++;
        return 0;
    }
    f->known = 1u;
    f->value = v;
    g_flag_writes++;
    return 1;
}

uint32_t mui_flag_writes(void) {
    return g_flag_writes;
}

uint32_t mui_flag_skips(void) {
    return g_flag_skips;
}

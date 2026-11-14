/* I794 (drift doc T1, P2, P3): the MUI side of the shared route table and history (include/selecta/uimodel/route.h, I678h). The click
 * targets resolve through sel_route_go, the history is the shared sel_route_hist, and the page an entry names is entered by sending the
 * engine lines the desktop UI sends (desk_route_enter, desk_page_load). Pure C, host-testable: the lines go through a callback. */
#ifndef MUI_ROUTE_H
#define MUI_ROUTE_H

#include <selecta/engine.h>
#include <selecta/uimodel/route.h>
#include <selecta/uimodel/select.h>

#include <stddef.h>
#include <stdint.h>

typedef int (*mui_route_send_fn)(void* ctx, const char* line);

/* I1329i: what the leaving page keeps (desk_rt_leave): the list scroll of a collection page, the facet chip of the Search page. */
typedef struct mui_route_leave_info {
    int32_t scroll;
    uint32_t facet;
} mui_route_leave_info;
typedef void (*mui_route_leave_fn)(void* ctx, mui_route_leave_info* out);

typedef struct mui_route {
    sel_route_hist rh;
    mui_route_leave_fn leave_fn; /* I1329i: asked once per page change; NULL keeps nothing */
    void* leave_ctx;
    int32_t restore_scroll;      /* I1329i: the scroll Back or Forward found; taken by mui_route_take_scroll when the page's rows are shown */
    uint32_t restore_pending;
    uint32_t entered; /* pages entered through the handler: the sentinel that it ran */
} mui_route;

void mui_route_init(mui_route* r);
void mui_route_set_leave(mui_route* r, mui_route_leave_fn fn, void* ctx);
/* I1329i: 1 and the scroll to put back once, after a Back or Forward onto a collection page; 0 otherwise. */
int mui_route_take_scroll(mui_route* r, int32_t* scroll);
/* I1329h: the PLAY_CONTEXT line (desk_play_context, via sel_uim_context_line) that plays row `picked` of a collection page and queues the rest
 * behind it, named after the page. The list shows the stored order; `pos` is the shown position the engine counts. Length, 0 when none. */
uint32_t mui_col_play_line(const sel_collection_page* pg, uint32_t picked, char* out, size_t cap);
/* A click on `target` (design doc section 9): resolves, records in the history and enters the page. 1 when a page opened. */
int mui_route_click(mui_route* r, sel_route_target target, uint32_t item_kind, uint32_t id, const char* key, const char* name, const char* sub,
                    mui_route_send_fn send, void* ctx);
/* A sidebar or top bar door: its engine lines ("a;b;c") are the entry's key, so Back and Forward send them again. */
int mui_route_door(mui_route* r, uint32_t door, const char* lines, mui_route_send_fn send, void* ctx);
/* Back (dir < 0) and Forward (dir > 0). 1 when the page changed. */
int mui_route_walk(mui_route* r, int dir, mui_route_send_fn send, void* ctx);
int mui_route_can(const mui_route* r, int dir);
/* I797 (S2): the sidebar door the history is on (the Mac draw_sidebar "here" test), -1 when no door row is current (a collection or shelf
 * page). The history writes Home as the Home page and every other door as OTHER_BASE + door. mui_route_lit_calls() is the sentinel. */
int mui_route_lit_door(const mui_route* r);
uint32_t mui_route_lit_calls(void);

/* The page the history is on. */
const sel_route_entry* mui_route_cur(const mui_route* r);
int mui_route_on_collection(const mui_route* r); /* an album, artist, composer or playlist page */
int mui_route_on_shelf(const mui_route* r);      /* the Show all page of a Home shelf */
/* The header line of the open page: the collection's or shelf's name ("Shelf 2" when unnamed). */
void mui_route_title(const mui_route* r, char* out, size_t cap);

/* I966: a set() of an attribute whose value did not change still runs MUI's notify and relayout path, and mui_refresh runs on every
 * NewInput. mui_flag remembers the last value written (zero-initialised = unknown); mui_flag_commit returns 1 only when the value
 * differs, so the caller sets the attribute only then. mui_flag_writes()/mui_flag_skips() are the sentinels. */
typedef struct mui_flag {
    uint8_t known;
    uint8_t value;
} mui_flag;
int mui_flag_commit(mui_flag* f, int value);
uint32_t mui_flag_writes(void);
uint32_t mui_flag_skips(void);

#endif

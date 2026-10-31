/* I794 (drift doc T1, P2, P3): the MUI side of the shared route table and history (include/selecta/uimodel/route.h, I678h). The click
 * targets resolve through sel_route_go, the history is the shared sel_route_hist, and the page an entry names is entered by sending the
 * engine lines the desktop UI sends (desk_route_enter, desk_page_load). Pure C, host-testable: the lines go through a callback. */
#ifndef MUI_ROUTE_H
#define MUI_ROUTE_H

#include <selecta/uimodel/route.h>

#include <stddef.h>
#include <stdint.h>

typedef int (*mui_route_send_fn)(void* ctx, const char* line);

typedef struct mui_route {
    sel_route_hist rh;
    uint32_t entered; /* pages entered through the handler: the sentinel that it ran */
} mui_route;

void mui_route_init(mui_route* r);
/* A click on `target` (design doc section 9): resolves, records in the history and enters the page. 1 when a page opened. */
int mui_route_click(mui_route* r, sel_route_target target, uint32_t item_kind, uint32_t id, const char* key, const char* name, const char* sub,
                    mui_route_send_fn send, void* ctx);
/* A sidebar or top bar door: its engine lines ("a;b;c") are the entry's key, so Back and Forward send them again. */
int mui_route_door(mui_route* r, uint32_t door, const char* lines, mui_route_send_fn send, void* ctx);
/* Back (dir < 0) and Forward (dir > 0). 1 when the page changed. */
int mui_route_walk(mui_route* r, int dir, mui_route_send_fn send, void* ctx);
int mui_route_can(const mui_route* r, int dir);

/* The page the history is on. */
const sel_route_entry* mui_route_cur(const mui_route* r);
int mui_route_on_collection(const mui_route* r); /* an album, artist or composer page */
int mui_route_on_shelf(const mui_route* r);      /* the Show all page of a Home shelf */
/* The header line of the open page: the collection's or shelf's name ("Shelf 2" when unnamed). */
void mui_route_title(const mui_route* r, char* out, size_t cap);

#endif

/* The click-to-page route table and the app-wide page history (I678h, design doc 2026-10-29-desktop-library-architecture section 9).
 * One table says which click target opens which page, so the desktop UI and the MUI UI share the map; one history walks the pages
 * Back and Forward, each entry (page, kind, key, scroll). Plain C99, no allocation. */
#ifndef SELECTA_UIMODEL_ROUTE_H
#define SELECTA_UIMODEL_ROUTE_H

#include <stdint.h>

/* Every click target of the section 9 map. */
typedef enum sel_route_target {
    SEL_ROUTE_DOOR_HOME = 0,     /* top bar Home */
    SEL_ROUTE_DOOR_SEARCH,       /* top bar Search */
    SEL_ROUTE_HEADER_TITLE,      /* sidebar header "Your Library" (D1: nothing) */
    SEL_ROUTE_CHIP,              /* chip, chip X, chevrons (filter the list only) */
    SEL_ROUTE_SIDEBAR_ROW,       /* sidebar row of any kind: that item's page, row marked open */
    SEL_ROUTE_HOME_TILE,         /* Home quick-access tile or shelf card: the item's page */
    SEL_ROUTE_HOME_SONG,         /* Home song card: plays, no page */
    SEL_ROUTE_SHOW_ALL,          /* shelf "Show all": the Shelf page */
    SEL_ROUTE_TRACK_ARTIST,      /* artist name on a track row */
    SEL_ROUTE_TRACK_ALBUM,       /* album name on a track row */
    SEL_ROUTE_META_ARTIST,       /* artist name on an album page meta line */
    SEL_ROUTE_ALBUM_CARD,        /* album card in "More by" or artist "Albums" */
    SEL_ROUTE_SONGINFO_COMPOSER, /* composer name in Song Information */
    SEL_ROUTE_BROWSE_TILE,       /* Browse tile (format, composer, source) */
    SEL_ROUTE_TRACK_ROW,         /* track row double-click / Enter: plays, no page */
    SEL_ROUTE_TARGETS
} sel_route_target;

typedef enum sel_route_page { SEL_ROUTE_PAGE_NONE = 0, SEL_ROUTE_PAGE_HOME, SEL_ROUTE_PAGE_SEARCH, SEL_ROUTE_PAGE_SHELF, SEL_ROUTE_PAGE_COLLECTION } sel_route_page;

typedef struct sel_route {
    uint32_t page;     /* sel_route_page; NONE when the click opens no page */
    uint32_t kind;     /* SEL_LIB_ITEM_* of a collection page, else 0 */
    int plays;         /* the click plays a song (no page) */
    int marks_sidebar; /* the open row is marked in the sidebar */
} sel_route;

/* The route of `target`. `item_kind` is the SEL_LIB_ITEM_* of the clicked item for the targets whose page depends on it (sidebar
 * row, Home tile, Browse tile); the other targets fix the kind. Returns 1 when the click opens a page, else 0. Pure. */
int sel_route_resolve(sel_route_target target, uint32_t item_kind, sel_route* out);

/* Printable names for the dump and the tests: "home", "collection"; "album", "artist". */
const char* sel_route_page_name(uint32_t page);
const char* sel_route_kind_name(uint32_t kind);

#define SEL_ROUTE_HIST_MAX 32u
#define SEL_ROUTE_KEY_MAX 96u

typedef struct sel_route_entry {
    uint32_t page;
    uint32_t kind;
    uint32_t id;    /* playlist id, else 0 */
    int32_t scroll; /* the page's scroll when it was left */
    char key[SEL_ROUTE_KEY_MAX];
    char name[SEL_ROUTE_KEY_MAX];
    char sub[SEL_ROUTE_KEY_MAX];
} sel_route_entry;

typedef struct sel_route_hist {
    sel_route_entry e[SEL_ROUTE_HIST_MAX];
    uint32_t n, pos;
    uint32_t calls; /* clicks taken through sel_route_go: the sentinel that this code ran */
} sel_route_hist;

void sel_route_hist_init(sel_route_hist* h);
/* Seeds the history with the page the window is on (no-op when not empty). */
void sel_route_hist_seed(sel_route_hist* h, const sel_route_entry* cur);
/* Remembers the scroll of the current entry (called before leaving it). */
void sel_route_hist_scroll(sel_route_hist* h, int32_t scroll);
/* New navigation: drops the forward entries, appends `to` (not repeated when equal to the current entry), the oldest falls off at the
 * cap. Returns 1 when an entry was added. */
int sel_route_hist_push(sel_route_hist* h, const sel_route_entry* to);
/* Back (dir < 0) / Forward (dir > 0): returns the entry now current, or NULL at the end. Does not count a route call. */
const sel_route_entry* sel_route_hist_walk(sel_route_hist* h, int dir);
int sel_route_hist_can(const sel_route_hist* h, int dir);

/* The one entry point of a click: resolves `target`, counts the call, and when it opens a page fills `out` with the entry to enter
 * (the caller saves the current scroll, then pushes it with sel_route_hist_push). Returns 1 when a page opens. */
int sel_route_go(sel_route_hist* h, sel_route_target target, uint32_t item_kind, uint32_t id, const char* key, const char* name, const char* sub,
                 sel_route_entry* out);

#endif

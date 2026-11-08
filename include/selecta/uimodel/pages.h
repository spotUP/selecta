/* The page table (I792, drift doc 2026-10-30-mui-mac-drift section 4): every page the UIs can show, one table. The desktop UI
 * (plugins/ui/desktop) takes its page ids, --view names and route page ids from it; the MUI UI maps each page to a slot or writes an
 * exemption (mui_page_slot in plugins/ui/mui/map), and tests/ui/mui_contract_test.c fails when a page has neither. Pure C99. */
#ifndef SELECTA_UIMODEL_PAGES_H
#define SELECTA_UIMODEL_PAGES_H

#include <stdint.h>
#include <selecta/engine.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The page ids; the numbers are the history's 100 + page (route.h SEL_ROUTE_PAGE_OTHER_BASE), so append, never reorder. */
typedef enum sel_uim_page_id {
    SEL_UIM_PG_HOME,
    SEL_UIM_PG_SEARCH,
    SEL_UIM_PG_FOLDERS,
    SEL_UIM_PG_PLAYLISTS,
    SEL_UIM_PG_QUEUE,
    SEL_UIM_PG_NOWPLAYING,
    SEL_UIM_PG_SONGS,
    SEL_UIM_PG_SONGINFO,
    SEL_UIM_PG_VIZ,
    SEL_UIM_PG_TRACKER,
    SEL_UIM_PG_SHELF,
    SEL_UIM_PG_BROWSE,
    SEL_UIM_PG_COLLECTION,
    SEL_UIM_PG_SETTINGS, /* I799a: the Settings page, one row per sel_uim_pref_row */
    SEL_UIM_PG_COUNT
} sel_uim_page_id;

typedef struct sel_uim_page {
    const char* name; /* the --view name, also the door table's page name */
    uint32_t route;   /* the sel_route_page the page has in the route table; 0 when it has none (the history then writes 100 + the id) */
} sel_uim_page;

/* Page `id`, NULL past SEL_UIM_PG_COUNT. */
const sel_uim_page* sel_uim_page_at(uint32_t id);
/* The id of the page called `name`, SEL_UIM_PG_COUNT when there is none. */
uint32_t sel_uim_page_by_name(const char* name);
/* The page id the history's route page `rp` stands for; SEL_UIM_PG_HOME for an unknown one. */
uint32_t sel_uim_page_of_route(uint32_t rp);
/* The route page id the history writes for page `id`. */
uint32_t sel_uim_page_route(uint32_t id);
/* The text UI's page keys and the page each opens (I279): the one table the desktop and MUI key loads and dispatches read. */
#define SEL_UIM_PAGE_KEY_COUNT 6u
/* Row i: the `@action` name in *action and the page it opens in *page; 0 past SEL_UIM_PAGE_KEY_COUNT. */
int sel_uim_page_key_at(uint32_t i, const char** action, uint32_t* page);

/* I1189: the Home shelves, one split for every UI. A heading row starts a shelf; the rows after it are its tiles; the quick-pick grid
 * (a heading with `quick` set) is not a shelf; a heading with no row under it is not a shelf either (I311). */
typedef struct sel_uim_shelf {
    uint32_t head;  /* index of the heading in the view window */
    uint32_t first; /* index of the first row after it */
    uint32_t tiles; /* rows with a key */
    uint32_t rows;  /* rows after the heading */
} sel_uim_shelf;

/* Splits the HOME window of `v` into at most `max` shelves in `out`; returns the count, 0 when `v` is not the HOME source. */
uint32_t sel_uim_home_shelves(const sel_view_state* v, sel_uim_shelf* out, uint32_t max);

/* I1189: the page a Home tile opens. A composer tile opens that composer's page, an album tile with an album id opens the album page
 * by that id, an item card opens its item's page; any other tile plays (the function returns 0). The strings are copies: a page
 * change replaces the view the row lives in. Feed kind/id/key/name/sub to sel_route_go with SEL_ROUTE_HOME_TILE. */
typedef struct sel_uim_tile {
    uint32_t kind; /* SEL_LIB_ITEM_* */
    uint32_t id;
    char key[160];
    char name[256];
    char sub[160];
} sel_uim_tile;
int sel_uim_tile_route(const sel_view_row* r, sel_uim_tile* out);

#ifdef __cplusplus
}
#endif
#endif

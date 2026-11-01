/* The page table (I792, drift doc 2026-10-30-mui-mac-drift section 4): every page the UIs can show, one table. The desktop UI
 * (plugins/ui/desktop) takes its page ids, --view names and route page ids from it; the MUI UI maps each page to a slot or writes an
 * exemption (mui_page_slot in plugins/ui/mui/map), and tests/ui/mui_contract_test.c fails when a page has neither. Pure C99. */
#ifndef SELECTA_UIMODEL_PAGES_H
#define SELECTA_UIMODEL_PAGES_H

#include <stdint.h>

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

#ifdef __cplusplus
}
#endif
#endif

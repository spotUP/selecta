/* The page table (I792): see include/selecta/uimodel/pages.h. */
#include <stddef.h>
#include <string.h>
#include <selecta/uimodel/pages.h>
#include <selecta/uimodel/route.h>

static const sel_uim_page g_pages[SEL_UIM_PG_COUNT] = {
    [SEL_UIM_PG_HOME] = {"home", SEL_ROUTE_PAGE_HOME},
    [SEL_UIM_PG_SEARCH] = {"search", SEL_ROUTE_PAGE_SEARCH},
    [SEL_UIM_PG_FOLDERS] = {"folders", 0u},
    [SEL_UIM_PG_PLAYLISTS] = {"playlists", 0u},
    [SEL_UIM_PG_QUEUE] = {"queue", 0u},
    [SEL_UIM_PG_NOWPLAYING] = {"nowplaying", 0u},
    [SEL_UIM_PG_SONGS] = {"songs", 0u},
    [SEL_UIM_PG_SONGINFO] = {"songinfo", 0u},
    [SEL_UIM_PG_VIZ] = {"viz", 0u},
    [SEL_UIM_PG_TRACKER] = {"tracker", 0u},
    [SEL_UIM_PG_SHELF] = {"shelf", SEL_ROUTE_PAGE_SHELF},
    [SEL_UIM_PG_BROWSE] = {"browse", 0u},
    [SEL_UIM_PG_COLLECTION] = {"collection", SEL_ROUTE_PAGE_COLLECTION},
    [SEL_UIM_PG_SETTINGS] = {"settings", 0u}};

const sel_uim_page* sel_uim_page_at(uint32_t id) {
    return id < (uint32_t)SEL_UIM_PG_COUNT ? &g_pages[id] : (const sel_uim_page*)0;
}

uint32_t sel_uim_page_by_name(const char* name) {
    uint32_t i;
    for (i = 0; name != NULL && i < (uint32_t)SEL_UIM_PG_COUNT; i++) {
        if (strcmp(g_pages[i].name, name) == 0) {
            return i;
        }
    }
    return (uint32_t)SEL_UIM_PG_COUNT;
}

uint32_t sel_uim_page_route(uint32_t id) {
    return id < (uint32_t)SEL_UIM_PG_COUNT && g_pages[id].route != 0u ? g_pages[id].route : (uint32_t)SEL_ROUTE_PAGE_OTHER_BASE + id;
}

uint32_t sel_uim_page_of_route(uint32_t rp) {
    uint32_t i;
    for (i = 0; i < (uint32_t)SEL_UIM_PG_COUNT; i++) {
        if (g_pages[i].route != 0u && g_pages[i].route == rp) {
            return i;
        }
    }
    if (rp >= (uint32_t)SEL_ROUTE_PAGE_OTHER_BASE && rp - (uint32_t)SEL_ROUTE_PAGE_OTHER_BASE < (uint32_t)SEL_UIM_PG_COUNT) {
        return rp - (uint32_t)SEL_ROUTE_PAGE_OTHER_BASE;
    }
    return (uint32_t)SEL_UIM_PG_HOME;
}

/* Unit test of the route table and the page history (I678h): the table lookups, push/pop, truncation on new navigation, the cap. */
#include <stdio.h>
#include <string.h>
#include <selecta/library_item.h>
#include <selecta/uimodel/route.h>

#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); return 1; } } while (0)

static sel_route_entry ent(uint32_t page, uint32_t kind, const char* key) {
    sel_route_entry e;
    memset(&e, 0, sizeof e);
    e.page = page;
    e.kind = kind;
    strcpy(e.key, key);
    return e;
}

int main(void) {
    sel_route r;
    sel_route_hist h;
    sel_route_entry e, home = ent(SEL_ROUTE_PAGE_HOME, 0, "");
    const sel_route_entry* w;
    uint32_t i;
    CHECK(sel_route_resolve(SEL_ROUTE_DOOR_HOME, 0, &r) && r.page == SEL_ROUTE_PAGE_HOME);
    CHECK(sel_route_resolve(SEL_ROUTE_DOOR_SEARCH, 0, &r) && r.page == SEL_ROUTE_PAGE_SEARCH);
    CHECK(!sel_route_resolve(SEL_ROUTE_HEADER_TITLE, 0, &r));
    CHECK(!sel_route_resolve(SEL_ROUTE_CHIP, 0, &r));
    CHECK(sel_route_resolve(SEL_ROUTE_SIDEBAR_ROW, SEL_LIB_ITEM_PLAYLIST, &r) && r.kind == SEL_LIB_ITEM_PLAYLIST && r.marks_sidebar);
    CHECK(sel_route_resolve(SEL_ROUTE_HOME_TILE, SEL_LIB_ITEM_ARTIST, &r) && r.kind == SEL_LIB_ITEM_ARTIST && r.page == SEL_ROUTE_PAGE_COLLECTION);
    CHECK(!sel_route_resolve(SEL_ROUTE_HOME_SONG, 0, &r) && r.plays);
    CHECK(sel_route_resolve(SEL_ROUTE_SHOW_ALL, 0, &r) && r.page == SEL_ROUTE_PAGE_SHELF);
    CHECK(sel_route_resolve(SEL_ROUTE_TRACK_ARTIST, 0, &r) && r.kind == SEL_LIB_ITEM_ARTIST);
    CHECK(sel_route_resolve(SEL_ROUTE_TRACK_ALBUM, 0, &r) && r.kind == SEL_LIB_ITEM_ALBUM);
    CHECK(sel_route_resolve(SEL_ROUTE_META_ARTIST, 0, &r) && r.kind == SEL_LIB_ITEM_ARTIST);
    CHECK(sel_route_resolve(SEL_ROUTE_ALBUM_CARD, 0, &r) && r.kind == SEL_LIB_ITEM_ALBUM);
    CHECK(sel_route_resolve(SEL_ROUTE_SONGINFO_COMPOSER, 0, &r) && r.kind == SEL_LIB_ITEM_COMPOSER);
    CHECK(sel_route_resolve(SEL_ROUTE_BROWSE_TILE, SEL_LIB_ITEM_COMPOSER, &r) && r.kind == SEL_LIB_ITEM_COMPOSER);
    CHECK(!sel_route_resolve(SEL_ROUTE_TRACK_ROW, 0, &r) && r.plays);
    CHECK(sel_route_resolve(SEL_ROUTE_DOOR_PAGE, 3, &r) && r.page == SEL_ROUTE_PAGE_OTHER_BASE + 3u && r.kind == 0); /* I678h2: a door of any other page is 100 + the UI page */
    CHECK(strcmp(sel_route_kind_name(SEL_LIB_ITEM_ALBUM), "album") == 0 && strcmp(sel_route_page_name(SEL_ROUTE_PAGE_COLLECTION), "collection") == 0);

    sel_route_hist_init(&h); /* sel_route_go counts every call, opening or not */
    CHECK(sel_route_go(&h, SEL_ROUTE_ALBUM_CARD, 0, 0, "7", "Alb", "Art", &e) && h.calls == 1 && e.kind == SEL_LIB_ITEM_ALBUM && strcmp(e.key, "7") == 0);
    CHECK(!sel_route_go(&h, SEL_ROUTE_HEADER_TITLE, 0, 0, NULL, NULL, NULL, &e) && h.calls == 2);

    sel_route_hist_init(&h); /* push, back, forward, truncation */
    sel_route_hist_seed(&h, &home);
    e = ent(SEL_ROUTE_PAGE_COLLECTION, SEL_LIB_ITEM_ALBUM, "1");
    CHECK(sel_route_hist_push(&h, &e) && h.n == 2 && h.pos == 1);
    CHECK(!sel_route_hist_push(&h, &e) && h.n == 2); /* the same page is not repeated */
    sel_route_hist_scroll(&h, 480);
    e = ent(SEL_ROUTE_PAGE_COLLECTION, SEL_LIB_ITEM_ARTIST, "a");
    CHECK(sel_route_hist_push(&h, &e) && h.n == 3);
    w = sel_route_hist_walk(&h, -1);
    CHECK(w != NULL && w->kind == SEL_LIB_ITEM_ALBUM && w->scroll == 480 && h.pos == 1);
    w = sel_route_hist_walk(&h, -1);
    CHECK(w != NULL && w->page == SEL_ROUTE_PAGE_HOME && h.pos == 0);
    CHECK(sel_route_hist_walk(&h, -1) == NULL && !sel_route_hist_can(&h, -1));
    w = sel_route_hist_walk(&h, 1);
    CHECK(w != NULL && w->kind == SEL_LIB_ITEM_ALBUM && sel_route_hist_can(&h, 1));
    e = ent(SEL_ROUTE_PAGE_SEARCH, 0, "");
    CHECK(sel_route_hist_push(&h, &e) && h.n == 3 && !sel_route_hist_can(&h, 1)); /* new navigation drops the forward entry */
    CHECK(sel_route_hist_walk(&h, 1) == NULL);

    sel_route_hist_init(&h); /* the cap: the oldest falls off */
    sel_route_hist_seed(&h, &home);
    for (i = 1; i < SEL_ROUTE_HIST_MAX + 5u; i++) {
        char k[16];
        snprintf(k, sizeof k, "%u", (unsigned)i);
        e = ent(SEL_ROUTE_PAGE_COLLECTION, SEL_LIB_ITEM_ALBUM, k);
        sel_route_hist_push(&h, &e);
    }
    CHECK(h.n == SEL_ROUTE_HIST_MAX && h.pos == SEL_ROUTE_HIST_MAX - 1u);
    CHECK(strcmp(h.e[0].key, "5") == 0 && strcmp(h.e[h.n - 1u].key, "36") == 0);
    printf("route_unit_ok\n");
    return 0;
}

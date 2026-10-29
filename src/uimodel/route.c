/* The click-to-page route table and the app-wide page history (I678h). See include/selecta/uimodel/route.h. */
#include <stddef.h>
#include <string.h>
#include <selecta/library_item.h>
#include <selecta/uimodel/route.h>

/* One row per click target: the page it opens, the kind it fixes (KIND_ITEM: the clicked item's own kind), whether it plays and marks. */
#define KIND_ITEM 0xFFFFFFFFu
static const struct {
    uint32_t page, kind;
    int plays, marks;
} g_route[SEL_ROUTE_TARGETS] = {
    [SEL_ROUTE_DOOR_HOME] = {SEL_ROUTE_PAGE_HOME, 0, 0, 0},
    [SEL_ROUTE_DOOR_SEARCH] = {SEL_ROUTE_PAGE_SEARCH, 0, 0, 0},
    [SEL_ROUTE_HEADER_TITLE] = {SEL_ROUTE_PAGE_NONE, 0, 0, 0},
    [SEL_ROUTE_CHIP] = {SEL_ROUTE_PAGE_NONE, 0, 0, 0},
    [SEL_ROUTE_SIDEBAR_ROW] = {SEL_ROUTE_PAGE_COLLECTION, KIND_ITEM, 0, 1},
    [SEL_ROUTE_HOME_TILE] = {SEL_ROUTE_PAGE_COLLECTION, KIND_ITEM, 0, 1},
    [SEL_ROUTE_HOME_SONG] = {SEL_ROUTE_PAGE_NONE, 0, 1, 0},
    [SEL_ROUTE_SHOW_ALL] = {SEL_ROUTE_PAGE_SHELF, 0, 0, 0},
    [SEL_ROUTE_TRACK_ARTIST] = {SEL_ROUTE_PAGE_COLLECTION, SEL_LIB_ITEM_ARTIST, 0, 1},
    [SEL_ROUTE_TRACK_ALBUM] = {SEL_ROUTE_PAGE_COLLECTION, SEL_LIB_ITEM_ALBUM, 0, 1},
    [SEL_ROUTE_META_ARTIST] = {SEL_ROUTE_PAGE_COLLECTION, SEL_LIB_ITEM_ARTIST, 0, 0},
    [SEL_ROUTE_ALBUM_CARD] = {SEL_ROUTE_PAGE_COLLECTION, SEL_LIB_ITEM_ALBUM, 0, 0},
    [SEL_ROUTE_SONGINFO_COMPOSER] = {SEL_ROUTE_PAGE_COLLECTION, SEL_LIB_ITEM_COMPOSER, 0, 0},
    [SEL_ROUTE_BROWSE_TILE] = {SEL_ROUTE_PAGE_COLLECTION, KIND_ITEM, 0, 0},
    [SEL_ROUTE_TRACK_ROW] = {SEL_ROUTE_PAGE_NONE, 0, 1, 0},
};

int sel_route_resolve(sel_route_target target, uint32_t item_kind, sel_route* out) {
    sel_route r = {SEL_ROUTE_PAGE_NONE, 0, 0, 0};
    if ((unsigned)target < (unsigned)SEL_ROUTE_TARGETS) {
        r.page = g_route[target].page;
        r.kind = g_route[target].kind == KIND_ITEM ? item_kind : g_route[target].kind;
        r.plays = g_route[target].plays;
        r.marks_sidebar = g_route[target].marks;
        if (r.page != SEL_ROUTE_PAGE_COLLECTION) {
            r.kind = 0;
        }
    }
    if (out != NULL) {
        *out = r;
    }
    return r.page != SEL_ROUTE_PAGE_NONE;
}

const char* sel_route_page_name(uint32_t page) {
    static const char* const n[] = {"none", "home", "search", "shelf", "collection"};
    return page < sizeof n / sizeof n[0] ? n[page] : "none";
}

const char* sel_route_kind_name(uint32_t kind) {
    static const char* const n[SEL_LIB_ITEM_KINDS] = {"liked", "recent", "local", "playlist", "album", "artist", "composer", "source"};
    return kind < (uint32_t)SEL_LIB_ITEM_KINDS ? n[kind] : "";
}

void sel_route_hist_init(sel_route_hist* h) { memset(h, 0, sizeof *h); }

static int entry_same(const sel_route_entry* a, const sel_route_entry* b) {
    return a->page == b->page && a->kind == b->kind && a->id == b->id && strcmp(a->key, b->key) == 0;
}

void sel_route_hist_seed(sel_route_hist* h, const sel_route_entry* cur) {
    if (h->n == 0u) {
        h->e[0] = *cur;
        h->n = 1u;
        h->pos = 0u;
    }
}

void sel_route_hist_scroll(sel_route_hist* h, int32_t scroll) {
    if (h->n > 0u) {
        h->e[h->pos].scroll = scroll;
    }
}

int sel_route_hist_push(sel_route_hist* h, const sel_route_entry* to) {
    if (h->n == 0u) {
        h->e[0] = *to;
        h->n = 1u;
        h->pos = 0u;
        return 1;
    }
    h->n = h->pos + 1u; /* the forward entries are dropped */
    if (entry_same(&h->e[h->pos], to)) {
        return 0;
    }
    if (h->n == SEL_ROUTE_HIST_MAX) {
        memmove(h->e, h->e + 1, (h->n - 1u) * sizeof h->e[0]);
        h->n--;
    }
    h->e[h->n++] = *to;
    h->pos = h->n - 1u;
    return 1;
}

int sel_route_hist_can(const sel_route_hist* h, int dir) {
    return dir < 0 ? h->pos > 0u : h->pos + 1u < h->n;
}

const sel_route_entry* sel_route_hist_walk(sel_route_hist* h, int dir) {
    if (!sel_route_hist_can(h, dir)) {
        return NULL;
    }
    h->pos = dir < 0 ? h->pos - 1u : h->pos + 1u;
    return &h->e[h->pos];
}

int sel_route_go(sel_route_hist* h, sel_route_target target, uint32_t item_kind, uint32_t id, const char* key, const char* name, const char* sub,
                 sel_route_entry* out) {
    sel_route r;
    h->calls++;
    if (!sel_route_resolve(target, item_kind, &r) || out == NULL) {
        return 0;
    }
    memset(out, 0, sizeof *out);
    out->page = r.page;
    out->kind = r.kind;
    out->id = id;
    if (key != NULL) strncpy(out->key, key, sizeof out->key - 1u);
    if (name != NULL) strncpy(out->name, name, sizeof out->name - 1u);
    if (sub != NULL) strncpy(out->sub, sub, sizeof out->sub - 1u);
    return 1;
}

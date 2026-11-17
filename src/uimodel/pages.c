/* The page table (I792): see include/selecta/uimodel/pages.h. */
#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <selecta/library_item.h>
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

static const struct {
    const char* action;
    uint32_t page;
} k_page_keys[SEL_UIM_PAGE_KEY_COUNT] = {{"@home", SEL_UIM_PG_HOME},     {"@library", SEL_UIM_PG_FOLDERS}, {"@folders", SEL_UIM_PG_PLAYLISTS},
                                         {"@info", SEL_UIM_PG_SONGINFO}, {"@viz", SEL_UIM_PG_VIZ},         {"@pattern", SEL_UIM_PG_TRACKER}};

int sel_uim_page_key_at(uint32_t i, const char** action, uint32_t* page) {
    if (i >= SEL_UIM_PAGE_KEY_COUNT) {
        return 0;
    }
    *action = k_page_keys[i].action;
    *page = k_page_keys[i].page;
    return 1;
}

uint32_t sel_uim_home_shelves(const sel_view_state* v, sel_uim_shelf* out, uint32_t max) {
    uint32_t i, k, n = 0;
    int in_quick = 0;
    if (v == NULL || v->source != SEL_VIEW_HOME) {
        return 0;
    }
    for (i = 0; i < v->row_n; i++) {
        if (v->rows[i].kind == SEL_ROW_HEADING) {
            in_quick = (int)v->rows[i].quick; /* I478: the quick-pick grid is not a shelf */
            if (in_quick) {
                continue;
            }
            if (n == max) {
                break;
            }
            out[n].head = i;
            out[n].first = i + 1u;
            out[n].tiles = 0;
            out[n].rows = 0;
            n++;
        } else if (n > 0u && !in_quick) {
            out[n - 1u].rows++;
            out[n - 1u].tiles += v->rows[i].key != NULL;
        }
    }
    for (i = 0, k = 0; i < n; i++) {
        if (out[i].rows > 0u) {
            out[k++] = out[i];
        }
    }
    return k;
}

int sel_uim_tile_route(const sel_view_row* r, sel_uim_tile* out) {
    const char* key = r->key != NULL ? r->key : "";
    const char* art = r->artist != NULL ? r->artist : "";
    memset(out, 0, sizeof *out);
    if (r->composer) { /* I479: the composer's collection page, key and name are the canon */
        out->kind = (uint32_t)SEL_LIB_ITEM_COMPOSER;
        snprintf(out->key, sizeof out->key, "%s", key);
        snprintf(out->name, sizeof out->name, "%s", key);
        return 1;
    }
    if (r->album && r->art_id != 0u) { /* I678h2: the album page by the id the row carries */
        out->kind = (uint32_t)SEL_LIB_ITEM_ALBUM;
        out->id = r->art_id;
        snprintf(out->key, sizeof out->key, "%u", (unsigned)r->art_id);
        snprintf(out->name, sizeof out->name, "%s", key);
        snprintf(out->sub, sizeof out->sub, "%s", art);
        return 1;
    }
    if (r->item_kind != 0u) { /* I678j: an item card opens that item's page */
        out->kind = r->item_kind - 1u;
        out->id = (uint32_t)strtoul(key, NULL, 10);
        snprintf(out->key, sizeof out->key, "%s", key);
        snprintf(out->name, sizeof out->name, "%s", r->text);
        snprintf(out->sub, sizeof out->sub, "%s", art);
        return 1;
    }
    return 0;
}

int sel_uim_about_line(const char* about, uint32_t i, uint32_t max, char* label, size_t lcap, char* value, size_t vcap) {
    uint32_t k, links = 0;
    const char* p = about;
    if (about == NULL || i >= max) return 0;
    for (k = 0; *p != '\0'; k++) {
        const char* v = p + strlen(p) + 1u;
        if (strcmp(p, "Link") == 0) links++;
        if (k == i) {
            if (strcmp(p, "Link") == 0) snprintf(label, lcap, "Link %u", (unsigned)links);
            else snprintf(label, lcap, "%s", p);
            snprintf(value, vcap, "%s", v);
            return 1;
        }
        p = v + strlen(v) + 1u;
    }
    return 0;
}

const char* sel_uim_tag_name(const sel_collection_page* pg, uint32_t k) {
    const char* p = pg != NULL ? pg->tags : "";
    if (k >= SEL_UIM_TAG_MAX) {
        return NULL;
    }
    while (*p != '\0' && k > 0u) {
        p += strlen(p) + 1u;
        k--;
    }
    return *p != '\0' ? p : NULL;
}

uint32_t sel_uim_tag_count(const sel_collection_page* pg) {
    uint32_t n = 0u;
    while (sel_uim_tag_name(pg, n) != NULL) {
        n++;
    }
    return n;
}

void sel_uim_tagchip_label(const char* active, char* buf, size_t cap) {
    if (active != NULL && active[0] != '\0') {
        snprintf(buf, cap, "Tag: %s", active);
    } else {
        snprintf(buf, cap, "Tag");
    }
}

static int tag_same(const char* a, const char* b) { /* whole name, case-insensitive like the engine filter */
    while (*a != '\0' && tolower((unsigned char)*a) == tolower((unsigned char)*b)) {
        a++;
        b++;
    }
    return *a == *b;
}

void sel_uim_tag_next(const sel_collection_page* pg, const char* active, char* out, size_t cap) {
    uint32_t i, n = sel_uim_tag_count(pg);
    uint32_t next = n > 0u ? 1u : 0u; /* 0 = All Tags, k = tag k - 1; no active tag, or one the page does not list: the first tag */
    if (active != NULL && active[0] != '\0') {
        for (i = 0; i < n; i++) {
            if (tag_same(sel_uim_tag_name(pg, i), active)) {
                next = i + 2u;
                break;
            }
        }
    }
    snprintf(out, cap, "%s", next >= 1u && next <= n ? sel_uim_tag_name(pg, next - 1u) : "");
}

void sel_uim_tag_line(const char* tag, char* line, size_t cap) {
    snprintf(line, cap, "COLLECTION_TAG \"%s\"", tag != NULL ? tag : "");
}

/* Desktop UI plugin (M3d, DUI-P1-4). It renders the engine snapshot through the shared UI model
 * (include/selecta/uimodel.h): one widget per region, redrawn only when the region's dirty bit is set. Drawing goes
 * to a backend; the only backend so far is `mem` (headless, no SDL), selected with `--backend mem`. */
#include <selecta/plugin.h>
#include <selecta/ui.h>
#include <selecta/uimodel.h>

#include <stdio.h>
#include <string.h>

typedef struct desk {
    sel_engine* engine;
    const sel_host* host;
    sel_uim uim;
    uint32_t frames;
    uint32_t widget_draws;
    uint32_t hash;
    int report; /* --stats: the close hook prints its sentinel line */
    sel_tag stats[7];
} desk;

static const char* desk_opt(const sel_ui_open_args* a, const char* name) {
    int i;
    for (i = 0; i + 1 < a->argc; i++) {
        if (strcmp(a->argv[i], name) == 0) {
            return a->argv[i + 1];
        }
    }
    return NULL;
}

static void desk_hash(desk* d, const char* s) {
    for (; *s != '\0'; s++) {
        d->hash = (d->hash ^ (uint8_t)*s) * 16777619u;
    }
}

/* Redraw the dirty regions; the mem backend records what a draw would show in the frame hash. */
static int desk_present(desk* d) {
    uint32_t dirty = sel_uim_take_dirty(&d->uim);
    const sel_uim_frame* f = &d->uim.frame;
    uint32_t bit;
    if (dirty == 0u) {
        return 0;
    }
    for (bit = 1u; bit <= SEL_UIM_DIRTY_STATUS; bit <<= 1) {
        if ((dirty & bit) == 0u) {
            continue;
        }
        d->widget_draws++;
        d->hash = (d->hash ^ bit) * 16777619u;
        if (bit == SEL_UIM_DIRTY_SONG) {
            desk_hash(d, f->title);
            desk_hash(d, f->artist);
            desk_hash(d, f->format);
        } else if (bit == SEL_UIM_DIRTY_PROGRESS) {
            desk_hash(d, f->clock);
            desk_hash(d, f->remaining);
        } else if (bit == SEL_UIM_DIRTY_STATUS) {
            desk_hash(d, f->status);
        }
    }
    d->frames++;
    return 1;
}

static void desk_close(void* state) {
    desk* d = (desk*)state;
    if (d != NULL) {
        if (d->report) {
            printf("desktop_ui closed frames=%u title=%s\n", (unsigned)d->frames, d->uim.frame.title);
        }
        d->host->mem_free(d->host, d);
    }
}

static void* desk_open(const sel_ui_open_args* a) {
    const char* backend = desk_opt(a, "--backend");
    desk* d;
    int i;
    if (backend != NULL && strcmp(backend, "mem") != 0) {
        fprintf(stderr, "selecta: desktop backend '%s' is not available in this build\n", backend);
        return NULL;
    }
    d = (desk*)a->host->mem_alloc(a->host, sizeof *d, SEL_MEM_FAST);
    if (d == NULL) {
        return NULL;
    }
    memset(d, 0, sizeof *d);
    d->engine = a->engine;
    d->host = a->host;
    d->hash = 2166136261u;
    for (i = 0; i < a->argc; i++) {
        if (strcmp(a->argv[i], "--stats") == 0) {
            d->report = 1;
        }
    }
    sel_uim_init(&d->uim);
    sel_uim_update(&d->uim, sel_engine_state(d->engine));
    desk_present(d);
    return d;
}

/* The snapshot is refreshed at the end of a pump, after the events, so the model reads the state the previous
 * pump left: an event here and the wait below together never miss a change. */
static void desk_on_event(void* state, const sel_message* evt) {
    desk* d = (desk*)state;
    if (evt->id == SEL_EVT_PREFS_CHANGED && strcmp(evt->args[0].v.s, "ui.desktop.mode") == 0) {
        sel_uim_set_layout(&d->uim, evt->args[1].v.s);
    }
    sel_uim_update(&d->uim, sel_engine_state(d->engine));
}

/* The mem backend has no input source: a turn with something to draw is handled, a quiet turn ends the run. */
static int desk_wait(void* state, uint32_t timeout_ms) {
    desk* d = (desk*)state;
    (void)timeout_ms;
    sel_uim_update(&d->uim, sel_engine_state(d->engine));
    return desk_present(d) ? 1 : -1;
}

static const sel_tag* desk_stats(void* state) {
    desk* d = (desk*)state;
    d->stats[0].tag = SEL_UI_STAT_FRAMES;
    d->stats[0].value = d->frames;
    d->stats[1].tag = SEL_UI_STAT_HASH;
    d->stats[1].value = d->hash;
    d->stats[2].tag = SEL_UI_STAT_WIDGET_DRAWS;
    d->stats[2].value = d->widget_draws;
    d->stats[3].tag = SEL_UI_STAT_UIM_UPDATES;
    d->stats[3].value = d->uim.updates;
    d->stats[4].tag = SEL_UI_STAT_UIM_DIRTY;
    d->stats[4].value = d->uim.last_taken;
    d->stats[5].tag = SEL_UI_STAT_UIM_LAYOUT;
    d->stats[5].value = d->uim.layout;
    d->stats[6].tag = SEL_TAG_END;
    d->stats[6].value = 0;
    return d->stats;
}

static sel_tag g_hooks[8];
static sel_tag g_desc[5];

static void set(sel_tag* t, uint32_t tag, uintptr_t value) {
    t->tag = tag;
    t->value = value;
}

const sel_tag* SEL_PLUGIN_ENTRY(uint32_t host_api) {
    if (host_api < 1u) {
        return NULL;
    }
    set(&g_hooks[0], SEL_UI_NAME, (uintptr_t) "Desktop");
    set(&g_hooks[1], SEL_UI_OPEN, SEL_TAG_FN(desk_open));
    set(&g_hooks[2], SEL_UI_ON_EVENT, SEL_TAG_FN(desk_on_event));
    set(&g_hooks[3], SEL_UI_WAIT, SEL_TAG_FN(desk_wait));
    set(&g_hooks[4], SEL_UI_CLOSE, SEL_TAG_FN(desk_close));
    set(&g_hooks[5], SEL_UI_STATS, SEL_TAG_FN(desk_stats));
    set(&g_hooks[6], SEL_UI_VIEW_STYLES, 1);
    set(&g_hooks[7], SEL_TAG_END, 0);

    set(&g_desc[0], SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_UI);
    set(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "desktop");
    set(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    set(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    set(&g_desc[4], SEL_TAG_END, 0);
    return g_desc;
}

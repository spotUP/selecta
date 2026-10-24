/* Desktop UI plugin (M3d, DUI-P1-4). It renders the engine snapshot through the shared UI model
 * (include/selecta/uimodel.h): one widget per region, redrawn only when the region's dirty bit is set. Drawing goes
 * to a backend; the only backend so far is `mem` (headless, no SDL), selected with `--backend mem`. */
#include <selecta/plugin.h>
#include <selecta/ui.h>
#include <selecta/uimodel.h>

#include "theme.h"

#include <stdio.h>
#include <string.h>

typedef struct desk {
    sel_engine* engine;
    const sel_host* host;
    sel_uim uim;
    uint32_t frames;
    uint32_t widget_draws;
    uint32_t hash;
    const char* resize; /* --resize WxH[,WxH...]: resize events the mem backend replays at open */
    sel_uim_layout layout;
    int actions; /* --actions: the close hook prints the action table and the help rows */
    int report; /* --stats: the close hook prints its sentinel line */
    txt_theme theme;       /* design tokens (DUI-P1-10): defaults, then --theme <file> over them */
    int tokens;            /* --tokens: the close hook prints what the mem backend painted with the tokens */
    sel_uim_budget budget; /* --format: the effects the canvas format affords */
    uint32_t tweens;       /* tweens started (title cross-fade on a song change) */
    uint32_t effect_prims; /* blur and shadow primitives run */
    sel_tag stats[9];
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
            /* Every effect asks the governor first (DUI-P1-9): the cover backdrop blur, the panel shadow and, from
             * the second frame on, a title cross-fade. */
            if (sel_uim_budget_allows(&d->budget, SEL_UIM_FX_BLUR)) {
                d->effect_prims++;
            }
            if (sel_uim_budget_allows(&d->budget, SEL_UIM_FX_SHADOW)) {
                d->effect_prims++;
            }
            if (d->frames > 0u && sel_uim_budget_allows(&d->budget, SEL_UIM_FX_TWEEN)) {
                d->tweens++;
            }
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

static void desk_print_actions(void) {
    uint32_t i, n = sel_uim_action_count();
    char row[96], msg[160];
    for (i = 0; i < n; i++) {
        const sel_uim_action* a = sel_uim_action_at(i);
        printf("action id=%s label=%s icon=%s menu=%s key=%s line=%s tooltip=%s\n", a->id, a->label, a->icon, a->menu,
               a->key[0] != '\0' ? a->key : "-", a->line, a->tooltip);
        sel_uim_help_row(i, row, sizeof row);
        printf("help %s\n", row);
    }
    printf("actions count=%u exemptions=%u check=%s\n", (unsigned)n, (unsigned)sel_uim_exemption_count(),
           sel_uim_actions_check(msg, sizeof msg) == 0 ? "ok" : msg);
}

/* Each WxH is one resize event: the layout is recomputed and the result printed, regions with their rectangles. */
static void desk_replay_resizes(desk* d) {
    const char* p = d->resize;
    unsigned w, h;
    int used;
    char msg[96];
    while (sscanf(p, "%ux%u%n", &w, &h, &used) == 2) {
        uint32_t i;
        sel_uim_layout_compute(&d->layout, w, h);
        printf("layout width=%u height=%u id=%s regions=%u check=%s\n", w, h,
               sel_uim_breakpoint_name(d->layout.breakpoint), (unsigned)d->layout.count,
               sel_uim_layout_check(&d->layout, msg, sizeof msg) == 0 ? "ok" : msg);
        for (i = 0; i < d->layout.count; i++) {
            const sel_uim_region* r = &d->layout.regions[i];
            printf("region name=%s x=%d y=%d w=%u h=%u\n", r->name, (int)r->x, (int)r->y, (unsigned)r->w,
                   (unsigned)r->h);
        }
        p += used;
        if (*p != ',') {
            break;
        }
        p++;
    }
}

/* What the mem backend paints: every widget takes its colour and size from a token, never from a literal. */
static void desk_print_tokens(const desk* d) {
    static const uint32_t none[SEL_GUI_TOKEN_COUNT] = {
#define X(id, name, def, lo, hi) def,
        SEL_GUI_TOKENS(X)
#undef X
    };
    uint32_t changed = sel_gui_tokens_diff(d->theme.token, none), i, n = 0u;
    printf("pixel widget=play_button rgb=0x%06x\n", (unsigned)d->theme.token[SEL_GUI_TOKEN_ACCENT]);
    printf("pixel widget=panel rgb=0x%06x\n", (unsigned)d->theme.token[SEL_GUI_TOKEN_SURFACE]);
    printf("pixel widget=label rgb=0x%06x\n", (unsigned)d->theme.token[SEL_GUI_TOKEN_TEXT]);
    printf("metric name=corner_radius px=%u\n", (unsigned)d->theme.token[SEL_GUI_TOKEN_RADIUS]);
    printf("metric name=gap px=%u\n", (unsigned)d->theme.token[SEL_GUI_TOKEN_SPACING]);
    for (i = 0; i < SEL_GUI_TOKEN_COUNT; i++) {
        n += (changed >> i) & 1u;
    }
    printf("tokens count=%u changed=%u\n", (unsigned)SEL_GUI_TOKEN_COUNT, (unsigned)n);
    for (i = 0; i < SEL_GUI_TOKEN_COUNT; i++) {
        if ((changed >> i) & 1u) {
            printf("token changed=%s\n", sel_gui_token_name((sel_gui_token)i));
        }
    }
}

/* Reads the --theme file over the defaults; 0 with a message on a missing file or a bad line. */
static int desk_load_theme(desk* d, const char* path) {
    char buf[4096];
    size_t n;
    int bad = 0;
    FILE* f = fopen(path, "rb");
    if (f == NULL) {
        fprintf(stderr, "selecta: desktop theme '%s' cannot be read\n", path);
        return 0;
    }
    n = fread(buf, 1, sizeof buf - 1, f);
    fclose(f);
    buf[n] = '\0';
    if (txt_theme_parse(&d->theme, buf, &bad) < 0) {
        fprintf(stderr, "selecta: desktop theme '%s' line %d is not a role or token line in range\n", path, bad);
        return 0;
    }
    return 1;
}

static void desk_close(void* state) {
    desk* d = (desk*)state;
    if (d != NULL) {
        if (d->report) {
            printf("desktop_ui closed frames=%u title=%s clock=%s status=%s\n", (unsigned)d->frames, d->uim.frame.title,
                   d->uim.frame.clock, d->uim.frame.status);
        }
        if (d->tokens) {
            desk_print_tokens(d);
        }
        if (d->resize != NULL) {
            desk_replay_resizes(d);
        }
        if (d->actions) {
            desk_print_actions();
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
    if (!sel_uim_budget_for(&d->budget, desk_opt(a, "--format"))) {
        fprintf(stderr, "selecta: desktop format '%s' is not rgba32 or index8\n", desk_opt(a, "--format"));
        a->host->mem_free(a->host, d);
        return NULL;
    }
    txt_theme_default(&d->theme);
    if (desk_opt(a, "--theme") != NULL && !desk_load_theme(d, desk_opt(a, "--theme"))) {
        a->host->mem_free(a->host, d);
        return NULL;
    }
    for (i = 0; i < a->argc; i++) {
        if (strcmp(a->argv[i], "--tokens") == 0) {
            d->tokens = 1;
        }
        if (strcmp(a->argv[i], "--stats") == 0) {
            d->report = 1;
        }
        if (strcmp(a->argv[i], "--resize") == 0 && i + 1 < a->argc) {
            d->resize = a->argv[i + 1];
        }
        if (strcmp(a->argv[i], "--actions") == 0) {
            d->actions = 1;
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
    d->stats[6].tag = SEL_UI_STAT_TWEENS;
    d->stats[6].value = d->tweens;
    d->stats[7].tag = SEL_UI_STAT_EFFECT_PRIMS;
    d->stats[7].value = d->effect_prims;
    d->stats[8].tag = SEL_TAG_END;
    d->stats[8].value = 0;
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

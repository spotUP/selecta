/* Text UI plugin (design doc section 7, plan M1.1 phase 5, row I60): renders
 * sel_engine_state() into the cell grid and turns keys into engine commands.
 * It holds no playback logic. Input comes from the terminal, or from a keys
 * file (--keys) so a test drives the whole UI with no terminal; --screen-dump
 * writes the last frame as UTF-8 text. --frame-dump appends every flushed frame's terminal bytes, so a
 * test sees exactly what the console is sent (row I218d2). */
#include <selecta/format_label.h>
#include <selecta/engine.h>
#include <selecta/host.h>
#include <selecta/plugin.h>
#include <selecta/protocol.h>
#include <selecta/ui.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "grid.h"
#include <selecta/uimodel/sidestack.h>
/* browse.h #defines SEL_UIM_SIDE_CHIPS (the chip count) and shadows the side-stack kind of that name: take the kind first. */
static const sel_uim_side_kind TUI_SIDE_KIND_CHIPS = SEL_UIM_SIDE_CHIPS;
#include <selecta/uimodel/browse.h>
#include <selecta/uimodel/doors.h>
#include <selecta/uimodel/libfilter.h>
#include <selecta/uimodel/cmdline.h>
#include <selecta/uimodel/keybind.h>
#include <selecta/uimodel.h>
#include <selecta/uimodel/keys.h>
#include <selecta/uimodel/palette.h>
#include <selecta/uimodel/briefs.h>
#include <selecta/uimodel/pages.h>
#include <selecta/uimodel/route.h>
#include <selecta/uimodel/viewcmds.h>
#include <selecta/library_item.h>
#include "spawn.h"
#include "splash.h"
#include "term.h"
#include <selecta/uimodel/theme.h>
#include <selecta/uimodel/viewcmds.h>

#define TUI_POOL_BYTES 1024u
#define TUI_DEFAULT_COLS 80u
#define TUI_DEFAULT_ROWS 24u
#define TUI_INFO_COLS 30u
#define TUI_SEEK_MS 5000
#define TUI_MARKS 16u
#define TUI_MARK_BYTES 256u
#define TUI_VOLUME_STEP 5
#define TUI_NOTE_BYTES 128u

enum { TUI_INFO_NONE = 0, TUI_INFO_NOWPLAYING = 1, TUI_INFO_SONGINFO = 2 };

#define TUI_MENU_MAX 10u

/* Menu items (I1190): the song menu in sel_uim_song_menu_label order, the playlist menu, and the playlist picker. */
enum { MN_PLAY, MN_NEXT, MN_QUEUE, MN_ADDPL, MN_REMOVE, MN_RENAME, MN_DELETE, MN_NEWFOLDER, MN_NEWPL, MN_PLPICK, MN_QUIT };

typedef struct tui {
    sel_engine* engine;
    const sel_host* host;
    txt_grid grid;
    txt_theme theme;
    txt_keys keys;
    txt_bindings binds;
    txt_cmdline cmd;
    int cmd_mode; /* the command line has the keys */
    int overlay;  /* 0 none, 1 command palette, 2 help (read only), 3 Settings (I1188) */
    txt_palette pal;
    char marks[TUI_MARKS][TUI_MARK_BYTES]; /* selected browse files for `!` */
    uint32_t mark_n;
    txt_term term;
    int have_term;
    char* out; /* flush buffer */
    size_t out_cap;
    char* key_text; /* --keys file contents, NUL terminated */
    size_t key_at;
    int use_keys_file;
    uint32_t wait_left; /* ms of a `wait` line still to burn */
    uint32_t turn_mark; /* host clock at the previous keys-file step */
    const char* dump_path;
    const char* frame_dump_path; /* --frame-dump: raw frame bytes appended (row I218d2) */
    int dirty;
    int info;
    int page;           /* 0 none, 1 Now Playing, 2 Song Information (I1191) */
    char si_note[TUI_NOTE_BYTES]; /* the Song Information note field */
    uint32_t si_actions, np_draws; /* I1191 sentinels */
    int splash_wanted;  /* no --exec was given: the start screen may show (row I355) */
    int splash_off;     /* a key or a song ended the start screen for good */
    int splash_shown;   /* the frame being drawn is the start screen */
    uint32_t splash_frames; /* flushes that carried the start screen: STAT_SPLASH_FRAMES */
    uint32_t meta_panel_rows; /* catalogue rows the info panel drew in the latest frame (I185 sentinel) */
    int viz;     /* viz panel: 0 off, 1 levels, 2 scope, 3 spectrum, 4 note dots, 5 phase, 6 SID registers (V cycles) */
    uint32_t layout; /* I1185: SEL_UIM_LAYOUT_* (pref ui.desktop.mode): full, window (list only), mini (header only), visual (panel only) */
    uint32_t style_drawn, style_draws; /* I1186 sentinels: style of the last pane draw; draws in Grid, Cover Flow, Compact */
    uint32_t layout_sets, sid_frames; /* I1185 sentinels: layout / visualizer pref keys taken; SID register panels drawn with a feed */
    int pattern; /* pattern panel shown (P toggles); excludes the viz panel */
    uint32_t voice; /* 1-based voice the digit keys touched last; S solos it */
    uint32_t pat_order, pat_row, pat_qid, pat_asked; /* what the last pattern request was for */
    int quit;
    uint32_t key_count;
    sel_tag stats[33];
    sel_uim uim;        /* I1222g: the shared frame the player bar is drawn from */
    uint32_t pv_x0;     /* I1222h: grid column of the main region; page columns are relative to it */
    uint32_t pv_y0;     /* I1222g: grid row of the main region's first row; page row 2 maps there (pages keep their old row numbers until I1222i) */
    uint32_t pv_rows;   /* I1222g: rows of the main region; page rows 2 .. pv_rows + 2 are drawn, the rest clipped */
    uint32_t player_draws, player_rows, topbar_draws, sidebar_draws;
    uint32_t frames;    /* flushes that wrote bytes */
    uint32_t hash;      /* FNV-1a over every flushed byte */
    int headless;       /* --size: fixed grid, keys from stdin, no terminal control */
    uint32_t viz_frames, scope_reads, pattern_rows; /* T39 sentinels */
    uint32_t dot_rows, phase_points; /* I104d sentinels */
    uint32_t cmdline_runs, palette_opens; /* T28 sentinels */
    uint32_t lib_opens, lib_searches;      /* T98 sentinels */
    uint32_t folder_opens;                 /* T83 sentinel: openings of the playlist folder tree */
    uint32_t table_actions;                /* I1184 sentinel: keys that fired a line of the shared action table */
    char table_log[192];                   /* I1184: those lines in order, '|' separated (the tail is kept) */
    int lib_screen;                        /* the query view is the Library screen */
    uint32_t lib_mode;                     /* I1187: the Library mode (sel_uim_libmode_name) the last mode key or Library opening set */
    int lib_mode_on;                       /* I1187: lib_mode is in force (cleared when Browse, Folders or Home take the pane) */
    uint32_t mode_keys, scope_keys;        /* I1187 sentinels: library mode keys and queue scope keys taken */
    uint32_t set_focus, set_opens, set_sets; /* I1188: the Settings row under the cursor; openings; PREF_SET lines its keys sent */
    int set_edit;                          /* I1188: a string row is being typed into set_buf */
    char set_buf[96];
    sel_route_hist rh;                     /* I1189: the page history (route.h) */
    int pg;                                /* I1189: page on screen, 0 none, 1 Shelf, 2 Collection */
    uint32_t pg_shelf, pg_cur, pg_serial, pg_kind, pg_opens_shelf, pg_opens_coll, pg_rows;
    char pg_key[128], pg_title[96];
    /* I1306: the song titles of the open Collection page, from ONE posted song_briefs read (the shared model, I1306b). */
    sel_uim_briefs brf;
    /* I1190: the song and playlist menu (overlay 4), its name prompt (cmd_mode 3) and confirm prompt (cmd_mode 4) */
    uint32_t mn_n, mn_cur, mn_opens, mn_picks, mn_answers, mn_pending;
    uint32_t mn_act[TUI_MENU_MAX], mn_pl[TUI_MENU_MAX]; /* item action; MN_PLPICK: playlist id (0 makes a new one) */
    char mn_lab[TUI_MENU_MAX][64];
    uint32_t mn_id, mn_folder, mn_parent, mn_qid; /* playlist or folder under the cursor; folder to create in; queue entry id */
    char mn_title[96], mn_key[256], mn_log[192];
    int lib_empty_query;                   /* the last library query was empty (all songs) */
} tui;

/* ---- commands ---- */

/* Sends one command line. The pool is decode scratch only: the engine copies string arguments on send (I1082b). */
static void tui_send(tui* u, const char* line) {
    char pool[TUI_POOL_BYTES];
    sel_engine_send_line(u->engine, line, pool, sizeof pool);
}

static void tui_sendf(tui* u, const char* fmt, const char* a) {
    char line[512];
    snprintf(line, sizeof line, fmt, a);
    tui_send(u, line);
}

/* I1222g: page drawing through a row offset. Page row 2 is the main region's first row; rows above it or past its last row are clipped.
 * No pointer is shifted: the offset is added to the row index only. */
static int tui_prow(const tui* u, uint32_t row, uint32_t* grow) {
    if (row < 2u || row - 2u > u->pv_rows) {
        return 0;
    }
    *grow = u->pv_y0 + (row - 2u);
    return 1;
}

static void tui_pput(tui* u, uint32_t col, uint32_t row, uint32_t cp, txt_style style) {
    uint32_t g;
    if (tui_prow(u, row, &g)) {
        txt_grid_put(&u->grid, u->pv_x0 + col, g, cp, style);
    }
}

static uint32_t tui_ptext(tui* u, uint32_t col, uint32_t row, const char* utf8, uint32_t maxcols, txt_style style) {
    uint32_t g;
    return tui_prow(u, row, &g) ? txt_grid_text(&u->grid, u->pv_x0 + col, g, utf8, maxcols, style) : 0u;
}

static void tui_pfill(tui* u, uint32_t col, uint32_t row, uint32_t w, uint32_t h, uint32_t cp, txt_style style) {
    uint32_t i, g;
    for (i = 0; i < h; i++) {
        if (tui_prow(u, row + i, &g)) {
            txt_grid_fill(&u->grid, u->pv_x0 + col, g, w, 1u, cp, style);
        }
    }
}

/* I1222g: the screen's regions come from the shared text layout (sel_uim_layout_compute_text), never from numbers here. */
static const sel_uim_region* tui_region(const sel_uim_layout* l, const char* name) {
    uint32_t i;
    for (i = 0; i < l->count; i++) {
        if (strcmp(l->regions[i].name, name) == 0) {
            return &l->regions[i];
        }
    }
    return NULL;
}

static void tui_layout(const tui* u, sel_uim_layout* l) {
    (void)sel_uim_layout_compute_text(l, u->grid.cols, u->grid.rows, 0, 1);
}

/* rows the pages can list: the main region minus its title row (its last row is the command line's when that is open) */
static uint32_t tui_main_rows(const tui* u) {
    sel_uim_layout l;
    const sel_uim_region* m;
    tui_layout(u, &l);
    m = tui_region(&l, "main");
    return m != NULL && m->h > 1u ? m->h - 1u : 1u;
}

static void tui_send_window(tui* u) {
    char line[48];
    snprintf(line, sizeof line, "VIEW_WINDOW %u", (unsigned)tui_main_rows(u));
    tui_send(u, line);
}

/* Regrows the grid and the flush buffer to cols x rows, tells the engine the new list height and
 * forces a full redraw. Returns 0, or -1 when memory is out. */
static int tui_resize(tui* u, uint32_t cols, uint32_t rows) {
    char* bigger;
    if (txt_grid_resize(&u->grid, cols, rows) != 0) {
        return -1;
    }
    bigger = (char*)u->host->mem_alloc(u->host, txt_grid_flush_bound(&u->grid), SEL_MEM_FAST);
    if (bigger == NULL) {
        return -1;
    }
    u->host->mem_free(u->host, u->out);
    u->out = bigger;
    u->out_cap = txt_grid_flush_bound(&u->grid);
    tui_send_window(u);
    u->dirty = 1;
    return 0;
}

/* ---- keys ---- */

static void tui_cursor(tui* u, const char* where) {
    tui_sendf(u, "VIEW_CURSOR %s", where);
}

static void tui_add_cursor_file(tui* u) {
    const sel_view_state* v = sel_engine_state(u->engine)->view;
    const char* key;
    /* A tree or drilldown node row keeps its song's id as cursor_id; it adds nothing. */
    if (v == NULL || (v->source != SEL_VIEW_QUERY && v->source != SEL_VIEW_BROWSE) ||
        (v->source == SEL_VIEW_QUERY && (v->cursor < v->window_start || v->cursor - v->window_start >= v->row_n ||
                                         v->rows[v->cursor - v->window_start].kind == SEL_ROW_NODE)) ||
        (key = sel_engine_view_cursor_key(u->engine)) == NULL) {
        return;
    }
    tui_sendf(u, "QUEUE_ADD %s", key);
}

static void tui_remove_cursor_entry(tui* u) {
    const sel_view_state* v = sel_engine_state(u->engine)->view;
    char line[48];
    if (v != NULL && v->source == SEL_VIEW_QUEUE && v->row_count > 0) {
        snprintf(line, sizeof line, "QUEUE_REMOVE %u", (unsigned)v->cursor_id);
        tui_send(u, line);
    }
}

static int tui_queue_view(const sel_view_state* v) {
    return v != NULL && v->source != SEL_VIEW_BROWSE; /* the queue and the library query share the styles */
}

/* y: regular -> tree -> drilldown -> grid -> cover flow -> compact -> regular. */
static void tui_next_style(tui* u) {
    const sel_view_state* v = sel_engine_state(u->engine)->view;
    static const char* const next[SEL_VIEW_STYLE_COUNT] = {"VIEW_STYLE tree",      "VIEW_STYLE drilldown", "VIEW_STYLE grid",
                                                            "VIEW_STYLE coverflow", "VIEW_STYLE compact",   "VIEW_STYLE regular"};
    if (tui_queue_view(v) && v->style < SEL_VIEW_STYLE_COUNT) {
        tui_send(u, next[v->style]);
    }
}

/* g: group the queue by folder, or by format when it is by folder. */
static void tui_next_group(tui* u) {
    const sel_view_state* v = sel_engine_state(u->engine)->view;
    if (tui_queue_view(v)) {
        tui_send(u, v->group != NULL && strcmp(v->group, "folder") == 0 ? "VIEW_GROUP format" : "VIEW_GROUP folder");
    }
}

/* Backspace in a drilldown list steps back one picked value. */
static int tui_drill_back(tui* u, const sel_view_state* v) {
    char line[48];
    if (!tui_queue_view(v) || v->style != SEL_VIEW_STYLE_DRILLDOWN || v->drill_n == 0) {
        return 0;
    }
    snprintf(line, sizeof line, "VIEW_DRILL %u ..", (unsigned)(v->drill_n - 1u));
    tui_send(u, line);
    return 1;
}

static void tui_subsong(tui* u, int delta) {
    const sel_state* s = sel_engine_state(u->engine);
    char line[48];
    int32_t to = (int32_t)s->song.subsong + delta;
    if (s->song.path == NULL || to < 0 || (uint32_t)to >= s->song.subsong_count) {
        return;
    }
    snprintf(line, sizeof line, "SUBSONG %d", (int)to);
    tui_send(u, line);
}

/* The path of the browse file row under the cursor, or 0. */
static int tui_cursor_path(const tui* u, char* out, size_t cap) {
    const sel_view_state* v = sel_engine_state(u->engine)->view;
    const char* path;
    if (v == NULL || v->source != SEL_VIEW_BROWSE || (path = sel_engine_view_cursor_key(u->engine)) == NULL) {
        return 0;
    }
    return snprintf(out, cap, "%s", path) < (int)cap;
}

/* `m`: toggles the file under the cursor in the selection that `:!` uses. */
static void tui_mark(tui* u) {
    char path[TUI_MARK_BYTES];
    uint32_t i;
    if (!tui_cursor_path(u, path, sizeof path)) {
        return;
    }
    for (i = 0; i < u->mark_n; i++) {
        if (strcmp(u->marks[i], path) == 0) {
            memmove(u->marks[i], u->marks[i + 1u], (u->mark_n - i - 1u) * sizeof u->marks[0]);
            u->mark_n--;
            return;
        }
    }
    if (u->mark_n < TUI_MARKS) {
        strcpy(u->marks[u->mark_n++], path);
    }
}

/* `:!program args`: runs the program with the selected files (else the file
 * under the cursor) appended. The terminal is given back while it runs. */
static void tui_run_program(tui* u, const char* line) {
    const char* paths[TUI_MARKS];
    char cur[TUI_MARK_BYTES];
    uint32_t n = 0, i;
    if (u->mark_n > 0) {
        for (i = 0; i < u->mark_n; i++) {
            paths[n++] = u->marks[i];
        }
    } else if (tui_cursor_path(u, cur, sizeof cur)) {
        paths[n++] = cur;
    }
    if (u->have_term) {
        txt_term_close(&u->term);
    }
    i = (uint32_t)txt_spawn(line, paths, n) == 0 ? 0u : 1u;
    if (u->have_term) {
        (void)txt_term_open(&u->term, u->term.in_fd, u->term.out_fd);
        txt_grid_invalidate(&u->grid);
    }
    if (i != 0) {
        snprintf(u->cmd.status, sizeof u->cmd.status, "Program failed: %s", line);
    }
}

/* Library screen (design doc 9b, 9c): the query view over LIBRARY_SEARCH. The first
 * opening lists all songs (the empty query) as a tree grouped by format. */
static void tui_library_search(tui* u, const char* query) {
    char line[TXT_CMD_LINE * 2 + 32];
    size_t n = (size_t)snprintf(line, sizeof line, "LIBRARY_SEARCH \"");
    const char* p;
    for (p = query; *p != '\0' && n + 4u < sizeof line; p++) {
        if (*p == '"' || *p == '\\') {
            line[n++] = '\\';
        }
        line[n++] = *p;
    }
    line[n++] = '"';
    line[n] = '\0';
    tui_send(u, line);
    tui_send(u, "VIEW_SOURCE query");
    if (!u->lib_screen) {
        u->lib_opens++;
        tui_send(u, "VIEW_STYLE tree");
        tui_send(u, "VIEW_GROUP format");
    }
    u->lib_screen = 1;
    u->lib_searches++;
    if (!u->lib_mode_on) { /* the Library opens as Tree, grouped by format */
        u->lib_mode = 1u;
        u->lib_mode_on = 1;
    }
    u->lib_empty_query = query[0] == '\0';
}

static void tui_library_open(tui* u) {
    const sel_view_state* v = sel_engine_state(u->engine)->view;
    if (!u->lib_screen || v == NULL || v->source != SEL_VIEW_QUERY) {
        u->lib_screen = 0;
        u->lib_mode_on = 0;
        tui_library_search(u, "");
    }
}

/* I1187: the Library modes the text UI cycles through (sel_uim_libmode_lines, the lines the desktop sends too): Composers, Tree,
 * Columns, Files, History. Sources is exempt: the desktop retired it (I678i, nothing sets it) and it lists no view rows. */
static void tui_next_lib_mode(tui* u) {
    char lines[SEL_UIM_LIB_LINES][SEL_UIM_LIB_LINE_MAX];
    uint32_t i, n, mode = u->lib_mode_on ? (u->lib_mode + 1u) % SEL_UIM_LIB_SOURCES : 1u;
    n = sel_uim_libmode_lines(mode, tui_main_rows(u), lines);
    for (i = 0; i < n; i++) {
        tui_send(u, lines[i]);
    }
    if (mode == 1u || mode == 2u) { /* the query modes are the Library screen */
        u->lib_opens += u->lib_screen ? 0u : 1u;
        u->lib_screen = 1;
        u->lib_searches++;
        u->lib_empty_query = 1;
    } else {
        u->lib_screen = 0;
    }
    u->lib_mode = mode;
    u->lib_mode_on = 1;
    u->mode_keys++;
}

/* I1187: the queue scope Library, Album, Playlist (SCOPE <name>, as the desktop Scope button sends). */
static void tui_next_scope(tui* u, const sel_state* s) {
    char line[32];
    (void)sel_uim_scope_line((sel_uim_scope_index(s->options[SEL_OPT_SCOPE]) + 1u) % SEL_UIM_QUEUE_SCOPES, line, sizeof line);
    tui_send(u, line);
    u->scope_keys++;
}

/* ---- I1188: the Settings screen. The rows are the shared table (sel_uim_pref_row_at, the filter the desktop Settings page and
 * the MUI window use, hidden for the other machine); a value is read with the engine pref the way desk_pref does, a change
 * is the PREF_SET line sel_uim_pref_line builds. Exempt: the UI Zoom of the desktop (a pixel scale, a character grid has
 * none) and the C64 ROM file chooser (a file dialog; the screen shows which ROM is in force, Reset to Bundled is the Delete key). ---- */

static const char* tui_pref(const tui* u, const char* key) {
    const sel_host* h = sel_engine_plugin_host(u->engine);
    const char* v = h != NULL && h->config_get != NULL ? h->config_get(h, key, "") : "";
    return v != NULL ? v : "";
}

static void tui_pref_send(tui* u, const sel_uim_pref_row* r, const char* val) {
    char line[300];
    sel_uim_pref_line(r->key, val, line, sizeof line);
    tui_send(u, line);
    u->set_sets++;
}

/* Left (dir -1) and Right, Enter, Space (dir +1) on the row under the cursor. */
static void tui_settings_step(tui* u, const sel_uim_pref_row* r, int dir) {
    const char* cur = tui_pref(u, r->key);
    char val[48];
    if (r->ui == (uint32_t)SEL_UIM_PREF_CYCLE) {
        uint32_t n = sel_uim_pref_choice(r->choices, 0u, val, sizeof val);
        uint32_t at = sel_uim_pref_choice_index(r->choices, cur);
        at = dir > 0 ? (at + 1u) % n : (at + n - 1u) % n;
        sel_uim_pref_choice(r->choices, at, val, sizeof val);
        tui_pref_send(u, r, val);
    } else if (r->ui == (uint32_t)SEL_UIM_PREF_CHECK) {
        tui_pref_send(u, r, cur[0] == '1' ? "0" : "1");
    } else if (r->ui == (uint32_t)SEL_UIM_PREF_SLIDER) {
        long lo = 0, hi = 100, v = cur[0] != '\0' ? strtol(cur, NULL, 10) : 0;
        sscanf(r->choices, "%ld,%ld", &lo, &hi);
        v = v < lo ? lo : (v > hi ? hi : v);
        v += dir * 5;
        v = v < lo ? lo : (v > hi ? hi : v);
        snprintf(val, sizeof val, "%ld", v);
        tui_pref_send(u, r, val);
    } else if (r->ui == (uint32_t)SEL_UIM_PREF_STRING && dir > 0) {
        snprintf(u->set_buf, sizeof u->set_buf, "%s", cur);
        u->set_edit = 1;
    }
}

static void tui_settings_open(tui* u) {
    u->overlay = 3;
    u->set_edit = 0;
    u->set_opens++;
    if (u->set_focus >= sel_uim_pref_row_count()) {
        u->set_focus = 0;
    }
}

static void tui_settings_key(tui* u, const txt_key* k) {
    uint32_t n = sel_uim_pref_row_count();
    const sel_uim_pref_row* r = sel_uim_pref_row_at(u->set_focus);
    size_t len = strlen(u->set_buf);
    if (u->set_edit) {
        if (k->code == 13u && r != NULL) {
            tui_pref_send(u, r, u->set_buf);
            u->set_edit = 0;
        } else if (k->code == 27u) {
            u->set_edit = 0;
        } else if (k->code == 127u && len > 0) {
            u->set_buf[len - 1u] = '\0';
        } else if (k->mods == 0 && k->code >= 0x20u && k->code < 0x7Fu && len + 1u < sizeof u->set_buf) {
            u->set_buf[len] = (char)k->code;
            u->set_buf[len + 1u] = '\0';
        }
        return;
    }
    if (k->code == 27u || k->code == 'q' || k->code == 'S') {
        u->overlay = 0;
    } else if ((k->code == TXT_KEY_UP || k->code == 'k') && u->set_focus > 0) {
        u->set_focus--;
    } else if ((k->code == TXT_KEY_DOWN || k->code == 'j') && u->set_focus + 1u < n) {
        u->set_focus++;
    } else if (k->code == TXT_KEY_HOME) {
        u->set_focus = 0;
    } else if (k->code == TXT_KEY_END && n > 0) {
        u->set_focus = n - 1u;
    } else if (r != NULL && (k->code == TXT_KEY_LEFT || k->code == 'h')) {
        tui_settings_step(u, r, -1);
    } else if (r != NULL && (k->code == TXT_KEY_RIGHT || k->code == 'l' || k->code == 13u || k->code == ' ')) {
        tui_settings_step(u, r, 1);
    } else if (r != NULL && r->ui == (uint32_t)SEL_UIM_PREF_ROM && k->code == TXT_KEY_DELETE) {
        int slot = sel_uim_rom_slot(r->choices);
        if (slot >= 0) {
            (void)sel_uim_rom_reset(tui_pref(u, "config.dir"), (uint32_t)slot);
        }
    }
}

/* UI-local actions (`@name` in the binding table). Returns 0 for an unknown name. */
static txt_style tui_style(const tui* u, txt_role r);

/* ---- Shelf and Collection pages (I1189) ----
 * The same pages the desktop opens (uimodel/pages.h, route.h): the Show all page of a Home shelf and the album, artist, composer
 * and playlist pages. The route table says which click opens which page and the history walks Back; the Home shelf split
 * (sel_uim_home_shelves) and the tile route (sel_uim_tile_route) are the shared ones. The engine builds a Collection page:
 * COLLECTION_OPEN fills state.collection, which the page lists. */

enum { TUI_PAGE_NONE = 0, TUI_PAGE_SHELF = 1, TUI_PAGE_COLLECTION = 2, TUI_SHELF_MAX = 8 };

static const char* const k_coll_shelf[SEL_COLLECTION_SHELVES] = {"Albums", "Appears On", "More by", "Spellings"};

/* Builds `COLLECTION_OPEN <kind> "<key>"[ 1]` ; " 1" rebuilds without touching Recents (Back and Forward). */
static void tui_collection_open(tui* u, uint32_t kind, const char* key, int restore) {
    char line[320];
    size_t o = (size_t)snprintf(line, sizeof line, "COLLECTION_OPEN %u \"", (unsigned)kind);
    const sel_collection_page* pg = sel_engine_state(u->engine)->collection;
    for (const char* c = key; *c != '\0' && o + 8u < sizeof line; c++) {
        if (*c == '"' || *c == '\\') {
            line[o++] = '\\';
        }
        line[o++] = *c;
    }
    line[o++] = '"';
    line[o] = '\0';
    if (restore) {
        snprintf(line + o, sizeof line - o, " 1");
    }
    u->pg_serial = pg != NULL ? pg->serial : 0u;
    tui_send(u, line);
}

/* Shows the page entry `e`: the history decided it, this fills it. */
static void tui_page_enter(tui* u, const sel_route_entry* e, int restore) {
    const sel_view_state* v = sel_engine_state(u->engine)->view;
    u->pg_cur = 0;
    if (e->page == (uint32_t)SEL_ROUTE_PAGE_SHELF) {
        u->pg = TUI_PAGE_SHELF;
        u->pg_shelf = e->id > 0u ? e->id - 1u : 0u;
        u->pg_opens_shelf++;
        if (v == NULL || v->source != SEL_VIEW_HOME) {
            tui_send(u, "VIEW_SOURCE home");
        }
    } else if (e->page == (uint32_t)SEL_ROUTE_PAGE_COLLECTION) {
        u->pg = TUI_PAGE_COLLECTION;
        u->pg_kind = e->kind;
        snprintf(u->pg_key, sizeof u->pg_key, "%s", e->key);
        snprintf(u->pg_title, sizeof u->pg_title, "%s", e->name);
        u->pg_opens_coll++;
        tui_collection_open(u, e->kind, e->key, restore);
    } else {
        u->pg = TUI_PAGE_NONE;
    }
}

/* One click: the route table resolves it, the history takes the entry, the page opens. Returns 1 when a page opened. */
static int tui_route(tui* u, sel_route_target target, uint32_t kind, uint32_t id, const char* key, const char* name, const char* sub) {
    sel_route_entry e;
    if (u->rh.n == 0u) { /* the page the screen is on when the first click comes */
        sel_route_entry home;
        memset(&home, 0, sizeof home);
        home.page = (uint32_t)SEL_ROUTE_PAGE_HOME;
        sel_route_hist_seed(&u->rh, &home);
    }
    if (!sel_route_go(&u->rh, target, kind, id, key, name, sub, &e)) {
        return 0;
    }
    sel_route_hist_scroll(&u->rh, (int32_t)u->pg_cur);
    sel_route_hist_push(&u->rh, &e);
    tui_page_enter(u, &e, 0);
    return 1;
}

/* Back: the page before in the history; the Home entry shows the plain view again. Returns 1 when a page was left. */
static int tui_page_back(tui* u) {
    const sel_route_entry* w;
    sel_route_entry e;
    if (u->pg == TUI_PAGE_NONE) {
        return 0;
    }
    if (!sel_route_hist_can(&u->rh, -1)) {
        u->pg = TUI_PAGE_NONE;
        return 1;
    }
    w = sel_route_hist_walk(&u->rh, -1);
    e = *w;
    tui_page_enter(u, &e, 1);
    u->pg_cur = (uint32_t)(e.scroll > 0 ? e.scroll : 0);
    return 1;
}

/* The Home shelf `u->pg_shelf` of the view; 0 when the shelf is gone. */
static int tui_page_shelf(const tui* u, const sel_view_state* v, sel_uim_shelf* out) {
    sel_uim_shelf sh[TUI_SHELF_MAX];
    uint32_t n = sel_uim_home_shelves(v, sh, TUI_SHELF_MAX);
    if (u->pg_shelf >= n) {
        return 0;
    }
    *out = sh[u->pg_shelf];
    return 1;
}

/* Line `i` of a Collection page (the shelf rows in shelf order, then the songs); 0 past the end. `tile` is set when Enter on the line
 * opens another page, `song` points at the song key when it is a song line. */
static int tui_coll_line(const sel_collection_page* pg, const sel_uim_briefs* briefs, uint32_t i, char* buf, size_t cap,
                         sel_uim_tile* tile, const char** song) {
    uint32_t s, k;
    const char* r;
    *song = NULL;
    memset(tile, 0, sizeof *tile);
    for (s = 0; s < SEL_COLLECTION_SHELVES; s++) {
        if (i < pg->shelf_n[s]) {
            const sel_engine_collection_row* row = &pg->shelf[s][i];
            if (s == 3u) { /* Spellings: a composer spelling, not a page */
                snprintf(buf, cap, "%s: %s  %u songs", k_coll_shelf[s], row->name, (unsigned)row->songs);
            } else {
                snprintf(buf, cap, "%s: %s  %s %u  %u songs", k_coll_shelf[s], row->name, row->artist, (unsigned)row->year, (unsigned)row->songs);
                tile->kind = (uint32_t)SEL_LIB_ITEM_ALBUM;
                tile->id = row->id;
                snprintf(tile->key, sizeof tile->key, "%u", (unsigned)row->id);
                snprintf(tile->name, sizeof tile->name, "%s", row->name);
                snprintf(tile->sub, sizeof tile->sub, "%s", row->artist);
            }
            return 1;
        }
        i -= pg->shelf_n[s];
    }
    r = pg->rows;
    for (k = 0; r != NULL && k < pg->songs && *r != '\0'; k++) {
        const char* plays = r + strlen(r) + 1u;
        if (k == i) {
            const char* base = strrchr(r, '/');
            const char* title = sel_uim_briefs_title(briefs, pg, k); /* I1306: the catalogued title, else the file name */
            const char* name = title != NULL ? title : base != NULL ? base + 1 : r;
            snprintf(buf, cap, "%s  %s plays", name, plays);
            *song = r;
            return 1;
        }
        r = plays + strlen(plays) + 1u;
    }
    return 0;
}

/* I1306: Collection page song rows show the catalogued title, like the desktop page (I948a), through the shared briefs model (I1306b). */
static void tui_briefs_follow(tui* u) {
    const sel_collection_page* pg = sel_engine_state(u->engine)->collection;
    if (sel_uim_briefs_take(&u->brf, u->engine)) {
        u->dirty = 1;
    }
    if (u->pg != 2 || pg == NULL || pg->serial == u->pg_serial || !sel_uim_briefs_due(&u->brf, u->engine, pg)) {
        return;
    }
    if (u->brf.buf != NULL) {
        u->host->mem_free(u->host, u->brf.buf);
    }
    u->brf.cap = sel_uim_briefs_cap(pg);
    u->brf.buf = (char*)u->host->mem_alloc(u->host, u->brf.cap, SEL_MEM_FAST);
    (void)sel_uim_briefs_post(&u->brf, u->engine, pg);
}

static uint32_t tui_page_count(const tui* u, const sel_state* s) {
    uint32_t n = 0, i;
    if (u->pg == TUI_PAGE_SHELF) {
        sel_uim_shelf sh;
        return s->view != NULL && tui_page_shelf(u, s->view, &sh) ? sh.rows : 0u;
    }
    if (s->collection != NULL) {
        for (i = 0; i < SEL_COLLECTION_SHELVES; i++) {
            n += s->collection->shelf_n[i];
        }
        n += s->collection->songs;
    }
    return n;
}

/* Enter on the page line under the cursor. */
static void tui_page_activate(tui* u) {
    const sel_state* s = sel_engine_state(u->engine);
    sel_uim_tile tile;
    char buf[320];
    const char* song;
    if (u->pg == TUI_PAGE_SHELF) {
        sel_uim_shelf sh;
        if (s->view == NULL || !tui_page_shelf(u, s->view, &sh) || u->pg_cur >= sh.rows) {
            return;
        }
        if (sel_uim_tile_route(&s->view->rows[sh.first + u->pg_cur], &tile)) {
            (void)tui_route(u, SEL_ROUTE_HOME_TILE, tile.kind, tile.id, tile.key, tile.name, tile.sub);
        } else { /* a song tile plays */
            snprintf(buf, sizeof buf, "VIEW_CURSOR %u", (unsigned)(s->view->window_start + sh.first + u->pg_cur));
            tui_send(u, buf);
            tui_send(u, "VIEW_ACTIVATE");
        }
    } else if (s->collection != NULL && tui_coll_line(s->collection, &u->brf, u->pg_cur, buf, sizeof buf, &tile, &song)) {
        if (song != NULL) {
            snprintf(buf, sizeof buf, "QUEUE_ADD %s", song);
            tui_send(u, buf);
        } else if (tile.kind != 0u) {
            (void)tui_route(u, SEL_ROUTE_ALBUM_CARD, tile.kind, tile.id, tile.key, tile.name, tile.sub);
        }
    }
}

/* Keys of an open page. Returns 1 when the page took the key. */
static int tui_page_key(tui* u, const txt_key* k) {
    uint32_t n = tui_page_count(u, sel_engine_state(u->engine));
    if (k->mods != 0) {
        return 0;
    }
    if (k->code == TXT_KEY_UP || k->code == 'k') {
        u->pg_cur -= u->pg_cur > 0u;
    } else if (k->code == TXT_KEY_DOWN || k->code == 'j') {
        u->pg_cur += u->pg_cur + 1u < n;
    } else if (k->code == 13u) {
        tui_page_activate(u);
    } else if (k->code == 27u || k->code == 127u) {
        (void)tui_page_back(u);
    } else {
        return 0;
    }
    return 1;
}

/* e and r: the artist or album under the cursor (or the playing song's artist) opens its Collection page. */
static void tui_open_field(tui* u, int album) {
    const sel_state* s = sel_engine_state(u->engine);
    const sel_view_state* v = s->view;
    const sel_view_row* r = NULL;
    const char* author = (const char*)sel_tag_find(s->song.meta, SEL_META_AUTHOR, 0);
    if (v != NULL && u->pg == TUI_PAGE_SHELF) {
        sel_uim_shelf sh;
        if (tui_page_shelf(u, v, &sh) && u->pg_cur < sh.rows) {
            r = &v->rows[sh.first + u->pg_cur];
        }
    } else if (v != NULL && u->pg == TUI_PAGE_NONE && v->cursor >= v->window_start && v->cursor - v->window_start < v->row_n) {
        r = &v->rows[v->cursor - v->window_start];
    }
    if (album) {
        if (r != NULL && r->art_id != 0u) {
            char key[16];
            snprintf(key, sizeof key, "%u", (unsigned)r->art_id);
            (void)tui_route(u, SEL_ROUTE_TRACK_ALBUM, (uint32_t)SEL_LIB_ITEM_ALBUM, r->art_id, key, r->key != NULL ? r->key : r->text,
                            r->artist != NULL ? r->artist : "");
        } else {
            snprintf(u->cmd.status, sizeof u->cmd.status, "No album under the cursor");
        }
        return;
    }
    if (r != NULL && r->artist != NULL && r->artist[0] != '\0') {
        author = r->artist;
    }
    if (author != NULL && author[0] != '\0' && (uintptr_t)author != (uintptr_t)-1) {
        (void)tui_route(u, SEL_ROUTE_TRACK_ARTIST, (uint32_t)SEL_LIB_ITEM_ARTIST, 0u, author, author, "");
    } else {
        snprintf(u->cmd.status, sizeof u->cmd.status, "No artist under the cursor");
    }
}

/* Enter on a Home tile that opens a page (composer, album, item card) opens it; 0 for a song tile. */
static int tui_home_tile_enter(tui* u) {
    const sel_view_state* v = sel_engine_state(u->engine)->view;
    sel_uim_tile tile;
    if (v == NULL || v->source != SEL_VIEW_HOME || v->cursor < v->window_start || v->cursor - v->window_start >= v->row_n) {
        return 0;
    }
    if (!sel_uim_tile_route(&v->rows[v->cursor - v->window_start], &tile)) {
        return 0;
    }
    return tui_route(u, SEL_ROUTE_HOME_TILE, tile.kind, tile.id, tile.key, tile.name, tile.sub);
}

/* Enter with the engine cursor on the heading of a Home shelf opens its Show all page (the desktop's shelf_header_enter). */
static int tui_shelf_header_enter(tui* u) {
    const sel_view_state* v = sel_engine_state(u->engine)->view;
    sel_uim_shelf sh[TUI_SHELF_MAX];
    uint32_t s, n, at;
    if (v == NULL || v->source != SEL_VIEW_HOME || v->cursor < v->window_start) {
        return 0;
    }
    at = v->cursor - v->window_start;
    n = sel_uim_home_shelves(v, sh, TUI_SHELF_MAX);
    for (s = 0; s < n; s++) {
        if (sh[s].head == at && sh[s].tiles > 0u) {
            return tui_route(u, SEL_ROUTE_SHOW_ALL, 0u, s + 1u, "", "", "");
        }
    }
    return 0;
}

static void tui_page_draw(tui* u, const sel_state* s, uint32_t width) {
    char buf[320];
    uint32_t i, n, drawn = 0, rows = tui_main_rows(u);
    txt_style label = tui_style(u, TXT_ROLE_LABEL);
    sel_uim_tile tile;
    const char* song;
    if (u->pg == TUI_PAGE_SHELF) {
        sel_uim_shelf sh;
        if (s->view == NULL || !tui_page_shelf(u, s->view, &sh)) {
            tui_ptext(u, 1, 2, "Shelf", width - 1u, label);
            return;
        }
        snprintf(buf, sizeof buf, "%s  Show all  %u rows", s->view->rows[sh.head].text, (unsigned)sh.rows);
        tui_ptext(u, 1, 2, buf, width - 1u, label);
        for (i = 0; i < sh.rows && i < rows; i++) {
            const sel_view_row* r = &s->view->rows[sh.first + i];
            txt_style st = tui_style(u, i == u->pg_cur ? TXT_ROLE_CURSOR : TXT_ROLE_FILE);
            snprintf(buf, sizeof buf, " %s%s%s", r->text, r->artist != NULL && r->artist[0] ? "  - " : "", r->artist != NULL ? r->artist : "");
            tui_pfill(u, 0, 3u + i, width, 1, ' ', st);
            tui_ptext(u, 1, 3u + i, buf, width - 1u, st);
            drawn++;
        }
    } else {
        const sel_collection_page* pg = s->collection;
        int ready = pg != NULL && pg->serial != u->pg_serial && pg->kind == u->pg_kind;
        snprintf(buf, sizeof buf, "%s  %s", sel_route_kind_name(u->pg_kind), ready ? pg->title : u->pg_title);
        if (ready && pg->sub[0] != '\0') {
            size_t at = strlen(buf);
            snprintf(buf + at, sizeof buf - at, "  - %s", pg->sub);
        }
        tui_ptext(u, 1, 2, buf, width - 1u, label);
        n = ready ? tui_page_count(u, s) : 0u;
        for (i = 0; i < n && i < rows; i++) {
            txt_style st = tui_style(u, i == u->pg_cur ? TXT_ROLE_CURSOR : TXT_ROLE_FILE);
            if (!tui_coll_line(pg, &u->brf, i, buf + 1, sizeof buf - 1u, &tile, &song)) {
                break;
            }
            buf[0] = ' ';
            tui_pfill(u, 0, 3u + i, width, 1, ' ', st);
            tui_ptext(u, 1, 3u + i, buf, width - 1u, st);
            drawn++;
        }
        if (!ready) {
            tui_ptext(u, 1, 3, "Loading", width - 1u, tui_style(u, TXT_ROLE_DIM));
        }
    }
    u->pg_rows = drawn;
}

/* ---- the song and playlist menu (I1190) ---- */

static void tui_menu_add(tui* u, uint32_t act, const char* label, uint32_t pl) {
    if (u->mn_n < TUI_MENU_MAX) {
        u->mn_act[u->mn_n] = act;
        u->mn_pl[u->mn_n] = pl;
        snprintf(u->mn_lab[u->mn_n], sizeof u->mn_lab[0], "%s", label);
        u->mn_n++;
    }
}

/* Sends a line the menu chose and keeps it in the sentinel log. */
static void tui_menu_send(tui* u, const char* line) {
    size_t len = strlen(u->mn_log);
    if (len + strlen(line) + 2u >= sizeof u->mn_log) {
        u->mn_log[0] = '\0';
        len = 0;
    }
    snprintf(u->mn_log + len, sizeof u->mn_log - len, "%s%s", len > 0 ? "|" : "", line);
    tui_send(u, line);
}

/* Opens the menu for the row under the cursor: a song (queue, library, browse file) or, in the playlist tree, a playlist or
 * folder plus New Folder and New Playlist. Labels come from the shared uimodel tables the desktop menus use. */
static void tui_menu_open(tui* u) {
    const sel_view_state* v = sel_engine_state(u->engine)->view;
    const sel_view_row* row = NULL;
    const char* key;
    uint32_t k;
    if (v == NULL) {
        return;
    }
    if (v->cursor >= v->window_start && v->cursor - v->window_start < v->row_n) {
        row = &v->rows[v->cursor - v->window_start];
    }
    u->mn_n = 0;
    u->mn_cur = 0;
    u->mn_id = u->mn_folder = u->mn_parent = u->mn_qid = 0;
    u->mn_key[0] = '\0';
    if (v->source == SEL_VIEW_FOLDERS) {
        snprintf(u->mn_title, sizeof u->mn_title, "%s", row != NULL && (row->kind == (uint32_t)SEL_ROW_PLAYLIST || row->kind == (uint32_t)SEL_ROW_FOLDER) ? row->text : "Playlists");
        if (row != NULL && (row->kind == (uint32_t)SEL_ROW_PLAYLIST || row->kind == (uint32_t)SEL_ROW_FOLDER)) {
            u->mn_id = row->id;
            u->mn_folder = row->kind == (uint32_t)SEL_ROW_FOLDER;
            u->mn_parent = u->mn_folder ? row->id : 0u;
            for (k = 0; k < 2u; k++) {
                uint32_t act = sel_uim_plmenu_action(u->mn_folder, k);
                if (act != (uint32_t)SEL_UIM_PLACT_NONE) {
                    tui_menu_add(u, act == (uint32_t)SEL_UIM_PLACT_RENAME ? MN_RENAME : MN_DELETE, sel_uim_plmenu_label(act), 0);
                }
            }
        }
        tui_menu_add(u, MN_NEWFOLDER, sel_uim_plbtn_label(0), 0);
        tui_menu_add(u, MN_NEWPL, sel_uim_plbtn_label(1), 0);
    } else if (row != NULL && row->kind != (uint32_t)SEL_ROW_NODE && row->kind != (uint32_t)SEL_ROW_DIR &&
               (key = sel_engine_view_cursor_key(u->engine)) != NULL) {
        int on_queue = v->source == SEL_VIEW_QUEUE;
        snprintf(u->mn_key, sizeof u->mn_key, "%s", key);
        snprintf(u->mn_title, sizeof u->mn_title, "%s", row->text != NULL ? row->text : key);
        u->mn_qid = v->cursor_id;
        for (k = 0; k < (uint32_t)SEL_UIM_SONG_MENU_ITEMS; k++) {
            tui_menu_add(u, k, sel_uim_song_menu_label(k, on_queue), 0);
        }
        u->mn_pending = (uint32_t)on_queue;
    } else {
        snprintf(u->cmd.status, sizeof u->cmd.status, "No song or playlist under the cursor");
        return;
    }
    u->mn_opens++;
    u->overlay = 4;
}

/* The playlists a song can join: New Playlist, then the saved ones (the desktop Add to Playlist submenu, read synchronously). */
static void tui_menu_playlists(tui* u) {
    sel_engine_library_item items[TUI_MENU_MAX - 1u];
    uint32_t n = sel_engine_library_items(u->engine, SEL_LIB_ITEM_BIT(SEL_LIB_ITEM_PLAYLIST), 0, items, TUI_MENU_MAX - 1u, NULL), i;
    u->mn_n = 0;
    u->mn_cur = 0;
    tui_menu_add(u, MN_PLPICK, sel_uim_plbtn_label(1), 0);
    for (i = 0; i < n; i++) {
        tui_menu_add(u, MN_PLPICK, items[i].name, items[i].id);
    }
}

/* A prompt answered: the name for Rename, New Folder, New Playlist. */
static void tui_menu_name(tui* u, const char* name) {
    char line[256];
    u->mn_answers++;
    if (u->mn_pending == (uint32_t)MN_RENAME) {
        if (sel_uim_plrename_line(u->mn_id, name, line, sizeof line) > 0u) {
            tui_menu_send(u, line);
        }
    } else if (sel_uim_plbtn_line(u->mn_pending == (uint32_t)MN_NEWFOLDER ? 0u : 1u, name, u->mn_parent, line, sizeof line) > 0u) {
        tui_menu_send(u, line);
    }
    tui_send(u, "VIEW_SOURCE folders");
}

/* A confirm answered: y runs the delete / library removal, anything else cancels. */
static void tui_menu_confirm(tui* u, uint32_t code) {
    char line[320];
    u->mn_answers++;
    if (u->mn_pending == (uint32_t)MN_QUIT) { /* I1367: Enter (or y) quits, Escape or any other key keeps running */
        if (code == 13u || code == (uint32_t)'y' || code == (uint32_t)'Y') {
            u->quit = 1;
        }
        return;
    }
    if (code != (uint32_t)'y' && code != (uint32_t)'Y') {
        return;
    }
    if (u->mn_pending == (uint32_t)MN_DELETE) {
        sel_uim_pldel_line(u->mn_folder, u->mn_id, line, sizeof line);
        tui_menu_send(u, line);
        tui_send(u, "VIEW_SOURCE folders");
    } else {
        snprintf(line, sizeof line, "LIBRARY_REMOVE \"%s\"", u->mn_key);
        tui_menu_send(u, line);
    }
}

static void tui_menu_pick(tui* u) {
    uint32_t act = u->mn_act[u->mn_cur];
    char line[320];
    u->mn_picks++;
    if (act == (uint32_t)MN_PLPICK) {
        snprintf(line, sizeof line, "PLAYLIST_ADD_SONG %u \"%s\"", (unsigned)u->mn_pl[u->mn_cur], u->mn_key);
        tui_menu_send(u, line);
        u->overlay = 0;
        return;
    }
    u->overlay = 0;
    if (act == (uint32_t)MN_PLAY) {
        tui_menu_send(u, "VIEW_ACTIVATE");
    } else if (act == (uint32_t)MN_NEXT || act == (uint32_t)MN_QUEUE) {
        snprintf(line, sizeof line, "%s \"%s\"", act == (uint32_t)MN_NEXT ? "QUEUE_PLAY_NEXT" : "QUEUE_ADD", u->mn_key);
        tui_menu_send(u, line);
    } else if (act == (uint32_t)MN_ADDPL) {
        tui_menu_playlists(u);
        u->overlay = 4;
    } else if (act == (uint32_t)MN_REMOVE && u->mn_pending == 1u) { /* on the queue: the entry leaves the queue */
        snprintf(line, sizeof line, "QUEUE_REMOVE %u", (unsigned)u->mn_qid);
        tui_menu_send(u, line);
    } else if (act == (uint32_t)MN_REMOVE || act == (uint32_t)MN_DELETE) {
        u->mn_pending = act;
        u->cmd_mode = 4;
        if (act == (uint32_t)MN_REMOVE) {
            sel_uim_remove_text(u->mn_title, u->mn_lab[0], sizeof u->mn_lab[0]);
        } else {
            sel_uim_pldel_text(u->mn_folder, u->mn_title, u->mn_lab[0], sizeof u->mn_lab[0]);
        }
    } else { /* Rename, New Folder, New Playlist: ask for the name */
        u->mn_pending = act;
        txt_cmdline_open(&u->cmd);
        u->cmd_mode = 3;
    }
}

static void tui_menu_key(tui* u, const txt_key* k) {
    if (k->code == 27u || k->code == 127u) {
        u->overlay = 0;
    } else if (k->code == 13u) {
        tui_menu_pick(u);
    } else if (k->code == TXT_KEY_UP || k->code == (uint32_t)'k') {
        u->mn_cur = u->mn_cur > 0u ? u->mn_cur - 1u : u->mn_n - 1u;
    } else if (k->code == TXT_KEY_DOWN || k->code == (uint32_t)'j') {
        u->mn_cur = u->mn_cur + 1u < u->mn_n ? u->mn_cur + 1u : 0u;
    }
}

static void tui_menu_draw(tui* u) {
    uint32_t w = u->grid.cols > 44u ? 44u : u->grid.cols, x0 = (u->grid.cols - w) / 2u, i;
    txt_style n = tui_style(u, TXT_ROLE_NORMAL), d = tui_style(u, TXT_ROLE_DIM), sel = tui_style(u, TXT_ROLE_CURSOR);
    for (i = 0; i < u->mn_n + 2u && 1u + i < u->grid.rows; i++) {
        txt_grid_fill(&u->grid, x0, 1u + i, w, 1, ' ', n);
    }
    txt_grid_text(&u->grid, x0 + 1u, 1, u->mn_title, w - 2u, d);
    for (i = 0; i < u->mn_n; i++) {
        txt_style st = i == u->mn_cur ? sel : n;
        txt_grid_fill(&u->grid, x0, 2u + i, w, 1, ' ', st);
        txt_grid_text(&u->grid, x0 + 2u, 2u + i, u->mn_lab[i], w - 3u, st);
    }
}

static int tui_local(tui* u, const char* name, const txt_key* k) {
    const sel_state* s = sel_engine_state(u->engine);
    char line[48];
    (void)k;
    if (u->pg != TUI_PAGE_NONE && strstr("home library folders switch_view search settings", name) != NULL) {
        u->pg = TUI_PAGE_NONE; /* another screen takes the pane: the page history ends */
        sel_route_hist_init(&u->rh);
    }
    if (strcmp(name, "artist_page") == 0 || strcmp(name, "album_page") == 0) {
        tui_open_field(u, name[1] == 'l');
    } else if (strcmp(name, "page_up") == 0 || strcmp(name, "page_down") == 0) {
        snprintf(line, sizeof line, "%c%u", name[5] == 'u' ? '-' : '+', (unsigned)tui_main_rows(u));
        tui_cursor(u, line);
    } else if (strcmp(name, "next_style") == 0) {
        tui_next_style(u);
    } else if (strcmp(name, "next_group") == 0) {
        tui_next_group(u);
    } else if (strcmp(name, "back") == 0) { /* the parent directory is always the first browse row */
        if (tui_page_back(u) || tui_drill_back(u, s->view)) {
            return 1;
        }
        if (s->view != NULL && s->view->source == SEL_VIEW_BROWSE) {
            tui_cursor(u, "top");
            tui_send(u, "VIEW_ACTIVATE");
        }
    } else if (strcmp(name, "switch_view") == 0) {
        /* Browse, Queue, Library, Browse ... */
        uint32_t src = s->view != NULL ? s->view->source : SEL_VIEW_QUEUE;
        if (src == SEL_VIEW_BROWSE) {
            u->lib_screen = 0;
            u->lib_mode_on = 0;
            tui_send(u, "VIEW_SHOW queue");
        } else if (src == SEL_VIEW_QUEUE) {
            tui_library_open(u);
        } else {
            u->lib_screen = 0;
            u->lib_mode_on = 0;
            tui_send(u, "VIEW_SHOW browse");
        }
    } else if (strcmp(name, "lib_mode") == 0) {
        tui_next_lib_mode(u);
    } else if (strcmp(name, "settings") == 0) {
        tui_settings_open(u);
    } else if (strcmp(name, "queue_scope") == 0) {
        tui_next_scope(u, s);
    } else if (strcmp(name, "library") == 0) {
        tui_library_open(u);
    } else if (strcmp(name, "folders") == 0) {
        /* The playlist folder tree (design doc 9c): the engine's folders view source. */
        u->lib_screen = 0;
        u->lib_mode_on = 0;
        u->folder_opens++;
        tui_send(u, "VIEW_SOURCE folders");
    } else if (strcmp(name, "home") == 0) {
        /* Recently played, jump back in, new this week (I191): the engine's home view source. */
        u->lib_screen = 0;
        u->lib_mode_on = 0;
        tui_send(u, "VIEW_SOURCE home");
    } else if (strcmp(name, "stats") == 0) {
        /* The statistics screen (F24, I192): the engine reads the stats SQL views; b again closes it. */
        tui_send(u, s->stats != NULL ? "STATS_HIDE" : "STATS_SHOW");
    } else if (strcmp(name, "search") == 0) {
        txt_cmdline_open(&u->cmd);
        u->cmd_mode = 2;
    } else if (strcmp(name, "play_pause") == 0) {
        tui_send(u, s->play == SEL_STOPPED ? "PLAY" : "PAUSE");
    } else if (strcmp(name, "subsong_prev") == 0) {
        tui_subsong(u, -1);
    } else if (strcmp(name, "subsong_next") == 0) {
        tui_subsong(u, 1);
    } else if (strcmp(name, "add_file") == 0) {
        tui_add_cursor_file(u);
    } else if (strcmp(name, "remove_entry") == 0) {
        tui_remove_cursor_entry(u);
    } else if (strcmp(name, "info") == 0) {
        u->info = !u->info;
    } else if (strcmp(name, "nowplaying") == 0 || strcmp(name, "songinfo") == 0) {
        int want = name[0] == 'n' ? TUI_INFO_NOWPLAYING : TUI_INFO_SONGINFO;
        u->page = u->page == want ? TUI_INFO_NONE : want;
        u->pattern = 0;
        u->viz = 0;
    } else if (strcmp(name, "page_close") == 0) {
        u->page = TUI_INFO_NONE;
    } else if (strcmp(name, "si_wrong") == 0 || strcmp(name, "si_skip") == 0 || strcmp(name, "si_save") == 0) {
        /* Mark Wrong, Skip Song, Save Note: the same lines the desktop and MUI buttons send (sel_uim_flag_line). */
        if (u->page == TUI_INFO_SONGINFO) {
            char fl[TUI_NOTE_BYTES + 32u];
            uint32_t b = name[3] == 'w' ? 0u : name[3] == 's' && name[4] == 'k' ? 1u : 2u;
            if (sel_uim_flag_line(b, u->si_note, fl, sizeof fl) > 0u) {
                u->si_actions++;
                tui_send(u, fl);
            }
        }
    } else if (strcmp(name, "si_edit") == 0) {
        if (u->page == TUI_INFO_SONGINFO) {
            txt_cmdline_open(&u->cmd);
            u->cmd_mode = 5;
        }
    } else if (strcmp(name, "viz") == 0) {
        u->viz = (u->viz + 1) % 7;
        u->pattern = 0;
    } else if (strcmp(name, "pattern") == 0) {
        u->pattern = !u->pattern;
        u->viz = 0;
        u->pat_asked = 0;
    } else if (strcmp(name, "show_list") == 0) {
        u->pattern = 0;
        u->viz = 0;
    } else if (strncmp(name, "voice", 5) == 0 && name[5] >= '1' && name[5] <= '9' && name[6] == '\0') {
        char vl[32];
        u->voice = (uint32_t)(name[5] - '0');
        snprintf(vl, sizeof vl, "MUTE %u toggle", (unsigned)u->voice);
        tui_send(u, vl);
    } else if (strcmp(name, "solo") == 0) {
        char vl[32];
        snprintf(vl, sizeof vl, "SOLO %u", (unsigned)(u->voice != 0 ? u->voice : 1u));
        tui_send(u, vl);
    } else if (strcmp(name, "command") == 0) {
        txt_cmdline_open(&u->cmd);
        u->cmd_mode = 1;
    } else if (strcmp(name, "palette") == 0 || strcmp(name, "help") == 0) {
        txt_palette_open(&u->pal, &u->binds);
        u->palette_opens++;
        u->overlay = name[0] == 'p' ? 1 : 2;
    } else if (strcmp(name, "menu") == 0) {
        tui_menu_open(u);
    } else if (strcmp(name, "mark") == 0) {
        tui_mark(u);
    } else if (strcmp(name, "quit") == 0) {
        u->quit = 1;
    } else {
        return 0;
    }
    return 1;
}

static void tui_cmd_key(tui* u, const txt_key* k) {
    txt_cmd_result r;
    if (u->cmd_mode == 4) { /* the confirm prompt takes one key */
        u->cmd_mode = 0;
        tui_menu_confirm(u, k->code);
        return;
    }
    r = txt_cmdline_key(&u->cmd, k->code, k->mods);
    if (r == TXT_CMD_EDITING) {
        return;
    }
    if (u->cmd_mode == 5) { /* the note field: Enter keeps the text (quotes dropped, the verb quotes the note), Escape leaves it */
        u->cmd_mode = 0;
        if (r == TXT_CMD_SUBMIT) {
            const char* t = txt_cmdline_text(&u->cmd);
            uint32_t n = 0;
            for (; *t != '\0' && n + 1u < TUI_NOTE_BYTES; t++) {
                if (*t != '"' && (unsigned char)*t >= 32u) {
                    u->si_note[n++] = *t;
                }
            }
            u->si_note[n] = '\0';
        }
        return;
    }
    if (u->cmd_mode == 3) { /* the name prompt: Escape cancels */
        u->cmd_mode = 0;
        if (r == TXT_CMD_SUBMIT) {
            tui_menu_name(u, txt_cmdline_text(&u->cmd));
        }
        return;
    }
    if (u->cmd_mode == 2) { /* the search prompt: an empty line lists all songs */
        u->cmd_mode = 0;
        if (r == TXT_CMD_SUBMIT) {
            tui_library_search(u, txt_cmdline_text(&u->cmd));
        }
        return;
    }
    u->cmd_mode = 0;
    if (r == TXT_CMD_SUBMIT && txt_cmdline_text(&u->cmd)[0] != '\0') {
        const char* line = txt_cmdline_text(&u->cmd);
        u->cmdline_runs++;
        if (line[0] == '!') {
            tui_run_program(u, line + 1);
        } else {
            tui_send(u, line);
        }
    }
}

static void tui_pref_local(tui* u, const char* line);
static void tui_table_note(tui* u, const char* line);

/* A line of the shared action table: noted for the exit report, the layout/visualizer keys act on the text panels, then sent.
 * Bound keys and the palette action rows (I1211) both come here. */
static void tui_run_table_action(tui* u, const char* action) {
    tui_table_note(u, action);
    if (strncmp(action, "PREF_SET ui.desktop.", 20) == 0) {
        tui_pref_local(u, action);
    }
    tui_send(u, action);
}

static void tui_overlay_key(tui* u, const txt_key* k) {
    txt_pal_result r;
    if (u->overlay == 3) {
        tui_settings_key(u, k);
        return;
    }
    if (u->overlay == 4) {
        tui_menu_key(u, k);
        return;
    }
    r = txt_palette_key(&u->pal, k->code, k->mods);
    if (r == TXT_PAL_EDITING) {
        return;
    }
    if (r == TXT_PAL_RUN && u->overlay == 1) {
        /* the palette runs the bare verb; commands with arguments are typed on the command line */
        const char* verb = txt_palette_picked(&u->pal);
        u->overlay = 0;
        if (verb != NULL && u->pal.items[u->pal.cursor].id >= SEL_CMD_COUNT + TXT_PAL_LOCAL_COUNT) {
            tui_run_table_action(u, txt_palette_picked_line(&u->pal)); /* I1211: an action row runs its line like its key */
        } else if (verb != NULL && u->pal.items[u->pal.cursor].id >= SEL_CMD_COUNT) { /* a local entry: its action word */
            if (strcmp(verb, "settings") == 0) {
                tui_settings_open(u);
            }
        } else if (verb != NULL) {
            tui_send(u, verb);
        }
        return;
    }
    if (r == TXT_PAL_RUN) {
        return; /* help only lists */
    }
    u->overlay = 0;
}

/* The binding context: the pattern and viz panels replace the list pane. */
static uint32_t tui_bind_ctx(const tui* u) {
    if (u->page != TUI_INFO_NONE) {
        return TXT_CTX_PAGE;
    }
    return (u->pattern || u->viz || u->layout == SEL_UIM_LAYOUT_MINI || u->layout == SEL_UIM_LAYOUT_VISUAL) ? TXT_CTX_PLAYER
                                                                                                          : TXT_CTX_LIST;
}

/* I1185: the shared table's layout and visualizer keys (PREF_SET ui.desktop.mode / ui.desktop.viz) answered with the
 * text panels: Full is the list with the info pane, Window the list alone, Mini the header alone, Visual the
 * visualizer panel alone; the viz ids pick the panel the `v` key cycles through. */
static void tui_pref_local(tui* u, const char* line) {
    static const struct {
        const char* id;
        int viz;
    } viz_ids[] = {{"viz_levels", 1}, {"viz_scope", 2}, {"viz_spectrum", 3}, {"viz_sidregs", 6}};
    static const char* const modes[] = {"full", "window", "mini", "visual"};
    const char* v = line + strlen("PREF_SET ui.desktop.");
    size_t i;
    if (strncmp(v, "mode ", 5) == 0) {
        for (i = 0; i < 4u; i++) {
            if (strcmp(v + 5, modes[i]) == 0) {
                u->layout = (uint32_t)i;
                u->pattern = 0;
                u->viz = i == SEL_UIM_LAYOUT_VISUAL ? (u->viz != 0 ? u->viz : 2) : 0;
                u->layout_sets++;
            }
        }
    } else if (strncmp(v, "viz ", 4) == 0) {
        for (i = 0; i < sizeof viz_ids / sizeof viz_ids[0]; i++) {
            if (strcmp(v + 4, viz_ids[i].id) == 0) {
                u->viz = viz_ids[i].viz;
                u->pattern = 0;
                u->layout_sets++;
            }
        }
    }
}

/* I1184 sentinel: a bound line that is a row of the shared action table (src/uimodel/actions.c) counts and is logged. */
static void tui_table_note(tui* u, const char* line) {
    uint32_t i, n = sel_uim_action_count();
    for (i = 0; i < n; i++) {
        if (strcmp(sel_uim_action_at(i)->line, line) == 0) {
            size_t len = strlen(u->table_log);
            u->table_actions++;
            if (len + strlen(line) + 2u >= sizeof u->table_log) {
                u->table_log[0] = '\0';
                len = 0;
            }
            snprintf(u->table_log + len, sizeof u->table_log - len, "%s%s", len > 0 ? "|" : "", line);
            return;
        }
    }
}

static void tui_key(tui* u, const txt_key* k) {
    const char* action;
    if (k->code == TXT_KEY_RESIZE) { /* the console says the window changed: ask its size (row I218c) */
        txt_term_ask_size(&u->term);
        return;
    }
    if (k->code == TXT_KEY_WINDOW) { /* the answer; the step loop regrows the grid from txt_term_size */
        txt_term_set_size(&u->term, k->x, k->y);
        if (!u->have_term && k->x > 0 && k->y > 0 && (k->x != u->grid.cols || k->y != u->grid.rows)) {
            (void)tui_resize(u, k->x, k->y); /* headless and keys-file runs have no terminal to ask */
        }
        return;
    }
    if (u->overlay) {
        u->key_count++;
        u->dirty = 1;
        tui_overlay_key(u, k);
        return;
    }
    if (u->cmd_mode) {
        u->key_count++;
        u->dirty = 1;
        tui_cmd_key(u, k);
        return;
    }
    u->splash_off = 1; /* any key ends the start screen and is then handled as usual (row I355) */
    u->cmd.status[0] = '\0';
    if (sel_engine_state(u->engine)->stats != NULL && ((k->code == 27u || k->code == 127u) && k->mods == 0)) {
        u->key_count++;
        u->dirty = 1;
        tui_send(u, "STATS_HIDE");
        return;
    }
    if (u->pg != TUI_PAGE_NONE && tui_page_key(u, k)) {
        u->key_count++;
        u->dirty = 1;
        return;
    }
    if (u->pg == TUI_PAGE_NONE && k->code == 13u && k->mods == 0 && (tui_shelf_header_enter(u) || tui_home_tile_enter(u))) {
        u->key_count++;
        u->dirty = 1;
        return;
    }
    action = txt_bindings_lookup(&u->binds, tui_bind_ctx(u), k->code, k->mods);
    if (action == NULL) {
        if (k->code == 27u && k->mods == 0) { /* I1367 / I623: Escape with nothing open asks before quitting; text from the uimodel dialog table (I1368) */
            u->key_count++;
            u->dirty = 1;
            u->mn_pending = (uint32_t)MN_QUIT;
            u->cmd_mode = 4;
            snprintf(u->mn_lab[0], sizeof u->mn_lab[0], "%s", sel_uim_dlg_text(SEL_UIM_DLG_QUIT));
        }
        return; /* not a key the UI knows */
    }
    u->key_count++;
    u->dirty = 1;
    if (action[0] == '@') {
        (void)tui_local(u, action + 1, k);
    } else {
        tui_run_table_action(u, action);
    }
}

/* ---- keys file ---- */

static const struct {
    const char* name;
    const char* bytes;
} k_tokens[] = {
    {"Up", "\x1b[A"},   {"Down", "\x1b[B"},  {"Right", "\x1b[C"},    {"Left", "\x1b[D"}, {"Home", "\x1b[H"},
    {"End", "\x1b[F"},  {"PageUp", "\x1b[5~"}, {"PageDown", "\x1b[6~"}, {"Enter", "\r"},   {"Tab", "\t"},
    {"Backspace", "\x7f"}, {"Escape", "\x1b"}, {"Space", " "},        {"Ctrl-P", "\x10"},
    {"Ctrl-Left", "\x1b[1;5D"}, {"Ctrl-Right", "\x1b[1;5C"},
    {"Alt-Left", "\x1b[1;3D"}, {"Alt-Right", "\x1b[1;3C"},
};

/* Reads the next line of the keys file; returns 0 at the end of the file. */
static int tui_next_line(tui* u, char* line, size_t cap) {
    while (u->key_text[u->key_at] != '\0') {
        size_t n = 0;
        while (u->key_text[u->key_at] != '\0' && u->key_text[u->key_at] != '\n') {
            if (n + 1 < cap) {
                line[n++] = u->key_text[u->key_at];
            }
            u->key_at++;
        }
        if (u->key_text[u->key_at] == '\n') {
            u->key_at++;
        }
        while (n > 0 && (line[n - 1] == '\r' || line[n - 1] == ' ' || line[n - 1] == '\t')) {
            n--;
        }
        line[n] = '\0';
        if (n > 0 && line[0] != '#') {
            return 1;
        }
    }
    return 0;
}

/* How long the shell turn before this step lasted. timeout_ms is UI_WAIT's
 * block budget, not elapsed time: a clockless shell passes its turn length and
 * file mode stands in for that block, so the turn lasted exactly timeout_ms
 * (deterministic). A device-paced shell passes 0 and the turn is read from the
 * host clock. */
static uint32_t tui_turn_ms(tui* u, uint32_t timeout_ms) {
    uint32_t now = u->host->clock_ms(u->host);
    uint32_t turn = timeout_ms > 0 ? timeout_ms : now - u->turn_mark;
    u->turn_mark = now;
    return turn;
}

/* One step of the keys file: a key (returns 1), a wait (0) or the end (-1). */
static int tui_file_step(tui* u, uint32_t timeout_ms) {
    char line[64], alt[16];
    txt_key k;
    const char* bytes = NULL;
    size_t i;
    if (u->wait_left > 0) {
        uint32_t turn = tui_turn_ms(u, timeout_ms);
        u->wait_left = u->wait_left > turn ? u->wait_left - turn : 0;
        return 0;
    }
    if (!tui_next_line(u, line, sizeof line)) {
        return -1;
    }
    if (strncmp(line, "wait ", 5) == 0) {
        u->wait_left = (uint32_t)strtoul(line + 5, NULL, 10);
        u->turn_mark = u->host->clock_ms(u->host);
        return 0;
    }
    for (i = 0; i < sizeof k_tokens / sizeof k_tokens[0]; i++) {
        if (strcmp(line, k_tokens[i].name) == 0) {
            bytes = k_tokens[i].bytes;
        }
    }
    if (bytes == NULL && strncmp(line, "Alt-", 4) == 0) { /* I1184: ESC then the key's bytes */
        const char* rest = line + 4;
        for (i = 0; i < sizeof k_tokens / sizeof k_tokens[0]; i++) {
            if (strcmp(rest, k_tokens[i].name) == 0) {
                rest = k_tokens[i].bytes;
                break;
            }
        }
        alt[0] = '\x1b';
        snprintf(alt + 1, sizeof alt - 1u, "%s", rest);
        bytes = alt;
    } else if (bytes == NULL && strncmp(line, "Ctrl-", 5) == 0 && line[5] != '\0' && line[6] == '\0') {
        if (line[5] >= '0' && line[5] <= '9') { /* I1185: a Ctrl digit has no control byte; CSI code ; 5 u */
            snprintf(alt, sizeof alt, "\x1b[%d;5u", (int)line[5]);
        } else {
            alt[0] = (char)(line[5] & 0x1F); /* Ctrl-b, Ctrl-d */
            alt[1] = '\0';
        }
        bytes = alt;
    }
    if (bytes == NULL) {
        bytes = line; /* a single printable character */
    }
    for (; *bytes != '\0'; bytes++) {
        if (txt_keys_feed(&u->keys, (uint8_t)*bytes, &k)) {
            tui_key(u, &k);
            return 1;
        }
    }
    if (txt_keys_timeout(&u->keys, &k)) {
        tui_key(u, &k);
        return 1;
    }
    return 0;
}

static char* tui_slurp(const sel_host* host, const char* path) {
    FILE* f = fopen(path, "rb");
    long n;
    char* buf;
    if (f == NULL) {
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    buf = n < 0 ? NULL : (char*)host->mem_alloc(host, (size_t)n + 1u, SEL_MEM_FAST);
    if (buf != NULL) {
        size_t got = fread(buf, 1, (size_t)n, f);
        buf[got] = '\0';
    }
    fclose(f);
    return buf;
}

/* ---- drawing ---- */

static txt_style tui_style(const tui* u, txt_role r) {
    return u->theme.role[r];
}




/* I1222g: the player bar, drawn from the shared sel_uim_frame (the desktop bar's content). Two rows: title - artist, transport glyphs
 * with shuffle / repeat / subsong, volume; clock, seek bar, length, status. One row: title - artist, play glyph, clock / length, volume. */
static void tui_bar_text(tui* u, uint32_t y, const char* text, uint32_t x, uint32_t w, txt_style st) {
    if (x < u->grid.cols) {
        txt_grid_text(&u->grid, x, y, text, w, st);
    }
}

static void tui_player_bar(tui* u, uint32_t y, uint32_t rows) {
    const sel_uim_frame* f = &u->uim.frame;
    uint32_t cols = u->grid.cols, bw, fill, i, vx;
    char buf[256], vol[24], mid[96];
    txt_style hs = tui_style(u, TXT_ROLE_HEADER), nm = tui_style(u, TXT_ROLE_NORMAL), dim = tui_style(u, TXT_ROLE_DIM);
    snprintf(vol, sizeof vol, "Volume %d", (int)f->volume);
    vx = cols > (uint32_t)strlen(vol) + 1u ? cols - (uint32_t)strlen(vol) - 1u : 0u;
    txt_grid_fill(&u->grid, 0, y, cols, rows, ' ', nm);
    snprintf(buf, sizeof buf, "%s%s%s", f->title[0] ? f->title : "-", f->artist[0] ? " - " : "", f->artist);
    snprintf(mid, sizeof mid, "%s %s %s", f->can_prev ? "|<" : "  ", f->play == SEL_PLAYING ? "||" : " >", f->can_next ? ">|" : "  ");
    if (rows == 1u) {
        txt_grid_fill(&u->grid, 0, y, cols, 1, ' ', hs);
        /* The state flags (shuffle, repeat, subsong) stay visible in the one-row form: they sit between the title and the clock. */
        char ind[64];
        uint32_t il, tw = cols > 44u ? cols - 44u : 1u;
        snprintf(ind, sizeof ind, "%s%s", f->random_on ? "Shuffle  " : "",
                 f->repeat_mode != 0u ? (f->repeat_mode == 2u ? "Repeat one  " : "Repeat all  ") : "");
        if (f->subsong_count > 1u) {
            char sub[24];
            snprintf(sub, sizeof sub, "%u/%u  ", (unsigned)f->subsong, (unsigned)f->subsong_count);
            strncat(ind, sub, sizeof ind - strlen(ind) - 1u);
        }
        il = (uint32_t)strlen(ind);
        if (il != 0u && tw > il + 8u) {
            tw -= il;
            tui_bar_text(u, y, ind, 1u + tw + 1u, il, hs);
        }
        tui_bar_text(u, y, buf, 1, tw, hs);
        snprintf(buf, sizeof buf, "%s  %s / %s", f->play == SEL_PLAYING ? ">" : "||", f->clock, f->length[0] ? f->length : "-:--");
        tui_bar_text(u, y, buf, cols > 42u ? cols - 42u : 0u, 24, hs);
        tui_bar_text(u, y, vol, vx, 12, hs);
        return;
    }
    txt_grid_fill(&u->grid, 0, y, cols, 1, ' ', hs);
    tui_bar_text(u, y, buf, 1, cols / 3u, hs);
    snprintf(buf, sizeof buf, "%s%s%s", mid, f->random_on ? "  Shuffle" : "",
             f->repeat_mode != 0u ? (f->repeat_mode == 2u ? "  Repeat one" : "  Repeat all") : "");
    if (f->subsong_count > 1u) {
        char sub[24];
        snprintf(sub, sizeof sub, "  %u/%u", (unsigned)f->subsong, (unsigned)f->subsong_count);
        strncat(buf, sub, sizeof buf - strlen(buf) - 1u);
    }
    tui_bar_text(u, y, buf, cols / 3u + 1u, cols / 3u, hs);
    tui_bar_text(u, y, vol, vx, 12, hs);
    /* row 2: clock, seek bar, length, status */
    txt_grid_fill(&u->grid, 0, y + 1u, cols, 1, ' ', nm);
    tui_bar_text(u, y + 1u, f->clock, 1, 8, nm);
    bw = cols > 40u ? cols / 3u : 8u;
    fill = (uint32_t)(((uint64_t)f->progress_q16 * bw) >> 16);
    for (i = 0; i < bw && 8u + i < cols; i++) {
        txt_grid_put(&u->grid, 8u + i, y + 1u, i < fill ? 0x2588u : 0x2500u, f->no_seek ? dim : nm);
    }
    tui_bar_text(u, y + 1u, f->length[0] ? f->length : "-:--", 9u + bw, 8, nm);
    tui_bar_text(u, y + 1u, f->status, 18u + bw, cols > 19u + bw ? cols - 19u - bw : 1u, dim);
}

/* I1222g: the top bar row, the desktop's labels as text: [Home], the search field with its hint, [Browse]. */
static void tui_topbar(tui* u, uint32_t y) {
    char hint[96], line[160];
    txt_grid_fill(&u->grid, 0, y, u->grid.cols, 1, ' ', tui_style(u, TXT_ROLE_HEADER));
    sel_uim_search_hint(1, NULL, 0, hint, sizeof hint);
    snprintf(line, sizeof line, "[Home]  [%s]  [Browse]", hint);
    txt_grid_text(&u->grid, 1, y, line, u->grid.cols - 1u, tui_style(u, TXT_ROLE_HEADER));
}


#define TUI_GRID_CELL 24u

/* I1186: the Grid, Cover Flow and Compact library styles as text. The engine lists the flat rows of the regular style for
 * them (view.c mode_of); the text UI lays them out. Grid: cells in columns. Cover Flow: the cursor row in the middle with
 * its neighbours around it (artwork has no text form, so the cover itself is exempt). Compact: one dense line per song. */
static void tui_styled_rows(tui* u, const sel_state* s, uint32_t width) {
    const sel_view_state* v = s->view;
    uint32_t rows = tui_main_rows(u), i, cur = v->cursor >= v->window_start ? v->cursor - v->window_start : 0;
    char buf[300];
    if (v->style == SEL_VIEW_STYLE_GRID) {
        uint32_t cols = (width - 1u) / TUI_GRID_CELL;
        cols = cols == 0 ? 1u : cols;
        for (i = 0; i < v->row_n && i / cols < rows; i++) {
            const sel_view_row* r = &v->rows[i];
            int at = v->window_start + i == v->cursor;
            int playing = r->kind == SEL_ROW_QUEUE && r->id == s->queue_playing;
            txt_style st = tui_style(u, at ? TXT_ROLE_CURSOR : playing ? TXT_ROLE_PLAYING : TXT_ROLE_FILE);
            uint32_t x = 1u + (i % cols) * TUI_GRID_CELL, y = 3u + i / cols;
            snprintf(buf, sizeof buf, "%c%s", playing ? '>' : ' ', r->text);
            tui_pfill(u, x, y, TUI_GRID_CELL - 1u, 1, ' ', st);
            tui_ptext(u, x, y, buf, TUI_GRID_CELL - 2u, st);
        }
    } else if (v->style == SEL_VIEW_STYLE_COVERFLOW) {
        uint32_t mid = rows / 2u, d;
        for (d = 0; d < rows; d++) {
            int64_t idx = (int64_t)cur + (int64_t)d - (int64_t)mid;
            const sel_view_row* r;
            uint32_t dist = d > mid ? d - mid : mid - d;
            txt_style st;
            if (idx < 0 || idx >= (int64_t)v->row_n) {
                continue;
            }
            r = &v->rows[idx];
            st = tui_style(u, dist == 0 ? TXT_ROLE_CURSOR : dist == 1 ? TXT_ROLE_FILE : TXT_ROLE_DIM);
            if (dist == 0) {
                snprintf(buf, sizeof buf, "<  %s%s%s  >", r->text, r->artist != NULL && r->artist[0] ? "  -  " : "",
                         r->artist != NULL ? r->artist : "");
            } else {
                snprintf(buf, sizeof buf, "%*s%s", (int)(2u * dist > 8u ? 8u : 2u * dist), "", r->text);
            }
            tui_pfill(u, 0, 3u + d, width, 1, ' ', st);
            tui_ptext(u, 1, 3u + d, buf, width - 1u, st);
        }
    } else {
        for (i = 0; i < v->row_n && i < rows; i++) {
            const sel_view_row* r = &v->rows[i];
            int at = v->window_start + i == v->cursor;
            int playing = r->kind == SEL_ROW_QUEUE && r->id == s->queue_playing;
            txt_style st = tui_style(u, at ? TXT_ROLE_CURSOR : playing ? TXT_ROLE_PLAYING : TXT_ROLE_FILE);
            snprintf(buf, sizeof buf, "%c%s", playing ? '>' : ' ', r->text);
            tui_pfill(u, 0, 3u + i, width, 1, ' ', st);
            tui_ptext(u, 1, 3u + i, buf, width - 1u, st);
        }
    }
    u->style_drawn = v->style;
    u->style_draws++;
}

static void tui_pane(tui* u, const sel_state* s, uint32_t width) {
    const sel_view_state* v = s->view;
    char buf[300];
    uint32_t i;
    txt_style title = tui_style(u, TXT_ROLE_LABEL);
    if (v == NULL) {
        return;
    }
    if (v->source == SEL_VIEW_BROWSE) {
        snprintf(buf, sizeof buf, "Browse  %s",
                 v->browse_leaf != NULL && v->browse_leaf[0] ? v->browse_leaf : (v->browse_path != NULL ? v->browse_path : ""));
    } else if (v->source == SEL_VIEW_FOLDERS) {
        snprintf(buf, sizeof buf, "Playlist folders  %u rows", (unsigned)v->row_count);
    } else if (v->source == SEL_VIEW_HOME) {
        snprintf(buf, sizeof buf, "Home");
    } else {
        sel_uim_queue_title(buf, sizeof buf, v, u->lib_screen);
    }
    if (u->lib_mode_on) { /* I1187: the desktop's mode tab and Scope button, as words on the pane title */
        size_t at = strlen(buf);
        snprintf(buf + at, sizeof buf - at, "  Mode %s", sel_uim_libmode_name(u->lib_mode));
    }
    if (v->source == SEL_VIEW_QUERY || v->source == SEL_VIEW_QUEUE) {
        size_t at = strlen(buf);
        snprintf(buf + at, sizeof buf - at, "  %s", sel_uim_scope_label(sel_uim_scope_index(s->options[SEL_OPT_SCOPE])));
    }
    tui_ptext(u, 1, 2, buf, width - 1u, title);
    if (v->source == SEL_VIEW_HOME && v->row_count == 0) {
        tui_ptext(u, 1, 3, "Nothing played yet", width - 1u, tui_style(u, TXT_ROLE_DIM));
    }
    if (v->source == SEL_VIEW_FOLDERS && v->row_count == 0) {
        tui_ptext(u, 1, 3, "No playlist folders yet", width - 1u, tui_style(u, TXT_ROLE_DIM));
    }
    if (u->lib_screen && v->source == SEL_VIEW_QUERY && v->row_count == 0) {
        tui_ptext(u, 1, 3,
                      u->lib_empty_query ? "Library is empty: scan a folder with :LIBRARY_SCAN <folder>"
                                         : "No songs match the search",
                      width - 1u, tui_style(u, TXT_ROLE_DIM));
    }
    if (tui_queue_view(v) && v->style >= SEL_VIEW_STYLE_GRID && v->style < SEL_VIEW_STYLE_COUNT) {
        tui_styled_rows(u, s, width);
        return;
    }
    u->style_drawn = v->style;
    for (i = 0; i < v->row_n && i < tui_main_rows(u); i++) {
        const sel_view_row* r = &v->rows[i];
        uint32_t y = 3u + i;
        int at_cursor = v->window_start + i == v->cursor;
        int playing = v->source == SEL_VIEW_QUEUE && r->kind == SEL_ROW_QUEUE && r->id == s->queue_playing;
        txt_role role = at_cursor ? TXT_ROLE_CURSOR
                        : playing ? TXT_ROLE_PLAYING
                        : r->kind == SEL_ROW_HEADING ? TXT_ROLE_LABEL
                        : r->kind == SEL_ROW_DIR || r->kind == SEL_ROW_NODE || r->kind == SEL_ROW_FOLDER ? TXT_ROLE_DIRECTORY
                                                 : TXT_ROLE_FILE;
        txt_style st = tui_style(u, role);
        if (r->kind == SEL_ROW_NODE) {
            snprintf(buf, sizeof buf, "%s %s (%u)", v->style == SEL_VIEW_STYLE_TREE ? (r->expanded ? "[-]" : "[+]") : "[>]",
                     r->text, (unsigned)r->count);
        } else if (r->kind == SEL_ROW_FOLDER || r->kind == SEL_ROW_PLAYLIST) {
            snprintf(buf, sizeof buf, "%*s%s %s", (int)(2u * r->depth), "", r->kind == SEL_ROW_FOLDER ? "[-]" : " - ", r->text);
        } else if (r->kind == SEL_ROW_HEADING || r->kind == SEL_ROW_HOME) {
            snprintf(buf, sizeof buf, "%*s%s", (int)(2u * r->depth), "", r->text);
        } else {
            snprintf(buf, sizeof buf, "%*s%c %s%s", (int)(2u * r->depth), "", playing ? '>' : ' ', r->text,
                     r->kind == SEL_ROW_DIR && strcmp(r->text, "..") != 0 ? "/" : "");
        }
        tui_pfill(u, 0, y, width, 1, ' ', st);
        tui_ptext(u, 1, y, buf, width - 1u, st);
    }
}


/* Every META tag the replayer reported, in tags.def order. */
static void tui_info(tui* u, const sel_state* s, uint32_t x, uint32_t top) {
    const uintptr_t absent = (uintptr_t)-1;
    uint32_t tag, y = top + 1u;
    uint32_t w = u->grid.cols - x - 1u;
    tui_ptext(u, x, top, "Information", w, tui_style(u, TXT_ROLE_LABEL));
    for (tag = SEL_META_TITLE; sel_tag_name(tag) != NULL && y < u->grid.rows - 1u; tag++) {
        uintptr_t v = sel_tag_find(s->song.meta, tag, absent);
        sel_tag_type type = SEL_TT_INT;
        char label[32], val[160];
        uint32_t i;
        if (v == absent || !sel_tag_type_of(tag, &type)) {
            continue;
        }
        if (type == SEL_TT_STR) {
            snprintf(val, sizeof val, "%s", (const char*)v);
            for (i = 0; val[i] != '\0'; i++) {
                if ((unsigned char)val[i] < 32u) {
                    val[i] = ' ';
                }
            }
        } else {
            snprintf(val, sizeof val, "%lu", (unsigned long)v);
        }
        sel_uim_tag_label(tag, label, sizeof label);
        tui_ptext(u, x, y, label, w, tui_style(u, TXT_ROLE_LABEL));
        tui_ptext(u, x + 12u, y, val, w > 12u ? w - 12u : 1u, tui_style(u, TXT_ROLE_VALUE));
        y++;
    }
    /* I185: the catalogue rows the engine took from the library (label and value pairs); absent data has no row. */
    u->meta_panel_rows = 0;
    if (s->panel != NULL) {
        const char* p = s->panel;
        while (*p != '\0' && y < u->grid.rows - 1u) {
            const char* value = p + strlen(p) + 1;
            char val[160];
            uint32_t i;
            snprintf(val, sizeof val, "%s", value);
            for (i = 0; val[i] != '\0'; i++) {
                if ((unsigned char)val[i] < 32u) {
                    val[i] = ' ';
                }
            }
            tui_ptext(u, x, y, p, w, tui_style(u, TXT_ROLE_LABEL));
            tui_ptext(u, x + 12u, y, val, w > 12u ? w - 12u : 1u, tui_style(u, TXT_ROLE_VALUE));
            y++;
            u->meta_panel_rows++;
            p = value + strlen(value) + 1;
        }
    }
}

/* Now Playing page (desktop PG_NOWPLAYING): the open song's title, artist and format, then the next songs of the queue. */
static void tui_nowplaying_page(tui* u, const sel_state* s) {
    char buf[300], format[SEL_FORMAT_LABEL_MAX];
    const char* title = (const char*)sel_tag_find(s->song.meta, SEL_META_TITLE, 0);
    const char* author = (const char*)sel_tag_find(s->song.meta, SEL_META_AUTHOR, 0);
    uint32_t k, first, y = 7;
    txt_style lab = tui_style(u, TXT_ROLE_LABEL);
    u->np_draws++;
    tui_ptext(u, 1, 2, "Now Playing", u->grid.cols - 2u, lab);
    if (s->song.path == NULL) {
        tui_ptext(u, 1, 4, sel_uim_state_text(SEL_UIM_SV_NOWPLAYING, SEL_UIM_ST_EMPTY), u->grid.cols - 2u,
                      tui_style(u, TXT_ROLE_DIM));
    } else {
        sel_format_label((const char*)sel_tag_find(s->song.meta, SEL_META_FORMAT, 0), format, sizeof format);
        tui_ptext(u, 1, 4, title != NULL && title[0] ? title : "-", u->grid.cols - 2u, tui_style(u, TXT_ROLE_VALUE));
        tui_ptext(u, 1, 5, author != NULL ? author : "", u->grid.cols - 2u, tui_style(u, TXT_ROLE_NORMAL));
        tui_ptext(u, 1, 6, format, u->grid.cols - 2u, tui_style(u, TXT_ROLE_DIM));
    }
    y++;
    tui_ptext(u, 1, y++, "Next in Queue", u->grid.cols - 2u, lab);
    for (k = 0; k < s->queue_count && s->queue[k].id != s->queue_playing; k++) {
    }
    for (first = ++k; k < s->queue_count && k - first < 8u && y + 1u < u->grid.rows; k++, y++) {
        const char* path = s->queue_text + s->queue[k].path_offset;
        const char* slash = strrchr(path, '/');
        snprintf(buf, sizeof buf, "%s", slash != NULL ? slash + 1 : path);
        tui_ptext(u, 1, y, buf, u->grid.cols - 2u, tui_style(u, TXT_ROLE_NORMAL));
    }
    /* I1313c: players this machine cannot run, each line as the registry filter wrote it (never derived here). */
    if (s->players_off != NULL && s->players_off[0] != '\0' && y + 2u < u->grid.rows) {
        const char* line;
        y++;
        tui_ptext(u, 1, y++, "Players Turned Off", u->grid.cols - 2u, lab);
        for (line = s->players_off; *line != '\0' && y + 1u < u->grid.rows; line += strlen(line) + 1u, y++) {
            tui_ptext(u, 1, y, line, u->grid.cols - 2u, tui_style(u, TXT_ROLE_DIM));
        }
    }
}

/* Song Information page (desktop draw_songinfo): the three buttons, the note field, then every tag of the open song. */
static void tui_songinfo_page(tui* u, const sel_state* s) {
    char buf[300];
    uint32_t b, x = 1;
    static const char* const keys[3] = {"w", "x", "Enter"};
    tui_ptext(u, 1, 2, "Song Information", u->grid.cols - 2u, tui_style(u, TXT_ROLE_LABEL));
    for (b = 0; b < 3u; b++) {
        snprintf(buf, sizeof buf, "%s %s", keys[b], sel_uim_flag_label(b));
        tui_ptext(u, x, 3, buf, u->grid.cols - x - 1u, tui_style(u, TXT_ROLE_VALUE));
        x += (uint32_t)strlen(buf) + 3u;
    }
    snprintf(buf, sizeof buf, "e Note: %s", u->si_note[0] != '\0' ? u->si_note : "");
    tui_ptext(u, 1, 4, buf, u->grid.cols - 2u, tui_style(u, u->si_note[0] != '\0' ? TXT_ROLE_NORMAL : TXT_ROLE_DIM));
    tui_info(u, s, 1u, 6u);
}

/* Palette and help overlay: query line, then verbs grouped by area (query empty)
 * or ranked (query typed), the bound key at the right edge. */
static void tui_overlay(tui* u) {
    const txt_palette* p = &u->pal;
    uint32_t w = u->grid.cols > 60u ? 60u : u->grid.cols;
    uint32_t x0 = (u->grid.cols - w) / 2u;
    uint32_t rows = u->grid.rows > 4u ? u->grid.rows - 4u : 1u;
    uint32_t y = 2, i, first = 0;
    const char* last_area = NULL;
    char line[96];
    txt_style n = tui_style(u, TXT_ROLE_NORMAL);
    txt_style d = tui_style(u, TXT_ROLE_DIM);
    txt_style sel = tui_style(u, TXT_ROLE_CURSOR);
    uint32_t r;
    for (r = 1; r < rows + 1u; r++) {
        txt_grid_fill(&u->grid, x0, 1u + r - 1u, w, 1, ' ', n);
    }
    snprintf(line, sizeof line, "%s %s", u->overlay == 1 ? "Command palette >" : "Help >", p->query);
    txt_grid_text(&u->grid, x0 + 1u, 1, line, w - 2u, n);
    if (p->cursor + 1u > rows - 1u) {
        first = p->cursor + 1u - (rows - 1u);
    }
    for (i = first; i < p->count && y < rows + 1u; i++) {
        const txt_palette_item* it = &p->items[i];
        txt_style st = i == p->cursor ? sel : n;
        if (p->len == 0 && (last_area == NULL || strcmp(last_area, it->area) != 0)) {
            last_area = it->area;
            txt_grid_text(&u->grid, x0 + 1u, y++, txt_palette_area_title(it->area), w - 2u, d);
            if (y >= rows + 1u) {
                break;
            }
        }
        txt_grid_fill(&u->grid, x0, y, w, 1, ' ', st);
        txt_grid_text(&u->grid, x0 + 3u, y, it->name, w - 12u, st);
        if (it->key[0] != '\0') {
            txt_grid_text(&u->grid, x0 + w - 8u, y, it->key, 7, st);
        }
        y++;
    }
}

/* I1188: the Settings screen, one row per shared table row: the label, then the value. */
static void tui_settings(tui* u) {
    uint32_t n = sel_uim_pref_row_count(), cols = u->grid.cols, rows = u->grid.rows > 4u ? u->grid.rows - 4u : 1u;
    uint32_t i, first = 0, y = 2, lw = 26;
    txt_style nrm = tui_style(u, TXT_ROLE_NORMAL), dim = tui_style(u, TXT_ROLE_DIM), cur = tui_style(u, TXT_ROLE_CURSOR);
    char line[160];
    for (i = 1; i <= rows + 1u; i++) {
        txt_grid_fill(&u->grid, 0, i, cols, 1, ' ', nrm);
    }
    txt_grid_text(&u->grid, 2, 1, "Settings", cols - 4u, nrm);
    if (u->set_focus >= rows) {
        first = u->set_focus + 1u - rows;
    }
    for (i = first; i < n && y < rows + 1u; i++, y++) {
        const sel_uim_pref_row* r = sel_uim_pref_row_at(i);
        const char* v = tui_pref(u, r->key);
        txt_style st = i == u->set_focus ? cur : nrm;
        char shown[64];
        if (r->ui == (uint32_t)SEL_UIM_PREF_CYCLE) {
            sel_uim_pref_choice(r->choices, sel_uim_pref_choice_index(r->choices, v), shown, sizeof shown);
            snprintf(line, sizeof line, "< %s >", shown);
        } else if (r->ui == (uint32_t)SEL_UIM_PREF_CHECK) {
            snprintf(line, sizeof line, "[%c] %s", v[0] == '1' ? 'x' : ' ', v[0] == '1' ? "On" : "Off");
        } else if (r->ui == (uint32_t)SEL_UIM_PREF_SLIDER) {
            long lo = 0, hi = 100, x = v[0] != '\0' ? strtol(v, NULL, 10) : lo;
            uint32_t fill, k;
            char bar[21];
            sscanf(r->choices, "%ld,%ld", &lo, &hi);
            x = x < lo ? lo : (x > hi ? hi : x);
            fill = hi > lo ? (uint32_t)((x - lo) * 20 / (hi - lo)) : 0u;
            for (k = 0; k < 20u; k++) {
                bar[k] = k < fill ? '#' : '-';
            }
            bar[20] = '\0';
            snprintf(line, sizeof line, "%3ld [%s]", x, bar);
        } else if (r->ui == (uint32_t)SEL_UIM_PREF_ROM) {
            int slot = sel_uim_rom_slot(r->choices);
            snprintf(line, sizeof line, "%s",
                     sel_uim_rom_source_text(slot >= 0 && sel_uim_rom_is_user(tui_pref(u, "config.dir"), (uint32_t)slot)));
        } else if (u->set_edit && i == u->set_focus) {
            snprintf(line, sizeof line, "%s_", u->set_buf);
        } else {
            snprintf(line, sizeof line, "%s", v[0] != '\0' ? v : "(default)");
        }
        txt_grid_fill(&u->grid, 0, y, cols, 1, ' ', st);
        txt_grid_text(&u->grid, 2, y, r->label, lw, st);
        txt_grid_text(&u->grid, 2u + lw + 2u, y, line, cols > lw + 6u ? cols - lw - 6u : 1u, st);
    }
    txt_grid_text(&u->grid, 2, rows + 1u, "Up Down move   Left Right change   Enter edit text   Esc close", cols - 4u, dim);
}

/* ---- viz and pattern panels (M1.3 I101) ---- */

static const char* const k_viz_title[] = {"", "Levels", "Scope", "Spectrum", "Note dots", "Phase", "SID registers"};


/* Phase panel: the last post-mix frames as an L (x) against R (y) plot in 2x4 braille cells, plus
 * the Q15 correlation as a percentage. Only the mixed path has a post-mix signal. */
#define TUI_PHASE_COLS 40u
#define TUI_PHASE_ROWS 12u
static void tui_phase_panel(tui* u, const sel_state* s, uint32_t width) {
    const sel_viz_frame* f = s->viz;
    txt_style norm = tui_style(u, TXT_ROLE_NORMAL);
    txt_style dim = tui_style(u, TXT_ROLE_DIM);
    uint32_t rows = tui_main_rows(u);
    uint32_t pw = width > 3u && width - 3u < TUI_PHASE_COLS ? width - 3u : TUI_PHASE_COLS;
    uint32_t ph = rows > 4u && rows - 4u < TUI_PHASE_ROWS ? rows - 4u : TUI_PHASE_ROWS;
    uint8_t cell[TUI_PHASE_ROWS][TUI_PHASE_COLS];
    static const uint8_t bit[4][2] = {{0x01, 0x08}, {0x02, 0x10}, {0x04, 0x20}, {0x40, 0x80}};
    uint32_t i, x, y;
    char buf[32];
    int32_t pct;
    if (!s->mixed_path) {
        tui_ptext(u, 1, 3, "unsupported_path", width - 1u, dim);
        return;
    }
    memset(cell, 0, sizeof cell);
    for (i = 0; i < SEL_VIZ_PCM_FRAMES; i += 4u) {
        uint32_t px = ((uint32_t)((int32_t)f->pcm[i * 2u] + 32768) * pw * 2u) >> 16;
        uint32_t py = ((uint32_t)(32767 - (int32_t)f->pcm[i * 2u + 1u]) * ph * 4u) >> 16;
        cell[py / 4u][px / 2u] |= bit[py % 4u][px % 2u];
        u->phase_points++;
    }
    for (y = 0; y < ph; y++) {
        for (x = 0; x < pw; x++) {
            tui_pput(u, 2u + x, 3u + y, cell[y][x] ? 0x2800u + cell[y][x] : ' ', norm);
        }
    }
    pct = (int32_t)f->correlation * 100 / 32768;
    snprintf(buf, sizeof buf, "Correlation %d%%", (int)pct);
    tui_ptext(u, 2, 3u + ph, buf, width - 2u, dim);
}


/* SID registers panel (I1185, desktop viz_sidregs, plugins/viz/sidregs/sidregs.c): per chip and voice the frequency,
 * pulse width, the gate and waveform flags, the envelope level and the filter route; per chip the filter cutoff,
 * resonance, mode flags and volume. The registers come from the engine's HW feed (f->sid), decoded as sidregs.c does. */
static void tui_sid_panel(tui* u, const sel_state* s, uint32_t width) {
    const sel_viz_frame* f = s->viz;
    txt_style label = tui_style(u, TXT_ROLE_LABEL);
    txt_style norm = tui_style(u, TXT_ROLE_NORMAL);
    txt_style dim = tui_style(u, TXT_ROLE_DIM);
    uint32_t chips = f->sid_chips > SEL_VIZ_SID_CHIPS ? SEL_VIZ_SID_CHIPS : f->sid_chips;
    uint32_t rows = tui_main_rows(u), k, v, y = 3;
    char buf[96];
    if (chips == 0u) {
        tui_ptext(u, 1, 3, "No SID register feed for this song", width - 1u, dim);
        return;
    }
    u->sid_frames++;
    for (k = 0; k < chips && y + 5u <= rows + 2u; k++) {
        const sel_viz_sid_chip* c = &f->sid[k];
        uint32_t cutoff = ((uint32_t)c->reg[0x16] << 3) | ((uint32_t)c->reg[0x15] & 7u);
        snprintf(buf, sizeof buf, "Chip %u", (unsigned)(k + 1u));
        tui_ptext(u, 1, y++, buf, width - 1u, label);
        for (v = 0; v < 3u; v++) {
            const uint8_t* r = &c->reg[7u * v];
            uint32_t ctl = r[4];
            snprintf(buf, sizeof buf, "Voice%u Fr %04X Pw %03X %s%s%s%s%s%s%s%s Env %2u%s", (unsigned)(v + 1u),
                     (unsigned)((uint32_t)r[0] | ((uint32_t)r[1] << 8)),
                     (unsigned)(((uint32_t)r[2] | ((uint32_t)r[3] << 8)) & 0xFFFu), (ctl & 1u) ? "Gate " : "",
                     (ctl & 16u) ? "Tri " : "", (ctl & 32u) ? "Saw " : "", (ctl & 64u) ? "Pul " : "",
                     (ctl & 128u) ? "Noi " : "", (ctl & 2u) ? "Sync " : "", (ctl & 4u) ? "Ring " : "",
                     (ctl & 8u) ? "Test " : "", (unsigned)c->env[v], ((c->reg[0x17] >> v) & 1u) ? " Flt" : "");
            tui_ptext(u, 1, y++, buf, width - 1u, norm);
        }
        snprintf(buf, sizeof buf, "Filter %04u Reso %2u %s%s%s Volume %2u", (unsigned)cutoff,
                 (unsigned)(c->reg[0x17] >> 4), (c->reg[0x18] & 16u) ? "LP " : "", (c->reg[0x18] & 32u) ? "BP " : "",
                 (c->reg[0x18] & 64u) ? "HP" : "", (unsigned)(c->reg[0x18] & 15u));
        tui_ptext(u, 1, y++, buf, width - 1u, dim);
    }
}

static void tui_viz_panel(tui* u, const sel_state* s, uint32_t width) {
    const sel_viz_frame* f = s->viz;
    txt_style label = tui_style(u, TXT_ROLE_LABEL);
    txt_style norm = tui_style(u, TXT_ROLE_NORMAL);
    txt_style dim = tui_style(u, TXT_ROLE_DIM);
    uint32_t rows = tui_main_rows(u), i, x;
    char buf[32];
    tui_ptext(u, 1, 2, k_viz_title[u->viz], width - 1u, label);
    if (f == NULL || f->seq == 0) {
        tui_ptext(u, 1, 3, "No visualisation data", width - 1u, dim);
        return;
    }
    if (u->viz == 3) { /* spectrum: one block column pair per band */
        uint32_t height = rows > 3u ? rows - 2u : 1u;
        uint32_t b, h;
        if (!f->spectrum_valid) {
            tui_ptext(u, 1, 3, "No spectrum available", width - 1u, dim);
            return;
        }
        for (b = 0; b < SEL_VIZ_BANDS && 2u + 3u * b + 2u < width; b++) {
            uint32_t fill = ((uint32_t)(f->band[b] > 0 ? f->band[b] : 0) * height) >> 14;
            fill = fill > height ? height : fill;
            for (h = 0; h < fill; h++) {
                tui_pput(u, 2u + 3u * b, 2u + height - h, 0x2588u, norm);
                tui_pput(u, 3u + 3u * b, 2u + height - h, 0x2588u, norm);
            }
        }
        return;
    }
    if (u->viz == 5) {
        tui_phase_panel(u, s, width);
        return;
    }
    if (u->viz == 6) {
        tui_sid_panel(u, s, width);
        return;
    }
    if (f->voice_count == 0) {
        tui_ptext(u, 1, 3, "No per voice data for this player", width - 1u, dim);
        return;
    }
    u->viz_frames++;
    for (i = 0; i < f->voice_count && i + 1u < rows; i++) {
        uint32_t y = 3u + i;
        int muted = (int)((s->mute_mask >> i) & 1u);
        snprintf(buf, sizeof buf, "Voice %2u", (unsigned)(i + 1u));
        tui_ptext(u, 1, y, buf, 9, label);
        if (u->viz == 1) {
            uint32_t on = ((uint32_t)(f->vu[i] > 0 ? f->vu[i] : 0) * 24u) >> 15;
            for (x = 0; x < 24u; x++) {
                tui_pput(u, 11u + x, y, x < on ? '#' : ' ', norm);
            }
            tui_pput(u, 10, y, '[', dim);
            tui_pput(u, 35, y, ']', dim);
            if (muted) {
                tui_ptext(u, 37, y, "Muted", width > 37u ? width - 37u : 1u, dim);
            }
        } else if (u->viz == 4) { /* note dots: one dot per sounding voice, column = pitch over 96 semitones */
            u->dot_rows++;
            if (f->pitch_q8[i] != 0 && !muted) {
                uint32_t span = width > 13u ? width - 13u : 1u;
                tui_pput(u, 11u + sel_uim_semitone(f->pitch_q8[i]) * span / 96u, y, 0x25CFu,
                             f->note_on[i] ? norm : dim);
            }
        } else { /* scope: peak magnitude of each 4 sample span as a block glyph */
            u->scope_reads++;
            for (x = 0; x < SEL_VIZ_SCOPE_FRAMES / 4u && 11u + x < width; x++) {
                uint32_t k, peak = 0;
                for (k = 0; k < 4u; k++) {
                    int32_t v = f->scope[i][x * 4u + k];
                    uint32_t m = (uint32_t)(v < 0 ? -v : v);
                    peak = m > peak ? m : peak;
                }
                tui_pput(u, 11u + x, y, muted ? ' ' : sel_uim_level_glyph(peak > 32767u ? 32767u : peak), norm);
            }
        }
    }
}


#define TUI_CELL_COLS 14u

static void tui_pattern_panel(tui* u, const sel_state* s, uint32_t width) {
    const sel_pattern_view* v = s->pattern;
    txt_style label = tui_style(u, TXT_ROLE_LABEL);
    txt_style norm = tui_style(u, TXT_ROLE_NORMAL);
    txt_style dim = tui_style(u, TXT_ROLE_DIM);
    char buf[96];
    uint32_t r, c, cols_fit = width > 6u ? (width - 5u) / TUI_CELL_COLS : 0;
    if (v == NULL || !v->valid) {
        tui_ptext(u, 1, 2, "Pattern", width - 1u, label);
        tui_ptext(u, 1, 3, "No pattern data", width - 1u, dim);
        return;
    }
    snprintf(buf, sizeof buf, "Pattern  order %u of %u  pattern %u  rows %u", (unsigned)(v->order + 1u),
             (unsigned)v->info.orders, (unsigned)s->song.pattern_pos.pattern, (unsigned)v->info.rows);
    tui_ptext(u, 1, 2, buf, width - 1u, label);
    for (r = 0; r < v->row_n && 3u + r + 1u < u->grid.rows; r++) {
        uint32_t row = v->row_lo + r, y = 3u + r;
        int at = v->order == s->song.pattern_pos.order && row == s->song.pattern_pos.row;
        txt_style st = at ? tui_style(u, TXT_ROLE_CURSOR) : norm;
        if (row >= v->info.rows) {
            break;
        }
        u->pattern_rows++;
        tui_pfill(u, 0, y, width, 1, ' ', st);
        snprintf(buf, sizeof buf, "%02X", (unsigned)row);
        tui_ptext(u, 1, y, buf, 3, at ? st : dim);
        for (c = 0; c < v->channels && c < cols_fit; c++) {
            const sel_rp_cell* cell = &v->cell[r][c];
            char note[4], ins[4], vol[4], fx[8];
            sel_uim_cell_fields(cell, note, ins, vol, fx);
            snprintf(buf, sizeof buf, "%s %s %s %s", note, ins, vol, fx);
            tui_pput(u, 4u + c * TUI_CELL_COLS, y, 0x2502u, tui_style(u, TXT_ROLE_BORDER));
            tui_ptext(u, 5u + c * TUI_CELL_COLS, y, buf, TUI_CELL_COLS - 1u, st);
        }
    }
}

/* Asks the engine for the pattern window around the playing row when the
 * panel is open and the position moved. The audio thread answers; the next
 * pump publishes it (sel_state.pattern). */
static void tui_pattern_ask(tui* u) {
    const sel_state* s = sel_engine_state(u->engine);
    uint32_t n = tui_main_rows(u) > SEL_PATTERN_ROWS ? SEL_PATTERN_ROWS : tui_main_rows(u);
    uint32_t row = s->song.pattern_pos.row, lo;
    if (u->pat_asked && u->pat_order == s->song.pattern_pos.order && u->pat_row == row &&
        u->pat_qid == s->song.queue_id) {
        return;
    }
    lo = row > n / 2u ? row - n / 2u : 0;
    if (sel_engine_pattern_request(u->engine, s->song.pattern_pos.order, lo, n) != 0) {
        u->pat_asked = 1;
        u->pat_order = s->song.pattern_pos.order;
        u->pat_row = row;
        u->pat_qid = s->song.queue_id;
    }
}

/* I192 (F24): the statistics screen. Renders the "label\0value\0" rows the engine read from the stats views;
 * no number is computed here. */
static void tui_stats_pane(tui* u, const sel_state* s, uint32_t width) {
    const char* p = s->stats;
    uint32_t y = 3;
    tui_ptext(u, 1, 2, "Statistics", width - 1u, tui_style(u, TXT_ROLE_LABEL));
    if (*p == '\0') {
        tui_ptext(u, 1, 3, "No statistics available", width - 1u, tui_style(u, TXT_ROLE_DIM));
        return;
    }
    while (*p != '\0' && y < u->grid.rows - 1u) {
        const char* value = p + strlen(p) + 1;
        tui_ptext(u, 1, y, p, 22u, tui_style(u, TXT_ROLE_LABEL));
        tui_ptext(u, 24, y, value, width > 25u ? width - 25u : 1u, tui_style(u, TXT_ROLE_VALUE));
        y++;
        p = value + strlen(value) + 1;
    }
}

/* Row I355: the start screen is the whole frame until a key or a song ends it. */
static void tui_splash_draw(tui* u) {
    uint32_t y, x0 = (u->grid.cols - TUI_SPLASH_COLS) / 2u, y0 = (u->grid.rows - TUI_SPLASH_ROWS) / 2u;
    txt_style st = tui_style(u, TXT_ROLE_NORMAL);
    txt_grid_clear(&u->grid, st);
    for (y = 0; y < TUI_SPLASH_ROWS; y++) {
        const unsigned char* p = (const unsigned char*)tui_splash[y];
        uint32_t x;
        for (x = 0; p[x] != '\0'; x++) {
            if (p[x] != ' ') {
                txt_grid_put(&u->grid, x0 + x, y0 + y, p[x], st); /* a Latin-1 byte is its own code point */
            }
        }
    }
}

/* I1222h: the door the open page belongs to, as the door table names its page (the sidebar selection). */
static const char* tui_door_page(const tui* u, const sel_state* s) {
    const sel_view_state* v = s->view;
    if (u->page == TUI_INFO_NOWPLAYING) {
        return "nowplaying";
    }
    if (u->page == TUI_INFO_SONGINFO) {
        return "songinfo";
    }
    if (v != NULL && v->source == SEL_VIEW_QUEUE) {
        return "queue";
    }
    if (v != NULL && v->source == SEL_VIEW_HOME) {
        return "home";
    }
    return v != NULL && v->source == SEL_VIEW_FOLDERS ? "folders" : "";
}

/* I1222h: the sidebar region, drawn from the shared side stack (sel_uim_side_stack over the door table, text spec: every row one cell)
 * with the filter chips wrapped by sel_uim_chips_flow. The icon rail gives one letter per door. The last column is the separator. */
static void tui_sidebar(tui* u, const sel_state* s, const sel_uim_region* r, int rail, int top_bar) {
    sel_uim_side_item it[16];
    sel_uim_side_spec sp;
    int32_t cw[SEL_UIM_LFILTERS], cx[SEL_UIM_LFILTERS], crow[SEL_UIM_LFILTERS];
    uint32_t n, i, k, x0 = (uint32_t)r->x, w = r->w > 1u ? r->w - 1u : r->w, nch = SEL_UIM_LFILTERS - 1u;
    const char* open = tui_door_page(u, s);
    const sel_view_state* v = s->view;
    txt_style nm = tui_style(u, TXT_ROLE_NORMAL), dim = tui_style(u, TXT_ROLE_DIM), lab = tui_style(u, TXT_ROLE_LABEL);
    txt_grid_fill(&u->grid, x0, (uint32_t)r->y, w, r->h, ' ', nm);
    for (i = 0; i < r->h; i++) {
        txt_grid_put(&u->grid, x0 + w, (uint32_t)r->y + i, 0x2502u, tui_style(u, TXT_ROLE_BORDER));
    }
    memset(&sp, 0, sizeof sp);
    sp.brand_h = sp.door_h = sp.libhdr_h = sp.chip_row_h = sp.list_row_h = 1;
    sp.brand_min_w = 12;
    sp.rail = rail;
    sp.top_bar = top_bar;
    for (k = 0; k < nch; k++) {
        cw[k] = (int32_t)strlen(sel_uim_lfilter_label(k + 1u)) + 2;
    }
    sp.chip_rows = w > 2u ? sel_uim_chips_flow(cw, nch, 1, (int32_t)w - 2, cx, crow) : 0;
    n = sel_uim_side_stack(&sp, (int32_t)w, (int32_t)r->h, it, 16u);
    for (i = 0; i < n; i++) {
        uint32_t y = (uint32_t)r->y + (uint32_t)it[i].y;
        if (it[i].kind == SEL_UIM_SIDE_BRAND) {
            txt_grid_text(&u->grid, x0 + 1u, y, "Selecta", w - 1u, lab);
        } else if (it[i].kind == SEL_UIM_SIDE_DOOR) {
            const sel_uim_door* d = sel_uim_door_at((uint32_t)it[i].door);
            txt_style st = strcmp(d->page, open) == 0 ? tui_style(u, TXT_ROLE_CURSOR) : nm;
            txt_grid_fill(&u->grid, x0, y, w, 1u, ' ', st);
            if (rail) { /* one letter per door; Song Information is I (S is Search) */
                char l[2];
                l[0] = strcmp(d->page, "songinfo") == 0 ? 'I' : d->label[0];
                l[1] = '\0';
                txt_grid_text(&u->grid, x0 + 1u, y, l, 1u, st);
            } else {
                txt_grid_text(&u->grid, x0 + 1u, y, d->label, w - 1u, st);
            }
        } else if (it[i].kind == SEL_UIM_SIDE_LIBHDR) {
            txt_grid_text(&u->grid, x0 + 1u, y, "Your Library", w - 1u, lab);
            if (w >= 18u) {
                txt_grid_text(&u->grid, x0 + w - 4u, y, "[+]", 3u, dim);
            }
        } else if (it[i].kind == TUI_SIDE_KIND_CHIPS) {
            for (k = 0; k < nch; k++) {
                char c[32];
                snprintf(c, sizeof c, "[%s]", sel_uim_lfilter_label(k + 1u));
                txt_grid_text(&u->grid, x0 + 1u + (uint32_t)cx[k], y + (uint32_t)crow[k], c, (uint32_t)cw[k], dim);
            }
        } else if (it[i].kind == SEL_UIM_SIDE_LIST && v != NULL) { /* the pane's own list data, with its cursor */
            uint32_t j;
            for (j = 0; j < v->row_n && j < (uint32_t)it[i].h; j++) {
                int cur = v->window_start + j == v->cursor;
                txt_style st = cur ? tui_style(u, TXT_ROLE_CURSOR) : nm;
                if (cur) {
                    txt_grid_fill(&u->grid, x0, y + j, w, 1u, ' ', st);
                }
                txt_grid_text(&u->grid, x0 + 1u, y + j, v->rows[j].text, w - 1u, st);
            }
        }
    }
}

static void tui_draw(tui* u) {
    const sel_state* s = sel_engine_state(u->engine);
    sel_uim_layout lay;
    const sel_uim_region *mr, *pr, *sr;
    uint32_t width, y, my = 0, mh = 0;
    if (s->song.path != NULL) {
        u->splash_off = 1;
    }
    u->splash_shown = u->splash_wanted && !u->splash_off && u->grid.cols >= TUI_SPLASH_COLS + 1u &&
                      u->grid.rows >= TUI_SPLASH_ROWS;
    if (u->splash_shown) {
        tui_splash_draw(u);
        return;
    }
    txt_grid_clear(&u->grid, tui_style(u, TXT_ROLE_NORMAL));
    (void)sel_uim_update(&u->uim, s);
    tui_layout(u, &lay);
    mr = u->layout == SEL_UIM_LAYOUT_MINI ? NULL : tui_region(&lay, "main");
    pr = tui_region(&lay, "player");
    sr = tui_region(&lay, "sidebar");
    if (mr != NULL) {
        my = (uint32_t)mr->y;
        mh = mr->h;
        /* the page view: its row 2 is the main region's first row; the pages keep their row numbers (I1222i makes them region-relative) */
        u->pv_x0 = (uint32_t)mr->x;
        u->pv_y0 = my;
        u->pv_rows = mh;
    }
    /* I1222h: pages draw inside the main region, their column 0 is its first column */
    width = mr == NULL ? u->grid.cols : mr->w;
    if (mr != NULL && u->info && u->layout == SEL_UIM_LAYOUT_FULL && mr->w > TUI_INFO_COLS + 20u) {
        width = mr->w - TUI_INFO_COLS;
    }
    if (mr == NULL) {
        ; /* the player bar alone */
    } else if (s->stats != NULL) {
        tui_stats_pane(u, s, width);
    } else if (u->page == TUI_INFO_NOWPLAYING) {
        tui_nowplaying_page(u, s);
    } else if (u->page == TUI_INFO_SONGINFO) {
        tui_songinfo_page(u, s);
    } else if (u->pattern) {
        tui_pattern_panel(u, s, width);
    } else if (u->viz) {
        tui_viz_panel(u, s, width);
    } else if (u->pg != TUI_PAGE_NONE) {
        tui_page_draw(u, s, width);
    } else {
        tui_pane(u, s, width);
    }
    if (mr != NULL && width < mr->w && u->page == TUI_INFO_NONE) {
        for (y = 2; y < mh + 2u; y++) {
            tui_pput(u, width, y, 0x2502u, tui_style(u, TXT_ROLE_BORDER));
        }
        tui_info(u, s, width + 2u, 2u);
    }
    if (u->overlay == 3) {
        tui_settings(u);
    } else if (u->overlay == 4) {
        tui_menu_draw(u);
    } else if (u->overlay) {
        tui_overlay(u);
    }
    if (sr != NULL && u->layout != SEL_UIM_LAYOUT_MINI) {
        tui_sidebar(u, s, sr, lay.breakpoint == SEL_UIM_BP_ICON_SIDEBAR, lay.topbar.h != 0u);
        u->sidebar_draws++;
    }
    if (lay.topbar.h != 0u) {
        tui_topbar(u, (uint32_t)lay.topbar.y);
        u->topbar_draws++;
    }
    if (pr != NULL) {
        tui_player_bar(u, (uint32_t)pr->y, pr->h);
        u->player_draws++;
        u->player_rows = pr->h;
    }
    /* I1222g: no hint row. The command line and the status text open on demand over the bottom row of the main region. */
    if (mr != NULL && (u->cmd_mode || u->cmd.status[0] != '\0')) {
        char line[TXT_CMD_LINE + TXT_CMD_STATUS + 8];
        uint32_t ly = my + mh - 1u;
        if (u->cmd_mode) {
            if (u->cmd_mode == 4) {
                if (u->mn_pending == (uint32_t)MN_QUIT) {
                    char hint[48];
                    sel_uim_dlg_hint(SEL_UIM_DLG_QUIT, hint, sizeof hint);
                    snprintf(line, sizeof line, "%s  %s", u->mn_lab[0], hint);
                } else {
                    snprintf(line, sizeof line, "%s  y Yes  any other key No", u->mn_lab[0]);
                }
            } else if (u->cmd_mode == 5) {
                snprintf(line, sizeof line, "Note: %s%s%s", txt_cmdline_text(&u->cmd), u->cmd.status[0] ? "   " : "", u->cmd.status);
            } else if (u->cmd_mode == 3) {
                snprintf(line, sizeof line, "%s name: %s", u->mn_pending == (uint32_t)MN_RENAME ? "Rename" : u->mn_pending == (uint32_t)MN_NEWFOLDER ? "New Folder" : "New Playlist", txt_cmdline_text(&u->cmd));
            } else {
                snprintf(line, sizeof line, "%c%s%s%s", u->cmd_mode == 2 ? '/' : ':', txt_cmdline_text(&u->cmd), u->cmd.status[0] ? "   " : "", u->cmd.status);
            }
        } else {
            snprintf(line, sizeof line, "%s", u->cmd.status);
        }
        txt_grid_fill(&u->grid, (uint32_t)mr->x, ly, mr->w, 1, ' ', tui_style(u, TXT_ROLE_NORMAL));
        txt_grid_text(&u->grid, (uint32_t)mr->x + 1u, ly, line, mr->w - 1u, tui_style(u, TXT_ROLE_NORMAL));
    }
}

static size_t tui_utf8(uint32_t cp, char* o) {
    if (cp < 0x80u) {
        o[0] = (char)cp;
        return 1;
    }
    if (cp < 0x800u) {
        o[0] = (char)(0xC0u | (cp >> 6));
        o[1] = (char)(0x80u | (cp & 0x3Fu));
        return 2;
    }
    if (cp < 0x10000u) {
        o[0] = (char)(0xE0u | (cp >> 12));
        o[1] = (char)(0x80u | ((cp >> 6) & 0x3Fu));
        o[2] = (char)(0x80u | (cp & 0x3Fu));
        return 3;
    }
    o[0] = (char)(0xF0u | (cp >> 18));
    o[1] = (char)(0x80u | ((cp >> 12) & 0x3Fu));
    o[2] = (char)(0x80u | ((cp >> 6) & 0x3Fu));
    o[3] = (char)(0x80u | (cp & 0x3Fu));
    return 4;
}

/* Writes the shown frame as plain text: UTF-8, right-trimmed lines. */
static void tui_dump(const tui* u) {
    FILE* f = fopen(u->dump_path, "wb");
    uint32_t y, x, last;
    char o[4];
    if (f == NULL) {
        return;
    }
    for (y = 0; y < u->grid.rows; y++) {
        last = 0;
        for (x = 0; x < u->grid.cols; x++) {
            if (u->grid.back[y * u->grid.cols + x].cp != ' ') {
                last = x + 1u;
            }
        }
        for (x = 0; x < last; x++) {
            fwrite(o, 1, tui_utf8(u->grid.back[y * u->grid.cols + x].cp, o), f);
        }
        fputc('\n', f);
    }
    fclose(f);
}

static void tui_present(tui* u) {
    long n;
    tui_draw(u);
    n = txt_grid_flush(&u->grid, u->out, u->out_cap);
    if (n > 0) {
        long b;
        u->frames++;
        u->splash_frames += (uint32_t)u->splash_shown;
        for (b = 0; b < n; b++) {
            u->hash = (u->hash ^ (uint8_t)u->out[b]) * 16777619u;
        }
        if (u->frame_dump_path != NULL) {
            FILE* f = fopen(u->frame_dump_path, "ab");
            if (f != NULL) {
                fwrite(u->out, 1, (size_t)n, f);
                fclose(f);
            }
        }
    }
    if (n > 0 && u->have_term) {
        txt_term_write(&u->term, u->out, (size_t)n);
    }
    if (u->dump_path != NULL) {
        tui_dump(u);
    }
    u->dirty = 0;
}

/* ---- plugin hooks ---- */

static const char* tui_opt(const sel_ui_open_args* a, const char* name) {
    int i;
    for (i = 0; i + 1 < a->argc; i++) {
        if (strcmp(a->argv[i], name) == 0) {
            return a->argv[i + 1];
        }
    }
    return NULL;
}

static void tui_close(void* state) {
    tui* u = (tui*)state;
    const sel_host* host;
    if (u == NULL) {
        return;
    }
    host = u->host;
    if (u->have_term) {
        txt_term_close(&u->term);
    }
    txt_grid_free(&u->grid);
    if (u->out != NULL) {
        host->mem_free(host, u->out);
    }
    if (u->key_text != NULL) {
        host->mem_free(host, u->key_text);
    }
    if (u->brf.buf != NULL) {
        host->mem_free(host, u->brf.buf);
    }
    host->mem_free(host, u);
}

/* Defaults, then keys.conf from the config directory (--config-dir, else
 * sel_config_dir), then --bindings <file>. A scripted run (--keys) skips the
 * user's keys.conf so a test never depends on the machine it runs on. A named
 * --bindings file that cannot be read fails the open; a missing keys.conf is
 * normal. Bad lines are reported on stderr and skipped. */
static int tui_load_bindings(tui* u, const sel_ui_open_args* a, int scripted) {
    const char* explicit_path = tui_opt(a, "--bindings");
    const char* cfg = tui_opt(a, "--config-dir");
    char path[512], dir[256];
    char* text;
    uint32_t first_bad = 0, bad;
    txt_bindings_default(&u->binds);
    if (explicit_path == NULL && !scripted) {
        if (cfg == NULL && sel_config_dir(dir, sizeof dir)) {
            cfg = dir;
        }
        if (cfg == NULL || snprintf(path, sizeof path, "%s/keys.conf", cfg) >= (int)sizeof path) {
            return 1;
        }
        explicit_path = path;
        text = tui_slurp(a->host, path);
        if (text == NULL) {
            return 1;
        }
    } else if (explicit_path != NULL) {
        text = tui_slurp(a->host, explicit_path);
        if (text == NULL) {
            fprintf(stderr, "selecta: cannot read bindings file %s\n", explicit_path);
            return 0;
        }
    } else {
        return 1;
    }
    bad = txt_bindings_load(&u->binds, text, &first_bad);
    if (bad != 0) {
        fprintf(stderr, "selecta: %s: %u bad line(s), first at line %u\n", explicit_path, (unsigned)bad,
                (unsigned)first_bad);
    }
    a->host->mem_free(a->host, text);
    return 1;
}

static void* tui_open(const sel_ui_open_args* a) {
    tui* u = (tui*)a->host->mem_alloc(a->host, sizeof *u, SEL_MEM_FAST);
    const char* keys_path = tui_opt(a, "--keys");
    uint32_t cols = TUI_DEFAULT_COLS, rows = TUI_DEFAULT_ROWS;
    if (u == NULL) {
        return NULL;
    }
    memset(u, 0, sizeof *u);
    u->engine = a->engine;
    u->host = a->host;
    u->splash_wanted = 1; /* a scripted --exec run starts on the player, not the start screen (row I355) */
    {
        int i;
        for (i = 0; i < a->argc; i++) {
            if (strcmp(a->argv[i], "--exec") == 0) {
                u->splash_wanted = 0;
            }
        }
    }
    u->dump_path = tui_opt(a, "--screen-dump");
    u->frame_dump_path = tui_opt(a, "--frame-dump");
    txt_theme_default(&u->theme);
    txt_keys_init(&u->keys);
    u->keys.amiga_console = txt_term_amiga_console;
    u->keys.latin1 = txt_term_latin1();
    if (!tui_load_bindings(u, a, keys_path != NULL)) {
        tui_close(u);
        return NULL;
    }
    if (keys_path != NULL) {
        u->key_text = tui_slurp(a->host, keys_path);
        if (u->key_text == NULL) {
            tui_close(u);
            return NULL;
        }
        u->use_keys_file = 1;
    } else if (tui_opt(a, "--size") != NULL) {
        /* Headless: a fixed grid and keys read from stdin, so a run is the same on every OS (tests). */
        if (sscanf(tui_opt(a, "--size"), "%ux%u", &cols, &rows) != 2 || cols == 0 || rows == 0) {
            tui_close(u);
            return NULL;
        }
        u->headless = 1;
        u->term.in_fd = 0;
        u->term.out_fd = 1;
    } else if (txt_term_open(&u->term, 0, 1) == 0) {
        u->have_term = 1;
        txt_term_size(&u->term, &cols, &rows);
    } else {
        tui_close(u);
        return NULL;
    }
    if (txt_grid_init(&u->grid, cols, rows) != 0) {
        tui_close(u);
        return NULL;
    }
    sel_uim_init(&u->uim);
    u->grid.latin1 = txt_term_latin1();
    /* --console-frames 1 gives a host run the Amiga console's frames, so tests can compare byte streams (row I218e). */
    u->grid.console = txt_term_amiga_console || tui_opt(a, "--console-frames") != NULL;
    if (tui_opt(a, "--console-frames") != NULL) {
        u->grid.latin1 = 1; /* the Amiga console is Latin-1, so the start screen's high bytes match byte for byte (row I355) */
    }
    u->out_cap = txt_grid_flush_bound(&u->grid);
    u->out = (char*)a->host->mem_alloc(a->host, u->out_cap, SEL_MEM_FAST);
    if (u->out == NULL) {
        tui_close(u);
        return NULL;
    }
    u->dirty = 1;
    u->hash = 2166136261u;
    tui_send_window(u);
    return u;
}

static void tui_on_event(void* state, const sel_message* evt) {
    tui* u = (tui*)state;
    if (evt->id == SEL_EVT_ERROR && evt->argc > 0) {
        /* I195g: an engine error (for example "open_failed http_status=404 <url>") shows on the status line. */
        snprintf(u->cmd.status, sizeof u->cmd.status, "Error: %s%s%s", evt->args[0].v.s, evt->argc > 1 ? " " : "",
                 evt->argc > 1 ? evt->args[1].v.s : "");
    }
    u->dirty = 1; /* every event can change what the snapshot shows */
}

static int tui_wait(void* state, uint32_t timeout_ms) {
    tui* u = (tui*)state;
    uint8_t buf[64];
    uint32_t cols, rows;
    long n, i;
    txt_key k;
    int handled = 0;
    if (u->viz || u->pattern) {
        u->dirty = 1; /* the panels follow the audio: redraw every turn */
        if (u->pattern) {
            tui_pattern_ask(u);
        }
    }
    tui_briefs_follow(u);
    if (u->dirty) {
        tui_present(u);
    }
    if (u->use_keys_file) {
        int r = tui_file_step(u, timeout_ms);
        return r < 0 || u->quit ? -1 : r;
    }
    n = txt_term_read(&u->term, buf, sizeof buf, (int)timeout_ms);
    if (n < 0) {
        return -1;
    }
    for (i = 0; i < n; i++) {
        if (txt_keys_feed(&u->keys, buf[i], &k)) {
            tui_key(u, &k);
            handled = 1;
        }
    }
    if (n == 0 && txt_keys_timeout(&u->keys, &k)) {
        tui_key(u, &k);
        handled = 1;
    }
    if (u->headless) {
        return u->quit ? -1 : handled;
    }
    txt_term_size(&u->term, &cols, &rows);
    if (cols != u->grid.cols || rows != u->grid.rows) {
        if (tui_resize(u, cols, rows) != 0) {
            return -1;
        }
        u->dirty = 1;
    }
    return u->quit ? -1 : handled;
}

static const sel_tag* tui_stats(void* state) {
    tui* u = (tui*)state;
    u->stats[0].tag = SEL_UI_STAT_FLUSHES;
    u->stats[0].value = u->grid.flushes;
    u->stats[1].tag = SEL_UI_STAT_KEYS;
    u->stats[1].value = u->key_count;
    u->stats[2].tag = SEL_UI_STAT_CMDLINE_RUNS;
    u->stats[2].value = u->cmdline_runs;
    u->stats[3].tag = SEL_UI_STAT_PALETTE_OPENS;
    u->stats[3].value = u->palette_opens;
    u->stats[4].tag = SEL_UI_STAT_BINDINGS;
    u->stats[4].value = u->binds.count;
    u->stats[5].tag = SEL_UI_STAT_VIZ_FRAMES;
    u->stats[5].value = u->viz_frames;
    u->stats[6].tag = SEL_UI_STAT_SCOPE_READS;
    u->stats[6].value = u->scope_reads;
    u->stats[7].tag = SEL_UI_STAT_PATTERN_ROWS;
    u->stats[7].value = u->pattern_rows;
    u->stats[8].tag = SEL_UI_STAT_DOT_ROWS;
    u->stats[8].value = u->dot_rows;
    u->stats[9].tag = SEL_UI_STAT_PHASE_POINTS;
    u->stats[9].value = u->phase_points;
    u->stats[10].tag = SEL_UI_STAT_FRAMES;
    u->stats[10].value = u->frames;
    u->stats[11].tag = SEL_UI_STAT_HASH;
    u->stats[11].value = u->hash;
    u->stats[12].tag = SEL_UI_STAT_LIBRARY_OPENS;
    u->stats[12].value = u->lib_opens;
    u->stats[13].tag = SEL_UI_STAT_LIBRARY_SEARCHES;
    u->stats[13].value = u->lib_searches;
    u->stats[14].tag = SEL_UI_STAT_FOLDER_OPENS;
    u->stats[14].value = u->folder_opens;
    u->stats[15].tag = SEL_UI_STAT_META_PANEL_ROWS;
    u->stats[15].value = u->meta_panel_rows;
    u->stats[16].tag = SEL_UI_STAT_SPLASH_FRAMES;
    u->stats[16].value = u->splash_frames;
    u->stats[17].tag = SEL_UI_STAT_TABLE_ACTIONS;
    u->stats[17].value = u->table_actions;
    u->stats[18].tag = SEL_UI_STAT_TABLE_ACTION_LOG;
    u->stats[18].value = (uintptr_t)u->table_log;
    u->stats[19].tag = SEL_UI_STAT_LAYOUT;
    u->stats[19].value = u->layout_sets << 8 | u->layout;
    u->stats[20].tag = SEL_UI_STAT_SID_FRAMES;
    u->stats[20].value = u->sid_frames;
    u->stats[21].tag = SEL_UI_STAT_STYLE;
    u->stats[21].value = u->style_draws << 8 | u->style_drawn;
    u->stats[22].tag = SEL_UI_STAT_LIBMODE;
    u->stats[22].value = u->scope_keys << 16 | u->mode_keys << 8 | (u->lib_mode_on ? u->lib_mode : 15u) << 4 |
                         sel_uim_scope_index(sel_engine_state(u->engine)->options[SEL_OPT_SCOPE]);
    u->stats[23].tag = SEL_UI_STAT_SETTINGS;
    u->stats[23].value = u->set_focus << 16 | u->set_opens << 8 | u->set_sets;
    u->stats[24].tag = SEL_UI_STAT_PAGES;
    u->stats[24].value = (u->pg_opens_shelf & 0xFFu) | (u->pg_opens_coll & 0xFFu) << 8 | (u->pg_rows & 0xFFu) << 16 | ((uint32_t)u->pg & 0xFu) << 24;
    u->stats[25].tag = SEL_UI_STAT_MENU;
    u->stats[25].value = (u->mn_opens & 0xFFu) | (u->mn_picks & 0xFFu) << 8 | (u->mn_answers & 0xFFu) << 16;
    u->stats[26].tag = SEL_UI_STAT_MENU_LOG;
    u->stats[26].value = (uintptr_t)u->mn_log;
    u->stats[27].tag = SEL_UI_STAT_SONGINFO_ACTIONS;
    u->stats[27].value = u->si_actions;
    u->stats[28].tag = SEL_UI_STAT_NOWPLAY_DRAWS;
    u->stats[28].value = u->np_draws;
    u->stats[29].tag = SEL_UI_STAT_PLAYER_BAR;
    u->stats[29].value = (u->player_draws & 0xFFFFu) | (u->player_rows << 16);
    u->stats[30].tag = SEL_UI_STAT_TOPBAR_DRAWS;
    u->stats[30].value = u->topbar_draws;
    u->stats[31].tag = SEL_UI_STAT_SIDEBAR_DRAWS;
    u->stats[31].value = u->sidebar_draws;
    u->stats[32].tag = SEL_TAG_END;
    u->stats[32].value = 0;
    return u->stats;
}

/* Filled at run time: a function pointer cast to an integer is not a
 * constant expression in C99. */
static sel_tag g_hooks[10];
static sel_tag g_desc[5];

static void set(sel_tag* t, uint32_t tag, uintptr_t value) {
    t->tag = tag;
    t->value = value;
}

const sel_tag* SEL_PLUGIN_ENTRY(uint32_t host_api) {
    if (host_api < 1u) {
        return NULL;
    }
    set(&g_hooks[0], SEL_UI_NAME, (uintptr_t) "Text");
    set(&g_hooks[1], SEL_UI_OPEN, SEL_TAG_FN(tui_open));
    set(&g_hooks[2], SEL_UI_ON_EVENT, SEL_TAG_FN(tui_on_event));
    set(&g_hooks[3], SEL_UI_WAIT, SEL_TAG_FN(tui_wait));
    set(&g_hooks[4], SEL_UI_CLOSE, SEL_TAG_FN(tui_close));
    set(&g_hooks[5], SEL_UI_STATS, SEL_TAG_FN(tui_stats));
    set(&g_hooks[6], SEL_UI_VIEW_STYLES, SEL_VIEW_STYLES_ALL);
    set(&g_hooks[7], SEL_TAG_END, 0);

    set(&g_desc[0], SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_UI);
    set(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "text");
    set(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    set(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    set(&g_desc[4], SEL_TAG_END, 0);
    return g_desc;
}

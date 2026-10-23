/* Text UI plugin (design doc section 7, plan M1.1 phase 5, row I60): renders
 * sel_engine_state() into the cell grid and turns keys into engine commands.
 * It holds no playback logic. Input comes from the terminal, or from a keys
 * file (--keys) so a test drives the whole UI with no terminal; --screen-dump
 * writes the last frame as UTF-8 text. */
#include <selecta/engine.h>
#include <selecta/host.h>
#include <selecta/plugin.h>
#include <selecta/protocol.h>
#include <selecta/ui.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "grid.h"
#include "keybind.h"
#include "keys.h"
#include "term.h"
#include "theme.h"

#define TUI_POOLS 4u
#define TUI_POOL_BYTES 1024u
#define TUI_DEFAULT_COLS 80u
#define TUI_DEFAULT_ROWS 24u
#define TUI_INFO_COLS 30u
#define TUI_CHROME_ROWS 4u /* two header lines, pane title, status line */
#define TUI_SEEK_MS 5000
#define TUI_VOLUME_STEP 5

typedef struct tui {
    sel_engine* engine;
    const sel_host* host;
    txt_grid grid;
    txt_theme theme;
    txt_keys keys;
    txt_bindings binds;
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
    int dirty;
    int info;
    int quit;
    uint32_t key_count;
    uint32_t pool_next;
    char pool[TUI_POOLS][TUI_POOL_BYTES];
    sel_tag stats[3];
} tui;

/* ---- commands ---- */

/* Sends one command line. String arguments live in a rotating pool until the
 * shell's next pump (the engine does not copy them). */
static void tui_send(tui* u, const char* line) {
    char* pool = u->pool[u->pool_next];
    u->pool_next = (u->pool_next + 1u) % TUI_POOLS;
    sel_engine_send_line(u->engine, line, pool, TUI_POOL_BYTES);
}

static void tui_sendf(tui* u, const char* fmt, const char* a) {
    char line[512];
    snprintf(line, sizeof line, fmt, a);
    tui_send(u, line);
}

static uint32_t tui_main_rows(const tui* u) {
    return u->grid.rows > TUI_CHROME_ROWS ? u->grid.rows - TUI_CHROME_ROWS : 1u;
}

static void tui_send_window(tui* u) {
    char line[48];
    snprintf(line, sizeof line, "VIEW_WINDOW %u", (unsigned)tui_main_rows(u));
    tui_send(u, line);
}

/* ---- keys ---- */

static void tui_cursor(tui* u, const char* where) {
    tui_sendf(u, "VIEW_CURSOR %s", where);
}

static void tui_add_cursor_file(tui* u) {
    const sel_view_state* v = sel_engine_state(u->engine)->view;
    uint32_t i;
    char path[512];
    if (v == NULL || v->source != SEL_VIEW_BROWSE || v->browse_path == NULL || v->cursor < v->window_start) {
        return;
    }
    i = v->cursor - v->window_start;
    if (i >= v->row_n || v->rows[i].kind != SEL_ROW_FILE) {
        return;
    }
    snprintf(path, sizeof path, "%s/%s", v->browse_path, v->rows[i].text);
    tui_sendf(u, "QUEUE_ADD %s", path);
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
    return v != NULL && v->source == SEL_VIEW_QUEUE;
}

/* v: regular -> tree -> drilldown -> regular. */
static void tui_next_style(tui* u) {
    const sel_view_state* v = sel_engine_state(u->engine)->view;
    static const char* const next[] = {"VIEW_STYLE tree", "VIEW_STYLE drilldown", "VIEW_STYLE regular"};
    if (tui_queue_view(v) && v->style <= SEL_VIEW_STYLE_DRILLDOWN) {
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

/* UI-local actions (`@name` in the binding table). Returns 0 for an unknown name. */
static int tui_local(tui* u, const char* name, const txt_key* k) {
    const sel_state* s = sel_engine_state(u->engine);
    char line[48];
    (void)k;
    if (strcmp(name, "page_up") == 0 || strcmp(name, "page_down") == 0) {
        snprintf(line, sizeof line, "%c%u", name[5] == 'u' ? '-' : '+', (unsigned)tui_main_rows(u));
        tui_cursor(u, line);
    } else if (strcmp(name, "next_style") == 0) {
        tui_next_style(u);
    } else if (strcmp(name, "next_group") == 0) {
        tui_next_group(u);
    } else if (strcmp(name, "back") == 0) { /* the parent directory is always the first browse row */
        if (tui_drill_back(u, s->view)) {
            return 1;
        }
        if (s->view != NULL && s->view->source == SEL_VIEW_BROWSE) {
            tui_cursor(u, "top");
            tui_send(u, "VIEW_ACTIVATE");
        }
    } else if (strcmp(name, "switch_view") == 0) {
        tui_send(u, s->view != NULL && s->view->source == SEL_VIEW_BROWSE ? "VIEW_SHOW queue" : "VIEW_SHOW browse");
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
    } else if (strcmp(name, "quit") == 0) {
        u->quit = 1;
    } else {
        return 0;
    }
    return 1;
}

static void tui_key(tui* u, const txt_key* k) {
    const char* action = txt_bindings_lookup(&u->binds, k->code, k->mods);
    if (action == NULL) {
        return; /* not a key the UI knows */
    }
    u->key_count++;
    u->dirty = 1;
    if (action[0] == '@') {
        (void)tui_local(u, action + 1, k);
    } else {
        tui_send(u, action);
    }
}

/* ---- keys file ---- */

static const struct {
    const char* name;
    const char* bytes;
} k_tokens[] = {
    {"Up", "\x1b[A"},   {"Down", "\x1b[B"},  {"Right", "\x1b[C"},    {"Left", "\x1b[D"}, {"Home", "\x1b[H"},
    {"End", "\x1b[F"},  {"PageUp", "\x1b[5~"}, {"PageDown", "\x1b[6~"}, {"Enter", "\r"},   {"Tab", "\t"},
    {"Backspace", "\x7f"}, {"Escape", "\x1b"}, {"Space", " "},
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
    char line[64];
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

static void tui_clock(char* out, size_t cap, uint32_t ms, int known) {
    if (!known) {
        snprintf(out, cap, "--:--");
    } else {
        snprintf(out, cap, "%02u:%02u", (unsigned)(ms / 60000u), (unsigned)((ms / 1000u) % 60u));
    }
}

static const char* tui_play_word(sel_play_state p) {
    return p == SEL_PLAYING ? "Playing" : p == SEL_PAUSED ? "Paused" : "Stopped";
}

static void tui_header(tui* u, const sel_state* s) {
    char buf[256], pos[16], dur[16];
    const char* title = (const char*)sel_tag_find(s->song.meta, SEL_META_TITLE, 0);
    const char* author = (const char*)sel_tag_find(s->song.meta, SEL_META_AUTHOR, 0);
    const char* format = (const char*)sel_tag_find(s->song.meta, SEL_META_FORMAT, 0);
    txt_style hs = tui_style(u, TXT_ROLE_HEADER);
    txt_grid_fill(&u->grid, 0, 0, u->grid.cols, 1, ' ', hs);
    snprintf(buf, sizeof buf, "%s  %s  %s", tui_play_word(s->play), title != NULL && title[0] ? title : "-",
             author != NULL ? author : "");
    txt_grid_text(&u->grid, 1, 0, buf, u->grid.cols - 1u, hs);
    tui_clock(pos, sizeof pos, s->song.position_ms, s->song.path != NULL);
    tui_clock(dur, sizeof dur, s->song.duration_ms, s->song.duration_ms != 0);
    snprintf(buf, sizeof buf, "%s  Subsong %u/%u  %s / %s  Volume %d", format != NULL && format[0] ? format : "-",
             (unsigned)(s->song.path != NULL ? s->song.subsong + 1u : 0u), (unsigned)s->song.subsong_count, pos, dur,
             (int)s->volume);
    txt_grid_text(&u->grid, 1, 1, buf, u->grid.cols - 1u, tui_style(u, TXT_ROLE_NORMAL));
}

static const char* tui_leaf(const char* path) {
    const char* slash = path != NULL ? strrchr(path, '/') : NULL;
    return slash != NULL && slash[1] != '\0' ? slash + 1 : (path != NULL ? path : "");
}

/* Queue heading: the entry count, or the grouping key and the drilldown
 * breadcrumb of picked values. */
static void tui_queue_title(char* buf, size_t cap, const sel_view_state* v) {
    uint32_t i;
    size_t n;
    if (v->style == SEL_VIEW_STYLE_REGULAR || v->group == NULL) {
        snprintf(buf, cap, "Queue  %u entries", (unsigned)v->row_count);
        return;
    }
    snprintf(buf, cap, "Queue by %s%s", v->group, v->style == SEL_VIEW_STYLE_TREE ? "  Tree" : "  Drilldown");
    for (i = 0; v->style == SEL_VIEW_STYLE_DRILLDOWN && i < v->drill_n && i < 2u; i++) {
        n = strlen(buf);
        snprintf(buf + n, cap - n, " > %s", v->drill[i]);
    }
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
        snprintf(buf, sizeof buf, "Browse  %s", tui_leaf(v->browse_path));
    } else {
        tui_queue_title(buf, sizeof buf, v);
    }
    txt_grid_text(&u->grid, 1, 2, buf, width - 1u, title);
    for (i = 0; i < v->row_n && i < tui_main_rows(u); i++) {
        const sel_view_row* r = &v->rows[i];
        uint32_t y = 3u + i;
        int at_cursor = v->window_start + i == v->cursor;
        int playing = r->kind == SEL_ROW_QUEUE && r->id == s->queue_playing;
        txt_role role = at_cursor ? TXT_ROLE_CURSOR
                        : playing ? TXT_ROLE_PLAYING
                        : r->kind == SEL_ROW_DIR || r->kind == SEL_ROW_NODE ? TXT_ROLE_DIRECTORY
                                                 : TXT_ROLE_FILE;
        txt_style st = tui_style(u, role);
        if (r->kind == SEL_ROW_NODE) {
            snprintf(buf, sizeof buf, "%s %s (%u)", v->style == SEL_VIEW_STYLE_TREE ? (r->expanded ? "[-]" : "[+]") : "[>]",
                     r->text, (unsigned)r->count);
        } else {
            snprintf(buf, sizeof buf, "%*s%c %s%s", (int)(2u * r->depth), "", playing ? '>' : ' ', r->text,
                     r->kind == SEL_ROW_DIR && strcmp(r->text, "..") != 0 ? "/" : "");
        }
        txt_grid_fill(&u->grid, 0, y, width, 1, ' ', st);
        txt_grid_text(&u->grid, 1, y, buf, width - 1u, st);
    }
}

/* Label for a META tag: META_TITLE becomes Title. */
static void tui_label(uint32_t tag, char* out, size_t cap) {
    const char* name = sel_tag_name(tag);
    size_t i = 0;
    if (name != NULL && strncmp(name, "META_", 5) == 0) {
        name += 5;
    }
    for (; name != NULL && name[i] != '\0' && i + 1 < cap; i++) {
        char c = name[i];
        out[i] = i == 0 ? c : (char)(c >= 'A' && c <= 'Z' ? c + 32 : c);
        if (c == '_') {
            out[i] = ' ';
        }
    }
    out[i] = '\0';
}

/* Every META tag the replayer reported, in tags.def order. */
static void tui_info(tui* u, const sel_state* s, uint32_t x) {
    const uintptr_t absent = (uintptr_t)-1;
    uint32_t tag, y = 3;
    uint32_t w = u->grid.cols - x - 1u;
    txt_grid_text(&u->grid, x, 2, "Information", w, tui_style(u, TXT_ROLE_LABEL));
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
        tui_label(tag, label, sizeof label);
        txt_grid_text(&u->grid, x, y, label, w, tui_style(u, TXT_ROLE_LABEL));
        txt_grid_text(&u->grid, x + 12u, y, val, w > 12u ? w - 12u : 1u, tui_style(u, TXT_ROLE_VALUE));
        y++;
    }
}

static void tui_draw(tui* u) {
    const sel_state* s = sel_engine_state(u->engine);
    uint32_t width = u->info && u->grid.cols > TUI_INFO_COLS + 20u ? u->grid.cols - TUI_INFO_COLS : u->grid.cols;
    uint32_t y;
    txt_grid_clear(&u->grid, tui_style(u, TXT_ROLE_NORMAL));
    tui_header(u, s);
    tui_pane(u, s, width);
    if (width < u->grid.cols) {
        for (y = 2; y + 1u < u->grid.rows; y++) {
            txt_grid_put(&u->grid, width, y, 0x2502u, tui_style(u, TXT_ROLE_BORDER));
        }
        tui_info(u, s, width + 2u);
    }
    txt_grid_fill(&u->grid, 0, u->grid.rows - 1u, u->grid.cols, 1, ' ', tui_style(u, TXT_ROLE_DIM));
    txt_grid_text(&u->grid, 1, u->grid.rows - 1u,
                  "Up Down Move  Enter Open  Backspace Parent  Tab Queue  Space Pause  Left Right Seek  Info  Quit",
                  u->grid.cols - 1u, tui_style(u, TXT_ROLE_DIM));
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
    u->dump_path = tui_opt(a, "--screen-dump");
    txt_theme_default(&u->theme);
    txt_keys_init(&u->keys);
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
    u->out_cap = txt_grid_flush_bound(&u->grid);
    u->out = (char*)a->host->mem_alloc(a->host, u->out_cap, SEL_MEM_FAST);
    if (u->out == NULL) {
        tui_close(u);
        return NULL;
    }
    u->dirty = 1;
    tui_send_window(u);
    return u;
}

static void tui_on_event(void* state, const sel_message* evt) {
    tui* u = (tui*)state;
    (void)evt;
    u->dirty = 1; /* every event can change what the snapshot shows */
}

static int tui_wait(void* state, uint32_t timeout_ms) {
    tui* u = (tui*)state;
    uint8_t buf[64];
    uint32_t cols, rows;
    long n, i;
    txt_key k;
    int handled = 0;
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
    txt_term_size(&u->term, &cols, &rows);
    if (cols != u->grid.cols || rows != u->grid.rows) {
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
    }
    return u->quit ? -1 : handled;
}

static const sel_tag* tui_stats(void* state) {
    tui* u = (tui*)state;
    u->stats[0].tag = SEL_UI_STAT_FLUSHES;
    u->stats[0].value = u->grid.flushes;
    u->stats[1].tag = SEL_UI_STAT_KEYS;
    u->stats[1].value = u->key_count;
    u->stats[2].tag = SEL_TAG_END;
    u->stats[2].value = 0;
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
    set(&g_hooks[6], SEL_UI_VIEW_STYLES, 1);
    set(&g_hooks[7], SEL_TAG_END, 0);

    set(&g_desc[0], SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_UI);
    set(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "text");
    set(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    set(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    set(&g_desc[4], SEL_TAG_END, 0);
    return g_desc;
}

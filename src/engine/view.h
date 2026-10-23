/* View model, regular style (design doc section 9b): one flat list over the
 * queue or the browse listing, a cursor and a window of visible rows. Pure
 * state over the queue and browse buffers; the engine owns commands and
 * events (cmd_VIEW_* in engine.c). The rows point into those buffers, so they
 * are valid until the next pump. The window rows are allocated once. */
#ifndef SELECTA_ENGINE_VIEW_H
#define SELECTA_ENGINE_VIEW_H

#include <selecta/engine.h>
#include <selecta/host.h>

#include "format.h"
#include "local.h"
#include "queue.h"

#define SEL_VIEW_WINDOW_MAX 256u
#define SEL_VIEW_WINDOW_DEFAULT 20u
#define SEL_VIEW_TITLE_DEFAULT "[%author% - ]%title%[  Votes %votes%]"
#define SEL_VIEW_VOTES_MAX 12u /* a vote count as text, NUL included */
#define SEL_VIEW_TEXT_MAX 160u /* one formatted row, NUL included */
#define SEL_VIEW_KEY_MAX 128u    /* bytes of the song key or album name a home row carries (I191a) */
#define SEL_VIEW_FOLDER_ROWS 256u /* rows of the folders source (the playlist folder tree, I179) */
#define SEL_VIEW_FOLDER_NAME 64u

/* The data the view lists; read-only here. */
typedef struct sel_view_src {
    const sel_queue* queue;
    const sel_browse* browse;
    const char* meta;          /* query source only: title, artist, format, duration per row (NULL: paths only) */
    const uint32_t* meta_off;  /* offset in `meta` of row i; UINT32_MAX none */
    uint32_t meta_rows;
    uint32_t playing; /* queue id of the playing entry, 0 none */
    const char* title_format;  /* view.title_format pref, expanded per queue row */
    uint32_t prefs_generation; /* bumps when any pref changes */
    /* Vote count of a queue path (I174a): copies the text into out[SEL_VIEW_VOTES_MAX], 0 when the song has none.
     * NULL: no library. The view calls it once per visible row and again only when votes_generation moves. */
    int (*votes)(void* ctx, const char* path, char* out);
    void* votes_ctx;
    uint32_t votes_generation;
} sel_view_src;

typedef struct sel_view_sig {
    uint32_t source, style, row_count, cursor, window_start, window_rows, queue_version, browse_generation, playing, prefs_generation;
    uint32_t group_key, drill_n, folder_generation, votes_generation;
} sel_view_sig;

/* Tree and drilldown group table (design doc section 3, VIEW_GROUP): the
 * distinct values of the grouping key over the queue, sorted by name. The
 * table is allocated once; a queue with more distinct values than fit folds
 * the rest into one "(more)" group. */
#define SEL_VIEW_GROUPS_MAX 512u
#define SEL_VIEW_GROUP_NAME 96u
typedef struct sel_vgroup {
    uint32_t id;       /* node id: 1.. in order of first appearance */
    uint32_t count;    /* songs in the group (after the drilldown filter) */
    uint32_t base;     /* index of the node row in the whole list */
    uint32_t expanded; /* tree only */
    char name[SEL_VIEW_GROUP_NAME];
} sel_vgroup;

typedef struct sel_view {
    sel_view_state pub;
    sel_view_row* rows;
    char* row_text; /* row_cap * SEL_VIEW_TEXT_MAX: the formatted queue rows */
    char* vote_text;      /* row_cap * SEL_VIEW_VOTES_MAX: the vote count of each row slot (I174a) */
    uint32_t* vote_id;    /* queue entry id each vote_text slot was read for, 0 none */
    uint32_t vote_seen;   /* votes_generation the slots belong to */
    uint32_t row_cap;
    uint32_t cursor_of[4], id_of[4]; /* the cursor each source was left at */
    uint32_t browse_seen;            /* browse generation the cursor belongs to */
    uint32_t land;                   /* cursor for the next browse listing */
    int active;                      /* a VIEW_ command ran: events are wanted */
    int force_changed;               /* emit VIEW_CHANGED at the next sync */
    sel_vgroup* groups;              /* SEL_VIEW_GROUPS_MAX, sorted by name */
    uint32_t ngroups, next_node, tbl_mode;
    uint32_t group_key;   /* 0 folder, 1 format */
    uint32_t drill_n;     /* values picked in the drilldown */
    char drill_value[2][SEL_VIEW_GROUP_NAME];
    uint32_t total;       /* rows in the whole list in tree and drilldown */
    int anchor_node;      /* pub.cursor_id names a node, not a song */
    sel_view_row* frows;             /* the folders source: SEL_VIEW_FOLDER_ROWS rows, text in ftext */
    char* ftext;
    char* fkey;                      /* SEL_VIEW_FOLDER_ROWS * SEL_VIEW_KEY_MAX: the key of each home row */
    uint32_t frow_n, fgen;           /* rows filled, bumps on every set */
    sel_view_sig last;
} sel_view;

/* Returns 0 when allocation fails. */
int sel_view_init(sel_view* v, const sel_host* host, uint32_t row_cap);
void sel_view_free(sel_view* v, const sel_host* host);

/* Fills the folders source from the library's FOLDER_TREE text ("F|P <id> <name>" per line, two spaces of
 * indent per level). Returns the rows kept (at most SEL_VIEW_FOLDER_ROWS). */
uint32_t sel_view_set_folders(sel_view* v, const char* tree);

/* Replaces the home rows from the library home text (H/S/C lines); returns the entry count (I191). An `S` line is
 * "S <key>\t<entry>" for a song row and "S @<album>\t<entry>" for an album row; the key lands in the row (I191a). */
uint32_t sel_view_set_home(sel_view* v, const char* text);

/* "queue", "browse", "query", "folders" or "home". 0: unknown source. */
int sel_view_show(sel_view* v, const sel_view_src* s, const char* source);

/* "N" absolute, "+N" / "-N" relative, "top", "bottom"; clamped to the list.
 * 0: the text is not a cursor position. */
int sel_view_cursor(sel_view* v, const sel_view_src* s, const char* spec);

/* 1..row_cap rows. 0: out of range. */
int sel_view_window(sel_view* v, const sel_view_src* s, uint32_t rows);

/* Re-anchors the cursor (a queue row by id), fits the window and fills the
 * rows. Cheap enough to call after every command. */
void sel_view_refresh(sel_view* v, const sel_view_src* s);

/* Style "regular", "tree" or "drilldown". 0: unknown. The cursor keeps its song. */
int sel_view_set_style(sel_view* v, const sel_view_src* s, const char* style);
const char* sel_view_style_name(const sel_view* v);

/* Group key "folder" or "format" (switches a regular view to the tree). 0: no
 * such key, so far (artist, album, genre, composer and source need the M3 catalogue). */
int sel_view_set_group(sel_view* v, const sel_view_src* s, const char* key);

/* Tree: show (on) or hide a node's songs. 0: not the tree, or no such node. */
int sel_view_expand(sel_view* v, const sel_view_src* s, uint32_t node, int on);

/* Drilldown: pick `value` at column `level` (0 first); ".." clears the level and
 * the deeper ones. 0: not the drilldown, level out of reach or no such value. */
int sel_view_drill(sel_view* v, const sel_view_src* s, uint32_t level, const char* value);

/* Enter on the cursor row. Returns 1 with *queue_index set when it is a song
 * to start; 0 when it was a node (toggled in the tree, drilled into in the drilldown). */
int sel_view_enter(sel_view* v, const sel_view_src* s, uint32_t* queue_index);

/* Refreshes, then compares with the state at the previous call. Returns 1
 * when anything the front end shows changed; *structure is set when the
 * source, style or window height changed (VIEW_CHANGED, else VIEW_ROWS only). */
int sel_view_changed(sel_view* v, const sel_view_src* s, int* structure);

#endif /* SELECTA_ENGINE_VIEW_H */

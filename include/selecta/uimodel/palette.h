/* Command palette and help overlay model of the text UI (plan M1.2 phase 5,
 * row I78; design doc section 7). Lists every verb of the COMMANDS table once,
 * grouped by its area (the commands.def section, reported by COMMANDS as
 * `area=`), with the key that is bound to it. A query ranks verbs by fuzzy
 * subsequence match. Pure C99, no allocation. */
#ifndef SELECTA_TEXT_PALETTE_H
#define SELECTA_TEXT_PALETTE_H

#include <selecta/uimodel/keybind.h>

#include <selecta/protocol.h>

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TXT_PAL_QUERY 48u
/* I1188: entries that are not engine verbs but screens of the text UI. Their ids follow the verbs (id >= SEL_CMD_COUNT);
 * the picked name is the screen's word, bound to the "@<name>" action. */
#define TXT_PAL_LOCAL_COUNT 1u

/* I1211: one row per action of the shared action table (sel_uim_action_at), after the verbs and local entries: id =
 * SEL_CMD_COUNT + TXT_PAL_LOCAL_COUNT + action index, name = the action label, line = the protocol line it sends, key = the
 * key the binding table gives that exact line. TXT_PAL_ACTION_MAX bounds the table (palette_test checks it holds). */
#define TXT_PAL_ACTION_MAX 128u
/* I1330: the most matches a palette holds; every copy of the rows (mui_palette.row) is sized from this, never SEL_CMD_COUNT. */
#define TXT_PAL_ITEM_MAX (SEL_CMD_COUNT + TXT_PAL_LOCAL_COUNT + TXT_PAL_ACTION_MAX)

typedef struct txt_palette_item {
    uint32_t id;       /* sel_cmd_id; SEL_CMD_COUNT + n for a local entry; above that an action row */
    const char* name;  /* verb, or the action label */
    const char* line;  /* what Enter sends: the verb itself, or the action's protocol line; NULL for a local entry */
    const char* area;  /* transport, queue, playlists, view, actions, protocol */
    char key[24];      /* bound key name, "" when none */
    int32_t score;     /* ranking against the query */
} txt_palette_item;

typedef enum txt_pal_result {
    TXT_PAL_EDITING = 0,
    TXT_PAL_RUN,    /* Enter on a verb: txt_palette_picked */
    TXT_PAL_CANCEL  /* Escape, or Backspace on an empty query */
} txt_pal_result;

typedef struct txt_palette {
    char query[TXT_PAL_QUERY];
    uint32_t len;
    txt_palette_item items[TXT_PAL_ITEM_MAX]; /* the matches, best first; grouped by area when the query is empty */
    uint32_t count;
    uint32_t cursor;
    const txt_bindings* binds;
} txt_palette;

/* Lists every verb, empty query. */
void txt_palette_open(txt_palette* p, const txt_bindings* binds);
void txt_palette_set_query(txt_palette* p, const char* query);
txt_pal_result txt_palette_key(txt_palette* p, uint32_t code, uint32_t mods);

/* The verb under the cursor, or NULL when nothing matches. */
const char* txt_palette_picked(const txt_palette* p);

/* The protocol line to send for the item under the cursor (verb or action line); NULL for a local entry or no match. */
const char* txt_palette_picked_line(const txt_palette* p);

/* "Playlists and library" for "playlists"; the area text itself when unknown. */
const char* txt_palette_area_title(const char* area);

/* I798: a verb that needs an argument cannot run bare (its signature has no trailing '?' or '*' covering every letter). Both
 * UIs open their command line with the verb typed for such a verb. */
int txt_palette_verb_needs_args(const char* verb);

#ifdef __cplusplus
}
#endif
#endif

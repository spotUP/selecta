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

typedef struct txt_palette_item {
    uint32_t id;       /* sel_cmd_id */
    const char* name;  /* verb */
    const char* area;  /* transport, queue, playlists, view, protocol */
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
    txt_palette_item items[SEL_CMD_COUNT]; /* the matches, best first; grouped by area when the query is empty */
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

/* "Playlists and library" for "playlists"; the area text itself when unknown. */
const char* txt_palette_area_title(const char* area);

#ifdef __cplusplus
}
#endif
#endif

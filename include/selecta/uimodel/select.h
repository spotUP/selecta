/* Shared multi-selection model (multi-select design, section 3; D9: unlimited runs). A selection is a set of listing
 * indices of ONE list, kept as sorted, disjoint, inclusive runs. Desktop, MUI and the TUI consume it; the engine only sees
 * it as a ranges string when a SONGS_* verb runs. The run arrays live on the heap and double; an allocation failure
 * returns 0 and leaves the selection as it was. sel_uim_selset_free releases them (list change, UI shutdown). */
#ifndef SELECTA_UIMODEL_SELECT_H
#define SELECTA_UIMODEL_SELECT_H

#include <stddef.h>
#include <stdint.h>

enum { SEL_UIM_LIST_VIEW = 1, SEL_UIM_LIST_QUEUE = 2, SEL_UIM_LIST_PLAYLIST = 3 };

typedef struct sel_uim_list_id {
    uint32_t kind;    /* SEL_UIM_LIST_* */
    uint32_t id;      /* playlist id, else 0 */
    uint32_t version; /* sel_view_state.version / queue_version / playlist version */
} sel_uim_list_id;

typedef struct sel_uim_run {
    uint32_t lo, hi;
} sel_uim_run;

typedef struct sel_uim_runs {
    sel_uim_run* v;
    uint32_t n, cap;
} sel_uim_runs;

typedef struct sel_uim_selset {
    sel_uim_list_id list;
    uint32_t anchor; /* Shift-click and Shift-arrow extend from here */
    sel_uim_runs run;  /* the whole selection */
    sel_uim_runs base; /* the runs fixed at the last non-Shift click; extend adds the live range over them */
    uint32_t allocs;   /* sentinel: heap growths made */
} sel_uim_selset;

void sel_uim_selset_init(sel_uim_selset* s);
void sel_uim_selset_free(sel_uim_selset* s);
/* A different list kind, id or version clears the set and frees its memory. Returns 1 when it cleared. */
int sel_uim_selset_bind(sel_uim_selset* s, sel_uim_list_id list);
void sel_uim_selset_clear(sel_uim_selset* s);
int sel_uim_selset_select_one(sel_uim_selset* s, uint32_t i);
int sel_uim_selset_toggle(sel_uim_selset* s, uint32_t i);
/* anchor..i over the base set; with no anchor yet it selects i alone. */
int sel_uim_selset_extend(sel_uim_selset* s, uint32_t i);
int sel_uim_selset_select_all(sel_uim_selset* s, uint32_t row_count);
int sel_uim_selset_contains(const sel_uim_selset* s, uint32_t i);
uint32_t sel_uim_selset_count(const sel_uim_selset* s);
uint32_t sel_uim_selset_run_count(const sel_uim_selset* s);
/* "0-11,40,52-60" into out (NUL-terminated); returns the length, or 0 when empty or it does not fit in cap. */
size_t sel_uim_selset_format(const sel_uim_selset* s, char* out, size_t cap);
/* Replaces the selection with the runs in `text` (ascending, disjoint); 0 on a syntax error or no memory, set unchanged. */
int sel_uim_selset_parse(sel_uim_selset* s, const char* text);

#endif

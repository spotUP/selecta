/* Design token tables (DUI-P1-10); the parser lives in theme.c next to the role parser. */
#include <selecta/uimodel/tokens.h>

#include <string.h>

static const char* const k_name[SEL_GUI_TOKEN_COUNT] = {
#define X(id, name, def, lo, hi) name,
    SEL_GUI_TOKENS(X)
#undef X
};
static const uint32_t k_def[SEL_GUI_TOKEN_COUNT] = {
#define X(id, name, def, lo, hi) def,
    SEL_GUI_TOKENS(X)
#undef X
};
static const uint32_t k_min[SEL_GUI_TOKEN_COUNT] = {
#define X(id, name, def, lo, hi) lo,
    SEL_GUI_TOKENS(X)
#undef X
};
static const uint32_t k_max[SEL_GUI_TOKEN_COUNT] = {
#define X(id, name, def, lo, hi) hi,
    SEL_GUI_TOKENS(X)
#undef X
};

const char* sel_gui_token_name(sel_gui_token t) { return (unsigned)t < SEL_GUI_TOKEN_COUNT ? k_name[t] : ""; }
uint32_t sel_gui_token_default(sel_gui_token t) { return (unsigned)t < SEL_GUI_TOKEN_COUNT ? k_def[t] : 0u; }
uint32_t sel_gui_token_min(sel_gui_token t) { return (unsigned)t < SEL_GUI_TOKEN_COUNT ? k_min[t] : 0u; }
uint32_t sel_gui_token_max(sel_gui_token t) { return (unsigned)t < SEL_GUI_TOKEN_COUNT ? k_max[t] : 0u; }

int sel_gui_token_find(const char* name, uint32_t len) {
    int i;
    for (i = 0; i < (int)SEL_GUI_TOKEN_COUNT; i++) {
        if (strlen(k_name[i]) == len && memcmp(k_name[i], name, len) == 0) {
            return i;
        }
    }
    return -1;
}

uint64_t sel_gui_tokens_diff(const uint32_t* a, const uint32_t* b) {
    uint64_t mask = 0u;
    uint32_t i;
    for (i = 0; i < SEL_GUI_TOKEN_COUNT; i++) {
        if (a[i] != b[i]) {
            mask |= (uint64_t)1u << i;
        }
    }
    return mask;
}

static const char* const b_name[SEL_GUI_BASE_COUNT] = {
#define X(id, name, def, lo, hi) name,
    SEL_GUI_BASE_TOKENS(X)
#undef X
};
static const uint32_t b_def[SEL_GUI_BASE_COUNT] = {
#define X(id, name, def, lo, hi) def,
    SEL_GUI_BASE_TOKENS(X)
#undef X
};
static const uint32_t b_min[SEL_GUI_BASE_COUNT] = {
#define X(id, name, def, lo, hi) lo,
    SEL_GUI_BASE_TOKENS(X)
#undef X
};
static const uint32_t b_max[SEL_GUI_BASE_COUNT] = {
#define X(id, name, def, lo, hi) hi,
    SEL_GUI_BASE_TOKENS(X)
#undef X
};
/* Stored as base + 1 so a role without an entry (ACCENT_TEXT, the cover colours and cover text) is 0 = a literal value, not GREY_0. */
static const int k_ref[SEL_GUI_TOKEN_COUNT] = {
#define X(role, base) [SEL_GUI_TOKEN_##role] = SEL_GUI_BASE_##base + 1,
    SEL_GUI_TOKEN_REFS(X)
#undef X
};

const char* sel_gui_base_name(sel_gui_base b) { return (unsigned)b < SEL_GUI_BASE_COUNT ? b_name[b] : ""; }
uint32_t sel_gui_base_default(sel_gui_base b) { return (unsigned)b < SEL_GUI_BASE_COUNT ? b_def[b] : 0u; }
uint32_t sel_gui_base_min(sel_gui_base b) { return (unsigned)b < SEL_GUI_BASE_COUNT ? b_min[b] : 0u; }
uint32_t sel_gui_base_max(sel_gui_base b) { return (unsigned)b < SEL_GUI_BASE_COUNT ? b_max[b] : 0u; }

int sel_gui_base_find(const char* name, uint32_t len) {
    int i;
    for (i = 0; i < (int)SEL_GUI_BASE_COUNT; i++) {
        if (strlen(b_name[i]) == len && memcmp(b_name[i], name, len) == 0) {
            return i;
        }
    }
    return -1;
}

int sel_gui_token_default_ref(sel_gui_token t) { return (unsigned)t < SEL_GUI_TOKEN_COUNT ? k_ref[t] - 1 : -1; }

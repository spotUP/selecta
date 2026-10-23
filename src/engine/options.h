/* One option parser for Amiga ToolTypes and the command line (plan M2 phase 2,
 * T58). Both feed sel_opts_apply, so KEY=VALUE means the same thing in an icon,
 * on the command line and in a text file. Keys map onto prefs (prefs.def) and,
 * where a commands.def verb exists, onto that verb. */
#ifndef SELECTA_ENGINE_OPTIONS_H
#define SELECTA_ENGINE_OPTIONS_H

#include "prefs.h"

#include <stdint.h>

typedef struct sel_opts {
    uint32_t count; /* options applied */
} sel_opts;

enum { SEL_OPTS_OK = 0, SEL_OPTS_UNKNOWN_KEY, SEL_OPTS_BAD_VALUE };

/* Applies one KEY=VALUE (key case-insensitive). */
int sel_opts_apply(sel_prefs* p, sel_opts* o, const char* key, const char* value);
/* Applies one "KEY=VALUE" word. */
int sel_opts_apply_word(sel_prefs* p, sel_opts* o, const char* word);
/* Applies every word of argv. Stops at the first failure and returns it. */
int sel_opts_parse_args(sel_prefs* p, sel_opts* o, int argc, char** argv);
/* Applies KEY=VALUE lines (ToolTypes text; blank lines and lines starting with ; or # are skipped). */
int sel_opts_parse_text(sel_prefs* p, sel_opts* o, const char* text);
/* Task priority option, clamped to -128..127. */
int sel_opts_priority(const sel_prefs* p);
/* FNV-1a over every pref value: equal prefs hash equal. */
uint32_t sel_opts_hash(const sel_prefs* p);

#endif

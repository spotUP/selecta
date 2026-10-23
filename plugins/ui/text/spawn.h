/* Runs a program for the command line's `!` (plan M1.2 phase 5, row I77). One
 * implementation per OS family under plugins/ui/text/<os>/ (RULES.md rule 3);
 * posix/spawn.c serves macOS and Linux. */
#ifndef SELECTA_TEXT_SPAWN_H
#define SELECTA_TEXT_SPAWN_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* `line` is the program followed by its own arguments, split at spaces (no
 * quoting); the n selected paths are appended as single arguments. Waits for
 * the program. Returns its exit status, or -1 when it cannot be started or
 * ends on a signal. */
int txt_spawn(const char* line, const char* const* paths, uint32_t n);

/* Writes `s` as one AmigaDOS command-line argument: wrapped in double quotes,
 * with `"` and `*` escaped by the AmigaDOS escape character `*`. Returns the
 * length written (excluding the NUL), or 0 when it does not fit in `cap`. */
size_t txt_quote_arg(char* out, size_t cap, const char* s);

#ifdef __cplusplus
}
#endif
#endif

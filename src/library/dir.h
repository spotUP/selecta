/* Directory iteration, one implementation per platform under
 * src/core/<os>/dir.c (RULES.md rule 3: OS headers live only there). The
 * library code above it is portable. */
#ifndef SELECTA_LIBRARY_DIR_H
#define SELECTA_LIBRARY_DIR_H

/* Called once per entry other than "." and "..". Return 0 to stop early. */
typedef int (*sel_dir_fn)(void* ctx, const char* name, int is_dir);

/* Lists `path`. Returns 0 when the directory cannot be opened, else 1. */
int sel_dir_each(const char* path, sel_dir_fn fn, void* ctx);

#endif /* SELECTA_LIBRARY_DIR_H */

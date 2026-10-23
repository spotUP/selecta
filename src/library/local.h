/* Local file browse (design doc: library; plan M1.1 phase 4). One fixed
 * buffer allocated at engine creation holds the listing of one directory:
 * ".." first (except at the filesystem root), then directories, then files,
 * each group sorted case-insensitively by name. M3 wraps this as the
 * `local` source without changing the listing code. */
#ifndef SELECTA_LIBRARY_LOCAL_H
#define SELECTA_LIBRARY_LOCAL_H

#include <selecta/host.h>
#include <stddef.h>
#include <stdint.h>

#define SEL_BROWSE_PATH_MAX 1024u

typedef struct sel_browse_entry {
    uint32_t name_offset; /* into the text arena */
    uint32_t is_dir;
} sel_browse_entry;

typedef struct sel_browse {
    sel_browse_entry* entries;
    char* text;
    uint32_t capacity, text_bytes, text_used, count;
    uint32_t truncated;  /* entries dropped from the current listing */
    uint32_t generation; /* bumped by every successful listing */
    char path[SEL_BROWSE_PATH_MAX];
} sel_browse;

/* Returns 0 when allocation fails. capacity and text_bytes must be non-zero. */
int sel_browse_init(sel_browse* b, const sel_host* host, uint32_t capacity, uint32_t text_bytes);
void sel_browse_free(sel_browse* b, const sel_host* host);

/* Lists `path` (empty: "/"). Returns 0, keeping the previous listing, when
 * the directory cannot be read. Hidden entries (leading '.') are skipped. */
int sel_browse_list(sel_browse* b, const char* path);

const char* sel_browse_name(const sel_browse* b, uint32_t index);

/* Index of the entry called `name`, or the count when there is none. */
uint32_t sel_browse_find(const sel_browse* b, const char* name);

/* Path style of this binary: AmigaDOS ("Volume:dir/sub", parent of "Work:Music" is "Work:")
 * on the Amiga build, "/" paths elsewhere. The *_style variants take it explicitly (tests). */
#if defined(__amigaos__) || defined(AMIGA)
#define SEL_PATH_STYLE_AMIGA 1
#else
#define SEL_PATH_STYLE_AMIGA 0
#endif

/* Whether `path` is a listing root: "/" or, with `amiga_style`, a bare "Volume:". */
int sel_browse_is_root_style(const char* path, int amiga_style);
int sel_browse_is_root(const char* path);
const char* sel_browse_basename_style(const char* path, int amiga_style);
int sel_browse_join_style(char* out, size_t cap, const char* dir, const char* name, int amiga_style);

/* `dir` + name into `out`; the name ".." goes to the parent (the root stays
 * the root). Returns 0 when it does not fit. */
int sel_browse_join(char* out, size_t cap, const char* dir, const char* name);

/* Last path component of `path` (the whole string when it has no separator). */
const char* sel_browse_basename(const char* path);

/* Whether `path` is absolute. "/..." always is; with `amiga_style` (the Amiga
 * build, or a test) "Volume:dir" and "PROGDIR:" are too. */
int sel_path_is_absolute_style(const char* path, int amiga_style);

/* The predicate for the platform this binary runs on. */
int sel_path_is_absolute(const char* path);

#endif /* SELECTA_LIBRARY_LOCAL_H */

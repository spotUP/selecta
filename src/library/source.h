/* Sources (design doc 9.1; plan 2026-10-25-m3-library.md Phase 4, I157). Every place music comes from
 * is a sel_source: one C file per source under src/library/sources/. The registry maps a source id to
 * its descriptor; the song key `<source id>:<path>` is the identity of a song across the library. */
#ifndef SELECTA_LIBRARY_SOURCE_H
#define SELECTA_LIBRARY_SOURCE_H

#include "song_key.h"
#include <selecta/tags.h>
#include <stddef.h>
#include <stdint.h>

#define SEL_SOURCE_ID_MAX 16u   /* longest source id incl. NUL */

/* Receives one catalogue row of a full listing (path, size, md5 or NULL, format, artist). */
typedef struct sel_listing_sink {
    void* ctx;
    int (*row)(void* ctx, const char* path, int64_t size, const char* md5, const char* format, const char* artist);
} sel_listing_sink;

/* Progress of a long listing; `cancel` is polled by the source. */
typedef struct sel_progress {
    void* ctx;
    void (*report)(void* ctx, uint32_t done, uint32_t total);
    int (*cancel)(void* ctx);
} sel_progress;

/* A way to fetch a file: ordered list of base URLs (mirrors) and the scheme. Filled by network sources. */
typedef struct sel_transport {
    const char* const* bases;
    uint32_t count;
    uint32_t kind; /* SEL_TRANSPORT_* */
} sel_transport;
enum { SEL_TRANSPORT_FILE = 0, SEL_TRANSPORT_FTP = 1, SEL_TRANSPORT_HTTP = 2, SEL_TRANSPORT_MIRRORS = 3 };

typedef struct sel_list_sink {
    void* ctx;
    int (*list)(void* ctx, const char* id, const char* name);
} sel_list_sink;

typedef struct sel_playlist {
    const char* name;
    const char* const* paths;
    uint32_t count;
} sel_playlist;

typedef struct sel_source {
    const char* id; /* "modland", "hvsc", "asma", "sndh", "zxart", "keygenmusic", "local", "hippo", "radio" */
    const char* name;
    const char* home;
    const char* about;
    const sel_tag* tags; /* SEL_SRC_HAS_LISTS, SEL_SRC_HOSTED_BY, SEL_SRC_HAS_MD5, SEL_SRC_TRANSPORTS */
    int (*listing)(void* st, sel_listing_sink* out, sel_progress* p);
    /* Format / artist / name from a path: fills out[0..3] (SRC_FORMAT, SRC_ARTIST, SRC_NAME, END); the
     * strings are copied into `scratch` (n bytes). 0 when the path has no such structure. */
    int (*parse)(const char* path, char* scratch, size_t n, sel_tag* out);
    const sel_transport* (*transport)(void* st);
    int (*url)(const char* path, char* out, size_t n);       /* canonical public URL */
    int (*path_in)(const char* url, char* path, size_t n);   /* inverse of url */
    int (*published_lists)(void* st, sel_list_sink* out);
    int (*read_list)(void* st, const char* id, sel_playlist* out);
} sel_source;

/* Registry. Registration is by static descriptor; ids are unique. Returns 0 on a duplicate id, a
 * full registry or an id longer than SEL_SOURCE_ID_MAX - 1. */
int sel_source_register(const sel_source* src);
const sel_source* sel_source_find(const char* id);
uint32_t sel_source_count(void);
const sel_source* sel_source_get(uint32_t index);
/* Registers the sources this build ships (local, modland, hvsc). Idempotent. */
void sel_source_register_builtin(void);

/* The `local` and `modland` descriptors (sources/local.c, sources/modland.c). */
const sel_source* sel_source_local(void);
const sel_source* sel_source_modland(void);
const sel_source* sel_source_hippo(void);   /* sources/hippo.c, hippoplayer.se playlists */
const sel_source* sel_source_hvsc(void); /* sources/hvsc.c, hosted by Modland */
const sel_source* sel_source_asma(void); /* sources/asma.c, the Atari SAP Music Archive */
const sel_source* sel_source_sndh(void); /* sources/sndh.c, the Atari ST SNDH archive */
const sel_source* sel_source_zxart(void); /* sources/zxart.c, ZX Spectrum music from zxart.ee */

/* Song key <source id>:<path>. 0 when it does not fit. */
int sel_song_key(const char* source, const char* path, char* out, size_t n);
/* Splits a key at the first ':' into id and path. 0 for a malformed key (no ':', empty id, id too
 * long, buffer too small). Paths may contain ':' (AmigaDOS "Work:Music"). */
/* 1 when `id` is a registered source whose songs live off the machine (every source but "local" and "radio").
 * The one place cache.c and devilbox.c ask, so a new source needs no string literal elsewhere (I211b). */
int sel_source_is_remote(const char* id);
int sel_song_key_split(const char* key, char* source, size_t source_n, char* path, size_t path_n);

/* Modland format table (design doc 9.1: one table maps a format name to a replayer across sources). */
typedef struct sel_format_entry {
    const char* name; /* Modland directory name: "Protracker", "Fasttracker 2", ... */
    const char* ext;  /* usual file extension without dot, lower case; "" when none */
} sel_format_entry;
uint32_t sel_format_count(void);
const sel_format_entry* sel_format_get(uint32_t index);
/* Case-insensitive lookups; NULL when unknown. */
const sel_format_entry* sel_format_by_name(const char* name);
const sel_format_entry* sel_format_by_ext(const char* ext);

#endif /* SELECTA_LIBRARY_SOURCE_H */

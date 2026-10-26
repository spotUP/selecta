/* Sources (design doc 9.1; plan 2026-10-25-m3-library.md Phase 4, I157). Every place music comes from
 * is a sel_source: one C file per source under src/library/sources/. The registry maps a source id to
 * its descriptor; the song key `<source id>:<path>` is the identity of a song across the library. */
#ifndef SELECTA_LIBRARY_SOURCE_H
#define SELECTA_LIBRARY_SOURCE_H

#include "song_key.h"
#include <selecta/host.h>
#include <selecta/source.h>
#include <selecta/tags.h>
#include <stddef.h>
#include <stdint.h>

/* Registry. Registration is by static descriptor; ids are unique. Returns 0 on a duplicate id, a
 * full registry or an id longer than SEL_SOURCE_ID_MAX - 1. */
int sel_source_register(const sel_source* src);
const sel_source* sel_source_find(const char* id);
uint32_t sel_source_count(void);
const sel_source* sel_source_get(uint32_t index);
/* Registers the sources this build ships (local, modland, hvsc). Idempotent. */
void sel_source_register_builtin(void);
/* Hands in the plugin enumeration (sel_registry_count / sel_registry_get) and the host for log lines; the
 * registry walk in sel_source_register_builtin adds every kind 7 plugin. NULL: no plugin sources. */
void sel_source_set_plugins(uint32_t (*count)(void), const sel_tag* (*get)(uint32_t), const sel_host* host);

/* The `local` and `modland` descriptors (sources/local.c, sources/modland.c). */
const sel_source* sel_source_local(void);
const sel_source* sel_source_modland(void);
const sel_source* sel_source_hippo(void);   /* sources/hippo.c, hippoplayer.se playlists */
const sel_source* sel_source_hvsc(void); /* sources/hvsc.c, hosted by Modland */
const sel_source* sel_source_asma(void); /* sources/asma.c, the Atari SAP Music Archive */
const sel_source* sel_source_sndh(void); /* sources/sndh.c, the Atari ST SNDH archive */
const sel_source* sel_source_keygenmusic(void); /* sources/keygenmusic.c, keygen and crack music from keygenmusic.tk */

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

/* The `keygenmusic` source sync (row I416; online-sources plan section 2; design doc 9.1): the music of keygens,
 * cracks and trainers (https://keygenmusic.tk, about 4,350 tracker modules). The site's own player loads one file,
 * /kgm/lib.txt: a JSON list with, per song, its path (`/kgm/<group>/<file>`, percent-encoded), the release group
 * "rg", what was cracked "sn" and the module's title "mdt". The sync streams that file and writes one catalogue row
 * per song through the same SCAN_BATCH request as the other sources. A path is relative to /kgm/ and decoded
 * (`!Others/DOT.EXE - Alcohol 120% 1.9.5.4327 crk.xm`), the song key is `keygenmusic:<path>`. Formats are Modland's
 * names for the four tracker kinds (bbmp keygenmusic.py FORMATS), so the engine finds the replayer as for Modland. */
#ifndef SELECTA_LIBRARY_SOURCES_KEYGENMUSIC_SYNC_H
#define SELECTA_LIBRARY_SOURCES_KEYGENMUSIC_SYNC_H

#include "local_source.h"

#include <stdio.h>

#define SEL_KEYGEN_LIST "lib.txt"
#define SEL_KEYGEN_ROOT "kgm/" /* where the songs live on the site: <site>/kgm/<path> */

#define SEL_KEYGEN_PATH_MAX 512u
#define SEL_KEYGEN_TEXT_MAX 192u

typedef struct sel_keygen_entry {
    char path[SEL_KEYGEN_PATH_MAX]; /* "/kgm/..." as the list has it, still percent-encoded */
    char group[SEL_KEYGEN_TEXT_MAX]; /* "rg": who released it, may be empty */
    char what[SEL_KEYGEN_TEXT_MAX];  /* "sn": what was cracked, may be empty */
} sel_keygen_entry;

typedef struct sel_keygen_sync_stats {
    uint32_t entries;      /* list objects read */
    uint32_t listing_rows; /* rows written to the catalogue */
    uint32_t skipped_rows; /* entries outside /kgm/, with a bad escape or an unsafe path */
    uint32_t batches;      /* SEL_LIB_REQ_SCAN_BATCH requests sent */
} sel_keygen_sync_stats;

/* Calls `fn` once per object of the top-level array of `f`; `fn` returns 0 to stop. Long strings are cut to the
 * field sizes. Returns the objects read, or -1 when `f` holds no array. */
int32_t sel_keygen_read_list(FILE* f, int (*fn)(void* ctx, const sel_keygen_entry* e), void* ctx);

/* Modland format name from the extension: xm "Fasttracker 2", mod "Protracker", it "Impulse Tracker", s3m
 * "Scream Tracker 3"; anything else "Fasttracker 2" (bbmp). */
const char* sel_keygen_format(const char* path);

/* The release group: `group` when given, else the folder of `path`, else (the folders "!Others" and "!FromUsers"
 * hold songs of many groups) the text before " - " in the file name; "- unknown" when none. 0 if `n` is too small. */
int sel_keygen_group(const char* path, const char* group, char* out, size_t n);

/* "/kgm/!Others/Lazzy - CrackMe%231.xm" -> "!Others/Lazzy - CrackMe#1.xm". 0 when the path is not below /kgm/, has a
 * bad escape, is unsafe or does not fit `n`. */
int sel_keygen_path(const char* listed, char* out, size_t n);

/* Reads <dir>/lib.txt into the catalogue. Returns 0 when the file cannot be read or holds no array, memory runs
 * out or the DB rejects a batch. */
int sel_keygen_sync(const sel_local_scan_config* cfg, const char* dir, sel_keygen_sync_stats* stats);

#endif

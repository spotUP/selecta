/* The `zxart` source sync (row I415; online-sources plan section 2; design doc 9.1): ZX-Art (https://zxart.ee), the
 * ZX Spectrum's music, about 29,700 tunes (AY trackers, beeper, digital). The site has a paged JSON export
 * (bbmp zxart.py): /api/language:eng/export:zxMusic/start:N/limit:1000 gives tunes, export:author the people. The
 * network step (zxart_net.c) stores the pages as `zxauthor_<k>.json` and `zxmusic_<k>.json` in one directory; this
 * file based sync reads them. A path is `<id>/<file name>` (the name percent-decoded); the song key is
 * `zxart:<path>`. A tune is filed under the format Modland gives its kind ("PT3" is "Spectrum - Pro Tracker 3"),
 * then under its composers, joined with " & ". Recordings (MP3, Ogg, WAV, FLAC) are not chip music and are left out. */
#ifndef SELECTA_LIBRARY_SOURCES_ZXART_SYNC_H
#define SELECTA_LIBRARY_SOURCES_ZXART_SYNC_H

#include "local_source.h"

#include <stdio.h>

#define SEL_ZXART_PAGE 1000u                 /* the most the API gives in one answer */
#define SEL_ZXART_PAGES_MAX 64u              /* pages read per list (29,731 tunes are 30 pages in 2026) */
#define SEL_ZXART_MUSIC "zxmusic_%u.json"    /* page k of the tunes, as downloaded */
#define SEL_ZXART_PEOPLE "zxauthor_%u.json"  /* page k of the people */
#define SEL_ZXART_AUTHORS_MAX 8u

typedef struct sel_zxart_item {
    int64_t id;       /* -1 when the object has none */
    int64_t year;     /* 0 = none */
    char title[256];
    char type[16];    /* the kind: "PT3", "AY", ... */
    char time[24];    /* "3:14.88" */
    char file[1024];  /* originalFileName, still percent-encoded */
    int has_url;      /* originalUrl is set */
    uint32_t author_n;
    int64_t authors[SEL_ZXART_AUTHORS_MAX];
} sel_zxart_item;

typedef struct sel_zxart_sync_stats {
    uint32_t tunes;        /* tune objects read */
    uint32_t people;       /* people read */
    uint32_t listing_rows; /* rows written to the catalogue */
    uint32_t skipped_rows; /* tunes left out: a recording, no file, an unsafe path */
    uint32_t batches;      /* SEL_LIB_REQ_SCAN_BATCH requests sent */
} sel_zxart_sync_stats;

/* Reads one page of an API answer: calls `fn` once per object of the list `what` ("zxMusic" or "author"), stopping
 * when `fn` returns 0. `fn` may be NULL (count only). `*total` is the answer's totalAmount. Returns the objects
 * read, or -1 when the answer is not JSON of that shape or its responseStatus is not "success". */
int32_t sel_zxart_read_page(FILE* f, const char* what, int (*fn)(void* ctx, const sel_zxart_item* it), void* ctx,
                            uint32_t* total);

/* The format a kind of tune is filed under (bbmp TYPES); `ext` is the lower case file extension. Never NULL. */
const char* sel_zxart_format(const char* kind, const char* ext, char* scratch, size_t n);

/* The tune's length in milliseconds from "3:14.88"; 0 when not said. */
int32_t sel_zxart_millis(const char* time);

/* Reads <dir>/zxauthor_*.json and <dir>/zxmusic_*.json into the catalogue. Returns 0 when the first page of either
 * cannot be read, memory runs out or the DB rejects a batch. */
int sel_zxart_sync(const sel_local_scan_config* cfg, const char* dir, sel_zxart_sync_stats* stats);

#endif

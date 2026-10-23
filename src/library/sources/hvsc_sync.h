/* The `hvsc` source sync (plan Phase 4, I162; design doc 9.1): reads the HVSC listing, Songlengths.md5 and
 * STIL.txt from one directory into the catalogue through the same LIBRARY_SYNC command as Modland. Offline
 * by design: the fetch layer puts the three files in <dir>; the canonical URL of a row comes from
 * sel_source_hvsc()->url, which resolves through the Modland host path layout. */
#ifndef SELECTA_LIBRARY_SOURCES_HVSC_SYNC_H
#define SELECTA_LIBRARY_SOURCES_HVSC_SYNC_H

#include "local_source.h"

#define SEL_HVSC_LISTING "hvsc_listing.txt"    /* one path per line: `/MUSICIANS/H/Hubbard_Rob/Commando.sid` */
#define SEL_HVSC_SONGLENGTHS "Songlengths.md5" /* `; /PATH` comment line, then `<md5>=<m:ss[.mmm]> ...` */
#define SEL_HVSC_STIL "STIL.txt"               /* `/PATH` line, COMMENT:/TITLE:/ARTIST:/AUTHOR:/NAME:, `(#n)` */
/* Where Modland serves the HVSC tree: <modland root>/pub/modules/HVSC/<hvsc path>. */
#define SEL_HVSC_HOST_DIR "pub/modules/HVSC/"

/* Stickers written per song (key `hvsc:<path>`):
 *   subtunes          decimal subtune count (from Songlengths.md5)
 *   lengths_ms        comma separated integer milliseconds, one per subtune
 *   stil.<field>      file level STIL note (field: comment, title, artist, author, name)
 *   stil.<n>.<field>  note of subtune n (1-based) */

typedef struct sel_hvsc_sync_stats {
    uint32_t listing_rows;    /* listing lines written to the catalogue */
    uint32_t skipped_rows;    /* listing lines outside MUSICIANS/GAMES/DEMOS */
    uint32_t batches;         /* SEL_LIB_REQ_SCAN_BATCH requests sent */
    uint32_t songlengths;     /* Songlengths.md5 entries parsed */
    uint32_t lengths_matched; /* ... that annotated a listed song */
    uint32_t subtunes;        /* subtunes summed over the parsed entries */
    uint32_t stil_entries;    /* STIL entries with at least one note */
    uint32_t stil_matched;    /* ... that attached to a listed song */
} sel_hvsc_sync_stats;

/* host path (`pub/modules/HVSC/<path>`) of an HVSC path; 0 when it does not fit or is absolute. */
int sel_hvsc_host_path(const char* path, char* out, size_t n);

/* "m:ss" or "m:ss.mmm" (optional trailing flag text such as "(G)") to integer milliseconds; -1 when malformed. */
int32_t sel_hvsc_parse_time_ms(const char* text);

/* Applies <dir>/hvsc_listing.txt, then Songlengths.md5 and STIL.txt when present. Returns 0 when the
 * listing cannot be read, memory runs out or the DB rejects a batch. */
int sel_hvsc_sync(const sel_local_scan_config* cfg, const char* dir, sel_hvsc_sync_stats* stats);

#endif

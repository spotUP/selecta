/* The `hippo` source sync (plan Phase 4, I163; design doc 9.1 and 17): reads the hippoplayer.se playlist list
 * and the fetched `.prg` files from one directory into the catalogue through the same LIBRARY_SYNC command as
 * Modland and HVSC. Offline by design: the fetch layer puts the files in <dir>; the canonical URL of a row
 * comes from sel_source_hippo()->url. One row per playlist: path = server filename, name = title,
 * artist = author. */
#ifndef SELECTA_LIBRARY_SOURCES_HIPPO_SYNC_H
#define SELECTA_LIBRARY_SOURCES_HIPPO_SYNC_H

#include "local_source.h"

#define SEL_HIPPO_LIST_JSON "hippo_list.json" /* GET /api/v1/playlists: [{"id":1,"title":..,"author":..,"genre":..,"filename":..}] */
#define SEL_HIPPO_LIST_TSV "uhcplaylists.tsv" /* GET /uhcplaylists.php: `Filename\tPath\tTitle\tAuthor\tGenre`, Latin-1 */
#define SEL_HIPPO_PRG_EXT ".prg"              /* fetched playlists, <dir>/<filename> */
#define SEL_HIPPO_URL "https://hippoplayer.se/playlist/"

/* Stickers per playlist (key `hippo:<filename>`): genre, hippo_id (JSON list only), and for a fetched `.prg`
 * prg_entries, prg_dividers, prg_stealth, prg_subsongs. */

typedef struct sel_hippo_sync_stats {
    uint32_t lists;     /* playlist rows parsed from the list(s) and written to the catalogue */
    uint32_t skipped;   /* list records without a filename */
    uint32_t batches;   /* SEL_LIB_REQ_SCAN_BATCH requests sent */
    uint32_t playlists; /* I1058: playlists written to the HippoPlayer folder of the Playlists tree */
    uint32_t prg_files; /* fetched .prg files the Hippo codec accepted */
    uint32_t prg_entries, prg_dividers, prg_stealth, prg_subsongs; /* summed over those files */
} sel_hippo_sync_stats;

/* I1058: calls fn(user, filename, title) for every list record of <dir>/hippo_list.json and <dir>/uhcplaylists.tsv that
 * has a plain filename. Reads the lists with the parsers of the sync; writes nothing. fn returns 0 to stop. */
void sel_hippo_each_file(const sel_host* host, const char* dir, int (*fn)(void* user, const char* file, const char* title),
                         void* user);

/* I1058a: the title of playlist `id` when it sits directly under the HippoPlayer folder of the Playlists tree. */
int sel_hippo_playlist_title(sel_library* lib, const sel_host* host, uint32_t id, char* title, size_t cap);

/* I1058a: the song keys of the prg file at `path` (NUL separated, extra NUL at the end) into out[cap]. 0 when the file
 * is missing, empty or not a HiPPrg. The one prg reader of the sync and of the lazy open. */
int sel_hippo_file_keys(const sel_host* host, const char* path, char* out, size_t cap);

/* Applies <dir>/hippo_list.json and/or <dir>/uhcplaylists.tsv (at least one must exist), then every
 * every .prg in <dir>. Returns 0 when no list can be read, memory runs out or the DB rejects a batch. */
int sel_hippo_sync(const sel_local_scan_config* cfg, const char* dir, sel_hippo_sync_stats* stats);

#endif

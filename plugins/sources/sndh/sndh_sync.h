/* The `sndh` source sync (row I414; online-sources plan section 2; design doc 9.1): SNDH, the Atari ST's YM2149
 * music (https://sndh.atari.org), about 5,900 tunes. The site publishes everything once a year as
 * /files/sndh<year>_lf.zip (100 MB, linked from /download.php) and keeps every tune unpacked below /sndh/sndh_lf/.
 * The catalogue is read from the zip's central directory, which sits at the end of the zip: two small Range
 * requests (bbmp sndh.py zip_list), not the 100 MB. The zip gives no checksums, so rows carry none. A path is
 * relative to /sndh/sndh_lf/ (`Mad_Max/Wings_Of_Death.sndh`), the song key is `sndh:<path>`. */
#ifndef SELECTA_PLUGINS_SOURCES_SNDH_SYNC_H
#define SELECTA_PLUGINS_SOURCES_SNDH_SYNC_H

#include <selecta/host.h>
#include <selecta/source.h>

#include <stdio.h>

#define SEL_SNDH_CD "sndh.cd"                /* the central directory bytes, as downloaded */
#define SEL_SNDH_FOLDER "sndh_lf/"           /* the folder of long file names inside the zip */
#define SEL_SNDH_ROOT "sndh/sndh_lf/"        /* where the tunes live on the site: <site>/sndh/sndh_lf/<path> */
#define SEL_SNDH_PATH_MAX 512u
#define SEL_SNDH_TAIL_BYTES 65536u           /* the zip's last bytes, which hold the end-of-directory record */
#define SEL_SNDH_CD_MAX (64ul << 20)         /* bbmp: a directory above 64 MB is refused */

typedef struct sel_sndh_entry {
    char name[SEL_SNDH_PATH_MAX]; /* the zip member name, as stored */
    uint32_t size;                /* uncompressed bytes */
    int utf8;                     /* general purpose flag bit 11: the name is UTF-8 (else CP437) */
    int too_long;                 /* the name did not fit; `name` is empty */
} sel_sndh_entry;

typedef struct sel_sndh_sync_stats {
    uint32_t entries;      /* central directory records read */
    uint32_t listing_rows; /* rows written to the catalogue */
    uint32_t skipped_rows; /* .sndh members with an unsafe path, a name that did not fit, or a non-ASCII CP437 name */
    uint32_t batches;      /* unused; kept at 0 (the engine batches the catalogue rows) */
} sel_sndh_sync_stats;

/* Calls `fn` once per central directory record of `f`, stopping at the first byte run that is not a record (the
 * end-of-directory record); `fn` returns 0 to stop. Returns the records read, or -1 when `f` does not start with a
 * record. */
int32_t sel_sndh_read_cd(FILE* f, int (*fn)(void* ctx, const sel_sndh_entry* e), void* ctx);

/* Finds the end-of-directory record in the last bytes `tail` of a zip: the directory's size and offset. 0 when
 * there is none, or the zip is zip64 (fields all ones). */
int sel_sndh_find_directory(const unsigned char* tail, size_t n, uint32_t* cd_offset, uint32_t* cd_size);

/* The highest year of the `files/sndh<year>_lf.zip` links of /download.php, or 0 when it links none. */
unsigned sel_sndh_newest_year(const char* page, size_t n);

/* The source descriptor's parse hook (sndh.c): format, artist and name of a path. */
int sel_sndh_parse(const char* path, char* scratch, size_t n, sel_tag* out);

/* Reads <dir>/sndh.cd into the catalogue. Returns 0 when the file cannot be read or is no directory, memory runs
 * out or the DB rejects a batch. */
int sel_sndh_sync(const sel_host* host, const char* dir, sel_sndh_sync_stats* stats);

#endif

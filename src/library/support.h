/* Support-file downloader (I751; bbmp README.md:416-446, downloads.py). Some replayers need a file their format's
 * maker gives out for download but does not allow passing on: Organya's default.wdb, Beepola's Spectrum ROM and
 * tables. They never go in git or in the shipped app. The user opts in (pref library.support_downloads = on); the
 * file is then fetched once, checked against the SHA-256 in the manifest and kept under `<cache dir>/support/`.
 *
 * Manifest: one entry per line, fields separated by TAB:
 *   id  file  url  size  sha256  cut_off  cut_len  piece_sha256  plugin  licence
 * `size`/`sha256` describe the download. cut_len 0: the download is the file. cut_len > 0: the file is bytes
 * [cut_off, cut_off+cut_len) of the download and piece_sha256 covers those. Several entries may share a url (the
 * download is repeated per entry; they are small). The built-in manifest is the table in support.c; the environment
 * variable SELECTA_SUPPORT_MANIFEST names a replacement file (tests, mirrors). */
#ifndef SELECTA_LIBRARY_SUPPORT_H
#define SELECTA_LIBRARY_SUPPORT_H

#include <stddef.h>

#define SEL_SUPPORT_MAX 16
#define SEL_SUPPORT_FIELD 256

typedef struct sel_support_entry {
    char id[48];
    char file[48];
    char url[SEL_SUPPORT_FIELD];
    unsigned long size;
    char sha256[65];
    unsigned long cut_off, cut_len;
    char piece_sha256[65];
    char plugin[32];
    char licence[SEL_SUPPORT_FIELD];
} sel_support_entry;

typedef enum sel_support_result {
    SEL_SUPPORT_OK = 0,         /* file present and verified */
    SEL_SUPPORT_NO_CONSENT = 1, /* opt-in pref is off: nothing was requested */
    SEL_SUPPORT_UNKNOWN = 2,    /* no such manifest entry */
    SEL_SUPPORT_FETCH = 3,      /* download failed (offline, HTTP error, wrong size) */
    SEL_SUPPORT_HASH = 4,       /* body or piece did not match the manifest: nothing was stored */
    SEL_SUPPORT_IO = 5          /* cache directory not writable */
} sel_support_result;

/* Loads the manifest (env override or built-in); returns the entry count. */
unsigned sel_support_manifest(sel_support_entry* out, unsigned cap);
const char* sel_support_result_name(sel_support_result r);

/* `<cache dir>/support/<file>`. Returns 1 when the file exists and its SHA-256 matches the manifest (the lookup a
 * plugin or the host does: no network, works offline), else 0. */
int sel_support_path(const char* cache_dir, const char* file, char* out, size_t cap);

/* Fetches one entry if missing. consent 0 returns NO_CONSENT without any request. A file already present and valid
 * makes no request (works offline). */
sel_support_result sel_support_get(const char* cache_dir, const sel_support_entry* e, int consent);

#endif

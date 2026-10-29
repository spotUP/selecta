/* One member of a remote zip, fetched with HTTP Range requests (row I695d). The zip's central directory is a file on
 * disk (the sync downloaded it); the member's local header and data come from the server, so a 100 MB archive costs
 * two small requests. Used by the cache for a song whose loose file the site does not serve (sndh: the year zip lists
 * tunes that have no file below /sndh/sndh_lf/). */
#ifndef SELECTA_LIBRARY_ZIP_MEMBER_H
#define SELECTA_LIBRARY_ZIP_MEMBER_H

#include <stdint.h>

#define SEL_ZIP_MEMBER_MAX (16ul << 20) /* a member above 16 MB is refused */
#define SEL_ZIP_NAME_MAX 512u           /* longest member name the lookup compares */

typedef struct sel_zip_member {
    uint32_t local_offset; /* offset of the local file header in the zip */
    uint32_t csize;        /* stored bytes */
    uint32_t usize;        /* unpacked bytes */
    uint16_t method;       /* 0 stored, 8 deflate */
} sel_zip_member;

/* Looks `name` up in the central directory file `cd_path`. 1 when found. */
int sel_zip_member_find(const char* cd_path, const char* name, sel_zip_member* out);

/* Downloads the member of the zip at `zip_url` and writes the unpacked bytes to `out_path`. Counts the requests sent
 * in *requests (may be NULL). 1 on success. */
int sel_zip_member_fetch(const char* zip_url, const sel_zip_member* m, const char* out_path, unsigned* requests);

#endif

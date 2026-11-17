/* UADE data set downloader (I774o R12; design 2026-11-17-i774-uade-tools-all.md section 4, D1). Every install carries the
 * baked files (songlengths.bin, uadeinfo.bin, uadecredits.bin, uadeprov.bin, uademd5.bin, optional extras); this only
 * UPDATES them. The server serves `manifest.txt` ("selecta-uadedata 1", "bake <id>", one "<md5> <size> <name>" line per
 * file) and `<id>/<name>` under the mirror source "uadedata". Files go through sel_cache_fetch (mirrors, md5 check);
 * the set is installed only when every file arrived with the right md5, otherwise the old set stays untouched. */
#ifndef SELECTA_LIBRARY_UADEDATA_H
#define SELECTA_LIBRARY_UADEDATA_H

#include "cache.h"

typedef enum sel_uadedata_result {
    SEL_UADEDATA_UPDATED = 1, /* new set installed */
    SEL_UADEDATA_CURRENT = 0, /* every file already matches the manifest */
    SEL_UADEDATA_FAILED = -1, /* manifest or a file unreachable, or malformed manifest; old set kept */
    SEL_UADEDATA_MD5 = -2     /* a file's md5 differed from the manifest; old set kept */
} sel_uadedata_result;

/* Update the files in `dir` (the config dir) from the server through cache `c`. Downloads are staged in the cache and
 * removed after the install. */
sel_uadedata_result sel_uadedata_update(sel_cache* c, const char* dir);

#endif

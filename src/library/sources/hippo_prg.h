/* Reads a fetched hippoplayer.se `.prg` through the existing Hippo codec (library/playlist/formats/hippo.c) and
 * counts what the codec kept (plan Phase 4, I163; design doc 17). Kept apart from hippo_sync.c because
 * playlist.h and source.h both declare a `sel_playlist`. */
#ifndef SELECTA_LIBRARY_SOURCES_HIPPO_PRG_H
#define SELECTA_LIBRARY_SOURCES_HIPPO_PRG_H

#include <selecta/host.h>
#include <stddef.h>
#include <stdint.h>

typedef struct sel_hippo_prg_stats {
    uint32_t entries;  /* all entries: songs, dividers, stealth */
    uint32_t dividers; /* 0xF7 lines */
    uint32_t stealth;  /* 0xA2 lines */
    uint32_t subsongs; /* entries carrying `#song=<digit>` */
} sel_hippo_prg_stats;

/* 1 when the codec accepted the bytes (memory comes from the host). */
int sel_hippo_prg_scan(const sel_host* host, const char* bytes, size_t len, sel_hippo_prg_stats* out);

#endif

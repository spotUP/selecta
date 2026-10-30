/* Source descriptor (design doc 9.1, I517 D1). Every place music comes from is a sel_source; a source plugin
 * (kind 7) returns one from its entry. Registry, song keys and the format table stay in src/library/source.h. */
#ifndef SELECTA_SOURCE_H
#define SELECTA_SOURCE_H

#include <selecta/host.h>
#include <selecta/tags.h>
#include <stdlib.h>
#include <string.h>

SEL_EXTERN_C_BEGIN

#define SEL_SOURCE_ID_MAX 16u   /* longest source id incl. NUL */
#define SEL_SOURCE_MAX 16u      /* most sources the registry holds */

/* Receives one catalogue row of a full listing (path, size, md5 or NULL, format, artist). */
typedef struct sel_listing_sink {
    void* ctx;
    int (*row)(void* ctx, const char* path, int64_t size, const char* md5, const char* format, const char* artist);
} sel_listing_sink;

/* Progress of a long listing; `cancel` is polled by the source. */
typedef struct sel_progress {
    void* ctx;
    void (*report)(void* ctx, uint32_t done, uint32_t total);
    int (*cancel)(void* ctx);
} sel_progress;

/* A way to fetch a file: ordered list of base URLs (mirrors) and the scheme. Filled by network sources. */
typedef struct sel_transport {
    const char* const* bases;
    uint32_t count;
    uint32_t kind; /* SEL_TRANSPORT_* */
} sel_transport;
enum { SEL_TRANSPORT_FILE = 0, SEL_TRANSPORT_FTP = 1, SEL_TRANSPORT_HTTP = 2, SEL_TRANSPORT_MIRRORS = 3 };

typedef struct sel_list_sink {
    void* ctx;
    int (*list)(void* ctx, const char* id, const char* name);
} sel_list_sink;

typedef struct sel_source_playlist {
    const char* name;
    const char* const* paths;
    uint32_t count;
} sel_source_playlist;

typedef struct sel_source {
    const char* id; /* "modland", "hvsc", "asma", "sndh", "zxart", "keygenmusic", "local", "hippo", "radio" */
    const char* name;
    const char* home;
    const char* about;
    const sel_tag* tags; /* SEL_SRC_HAS_LISTS, SEL_SRC_HOSTED_BY, SEL_SRC_HAS_MD5, SEL_SRC_TRANSPORTS */
    int (*listing)(void* st, sel_listing_sink* out, sel_progress* p);
    /* Format / artist / name from a path: fills out[0..3] (SRC_FORMAT, SRC_ARTIST, SRC_NAME, END); the
     * strings are copied into `scratch` (n bytes). 0 when the path has no such structure. */
    int (*parse)(const char* path, char* scratch, size_t n, sel_tag* out);
    const sel_transport* (*transport)(void* st);
    int (*url)(const char* path, char* out, size_t n);       /* canonical public URL */
    int (*path_in)(const char* url, char* path, size_t n);   /* inverse of url */
    int (*published_lists)(void* st, sel_list_sink* out);
    int (*read_list)(void* st, const char* id, sel_source_playlist* out);
    /* Fetch or refresh this source's catalogue. `arg` is a URL or a local mirror path; the source decides which
     * (the engine no longer calls sel_modland_is_url). Stats go out as tags (SEL_SYNC_*), so the engine needs no
     * per-source field. `stats` has room for `stats_cap` entries including the SEL_TAG_END. 1 on success. */
    int (*sync)(const sel_host* host, const char* arg, int64_t now, sel_tag* stats, uint32_t stats_cap);
    uint32_t flags;           /* SEL_SOURCE_REMOTE, SEL_SOURCE_SYNCS, SEL_SOURCE_NEW_THIS_WEEK */
    const char* sync_default; /* default URL for the Sources page Sync button; NULL when none */
    /* SEL_SOURCE_STREAMS only (I725c): the live stream URL a song of this source plays through the one stream path
     * (sel_live, ICY title), written to `out`. May tell the service the station was played (radio-browser click
     * count). 1 resolved, 0 when it cannot be played. NULL for every other source. */
    int (*play_url)(const sel_host* host, const char* path, char* out, size_t n);
} sel_source;
enum {
    SEL_SOURCE_REMOTE = 1u,        /* songs live off the machine: fetched through the cache */
    SEL_SOURCE_SYNCS = 2u,         /* has a sync */
    SEL_SOURCE_NEW_THIS_WEEK = 4u, /* the sync reports SEL_SYNC_NEW_THIS_WEEK; the engine persists it */
    SEL_SOURCE_STREAMS = 8u        /* songs are live streams: never cached, played from play_url (I725c) */
};

/* Sync on launch (I527): 1 when a source last synced at `synced` (unix time, 0 never) is due for the sync offer at `now`.
 * `days` 0 turns the offer off. `notice_at` is when the user last said Not now (0 never); the later of it and `synced`
 * starts the clock, so a dismissed offer returns after `days` days. */
static inline int sel_source_sync_due(int64_t synced, int64_t notice_at, int64_t now, uint32_t days) {
    int64_t from = synced > notice_at ? synced : notice_at;
    if (days == 0u) return 0;
    if (from <= 0) return 1;
    return now - from >= (int64_t)days * 86400;
}

/* I582: the snooze time (unix seconds, whole days) the `library.sync_snoozed` list "id:days,id:days" holds for `id`; 0 none. */
static inline int64_t sel_source_snooze_at(const char* csv, const char* id) {
    size_t n;
    const char* p;
    if (csv == NULL || id == NULL || id[0] == '\0') return 0;
    n = strlen(id);
    for (p = csv; *p != '\0';) {
        const char* end = strchr(p, ',');
        size_t len = end != NULL ? (size_t)(end - p) : strlen(p);
        if (len > n && p[n] == ':' && strncmp(p, id, n) == 0) return (int64_t)strtoll(p + n + 1, NULL, 10) * 86400;
        p += len;
        if (*p == ',') p++;
    }
    return 0;
}

SEL_EXTERN_C_END

#endif /* SELECTA_SOURCE_H */

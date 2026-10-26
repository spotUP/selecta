/* Catalogue service for source plugins (I517 D2). `sel_host.catalogue` returns the table. A source never sees the
 * database: it opens a batch, queues rows, commits. The engine batches the rows into library requests. */
#ifndef SELECTA_CATALOGUE_H
#define SELECTA_CATALOGUE_H

#include <selecta/host.h>

SEL_EXTERN_C_BEGIN

typedef struct sel_catalogue_song {
    const char* path;   /* path inside the source */
    const char* format; /* "" = unknown */
    const char* artist;
    const char* album;
    const char* name;
    const char* md5;    /* "" = unknown */
    int64_t size;
    int32_t duration_ms; /* 0 = unknown */
    int32_t year;        /* 0 = unknown */
} sel_catalogue_song;

typedef struct sel_catalogue_dir_sink {
    void* ctx;
    int (*entry)(void* ctx, const char* name, int is_dir); /* nonzero stops the listing */
} sel_catalogue_dir_sink;

typedef struct sel_catalogue {
    /* Starts a batch for `source_id`. 0 when one is open already or the library is absent. */
    int (*begin)(const sel_host* host, const char* source_id);
    /* Queues one song row; flushes to the library when the queue is full. 0 on failure. */
    int (*song)(const sel_host* host, const sel_catalogue_song* s);
    /* Removes the songs at `paths` of the open source (a changelog's deleted entries). */
    int (*remove)(const sel_host* host, const char* const* paths, uint32_t count);
    /* Re-keys the row at `from` to the song `to` in place; ratings, history and playlist entries stay. */
    int (*move)(const sel_host* host, const char* from, const sel_catalogue_song* to);
    /* Writes what is queued and ends the batch. 0 when any write failed. */
    int (*commit)(const sel_host* host);
    /* Drops the queue and ends the batch. */
    void (*abort)(const sel_host* host);
    /* The directory a source keeps downloads in, created; "<cache>/sync/<source_id>". 0 when it cannot be made. */
    int (*cache_dir)(const sel_host* host, const char* source_id, char* out, uint32_t n);
    /* Lists one directory of the machine (a local mirror copy). 0 when it cannot be listed. */
    int (*list_dir)(const sel_host* host, const char* path, const sel_catalogue_dir_sink* sink);
    /* Epoch seconds the engine stamps on new rows. */
    int64_t (*now)(const sel_host* host);
} sel_catalogue;

SEL_EXTERN_C_END

#endif /* SELECTA_CATALOGUE_H */

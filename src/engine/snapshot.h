/* Published engine snapshot (I1082d, design doc 2026-11-05-I1082-engine-thread section 2.1).
 * At the end of a pump the engine thread deep-copies sel_state (every pointer field: queue, view and its rows, viz, pattern,
 * lists, strings, tag lists, collection page) into one of three buffers and publishes it with one atomic exchange; the UI
 * thread acquires the newest at the start of its turn and reads only that until the next acquire. Triple buffer: the writer
 * never touches the buffer the reader holds, the reader never sees a buffer in the middle of a copy.
 * Granularity (measured, see the I1082d commit): everything small is copied every publish; the play queue (entries plus path
 * text) is copied only when queue_version moved since that buffer last took it, and the collection page's song block only when
 * its serial or size moved. */
#ifndef SELECTA_SNAPSHOT_H
#define SELECTA_SNAPSHOT_H

#include <selecta/engine.h>
#include <selecta/host.h>

typedef struct sel_snap_blob {
    char* p;
    uint32_t cap;
} sel_snap_blob;

typedef struct sel_snap_buf {
    sel_state st;
    sel_snap_blob core;  /* everything but the queue and the collection song block; rebuilt every publish */
    sel_snap_blob queue; /* entries, then path text */
    sel_snap_blob rows;  /* collection song block */
    uint32_t q_ver, q_count, q_bytes, q_valid;
    uint32_t r_key[3], r_len, r_valid;
} sel_snap_buf;

typedef struct sel_snap {
    const sel_host* host;
    sel_snap_buf buf[3];
    uint32_t write;  /* publisher private */
    uint32_t read;   /* UI private: the buffer sel_snap_state returns */
    uint32_t middle; /* shared: buffer index, plus 4 when it holds an unread snapshot */
    uint32_t publishes, bytes_max, us_max; /* sentinels, written by the publisher, read by anyone */
} sel_snap;

int sel_snap_init(sel_snap* s, const sel_host* host);
void sel_snap_free(sel_snap* s);
/* Publisher thread: copies `live` into the write buffer and publishes it. 0 on no memory (the previous snapshot stays). */
int sel_snap_publish(sel_snap* s, const sel_state* live);
/* UI thread: takes the newest published snapshot. 1 when it moved. */
int sel_snap_acquire(sel_snap* s);
/* UI thread: the acquired snapshot (zeroed before the first publish). */
const sel_state* sel_snap_state(const sel_snap* s);

#endif

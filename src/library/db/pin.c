/* I406b: pins online search hits (SOURCE_SEARCH) into the library, so a hit the user played, queued or added to a
 * playlist is a normal songs row after the next restart. The engine command thread only copies the hit into a small
 * ring (sel_library_pin) and wakes the library job thread, which writes the queued rows as ONE SEL_LIB_REQ_SCAN_BATCH
 * (the same request the folder scan and the source syncs use); the command thread never waits on the library (I782:
 * a sync holds the call lock for minutes). Without threads (Amiga) the row is written inline. */
#include "db_internal.h"

#include "../source.h"
#include "../../core/count.h"

#include <string.h>

static void pin_copy(char* dst, size_t cap, const char* s) {
    snprintf(dst, cap, "%s", s != NULL ? s : "");
}

/* The site's own format string ("mod", "xm") as the library's format name ("Protracker"): the same table the scanner
 * and the Modland sync use (formats_table.c). A string the table does not know stays as it is. */
static void pin_format(char* dst, size_t cap, const char* site) {
    const sel_format_entry* f = site != NULL && site[0] != '\0' ? sel_format_by_ext(site) : NULL;

    pin_copy(dst, cap, f != NULL ? f->name : site);
}

static void pin_write(sel_library* lib, const sel_pin_slot* q, unsigned n) {
    sel_scan_row rows[SEL_PIN_MAX];
    sel_scan_batch b;
    sel_message reply;
    unsigned i;

    for (i = 0; i < n; i++) {
        memset(&rows[i], 0, sizeof rows[i]);
        rows[i].source = q[i].source;
        rows[i].path = q[i].path;
        rows[i].name = q[i].title;
        rows[i].artist = q[i].artist;
        rows[i].format = q[i].format;
        rows[i].album = "";
        rows[i].md5 = "";
    }
    memset(&b, 0, sizeof b); /* new_day = 0: a pin records no New-level entry */
    b.rows = rows;
    b.count = n;
    b.pin = 1;
    b.now = 0; /* added = 0: a pin is the user's own pick, never "New" (Q4) */
    memset(&reply, 0, sizeof reply);
    if (sel_library_request(lib, SEL_LIB_REQ_SCAN_BATCH, &b, &reply) && reply.args[1].v.i == 0) {
        lib->pin_rows += (unsigned)reply.args[0].v.i;
    }
}

uint32_t sel_library_write_gen(sel_library* lib) {
    return lib != NULL ? SEL_ATOMIC_LOAD_ACQ(&lib->write_gen) : 0;
}

uint32_t sel_library_pin_gen(sel_library* lib) {
    return lib != NULL ? SEL_ATOMIC_LOAD_ACQ(&lib->pin_gen) : 0;
}

uint32_t sel_library_touch_gen(sel_library* lib) {
    return lib != NULL ? SEL_ATOMIC_LOAD_ACQ(&lib->touch_gen) : 0;
}

uint32_t sel_library_pin_rows(sel_library* lib) {
    return lib != NULL ? lib->pin_rows : 0;
}

uint32_t sel_library_pin_dropped(sel_library* lib) {
    return lib != NULL ? lib->pin_dropped : 0;
}

#if !defined(SEL_LIBRARY_NO_THREAD)

void sel_pin_job_run(sel_library* lib) {
    sel_pin_slot q[SEL_PIN_MAX];
    unsigned n;

    if (lib->pin_n == 0) { /* close calls this after the join, with or without a started job thread */
        return;
    }
    pthread_mutex_lock(&lib->job_lock);
    n = lib->pin_n;
    memcpy(q, lib->pin_q, n * sizeof q[0]);
    lib->pin_n = 0;
    pthread_mutex_unlock(&lib->job_lock);
    pin_write(lib, q, n);
}

void sel_library_pin(sel_library* lib, const char* source, const char* path, const char* title, const char* artist,
                     const char* format) {
    sel_pin_slot* s;

    if (lib == NULL || source == NULL || path == NULL || !sel_job_start(lib)) {
        return;
    }
    pthread_mutex_lock(&lib->job_lock);
    if (lib->pin_n >= SEL_PIN_MAX) {
        lib->pin_dropped++;
        pthread_mutex_unlock(&lib->job_lock);
        return;
    }
    s = &lib->pin_q[lib->pin_n++];
    pin_copy(s->source, sizeof s->source, source);
    pin_copy(s->path, sizeof s->path, path);
    pin_copy(s->title, sizeof s->title, title);
    pin_copy(s->artist, sizeof s->artist, artist);
    pin_format(s->format, sizeof s->format, format);
    lib->job_pin = 1;
    sel_job_wake(lib);
    pthread_mutex_unlock(&lib->job_lock);
}

#else

void sel_pin_job_run(sel_library* lib) {
    (void)lib;
}

void sel_library_pin(sel_library* lib, const char* source, const char* path, const char* title, const char* artist,
                     const char* format) {
    sel_pin_slot s;

    if (lib == NULL || source == NULL || path == NULL) {
        return;
    }
    pin_copy(s.source, sizeof s.source, source);
    pin_copy(s.path, sizeof s.path, path);
    pin_copy(s.title, sizeof s.title, title);
    pin_copy(s.artist, sizeof s.artist, artist);
    pin_format(s.format, sizeof s.format, format);
    pin_write(lib, &s, 1);
}

#endif

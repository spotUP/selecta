/* I1415: see include/selecta/uimodel/open_progress.h. */
#include <selecta/uimodel/open_progress.h>

#include <stdio.h>

int32_t sel_open_progress_fraction(const sel_open_progress* p) {
    uint32_t done;

    if (p == NULL || !p->active || p->phase != SEL_OPEN_DOWNLOADING || p->total == 0u) {
        return -1;
    }
    done = p->done < p->total ? p->done : p->total;
    return (int32_t)(((uint64_t)done << 16) / p->total);
}

void sel_open_progress_text(const sel_open_progress* p, char* out, size_t n) {
    char what[40];
    int32_t f;

    if (n == 0u) {
        return;
    }
    out[0] = '\0';
    if (p == NULL || !p->active) {
        return;
    }
    switch (p->phase) {
    case SEL_OPEN_CONNECTING:
        snprintf(out, n, "Connecting");
        return;
    case SEL_OPEN_UNPACKING:
        snprintf(out, n, "Unpacking");
        return;
    case SEL_OPEN_STARTING:
        snprintf(out, n, "Starting");
        return;
    case SEL_OPEN_DOWNLOADING:
        break;
    default:
        return;
    }
    f = sel_open_progress_fraction(p);
    if (f >= 0) {
        snprintf(what, sizeof what, "%u%%", (unsigned)((uint64_t)(p->done < p->total ? p->done : p->total) * 100u / p->total));
    } else if (p->done >= 1048576u) {
        snprintf(what, sizeof what, "%u.%u MB", (unsigned)(p->done >> 20), (unsigned)(((p->done & 0xFFFFFu) * 10u) >> 20));
    } else if (p->done >= 1024u) {
        snprintf(what, sizeof what, "%u KB", (unsigned)(p->done >> 10));
    } else {
        snprintf(what, sizeof what, "%u B", (unsigned)p->done);
    }
    if (p->files > 1u) {
        snprintf(out, n, "Downloading file %u of %u - %s", (unsigned)p->file, (unsigned)p->files, what);
    } else {
        snprintf(out, n, "Downloading %s", what);
    }
}

/* `selecta-cli record <url> <file> [--bytes N] [--seconds S]` (F36, I196, T94): stream-to-disk. The raw stream bytes
 * are written as they arrive by the record sink in the live download (src/library/stream_open.c), the same single
 * HTTP request OPEN uses. Ends when the stream ends, N bytes are recorded, S seconds passed, or SIGINT/SIGTERM
 * (STOP); the file is closed by sel_live_close, so it is a complete prefix of the stream either way.
 * Prints `stream_bytes= recorded_bytes= record_taps= end=`; exit 0 when bytes were recorded, 1 otherwise, 2 usage. */
#include "cli.h"
#include "stream_open.h"

#include <selecta/host.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __amigaos__
int sel_cli_record(int argc, char** argv) {
    (void)argc;
    (void)argv;
    fprintf(stderr, "record: stream recording needs the desktop build (Amiga sockets arrive with I199)\n");
    return 1;
}
#else
#include <signal.h>
#include <unistd.h>

static volatile sig_atomic_t g_stop;
static void on_signal(int sig) {
    (void)sig;
    g_stop = 1;
}

int sel_cli_record(int argc, char** argv) {
    const char* url = NULL;
    const char* file = NULL;
    unsigned long limit = 0, seconds = 0, waited_ms = 0;
    const char* end = "stream_end";
    sel_live* live;
    int i, rc;
    for (i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--bytes") == 0 && i + 1 < argc) {
            limit = strtoul(argv[++i], NULL, 10);
        } else if (strcmp(argv[i], "--seconds") == 0 && i + 1 < argc) {
            seconds = strtoul(argv[++i], NULL, 10);
        } else if (url == NULL) {
            url = argv[i];
        } else if (file == NULL) {
            file = argv[i];
        } else {
            url = NULL;
            break;
        }
    }
    if (url == NULL || file == NULL) {
        fprintf(stderr, "usage: selecta-cli record <url> <file> [--bytes N] [--seconds S]\n");
        return 2;
    }
    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);
    live = sel_live_open_record(sel_host_default(), url, NULL, 0, file, limit);
    if (live == NULL) {
        fprintf(stderr, "record: cannot start (url %s, file %s)\n", url, file);
        return 1;
    }
    for (;;) {
        sel_live_discard(live, 65536u); /* no decoder reads the ring: keep the socket flowing */
        if (g_stop) {
            end = "stop";
            break;
        }
        if (limit != 0 && sel_live_recorded(live) >= limit) {
            end = "byte_limit";
            break;
        }
        if (sel_live_finished(live)) {
            break;
        }
        if (seconds != 0 && waited_ms >= seconds * 1000ul) {
            end = "time_limit";
            break;
        }
        usleep(2000);
        waited_ms += 2;
    }
    sel_live_close(live);
    printf("RECORD stream_bytes=%u recorded_bytes=%u record_taps=%u end=%s\n", (unsigned)sel_stream_received(),
           (unsigned)sel_stream_recorded_total(), (unsigned)sel_stream_record_taps(), end);
    rc = sel_stream_recorded_total() > 0 ? 0 : 1;
    return rc;
}
#endif

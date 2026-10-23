/* DEViLBOX lyrics off the engine thread (I193, F19). The engine asks for lyrics of a song that has no `.lrc` and no
 * song message: sel_library_lyrics_fetch stores the path in the handle and wakes the library job thread (job.c,
 * the thread behind ratings, votes and prefetch). That thread hashes the file and sends GET /api/lyrics/<md5>, so an
 * unreachable server stalls only that thread. The outcome is kept in the handle (sel_library_lyrics_result). A newer
 * request replaces one still queued. Without threads (Amiga until Phase 12) the request runs on the caller's task. */
#include "db_internal.h"

#include "../devilbox_loudness.h"
#include "../devilbox_lyrics.h"
#include "../lyrics.h"
#include "../net/devilbox.h"

#include <stdio.h>
#include <string.h>

#if !defined(SEL_LIBRARY_NO_THREAD)
#define LYR_LOCK(l) pthread_mutex_lock(&(l)->job_lock)
#define LYR_UNLOCK(l) pthread_mutex_unlock(&(l)->job_lock)
#else
#define LYR_LOCK(l) ((void)(l))
#define LYR_UNLOCK(l) ((void)(l))
#endif

#define LYR_SENT_MAX 64u
static char g_sent[LYR_SENT_MAX][33]; /* md5s already POSTed this session (one job thread, or the inline caller) */
static unsigned g_sent_n;

static int already_sent(const char* hash) {
    unsigned i;

    for (i = 0; i < g_sent_n && i < LYR_SENT_MAX; i++) {
        if (strcmp(g_sent[i], hash) == 0) return 1;
    }
    return 0;
}

/* I193b: the .lrc beside the song goes up once, after a definite 404 and only with a token. */
static void lyrics_upload(const char* song_path, const char* hash) {
    static char lrc[SEL_DVB_LYRICS_UPLOAD_MAX + 1u];
    char lrc_path[600];
    char token[SEL_DEVILBOX_TOKEN_MAX];
    FILE* f;
    size_t n;

    if (already_sent(hash)) return;
    if (!sel_devilbox_token(token, sizeof token)) return;
    if (!sel_lrc_path(song_path, lrc_path, sizeof lrc_path) || (f = fopen(lrc_path, "rb")) == NULL) goto done;
    n = fread(lrc, 1, sizeof lrc, f);
    fclose(f);
    if (n == 0u || n > SEL_DVB_LYRICS_UPLOAD_MAX) goto done;
    strcpy(g_sent[g_sent_n++ % LYR_SENT_MAX], hash);
    (void)sel_dvb_lyrics_put(hash, token, lrc, n);
done:
    memset(token, 0, sizeof token);
}

void sel_lyrics_job_run(sel_library* lib) {
    char path[sizeof lib->lyr_path];
    char hash[33];
    static char text[SEL_LYRICS_TEXT_MAX]; /* only the one job thread (or the inline caller) runs this */
    size_t len = 0;
    int status = SEL_DVB_LYRICS_MISSING;
    int http = 0, local;
    unsigned seq;

    LYR_LOCK(lib);
    memcpy(path, lib->lyr_path, sizeof path);
    local = lib->lyr_local;
    LYR_UNLOCK(lib);

    text[0] = '\0';
    if (sel_dvb_loudness_file_hash(path, hash)) {
        status = sel_dvb_lyrics_get_ex(hash, text, sizeof text, &len, &http);
        if (local && http == 404) lyrics_upload(path, hash);
    }
    LYR_LOCK(lib);
    seq = lib->lyr_res.seq + 1u;
    lib->lyr_res.seq = seq;
    lib->lyr_res.status = status;
    memcpy(lib->lyr_res.path, path, sizeof path);
    memcpy(lib->lyr_res.text, text, len + 1u);
    LYR_UNLOCK(lib);
}

unsigned sel_library_lyrics_seq(sel_library* lib) {
    unsigned seq;

    if (lib == NULL) return 0;
    LYR_LOCK(lib);
    seq = lib->lyr_res.seq;
    LYR_UNLOCK(lib);
    return seq;
}

void sel_library_lyrics_result(sel_library* lib, sel_lyrics_result* out) {
    memset(out, 0, sizeof *out);
    if (lib == NULL) return;
    LYR_LOCK(lib);
    *out = lib->lyr_res;
    LYR_UNLOCK(lib);
}

static int lyrics_set(sel_library* lib, const char* song_path, int local_lrc) {
    lib->lyr_local = local_lrc;
    if (strlen(song_path) >= sizeof lib->lyr_path) return 0;
    strcpy(lib->lyr_path, song_path);
    return 1;
}

#if !defined(SEL_LIBRARY_NO_THREAD)

void sel_library_lyrics_fetch(sel_library* lib, const char* song_path, int local_lrc) {
    if (lib == NULL || song_path == NULL) return;
    if (!sel_job_start(lib)) {
        if (lyrics_set(lib, song_path, local_lrc)) sel_lyrics_job_run(lib); /* no thread to be had: ask here */
        return;
    }
    pthread_mutex_lock(&lib->job_lock);
    if (lyrics_set(lib, song_path, local_lrc)) {
        lib->job_lyric = 1;
        sel_job_wake(lib);
    }
    pthread_mutex_unlock(&lib->job_lock);
}

#else

void sel_library_lyrics_fetch(sel_library* lib, const char* song_path, int local_lrc) {
    if (lib != NULL && song_path != NULL && lyrics_set(lib, song_path, local_lrc)) sel_lyrics_job_run(lib);
}

#endif

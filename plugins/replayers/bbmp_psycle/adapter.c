/* bbmp psycle replayer (row I710u): Psycle songs (.psy, PSY2SONG / PSY3SONG) played by bbmp's csrc/psycle helper
 * (third_party/bbmp_psycle: Psycle r12005 player code, GPL-2.0-or-later, bbmp glue GPL-3.0-or-later) and Psycle's own native
 * plugins. A RENDER-kind adapter that runs the helper as a child process: the helper keeps process-wide singletons and ends
 * with _exit() by design, so it cannot live inside the host. The helper and its plugin libraries sit in "bbmp_psycle/" beside
 * this module (bin/bbmp-psycle, lib/psycle-plugins; the app plugin folder carries it whole, SEL_MODULE_DATA_DIR).
 * Protocol (bbmp-psycle.cpp): `bbmp-psycle <file> 0 <rate> [start-ms]` prints one JSON line, then s16le stereo to the end of
 * the song. Playback is streamed from the pipe; a seek restarts the helper at the new position. */
#define _GNU_SOURCE
#include <selecta/plugin.h>
#include <selecta/replayer.h>

#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;

typedef struct bbpsy_state {
    const sel_host* host;
    char home[1024];  /* <module folder>/bbmp_psycle */
    char file[1100];  /* the song, written to a temp file the helper can read */
    pid_t pid;
    int fd;
    uint32_t rate;
    uint32_t duration_ms;
    char title[256], author[256], type[96];
    sel_tag meta[5];
} bbpsy_state;

static int is_psy(const uint8_t* d, size_t n) {
    return n >= 12u && (memcmp(d, "PSY3SONG", 8) == 0 || memcmp(d, "PSY2SONG", 8) == 0);
}

static int rp_probe(const uint8_t* data, size_t size, const char* name) {
    (void)name;
    return is_psy(data, size) ? SEL_PROBE_YES : SEL_PROBE_NO;
}

/* <module folder>/bbmp_psycle, or 0 when the module's folder is unknown. */
static int find_home(char* home, size_t cap) {
    Dl_info di;
    char* slash;
    if (dladdr((void*)&find_home, &di) == 0 || di.dli_fname == NULL || strlen(di.dli_fname) + 16u >= cap) {
        return 0;
    }
    strcpy(home, di.dli_fname);
    slash = strrchr(home, '/');
    if (slash == NULL) {
        return 0;
    }
    strcpy(slash + 1, "bbmp_psycle");
    return 1;
}

static void stop_child(bbpsy_state* s) {
    if (s->fd >= 0) {
        close(s->fd);
        s->fd = -1;
    }
    if (s->pid > 0) {
        kill(s->pid, SIGKILL);
        while (waitpid(s->pid, NULL, 0) < 0 && errno == EINTR) {
        }
        s->pid = 0;
    }
}

/* Value of a JSON string field of the helper's one-line header; decodes \" \\ \n \uXXXX (BMP) into UTF-8. */
static void json_str(const char* line, const char* key, char* out, size_t cap) {
    char pat[32];
    const char* p;
    size_t n = 0;
    out[0] = '\0';
    snprintf(pat, sizeof pat, "\"%s\":\"", key);
    p = strstr(line, pat);
    if (p == NULL) {
        return;
    }
    p += strlen(pat);
    while (*p != '\0' && *p != '"' && n + 4u < cap) {
        if (*p == '\\' && p[1] != '\0') {
            p++;
            if (*p == 'u' && p[1] && p[2] && p[3] && p[4]) {
                char hex[5] = {p[1], p[2], p[3], p[4], 0};
                unsigned cp = (unsigned)strtoul(hex, NULL, 16);
                if (cp < 0x80u) {
                    out[n++] = (char)cp;
                } else if (cp < 0x800u) {
                    out[n++] = (char)(0xC0u | (cp >> 6));
                    out[n++] = (char)(0x80u | (cp & 0x3Fu));
                } else {
                    out[n++] = (char)(0xE0u | (cp >> 12));
                    out[n++] = (char)(0x80u | ((cp >> 6) & 0x3Fu));
                    out[n++] = (char)(0x80u | (cp & 0x3Fu));
                }
                p += 5;
                continue;
            }
            out[n++] = *p == 'n' ? '\n' : *p;
            p++;
            continue;
        }
        out[n++] = *p++;
    }
    out[n] = '\0';
}

/* Runs the helper at start_ms; reads its header line. 1 on success (a song of at least one track), else 0. */
static int run_helper(bbpsy_state* s, uint32_t start_ms) {
    char bin[1200], plugins[1200], rate[16], start[16], line[8192];
    char* argv[6];
    char** envp;
    size_t ne = 0, i, used = 0;
    int pfd[2];
    posix_spawn_file_actions_t fa;
    stop_child(s);
    snprintf(bin, sizeof bin, "%s/bin/bbmp-psycle", s->home);
    snprintf(plugins, sizeof plugins, "BBMP_PSYCLE_PLUGINS=%s/lib/psycle-plugins", s->home);
    if (access(bin, X_OK) != 0 || pipe(pfd) != 0) {
        return 0;
    }
    snprintf(rate, sizeof rate, "%u", (unsigned)s->rate);
    snprintf(start, sizeof start, "%u", (unsigned)start_ms);
    argv[0] = bin; argv[1] = s->file; argv[2] = (char*)"0"; argv[3] = rate; argv[4] = start; argv[5] = NULL;
    while (environ[ne] != NULL) {
        ne++;
    }
    envp = (char**)malloc((ne + 2u) * sizeof *envp);
    if (envp == NULL) {
        close(pfd[0]); close(pfd[1]);
        return 0;
    }
    for (i = 0; i < ne; i++) {
        envp[i] = environ[i];
    }
    envp[ne] = plugins;
    envp[ne + 1u] = NULL;
    posix_spawn_file_actions_init(&fa);
    posix_spawn_file_actions_adddup2(&fa, pfd[1], 1);
    posix_spawn_file_actions_addopen(&fa, 2, "/dev/null", O_WRONLY, 0);
    posix_spawn_file_actions_addclose(&fa, pfd[0]);
    posix_spawn_file_actions_addclose(&fa, pfd[1]);
    if (posix_spawn(&s->pid, bin, &fa, NULL, argv, envp) != 0) {
        s->pid = 0;
    }
    posix_spawn_file_actions_destroy(&fa);
    free(envp);
    close(pfd[1]);
    if (s->pid <= 0) {
        close(pfd[0]);
        return 0;
    }
    s->fd = pfd[0];
    while (used + 1u < sizeof line) { /* the header line, byte by byte: the PCM that follows stays in the pipe */
        char c;
        ssize_t n = read(s->fd, &c, 1);
        if (n < 0 && errno == EINTR) {
            continue;
        }
        if (n <= 0 || c == '\n') {
            break;
        }
        line[used++] = c;
    }
    line[used] = '\0';
    if (strstr(line, "\"tracks\":1") == NULL) {
        stop_child(s);
        return 0;
    }
    if (start_ms == 0u) {
        const char* d = strstr(line, "\"duration_ms\":");
        s->duration_ms = d != NULL ? (uint32_t)strtoul(d + 14, NULL, 10) : 0u;
        json_str(line, "title", s->title, sizeof s->title);
        json_str(line, "author", s->author, sizeof s->author);
        json_str(line, "type", s->type, sizeof s->type);
    }
    return 1;
}

static void rp_close(void* state) {
    bbpsy_state* s = (bbpsy_state*)state;
    if (s != NULL) {
        const sel_host* host = s->host;
        stop_child(s);
        if (s->file[0] != '\0') {
            unlink(s->file);
        }
        host->mem_free(host, s);
    }
}

static void* rp_open(const sel_song_source* src, const sel_host* host, sel_rp_info* info) {
    bbpsy_state* s;
    const char* tmp;
    int fd;
    size_t off = 0;
    if (src->stream != NULL || src->data == NULL || !is_psy(src->data, src->size)) {
        return NULL;
    }
    s = (bbpsy_state*)host->mem_alloc(host, sizeof *s, SEL_MEM_FAST);
    if (s == NULL) {
        return NULL;
    }
    memset(s, 0, sizeof *s);
    s->host = host;
    s->fd = -1;
    tmp = getenv("TMPDIR");
    snprintf(s->file, sizeof s->file, "%s/selecta_psy_XXXXXX", tmp != NULL && tmp[0] != '\0' ? tmp : "/tmp");
    fd = mkstemp(s->file);
    if (fd < 0) {
        s->file[0] = '\0';
        rp_close(s);
        return NULL;
    }
    while (off < src->size) {
        ssize_t n = write(fd, src->data + off, src->size - off);
        if (n <= 0) {
            break;
        }
        off += (size_t)n;
    }
    close(fd);
    if (off != src->size || !find_home(s->home, sizeof s->home)) {
        rp_close(s);
        return NULL;
    }
    {
        char bin[1200];
        snprintf(bin, sizeof bin, "%s/bin/bbmp-psycle", s->home);
        if (access(bin, X_OK) != 0) { /* no helper beside the module: not a player */
            rp_close(s);
            return NULL;
        }
    }
    info->voice_count = 0;
    info->subsong_count = 1;
    info->default_subsong = 0;
    info->timebase_hz = 0;
    return s;
}

static int rp_start(void* state, uint32_t subsong, uint32_t rate, const sel_voices* voices) {
    bbpsy_state* s = (bbpsy_state*)state;
    (void)voices;
    if (rate < 8000u || rate > 192000u || subsong != 0u) {
        return 0;
    }
    s->rate = rate;
    return run_helper(s, 0u);
}

static uint32_t rp_render(void* state, int16_t* out, uint32_t frames) {
    bbpsy_state* s = (bbpsy_state*)state;
    size_t want = (size_t)frames * 4u, got = 0;
    while (s->fd >= 0 && got < want) {
        ssize_t n = read(s->fd, (char*)out + got, want - got);
        if (n < 0 && errno == EINTR) {
            continue;
        }
        if (n <= 0) {
            break;
        }
        got += (size_t)n;
    }
    return (uint32_t)(got / 4u);
}

static int rp_seek(void* state, uint32_t ms) {
    return run_helper((bbpsy_state*)state, ms);
}

static const sel_tag* rp_metadata(void* state) {
    bbpsy_state* s = (bbpsy_state*)state;
    uint32_t n = 0;
    if (s->title[0] != '\0') {
        s->meta[n].tag = SEL_META_TITLE;
        s->meta[n++].value = (uintptr_t)s->title;
    }
    if (s->author[0] != '\0') {
        s->meta[n].tag = SEL_META_AUTHOR;
        s->meta[n++].value = (uintptr_t)s->author;
    }
    s->meta[n].tag = SEL_META_FORMAT;
    s->meta[n++].value = (uintptr_t)(s->type[0] != '\0' ? s->type : "Psycle");
    if (s->duration_ms != 0u) {
        s->meta[n].tag = SEL_META_DURATION_MS;
        s->meta[n++].value = (uintptr_t)s->duration_ms;
    }
    s->meta[n].tag = SEL_TAG_END;
    s->meta[n].value = 0;
    return s->meta;
}

static sel_tag g_hooks[12];
static sel_tag g_desc[5];

static void set(sel_tag* t, uint32_t tag, uintptr_t value) {
    t->tag = tag;
    t->value = value;
}

const sel_tag* SEL_PLUGIN_ENTRY(uint32_t host_api) {
    if (host_api < 1u) {
        return NULL;
    }
    set(&g_hooks[0], SEL_RP_NAME, (uintptr_t) "bbmp_psycle");
    set(&g_hooks[1], SEL_RP_VERSION, 0x00010000);
    set(&g_hooks[2], SEL_RP_KIND, SEL_RP_KIND_RENDER);
    set(&g_hooks[3], SEL_RP_EXTENSIONS, (uintptr_t) "psy");
    set(&g_hooks[4], SEL_RP_PROBE, SEL_TAG_FN(rp_probe));
    set(&g_hooks[5], SEL_RP_OPEN, SEL_TAG_FN(rp_open));
    set(&g_hooks[6], SEL_RP_CLOSE, SEL_TAG_FN(rp_close));
    set(&g_hooks[7], SEL_RP_START, SEL_TAG_FN(rp_start));
    set(&g_hooks[8], SEL_RP_RENDER, SEL_TAG_FN(rp_render));
    set(&g_hooks[9], SEL_RP_SEEK, SEL_TAG_FN(rp_seek));
    set(&g_hooks[10], SEL_RP_METADATA, SEL_TAG_FN(rp_metadata));
    set(&g_hooks[11], SEL_TAG_END, 0);

    set(&g_desc[0], SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_REPLAYER);
    set(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "bbmp_psycle");
    set(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    set(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    set(&g_desc[4], SEL_TAG_END, 0);
    return g_desc;
}
